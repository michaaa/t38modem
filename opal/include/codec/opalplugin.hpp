/*
 * opalplugins.hpp
 *
 * OPAL codec plugins handler (C++ version)
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (C) 2010 Vox Lucida
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Vox Lucida
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 24119 $
 * $Author: rjongbloed $
 * $Date: 2010-03-12 07:28:47 +0100 (Fr, 12. Mär 2010) $
 */

#ifndef OPAL_CODEC_OPALPLUGIN_HPP
#define OPAL_CODEC_OPALPLUGIN_HPP

#include "opalplugin.h"

#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <limits.h>


/////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINCODEC_TRACING
  #define PLUGINCODEC_TRACING 1
#endif

#if PLUGINCODEC_TRACING
  static PluginCodec_LogFunction PluginCodec_LogFunctionInstance;

  static int PluginCodec_SetLogFunction(const PluginCodec_Definition *, void *, const char *, void * parm, unsigned * len)
  {
    if (len == NULL || *len != sizeof(PluginCodec_LogFunction))
      return false;

    PluginCodec_LogFunctionInstance = (PluginCodec_LogFunction)parm;
    if (PluginCodec_LogFunctionInstance != NULL)
      PluginCodec_LogFunctionInstance(4, __FILE__, __LINE__, "Plugin", "Started logging.");

    return true;
  }

  #define PLUGINCODEC_CONTROL_LOG_FUNCTION { PLUGINCODEC_CONTROL_SET_LOG_FUNCTION, PluginCodec_SetLogFunction },
#else
  #define PLUGINCODEC_CONTROL_LOG_FUNCTION
#endif

#if !defined(PTRACE)
  #if PLUGINCODEC_TRACING
    #include <sstream>
    #define PTRACE(level, section, args) \
      if (PluginCodec_LogFunctionInstance != NULL && PluginCodec_LogFunctionInstance(level, NULL, 0, NULL, NULL)) { \
        std::ostringstream strm; strm << args; \
        PluginCodec_LogFunctionInstance(level, __FILE__, __LINE__, section, strm.str().c_str()); \
      } else (void)0
  #else
    #define PTRACE(level, section, expr)
  #endif
#endif


/////////////////////////////////////////////////////////////////////////////

class PluginCodec_MediaFormat
{
    friend class PluginCodec;

  public:
    typedef struct PluginCodec_Option const * const * OptionsTable;
    typedef std::map<std::string, std::string> OptionMap;

  protected:
    OptionsTable m_options;

  public:
    PluginCodec_MediaFormat(OptionsTable options)
      : m_options(options)
    {
    }


    virtual ~PluginCodec_MediaFormat()
    {
    }


    bool AdjustOptions(void * parm, unsigned * parmLen, bool (PluginCodec_MediaFormat:: * adjuster)(OptionMap & original, OptionMap & changed))
    {
      if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
        return false;

      OptionMap originalOptions;
      for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2)
        originalOptions[option[0]] = option[1];

      OptionMap changedOptions;
      if (!(this->*adjuster)(originalOptions, changedOptions))
        return false;

      char ** options = (char **)calloc(changedOptions.size()*2+1, sizeof(char *));
      *(char ***)parm = options;
      if (options == NULL)
        return false;

      for (OptionMap::iterator i = changedOptions.begin(); i != changedOptions.end(); ++i) {
        *options++ = strdup(i->first.c_str());
        *options++ = strdup(i->second.c_str());
      }

