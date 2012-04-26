/*
 * main.h
 *
 * A simple OPAL "net telephone" application.
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * $Revision: 21857 $
 * $Author: rjongbloed $
 * $Date: 2008-12-22 05:03:10 +0100 (Mo, 22. Dez 2008) $
 */

#ifndef _SimpleOpal_MAIN_H
#define _SimpleOpal_MAIN_H

#include <ptclib/ipacl.h>
#include <opal/manager.h>
#include <opal/pcss.h>
#include <opal/ivr.h>


#ifndef OPAL_PTLIB_AUDIO
#error Cannot compile without PTLib sound channel support!
#endif


class MyManager;
class SIPEndPoint;
class H323EndPoint;
class H323SEndPoint;
class IAX2EndPoint;

class MyPCSSEndPoint : public OpalPCSSEndPoint
{
  PCLASSINFO(MyPCSSEndPoint, OpalPCSSEndPoint);

  public:
    MyPCSSEndPoint(MyManager & manager);

    virtual PBoolean OnShowIncoming(const OpalPCSSConnection & connection);
    virtual PBoolean OnShowOutgoing(const OpalPCSSConnection & connection);

    PBoolean SetSoundDevice(PArgList & args, const char * optionName, PSoundChannel::Directions dir);

    PString incomingConnectionToken;
    bool    autoAnswer;
};


class MyManager : public OpalManager
{
  PCLASSINFO(MyManager, OpalManager);

  public:
    MyManager();
    ~MyManager();

    PBoolean Initialise(PArgList & args);
    void Main(PArgList & args);

    virtual void OnEstablishedCall(
      OpalCall & call   /// Call that was completed
    );
    virtual void OnClearedCall(
      OpalCall & call   /// Connection that was established
    );
    virtual PBoolean OnOpenMediaStream(
      OpalConnection & connection,  /// Connection that owns the media stream
      OpalMediaStream & stream    /// New media stream being opened
    );
    virtual void OnUserInputString(
      OpalConnection & connection,  /// Connection input has come from
      const PString & value         /// String value of indication
    );

  protected:
    bool InitialiseH323EP(PArgList & args, PBoolean secure, H323EndPoint * h323EP);

    PString currentCallToken;
    PString heldCallToken;

#if OPAL_LID
    OpalLineEndPoint * potsEP;
#endif
    MyPCSSEndPoint   * pcssEP;
#if OPAL_H323
    H323EndPoint     * h323EP;
#endif
#if OPAL_SIP
    SIPEndPoint      * sipEP;
#endif
#if OPAL_IAX2
    IAX2EndPoint     * iax2EP;
#endif
#if OPAL_IVR
    OpalIVREndPoint  * ivrEP;
#endif
#if OPAL_FAX
    OpalFaxEndPoint  * faxEP;
#endif

    bool    pauseBeforeDialing;
    PString srcEP;

    void HangupCurrentCall();
    void StartCall(const PString & ostr);
    void HoldRetrieveCall();
    void TransferCall(const PString & dest);
#if OPAL_PTLIB_CONFIG_FILE
    void NewSpeedDial(const PString & ostr);
    void ListSpeedDials();
#endif // OPAL_PTLIB_CONFIG_FILE
    void SendMessageToRemoteNode(const PString & ostr);
    void SendTone(const char tone);
};


class SimpleOpalProcess : public PProcess
{
  PCLASSINFO(SimpleOpalProcess, PProcess)

  public:
    SimpleOpalProcess();

    void Main();

  protected:
    MyManager * opal;
};


#endif  // _SimpleOpal_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
