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

ifdef board
include lib/boards/${board}
endif

define build_uboot
	# Compiling u-boot
	@ echo $1, $2, $3, $4, $5, $6
	@ echo $(1) > board.txt
	@ echo $(2) >> board.txt
	@ if [ "$(3)" != "" ]; then echo "$(3)";fi >> board.txt 
	@ echo ARCH='"'$(4)'"' >> board.txt
	@ echo CROSS_COMPILE='"'$(5)'"' >> board.txt
	@ echo OUTPUT='"'$(6)'"' >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
endef

define build_kernel
	@ echo $(1) > board.txt
	@ echo $(2) >> board.txt
	@ echo ARCH='"'$(3)'"' >> board.txt
	@ echo CROSS_COMPILE='"'$(4)'"' >> board.txt
	@ echo DEFCONFIG='"'$(5)'"' >> board.txt
	@ echo OUTPUT='"'$(6)'"' >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
endef

define build_image
	@ echo $(1) > board.txt
	@ if [ "$(1)" = "opione" ] || [ "$(1)" = "opipc" ]; then echo orangepi;fi >> board.txt
	@ echo $(2) >> board.txt
	@ echo $(3) >> board.txt
	@ if [ "$(4)" != "arm64" ]; then echo "$(4)";fi >> board.txt 
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
	@echo "  Amlogic:    lepotato odroidc4 odroidn2 odroidn2plus radxazero"
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
	@echo "  make uboot board=XXX         Make u-boot"
	@echo "  make kernel board=XXX        Make linux kernel"
	@echo "  make rootfs                  Make arm64 rootfs tarball"
	@echo "  make rootfsv7                Make armhf rootfs tarball"
	@echo "  make image board=XXX         Make bootable Debian image"
	@echo "  make all board=XXX           Feeling lucky?"
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

uboot:
	@ echo ${board}
	@ echo ${IMG_EXP}
	$(call build_uboot,${BOARD_OVERRIDE},${FAMILY},${SUB_CHIP},${ARCH},${CROSS_COMPILE},${OUTPUT})

kernel:
	# Compiling kernel
	$(call build_kernel,${BOARD_OVERRIDE},${FAMILY},${ARCH},${CROSS_COMPILE},${DEFAULT_CONFIG},${OUTPUT})

image:
	# Creating image
	$(call build_image,${BOARD_OVERRIDE},${FAMILY},${P_VALUE},${ARCH})

all:
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot,${BOARD_OVERRIDE},${FAMILY},${SUB_CHIP},${ARCH},${CROSS_COMPILE},${OUTPUT})
	# Building linux package
	$(call build_kernel,${BOARD_OVERRIDE},${FAMILY},${ARCH},${CROSS_COMPILE},${DEFAULT_CONFIG},${OUTPUT})
	# Creating ROOTFS tarball
	$(call create_rootfs,${ROOTFS_ARCH})
	# Creating image
	$(call build_image,${BOARD_OVERRIDE},${FAMILY},${P_VALUE},)

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
