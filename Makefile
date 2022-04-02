# menu
MENU=./lib/dialog/menu
CONF=./lib/dialog/config
DIALOGRC=$(shell cp -f lib/dialogrc ~/.dialogrc)

# misc
RFS=./scripts/rootfs
ROOTFS=sudo ./scripts/rootfs
RFSV7=./scripts/rootfs
ROOTFSV7=sudo ./scripts/rootfs
CLN=./scripts/clean
CLEAN=sudo ./scripts/clean

PURGE=$(shell sudo rm -fdr sources)
PURGEALL=$(shell sudo rm -fdr sources output)

# uboot and linux
XUBOOT=./scripts/uboot
UBOOT=sudo ./scripts/uboot
XKERNEL=./scripts/linux
KERNEL=sudo ./scripts/linux

# image
IMG=./scripts/stage1
IMAGE=sudo ./scripts/stage1
STG2=./scripts/stage2

# dependencies
CCOMPILE=./scripts/.ccompile
NCOMPILE=./scripts/.ncompile

# do not edit above this line

help:
	@echo ""
	@echo "\e[1;31m                  Debian Image Builder\e[0m"
	@echo "\e[1;37m                  ********************"
	@echo "Boards:\e[0m"
	@echo "  Allwinner:  nanopim1 nanopineo nanopi opione opipc tritium"
	@echo "  Amlogic:    lepotato odroidc4 odroidn2 odroidn2+ rzero"
	@echo "  Broadcom:   raspi4"
	@echo "  Rockchip:   nanopc renegade rockpro64"
	@echo ""
	@echo "\e[1;37mCommand List:\e[0m"
	@echo "  make ccompile                Install x86_64 dependencies"
	@echo "  make ncompile                Install aarch64 dependencies"
	@echo "  make config                  Create user data file"
	@echo "  make menu                    Menu interface"
	@echo "  make cleanup                 Clean up image errors"
	@echo "  make purge                   Remove sources directory"
	@echo "  make purge-all               Remove sources and output directory"
	@echo ""
	@echo "  make board-uboot             Make u-boot"
	@echo "  make board-kernel            Make linux kernel"
	@echo "  make rootfs                  Make arm64 rootfs tarball"
	@echo "  make rootfsv7                Make armhf rootfs tarball"
	@echo "  make board-image             Make bootable Debian image"
	@echo "  make board-all               Feeling lucky?"
	@echo ""
	@echo "For details consult the \e[1;37mREADME.md\e[0m file"
	@echo


ccompile:
	# Installing cross dependencies:
	@chmod +x ${CCOMPILE}
	@${CCOMPILE}

ncompile:
	# Installing native dependencies:
	@chmod +x ${NCOMPILE}
	@${NCOMPILE}

### TRITIUM
tritium-uboot:
	# Compiling u-boot
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/tritium'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

tritium-kernel:
	# Compiling kernel
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/tritium'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

tritium-image:
	# Creating image
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

tritium-all:
	# T R I T I U M
	# - - - - - - - -
	# Compiling u-boot
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/tritium'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/tritium'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### PINEA64
pine64-uboot:
	# Compiling u-boot
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/pine64'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

pine64-kernel:
	# Compiling kernel
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/pine64'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

pine64-image:
	# Creating image
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

pine64-all:
	# P I N E 6 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/pine64'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/pine64'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### NANOPI NEO PLUS2
nanopi-uboot:
	# Compiling u-boot
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopi'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

nanopi-kernel:
	# Compiling kernel
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopi'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

nanopi-image:
	# Creating image
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

nanopi-all:
	# N A N O  P I  N E O + 2
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopi'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopi'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### NANOPI NEO H3
nanopineo-uboot:
	# Compiling u-boot
	@ echo nanopineo > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopineo'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

nanopineo-kernel:
	# Compiling kernel
	@ echo nanopineo > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopineo'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

nanopineo-image:
	# Creating image
	@ echo nanopineo > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

nanopineo-all:
	# N A N O  P I  N E O
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopineo > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopineo'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopineo > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopineo'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-armhf'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo nanopineo > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### NANOPI M1 H3
nanopim1-uboot:
	# Compiling u-boot
	@ echo nanopim1 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopim1'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

nanopim1-kernel:
	# Compiling kernel
	@ echo nanopim1 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopim1'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

nanopim1-image:
	# Creating image
	@ echo nanopim1 > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

nanopim1-all:
	# N A N O  P I  M 1
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopim1 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopim1'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopim1 > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopim1'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-armhf'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo nanopim1 > board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### ORANGEPI ONE H3
opione-uboot:
	# Compiling u-boot
	@ echo opione > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/opione'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

opione-kernel:
	# Compiling kernel
	@ echo opione > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/opione'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

opione-image:
	# Creating image
	@ echo opione > board.txt
	@ echo orangepi >> board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

opione-all:
	# O R A N G E  P I  O N E
	# - - - - - - - -
	# Compiling u-boot
	@ echo opione > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/opione'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo opione > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/opione'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-armhf'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo opione > board.txt
	@ echo orangepi >> board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### ORANGEPI PC H3
opipc-uboot:
	# Compiling u-boot
	@ echo opipc > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/opipc'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

opipc-kernel:
	# Compiling kernel
	@ echo opipc > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/opipc'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

opipc-image:
	# Creating image
	@ echo opipc > board.txt
	@ echo orangepi >> board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

