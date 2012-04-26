/*
 * main.cxx
 *
 * OPAL application source file for testing codecs
 *
 * Main program entry point.
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
 * $Revision: 23520 $
 * $Author: rjongbloed $
 * $Date: 2009-09-24 06:57:23 +0200 (Do, 24. Sep 2009) $
 */

#include "precompile.h"
#include "main.h"

#include <codec/ratectl.h>
#include <opal/patch.h>

#include <ptclib/random.h>

#define OUTPUT_BPS(strm, rate) \
  if (rate < 10000ULL) strm << rate << ' '; \
  else if (rate < 10000000ULL) strm << rate/1000.0 << " k"; \
  else if (rate < 10000000000ULL) strm << rate/1000000.0 << " M"; \
  strm << "bps"; \

PMutex coutMutex;
static unsigned g_infoCount = 0;

PCREATE_PROCESS(CodecTest);

CodecTest::CodecTest()
  : PProcess("OPAL Audio/Video Codec Tester", "codectest", 1, 0, ReleaseCode, 0)
{
}


CodecTest::~CodecTest()
{
}


void CodecTest::Main()
{
  PArgList & args = GetArguments();

  args.Parse("b-bit-rate:"
             "c-crop."
             "C-rate-control:"
             "d-drop:"
             "D-display-device:"
             "-display-driver:"
             "F-audio-frames:"
             "G-grab-device:"
             "-grab-driver:"
             "-grab-format:"
             "-grab-channel:"
             "h-help."
             "i-info."
             "m-suppress-marker."
             "M-force-marker."
             "O-option:"
             "p-payload-size:"
             "P-play-device:"
             "-play-driver:"
             "-play-buffers:"
             "r-frame-rate:"
             "R-record-device:"
             "-record-driver:"
             "s-frame-size:"
             "-single-step."
             "S-simultaneous:"
             "T-statistics."
             "-count:"
             "-noprompt."
             "-snr."
             "-list."
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

  if (args.HasOption("list")) {
    OpalPluginCodecManager * codecMgr = PFactory<PPluginModuleManager>::CreateInstanceAs<OpalPluginCodecManager>("OpalPluginCodecManager");
    if (codecMgr == NULL) {
      cerr << "error: Cannot get codec manager" << endl;
      return;
    }
    PPluginModuleManager::PluginListType pluginList = codecMgr->GetPluginList();

    cout << "Plugin codecs:" << endl;
    for (int i = 0; i < pluginList.GetSize(); i++) {
      PDynaLink & plugin =  pluginList.GetDataAt(i);

      PStringList codecNames;
      PluginCodec_GetCodecFunction getCodecs;
      if (plugin.GetFunction(PLUGIN_CODEC_GET_CODEC_FN_STR, (PDynaLink::Function &)getCodecs)) {
        unsigned count = 0;
        PluginCodec_Definition * codecs = (*getCodecs)(&count, PLUGIN_CODEC_VERSION_OPTIONS);
        while (count > 0) {
          PString name(codecs->descr);
          if (codecNames.GetStringsIndex(name) == P_MAX_INDEX)
            codecNames.AppendString(name);
          --count;
          ++codecs;
        }
      }

      cout << "   " << plugin.GetName(true) << endl;
      if (codecNames.GetSize() == 0) 
        cout << "      No codecs defined" << endl;
      else
        cout << "      " << setfill(',') << codecNames << setfill(' ') << endl;
      
    }
    cout << "\n\n";
    return;
  }

  if (args.HasOption('h') || args.GetCount() == 0) {
    PError << "usage: " << GetFile().GetTitle() << " [ options ] fmtname [ fmtname ]\n"
              "  where fmtname is the Media Format Name for the codec(s) to test, up to two\n"
              "  formats (one audio and one video) may be specified.\n"
              "\n"
              "Available options are:\n"
              "  --help                  : print this help message.\n"
              "  --record-driver drv     : audio recorder driver.\n"
              "  -R --record-device dev  : audio recorder device.\n"
              "  --play-driver drv       : audio player driver.\n"
              "  -P --play-device dev    : audio player device.\n"
              "  -F --audio-frames n     : audio frames per packet, default 1.\n"
              "  --play-buffers n        : audio player buffers, default 8.\n"
              "  --grab-driver drv       : video grabber driver.\n"
              "  -G --grab-device dev    : video grabber device.\n"
              "  --grab-format fmt       : video grabber format (\"pal\"/\"ntsc\")\n"
              "  --grab-channel num      : video grabber channel.\n"
              "  --display-driver drv    : video display driver to use.\n"
              "  -D --display-device dev : video display device to use.\n"
              "  -s --frame-size size    : video frame size (\"qcif\", \"cif\", WxH)\n"
              "  -r --frame-rate size    : video frame rate (frames/second)\n"
              "  -b --bit-rate size      : video bit rate (bits/second)\n"
              "  -O --option opt=val     : set media format option to value\n"
              "  --single-step           : video single frame at a time mode\n"
              "  -c --crop               : crop rather than scale if resizing\n"
              "  -m --suppress-marker    : suppress marker bits to decoder\n"
              "  -M --force-marker       : force marker bits to decoder\n"
              "  -p --payload-size sz    : Set size of maximum RTP payload for encoded data\n"
              "  -S --simultanoues n     : Number of simultaneous encode/decode threads\n"
              "  -T --statistics         : output statistics files\n"
              "  -C --rate-control       : enable rate control\n"
              "  -d --drop N             : randomly drop N% of encoded packets\n"
              "  --count n               : set number of frames to transcode\n"
              "  --noprompt              : do not prompt for commands, i.e. exit when input closes\n"
              "  --snr                   : calculate signal-to-noise ratio between input and output\n"
              "  -i --info               : display per-frame info (use multiple times for more info)\n"
              "  --list                  : list all available plugin codecs\n"
#if PTRACING
              "  -o or --output file     : file name for output of log messages\n"       
              "  -t or --trace           : degree of verbosity in error log (more times for more detail)\n"     
#endif
              "\n"
              "e.g. " << GetFile().GetTitle() << " --grab-device fake --grab-channel 2 GSM-AMR H.264\n\n";
    return;
  }

  g_infoCount = args.GetOptionCount('i');

  unsigned threadCount = args.GetOptionString('S').AsInteger();
  if (threadCount > 0) {
    PList<TestThreadInfo> infos;
    unsigned i;
    for (i = 0; i < threadCount; ++i) {
      cout << "Creating thread " << i+1 << endl;
      TestThreadInfo * info = new TestThreadInfo(i+1);
      if (!info->Initialise(args)) {
        cout << "Failed to create thread, aborting." << endl;
        return;
      }
      infos.Append(info);
    }

    cout << "Starting threads." << endl;
    for (i = 0; i < threadCount; ++i)
      infos[i].Start();

    coutMutex.Wait();
    cout << "\n\n\nTests running, press return to stop." << endl;
    coutMutex.Signal();

    PCaselessString cmd;
    cin >> cmd;

    coutMutex.Wait();
    cout << "\n\n\nStopping tests." << endl;
    coutMutex.Signal();

    for (i = 0; i < threadCount; ++i)
      infos[i].Stop();

    return;
  }

  TestThreadInfo test(0);

  if (!test.Initialise(args))
    return;

#if 0
  if (video.encoder == NULL && audio.encoder == NULL) {
    cout << "Could not identify media formats ";
    for (int i = 0; i < args.GetCount(); ++i)
      cout << args[i] << ' ';
    cout << endl;
    return;
  }
#endif

  test.Start();

  // command line
  if (args.HasOption("noprompt")) {
    cout << "Waiting for finish" << endl;
    test.Wait();
  }
  else {
    for (;;) {

      // display the prompt
      cout << "codectest> " << flush;
      PCaselessString cmd;
      cin >> cmd;

      if (cmd == "q" || cmd == "x" || cmd == "quit" || cmd == "exit")
        break;

      if (cmd.NumCompare("n") == EqualTo) {
        int steps = cmd.Mid(1).AsUnsigned();
        do {
          test.video.frameWait.Signal();
        } while (--steps > 0);
        continue;
      }

      if (cmd == "vfu") {
        if (test.video.encoder == NULL)
          cout << "\nNo video encoder running!" << endl;
        else
          test.video.encoder->ExecuteCommand(OpalVideoUpdatePicture());
        continue;
      }

      if (cmd == "fg") {
        if (test.video.grabber == NULL)
          cout << "\nNo video grabber running!" << endl;
        else if (!test.video.grabber->SetVFlipState(!test.video.grabber->GetVFlipState()))
          cout << "\nCould not toggle Vflip state of video grabber device" << endl;
        continue;
      }

      if (cmd == "fd") {
        if (test.video.display == NULL)
          cout << "\nNo video display running!" << endl;
        else if (!test.video.display->SetVFlipState(!test.video.display->GetVFlipState()))
          cout << "\nCould not toggle Vflip state of video display device" << endl;
        continue;
      }

      unsigned width, height;
      if (PVideoFrameInfo::ParseSize(cmd, width, height)) {
        test.video.pause.Signal();
        if (test.video.grabber == NULL)
          cout << "\nNo video grabber running!" << endl;
        else if (!test.video.grabber->SetFrameSizeConverter(width, height))
          cout << "Video grabber device could not be set to size " << width << 'x' << height << endl;
        if (test.video.display == NULL)
          cout << "\nNo video display running!" << endl;
        else if (!test.video.display->SetFrameSizeConverter(width, height))
          cout << "Video display device could not be set to size " << width << 'x' << height << endl;
        test.video.resume.Signal();
        continue;
      }

      cout << "Select:\n"
              "  vfu    : Video Fast Update (force I-Frame)\n"
              "  fg     : Flip video grabber top to bottom\n"
              "  fd     : Flip video display top to bottom\n"
              "  qcif   : Set size of grab & display to qcif\n"
              "  cif    : Set size of grab & display to cif\n"
              "  WxH    : Set size of grab & display W by H\n"
              "  N      : Next video frame in sinlge step mode\n"
              "  Q or X : Exit program\n" << endl;
    } // end for

    cout << "Exiting." << endl;
    test.Stop();
  }
}


int TranscoderThread::InitialiseCodec(PArgList & args, const OpalMediaFormat & rawFormat)
{
  if (args.HasOption('m'))
    markerHandling = SuppressMarkers;
  else if (args.HasOption('M'))
    markerHandling = ForceMarkers;

  framesToTranscode = -1;
  PString s = args.GetOptionString("count");
  if (!s.IsEmpty())
    framesToTranscode = s.AsInteger();

  calcSNR = args.HasOption("snr");

  for (PINDEX i = 0; i < args.GetCount(); i++) {
    OpalMediaFormat mediaFormat = args[i];
    if (mediaFormat.IsEmpty()) {
      cout << "Unknown media format name \"" << args[i] << '"' << endl;
      return 0;
    }

    if (mediaFormat.GetMediaType() == rawFormat.GetMediaType()) {
      if (rawFormat == mediaFormat) {
        decoder = NULL;
        encoder = NULL;
      }
      else {
        OpalMediaFormat adjustedRawFormat = rawFormat;
        if (rawFormat == OpalPCM16) {
          if (mediaFormat.GetClockRate() != rawFormat.GetClockRate() || mediaFormat.GetPayloadType() == RTP_DataFrame::G722)
            adjustedRawFormat = OpalPCM16_16KHZ;
          if (args.HasOption('F')) {
            unsigned fpp = args.GetOptionString('F').AsUnsigned();
            if (fpp > 0)
              mediaFormat.SetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), fpp);
          }
        }

        if ((encoder = OpalTranscoder::Create(adjustedRawFormat, mediaFormat)) == NULL) {
          cout << "Could not create encoder for media format \"" << mediaFormat << '"' << endl;
          return false;
        }

        if ((decoder = OpalTranscoder::Create(mediaFormat, adjustedRawFormat)) == NULL) {
          cout << "Could not create decoder for media format \"" << mediaFormat << '"' << endl;
          return false;
        }
      }

      return -1;
    }
  }

  return 1;
}


