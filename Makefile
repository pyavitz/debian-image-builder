# menu
MENU=./lib/menu
CONF=./lib/config
DIALOGRC=$(shell cp -f lib/dialogrc ~/.dialogrc)

# functions
RFS=./scripts/make-rootfs
ROOTFS=sudo ./scripts/make-rootfs
CLN=./scripts/clean
CLEAN=sudo ./scripts/clean

PURGE=$(shell sudo rm -fdr potato) \
$(shell sudo rm -fdr odroidc4) \
$(shell sudo rm -fdr odroidn2) \
$(shell sudo rm -fdr tritium) \
$(shell sudo rm -fdr pine64) \
$(shell sudo rm -fdr nanopi)

# tritium
TRI-UBOOT=./scripts/tritium-uboot
TRI-KERNEL=./scripts/tritium-kernel
TRI-IMG=./scripts/allwinner-stage1
TRI-IMAGE=sudo ./scripts/allwinner-stage1
TRI-STG2=./scripts/allwinner-stage2

# pine64
PINE-UBOOT=./scripts/pine64-uboot
PINE-KERNEL=./scripts/pine64-kernel
PINE-IMG=./scripts/allwinner-stage1
PINE-IMAGE=sudo ./scripts/allwinner-stage1
PINE-STG2=./scripts/allwinner-stage2

# odroid-c4
ODC4-UBOOT=./scripts/odroidc4-uboot
ODC4-KERNEL=./scripts/odroidc4-kernel
ODC4-IMG=./scripts/amlogic-stage1
ODC4-IMAGE=sudo ./scripts/amlogic-stage1
ODC4-STG2=./scripts/amlogic-stage2

# odroid-n2
ODN2-UBOOT=./scripts/odroidn2-uboot
ODN2-KERNEL=./scripts/odroidn2-kernel
ODN2-IMG=./scripts/amlogic-stage1
ODN2-IMAGE=sudo ./scripts/amlogic-stage1
ODN2-STG2=./scripts/amlogic-stage2

# le-potato
LEP-UBOOT=./scripts/lepotato-uboot
LEP-KERNEL=./scripts/lepotato-kernel
LEP-IMG=./scripts/amlogic-stage1
LEP-IMAGE=sudo ./scripts/amlogic-stage1
LEP-STG2=./scripts/amlogic-stage2

# nanopi neo plus 2
NPI-UBOOT=./scripts/nanopi-uboot
NPI-KERNEL=./scripts/nanopi-kernel
NPI-IMG=./scripts/allwinner-stage1
NPI-IMAGE=sudo ./scripts/allwinner-stage1
NPI-STG2=./scripts/allwinner-stage2
# do not edit above this line

help:
	@echo
	@echo "Boards: tritium pine64 odroidc4 odroidn2 lepotato nanopi"
	@echo
	@echo "  make install-depends         Install all dependencies"
	@echo "  make config                  Create user data file"
	@echo "  make menu                    Menu interface"
	@echo "  make cleanup                 Clean up image errors"
	@echo "  make purge                   Remove tmp directory"
	@echo
	@echo " Legacy commands:"
	@echo
	@echo "  make board-uboot             Make u-boot"
	@echo "  make board-kernel            Make linux kernel"
	@echo "  make rootfs                  Make ROOTFS tarball"
	@echo "  make board-image             Make bootable Debian image"
	@echo "  make board-all               Feeling lucky?"
	@echo
	@echo "For details consult the readme.md file"
	@echo


install-depends:
	# Install all dependencies
	sudo apt install build-essential bison bc git dialog patch \
	dosfstools zip unzip qemu debootstrap qemu-user-static rsync \
	kmod cpio flex libssl-dev libncurses5-dev parted device-tree-compiler \
	libfdt-dev python3-distutils python3-dev swig fakeroot lzop lz4 \
	crossbuild-essential-arm64

### TRITIUM
tritium-uboot:
	# Compiling u-boot
	@chmod +x ${TRI-UBOOT}
	@${TRI-UBOOT}

tritium-kernel:
	# Compiling kernel
	@chmod +x ${TRI-KERNEL}
	@${TRI-KERNEL}

tritium-image:
	# Making bootable Debian image
	@ echo tritium > board.txt 
	@chmod +x ${TRI-IMG}
	@chmod +x ${TRI-STG2}
	@${TRI-IMAGE}

