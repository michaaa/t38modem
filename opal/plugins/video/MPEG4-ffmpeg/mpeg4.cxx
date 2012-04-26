/*
 * MPEG4 Plugin codec for OpenH323/OPAL
 * This code is based on the H.263 plugin implementation in OPAL.
 *
 * The original files, and this version of the original code, are released under the same 
 * MPL 1.0 license. Substantial portions of the original code were contributed
 * by Salyens and March Networks and their right to be identified as copyright holders
 * of the original code portions and any parts now included in this new copy is asserted through 
 * their inclusion in the copyright notices below.
 *
 * Copyright (C) 2007 Canadian Bank Note, Limited
 * Copyright (C) 2006 Post Increment
 * Copyright (C) 2005 Salyens
 * Copyright (C) 2001 March Networks Corporation
 * Copyright (C) 1999-2000 Equivalence Pty. Ltd.
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): Josh Mahonin (jmahonin@cbnco.com)
 *                 Michael Smith (msmith@cbnco.com)
 *                 Guilhem Tardy (gtardy@salyens.com)
 *                 Craig Southeren (craigs@postincrement.com)
 *                 Matthias Schneider (ma30002000@yahoo.de)
 *
 * NOTES:
 * Initial implementation of MPEG4 codec plugin using ffmpeg.
 * Untested under Windows or H.323
 *
 * $Revision: 21208 $
 * $Author: ms30002000 $
 * $Date: 2008-10-01 21:24:00 +0200 (Mi, 01. Okt 2008) $
 */

/*
  Notes
  -----

  This codec implements an MPEG4 encoder/decoder using the ffmpeg library
  (http://ffmpeg.mplayerhq.hu/).  Plugin infrastructure is based off of the
  H.263 plugin and MPEG4 codec functions originally created by Michael Smith.

 */

// Plugin specific
#define _CRT_SECURE_NO_DEPRECATE

#include "plugin-config.h"

#include <codec/opalplugin.h>

extern "C" {
PLUGIN_CODEC_IMPLEMENT(FFMPEG_MPEG4)
};


#include <stdlib.h>
#include "trace.h"
#include "dyna.h"
#include "rtpframe.h"

typedef unsigned char BYTE;

// Needed C++ headers
#include <string.h>
#include <stdint.h>             // for uint8_t
#include <math.h>               // for rint() in dsputil.h
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <deque>

using namespace std;

// FFMPEG specific headers
extern "C" {

// Public ffmpeg headers.
// We'll pull them in from their locations in the ffmpeg source tree,
// but it would be possible to get them all from /usr/include/ffmpeg
// with #include <ffmpeg/...h>.
#ifdef LIBAVCODEC_HAVE_SOURCE_DIR
#include <libavutil/common.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>

// Private headers from the ffmpeg source tree.
#include <libavutil/intreadwrite.h>
#include <libavutil/bswap.h>
#include <libavcodec/mpegvideo.h>

#else /* LIBAVCODEC_HAVE_SOURCE_DIR */
#include LIBAVCODEC_HEADER
#endif /* LIBAVCODEC_HAVE_SOURCE_DIR */
}

#define RTP_DYNAMIC_PAYLOAD  96

#define MPEG4_CLOCKRATE     90000
#define MPEG4_BITRATE       3000000

#define CIF_WIDTH       352
#define CIF_HEIGHT      288

#define CIF4_WIDTH      (CIF_WIDTH*2)
#define CIF4_HEIGHT     (CIF_HEIGHT*2)

#define CIF16_WIDTH     (CIF_WIDTH*4)
#define CIF16_HEIGHT    (CIF_HEIGHT*4)

#define QCIF_WIDTH     (CIF_WIDTH/2)
#define QCIF_HEIGHT    (CIF_HEIGHT/2)

#define SQCIF_WIDTH     128
#define SQCIF_HEIGHT    96

#define MAX_MPEG4_PACKET_SIZE     2048

const static struct mpeg4_profile_level {
    unsigned profileLevel;
    const char* profileName;
    unsigned profileNumber;
    unsigned level;
    unsigned maxQuantTables;       /* Max. unique quant. tables */
    unsigned maxVMVBufferSize;     /* max. VMV buffer size(MB units) */
    unsigned frame_size;           /* max. VCV buffer size (MB) */
    unsigned mbps;                 /* VCV decoder rate (MB/s) 4 */
    unsigned boundaryMbps;         /* VCV boundary MB decoder rate (MB/s)9 */
    unsigned maxBufferSize;        /* max. total VBV buffer size (units of 16384 bits) */
    unsigned maxVOLBufferSize;     /* max. VOL VBV buffer size (units of 16384 bits) */
    unsigned maxVideoPacketLength; /* max. video packet length (bits) */
    long unsigned bitrate;
} mpeg4_profile_levels[] = {
    {   1, "Simple",                     1, 1, 1,   198,    99,   1485,      0,  10,  10,  2048,    64000 },
    {   2, "Simple",                     1, 2, 1,   792,   396,   5940,      0,  40,  40,  4096,   128000 },
    {   3, "Simple",                     1, 3, 1,   792,   396,  11880,      0,  40,  40,  8192,   384000 },
    {   4, "Simple",                     1, 4, 1,  2400,  1200,  36000,      0,  80,  80, 16384,  4000000 }, // is really 4a
    {   5, "Simple",                     1, 5, 1,  3240,  1620,  40500,      0, 112, 112, 16384,  8000000 },
    {   8, "Simple",                     1, 0, 1,   198,    99,   1485,      0,  10,  10,  2048,    64000 },
    {   9, "Simple",                     1, 0, 1,   198,    99,   1485,      0,  20,  20,  2048,   128000 }, // 0b
    {  17, "Simple Scalable",            2, 1, 1,  1782,   495,   7425,      0,  40,  40,  2048,   128000 },
    {  18, "Simple Scalable",            2, 2, 1,  3168,   792,  23760,      0,  40,  40,  4096,   256000 },
    {  33, "Core",                       3, 1, 4,   594,   198,   5940,   2970,  16,  16,  4096,   384000 },
    {  34, "Core",                       3, 2, 4,  2376,   792,  23760,  11880,  80,  80,  8192,  2000000 },
    {  50, "Main",                       4, 2, 4,  3960,  1188,  23760,  11880,  80,  80,  8192,  2000000 },
    {  51, "Main",                       4, 3, 4, 11304,  3240,  97200,  48600, 320, 320, 16384, 15000000 },
    {  52, "Main",                       4, 4, 4, 65344, 16320, 489600, 244800, 760, 760, 16384, 38400000 },
    {  66, "N-Bit",                      5, 2, 4,  2376,   792,  23760,  11880,  80,  80,  8192,  2000000 },
    { 145, "Advanced Real Time Simple",  6, 1, 1,   198,    99,   1485,      0,  10,  10,  8192,    64000 },
    { 146, "Advanced Real Time Simple",  6, 2, 1,   792,   396,   5940,      0,  40,  40, 16384,   128000 },
    { 147, "Advanced Real Time Simple",  6, 3, 1,   792,   396,  11880,      0,  40,  40, 16384,   384000 },
    { 148, "Advanced Real Time Simple",  6, 4, 1,   792,   396,  11880,      0,  80,  80, 16384,  2000000 },
    { 161, "Core Scalable",              7, 1, 4,  2376,   792,  14850,   7425,  64,  64,  4096,   768000 },
    { 162, "Core Scalable",              7, 2, 4,  2970,   990,  29700,  14850,  80,  80,  4096,  1500000 },
    { 163, "Core Scalable",              7, 3, 4, 12906,  4032, 120960,  60480,  80,  80, 16384,  4000000 },
    { 177, "Advanced Coding Efficiency", 8, 1, 4,  1188,   792,  11880,   5940,  40,  40,  8192,   384000 },
    { 178, "Advanced Coding Efficiency", 8, 2, 4,  2376,  1188,  23760,  11880,  80,  80,  8192,  2000000 },
    { 179, "Advanced Coding Efficiency", 8, 3, 4,  9720,  3240,  97200,  48600, 320, 320, 16384, 15000000 },
    { 180, "Advanced Coding Efficiency", 8, 4, 4, 48960, 16320, 489600, 244800, 760, 760, 16384, 38400000 },
    { 193, "Advanced Core",              9, 1, 4,   594,   198,   5940,   2970,  16,   8,  4096,   384000 },
    { 194, "Advanced Core",              9, 2, 4,  2376,   792,  23760,  11880,  80,  40,  8192,  2000000 },
    { 240, "Advanced Simple",           10, 0, 1,   297,    99,   2970,    100,  10,  10,  2048,   128000 },
    { 241, "Advanced Simple",           10, 1, 1,   297,    99,   2970,    100,  10,  10,  2048,   128000 },
    { 242, "Advanced Simple",           10, 2, 1,  1188,   396,   5940,    100,  40,  40,  4096,   384000 },
    { 243, "Advanced Simple",           10, 3, 1,  1188,   396,  11880,    100,  40,  40,  4096,   768000 },
    { 244, "Advanced Simple",           10, 4, 1,  2376,   792,  23760,     50,  80,  80,  8192,  3000000 },
    { 245, "Advanced Simple",           10, 5, 1,  4860,  1620,  48600,     25, 112, 112, 16384,  8000000 },
    { 0 }
};

