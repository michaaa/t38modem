/*
 * t38proto.cxx
 *
 * T.38 protocol handler
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 1998-2002 Equivalence Pty. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): Vyacheslav Frolov.
 *
 * $Revision: 24163 $
 * $Author: rjongbloed $
 * $Date: 2010-03-29 09:52:44 +0200 (Mo, 29. Mär 2010) $
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "t38proto.h"
#endif

#include <opal/buildopts.h>

#include <t38/t38proto.h>
#include <opal/patch.h>
#include <codec/opalpluginmgr.h>


/////////////////////////////////////////////////////////////////////////////

#if OPAL_FAX

#include <asn/t38.h>

OPAL_DEFINE_MEDIA_COMMAND(OpalFaxTerminate, PLUGINCODEC_CONTROL_TERMINATE_CODEC);

#define new PNEW


static const char TIFF_File_FormatName[] = "TIFF-File";


/////////////////////////////////////////////////////////////////////////////

class OpalFaxMediaStream : public OpalNullMediaStream
{
  public:
    OpalFaxMediaStream(OpalFaxConnection & conn,
                       const OpalMediaFormat & mediaFormat,
                       unsigned sessionID,
                       bool isSource)
      : OpalNullMediaStream(conn, mediaFormat, sessionID, isSource, isSource, true)
      , m_connection(conn)
    {
      m_isAudio = true; // Even though we are not REALLY audio, act like we are
    }

  private:
    OpalFaxConnection      & m_connection;
};


/////////////////////////////////////////////////////////////////////////////

class T38PseudoRTP_Handler : public RTP_Encoding
{
  public:
    T38PseudoRTP_Handler()
    {
      PStringToString options;

      options.SetAt("T38-UDPTL-Redundancy", "32767:1");           // re-send all ifp packets 1 time
      options.SetAt("T38-UDPTL-Redundancy-Interval", "0");        // re-send redundancy ifp packets only with next ifp
      options.SetAt("T38-UDPTL-Keep-Alive-Interval", "0");        // no send keep-alive packets
      options.SetAt("T38-UDPTL-Optimise-On-Retransmit", "false"); // not optimise udptl packets on retransmit

      ApplyStringOptions(options);
    }


    void OnStart(RTP_Session & _rtpUDP)
    {
      RTP_Encoding::OnStart(_rtpUDP);

      rtpUDP->SetJitterBufferSize(0, 0);
      m_consecutiveBadPackets  = 0;
      m_oneGoodPacket          = false;
      m_expectedSequenceNumber = 0;
      m_secondaryPacket        = -1;

      m_sentPacketRedundancy.clear();
      m_sentPacket = T38_UDPTLPacket();
      m_sentPacket.m_error_recovery.SetTag(T38_UDPTLPacket_error_recovery::e_secondary_ifp_packets);
      m_sentPacket.m_seq_number = (unsigned)-1;
      rtpUDP->SetNextSentSequenceNumber(0);
    }


    void ApplyStringOptions(const PStringToString & stringOptions)
    {
      for (PINDEX i = 0 ; i < stringOptions.GetSize() ; i++) {
        PCaselessString key = stringOptions.GetKeyAt(i);

        if (key == "T38-UDPTL-Redundancy") {
          PStringArray value = stringOptions.GetDataAt(i).Tokenise(",", FALSE);
          PWaitAndSignal mutex(m_writeMutex);

          m_redundancy.clear();

          for (PINDEX i = 0 ; i < value.GetSize() ; i++) {
            PStringArray pair = value[i].Tokenise(":", FALSE);

            if (pair.GetSize() == 2) {
              long size = pair[0].AsInteger();
              long redundancy = pair[1].AsInteger();

              if (size > INT_MAX)
                size = INT_MAX;

              if (size > 0 && redundancy >= 0) {
                m_redundancy[(int)size] = (int)redundancy;
                continue;
              }
            }

            PTRACE(2, "T38_UDPTL\tIgnored redundancy \"" << value[i] << "\"");
          }

#if PTRACING
          if (PTrace::CanTrace(3)) {
            PStringStream s;

            for (std::map<int, int>::iterator i = m_redundancy.begin() ; i != m_redundancy.end() ; i++) {
              if (!s.IsEmpty())
                s << ",";

              s << i->first << ":" << i->second;
            }

            PTRACE(3, "T38_UDPTL\tUse redundancy \"" << s << "\"");
          }
#endif
        }
        else
        if (key == "T38-UDPTL-Redundancy-Interval") {
          PWaitAndSignal mutex(m_writeMutex);
          m_redundancyInterval = stringOptions.GetDataAt(i).AsUnsigned();
          PTRACE(3, "T38_UDPTL\tUse redundancy interval " << m_redundancyInterval);
        }
        else
        if (key == "T38-UDPTL-Keep-Alive-Interval") {
          PWaitAndSignal mutex(m_writeMutex);
          m_keepAliveInterval = stringOptions.GetDataAt(i).AsUnsigned();
          PTRACE(3, "T38_UDPTL\tUse keep-alive interval " << m_keepAliveInterval);
        }
        else
        if (key == "T38-UDPTL-Optimise-On-Retransmit") {
          PCaselessString value = stringOptions.GetDataAt(i);
          PWaitAndSignal mutex(m_writeMutex);

          m_optimiseOnRetransmit =
            (value.IsEmpty() || (value == "true") || (value == "yes") || value.AsInteger() != 0);

          PTRACE(3, "T38_UDPTL\tUse optimise on retransmit - " << (m_optimiseOnRetransmit ? "true" : "false"));
        }
        else {
          PTRACE(4, "T38_UDPTL\tIgnored option " << key << " = \"" << stringOptions.GetDataAt(i) << "\"");
        }
      }
    }


    PBoolean WriteData(RTP_DataFrame & frame, bool oob)
    {
      if (oob)
        return false;

      return RTP_Encoding::WriteData(frame, false);
    }


    RTP_Session::SendReceiveStatus OnSendData(RTP_DataFrame & frame)
    {
      RTP_Session::SendReceiveStatus status = RTP_Encoding::OnSendData(frame);

      if (status == RTP_Session::e_ProcessPacket && frame.GetPayloadSize() == 0)
        status = RTP_Session::e_IgnorePacket;

      return status;
    }


    bool WriteDataPDU(RTP_DataFrame & frame)
    {
      PINDEX plLen = frame.GetPayloadSize();

      if (plLen == 0) {
        PTRACE(2, "T38_UDPTL\tInternal error - empty payload");
        return false;
      }

      PWaitAndSignal mutex(m_writeMutex);

      if (!m_sentPacketRedundancy.empty()) {
        T38_UDPTLPacket_error_recovery &recovery = m_sentPacket.m_error_recovery;

        if (recovery.GetTag() == T38_UDPTLPacket_error_recovery::e_secondary_ifp_packets) {
          // shift old primary ifp packet to secondary list

          T38_UDPTLPacket_error_recovery_secondary_ifp_packets &secondary = recovery;

          if (secondary.SetSize(secondary.GetSize() + 1)) {
            for (int i = secondary.GetSize() - 2 ; i >= 0 ; i--) {
              secondary[i + 1] = secondary[i];
              secondary[i] = T38_UDPTLPacket_error_recovery_secondary_ifp_packets_subtype();
            }

            secondary[0].SetValue(m_sentPacket.m_primary_ifp_packet.GetValue());
            m_sentPacket.m_primary_ifp_packet = T38_UDPTLPacket_primary_ifp_packet();
          }
        } else {
          PTRACE(3, "T38_UDPTL\tNot implemented yet " << recovery.GetTagName());
        }
      }

      // calculate redundancy for new ifp packet

      int redundancy = 0;

      for (std::map<int, int>::iterator i = m_redundancy.begin() ; i != m_redundancy.end() ; i++) {
        if (plLen <= i->first) {
          if (redundancy < i->second)
            redundancy = i->second;

          break;
        }
      }

      if (redundancy > 0 || !m_sentPacketRedundancy.empty())
        m_sentPacketRedundancy.insert(m_sentPacketRedundancy.begin(), redundancy + 1);

      // set new primary ifp packet

      m_sentPacket.m_seq_number = frame.GetSequenceNumber();
      m_sentPacket.m_primary_ifp_packet.SetValue(frame.GetPayloadPtr(), plLen);

      bool ok = WriteUDPTL();

      DecrementSentPacketRedundancy(true);

      return ok;
    }


    void OnWriteDataIdle()
    {
      PWaitAndSignal mutex(m_writeMutex);

      WriteUDPTL();

      DecrementSentPacketRedundancy(m_optimiseOnRetransmit);
    }


    void SetWriteDataIdleTimer(PTimer & timer)
    {
      PWaitAndSignal mutex(m_writeMutex);

      if (m_sentPacketRedundancy.empty() || m_redundancyInterval <= 0)
        timer = m_keepAliveInterval;
      else
        timer = m_redundancyInterval;
    }


    void DecrementSentPacketRedundancy(bool stripRedundancy)
    {
      int iMax = (int)m_sentPacketRedundancy.size() - 1;

      for (int i = iMax ; i >= 0 ; i--) {
        m_sentPacketRedundancy[i]--;

        if (i == iMax && m_sentPacketRedundancy[i] <= 0)
          iMax--;
      }

      m_sentPacketRedundancy.resize(iMax + 1);

      if (stripRedundancy) {
        T38_UDPTLPacket_error_recovery &recovery = m_sentPacket.m_error_recovery;

        if (recovery.GetTag() == T38_UDPTLPacket_error_recovery::e_secondary_ifp_packets) {
          T38_UDPTLPacket_error_recovery_secondary_ifp_packets &secondary = recovery;
          secondary.SetSize(iMax > 0 ? iMax : 0);
        } else {
          PTRACE(3, "T38_UDPTL\tNot implemented yet " << recovery.GetTagName());
        }
      }
    }


    bool WriteUDPTL()
    {
      PTRACE(5, "T38_UDPTL\tEncoded transmitted UDPTL data :\n  " << setprecision(2) << m_sentPacket);

      PPER_Stream rawData;
      m_sentPacket.Encode(rawData);
      rawData.CompleteEncoding();

      PTRACE(4, "T38_UDPTL\tSending UDPTL of size " << rawData.GetSize());

      return rtpUDP->WriteDataOrControlPDU(rawData.GetPointer(), rawData.GetSize(), true);
    }


    RTP_Session::SendReceiveStatus OnSendControl(RTP_ControlFrame & /*frame*/, PINDEX & /*len*/)
    {
      return RTP_Session::e_IgnorePacket; // Non fatal error, just ignore
    }


    int WaitForPDU(PUDPSocket & dataSocket, PUDPSocket & controlSocket, const PTimeInterval &)
    {
      if (m_secondaryPacket >= 0)
        return -1; // Force immediate call to ReadDataPDU

      // Break out once a second so closes down in orderly fashion
      return PSocket::Select(dataSocket, controlSocket, 1000);
    }


    RTP_Session::SendReceiveStatus OnReadTimeout(RTP_DataFrame & frame)
    {
      // Override so do not do sender reports (RTP only) and push
      // through a zero length packet so checks for orderly shut down
      frame.SetPayloadSize(0);
      return RTP_Session::e_ProcessPacket;
    }


    void SetFrameFromIFP(RTP_DataFrame & frame, const PASN_OctetString & ifp, unsigned sequenceNumber)
    {
      frame.SetPayloadSize(ifp.GetDataLength());
      memcpy(frame.GetPayloadPtr(), (const BYTE *)ifp, ifp.GetDataLength());
      frame.SetSequenceNumber((WORD)(sequenceNumber & 0xffff));
      if (m_secondaryPacket <= 0)
        m_expectedSequenceNumber = sequenceNumber+1;
    }

    RTP_Session::SendReceiveStatus ReadDataPDU(RTP_DataFrame & frame)
    {
      if (m_secondaryPacket >= 0) {
        if (m_secondaryPacket == 0)
          SetFrameFromIFP(frame, m_receivedPacket.m_primary_ifp_packet, m_receivedPacket.m_seq_number);
        else {
          T38_UDPTLPacket_error_recovery_secondary_ifp_packets & secondaryPackets = m_receivedPacket.m_error_recovery;
          SetFrameFromIFP(frame, secondaryPackets[m_secondaryPacket-1], m_receivedPacket.m_seq_number - m_secondaryPacket);
        }
        --m_secondaryPacket;
        return RTP_Session::e_ProcessPacket;
      }

      BYTE thisUDPTL[500];
      RTP_Session::SendReceiveStatus status = rtpUDP->ReadDataOrControlPDU(thisUDPTL, sizeof(thisUDPTL), true);
      if (status != RTP_Session::e_ProcessPacket)
        return status;

      PINDEX pduSize = rtpUDP->GetDataSocket().GetLastReadCount();
      
      PTRACE(4, "T38_UDPTL\tRead UDPTL of size " << pduSize);

      PPER_Stream rawData(thisUDPTL, pduSize);

      // Decode the PDU
      if (!m_receivedPacket.Decode(rawData)) {
  #if PTRACING
        if (m_oneGoodPacket)
          PTRACE(2, "RTP_T38\tRaw data decode failure:\n  "
                 << setprecision(2) << rawData << "\n  UDPTL = "
                 << setprecision(2) << m_receivedPacket);
        else
          PTRACE(2, "RTP_T38\tRaw data decode failure: " << rawData.GetSize() << " bytes.");
  #endif

        m_consecutiveBadPackets++;
        if (m_consecutiveBadPackets < 100)
          return RTP_Session::e_IgnorePacket;

        PTRACE(1, "RTP_T38\tRaw data decode failed 100 times, remote probably not switched from audio, aborting!");
        return RTP_Session::e_AbortTransport;
      }

      PTRACE_IF(3, !m_oneGoodPacket, "T38_UDPTL\tFirst decoded UDPTL packet");
      m_oneGoodPacket = true;
      m_consecutiveBadPackets = 0;

      PTRACE(5, "T38_UDPTL\tDecoded UDPTL packet:\n  " << setprecision(2) << m_receivedPacket);

      int missing = m_receivedPacket.m_seq_number - m_expectedSequenceNumber;
      if (missing > 0 && m_receivedPacket.m_error_recovery.GetTag() == T38_UDPTLPacket_error_recovery::e_secondary_ifp_packets) {
        // Packets are missing and we have redundency in the UDPTL packets
        T38_UDPTLPacket_error_recovery_secondary_ifp_packets & secondaryPackets = m_receivedPacket.m_error_recovery;
        if (secondaryPackets.GetSize() > 0) {
          PTRACE(4, "T38_UDPTL\tUsing redundant data to reconstruct missing/out of order packet at SN=" << m_expectedSequenceNumber);
          m_secondaryPacket = missing;
          if (m_secondaryPacket > secondaryPackets.GetSize())
            m_secondaryPacket = secondaryPackets.GetSize();
          SetFrameFromIFP(frame, secondaryPackets[m_secondaryPacket-1], m_receivedPacket.m_seq_number - m_secondaryPacket);
          --m_secondaryPacket;
          return RTP_Session::e_ProcessPacket;
        }
      }

      SetFrameFromIFP(frame, m_receivedPacket.m_primary_ifp_packet, m_receivedPacket.m_seq_number);
      m_expectedSequenceNumber = m_receivedPacket.m_seq_number+1;

      return RTP_Session::e_ProcessPacket;
    }


  protected:
    int             m_consecutiveBadPackets;
    bool            m_oneGoodPacket;
    T38_UDPTLPacket m_receivedPacket;
    unsigned        m_expectedSequenceNumber;
    int             m_secondaryPacket;

    std::map<int, int>  m_redundancy;
    PTimeInterval       m_redundancyInterval;
    PTimeInterval       m_keepAliveInterval;
    bool                m_optimiseOnRetransmit;
    std::vector<int>    m_sentPacketRedundancy;
    T38_UDPTLPacket     m_sentPacket;
    PMutex              m_writeMutex;
};


