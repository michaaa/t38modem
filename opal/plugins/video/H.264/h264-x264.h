/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) Matthias Schneider, All Rights Reserved
 *
 * This code is based on the file h261codec.cxx from the OPAL project released
 * under the MPL 1.0 license which contains the following:
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
 * Contributor(s): Matthias Schneider (ma30002000@yahoo.de)
 *                 Michele Piccini (michele@piccini.com)
 *                 Derek Smithies (derek@indranet.co.nz)
 *                 Simon Horne (s.horne@packetizer.com)
 *
 *
 */

/*
  Notes
  -----

 */

#ifndef __H264_X264_H__
#define __H264_X264_H__ 1

#include "plugin-config.h"

#include <stdarg.h>
#include <codec/opalplugin.h>

#ifdef _MSC_VER
 #include "../common/vs-stdint.h"
 #include "../common/critsect.h"
#else
 #include <stdint.h>
 #include "critsect.h"
#endif

#include "shared/h264frame.h"



extern "C" {
#ifdef _MSC_VER
  #include "libavcodec/avcodec.h"
#else
  #include LIBAVCODEC_HEADER
#endif
};

#define P720_WIDTH 720
#define P720_HEIGHT 480
#define CIF4_WIDTH 704
#define CIF4_HEIGHT 576
#define CIF_WIDTH 352
#define CIF_HEIGHT 288
#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144
#define SQCIF_WIDTH 128
#define SQCIF_HEIGHT 96
#define IT_QCIF 0
#define IT_CIF 1

typedef unsigned char u_char;

static void logCallbackFFMPEG (void* v, int level, const char* fmt , va_list arg);

class H264EncoderContext 
{
  public:
    H264EncoderContext ();
    ~H264EncoderContext ();

    int EncodeFrames (const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags);

    void SetMaxRTPFrameSize (unsigned size);
    void SetMaxKeyFramePeriod (unsigned period);
    void SetTargetBitrate (unsigned rate);
    void SetFrameWidth (unsigned width);
    void SetFrameHeight (unsigned height);
    void SetFrameRate (unsigned rate);
    void SetTSTO (unsigned tsto);
    void SetProfileLevel (unsigned profile, unsigned constraints, unsigned level);
    void ApplyOptions ();
    void Lock ();
    void Unlock ();

  protected:
    CriticalSection _mutex;
};

class H264DecoderContext
{
  public:
    H264DecoderContext();
    ~H264DecoderContext();
    int DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags);

  protected:
    CriticalSection _mutex;

    AVCodec* _codec;
    AVCodecContext* _context;
    AVFrame* _outputFrame;
    H264Frame* _rxH264Frame;

    bool _gotIFrame;
    bool _gotAGoodFrame;
    int _frameCounter;
    int _skippedFrameCounter;
};

static int valid_for_protocol    ( const struct PluginCodec_Definition *, void *, const char *,
                                   void * parm, unsigned * parmLen);
static int get_codec_options     ( const struct PluginCodec_Definition * codec, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int free_codec_options    ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);

static void * create_encoder     ( const struct PluginCodec_Definition *);
static void destroy_encoder      ( const struct PluginCodec_Definition *, void * _context);
static int codec_encoder         ( const struct PluginCodec_Definition *, void * _context,
                                   const void * from, unsigned * fromLen,
                                   void * to, unsigned * toLen,
                                   unsigned int * flag);
static int to_normalised_options ( const struct PluginCodec_Definition *, void *, const char *,
                                   void * parm, unsigned * parmLen);
