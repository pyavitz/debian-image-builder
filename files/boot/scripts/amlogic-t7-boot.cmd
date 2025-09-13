# DEBIAN IMAGE BUILDER (AMLOGIC T7)

setenv rootfstype ""
setenv kernel ""
setenv initramfs ""

# Load uconfig.txt
if test -e ${devtype} ${devnum}:${distro_bootpart} uconfig.txt; then
	setenv uconfig "uconfig.txt"
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/uconfig.txt; then
	setenv uconfig "boot/uconfig.txt"
fi
echo "Loading ${uconfig} from ${devtype} ${devnum}:${distro_bootpart} ..."
load ${devtype} ${devnum}:${distro_bootpart} ${scriptaddr} ${uconfig}
env import -t ${scriptaddr} ${filesize}

# Set boot variables
if test -e ${devtype} ${devnum}:${distro_bootpart} boot.scr; then
	setenv fk_kvers ${kernel}
	setenv initrd ${initramfs}
	setenv fdtdir ${platform}
	setenv user_overlay_dir user-overlays
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/boot.scr; then
	setenv fk_kvers boot/${kernel}
	setenv initrd boot/${initramfs}
	setenv fdtdir boot/${platform}
	setenv user_overlay_dir boot/user-overlays
fi

setenv bootargs "${console} root=${rootdev} rw ${rootfstype} ${verbose} fsck.repair=yes ${extra} rootwait partition_type=generic ${bootargs} init=/sbin/init"

setenv loading ""
${loading} ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd} \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} ${fk_kvers} \
&& echo "Loading ${fdtdir}/${fdtfile} ..." \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r} ${fdtdir}/${fdtfile}

fdt addr ${fdt_addr_r}
fdt resize 65536

if test "${mipi_lcd_exist}" = "0"; then
	fdt set /lcd status disabled
	fdt set /lcd1 status disabled
	fdt set /lcd2 status disabled
	fdt set /soc/apb4@fe000000/i2c@6c000/gt9xx@14 status disabled
	fdt set /soc/apb4@fe000000/i2c@6c000/ft5336@38 status disabled
else
	if test "${panel_type}" = "mipi_1"; then
		fdt set /drm-subsystem fbdev_sizes <1920 1200 1920 2400 32>
	else
		fdt set /drm-subsystem fbdev_sizes <1080 1920 1080 3840 32>
	fi
fi

if test -n ${overlays}; then
	for dtoverlay in ${overlays}; do
		echo "Applying ${dtoverlay} ..."
		load ${devtype} ${devnum}:${distro_bootpart} ${scriptaddr} ${fdtdir}/overlays/${dtoverlay}.dtbo && fdt apply ${scriptaddr}
	done
fi
if test -n ${user_overlays}; then
	for dtoverlay in ${user_overlays}; do
		echo "Applying ${dtoverlay} ..."
		load ${devtype} ${devnum}:${distro_bootpart} ${scriptaddr} ${user_overlay_dir}/${dtoverlay}.dtbo && fdt apply ${scriptaddr}
	done
fi

echo "Booting $bootlabel from ${devtype} ${devnum}:${distro_bootpart} ..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
