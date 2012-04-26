/*
 * main.h
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
 * $Revision: 23648 $
 * $Author: rjongbloed $
 * $Date: 2009-10-13 10:32:32 +0200 (Di, 13. Okt 2009) $
 */

#ifndef _SipIM_MAIN_H
#define _SipIM_MAIN_H

#include <opal/manager.h>
#include <sip/sipep.h>

#if !OPAL_HAS_IM
#error Cannot compile IM sample program without IM!
#endif

class MyManager : public OpalManager
{
    PCLASSINFO(MyManager, OpalManager)

  public:
    virtual void OnClearedCall(OpalCall & call); // Callback override
    void OnApplyStringOptions(OpalConnection & conn, OpalConnection::StringOptions & options);

    PSyncPoint m_connected;
    PSyncPoint m_completed;
    PString m_callToken;
    OpalMediaFormat m_imFormat;
};

class MyPCSSEndPoint : public OpalPCSSEndPoint
{
  PCLASSINFO(MyPCSSEndPoint, OpalPCSSEndPoint);

  public:
    MyPCSSEndPoint(MyManager & manager)
      : OpalPCSSEndPoint(manager)
    { }

    virtual PBoolean OnShowIncoming(const OpalPCSSConnection & connection);
    virtual PBoolean OnShowOutgoing(const OpalPCSSConnection & connection);
};

class SipIM : public PProcess
{
    PCLASSINFO(SipIM, PProcess)

  public:
    SipIM();
    ~SipIM();

    enum Mode {
      Use_MSRP,
      Use_SIPIM,
      Use_T140
    };

    virtual void Main();

  private:
    MyManager * m_manager;
};


#endif  // _SipIM_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
