/*  
 * Fax plugin codec for OPAL using SpanDSP
 *
 * Copyright (C) 2008 Post Increment, All Rights Reserved
 *
 * Contributor(s): Craig Southeren (craigs@postincrement.com)
 *                 Robert Jongbloed (robertj@voxlucida.com.au)
 *                 Vyacheslav Frolov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Revision$
 * $Author$
 * $Date$
 */


#define LOGGING 1
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_CONTEXT_ID 3

extern "C" {
#include "spandsp.h"
};

#include <codec/opalplugin.h>


#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN32_WCE)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <io.h>
  #define strcasecmp _stricmp
  #define access _access
  #define W_OK 2
  #define R_OK 4
  #define DIR_SEPERATORS "/\\:"
#else
  #include <unistd.h>
  #include <pthread.h>
  #define DIR_SEPERATORS "/"
#endif

#include <sstream>
#include <vector>
#include <queue>
#include <map>


#define   T38_PAYLOAD_CODE          38

#define   BITS_PER_SECOND           14400
#define   MICROSECONDS_PER_FRAME    20000
#define   SAMPLES_PER_FRAME         160
#define   BYTES_PER_FRAME           320
#define   PREF_FRAMES_PER_PACKET    1
#define   MAX_FRAMES_PER_PACKET     1


#define   PCM_TRANSMIT_ON_IDLE      true
#define   USE_ECM                   false
#define   SUPPORTED_MODEMS_PCM      (T30_SUPPORT_V27TER | T30_SUPPORT_V29)
#define   SUPPORTED_MODEMS_NO_PCM   (SUPPORTED_MODEMS_PCM /*| T30_SUPPORT_V17*/)


#if LOGGING

static PluginCodec_LogFunction LogFunction;

#define PTRACE_PARAM(param) param

#define PTRACE(level, args) \
    if (LogFunction != NULL && LogFunction(level, NULL, 0, NULL, NULL)) { \
    std::ostringstream strm; strm << args; \
      LogFunction(level, __FILE__, __LINE__, "FaxCodec", strm.str().c_str()); \
    } else (void)0

static void SpanDSP_Message(int level, const char *text)
{
  if (*text != '\0' && LogFunction != NULL) {
    if (level >= SPAN_LOG_DEBUG)
      level = 5;
    else
    if (level >= SPAN_LOG_FLOW)
      level = 4;
    //else
    //if (level >= SPAN_LOG_PROTOCOL_WARNING)
    //  level = 3;
    else
    if (level >= SPAN_LOG_PROTOCOL_ERROR)
      level = 3;
    //else
    //if (level >= SPAN_LOG_WARNING)
    //  level = 3;
    //else
    //if (level >= SPAN_LOG_ERROR)
    //  level = 2;
    else
      level = 2;

    if (!LogFunction(level, NULL, 0, NULL, NULL))
      return;

    int last = strlen(text)-1;
    if (text[last] == '\n')
      ((char *)text)[last] = '\0';
    LogFunction(level, __FILE__, __LINE__, "SpanDSP", text);
  }
}

static void InitLogging(logging_state_t * logging, const std::string & tag)
{
  span_log_set_message_handler(logging, SpanDSP_Message);

  int level = SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG;

  if (!tag.empty()) {
    level |= SPAN_LOG_SHOW_TAG;
    span_log_set_tag(logging, tag.c_str());
  }

  span_log_set_level(logging, level);
}

class Tag
{
  protected:
    std::string m_tag;
};

#else // LOGGING

#define PTRACE_PARAM(param)
#define PTRACE(level, expr)
#define InitLogging(logging, tag)

#endif // LOGGING

/////////////////////////////////////////////////////////////////////////////

static const char L16Format[] = "L16";
static const char T38Format[] = "T.38";
static const char TIFFFormat[] = "TIFF-File";
static const char T38sdp[]    = "t38";


static struct PluginCodec_Option const ReceivingOption =
{
  PluginCodec_BoolOption,     // PluginCodec_OptionTypes
  "Receiving",                // Generic (human readable) option name
  1,                          // Read Only flag
  PluginCodec_AlwaysMerge,    // Merge mode
  "0",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0                           // H.245 Generic Capability number and scope bits
};

static struct PluginCodec_Option const TiffFileNameOption =
{
  PluginCodec_StringOption,   // PluginCodec_OptionTypes
  "TIFF-File-Name",           // Generic (human readable) option name
  1,                          // Read Only flag
  PluginCodec_AlwaysMerge,    // Merge mode
  "",                         // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0                           // H.245 Generic Capability number and scope bits
};

static struct PluginCodec_Option const T38FaxVersion =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  "T38FaxVersion",            // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "0",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "0",                        // Minimum value
  "1"                         // Maximum value
};

static struct PluginCodec_Option const T38FaxRateManagement =
{
  PluginCodec_EnumOption,     // PluginCodec_OptionTypes
  "T38FaxRateManagement",     // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "transferredTCF",           // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "localTCF:transferredTCF"   // enum values
};

static struct PluginCodec_Option const T38MaxBitRate =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  "T38MaxBitRate",            // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "14400",                    // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "300",                      // Minimum value
  "56000"                     // Maximum value
};

static struct PluginCodec_Option const T38FaxMaxBuffer =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  "T38FaxMaxBuffer",          // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "2000",                     // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "100",                       // Minimum value
  "9999"                      // Maximum value
};

static struct PluginCodec_Option const T38FaxMaxDatagram =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  "T38FaxMaxDatagram",        // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "528",                      // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "10",                       // Minimum value
  "1500"                      // Maximum value
};

static struct PluginCodec_Option const T38FaxUdpEC =
{
  PluginCodec_EnumOption,     // PluginCodec_OptionTypes
  "T38FaxUdpEC",              // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "t38UDPRedundancy",         // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "t38UDPFEC:t38UDPRedundancy"// enum values
};