// This table is used in order to select a different resolution if the desired one
// consists of more macroblocks than the level limit
const static struct mpeg4_resolution {
    unsigned width;
    unsigned height;
    unsigned macroblocks;
} mpeg4_resolutions[] = {
    { 1920, 1088, 8160 },
    { 1600, 1200, 7500 },
    { 1408, 1152, 6336 },
    { 1280, 1024, 5120 },
    { 1280,  720, 3600 },
    { 1024,  768, 3072 },
    {  800,  600, 1900 },
    {  704,  576, 1584 },
    {  640,  480, 1200 },
    {  352,  288,  396 },
    {  320,  240,  300 },
    {  176,  144,   99 },
    {  128,   96,   48 },
    { 0 }
};

FFMPEGLibrary FFMPEGLibraryInstance(CODEC_ID_MPEG4);

static void logCallbackFFMPEG (void* v, int level, const char* fmt , va_list arg) {
  char buffer[512];
  int severity = 0;
  if (v) {
    switch (level)
    {
      case AV_LOG_QUIET: severity = 0; break;
      case AV_LOG_ERROR: severity = 1; break;
      case AV_LOG_INFO:  severity = 4; break;
      case AV_LOG_DEBUG: severity = 4; break;
      default:           severity = 4; break;
    }
    snprintf(buffer, sizeof(buffer), "MPEG4\tFFMPEG\t");
    vsprintf(buffer + strlen(buffer), fmt, arg);
    if (strlen(buffer) > 0)
      buffer[strlen(buffer)-1] = 0;
    if (severity == 4) 
      { TRACE_UP (severity, buffer); }
    else
      { TRACE (severity, buffer); }
  }
}
    


static bool mpeg4IsIframe (BYTE * frameBuffer, unsigned int frameLen )
{
  bool isIFrame = false;
  unsigned i = 0;
  while ((i+4)<= frameLen) {
    if ((frameBuffer[i] == 0) && (frameBuffer[i+1] == 0) && (frameBuffer[i+2] == 1)) {
      if (frameBuffer[i+3] == 0xb0)
        TRACE_UP(4, "Found visual_object_sequence_start_code, Profile/Level is " << (unsigned) frameBuffer[i+4]);
      if (frameBuffer[i+3] == 0xb6) {
        unsigned vop_coding_type = (unsigned) ((frameBuffer[i+4] & 0xC0) >> 6);
        TRACE_UP(4, "Found vop_start_code, is vop_coding_type is " << vop_coding_type );
        if (vop_coding_type == 0)
          isIFrame = true;
        if (!Trace::CanTraceUserPlane(4))
	  return isIFrame;
      }
    }
    i++;	
  }
  return isIFrame;
}

/////////////////////////////////////////////////////////////////////////////
//
// define the encoding context
//

class MPEG4EncoderContext
{
  public:

    MPEG4EncoderContext();
    ~MPEG4EncoderContext();

    int EncodeFrames(const BYTE * src, unsigned & srcLen,
                     BYTE * dst, unsigned & dstLen, unsigned int & flags);
    static void RtpCallback(AVCodecContext *ctx, void *data, int data_size,
                            int num_mb);
    
    void SetIQuantFactor(float newFactor);
    void SetKeyframeUpdatePeriod(int interval);
    void SetMaxBitrate(int max);
    void SetFPS(int frameTime);
    void SetFrameHeight(int height);
    void SetFrameWidth(int width);
    void SetQMin(int qmin);
    void SetTSTO(unsigned tsto);
    void SetProfileLevel (unsigned profileLevel);
    int GetFrameBytes();

  protected:
    bool OpenCodec();
    void CloseCodec();

    // sets encoding paramters
    void SetDynamicEncodingParams(bool restartOnResize);
    void SetStaticEncodingParams();
    void ResizeEncodingFrame(bool restartCodec);

    // reset libavcodec rate control
    void ResetBitCounter(int spread);

    // Modifiable quantization factor.  Defaults to -0.8
    float _iQuantFactor;

    // Max VBV buffer size in bits.
    unsigned _maxBufferSize;

    // Interval in seconds between forced IFrame updates if enabled
    int _keyframeUpdatePeriod;


    // Modifiable upper limit for bits/s transferred
    int _bitRateHighLimit;

    // Frames per second.  Defaults to 24.
    int _targetFPS;
    
    // packet sizes generating in RtpCallback
    deque<unsigned> _packetSizes;
    unsigned _lastPktOffset;

    // raw and encoded frame buffers
    BYTE * _encFrameBuffer;
    unsigned int _encFrameLen;
    BYTE * _rawFrameBuffer;
    unsigned int _rawFrameLen;

    // codec, context and picture
    AVCodec        *_avcodec;
    AVCodecContext *_avcontext;
    AVFrame        *_avpicture;

    // encoding and frame settings
    unsigned _videoTSTO;
    int _videoQMin; // dynamic video quality min/max limits, 1..31

    int _frameNum;
    unsigned int _frameWidth;
    unsigned int _frameHeight;

    unsigned long _lastTimeStamp;
    bool _isIFrame;

    enum StdSize { 
      SQCIF, 
      QCIF, 
      CIF, 
      CIF4, 
      CIF16, 
      NumStdSizes,
      UnknownStdSize = NumStdSizes
    };

    static int GetStdSize(int width, int height)
    {
      static struct { 
        int width; 
        int height; 
      } StandardVideoSizes[NumStdSizes] = {
        {  128,   96}, // SQCIF
        {  176,  144}, // QCIF
        {  352,  288}, // CIF
        {  704,  576}, // 4CIF
        { 1408, 1152}, // 16CIF
      };

      int sizeIndex;
      for (sizeIndex = 0; sizeIndex < NumStdSizes; ++sizeIndex )
        if (StandardVideoSizes[sizeIndex].width == width
            && StandardVideoSizes[sizeIndex].height == height)
          return sizeIndex;
      return UnknownStdSize;
    }
};

/////////////////////////////////////////////////////////////////////////////
//
//  Encoding context constructor.  Sets some encoding parameters, registers the
//  codec and allocates the context and frame.
//

