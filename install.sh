#!/bin/sh

# This script attempts to install various files where they should be.  It is 
# an adjunct to the install scripts built into the Makefile
# Author Daniel Suthers
# Modified for Heyu ver 2 by Charles W. Sullivan
# Version 1.2

# This script also checks for permissions on common files and directories.
#

# Who am i?
ME=`id | sed "s/\(uid=[0-9]*\).*/\1/"`
if [ $ME = "uid=0" ] ; then
   ME=root
fi

# Recover user's home directory and uid/gid, if 'make' was run as normal user
if [ -e ./userhome.tmp ] && [ -e ./usergroup.tmp ] ; then
   USERHOME=`cat ./userhome.tmp`
   USERUID=`cat ./usergroup.tmp | sed "s/\(uid=[0-9]*\).*/\1/" |cut -d= -f2`
   USERGID=`cat ./usergroup.tmp | sed "s/\(gid=[0-9]*\).*/\1/" |cut -d= -f3`
   if [ "$USERHOME" != "/root" ] && [ "$USERHOME" != "/" ] ; then
      UIDGID=$USERUID:$USERGID
   else
      UIDGID=
   fi
else
   USERHOME=$HOME
   UIDGID=
fi

# Recover the LOCKDIR, SPOOLDIR, and SYSBASEDIR directories compiled into Heyu
eval `./heyu list`

# Files:  x10config 
FOUND=
for FL in $X10CONFIG $USERHOME/.heyu/x10config $SYSBASEDIR/x10.conf /etc/heyu/x10.conf ; do
if [ -e $FL ] ; then
    FOUND=$FL
    echo "An X10 Configuration file was found at $FL"
    break
fi
done

# Is there a heyu version 1.xx config file to use?
FOUNDOLD=
if [ "$FOUND" = "" ] ; then
    for FL in $USERHOME/.x10config /etc/x10.conf ; do
        if [ -e $FL ] ; then
            FOUNDOLD=$FL
            break
        fi
    done
    if [ "$FOUNDOLD" != "" ] ; then
        echo "A Heyu version 1.xx configuration file was found at $FOUNDOLD"
        echo "Would you like to use this configuration file for Heyu version 2 ?"
        echo "(The original will not be changed.)"
        while : ; do
            echo -n "Y or N ? "
            read  CHOICE
            if [ "$CHOICE" = "Y" ] || [ "$CHOICE" = "N" ] ; then
               break
            fi
        done
        if [ "$CHOICE" = "Y" ] ; then
            if [ $FOUNDOLD = $USERHOME/.x10config ] ; then
                if [ ! -d $USERHOME/.heyu ] ; then
                    echo "Creating directory $USERHOME/.heyu"
                    mkdir -p -m 755 $USERHOME/.heyu
                    if [ "$UIDGID" != "" ] ; then
                        chown $UIDGID $USERHOME/.heyu
                    fi
                fi
                FOUND=$USERHOME/.heyu/x10config
            else
                if [ ! -d $SYSBASEDIR ] ; then
                    echo "Creating directory $SYSBASEDIR with permissions rwxrwxrwx."
                    mkdir -p -m 777 $SYSBASEDIR
                    echo "Adjust ownership and permissions as required."
                fi
                FOUND=$SYSBASEDIR/x10.conf
            fi
            echo "Copying $FOUNDOLD to $FOUND"
            cp -p $FOUNDOLD  $FOUND
            echo "(See 'man x10config2' for new features and options)"
        fi
    fi
fi
       
           
if [ "$FOUND" = "" ] ; then
    echo ""
    echo "I did not find a Heyu configuration file."
    echo "Where would you like the sample Heyu configuration file installed?"
    if [ $USERHOME = /root ] || [ $USERHOME = / ] ; then
        echo "  1. In directory $USERHOME/.heyu/  (NOT recommended!)"
        echo "  2. In subdirectory .heyu/ under a user home directory"
    else
        echo "  1. In directory $USERHOME/.heyu/"
        echo "  2. In subdirectory .heyu/ under a different user home directory"
    fi
    echo "  3. In directory $SYSBASEDIR  (for system-wide access)" 
    echo "  4. No thanks, I'll take care of it myself"
    while : ; do
        echo -n "Choice [1, 2, 3, 4] ? " 
        read CHOICE
        if [ "$CHOICE" = "1" ] || [ "$CHOICE" = "2" ] || [ "$CHOICE" = "3" ] || [ "$CHOICE" = "4" ] ; then
           break
        fi
    done

    if [ $CHOICE = 1 ] ; then
        if [ ! -d $USERHOME/.heyu ] ; then
            echo "Creating directory $USERHOME/.heyu"
            mkdir -p -m 755 $USERHOME/.heyu
            if [ "$UIDGID" != "" ] ; then
                chown $UIDGID $USERHOME/.heyu
            fi
        fi
        FOUND=$USERHOME/.heyu/x10config
    elif [ $CHOICE = 2 ] ; then
        while : ; do
           echo -n "Enter a home directory: "
           read USERHOME
           if [ -d $USERHOME ] ; then
               break
           else
              echo "Home directory $USERHOME does not exist!"
           fi
        done
        if [ -f $USERHOME/.heyu/x10config ] ; then
            echo "Configuration file $USERHOME/.heyu/x10config already exists."
            echo "No changes will be made."
        else
            if [ ! -d $USERHOME/.heyu ] ; then
                echo "Creating directory $USERHOME/.heyu with permissions rwxr-xr-x."
                mkdir -p -m 755 $USERHOME/.heyu
                echo "Adjust ownership and permissions as required."
                UIDGID=
            fi
        fi
        FOUND=$USERHOME/.heyu/x10config
    elif [ $CHOICE = 3 ] ; then
        if [ ! -d $SYSBASEDIR ] ; then
           echo "Creating directory $SYSBASEDIR with permissions rwxrwxrwx."
           mkdir -p -m 777 $SYSBASEDIR
           echo "Adjust ownership and permissions as required."
           UIDGID=
        fi
        FOUND=$SYSBASEDIR/x10.conf
    else
        FOUND=
    fi