static struct PluginCodec_Option const T38FaxFillBitRemoval =
{
  PluginCodec_BoolOption,     // PluginCodec_OptionTypes
  "T38FaxFillBitRemoval",     // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "0",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0                           // H.245 Generic Capability number and scope bits
};

static struct PluginCodec_Option const T38FaxTranscodingMMR =
{
  PluginCodec_BoolOption,     // PluginCodec_OptionTypes
  "T38FaxTranscodingMMR",     // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "0",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0                           // H.245 Generic Capability number and scope bits
};

static struct PluginCodec_Option const T38FaxTranscodingJBIG =
{
  PluginCodec_BoolOption,     // PluginCodec_OptionTypes
  "T38FaxTranscodingJBIG",    // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "0",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0                           // H.245 Generic Capability number and scope bits
};

static struct PluginCodec_Option const * const OptionTableTIFF[] = {
  &ReceivingOption,
  &TiffFileNameOption,
  NULL
};

static struct PluginCodec_Option const * const OptionTableT38[] = {
  &T38FaxVersion,
  &T38FaxRateManagement,
  &T38MaxBitRate,
  &T38FaxMaxBuffer,
  &T38FaxMaxDatagram,
  &T38FaxUdpEC,
  &T38FaxFillBitRemoval,
  &T38FaxTranscodingMMR,
  &T38FaxTranscodingJBIG,
  NULL
};

static struct PluginCodec_Option const * const OptionTablePCM[] = {
  NULL
};


/////////////////////////////////////////////////////////////////
//
// define a class to implement a critical section mutex
// based on PCriticalSection from PWLib

#ifdef _WIN32
class CriticalSection
{
private:
  CRITICAL_SECTION m_CriticalSection;
public:
  inline CriticalSection()  { InitializeCriticalSection(&m_CriticalSection); }
  inline ~CriticalSection() { DeleteCriticalSection(&m_CriticalSection); }
  inline void Wait()        { EnterCriticalSection(&m_CriticalSection); }
  inline void Signal()      { LeaveCriticalSection(&m_CriticalSection); }
};
#else
class CriticalSection
{
private:
  pthread_mutex_t m_Mutex;
public:
  inline CriticalSection()  { pthread_mutex_init(&m_Mutex, NULL); }
  inline ~CriticalSection() { pthread_mutex_destroy(&m_Mutex); }
  inline void Wait()        { pthread_mutex_lock(&m_Mutex); }
  inline void Signal()      { pthread_mutex_unlock(&m_Mutex); }
};
#endif
    
class WaitAndSignal
{
  private:
    CriticalSection & m_criticalSection;

    void operator=(const WaitAndSignal &) { }
    WaitAndSignal(const WaitAndSignal & other) : m_criticalSection(other.m_criticalSection) { }
  public:
    inline WaitAndSignal(const CriticalSection & cs) 
      : m_criticalSection((CriticalSection &)cs) { m_criticalSection.Wait(); }
    inline ~WaitAndSignal()                      { m_criticalSection.Signal(); }
};


/////////////////////////////////////////////////////////////////

class FaxSpanDSP
#if LOGGING
  : virtual public Tag
#endif
{
  private:
    unsigned        m_referenceCount;

  protected:
    bool            m_completed;
    CriticalSection m_mutex;

    bool            m_useECM;
    int             m_supported_modems;

  public:
    FaxSpanDSP()
      : m_referenceCount(1)
      , m_completed(false)
      , m_useECM(USE_ECM)
      , m_supported_modems(SUPPORTED_MODEMS_PCM)
    {
    }


    virtual ~FaxSpanDSP()
    {
    }


    void AddReference()
    {
      WaitAndSignal mutex(m_mutex);
      ++m_referenceCount;
    }


    bool Dereference()
    {
      WaitAndSignal mutex(m_mutex);
      return --m_referenceCount == 0;
    }


    bool SetOptions(const char * const * options)
    {
      if (options == NULL)
        return false;

      while (options[0] != NULL && options[1] != NULL) {
        if (!SetOption(options[0], options[1]))
          return false;
        options += 2;
      }

      return true;
    }

    virtual bool Encode(const void * fromPtr, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags) = 0;
    virtual bool Decode(const void * fromPtr, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags) = 0;
    virtual bool Terminate() = 0;
    virtual bool GetStats(void * fromPtr, unsigned fromLen) = 0;


  protected:
    virtual bool SetOption(const char * PTRACE_PARAM(option), const char * PTRACE_PARAM(value))
    {
      PTRACE(3, m_tag << " SetOption: " << option << "=" << value);

      return true;
    }

    bool HasError(bool retval)
    {
      if (m_completed)
        return true;

      if (retval)
        return false;

      /// Error exit
      m_completed = true;
      return true;
    }
};


/////////////////////////////////////////////////////////////////

class FaxT38
#if LOGGING
  : virtual public Tag
