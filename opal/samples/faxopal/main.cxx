/*
 * main.cxx
 *
 * OPAL application source file for sending/receiving faxes via T.38
 *
 * Copyright (c) 2008 Vox Lucida Pty. Ltd.
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
 * $Revision: 23865 $
 * $Author: rjongbloed $
 * $Date: 2009-12-10 07:01:28 +0100 (Do, 10. Dez 2009) $
 */

#include "precompile.h"
#include "main.h"


// -ttttt c:\temp\testfax.tif sip:fax@10.0.1.11
// -ttttt c:\temp\incoming.tif

PCREATE_PROCESS(FaxOPAL);


FaxOPAL::FaxOPAL()
  : PProcess("OPAL T.38 Fax", "FaxOPAL", OPAL_MAJOR, OPAL_MINOR, ReleaseCode, OPAL_BUILD)
  , m_manager(NULL)
{
}


FaxOPAL::~FaxOPAL()
{
  delete m_manager;
}


void FaxOPAL::Main()
{
  PArgList & args = GetArguments();

  args.Parse("a-audio."
             "d-directory:"
             "g-gk-host:"
             "G-gk-id:"
             "h-help."
             "H-h323:"
             "I-inband-detect."
             "i-inband-send."
             "L-lines:"
             "m-mode:"
             "N-stun:"
             "p-password:"
             "P-proxy:"
             "r-register:"
             "S-sip:"
             "u-user:"
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
    cerr << "usage: " << GetFile().GetTitle() << " [ options ] filename [ url ]\n"
            "\n"
            "Available options are:\n"
            "  --help                  : print this help message.\n"
            "  -d or --directory dir   : Set default directory for fax receive\n"
            "  -a or --audio           : Send fax as G.711 audio\n"
            "  -u or --user name       : Set local username, defaults to OS username.\n"
            "  -p or --password pwd    : Set password for authentication.\n"
#if OPAL_SIP
            "  -S or --sip interface   : SIP listens on interface, defaults to udp$*:5060, 'x' disables.\n"
            "  -r or --register server : SIP registration to server.\n"
            "  -P or --proxy url       : SIP outbound proxy.\n"
#endif
#if OPAL_H323
            "  -H or --h323 interface  : H.323 listens on interface, defaults to tcp$*:1720, 'x' disables.\n"
            "  -g or --gk-host host    : H.323 gatekeeper host.\n"
            "  -G or --gk-id id        : H.323 gatekeeper identifier.\n"
#endif
            "  -L or --lines devices   : Set Line Interface Devices.\n"
            "  -N or --stun server     : Set NAT traversal STUN server.\n"
            "  -I or --inband-detect   : Disable detection of in-band tones.\n"
            "  -i or --inband-send     : Disable transmission of in-band tones.\n"
#if PTRACING
            "  -o or --output file     : file name for output of log messages\n"       
            "  -t or --trace           : degree of verbosity in error log (more times for more detail)\n"     
#endif
            "\n"
            "e.g. " << GetFile().GetTitle() << " send_fax.tif sip:fred@bloggs.com\n"
            "\n"
            "     " << GetFile().GetTitle() << " received_fax.tif\n\n";
    return;
  }

  m_manager = new MyManager();

  if (args.HasOption('u'))
    m_manager->SetDefaultUserName(args.GetOptionString('u'));

  PString prefix = args.HasOption('a') ? "fax" : "t38";

  // Create audio or T.38 fax endpoint.
  MyFaxEndPoint * fax  = new MyFaxEndPoint(*m_manager);
  if (args.HasOption('d'))
    fax->SetDefaultDirectory(args.GetOptionString('d'));


  PCaselessString interfaces;

  if (args.HasOption('N')) {
    PSTUNClient::NatTypes nat = m_manager->SetSTUNServer(args.GetOptionString('N'));
    cout << "STUN server \"" << m_manager->GetSTUNClient()->GetServer() << "\" replies " << nat;
    PIPSocket::Address externalAddress;
    if (nat != PSTUNClient::BlockedNat && m_manager->GetSTUNClient()->GetExternalAddress(externalAddress))
      cout << " with address " << externalAddress;
    cout << endl;
  }


#if OPAL_SIP
  // Set up SIP
  interfaces = args.GetOptionString('S');
  if (interfaces != "x") {
    MySIPEndPoint * sip  = new MySIPEndPoint(*m_manager);
    if (!sip->StartListeners(interfaces.Lines())) {
      cerr << "Could not start SIP listeners." << endl;
      return;
    }

    if (args.HasOption('P'))
      sip->SetProxy(args.GetOptionString('P'), args.GetOptionString('u'), args.GetOptionString('p'));

    if (args.HasOption('r')) {
      SIPRegister::Params params;
      params.m_addressOfRecord = args.GetOptionString('r');
      params.m_password = args.GetOptionString('p');
      params.m_expire = 300;

      PString aor;
      if (!sip->Register(params, aor)) {
        cerr << "Could not start SIP registration to " << params.m_addressOfRecord << endl;
        return;
      }

      sip->m_completed.Wait();

      if (!sip->IsRegistered(aor)) {
        cerr << "Could not complete SIP registration for " << aor << endl;
        return;
      }

      m_manager->AddRouteEntry(prefix + ":.*\ttel:.* = sip:<dn>" + aor.Mid(aor.Find('@')));
    }

    m_manager->AddRouteEntry("sip.*:.* = " + prefix + ":" + args[0] + ";receive");
  }
#endif // OPAL_SIP


#if OPAL_H323
  // Set up H.323
  interfaces = args.GetOptionString('H');
  if (interfaces != "x") {
    H323EndPoint * h323 = new H323EndPoint(*m_manager);
    if (!h323->StartListeners(interfaces.Lines())) {
      cerr << "Could not start H.323 listeners." << endl;
      return;
    }

    if (args.HasOption('g') || args.HasOption('G')) {
      if (!h323->UseGatekeeper(args.GetOptionString('g'), args.GetOptionString('G'))) {
        cerr << "Could not complete gatekeeper registration" << endl;
        return;
      }

      m_manager->AddRouteEntry(prefix + ":.*\ttel:.* = h323:<dn>");
    }

    m_manager->AddRouteEntry("h323.*:.* = " + prefix + ":" + args[0] + ";receive");
  }
#endif // OPAL_H323


  // If we have LIDs speficied in command line, load them
  if (args.HasOption('L')) {
    OpalLineEndPoint * lines = new OpalLineEndPoint(*m_manager);
    if (!lines->AddDeviceNames(args.GetOptionString('L').Lines()))
      cerr << "Could not start Line Interface Device(s)" << endl;
    else {
      m_manager->AddRouteEntry("pots.*:.* = " + prefix + ":" + args[0] + ";receive");
      m_manager->AddRouteEntry("pstn.*:.* = " + prefix + ":" + args[0] + ";receive");
    }
  }


  OpalConnection::StringOptions stringOptions;
  if (args.HasOption('I'))
    stringOptions.SetAt(OPAL_OPT_DETECT_INBAND_DTMF, "false");
  if (args.HasOption('i'))
    stringOptions.SetAt(OPAL_OPT_SEND_INBAND_DTMF, "false");

  if (args.GetCount() == 1)
    cout << "Awaiting incoming fax, saving as " << args[0] << " ... " << flush;
  else {
    PString token;
    if (!m_manager->SetUpCall(prefix + ":" + args[0], args[1], token, NULL, 0, &stringOptions)) {
      cerr << "Could not start call to \"" << args[1] << '"' << endl;
      return;
    }
    cout << "Sending " << args[0] << " to " << args[1] << " ... " << flush;
  }

  // Wait for call to come in and finish
  m_manager->m_completed.Wait();
  cout << " ... completed.";
}