void SetOptions(PArgList & args, OpalMediaFormat & mediaFormat)
{
  PStringArray options = args.GetOptionString('O').Lines();
  for (PINDEX opt = 0; opt < options.GetSize(); ++opt) {
    PString option = options[opt];
    PINDEX equal = option.Find('=');
    if (equal == P_MAX_INDEX)
      cerr << "Illegal option \"" << option << "\" used." << endl;
    else {
      PString name = option.Left(equal);
      PString value = option.Mid(equal+1);
      if (mediaFormat.SetOptionValue(name, value))
        cout << "Option \"" << name << "\" set to \"" << value << "\"." << endl;
      else
        cerr << "Could not set option \"" << name << "\" to \"" << value << "\"." << endl;
    }
  }

  mediaFormat.ToCustomisedOptions();
}

static int udiff(unsigned int const subtrahend, unsigned int const subtractor) 
{
  return subtrahend - subtractor;
}


static double square(double const arg) 
{
  return(arg*arg);
}

static double CalcSNR(const BYTE * src1, const BYTE * src2, PINDEX dataLen)
{
  double diff2 = 0.0;
  for (PINDEX i = 0; i < dataLen; ++i) 
    diff2 += square(udiff(*src1++, *src2++));

  double const snr = diff2 / dataLen / 255;

  return snr;
}

