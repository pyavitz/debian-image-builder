#!/bin/bash
# Das U-Boot flashing script
# Depends on prerequisites defined by P. Yavitz
# URL: https://github.com/pyavitz/debian-image-builder

# developer debug switch
VERBOSITY="0"
if [ $VERBOSITY -eq 1 ]; then
	set -x
fi

if [[ -f "/etc/opt/board.txt" ]]; then
	. /etc/opt/board.txt
fi

# unsupported
if [[ "$BOARD" == "odroidc1" ]]; then
	echo -e "The ${DEFAULT_MOTD} is not supported by this script"
	exit 0
fi
if [[ "$PETITBOOT" == "true" ]]; then
	echo -e "Petitboot is not supported by this script"
	exit 0
fi

# check privilege
if [ "$USER" != "root" ]; then
	echo -e "Please run this as root or with sudo privileges."
	exit 1
fi

error_prompt () {
export NEWT_COLORS='root=,black'
whiptail --msgbox "    ${REPORT}" 0 0
exit 0
}

# check for u-boot directory
if [[ -d "/usr/lib/u-boot" ]]; then
	DIR="/usr/lib/u-boot"
else
	REPORT="Could not find u-boot directory."
	error_prompt
fi

# set device node variable
MMC=`findmnt -v -n -o SOURCE / | sed 's/..$//'`

# locate target device node
if [[ -e "${MMC}boot0" ]]; then
	EMMC="1"
else
	EMMC="0"
fi

target_device () {
echo -en "${FAMILY}: " | sed -e 's/\(.*\)/\U\1/'
echo -e "${DEFAULT_MOTD}" | sed -e 's/\(.*\)/\U\1/'
if [ $EMMC -eq 1 ]; then
	echo -en "== eMMC: "
else
	echo -en "== SDCARD: "
fi
echo -e "$MMC"
}

extlinux_conf () {
ROOTFS=`findmnt -v -n -o SOURCE /`
FSTYPE=`findmnt -v -n -o FSTYPE /`
if [[ "$FSTYPE" == "ext4" ]]; then
	ROOT_FSTYPE="rootfstype=ext4"
fi
if [[ "$FSTYPE" == "btrfs" ]]; then
	ROOT_FSTYPE="rootfstype=btrfs rootflags=subvol=@"
fi
if [[ "$FSTYPE" == "xfs" ]]; then
	ROOT_FSTYPE="rootfstype=xfs"
fi
FDT="../${FAMILY}/${DTB}.dtb"
FDTDIR="../${FAMILY}/"
CMDLINE="append earlyprintk ${CONSOLE} rw root=PARTUUID=${NEW_PARTUUID} rootwait ${ROOT_FSTYPE} fsck.repair=yes ${EXTRA} loglevel=1 init=/sbin/init"
rm -f /boot/extlinux/extlinux.conf
tee /boot/extlinux/extlinux.conf <<EOF
label default
	kernel ../Image
	initrd ../uInitrd
	fdtdir ${FDTDIR}
	fdt ${FDT}
	#fdtoverlays
	${CMDLINE}
EOF
}