opipc-all:
	# O R A N G E  P I  P C
	# - - - - - - - -
	# Compiling u-boot
	@ echo opipc > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo OUTPUT='"'../output/opipc'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo opipc > board.txt
	@ echo allwinner >> board.txt
	@ echo ARCH='"'arm'"' >> board.txt
	@ echo CROSS_COMPILE='"'arm-linux-gnueabihf-'"' >> board.txt
	@ echo DEFCONFIG='"'allwinner-sun8i_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/opipc'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-armhf'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo opipc > board.txt
	@ echo orangepi >> board.txt
	@ echo allwinner >> board.txt
	@ echo p1 >> board.txt
	@ echo arm >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### ODROID C4
odroidc4-uboot:
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidc4'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidc4-kernel:
	# Compiling kernel
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'odroid_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidc4'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

odroidc4-image:
	# Creating image
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

odroidc4-all:
	# O D R O I D  C 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidc4'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'odroid_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidc4'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### ODROID N2
odroidn2-uboot:
	# Compiling u-boot
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidn2-kernel:
	# Compiling kernel
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'odroid_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	
odroidn2-image:
	# Creating image
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

odroidn2-all:
	# O D R O I D  N 2
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'odroid_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### ODROID N2 Plus
odroidn2+-uboot:
	# Compiling u-boot
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2plus'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidn2+-kernel:
	# Compiling kernel
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'odroid_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2plus'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	
odroidn2+-image:
	# Creating image
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

odroidn2+-all:
	# O D R O I D  N 2  P L U S
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2plus'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'odroid_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/odroidn2plus'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### LE POTATO
lepotato-uboot:
	# Compiling u-boot
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/lepotato'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

lepotato-kernel:
	# Compiling kernel
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'amlogic_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/lepotato'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

lepotato-image:
	# Creating image
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

lepotato-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/lepotato'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'amlogic_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/lepotato'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}
	
### Radxa Zero
rzero-uboot:
	# Compiling u-boot
	@ echo radxazero > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/radxazero'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

rzero-kernel:
	# Compiling kernel
	@ echo radxazero > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'amlogic_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/radxazero'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

rzero-image:
	# Creating image
	@ echo radxazero > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

rzero-all:
	# R A D X A  Z E R O
	# - - - - - - - -
	# Compiling u-boot
	@ echo radxazero > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/radxazero'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo radxazero > board.txt
	@ echo amlogic >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'amlogic_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/radxazero'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo radxazero > board.txt
	@ echo amlogic >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

# RENEGADE
renegade-uboot:
	# Compiling u-boot
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3328 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/renegade'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

renegade-kernel:
	# Compiling kernel
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/renegade'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

renegade-image:
	# Creating image
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

renegade-all:
	# R E N E G A D E
	# - - - - - - - -
	# Compiling u-boot
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3328 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/renegade'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/renegade'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

# ROCK64
rock64-uboot:
	# Compiling u-boot
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3328 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/rock64'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

rock64-kernel:
	# Compiling kernel
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/rock64'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

rock64-image:
	# Creating image
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

rock64-all:
	# R O C K 6 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3328 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/rock64'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/rock64'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

# ROCK64PRO
rockpro64-uboot:
	# Compiling u-boot
	@ echo rockpro64 > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3399 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/rockpro64'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

rockpro64-kernel:
	# Compiling kernel
	@ echo rockpro64 > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/rockpro64'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

rockpro64-image:
	# Creating image
	@ echo rockpro64 > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

rockpro64-all:
	# R O C K 6 4 P R O
	# - - - - - - - -
	# Compiling u-boot
	@ echo rockpro64 > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3399 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/rockpro64'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo rockpro64 > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/rockpro64'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo rockpro64 > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

# NANOPC-T4
nanopc-uboot:
	# Compiling u-boot
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3399 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopc'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

nanopc-kernel:
	# Compiling kernel
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopc'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

nanopc-image:
	# Creating image
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

nanopc-all:
	# N A N O P C - T 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@ echo rk3399 >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopc'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'rockchip64_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/nanopc'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@ echo p1 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

# RASPBERRY PI 4B
raspi4-uboot:
	# Compiling u-boot
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/raspi4'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

raspi4-kernel:
	# Compiling kernel
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'bcm2711_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/raspi4'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

raspi4-image:
	# Creating image
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@ echo p2 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

raspi4-all:
	# R A S P B E R R Y  P I  4 B
	# - - - - - - - -
	# Compiling u-boot
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo OUTPUT='"'../output/raspi4'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@ echo ARCH='"'arm64'"' >> board.txt
	@ echo CROSS_COMPILE='"'aarch64-linux-gnu-'"' >> board.txt
	@ echo DEFCONFIG='"'bcm2711_defconfig'"' >> board.txt
	@ echo OUTPUT='"'../output/raspi4'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@ echo p2 >> board.txt
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}

### MISCELLANEOUS
menu:
	# Menu
	@chmod +x ${MENU}
	@${MENU}
config:
	# Config Menu
	@chmod +x ${CONF}
	@${CONF}

dialogrc:
	# Builder theme set
	@${DIALOGRC}

rootfs:
	# ROOTFS
	@ echo ROOTFS_ARCH='"'rootfs-aarch64'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
	
rootfsv7:
	# ROOTFS
	@ echo ROOTFS_ARCH='"'rootfs-armhf'"' > board.txt
	@chmod +x ${RFSV7}
	@${ROOTFSV7}

cleanup:
	# Cleaning up
	@chmod +x ${CLN}
	@${CLEAN}

purge:
	# Removing sources directory
	@${PURGE}

purge-all:
	# Removing sources and output directory
	@${PURGEALL}