bool AudioThread::Initialise(PArgList & args)
{
  switch (InitialiseCodec(args, OpalPCM16)) {
    case 0 :
      return false;
    case 1 :
      return true;
  }

  readSize = encoder != NULL ? encoder->GetOptimalDataFrameSize(TRUE) : 480;
  OpalMediaFormat mediaFormat = encoder != NULL ? encoder->GetOutputFormat() : OpalPCM16;

  cout << "Audio media format set to " << mediaFormat << endl;

  // Audio recorder
  PString driverName = args.GetOptionString("record-driver");
  PString deviceName = args.GetOptionString("record-device");
  recorder = PSoundChannel::CreateOpenedChannel(driverName, deviceName, PSoundChannel::Recorder, 1, mediaFormat.GetClockRate());
  if (recorder == NULL) {
    cerr << "Cannot use ";
    if (driverName.IsEmpty() && deviceName.IsEmpty())
      cerr << "default ";
    cerr << "audio recorder";
    if (!driverName)
      cerr << ", driver \"" << driverName << '"';
    if (!deviceName)
      cerr << ", device \"" << deviceName << '"';
    cerr << ", must be one of:\n";
    PStringList devices = PSoundChannel::GetDriversDeviceNames("*", PSoundChannel::Recorder);
    for (PINDEX i = 0; i < devices.GetSize(); i++)
      cerr << "   " << devices[i] << '\n';
    cerr << endl;
    return false;
  }

  unsigned bufferCount = args.GetOptionString("record-buffers", "8").AsUnsigned();

  cout << "Audio Recorder ";
  if (!driverName.IsEmpty())
    cout << "driver \"" << driverName << "\" and ";
  cout << "device \"" << recorder->GetName() << "\" using "
       << bufferCount << 'x' << readSize << " byte buffers ";

  if (!recorder->SetBuffers(readSize, bufferCount)) {
    cout << "could not be set." << endl;
    return false;
  }

  cout << "opened and initialised." << endl;

  // Audio player
  driverName = args.GetOptionString("play-driver");
  deviceName = args.GetOptionString("play-device");
  player = PSoundChannel::CreateOpenedChannel(driverName, deviceName, PSoundChannel::Player);
  if (player == NULL) {
    cerr << "Cannot use ";
    if (driverName.IsEmpty() && deviceName.IsEmpty())
      cerr << "default ";
    cerr << "audio player";
    if (!driverName)
      cerr << ", driver \"" << driverName << '"';
    if (!deviceName)
      cerr << ", device \"" << deviceName << '"';
    cerr << ", must be one of:\n";
    PStringList devices = PSoundChannel::GetDriversDeviceNames("*", PSoundChannel::Player);
    for (PINDEX i = 0; i < devices.GetSize(); i++)
      cerr << "   " << devices[i] << '\n';
    cerr << endl;
    return false;
  }

  bufferCount = args.GetOptionString("play-buffers", "8").AsUnsigned();

  cout << "Audio Player ";
  if (!driverName.IsEmpty())
    cout << "driver \"" << driverName << "\" and ";
  cout << "device \"" << player->GetName() << "\" using "
       << bufferCount << 'x' << readSize << " byte buffers ";

  if (!player->SetBuffers(readSize, bufferCount)) {
    cout << "could not be set." << endl;
    return false;
  }

  if (encoder == NULL) 
    frameTime = mediaFormat.GetFrameTime();
  else {
    OpalMediaFormat mediaFormat = encoder->GetOutputFormat();
    SetOptions(args, mediaFormat);
    encoder->UpdateMediaFormats(OpalMediaFormat(), mediaFormat);
  }

  cout << "opened and initialised." << endl;
  running = true;

  return true;
}