#endif
{
  private:
    unsigned        m_protoVersion;

    int             m_sequence;

    t38_core_state_t * m_t38core;

    std::queue< std::vector<uint8_t> > m_t38Queue;


  protected:
    FaxT38()
      : m_protoVersion(0)
      , m_sequence(0)
      , m_t38core(NULL)
    {
    }


    bool SetOption(const char * option, const char * value)
    {
      if (strcasecmp(option, T38FaxVersion.m_name) == 0) {
        m_protoVersion = atoi(value);
        return true;
      }

      return true;
    }


    bool Open(t38_core_state_t * t38core)
    {
      m_t38core = t38core;
      InitLogging(t38_core_get_logging_state(m_t38core), m_tag);
      t38_set_t38_version(m_t38core, m_protoVersion);

      return true;
    }


    bool EncodeRTP(void * toPtr, unsigned & toLen, unsigned & flags)
    {
      if (m_t38Queue.empty()) {
        toLen = 0;
        flags = PluginCodec_ReturnCoderLastFrame;
        return true;
      }

      std::vector<uint8_t> & packet = m_t38Queue.front();
      size_t size = packet.size() + PluginCodec_RTP_MinHeaderSize;

      if (toLen < size)
        return false;

      toLen = size;

      memcpy(PluginCodec_RTP_GetPayloadPtr(toPtr), &packet[0], packet.size());

      uint16_t seq = uint16_t(m_sequence++);

      PluginCodec_RTP_SetSequenceNumber(toPtr, seq);

      m_t38Queue.pop();

      if (m_t38Queue.empty())
        flags = PluginCodec_ReturnCoderLastFrame;

      return true;
    }


    bool DecodeRTP(const void * fromPtr, unsigned & fromLen)
    {
      int payloadSize = fromLen - PluginCodec_RTP_GetHeaderLength(fromPtr);

      if (payloadSize < 0)
        return false;

      if (payloadSize > 0) {
        if (t38_core_rx_ifp_packet(m_t38core,
                                   PluginCodec_RTP_GetPayloadPtr(fromPtr),
                                   payloadSize,
                                   PluginCodec_RTP_GetSequenceNumber(fromPtr)) == -1)
        {
          return false;
        }
      }

      return true;
    }


    static int QueueT38(t38_core_state_t *, void * user_data, const uint8_t * buf, int len, int count)
    {
      if (user_data != NULL)
        ((FaxT38 *)user_data)->QueueT38(buf, len, count);
      return 0;
    }

  private:
    void QueueT38(const uint8_t * buf, int len, int /*count*/)
    {
      PTRACE(LOG_LEVEL_DEBUG, m_tag << " FaxT38::QueueT38 len=" << len);

      m_t38Queue.push(std::vector<uint8_t>());
      std::vector<uint8_t> & packet = m_t38Queue.back();

      packet.resize(len);
      memcpy(&packet[0], buf, len);
    }
};


/////////////////////////////////////////////////////////////////

class FaxPCM
#if LOGGING
  : virtual public Tag
#endif
{
  private:
    bool            m_transmit_on_idle;

  protected:
    FaxPCM()
      : m_transmit_on_idle(PCM_TRANSMIT_ON_IDLE)
    {
    }

    bool TransmitOnIdle() const { return m_transmit_on_idle; }

    bool SetOption(const char * /*option*/, const char * /*value*/)
    {
      return true;
    }
};


/////////////////////////////////////////////////////////////////

struct MyStats : public t30_stats_t
{
  MyStats(bool completed, bool receiving)
    : m_completed(completed)
    , m_receiving(receiving)
  {
  }

  bool m_completed;
  bool m_receiving;
};

static std::ostream & operator<<(std::ostream & strm, const MyStats & stats)
{
  static const char * const CompressionNames[4] = { "N/A", "T.4 1d", "T.4 2d", "T.6" };

  strm << "Status=";
  if (stats.m_completed)
    strm << stats.current_status << " (" << t30_completion_code_to_str(stats.current_status) << ')';
  else
    strm << "-1 (In progress)";
  strm << "\n"
          "Bit Rate=" << stats.bit_rate << "\n"
          "Encoding=" << stats.encoding << ' ' << CompressionNames[stats.encoding&3] << "\n"
          "Error Correction=" << stats.error_correcting_mode << "\n"
          "Tx Pages=" << (stats.m_receiving ? -1 : stats.pages_tx) << "\n"
          "Rx Pages=" << (stats.m_receiving ? stats.pages_rx : -1) << "\n"
          "Total Pages=" << stats.pages_in_file << "\n"
          "Image Bytes=" << stats.image_size << "\n"
          "Resolution=" << stats.x_resolution << 'x' << stats.y_resolution << "\n"
          "Page Size=" << stats.width << 'x' << stats.length << "\n"
          "Bad Rows=" << stats.bad_rows << "\n"
          "Most Bad Rows=" << stats.longest_bad_row_run << "\n"
          "Correction Retries=" << stats.error_correcting_mode_retries;

  return strm;
}


class FaxTIFF : public FaxSpanDSP
{
  private:
    bool            m_receiving;
    std::string     m_tiffFileName;
    std::string     m_stationIdentifer;

  protected:
    FaxTIFF()
      : m_receiving(false)
      , m_stationIdentifer("-")
    {
    }


    virtual bool SetOption(const char * option, const char * value)
    {
      if (!FaxSpanDSP::SetOption(option, value))
        return false;

      if (strcasecmp(option, TiffFileNameOption.m_name) == 0) {
        if (m_tiffFileName.empty())
          m_tiffFileName = value;
#if LOGGING
        else if (*value != '\0' && m_tiffFileName != value)
          PTRACE(2, m_tag << " Cannot change filename in mid stream from \"" << m_tiffFileName << "\" to \"" << value << '"');
#endif
        return true;
      }

      if (strcasecmp(option, ReceivingOption.m_name) == 0) {
        m_receiving = atoi(value) != 0;
        return true;
      }

      return true;
    }