PFACTORY_CREATE(PFactory<RTP_Encoding>, T38PseudoRTP_Handler, "udptl", false);


/////////////////////////////////////////////////////////////////////////////

OpalFaxEndPoint::OpalFaxEndPoint(OpalManager & mgr, const char * g711Prefix, const char * t38Prefix)
  : OpalEndPoint(mgr, g711Prefix, CanTerminateCall)
  , m_t38Prefix(t38Prefix)
  , m_defaultDirectory(".")
{
  if (t38Prefix != NULL)
    mgr.AttachEndPoint(this, m_t38Prefix);

  PTRACE(3, "Fax\tCreated Fax endpoint");
}


OpalFaxEndPoint::~OpalFaxEndPoint()
{
  PTRACE(3, "Fax\tDeleted Fax endpoint.");
}


PSafePtr<OpalConnection> OpalFaxEndPoint::MakeConnection(OpalCall & call,
                                                    const PString & remoteParty,
                                                             void * userData,
                                                       unsigned int /*options*/,
                                    OpalConnection::StringOptions * stringOptions)
{
  if (!OpalMediaFormat(TIFF_File_FormatName).IsValid()) {
    PTRACE(1, "TIFF File format not valid! Missing plugin?");
    return false;
  }

  PINDEX prefixLength = remoteParty.Find(':');
  PStringArray tokens = remoteParty.Mid(prefixLength+1).Tokenise(";", true);
  if (tokens.IsEmpty()) {
    PTRACE(2, "Fax\tNo filename specified!");
    return NULL;
  }

  bool receiving = false;
  PString stationId = GetDefaultDisplayName();

  for (PINDEX i = 1; i < tokens.GetSize(); ++i) {
    if (tokens[i] *= "receive")
      receiving = true;
    else if (tokens[i].Left(10) *= "stationid=")
      stationId = tokens[i].Mid(10);
  }

  PString filename = tokens[0];
  if (!PFilePath::IsAbsolutePath(filename))
    filename.Splice(m_defaultDirectory, 0);

  if (!receiving && !PFile::Exists(filename)) {
    PTRACE(2, "Fax\tCannot find filename '" << filename << "'");
    return NULL;
  }

  OpalConnection::StringOptions localOptions;
  if (stringOptions == NULL)
    stringOptions = &localOptions;

  if ((*stringOptions)("stationid").IsEmpty())
    stringOptions->SetAt("stationid", stationId);

  stringOptions->SetAt(OPAL_OPT_DISABLE_JITTER, "1");

  return AddConnection(CreateConnection(call, userData, stringOptions, filename, receiving,
                                        remoteParty.Left(prefixLength) *= GetPrefixName()));
}