void MyManager::OnClearedCall(OpalCall & /*call*/)
{
  m_completed.Signal();
}


void MySIPEndPoint::OnRegistrationStatus(const RegistrationStatus & status)
{
  SIPEndPoint::OnRegistrationStatus(status);
  if (status.m_reason >= SIP_PDU::Successful_OK)
    m_completed.Signal();
}


void MyFaxEndPoint::OnFaxCompleted(OpalFaxConnection & connection, bool failed)
{
  OpalMediaStatistics stats;
  connection.GetStatistics(stats);
  switch (stats.m_fax.m_result) {
    case -2 :
      cout << " failed to establish ";
      break;
    case 0 :
      cout << " successful "
           << (connection.IsReceive() ? stats.m_fax.m_rxPages : stats.m_fax.m_txPages)
           << " of " << stats.m_fax.m_totalPages << ' ';
      break;
    case 41 :
      cout << " failed to open TIFF file ";
      break;
    case 42 :
    case 43 :
    case 44 :
    case 45 :
    case 46 :
      cout << " illegal TIFF file ";
      break;
    default :
      cout << " T.30 error " << stats.m_fax.m_result << ' ';
  }
  OpalFaxEndPoint::OnFaxCompleted(connection, failed);
}


// End of File ///////////////////////////////////////////////////////////////
