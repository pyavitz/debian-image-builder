if test -e ${devtype} ${devnum}:${distro_bootpart} boot.scr; then
	setenv prefix ""
	part uuid ${devtype} ${devnum}:2 uuid
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/boot.scr; then
	setenv prefix "boot"
	part uuid ${devtype} ${devnum}:1 uuid
fi

setenv platform ""
setenv user_env "${prefix}/${platform}Env.txt"
echo "Loading environment (${devtype} ${devnum}:${distro_bootpart})"
load ${devtype} ${devnum}:${distro_bootpart} ${scriptaddr} ${user_env}
env import -t ${scriptaddr} ${filesize}

setenv rootdev "PARTUUID=${uuid}"
setenv fk_kvers "${prefix}/${kernel}"
setenv initrd "${prefix}/${initramfs}"
setenv fdtdir "${prefix}/${platform}"
setenv user_overlay_dir "${prefix}/user-overlays"
setenv bootargs "${console} rw root=${rootdev} rootfstype=${rootfstype} loglevel=${loglevel} fsck.repair=yes ${extra} rootwait init=/sbin/init"

load ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd}
load ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} ${fk_kvers}
echo "Loading ${fdtfile} (${devtype} ${devnum}:${distro_bootpart})"
load ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r} ${fdtdir}/${fdtfile}

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

echo "Booting $bootlabel (${devtype} ${devnum}:${distro_bootpart})" \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

echo "Trying bootm ..." \
&& bootm ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
