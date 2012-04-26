/*
 * main.h
 *
 * Copyright (C) 2009 Post Increment
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
 * The Original Code is Opal
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision:$
 * $Author:$
 * $Date:$
 */

#ifndef _OpalEcho_MAIN_H
#define _OpalEcho_MAIN_H

#define	NEW_API		((OPAL_MAJOR >=3) && (OPAL_MINOR >= 7))

#if NEW_API
  #ifdef __GNUC__
    #warning "using new api"
  #else
    #pragma message("using new api")
  #endif
#else
  #ifdef __GNUC__
    #warning "using new api"
  #else
    #pragma message("using old api")
  #endif
#endif

class EchoConnection;

class EchoEndPoint : public OpalLocalEndPoint
{
  PCLASSINFO(EchoEndPoint, OpalLocalEndPoint);
  public:
    EchoEndPoint(OpalManager & manager)
      : OpalLocalEndPoint(manager, "echo")
    { }

    virtual OpalLocalConnection * CreateConnection(
      OpalCall & call,    ///<  Owner of connection
      void * userData     ///<  Arbitrary data to pass to connection
#if NEW_API
      ,
      unsigned options,
      OpalConnection::StringOptions * stringOptions
#endif
    );

    bool OnReadMediaFrame(
      const OpalLocalConnection & connection, ///<  Connection for media
      const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
      RTP_DataFrame & frame                   ///<  RTP frame for data
    );

    virtual bool OnWriteMediaFrame(
      const OpalLocalConnection & connection, ///<  Connection for media
      const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
      RTP_DataFrame & frame                   ///<  RTP frame for data
    );
};

class MyManager : public OpalManager
{
  PCLASSINFO(MyManager, OpalManager);

  public:
    MyManager();
    ~MyManager();

    PBoolean Initialise(PConfig & cfg, PConfigPage * rsrc);

  protected:
    SIPEndPoint      * sipEP;
    EchoEndPoint     * echoEP;

  friend class OpalEcho;
};


class OpalEcho : public PHTTPServiceProcess
{
  PCLASSINFO(OpalEcho, PHTTPServiceProcess)

  public:
    OpalEcho();
    virtual void Main();
    virtual PBoolean OnStart();
    virtual void OnStop();
    virtual void OnControl();
    virtual void OnConfigChanged();
    virtual PBoolean Initialise(const char * initMsg);

    static OpalEcho & Current() { return (OpalEcho &)PProcess::Current(); }

    MyManager manager;
};

#endif  // _OpalEcho_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