bool VideoThread::Initialise(PArgList & args)
{
  switch (InitialiseCodec(args, OpalYUV420P)) {
    case 0 :
      return false;
    case 1 :
      return true;
  }

  OpalMediaFormat mediaFormat = encoder != NULL ? encoder->GetOutputFormat() : OpalYUV420P;

  cout << "Video media format set to " << mediaFormat << endl;

  // Video grabber
  PString driverName = args.GetOptionString("grab-driver");
  PString deviceName = args.GetOptionString("grab-device");
  grabber = PVideoInputDevice::CreateOpenedDevice(driverName, deviceName, FALSE);
  if (grabber == NULL) {
    cerr << "Cannot use ";
    if (driverName.IsEmpty() && deviceName.IsEmpty())
      cerr << "default ";
    cerr << "video grab";
    if (!driverName)
      cerr << ", driver \"" << driverName << '"';
    if (!deviceName)
      cerr << ", device \"" << deviceName << '"';
    cerr << ", must be one of:\n";
    PStringList devices = PVideoInputDevice::GetDriversDeviceNames("*");
    for (PINDEX i = 0; i < devices.GetSize(); i++)
      cerr << "   " << devices[i] << '\n';
    cerr << endl;
    return false;
  }

  cout << "Video Grabber ";
  if (!driverName.IsEmpty())
    cout << "driver \"" << driverName << "\" and ";
  cout << "device \"" << grabber->GetDeviceName() << "\" opened." << endl;


  if (args.HasOption("grab-format")) {
    PVideoDevice::VideoFormat format;
    PCaselessString formatString = args.GetOptionString("grab-format");
    if (formatString == "PAL")
      format = PVideoDevice::PAL;
    else if (formatString == "NTSC")
      format = PVideoDevice::NTSC;
    else if (formatString == "SECAM")
      format = PVideoDevice::SECAM;
    else if (formatString == "Auto")
      format = PVideoDevice::Auto;
    else {
      cerr << "Illegal video grabber format name \"" << formatString << '"' << endl;
      return false;
    }
    if (!grabber->SetVideoFormat(format)) {
      cerr << "Video grabber device could not be set to input format \"" << formatString << '"' << endl;
      return false;
    }
  }
  cout << "Grabber input format set to " << grabber->GetVideoFormat() << endl;

  if (args.HasOption("grab-channel")) {
    int videoInput = args.GetOptionString("grab-channel").AsInteger();
    if (!grabber->SetChannel(videoInput)) {
      cerr << "Video grabber device could not be set to channel " << videoInput << endl;
      return false;
    }
  }
  cout << "Grabber channel set to " << grabber->GetChannel() << endl;

  
  if (args.HasOption("frame-rate"))
    frameRate = args.GetOptionString("frame-rate").AsUnsigned();
  else
    frameRate = grabber->GetFrameRate();

  mediaFormat.SetOptionInteger(OpalVideoFormat::FrameTimeOption(), mediaFormat.GetClockRate()/frameRate);

  if (!grabber->SetFrameRate(frameRate)) {
    cerr << "Video grabber device could not be set to frame rate " << frameRate << endl;
    return false;
  }

  cout << "Grabber frame rate set to " << grabber->GetFrameRate() << endl;

  // Video display
  driverName = args.GetOptionString("display-driver");
  deviceName = args.GetOptionString("display-device");
  display = PVideoOutputDevice::CreateOpenedDevice(driverName, deviceName, FALSE);
  if (display == NULL) {
    cerr << "Cannot use ";
    if (driverName.IsEmpty() && deviceName.IsEmpty())
      cerr << "default ";
    cerr << "video display";
    if (!driverName)
      cerr << ", driver \"" << driverName << '"';
    if (!deviceName)
      cerr << ", device \"" << deviceName << '"';
    cerr << ", must be one of:\n";
    PStringList devices = PVideoOutputDevice::GetDriversDeviceNames("*");
    for (PINDEX i = 0; i < devices.GetSize(); i++)
      cerr << "   " << devices[i] << '\n';
    cerr << endl;
    return false;
  }

  cout << "Display ";
  if (!driverName.IsEmpty())
    cout << "driver \"" << driverName << "\" and ";
  cout << "device \"" << display->GetDeviceName() << "\" opened." << endl;

  // Configure sizes/speeds
  unsigned width, height;
  if (args.HasOption("frame-size")) {
    PString sizeString = args.GetOptionString("frame-size");
    if (!PVideoFrameInfo::ParseSize(sizeString, width, height)) {
      cerr << "Illegal video frame size \"" << sizeString << '"' << endl;
      return false;
    }
  }
  else
    grabber->GetFrameSize(width, height);

  mediaFormat.SetOptionInteger(OpalVideoFormat::FrameWidthOption(), width);
  mediaFormat.SetOptionInteger(OpalVideoFormat::FrameHeightOption(), height);

  PVideoFrameInfo::ResizeMode resizeMode = args.HasOption("crop") ? PVideoFrameInfo::eCropCentre : PVideoFrameInfo::eScale;
  if (!grabber->SetFrameSizeConverter(width, height, resizeMode)) {
    cerr << "Video grabber device could not be set to size " << width << 'x' << height << endl;
    return false;
  }
  cout << "Grabber frame size set to " << grabber->GetFrameWidth() << 'x' << grabber->GetFrameHeight() << endl;

  if  (!display->SetFrameSizeConverter(width, height, resizeMode)) {
    cerr << "Video display device could not be set to size " << width << 'x' << height << endl;
    return false;
  }

  cout << "Display frame size set to " << display->GetFrameWidth() << 'x' << display->GetFrameHeight() << endl;

  if (!grabber->SetColourFormatConverter("YUV420P") ) {
    cerr << "Video grabber device could not be set to colour format YUV420P" << endl;
    return false;
  }

  cout << "Grabber colour format set to " << grabber->GetColourFormat() << " (";
  if (grabber->GetColourFormat() == "YUV420P")
    cout << "native";
  else
    cout << "converted to YUV420P";
  cout << ')' << endl;

  if (!display->SetColourFormatConverter("YUV420P")) {
    cerr << "Video display device could not be set to colour format YUV420P" << endl;
    return false;
  }

  cout << "Display colour format set to " << display->GetColourFormat() << " (";
  if (display->GetColourFormat() == "YUV420P")
    cout << "native";
  else
    cout << "converted from YUV420P";
  cout << ')' << endl;


  if (args.HasOption("bit-rate")) {
    PString str = args.GetOptionString("bit-rate");
    double bitrate = str.AsReal();
    switch (str[str.GetLength()-1]) {
      case 'K' :
      case 'k' :
        bitrate *= 1000;
        break;
      case 'M' :
      case 'm' :
        bitrate *= 1000000;
        break;
    }
    if (bitrate < 100 || bitrate > INT_MAX) {
      cerr << "Could not set bit rate to " << str << endl;
      return false;
    }
    mediaFormat.SetOptionInteger(OpalVideoFormat::TargetBitRateOption(), (unsigned)bitrate);
    if (mediaFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption()) < bitrate)
      mediaFormat.SetOptionInteger(OpalVideoFormat::MaxBitRateOption(), (unsigned)bitrate);
  }
  cout << "Target bit rate set to " << mediaFormat.GetOptionInteger(OpalVideoFormat::TargetBitRateOption()) << " bps" << endl;

  SetOptions(args, mediaFormat);

  unsigned bitRate = mediaFormat.GetOptionInteger(OpalVideoFormat::TargetBitRateOption());
  PString rc = args.GetOptionString('C');
  if (rc.IsEmpty()) 
    rateController = NULL;
  else {
    rateController = PFactory<OpalVideoRateController>::CreateInstance(rc);
    if (rateController != NULL) {
      rateController->Open(mediaFormat);
      cout << "Video rate controller enabled for bit rate " << bitRate << " bps and frame rate " << frameRate << endl;
    }
  }

  if (args.HasOption('T'))
    frameFn = "frame_stats.csv";

  frameTime = mediaFormat.GetFrameTime();
  if (encoder != NULL) {
    encoder->UpdateMediaFormats(OpalMediaFormat(), mediaFormat);
    if (args.HasOption('p'))
      encoder->SetMaxOutputSize(args.GetOptionString('p').AsUnsigned());
  }

  singleStep = args.HasOption("single-step");

  sumYSNR = sumCbSNR = sumCrSNR = 0.0;
  snrCount = 0;
  running = true;

  m_dropPercent = args.GetOptionString('d').AsInteger();
  cout << "Dropping " << m_dropPercent << "% of encoded frames" << endl;

  return true;
}


