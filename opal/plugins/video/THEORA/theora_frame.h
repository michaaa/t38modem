/*****************************************************************************/
/* The contents of this file are subject to the Mozilla Public License       */
/* Version 1.0 (the "License"); you may not use this file except in          */
/* compliance with the License.  You may obtain a copy of the License at     */
/* http://www.mozilla.org/MPL/                                               */
/*                                                                           */
/* Software distributed under the License is distributed on an "AS IS"       */
/* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the  */
/* License for the specific language governing rights and limitations under  */
/* the License.                                                              */
/*                                                                           */
/* The Original Code is the Open H323 Library.                               */
/*                                                                           */
/* The Initial Developer of the Original Code is Matthias Schneider          */
/* Copyright (C) 2007 Matthias Schneider, All Rights Reserved.               */
/*                                                                           */
/* Contributor(s): Matthias Schneider (ma30002000@yahoo.de)                  */
/*                                                                           */
/* Alternatively, the contents of this file may be used under the terms of   */
/* the GNU General Public License Version 2 or later (the "GPL"), in which   */
/* case the provisions of the GPL are applicable instead of those above.  If */
/* you wish to allow use of your version of this file only under the terms   */
/* of the GPL and not to allow others to use your version of this file under */
/* the MPL, indicate your decision by deleting the provisions above and      */
/* replace them with the notice and other provisions required by the GPL.    */
/* If you do not delete the provisions above, a recipient may use your       */
/* version of this file under either the MPL or the GPL.                     */
/*                                                                           */
/* The Original Code was written by Matthias Schneider <ma30002000@yahoo.de> */
/*****************************************************************************/


#ifndef __THEORAFRAME_H__
#define __THEORAFRAME_H__ 1

#include <stdint.h>
#include <vector>
#include "rtpframe.h"

extern "C"
{
  #include <theora/theora.h>
}

#define THEORA_HEADER_PACKET_SIZE               42
#define THEORA_SEND_CONFIG_INTERVAL             250
// (in frames)

enum theoraTDT { RAW_THEORA_PAYLOAD = 0,
                 THEORA_PACKED_CONFIG_PAYLOAD,
                 LEGACY_THEORA_COMMENT_PAYLOAD,
                 RESERVED 
                };

enum theoraFT { NOT_FRAGMENTED = 0,
                 START_FRAGMENT, 
                 CONTINUATION_FRAGMENT,
                 END_FRAGMENT
               };

enum codecInFlags {
  silenceFrame      = 1,
  forceIFrame       = 2
};

enum codecOutFlags {
  isLastFrame     = 1,
  isIFrame        = 2,
  requestIFrame   = 4
};

typedef struct data_t
{
  uint32_t pos;
  uint32_t len;
  uint8_t* ptr;
} data_t;

typedef struct packet_t
{
  uint32_t pos;
  uint16_t len;
} packet_t;


class theoraFrame
{
public:
  theoraFrame();
  ~theoraFrame();

  void BeginNewFrame();
  void SetFromHeaderConfig (ogg_packet* tablePacket);
  void SetFromTableConfig (ogg_packet* tablePacket);
  void SetMaxPayloadSize (uint16_t maxPayloadSize) { _maxPayloadSize = maxPayloadSize; }
  void SetTimestamp (uint64_t timestamp) { _timestamp = timestamp; }

  void SetFromFrame (ogg_packet* framePacket);
  bool HasRTPFrames () { return (_encodedData.len > 0); }
  void GetRTPFrame (RTPFrame & frame, unsigned int & flags);

  bool SetFromRTPFrame (RTPFrame & frame, unsigned int & flags);
  bool HasOggPackets ();
  void GetOggPacket(ogg_packet* oggPacket);
  void SetIsIFrame (bool isAnIFrame) { _isIFrame = isAnIFrame; }
  bool IsIFrame () {return (_isIFrame); }

private:
  void assembleRTPFrame(RTPFrame & frame, data_t & frameData, bool sendPackedConfig);
  bool disassembleRTPFrame(RTPFrame & frame, data_t & frameData, bool isPackedConfig);

    // general stuff
  uint64_t _timestamp;
  uint16_t _maxPayloadSize;

  data_t _packedConfigData;
  data_t _encodedData;
  std::vector<packet_t> _packets;

  bool _configSent;
  uint32_t _frameCount;
  bool _isIFrame;

  // for deencapsulation
  bool _headerReturned;
  uint32_t _ident;
};

#endif /* __THEORAFRAME_H__ */
