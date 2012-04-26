/*
 * call.cxx
 *
 * Call management
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 24149 $
 * $Author: rjongbloed $
 * $Date: 2010-03-23 09:32:00 +0100 (Di, 23. Mär 2010) $
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "call.h"
#endif

#include <opal/buildopts.h>

#include <opal/call.h>

#include <opal/manager.h>
#include <opal/endpoint.h>
#include <opal/patch.h>
#include <opal/transcoders.h>


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

OpalCall::OpalCall(OpalManager & mgr)
  : manager(mgr)
  , myToken(mgr.GetNextToken('C'))
  , isEstablished(PFalse)
  , isClearing(PFalse)
  , callEndReason(OpalConnection::NumCallEndReasons)
  , endCallSyncPoint(NULL)
#if OPAL_HAS_MIXER
  , m_recordManager(NULL)
#endif
{
  manager.activeCalls.SetAt(myToken, this);

  connectionsActive.DisallowDeleteObjects();

  PTRACE(3, "Call\tCreated " << *this);
}

#ifdef _MSC_VER
#pragma warning(default:4355)
#endif


OpalCall::~OpalCall()
{
#if OPAL_HAS_MIXER
  delete m_recordManager;
#endif

  PTRACE(3, "Call\t" << *this << " destroyed.");
}


void OpalCall::PrintOn(ostream & strm) const
{
  strm << "Call[" << myToken << ']';
}


void OpalCall::OnEstablishedCall()
{
  manager.OnEstablishedCall(*this);
}


void OpalCall::SetCallEndReason(OpalConnection::CallEndReason reason)
{
  // Only set reason if not already set to something
  if (callEndReason == OpalConnection::NumCallEndReasons)
    callEndReason = reason;
}


void OpalCall::Clear(OpalConnection::CallEndReason reason, PSyncPoint * sync)
{
  PTRACE(3, "Call\tClearing " << (sync != NULL ? "(sync) " : "") << *this << " reason=" << reason);

  if (!LockReadWrite())
    return;

  isClearing = PTrue;

  SetCallEndReason(reason);

  if (sync != NULL && !connectionsActive.IsEmpty()) {
    // only set the sync point if it is NULL
    if (endCallSyncPoint == NULL)
      endCallSyncPoint = sync;
    else {
      PAssertAlways("Can only have one thread doing ClearCallSynchronous");
    }
  }

  UnlockReadWrite();

  InternalOnClear();

  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadWrite))
    connection->Release(reason);
}


void OpalCall::OnCleared()
{
  manager.OnClearedCall(*this);

#if OPAL_HAS_MIXER
  StopRecording();
#endif

  if (!LockReadWrite())
    return;

  if (endCallSyncPoint != NULL) {
    endCallSyncPoint->Signal();
    endCallSyncPoint = NULL;
  }

  UnlockReadWrite();
}


void OpalCall::OnNewConnection(OpalConnection & connection)
{
  manager.OnNewConnection(connection);
  SetPartyNames();
}


PBoolean OpalCall::OnSetUp(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnSetUp " << connection);

  if (isClearing)
    return false;

  SetPartyNames();

  bool ok = false;

  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadWrite, &connection)) {
    if (otherConnection->SetUpConnection() && otherConnection->OnSetUpConnection())
      ok = true;
  }

  return ok;
}


void OpalCall::OnProceeding(OpalConnection & PTRACE_PARAM(connection))
{
  PTRACE(3, "Call\tOnProceeding " << connection);
}


PBoolean OpalCall::OnAlerting(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnAlerting " << connection);

  if (isClearing)
    return false;

  PBoolean hasMedia = connection.GetMediaStream(OpalMediaType::Audio(), true) != NULL;

  bool ok = false;

  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadWrite, &connection)) {
    if (otherConnection->SetAlerting(connection.GetRemotePartyName(), hasMedia))
      ok = true;
  }

  SetPartyNames();

  return ok;
}

OpalConnection::AnswerCallResponse OpalCall::OnAnswerCall(OpalConnection & PTRACE_PARAM(connection),
                                                          const PString & PTRACE_PARAM(caller))
{
  PTRACE(3, "Call\tOnAnswerCall " << connection << " caller \"" << caller << '"');
  return OpalConnection::AnswerCallDeferred;
}


PBoolean OpalCall::OnConnected(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnConnected " << connection);

  if (isClearing || !LockReadOnly())
    return false;

  bool havePartyB = connectionsActive.GetSize() == 1 && !m_partyB.IsEmpty();

  UnlockReadOnly();

  if (havePartyB) {
    if (manager.MakeConnection(*this, m_partyB, NULL, 0,
                                const_cast<OpalConnection::StringOptions *>(&connection.GetStringOptions())) != NULL)
      return OnSetUp(connection);

    connection.Release(OpalConnection::EndedByNoUser);
    return false;
  }

  bool ok = false;
  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadWrite, &connection)) {
    if (otherConnection->GetPhase() >= OpalConnection::ConnectedPhase ||
        otherConnection->SetConnected())
      ok = true;
  }

  SetPartyNames();

  return ok;
}


PBoolean OpalCall::OnEstablished(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnEstablished " << connection);

  PSafeLockReadWrite lock(*this);
  if (isClearing || !lock.IsLocked())
    return false;

  if (isEstablished)
    return true;

  if (connectionsActive.GetSize() < 2)
    return false;

  connection.StartMediaStreams();

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReference); conn != NULL; ++conn) {
    if (conn->GetPhase() != OpalConnection::EstablishedPhase)
      return false;
  }

  isEstablished = true;
  OnEstablishedCall();

  return true;
}


PSafePtr<OpalConnection> OpalCall::GetOtherPartyConnection(const OpalConnection & connection) const
{
  PTRACE(3, "Call\tGetOtherPartyConnection " << connection);

  PSafePtr<OpalConnection> otherConnection;
  EnumerateConnections(otherConnection, PSafeReference, &connection);
  return otherConnection;
}


bool OpalCall::Hold()
{
  PTRACE(3, "Call\tSetting to On Hold");

  bool ok = false;

  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadWrite)) {
    if (connection->HoldConnection())
      ok = true;
  }

  return ok;
}


bool OpalCall::Retrieve()
{
  PTRACE(3, "Call\tRetrieve from On Hold");

  bool ok = false;

  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadWrite)) {
    if (connection->RetrieveConnection())
      ok = true;
  }

  return ok;
}


bool OpalCall::IsOnHold() const
{
  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadOnly)) {
    if (connection->IsConnectionOnHold(false) || connection->IsConnectionOnHold(true))
      return true;
  }

  return false;
}


bool OpalCall::Transfer(const PString & newAddress, OpalConnection * connection)
{
  PCaselessString prefix = newAddress.Left(newAddress.Find(':'));

  if (connection == NULL) {
    for (PSafePtr<OpalConnection> conn = GetConnection(0); conn != NULL; ++conn) {
      if (prefix == conn->GetPrefixName())
        return conn->TransferConnection(newAddress);
    }

    PTRACE(2, "Call\tUnable to resolve transfer to \"" << newAddress << '"');
    return false;
  }

  if (prefix == "*")
    return connection->TransferConnection(connection->GetPrefixName() + newAddress.Mid(1));

  if (prefix == connection->GetPrefixName() || manager.HasCall(newAddress))
    return connection->TransferConnection(newAddress);

  PTRACE(3, "Call\tTransferring " << *connection << " to \"" << newAddress << '"');

  PSafePtr<OpalConnection> connectionToKeep = GetOtherPartyConnection(*connection);
  if (connectionToKeep == NULL)
    return false;

  PSafePtr<OpalConnection> newConnection = manager.MakeConnection(*this, newAddress);
  if (newConnection == NULL)
    return false;

  OpalConnection::Phases oldPhase = connection->GetPhase();
  connection->SetPhase(OpalConnection::ForwardingPhase);

  // Restart with new connection
  if (newConnection->SetUpConnection() && newConnection->OnSetUpConnection()) {
    connectionToKeep->AutoStartMediaStreams(true);
    connection->Release(OpalConnection::EndedByCallForwarded);
    newConnection->StartMediaStreams();
    return true;
  }

  newConnection->Release(OpalConnection::EndedByTemporaryFailure);
  connection->SetPhase(oldPhase);
  return false;
}


OpalMediaFormatList OpalCall::GetMediaFormats(const OpalConnection & connection,
                                              PBoolean includeSpecifiedConnection)
{
  OpalMediaFormatList commonFormats;

  PBoolean first = PTrue;

  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadOnly, includeSpecifiedConnection ? NULL : &connection)) {
    OpalMediaFormatList possibleFormats = OpalTranscoder::GetPossibleFormats(otherConnection->GetMediaFormats());
    connection.AdjustMediaFormats(true, possibleFormats, otherConnection);
    if (first) {
      commonFormats = possibleFormats;
      first = PFalse;
    }
    else {
      // Want intersection of the possible formats for all connections.
      for (OpalMediaFormatList::iterator format = commonFormats.begin(); format != commonFormats.end(); ) {
        if (possibleFormats.HasFormat(*format))
          ++format;
        else
          commonFormats.erase(format++);
      }
    }
  }

  connection.AdjustMediaFormats(true, commonFormats, NULL);

  PTRACE(4, "Call\tGetMediaFormats for " << connection << '\n'
         << setfill('\n') << commonFormats << setfill(' '));

  return commonFormats;
}


PBoolean OpalCall::OpenSourceMediaStreams(OpalConnection & connection, 
                                     const OpalMediaType & mediaType,
                                                  unsigned sessionID, 
                                   const OpalMediaFormat & preselectedFormat
#if OPAL_VIDEO
                            , OpalVideoFormat::ContentRole contentRole
#endif
                                   )
{
  PSafeLockReadOnly lock(*this);
  if (isClearing || !lock.IsLocked())
    return false;

#if PTRACING
  PStringStream traceText;
  if (PTrace::CanTrace(2)) {
    traceText << " for " << mediaType << " session " << sessionID;
    if (preselectedFormat.IsValid())
      traceText << " (" << preselectedFormat << ')';
    traceText << " on " << connection;
  }
#endif

  if (IsOnHold()) {
    PTRACE(3, "Call\tOpenSourceMediaStreams (call on hold)" << traceText);
    return false;
  }

  // Check if already done
  OpalMediaStreamPtr sinkStream;
  OpalMediaStreamPtr sourceStream = connection.GetMediaStream(sessionID, true);
  if (sourceStream != NULL) {
    OpalMediaPatch * patch = sourceStream->GetPatch();
    if (patch != NULL)
      sinkStream = patch->GetSink();
    if (sourceStream->GetMediaFormat() == preselectedFormat ||
        (sinkStream != NULL && sinkStream->GetMediaFormat() == preselectedFormat)) {
      PTRACE(3, "Call\tOpenSourceMediaStreams (already opened)" << traceText);
      return true;
    }
    if (sinkStream == NULL && sourceStream->IsOpen()) {
      PTRACE(3, "Call\tOpenSourceMediaStreams (is opening)" << traceText);
      return true;
    }
  }

  if (sessionID == 0)
    sessionID = connection.GetNextSessionID(mediaType, true);

  PTRACE(3, "Call\tOpenSourceMediaStreams " << (sourceStream != NULL ? "replace" : "open") << traceText);
  sourceStream.SetNULL();

  // Create the sinks and patch if needed
  bool startedOne = false;
  OpalMediaPatch * patch = NULL;
  OpalMediaFormat sourceFormat, sinkFormat;

  // Reorder destinations so we give preference to symmetric codecs
  if (sinkStream != NULL)
    sinkFormat = sinkStream->GetMediaFormat();

  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadWrite, &connection)) {
    PStringArray order;
    if (sinkFormat.IsValid())
      order += sinkFormat.GetName(); // Preferential treatment to format already in use
    order += '@' + mediaType;        // And media of the same type

    OpalMediaFormatList sinkMediaFormats = otherConnection->GetMediaFormats();
    connection.AdjustMediaFormats(false, sinkMediaFormats, otherConnection);
    if (sinkMediaFormats.IsEmpty()) {
      PTRACE(2, "Call\tOpenSourceMediaStreams failed with no sink formats" << traceText);
      return false;
    }

    if (preselectedFormat.IsValid() && sinkMediaFormats.HasFormat(preselectedFormat))
      sinkMediaFormats = preselectedFormat;
    else
      sinkMediaFormats.Reorder(order);

    OpalMediaFormatList sourceMediaFormats;
    if (sourceFormat.IsValid())
      sourceMediaFormats = sourceFormat; // Use the source format already established
    else {
      sourceMediaFormats = connection.GetMediaFormats();
      otherConnection->AdjustMediaFormats(false, sourceMediaFormats, &connection);
      if (sourceMediaFormats.IsEmpty()) {
        PTRACE(2, "Call\tOpenSourceMediaStreams failed with no source formats" << traceText);
        return false;
      }

      if (preselectedFormat.IsValid() && sourceMediaFormats.HasFormat(preselectedFormat))
        sourceMediaFormats = preselectedFormat;
      else
        sourceMediaFormats.Reorder(order);
    }

    // Get other media directions format so we give preference to symmetric codecs
    OpalMediaStreamPtr otherDirection = sessionID != 0 ? otherConnection->GetMediaStream(sessionID, true)
                                                       : otherConnection->GetMediaStream(mediaType, true);
    if (otherDirection != NULL) {
      PString priorityFormat = otherDirection->GetMediaFormat().GetName();
      sourceMediaFormats.Reorder(priorityFormat);
      sinkMediaFormats.Reorder(priorityFormat);
    }

#if OPAL_VIDEO
    if (contentRole != OpalVideoFormat::eNoRole) {
      // Remove all media formats no supporting the role
      OpalMediaFormatList * lists[2] = { &sourceMediaFormats, &sinkMediaFormats };
      for (PINDEX i = 0; i < 2; i++) {
        OpalMediaFormatList::iterator format = lists[i]->begin();
        while (format != lists[i]->end()) {
          if ( format->GetMediaType() != mediaType ||
              (format->IsTransportable() &&
              (format->GetOptionInteger(OpalVideoFormat::ContentRoleMaskOption())&OpalVideoFormat::ContentRoleBit(contentRole)) == 0))
            *lists[i] -= *format++;
          else
            ++format;
        }
        if (lists[i]->IsEmpty()) {
          PTRACE(2, "Call\tUnsupported Content Role " << contentRole << traceText);
          return false;
        }
      }
    }
#endif

    OpalMediaFormatList localMediaFormats = OpalMediaFormat::GetAllRegisteredMediaFormats();
    otherConnection->AdjustMediaFormats(true, localMediaFormats, &connection);
    if (!SelectMediaFormats(mediaType,
                            sourceMediaFormats,
                            sinkMediaFormats,
                            localMediaFormats,
                            sourceFormat,
                            sinkFormat))
      return false;

#if OPAL_VIDEO
    sourceFormat.SetOptionEnum(OpalVideoFormat::ContentRoleOption(), contentRole);
    sinkFormat.SetOptionEnum(OpalVideoFormat::ContentRoleOption(), contentRole);
#endif

    if (sessionID == 0) {
      sessionID = otherConnection->GetNextSessionID(mediaType, false);
      PTRACE(3, "Call\tOpenSourceMediaStreams using session " << sessionID << " on " << connection);
    }

    // Finally have the negotiated formats, open the streams
    if (sourceStream == NULL) {
      sourceStream = connection.OpenMediaStream(sourceFormat, sessionID, true);
      if (sourceStream == NULL)
        return false;
    }

    sinkStream = otherConnection->OpenMediaStream(sinkFormat, sessionID, false);
    if (sinkStream != NULL) {
      startedOne = true;
      if (patch == NULL) {
        patch = manager.CreateMediaPatch(*sourceStream, sinkStream->RequiresPatchThread(sourceStream) &&
                                                        sourceStream->RequiresPatchThread(sinkStream));
        if (patch == NULL)
          return false;
      }
      patch->AddSink(sinkStream);
    }
  }

  if (!startedOne) {
    if (sourceStream != NULL)
      connection.RemoveMediaStream(*sourceStream);
    return false;
  }

  if (patch != NULL) {
    // if a patch was created, make sure the callback is called, just once per connection
    while (EnumerateConnections(otherConnection, PSafeReadWrite))
      otherConnection->OnPatchMediaStream(otherConnection == &connection, *patch);
  }

  return true;
}


bool OpalCall::SelectMediaFormats(const OpalMediaType & mediaType,
                                  const OpalMediaFormatList & srcFormats,
                                  const OpalMediaFormatList & dstFormats,
                                  const OpalMediaFormatList & allFormats,
                                  OpalMediaFormat & srcFormat,
                                  OpalMediaFormat & dstFormat) const
{
  if (OpalTranscoder::SelectFormats(mediaType, srcFormats, dstFormats, allFormats, srcFormat, dstFormat)) {
    PTRACE(3, "Call\tSelected media formats " << srcFormat << " -> " << dstFormat);
    return true;
  }

  PTRACE(2, "Call\tSelectMediaFormats could not find compatible " << mediaType << " format:\n"
            "  source formats=" << setfill(',') << srcFormats << "\n"
            "   sink  formats=" << dstFormats << setfill(' '));
  return false;
}


void OpalCall::CloseMediaStreams()
{
  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadWrite))
    connection->CloseMediaStreams();
}


void OpalCall::OnRTPStatistics(const OpalConnection & /*connection*/, const RTP_Session & /*session*/)
{
}


