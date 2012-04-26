/*
 * main.cxx
 *
 * OPAL application source file for playing RTP from a PCAP file
 *
 * Main program entry point.
 *
 * Copyright (c) 2007 Equivalence Pty. Ltd.
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 24103 $
 * $Author: rjongbloed $
 * $Date: 2010-03-05 04:47:43 +0100 (Fr, 05. Mär 2010) $
 */

#include "precompile.h"
#include "main.h"

#include <ptlib/vconvert.h>
#include <ptlib/pipechan.h>

PCREATE_PROCESS(PlayRTP);

const int g_extraHeight = 35;

struct DiscoveredRTPInfo {
  DiscoveredRTPInfo()
  { 
    m_found[0] = m_found[1] = false;
    m_ssrc_matches[0] = m_ssrc_matches[1] = 0;
    m_seq_matches[0]  = m_seq_matches[1]  = 0;
    m_ts_matches[0]   = m_ts_matches[1]   = 0;
    m_index[0] = m_index[1] = 0;
  }

  PIPSocketAddressAndPort m_addr[2];
  RTP_DataFrame::PayloadTypes m_payload[2];
  BOOL m_found[2];

  DWORD m_ssrc[2];
  WORD  m_seq[2];
  DWORD m_ts[2];

  unsigned m_ssrc_matches[2];
  unsigned m_seq_matches[2];
  unsigned m_ts_matches[2];

  RTP_DataFrame * m_firstFrame[2];

  PString m_type[2];
  PString m_format[2];

  size_t m_index[2];
};

typedef std::map<std::string, DiscoveredRTPInfo> DiscoveredRTPMap;
DiscoveredRTPMap discoveredRTPMap;

void Reverse(char * ptr, size_t sz)
{
  char * top = ptr+sz-1;
  while (ptr < top) {
    char t = *ptr;
    *ptr = *top;
    *top = t;
    ptr++;
    top--;
  }
}

#define REVERSE(p) Reverse((char *)p, sizeof(p))

bool IdentifyMediaType(const RTP_DataFrame & rtp, PString & type, PString & format)
{
  OpalMediaFormatList formats = OpalMediaFormat::GetAllRegisteredMediaFormats();

  RTP_DataFrame::PayloadTypes pt = rtp.GetPayloadType();

  type   = "Unknown";
  format = "Unknown";

  // look for known audio types
  if (pt <= RTP_DataFrame::Cisco_CN) {
    OpalMediaFormatList::const_iterator r;
    if ((r = formats.FindFormat(pt, OpalMediaFormat::AudioClockRate)) != formats.end()) {
      type   = r->GetMediaType();
      format = r->GetName();
      return true;
    }
    return false;
  }

  // look for known video types
  if (pt <= RTP_DataFrame::LastKnownPayloadType) {
    OpalMediaFormatList::const_iterator r;
    if ((r = formats.FindFormat(pt, OpalMediaFormat::VideoClockRate)) != formats.end()) {
      type   = r->GetMediaType();
      format = r->GetName();
      return true;
    }
    return false;
  }

  // try and identify media by inspection
  const BYTE * data = rtp.GetPayloadPtr();

  // xxx00111 01000010 xxxx0000 - H.264
  if (rtp.GetPayloadSize() > 6 && (data[0]&0x1f) == 7 && data[1] == 0x42 && (data[2]&0x0f) == 0) {
    type = OpalMediaType::Video();
    OpalMediaFormatList::const_iterator r = formats.FindFormat("*h.264*");
    if (r != formats.end())
      format = r->GetName();
  }

  // 0x00 0x00 0x1b - MPEG4
  else if (rtp.GetPayloadSize() > 6 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) {
    type = OpalMediaType::Video();
    OpalMediaFormatList::const_iterator r = formats.FindFormat("*mpeg4*");
    if (r != formats.end())
      format = r->GetName();
  }

  else
    cout << hex << (int)data[0] << ' ' << (int)data[1] << ' ' << (int)data[2] << dec << endl;

  return false;
}

