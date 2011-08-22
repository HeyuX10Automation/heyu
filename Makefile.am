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
#   As an additional permission under section 7 of the GNU General
#   Public License, version 3, the author permits you to distribute this
#   file bundled with Heyu software, under the same license terms that
#   Heyu is distributed, as long as it is propagated for the sole
#   purpose of configuring and building Heyu.
#   

bin_PROGRAMS = heyu
heyu_SOURCES = date.c erase.c info.c  message.c relay.c \
	monitor.c reset.c setclock.c stop.c \
	tty.c x10.c xread.c xwrite.c status.c cm11a.c \
	eeprom.c process.c sun.c cmd.c config.c x10state.c \
	poll.c modules.c cm17a.c xsync.c timing.c cm10a.c \
	tty_aux.c relay_aux.c x10aux.c rfxcom.c digimax.c oregon.c \
	x10.h eeprom.h version.h process.h sun.h x10state.h \
	rfxcom.h digimax.h oregon.h
dist_man_MANS = heyu.1 x10config.5 x10sched.5 x10scripts.5 x10cm17a.5 x10aux.5 \
	x10rfxsensors.5 x10rfxmeters.5 x10digimax.5 x10oregon.5 x10kaku.5
dist_doc_DATA = README README2 protocol.txt history.txt heyufaq.txt \
	INSTALL LICENSE GPL-3 README.cm10a README.webhook release_notes.txt
dist_pkgsysconf_DATA = x10config.sample x10.sched.sample
EXTRA_DIST = Configure.sh install.sh README.INSTALL

@MAN1DIR@
@MAN5DIR@

*.o: Makefile
