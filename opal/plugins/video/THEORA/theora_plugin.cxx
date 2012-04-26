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

#include "theora_plugin.h"

#include "trace.h"
#include "rtpframe.h"

#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN32_WCE)
  #include <malloc.h>
  #define STRCMPI  _strcmpi
#else
  #include <semaphore.h>
  #define STRCMPI  strcasecmp
#endif
#include <string.h>
#include <stdio.h>

theoraEncoderContext::theoraEncoderContext()
{
  ogg_packet headerPacket, tablePacket;
  _frameCounter=0;
  
  _txTheoraFrame = new theoraFrame();
  _txTheoraFrame->SetMaxPayloadSize(THEORA_PAYLOAD_SIZE);

  theora_info_init( &_theoraInfo );
  _theoraInfo.frame_width        = 352;  // Must be multiple of 16
  _theoraInfo.frame_height       = 288; // Must be multiple of 16
  _theoraInfo.width              = _theoraInfo.frame_width;
  _theoraInfo.height             = _theoraInfo.frame_height;
  _theoraInfo.offset_x           = 0;
  _theoraInfo.offset_y           = 0;
  _theoraInfo.fps_numerator      = THEORA_FRAME_RATE;
  _theoraInfo.fps_denominator    = 1;
  _theoraInfo.aspect_numerator   = _theoraInfo.width;  // Aspect =  width/height
  _theoraInfo.aspect_denominator = _theoraInfo.height; //
  _theoraInfo.colorspace         = OC_CS_UNSPECIFIED;
  _theoraInfo.target_bitrate     = THEORA_BITRATE * 2 / 3; // Anywhere between 45kbps and 2000kbps
  _theoraInfo.quality            = 16; 
  _theoraInfo.dropframes_p                 = 0;
  _theoraInfo.quick_p                      = 1;
  _theoraInfo.keyframe_auto_p              = 1;
  _theoraInfo.keyframe_frequency = THEORA_KEY_FRAME_INTERVAL;
  _theoraInfo.keyframe_frequency_force     = _theoraInfo.keyframe_frequency;
  _theoraInfo.keyframe_data_target_bitrate = THEORA_BITRATE;
  _theoraInfo.keyframe_auto_threshold      = 80;
  _theoraInfo.keyframe_mindistance         = 8;
  _theoraInfo.noise_sensitivity            = 1;

  theora_encode_init( &_theoraState, &_theoraInfo );

  theora_encode_header( &_theoraState, &headerPacket );
  _txTheoraFrame->SetFromHeaderConfig(&headerPacket);

  theora_encode_tables( &_theoraState, &tablePacket );
  _txTheoraFrame->SetFromTableConfig(&tablePacket);
}

theoraEncoderContext::~theoraEncoderContext()
{
  theora_clear( &_theoraState );
  theora_info_clear (&_theoraInfo);
  if (_txTheoraFrame) delete _txTheoraFrame;
}

void theoraEncoderContext::SetTargetBitrate(unsigned rate)
{
  _theoraInfo.target_bitrate     = rate * 2 / 3; // Anywhere between 45kbps and 2000kbps}
  _theoraInfo.keyframe_data_target_bitrate = rate;
}

void theoraEncoderContext::SetFrameRate(unsigned rate)
{
  _theoraInfo.fps_numerator      = (int)((rate + .5) * 1000); // ???
  _theoraInfo.fps_denominator    = 1000;
}

void theoraEncoderContext::SetFrameWidth(unsigned width)
{
  _theoraInfo.frame_width        = width;  // Must be multiple of 16
  _theoraInfo.width              = _theoraInfo.frame_width;
}

void theoraEncoderContext::SetFrameHeight(unsigned height)
{
  _theoraInfo.frame_height       = height; // Must be multiple of 16
  _theoraInfo.height             = _theoraInfo.frame_height;
}

void theoraEncoderContext::ApplyOptions()
{
  ogg_packet headerPacket, tablePacket;

  theora_clear( &_theoraState );
  theora_encode_init( &_theoraState, &_theoraInfo );

  theora_encode_header( &_theoraState, &headerPacket );
  _txTheoraFrame->SetFromHeaderConfig(&headerPacket);

  theora_encode_tables( &_theoraState, &tablePacket );
  _txTheoraFrame->SetFromTableConfig(&tablePacket);
}
void theoraEncoderContext::Lock()
{
  _mutex.Wait();
}

