#
# Makefile for GSM-AMR Codec Plugin
#
# Copyright (C) 2004 MX Telecom Ltd.
#
# $Revision: 23464 $
# $Author: csoutheren $
# $Date: 2009-09-16 05:03:33 +0200 (Mi, 16. Sep 2009) $

AC_PLUGIN_DIR=opal-3.9.0/codecs/audio
prefix=/opt/t38modem
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
target_os=linux-gnu

OBJDIR	= ./obj
SRCDIR  = ./src
PLUGINDIR=../..

BASENAME= gsmamrcodec
SONAME	= $(BASENAME)

CC	 	=gcc
CFLAGS	 	= -Os 
CXX	 	=g++
LDSO	 	=-shared -Wl,-soname,$(SONAME)
PLUGINEXT	=so
STDCCFLAGS	= -fPIC
LDFLAGS		=

SRCS	= amrcodec.c \
	interf_enc.c \
	interf_dec.c \
	sp_dec.c \
	sp_enc.c

vpath	%.o $(OBJDIR)
vpath	%.c $(SRCDIR)

CFLAGS += -I../../../include -I$(SRCDIR)
CFLAGS += -DIF2
EXTRACFLAGS=-I$(PLUGINDIR)

PLUGIN	= ./$(BASENAME)_pwplugin.$(PLUGINEXT)

ifeq ($(VERBOSE),)
Q_CC = @echo [CC] `echo $< | sed s^@OPALDIR@/^^` ;
Q_LD = @echo [LD] `echo $(PLUGIN) | sed s^@OPALDIR@/^^` ;
endif


$(OBJDIR)/%.o : %.c
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(Q_CC)$(CC) -c -I../../../include $(STDCCFLAGS) $(EXTRACFLAGS) $(CFLAGS) -o $@ $<

OBJECTS	= $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(notdir $(SRCS))))


$(PLUGIN): $(OBJECTS)

ifeq (solaris,$(findstring solaris,$(target_os)))

	$(Q_LD)$(CC) $(LDSO) $@ -o $@ $^
else	
	$(Q_LD)$(CC) $(LDSO) -o $@ $^

endif	

install:
	mkdir -p $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)
	install $(PLUGIN) $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(OBJECTS) $(PLUGIN)

###########################################