flash_uboot(){
# allwinner
if [[ "$FAMILY" == "allwinner" ]] && [[ -f "${DIR}/u-boot-sunxi-with-spl.bin" ]]; then
	target_device
	sleep .50
	# flash binary
	dd if="${DIR}/u-boot-sunxi-with-spl.bin" of="${MMC}" conv=fsync bs=1024 seek=8
fi

# amlogic / emmc
if [[ "$FAMILY" == "amlogic" ]] && [ $EMMC -eq 1 ] && [[ -f "${DIR}/u-boot.bin" ]]; then
	target_device
	sleep .50
	# flash binary
	dd if="${DIR}/u-boot.bin" of="${MMC}" bs=512 seek=1
fi
# amlogic / sdcard
if [[ "$FAMILY" == "amlogic" ]] && [ $EMMC -eq 0 ] && [[ -f "${DIR}/u-boot.bin.sd.bin" ]]; then
	target_device
	sleep .50
	# flash binary
	dd if="${DIR}/u-boot.bin.sd.bin" of="${MMC}" conv=fsync bs=1 count=442
	dd if="${DIR}/u-boot.bin.sd.bin" of="${MMC}" conv=fsync bs=512 skip=1 seek=1
fi

# freescale
if [[ "$FAMILY" == "freescale" ]] && [[ "$ARCH" == "arm" ]] && [[ -f "${DIR}/sploader.bin" ]] && [[ -f "${DIR}/u-boot.bin" ]]; then
	target_device
	sleep .50
	# flash loader and binary
	dd if="${DIR}/sploader.bin" of="${MMC}" bs=1k seek=1 conv=sync
	dd if="${DIR}/u-boot.bin" of="${MMC}" bs=1k seek=69 conv=sync
fi
if [[ "$FAMILY" == "freescale" ]] && [[ "$ARCH" == "arm64" ]] && [[ -f "${DIR}/u-boot.bin" ]]; then
	target_device
	sleep .50
	# flash binary
	dd if="${DIR}/u-boot.bin" of="${MMC}" bs=1k seek=33
fi

# rockchip
if [[ "$FAMILY" == "rockchip" ]] && [[ -f "${DIR}/idbloader.bin" ]] && [[ -f "${DIR}/u-boot.itb" ]]; then
	target_device
	sleep .50
	# flash loader and binary
	dd if="${DIR}/idbloader.bin" of="${MMC}" seek=64
	dd if="${DIR}/u-boot.itb" of="${MMC}" seek=16384
fi

# samsung / odroid xu4 / emmc
if [[ "$FAMILY" == "samsung" ]] && [[ "$BOARD" == "odroidxu4" ]]; then
	if [ $EMMC -eq 1 ] && [[ -f "${DIR}/bl1.bin" ]] && [[ -f "${DIR}/bl2.bin" ]] && [[ -f "${DIR}/u-boot.bin" ]] && [[ "${DIR}/tzsw.bin" ]]; then
		DEVICE=`ls /dev/mmcblk*boot0 | sed 's/^.....//'`
		echo 0 > /sys/block/${DEVICE}/force_ro
		target_device
		sleep .50
		# flash binaries
		dd if="${DIR}/bl1.bin" of="/dev/${DEVICE}" seek=0 conv=fsync
		dd if="${DIR}/bl2.bin" of="/dev/${DEVICE}" seek=30 conv=fsync
		dd if="${DIR}/u-boot.bin" of="/dev/${DEVICE}" seek=62 conv=fsync
		dd if="${DIR}/tzsw.bin" of="/dev/${DEVICE}" seek=1502 conv=fsync
		dd if="/dev/zero" of="/dev/${DEVICE}" seek=2015 bs=512 count=32 conv=fsync
	fi
fi
# samsung / odroid xu4 / sdcard
if [[ "$FAMILY" == "samsung" ]] && [[ "$BOARD" == "odroidxu4" ]]; then
	if [ $EMMC -eq 0 ] && [[ -f "${DIR}/bl1.bin" ]] && [[ -f "${DIR}/bl2.bin" ]] && [[ -f "${DIR}/u-boot.bin" ]] && [[ "${DIR}/tzsw.bin" ]]; then
		target_device
		sleep .50
		# flash binaries
		dd if="${DIR}/bl1.bin" of="${MMC}" seek=1 conv=fsync
		dd if="${DIR}/bl2.bin" of="${MMC}" seek=31 conv=fsync
		dd if="${DIR}/u-boot.bin" of="${MMC}" seek=63 conv=fsync
		dd if="${DIR}/tzsw.bin" of="${MMC}" seek=1503 conv=fsync
		dd if="/dev/zero" of="${MMC}" seek=2015 bs=512 count=32 conv=fsync
	fi
fi
}

CUR_PARTUUID=$(blkid -o export -- $ROOTFS | sed -ne 's/^PARTUUID=//p')
CHK_PARTUUID=$(blkid -o export -- $ROOTFS | sed -ne 's/^PARTUUID=//p')
echo OLD_PARTUUID='"'$CUR_PARTUUID'"' > /etc/opt/part-uuid.txt
sleep .25
flash_uboot
sleep .25
echo NEW_PARTUUID='"'$CHK_PARTUUID'"' >> /etc/opt/part-uuid.txt
source /etc/opt/part-uuid.txt
if [[ "$OLD_PARTUUID" == "$NEW_PARTUUID" ]]; then
	rm -f /etc/opt/part-uuid.txt
	echo -e ""
	echo -e "You may now reboot."
	exit 0
else
	echo -e ""
	echo -e "Generating new extlinux.conf"
	extlinux_conf
	rm -f /etc/opt/part-uuid.txt
	echo -e ""
	echo -e "You may now reboot."
	exit 0
fi

exit 0
