/*
 * main.cxx
 *
 * OPAL application source file for seing IM via SIP
 *
 * Copyright (c) 2008 Post Increment
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
 * $Revision: 23696 $
 * $Author: rjongbloed $
 * $Date: 2009-10-22 07:56:55 +0200 (Do, 22. Okt 2009) $
 */

#include "main.h"

#include <opal/pcss.h>
#include <im/rfc4103.h>
#include <opal/patch.h>

PCREATE_PROCESS(SipIM);

SipIM::SipIM()
  : PProcess("OPAL SIP IM", "SipIM", OPAL_MAJOR, OPAL_MINOR, ReleaseCode, OPAL_BUILD)
  , m_manager(NULL)
{
}


SipIM::~SipIM()
{
  delete m_manager;
}


void SipIM::Main()
{
  PArgList & args = GetArguments();

  args.Parse(
             "u-user:"
             "h-help."
             "-sipim."
             "-t140."
             "-msrp."
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

  if (args.HasOption('h')) {
    cerr << "usage: " << GetFile().GetTitle() << " [ options ] [url]\n"
            "\n"
            "Available options are:\n"
            "  -u or --user            : set local username.\n"
            "  --help                  : print this help message.\n"
#if OPAL_HAS_MSRP
            "  --msrp                  : use MSRP (default)\n"
#endif
#if OPAL_HAS_SIPIM
            "  --sipim                 : use SIPIM\n"
#endif
            "  --t140                  : use T.140\n"
#if PTRACING
            "  -o or --output file     : file name for output of log messages\n"       
            "  -t or --trace           : degree of verbosity in error log (more times for more detail)\n"     
#endif
            "\n"
            "e.g. " << GetFile().GetTitle() << " sip:fred@bloggs.com\n"
            ;
    return;
  }

  MyManager m_manager;

  if (args.HasOption('u'))
    m_manager.SetDefaultUserName(args.GetOptionString('u'));

  Mode mode;
  if (args.HasOption("t140")) {
    mode = Use_T140;
    m_manager.m_imFormat = OpalT140;
  }
#if OPAL_HAS_SIPIM
  else if (args.HasOption("sipim")) {
    mode = Use_SIPIM;
    m_manager.m_imFormat = OpalSIPIM;
  }
#endif
#if OPAL_HAS_MSRP
  else if (args.HasOption("msrp")) {
    mode = Use_MSRP;
    m_manager.m_imFormat = OpalMSRP;
  }
#endif
  else {
    cout << "error: must select IM mode" << endl;
    return;
  }

  OpalMediaFormatList allMediaFormats;

  SIPEndPoint * sip  = new SIPEndPoint(m_manager);
  if (!sip->StartListeners(PStringArray())) {
    cerr << "Could not start SIP listeners." << endl;
    return;
  }
  allMediaFormats += sip->GetMediaFormats();

  MyPCSSEndPoint * pcss = new MyPCSSEndPoint(m_manager);
  allMediaFormats += pcss->GetMediaFormats();
  pcss->SetSoundChannelPlayDevice("NULL");
  pcss->SetSoundChannelRecordDevice("NULL");

  allMediaFormats = OpalTranscoder::GetPossibleFormats(allMediaFormats); // Add transcoders
  for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
    if (!allMediaFormats[i].IsTransportable())
      allMediaFormats.RemoveAt(i--); // Don't show media formats that are not used over the wire
  }


  m_manager.AddRouteEntry("sip:.*\t.*=pc:*");
  m_manager.AddRouteEntry("pc:.*\t.*=sip:<da>");

  cout << "Available codecs: " << setfill(',') << allMediaFormats << setfill(' ') << endl;

  PString imFormatMask = PString("!") + (const char *)m_manager.m_imFormat;
  m_manager.SetMediaFormatMask(imFormatMask);
  allMediaFormats.Remove(imFormatMask);
  
  cout << "Codecs to be used: " << setfill(',') << allMediaFormats << setfill(' ') << endl;

  OpalConnection::StringOptions options;
  options.SetAt(OPAL_OPT_AUTO_START, m_manager.m_imFormat.GetMediaType() + ":exclusive");

  if (args.GetCount() == 0)
    cout << "Awaiting incoming call ..." << flush;
  else {
    if (!m_manager.SetUpCall("pc:", args[0], m_manager.m_callToken, NULL, 0, &options)) {
      cerr << "Could not start IM to \"" << args[0] << '"' << endl;
      return;
    }
  }

  m_manager.m_connected.Wait();

  RFC4103Context rfc4103(m_manager.m_imFormat);

  int count = 1;
  for (;;) {
    PThread::Sleep(5000);
    const char * textData = "Hello, world";

    if (count > 0) {
      PSafePtr<OpalCall> call = m_manager.FindCallWithLock(m_manager.m_callToken);
      if (call != NULL) {
        PSafePtr<OpalPCSSConnection> conn = call->GetConnectionAs<OpalPCSSConnection>();
        if (conn != NULL) {
          RTP_DataFrameList frameList = rfc4103.ConvertToFrames("text/plain", textData);
          OpalMediaFormat fmt(m_manager.m_imFormat);
          for (PINDEX i = 0; i < frameList.GetSize(); ++i) {
            conn->TransmitInternalIM(fmt, (RTP_IMFrame &)frameList[i]);
          }
        }
      }
      --count;
    }
  }

  // Wait for call to come in and finish
  m_manager.m_completed.Wait();
  cout << " completed.";
}

void MyManager::OnClearedCall(OpalCall & /*call*/)
{
  m_connected.Signal();
  m_completed.Signal();
}

void MyManager::OnApplyStringOptions(OpalConnection & conn, OpalConnection::StringOptions & options)
{
  options.SetAt(OPAL_OPT_AUTO_START, m_imFormat.GetMediaType() + ":exclusive");
  OpalManager::OnApplyStringOptions(conn, options);
}

PBoolean MyPCSSEndPoint::OnShowIncoming(const OpalPCSSConnection & connection)
{
  MyManager & mgr = (MyManager &)manager;
  mgr.m_callToken = connection.GetCall().GetToken();
  AcceptIncomingConnection(mgr.m_callToken);
  mgr.m_connected.Signal();
  cout << "Incoming call connected" << endl;

  return PTrue;
}


PBoolean MyPCSSEndPoint::OnShowOutgoing(const OpalPCSSConnection & connection)
{
  MyManager & mgr = (MyManager &)manager;

  cout << "Outgoing call connected" << endl;
  mgr.m_connected.Signal();
  return PTrue;
}


// End of File ///////////////////////////////////////////////////////////////
