#
# Makefile for G.722 codec plugin
#
# Copyright (C) 2008 Vox Lucida Pty. Ltd., All Rights Reserved
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
# The Original Code is Open Phone Abstraction Library.
#
# The Initial Developer of the Original Code is Robert Jongbloed
#
# Contributor(s): ______________________________________.
#
# $Revision: 23464 $
# $Author: csoutheren $
# $Date: 2009-09-16 05:03:33 +0200 (Mi, 16. Sep 2009) $
#

AC_PLUGIN_DIR=opal-3.9.0/codecs/audio
prefix=/opt/t38modem
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
target_os=linux-gnu

SONAME	= g722
STDCCFLAGS +=  -fPIC -fPIC

SRCDIR	= ./VoIPCodecs
OBJDIR	= ./obj
PLUGINDIR=../..

CC		=gcc
CXX		=g++
LDSO		=-shared -Wl,-soname,$(SONAME)
PLUGINEXT	=so
STDCCFLAGS	= -fPIC
LDFLAGS		=
EXTRACFLAGS     =-I$(PLUGINDIR)

SRCS	+= g722codec.c \
	   $(SRCDIR)/g722_encode.c \
	   $(SRCDIR)/g722_decode.c \
	   $(SRCDIR)/bitstream.c 

vpath	%.o $(OBJDIR)
vpath	%.c $(SRCDIR)

ifeq ($(VERBOSE),)
Q_CC = @echo [CC] `echo $< | sed s^@OPALDIR@/^^` ;
Q_LD = @echo [LD] `echo $(PLUGIN) | sed s^@OPALDIR@/^^` ;
endif


$(OBJDIR)/%.o : %.c
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(Q_CC)$(CC) -I../../../include $(STDCCFLAGS) $(OPTCCFLAGS) $(EXTRACFLAGS) $(CFLAGS) -c $< -o $@

PLUGIN	= ./g722_audio_pwplugin.$(PLUGINEXT)

OBJECTS = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(notdir $(SRCS))))

all: $(PLUGIN)

$(PLUGIN): $(OBJECTS)
ifeq (solaris,$(findstring solaris,$(target_os)))

	$(Q_LD)$(CC) $(LDSO) $@ -o $@ $^ $(EXTRALIBS)
else
	$(Q_LD)$(CC) $(LDSO) -o $@ $^ $(EXTRALIBS)

endif	

install:
	mkdir -p $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)
	install $(PLUGIN) $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(OBJECTS) $(PLUGIN)

###########################################