fi           
       
if [ "$FOUND" != "" ] && [ ! -f $FOUND ]  ; then
        echo "The sample configuration file will be installed as $FOUND"
        echo ""
	echo "I will add the TTY port for your CM11 to the config file"
	while : ; do
	    case `uname -s` in
	    	*inux)
		    echo "Specify /dev/ttyS0, /dev/ttyS1, etc., or the word dummy"
		    ;;
                *penbsd)
                    echo "Specify /dev/tty00, /dev/tty01, etc., or the word dummy"
                    ;;
		*unos)
		    echo "Specify /dev/term/a, /dev/term/b, etc., or the word dummy"
		    ;;
		*)
		    echo "Specify the pathname to the device, e.g., /dev/cua0, or the word dummy"
		    ;;
	    esac
	    echo -n "To which port is the CM11 attached? "
	    read WHERE
            if [ "$WHERE" = "dummy" ] ; then
                TTY=$WHERE
                break
	    elif [ "$WHERE" != "" ]  ; then
		if [ -e $WHERE ] ; then
		    TTY=$WHERE
		    break
		fi
		echo "I could not find the device you specified. Please try again."
	    fi
	done

#	sed "s;^TTY.*;TTY	$TTY;" x10config.sample > $FOUND
	grep -v "TTY_" x10config.sample | sed "s;^TTY.*;TTY	$TTY;"  > $FOUND

        if [ "$UIDGID" != "" ] ; then
            echo "Setting uid:gid = $UIDGID for $FOUND"
            chown $UIDGID $FOUND
        fi
        chmod 666 $FOUND

fi

if [ "$FOUND" != "" ] ; then
#   eval `sed -n "s/^TTY[ 	]*/TTY=/p" $FOUND`
    eval `grep -v "TTY_" $FOUND | sed -n "s/^TTY[ 	]*/TTY=/p"`
fi

if [ "$FOUND" != "" ] && [ "$TTY" != "dummy" ] ; then
    #Check TTY permisions
    set `ls -l $TTY` none
    if [ $1 = "none" ] ; then
        echo "fatal error:  The TTY device $TTY can not be located"
        exit
    fi

    if [ "$1" != "crwxrwxrwx" ] ; then
        if [ "$ME" != root ] ; then
            echo "If you want users other than root to be able to run HEYU, "
            echo "you'll have to log in as root and run the command \"chmod 777 $TTY\""
        else
            echo "Changing TTY permissions to 777"
            chmod 777 $TTY
        fi
    else
        echo "The TTY permissions are OK."
    fi
fi

# Directories: spool and lock 
# get the lockdir and spooldir compile options by using the command 'heyu list'
eval `./heyu list`
if [ "$SPOOLDIR" = "" ] ; then
    echo "I could not determine the spool directory.  Please make sure it "
    echo "exists and is set to mode 777"
else
    if [ ! -d $SPOOLDIR ] ; then
        if [ "$ME" = root ] ; then
            mkdir -p $SPOOLDIR
            chmod 777 $SPOOLDIR
	    echo "The directory $SPOOLDIR was created with the permissions 777."
	else
	    echo "Please log in as root and create the directory $SPOOLDIR with"
	    echo "the permissions 777."
	fi

    fi
    set `ls -lad $SPOOLDIR` none
    if [ $1 != drwxrwxrwx ] ; then
	if [ "$ME" = root ] ; then
	    chmod 777 $SPOOLDIR
	    echo "The permissions for the directory $SPOOLDIR were set to 777"
	else
	    echo "Please log in as root and run the command \"chmod 777 $SPOOLDIR\""
	fi
    else
        echo "The permissions for the SPOOL directory ($SPOOLDIR) are OK"
    fi
fi

# Now we do it all over again for the LOCKDIR

if [ "$LOCKDIR" = "" ] ; then
    echo "I could not determine the lock directory.  Please make sure it "
    echo "exists and is set to mode 1777"
else
    if [ ! -d $LOCKDIR ] ; then
        if [ "$ME" = root ] ; then
            mkdir $LOCKDIR
            chmod 1777 $LOCKDIR
	    echo "The directory $LOCKDIR was created with the permissions 1777."
	else
	    echo "Please log in as root and create the directory $LOCKDIR with"
	    echo "the permissions 1777."
	fi

    fi
    set `ls -lad $LOCKDIR` none
    if [ $1 != drwxrwxrwt ] ; then
	if [ "$ME" = root ] ; then
	    chmod 1777 $LOCKDIR
	    echo "The permissions for the directory $LOCKDIR were set to 1777"
	else
	    echo "Please log in as root and run the command \"chmod 1777 $LOCKDIR\""
	fi
    else
        echo "The permissions for the LOCK directory ($LOCKDIR) are OK"
    fi
fi

