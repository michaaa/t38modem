/*
 * main.h
 *
 * OPAL application source file for testing codecs
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
 * $Revision: 22696 $
 * $Author: rjongbloed $
 * $Date: 2009-05-22 03:50:31 +0200 (Fr, 22. Mai 2009) $
 */

#ifndef _CodecTest_MAIN_H
#define _CodecTest_MAIN_H

#include <codec/vidcodec.h>
#include <opal/patch.h>

class TranscoderThread : public PThread
{
  public:
    TranscoderThread(unsigned _num, const char * name)
      : PThread(5000, NoAutoDeleteThread, NormalPriority, name)
      , running(false)
      , encoder(NULL)
      , decoder(NULL)
      , num(_num)
      , timestamp(0)
      , markerHandling(NormalMarkers)
      , rateController(NULL)
      , m_dropPercent(0)
    {
    }

    ~TranscoderThread()
    {
      delete encoder;
      delete decoder;
    }

    int InitialiseCodec(PArgList & args, const OpalMediaFormat & rawFormat);

    virtual void Main();

    virtual bool Read(RTP_DataFrame & frame) = 0;
    virtual bool Write(const RTP_DataFrame & frame) = 0;
    void Start() { if (running) Resume(); }
    virtual void Stop() = 0;

    virtual void UpdateStats(const RTP_DataFrame &) { }

    virtual void CalcSNR(const RTP_DataFrame & /*src*/, const RTP_DataFrame & /*dst*/)
    {  }

    virtual void ReportSNR()
    {  }

    bool running;

    PSyncPointAck pause;
    PSyncPoint    resume;

    OpalTranscoder * encoder;
    OpalTranscoder * decoder;
    unsigned         num;
    DWORD            timestamp;
    enum MarkerHandling {
      SuppressMarkers,
      ForceMarkers,
      NormalMarkers
    } markerHandling;

    PDECLARE_NOTIFIER(OpalMediaCommand, TranscoderThread, OnTranscoderCommand);
    bool m_forceIFrame;

    OpalVideoRateController * rateController;
    int framesToTranscode;
    int frameTime;
    bool calcSNR;
    int m_dropPercent;
};


class AudioThread : public TranscoderThread
{
  public:
    AudioThread(unsigned _num)
      : TranscoderThread(_num, "Audio")
      , recorder(NULL)
      , player(NULL)
      , readSize(0)
    {
    }

    ~AudioThread()
    {
      delete recorder;
      delete player;
    }

    bool Initialise(PArgList & args);

    virtual void Main();
    virtual bool Read(RTP_DataFrame & frame);
    virtual bool Write(const RTP_DataFrame & frame);
    virtual void Stop();

    PSoundChannel * recorder;
    PSoundChannel * player;
    PINDEX          readSize;
};

class VideoThread : public TranscoderThread
{
  public:
    VideoThread(unsigned _num)
      : TranscoderThread(_num, "Video")
      , grabber(NULL)
      , display(NULL)
      , singleStep(false)
      , frameWait(0, INT_MAX)
    {
    }

    ~VideoThread()
    {
      delete grabber;
      delete display;
    }

    bool Initialise(PArgList & args);

    virtual void Main();
    virtual bool Read(RTP_DataFrame & frame);
    virtual bool Write(const RTP_DataFrame & frame);
    virtual void Stop();

    void CalcVideoPacketStats(const RTP_DataFrameList & frames, bool isIFrame);
    void WriteFrameStats(const PString & str);

    PVideoInputDevice  * grabber;
    PVideoOutputDevice * display;

    bool                 singleStep;
    PSemaphore           frameWait;
    unsigned             frameRate;

    void SaveSNRFrame(const RTP_DataFrame * src)
    { snrSourceFrames.push(src); }

    void CalcSNR(const RTP_DataFrame & src);
    void CalcSNR(const RTP_DataFrame & src, const RTP_DataFrame & dst);
    void ReportSNR();

    PString frameFn;
    PTextFile frameStatFile;
    PInt64 frameCount;
    DWORD frameStartTimestamp;
    std::queue<const RTP_DataFrame *> snrSourceFrames;

    unsigned snrWidth, snrHeight;
    double sumYSNR;
    double sumCbSNR;
    double sumCrSNR;
    PInt64 snrCount;

    OpalBitRateCalculator m_bitRateCalc;
};


class CodecTest : public PProcess
{
  PCLASSINFO(CodecTest, PProcess)

  public:
    CodecTest();
    ~CodecTest();

    virtual void Main();

    class TestThreadInfo : public PObject
    {
      public:
        TestThreadInfo(unsigned num)
          : audio(num)
          , video(num)
        {
        }

        bool Initialise(PArgList & args) { return audio.Initialise(args) && video.Initialise(args); }
        void Start() { audio.Start(); video.Start(); }
        void Stop()  { audio.Stop(); video.Stop(); }
        void Wait()  { audio.WaitForTermination(); video.WaitForTermination(); }

        AudioThread audio;
        VideoThread video;
    };
};


#endif  // _CodecTest_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