void DisplaySessions(bool show = true)
{
  // display matches
  DiscoveredRTPMap::iterator r;
  int index = 1;
  for (r = discoveredRTPMap.begin(); r != discoveredRTPMap.end(); ++r) {
    DiscoveredRTPInfo & info = r->second;
    for (int dir = 0; dir < 2; ++dir) {
      if (info.m_found[dir]) {
#if 0
        if (info.m_seq_matches[dir] > 5 &&
            info.m_ts_matches[dir] > 5 &&
            info.m_ssrc_matches[dir] > 5) {
#endif
      {
          info.m_index[dir] = index++;
          PString type, format;
          IdentifyMediaType(*info.m_firstFrame[dir], info.m_type[dir], info.m_format[dir]);

          if (show) {
            if (info.m_payload[dir] != info.m_firstFrame[dir]->GetPayloadType()) {
              cout << "Mismatched payload types" << endl;
            }
            cout << info.m_index[dir] << " : " << info.m_addr[dir].AsString() 
                                      << " -> " << info.m_addr[1-dir].AsString() 
                                      << ", " << info.m_payload[dir] 
                                      << " " << info.m_type[dir]
                                      << " " << info.m_format[dir] << endl;
          }
        }
      }
    }
  }
}

PlayRTP::PlayRTP()
  : PProcess("OPAL Audio/Video Codec Tester", "PlayRTP", 1, 0, ReleaseCode, 0)
  , m_srcIP(PIPSocket::GetDefaultIpAny())
  , m_dstIP(PIPSocket::GetDefaultIpAny())
  , m_srcPort(0)
  , m_dstPort(0)
  , m_transcoder(NULL)
  , m_player(NULL)
  , m_display(NULL)
{
}


PlayRTP::~PlayRTP()
{
  delete m_transcoder;
  delete m_player;
}


void PlayRTP::Main()
{
  PArgList & args = GetArguments();

  args.Parse("h-help."
             "m-mapping:"
             "S-src-ip:"
             "D-dst-ip:"
             "s-src-port:"
             "d-dst-port:"
             "A-audio-driver:"
             "a-audio-device:"
             "V-video-driver:"
             "v-video-device:"
             "p-singlestep."
             "P-payload-file:"
             "i-info."
             "f-find."
             "Y-video-file:"
             "E:"
             "T:"
             "X."
             "O:"
             "-session:"
             "-nodelay."
#if PTRACING
             "o-output:"             "-no-output."
             "t-trace."              "-no-trace."
#endif
             , FALSE);

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
         PTrace::Blocks | PTrace::Timestamp | PTrace::Thread | PTrace::FileAndLine);
#endif

  if (args.HasOption('h') || args.GetCount() == 0) {
    PError << "usage: " << GetFile().GetTitle() << " [ options ] filename [ filename ... ]\n"
              "\n"
              "Available options are:\n"
              "  --help                   : print this help message.\n"
              "  -m or --mapping N=fmt    : Set mapping of payload type to format, eg 101=H.264\n"
              "  -S or --src-ip addr      : Source IP address, default is any\n"
              "  -D or --dst-ip addr      : Destination IP address, default is any\n"
              "  -s or --src-port N       : Source UDP port, default is any\n"
              "  -d or --dst-port N       : Destination UDP port, default is any\n"
              "  -A or --audio-driver drv : Audio player driver.\n"
              "  -a or --audio-device dev : Audio player device.\n"
              "  -V or --video-driver drv : Video display driver to use.\n"
              "  -v or --video-device dev : Video display device to use.\n"
              "  -p or --singlestep       : Single step through input data.\n"
              "  -i or --info             : Display per-frame information.\n"
              "  -f or --find             : find and display list of RTP sessions.\n"
              "  -P or --payload-file fn  : write RTP payload to file\n"
              "  -Y file                  : write decoded video to file\n"
              "  -E file                  : write event log to file\n"
              "  -T title                 : put text in extra video information\n"
              "  -X                       : enable extra video information\n"
              "  -O --option fmt:opt=val  : Set codec option (may be used multiple times)\n"
              "                           :  fmt is name of codec, eg \"H.261\"\n"
              "                           :  opt is name of option, eg \"Target Bit Rate\"\n"
              "                           :  val is value of option, eg \"48000\"\n"
              "  --session num            : automatically select session num\n"
              "  --nodelay                : do not delay as per timestamps\n"
#if PTRACING
              "  -o or --output file     : file name for output of log messages\n"       
              "  -t or --trace           : degree of verbosity in error log (more times for more detail)\n"     
#endif
              "\n"
              "e.g. " << GetFile().GetTitle() << " conversation.pcap\n\n";
    return;
  }

  m_extendedInfo = args.HasOption('X') || args.HasOption('T');
  m_noDelay      = args.HasOption("nodelay");

  PStringArray options = args.GetOptionString('O').Lines();
  for (PINDEX i = 0; i < options.GetSize(); i++) {
    const PString & optionDescription = options[i];
    PINDEX colon = optionDescription.Find(':');
    PINDEX equal = optionDescription.Find('=', colon+2);
    if (colon == P_MAX_INDEX || equal == P_MAX_INDEX) {
      cerr << "Invalid option description \"" << optionDescription << '"' << endl;
      continue;
    }
    OpalMediaFormat mediaFormat = optionDescription.Left(colon);
    if (mediaFormat.IsEmpty()) {
      cerr << "Invalid media format in option description \"" << optionDescription << '"' << endl;
      continue;
    }
    PString optionName = optionDescription(colon+1, equal-1);
    if (!mediaFormat.HasOption(optionName)) {
      cerr << "Invalid option name in description \"" << optionDescription << '"' << endl;
      continue;
    }
    PString valueStr = optionDescription.Mid(equal+1);
    if (!mediaFormat.SetOptionValue(optionName, valueStr)) {
      cerr << "Invalid option value in description \"" << optionDescription << '"' << endl;
      continue;
    }
    OpalMediaFormat::SetRegisteredMediaFormat(mediaFormat);
    cout << "Set option \"" << optionName << "\" to \"" << valueStr << "\" in \"" << mediaFormat << '"' << endl;
  }

  if (args.HasOption('f')) {
    Find(args[0]);
    if (discoveredRTPMap.size() == 0) {
      cerr << "error: no RTP sessions found" << endl;
      return;
    }
    cout << "Found " << discoveredRTPMap.size() << " sessions:\n" << endl;
    DisplaySessions();
    return;
  }

  if (!args.HasOption('m')) {
    Find(args[0]);
    if (discoveredRTPMap.size() == 0) {
      cerr << "error: no RTP sessions found - please use -m/-S/-D option to specify session manually" << endl;
      return;
    }

    PString selected = args.GetOptionString("session");
    size_t num;
    if (!selected.IsEmpty()) {
      DisplaySessions(false);
      num = atoi(selected);
      if (num <= 0 || num > discoveredRTPMap.size()*2) {
        cout << "Session " << num << " is not valid" << endl;
        return;
      }
    }
    else {
      cout << "Select one of the following sessions:\n" << endl;
      DisplaySessions();

      for (;;) {
        cout << "Select (1-" << discoveredRTPMap.size()*2 << ") ? " << flush;
        PString line;
        cin >> line;
        line = line.Trim();
        num = line.AsUnsigned();
        if (num > 0 && num <= discoveredRTPMap.size()*2)
          break;
        cout << "Session " << num << " is not valid" << endl;
      }
    }

    DiscoveredRTPMap::iterator r;
    bool found = false;
    for (r = discoveredRTPMap.begin(); r != discoveredRTPMap.end(); ++r) {
      DiscoveredRTPInfo & info = r->second;
      if (info.m_index[0] == num) {
        OpalMediaFormat mf = info.m_format[0];
        mf.SetPayloadType(info.m_payload[0]);
        m_payloadType2mediaFormat[info.m_payload[0]] = mf;
        m_srcIP = info.m_addr[0].GetAddress();
        m_dstIP = info.m_addr[1].GetAddress();
        m_srcPort = info.m_addr[0].GetPort();
        m_dstPort = info.m_addr[1].GetPort();
        found = true;
      }
      else if (info.m_index[1] == num) {
        OpalMediaFormat mf = info.m_format[1];
        mf.SetPayloadType(info.m_payload[1]);
        m_payloadType2mediaFormat[info.m_payload[1]] = mf;
        m_srcIP = info.m_addr[1].GetAddress();
        m_dstIP = info.m_addr[0].GetAddress();
        m_srcPort = info.m_addr[1].GetPort();
        m_dstPort = info.m_addr[0].GetPort();
        found = true;
      }
    }
    if (!found) {
      cout << "Session " << num << " not valid" << endl;
      return;
    }
  }

  else {
    OpalMediaFormatList list = OpalMediaFormat::GetAllRegisteredMediaFormats();
    for (PINDEX i = 0; i < list.GetSize(); i++) {
      if (list[i].GetPayloadType() < RTP_DataFrame::DynamicBase)
        m_payloadType2mediaFormat[list[i].GetPayloadType()] = list[i];
    }

    PStringArray mappings = args.GetOptionString('m').Lines();
    for (PINDEX i = 0; i < mappings.GetSize(); i++) {
      const PString & mapping = mappings[i];
      PINDEX equal = mapping.Find('=');
      if (equal == P_MAX_INDEX) {
        cout << "Invalid syntax for mapping \"" << mapping << '"' << endl;
        continue;
      }

      RTP_DataFrame::PayloadTypes pt = (RTP_DataFrame::PayloadTypes)mapping.Left(equal).AsUnsigned();
      if (pt > RTP_DataFrame::MaxPayloadType) {
        cout << "Invalid payload type for mapping \"" << mapping << '"' << endl;
        continue;
      }

      OpalMediaFormat mf = mapping.Mid(equal+1);
      if (!mf.IsTransportable()) {
        cout << "Invalid media format for mapping \"" << mapping << '"' << endl;
        continue;
      }

      mf.SetPayloadType(pt);
      m_payloadType2mediaFormat[pt] = mf;
    }

    m_srcIP = args.GetOptionString('S', m_srcIP.AsString());
    m_dstIP = args.GetOptionString('D', m_dstIP.AsString());

    m_srcPort = PIPSocket::GetPortByService("udp", args.GetOptionString('s'));
    m_dstPort = PIPSocket::GetPortByService("udp", args.GetOptionString('d', "5000"));
  }

  if (args.HasOption('E')) {
    if (!m_eventLog.Open(args.GetOptionString('E'), PFile::WriteOnly)) {
      cerr << "cannot open event log file '" << args.GetOptionString('E') << "'" << endl;
      return;
    }
    m_eventLog << "Event log created " << PTime().AsString() << " from " << args[0] << endl;
    m_eventLog << "Decoding following streams from " << m_srcIP << ":" << m_srcPort << " to " << m_dstIP << ":" << m_dstPort << endl;
    for (std::map<RTP_DataFrame::PayloadTypes, OpalMediaFormat>::iterator r = m_payloadType2mediaFormat.begin(); r != m_payloadType2mediaFormat.end(); ++r) {
      m_eventLog << "  " << r->second << ", payload type " << (unsigned int)r->second.GetPayloadType() << endl;
    }
    m_eventLog << endl;
  }

  m_singleStep = args.HasOption('p');
  m_info       = args.GetOptionCount('i');

  if (args.HasOption('P')) {
    if (!m_payloadFile.Open(args.GetOptionString('P'), PFile::ReadWrite)) {
      cerr << "Cannot create \"" << m_payloadFile.GetFilePath() << '\"' << endl;
      return;
    }
    cout << "Dumping RTP payload to \"" << m_payloadFile.GetFilePath() << '\"' << endl;
  }

  if (args.HasOption('Y')) {
    PFilePath yuvFileName = args.GetOptionString('Y');
    m_extraText = yuvFileName.GetFileName();

    if (yuvFileName.GetType() != ".yuv") {
      m_encodedFileName = yuvFileName;
      yuvFileName += ".yuv";
    }

    if (!m_yuvFile.Open(yuvFileName, PFile::ReadWrite)) {
      cerr << "Cannot create '" << yuvFileName << "'" << endl;
      return;
    }
    cout << "Writing video to \"" << yuvFileName << '\"' << endl;
  }

  if (m_extendedInfo) {
    if (args.HasOption('T'))
      m_extraText = args.GetOptionString('T').Trim();
    m_extraHeight = g_extraHeight + ((m_extraText.GetLength() == 0) ? 0 : 17);
  }

  // Audio player
  {
    PString driverName = args.GetOptionString('A');
    PString deviceName = args.GetOptionString('a');
    m_player = PSoundChannel::CreateOpenedChannel(driverName, deviceName, PSoundChannel::Player);
    if (m_player == NULL) {
      PStringList devices = PSoundChannel::GetDriversDeviceNames("*", PSoundChannel::Player);
      if (devices.IsEmpty()) {
        cerr << "No audio devices in the system!" << endl;
        return;
      }

      if (!driverName.IsEmpty() || !deviceName.IsEmpty()) {
        cerr << "Cannot use ";
        if (driverName.IsEmpty() && deviceName.IsEmpty())
          cerr << "default ";
        cerr << "audio player";
        if (!driverName)
          cerr << ", driver \"" << driverName << '"';
        if (!deviceName)
          cerr << ", device \"" << deviceName << '"';
        cerr << ", must be one of:\n";
        for (PINDEX i = 0; i < devices.GetSize(); i++)
          cerr << "   " << devices[i] << '\n';
        cerr << endl;
        return;
      }

      PStringList::iterator it = devices.begin();
      while ((m_player = PSoundChannel::CreateOpenedChannel(driverName, *it, PSoundChannel::Player)) == NULL) {
        cerr << "Cannot use audio device \"" << *it << '"' << endl;
        if (++it == devices.end()) {
          cerr << "Unable to find an available sound device." << endl;
          return;
        }
      }
    }

    cout << "Audio Player ";
    if (!driverName.IsEmpty())
      cout << "driver \"" << driverName << "\" and ";
    cout << "device \"" << m_player->GetName() << "\" opened." << endl;
  }

  // Video display
  PString driverName = args.GetOptionString('V');
  PString deviceName = args.GetOptionString('v');
  m_display = PVideoOutputDevice::CreateOpenedDevice(driverName, deviceName, FALSE);
  if (m_display == NULL) {
    cerr << "Cannot use ";
    if (driverName.IsEmpty() && deviceName.IsEmpty())
      cerr << "default ";
    cerr << "video display";
    if (!driverName)
      cerr << ", driver \"" << driverName << '"';
    if (!deviceName)
      cerr << ", device \"" << deviceName << '"';
    cerr << ", must be one of:\n";
    PStringList devices = PVideoOutputDevice::GetDriversDeviceNames("*");
    for (PINDEX i = 0; i < devices.GetSize(); i++)
      cerr << "   " << devices[i] << '\n';
    cerr << endl;
    return;
  }

  m_display->SetColourFormatConverter(OpalYUV420P);

  cout << "Display ";
  if (!driverName.IsEmpty())
    cout << "driver \"" << driverName << "\" and ";
  cout << "device \"" << m_display->GetDeviceName() << "\" opened." << endl;

  for (PINDEX i = 0; i < args.GetCount(); i++)
    Play(args[i]);
}