    bool Open(t30_state_t * t30state)
    {
      InitLogging(t30_get_logging_state(t30state), m_tag);

      if (m_tiffFileName.empty()) {
        PTRACE(1, m_tag << " No TIFF file to " << m_receiving ? "receive" : "transmit");
        return false;
      }

      if (m_receiving) {
        std::string dir;
        std::string::size_type pos = m_tiffFileName.find_last_of(DIR_SEPERATORS);
        if (pos == std::string::npos)
          dir = ".";
        else
          dir.assign(m_tiffFileName, 0, pos+1);

        if (access(dir.c_str(), W_OK) != 0) {
          PTRACE(1, m_tag << " Cannot set receive TIFF file to \"" << m_tiffFileName << '"');
          return false;
        }

        t30_set_rx_file(t30state, m_tiffFileName.c_str(), -1);
        PTRACE(3, m_tag << " Set receive TIFF file to \"" << m_tiffFileName << '"');
      }
      else {
        if (access(m_tiffFileName.c_str(), R_OK) != 0) {
          PTRACE(1, m_tag << " Cannot set transmit TIFF file to \"" << m_tiffFileName << '"');
          return false;
        }

        t30_set_tx_file(t30state, m_tiffFileName.c_str(), -1, -1);
        PTRACE(3, m_tag << " Set transmit TIFF file to \"" << m_tiffFileName << '"');
      }

      t30_set_phase_b_handler(t30state, PhaseB, this);
      t30_set_phase_d_handler(t30state, PhaseD, this);
      t30_set_phase_e_handler(t30state, PhaseE, this);
      t30_set_tx_ident(t30state, m_stationIdentifer.c_str());
      t30_set_ecm_capability(t30state, m_useECM);
      t30_set_supported_modems(t30state, m_supported_modems);

      return true;
    }


    bool GetStats(t30_state_t * t30state, void * fromPtr, unsigned fromLen)
    {
      if (t30state == NULL)
        return false;

      MyStats stats(m_completed, m_receiving);
      t30_get_transfer_statistics(t30state, &stats);
      std::stringstream strm;
      strm << stats;

      std::string str = strm.str();
      size_t len = str.length() + 1;
      if (len > fromLen) {
        len = fromLen;
        str[len-1] = '\0';
      }

      memcpy(fromPtr, str.c_str(), len);

      PTRACE(LOG_LEVEL_DEBUG, m_tag << " SpanDSP statistics:\n" << (char *)fromPtr);

      return true;
    }


    bool IsReceiving() const { return m_receiving; }


    static int PhaseB(t30_state_t * t30state, void * user_data, int result)
    {
      if (user_data != NULL)
        ((FaxTIFF *)user_data)->PhaseB(t30state, result);
      return T30_ERR_OK;
    }

    static int PhaseD(t30_state_t * t30state, void * user_data, int result)
    {
      if (user_data != NULL)
        ((FaxTIFF *)user_data)->PhaseD(t30state, result);
      return T30_ERR_OK;
    }

    static void PhaseE(t30_state_t * t30state, void * user_data, int result)
    {
      if (user_data != NULL)
        ((FaxTIFF *)user_data)->PhaseE(t30state, result);
    }


  private:
    void PhaseB(t30_state_t * t30state, int)
    {
      MyStats stats(m_completed, m_receiving);
      t30_get_transfer_statistics(t30state, &stats);
      PTRACE(3, m_tag << " SpanDSP entered Phase B:\n" << stats);
    }

    void PhaseD(t30_state_t * t30state, int)
    {
      MyStats stats(m_completed, m_receiving);
      t30_get_transfer_statistics(t30state, &stats);
      PTRACE(3, m_tag << " SpanDSP entered Phase D:\n" << stats);
    }

    void PhaseE(t30_state_t * t30state, int result)
    {
      if (result >= 0)
        m_completed = true; // Finished, exit codec loops

      MyStats stats(m_completed, m_receiving);
      t30_get_transfer_statistics(t30state, &stats);
      PTRACE(3, m_tag << " SpanDSP entered Phase E:\n" << stats);
    }
};


/////////////////////////////////////////////////////////////////

class T38_PCM : public FaxSpanDSP, public FaxT38, public FaxPCM
{
  protected:
    t38_gateway_state_t * m_t38State;

  public:
    T38_PCM(PTRACE_PARAM(const std::string &tag))
      : m_t38State(NULL)
    {
#if LOGGING
      m_tag = tag;
#endif

      PTRACE(4, m_tag << " Created T38_PCM");
    }


    ~T38_PCM()
    {
      if (m_t38State != NULL) {
        t38_gateway_release(m_t38State);
        t38_gateway_free(m_t38State);
        PTRACE(3, m_tag << " Closed T38_PCM/SpanDSP");
      }

      PTRACE(4, m_tag << " Deleted T38_PCM instance.");
    }


    virtual bool Encode(const void * fromPtr, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags)
    {
      // encode PCM-raw to T.38-RTP

      WaitAndSignal mutex(m_mutex);

      if (!Open())
        return false;

      int samplesLeft = t38_gateway_rx(m_t38State, (int16_t *)fromPtr, fromLen/2);

      if (samplesLeft < 0)
        return false;

      fromLen -= samplesLeft*2;

      if (!FaxT38::EncodeRTP(toPtr, toLen, flags))
        return false;


      PTRACE(LOG_LEVEL_DEBUG, m_tag <<
                              " T38_PCM::Encode: fromLen=" << fromLen << " toLen=" << toLen <<
                              " seq=" << (toLen > 0 ? PluginCodec_RTP_GetSequenceNumber(toPtr) : 0));

      return true;
    }


    virtual bool Decode(const void * fromPtr, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags)
    {
      // decode T.38-RTP to PCM-raw

      WaitAndSignal mutex(m_mutex);

      if (!Open())
        return false;

      if (!FaxT38::DecodeRTP(fromPtr, fromLen))
        return false;

      int samplesGenerated = t38_gateway_tx(m_t38State, (int16_t *)toPtr, toLen/2);

      if (samplesGenerated < 0)
        return false;

      toLen = samplesGenerated*2;

      flags = PluginCodec_ReturnCoderLastFrame;

      PTRACE(LOG_LEVEL_DEBUG, m_tag <<
                              " T38_PCM::Decode: fromLen=" << fromLen << " toLen=" << toLen <<
                              " seq=" << PluginCodec_RTP_GetSequenceNumber(fromPtr) <<
                              " ts=" << PluginCodec_RTP_GetTimestamp(fromPtr) <<
                              (toLen >= sizeof(long) && *(long *)toPtr ? " **********" : ""));

      return true;
    }