PBoolean OpalCall::IsMediaBypassPossible(const OpalConnection & connection,
                                     unsigned sessionID) const
{
  PTRACE(3, "Call\tIsMediaBypassPossible " << connection << " session " << sessionID);

  PSafePtr<OpalConnection> otherConnection;
  return EnumerateConnections(otherConnection, PSafeReadOnly, &connection) &&
         manager.IsMediaBypassPossible(connection, *otherConnection, sessionID);
}


void OpalCall::OnUserInputString(OpalConnection & connection, const PString & value)
{
  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadWrite)) {
    if (otherConnection != &connection)
      otherConnection->SendUserInputString(value);
    else
      connection.SetUserInput(value);
  }
}


void OpalCall::OnUserInputTone(OpalConnection & connection,
                               char tone,
                               int duration)
{
  bool reprocess = duration > 0 && tone != ' ';

  PSafePtr<OpalConnection> otherConnection;
  while (EnumerateConnections(otherConnection, PSafeReadWrite, &connection)) {
    if (otherConnection->SendUserInputTone(tone, duration))
      reprocess = false;
  }

  if (reprocess)
    connection.OnUserInputString(tone);
}


void OpalCall::OnReleased(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnReleased " << connection);

  SetCallEndReason(connection.GetCallEndReason());

  connectionsActive.Remove(&connection);

  // A call will evaporate when one connection left, at some point this is
  // to be changes so can have "parked" connections. 
  if(connectionsActive.GetSize() == 1)
  {
    PSafePtr<OpalConnection> last = connectionsActive.GetAt(0, PSafeReference);
    if (last != NULL)
      last->Release(connection.GetCallEndReason());
  }

  InternalOnClear();
}


