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

#define build_uboot
#	# Compiling u-boot
#	@ echo $(1) > board.txt
#	@ echo $(2) >> board.txt
#	@ echo ARCH='"'$(3)'"' >> board.txt
#	@ echo CROSS_COMPILE='"'$(4)'"' >> board.txt
#	@ echo OUTPUT='"'../output/$(5)'"' >> board.txt
#	@chmod +x ${XUBOOT}
#	@${UBOOT}
#endef

define build_uboot
	# Compiling u-boot
	@ echo $(1) > board.txt
	@ echo $(2) >> board.txt
	@ if [ "$(3)" != "" ]; then echo "$(3)";fi >> board.txt 
	@ echo ARCH='"'$(4)'"' >> board.txt
	@ echo CROSS_COMPILE='"'$(5)'"' >> board.txt
	@ echo OUTPUT='"'../output/$(6)'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
endef

define build_kernel
	@ echo $(1) > board.txt
	@ echo $(2) >> board.txt
	@ echo ARCH='"'$(3)'"' >> board.txt
	@ echo CROSS_COMPILE='"'$(4)'"' >> board.txt
	@ echo DEFCONFIG='"'$(5)'"' >> board.txt
	@ echo OUTPUT='"'../output/$(6)'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
endef

define build_image
	@ echo $(1) > board.txt
	@ echo $(2) >> board.txt
	@ echo $(3) >> board.txt
	@ if [ "$(4)" != "" ]; then echo "$(4)";fi >> board.txt 
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}
endef

define create_rootfs
	@ echo ROOTFS_ARCH='"'$(1)'"' > board.txt
	@chmod +x ${RFS}
	@${ROOTFS}
endef

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
	$(call build_uboot,tritium,allwinner,,arm64,aarch64-linux-gnu-,tritium)

tritium-kernel:
	# Compiling kernel
	$(call build_kernel,tritium,allwinner,arm64,aarch64-linux-gnu-,allwinner_defconfig,tritium)

tritium-image:
	# Creating image
	$(call build_image,tritium,allwinner,p1,)

tritium-all:
	# T R I T I U M
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,tritium,allwinner,,arm64,aarch64-linux-gnu-,tritium)
	# Building linux package
	$(call build_kernel,tritium,allwinner,arm64,aarch64-linux-gnu-,allwinner_defconfig,tritium)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,tritium,allwinner,p1,)

### PINEA64
pine64-uboot:
	# Compiling u-boot
	$(call build_uboot,pine64,allwinner,,arm64,aarch64-linux-gnu-,pine64)

pine64-kernel:
	# Compiling kernel
	$(call build_kernel,pine64,allwinner,arm64,aarch64-linux-gnu-,allwinner_defconfig,pine64)

pine64-image:
	# Creating image
	$(call build_image,pine64,allwinner,p1,)

pine64-all:
	# P I N E 6 4
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,pine64,allwinner,,arm64,aarch64-linux-gnu-,pine64)
	# Building linux package
	$(call build_kernel,pine64,allwinner,arm64,aarch64-linux-gnu-,allwinner_defconfig,pine64)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,pine64,allwinner,p1,)

### NANOPI NEO PLUS2
nanopi-uboot:
	# Compiling u-boot
	$(call build_uboot,nanopi,allwinner,,arm64,aarch64-linux-gnu-,nanopi)

nanopi-kernel:
	# Compiling kernel
	$(call build_kernel,nanopi,allwinner,arm64,aarch64-linux-gnu-,allwinner_defconfig,nanopi)

nanopi-image:
	# Creating image
	$(call build_image,nanopi,allwinner,p1,)

nanopi-all:
	# N A N O  P I  N E O + 2
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,nanopi,allwinner,,arm64,aarch64-linux-gnu-,nanopi)
	# Building linux package
	$(call build_kernel,nanopi,allwinner,arm64,aarch64-linux-gnu-,allwinner_defconfig,nanopi)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,nanopi,allwinner,p1,)

### NANOPI NEO H3
nanopineo-uboot:
	# Compiling u-boot
	$(call build_uboot,nanopineo,allwinner,,arm,arm-linux-gnueabihf-,nanopineo)

nanopineo-kernel:
	# Compiling kernel
	$(call build_kernel,nanopineo,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,nanopineo)

nanopineo-image:
	# Creating image
	$(call build_image,nanopineo,allwinner,p1,arm)

nanopineo-all:
	# N A N O  P I  N E O
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,nanopineo,allwinner,,arm,arm-linux-gnueabihf-,nanopineo)
	# Building linux package
	$(call build_kernel,nanopineo,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,nanopineo)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-armhf)
	# Creating image
	$(call build_image,nanopineo,allwinner,p1,arm)

