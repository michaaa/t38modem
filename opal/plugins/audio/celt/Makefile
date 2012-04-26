#
# Makefile for Celt Codec Plugin
#
# based on the GSM-AMR one
#

AC_PLUGIN_DIR=opal-3.9.0/codecs/audio
prefix=/opt/t38modem
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
target_os=linux-gnu

PLUGINDIR=../..

BASENAME= celtcodec
SONAME	= $(BASENAME)

CC	 	=gcc
CFLAGS	 	= -Os 
CXX	 	=g++
LDSO	 	=-shared -Wl,-soname,$(SONAME)
PLUGINEXT	=so
STDCCFLAGS	= -fPIC
LDFLAGS		=
LIBS            =

SRCS	= celtcodec.c

CFLAGS += -I../../../include
EXTRACFLAGS=-I$(PLUGINDIR)

EXTRACFLAGS += 
LIBS        += 

PLUGIN	= ./$(BASENAME)_pwplugin.$(PLUGINEXT)

ifeq ($(VERBOSE),)
Q_CC = @echo [CC] `echo $< | sed s^@OPALDIR@/^^` ;
Q_LD = @echo [LD] `echo $(PLUGIN) | sed s^@OPALDIR@/^^` ;
endif

%.o : %.c
	$(Q_CC)$(CC) -c $(STDCCFLAGS) $(EXTRACFLAGS) $(CFLAGS) -o $@ $<

OBJECTS	= $(patsubst %.c,%.o,$(notdir $(SRCS)))

$(PLUGIN): $(OBJECTS)

ifeq (solaris,$(findstring solaris,$(target_os)))

	$(Q_LD)$(CC) $(LDSO) $@ -o $@ $^ $(LIBS)
else	
	$(Q_LD)$(CC) $(LDSO) -o $@ $^ $(LIBS)

endif

install:
	mkdir -p $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)
	install $(PLUGIN) $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(AC_PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(OBJECTS) $(PLUGIN)

###########################################
