# menu
MENU=./lib/dialog/menu
CONF=./lib/dialog/config
DIALOGRC=$(shell cp -f lib/dialogrc ~/.dialogrc)

# functions
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

# broadcom
RPI-IMG=./scripts/broadcom-stage1
RPI-IMAGE=sudo ./scripts/broadcom-stage1
RPI-STG2=./scripts/broadcom-stage2

# dependencies
CCOMPILE=./scripts/.ccompile
NCOMPILE=./scripts/.ncompile

# do not edit above this line

help:
	@echo ""
	@echo "\e[1;31m                  Debian Image Builder\e[0m"
	@echo "\e[1;37m                  ********************"
	@echo "Boards:\e[0m"
	@echo "  Allwinner:  nanopi pine64 tritium"
	@echo "  Amlogic:    lepotato odroidc4 odroidn2 odroidn2plus"
	@echo "  Broadcom:   raspi4"
	@echo "  Rockchip:   nanopc renegade rock64"
	@echo ""
	@echo "\e[1;37mCommand List:\e[0m"
	@echo "  make ccompile                Install all dependencies"
	@echo "  make ncompile                Install all native dependencies"
	@echo "  make config                  Create user data file"
	@echo "  make menu                    Menu interface"
	@echo "  make cleanup                 Clean up image errors"
	@echo "  make purge                   Remove sources directory"
	@echo "  make purge-all               Remove sources and output directory"
	@echo ""
	@echo "  make board-uboot             Make u-boot"
	@echo "  make board-kernel            Make linux kernel"
	@echo "  make rootfs                  Make rootfs tarball"
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
	@chmod +x ${XUBOOT}
	@${UBOOT}

tritium-kernel:
	# Compiling kernel
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

tritium-image:
	# Creating image
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

tritium-all:
	# T R I T I U M
	# - - - - - - - -
	# Compiling u-boot
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo tritium > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### PINEA64
pine64-uboot:
	# Compiling u-boot
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

pine64-kernel:
	# Compiling kernel
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

pine64-image:
	# Creating image
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

pine64-all:
	# P I N E 6 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo pine64 > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### NANOPI NEO PLUS2
nanopi-uboot:
	# Compiling u-boot
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

nanopi-kernel:
	# Compiling kernel
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

nanopi-image:
	# Creating image
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

nanopi-all:
	# N A N O  P I  N E O + 2
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo nanopi > board.txt
	@ echo allwinner >> board.txt
	@chmod +x ${ALL-IMG}
	@chmod +x ${ALL-STG2}
	@${ALL-IMAGE}

### ODROID C4
odroidc4-uboot:
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidc4-kernel:
	# Compiling kernel
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

odroidc4-image:
	# Creating image
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

odroidc4-all:
	# O D R O I D  C 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

### ODROID HC4
odroidhc4-uboot:
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidhc4-kernel:
	# Compiling kernel
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

odroidhc4-image:
	# Creating image
	@ echo odroidhc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

odroidhc4-all:
	# O D R O I D  H C 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidhc4 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

### ODROID N2
odroidn2-uboot:
	# Compiling u-boot
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidn2-kernel:
	# Compiling kernel
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	
odroidn2-image:
	# Creating image
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

odroidn2-all:
	# O D R O I D  N 2
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidn2 > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

### ODROID N2 Plus
odroidn2plus-uboot:
	# Compiling u-boot
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

odroidn2plus-kernel:
	# Compiling kernel
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	
odroidn2plus-image:
	# Creating image
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

odroidn2plus-all:
	# O D R O I D  N 2  P L U S
	# - - - - - - - -
	# Compiling u-boot
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo odroidn2plus > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

### LE POTATO
lepotato-uboot:
	# Compiling u-boot
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

lepotato-kernel:
	# Compiling kernel
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

lepotato-image:
	# Creating image
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

lepotato-all:
	# L E P O T A T O
	# - - - - - - - -
	# Compiling u-boot
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo lepotato > board.txt
	@ echo amlogic >> board.txt
	@chmod +x ${AML-IMG}
	@chmod +x ${AML-STG2}
	@${AML-IMAGE}

# RENEGADE
renegade-uboot:
	# Compiling u-boot
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

renegade-kernel:
	# Compiling kernel
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

renegade-image:
	# Creating image
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

renegade-all:
	# R E N E G A D E
	# - - - - - - - -
	# Compiling u-boot
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo renegade > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
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
	@chmod +x ${XUBOOT}
	@${UBOOT}

rock64-kernel:
	# Compiling kernel
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

rock64-image:
	# Creating image
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

rock64-all:
	# R O C K 6 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo rock64 > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

# NANOPC-T4
nanopc-uboot:
	# Compiling u-boot
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

nanopc-kernel:
	# Compiling kernel
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

nanopc-image:
	# Creating image
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

nanopc-all:
	# N A N O P C - T 4
	# - - - - - - - -
	# Compiling u-boot
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo nanopc > board.txt
	@ echo rockchip >> board.txt
	@chmod +x ${ROC-IMG}
	@chmod +x ${ROC-STG2}
	@${ROC-IMAGE}

# RASPBERRY PI 4B
raspi4-uboot:
	# Compiling u-boot
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}

raspi4-kernel:
	# Compiling kernel
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}

raspi4-image:
	# Creating image
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@chmod +x ${RPI-IMG}
	@chmod +x ${RPI-STG2}
	@${RPI-IMAGE}

raspi4-all:
	# R A S P B E R R Y  P I  4 B
	# - - - - - - - -
	# Compiling u-boot
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@chmod +x ${XUBOOT}
	@${UBOOT}
	# Building linux package
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@chmod +x ${XKERNEL}
	@${KERNEL}
	# Creating ROOTFS tarball
	@chmod +x ${RFS}
	@${ROOTFS}
	# Creating image
	@ echo bcm2711 > board.txt
	@ echo broadcom >> board.txt
	@chmod +x ${RPI-IMG}
	@chmod +x ${RPI-STG2}
	@${RPI-IMAGE}

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
	# Rootfs: arm64
	@chmod +x ${RFS}
	@${ROOTFS}

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