### NANOPI M1 H3
nanopim1-uboot:
	# Compiling u-boot
	$(call build_uboot,nanopim1,allwinner,,arm,arm-linux-gnueabihf-,nanopim1)

nanopim1-kernel:
	# Compiling kernel
	$(call build_kernel,nanopim1,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,nanopim1)

nanopim1-image:
	# Creating image
	$(call build_image,nanopim1,allwinner,p1,arm)

nanopim1-all:
	# N A N O  P I  M 1
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,nanopim1,allwinner,,arm,arm-linux-gnueabihf-,nanopim1)
	# Building linux package
	$(call build_kernel,nanopim1,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,nanopim1)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-armhf)
	# Creating image
	$(call build_image,nanopim1,allwinner,p1,arm)

### ORANGEPI ONE H3
opione-uboot:
	# Compiling u-boot
	$(call build_uboot,opione,allwinner,,arm,arm-linux-gnueabihf-,opione)

opione-kernel:
	# Compiling kernel
	$(call build_kernel,opione,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,opione)

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
	$(call build_uboot,opione,allwinner,,arm,arm-linux-gnueabihf-,opione)
	# Building linux package
	$(call build_kernel,opione,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,opione)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-armhf)
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
	$(call build_uboot,opipc,allwinner,,arm,arm-linux-gnueabihf-,opipc)

opipc-kernel:
	# Compiling kernel
	$(call build_kernel,opipc,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,opipc)

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
	$(call build_uboot,opipc,allwinner,,arm,arm-linux-gnueabihf-,opipc)
	# Building linux package
	$(call build_kernel,opipc,allwinner,arm,arm-linux-gnueabihf-,allwinner-sun8i_defconfig,opipc)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-armhf)
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
	$(call build_uboot,odroidc4,amlogic,,arm64,aarch64-linux-gnu-,odroidc4)

odroidc4-kernel:
	# Compiling kernel
	$(call build_kernel,odroidc4,amlogic,arm64,aarch64-linux-gnu-,odroid_defconfig,odroidc4)

odroidc4-image:
	# Creating image
	$(call build_image,odroidc4,amlogic,p1,)

odroidc4-all:
	# O D R O I D  C 4
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,odroidc4,amlogic,,arm64,aarch64-linux-gnu-,odroidc4)
	# Building linux package
	$(call build_kernel,odroidc4,amlogic,arm64,aarch64-linux-gnu-,odroid_defconfig,odroidc4)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,odroidc4,amlogic,p1,)

### ODROID N2
odroidn2-uboot:
	# Compiling u-boot
	$(call build_uboot,odroidn2,amlogic,,arm64,aarch64-linux-gnu-,odroidn2)

odroidn2-kernel:
	# Compiling kernel
	$(call build_kernel,odroidn2,amlogic,arm64,aarch64-linux-gnu-,odroid_defconfig,odroidn2)
	
odroidn2-image:
	# Creating image
	$(call build_image,odroidn2,amlogic,p1,)

odroidn2-all:
	# O D R O I D  N 2
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,odroidn2,amlogic,,arm64,aarch64-linux-gnu-,odroidn2)
	# Building linux package
	$(call build_kernel,odroidn2,amlogic,arm64,aarch64-linux-gnu-,odroid_defconfig,odroidn2)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,odroidn2,amlogic,p1,)

### ODROID N2 Plus
odroidn2+-uboot:
	# Compiling u-boot
	$(call build_uboot,odroidn2plus,amlogic,,arm64,aarch64-linux-gnu-,odroidn2plus)

odroidn2+-kernel:
	# Compiling kernel
	$(call build_kernel,odroidn2plus,amlogic,arm64,aarch64-linux-gnu-,odroid_defconfig,odroidn2plus)
	
odroidn2+-image:
	# Creating image
	$(call build_image,odroidn2plus,amlogic,p1,)

odroidn2+-all:
	# O D R O I D  N 2  P L U S
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,odroidn2plus,amlogic,,arm64,aarch64-linux-gnu-,odroidn2plus)
	# Building linux package
	$(call build_kernel,odroidn2plus,amlogic,arm64,aarch64-linux-gnu-,odroid_defconfig,odroidn2plus)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,odroidn2plus,amlogic,p1,)

### LE POTATO
lepotato-uboot:
	# Compiling u-boot
	$(call build_uboot,lepotato,amlogic,,arm64,aarch64-linux-gnu-,lepotato)

lepotato-kernel:
	# Compiling kernel
	$(call build_kernel,lepotato,amlogic,arm64,aarch64-linux-gnu-,amlogic_defconfig,lepotato)

lepotato-image:
	# Creating image
	$(call build_image,lepotato,amlogic,p1,)