OpalFaxConnection * OpalFaxEndPoint::CreateConnection(OpalCall & call,
                                                      void * /*userData*/,
                                                      OpalConnection::StringOptions * stringOptions,
                                                      const PString & filename,
                                                      bool receiving,
                                                      bool disableT38)
{
  return new OpalFaxConnection(call, *this, filename, receiving, disableT38, stringOptions);
}


OpalMediaFormatList OpalFaxEndPoint::GetMediaFormats() const
{
  OpalMediaFormatList formats;
  formats += OpalT38;
  formats += TIFF_File_FormatName;
  return formats;
}


void OpalFaxEndPoint::AcceptIncomingConnection(const PString & token)
{
  PSafePtr<OpalFaxConnection> connection = PSafePtrCast<OpalConnection, OpalFaxConnection>(GetConnectionWithLock(token, PSafeReadOnly));
  if (connection != NULL)
    connection->AcceptIncoming();
}


void OpalFaxEndPoint::OnFaxCompleted(OpalFaxConnection & connection, bool failed)
{
  PTRACE(3, "FAX\tFax " << (failed ? "failed" : "completed") << " on connection: " << connection);
}


/////////////////////////////////////////////////////////////////////////////

OpalFaxConnection::OpalFaxConnection(OpalCall        & call,
                                     OpalFaxEndPoint & ep,
                                     const PString   & filename,
                                     bool              receiving,
                                     bool              disableT38,
                                     OpalConnection::StringOptions * stringOptions)
  : OpalConnection(call, ep, ep.GetManager().GetNextToken('F'), 0, stringOptions)
  , m_endpoint(ep)
  , m_filename(filename)
  , m_receiving(receiving)
  , m_disableT38(disableT38)
  , m_releaseTimeout(0, 0, 1)
  , m_tiffFileFormat(TIFF_File_FormatName)
  , m_awaitingSwitchToT38(!disableT38)
{
  m_tiffFileFormat.SetOptionString("TIFF-File-Name", filename);
  m_tiffFileFormat.SetOptionBoolean("Receiving", receiving);

  m_faxTimer.SetNotifier(PCREATE_NOTIFIER(OnSendCNGCED));

  PTRACE(3, "FAX\tCreated FAX connection with token \"" << callToken << "\","
            " receiving=" << receiving << ","
            " disabledT38=" << disableT38 << ","
            " filename=\"" << filename << '"');
}