void theoraEncoderContext::Unlock()
{
  _mutex.Signal();
}

int theoraEncoderContext::EncodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);
  int ret;
  yuv_buffer yuv;
  ogg_packet framePacket;

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen);

  dstLen = 0;

  // from here, we are encoding a new frame
  if (!_txTheoraFrame)
  {
    return 0;
  }

  // if there are RTP packets to return, return them
  if  (_txTheoraFrame->HasRTPFrames())
  {
    _txTheoraFrame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    return 1;
  }

  if (srcRTP.GetPayloadSize() < sizeof(frameHeader))
  {
   TRACE(1, "THEORA\tEncoder\tVideo grab too small, Close down video transmission thread");
   return 0;
  }

  frameHeader * header = (frameHeader *)srcRTP.GetPayloadPtr();
  if (header->x != 0 || header->y != 0)
  {
    TRACE(1, "THEORA\tEncoder\tVideo grab of partial frame unsupported, Close down video transmission thread");
    return 0;
  }

  // do a validation of size
  // if the incoming data has changed size, tell the encoder
  if (_theoraInfo.frame_width != header->width || _theoraInfo.frame_height != header->height)
  {
    _theoraInfo.frame_width        = header->width;  // Must be multiple of 16
    _theoraInfo.frame_height       = header->height; // Must be multiple of 16
    _theoraInfo.width              = _theoraInfo.frame_width;
    _theoraInfo.height             = _theoraInfo.frame_height;
    _theoraInfo.aspect_numerator   = _theoraInfo.width;  // Aspect =  width/height
    _theoraInfo.aspect_denominator = _theoraInfo.height; //
    ApplyOptions();
  } 

  // Prepare the frame to be encoded
  yuv.y_width   = header->width;
  yuv.y_height  = _theoraInfo.height;
  yuv.uv_width  = (int) (header->width / 2);
  yuv.uv_height = (int) (_theoraInfo.height / 2);
  yuv.y_stride  = header->width;
  yuv.uv_stride = (int) (header->width /2);
  yuv.y         = (unsigned char *)(((unsigned char *)header) + sizeof(frameHeader));
  yuv.u         = (unsigned char *)((((unsigned char *)header) + sizeof(frameHeader)) 
                           + (int)(yuv.y_stride*header->height)); 
  yuv.v         = (unsigned char *)(yuv.u + (int)(yuv.uv_stride *header->height/2)); 

  ret = theora_encode_YUVin( &_theoraState, &yuv );
  if (ret != 0) {
    if (ret == -1) {
      TRACE(1, "THEORA\tEncoder\tEncoding failed: The size of the given frame differs from those previously input (should not happen)")
    } else {
      TRACE(1, "THEORA\tEncoder\tEncoding failed: " << theoraErrorMessage(ret));
    }
    return 0;
  }

  ret = theora_encode_packetout( &_theoraState, 0 /* not last Packet */, &framePacket );
  switch (ret) {
    case  0: TRACE(1, "THEORA\tEncoder\tEncoding failed (packet): No internal storage exists OR no packet is ready"); return 0; break;
    case -1: TRACE(1, "THEORA\tEncoder\tEncoding failed (packet): The encoding process has completed but something is not ready yet"); return 0; break;
    case  1: TRACE_UP(4, "THEORA\tEncoder\tSuccessfully encoded OGG packet of " << framePacket.bytes << " bytes"); break;
    default: TRACE(1, "THEORA\tEncoder\tEncoding failed (packet): " << theoraErrorMessage(ret)); return 0; break;
  }

  _txTheoraFrame->SetFromFrame(&framePacket);
  _txTheoraFrame->SetIsIFrame(theora_packet_iskeyframe(&framePacket));
  _txTheoraFrame->SetTimestamp(srcRTP.GetTimestamp());
  _frameCounter++; 

  if (_txTheoraFrame->HasRTPFrames())
  {
    _txTheoraFrame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    return 1;
  }

  return 0;
}