lepotato-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,lepotato,amlogic,,arm64,aarch64-linux-gnu-,lepotato)
	# Building linux package
	$(call build_kernel,lepotato,amlogic,arm64,aarch64-linux-gnu-,amlogic_defconfig,lepotato)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,lepotato,amlogic,p1,)
	
### Radxa Zero
rzero-uboot:
	# Compiling u-boot
	$(call build_uboot,radxazero,amlogic,,arm64,aarch64-linux-gnu-,radxazero)

rzero-kernel:
	# Compiling kernel
	$(call build_kernel,radxazero,amlogic,arm64,aarch64-linux-gnu-,amlogic_defconfig,radxazero)

rzero-image:
	# Creating image
	$(call build_image,radxazero,amlogic,p1,)

rzero-all:
	# R A D X A  Z E R O
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,radxazero,amlogic,,arm64,aarch64-linux-gnu-,radxazero)
	# Building linux package
	$(call build_kernel,radxazero,amlogic,arm64,aarch64-linux-gnu-,amlogic_defconfig,radxazero)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,radxazero,amlogic,p1,)

# RENEGADE
renegade-uboot:
	# Compiling u-boot
	$(call build_uboot,renegad,rockchip,rk3399,arm64,aarch64-linux-gnu-,renegade)

renegade-kernel:
	# Compiling kernel
	$(call build_kernel,renegade,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,renegade)

renegade-image:
	# Creating image
	$(call build_image,renegade,rockchip,p1,)

renegade-all:
	# R E N E G A D E
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,renegad,rockchip,rk3399,arm64,aarch64-linux-gnu-,renegade)
	# Building linux package
	$(call build_kernel,renegade,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,renegade)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,renegade,rockchip,p1,)

# ROCK64
rock64-uboot:
	# Compiling u-boot
	$(call build_uboot,rock64,rockchip,rk3399,arm64,aarch64-linux-gnu-,rock64)

rock64-kernel:
	# Compiling kernel
	$(call build_kernel,rock64,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,rock64)

rock64-image:
	# Creating image
	$(call build_image,rock64,rockchip,p1,)

rock64-all:
	# R O C K 6 4
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,rock64,rockchip,rk3399,arm64,aarch64-linux-gnu-,rock64)
	# Building linux package
	$(call build_kernel,rock64,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,rock64)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,rock64,rockchip,p1,)

# ROCK64PRO
rockpro64-uboot:
	# Compiling u-boot
	$(call build_uboot,rockpro64,rockchip,rk3399,arm64,aarch64-linux-gnu-,rockpro64)

rockpro64-kernel:
	# Compiling kernel
	$(call build_kernel,rockpro64,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,rockpro64)

rockpro64-image:
	# Creating image
	$(call build_image,rockpro64,rockchip,p1,)

rockpro64-all:
	# R O C K 6 4 P R O
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,rockpro64,rockchip,rk3399,arm64,aarch64-linux-gnu-,rockpro64)
	# Building linux package
	$(call build_kernel,rockpro64,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,rockpro64)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,rockpro64,rockchip,p1,)

# NANOPC-T4
nanopc-uboot:
	# Compiling u-boot
	$(call build_uboot,nanopc,rockchip,rk3399,arm64,aarch64-linux-gnu-,nanopc)

nanopc-kernel:
	# Compiling kernel
	$(call build_kernel,nanopc,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,nanopc)

nanopc-image:
	# Creating image
	$(call build_image,nanopc,rockchip,p1,)

nanopc-all:
	# N A N O P C - T 4
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,nanopc,rockchip,rk3399,arm64,aarch64-linux-gnu-,nanopc)
	# Building linux package
	$(call build_kernel,nanopc,rockchip,arm64,aarch64-linux-gnu-,rockchip64_defconfig,nanopc)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,nanopc,rockchip,p1,)

# RASPBERRY PI 4B
raspi4-uboot:
	# Compiling u-boot
	$(call build_uboot,bcm2711,broadcom,arm64,aarch64-linux-gnu-,raspi4)

raspi4-kernel:
	# Compiling kernel
	$(call build_kernel,bcm2711,broadcom,arm64,aarch64-linux-gnu-,bcm2711_defconfig,raspi4)

raspi4-image:
	# Creating image
	$(call build_image,bcm2711,broadcom,p2,)

raspi4-all:
	# R A S P B E R R Y  P I  4 B
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,bcm2711,broadcom,arm64,aarch64-linux-gnu-,raspi4)
	# Building linux package
	$(call build_kernel,bcm2711,broadcom,arm64,aarch64-linux-gnu-,bcm2711_defconfig,raspi4)
	# Creating ROOTFS tarball
	$(call create_rootfs,rootfs-aarch64)
	# Creating image
	$(call build_image,bcm2711,broadcom,p2,)

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
	$(call create_rootfs,rootfs-aarch64)
	
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
