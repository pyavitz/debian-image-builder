# DEBIAN IMAGE BUILDER

setenv bootlabel ""
setenv rootfstype ""
setenv verbose ""
setenv console ""
setenv extra ""
setenv kernel ""
setenv initramfs ""
setenv platform ""
setenv fdtfile ""

setenv nvme_boot "false"
setenv nvme_devtype "nvme"
setenv nvme_devnum "0"
setenv nvme_bootpart "1"

if test "${nvme_boot}" = true; then
	if test -e ${nvme_devtype} ${nvme_devnum}:${nvme_bootpart} /boot.scr; then
		setenv devtype $nvme_devtype
		setenv devnum $nvme_devnum
		setenv distro_bootpart $nvme_bootpart
	elif test -e ${nvme_devtype} ${nvme_devnum}:${nvme_bootpart} /boot/boot.scr; then
		setenv devtype $nvme_devtype
		setenv devnum $nvme_devnum
		setenv distro_bootpart $nvme_bootpart
	fi
fi

if test -e ${devtype} ${devnum}:${distro_bootpart} ${kernel}; then
	setenv fk_kvers ${kernel}
	part uuid ${devtype} ${devnum}:2 uuid
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/${kernel}; then
	setenv fk_kvers boot/${kernel}
	part uuid ${devtype} ${devnum}:1 uuid
fi
if test -e ${devtype} ${devnum}:${distro_bootpart} ${initramfs}; then
	setenv initrd ${initramfs}
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/${initramfs}; then
	setenv initrd boot/${initramfs}
fi
if test -e ${devtype} ${devnum}:${distro_bootpart} ${platform}/${fdtfile}; then
	setenv fdtdir ${platform}
elif test -e ${devtype} ${devnum}:${distro_bootpart} boot/${platform}/${fdtfile}; then
	setenv fdtdir boot/${platform}
fi

setenv bootargs "${console} rw root=PARTUUID=${uuid} ${rootfstype} ${verbose} fsck.repair=yes ${extra} rootwait"

setenv loading ""
${loading} ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} ${fk_kvers} \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r} ${fdtdir}/${fdtfile} \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd}

if test -e ${devtype} ${devnum}:${distro_bootpart} ${fdtdir}/overlays/overlays.txt; then
	fdt addr ${fdt_addr_r}
	fdt resize
	setexpr fdtovaddr ${fdt_addr_r} + ${fdtoverlay_addr_r}
	if load ${devtype} ${devnum}:${distro_bootpart} ${fdtovaddr} ${fdtdir}/overlays/overlays.txt \
		&& env import -t ${fdtovaddr} ${filesize} && test -n ${overlays}; then
		echo "Loaded overlay.txt: ${overlays}"
		for ov in ${overlays}; do
			echo "Applying ${ov} ..."
			load ${devtype} ${devnum}:${distro_bootpart} ${fdtovaddr} ${fdtdir}/overlays/${ov}.dtbo && fdt apply ${fdtovaddr}
		done
	fi
fi

echo "Booting $bootlabel from ${devtype} ${devnum}:${distro_bootpart} ..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

echo "Trying bootm ..." \
&& bootm ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