void AudioThread::Main()
{
  if (recorder == NULL || player == NULL) {
    cerr << "Audio cannot open recorder or player" << endl;
    return;
  }

  TranscoderThread::Main();
}


void VideoThread::Main()
{
  if (grabber == NULL || display == NULL) {
    cerr << "Video cannot open grabber or display" << endl;
    return;
  }

  grabber->Start();
  display->Start();

  //decoder->SetCommandNotifier(PCREATE_NOTIFIER(OnTranscoderCommand));

  m_forceIFrame = false;
  TranscoderThread::Main();
}

void TranscoderThread::OnTranscoderCommand(OpalMediaCommand & cmd, INT)
{
  if (PIsDescendant(&cmd, OpalVideoUpdatePicture)) {
    coutMutex.Wait();
    cout << "decoder lost sync" << endl;
    coutMutex.Signal();
    m_forceIFrame = true;
    ((OpalVideoTranscoder *)encoder)->ForceIFrame();
  }
  else {
    coutMutex.Wait();
    cout << "unknown decoder command " << cmd.GetName() << endl;
    coutMutex.Signal();
  }
}

void VideoThread::CalcSNR(const RTP_DataFrame & dst)
{
  while (snrSourceFrames.size() > 0) {
    const RTP_DataFrame * src = snrSourceFrames.front();
    snrSourceFrames.pop();
    if (src->GetTimestamp() == dst.GetTimestamp()) {
      CalcSNR(*src, dst);
      delete src;
      return;
    }
    delete src;
  }
}

void VideoThread::CalcSNR(const RTP_DataFrame & src, const RTP_DataFrame & dst)
{
  if (src.GetPayloadSize() < (PINDEX)sizeof(OpalVideoTranscoder::FrameHeader) ||
      dst.GetPayloadSize() < (PINDEX)sizeof(OpalVideoTranscoder::FrameHeader))
    return;

  const BYTE * src1 = src.GetPayloadPtr();
  const BYTE * src2 = dst.GetPayloadPtr();

  const OpalVideoTranscoder::FrameHeader * hdr1 = (OpalVideoTranscoder::FrameHeader *)src1;
  const OpalVideoTranscoder::FrameHeader * hdr2 = (OpalVideoTranscoder::FrameHeader *)src2;

  if (hdr1->height != hdr2->height || hdr1->width != hdr2->width)
    return;

  if (hdr1->height != snrHeight || hdr1->width != snrWidth) {
    sumYSNR = sumCbSNR = sumCrSNR = 0.0;
    snrCount = 0;
    snrHeight = hdr1->height;
    snrWidth  = hdr1->width;
  }

  unsigned size = snrHeight * snrWidth;

  int tsize = sizeof(OpalVideoTranscoder::FrameHeader) + size*3/2;

  if (src.GetPayloadSize() < tsize || dst.GetPayloadSize() < tsize)
    return;

  src1 += sizeof(OpalVideoTranscoder::FrameHeader);
  src2 += sizeof(OpalVideoTranscoder::FrameHeader);

  sumYSNR  = ::CalcSNR(src1, src2, size);
  src1 += size;
  src2 += size;

  size = size / 4;
  sumCbSNR = ::CalcSNR(src1, src2, size);
  src1 += size;
  src2 += size;

  sumCrSNR = ::CalcSNR(src1, src2, size);

  ++snrCount;
}