OpalFaxConnection::~OpalFaxConnection()
{
  PTRACE(3, "FAX\tDeleted FAX connection.");
}


PString OpalFaxConnection::GetPrefixName() const
{
  return m_disableT38 ? m_endpoint.GetPrefixName() : m_endpoint.GetT38Prefix();
}


void OpalFaxConnection::ApplyStringOptions(OpalConnection::StringOptions & stringOptions)
{
  m_stationId = stringOptions("stationid");
  OpalConnection::ApplyStringOptions(stringOptions);
}


OpalMediaFormatList OpalFaxConnection::GetMediaFormats() const
{
  OpalMediaFormatList formats;

  if (!m_filename.IsEmpty())
    formats += m_tiffFileFormat;

  formats += OpalPCM16;

  if (!m_disableT38) {
    formats += OpalRFC2833;
    formats += OpalCiscoNSE;
  }

  return formats;
}


void OpalFaxConnection::AdjustMediaFormats(bool local, OpalMediaFormatList & mediaFormats, OpalConnection * otherConnection) const
{
  // Remove everything but G.711 or fax stuff
  OpalMediaFormatList::iterator i = mediaFormats.begin();
  while (i != mediaFormats.end()) {
    if ((m_awaitingSwitchToT38 && i->GetMediaType() == OpalMediaType::Audio()) ||
        (*i == OpalG711_ULAW_64K || *i == OpalG711_ALAW_64K || *i == OpalRFC2833 || *i == OpalCiscoNSE))
      ++i;
    else if (i->GetMediaType() != OpalMediaType::Fax() || (m_disableT38 && *i == OpalT38))
      mediaFormats -= *i++;
    else {
      i->SetOptionString("TIFF-File-Name", m_filename);
      i->SetOptionBoolean("Receiving", m_receiving);
      ++i;
    }
  }

  OpalConnection::AdjustMediaFormats(local, mediaFormats, otherConnection);
}