struct pcap_hdr_s { 
  DWORD magic_number;   /* magic number */
  WORD  version_major;  /* major version number */
  WORD  version_minor;  /* minor version number */
  DWORD thiszone;       /* GMT to local correction */
  DWORD sigfigs;        /* accuracy of timestamps */
  DWORD snaplen;        /* max length of captured packets, in octets */
  DWORD network;        /* data link type */
} pcap_hdr;

struct pcaprec_hdr_s { 
    DWORD ts_sec;         /* timestamp seconds */
    DWORD ts_usec;        /* timestamp microseconds */
    DWORD incl_len;       /* number of octets of packet saved in file */
    DWORD orig_len;       /* actual length of packet */
} pcaprec_hdr;


void PlayRTP::Find(const PFilePath & filename)
{
  PFile pcap;
  if (!pcap.Open(filename, PFile::ReadOnly)) {
    cout << "Could not open file \"" << filename << '"' << endl;
    return;
  }

  if (!pcap.Read(&pcap_hdr, sizeof(pcap_hdr))) {
    cout << "Could not read header from \"" << filename << '"' << endl;
    return;
  }

  bool fileOtherEndian;
  if (pcap_hdr.magic_number == 0xa1b2c3d4)
    fileOtherEndian = false;
  else if (pcap_hdr.magic_number == 0xd4c3b2a1)
    fileOtherEndian = true;
  else {
    cout << "File \"" << filename << "\" is not a PCAP file, bad magic number." << endl;
    return;
  }

  if (fileOtherEndian) {
    REVERSE(pcap_hdr.version_major);
    REVERSE(pcap_hdr.version_minor);
    REVERSE(pcap_hdr.thiszone);
    REVERSE(pcap_hdr.sigfigs);
    REVERSE(pcap_hdr.snaplen);
    REVERSE(pcap_hdr.network);
  }

  cout << "Analysing PCAP v" << pcap_hdr.version_major << '.' << pcap_hdr.version_minor << " file \"" << filename << '"' << endl;

  PBYTEArray packetData(pcap_hdr.snaplen); // Every packet is smaller than this
  PBYTEArray fragments;

  while (!pcap.IsEndOfFile()) {

    if (!pcap.Read(&pcaprec_hdr, sizeof(pcaprec_hdr))) {
      cout << "Truncated file \"" << filename << '"' << endl;
      return;
    }

    if (fileOtherEndian) {
      REVERSE(pcaprec_hdr.ts_sec);
      REVERSE(pcaprec_hdr.ts_usec);
      REVERSE(pcaprec_hdr.incl_len);
      REVERSE(pcaprec_hdr.orig_len);
    }

    if (!pcap.Read(packetData.GetPointer(pcaprec_hdr.incl_len), pcaprec_hdr.incl_len)) {
      cout << "Truncated file \"" << filename << '"' << endl;
      return;
    }

    const BYTE * packet = packetData;
    switch (pcap_hdr.network) {
      case 1 :
        if (*(PUInt16b *)(packet+12) != 0x800)
          continue; // Not IP, next packet

        packet += 14; // Skip Data Link Layer Header
        break;

      default :
        cout << "Unsupported Data Link Layer in file \"" << filename << '"' << endl;
        return;
    }

    // Check for fragmentation bit
    packet += 6;
    bool isFragment = (*packet & 0x20) != 0;
    int fragmentOffset = (((packet[0]&0x1f)<<8)+packet[1])*8;

    // Skip first bit of IP header
    packet += 3;
    if (*packet != 0x11)
      continue; // Not UDP

    PIPSocketAddressAndPort rtpSrc;
    PIPSocketAddressAndPort rtpDst;

    packet += 3;
    rtpSrc.SetAddress(PIPSocket::Address(4, packet));
    //if (!m_srcIP.IsAny() && m_srcIP != PIPSocket::Address(4, packet))
    //  continue; // Not specified source IP address

    packet += 4;
    rtpDst.SetAddress(PIPSocket::Address(4, packet));
    //if (!m_dstIP.IsAny() && m_dstIP != PIPSocket::Address(4, packet))
    //  continue; // Not specified destination IP address

    // On to the UDP header
    packet += 4;

    // As we are past IP header, handle fragmentation now
    PINDEX fragmentsSize = fragments.GetSize();
    if (isFragment || fragmentsSize > 0) {
      if (fragmentsSize != fragmentOffset) {
        cout << "Missing IP fragment in \"" << filename << '"' << endl;
        fragments.SetSize(0);
        continue;
      }

      fragments.Concatenate(PBYTEArray(packet, pcaprec_hdr.incl_len - (packet - packetData), false));

      if (isFragment)
        continue;

      packetData = fragments;
      pcaprec_hdr.incl_len = packetData.GetSize();
      fragments.SetSize(0);
      packet = packetData;
    }

    // Check UDP ports
    //if (m_srcPort != 0 && m_srcPort != *(PUInt16b *)packet)
    //  continue;
    rtpSrc.SetPort(*(PUInt16b *)packet);

    packet += 2;
    //if (m_dstPort != 0 && m_dstPort != *(PUInt16b *)packet)
    //  continue;
    rtpDst.SetPort(*(PUInt16b *)packet);

    // On to (probably) RTP header
    packet += 6;

    // see if this is an RTP packet
    int rtpLen = pcaprec_hdr.incl_len - (packet - packetData);

    // must be at least this long
    if (rtpLen < RTP_DataFrame::MinHeaderSize)
      continue;

    // must have version number 2
    RTP_DataFrame rtp(packet, rtpLen, FALSE);
    if (rtp.GetVersion() != 2)
      continue;

    // determine if reverse or forward session
    bool reverse;
    if (rtpSrc.GetAddress() != rtpDst.GetAddress())
      reverse = rtpSrc.GetAddress() > rtpDst.GetAddress();
    else
      reverse = rtpSrc.GetPort() > rtpDst.GetPort();

    PString key;
    if (reverse)
      key = rtpDst.AsString() + "|" + rtpSrc.AsString();
    else
      key = rtpSrc.AsString() + "|" + rtpDst.AsString();

    std::string k(key);

    // see if we have identified this potential session before
    DiscoveredRTPMap::iterator r;
    if ((r = discoveredRTPMap.find(k)) == discoveredRTPMap.end()) {
      DiscoveredRTPInfo info;
      int dir = reverse ? 1 : 0;
      info.m_found  [dir]  = true;
      info.m_addr[dir]     = rtpSrc;
      info.m_addr[1 - dir] = rtpDst;
      info.m_payload[dir]  = rtp.GetPayloadType();
      info.m_seq[dir]      = rtp.GetSequenceNumber();
      info.m_ts[dir]       = rtp.GetTimestamp();

      info.m_ssrc[dir]     = rtp.GetSyncSource();
      info.m_seq[dir]      = rtp.GetSequenceNumber();
      info.m_ts[dir]       = rtp.GetTimestamp();

      info.m_firstFrame[dir] = new RTP_DataFrame(rtp.GetPointer(), rtp.GetSize());

      discoveredRTPMap.insert(DiscoveredRTPMap::value_type(k, info));
    }
    else {
      DiscoveredRTPInfo & info = r->second;
      int dir = reverse ? 1 : 0;
      if (!info.m_found[dir]) {
        info.m_found  [dir]  = true;
        info.m_addr[dir]     = rtpSrc;
        info.m_addr[1 - dir] = rtpDst;
        info.m_payload[dir]  = rtp.GetPayloadType();
        info.m_seq[dir]      = rtp.GetSequenceNumber();
        info.m_ts[dir]       = rtp.GetTimestamp();

        info.m_ssrc[dir]     = rtp.GetSyncSource();
        info.m_seq[dir]      = rtp.GetSequenceNumber();
        info.m_ts[dir]       = rtp.GetTimestamp();

        info.m_firstFrame[dir] = new RTP_DataFrame(rtp.GetPointer(), rtp.GetSize());
      }
      else
      {
        WORD seq = rtp.GetSequenceNumber();
        DWORD ts = rtp.GetTimestamp();
        DWORD ssrc = rtp.GetSyncSource();
        if (info.m_ssrc[dir] == ssrc)
          ++info.m_ssrc_matches[dir];
        if ((info.m_seq[dir]+1) == seq)
          ++info.m_seq_matches[dir];
        info.m_seq[dir] = seq;
        if ((info.m_ts[dir]+1) < ts)
          ++info.m_ts_matches[dir];
        info.m_ts[dir] = ts;
      }
    }
  }

  // display matches
  DiscoveredRTPMap::iterator r = discoveredRTPMap.begin();
  while (r != discoveredRTPMap.end()) {
    DiscoveredRTPInfo & info = r->second;
    if (
        (info.m_found[0] && (
         info.m_seq_matches[0] > 5 ||
         info.m_ts_matches[0] > 5 ||
         info.m_ssrc_matches[0] > 5
         )
         ) 
         ||
        (info.m_found[1] && (
         info.m_seq_matches[1] > 5 ||
         info.m_ts_matches[1] > 5 ||
         info.m_ssrc_matches[1] > 5
         )
         ) 
         )
    {
        ++r;
    }
    else {
      discoveredRTPMap.erase(r);
      r = discoveredRTPMap.begin();
    }
  }
}

