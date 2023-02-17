# menu
MENU=./lib/dialog/menu
GMENU=./lib/dialog/general
CONF=./lib/dialog/config
DIALOGRC=$(shell cp -f lib/dialogrc ~/.dialogrc)

# misc
RFS=./scripts/rootfs
RFSX=./scripts/rootfs-extra
ROOTFS=sudo ./scripts/rootfs
CLN=./scripts/clean
CLEAN=sudo ./scripts/clean

PURGE=$(shell sudo rm -fdr sources)
PURGEALL=$(shell sudo rm -fdr sources output)

# logger
RIT=./scripts/runit
LIT=./scripts/loggit

# uboot and linux
XUBOOT=./scripts/uboot
UBOOT=sudo ./scripts/uboot
XKERNEL=./scripts/linux
KERNEL=sudo ./scripts/linux

# image
IMG=./scripts/stage1
IMAGE=sudo ./scripts/stage1
STG2=./scripts/stage2
XUSB=./scripts/usbboot
USBBOOT=sudo ./scripts/usbboot

# dependencies
CCOMPILE=./scripts/.ccompile
NCOMPILE=./scripts/.ncompile

# BOARDS
BOARDS=$(shell sudo cp lib/boards/${board} board.txt)

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
	@chmod +x ${RFSX}
	@${ROOTFS}
endef 

define build_usbboot
	@${BOARDS}
	@chmod +x ${XUSB}
	@${USBBOOT}
endef

# USAGE
help:
	@echo ""
	@echo "\t\t\t\e[1;31mDebian Image Builder\e[0m"
	@echo "\t\t\t\e[1;37m********************\e[0m"
	@echo "\e[1;37mBoards:\e[0m"
	@echo "  Allwinner:  nanopim1 nanopineo nanopineoplus2 orangepione orangepipc"
	@echo "              orangepir1 pinea64plus tritium"
	@echo "  Amlogic:    lepotato odroidc4 odroidhc4 odroidn2 odroidn2plus radxazero"
	@echo "  Broadcom:   raspizero raspi2 raspi3 raspi3-32b raspi4"
	@echo "  Rockchip:   nanopct4 renegade rock64 rockpro64"
	@echo "  Samsung:    odroidxu4"
	@echo ""
	@echo "  WIP:        cubietruck cuboxi bananapim5 bananapip2zero khadasedge2"
	@echo "              nanopir4s nanopir4se nanopir5s odroidc1 odroidm1 orangepi5"
	@echo "              pinebookpro rock5b tinker x96air"
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

# DEPENDENCIES
ccompile:
	# Installing cross dependencies:
	@chmod +x ${CCOMPILE}
	@${CCOMPILE}

ncompile:
	# Installing native dependencies:
	@chmod +x ${NCOMPILE}
	@${NCOMPILE}


# COMMANDS
uboot: 
	# Compiling u-boot
	$(call build_uboot)

kernel: 
	@rm -f github.txt
ifdef version
	@$(shell sed -i "12s/.*/VERSION="'"${version}"'"/" userdata.txt)
endif

ifdef myconfig
	@$(shell sed -i "24s/.*/custom_defconfig=1/" userdata.txt)
	@$(shell sed -i "25s/.*/MYCONFIG="'"${myconfig}_defconfig"'"/" userdata.txt)
endif

# GITHUB
ifdef repo
	@echo "$(repo)" > github.txt
endif
ifdef branch
	@echo "$(branch)" >> github.txt
endif
	# Compiling kernel
	$(call build_kernel)

image:
	# Creating image
	$(call build_image)

all: 
	# - - - - - - - -
	# Compiling u-boot
	$(call build_uboot)
	# Compiling kernel
	$(call build_kernel)
	# Creating ROOTFS tarball
	$(call create_rootfs)
	# Creating image
	$(call build_image)

usbboot:
	# Creating image
	$(call build_usbboot)

# MISCELLANEOUS
menu: 
	# Menu
	@chmod +x ${MENU}
	@chmod +x ${GMENU}
	@chmod +x ${RIT}
	@chmod +x ${LIT}
	@${MENU}

# reset myconfig to 0
myconfig_0:
	@$(shell sed -i "24s/.*/custom_defconfig=0/" userdata.txt)

config:
	# Please be patient
	@chmod +x ${CONF}
	@${CONF}

dialogrc:
	# Setting builder theme
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
