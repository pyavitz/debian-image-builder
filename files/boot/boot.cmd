# DEBIAN IMAGE BUILDER

setenv bootlabel ""
setenv uuid ""
setenv rootfstype ""
setenv verbose ""
setenv console ""
setenv extra ""
setenv fk_kvers ""
setenv initrd ""
setenv fdtdir ""
setenv fdtfile ""

setenv bootargs "${console} rw root=${uuid} ${rootfstype} ${verbose} fsck.repair=yes ${extra} rootwait"

setenv loading ""
${loading} ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} ${fk_kvers} \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r} ${fdtdir}/${fdtfile} \
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd}

fdt addr ${fdt_addr_r}
fdt resize
setexpr fdtovaddr ${fdt_addr_r} + ${fdtoverlay_addr_r}

if load ${devtype} ${devnum}:${distro_bootpart} ${fdtovaddr} ${fdtdir}/overlays/overlays.txt \
	&& env import -t ${fdtovaddr} ${filesize} && test -n ${overlays}; then
	echo "Loaded overlay.txt: ${overlays}"
	for ov in ${overlays}; do
		echo "Overlaying ${ov} ..."
		load ${devtype} ${devnum}:${distro_bootpart} ${fdtovaddr} ${fdtdir}/overlays/${ov}.dtbo && fdt apply ${fdtovaddr}
	done
fi

echo "Booting $bootlabel from ${devtype} ${devnum}:${distro_bootpart} ..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

echo "Trying bootm ..." \
&& bootm ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
