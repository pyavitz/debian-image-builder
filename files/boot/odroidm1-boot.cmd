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

setenv bootargs "${bootargs} quiet splash"
setenv overlay_resize 8192

setenv bootlabel "ODROID M1"

# Default serial console
setenv console "ttyFIQ0"

# Default TTY console
setenv bootargs "${bootargs} earlycon=uart8250,mmio32,0xfe660000 console=tty1"

# Builder bootargs
setenv bootargs "root=UUID= net.ifnames=0 rootwait rw"

# MISC
#
setenv bootargs "${bootargs} pci=nomsi"
setenv bootargs "${bootargs} fsck.mode=force fsck.repair=yes"
setenv bootargs "${bootargs} mtdparts=sfc_nor:0x20000@0xe0000(env),0x200000@0x100000(uboot),0x100000@0x300000(splash),0xc00000@0x400000(firmware)"

#setenv bootargs "${bootargs} mem=4096M"

#load ${devtype} ${devno}:${partition} ${loadaddr} ${prefix}config.ini \
#    &&  ini generic ${loadaddr}
#if test -n "${overlay_profile}"; then
#    ini overlay_${overlay_profile} ${loadaddr}
#fi

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

#
# 'resolution' and 'refresh' are to select a default display resolution and its refresh
# rate, it can be defined in '/boot/config.ini'.
#
#  [generic]
#  resolution=1920x1080
#  refresh=60
#
# Examples resolutions:
#     1920x1080
#     1024x768
#     800x600
#     640x480

if test -n "${resolution}"; then
  setenv refresh 60
  if test -n "${refresh}"; then
    setenv refresh "${refresh}"
  fi
  setenv bootargs "${bootargs} video=HDMI-A-1:${resolution}@${refresh}"
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