    virtual bool Terminate()
    {
      WaitAndSignal mutex(m_mutex);

      PTRACE(4, m_tag << " T38_PCM::Terminate");
      return Open();
    }


    virtual bool GetStats(void * /*fromPtr*/, unsigned /*fromLen*/)
    {
      // WaitAndSignal mutex(m_mutex);

      return false;
    }


  protected:
    virtual bool SetOption(const char * option, const char * value)
    {
      if (!FaxSpanDSP::SetOption(option, value))
        return false;

      if (!FaxT38::SetOption(option, value))
        return false;

      if (!FaxPCM::SetOption(option, value))
        return false;

      return true;
    }


    bool Open()
    {
      if (m_completed)
        return false;

      if (m_t38State != NULL)
        return true;

      PTRACE(3, m_tag << " Opening T38_PCM/SpanDSP");
  
      m_t38State = t38_gateway_init(NULL, FaxT38::QueueT38, (FaxT38 *)this);
      if (HasError(m_t38State != NULL))
        return false;

      if (HasError(FaxT38::Open(t38_gateway_get_t38_core_state(m_t38State))))
        return false;

      InitLogging(t38_gateway_get_logging_state(m_t38State), m_tag);

      t38_gateway_set_transmit_on_idle(m_t38State, TransmitOnIdle());
      t38_gateway_set_ecm_capability(m_t38State, m_useECM);
      t38_gateway_set_supported_modems(m_t38State, m_supported_modems);
      //t38_gateway_set_nsx_suppression(m_t38State, NULL, 0, NULL, 0);

      return true;
    }
};


/////////////////////////////////////////////////////////////////

class TIFF_T38 : public FaxTIFF, public FaxT38
{
  protected:
    t38_terminal_state_t * m_t38State;

  public:
    TIFF_T38(PTRACE_PARAM(const std::string &tag))
      : m_t38State(NULL)
    {
#if LOGGING
      m_tag = tag;
#endif

      m_supported_modems = SUPPORTED_MODEMS_NO_PCM;

      PTRACE(4, m_tag << " Created TIFF_T38");
    }


    ~TIFF_T38()
    {
      if (m_t38State != NULL) {
        t30_terminate(t38_terminal_get_t30_state(m_t38State)); //call PhaseE
        t38_terminal_release(m_t38State);
        t38_terminal_free(m_t38State);
        PTRACE(3, m_tag << " Closed TIFF_T38/SpanDSP");
      }

      PTRACE(4, m_tag << " Deleted TIFF_T38 instance.");
    }


    virtual bool Encode(const void * /*fromPtr*/, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags)
    {
      // encode TIFF-raw to T.38-RTP

      WaitAndSignal mutex(m_mutex);

      if (!Open())
        return false;

      t38_terminal_send_timeout(m_t38State, fromLen/2);

      if (!FaxT38::EncodeRTP(toPtr, toLen, flags))
        return false;

      PTRACE(LOG_LEVEL_DEBUG, m_tag <<
                              " TIFF_T38::Encode: fromLen=" << fromLen << " toLen=" << toLen <<
                              " seq=" << (toLen > 0 ? PluginCodec_RTP_GetSequenceNumber(toPtr) : 0));

      return true;
    }


    virtual bool Decode(const void * fromPtr, unsigned & fromLen, void * /*toPtr*/, unsigned & toLen, unsigned & flags)
    {
      // decode T.38-RTP to PCM-raw

      WaitAndSignal mutex(m_mutex);

      if (!Open())
        return false;

      if (!FaxT38::DecodeRTP(fromPtr, fromLen))
        return false;

      toLen = 0;
      flags = PluginCodec_ReturnCoderLastFrame;

      PTRACE(LOG_LEVEL_DEBUG, m_tag <<
                              " TIFF_T38::Decode: fromLen=" << fromLen <<
                              " seq=" << PluginCodec_RTP_GetSequenceNumber(fromPtr) <<
                              " ts=" << PluginCodec_RTP_GetTimestamp(fromPtr));

      return true;
    }


    virtual bool Terminate()
    {
      WaitAndSignal mutex(m_mutex);

      PTRACE(4, m_tag << " TIFF_T38::Terminate");

      if (!Open())
        return false;

      t30_terminate(t38_terminal_get_t30_state(m_t38State));
      return true;
    }


    virtual bool GetStats(void * fromPtr, unsigned fromLen)
    {
      WaitAndSignal mutex(m_mutex);

      return FaxTIFF::GetStats(m_t38State != NULL ? t38_terminal_get_t30_state(m_t38State) : NULL, fromPtr, fromLen);
    }


  protected:
    virtual bool SetOption(const char * option, const char * value)
    {
      if (!FaxTIFF::SetOption(option, value))
        return false;

      if (!FaxT38::SetOption(option, value))
        return false;

      return true;
    }


    bool Open()
    {
      if (m_completed)
        return false;

      if (m_t38State != NULL)
        return true;

      PTRACE(3, m_tag << " Opening TIFF_T38/SpanDSP for " << (IsReceiving() ? "receive" : "transmit"));

      m_t38State = t38_terminal_init(NULL, !IsReceiving(), FaxT38::QueueT38, (FaxT38 *)this);
      if (HasError(m_t38State != NULL))
        return false;

      if (HasError(FaxTIFF::Open(t38_terminal_get_t30_state(m_t38State))))
        return false;

      if (HasError(FaxT38::Open(t38_terminal_get_t38_core_state(m_t38State))))
        return false;

      InitLogging(t38_terminal_get_logging_state(m_t38State), m_tag);
      t38_terminal_set_config(m_t38State, false);

      return true;
    }
};