void VideoThread::ReportSNR()
{
  coutMutex.Wait();

  /* The PSNR is the mean of the sum of squares of the differences,
     normalized to the range 0..1
  */
  double const yPsnr = sumYSNR / snrCount;

  cout << "Y  color component ";
  if (yPsnr <= 1e-9)
    cout << "identical";
  else
    cout << setprecision(2) << (10 * log10(1/yPsnr)) << " dB";
  cout << endl;

  double const cbPsnr = sumCbSNR / snrCount;

  cout << "Cb color component ";
  if (cbPsnr <= 1e-9)
    cout << "identical";
  else
    cout << setprecision(2) << (10 * log10(1/cbPsnr)) << " dB";
  cout << endl;

  double const crPsnr = sumCrSNR / snrCount;

  cout << "Cr color component ";
  if (crPsnr <= 1e-9)
    cout << "identical";
  else
    cout << setprecision(2) << (10 * log10(1/crPsnr)) << " dB";
  cout << endl;

  coutMutex.Signal();
}

struct FrameHistoryEntry {
  FrameHistoryEntry(unsigned size_, PInt64 timeStamp_, bool iFrame_)
    : size(size_), timeStamp(timeStamp_), iFrame(iFrame_)
  { }

  unsigned size;
  PInt64 timeStamp;
  bool iFrame;
};

