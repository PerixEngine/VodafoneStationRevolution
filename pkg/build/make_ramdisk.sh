#!/bin/bash -e

make_node_rm()
{
    rm -f $1
    $JMKE_BUILDDIR/pkg/build/exec_as_root mknod $@
}
JMKE_MKNOD=make_node_rm

exec_alert()
{
    if ! $@
    then
        echo "failed executing: $@"
	exit_build -1
    fi
}

exec_alert_root()
{
    exec_alert $JMKE_BUILDDIR/pkg/build/exec_as_root $@ 
}

exit_build()
{
    exit $1
}

cp_mkdir()
{
    local src=$1
    local dst=$2
    local dir=`dirname $dst`

    mkdir -p $dir
    cp -f $src $dst
}

if [ "$TARGET_ENDIANESS" = "BIG" ] ; then
    TARGET_ENDIANESS_L=b
else
    TARGET_ENDIANESS_L=l
fi

# Create everything which is not created somewhere else
cd $JMKE_RAMDISK_RW_DIR

if [ "y" = "$CONFIG_RG_RGLOADER" ] ; then
    REDUCED_DEVICE_SET="y"
fi    

if [ -z $REDUCED_DEVICE_SET ] || [ -n $CONFIG_RG_UML ] ; then
$JMKE_MKDIR proc
$JMKE_MKDIR etc
$JMKE_MKDIR sys
fi
if [ -z $REDUCED_DEVICE_SET ] ; then
$JMKE_MKDIR var/state/dhcp
$JMKE_MKDIR var/log
$JMKE_MKDIR var/lock/subsys
$JMKE_MKDIR var/run
$JMKE_MKDIR var/tmp
$JMKE_MKDIR var/flash/
$JMKE_MKDIR var/lib/misc/
$JMKE_MKDIR var/dev/
$JMKE_MKDIR var/cache/
$JMKE_MKDIR var/sys/
$JMKE_MKDIR var/proc/
fi
$JMKE_MKDIR tmp
$JMKE_CHMOD 777 tmp
$JMKE_MKDIR tmp/modules_dir

if [ "y" = "$CONFIG_RG_HA_X10" ] ; then
  $JMKE_MKDIR var/tmp/heyu
fi

$JMKE_MKDIR mnt/permst_disk
$JMKE_MKDIR mnt/flash
$JMKE_MKDIR mnt/mainfs
$JMKE_MKDIR mnt/dlmfs

if [ "y" = "$CONFIG_RG_FFS" ] ; then 	 
    $JMKE_MKDIR ./$CONFIG_RG_FFS_MNT_DIR 	 
fi

if [ -n "$CONFIG_JFFS2_FS" ] ; then
    if [ "$CONFIG_JFFS2_FS" != "n" ] ; then
	if [ "$CONFIG_RG_DOCSIS" = "y" ] ; then
	    $JMKE_LN mnt/jffs2 nvram
	fi
    fi
fi

