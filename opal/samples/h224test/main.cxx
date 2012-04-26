/*
 * main.cxx
 *
 * OPAL application source file for testing H.224 sessions
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
 * $Revision: 22678 $
 * $Author: rjongbloed $
 * $Date: 2009-05-21 02:35:59 +0200 (Do, 21. Mai 2009) $
 */

#include "main.h"

#include <h224/h224.h>

#include <opal/pcss.h>
#include <opal/patch.h>

PCREATE_PROCESS(H224Test);

H224Test::H224Test()
  : PProcess("OPAL H.224 Test", "H224Test", OPAL_MAJOR, OPAL_MINOR, ReleaseCode, OPAL_BUILD)
  , m_manager(NULL)
{
}


H224Test::~H224Test()
{
  delete m_manager;
}


void H224Test::Main()
{
  PArgList & args = GetArguments();

  args.Parse(
             "u-user:"
             "h-help."
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

  m_manager.m_h224Format = OpalH224AnnexQ;

  OpalMediaFormatList allMediaFormats;

  SIPEndPoint * sip  = new SIPEndPoint(m_manager);
  if (!sip->StartListeners(PStringArray())) {
    cerr << "Could not start SIP listeners." << endl;
    return;
  }
  allMediaFormats += sip->GetMediaFormats();

  MyPCSSEndPoint * pcss = new MyPCSSEndPoint(m_manager);
  allMediaFormats += pcss->GetMediaFormats();

  allMediaFormats = OpalTranscoder::GetPossibleFormats(allMediaFormats); // Add transcoders
  for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
    if (!allMediaFormats[i].IsTransportable())
      allMediaFormats.RemoveAt(i--); // Don't show media formats that are not used over the wire
  }

  m_manager.AddRouteEntry("sip:.*  = pc:<du>");
  m_manager.AddRouteEntry("pc:.*   = sip:<da>");

  cout << "Available codecs: " << setfill(',') << allMediaFormats << setfill(' ') << endl;

  PString formatMask = PString("!") + (const char *)m_manager.m_h224Format;
  m_manager.SetMediaFormatMask(formatMask);
  allMediaFormats.Remove(formatMask);
  
  cout << "Codecs to be used: " << setfill(',') << allMediaFormats << setfill(' ') << endl;

  OpalConnection::StringOptions * options = new OpalConnection::StringOptions();
  options->SetAt("autostart", m_manager.m_h224Format.GetMediaType() + ":exclusive");

  if (args.GetCount() == 0)
    cout << "Awaiting incoming call ..." << flush;
  else {
    if (!m_manager.SetUpCall("pc:", args[0], m_manager.m_callToken, NULL, 0, options)) {
      cerr << "Could not start H.224 to \"" << args[0] << '"' << endl;
      return;
    }
  }

  m_manager.m_connected.Wait();

  for (;;) {
    PThread::Sleep(1000);
    PSafePtr<OpalCall> call = m_manager.FindCallWithLock(m_manager.m_callToken);
    if (call != NULL) {
      PSafePtr<OpalPCSSConnection> conn = call->GetConnectionAs<OpalPCSSConnection>();
      /*
      if (conn != NULL)
        conn->SendIM(m_manager.m_imFormat, T140String(textData));
        */
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
  options.SetAt("autostart", m_h224Format.GetMediaType() + ":exclusive");
  OpalManager::OnApplyStringOptions(conn, options);
}

PBoolean MyPCSSEndPoint::OnShowIncoming(const OpalPCSSConnection & connection)
{
  MyManager & mgr = (MyManager &)manager;
  mgr.m_callToken = connection.GetCall().GetToken();
  AcceptIncomingConnection(mgr.m_callToken);
  mgr.m_connected.Signal();
  cout << "Incoming call connected" << endl;

  //((OpalConnection &)connection).AddIMListener(PCREATE_NOTIFIER(OnReceiveIM));

  return PTrue;
}


PBoolean MyPCSSEndPoint::OnShowOutgoing(const OpalPCSSConnection & connection)
{
  MyManager & mgr = (MyManager &)manager;

  //((OpalConnection &)connection).AddIMListener(PCREATE_NOTIFIER(OnReceiveIM));

  cout << "Outgoing call connected" << endl;
  mgr.m_connected.Signal();
  return PTrue;
}

void MyPCSSEndPoint::OnReceiveIM(OpalConnection::IMInfo & im, INT)
{
  cout << "Received IM: " << im.body << endl;
}



// End of File ///////////////////////////////////////////////////////////////
