# DEBIAN IMAGE BUILDER (SPACEMIT)

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

# Set device tree binary
if test "${product_name}" = "k1-x_deb1"; then
	setenv bootlabel "BananaPi BPI-F3"
	setenv fdtfile "k1-bananapi-f3.dtb";
elif test "${product_name}" = "k1-x_MUSE-Book"; then
	setenv bootlabel "SpacemiT MUSE Book"
	setenv fdtfile "k1-musebook.dtb";
elif test "${product_name}" = "k1-x_MUSE-Pi-Pro"; then
	setenv bootlabel "SpacemiT MusePi Pro"
	setenv fdtfile "k1-musepi-pro.dtb";
fi

# Set boot variables
if test -e ${devtype} ${devnum}:${distro_bootpart} boot.scr; then
	setenv fk_kvers ${kernel}
	setenv initrd ${initramfs}
	setenv fdtdir ${platform}
	setenv user_overlay_dir user-overlays
	part uuid ${devtype} ${devnum}:2 uuid
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/boot.scr; then
	setenv fk_kvers boot/${kernel}
	setenv initrd boot/${initramfs}
	setenv fdtdir boot/${platform}
	setenv user_overlay_dir boot/user-overlays
	part uuid ${devtype} ${devnum}:1 uuid
fi

setenv bootargs "${console} rw root=PARTUUID=${uuid} ${rootfstype} ${verbose} fsck.repair=yes ${extra} rootwait init=/sbin/init"

setenv loading ""
${loading} ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd} \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} ${fk_kvers} \
&& echo "Loading ${fdtdir}/${fdtfile} ..." \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r} ${fdtdir}/${fdtfile}

fdt addr ${fdt_addr_r}
fdt resize 65536
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

echo "Trying bootm ..." \
&& bootm ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