PBoolean OpalFaxConnection::SetUpConnection()
{
  // Check if we are A-Party in this call, so need to do things differently
  if (ownerCall.GetConnection(0) == this) {
    SetPhase(SetUpPhase);

    if (!OnIncomingConnection(0, NULL)) {
      Release(EndedByCallerAbort);
      return false;
    }

    PTRACE(2, "FAX\tOutgoing call routed to " << ownerCall.GetPartyB() << " for " << *this);
    if (!ownerCall.OnSetUp(*this)) {
      Release(EndedByNoAccept);
      return false;
    }

    return true;
  }

  PTRACE(3, "FAX\tSetUpConnection(" << remotePartyName << ')');
  SetPhase(AlertingPhase);
  OnAlerting();

  OnConnectedInternal();

  // Do not need to do audio mode initially if not going to T.38, already there!
  if (m_disableT38)
    return true;

  if (GetMediaStream(PString::Empty(), true) == NULL)
    ownerCall.OpenSourceMediaStreams(*this, OpalMediaType::Audio());

  return true;
}


PBoolean OpalFaxConnection::SetAlerting(const PString & calleeName, PBoolean)
{
  PTRACE(3, "Fax\tSetAlerting(" << calleeName << ')');
  SetPhase(AlertingPhase);
  remotePartyName = calleeName;
  return true;
}


