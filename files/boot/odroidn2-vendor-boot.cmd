# Expects to be called with the following environment variables set:
#
#  devtype              e.g. mmc/scsi etc
#  devnum               The device number of the given type
#  bootpart             The partition containing the boot files
#  distro_bootpart      The partition containing the boot files
#                       (introduced in u-boot mainline 2016.01)
#  prefix               Prefix within the boot partiion to the boot files
#  kernel_addr_r        Address to load the kernel to
#  fdt_addr_r           Address to load the FDT to
#  ramdisk_addr_r       Address to load the initrd to.
#
# The uboot must support the booti and generic filesystem load commands.

setenv bootlabel ""
setenv uuid ""
setenv rootfstype ""
setenv verbose ""
setenv usbquirks ""
setenv extra ""
setenv console ""
setenv display ""

# kernel
setenv fk_kvers ""
setenv initrd ""
setenv kernel_addr_r ""

# fdt
setenv fdtdir ""
setenv fdtfile ""
setenv fdt_addr_r ""

if test "${variant}" = "n2_plus"; then
        setenv max_freq_a73 "2208"
        setenv max_freq_a53 "1908"
else
        setenv max_freq_a73 "1800"
        setenv max_freq_a53 "1896"
fi

setenv maxcpus "6"
setenv bootargs "${bootargs} max_freq_a53=${max_freq_a53} max_freq_a73=${max_freq_a73} maxcpus=${maxcpus}"

load mmc ${devno}:1 ${loadaddr} config.ini && ini user_options ${loadaddr} && ini kernel ${loadaddr} && ini fdt ${loadaddr} && ini odroidn2 ${loadaddr}

if test "x${overlay_profile}" != "x"; then
	ini overlay_${overlay_profile} ${loadaddr}
fi

setenv bootargs "${bootargs} ${console} ro root=${uuid} net.ifnames=0 rootfstype=${rootfstype} fsck.repair=yes loglevel=${verbose} ${extra} ${usbquirks} ${display} rootwait"

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
&& echo "Booting ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${prefix}${fk_kvers} \
&& load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}${fdtdir}/${fdtfile} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}${initrd} \
&& echo "Booting from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
