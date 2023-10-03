# header
HEADER=./scripts/.header

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

# purge
PURGE=$(shell sudo rm -fdr sources)
PURGELOG=$(shell sudo rm -fdr output/logs)
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
	@${HEADER}
	@echo ""
	@echo "\e[1;37mCommand List:\e[0m"
	@echo "  make ccompile                Install x86_64 dependencies"
	@echo "  make ncompile                Install aarch64 dependencies"
	@echo "  make config                  Create user data file"
	@echo "  make menu                    Menu interface"
	@echo "  make cleanup                 Clean up rootfs / image errors"
	@echo "  make purge                   Remove sources directory"
	@echo "  make purge-all               Remove sources and output directory"
	@echo ""
	@echo "  make list                    List boards"
	@echo "  make uboot board=xxxx        Build u-boot package"
	@echo "  make kernel board=xxxx       Build linux kernel package"
	@echo "  make rootfs board=xxxx       Create rootfs tarball"
	@echo "  make image board=xxxx        Make bootable image"
	@echo "  make usbboot board=xxxx      Make bootable usb SDCARD image"
	@echo "  make all board=xxxx          Feeling lucky?"
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
	@rm -f override.txt
# ARCHITECTURE
ifdef arch
	@echo 'ARCH_EXT="$(arch)"' > override.txt
endif
# FORCE COMPILE
ifdef precompile
	@echo 'PRECOMPILED_UBOOT="$(precompile)"' >> override.txt
endif
# FORCE VERSION
ifdef force_version
	@echo 'FORCE_VERSION="$(force_version)"' >> override.txt
endif
# VERBOSE
ifdef verbose
	@$(shell sed -i "s/^VERBOSE=.*/VERBOSE="'"${verbose}"'"/" userdata.txt)
endif
	# Compiling u-boot
	$(call build_uboot)

kernel:
	@rm -f github.txt
	@rm -f override.txt
ifdef build
	@$(shell sed -i "s/^BUILD_VERSION=.*/BUILD_VERSION="'"${build}"'"/" userdata.txt)
endif
ifdef menuconfig
	@$(shell sed -i "s/^MENUCONFIG=.*/MENUCONFIG="'"${menuconfig}"'"/" userdata.txt)
endif
ifdef myconfig
	@$(shell sed -i "s/^CUSTOM_DEFCONFIG=.*/CUSTOM_DEFCONFIG="'"1"'"/" userdata.txt)
	@$(shell sed -i "s/^MYCONFIG=.*/MYCONFIG="'"${myconfig}_defconfig"'"/" userdata.txt)
endif
ifdef version
	@$(shell sed -i "s/^VERSION=.*/VERSION="'"${version}"'"/" userdata.txt)
endif
# GITHUB
ifdef github
	@echo "$(github)" > github.txt
endif
ifdef repo
	@echo "$(repo)" >> github.txt
endif
ifdef branch
	@echo "$(branch)" >> github.txt
endif
ifdef force_github
	@echo 'FORCE_GITHUB="$(force_github)"' > override.txt
endif
# ARCHITECTURE
ifdef arch
	@echo 'ARCH_EXT="$(arch)"' > override.txt
endif
# FORCE VERSION
ifdef force_version
	@echo 'FORCE_VERSION="$(force_version)"' >> override.txt
endif
# VERBOSE
ifdef verbose
	@$(shell sed -i "s/^VERBOSE=.*/VERBOSE="'"${verbose}"'"/" userdata.txt)
endif
	# Compiling kernel
	$(call build_kernel)

image:
	@rm -f override.txt
# DISTRO AND RELEASE
ifdef distro
	@$(shell sed -i "s/^DISTRO=.*/DISTRO="'"${distro}"'"/" userdata.txt)
endif
ifdef release
	@$(shell sed -i "s/^DISTRO_VERSION=.*/DISTRO_VERSION="'"${release}"'"/" userdata.txt)
endif
# ARCHITECTURE
ifdef arch
	@echo 'ARCH_EXT="$(arch)"' > override.txt
endif
# VERBOSE
ifdef verbose
	@$(shell sed -i "s/^VERBOSE=.*/VERBOSE="'"${verbose}"'"/" userdata.txt)
endif
	# Creating image
	$(call build_image)

all:
	# Compiling u-boot
	$(call build_uboot)
	# Compiling kernel
	$(call build_kernel)
	# Creating ROOTFS tarball
	$(call create_rootfs)
	# Creating image
	$(call build_image)

usbboot:
	# Creating usb image
	$(call build_usbboot)

# MISCELLANEOUS
menu:
	# Menu
	@chmod +x ${MENU}
	@chmod +x ${GMENU}
	@chmod +x ${RIT}
	@chmod +x ${LIT}
	@${MENU}

# USERDATA RESET
reset:
	# Resetting userdata.txt file
	@$(shell sed -i "s/^BUILD_VERSION=.*/BUILD_VERSION="'"1"'"/" userdata.txt)
	@$(shell sed -i "s/^MENUCONFIG=.*/MENUCONFIG="'"0"'"/" userdata.txt)
	@$(shell sed -i "s/^CUSTOM_DEFCONFIG=.*/CUSTOM_DEFCONFIG="'"0"'"/" userdata.txt)
	@$(shell sed -i "s/^MYCONFIG=.*/MYCONFIG="'"_defconfig"'"/" userdata.txt)

# LIST BOARDS
list:
	# Boards
	@ls lib/boards/

config:
	# Please be patient
	@chmod +x ${CONF}
	@${CONF}

dialogrc:
	# Setting builder theme
	@${DIALOGRC}

rootfs:
	@rm -f override.txt
# DISTRO AND RELEASE
ifdef distro
	@$(shell sed -i "s/^DISTRO=.*/DISTRO="'"${distro}"'"/" userdata.txt)
endif
ifdef release
	@$(shell sed -i "s/^DISTRO_VERSION=.*/DISTRO_VERSION="'"${release}"'"/" userdata.txt)
endif
# ARCHITECTURE
ifdef arch
	@echo 'ARCH_EXT="$(arch)"' > override.txt
endif
# VERBOSE
ifdef verbose
	@$(shell sed -i "s/^VERBOSE=.*/VERBOSE="'"${verbose}"'"/" userdata.txt)
endif
	# Root Filesystem
	$(call create_rootfs)

cleanup:
	@chmod +x ${CLN}
	@${CLEAN}

purge:
	# Removing sources directory
	@${PURGE}

purge-log:
	# Removing all logs
	@${PURGELOG}

purge-all:
	# Removing sources and output directory
	@${PURGEALL}