void TranscoderThread::Main()
{
  PUInt64 maximumBitRate = 0;
  PUInt64 totalInputFrameCount = 0;
  PUInt64 totalEncodedPacketCount = 0;
  PUInt64 totalOutputFrameCount = 0;
  PUInt64 totalEncodedByteCount = 0;
  PUInt64 skippedFrames = 0;
  PUInt64 totalDroppedPacketCount = 0;

  bool oldSrcState = true;
  bool oldOutState = true;
  bool oldEncState = true;
  bool oldDecState = true;

  WORD sequenceNumber = 0;

  bool isVideo = PIsDescendant(encoder, OpalVideoTranscoder);

  PTimeInterval startTick = PTimer::Tick();

  if (isVideo) {
    ((VideoThread *)this)->m_bitRateCalc.SetQuanta(frameTime/90);
  }

  //////////////////////////////////////////////
  //
  // main loop
  //

  RTP_DataFrame * srcFrame_ = NULL;

  while ((running && framesToTranscode < 0) || (framesToTranscode-- > 0)) {

    //////////////////////////////////////////////
    //
    //  acquire and format source frame
    //
    if (srcFrame_ != NULL)
      delete srcFrame_;
    srcFrame_ = new RTP_DataFrame(0);
    RTP_DataFrame & srcFrame = *srcFrame_;

    {
      bool state = Read(srcFrame);
      if (oldSrcState != state) {
        oldSrcState = state;
        cerr << "Source " << (state ? "restor" : "fail") << "ed at frame " << totalInputFrameCount << endl;
        break;
      }
    }

    srcFrame.SetTimestamp(timestamp);
    timestamp += frameTime;
    ++totalInputFrameCount;

    //////////////////////////////////////////////
    //
    //  allow the rate controller to skip frames
    //
    bool rateControlForceIFrame = false;
    if (rateController != NULL && rateController->SkipFrame(rateControlForceIFrame)) {
      coutMutex.Wait();
      cout << "Packet pacer forced skip of input frame " << totalInputFrameCount-1 << endl;
      coutMutex.Signal();
      ++skippedFrames;
    }

    else {

      //////////////////////////////////////////////
      //
      //  push frames through encoder
      //
      RTP_DataFrameList encFrames;
      if (encoder == NULL) 
        encFrames.Append(new RTP_DataFrame(srcFrame)); 
      else {
        if (isVideo) {
          if (m_forceIFrame) {
            coutMutex.Wait();
            cout << "Decoder forced I-frame at input frame " << totalInputFrameCount-1 << endl;
            coutMutex.Signal();
          }
          if (rateControlForceIFrame) {
            coutMutex.Wait();
            cout << "Rate controller forced I-frame at input frame " << totalInputFrameCount-1 << endl;
            coutMutex.Signal();
          }
          if (m_forceIFrame || rateControlForceIFrame)
            ((OpalVideoTranscoder *)encoder)->ForceIFrame();
        }

        bool state = encoder->ConvertFrames(srcFrame, encFrames);
        if (oldEncState != state) {
          oldEncState = state;
          cerr << "Encoder " << (state ? "restor" : "fail") << "ed at input frame " << totalInputFrameCount-1 << endl;
          continue;
        }
        if (isVideo && ((OpalVideoTranscoder *)encoder)->WasLastFrameIFrame()) {
          coutMutex.Wait();
          cout << "Encoder returned I-Frame at input frame " << totalInputFrameCount-1 << endl;
          coutMutex.Signal();
        }
      }

      //////////////////////////////////////////////
      //
      //  re-format encoded frames
      //
      unsigned long encodedPayloadSize = 0;
      unsigned long encodedPacketCount = 0;
      unsigned long encodedDataSize    = 0;
      for (PINDEX i = 0; i < encFrames.GetSize(); i++) {
        encFrames[i].SetSequenceNumber(++sequenceNumber);
        ++encodedPacketCount;
        encodedPayloadSize += encFrames[i].GetPayloadSize();
        encodedDataSize    += encFrames[i].GetPayloadSize() + encFrames[i].GetHeaderSize();
        switch (markerHandling) {
          case SuppressMarkers :
            encFrames[i].SetMarker(false);
            break;
          case ForceMarkers :
            encFrames[i].SetMarker(true);
            break;
          default :
            break;
        }

        if (g_infoCount > 0) {
          RTP_DataFrame & rtp = encFrames[i];
          coutMutex.Wait();
          if (g_infoCount > 1) 
            cout << "Inframe=" << totalInputFrameCount << ",outframe=#" << i << ":pt=" << rtp.GetPayloadType() << ",psz=" << rtp.GetPayloadSize() << ",m=" << (rtp.GetMarker() ? "1" : "0") << ",";
          cout << "ssrc=" << hex << rtp.GetSyncSource() << dec << ",ts=" << rtp.GetTimestamp() << ",seq = " << rtp.GetSequenceNumber();
          if (g_infoCount > 2) {
            cout << "\n   data=";
            cout << hex << setfill('0') << ::setw(2);
            for (PINDEX i = 0; i < PMIN(10, rtp.GetPayloadSize()); ++i)
              cout << (int)rtp.GetPayloadPtr()[i] << ' ';
            cout << dec << setfill(' ') << ::setw(0);
          }
          cout << endl;
          coutMutex.Signal();
        }
      }

      totalEncodedPacketCount += encFrames.GetSize();

      if (isVideo && calcSNR) {
        ((VideoThread *)this)->SaveSNRFrame(srcFrame_);
        srcFrame_ = NULL;
      }

      //////////////////////////////////////////////
      //
      //  drop encoded frames if required
      //
      if (m_dropPercent > 0) {
        PINDEX i = 0;
        while (i < encFrames.GetSize()) {
          int n = PRandom::Number() % 100; 
          if (n >= m_dropPercent) 
            i++;
          else {
            ++totalDroppedPacketCount;
            encFrames.RemoveAt(i);
          }
        }
      }

      //////////////////////////////////////////////
      //
      //  push audio/video frames through NULL decoder
      //
      if (encoder == NULL) {
        totalEncodedByteCount += encodedPayloadSize;
        RTP_DataFrameList outFrames;
        outFrames = encFrames;
        if (outFrames.GetSize() != 1)
          cerr << "NULL decoder returned != 1 output frame for input frame " << totalInputFrameCount-1 << endl;
        else {
          bool state = Write(outFrames[0]);
          if (oldOutState != state) {
            oldOutState = state;
            cerr << "Output write " << (state ? "restor" : "fail") << "ed at input frame " << totalInputFrameCount << endl;
          }
          totalOutputFrameCount++;
        }
      }

      //////////////////////////////////////////////
      //
      //  push audio/video frames through explicit decoder
      //
      else if (rateController == NULL) {
        totalEncodedByteCount += encodedPayloadSize;
        if (isVideo)
          ((VideoThread *)this)->CalcVideoPacketStats(encFrames, ((OpalVideoTranscoder *)decoder)->WasLastFrameIFrame());
        RTP_DataFrameList outFrames;
        for (PINDEX i = 0; i < encFrames.GetSize(); i++) {
          bool state = decoder->ConvertFrames(encFrames[i], outFrames);
          if (oldDecState != state) {
            oldDecState = state;
            cerr << "Decoder " << (state ? "restor" : "fail") << "ed at input frame " << totalInputFrameCount-1 << endl;
            continue;
          }
          if (outFrames.GetSize() > 1) 
            cerr << "Non rate controlled video decoder returned != 1 output frame for input frame " << totalInputFrameCount-1 << endl;
          else if (outFrames.GetSize() == 1) {
            if (isVideo) {
              if (((OpalVideoTranscoder *)decoder)->WasLastFrameIFrame()) {
                coutMutex.Wait();
                cout << "Decoder returned I-Frame at output frame " << totalOutputFrameCount << endl;
                coutMutex.Signal();
              } 
              if (g_infoCount > 1) {
                coutMutex.Wait();
                cout << "Decoder generated payload of size " << outFrames[0].GetPayloadSize() << endl;
                coutMutex.Signal();
              }
            }
            bool state = Write(outFrames[0]);
            if (oldOutState != state) {
              oldOutState = state;
              cerr << "Output write " << (state ? "restor" : "fail") << "ed at input frame " << totalInputFrameCount << endl;
            }
            if (isVideo && calcSNR) 
              ((VideoThread *)this)->CalcSNR(outFrames[0]);
            totalOutputFrameCount++;
          }
        }
      }

      //////////////////////////////////////////////
      //
      //  push video frames into rate controller 
      //
      else 
        rateController->Push(encFrames, ((OpalVideoTranscoder *)encoder)->WasLastFrameIFrame());
    }

    //////////////////////////////////////////////
    //
    //  pop video frames from rate controller and explicit decoder
    //
    if (rateController != NULL) {
      for (;;) {
        bool outIFrame;
        RTP_DataFrameList pacedFrames;
        if (!rateController->Pop(pacedFrames, outIFrame, false))
          break;
        ((VideoThread *)this)->CalcVideoPacketStats(pacedFrames, outIFrame);
        RTP_DataFrameList outFrames;
        for (PINDEX i = 0; i < pacedFrames.GetSize(); i++) {
          totalEncodedByteCount += pacedFrames[i].GetPayloadSize();
          bool state = decoder->ConvertFrames(pacedFrames[i], outFrames);
          if (oldDecState != state) {
            oldDecState = state;
            cerr << "Decoder " << (state ? "restor" : "fail") << "ed at input frame " << totalInputFrameCount-1 << endl;
            continue;
          }
          if (outFrames.GetSize() > 1) 
            cerr << "Rate controlled video decoder returned > 1 output frame for input frame " << totalInputFrameCount-1 << endl;
          else if (outFrames.GetSize() == 1) {
            if (((OpalVideoTranscoder *)decoder)->WasLastFrameIFrame()) {
              coutMutex.Wait();
              cout << "Decoder returned I-Frame at output frame " << totalOutputFrameCount << endl;
              coutMutex.Signal();
            }
            bool state = Write(outFrames[0]);
            if (oldOutState != state) {
              oldOutState = state;
              cerr << "Output write " << (state ? "restor" : "fail") << "ed at input frame " << totalInputFrameCount-1 << endl;
            }
            if (isVideo && calcSNR) 
              ((VideoThread *)this)->CalcSNR(outFrames[0]);
            totalOutputFrameCount++;
          }
        }
      }
    }

    {
      PTimeInterval duration = PTimer::Tick() - startTick;
      PInt64 msecs = duration.GetMilliSeconds();
      if (msecs < frameTime / 90)
        msecs = frameTime / 90;
      PUInt64 bitRate = totalEncodedByteCount*8*1000/msecs;
      if (bitRate > maximumBitRate)
        maximumBitRate = bitRate;
    }

    if (pause.Wait(0)) {
      pause.Acknowledge();
      resume.Wait();
    }
  }

  //
  //  end of main loop
  //
  ///////////////////////////////////////////////////////

  PTimeInterval duration = PTimer::Tick() - startTick;

  PProcess::Times cpuTimes;
  GetTimes(cpuTimes);

  coutMutex.Wait();

  cout << fixed << setprecision(1);
  if (totalEncodedByteCount < 10000ULL)
    cout << totalEncodedByteCount << ' ';
  else if (totalEncodedByteCount < 10000000ULL)
    cout << totalEncodedByteCount/1000.0 << " k";
  else if (totalEncodedByteCount < 10000000000ULL)
    cout << totalEncodedByteCount /1000000.0 << " M";
  cout << "bytes,"
       << totalEncodedPacketCount << " packets,"
       << totalInputFrameCount << " frames over " << duration << " seconds at "
       << (totalInputFrameCount*1000.0/duration.GetMilliSeconds()) << " fps" << endl;

  if (m_dropPercent > 0) {
    cout << totalDroppedPacketCount << " dropped frames(" << totalDroppedPacketCount*100.0/totalEncodedPacketCount << "%)" << endl;
  }

  cout << "Average bit rate = ";
  {
    PInt64 msecs = duration.GetMilliSeconds();
    if (msecs == 0) 
      cout << "N/A";
    else {
      PUInt64 bitRate = totalEncodedByteCount*8*1000/msecs;
      OUTPUT_BPS(cout, bitRate);
    }
  }

  cout << ",max bit rate = ";
  if (maximumBitRate == 0)
    cout << "N/A";
  else {
    OUTPUT_BPS(cout, maximumBitRate);
  }

  cout << "," << skippedFrames << " skipped frames (" << (skippedFrames * 100.0)/totalInputFrameCount << "%)";
  cout << endl;

  cout << "CPU used: " << cpuTimes << endl;

  coutMutex.Signal();

  if (calcSNR) 
    ReportSNR();
}


