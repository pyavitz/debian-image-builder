setenv bootargs console=tty console=ttyS0,115200 no_console_suspend root=/dev/mmcblk0p1 rootfstype=ext4 rootwait
ext4load ${devtype} ${devnum} $fdt_addr_r boot/allwinner/$fdtfile
ext4load ${devtype} ${devnum} $ramdisk_addr_r boot/uInitrd
ext4load ${devtype} ${devnum} $kernel_addr_r boot/Image
booti $kernel_addr_r - $fdt_addr_r
