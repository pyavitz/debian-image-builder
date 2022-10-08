# ODROID MAINLINE
setenv bootlabel ""
setenv uuid ""
setenv rootfstype ""
setenv verbose ""
setenv usbquirks ""
setenv extra ""
setenv console ""

setenv fk_kvers ""
setenv initrd ""
setenv kernel_addr_r ""
setenv ramdisk_addr_r ""
setenv fdtdir ""
setenv fdtfile ""
setenv fdt_addr_r ""

load mmc ${devno}:1 ${loadaddr} config.ini && ini user_options ${loadaddr}

if test "x${overlay_profile}" != "x"; then
	ini overlay_${overlay_profile} ${loadaddr}
fi

setenv bootargs "${console} ro root=${uuid} net.ifnames=0 ${rootfstype} fsck.repair=yes loglevel=${verbose} ${extra} ${usbquirks} rootwait"

if test -z "${distro_bootpart}"; then
	setenv partition ${bootpart}
else
	setenv partition ${distro_bootpart}
fi

load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${prefix}${fk_kvers} \
&& load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}${fdtdir}/${fdtfile} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}${initrd} \
&& echo "Booting ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${prefix}${fk_kvers} \
&& load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}${fdtdir}/${fdtfile} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}${initrd} \
&& echo "Booting from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
