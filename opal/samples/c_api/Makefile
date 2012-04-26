#
# Makefile
#
# An example of the "C" interface to OPAL
#
# Open Phone Abstraction Library
#
# Copyright (c) 2008 Vox Lucida
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
# The Initial Developer of the Original Code is Vox Lucida (Robert Jongbloed)
#
# Contributor(s): ______________________________________.
#
# $Revision$
# $Author$
# $Date$

PROG		= c_api
SOURCES		:= main.c


# If we have environment variables for local copies of
# OPAL and PTLib then use those packages by overriding
# the search path for pkg-config
ifneq ($(PTLIBDIR),)
ifneq ($(OPALDIR),)
PKG_CONFIG_PATH=$(OPALDIR):$(PTLIBDIR)
endif
endif


# Get required flags from library

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config $(PKG_FLAGS) --cflags opal)
LDFLAGS += -ldl


OBJDIR := obj

vpath	%.o   $(OBJDIR)

$(OBJDIR)/%.o : %.c
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(CC) $(CFLAGS) -c $< -o $@

$(OPAL_DEPDIR)/%.dep : %.cxx 
	@if [ ! -d $(OPAL_DEPDIR) ] ; then mkdir -p $(OPAL_DEPDIR) ; fi
	@printf %s $(OPAL_OBJDIR)/ > $@
	$(CC) $(CFLAGS:-g=) -M $< >> $@


# This builds the application

OBJECTS = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(patsubst %.cxx,%.o,$(notdir $(SOURCES)))))

$(OBJDIR)/$(PROG): $(OBJECTS)
	 $(CC) $^ $(LDFLAGS) -o $@

depend:

clean:
	rm $(OBJECTS) $(OBJDIR)/$(PROG)