PBoolean OpalFaxConnection::SetConnected()
{
  if (GetMediaStream(PString::Empty(), true) == NULL)
    ownerCall.OpenSourceMediaStreams(*this, OpalMediaType::Audio());
  return OpalConnection::SetConnected();
}


void OpalFaxConnection::OnEstablished()
{
  OpalConnection::OnEstablished();

  // If switched and we don't need to do CNG/CED any more, or T.38 is disabled
  // in which case the SpanDSP will deal with CNG/CED stuff.
  if (m_awaitingSwitchToT38) {
    m_faxTimer.SetInterval(1000);
    PTRACE(3, "T38\tStarting timer for CNG/CED tone");
  }
}


void OpalFaxConnection::OnReleased()
{
  m_faxTimer.Stop(false);
  OpalConnection::OnReleased();
}


OpalMediaStream * OpalFaxConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource)
{
  return new OpalFaxMediaStream(*this, mediaFormat, sessionID, isSource);
}


void OpalFaxConnection::OnStartMediaPatch(OpalMediaPatch & patch)
{
  // Have switched to T.38 mode
  if (patch.GetSink()->GetMediaFormat() == OpalT38) {
    m_faxTimer.Stop(false);
    m_awaitingSwitchToT38 = false;
  }

  OpalConnection::OnStartMediaPatch(patch);
}


