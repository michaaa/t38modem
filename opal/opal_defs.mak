#
# opal_inc.mak
#
# Make symbols include file for Open Phone Abstraction library
#
# Copyright (c) 2001 Equivalence Pty. Ltd.
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Open Phone Abstraction library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Contributor(s): ______________________________________.
#
# $Revision: 23534 $
# $Author: csoutheren $
# $Date: 2009-09-25 07:00:41 +0200 (Fr, 25. Sep 2009) $
#

ifndef DEBUG_BUILD
DEBUG_BUILD             = no
endif
ifndef OPAL_SHARED_LIB
OPAL_SHARED_LIB         = yes
endif


# Tool names detected by configure
CXX                     = g++
CC                      = gcc
INSTALL                 = /usr/bin/install -c
AR                      = ar
RANLIB                  = ranlib
LD                      = g++
ARCHIVE                 = ar rcs
SWIG                    = 

# The install directories
prefix                  = /opt/t38modem
exec_prefix             = ${prefix}
libdir                  = ${exec_prefix}/lib
includedir              = ${prefix}/include
datarootdir             = ${prefix}/share

# The opal source and destination dirs
OPALDIR                 = /root/build/opal
OPAL_SRCDIR             = $(OPALDIR)/src
OPAL_INCDIR             = $(OPALDIR)/include
OPAL_LIBDIR             = ${OPALDIR}/lib_linux_x86_64

# The library file names
RELEASE_LIB_NAME                = libopal
RELEASE_LIB_FILENAME_STATIC     = libopal_s.a
RELEASE_LIB_FILENAME_SHARED     = libopal.so
RELEASE_LIB_FILENAME_SHARED_MAJ = @RELEASE_LIB_FILENAME_SHARED_MAJ@
RELEASE_LIB_FILENAME_SHARED_MIN = @RELEASE_LIB_FILENAME_SHARED_MIN@
RELEASE_LIB_FILENAME_SHARED_PAT = libopal.so.3.9-beta0
RELEASE_CFLAGS                  =  -Os
RELEASE_LIBS                    = -L/opt/t38modem/lib -lpt  
RELEASE_OPAL_OBJDIR	        = ${OPALDIR}/lib_linux_x86_64/obj
RELEASE_OPAL_DEPDIR	        = ${OPALDIR}/lib_linux_x86_64/obj

# The library file names
DEBUG_LIB_NAME                  = libopal_d
DEBUG_LIB_FILENAME_STATIC       = libopal_d_s.a
DEBUG_LIB_FILENAME_SHARED       = libopal_d.so
DEBUG_LIB_FILENAME_SHARED_MAJ   = @DEBUG_LIB_FILENAME_SHARED_MAJ@
DEBUG_LIB_FILENAME_SHARED_MIN   = @DEBUG_LIB_FILENAME_SHARED_MIN@
DEBUG_LIB_FILENAME_SHARED_PAT   = libopal_d.so.3.9-beta0
DEBUG_CFLAGS                    =  -g3 -ggdb -O0 -D_DEBUG
DEBUG_LIBS                      = -L/opt/t38modem/lib -lpt_d  
DEBUG_OPAL_OBJDIR	        = ${OPALDIR}/lib_linux_x86_64/obj_d
DEBUG_OPAL_DEPDIR	        = ${OPALDIR}/lib_linux_x86_64/obj_d

# Compile and linker flags
CFLAGS           :=  -Wall -Wextra -Wstrict-aliasing -Wfloat-equal -Wno-comment -Wno-unused -Winit-self -Wno-missing-field-initializers -fPIC -DP_64BIT -DPTRACING=1 -D_REENTRANT -fno-exceptions -I/opt/t38modem/include    -I$(OPAL_INCDIR) $(CFLAGS)
CXXFLAGS         :=  -Wall -Wextra -Wstrict-aliasing -Wfloat-equal -Wno-comment -Wno-unused -Winit-self -Wno-missing-field-initializers -fPIC -DP_64BIT -DPTRACING=1 -D_REENTRANT -fno-exceptions -I/opt/t38modem/include   -felide-constructors -Wreorder  -I$(OPAL_INCDIR) $(CXXFLAGS)
LIBS             +=  -ldl
LDSOOPTS          = -shared -Wl,-soname,$(LIB_FILENAME_SHARED_PAT)

HAVE_RANLIB      := no

OPAL_H323         = yes
OPAL_SIP          = yes
OPAL_IAX2         = yes
OPAL_VIDEO        = yes
OPAL_ZRTP         = no
OPAL_LID	  = yes
OPAL_IVR	  = yes
OPAL_HAS_H224     = yes
OPAL_HAS_H281     = yes
OPAL_H450         = yes
OPAL_H460         = yes
OPAL_H501         = yes
OPAL_T120DATA     = @OPAL_T120DATA@
OPAL_SRTP	  = no
OPAL_RFC4175	  = yes
OPAL_AEC          = yes
OPAL_G711PLC      = yes
OPAL_T38_CAP	  = yes
OPAL_FAX          = yes
OPAL_JAVA         = no
SPEEXDSP_SYSTEM   = no
OPAL_HAS_MSRP	  = yes
OPAL_HAS_SIPIM	  = yes
OPAL_HAS_RFC4103  = yes
OPAL_HAS_MIXER    = yes
OPAL_HAS_PCSS     = yes

OPAL_PLUGINS      = yes
OPAL_SAMPLES      = no

OPAL_PTLIB_SSL    = no
OPAL_PTLIB_SSL_AES= no
OPAL_PTLIB_ASN    = yes
OPAL_PTLIB_EXPAT  = yes
OPAL_PTLIB_AUDIO  = yes
OPAL_PTLIB_VIDEO  = yes
OPAL_PTLIB_WAVFILE= yes
OPAL_PTLIB_DTMF   = yes
OPAL_PTLIB_IPV6   = yes
OPAL_PTLIB_DNS    = yes
OPAL_PTLIB_LDAP   = no
OPAL_PTLIB_VXML   = yes
OPAL_PTLIB_CONFIG_FILE=yes


ifeq ($(DEBUG_BUILD),yes)
  CFLAGS   := $(DEBUG_CFLAGS) $(CFLAGS)
  CXXFLAGS := $(DEBUG_CFLAGS) $(CXXFLAGS)
  LIBS     += $(DEBUG_LIBS)
  LIB_NAME  = $(DEBUG_LIB_NAME)
else
  CFLAGS   := $(RELEASE_CFLAGS) $(CFLAGS)
  CXXFLAGS := $(RELEASE_CFLAGS) $(CXXFLAGS)
  LIBS     += $(RELEASE_LIBS)
  LIB_NAME  = $(RELEASE_LIB_NAME)
endif


# End of file
