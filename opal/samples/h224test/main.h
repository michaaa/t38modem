/*
 * main.h
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

#ifndef _H224Test_MAIN_H
#define _H224Test_MAIN_H

#include <opal/manager.h>
#include <sip/sipep.h>

class MyManager : public OpalManager
{
    PCLASSINFO(MyManager, OpalManager)

  public:
    virtual void OnClearedCall(OpalCall & call); // Callback override
    void OnApplyStringOptions(OpalConnection & conn, OpalConnection::StringOptions & options);

    PSyncPoint m_connected;
    PSyncPoint m_completed;
    PString m_callToken;
    OpalMediaFormat m_h224Format;
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

    PDECLARE_NOTIFIER(OpalConnection::IMInfo, MyPCSSEndPoint, OnReceiveIM);
};

class H224Test : public PProcess
{
    PCLASSINFO(H224Test, PProcess)

  public:
    H224Test();
    ~H224Test();

    virtual void Main();

  private:
    MyManager * m_manager;
};


#endif  // _H224Test_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