void theoraEncoderContext::SetMaxRTPFrameSize (unsigned size)
{
  _txTheoraFrame->SetMaxPayloadSize(size);
}

void theoraEncoderContext::SetMaxKeyFramePeriod (unsigned period)
{
  _theoraInfo.keyframe_frequency = period;
}

theoraDecoderContext::theoraDecoderContext()
{
  _frameCounter = 0; 
  _gotHeader = false;
  _gotTable  = false;
  _gotIFrame = false;
  _gotAGoodFrame = true;

  _rxTheoraFrame = new theoraFrame();
  theora_info_init( &_theoraInfo );
}

theoraDecoderContext::~theoraDecoderContext()
{
  if (_gotHeader && _gotTable) theora_clear( &_theoraState );
  theora_info_clear( &_theoraInfo );
  if (_rxTheoraFrame) delete _rxTheoraFrame;
}

int theoraDecoderContext::DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen, 0);
  dstLen = 0;

  if (!_rxTheoraFrame->SetFromRTPFrame(srcRTP, flags)) {
    _rxTheoraFrame->BeginNewFrame();    
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  };

// linphone does not send markers so for now we ignore them (should be fixed)
//if (srcRTP.GetMarker()==0)
//  return 1;

  if (!_rxTheoraFrame->HasOggPackets())
    return 1;

  yuv_buffer yuv;
  ogg_packet oggPacket;
  theora_comment theoraComment;
  bool gotFrame = false;
  int ret;

  while (_rxTheoraFrame->HasOggPackets()) {
    _rxTheoraFrame->GetOggPacket (&oggPacket);
    if (theora_packet_isheader(&oggPacket)) {

      TRACE_UP(4, "THEORA\tDecoder\tGot OGG header packet with size " << oggPacket.bytes);

      // In case we receive new header packets when the stream is already established
      // we have to wait to get both table and header packet until reopening the decoder
      if (_gotHeader && _gotTable) {
        TRACE(4, "THEORA\tDecoder\tGot OGG header packet after stream was established");
        theora_clear( &_theoraState );
        theora_info_clear( &_theoraInfo );
        theora_info_init( &_theoraInfo );
        _gotHeader = false;
        _gotTable = false; 
        _gotIFrame = false;
      }

      theora_comment_init(&theoraComment);
      theoraComment.vendor = (char*) "theora";
      ret = theora_decode_header( &_theoraInfo, &theoraComment, &oggPacket );
      if (ret != 0) {
        TRACE(1, "THEORA\tDecoder\tDecoding failed (header packet): " << theoraErrorMessage(ret));
        flags = (_gotAGoodFrame ? requestIFrame : 0);
        _gotAGoodFrame = false;
        return 1;
      }

      if (oggPacket.bytes == THEORA_HEADER_PACKET_SIZE) 
        _gotHeader = true; 
       else 
        _gotTable = true;

      if (_gotHeader && _gotTable) theora_decode_init( &_theoraState, &_theoraInfo );
      if (!_rxTheoraFrame->HasOggPackets())
        return 1;
    } 
    else {

      if (_gotHeader && _gotTable) {

        if (theora_packet_iskeyframe(&oggPacket)) {

          TRACE_UP(4, "THEORA\tDecoder\tGot OGG keyframe data packet with size " << oggPacket.bytes);
          ret = theora_decode_packetin( &_theoraState, &oggPacket );
          if (ret != 0) {
            TRACE(1, "THEORA\tDecoder\tDecoding failed (packet): " << theoraErrorMessage(ret));
            flags = (_gotAGoodFrame ? requestIFrame : 0);
            _gotAGoodFrame = false;
            return 1;
          }
          theora_decode_YUVout( &_theoraState, &yuv);
          _gotIFrame = true;
          gotFrame = true;
        } 
        else {

          if  (_gotIFrame) {

            TRACE_UP(4, "THEORA\tDecoder\tGot OGG non-keyframe data packet with size " << oggPacket.bytes);
            ret = theora_decode_packetin( &_theoraState, &oggPacket );
            if (ret != 0) {
              TRACE(1, "THEORA\tDecoder\tDecoding failed (packet): " << theoraErrorMessage(ret));
              flags = (_gotAGoodFrame ? requestIFrame : 0);
              _gotAGoodFrame = false;
              return 1;
            }
            theora_decode_YUVout( &_theoraState, &yuv);
            gotFrame = true;
          } 
          else {

            TRACE(1, "THEORA\tDecoder\tGot OGG non-keyframe data Packet but still waiting for keyframe");
            flags = (_gotAGoodFrame ? requestIFrame : 0);
            _gotAGoodFrame = false;
            return 1;
          }
        }
      } 
      else {

        TRACE(1, "THEORA\tDecoder\tGot OGG data packet but still waiting for header and/or table Packets");
        return 0;
      }
    }
  }

  TRACE_UP(4, "THEORA\tDecoder\tNo more OGG packets to decode");

  if (gotFrame) {

    int size = _theoraInfo.width * _theoraInfo.height;
    int frameBytes = (int) (size * 3 / 2);
    PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();

    TRACE_UP(4, "THEORA\tDecoder\tDecoded Frame with resolution: " << _theoraInfo.width << "x" << _theoraInfo.height);

    header->x = header->y = 0;
    header->width = _theoraInfo.width;
    header->height = _theoraInfo.height;

    unsigned int i = 0;
    int width2 = (header->width >> 1);

    uint8_t* dstY = (uint8_t*) OPAL_VIDEO_FRAME_DATA_PTR(header);
    uint8_t* dstU = (uint8_t*) dstY + size;
    uint8_t* dstV = (uint8_t*) dstU + (size >> 2);

    uint8_t* srcY = yuv.y;
    uint8_t* srcU = yuv.u;
    uint8_t* srcV = yuv.v;

    for (i = 0 ; i < header->height ; i+=2) {

      memcpy (dstY, srcY, header->width); 
      srcY +=yuv.y_stride; 
      dstY +=header->width;

      memcpy (dstY, srcY, header->width); 
      srcY +=yuv.y_stride; 
      dstY +=header->width;

      memcpy(dstU, srcU, width2); 
      srcU+=yuv.uv_stride;
      dstU+=width2; 

      memcpy (dstV, srcV, width2); 
      srcV += yuv.uv_stride;
      dstV +=width2; 
    }

    dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
    dstRTP.SetTimestamp(srcRTP.GetTimestamp());
    dstRTP.SetMarker(1);
    dstLen = dstRTP.GetFrameLen();
    flags = PluginCodec_ReturnCoderLastFrame;
    _frameCounter++;
    _gotAGoodFrame = true;
    return 1;
  } 
  else { /*gotFrame */

    TRACE(1, "THEORA\tDecoder\tDid not get a decoded frame");
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }
}