#if 0

static void DrawText(unsigned x, unsigned y, unsigned frameWidth, unsigned frameHeight, BYTE * frame, const char * text)
{
  BYTE * output  = frame + ((y * frameWidth) + x) * 3/2;
  unsigned uoffs = (frameWidth * frameHeight) / 4;

  while (*text != '\0') {
    const PVideoFont::LetterData * letter = PVideoFont::GetLetterData(*text++);
    if (letter != NULL) {
      for (PINDEX y = 0; y < PVideoFont::MAX_L_HEIGHT; ++y) {
        BYTE * outputLine0 = output + (y+0) * frameWidth*2;
        BYTE * outputLine1 = outputLine0 + frameWidth;
        const char * line;
        for (line = letter->line[y]; *line != '\0'; ++line) {
          outputLine0[0]       = outputLine0[1]       = outputLine1[0]       = outputLine1[1]       = (*line == ' ') ? 16 : 240;
          outputLine0[uoffs*2] = outputLine0[uoffs*3] = outputLine1[uoffs*2] = outputLine1[uoffs*3] = 0x80;
          outputLine0 += 2;
          outputLine1 += 2;
        }
      }
      output += 1 + strlen(letter->line[0]) * 2;
    }
  }
}

#else

static void DrawText(unsigned x, unsigned y, unsigned frameWidth, unsigned frameHeight, BYTE * frame, const char * text)
{
  BYTE * output  = frame + ((y * frameWidth) + x);
  unsigned uoffs = (frameWidth * frameHeight) / 4;

  while (*text != '\0') {
    const PVideoFont::LetterData * letter = PVideoFont::GetLetterData(*text++);
    if (letter != NULL) {
      for (PINDEX y = 0; y < PVideoFont::MAX_L_HEIGHT; ++y) {
        BYTE * Y = output + y * frameWidth;
        const char * line = letter->line[y];
        {
          int UVwidth = (strlen(line)+2)/2;
          memset(Y + uoffs*2, 0x80, UVwidth);
          memset(Y + uoffs*2, 0x80, UVwidth);
        }
        while (*line != '\0')
          *Y++ = (*line++ == ' ') ? 16 : 240;
      }
      output += 1 + strlen(letter->line[0]);
    }
  }
}

