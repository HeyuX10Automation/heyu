#
# Makefile.am - Automake source file for Heyu
#
# Copyright (C) 2011 Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

bin_PROGRAMS = heyu
heyu_SOURCES = date.c erase.c info.c  message.c relay.c \
	monitor.c reset.c setclock.c stop.c \
	tty.c x10.c xread.c xwrite.c status.c cm11a.c \
	eeprom.c process.c sun.c cmd.c config.c x10state.c \
	poll.c modules.c cm17a.c xsync.c timing.c cm10a.c \
	tty_aux.c relay_aux.c x10aux.c rfxcom.c digimax.c oregon.c \
	x10.h eeprom.h process.h sun.h x10state.h \
	rfxcom.h digimax.h oregon.h
if XPLLIB_SRCDIR
heyu_CPPFLAGS = -I$(xPLLib_srcdir) $(xPL_isrc)
heyu_DEPENDENCIES = libxPL.a
heyu_LDADD = libxPL.a
noinst_LIBRARIES = libxPL.a
nodist_libxPL_a_SOURCES = $(xPLLib_srcdir)/xPL-config.c $(xPLLib_srcdir)/xPL.h \
	$(xPLLib_srcdir)/xPL-hub.c $(xPLLib_srcdir)/xPL-io.c \
	$(xPLLib_srcdir)/xPL-listeners.c $(xPLLib_srcdir)/xPL-message.c \
	$(xPLLib_srcdir)/xPL_priv.h $(xPLLib_srcdir)/xPL-service.c \
	$(xPLLib_srcdir)/xPL-store.c $(xPLLib_srcdir)/xPL-utils.c
EXTRA_PROGRAMS = xPL_Hub
nodist_xPL_Hub_SOURCES = $(xPLLib_srcdir)/examples/xPL_Hub.c
xPL_Hub_DEPENDENCIES = libxPL.a
xPL_Hub_LDADD = libxPL.a
CLEANFILES = xPL_Hub stamp-xPL_Hub
endif
dist_man_MANS = heyu.1 x10config.5 x10sched.5 x10scripts.5 x10cm17a.5 x10aux.5 \
	x10rfxsensors.5 x10rfxmeters.5 x10digimax.5 x10oregon.5 x10kaku.5
dist_doc_DATA = README README1 README2 protocol.txt history.txt heyufaq.txt \
	AUTHORS COPYING INSTALL README.cm10a README.webhook release_notes.txt
dist_pkgsysconf_DATA = x10config.sample x10.sched.sample
EXTRA_DIST = Configure.sh post-install.sh

@MAN1DIR@
@MAN5DIR@

*.o: Makefile

if XPLLIB_ARCHIVE
$(nodist_libxPL_a_SOURCES) $(nodist_xPL_Hub_SOURCES): $(xPLLib_archive)
	@mkdir -p "$(xPLLib_srcdir)"
	@case "$<" in \
	*.tar.gz|*.tgz) GZIP=$(GZIP_ENV) gzip -dc "$<" | \
				$(am__untar) -mC "$(xPLLib_srcdir)/.." ;;\
	*.tar.bz2) bzip2 -dc "$<" | $(am__untar) -mC "$(xPLLib_srcdir)/.." ;;\
	*.tar.lzma) lzma -dc "$<" | $(am__untar) -mC "$(xPLLib_srcdir)/.." ;;\
	*.tar.xz) xz -dc "$<" | $(am__untar) -mC "$(xPLLib_srcdir)/.." ;;\
	*.tar.Z) uncompress -c "$<" | $(am__untar) -mC "$(xPLLib_srcdir)/.." ;;\
	*.shar.gz) unshar "$<" -d "$(xPLLib_srcdir)/.." ;;\
	*.zip) unzip "$<" -D -d "$(xPLLib_srcdir)/.." ;;\
	esac

distclean-local:
	-rm -rf $(xPLLib_srcdir)
endif

