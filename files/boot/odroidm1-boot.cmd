# ODROID MAINLINE
setenv overlay_resize 8192
setenv bootlabel ""
setenv uuid ""
setenv rootfstype ""
setenv verbose ""
setenv usbquirks ""
setenv extra ""
setenv console ""

setenv fk_kvers ""
setenv initrd ""
setenv fdtdir ""
setenv fdtfile ""

load ${devtype} ${devno}:${partition} ${loadaddr} ${prefix}config.ini && ini user_options ${loadaddr}

setenv bootargs "${console} ro root=${uuid} net.ifnames=0 rootfstype=${rootfstype} fsck.repair=yes loglevel=${verbose} ${extra} ${usbquirks} rootwait"

if test -z "${distro_bootpart}"; then
	setenv partition ${bootpart}
else
	setenv partition ${distro_bootpart}
fi

load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}${fdtdir}/${fdtfile}
fdt addr ${fdt_addr_r}

if test "x{overlays}" != "x"; then
	for overlay in ${overlays}; do
		fdt resize ${overlay_resize}
		load ${devtype} ${devnum}:1 ${loadaddr} ${prefix}${fdtdir}/overlays/${board}/${overlay}.dtbo \
		&& fdt apply ${loadaddr}
	done
fi

load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}${fk_kvers} \
&& unzip ${ramdisk_addr_r} ${kernel_addr_r} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}${initrd} \
&& echo "Booting ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