void OpalCall::InternalOnClear()
{
  if (connectionsActive.IsEmpty() && manager.activeCalls.Contains(GetToken())) {
    OnCleared();
    manager.activeCalls.RemoveAt(GetToken());
  }
}


void OpalCall::OnHold(OpalConnection & /*connection*/,
                      bool /*fromRemote*/, 
                      bool /*onHold*/)
{
}

#if OPAL_HAS_MIXER

bool OpalCall::StartRecording(const PFilePath & fn, const OpalRecordManager::Options & options)
{
  StopRecording();

  OpalRecordManager * newManager = OpalRecordManager::Factory::CreateInstance(fn.GetType());
  if (newManager == NULL) {
    PTRACE(2, "OPAL\tCannot record to file type " << fn);
    return false;
  }

  // create the mixer entry
  if (!newManager->Open(fn, options)) {
    delete newManager;
    return false;
  }

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return false;

  m_recordManager = newManager;

  // tell each connection to start sending data
  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadWrite))
    connection->EnableRecording();

  return true;
}

bool OpalCall::IsRecording() const
{
  PSafeLockReadOnly lock(*this);
  return m_recordManager != NULL && m_recordManager->IsOpen();
}


void OpalCall::StopRecording()
{
  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked() || m_recordManager == NULL)
    return;

  // tell each connection to stop sending data
  PSafePtr<OpalConnection> connection;
  while (EnumerateConnections(connection, PSafeReadWrite))
    connection->DisableRecording();

  m_recordManager->Close();
  delete m_recordManager;
  m_recordManager = NULL;
}


