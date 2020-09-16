# menu
MENU=./lib/menu
CONF=./lib/config
DIALOGRC=$(shell cp -f lib/dialogrc ~/.dialogrc)

# functions
RFS=./scripts/rootfs
ROOTFS=sudo ./scripts/rootfs
RFSV7=./scripts/rootfsv7
ROOTFSV7=sudo ./scripts/rootfsv7
CLN=./scripts/clean
CLEAN=sudo ./scripts/clean

PURGE=$(shell sudo rm -fdr allwinner) \
$(shell sudo rm -fdr amlogic) \
$(shell sudo rm -fdr rockchip)

# uboot and linux
UBOOT=./scripts/uboot
KERNEL=./scripts/linux

# allwinner
ALL-IMG=./scripts/allwinner-stage1
ALL-IMAGE=sudo ./scripts/allwinner-stage1
ALL-STG2=./scripts/allwinner-stage2

# amlogic
AML-IMG=./scripts/amlogic-stage1
AML-IMAGE=sudo ./scripts/amlogic-stage1
AML-STG2=./scripts/amlogic-stage2

# rockchip
ROC-IMG=./scripts/rockchip-stage1
ROC-IMAGE=sudo ./scripts/rockchip-stage1
ROC-STG2=./scripts/rockchip-stage2
# do not edit above this line

help:
	@echo
	@echo "Boards: tritium pine64 odroidc4 odroidn2 lepotato nanopi opir1 renegade rock64"
	@echo
	@echo "  make ccompile-depends        Install all dependencies"
	@echo "  make ncompile-depends        Install all native dependencies"
	@echo "  make config                  Create user data file"
	@echo "  make menu                    Menu interface"
	@echo "  make cleanup                 Clean up image errors"
	@echo "  make purge                   Remove tmp directory"
	@echo
	@echo " Legacy commands:"
	@echo
	@echo "  make board-uboot             Make u-boot"
	@echo "  make board-kernel            Make linux kernel"
	@echo "  make rootfs                  Make arm64 ROOTFS tarball"
	@echo "  make rootfsv7                Make armhf ROOTFS tarball"
	@echo "  make board-image             Make bootable Debian image"
	@echo "  make board-all               Feeling lucky?"
	@echo
	@echo "For details consult the readme.md file"
	@echo


ccompile-depends:
	# Install all dependencies
	sudo apt install build-essential bison bc git dialog patch \
	dosfstools zip unzip qemu debootstrap qemu-user-static rsync \
	kmod cpio flex libssl-dev libncurses5-dev parted device-tree-compiler \
	libfdt-dev python3-distutils python3-dev swig fakeroot lzop lz4 \
	aria2 crossbuild-essential-arm64

ncompile-depends:
	# Install all dependencies
	sudo apt install build-essential bison bc git dialog patch \
	dosfstools zip unzip qemu debootstrap qemu-user-static rsync \
	kmod cpio flex libssl-dev libncurses5-dev parted device-tree-compiler \
	libfdt-dev python3-distutils python3-dev swig fakeroot lzop lz4 \
	aria2

### TRITIUM
tritium-uboot:
	# Compiling u-boot
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

tritium-kernel:
	# Compiling kernel
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

tritium-image:
	# Making bootable Debian image
	@ echo tritium > board.txt 
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

tritium-all:
	# T R I T I U M
	# - - - - - - - -
	# Compiling u-boot
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo tritium > board.txt 
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### PINEA64
pine64-uboot:
	# Compiling u-boot
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

pine64-kernel:
	# Compiling kernel
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

pine64-image:
	# Making bootable Debian image
	@ echo pine64 > board.txt 
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

pine64-all:
	# P I N E 6 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo pine64 > board.txt 
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### NANOPI NEO PLUS2
nanopi-uboot:
	# Compiling u-boot
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

nanopi-kernel:
	# Compiling kernel
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

nanopi-image:
	# Making bootable Debian image
	@ echo nanopi > board.txt 
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

nanopi-all:
	# N A N O  P I  N E O + 2
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo nanopi > board.txt 
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### ORANGEPI R1
opir1-uboot:
	# Compiling u-boot
	@ echo opir1 > board.txt
	@ echo armv7 >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

opir1-kernel:
	# Compiling kernel
	@ echo opir1 > board.txt
	@ echo armv7 >> board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

opir1-image:
	# Making bootable Debian image
	@ echo opir1 > board.txt
	@ echo armv7 >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

opir1-all:
	# O R A N G E  P I  R 1
	# - - - - - - - -
	# Compiling u-boot
	@ echo opir1 > board.txt
	@ echo armv7 >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo opir1 > board.txt
	@ echo armv7 >> board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFSV7}
	@${ROOTFSV7}
	# Making bootable Debian image
	@ echo opir1 > board.txt
	@ echo armv7 >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### ODROID-C4
odroidc4-uboot:
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

odroidc4-kernel:
	# Compiling kernel
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt 
	@chmod +x ${KERNEL}
	@${KERNEL}

odroidc4-image:
	# Making bootable Debian image
	@ echo odroidc4 > board.txt 
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

odroidc4-all:
	# O D R O I D - C 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo odroidc4 > board.txt 
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

### ODROID-N2
odroidn2-uboot:
	# Compiling u-boot
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

odroidn2-kernel:
	# Compiling kernel
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	
odroidn2-image:
	# Making bootable Debian image
	@ echo odroidn2 > board.txt 
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

odroidn2-all:
	# O D R O I D - N 2
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo odroidn2 > board.txt 
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

### LE POTATO
lepotato-uboot:
	# Compiling u-boot
	@ echo lepotato > board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

lepotato-kernel:
	# Compiling kernel
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

lepotato-image:
	# Making bootable Debian image
	@ echo lepotato > board.txt 
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

lepotato-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	@ echo lepotato > board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo lepotato > board.txt 
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

# RENEGADE
renegade-uboot:
	# Compiling u-boot
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

renegade-kernel:
	# Compiling kernel
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

renegade-image:
	# Making bootable Debian image
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

renegade-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

# ROCK64
rock64-uboot:
	# Compiling u-boot
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}

rock64-kernel:
	# Compiling kernel
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}

rock64-image:
	# Making bootable Debian image
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

rock64-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${UBOOT}
	@${UBOOT}
	# Building linux package
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${KERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

### MISCELLANEOUS
menu:
	# Builder Menu
	@chmod +x ${MENU}
	@${MENU}
config:
	# User Data File
	@chmod +x ${CONF}
	@${CONF}

dialogrc:
	# Builder theme set
	@${DIALOGRC}

rootfs:
	# DEBIAN ROOTFS 
	@chmod +x ${RFS}
	@${ROOTFS}

rootfsv7:
	# DEBIAN ROOTFS 
	@chmod +x ${RFSV7}
	@${ROOTFSV7}

cleanup:
	# Cleaning up
	@chmod +x ${CLN}
	@${CLEAN}

purge:
	# Removing tmp directory
	@${PURGE}