static int to_customised_options ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_set_options   ( const struct PluginCodec_Definition *, void * _context, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_get_output_data_size ( const PluginCodec_Definition *, void *, const char *,
                                   void *, unsigned *);

static void * create_decoder     ( const struct PluginCodec_Definition *);
static void destroy_decoder      ( const struct PluginCodec_Definition *, void * _context);
static int codec_decoder         ( const struct PluginCodec_Definition *, void * _context, 
                                   const void * from, unsigned * fromLen,
                                   void * to, unsigned * toLen,
                                   unsigned int * flag);
static int decoder_get_output_data_size ( const PluginCodec_Definition * codec, void *, const char *,
                                   void *, unsigned *);

///////////////////////////////////////////////////////////////////////////////
static int merge_profile_level_h264(char ** result, const char * dest, const char * src);
static int merge_packetization_mode(char ** result, const char * dest, const char * src);
static void free_string(char * str);
///////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1143692893,                                                   // timestamp = Thu 30 Mar 2006 04:28:13 AM UTC

  "Matthias Schneider",                                         // source code author
  "1.0",                                                        // source code version
  "ma30002000@yahoo.de",                                        // source code email
  "",                                                           // source code URL
  "Copyright (C) 2006 by Matthias Schneider",                   // source code copyright
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license

  "x264 / ffmpeg H.264",                                        // codec description
  "x264: Laurent Aimar, ffmpeg: Michael Niedermayer",           // codec author
  "", 							        // codec version
  "fenrir@via.ecp.fr, ffmpeg-devel-request@mplayerhq.hu",       // codec email
  "http://developers.videolan.org/x264.html, \
   http://ffmpeg.mplayerhq.hu", 				// codec URL
  "x264: Copyright (C) 2003 Laurent Aimar, \
   ffmpeg: Copyright (c) 2002-2003 Michael Niedermayer",        // codec copyright information
  "x264: GNU General Public License as published Version 2, \
   ffmpeg: GNU Lesser General Public License, Version 2.1",     // codec license
  PluginCodec_License_GPL                                       // codec license code
};

static const char YUV420PDesc[]  = { "YUV420P" };

static PluginCodec_ControlDefn EncoderControls[] = {
  { PLUGINCODEC_CONTROL_VALID_FOR_PROTOCOL,    valid_for_protocol },
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     encoder_set_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  encoder_get_output_data_size },
  { NULL }
};

static PluginCodec_ControlDefn DecoderControls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  decoder_get_output_data_size },
  { NULL }
};

/////////////////////////////////////////////////////////////////////////////
// SIP definitions
/*
Still to consider
       sprop-parameter-sets: this may be a NAL
       max-mbps, max-fs, max-cpb, max-dpb, and max-br
       parameter-add
       max-rcmd-nalu-size:
*/

static const char h264Desc[]      = { "H.264" };
static const char sdpH264[]       = { "h264" };

#define H264_CLOCKRATE        90000
#define H264_BITRATE         768000
#define H264_PAYLOAD_SIZE      1400
#define H264_FRAME_RATE          25
#define H264_KEY_FRAME_INTERVAL  75

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Option const RFC3984packetizationMode =
{
  PluginCodec_StringOption,             // Option type
  "CAP RFC3894 Packetization Mode",     // User visible name
  false,                                // User Read/Only flag
  PluginCodec_CustomMerge,              // Merge mode
  "1",                                  // Initial value (Baseline, Level 3)
  "packetization-mode",                 // FMTP option name 
  "5",                                  // FMTP default value
  0,
  "1",
  "5",
  merge_packetization_mode,             // Function to do merge
  free_string                           // Function to free memory in string
};

static struct PluginCodec_Option const RFC3984profileLevel =
{
  PluginCodec_StringOption,             // Option type
  "CAP RFC3894 Profile Level",          // User visible name
  false,                                // User Read/Only flag
  PluginCodec_CustomMerge,              // Merge mode
  "42C01E",                             // Initial value (Baseline, Level 3)
  "profile-level-id",                   // FMTP option name 
  "000000",                             // FMTP default value
  0,
  "000000",
  "58F033",
  merge_profile_level_h264,             // Function to do merge
  free_string                           // Function to free memory in string
};

static struct PluginCodec_Option const * const optionTable[] = {
  &RFC3984packetizationMode,
  &RFC3984profileLevel,
  NULL
};

