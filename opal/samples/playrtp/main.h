/*
 * main.h
 *
 * OPAL application source file for playing RTP from a PCAP file
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

#ifndef _PlayRTP_MAIN_H
#define _PlayRTP_MAIN_H

#include <ptclib/pvidfile.h>


class PlayRTP : public PProcess
{
  PCLASSINFO(PlayRTP, PProcess)

  public:
    PlayRTP();
    ~PlayRTP();

    virtual void Main();
    void Play(const PFilePath & filename);
    void Find(const PFilePath & filename);

    PDECLARE_NOTIFIER(OpalMediaCommand, PlayRTP, OnTranscoderCommand);

    std::map<RTP_DataFrame::PayloadTypes, OpalMediaFormat> m_payloadType2mediaFormat;

    PIPSocket::Address m_srcIP;
    PIPSocket::Address m_dstIP;

    WORD m_srcPort;
    WORD m_dstPort;
    bool m_singleStep;
    int  m_info;
    bool m_extendedInfo;
    bool m_noDelay;
    bool m_writeEventLog;

    PFile     m_payloadFile;
    PTextFile m_eventLog;
    PYUVFile  m_yuvFile;
    PFilePath m_encodedFileName;
    PString   m_extraText;
    int       m_extraHeight;

    OpalTranscoder     * m_transcoder;
    PSoundChannel      * m_player;
    PVideoOutputDevice * m_display;

    unsigned m_fragmentationCount;
    unsigned m_packetCount;

    bool     m_vfu;
    bool     m_videoError;
    unsigned m_videoFrames;
};


#endif  // _PlayRTP_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
