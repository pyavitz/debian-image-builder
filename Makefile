# menu
MENU=./lib/dialog/menu
CONF=./lib/dialog/config
DIALOGRC=$(shell cp -f lib/dialogrc ~/.dialogrc)

# misc
RFS=./scripts/rootfs
ROOTFS=sudo ./scripts/rootfs
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

# boards
BOARDS=$(shell cp lib/boards/${board} board.txt)

ifdef board
include lib/boards/${board}
endif

define build_uboot
	@${BOARDS}
	@chmod +x ${XUBOOT}
	@${UBOOT}
endef

define build_kernel
	@${BOARDS}
	@chmod +x ${XKERNEL}
	@${KERNEL}
endef

define build_image
	@${BOARDS}
	@chmod +x ${IMG}
	@chmod +x ${STG2}
	@${IMAGE}
endef

define create_rootfs
	@${BOARDS}
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
	@echo "  Rockchip:   nanopc renegade rock64 rockpro64"
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
	@echo "  make uboot board=XXX         Build u-boot"
	@echo "  make kernel board=XXX        Build linux kernel package"
	@echo "  make rootfs board=XXX        Create rootfs tarball"
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
	# Compiling u-boot
	$(call build_uboot)

kernel:
	# Compiling kernel
	$(call build_kernel)

image:
	# Creating image
	$(call build_image)

all:
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot)
	# Building linux package
	$(call build_kernel)
	# Creating ROOTFS tarball
	$(call create_rootfs)
	# Creating image
	$(call build_image)

### MISCELLANEOUS
menu:
	# Menu
	@ if [ -e ./output ]; then :; else mkdir -p ./output;fi;
	@ sudo rm -f ./output/make.log
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
	$(call create_rootfs)
	
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
