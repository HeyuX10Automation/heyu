#!/bin/sh

# This program will configure the makefile for various OSes.
#
# Config version 1.1
SYS=
OPTIONS='--localstatedir=/var --mandir=$(prefix)/man'
OPTIONS="$OPTIONS --enable-postinst=./post-install.sh"

while [ $# -ge 1 ] ; do
    case "$1" in
	help|-h|-help)
	    echo "Usage: $0 <system type> [-nocm17a] [-noext0]"
	    echo " Valid system types are linux,sunos,solaris,opensolaris,opensolaris_bsd,"
	    echo " freebsd,openbsd,netbsd,darwin,sco,aix,sysv,attsvr4,nextstep,osf,generic"
	    echo " If no system type is specified, the output of uname(1) will be used."
            echo " Switch -nocm17a omits support for the optional CM17A Firecracker device." 
            echo " Switch -noext0 omits support for extended type 0 shutter controllers."
            echo " Switch -norfxs omits support for RFXSensors."
            echo " Switch -norfxm omits support for RFXMeters."
            echo " Switch -nodmx omits support for the Digimax thermostat."
            echo " Switch -noore omits support for Oregon sensors."
            echo " Switch -nokaku omits support for KaKu/HomeEasy signals"
            echo " Switch -norfxlan omits support for RFXLAN receiver"
            echo " Switch -flags=n sets the number of user flags (32 min, 1024 max)"
            echo " Switch -counters=n sets the number of counters (32 min, 1024 max)"
            echo " Switch -timers=n sets the number of timers (32 min, 1024 max)"
            echo " Above numbers are rounded up to the nearest multiple of 32"
	    rm -f Makefile
	    exit
	    ;;
        nocm17a|-nocm17a|NOCM17A|-NOCM17A|--disable-cm17a)
            OPTIONS="$OPTIONS --disable-cm17a"
            ;;
        noext0|-noext0|NOEXT0|-NOEXT0|--disable-ext0)
            OPTIONS="$OPTIONS --disable-ext0"
            ;;
        norfxs|-norfxs|NORFXS|-NORFXS|--disable-rfxs)
            OPTIONS="$OPTIONS --disable-rfxs"
            ;;
        norfxm|-norfxm|NORFXM|-NORFXM|--disable-rfxm)
            OPTIONS="$OPTIONS --disable-rfxm"
            ;;
        nodmx|-nodmx|NODMX|-NODMX|--disable-dmx)
            OPTIONS="$OPTIONS --disable-dmx"
            ;;
        noore|-noore|NOORE|-NOORE|--disable-ore)
            OPTIONS="$OPTIONS --disable-ore"
            ;;
        nokaku|-nokaku|NOKAKU|-NOKAKU|--disable-kaku)
            OPTIONS="$OPTIONS --disable-kaku"
            ;;
        norfxlan|-norfxlan|NORFXLAN|-NORFXLAN|--disable-rfxlan)
            OPTIONS="$OPTIONS --disable-rfxlan"
            ;;
	flags=*|-flags=*|FLAGS=*|-FLAGS=*)
	    IFS="${IFS}=" read keyword FLAGS <<EoF
$1
EoF
            OPTIONS="$OPTIONS FLAGS=$FLAGS"
	    ;;
	counters=*|-counters=*|COUNTERS=*|-COUNTERS=*)
	    IFS="${IFS}=" read keyword COUNTERS <<EoF
$1
EoF
            OPTIONS="$OPTIONS COUNTERS=$COUNTERS"
	    ;;
	timers=*|-timers=*|TIMERS=*|-TIMERS=*)
	    IFS="${IFS}=" read keyword TIMERS <<EoF
$1
EoF
            OPTIONS="$OPTIONS TIMERS=$TIMERS"
	    ;;
	*)
	    SYS="$1"
	    ;;
    esac
    shift
done

test -x config.guess && test -x config.sub && \
test -x install-sh && test -x missing && test -x depcomp && \
test -s aclocal.m4 && test aclocal.m4 -nt configure.ac && \
test -s Makefile.in && test Makefile.in -nt configure.ac && \
			test Makefile.in -nt Makefile.am && \
test -x configure && test configure -nt configure.ac && \
test -s config.h.in && test config.h.in -nt configure.ac || {
	type autoreconf >/dev/null && \
	type autoconf >/dev/null && type autoheader >/dev/null || {
		echo "Please install autoconf package and re-run ./Configure.sh"
		rm -f Makefile
		exit
	}
	type aclocal >/dev/null && type automake >/dev/null || {
		echo "Please install automake package and re-run ./Configure.sh"
		rm -f Makefile
		exit
	}
	if
		test -x config.guess && test -x config.sub && \
		test -x install-sh && test -x missing && test -x depcomp
	then
		autoreconf --verbose
	else
		autoreconf --verbose --install --symlink
	fi
}

cat <<EoF

This script will create a Makefile based by default on
the output of uname(1), or otherwise on the system type
parameter you enter.
 
EoF



if [ "$SYS" = "" ] ; then
    SYS=`uname -s | tr '[A-Z]' '[a-z]'`
fi

case "$SYS" in
    linux)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	;;
    sunos*|solaris)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/locks\"'
	;;
    opensolaris)
	OPTIONS="$OPTIONS --prefix=/opt/heyu"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/locks\"'
	;;
    opensolaris_bsd)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/locks\"'
	;;
    freebsd)
	CPPFLAGS='-DLOCKDIR=\"/var/spool/lock\"'
	;;
    openbsd)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/lock\"'
       ;;
    netbsd)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/lock\"'
	;;
    darwin)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	;;
    sco*)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/locks\" -DSPOOLDIR=\"/usr/tmp/heyu\"'
	OPTIONS="$OPTIONS man1dir=/usr/local/man/man.1"
	OPTIONS="$OPTIONS man5dir=/usr/local/man/man.5"
	;;
    aix|sysv)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	;;
    attsvr4)
	CPPFLAGS='-DLOCKDIR=\"/var/spool/locks\" -DSPOOLDIR=\"/var/spool/heyu\"'
	CPPFLAGS="$CPPFLAGS -I/usr/local/include"
	LDFLAGS="-L/usr/ucblib"
	LIBS="-lucb -lgen -lcmd"
	;;
    nextstep)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS="-posix"
	LDFLAGS="-posix"
	;;
    osf)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	CPPFLAGS='-DLOCKDIR=\"/var/spool/locks\"'
      ;;
    generic)
	OPTIONS="$OPTIONS --sysconfdir=/etc"
	;;
    *)
    	echo "Your system type was not understood.  Please try one of "
    	echo "the following".
    	$0 -help
    	exit
    	;;
esac


(	set -x
	./configure $OPTIONS CPPFLAGS="$CPPFLAGS" LDFLAGS="$LDFLAGS" \
			LIBS="$LIBS"
) || {
	rm -f Makefile
	exit
}

if [ "$SYS" = "sysv" ] ; then
echo "The Makefile has been created for sysv, however if"
echo "you are running under AT&T SysV r4, please re-run"
echo "Configure.sh with the system type parameter attsvr4"
else
echo "The Makefile has been created for $SYS."
fi
if [ "$SYS" = "opensolaris" ] ; then
echo "Please see \"Notes for OpenSolaris\" in file README before proceeding."
echo
fi

echo ""
echo "Note: If you are upgrading from an earlier version,"
echo "run 'heyu stop' before proceeding further."
echo ""
echo "** Now run 'make' as a normal user **"
echo ""