void OpalFaxConnection::OnStopMediaPatch(OpalMediaPatch & patch)
{
  // Finished the fax transmission, look for TIFF
  OpalMediaStream & source = patch.GetSource();
  if (source.GetMediaFormat() == m_tiffFileFormat) {
    m_faxTimer.Stop();

    // Look for other sink, not the one from this patch
    OpalMediaStreamPtr sink = GetMediaStream(source.GetID(), !source.IsSource());
    if (sink != NULL)
      sink->ExecuteCommand(OpalFaxTerminate());
    else
      source.ExecuteCommand(OpalFaxTerminate());

    // Not an explicit switch, so fax plug in indicated end of fax
    if (m_faxMediaStreamsSwitchState == e_NotSwitchingFaxMediaStreams) {
      OnFaxCompleted(false);
      PThread::Create(PCREATE_NOTIFIER(ReleaseConnection));
    }
  }

  OpalConnection::OnStopMediaPatch(patch);
}


PBoolean OpalFaxConnection::SendUserInputTone(char tone, unsigned duration)
{
  OnUserInputTone(tone, duration);
  return true;
}


void OpalFaxConnection::OnUserInputTone(char tone, unsigned /*duration*/)
{
  // Not yet switched and got a CED from the remote system, start switch
  if (m_awaitingSwitchToT38 && !m_receiving && toupper(tone) == 'Y') {
    PTRACE(3, "T38\tRequesting mode change in response to CED");
    PThread::Create(PCREATE_NOTIFIER(OpenFaxStreams));
  }
}


