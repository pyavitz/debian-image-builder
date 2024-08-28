#!/bin/bash
# Flash android recovery

if [ "$USER" != "root" ]; then
        echo "Please run this as root or with sudo privileges."
        exit 1
fi

if [[ -f "/etc/opt/board.txt" ]]; then
	. /etc/opt/board.txt
else
	echo "This script is not supported."
	exit 0
fi
if [[ -f "/usr/lib/u-boot/vendor-u-boot.bin" ]]; then
	V_UBOOT="/usr/lib/u-boot/vendor-u-boot.bin"
else
	echo "No recovery binary available."
	exit 1
fi
if [[ `lsblk | grep boot0` ]]; then
	MMC=`ls /dev/mmcblk*boot0 | sed 's/boot0//g'`
else
	echo "Did not detect an eMMC."
	exit 1
fi

if [[ "$FAMILY_EXT" == "ac2xx" ]] && [[ -f "/usr/lib/u-boot/vendor-u-boot.bin" ]]; then
	echo -e ""
	echo -e "Flashing recovery binary ..."
	dd if="${V_UBOOT}" of="${MMC}" conv=fsync bs=1 count=442
	dd if="${V_UBOOT}" of="${MMC}" conv=fsync bs=512 skip=1 seek=1
	ROOTFS=`findmnt -v -n -o SOURCE /`
	PARTUUID=$(blkid -o export -- $ROOTFS | sed -ne 's/^PARTUUID=//p')
	if [[ -f "/boot/extlinux/extlinux.conf" ]]; then
		sed -i "s,root=PARTUUID=[^ ]*,root=PARTUUID=${PARTUUID}," /boot/extlinux/extlinux.conf
	fi
	echo -e "Done."
	echo -e ""
	echo "You may now power down the board and enter recovery mode."
fi

sync
exit 0
