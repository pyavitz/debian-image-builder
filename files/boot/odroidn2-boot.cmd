# Bootscript using the new unified bootcmd handling
#
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

setenv bootargs " ${bootargs} ro root=UUID= net.ifname=0 rootfstype=ext4 fsck.repair=yes loglevel=1 rootwait"

setenv bootlabel "Odroid N2"

# Default serial console
# setenv console "ttyAML0,115200n8"

# Default TTY console
setenv bootargs "${bootargs} console=tty1 console=both"
setenv fdt_addr_r "0x20000000"
setenv bootargs "${bootargs} cma=800M"
setenv bootargs "${bootargs} clk_ignore_unused"

if test -n "${console}"; then
  setenv bootargs "${bootargs} console=${console}"
fi

if test -z "${fk_kvers}"; then
   setenv fk_kvers "Image"
fi

if test "${fk_kvers}" = "Image"; then
   setenv kernel_addr_r "0x10800000"
fi

if test -z "${fdtfile}"; then
   setenv fdtfile "meson64_odroid${variant}.dtb"
fi

if test -z "${distro_bootpart}"; then
  setenv partition ${bootpart}
else
  setenv partition ${distro_bootpart}
fi

load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${prefix}Image \
&& load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}amlogic/${fdtfile} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}uInitrd \
&& echo "Booting Debian ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${prefix}Image \
&& load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}amlogic/${fdtfile} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}uInitrd \
&& echo "Booting Debian ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}

load ${devtype} ${devnum}:${partition} ${kernel_addr_r} ${prefix}Image \
&& load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}amlogic/${fdtfile} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}uInitrd \
&& echo "Booting Debian from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