if [ "$CONFIG_RG_DOCSIS" = "y" ] ; then
  $JMKE_MKDIR usr
  $JMKE_LN /bin usr/sbin
  $JMKE_LN /bin sbin
  $JMKE_CHMOD +x $MAINFS_DIR/etc/scripts/*.sh
fi

if [ "$CONFIG_RG_VODAFONE_MT_SHELL" = "y" ] ; then
  $JMKE_MKDIR ./$CONFIG_RG_VODAFONE_MT_DIR_NAME
fi 

$JMKE_MKDIR $JMKE_RAMDISK_DEV_DIR
cd $JMKE_RAMDISK_DEV_DIR

if [ "y" = "$CONFIG_HW_ST_20190" ] ; then
$JMKE_MKNOD ctrle c 120 0
fi

if [ -z $REDUCED_DEVICE_SET ] ; then
    for i in 0 1 2 3 4 5 6 7 8 9
    do
	$JMKE_MKNOD ttyUSB${i} c 188 ${i}
    done
fi
if [ -n "$CONFIG_USB_PRINTER" ] ; then
    for i in 0 1 2 3 4 5 6 7 8 9
    do
	$JMKE_MKNOD usblp$i c 180 $i
    done
fi

$JMKE_MKNOD video0 c 81 0

$JMKE_MKNOD random c 1 8
$JMKE_MKNOD urandom c 1 9
$JMKE_MKNOD loop0 b 7 0
$JMKE_MKNOD loop1 b 7 1

if [ "y" = "$CONFIG_HW_UML_LOOP_STORAGE" ] ; then
# uml disk emulation are implemented as loop devices, named lda..ldz,
# starting with minor 4, to spare some loop devices for other purposes.
    LOOPDISK_START_MINOR=4
    minor=$LOOPDISK_START_MINOR
    for i in a b c d e f g h i j k l m n o p q r s t u v w x y z \
	aa ab ac ad ae af
    do
	$JMKE_MKNOD ld$i b 7 $minor
	minor=$((minor+1))  
    done
fi

$JMKE_MKNOD fb0 c 29 0

if [ -z $REDUCED_DEVICE_SET ] ; then
$JMKE_MKNOD cua0 c 5 64
$JMKE_MKNOD fb1 c 29 32
$JMKE_MKNOD fb2 c 29 64
$JMKE_MKNOD fb3 c 29 96
$JMKE_MKNOD fd0 b 2 0
$JMKE_MKNOD fb0H1440 b 2 28
$JMKE_MKNOD full c 1 7
$JMKE_MKNOD hda b 3 0
$JMKE_MKNOD hda1 b 3 1
$JMKE_MKNOD hda2 b 3 2
$JMKE_MKNOD hda3 b 3 3
$JMKE_MKNOD hda4 b 3 4
$JMKE_MKNOD hda5 b 3 5
$JMKE_MKNOD hda6 b 3 6
$JMKE_MKNOD hda7 b 3 7
$JMKE_MKNOD hda8 b 3 8
$JMKE_MKNOD hdb b 3 64
$JMKE_MKNOD hdb1 b 3 65
$JMKE_MKNOD hdb2 b 3 66
$JMKE_MKNOD hdb3 b 3 67
$JMKE_MKNOD hdb4 b 3 68
$JMKE_MKNOD hdb5 b 3 69
$JMKE_MKNOD hdb6 b 3 70
$JMKE_MKNOD hdb7 b 3 71
$JMKE_MKNOD hdb8 b 3 72
$JMKE_MKNOD hdc b 22 0
$JMKE_MKNOD hdc1 b 22 1
$JMKE_MKNOD hdc2 b 22 2
$JMKE_MKNOD hdc3 b 22 3
$JMKE_MKNOD hdc4 b 22 4
$JMKE_MKNOD hdc5 b 22 5
$JMKE_MKNOD hdc6 b 22 6
$JMKE_MKNOD hdc7 b 22 7
$JMKE_MKNOD hdc8 b 22 8
$JMKE_MKNOD hdd b 22 64
$JMKE_MKNOD hdd1 b 22 65
$JMKE_MKNOD hdd2 b 22 66
$JMKE_MKNOD hdd3 b 22 67
$JMKE_MKNOD hdd4 b 22 68
$JMKE_MKNOD hdd5 b 22 69
$JMKE_MKNOD hdd6 b 22 70
$JMKE_MKNOD hdd7 b 22 71
$JMKE_MKNOD hdd8 b 22 72
fi
$JMKE_MKNOD kmem c 1 2
$JMKE_MKNOD mem c 1 1
$JMKE_MKNOD null c 1 3
$JMKE_MKNOD port c 1 4 
$JMKE_MKNOD ptyp0 c 2 0
$JMKE_MKNOD ptyp1 c 2 1
$JMKE_MKNOD ptyp2 c 2 2
$JMKE_MKNOD ptyp3 c 2 3
$JMKE_MKNOD ptyp4 c 2 4
$JMKE_MKNOD ptyp5 c 2 5
$JMKE_MKNOD ptyp6 c 2 6
$JMKE_MKNOD ptyp7 c 2 7
if [ -z $REDUCED_DEVICE_SET ] ; then
$JMKE_MKNOD ptyp8 c 2 8
$JMKE_MKNOD ptyp9 c 2 9
$JMKE_MKNOD ptypa c 2 10
$JMKE_MKNOD ptypb c 2 11
$JMKE_MKNOD ptypc c 2 12
$JMKE_MKNOD ptypd c 2 13
$JMKE_MKNOD ptype c 2 14
$JMKE_MKNOD ptypf c 2 15
$JMKE_MKNOD ptyq0 c 2 16
$JMKE_MKNOD ptyq1 c 2 17
$JMKE_MKNOD ptyq2 c 2 18
$JMKE_MKNOD ptyq3 c 2 19
$JMKE_MKNOD ptyq4 c 2 20
$JMKE_MKNOD ptyq5 c 2 21
$JMKE_MKNOD ptyq6 c 2 22
$JMKE_MKNOD ptyq7 c 2 23
$JMKE_MKNOD ptyq8 c 2 24
$JMKE_MKNOD ptyq9 c 2 25
$JMKE_MKNOD ptyqa c 2 26
$JMKE_MKNOD ptyqb c 2 27
$JMKE_MKNOD ptyqc c 2 28
$JMKE_MKNOD ptyqd c 2 29
$JMKE_MKNOD ptyqe c 2 30
$JMKE_MKNOD ptyqf c 2 31
$JMKE_MKNOD ptyr0 c 2 32
$JMKE_MKNOD ptyr1 c 2 33
$JMKE_MKNOD ptyr2 c 2 34
$JMKE_MKNOD ptyr3 c 2 35
$JMKE_MKNOD ptyr4 c 2 36
$JMKE_MKNOD ptyr5 c 2 37
$JMKE_MKNOD ptyr6 c 2 38
$JMKE_MKNOD ptyr7 c 2 39
$JMKE_MKNOD ptyr8 c 2 40
$JMKE_MKNOD ptyr9 c 2 41
$JMKE_MKNOD ptyra c 2 42
$JMKE_MKNOD ptyrb c 2 43
$JMKE_MKNOD ptyrc c 2 44
$JMKE_MKNOD ptyrd c 2 45
$JMKE_MKNOD ptyre c 2 46
$JMKE_MKNOD ptyrf c 2 47
$JMKE_MKNOD ptys0 c 2 48
$JMKE_MKNOD ptys1 c 2 49
$JMKE_MKNOD ptys2 c 2 50
$JMKE_MKNOD ptys3 c 2 51
$JMKE_MKNOD ptys4 c 2 52
$JMKE_MKNOD ptys5 c 2 53
$JMKE_MKNOD ptys6 c 2 54
$JMKE_MKNOD ptys7 c 2 55
$JMKE_MKNOD ptys8 c 2 56
$JMKE_MKNOD ptys9 c 2 57
$JMKE_MKNOD ptysa c 2 58
$JMKE_MKNOD ptysb c 2 59
$JMKE_MKNOD ptysc c 2 60
$JMKE_MKNOD ptysd c 2 61
$JMKE_MKNOD ptyse c 2 62
$JMKE_MKNOD ptysf c 2 63
$JMKE_MKNOD rtc c 10 135
$JMKE_MKNOD tpanel c 10 11
fi
$JMKE_MKNOD ram0 b 1 0
$JMKE_MKNOD ram1 b 1 1
$JMKE_MKNOD ram2 b 1 2
$JMKE_MKNOD ram3 b 1 3
$JMKE_MKNOD nvram c 10 144
$JMKE_MKNOD tty c 5 0 
$JMKE_MKNOD tty0 c 4 0
$JMKE_MKNOD tty1 c 4 1
$JMKE_MKNOD tty2 c 4 2
if [ -z $REDUCED_DEVICE_SET ] ; then
$JMKE_MKNOD tty3 c 4 3
$JMKE_MKNOD tty4 c 4 4
$JMKE_MKNOD tty5 c 4 5
$JMKE_MKNOD tty6 c 4 6
$JMKE_MKNOD tty7 c 4 7
$JMKE_MKNOD tty8 c 4 8
fi
$JMKE_MKNOD ttyS0 c 4 64
$JMKE_MKNOD ttyS1 c 4 65
$JMKE_MKNOD ttyp0 c 3 0
$JMKE_MKNOD ttyp1 c 3 1
$JMKE_MKNOD ttyp2 c 3 2
if [ -z $REDUCED_DEVICE_SET ] ; then
$JMKE_MKNOD ttyp3 c 3 3
$JMKE_MKNOD ttyp4 c 3 4
$JMKE_MKNOD ttyp5 c 3 5
$JMKE_MKNOD ttyp6 c 3 6
$JMKE_MKNOD ttyp7 c 3 7
$JMKE_MKNOD ttyp8 c 3 8
$JMKE_MKNOD ttyp9 c 3 9
$JMKE_MKNOD ttypa c 3 10
$JMKE_MKNOD ttypb c 3 11
$JMKE_MKNOD ttypc c 3 12
$JMKE_MKNOD ttypd c 3 13
$JMKE_MKNOD ttype c 3 14
$JMKE_MKNOD ttypf c 3 15
$JMKE_MKNOD ttyq0 c 3 16
$JMKE_MKNOD ttyq1 c 3 17
$JMKE_MKNOD ttyq2 c 3 18
$JMKE_MKNOD ttyq3 c 3 19
$JMKE_MKNOD ttyq4 c 3 20
$JMKE_MKNOD ttyq5 c 3 21
$JMKE_MKNOD ttyq6 c 3 22
$JMKE_MKNOD ttyq7 c 3 23
$JMKE_MKNOD ttyq8 c 3 24
$JMKE_MKNOD ttyq9 c 3 25
$JMKE_MKNOD ttyqa c 3 26
$JMKE_MKNOD ttyqb c 3 27
$JMKE_MKNOD ttyqc c 3 28
$JMKE_MKNOD ttyqd c 3 29
$JMKE_MKNOD ttyqe c 3 30
$JMKE_MKNOD ttyqf c 3 31
$JMKE_MKNOD ttyr0 c 3 32
$JMKE_MKNOD ttyr1 c 3 33
$JMKE_MKNOD ttyr2 c 3 34
$JMKE_MKNOD ttyr3 c 3 35
$JMKE_MKNOD ttyr4 c 3 36
$JMKE_MKNOD ttyr5 c 3 37
$JMKE_MKNOD ttyr6 c 3 38
$JMKE_MKNOD ttyr7 c 3 39
$JMKE_MKNOD ttyr8 c 3 40
$JMKE_MKNOD ttyr9 c 3 41
$JMKE_MKNOD ttyra c 3 42
$JMKE_MKNOD ttyrb c 3 43
$JMKE_MKNOD ttyrc c 3 44
$JMKE_MKNOD ttyrd c 3 45
$JMKE_MKNOD ttyre c 3 46
$JMKE_MKNOD ttyrf c 3 47
$JMKE_MKNOD ttys0 c 3 48
$JMKE_MKNOD ttys1 c 3 49
$JMKE_MKNOD ttys2 c 3 50
$JMKE_MKNOD ttys3 c 3 51
$JMKE_MKNOD ttys4 c 3 52
$JMKE_MKNOD ttys5 c 3 53
$JMKE_MKNOD ttys6 c 3 54
$JMKE_MKNOD ttys7 c 3 55
$JMKE_MKNOD ttys8 c 3 56
$JMKE_MKNOD ttys9 c 3 57
$JMKE_MKNOD ttysa c 3 58
$JMKE_MKNOD ttysb c 3 59
$JMKE_MKNOD ttysc c 3 60
$JMKE_MKNOD ttysd c 3 61
$JMKE_MKNOD ttyse c 3 62
$JMKE_MKNOD ttysf c 3 63
$JMKE_MKNOD vscisdn c 61 0
$JMKE_MKNOD vsctdm c 60 0
fi
$JMKE_MKNOD zero c 1 5
$JMKE_MKNOD chardev c 254 0

#MTD (Standard flash support)
if [ "y" = "$CONFIG_MTD" ] ; then
$JMKE_MKNOD mtd0 c 90 0
$JMKE_MKNOD mtdr0 c 90 1
$JMKE_MKNOD mtd1 c 90 2
$JMKE_MKNOD mtdr1 c 90 3
$JMKE_MKNOD mtd2 c 90 4
$JMKE_MKNOD mtdr2 c 90 5
$JMKE_MKNOD mtd3 c 90 6
$JMKE_MKNOD mtdr3 c 90 7
$JMKE_MKNOD mtd4 c 90 8
$JMKE_MKNOD mtdr4 c 90 9
$JMKE_MKNOD mtd5 c 90 10
$JMKE_MKNOD mtdr5 c 90 11
$JMKE_MKNOD mtd6 c 90 12
$JMKE_MKNOD mtdr6 c 90 13
$JMKE_MKNOD mtd7 c 90 14
$JMKE_MKNOD mtdr7 c 90 15
$JMKE_MKNOD mtdblock0 b 31 0
$JMKE_MKNOD mtdblock1 b 31 1
$JMKE_MKNOD mtdblock2 b 31 2
$JMKE_MKNOD mtdblock3 b 31 3
$JMKE_MKNOD mtdblock4 b 31 4
$JMKE_MKNOD mtdblock5 b 31 5
$JMKE_MKNOD mtdblock6 b 31 6
$JMKE_MKNOD mtdblock7 b 31 7
$JMKE_MKNOD mtdblock8 b 31 8
fi
if [ -n "$CONFIG_MTD_UBI" ] ; then
  $JMKE_MKNOD ubi_ctrl c 10 240
  $JMKE_MKNOD ubi0 c 238 0
  $JMKE_MKNOD ubi1 c 239 0
  $JMKE_MKNOD ubi0_0 c 238 1
  $JMKE_MKNOD ubi0_1 c 238 2
  $JMKE_MKNOD ubi0_2 c 238 3
  $JMKE_MKNOD ubi0_3 c 238 4
  $JMKE_MKNOD ubi1_0 c 239 1
  $JMKE_MKNOD ubi1_1 c 239 2
  $JMKE_MKNOD ubi1_2 c 239 3
  $JMKE_MKNOD ubi1_3 c 239 4
fi

if [ -n "$CONFIG_RG_HW_WATCHDOG_ARCH_LINUX_GENERIC" ] ; then
  $JMKE_MKNOD watchdog c 10 130
fi

#Mindspeed needed node
#MSP
if [ "$CONFIG_COMCERTO_VED" = "y" ] ; then
  $JMKE_MKNOD msp c 237 0
fi
if [ "$CONFIG_COMCERTO_VED_M821" = "y" ] ; then
  $JMKE_MKNOD msp c 237 0
fi
if [ "$CONFIG_COMCERTO_FPP_LOADER" = "y" ] ; then
  $JMKE_MKNOD fpp c 236 0
fi

#Matisse
if [ "$CONFIG_COMCERTO_MATISSE" = "y" ] ; then
  $JMKE_MKNOD dci0 c 121 0
  $JMKE_MKNOD dci1 c 121 1
  $JMKE_MKNOD m828xx0 c 244 0
  $JMKE_MKNOD sti c 242 0
fi

if [ -n "$CONFIG_CHR_DEV_SG" ] ; then
  for i in `seq 0 9`; do $JMKE_MKNOD sg$i c 21 $i; done
fi

#SCSI disks (required for firewire-ieee1394 and USB)
if [ "y" = "$CONFIG_BLK_DEV_SD" -o "m" = "$CONFIG_BLK_DEV_SD" ] ; then
    m=8
    n=0
    for i in a b c d e f g h i j k l m n o p q r s t u v w x y z \
	aa ab ac ad ae af
    do
	for j in '' 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	do
	    $JMKE_MKNOD sd$i$j b $m $n
	    n=$((n+1))
	    if [ $n -gt 255 ]
	    then
		n=0
		m=65
	    fi
	done
    done
fi

# Create 2 block device inodes for 2 md devices
if [ -n "$CONFIG_BLK_DEV_MD" ] ; then
  $JMKE_MKNOD md0 b 9 0
  $JMKE_MKNOD md1 b 9 1
fi

if [ -n "$CONFIG_RG_UML" ] ; then
  $JMKE_MKNOD ubd0 b 98 0
  $JMKE_MKNOD ubd1 b 98 1
fi

if [ "" != "$CONFIG_VINETIC" ] ; then
  $JMKE_MKNOD vmmc10 c 246 10
  $JMKE_MKNOD vmmc11 c 246 11
  $JMKE_MKNOD vmmc12 c 246 12
  $JMKE_MKNOD vmmc13 c 246 13
  $JMKE_MKNOD vmmc14 c 246 14
  $JMKE_MKNOD vmmc15 c 246 15
  $JMKE_MKNOD vmmc16 c 246 16
  $JMKE_MKNOD vmmc17 c 246 17
  $JMKE_MKNOD vmmc18 c 246 18
fi

if [ "" != "$CONFIG_DUSLIC" ] ; then
  $JMKE_MKNOD dus10 c 229 10
  $JMKE_MKNOD dus11 c 229 11
  $JMKE_MKNOD dus12 c 229 12
fi

if [ "" != "$CONFIG_ZSP400" ] ; then
  $JMKE_MKNOD vp0 c 120 0
  $JMKE_MKNOD vp1 c 120 1
  $JMKE_MKNOD vp2 c 120 2
  $JMKE_MKNOD vp3 c 120 3
fi

if [ "" != "$CONFIG_RG_CONSOLE_TAP" ] ; then
  $JMKE_MKNOD console_tap c 5 53
fi

if ! [ -e $JMKE_RAMDISK_DEV_DIR/console ] ; then
  if [ $CONFIG_RG_CONSOLE_DEVICE = "console" -o "" != "$CONFIG_RG_KLOG" ] ; then
    $JMKE_MKNOD console c 5 1
  else
    $JMKE_LN /dev/$CONFIG_RG_CONSOLE_DEVICE $JMKE_RAMDISK_DEV_DIR/console
  fi
fi

if [ "" != "$CONFIG_I2C_CHARDEV" ] ; then
  $JMKE_MKDIR i2c
  $JMKE_MKNOD i2c/0 c 89 0
  $JMKE_MKNOD i2c/1 c 89 1
  $JMKE_LN i2c/0 i2c-0
  $JMKE_LN i2c/1 i2c-1
fi

if [ -n "$CONFIG_TUN" ] ; then
  if ! [ -e $JMKE_RAMDISK_DEV_DIR/net ] ; then
    $JMKE_MKDIR net
  fi
  $JMKE_MKNOD net/tun c 10 200
fi

cd $JMKE_RAMDISK_RW_DIR

find dev/ -not -type l -and -not -type d | xargs $JMKE_CHMOD 666
$JMKE_CHMOD 620 dev/ttyp*

# Let the usual mechanism create /home as file links to mainfs, but here
# override with a link to the real directory to save inodes on the writable
# root file system.
exec_alert_root rm -rf home
$JMKE_LN /mnt/mainfs/home home

cd $DISK_IMAGE_DIR

LIBS=""

if [ "y" = "$LIBC_IN_TOOLCHAIN" ] ; then
    if [ "y" = "$CONFIG_GLIBC" ] ; then
	LIBS="libc.so.6 libdl.so.2 libutil.so.1 libresolv.so.2 \
	    libcrypt.so.1 libnss_files.so.2 libnss_dns.so.2 libnsl.so.1 \
	    libm.so.6"
	
	if [ "y" = "$CONFIG_CAVIUM_OCTEON" ] ; then
	    LIBS="ld.so.1 "$LIBS
	else
	    LIBS="ld-linux.so.2 "$LIBS
	fi	
	
	if [ "y" = "$CONFIG_RG_CXX" ] ; then
	    LIBS="libstdc++.so.5 "$LIBS
	fi

	mkdir -p $JMKE_RAMDISK_RW_DIR/$TOOLS_PREFIX
	$JMKE_LN / $JMKE_RAMDISK_RW_DIR/$TOOLS_PREFIX/$TARGET_MACHINE
    fi
    
    if [ "y" = "$CONFIG_ULIBC" ] ; then
	LIBS="ld-uClibc.so.0 libc.so.0 libcrypt.so.0 libdl.so.0 libm.so.0 \
	    libnsl.so.0 libresolv.so.0 librt.so.0 libutil.so.0"
    fi
	
    if [ "y" = "$CONFIG_RG_THREADS" ] ; then
        LIBS="libpthread.so.0 libthread_db.so.1 "$LIBS
    fi
    
    if [ "y" = "$CONFIG_64BIT" ] ; then
        $JMKE_LN lib $JMKE_RAMDISK_RW_DIR/lib64
    fi
else
    if [ "y" = "$CONFIG_RG_CXX" ] ; then
        LIBS="libgcc_s.so.1 "$LIBS
    fi
fi

mkdir -p $JMKE_RAMDISK_RW_DIR/lib
LIBC_BUILD=$JMKE_BUILDDIR/pkg/build/lib/
for lib in $LIBS
do
    lib_base=`basename $lib`
    cp_mkdir $LIBC_BUILD/$lib $MAINFS_DIR/lib/$lib
    $JMKE_LN /mnt/mainfs/lib/$lib_base $JMKE_RAMDISK_RW_DIR/lib/
done

if [ "y" = "$CONFIG_INSURE" ] ; then
    for lib in libinsure.so libtql_c_gcc.so libdlsym.so ; do
	cp -f /usr/local/parasoft/lib.linux2/$lib $MAINFS_DIR/lib/
	$JMKE_LN /mnt/mainfs/lib/`basename $lib` $JMKE_RAMDISK_RW_DIR/lib/
    done

    mkdir -p $JMKE_RAMDISK_RW_DIR/usr/local/parasoft/
    $JMKE_LN  /lib $JMKE_RAMDISK_RW_DIR/usr/local/parasoft/lib.linux2
    cp -f $JMKE_BUILDDIR/pkg/build/pkg/build/psrc $JMKE_RAMDISK_RW_DIR/usr/local/parasoft/.psrc
fi

if [ "y" = "$CONFIG_CRAMFS_DYN_BLOCKSIZE" ] ; then
    CRAMFS_BLKSZ=$CONFIG_CRAMFS_BLKSZ
else
    CRAMFS_BLKSZ=4096
fi

# change uid & gid from current user on the host machine to a valid OpenRG user
fix_user_and_group()
{
     exec_alert_root chown -h -R 0:0 "$@"

     # enable the current non-root user to override these files later (by 
     # running another 'make', 'make distclean' etc.
     $JMKE_CHMOD a+w -R "$@" 
}

exec_alert_root cp -a --remove-destination $JMKE_RAMDISK_RW_DIR/* $RAMDISK_MOUNT_POINT

if [ "y" = "$CONFIG_SIMPLE_RAMDISK" ] ; then
    # Copy mainfs onto the ramdisk image...
    exec_alert_root cp -af $MAINFS_DIR/* $RAMDISK_MOUNT_POINT/mnt/mainfs
fi

if [ "y" = "$CONFIG_RG_KERNEL_NEEDED_SYMBOLS" ] ; then
    # Get undefined symbols from all modules
    find $MAINFS_DIR/lib/modules $MODULES_DIR/lib/modules -type f | \
	xargs --no-run-if-empty -n 1 $NM --undefined-only | \
	sed -e 's/^[ \t]*U //' | sort -u > $NEEDED_SYMBOLS
fi

if [ "y" = "$CONFIG_RG_MODULES_INITRAMFS" -a -d $MODULES_DIR ] ; then
    exec_alert_root cp -af $MODULES_DIR/* $RAMDISK_MOUNT_POINT/tmp/modules_dir
fi

if [ "y" = "$CONFIG_RG_DLM" -a "y" = "$CONFIG_CRAMFS" ] ; then
  fix_user_and_group $DLM_TARGET_DIR
  $JMKE_BUILDDIR/pkg/cramfs/mkcramfs -E -b $CRAMFS_BLKSZ \
    -c none $DLM_TARGET_DIR $DISK_IMAGE_DIR/dlm.img
fi

if [ -z "$CONFIG_RG_INITFS_RAMFS" ] ; then
    exec_alert_root chown -h -R 0:0 $RAMDISK_MOUNT_POINT
fi	

fix_user_and_group $MAINFS_DIR
if [ "cramfs" = "$CONFIG_RG_MAINFS_TYPE" ] ; then
  $JMKE_BUILDDIR/pkg/cramfs/mkcramfs -E -b $CRAMFS_BLKSZ \
    -c $CONFIG_RG_CRAMFS_COMP_METHOD $MAINFS_DIR \
    $COMPRESSED_DISK
fi

if [ "squashfs" = "$CONFIG_RG_MAINFS_TYPE" ] ; then
  mksquashfs $MAINFS_DIR $COMPRESSED_DISK \
    -comp $CONFIG_RG_SQUASHFS_COMP_METHOD -no-xattrs 
fi

if [ "y" = "$CONFIG_RG_MAINFS_BUILTIN" ] ; then
  cp_mkdir $COMPRESSED_DISK $RAMDISK_MOUNT_POINT
else
  $JMKE_LN /initrd.image $RAMDISK_MOUNT_POINT/mainfs.img
fi

if [ "$CONFIG_BINFMT_FLAT" = "y" ] ; then
    EXECUTABLES_TYPE="flat"
fi

if [ -n "$CONFIG_RG_UML" ] ; then
  if [ -z "$CONFIG_RG_INITFS_RAMFS" ] ; then
    # XXX DEVELOPMENT, for now we use a ubd device and not ramdisk for UML
    gunzip -c $COMPRESSED_DISK > $JMKE_BUILDDIR/os/linux/fs.img
  else
    touch $JMKE_BUILDDIR/os/linux/fs.img
  fi
fi
