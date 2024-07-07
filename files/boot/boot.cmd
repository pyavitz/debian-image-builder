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
&& ${loading} ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} ${initrd} \
&& echo "Booting $bootlabel from ${devtype} ${devnum}:${distro_bootpart} ..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