      return true;
    }


    virtual bool ToNormalised(OptionMap & original, OptionMap & changed) = 0;


    virtual bool ToCustomised(OptionMap & original, OptionMap & changed) = 0;


    static void Change(const char * value,
                       OptionMap  & original,
                       OptionMap  & changed,
                       const char * option)
    {
      if (original[option] != value)
        changed[option] = value;
    }


    static unsigned String2Unsigned(const std::string & str)
    {
      return strtoul(str.c_str(), NULL, 10);
    }


    static void Unsigned2String(unsigned value, std::string & str)
    {
      // Not very efficient, but really, really simple
      if (value > 9)
        Unsigned2String(value/10, str);
      str += (char)(value%10 + '0');
    }


    static void Change(unsigned     value,
                       OptionMap  & original,
                       OptionMap  & changed,
                       const char * option)
    {
      if (String2Unsigned(original[option]) != value)
        Unsigned2String(value, changed[option]);
    }


    static void ClampMax(unsigned     maximum,
                         OptionMap  & original,
                         OptionMap  & changed,
                         const char * option)
    {
      unsigned value = String2Unsigned(original[option]);
      if (value > maximum)
        Unsigned2String(maximum, changed[option]);
    }


    static void ClampMin(unsigned     minimum,
                         OptionMap  & original,
                         OptionMap  & changed,
                         const char * option)
    {
      unsigned value = String2Unsigned(original[option]);
      if (value < minimum)
        Unsigned2String(minimum, changed[option]);
    }
};


/////////////////////////////////////////////////////////////////////////////

class PluginCodec
{
  protected:
    PluginCodec(const PluginCodec_Definition * defn)
      : m_definition(defn)
      , m_optionsSame(false)
      , m_maxBitRate(defn->bitsPerSec)
      , m_frameTime((defn->sampleRate/1000*defn->usPerFrame)/1000) // Odd way of calculation to avoid 32 bit integer overflow
    {
      PTRACE(3, "Plugin", "Codec created: \"" << defn->descr
             << "\", \"" << defn->sourceFormat << "\" -> \"" << defn->destFormat << '"');
    }


  public:
    virtual ~PluginCodec()
    {
    }


    virtual bool Construct()
    {
      return true;
    }


    virtual bool Transcode(const void * fromPtr,
                             unsigned & fromLen,
                                 void * toPtr,
                             unsigned & toLen,
                             unsigned & flags) = 0;


    virtual size_t GetOutputDataSize()
    {
      return 576-20-16; // Max safe MTU size (576 bytes as per RFC879) minus IP & UDP headers
    }


    virtual bool SetOptions(const char * const * options)
    {
      m_optionsSame = true;

      // get the media format options after adjustment from protocol negotiation
      for (const char * const * option = options; *option != NULL; option += 2) {
        if (!SetOption(option[0], option[1]))
          return false;
      }

      if (m_optionsSame)
        return true;

      return OnChangedOptions();
    }


    virtual bool OnChangedOptions()
    {
      return true;
    }