/////////////////////////////////////////////////////////////////

class TIFF_PCM : public FaxTIFF, public FaxPCM
{
  protected:
    fax_state_t          * m_faxState;

  public:
    TIFF_PCM(PTRACE_PARAM(const std::string &tag))
      : m_faxState(NULL)
    {
#if LOGGING
      m_tag = tag;
#endif

      PTRACE(4, m_tag << " Created TIFF_PCM");
    }


    ~TIFF_PCM()
    {
      if (m_faxState != NULL) {
        t30_terminate(fax_get_t30_state(m_faxState)); //to call PhaseE with audio Fax
        fax_release(m_faxState);
        fax_free(m_faxState);
        PTRACE(3, m_tag << " Closed TIFF_PCM/SpanDSP");
      }

      PTRACE(4, m_tag << " Deleted TIFF_PCM instance.");
    }


    virtual bool Encode(const void * fromPtr, unsigned & fromLen, void * /*toPtr*/, unsigned & toLen, unsigned & flags)
    {
      // encode PCM-raw to TIFF-raw

      WaitAndSignal mutex(m_mutex);

      if (!Open())
        return false;

      int samplesLeft = fax_rx(m_faxState, (int16_t *)fromPtr, fromLen/2);

      if (samplesLeft < 0)
        return false;

      fromLen -= samplesLeft*2;

      toLen = 0;
      flags = PluginCodec_ReturnCoderLastFrame;

      PTRACE(LOG_LEVEL_DEBUG, m_tag << " TIFF_PCM::Encode: fromLen=" << fromLen);

      return true;
    }


    virtual bool Decode(const void * /*fromPtr*/, unsigned & PTRACE_PARAM(fromLen), void * toPtr, unsigned & toLen, unsigned & flags)
    {
      // decode TIFF-raw to PCM-raw

      WaitAndSignal mutex(m_mutex);

      if (!Open())
        return false;

      int samplesGenerated = fax_tx(m_faxState, (int16_t *)toPtr, toLen/2);

      if (samplesGenerated < 0)
        return false;

      toLen = samplesGenerated*2;
      flags = PluginCodec_ReturnCoderLastFrame;

      PTRACE(LOG_LEVEL_DEBUG, m_tag <<
                              " TIFF_PCM::Decode: fromLen=" << fromLen << " toLen=" << toLen <<
                              (toLen >= sizeof(long) && *(long *)toPtr ? " **********" : ""));

      return true;
    }


    virtual bool Terminate()
    {
      WaitAndSignal mutex(m_mutex);

      PTRACE(4, m_tag << " TIFF_PCM::Terminate");

      if (!Open())
        return false;

      t30_terminate(fax_get_t30_state(m_faxState));
      return true;
    }


    virtual bool GetStats(void * fromPtr, unsigned fromLen)
    {
      WaitAndSignal mutex(m_mutex);

      return FaxTIFF::GetStats(m_faxState != NULL ? fax_get_t30_state(m_faxState) : NULL, fromPtr, fromLen);
    }

  protected:
    virtual bool SetOption(const char * option, const char * value)
    {
      if (!FaxTIFF::SetOption(option, value))
        return false;

      if (!FaxPCM::SetOption(option, value))
        return false;

      return true;
    }


    bool Open()
    {
      if (m_completed)
        return false;

      if (m_faxState != NULL)
        return true;

      PTRACE(3, m_tag << " Opening TIFF_PCM/SpanDSP for " << (IsReceiving() ? "receive" : "transmit"));

      m_faxState = fax_init(NULL, !IsReceiving());
      if (HasError(m_faxState != NULL))
        return false;

      if (HasError(FaxTIFF::Open(fax_get_t30_state(m_faxState))))
        return false;

      InitLogging(fax_get_logging_state(m_faxState), m_tag);
      fax_set_transmit_on_idle(m_faxState, TransmitOnIdle());

      return true;
    }
};


/////////////////////////////////////////////////////////////////
typedef std::vector<unsigned char> InstanceKey;

#if LOGGING
static std::string KeyToStr(const InstanceKey &key)
{
  std::ostringstream strm;

  for (size_t i = 0 ; i < key.size() ; i++) {
    unsigned char ch = key[i];

    if (ch >= 0x20 && ch <= 0x7E)
      strm << ch;
    else
      strm << "<0x" << std::hex << (unsigned)ch << std::dec << ">";
  }

  return strm.str();
}
#endif

typedef std::map<InstanceKey, FaxSpanDSP *> InstanceMapType;

static InstanceMapType InstanceMap;
static CriticalSection InstanceMapMutex;

class FaxCodecContext
{
  private:
    const PluginCodec_Definition * m_definition;
    InstanceKey                    m_key;
    FaxSpanDSP                   * m_instance;

  public:
    FaxCodecContext(const PluginCodec_Definition * defn)
      : m_definition(defn)
      , m_instance(NULL)
    {
    }


    ~FaxCodecContext()
    {
      if (m_instance == NULL)
        return;

      WaitAndSignal mutex(InstanceMapMutex);

      InstanceMapType::iterator iter = InstanceMap.find(m_key);
      if (iter != InstanceMap.end() && iter->second->Dereference()) {
        delete iter->second;
        InstanceMap.erase(iter);
        PTRACE(LOG_LEVEL_CONTEXT_ID, KeyToStr(m_key) << " Context Id removed");
      }
    }