if XPLLIB_URL
DISTCLEANFILES = $(xPLLib_archive)

AMWGET_O = $(SHELL) "$(srcdir)/missing" --run $(WGET_O)

$(xPLLib_archive):
	@$(AMWGET_O) '$@' '$(xPLLib_URL)'
endif

if POSTINST
if XPLLIB_SRCDIR
all-local: stamp-xPL_Hub usergroup.tmp userhome.tmp

stamp-xPL_Hub: $(nodist_xPL_Hub_SOURCES) Makefile
	@if netstat -uln 2>/dev/null | grep -q ":3865 " ||\
			type xPL_Hub$(EXEEXT) 2>/dev/null ||\
			type xplhub.pl 2>/dev/null ||\
			type xPLHub.jar 2>/dev/null; then \
		rm -f xPL_Hub$(EXEEXT);\
	elif $(MAKE) $(AM_MAKEFLAGS) xPL_Hub; then \
	echo "";\
	echo "An xPL hub daemon, required for running Heyu built with xPL";\
	echo "suppport enabled, has been additionally compiled for you, and";\
	echo "will be installed along with the Heyu binary upon executing";\
	echo "'make install', unless a running xPL hub is detected by then.";\
	echo "";\
	echo "If this is not what you want, i.e., you have an xPL hub already";\
	echo "installed in your system and don't want to install another one,";\
	echo "please remove the xPL_Hub executable from the current directory";\
	echo "before proceeding with 'make install'.";\
	echo "";\
	echo "If removed, it can be rebuilt with 'make xPL_Hub'.";\
	echo "";\
	fi
	@touch $@
else
all-local: usergroup.tmp userhome.tmp
endif

usergroup.tmp userhome.tmp: $(bin_PROGRAMS)
	@echo `id` >usergroup.tmp
	@echo ${HOME}  >userhome.tmp
	@echo ""
	@echo "**Now become root and run 'make install' **"
	@echo ""

install-exec-hook:
	@$(POST_INSTALL)
	@POSTINST@
if XPLLIB_SRCDIR
	@if ! netstat -uln|grep -q ":3865 " && test -x xPL_Hub$(EXEEXT);then \
		$(MAKE) $(AM_MAKEFLAGS) bin_PROGRAMS=xPL_Hub$(EXEEXT) \
				install-binPROGRAMS; fi

install-data-hook:
	@if netstat -uln|grep -q ":3865 " || test -x xPL_Hub$(EXEEXT); then :;\
	else \
	echo "";\
	echo "Heyu has been built with xPL support enabled, and it requires";\
	echo "an xPL hub daemon running before it can be used.";\
	echo "";\
	echo "If you have an xPL hub already installed in your system, please";\
	echo "start it up before running Heyu.  If you don't have one, please";\
	echo "see http://xplproject.org.uk/wiki/index.php?title=Core_Software";\
	echo "for possible options.";\
	echo;\
	echo "Please also set up the hub for being started on boot, before";\
	echo "Heyu is run.";\
	echo;\
	echo "You can also relay on Heyu starting the hub automatically for";\
	echo "you if you choose one of Heyu known xPL hubs, and its binary";\
	echo "can be found in your \$$PATH.  The xPL hubs known to Heyu are:";\
	echo "* xPL_Hub   - see http://www.xpl4java.org/xPL4Linux/,";\
	echo "* xplhub.pl - see http://johnb.xplproject.org.uk/info/perl/.";\
	echo;\
	echo "Another example of a hub that can be started automatically by";\
	echo "Heyu, but only if your system is configured for directly";\
	echo "running Java executables found in your \$$PATH, is xPLHub.jar,";\
	echo "which can be downloaded from http://www.xpl4java.org/xPL4Java/.";\
	echo "";\
	echo "Finally, you can set up Heyu to start up any hub of your choice";\
	echo "by providing its command name and arguments in your x10config.";\
	echo "See 'man x10config' for details.";\
	echo "";\
	fi
endif
endif