bool OpalCall::OnStartRecording(const PString & streamId, const OpalMediaFormat & format)
{
  return m_recordManager != NULL && m_recordManager->OpenStream(streamId, format);
}


void OpalCall::OnStopRecording(const PString & streamId)
{
  if (m_recordManager != NULL)
    m_recordManager->CloseStream(streamId);
}


void OpalCall::OnRecordAudio(const PString & streamId, const RTP_DataFrame & frame)
{
  if (m_recordManager != NULL && !m_recordManager->WriteAudio(streamId, frame))
    m_recordManager->CloseStream(streamId);
}


#if OPAL_VIDEO

void OpalCall::OnRecordVideo(const PString & streamId, const RTP_DataFrame & frame)
{
  if (m_recordManager != NULL && !m_recordManager->WriteVideo(streamId, frame))
    m_recordManager->CloseStream(streamId);
}

#endif

#endif


bool OpalCall::IsNetworkOriginated() const
{
  PSafePtr<OpalConnection> connection = PSafePtr<OpalConnection>(connectionsActive, PSafeReadOnly);
  return connection == NULL || connection->IsNetworkConnection();
}


void OpalCall::SetPartyNames()
{
  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  PSafePtr<OpalConnection> connectionA = connectionsActive.GetAt(0, PSafeReadOnly);
  if (connectionA == NULL)
    return;

  bool networkA = connectionA->IsNetworkConnection();
  if (networkA)
    m_partyA = connectionA->GetRemotePartyURL();
  if (!networkA || m_partyA.IsEmpty())
    m_partyA = connectionA->GetLocalPartyURL();

  PSafePtr<OpalConnection> connectionB = connectionsActive.GetAt(1, PSafeReadOnly);
  if (connectionB == NULL)
    return;

  if (connectionB->IsNetworkConnection()) {
    if (!networkA)
      connectionA->CopyPartyNames(*connectionB);
    m_partyB = connectionB->GetRemotePartyURL();
  }
  else {
    if (networkA) {
      connectionB->CopyPartyNames(*connectionA);
      m_partyB = connectionA->GetCalledPartyURL();
    }
    if (m_partyB.IsEmpty())
      m_partyB = connectionB->GetLocalPartyURL();
  }
}


bool OpalCall::EnumerateConnections(PSafePtr<OpalConnection> & connection,
                                    PSafetyMode mode,
                                    const OpalConnection * skipConnection) const
{
  if (connection == NULL)
    connection = PSafePtr<OpalConnection>(connectionsActive, PSafeReference);
  else {
    connection.SetSafetyMode(PSafeReference);
    ++connection;
  }

  while (connection != NULL) {
    if (connection != skipConnection &&
        connection->GetPhase() <= OpalConnection::EstablishedPhase &&
        connection.SetSafetyMode(mode))
      return true;
    ++connection;
  }

  return false;
}


/////////////////////////////////////////////////////////////////////////////