//#define H323_H264_TEST 1
#ifdef H323_H264_TEST
///////////////////////////////////////////////////////////////////////////
// H.323 Definitions

// MEGA MACRO to set options
#define DECLARE_GENERIC_OPTIONS(prefix) \
static struct PluginCodec_Option const prefix##_h323profile = \
  { PluginCodec_IntegerOption, "Profile", false, PluginCodec_NoMerge, (const char *)prefix##_Profile , "profile", "0", 0, "0", "0" }; \
static struct PluginCodec_Option const prefix##_h323level = \
 { PluginCodec_IntegerOption, "Level", false, PluginCodec_NoMerge, (const char *)prefix##_Level , "level", "0", 0, "0", "0" }; \
static struct PluginCodec_Option const * const prefix##_OptionTable[] = { \
  &prefix##_h323profile, \
  &prefix##_h323level, \
  NULL \
}; \
static const struct PluginCodec_H323GenericParameterDefinition prefix##_h323params[] = \
{ \
	{1,41, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_BooleanArray,{prefix##_Profile}}, \
	{1,42, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin,{prefix##_Level}}, \
	NULL \
}; \
static struct PluginCodec_H323GenericCodecData prefix##_h323GenericData[] = { \
    OpalPluginCodec_Identifer_H264_RFC3984, \
	prefix##_MaxBitRate, \
    sizeof(prefix##_h323params), \
	prefix##_h323params \
}; \

#define DECLARE_H323PARAM(prefix) \
{ \
  /* encoder */ \
  PLUGIN_CODEC_VERSION_OPTIONS,	      /* codec API version */ \
  &licenseInfo,                       /* license information */ \
  prefix##_VideoType |                /* video type */ \
  PluginCodec_RTPTypeShared |         /* specified RTP type */ \
  PluginCodec_RTPTypeDynamic,         /* specified RTP type */ \
  prefix##_Desc,                      /* text decription */ \
  YUV420PDesc,                        /* source format */ \
  prefix##_MediaFmt,                  /* destination format */ \
  prefix##_OptionTable,			      /* user data */ \
  H264_CLOCKRATE,                     /* samples per second */ \
  H264_BITRATE,				          /* raw bits per second */ \
  20000,                              /* nanoseconds per frame */ \
  prefix##_FrameHeight,               /* samples per frame */ \
  prefix##_FrameWidth,			      /* bytes per frame */ \
  10,                                 /* recommended number of frames per packet */ \
  60,                                 /* maximum number of frames per packet  */ \
  0,                                  /* IANA RTP payload code */ \
  NULL,                               /* RTP payload name */ \
  create_encoder,                     /* create codec function */ \
  destroy_encoder,                    /* destroy codec */ \
  codec_encoder,                      /* encode/decode */ \
  EncoderControls,                    /* codec controls */ \
  PluginCodec_H323Codec_generic,      /* h323CapabilityType */ \
  (struct PluginCodec_H323GenericCodecData *)&prefix##_h323GenericData /* h323CapabilityData */ \
}, \
{  \
  /* decoder */ \
  PLUGIN_CODEC_VERSION_OPTIONS,	      /* codec API version */ \
  &licenseInfo,                       /* license information */ \
  PluginCodec_MediaTypeVideo |        /* audio codec */ \
  PluginCodec_RTPTypeShared |         /* specified RTP type */ \
  PluginCodec_RTPTypeDynamic,         /* specified RTP type */ \
  prefix##_Desc,                      /* text decription */ \
  prefix##_MediaFmt,                  /* source format */ \
  YUV420PDesc,                        /* destination format */ \
  prefix##_OptionTable,			      /* user data */ \
  H264_CLOCKRATE,                     /* samples per second */ \
  H264_BITRATE,				          /* raw bits per second */ \
  20000,                              /* nanoseconds per frame */ \
  prefix##_FrameHeight,               /* samples per frame */ \
  prefix##_FrameWidth,			      /* bytes per frame */ \
  10,                                 /* recommended number of frames per packet */ \
  60,                                 /* maximum number of frames per packet  */ \
  0,                                  /* IANA RTP payload code */ \
  NULL,                               /* RTP payload name */ \
  create_decoder,                     /* create codec function */ \
  destroy_decoder,                    /* destroy codec */ \
  codec_decoder,                      /* encode/decode */ \
  DecoderControls,                  /* codec controls */ \
  PluginCodec_H323Codec_generic,      /* h323CapabilityType */ \
  (struct PluginCodec_H323GenericCodecData *)&prefix##_h323GenericData /* h323CapabilityData */ \
} \


/////////////////////////////////////////////////////////////////////////////
// Codec Definitions

// SIP 42E015 is 
//  Profile : H264_PROFILE_BASE + H264_PROFILE_MAIN 
//  Level : 2:1  compatible H.323 codec is 4CIF.

#define H264_PROFILE_BASE      64   // only do base and main at this stage
#define H264_PROFILE_MAIN      32
#define H264_PROFILE_EXTENDED  16
#define H264_PROFILE_HIGH       8   

// NOTE: All these values are subject to change Values need to be confirmed!
#define H264_LEVEL1           15      // SQCIF  30 fps
#define H264_LEVEL1_MBPS      64000
#define H264_LEVEL1b          19      // QCIF 15 fps
#define H264_LEVEL1b_MBPS     128000
#define H264_LEVEL1_2         29      //  CIF 15 fps / QCIF 30 fps 
#define H264_LEVEL1_2_MBPS    384000
#define H264_LEVEL2_1         50      // 4CIF 15fps / cif 30fps - To Match SIP 42E015
#define H264_LEVEL2_1_MBPS    4000000
#define H264_LEVEL3           64      // 720P 30fps ? Need to confirm
#define H264_LEVEL3_MBPS      10000000

// H.264 SCIF
static const char     H264SQCIF_Desc[]          = { "H.264-SQCIF" };
static const char     H264SQCIF_MediaFmt[]      = { "H.264-SQCIF" };                             
static unsigned int   H264SQCIF_FrameHeight     = SQCIF_HEIGHT;               
static unsigned int   H264SQCIF_FrameWidth      = SQCIF_WIDTH; 
static unsigned int   H264SQCIF_Profile         = H264_PROFILE_BASE + H264_PROFILE_MAIN;
static unsigned int   H264SQCIF_Level           = H264_LEVEL1;
static unsigned int   H264SQCIF_MaxBitRate      = H264_LEVEL1_MBPS;
static unsigned int   H264SQCIF_VideoType       = PluginCodec_MediaTypeVideo;
DECLARE_GENERIC_OPTIONS(H264SQCIF)

// H.264 QCIF
static const char     H264QCIF_Desc[]          = { "H.264-QCIF" };
static const char     H264QCIF_MediaFmt[]      = { "H.264-QCIF" };                             
static unsigned int   H264QCIF_FrameHeight     = QCIF_HEIGHT;               
static unsigned int   H264QCIF_FrameWidth      = QCIF_WIDTH;
static unsigned int   H264QCIF_Profile         = H264_PROFILE_BASE + H264_PROFILE_MAIN;
static unsigned int   H264QCIF_Level           = H264_LEVEL1b;
static unsigned int   H264QCIF_MaxBitRate      = H264_LEVEL1b_MBPS;
static unsigned int   H264QCIF_VideoType       = PluginCodec_MediaTypeVideo;
DECLARE_GENERIC_OPTIONS(H264QCIF)


// H.264 CIF
static const char     H264CIF_Desc[]          = { "H.264-CIF" };
static const char     H264CIF_MediaFmt[]      = { "H.264-CIF" };                             
static unsigned int   H264CIF_FrameHeight     = CIF_HEIGHT;               
static unsigned int   H264CIF_FrameWidth      = CIF_WIDTH; 
static unsigned int   H264CIF_Profile         = H264_PROFILE_BASE + H264_PROFILE_MAIN;
static unsigned int   H264CIF_Level           = H264_LEVEL1_2;
static unsigned int   H264CIF_MaxBitRate      = H264_LEVEL1_2_MBPS;
static unsigned int   H264CIF_VideoType       = PluginCodec_MediaTypeVideo | PluginCodec_MediaTypeExtVideo;
DECLARE_GENERIC_OPTIONS(H264CIF)


// H.264 CIF4   // compatible SIP profile
static const char     H264CIF4_Desc[]          = { "H.264-4CIF" };
static const char     H264CIF4_MediaFmt[]      = { "H.264-4CIF" };                             
static unsigned int   H264CIF4_FrameHeight     = CIF4_HEIGHT;               
static unsigned int   H264CIF4_FrameWidth      = CIF4_WIDTH;
static unsigned int   H264CIF4_Profile         = H264_PROFILE_BASE + H264_PROFILE_MAIN;
static unsigned int   H264CIF4_Level           = H264_LEVEL2_1;
static unsigned int   H264CIF4_MaxBitRate      = H264_LEVEL2_1_MBPS;
static unsigned int   H264CIF4_VideoType       = PluginCodec_MediaTypeVideo | PluginCodec_MediaTypeExtVideo;
DECLARE_GENERIC_OPTIONS(H264CIF4)


// H.264 720P
static const char     H264720P_Desc[]          = { "H.264-720P" };
static const char     H264720P_MediaFmt[]      = { "H.264-720P" };                             
static unsigned int   H264720P_FrameHeight     = P720_HEIGHT;               
static unsigned int   H264720P_FrameWidth      = P720_WIDTH; 
static unsigned int   H264720P_Profile         = H264_PROFILE_BASE + H264_PROFILE_MAIN;
static unsigned int   H264720P_Level           = H264_LEVEL3;
static unsigned int   H264720P_MaxBitRate      = H264_LEVEL3_MBPS;
static unsigned int   H264720P_VideoType       = /*PluginCodec_MediaTypeVideo |*/PluginCodec_MediaTypeExtVideo;
DECLARE_GENERIC_OPTIONS(H264720P)

#endif

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition h264CodecDefn[] = {
{ 

  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeShared |         // specified RTP type
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  h264Desc,                           // text decription
  YUV420PDesc,                        // source format
  h264Desc,                           // destination format

  optionTable,                        // user data 

  H264_CLOCKRATE,                     // samples per second
  H264_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF4_WIDTH,                       // frame width
    CIF4_HEIGHT,                      // frame height
    10,                               // recommended frame rate
    60,                               // maximum frame rate
  }},

  0,                                  // IANA RTP payload code
  sdpH264,                            // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  EncoderControls,                    // codec controls

  PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
  NULL                                // h323CapabilityData
},
{ 
  // SIP decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeShared |         // specified RTP type
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  h264Desc,                           // text decription
  h264Desc,                           // source format
  YUV420PDesc,                        // destination format

  optionTable,                        // user data 

  H264_CLOCKRATE,                     // samples per second
  H264_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF4_WIDTH,                       // frame width
    CIF4_HEIGHT,                      // frame height
    10,                               // recommended frame rate
    60,                               // maximum frame rate
  }},
  0,                                  // IANA RTP payload code
  sdpH264,                            // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
  NULL                                // h323CapabilityData
},
#ifdef H323_H264_TEST
  DECLARE_H323PARAM(H264SQCIF),
  DECLARE_H323PARAM(H264QCIF),
  DECLARE_H323PARAM(H264CIF),
  DECLARE_H323PARAM(H264CIF4),
  DECLARE_H323PARAM(H264720P)
#endif
};

#endif /* __H264-X264_H__ */
