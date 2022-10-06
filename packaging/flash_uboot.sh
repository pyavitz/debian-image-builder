#!/bin/bash
# Das U-Boot flashing script
# Depends on prerequisites defined by P. Yavitz
# URL: https://github.com/pyavitz/debian-image-builder

# developer debug switch
VERBOSITY="0"
if [ $VERBOSITY -eq 1 ]; then
	set -x
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

# verify supported device and set device node variable
if [[ -f "/etc/opt/board.txt" ]]; then
	if [[ -d "/usr/lib/u-boot" ]]; then
		DIR="/usr/lib/u-boot"
	else
		REPORT="Could not find u-boot directory."
		error_prompt
	fi
	MMC=`findmnt -v -n -o SOURCE / | sed 's/p1//g' | sed 's/p2//g'`
	. /etc/opt/board.txt
else
	REPORT="Your board is not supported."
	error_prompt
fi

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

# allwinner
if [[ "$FAMILY" == "allwinner" ]] && [[ -f "${DIR}/u-boot-sunxi-with-spl.bin" ]]; then
	target_device
	sleep .50
	# flash binary
	dd if="${DIR}/u-boot-sunxi-with-spl.bin" of="${MMC}" conv=fsync bs=1024 seek=8
	echo -e "You may now reboot."
fi

# amlogic
if [[ "$FAMILY" == "amlogic" ]]; then
	if [ $EMMC -eq 1 ] && [[ -f "${DIR}/u-boot.bin" ]]; then
		target_device
		sleep .50
		# flash binary
		dd if="${DIR}/u-boot.bin" of="${MMC}" bs=512 seek=1
		echo -e "You may now reboot."
	fi
else
	if [ $EMMC -eq 0 ] && [[ -f "${DIR}/u-boot.bin.sd.bin" ]]; then
		target_device
		sleep .50
		# flash binary
		dd if="${DIR}/u-boot.bin.sd.bin" of="${MMC}" conv=fsync bs=1 count=442
		dd if="${DIR}/u-boot.bin.sd.bin" of="${MMC}" conv=fsync bs=512 skip=1 seek=1
		echo -e "You may now reboot."
	fi
fi
	
# freescale
if [[ "$FAMILY" == "freescale" ]] && [[ -f "${DIR}/sploader.bin" ]] && [[ -f "${DIR}/u-boot.bin" ]]; then
	target_device
	sleep .50
	# flash loader and binary
	dd if="${DIR}/sploader.bin" of="${MMC}" bs=1k seek=1 conv=sync
	dd if="${DIR}/u-boot.bin" of="${MMC}" bs=1k seek=69 conv=sync
	echo -e "You may now reboot."
fi

# rockchip
if [[ "$FAMILY" == "rockchip" ]] && [[ -f "${DIR}/idbloader.bin" ]] && [[ -f "${DIR}/u-boot.itb" ]]; then
	target_device
	sleep .50
	# flash loader and binary
	dd if="${DIR}/idbloader.bin" of="${MMC}" seek=64
	dd if="${DIR}/u-boot.itb" of="${MMC}" seek=16384
	echo -e "You may now reboot."
fi

# samsung / odroid xu4
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
else
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

exit 0