/////////////////////////////////////////////////////////////////////////////

const char*
theoraErrorMessage(int code)
{
  static const char *error;
  static char errormsg [1024];	

  switch (code) {
    case OC_FAULT:
      error = "General failure";
      break;
    case OC_EINVAL:
      error = "Library encountered invalid internal data";
      break;
    case OC_DISABLED:
      error = "Requested action is disabled";
      break;
    case OC_BADHEADER:
      error = "Header packet was corrupt/invalid";
      break;
    case OC_NOTFORMAT:
      error = "Packet is not a theora packet";
      break;
    case OC_VERSION:
      error = "Bitstream version is not handled";
      break;
    case OC_IMPL:
      error = "Feature or action not implemented";
      break;
    case OC_BADPACKET:
      error = "Packet is corrupt";
      break;
    case OC_NEWPACKET:
      error = "Packet is an (ignorable) unhandled extension";
      break;
    case OC_DUPFRAME:
      error = "Packet is a dropped frame";
      break;
    default:
      snprintf (errormsg, sizeof (errormsg), "%u", code);
      return (errormsg);
  }
  snprintf (errormsg, sizeof(errormsg), "%s (%u)", error, code);
  return (errormsg);
}

static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

/////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition *)
{
  return new theoraEncoderContext;
}

