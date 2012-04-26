/*
 * main.cxx
 *
 * OPAL application source file for Interactive Voice Response using VXML
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
 * $Revision: 22852 $
 * $Author: rjongbloed $
 * $Date: 2009-06-12 12:22:51 +0200 (Fr, 12. Jun 2009) $
 */

#include "precompile.h"
#include "main.h"

// Debug: -ttttodebugstream file:../callgen/ogm.wav sip:10.0.1.12


PCREATE_PROCESS(IvrOPAL);


IvrOPAL::IvrOPAL()
  : PProcess("Vox Gratia", "IvrOPAL", 1, 0, ReleaseCode, 0)
  , m_manager(NULL)
{
}


IvrOPAL::~IvrOPAL()
{
  delete m_manager;
}


void IvrOPAL::Main()
{
  PArgList & args = GetArguments();

  args.Parse("g-gk-host:"
             "G-gk-id:"
             "h-help."
             "H-h323:"
             "N-stun:"
             "p-password:"
             "P-proxy:"
             "r-register:"
             "S-sip:"
             "u-user:"
#if PTRACING
             "t-trace."              "-no-trace."
             "o-output:"             "-no-output."
#endif
             , FALSE);

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
         PTrace::Blocks | PTrace::Timestamp | PTrace::Thread | PTrace::FileAndLine);
#endif

  if (args.HasOption('h') || args.GetCount() == 0) {
    cerr << "usage: " << GetFile().GetTitle() << " [ options ] vxml [ url ]\n"
            "\n"
            "Available options are:\n"
            "  -h or --help            : print this help message.\n"
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
            "  -N or --stun server     : Set NAT traversal STUN server.\n"
#if PTRACING
            "  -o or --output file     : file name for output of log messages\n"       
            "  -t or --trace           : degree of verbosity in error log (more times for more detail)\n"     
#endif
            "\n"
            "where vxml is a VXML script, a URL to a VXML script or a WAV file, or a\n"
            "series of commands separated by ';'.\n"
            "\n"
            "Commands are:\n"
            "  repeat=n    Repeat next WAV file n times.\n"
            "  delay=n     Delay after repeats n milliseconds.\n"
            "  voice=name  Set Text To Speech voice to name\n"
            "  tone=t      Emit DTMF tone t\n"
            "  silence=n   Emit silence for n milliseconds\n"
            "  speak=text  Speak the text using the Text To Speech system.\n"
            "  speak=$var  Speak the internal variable using the Text To Speech system.\n"
            "\n"
            "Variables may be one of:\n"
            "  Time\n"
            "  Originator-Address\n"
            "  Remote-Address\n"
            "  Source-IP-Address\n"
            "\n"
            "If a second parameter is provided and outgoing call is made and when answered\n"
            "the script is executed. If no second paramter is provided then the program\n"
            "will continuosly listen for incoming calls and execute the script on each\n"
            "call. Simultaneous calls to the limits of the operating system arre possible.\n"
            "\n"
            "e.g. " << GetFile().GetTitle() << " file://message.wav sip:fred@bloggs.com\n"
            "     " << GetFile().GetTitle() << " http://voicemail.vxml\n"
            "     " << GetFile().GetTitle() << " \"repeat=5;delay=2000;speak=Hello, this is IVR!\"\n"
            "\n";
    return;
  }

  m_manager = new MyManager();

  if (args.HasOption('N')) {
    PSTUNClient::NatTypes nat = m_manager->SetSTUNServer(args.GetOptionString('N'));
    cout << "STUN server \"" << m_manager->GetSTUNClient()->GetServer() << "\" replies " << nat;
    PIPSocket::Address externalAddress;
    if (nat != PSTUNClient::BlockedNat && m_manager->GetSTUNClient()->GetExternalAddress(externalAddress))
      cout << " with address " << externalAddress;
    cout << endl;
  }

  if (args.HasOption('u'))
    m_manager->SetDefaultUserName(args.GetOptionString('u'));

  PCaselessString interfaces;

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
    }

    m_manager->AddRouteEntry("sip.*:.* = ivr:");
    m_manager->AddRouteEntry("ivr:.* = sip:<da>");
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

    if (args.HasOption('g') || args.HasOption('G'))
      h323->UseGatekeeper(args.GetOptionString('g'), args.GetOptionString('G'));

    m_manager->AddRouteEntry("h323.*:.* = ivr:");
    m_manager->AddRouteEntry("ivr:.* = h323:<da>");
  }
#endif // OPAL_H323


  // Set up IVR
  OpalIVREndPoint * ivr  = new OpalIVREndPoint(*m_manager);
  ivr->SetDefaultVXML(args[0]);


  if (args.GetCount() == 1)
    cout << "Awaiting incoming call, using VXML \"" << args[0] << "\" ..." << flush;
  else {
    PString token;
    if (!m_manager->SetUpCall("ivr:", args[1], token)) {
      cerr << "Could not start call to \"" << args[1] << '"' << endl;
      return;
    }
    cout << "Playing " << args[0] << " to " << args[1] << " ..." << flush;
  }

  // Wait for call to come in and finish
  m_manager->m_completed.Wait();
  cout << " completed.";

  MyManager * mgr = m_manager;
  m_manager = NULL;
  delete mgr;
}


void MyManager::OnClearedCall(OpalCall & call)
{
  if (call.GetPartyA().NumCompare("ivr") == EqualTo)
    m_completed.Signal();
}


bool IvrOPAL::OnInterrupt(bool)
{
  if (m_manager == NULL)
    return false;

  m_manager->m_completed.Signal();
  return true;
}


void MySIPEndPoint::OnRegistrationStatus(const RegistrationStatus & status)
{
  SIPEndPoint::OnRegistrationStatus(status);
  if (status.m_reason >= SIP_PDU::Successful_OK)
    m_completed.Signal();
}


// End of File ///////////////////////////////////////////////////////////////
