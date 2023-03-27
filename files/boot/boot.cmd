# DEBIAN IMAGE BUILDER
# mkimage -C none -A arm -T script -d boot.cmd boot.scr

setenv bootlabel ""
setenv uuid ""
setenv rootfstype ""
setenv verbose "5"
setenv console ""
setenv extra ""
setenv fk_kvers ""
setenv initrd ""
setenv fdtdir ""
setenv fdtfile ""

setenv bootargs "${console} rw root=${uuid} ${rootfstype} loglevel=${verbose} fsck.repair=yes ${extra} rootwait"

setenv load ""
${load} ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${fk_kvers} \
&& ${load} ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${fdtdir}/${fdtfile} \
&& ${load} ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${initrd} \
&& echo "Booting ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