static void destroy_encoder(const struct PluginCodec_Definition *, void * _context)
{
  theoraEncoderContext * context = (theoraEncoderContext *)_context;
  delete context;
}

static int codec_encoder(const struct PluginCodec_Definition * ,
                                           void * _context,
                                     const void * from,
                                       unsigned * fromLen,
                                           void * to,
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  theoraEncoderContext * context = (theoraEncoderContext *)_context;
  return context->EncodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  int capWidth = 15;
  int capHeight = 15;
  int frameWidth = 352;
  int frameHeight = 288;

  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], "CAP Width") == 0)
      capWidth = atoi(option[1]);
    else if (STRCMPI(option[0], "CAP Height") == 0)
      capHeight = atoi(option[1]) ;
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      frameWidth = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      frameHeight = atoi(option[1]);
  }

  if ((capWidth == 15) || ( capHeight == 15)) {
    capWidth = 640;
    capHeight = 480;
  }

  frameWidth = min (capWidth, frameWidth);
  frameHeight = min (capHeight, frameHeight);
    
  frameWidth -= frameWidth % 16;
  frameHeight -= frameHeight % 16;

  char ** options = (char **)calloc(5, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_FRAME_WIDTH);
  options[ 1] = num2str(frameWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_FRAME_HEIGHT);
  options[ 3] = num2str(frameHeight);

  return 1;
}

static int to_customised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  int capWidth = 352;
  int capHeight = 288;
  int maxWidth = 1280;
  int maxHeight = 720;

  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH) == 0)
      maxWidth = atoi(option[1]) - (atoi(option[1]) % 16); // FIXME
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT) == 0)
      maxHeight = atoi(option[1]) - (atoi(option[1]) % 16);
    else if (STRCMPI(option[0], "CAP Width") == 0)
      capWidth = atoi(option[1]);
    else if (STRCMPI(option[0], "CAP Height") == 0)
      capHeight = atoi(option[1]) ;

  }

  capWidth = min (capWidth, maxWidth);
  capHeight = min (capHeight, maxHeight);

  capWidth -= capWidth % 16;
  capHeight -= capHeight % 16;
  
  char ** options = (char **)calloc(5, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup("CAP Width");
  options[ 1] = num2str(capWidth);
  options[ 2] = strdup("CAP Height");
  options[ 3] = num2str(capHeight);

  return 1;

}

static int encoder_set_options(
      const struct PluginCodec_Definition *, 
      void * _context, 
      const char *, 
      void * parm, 
      unsigned * parmLen)
{
  theoraEncoderContext * context = (theoraEncoderContext *)_context;

  if (parmLen == NULL || *parmLen != sizeof(const char **)) return 0;

  context->Lock();
  if (parm != NULL) {
    const char ** options = (const char **)parm;
    int i;
    for (i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
         context->SetTargetBitrate(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
         context->SetFrameRate((int)(THEORA_CLOCKRATE / atoi(options[i+1])));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
         context->SetFrameHeight(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
         context->SetFrameWidth(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_MAX_FRAME_SIZE) == 0)
	 context->SetMaxRTPFrameSize (atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
	 context->SetMaxKeyFramePeriod (atoi(options[i+1]));
      TRACE (4, "THEORA\tEncoder\tOption " << options[i] << " = " << atoi(options[i+1]));
    }
    context->ApplyOptions();
  }
  context->Unlock();

  return 1;
}


static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return 2000; //FIXME
}

/////////////////////////////////////////////////////////////////////////////


static void * create_decoder(const struct PluginCodec_Definition *)
{
  return new theoraDecoderContext;
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  theoraDecoderContext * context = (theoraDecoderContext *)_context;
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
  theoraDecoderContext * context = (theoraDecoderContext *)_context;
  return context->DecodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" {

PLUGIN_CODEC_IMPLEMENT(THEORA)

PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
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

  if (version < PLUGIN_CODEC_VERSION_VIDEO) {
    *count = 0;
    return NULL;
  }
  else {
    *count = sizeof(theoraCodecDefn) / sizeof(struct PluginCodec_Definition);
    return theoraCodecDefn;
  }
  
}
};