bool AudioThread::Read(RTP_DataFrame & frame)
{
  frame.SetPayloadSize(readSize);
  return recorder->Read(frame.GetPayloadPtr(), readSize);
}


bool AudioThread::Write(const RTP_DataFrame & frame)
{
  return player->Write(frame.GetPayloadPtr(), frame.GetPayloadSize());
}


void AudioThread::Stop()
{
  if (running) {
    running = false;
    WaitForTermination();
  }
}


bool VideoThread::Read(RTP_DataFrame & data)
{
  if (singleStep)
    frameWait.Wait();

  data.SetPayloadSize(grabber->GetMaxFrameBytes()+sizeof(OpalVideoTranscoder::FrameHeader));
  data.SetMarker(TRUE);

  unsigned width, height;
  grabber->GetFrameSize(width, height);
  OpalVideoTranscoder::FrameHeader * frame = (OpalVideoTranscoder::FrameHeader *)data.GetPayloadPtr();
  frame->x = frame->y = 0;
  frame->width = width;
  frame->height = height;
  data.SetPayloadSize(sizeof(OpalVideoTranscoder::FrameHeader) + width*height*3/2);

  return grabber->GetFrameData(OPAL_VIDEO_FRAME_DATA_PTR(frame));
}


bool VideoThread::Write(const RTP_DataFrame & data)
{
  const OpalVideoTranscoder::FrameHeader * frame = (const OpalVideoTranscoder::FrameHeader *)data.GetPayloadPtr();
  display->SetFrameSize(frame->width, frame->height);
  return display->SetFrameData(frame->x, frame->y,
                               frame->width, frame->height,
                               OPAL_VIDEO_FRAME_DATA_PTR(frame), data.GetMarker());
}


void VideoThread::Stop()
{
  if (running) {
    running = false;
    frameWait.Signal();
    WaitForTermination();
  }
}


void VideoThread::CalcVideoPacketStats(const RTP_DataFrameList & packets, bool isIFrame)
{
  static unsigned maximumBitRate = 0;
  static unsigned maximumAvgBitRate = 0;

  for (PINDEX i = 0; i < packets.GetSize(); ++i) {
    
    //m_bitRateCalc.AddPacket(packets[i].GetPayloadSize());

    if (rateController != NULL) {
      OpalBitRateCalculator & m_bitRateCalc = rateController->m_bitRateCalc;

      unsigned r = m_bitRateCalc.GetBitRate();
      if (r > maximumBitRate)
        maximumBitRate = r;

      unsigned a = m_bitRateCalc.GetAverageBitRate();
      if (a > maximumAvgBitRate)
        maximumAvgBitRate = a;

      PStringStream str;
      str << "index=" << (packets[i].GetTimestamp() / 3600)
          << ",ps=" << packets[i].GetPayloadSize()
          << ",rate=" << r
          << ",avg=" << a 
          << ",maxrate=" << maximumBitRate
          << ",maxavg=" << maximumAvgBitRate 
          << ",f=" << (isIFrame?"I":"") << (packets[i].GetMarker()?"M":"");
      ((VideoThread *)this)->WriteFrameStats(str);
    }
  }
}


void VideoThread::WriteFrameStats(const PString & str)
{
  if (!frameFn.IsEmpty()) {
    frameStatFile.Open(frameFn, PFile::WriteOnly);
    frameFn.MakeEmpty();
  }

  if (frameStatFile.IsOpen()) 
    frameStatFile << str << endl;
}

PInt64 BpsTokbps(PInt64 Bps)
{
  return ((Bps * 8) + 500) / 1000;
}

// End of File ///////////////////////////////////////////////////////////////