tritium-all:
	# T R I T I U M
	# - - - - - - - -
	# Compiling u-boot
	@chmod +x ${TRI-UBOOT}
	@${TRI-UBOOT}
	# Building linux package
	@chmod +x ${TRI-KERNEL}
	@${TRI-KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo tritium > board.txt 
	@chmod +x ${TRI-IMG}
	@chmod +x ${TRI-STG2}
	@${TRI-IMAGE}

### PINE64
pine64-uboot:
	# Compiling u-boot
	@chmod +x ${PINE-UBOOT}
	@${PINE-UBOOT}

pine64-kernel:
	# Compiling kernel
	@chmod +x ${PINE-KERNEL}
	@${PINE-KERNEL}

pine64-image:
	# Making bootable Debian image
	@ echo pine64 > board.txt 
	@chmod +x ${PINE-IMG}
	@chmod +x ${PINE-STG2}
	@${PINE-IMAGE}

pine64-all:
	# P I N E 6 4
	# - - - - - - - -
	# Compiling u-boot
	@chmod +x ${PINE-UBOOT}
	@${PINE-UBOOT}
	# Building linux package
	@chmod +x ${PINE-KERNEL}
	@${PINE-KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo pine64 > board.txt 
	@chmod +x ${PINE-IMG}
	@chmod +x ${PINE-STG2}
	@${PINE-IMAGE}


### ODROID-C4
odroidc4-uboot:
	# Compiling u-boot
	@chmod +x ${ODC4-UBOOT}
	@${ODC4-UBOOT}

odroidc4-kernel:
	# Compiling kernel
	@chmod +x ${ODC4-KERNEL}
	@${ODC4-KERNEL}

odroidc4-image:
	# Making bootable Debian image
	@ echo odroidc4 > board.txt 
	@chmod +x ${ODC4-IMG}
	@chmod +x ${ODC4-STG2}
	@${ODC4-IMAGE}

odroidc4-all:
	# O D R O I D - C 4
	# - - - - - - - -
	# Compiling u-boot
	@chmod +x ${ODC4-UBOOT}
	@${ODC4-UBOOT}
	# Building linux package
	@chmod +x ${ODC4-KERNEL}
	@${ODC4-KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo odroidc4 > board.txt 
	@chmod +x ${ODC4-IMG}
	@chmod +x ${ODC4-STG2}
	@${ODC4-IMAGE}

### ODROID-N2
odroidn2-uboot:
	# Compiling u-boot
	@chmod +x ${ODN2-UBOOT}
	@${ODN2-UBOOT}

odroidn2-kernel:
	# Compiling kernel
	@chmod +x ${ODN2-KERNEL}
	@${ODN2-KERNEL}
	
odroidn2-image:
	# Making bootable Debian image
	@ echo odroidn2 > board.txt 
	@chmod +x ${ODN2-IMG}
	@chmod +x ${ODN2-STG2}
	@${ODN2-IMAGE}

odroidn2-all:
	# O D R O I D - N 2
	# - - - - - - - -
	# Compiling u-boot
	@chmod +x ${ODN2-UBOOT}
	@${ODN2-UBOOT}
	# Building linux package
	@chmod +x ${ODN2-KERNEL}
	@${ODN2-KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo odroidn2 > board.txt 
	@chmod +x ${ODN2-IMG}
	@chmod +x ${ODN2-STG2}
	@${ODN2-IMAGE}


### LEPOTATO
lepotato-uboot:
	# Compiling u-boot
	@chmod +x ${LEP-UBOOT}
	@${LEP-UBOOT}

lepotato-kernel:
	# Compiling kernel
	@chmod +x ${LEP-KERNEL}
	@${LEP-KERNEL}

lepotato-image:
	# Making bootable Debian image
	@ echo lepotato > board.txt 
	@chmod +x ${LEP-IMG}
	@chmod +x ${LEP-STG2}
	@${LEP-IMAGE}

lepotato-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	@chmod +x ${LEP-UBOOT}
	@${LEP-UBOOT}
	# Building linux package
	@chmod +x ${LEP-KERNEL}
	@${LEP-KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo lepotato > board.txt 
	@chmod +x ${LEP-IMG}
	@chmod +x ${LEP-STG2}
	@${LEP-IMAGE}

### NANOPI NEO PLUS 2
nanopi-uboot:
	# Compiling u-boot
	@chmod +x ${NPI-UBOOT}
	@${NPI-UBOOT}

nanopi-kernel:
	# Compiling kernel
	@chmod +x ${NPI-KERNEL}
	@${NPI-KERNEL}

nanopi-image:
	# Making bootable Debian image
	@ echo nanopi > board.txt 
	@chmod +x ${NPI-IMG}
	@chmod +x ${NPI-STG2}
	@${NPI-IMAGE}

nanopi-all:
	# N A N O P I N E O + 2
	# - - - - - - - -
	# Compiling u-boot
	@chmod +x ${NPI-UBOOT}
	@${NPI-UBOOT}
	# Building linux package
	@chmod +x ${NPI-KERNEL}
	@${NPI-KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Making bootable Debian image
	@ echo nanopi > board.txt 
	@chmod +x ${NPI-IMG}
	@chmod +x ${NPI-STG2}
	@${NPI-IMAGE}

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

cleanup:
	# Cleaning up
	@chmod +x ${CLN}
	@${CLEAN}

purge:
	# Removing tmp directory
	@${PURGE}
##