void OpalFaxConnection::AcceptIncoming()
{
  if (LockReadWrite()) {
    OnConnectedInternal();
    UnlockReadWrite();
  }
}


void OpalFaxConnection::OnFaxCompleted(bool failed)
{
  m_endpoint.OnFaxCompleted(*this, failed);

  // Prevent to reuse filename
  m_filename.MakeEmpty();
}


void OpalFaxConnection::GetStatistics(OpalMediaStatistics & statistics) const
{
  OpalMediaStreamPtr stream;
  if ((stream = GetMediaStream(OpalMediaType::Fax(), false)) == NULL &&
      (stream = GetMediaStream(OpalMediaType::Fax(), true )) == NULL) {

    PSafePtr<OpalConnection> other = GetOtherPartyConnection();
    if (other == NULL)
      return;

    if ((stream = other->GetMediaStream(OpalMediaType::Fax(), false)) == NULL &&
        (stream = other->GetMediaStream(OpalMediaType::Fax(), true )) == NULL)
      return;
  }

  stream->GetStatistics(statistics);
}


void OpalFaxConnection::OnSendCNGCED(PTimer &, INT)
{
  if (m_awaitingSwitchToT38 && LockReadOnly()) {
    PTimeInterval elapsed = PTime() - connectedTime;
    if (m_releaseTimeout > 0 && elapsed > m_releaseTimeout) {
      PTRACE(2, "T38\tDid not switch to T.38 mode, releasing connection");
      Release(OpalConnection::EndedByCapabilityExchange);
    }
    else if (m_receiving) {
      PTRACE(2, "T38\tSwitching to T.38 mode");
      PThread::Create(PCREATE_NOTIFIER(OpenFaxStreams));
    }
    else
    if (m_switchTimeout > 0 && elapsed > m_switchTimeout) {
      PTRACE(2, "T38\tDid not switch to T.38 mode, forcing switch");
      PThread::Create(PCREATE_NOTIFIER(OpenFaxStreams));
    }
    else {
      // Cadence for CNG is 500ms on 3 seconds off
      OpalConnection::OnUserInputTone('X', 500);
      m_faxTimer = 3500;
    }
    UnlockReadOnly();
  }
}


bool OpalFaxConnection::SwitchFaxMediaStreams(bool enableFax)
{
  PSafePtr<OpalConnection> other = GetOtherPartyConnection();
  if (other != NULL && other->SwitchFaxMediaStreams(enableFax))
    return true;

  PTRACE(1, "T38\tMode change request to " << (enableFax ? "fax" : "audio") << " failed");
  return false;
}


void OpalFaxConnection::OnSwitchedFaxMediaStreams(bool enabledFax)
{
  m_awaitingSwitchToT38 = false;

  if (enabledFax) {
    PTRACE(3, "T38\tMode change request to fax succeeded");
  }
  else {
    PTRACE(4, "T38\tMode change request to fax failed, falling back to G.711");
    m_disableT38 = true;
    SwitchFaxMediaStreams(false);
  }
}


void OpalFaxConnection::OpenFaxStreams(PThread &, INT)
{
  if (LockReadWrite()) {
    m_awaitingSwitchToT38 = false;
    if (!SwitchFaxMediaStreams(true)) {
      OnFaxCompleted(true);
      Release(OpalConnection::EndedByCapabilityExchange);
    }
    UnlockReadWrite();
  }
}


void OpalFaxConnection::ReleaseConnection(PThread &, INT)
{
  if (LockReadWrite()) {
    Release(OpalConnection::EndedByLocalUser);
    UnlockReadWrite();
  }
}


#endif // OPAL_FAX

