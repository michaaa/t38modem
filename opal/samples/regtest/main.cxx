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
 * $Revision: 22301 $
 * $Author: rjongbloed $
 * $Date: 2009-03-26 15:25:06 +1100 (Thu, 26 Mar 2009) $
 */

#include <opal/manager.h>
#include <sip/sipep.h>

class MyManager : public OpalManager
{
  PCLASSINFO(MyManager, OpalManager)
};


class RegTest : public PProcess
{
  PCLASSINFO(RegTest, PProcess)

  public:
    RegTest();
    ~RegTest();

    virtual void Main();

  private:
    MyManager * m_manager;
};

PCREATE_PROCESS(RegTest);

RegTest::RegTest()
  : PProcess("OPAL RegTest", "RegTest", OPAL_MAJOR, OPAL_MINOR, ReleaseCode, OPAL_BUILD)
  , m_manager(NULL)
{
}


RegTest::~RegTest()
{
  delete m_manager;
}


void RegTest::Main()
{
  PArgList & args = GetArguments();

  args.Parse(
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
  SIPEndPoint * sipEp = new SIPEndPoint(m_manager);

  sipEp->StartListener("");

  PDictionary<POrdinalKey, PString> indexToAor;

  const int max = 2;

  //for (;;) 
  {

    PINDEX i;
    for (i = 0; i < max; ++i) {

      PString username;
      username.sprintf("user%i", i+1);

      PString password;
      password.sprintf("pwd%i", i+1);

      PString aor;
      aor = username + "@wanchai.postincrement.net";

      PString contactAddress;
      contactAddress = username + "@10.0.2.15";

      SIPRegister::Params params;

      params.m_addressOfRecord  = aor;
      params.m_registrarAddress = "joejim2.postincrement.net";
      params.m_contactAddress   = contactAddress;
      params.m_authID           = username;
      params.m_password         = password;
      params.m_expire           = 120;
      //params.m_restoreTime;
      //params.m_minRetryTime;
      //params.m_maxRetryTime;

      PString returnedAOR;

      if (!sipEp->Register(params, returnedAOR)) 
        cerr << "Registration of " << aor << " failed" << endl;
      else {
        cerr << aor << " registered as " << returnedAOR << endl;
        indexToAor.SetAt(i+1, new PString(returnedAOR));
      }
    }

    Sleep(10000);

    for (i = 0; i < max; ++i) {
      PString * aor = indexToAor.GetAt(i+1);
      if (aor == NULL)
        cerr << "No AOR for index " << i+1 << endl;
      else if (!sipEp->Unregister(*aor /*, 5000*/))
        cerr << "Unregister for index " << i+1 << " failed" << endl;
      else
        cerr << "Unregister for index " << i+1 << " succeeded" << endl;
      Sleep(100);
      indexToAor.RemoveAt(POrdinalKey(i+1));
    }

    Sleep(10000);
  }

  // Wait for call to come in and finish
  cout << " completed.";
}


// End of File ///////////////////////////////////////////////////////////////