    bool SetContextId(void * parm, unsigned * parmLen)
    {
      if (parm == NULL || parmLen == NULL || *parmLen == 0)
        return false;

      if (m_instance != NULL)
        return false;

      m_key.resize(*parmLen);
      memcpy(&m_key[0], parm, *parmLen);

#if LOGGING
      std::string key = KeyToStr(m_key);
#endif

      WaitAndSignal mutex(InstanceMapMutex);

      InstanceMapType::iterator iter = InstanceMap.find(m_key);
      if (iter != InstanceMap.end()) {
        PTRACE(LOG_LEVEL_CONTEXT_ID, key << " Context Id found");

        m_instance = iter->second;
        m_instance->AddReference();
      }
      else {
        if (m_definition->sourceFormat == TIFFFormat) {
          if (m_definition->destFormat == T38Format)
            m_instance = new TIFF_T38(PTRACE_PARAM(key));
          else
            m_instance = new TIFF_PCM(PTRACE_PARAM(key));
        }
        else if (m_definition->sourceFormat == T38Format) {
          if (m_definition->destFormat == TIFFFormat)
            m_instance = new TIFF_T38(PTRACE_PARAM(key));
          else
            m_instance = new T38_PCM(PTRACE_PARAM(key));
        }
        else {
          if (m_definition->destFormat == TIFFFormat)
            m_instance = new TIFF_PCM(PTRACE_PARAM(key));
          else
            m_instance = new T38_PCM(PTRACE_PARAM(key));
        }
        InstanceMap[m_key] = m_instance;

        PTRACE(LOG_LEVEL_CONTEXT_ID, key << " Context Id added");
      }

      return true;
    }


    bool SetOptions(const char * const * options)
    {
      return m_instance != NULL && m_instance->SetOptions(options);
    }


    bool Encode(const void * fromPtr, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags)
    {
      return m_instance != NULL && m_instance->Encode(fromPtr, fromLen, toPtr, toLen, flags);
    }


    bool Decode(const void * fromPtr, unsigned & fromLen, void * toPtr, unsigned & toLen, unsigned & flags)
    {
      return m_instance != NULL && m_instance->Decode(fromPtr, fromLen, toPtr, toLen, flags);
    }


    bool Terminate()
    {
      return m_instance != NULL && m_instance->Terminate();
    }


    bool GetStats(void *fromPtr, unsigned fromLen)
    {
      return m_instance != NULL && m_instance->GetStats(fromPtr, fromLen);
    }
};


/////////////////////////////////////////////////////////////////////////////

static int terminate_codec(const PluginCodec_Definition * ,
                                                   void * context,
                                             const char * ,
                                                   void *,
                                               unsigned *)
{
  return context != NULL && ((FaxCodecContext *)context)->Terminate();
}


static int get_codec_stats(const PluginCodec_Definition * ,
                                                     void * context,
                                               const char * ,
                                                     void * parm,
                                                 unsigned * parmLen)
{
  return context != NULL &&
         parm != NULL &&
         parmLen != NULL &&
         ((FaxCodecContext *)context)->GetStats(parm, *parmLen);
}


static int get_codec_options(const PluginCodec_Definition * ,
                                                     void * context,
                                               const char * ,
                                                     void * parm,
                                                 unsigned * parmLen)
{
  if (parm == NULL || parmLen == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
    return false;

  if (context != NULL) {
    if (strcasecmp((char *)context, T38Format) == 0) {
      *(struct PluginCodec_Option const * const * *)parm = OptionTableT38;
      return true;
    }

    if (strcasecmp((char *)context, TIFFFormat) == 0) {
      *(struct PluginCodec_Option const * const * *)parm = OptionTableTIFF;
      return true;
    }
  }

  *(struct PluginCodec_Option const * const * *)parm = OptionTablePCM;

  return true;
}


static int set_codec_options(const PluginCodec_Definition * ,
                                                     void * context,
                                               const char * , 
                                                     void * parm, 
                                                 unsigned * parmLen)
{
  return context != NULL &&
         parm != NULL &&
         parmLen != NULL &&
         *parmLen == sizeof(const char **) &&
         ((FaxCodecContext *)context)->SetOptions((const char * const *)parm);
}


static int set_instance_id(const PluginCodec_Definition * ,
                                                   void * context,
                                             const char * ,
                                                   void * parm,
                                               unsigned * parmLen)
{
  return context != NULL && ((FaxCodecContext *)context)->SetContextId(parm, parmLen);
}


#if LOGGING
static int set_log_function(const PluginCodec_Definition * ,
                                                   void * ,
                                             const char * ,
                                                   void * parm,
                                               unsigned * parmLen)
{
  if (parmLen == NULL || *parmLen != sizeof(PluginCodec_LogFunction))
    return false;

  LogFunction = (PluginCodec_LogFunction)parm;
  return true;
}
#endif


static struct PluginCodec_ControlDefn Controls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS, get_codec_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS, set_codec_options },
  { PLUGINCODEC_CONTROL_SET_INSTANCE_ID,   set_instance_id },
  { PLUGINCODEC_CONTROL_GET_STATISTICS,    get_codec_stats },
  { PLUGINCODEC_CONTROL_TERMINATE_CODEC,   terminate_codec },
#if LOGGING
  { PLUGINCODEC_CONTROL_SET_LOG_FUNCTION,  set_log_function },
#endif
  { NULL }
};


/////////////////////////////////////////////////////////////////////////////

static void * Create(const PluginCodec_Definition * codec)
{
  return new FaxCodecContext(codec);
}


static void Destroy(const PluginCodec_Definition * /*codec*/, void * context)
{
  delete (FaxCodecContext *)context;
}