MPEG4EncoderContext::MPEG4EncoderContext() 
:   _encFrameBuffer(NULL),
    _rawFrameBuffer(NULL), 
    _avcodec(NULL),
    _avcontext(NULL),
    _avpicture(NULL)
{ 

  // Some sane default video settings
  _targetFPS = 24;
  _videoQMin = 2;
  _videoTSTO = 10;
  _iQuantFactor = -0.8f;
  _maxBufferSize = 112 * 16384;

  _keyframeUpdatePeriod = 0; 

  _frameNum = 0;
  _lastPktOffset = 0;
  
  _isIFrame = false;

  if (!FFMPEGLibraryInstance.IsLoaded()){
    return;
  }

  // Default frame size.  These may change after encoder_set_options
  _frameWidth  = CIF_WIDTH;
  _frameHeight = CIF_HEIGHT;
  _rawFrameLen = (_frameWidth * _frameHeight * 3) / 2;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Close the codec and free our pointers
//

MPEG4EncoderContext::~MPEG4EncoderContext()
{
  if (FFMPEGLibraryInstance.IsLoaded()) {
    CloseCodec();
  }
  if (_rawFrameBuffer) {
    delete[] _rawFrameBuffer;
    _rawFrameBuffer = NULL;
  }
  if (_encFrameBuffer) {
    delete[] _encFrameBuffer;
    _encFrameBuffer = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// Returns the number of bytes / frame.  This is called from 
// encoder_get_output_data_size to get an accurate frame size
//

int MPEG4EncoderContext::GetFrameBytes() {
    return _frameWidth * _frameHeight;
}


/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _iQuantFactor.  This may be called from 
// encoder_set_options if the "IQuantFactor" real option has been passed 
//

void MPEG4EncoderContext::SetIQuantFactor(float newFactor) {
    _iQuantFactor = newFactor;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _keyframeUpdatePeriod.  This is called from 
// encoder_set_options if the "Keyframe Update Period" integer option is passed

void MPEG4EncoderContext::SetKeyframeUpdatePeriod(int interval) {
    _keyframeUpdatePeriod = interval;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _maxBitRateLimit. This is called from encoder_set_options
// if the "MaxBitrate" integer option is passed 

void MPEG4EncoderContext::SetMaxBitrate(int max) {
    _bitRateHighLimit = max;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _targetFPS. This is called from encoder_set_options
// if the "FPS" integer option is passed 

void MPEG4EncoderContext::SetFPS(int frameTime) {
    _targetFPS = (MPEG4_CLOCKRATE / frameTime);
}


/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _frameWidth. This is called from encoder_set_options
// when the "Frame Width" integer option is passed 

void MPEG4EncoderContext::SetFrameWidth(int width) {
    _frameWidth = width;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _frameHeight. This is called from encoder_set_options
// when the "Frame Height" integer option is passed 

void MPEG4EncoderContext::SetFrameHeight(int height) {
    _frameHeight = height;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _videoQMin. This is called from encoder_set_options
// when the "Minimum Quality" integer option is passed

void MPEG4EncoderContext::SetQMin(int qmin) {
    _videoQMin = qmin;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _videoQMax. This is called from encoder_set_options
// when the "Maximum Quality" integer option is passed 

void MPEG4EncoderContext::SetTSTO(unsigned tsto) {
    _videoTSTO = tsto;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _frameHeight. This is called from encoder_set_options
// when the "Encoding Quality" integer option is passed 

void MPEG4EncoderContext::SetProfileLevel (unsigned profileLevel) {
  int i = 0;
  while (mpeg4_profile_levels[i].profileLevel) {
    if (mpeg4_profile_levels[i].profileLevel == profileLevel)
      break;
    i++; 
  }
  
  if (!mpeg4_profile_levels[i].profileLevel) {
    TRACE(1, "MPEG4\tCap\tIllegal Profle-Level negotiated");
    return;
  }
  _maxBufferSize = mpeg4_profile_levels[i].maxBufferSize * 16384;


}

/////////////////////////////////////////////////////////////////////////////
//
//  The ffmpeg rate control averages bit usage over an entire file.
//  We're only interested in instantaneous usage. Past periods of
//  over-bandwidth or under-bandwidth shouldn't affect the next
//  frame. So we reset the total_bits counter toward what it should be,
//  picture_number * bit_rate / fps,
//  bit by bit as we go.
//
//  We also reset the vbv buffer toward the value where it doesn't
//  affect quantization - rc_buffer_size/2.
//

#ifdef LIBAVCODEC_HAVE_SOURCE_DIR
void MPEG4EncoderContext::ResetBitCounter(int spread) {
    MpegEncContext *s = (MpegEncContext *) _avcontext->priv_data;
    int64_t wanted_bits
        = int64_t(s->bit_rate * double(s->picture_number)
                  * av_q2d(_avcontext->time_base));
    s->total_bits += (wanted_bits - s->total_bits) / int64_t(spread);

    double want_buffer = double(_avcontext->rc_buffer_size / 2);
    s->rc_context.buffer_index
        += (want_buffer - s->rc_context.buffer_index) / double(spread);
}
#endif

/////////////////////////////////////////////////////////////////////////////
//
// Set static encoding parameters.  These should be values that remain
// unchanged through the duration of the encoding context.
//

void MPEG4EncoderContext::SetStaticEncodingParams(){
    _avcontext->pix_fmt = PIX_FMT_YUV420P;
    _avcontext->mb_decision = FF_MB_DECISION_SIMPLE;    // high quality off
    _avcontext->rtp_payload_size = 750;                 // ffh263 uses 750
    _avcontext->rtp_callback = &MPEG4EncoderContext::RtpCallback;

    // Reduce the difference in quantization between frames.
    _avcontext->qblur = 0.3;
    // default is tex^qComp; 1 is constant bitrate
    _avcontext->rc_eq = (char*) "1";
    //avcontext->rc_eq = "tex^qComp";
    // These ones technically could be dynamic, I think
    _avcontext->rc_min_rate = 0;
    // This is set to 0 in ffmpeg.c, the command-line utility.
    _avcontext->rc_initial_cplx = 0.0f;

    // And this is set to 1.
    // It seems to affect how aggressively the library will raise and lower
    // quantization to keep bandwidth constant. Except it's in reference to
    // the "vbv buffer", not bits per second, so nobody really knows how
    // it works.
    _avcontext->rc_buffer_aggressivity = 1.0f;

    // Ratecontrol buffer size, in bits. Usually 0.5-1 second worth.
    // 224 kbyte is what VLC uses, and it seems to fix the quantization pulse (at Level 5)
    _avcontext->rc_buffer_size = _maxBufferSize;

    // In MEncoder this defaults to 1/4 buffer size, but in ffmpeg.c it
    // defaults to 3/4. I think the buffer is supposed to stabilize at
    // about half full. Note that setting this after avcodec_open() has
    // no effect.
    _avcontext->rc_initial_buffer_occupancy = _avcontext->rc_buffer_size * 1/2;

    // Defaults to -0.8, which is good for high bandwidth and not negative
    // enough for low bandwidth.
    _avcontext->i_quant_factor = _iQuantFactor;
    _avcontext->i_quant_offset = 0.0;

    // Set our initial target FPS, gop_size and create throttler
    _avcontext->time_base.num = 1;
    _avcontext->time_base.den = _targetFPS;

    // Number of frames for a group of pictures
    if (_keyframeUpdatePeriod == 0)
      _avcontext->gop_size = _targetFPS * 8;
    else
      _avcontext->gop_size = _keyframeUpdatePeriod;
    
    _avpicture->quality = _videoQMin;

#ifdef USE_ORIG
    _avcontext->flags |= CODEC_FLAG_PART;   // data partitioning
    _avcontext->flags |= CODEC_FLAG_4MV;    // 4 motion vectors
#else
    _avcontext->max_b_frames=0; /*don't use b frames*/
    _avcontext->flags|=CODEC_FLAG_AC_PRED;
    _avcontext->flags|=CODEC_FLAG_H263P_UMV;
    /*c->flags|=CODEC_FLAG_QPEL;*/ /*don't enable this one: this forces profile_level to advanced simple profile */
    _avcontext->flags|=CODEC_FLAG_4MV;
    _avcontext->flags|=CODEC_FLAG_GMC;
    _avcontext->flags|=CODEC_FLAG_LOOP_FILTER;
    _avcontext->flags|=CODEC_FLAG_H263P_SLICE_STRUCT;
#endif
    _avcontext->opaque = this;              // for use in RTP callback
}

/////////////////////////////////////////////////////////////////////////////
//
// Set dynamic encoding parameters.  These are values that may change over the
// encoding context's lifespan
//

void MPEG4EncoderContext::SetDynamicEncodingParams(bool restartOnResize) {
    // If no bitrate limit is set, max out at 3 mbit
    // Use 75% of available bandwidth so not as many frames are dropped
    unsigned bitRate
        = (_bitRateHighLimit ? 3*_bitRateHighLimit/4 : MPEG4_BITRATE);
    _avcontext->bit_rate = bitRate;

    // In ffmpeg this is the tolerance over the entire file. We reset the
    // bit counter toward the expected value a little bit every frame,
    // so this probably needs to be readjusted.
    _avcontext->bit_rate_tolerance = bitRate;

    // The rc_* values affect the quantizer. Not even sure what bit_rate does.
    _avcontext->rc_max_rate = bitRate;

    // Update the quantization factor
    _avcontext->i_quant_factor = _iQuantFactor;

    // Set full-frame min/max quantizers.
    // Note: mb_qmin, mb_max don't seem to be used in the libavcodec code.

    _avcontext->qmin = _videoQMin;
    _avcontext->qmax = round ( (double)(31 - _videoQMin) / 31 * _videoTSTO + _videoQMin);
    _avcontext->qmax = std::min( _avcontext->qmax, 31);

    // Lagrange multipliers - this is how the context defaults do it:
    _avcontext->lmin = _avcontext->qmin * FF_QP2LAMBDA;
    _avcontext->lmax = _avcontext->qmax * FF_QP2LAMBDA;

    // If framesize has changed or is not yet initialized, fix it up
    if((unsigned)_avcontext->width != _frameWidth || (unsigned)_avcontext->height != _frameHeight) {
        ResizeEncodingFrame(restartOnResize);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Updates the context's frame size and creates new buffers
//

void MPEG4EncoderContext::ResizeEncodingFrame(bool restartCodec) {
    _avcontext->width = _frameWidth;
    _avcontext->height = _frameHeight;

    // Restart to force avcodec to use the new frame sizes
    if (restartCodec) {
        CloseCodec();
        OpenCodec();
    }

    _rawFrameLen = (_frameWidth * _frameHeight * 3) / 2;
    if (_rawFrameBuffer)
    {
        delete[] _rawFrameBuffer;
    }
    _rawFrameBuffer = new BYTE[_rawFrameLen + FF_INPUT_BUFFER_PADDING_SIZE];

    if (_encFrameBuffer)
    {
        delete[] _encFrameBuffer;
    }
    _encFrameLen = _rawFrameLen/2;         // assume at least 50% compression...
    _encFrameBuffer = new BYTE[_encFrameLen];

    // Clear the back padding
    memset(_rawFrameBuffer + _rawFrameLen, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    const unsigned fsz = _frameWidth * _frameHeight;
    _avpicture->data[0] = _rawFrameBuffer;              // luminance
    _avpicture->data[1] = _rawFrameBuffer + fsz;        // first chroma channel
    _avpicture->data[2] = _avpicture->data[1] + fsz/4;  // second
    _avpicture->linesize[0] = _frameWidth;
    _avpicture->linesize[1] = _avpicture->linesize[2] = _frameWidth/2;
}

/////////////////////////////////////////////////////////////////////////////
//
// Initializes codec parameters and opens the encoder
//

bool MPEG4EncoderContext::OpenCodec()
{
  _avcontext = FFMPEGLibraryInstance.AvcodecAllocContext();
  if (_avcontext == NULL) {
    TRACE(1, "MPEG4\tEncoder\tFailed to allocate context for encoder");
    return false;
  }

  _avpicture = FFMPEGLibraryInstance.AvcodecAllocFrame();
  if (_avpicture == NULL) {
    TRACE(1, "MPEG4\tEncoder\tFailed to allocate frame for encoder");
    return false;
  }

  if((_avcodec = FFMPEGLibraryInstance.AvcodecFindEncoder(CODEC_ID_MPEG4)) == NULL){
    TRACE(1, "MPEG4\tEncoder\tCodec not found for encoder");
    return false;
  }

  // debugging flags
  if (Trace::CanTraceUserPlane(4)) {
    _avcontext->debug |= FF_DEBUG_RC;
    _avcontext->debug |= FF_DEBUG_PICT_INFO;
    _avcontext->debug |= FF_DEBUG_MV;
  }
  
  SetStaticEncodingParams();
  SetDynamicEncodingParams(false);    // don't force a restart, it's not open
  if (FFMPEGLibraryInstance.AvcodecOpen(_avcontext, _avcodec) < 0)
  {
    TRACE(1, "MPEG4\tEncoder\tCould not open codec");
    return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// Close the codec
//

void MPEG4EncoderContext::CloseCodec()
{
  if (_avcontext != NULL) {
    if(_avcontext->codec != NULL)
      FFMPEGLibraryInstance.AvcodecClose(_avcontext);
    FFMPEGLibraryInstance.AvcodecFree(_avcontext);
    _avcontext = NULL;
  }
  if (_avpicture != NULL) {
    FFMPEGLibraryInstance.AvcodecFree(_avpicture);
    _avpicture = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// This is called after an Favcodec_encode_frame() call.  This populates the 
// _packetSizes deque with offsets, no greated than max_rtp which we will use
// to create RTP packets.
//

void MPEG4EncoderContext::RtpCallback(AVCodecContext *priv_data, void * /*data*/,
                                      int size, int /*num_mb*/)
{
  MPEG4EncoderContext *c = static_cast<MPEG4EncoderContext *>(priv_data->opaque);
  c->_packetSizes.push_back(size);
}
		    
/////////////////////////////////////////////////////////////////////////////
//
// The main encoding loop.  If there are no packets ready to be sent, generate
// them by encoding a frame.  When there are, create packets from the encoded
// frame buffer and send them out
//

int MPEG4EncoderContext::EncodeFrames(const BYTE * src, unsigned & srcLen,
                                      BYTE * dst, unsigned & dstLen,
                                      unsigned int & flags)
{
    if (!FFMPEGLibraryInstance.IsLoaded()){
        return 0;
    }

    if (dstLen < 16)
      return false;

    // create frames frame from their respective buffers
    RTPFrame srcRTP(src, srcLen);
    RTPFrame dstRTP(dst, dstLen);

    // create the video frame header from the source and update video size
    PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
    _frameWidth  = header->width;
    _frameHeight = header->height;

    if (_packetSizes.empty()) {
        if (_avcontext == NULL) {
            OpenCodec();
        }
        else {
            // set our dynamic parameters, restart the codec if we need to
            SetDynamicEncodingParams(true);
        }
        _lastTimeStamp = srcRTP.GetTimestamp();
        _lastPktOffset = 0; 

        // generate the raw picture
        memcpy(_rawFrameBuffer, OPAL_VIDEO_FRAME_DATA_PTR(header), _rawFrameLen);

        // Should the next frame be an I-Frame?
        if ((flags & PluginCodec_CoderForceIFrame) || (_frameNum == 0))
        {
            _avpicture->pict_type = FF_I_TYPE;
        }
        else // No IFrame requested, let avcodec decide what to do
        {
            _avpicture->pict_type = 0;
        }

        // Encode a frame
        int total = FFMPEGLibraryInstance.AvcodecEncodeVideo
                            (_avcontext, _encFrameBuffer, _encFrameLen,
                             _avpicture);


        if (total > 0) {
            _frameNum++; // increment the number of frames encoded
#ifdef LIBAVCODEC_HAVE_SOURCE_DIR
            ResetBitCounter(8); // Fix ffmpeg rate control
#endif
	    _isIFrame = mpeg4IsIframe(_encFrameBuffer, total );
        }
    }

    flags = 0;

    if (_isIFrame)
      flags |= PluginCodec_ReturnCoderIFrame;

    // get the next packet
    if (_packetSizes.size() == 0) 

      dstLen = 0;

    else {

      unsigned pktLen = _packetSizes.front();
      _packetSizes.pop_front();

      // if too large, split it
      unsigned maxRtpSize = dstLen - dstRTP.GetHeaderSize();
      if (pktLen > maxRtpSize) {
          _packetSizes.push_front(pktLen - maxRtpSize);
          pktLen = maxRtpSize;
      }

      dstRTP.SetPayloadSize(pktLen); 

      // Copy the encoded data from the buffer into the outgoign RTP 
      memcpy(dstRTP.GetPayloadPtr(), &_encFrameBuffer[_lastPktOffset], pktLen);
      _lastPktOffset += pktLen;

      // If there are no more packet sizes left, we've reached the last packet
      // for the frame, set the marker bit and flags
      if (_packetSizes.empty()) {
          dstRTP.SetMarker(true);
          flags |= PluginCodec_ReturnCoderLastFrame;
      }

      // set timestamp and adjust dstLen to include header size
      dstRTP.SetTimestamp(_lastTimeStamp);

      dstLen = dstRTP.GetHeaderSize() + pktLen;

    }
    
    return 1;
}

/////////////////////////////////////////////////////////////////////////////
//
// OPAL plugin functions
//
static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

static int get_codec_options(const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
    if (parmLen == NULL || parm == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
        return 0;

    *(const void **)parm = codec->userData;
    *parmLen = 0; //FIXME
    return 1;
}

static int free_codec_options ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  char ** strings = (char **) parm;
  for (char ** string = strings; *string != NULL; string++)
    free(*string);
  free(strings);
  return 1;
}

static int valid_for_protocol ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

static int adjust_bitrate_to_profile_level (unsigned & targetBitrate, unsigned profileLevel, int idx = -1)
{
  int i = 0;
  if (idx == -1) { 
    while (mpeg4_profile_levels[i].profileLevel) {
      if (mpeg4_profile_levels[i].profileLevel == profileLevel)
        break;
    i++; 
    }
  
    if (!mpeg4_profile_levels[i].profileLevel) {
      TRACE(1, "MPEG4\tCap\tIllegal Profle-Level negotiated");
      return 0;
    }
  }
  else
    i = idx;

// Correct Target Bitrate
  TRACE(4, "MPEG4\tCap\tAdjusting to " << mpeg4_profile_levels[i].profileName << " Profile, Level " <<  mpeg4_profile_levels[i].level);
  TRACE(4, "MPEG4\tCap\tBitrate: " << targetBitrate << "(" << mpeg4_profile_levels[i].bitrate << ")");
  if (targetBitrate > mpeg4_profile_levels[i].bitrate)
    targetBitrate = mpeg4_profile_levels[i].bitrate;

  return 1;
}

static int adjust_to_profile_level (unsigned & width, unsigned & height, unsigned & frameTime, unsigned & targetBitrate, unsigned profileLevel)
{
  int i = 0;
  while (mpeg4_profile_levels[i].profileLevel) {
    if (mpeg4_profile_levels[i].profileLevel == profileLevel)
      break;
   i++; 
  }

  if (!mpeg4_profile_levels[i].profileLevel) {
    TRACE(1, "MPEG4\tCap\tIllegal Level negotiated");
    return 0;
  }

// Correct max. number of macroblocks per frame
  uint32_t nbMBsPerFrame = width * height / 256;
  unsigned j = 0;
  TRACE(4, "MPEG4\tCap\tFrame Size: " << nbMBsPerFrame << "(" << mpeg4_profile_levels[i].frame_size << ")");
  if    ( (nbMBsPerFrame          > mpeg4_profile_levels[i].frame_size) ) {

    while (mpeg4_resolutions[j].width) {
      if  ( (mpeg4_resolutions[j].macroblocks <= mpeg4_profile_levels[i].frame_size) )
          break;
      j++; 
    }
    if (!mpeg4_resolutions[j].width) {
      TRACE(1, "MPEG4\tCap\tNo Resolution found that has number of macroblocks <=" << mpeg4_profile_levels[i].frame_size);
      return 0;
    }
    else {
      width  = mpeg4_resolutions[j].width;
      height = mpeg4_resolutions[j].height;
    }
  }

// Correct macroblocks per second
  uint32_t nbMBsPerSecond = width * height / 256 * (90000 / frameTime);
  TRACE(4, "MPEG4\tCap\tMBs/s: " << nbMBsPerSecond << "(" << mpeg4_profile_levels[i].mbps << ")");
  if (nbMBsPerSecond > mpeg4_profile_levels[i].mbps)
    frameTime =  (unsigned) (90000 / 256 * width  * height / mpeg4_profile_levels[i].mbps );

  adjust_bitrate_to_profile_level (targetBitrate, profileLevel, i);
  return 1;
}

/////////////////////////////////////////////////////////////////////////////
//
// OPAL plugin encoder functions
//

static void * create_encoder(const struct PluginCodec_Definition * /*codec*/)
{
  return new MPEG4EncoderContext;
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  unsigned profileLevel = 1;
  unsigned width = 352;
  unsigned height = 288;
  unsigned frameTime = 3000;
  unsigned targetBitrate = 64000;

  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
      if (STRCMPI(option[0], "CAP RFC3016 Profile Level") == 0)
        profileLevel = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        width = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        height = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
        frameTime = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
        targetBitrate = atoi(option[1]);
  }

  // Though this is not a strict requirement we enforce 
  //it here in order to obtain optimal compression results
  width -= width % 16;
  height -= height % 16;

  if (profileLevel == 0) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    // Simple Profile, Level 5
    profileLevel = 5;
#else
    // Default handling according to RFC 3016
    // Simple Profile, Level 1
    profileLevel = 1;
#endif
  }

  if (!adjust_to_profile_level (width, height, frameTime, targetBitrate, profileLevel))
    return 0;

  char ** options = (char **)calloc(9, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[0] = strdup(PLUGINCODEC_OPTION_FRAME_WIDTH);
  options[1] = num2str(width);
  options[2] = strdup(PLUGINCODEC_OPTION_FRAME_HEIGHT);
  options[3] = num2str(height);
  options[4] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
  options[5] = num2str(frameTime);
  options[6] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[7] = num2str(targetBitrate);

  return 1;
}

// This is assuming the second param is the context...
static int encoder_set_options(
      const PluginCodec_Definition * , 
      void * _context, 
      const char * , 
      void *parm, 
      unsigned *parmLen )
{
  if (parmLen == NULL || *parmLen != sizeof(const char **))
    return 0;

  MPEG4EncoderContext * context = (MPEG4EncoderContext *)_context;
  // Scan the options passed in and check for:
  // 1) "Frame Width" sets the encoding frame width
  // 2) "Frame Height" sets the encoding frame height
  // 3) "Target Bit Rate" sets the bitrate upper limit
  // 5) "Tx Keyframe Period" interval in frames before an IFrame is sent
  // 6) "Minimum Quality" sets the minimum encoding quality
  // 7) "Maximum Quality" sets the maximum encoding quality
  // 8) "Encoding Quality" sets the default encoding quality
  // 9) "Dynamic Encoding Quality" enables dynamic adjustment of encoding quality
  // 10) "Bandwidth Throttling" will turn on bandwidth throttling for the encoder
  // 11) "IQuantFactor" will update the quantization factor to a float value

  if (parm != NULL) {
    const char ** options = (const char **)parm;
    unsigned targetBitrate = 64000;
    unsigned profileLevel = 1;
    for (int i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], "CAP RFC3016 Profile Level") == 0)
         profileLevel = atoi(options[i+1]);
      else if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        context->SetFrameWidth(atoi(options[i+1]));
      else if(STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        context->SetFrameHeight(atoi(options[i+1]));
      else if(STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
         targetBitrate = atoi(options[i+1]);
      else if(STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
        context->SetFPS(atoi(options[i+1]));
      else if(STRCMPI(options[i], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
        context->SetKeyframeUpdatePeriod(atoi(options[i+1]));
      else if(STRCMPI(options[i], PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF) == 0)
        context->SetTSTO(atoi(options[i+1]));
      else if(STRCMPI(options[i], "Minimum Quality") == 0)
        context->SetQMin(atoi(options[i+1]));
      else if(STRCMPI(options[i], "IQuantFactor") == 0)
        context->SetIQuantFactor(atof(options[i+1]));
    }

    if (profileLevel == 0) {
#ifdef WITH_RFC_COMPLIANT_DEFAULTS
      profileLevel = 1;
#else
      profileLevel = 5;
#endif
    }
    if (!adjust_bitrate_to_profile_level (targetBitrate, profileLevel))
      return 0;

    context->SetMaxBitrate(targetBitrate);
    context->SetProfileLevel(profileLevel);
  }
  return 1;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/,
                            void * _context)
{
  MPEG4EncoderContext * context = (MPEG4EncoderContext *)_context;
  delete context;
}

static int codec_encoder(const struct PluginCodec_Definition * , 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flags)
{
  MPEG4EncoderContext * context = (MPEG4EncoderContext *)_context;
  return context->EncodeFrames((const BYTE *)from, *fromLen, (BYTE *)to, *toLen,
                               *flags);
}

static int encoder_get_output_data_size(const PluginCodec_Definition * /*codec*/, void *_context, const char *, void *, unsigned *)
{
  MPEG4EncoderContext * context = (MPEG4EncoderContext *)_context;
  return context->GetFrameBytes() * 3 / 2;
}

static PluginCodec_ControlDefn sipEncoderControls[] = {
  { PLUGINCODEC_CONTROL_VALID_FOR_PROTOCOL,    valid_for_protocol },
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     encoder_set_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  encoder_get_output_data_size },
  { NULL }
};

/////////////////////////////////////////////////////////////////////////////
//
// Define the decoder context
//

class MPEG4DecoderContext
{
  public:
    MPEG4DecoderContext();
    ~MPEG4DecoderContext();

    bool DecodeFrames(const BYTE * src, unsigned & srcLen,
                      BYTE * dst, unsigned & dstLen, unsigned int & flags);

    bool DecoderError(int threshold);
    void SetErrorRecovery(bool error);
    void SetErrorThresh(int thresh);
    void SetDisableResize(bool disable);

    void SetFrameHeight(int height);
    void SetFrameWidth(int width);

    int GetFrameBytes();

  protected:

    bool OpenCodec();
    void CloseCodec();

    void SetStaticDecodingParams();
    void SetDynamicDecodingParams(bool restartOnResize);
    void ResizeDecodingFrame(bool restartCodec);

    BYTE * _encFrameBuffer;
    unsigned int _encFrameLen;

    AVCodec        *_avcodec;
    AVCodecContext *_avcontext;
    AVFrame        *_avpicture;

    int _frameNum;

    bool _doError;
    int _keyRefreshThresh;

    bool _disableResize;

    unsigned _lastPktOffset;
    unsigned int _frameWidth;
    unsigned int _frameHeight;

    bool _gotAGoodFrame;
};

/////////////////////////////////////////////////////////////////////////////
//
// Constructor.  Register the codec, allocate the context and frame and set
// some parameters
//

MPEG4DecoderContext::MPEG4DecoderContext() 
:   _encFrameBuffer(NULL),
    _frameNum(0),
    _doError(true),
    _keyRefreshThresh(1),
    _disableResize(false),
    _lastPktOffset(0),
    _frameWidth(0),
    _frameHeight(0)
{
  if (!FFMPEGLibraryInstance.IsLoaded()) {
    return;
  }

  // Default frame sizes.  These may change after decoder_set_options is called
  _frameWidth  = CIF_WIDTH;
  _frameHeight = CIF_HEIGHT;
  _gotAGoodFrame = true;

  OpenCodec();
}

/////////////////////////////////////////////////////////////////////////////
//
// Close the codec, free the context, frame and buffer
//

MPEG4DecoderContext::~MPEG4DecoderContext()
{
  if (FFMPEGLibraryInstance.IsLoaded()) {
    CloseCodec();
  }
  if(_encFrameBuffer) {
    delete[] _encFrameBuffer;
    _encFrameBuffer = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// Returns the number of bytes / frame.  This is called from 
// decoder_get_output_data_size to get an accurate frame size
//

int MPEG4DecoderContext::GetFrameBytes() {
    return _frameWidth * _frameHeight;
}



/////////////////////////////////////////////////////////////////////////////
//
// Setter for decoding error recovery.  Called from decoder_set_options if
// the user passes the "Error Recovery" option.
//

void MPEG4DecoderContext::SetErrorRecovery(bool error) {
    _doError = error;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter for DecoderError threshold.  Called from decoder_set_options if
// the user passes "Error Threshold" option.
//

void MPEG4DecoderContext::SetErrorThresh(int thresh) {
    _keyRefreshThresh = thresh;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter for the automatic resize boolean.  Called from decoder_set_options if
// the user passes "Disable Resize" option.
//

void MPEG4DecoderContext::SetDisableResize(bool disable) {
    _disableResize = disable;
}


/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _frameWidth. This is called from decoder_set_options
// when the "Frame Width" integer option is passed 

void MPEG4DecoderContext::SetFrameWidth(int width) {
    _frameWidth = width;
}

/////////////////////////////////////////////////////////////////////////////
//
// Setter function for _frameHeight. This is called from decoder_set_options
// when the "Frame Height" integer option is passed 

void MPEG4DecoderContext::SetFrameHeight(int height) {
    _frameHeight = height;
}

/////////////////////////////////////////////////////////////////////////////
//
// Sets static decoding parameters.
//

void MPEG4DecoderContext::SetStaticDecodingParams() {
    _avcontext->flags |= CODEC_FLAG_4MV; 
    _avcontext->flags |= CODEC_FLAG_PART;
    _avcontext->workaround_bugs = 0; // no workaround for buggy implementations
}


/////////////////////////////////////////////////////////////////////////////
//
// Check for errors on I-Frames.  If we found one, ask for another.
//
#ifdef LIBAVCODEC_HAVE_SOURCE_DIR
bool MPEG4DecoderContext::DecoderError(int threshold) {
    if (_doError) {
        int errors = 0;
        MpegEncContext *s = (MpegEncContext *) _avcontext->priv_data;
        if (s->error_count && _avcontext->coded_frame->pict_type == FF_I_TYPE) {
            const uint8_t badflags = AC_ERROR | DC_ERROR | MV_ERROR;
            for (int i = 0; i < s->mb_num && errors < threshold; ++i) {
                if (s->error_status_table[s->mb_index2xy[i]] & badflags)
                    ++errors;
            }
        }
        return (errors >= threshold);
    }
    return false;
}
#endif

/////////////////////////////////////////////////////////////////////////////
//
// If there's any other dynamic decoder updates to do, put them here.  For
// now it just checks to see if the frame needs resizing.
//

void MPEG4DecoderContext::SetDynamicDecodingParams(bool restartOnResize) {
    if (_frameWidth != (unsigned)_avcontext->width || _frameHeight != (unsigned)_avcontext->height)
    {
        ResizeDecodingFrame(restartOnResize);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Resizes the decoding frame, updates the buffer
//

void MPEG4DecoderContext::ResizeDecodingFrame(bool restartCodec) {
    _avcontext->width = _frameWidth;
    _avcontext->height = _frameHeight;
    unsigned _rawFrameLen = (_frameWidth * _frameHeight * 3) / 2;
    if (_encFrameBuffer)
    {
        delete[] _encFrameBuffer;
    }
    _encFrameLen = _rawFrameLen/2;         // assume at least 50% compression...
    _encFrameBuffer = new BYTE[_encFrameLen];
    // Restart to force avcodec to use the new frame sizes
    if (restartCodec) {
        CloseCodec();
        OpenCodec();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Set our parameters and open the decoder
//

bool MPEG4DecoderContext::OpenCodec()
{
    if ((_avcodec = FFMPEGLibraryInstance.AvcodecFindDecoder(CODEC_ID_MPEG4)) == NULL) {
        TRACE(1, "MPEG4\tDecoder\tCodec not found for encoder");
        return false;
    }
        
    _avcontext = FFMPEGLibraryInstance.AvcodecAllocContext();
    if (_avcontext == NULL) {
        TRACE(1, "MPEG4\tDecoder\tFailed to allocate context for encoder");
        return false;
    }

    _avpicture = FFMPEGLibraryInstance.AvcodecAllocFrame();
    if (_avpicture == NULL) {
        TRACE(1, "MPEG4\tDecoder\tFailed to allocate frame for decoder");
        return false;
    }

    _avcontext->codec = NULL;

    SetStaticDecodingParams();
    SetDynamicDecodingParams(false);    // don't force a restart, it's not open
    if (FFMPEGLibraryInstance.AvcodecOpen(_avcontext, _avcodec) < 0) {
        TRACE(1, "MPEG4\tDecoder\tFailed to open MPEG4 decoder");
        return false;
    }
    TRACE(1, "MPEG4\tDecoder\tDecoder successfully opened");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// Closes the decoder
//

void MPEG4DecoderContext::CloseCodec()
{
  if (_avcontext != NULL) {
    if(_avcontext->codec != NULL)
      FFMPEGLibraryInstance.AvcodecClose(_avcontext);
    FFMPEGLibraryInstance.AvcodecFree(_avcontext);
    _avcontext = NULL;
  }
  if(_avpicture != NULL) {
    FFMPEGLibraryInstance.AvcodecFree(_avpicture);    
    _avpicture = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// Frame decoding loop.  Copies incoming RTP payloads into a buffer, when a
// marker frame is received, send the completed frame into the decoder then
// display.
//

bool MPEG4DecoderContext::DecodeFrames(const BYTE * src, unsigned & srcLen,
                                       BYTE * dst, unsigned & dstLen,
                                       unsigned int & flags)
{
    if (!FFMPEGLibraryInstance.IsLoaded())
        return 0;

    // Creates our frames
    RTPFrame srcRTP(src, srcLen);
    RTPFrame dstRTP(dst, dstLen, RTP_DYNAMIC_PAYLOAD);
    dstLen = 0;
    flags = 0;
    
    int srcPayloadSize = srcRTP.GetPayloadSize();
    SetDynamicDecodingParams(true); // Adjust dynamic settings, restart allowed
    
    // Don't exceed buffer limits.  _encFrameLen set by ResizeDecodingFrame
    if(_lastPktOffset + srcPayloadSize < _encFrameLen)
    {
        // Copy the payload data into the buffer and update the offset
        memcpy(_encFrameBuffer + _lastPktOffset, srcRTP.GetPayloadPtr(),
               srcPayloadSize);
        _lastPktOffset += srcPayloadSize;
    }
    else {

        // Likely we dropped the marker packet, so at this point we have a
        // full buffer with some of the frame we wanted and some of the next
        // frame.  

        //I'm on the fence about whether to send the data to the
        // decoder and hope for the best, or to throw it all away and start 
        // again.


        // throw the data away and ask for an IFrame
        TRACE(1, "MPEG4\tDecoder\tWaiting for an I-Frame");
        _lastPktOffset = 0;
        flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
        _gotAGoodFrame = false;
        return 1;
    }

    // decode the frame if we got the marker packet
    int got_picture = 0;
    if (srcRTP.GetMarker()) {
        _frameNum++;
        int len = FFMPEGLibraryInstance.AvcodecDecodeVideo
                        (_avcontext, _avpicture, &got_picture,
                         _encFrameBuffer, _lastPktOffset);

        if (len >= 0 && got_picture) {
#ifdef LIBAVCODEC_HAVE_SOURCE_DIR
            if (DecoderError(_keyRefreshThresh)) {
                // ask for an IFrame update, but still show what we've got
                flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
                _gotAGoodFrame = false;
            }
#endif
            TRACE_UP(4, "MPEG4\tDecoder\tDecoded " << len << " bytes" << ", Resolution: " << _avcontext->width << "x" << _avcontext->height);
            // If the decoding size changes on us, we can catch it and resize
            if (!_disableResize
                && (_frameWidth != (unsigned)_avcontext->width
                   || _frameHeight != (unsigned)_avcontext->height))
            {
                // Set the decoding width to what avcodec says it is
                _frameWidth  = _avcontext->width;
                _frameHeight = _avcontext->height;
                // Set dynamic settings (framesize), restart as needed
                SetDynamicDecodingParams(true);
                return true;
            }

            // it's stride time
            int frameBytes = (_frameWidth * _frameHeight * 3) / 2;
            PluginCodec_Video_FrameHeader * header
                = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
            header->x = header->y = 0;
            header->width = _frameWidth;
            header->height = _frameHeight;
            unsigned char *dstData = OPAL_VIDEO_FRAME_DATA_PTR(header);
            for (int i=0; i<3; i ++) {
                unsigned char *srcData = _avpicture->data[i];
                int dst_stride = i ? _frameWidth >> 1 : _frameWidth;
                int src_stride = _avpicture->linesize[i];
                int h = i ? _frameHeight >> 1 : _frameHeight;
                if (src_stride==dst_stride) {
                    memcpy(dstData, srcData, dst_stride*h);
                    dstData += dst_stride*h;
                } 
                else 
                {
                    while (h--) {
                        memcpy(dstData, srcData, dst_stride);
                        dstData += dst_stride;
                        srcData += src_stride;
                    }
                }
            }
            // Treating the screen as an RTP is weird
            dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader)
                                  + frameBytes);
            dstRTP.SetPayloadType(RTP_DYNAMIC_PAYLOAD);
            dstRTP.SetTimestamp(srcRTP.GetTimestamp());
            dstRTP.SetMarker(true);
            dstLen = dstRTP.GetFrameLen();
            flags = PluginCodec_ReturnCoderLastFrame;
            _gotAGoodFrame = true;
        }
        else {
            TRACE(1, "MPEG4\tDecoder\tDecoded "<< len << " bytes without getting a Picture..."); 
            // decoding error, ask for an IFrame update
            flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
            _gotAGoodFrame = false;
        }
        _lastPktOffset = 0;
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
// OPAL plugin decoder functions
//

static void * create_decoder(const struct PluginCodec_Definition *)
{
  return new MPEG4DecoderContext;
}

// This is assuming the second param is the context...
static int decoder_set_options(
      const struct PluginCodec_Definition *, 
      void * _context , 
      const char *, 
      void * parm, 
      unsigned * parmLen)
{
  if (parmLen == NULL || *parmLen != sizeof(const char **))
    return 0;

  MPEG4DecoderContext * context = (MPEG4DecoderContext *)_context;

  // This checks if any of the following options have been passed:
  // 1) get the "Frame Width" media format parameter and let the decoder know
  // 2) get the "Frame Height" media format parameter and let the decoder know
  // 3) Check if "Error Recovery" boolean option has been passed, then sets the
  //    value in the decoder context
  // 4) Check if "Error Threshold" integer has been passed, then sets the value
  // 5) Check if "Disable Resize" boolean has been passed, then sets the value

  if (parm != NULL) {
    const char ** options = (const char **)parm;
    int i;
    for (i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        context->SetFrameWidth(atoi(options[i+1]));
      else if(STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        context->SetFrameHeight(atoi(options[i+1]));
      else if(STRCMPI(options[i], "Error Recovery") == 0)
        context->SetErrorRecovery(atoi(options[i+1]));
      else if(STRCMPI(options[i], "Error Threshold") == 0)
        context->SetErrorThresh(atoi(options[i+1]));
      else if(STRCMPI(options[i], "Disable Resize") == 0)
        context->SetDisableResize(atoi(options[i+1]));
    }
  }
  return 1;
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  MPEG4DecoderContext * context = (MPEG4DecoderContext *)_context;
  delete context;
}

static int codec_decoder(const struct PluginCodec_Definition *, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  MPEG4DecoderContext * context = (MPEG4DecoderContext *)_context;
  return context->DecodeFrames((const BYTE *)from, *fromLen, (BYTE *)to, *toLen, *flag);
}

static int decoder_get_output_data_size(const PluginCodec_Definition * /*codec*/, void * _context, const char *, void *, unsigned *)
{
  MPEG4DecoderContext * context = (MPEG4DecoderContext *) _context;
  return sizeof(PluginCodec_Video_FrameHeader) + (context->GetFrameBytes() * 3 / 2);
}

// Plugin Codec Definitions

static PluginCodec_ControlDefn sipDecoderControls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     decoder_set_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  decoder_get_output_data_size },
  { NULL }
};

/////////////////////////////////////////////////////////////////////////////


static struct PluginCodec_information licenseInfo = {
1168031935,                                                       // timestamp = Fri 5 Jan 2006 21:19:40 PM UTC
  
  "Craig Southeren, Guilhem Tardy, Derek Smithies, Michael Smith, Josh Mahonin", // authors
  "1.0",                                                        // source code version
  "openh323@openh323.org",                                      // source code email
  "http://sourceforge.net/projects/openh323",                   // source code URL
  "Copyright (C) 2007 Canadian Bank Note, Limited.",
  "Copyright (C) 2006 by Post Increment"                       // source code copyright
  ", Copyright (C) 2005 Salyens"
  ", Copyright (C) 2001 March Networks Corporation"
  ", Copyright (C) 1999-2000 Equivalence Pty. Ltd."
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license
  
  "FFMPEG",                                                     // codec description
  "Michael Niedermayer, Fabrice Bellard",                       // codec author
  "4.7.1",                                                      // codec version
  "ffmpeg-devel-request@ mplayerhq.hu",                         // codec email
  "http://sourceforge.net/projects/ffmpeg/",                    // codec URL
  "Copyright (c) 2000-2001 Fabrice Bellard"                     // codec copyright information
  ", Copyright (c) 2002-2003 Michael Niedermayer",
  "GNU LESSER GENERAL PUBLIC LICENSE, Version 2.1, February 1999", // codec license
  PluginCodec_License_LGPL                                         // codec license code
};

/////////////////////////////////////////////////////////////////////////////

static const char YUV420PDesc[]  = { "YUV420P" };
static const char mpeg4Desc[]      = { "MPEG4" };
static const char sdpMPEG4[]   = { "MP4V-ES" };

/////////////////////////////////////////////////////////////////////////////

enum
{
    H245_ANNEX_E_PROFILE_LEVEL = 0 | PluginCodec_H245_NonCollapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H245_ANNEX_E_OBJECT        = 1 | PluginCodec_H245_NonCollapsing |                        PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H245_ANNEX_E_DCI           = 2 | PluginCodec_H245_NonCollapsing |                        PluginCodec_H245_OLC,
    H245_ANNEX_E_DRAWING_ORDER = 3 | PluginCodec_H245_NonCollapsing |                        PluginCodec_H245_OLC,
    H245_ANNEX_E_VBCH          = 4 | PluginCodec_H245_Collapsing    | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
};

static int MergeProfileAndLevelMPEG4(char ** result, const char * dest, const char * src)
{
  // Due to the "special case" where the value 8 and 9 is simple profile level zero and zero b,
  // we cannot actually use a simple min merge!
  unsigned dstPL = strtoul(dest, NULL, 10);
  unsigned srcPL = strtoul(src, NULL, 10);

  unsigned dstProfile;
  int dstLevel;
  unsigned srcProfile;
  int srcLevel;

  switch (dstPL) {
    case 0:
      dstProfile = 0;
      dstLevel = -10;
      break;
    case 8:
      dstProfile = 0;
      dstLevel = -2;
      break;
    case 9:
      dstProfile = 0;
      dstLevel = -1;
      break;
    default:
      dstProfile = (dstPL>>4)&7;
      dstLevel = dstPL&7;
      break;
  }

  switch (srcPL) {
    case 0:
      srcProfile = 0;
      srcLevel = -10;
      break;
    case 8:
      srcProfile = 0;
      srcLevel = -2;
      break;
    case 9:
      srcProfile = 0;
      srcLevel = -1;
      break;
    default:
      srcProfile = (srcPL>>4)&7;
      srcLevel = srcPL&7;
      break;
  }


  if (dstProfile > srcProfile)
    dstProfile = srcProfile;
  if (dstLevel > srcLevel)
    dstLevel = srcLevel;

  char buffer[10];
  
  switch (dstLevel) {
    case -10:
      sprintf(buffer, "%u", (0));
      break;
    case -2:
      sprintf(buffer, "%u", (8));
      break;
    case -1:
      sprintf(buffer, "%u", (9));
      break;
    default:
      sprintf(buffer, "%u", (dstProfile<<4)|(dstLevel));
      break;
  }
  
  *result = strdup(buffer);

  return true;
}

static void FreeString(char * str)
{
  free(str);
}

static struct PluginCodec_Option const H245ProfileLevelMPEG4 =
{
  PluginCodec_IntegerOption,          // Option type
  "H.245 Profile & Level",            // User visible name
  false,                              // User Read/Only flag
  PluginCodec_CustomMerge,            // Merge mode
  "5",                                // Initial value (Simple Profile/Level 5)
  "profile-level-id",                 // FMTP option name
  "0",                                // FMTP default value (Simple Profile/Level 1)
  H245_ANNEX_E_PROFILE_LEVEL,         // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "245",                              // Maximum value
  MergeProfileAndLevelMPEG4,          // Function to do merge
  FreeString                          // Function to free memory in string
};

static struct PluginCodec_Option const H245ObjectMPEG4 =
{
  PluginCodec_IntegerOption,          // Option type
  "H.245 Object Id",                  // User visible name
  true,                               // User Read/Only flag
  PluginCodec_EqualMerge,             // Merge mode
  "0",                                // Initial value
  NULL,                               // FMTP option name
  NULL,                               // FMTP default value
  H245_ANNEX_E_OBJECT,                // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "15"                                // Maximum value
};

static struct PluginCodec_Option const DecoderConfigInfoMPEG4 =
{
  PluginCodec_OctetsOption,           // Option type
  "DCI",                              // User visible name
  false,                              // User Read/Only flag
  PluginCodec_NoMerge,                // Merge mode
  "",                                 // Initial value
  "config",                           // FMTP option name
  NULL,                               // FMTP default value
  H245_ANNEX_E_DCI                    // H.245 generic capability code and bit mask
};

static struct PluginCodec_Option const MediaPacketizationMPEG4 =
{
  PluginCodec_StringOption,           // Option type
  PLUGINCODEC_MEDIA_PACKETIZATION,    // User visible name
  true,                               // User Read/Only flag
  PluginCodec_EqualMerge,             // Merge mode
  OpalPluginCodec_Identifer_MPEG4     // Initial value
};


static struct PluginCodec_Option const * const optionTable[] = {
  &H245ProfileLevelMPEG4,
  &H245ObjectMPEG4,
  &DecoderConfigInfoMPEG4,
//  &ProfileMPEG4,
//  &LevelMPEG4,
  &MediaPacketizationMPEG4,
  NULL
};

static struct PluginCodec_H323GenericCodecData H323GenericMPEG4 = {
  OpalPluginCodec_Identifer_MPEG4
};


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition mpeg4CodecDefn[2] = {
{ 
  // Encoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // video codec
  PluginCodec_RTPTypeShared |
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  mpeg4Desc,                          // text decription
  YUV420PDesc,                        // source format
  mpeg4Desc,                          // destination format

  optionTable,                        // user data 

  MPEG4_CLOCKRATE,                    // samples per second
  MPEG4_BITRATE,                      // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF_WIDTH,                        // frame width
    CIF_HEIGHT,                       // frame height
    10,                               // recommended frame rate
    60,                               // maximum frame rate
  }},
  0,                                  // IANA RTP payload code
  sdpMPEG4,                           // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  sipEncoderControls,                 // codec controls

  PluginCodec_H323Codec_generic,      // h323CapabilityType
  &H323GenericMPEG4                   // h323CapabilityData
},
{ 
  // Decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // video codec
  PluginCodec_RTPTypeShared |
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  mpeg4Desc,                          // text decription
  mpeg4Desc,                          // source format
  YUV420PDesc,                        // destination format

  optionTable,                        // user data 

  MPEG4_CLOCKRATE,                    // samples per second
  MPEG4_BITRATE,                      // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF_WIDTH,                        // frame width
    CIF_HEIGHT,                       // frame height
    10,                               // recommended frame rate
    60,                               // maximum frame rate
  }},
  0,                                  // IANA RTP payload code
  sdpMPEG4,                           // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  sipDecoderControls,                 // codec controls

  PluginCodec_H323Codec_generic,      // h323CapabilityType
  &H323GenericMPEG4                   // h323CapabilityData
},

};

/////////////////////////////////////////////////////////////////////////////

extern "C" {
PLUGIN_CODEC_DLL_API struct PluginCodec_Definition *
PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
{
  char * debug_level = getenv ("PTLIB_TRACE_CODECS");
  if (debug_level!=NULL) {
    Trace::SetLevel(atoi(debug_level));
  } 
  else {
    Trace::SetLevel(0);
  }

  debug_level = getenv ("PTLIB_TRACE_CODECS_USER_PLANE");
  if (debug_level!=NULL) {
    Trace::SetLevelUserPlane(atoi(debug_level));
  } 
  else {
    Trace::SetLevelUserPlane(0);
  }

  if (!FFMPEGLibraryInstance.Load()) {
    *count = 0;
    TRACE(1, "MPEG4\tCodec\tDisabled");
    return NULL;
  }

  FFMPEGLibraryInstance.AvLogSetLevel(AV_LOG_DEBUG);
  FFMPEGLibraryInstance.AvLogSetCallback(&logCallbackFFMPEG);

  // check version numbers etc
  if (version < PLUGIN_CODEC_VERSION_OPTIONS) {
    *count = 0;
    TRACE(1, "MPEG4\tCodec\tDisabled - plugin version mismatch");
    return NULL;
  }
  else {
    *count = sizeof(mpeg4CodecDefn) / sizeof(struct PluginCodec_Definition);
    TRACE(1, "MPEG4\tCodec\tEnabled");
    return mpeg4CodecDefn;
  }
}
};