#endif

void PlayRTP::Play(const PFilePath & filename)
{
  PFile pcap;
  if (!pcap.Open(filename, PFile::ReadOnly)) {
    cout << "Could not open file \"" << filename << '"' << endl;
    return;
  }

  if (!pcap.Read(&pcap_hdr, sizeof(pcap_hdr))) {
    cout << "Could not read header from \"" << filename << '"' << endl;
    return;
  }

  bool fileOtherEndian;
  if (pcap_hdr.magic_number == 0xa1b2c3d4)
    fileOtherEndian = false;
  else if (pcap_hdr.magic_number == 0xd4c3b2a1)
    fileOtherEndian = true;
  else {
    cout << "File \"" << filename << "\" is not a PCAP file, bad magic number." << endl;
    return;
  }

  if (fileOtherEndian) {
    REVERSE(pcap_hdr.version_major);
    REVERSE(pcap_hdr.version_minor);
    REVERSE(pcap_hdr.thiszone);
    REVERSE(pcap_hdr.sigfigs);
    REVERSE(pcap_hdr.snaplen);
    REVERSE(pcap_hdr.network);
  }

  cout << "Playing PCAP v" << pcap_hdr.version_major << '.' << pcap_hdr.version_minor << " file \"" << filename << '"' << endl;

  PBYTEArray packetData(pcap_hdr.snaplen); // Every packet is smaller than this
  PBYTEArray fragments;

  RTP_DataFrame::PayloadTypes rtpStreamPayloadType = RTP_DataFrame::IllegalPayloadType;
  RTP_DataFrame::PayloadTypes lastUnsupportedPayloadType = RTP_DataFrame::IllegalPayloadType;
  DWORD lastTimeStamp = 0;

  m_fragmentationCount = 0;
  m_packetCount = 0;

  RTP_DataFrame extendedData;
  m_videoError = false;
  m_videoFrames = 0;

  bool isAudio = false;
  bool needInfoHeader = true;

  PTimeInterval playStartTick = PTimer::Tick();

  while (!pcap.IsEndOfFile()) {
    if (!pcap.Read(&pcaprec_hdr, sizeof(pcaprec_hdr))) {
      cout << "Truncated file \"" << filename << '"' << endl;
      return;
    }

    if (fileOtherEndian) {
      REVERSE(pcaprec_hdr.ts_sec);
      REVERSE(pcaprec_hdr.ts_usec);
      REVERSE(pcaprec_hdr.incl_len);
      REVERSE(pcaprec_hdr.orig_len);
    }

    if (!pcap.Read(packetData.GetPointer(pcaprec_hdr.incl_len), pcaprec_hdr.incl_len)) {
      cout << "Truncated file \"" << filename << '"' << endl;
      return;
    }

    const BYTE * packet = packetData;
    switch (pcap_hdr.network) {
      case 1 :
        if (*(PUInt16b *)(packet+12) != 0x800)
          continue; // Not IP, next packet

        packet += 14; // Skip Data Link Layer Header
        break;

      default :
        cout << "Unsupported Data Link Layer in file \"" << filename << '"' << endl;
        return;
    }

    // Check for fragmentation bit
    packet += 6;
    bool isFragment = (*packet & 0x20) != 0;
    int fragmentOffset = (((packet[0]&0x1f)<<8)+packet[1])*8;

    // Skip first bit of IP header
    packet += 3;
    if (*packet != 0x11)
      continue; // Not UDP

    packet += 3;
    if (!m_srcIP.IsAny() && m_srcIP != PIPSocket::Address(4, packet))
      continue; // Not specified source IP address

    packet += 4;
    if (!m_dstIP.IsAny() && m_dstIP != PIPSocket::Address(4, packet))
      continue; // Not specified destination IP address

    // On to the UDP header
    packet += 4;

    // As we are past IP header, handle fragmentation now
    PINDEX fragmentsSize = fragments.GetSize();
    if (isFragment || fragmentsSize > 0) {
      if (fragmentsSize != fragmentOffset) {
        cout << "Missing IP fragment in \"" << filename << '"' << endl;
        fragments.SetSize(0);
        continue;
      }

      fragments.Concatenate(PBYTEArray(packet, pcaprec_hdr.incl_len - (packet - packetData), false));

      if (isFragment)
        continue;

      packetData = fragments;
      pcaprec_hdr.incl_len = packetData.GetSize();
      fragments.SetSize(0);
      packet = packetData;
    }

    // Check UDP ports
    if (m_srcPort != 0 && m_srcPort != *(PUInt16b *)packet)
      continue;

    packet += 2;
    if (m_dstPort != 0 && m_dstPort != *(PUInt16b *)packet)
      continue;

    // On to (probably) RTP header
    packet += 6;
    RTP_DataFrame rtp(packet, pcaprec_hdr.incl_len - (packet - packetData), FALSE);

    if (rtp.GetVersion() != 2)
      continue;

    if (rtpStreamPayloadType != rtp.GetPayloadType()) {
      if (rtpStreamPayloadType != RTP_DataFrame::IllegalPayloadType) {
        cout << "Payload type changed in mid file \"" << filename << '"' << endl;
        continue;
      }
      rtpStreamPayloadType = rtp.GetPayloadType();
    }

    if (m_transcoder == NULL) {
      if (m_payloadType2mediaFormat.find(rtpStreamPayloadType) == m_payloadType2mediaFormat.end()) {
        if (lastUnsupportedPayloadType != rtpStreamPayloadType) {
          cout << "Unsupported Payload Type " << rtpStreamPayloadType << " in file \"" << filename << '"' << endl;
          lastUnsupportedPayloadType = rtpStreamPayloadType;
        }
        rtpStreamPayloadType = RTP_DataFrame::IllegalPayloadType;
        continue;
      }

      OpalMediaFormat srcFmt = m_payloadType2mediaFormat[rtpStreamPayloadType];
      OpalMediaFormat dstFmt;
      if (srcFmt.GetMediaType() == OpalMediaType::Audio()) {
        dstFmt = OpalPCM16;
        m_noDelay = true; // Will be paced by output device.
        isAudio = true;
        unsigned frame = srcFmt.GetFrameTime();
        if (frame < 160)
          frame = 160;
        m_player->SetBuffers(frame*2, 2000/frame); // 250ms of buffering of Vista goes funny
      }
      else if (srcFmt.GetMediaType() == OpalMediaType::Video()) {
        dstFmt = OpalYUV420P;
        m_display->Start();
      }
      else {
        cout << "Unsupported Media Type " << srcFmt.GetMediaType() << " in file \"" << filename << '"' << endl;
        return;
      }

      m_transcoder = OpalTranscoder::Create(srcFmt, dstFmt);
      if (m_transcoder == NULL) {
        cout << "No transcoder for " << srcFmt << " in file \"" << filename << '"' << endl;
        return;
      }

      cout << "Decoding " << srcFmt << " from file \"" << filename << '"' << endl;
      m_transcoder->SetCommandNotifier(PCREATE_NOTIFIER(OnTranscoderCommand));
      lastTimeStamp = rtp.GetTimestamp();
    }

    const OpalMediaFormat & inputFmt = m_transcoder->GetInputFormat();

    if (inputFmt == "H.264") {
      static BYTE const StartCode[4] = { 0, 0, 0, 1 };
      m_payloadFile.Write(StartCode, sizeof(StartCode));
    }
    m_payloadFile.Write(rtp.GetPayloadPtr(), rtp.GetPayloadSize());

    if (!m_noDelay) {
      if (rtp.GetTimestamp() != lastTimeStamp) {
        unsigned msecs = (rtp.GetTimestamp() - lastTimeStamp)/inputFmt.GetTimeUnits();
        if (msecs < 3000) 
          PThread::Sleep(msecs);
        else 
          cout << "ignoring timestamp jump > 3 seconds" << endl;
        lastTimeStamp = rtp.GetTimestamp();
      }
    }

    if (fragmentsSize > 0)
      ++m_fragmentationCount;

    if (m_info > 0) {
      if (needInfoHeader) {
        needInfoHeader = false;
        if (m_info > 0) {
          if (m_info > 1)
            cout << "Packet,RealTime,CaptureTime,";
          cout << "SSRC,SequenceNumber,TimeStamp";
          if (m_info > 1) {
            cout << ",Marker,PayloadType,payloadSize";
            if (isAudio)
              cout << ",DecodedSize";
            else
              cout << ",Width,Height";
            if (m_info > 2)
              cout << ",Data";
          }
          cout << '\n';
        }
      }

      if (m_info > 1)
        cout << m_packetCount << ','
             << PTimer::Tick().GetMilliSeconds() << ','
             << pcaprec_hdr.ts_sec << '.' << setfill('0') << setw(6) << pcaprec_hdr.ts_usec << setfill(' ') << ',';
      cout << "0x" << hex << rtp.GetSyncSource() << dec << ',' << rtp.GetSequenceNumber() << ',' << rtp.GetTimestamp();
      if (m_info > 1)
        cout << ',' << rtp.GetMarker() << ',' << rtp.GetPayloadType() << ',' << rtp.GetPayloadSize();
    }

    if (m_singleStep) 
      cout << "Input packet of length " << rtp.GetPayloadSize() << (rtp.GetMarker() ? " with MARKER" : "") << " -> ";

    m_vfu = false;
    RTP_DataFrameList output;
    if (!m_transcoder->ConvertFrames(rtp, output)) {
      cout << "Error decoding file \"" << filename << '"' << endl;
      return;
    }

    if (output.GetSize() == 0) {
      if (m_singleStep) 
        cout << "no frame" << endl;
    }
    else {
      if (m_singleStep) {
        cout << output.GetSize() << " packets" << endl;
        char ch;
        cin >> ch;
        if (ch == 'c')
          m_singleStep = false;
      }

      for (PINDEX i = 0; i < output.GetSize(); i++) {
        const RTP_DataFrame & data = output[i];
        if (isAudio) {
          m_player->Write(data.GetPayloadPtr(), data.GetPayloadSize());
          if (m_info > 1)
            cout << ',' << data.GetPayloadSize();
        }
        else {
          ++m_videoFrames;

          OpalVideoTranscoder * video = (OpalVideoTranscoder *)m_transcoder;
          const OpalVideoTranscoder::FrameHeader * frame = (const OpalVideoTranscoder::FrameHeader *)data.GetPayloadPtr();
          if (video->WasLastFrameIFrame()) {
            m_eventLog << "Frame " << m_videoFrames << ": I-frame received";
            if (m_videoError)
              m_eventLog << " - decode error cleared";
            m_eventLog << endl;
            m_videoError = false;
          }

          m_yuvFile.SetFrameSize(frame->width, frame->height + (m_extendedInfo ? m_extraHeight : 0));

          if (!m_extendedInfo) {
            m_display->SetFrameSize(frame->width, frame->height);
            m_display->SetFrameData(frame->x, frame->y,
                                    frame->width, frame->height,
                                    OPAL_VIDEO_FRAME_DATA_PTR(frame), data.GetMarker());
            m_yuvFile.WriteFrame(OPAL_VIDEO_FRAME_DATA_PTR(frame));
          }
          else {
            int extendedHeight = frame->height + m_extraHeight;
            extendedData.SetSize(data.GetHeaderSize() + sizeof(OpalVideoTranscoder::FrameHeader) + (frame->width * extendedHeight * 3 / 2));
            memcpy(extendedData.GetPointer(), (const BYTE *)data, data.GetHeaderSize());
            OpalVideoTranscoder::FrameHeader * extendedFrame = (OpalVideoTranscoder::FrameHeader *)extendedData.GetPayloadPtr();
            *extendedFrame = *frame;
            extendedFrame->height = extendedHeight;

            PColourConverter::FillYUV420P(0,  0,                extendedFrame->width, extendedFrame->height, 
                                          extendedFrame->width, extendedFrame->height,  OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame), 
                                          0, 0, 0);

            char text[60];
            sprintf(text, "Seq:%08u  Ts:%08u",
                           rtp.GetSequenceNumber(),
                           rtp.GetTimestamp());
            DrawText(4, 4, extendedFrame->width, extendedFrame->height, OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame), text);

            sprintf(text, "TC:%06u  %c %c %c", 
                           m_videoFrames, 
                           m_vfu ? 'V' : ' ', 
                           video->WasLastFrameIFrame() ? 'I' : ' ', 
                           m_videoError ? 'E' : ' ');
            DrawText(4, 20, extendedFrame->width, extendedFrame->height, OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame), text);

            if (m_extraText.GetLength() > 0) 
              DrawText(4, 37, extendedFrame->width, extendedFrame->height, OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame), m_extraText);

            PColourConverter::CopyYUV420P(0, 0,           frame->width,  frame->height, frame->width,         frame->height,         OPAL_VIDEO_FRAME_DATA_PTR(frame),
                                          0, m_extraHeight, frame->width,  frame->height, extendedFrame->width, extendedFrame->height, OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame), 
                                          PVideoFrameInfo::eCropTopLeft);

            m_display->SetFrameSize(extendedFrame->width, extendedFrame->height);
            m_display->SetFrameData(extendedFrame->x,     extendedFrame->y,
                                    extendedFrame->width, extendedFrame->height,
                                    OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame), data.GetMarker());

            m_yuvFile.WriteFrame(OPAL_VIDEO_FRAME_DATA_PTR(extendedFrame));
          }
          //if (m_vfu)
          //  m_singleStep = true;

          if (m_info > 1)
            cout << ',' << frame->width << ',' << frame->height;
        }
      }
    }

    if (m_info > 0) {
      if (m_info > 2) {
        PINDEX psz = rtp.GetPayloadSize();
        if (m_info == 2 && psz > 30)
          psz = 30;
        cout << ',' << hex << setfill('0');
        const BYTE * ptr = rtp.GetPayloadPtr();
        for (PINDEX i = 0; i < psz; ++i)
          cout << setw(2) << (unsigned)*ptr++;
        cout << dec << setfill(' ');
      }
      cout << '\n';
    }

    ++m_packetCount;

    if (m_packetCount % 250 == 0)
      m_eventLog << "Packet " << m_packetCount << ": still processing" << endl;
  }

  m_eventLog << "Packet " << m_packetCount << ": completed" << endl;

  delete m_transcoder;
  m_transcoder = NULL;


  // Output final stats.
  cout << (m_yuvFile.IsOpen() ? "Written " : "Played ") << m_packetCount << " packets";

  if (m_videoFrames > 0) {
    cout << ", " << m_videoFrames << " frames at "
         << m_yuvFile.GetFrameWidth() << "x" << m_yuvFile.GetFrameHeight();
    if (!m_yuvFile.IsOpen()) {
      PTimeInterval playTime = PTimer::Tick() - playStartTick;
      cout << ", " << playTime << " seconds, "
           << fixed << setprecision(1)
           << (m_videoFrames*1000.0/playTime.GetMilliSeconds()) << "fps";
    }
  }

  if (m_fragmentationCount > 0)
    cout << ", " << m_fragmentationCount << " fragments";

  cout << endl;


  if (!m_encodedFileName.IsEmpty()) {
    PStringStream args; 
    args << "ffmpeg -r 10 -y -s " << m_yuvFile.GetFrameWidth() << 'x' << m_yuvFile.GetFrameHeight()
         << " -i '" << m_yuvFile.GetFilePath() << "' '" << m_encodedFileName << '\'';
    cout << "Executing command \"" << args << '\"' << endl;
    PPipeChannel cmd;
    if (!cmd.Open(args, PPipeChannel::ReadWriteStd, true)) 
      cout << "failed";
    cmd.Execute();
    cmd.WaitForTermination();
    cout << "done" << endl;
  }
}


void PlayRTP::OnTranscoderCommand(OpalMediaCommand & command, INT /*extra*/)
{
  if (PIsDescendant(&command, OpalVideoUpdatePicture)) {
    m_eventLog << "Packet " << m_packetCount << ", frame " << m_videoFrames << ": decoding error (VFU sent)";
      
    OpalVideoUpdatePicture2 * vfu2 = dynamic_cast<OpalVideoUpdatePicture2 *>(&command);
    if (vfu2 != NULL) {
      m_eventLog << " for seq " << vfu2->GetSequenceNumber() << ", ts " << vfu2->GetSequenceNumber();
    }
    m_eventLog << endl;
    cout << "Decoder error in received stream." << endl;
    m_videoError = m_vfu = true;
  }
}


// End of File ///////////////////////////////////////////////////////////////
