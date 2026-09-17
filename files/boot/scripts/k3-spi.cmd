setenv devnum 0
setenv distro_bootpart 1
setenv devtype usb
setenv loadaddr 0x148000000

echo "Checking execution state..."
if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} flash_done.txt; then
	echo "===================================================="
	echo " MATCH: 'flash_done.txt' detected!"
	echo " SPI flash update already completed. Skipping sequence."
	echo "===================================================="
else
	echo "Probing SPI Flash chip..."
	if sf probe; then
		echo "Flash detected successfully."
		if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} bootinfo_spinor.bin; then
			echo "Flashing bootinfo to 0x0..."
			sf update ${loadaddr} 0x0 ${filesize}
		fi
		if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} FSBL.bin; then
			echo "Flashing FSBL to 0x20000..."
			sf update ${loadaddr} 0x20000 ${filesize}
		fi
		if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} u-boot-env-default.bin; then
			echo "Flashing env to 0xA0000..."
			sf update ${loadaddr} 0xA0000 ${filesize}
		fi
		if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} esos.bin; then
			echo "Flashing esos to 0xB0000..."
			sf update ${loadaddr} 0xB0000 ${filesize}
		fi
		if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} fw_dynamic.bin; then
			echo "Flashing opensbi to 0x1B0000..."
			sf update ${loadaddr} 0x1B0000 ${filesize}
		fi
		if load ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} u-boot.bin; then
			echo "Flashing uboot to 0x210000..."
			sf update ${loadaddr} 0x210000 ${filesize}
		fi
		echo "Writing execution marker..."
		mw.b ${loadaddr} 0x59 4
		if fatwrite ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} flash_done.txt 4; then
			echo "Success: Created FAT marker file."
		elif ext4write ${devtype} ${devnum}:${distro_bootpart} ${loadaddr} flash_done.txt 4; then
			echo "Success: Created EXT4 marker file."
		else
			echo "WARNING: Filesystem write-support disabled in U-Boot."
			echo "Please manually remove or rename boot.scr to prevent loops."
			sleep 3
		fi
		echo "------------------------------------------------------------------"
		echo "Verified flashing sequence complete! Resetting."
		echo "------------------------------------------------------------------"
		sleep 2
		reset
	else
		echo "ERROR: SPI Flash not found. Halting sequence."
	fi
fi
