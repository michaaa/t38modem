/*
 * THEORA Plugin codec for OpenH323/OPAL
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
 *
 *
 */

/*
  Notes
  -----

 */

#ifndef __THEORA_PLUGIN_H__
#define __THEORA_PLUGIN_H__ 1

#include "plugin-config.h"
#include <stdarg.h>
#include <stdint.h>
#include <codec/opalplugin.h>
#include "theora_frame.h"
#include "critsect.h"

extern "C" {
  #include <theora/theora.h>
};

#define CIF4_WIDTH 704
#define CIF4_HEIGHT 576
#define CIF_WIDTH 352
#define CIF_HEIGHT 288
#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144
#define IT_QCIF 0
#define IT_CIF 1

typedef unsigned char u_char;

class theoraEncoderContext 
{
  public:
    theoraEncoderContext ();
    ~theoraEncoderContext ();

    void SetMaxRTPFrameSize (unsigned size);
    void SetMaxKeyFramePeriod (unsigned period);
    void SetTargetBitrate (unsigned rate);
    void SetFrameRate (unsigned rate);
    void SetFrameWidth (unsigned width);
    void SetFrameHeight (unsigned height);
    void ApplyOptions ();
    void Lock ();
    void Unlock ();

    int EncodeFrames (const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags);

  protected:
    void ApplyOptionsInternal ();
    CriticalSection _mutex;

    theora_info _theoraInfo;
    theora_state _theoraState;

    int _frameCounter;
    theoraFrame* _txTheoraFrame;
};

class theoraDecoderContext
{
  public:
    theoraDecoderContext();
    ~theoraDecoderContext();
    int DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags);

  protected:
    CriticalSection _mutex;

    theora_info _theoraInfo;
    theora_state _theoraState;
    theoraFrame* _rxTheoraFrame;

    bool _gotIFrame;
    bool _gotAGoodFrame;
    bool _gotHeader;
    bool _gotTable;
    int _frameCounter;
};

const char* theoraErrorMessage(int code);

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


static struct PluginCodec_information licenseInfo = {
  1143692893,                                                   // timestamp = Thu 30 Mar 2006 04:28:13 AM UTC

  "Matthias Schneider",                                         // source code author
  "1.0",                                                        // source code version
  "ma30002000@yahoo.de",                                        // source code email
  "",                                                           // source code URL
  "Copyright (C) 2007 by Matthias Schneider",                   // source code copyright
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license

  "libtheora",                                                  // codec description
  "",                                                           // codec author
  "1.0 alpha7", 					        // codec version
  "theora@xiph.org",                                            // codec email
  "http://www.theora.org/", 		      		        // codec URL
  "Xiph.Org Foundation http://www.xiph.org",                    // codec copyright information
  "BSD-style",                                                  // codec license
  PluginCodec_License_BSD                                       // codec license code
};

/////////////////////////////////////////////////////////////////////////////

static const char YUV420PDesc[]  = { "YUV420P" };
static const char theoraDesc[]   = { "theora" };
static const char sdpTHEORA[]    = { "theora" };
#define THEORA_CLOCKRATE        90000
#define THEORA_BITRATE         768000
#define THEORA_PAYLOAD_SIZE      1400
#define THEORA_FRAME_RATE          25
#define THEORA_KEY_FRAME_INTERVAL 125

/////////////////////////////////////////////////////////////////////////////

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

static struct PluginCodec_Option const samplingOption =
  { PluginCodec_StringOption, "CAP Sampling",  false, PluginCodec_EqualMerge, "YCbCr-4:2:0", "sampling", "YCbCr-4:2:0"};

static struct PluginCodec_Option const widthOption =
  { PluginCodec_IntegerOption, "CAP Width", false, PluginCodec_MinMerge, "704", "width", "15", 0, "15", "1280" };

static struct PluginCodec_Option const heightOption =
  { PluginCodec_IntegerOption, "CAP Height",  false, PluginCodec_MinMerge, "576", "height",  "15", 0, "15", "720" };

static struct PluginCodec_Option const deliveryOption =
  { PluginCodec_StringOption, "CAP Delivery",  false, PluginCodec_EqualMerge, "in_band", "delivery-method",  "in_band"};

static struct PluginCodec_Option const minRxFrameWidth =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "176", NULL, NULL, 0, "16", "1280" };
static struct PluginCodec_Option const minRxFrameHeight =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "144", NULL, NULL, 0, "16", "720"  };
static struct PluginCodec_Option const maxRxFrameWidth =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "1280", NULL, NULL, 0, "16", "1280"  };
static struct PluginCodec_Option const maxRxFrameHeight =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "720", NULL, NULL, 0, "16", "720"  };


//configuration

static struct PluginCodec_Option const * const optionTable[] = {
  &samplingOption,
  &widthOption,
  &heightOption,
  &deliveryOption,
  &minRxFrameWidth,
  &minRxFrameHeight,
  &maxRxFrameWidth,
  &maxRxFrameHeight,
  NULL
};

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition theoraCodecDefn[2] = {
{ 
  // SIP encoder#define NUM_DEFNS   (sizeof(theoraCodecDefn) / sizeof(struct PluginCodec_Definition))
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeShared |         // specified RTP type
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  theoraDesc,                         // text decription
  YUV420PDesc,                        // source format
  theoraDesc,                         // destination format

  optionTable,                        // user data 

  THEORA_CLOCKRATE,                   // samples per second
  THEORA_BITRATE,                     // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF4_WIDTH,                       // frame width
    CIF4_HEIGHT,                      // frame height
    10,                               // recommended frame rate
    60,                               // maximum frame rate
  }},

  0,                                  // IANA RTP payload code
  sdpTHEORA,                          // RTP payload name

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

  theoraDesc,                         // text decription
  theoraDesc,                         // source format
  YUV420PDesc,                        // destination format

  optionTable,                        // user data 

  THEORA_CLOCKRATE,                   // samples per second
  THEORA_BITRATE,                     // raw bits per second
  20000,                              // nanoseconds per frame

{{
  CIF4_WIDTH,                         // frame width
  CIF4_HEIGHT,                        // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
}},
  0,                                  // IANA RTP payload code
  sdpTHEORA,                          // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323Codec_NoH323,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},
};

#endif /* __THEORA_H__ */
