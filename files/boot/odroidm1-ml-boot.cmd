# Bootscript using the new unified bootcmd handling
#
# Expects to be called with the following environment variables set:
#
#  devtype              e.g. mmc/scsi etc
#  devnum               The device number of the given type
#  bootpart             The partition containing the boot files
#                       (introduced in u-boot mainline 2016.01)
#  prefix               Prefix within the boot partiion to the boot files
#  kernel_addr_r        Address to load the kernel to
#  fdt_addr_r           Address to load the FDT to
#  ramdisk_addr_r       Address to load the initrd to.
#
# The uboot must support the booti and generic filesystem load commands.

setenv board odroid${variant}

setenv bootargs " ${bootargs} root=UUID= loglevel=1"
setenv overlay_resize 8192
# pointless when trouble shooting
#setenv bootargs "quiet splash plymouth.ignore-serial-consoles"

setenv bootlabel "Odroid M1"

# Default TTY console
setenv bootargs "${bootargs} console=tty1 console=ttyS2,1500000 console=both"

# MISC
#
setenv bootargs "${bootargs} pci=nomsi net.ifnames=0"
setenv bootargs "${bootargs} fsck.mode=force fsck.repair=yes"

load ${devtype} ${devno}:${partition} ${loadaddr} ${prefix}config.ini \
    &&  ini generic ${loadaddr}
if test -n "${overlay_profile}"; then
    ini overlay_${overlay_profile} ${loadaddr}
fi

if test -n "${console}"; then
  setenv bootargs "${bootargs} console=${console}"
fi

if test -z "${fk_kvers}"; then
   setenv fk_kvers "Image"
fi

if test -z "${fdtfile}"; then
   setenv fdtfile "rk3568-odroid-m1.dtb"
fi

if test -z "${distro_bootpart}"; then
  setenv partition ${bootpart}
else
  setenv partition ${distro_bootpart}
fi

load ${devtype} ${devnum}:${partition} ${fdt_addr_r} ${prefix}rockchip/${fdtfile}
fdt addr ${fdt_addr_r}

#if test "x{overlays}" != "x"; then
#    for overlay in ${overlays}; do
#        fdt resize ${overlay_resize}
#        load ${devtype} ${devnum}:1 ${loadaddr} ${prefix}rockchip/overlays/${board}/${overlay}.dtbo \
#                && fdt apply ${loadaddr}
#    done
#fi

load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}Image \
&& unzip ${ramdisk_addr_r} ${kernel_addr_r} \
&& load ${devtype} ${devnum}:${partition} ${ramdisk_addr_r} ${prefix}uInitrd \
&& echo "Booting Debian ${fk_kvers} from ${devtype} ${devnum}:${partition}..." \
&& booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
