setenv bootlabel ""
setenv uuid ""
setenv rootfstype "ext4"
setenv verbose "1"
setenv usbquirks ""
setenv extra ""
setenv console ""

# kernel
setenv fk_kvers ""
setenv initrd ""

# fdt
setenv fdtdir ""
setenv fdtfile ""

setenv bootargs " ${console} ro root=${uuid} rootfstype=${rootfstype} loglevel=${verbose} ${extra} ${usbquirks} rootwait"

ext4load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${fk_kvers} \
&& ext4load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${fdtdir}/${fdtfile} \
&& ext4load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${initrd} \
&& echo "Booting ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

ext4load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${fk_kvers} \
&& ext4load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${fdtdir}/${fdtfile} \
&& ext4load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${initrd} \
&& echo "Booting from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