    virtual bool SetOption(const char * optionName, const char * optionValue)
    {
      if (strcasecmp(optionName, PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
        return SetOptionUnsigned(m_maxBitRate, optionValue, 1, m_definition->bitsPerSec);

      if (strcasecmp(optionName, PLUGINCODEC_OPTION_FRAME_TIME) == 0)
        return SetOptionUnsigned(m_frameTime, optionValue, m_definition->sampleRate/1000, m_definition->sampleRate); // 1ms to 1 second

      return true;
    }


    bool SetOptionUnsigned(unsigned & oldValue, const char * optionValue, unsigned minimum, unsigned maximum = UINT_MAX)
    {
      char * end;
      unsigned newValue = strtoul(optionValue, &end, 10);
      if (*end != '\0')
        return false;

      if (newValue < minimum)
        newValue = minimum;
      else if (newValue > maximum)
        newValue = maximum;

      if (oldValue != newValue) {
        oldValue = newValue;
        m_optionsSame = false;
      }

      return true;
    }


    bool SetOptionBoolean(bool & oldValue, const char * optionValue)
    {
      bool newValue;
      if (strcmp(optionValue, "0") == 0)
        newValue = false;
      else if (strcmp(optionValue, "1") == 0)
        newValue = true;
      else
        return false;

      if (oldValue != newValue) {
        oldValue = newValue;
        m_optionsSame = false;
      }

      return true;
    }


    bool SetOptionBit(unsigned & oldValue, unsigned bit, const char * optionValue)
    {
      bool newValue;
      if (strcmp(optionValue, "0") == 0)
        newValue = false;
      else if (strcmp(optionValue, "1") == 0)
        newValue = true;
      else
        return false;

      if (((oldValue&bit) != 0) != newValue) {
        if (newValue)
          oldValue |= bit;
        else
          oldValue &= ~bit;
        m_optionsSame = false;
      }

      return true;
    }


    template <class CodecClass> static void * Create(const PluginCodec_Definition * defn)
    {
      CodecClass * codec = new CodecClass(defn);
      if (codec != NULL && codec->Construct())
        return codec;

      PTRACE(1, "Plugin", "Could not open codec, no context being returned.");
      delete codec;
      return NULL;
    }


    static void Destroy(const PluginCodec_Definition * /*defn*/, void * context)
    {
      delete (PluginCodec *)context;
    }


    static int Transcode(const PluginCodec_Definition * /*defn*/,
                                                 void * context,
                                           const void * fromPtr,
                                             unsigned * fromLen,
                                                 void * toPtr,
                                             unsigned * toLen,
                                         unsigned int * flags)
    {
      if (context != NULL && fromPtr != NULL && fromLen != NULL && toPtr != NULL && toLen != NULL && flags != NULL)
        return ((PluginCodec *)context)->Transcode(fromPtr, *fromLen, toPtr, *toLen, *flags);

      PTRACE(1, "Plugin", "Invalid parameter to Transcode.");
      return false;
    }


    static int GetOutputDataSize(const PluginCodec_Definition *, void * context, const char *, void *, unsigned *)
    {
      return context != NULL ? ((PluginCodec *)context)->GetOutputDataSize() : 0;
    }


    static int ToNormalised(const PluginCodec_Definition * defn, void *, const char *, void * parm, unsigned * len)
    {
      return defn->userData != NULL ? ((PluginCodec_MediaFormat *)defn->userData)->AdjustOptions(parm, len, &PluginCodec_MediaFormat::ToNormalised) : -1;
    }


    static int ToCustomised(const PluginCodec_Definition * defn, void *, const char *, void * parm, unsigned * len)
    {
      return defn->userData != NULL ? ((PluginCodec_MediaFormat *)defn->userData)->AdjustOptions(parm, len, &PluginCodec_MediaFormat::ToCustomised) : -1;
    }


    static int FreeOptions(const PluginCodec_Definition *, void *, const char *, void * parm, unsigned * len)
    {
      if (parm == NULL || len == NULL || *len != sizeof(char ***))
        return false;

      char ** strings = (char **)parm;
      for (char ** string = strings; *string != NULL; string++)
        free(*string);
      free(strings);
      return true;
    }


    static int GetOptions(const struct PluginCodec_Definition * codec, void *, const char *, void * parm, unsigned * len)
    {
      if (parm == NULL || len == NULL || *len != sizeof(struct PluginCodec_Option **))
        return false;

      *(const void **)parm = codec->userData != NULL ? ((PluginCodec_MediaFormat *)codec->userData)->m_options : NULL;
      *len = 0;
      return true;
    }


    static int SetOptions(const PluginCodec_Definition *, 
                                 void * context,
                                 const char * , 
                                 void * parm, 
                                 unsigned * len)
    {
      PluginCodec * codec = (PluginCodec *)context;
      return len != NULL && *len == sizeof(const char **) && parm != NULL &&
             codec != NULL && codec->SetOptions((const char * const *)parm);
    }

  protected:
    const PluginCodec_Definition * m_definition;

    bool     m_optionsSame;
    unsigned m_maxBitRate;
    unsigned m_frameTime;
};


#define PLUGINCODEC_DEFINE_CONTROL_TABLE(name) \
  static PluginCodec_ControlDefn name[] = { \
    { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  PluginCodec::GetOutputDataSize }, \
    { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, PluginCodec::ToNormalised }, \
    { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, PluginCodec::ToCustomised }, \
    { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    PluginCodec::FreeOptions }, \
    { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     PluginCodec::SetOptions }, \
    { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     PluginCodec::GetOptions }, \
    PLUGINCODEC_CONTROL_LOG_FUNCTION \
    { NULL } \
  }



#endif // OPAL_CODEC_OPALPLUGIN_HPP