static int Encode(const PluginCodec_Definition * /*codec*/,
                                          void * context,
                                    const void * fromPtr,
                                      unsigned * fromLen,
                                          void * toPtr,
                                      unsigned * toLen,
                                      unsigned * flags)
{
  return context != NULL && ((FaxCodecContext *)context)->Encode(fromPtr, *fromLen, toPtr, *toLen, *flags);
}



static int Decode(const PluginCodec_Definition * /*codec*/,
                                          void * context,
                                    const void * fromPtr,
                                      unsigned * fromLen,
                                          void * toPtr,
                                      unsigned * toLen,
                                      unsigned * flags)
{
  return context != NULL && ((FaxCodecContext *)context)->Decode(fromPtr, *fromLen, toPtr, *toLen, *flags);
}


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information LicenseInfo = {
  1081086550,                              // timestamp = Sun 04 Apr 2004 01:49:10 PM UTC = 

  "Craig Southeren, Post Increment",                           // source code author
  "1.0",                                                       // source code version
  "craigs@postincrement.com",                                  // source code email
  "http://www.postincrement.com",                              // source code URL
  "Copyright (C) 2007 by Post Increment, All Rights Reserved", // source code copyright
  "MPL 1.0",                                                   // source code license
  PluginCodec_License_MPL,                                     // source code license
  
  "T.38 Fax Codec",                                            // codec description
  "Craig Southeren",                                           // codec author
  "Version 1",                                                 // codec version
  "craigs@postincrement.com",                                  // codec email
  "",                                                          // codec URL
  "",                                                          // codec copyright information
  NULL,                                                        // codec license
  PluginCodec_License_MPL                                      // codec license code
};

#define MY_API_VERSION PLUGIN_CODEC_VERSION_OPTIONS

static PluginCodec_Definition faxCodecDefn[] = {

  { 
    // encoder
    MY_API_VERSION,                     // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeFax |          // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRTP |         // RTP output data
    PluginCodec_RTPTypeExplicit,        // explicit RTP type

    "PCM to T.38 Codec",                // text decription
    L16Format,                          // source format
    T38Format,                          // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    MICROSECONDS_PER_FRAME,             // microseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    T38_PAYLOAD_CODE,                   // internal RTP payload code
    T38sdp,                             // RTP payload name

    Create,                             // create codec function
    Destroy,                            // destroy codec
    Encode,                             // encode/decode
    Controls,                           // codec controls

    PluginCodec_H323T38Codec,           // h323CapabilityType 
    NULL                                // h323CapabilityData
  },

  { 
    // decoder
    MY_API_VERSION,                     // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_EmptyPayload |
    PluginCodec_MediaTypeFax |          // audio codec
    PluginCodec_InputTypeRTP |          // RTP input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeExplicit,        // explicit RTP type

    "T.38 to PCM Codec",                // text decription
    T38Format,                          // source format
    L16Format,                          // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    MICROSECONDS_PER_FRAME,             // microseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    T38_PAYLOAD_CODE,                   // internal RTP payload code
    T38sdp,                             // RTP payload name

    Create,                             // create codec function
    Destroy,                            // destroy codec
    Decode,                             // encode/decode
    Controls,                           // codec controls

    PluginCodec_H323T38Codec,           // h323CapabilityType 
    NULL                                // h323CapabilityData
  },

  { 
    // encoder
    MY_API_VERSION,                     // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeFax |          // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRTP |         // RTP output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    "TIFF to T.38 Codec",               // text decription
    TIFFFormat,                         // source format
    T38Format,                          // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    MICROSECONDS_PER_FRAME,             // microseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    0,                                  // dynamic payload
    NULL,                               // RTP payload name

    Create,                             // create codec function
    Destroy,                            // destroy codec
    Encode,                             // encode/decode
    Controls,                           // codec controls

    PluginCodec_H323T38Codec,           // h323CapabilityType 
    NULL                                // h323CapabilityData
  },

  { 
    // decoder
    MY_API_VERSION,                     // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeFax |          // audio codec
    PluginCodec_InputTypeRTP |          // RTP input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    "T.38 to TIFF Codec",               // text decription
    T38Format,                          // source format
    TIFFFormat,                         // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    MICROSECONDS_PER_FRAME,             // microseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    0,                                  // dynamic payload
    NULL,                               // RTP payload name

    Create,                             // create codec function
    Destroy,                            // destroy codec
    Decode,                             // encode/decode
    Controls,                           // codec controls

    PluginCodec_H323T38Codec,           // h323CapabilityType 
    NULL                                // h323CapabilityData
  },

  { 
    // encoder
    MY_API_VERSION,                     // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeFax |          // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    "PCM to TIFF Codec",                // text decription
    L16Format,                          // source format
    TIFFFormat,                         // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    MICROSECONDS_PER_FRAME,             // microseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    0,                                  // dynamic payload
    NULL,                               // RTP payload name

    Create,                             // create codec function
    Destroy,                            // destroy codec
    Encode,                             // encode/decode
    Controls,                           // codec controls

    0,                                  // h323CapabilityType 
    NULL                                // h323CapabilityData
  },

  { 
    // decoder
    MY_API_VERSION,                     // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeFax |          // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    "TIFF to PCM Codec",                // text decription
    TIFFFormat,                         // source format
    L16Format,                          // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    MICROSECONDS_PER_FRAME,             // microseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    0,                                  // dynamic payload
    NULL,                               // RTP payload name

    Create,                             // create codec function
    Destroy,                            // destroy codec
    Decode,                             // encode/decode
    Controls,                           // codec controls

    0,                                  // h323CapabilityType 
    NULL                                // h323CapabilityData
  },

};

/////////////////////////////////////////////////////////////////////////////

extern "C" {

PLUGIN_CODEC_IMPLEMENT_ALL(SpanDSP, faxCodecDefn, MY_API_VERSION)

};

 	  	 
