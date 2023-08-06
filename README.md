## Notice
* Requested [images](https://github.com/pyavitz/binary/releases/tag/images)
* Overlay [changes](https://github.com/pyavitz/debian-image-builder/commit/67eafb34cedd24cb68da57ac318f85b94ec4af86)

## Boards
```sh
Allwinner:      # NanoPi M1/NEO/Plus2/R1, OrangePi One/PC/R1 and Tritium
Amlogic:        # BananaPi CM4IO/M2S, Le Potato, Odroid C4/HC4/N2/N2L/N2+ and Radxa Zero
Broadcom:       # Raspberry Pi 0W/2B/3B+/4B
Rockchip:       # NanoPC-T4, Odroid M1, Renegade, ROCK64, ROCKPro64 and Tinkerboard
Samsung:        # Odroid XU4

WIP:		# Cubietruck, Cubox-i, BananaPi M2Pro/M5 M2/P2 Zero, Khadas Edge2
		# NanoPi R4S/R4SE/R5S/R5C, Odroid C1, OrangePi 5, Pinebook Pro
		# Indiedroid Nova, and Radxa E25/Rock5b
```

### Host dependencies for Debian Bullseye / Bookworm and Ubuntu Jammy Jellyfish
* **Debian Bullseye** (recommended)
* **Debian Bookworm** (testing)
* **Ubuntu Jammy Jellyfish** (recommended)

**Install options:**
* Run the `./install.sh` script (recommended)
* Run builder [make commands](https://github.com/pyavitz/debian-image-builder#install-dependencies) (dependency: make)
* Review [package list](https://raw.githubusercontent.com/pyavitz/debian-image-builder/feature/lib/.package.list) and install manually

---

## Instructions

#### Install dependencies

```sh
make ccompile   # x86_64
make ncompile   # aarch64
```

#### Menu interface

```sh
make config     # Create user data file
make menu       # Open menu interface
make dialogrc   # Set builder theme (optional)
```
#### Miscellaneous
```sh
make cleanup    # Clean up image errors
make purge      # Remove sources directory
make purge-all  # Remove sources and output directory
```
#### Config Menu
* [Profile options](https://github.com/pyavitz/debian-image-builder/wiki)
* [Vendor kernel options](https://github.com/pyavitz/debian-image-builder/wiki/Building-vendor-kernels)

---

* Review the userdata.txt file for further options: locales, timezone and nameserver(s)
* 1 active | 0 inactive
```sh
Name:			# Your name
Username:		# Your username
Password:		# Your password
Hostname:		# Set custom system hostname

Distribution
Distro:			# Supported: debian, devuan and ubuntu
Release:		# Debian: bullseye, bookworm, testing, unstable and sid
			# Devuan: chimaera, daedalus, testing, unstable and ceres
			# Ubuntu: focal and jammy
Network Manager		# 1 networkmanager | 0 ifupdown

U-Boot and Linux
U-Boot:			# Supported: v2023.01
Branch:			# Supported: 5.15 / 6.1 and "maybe" current stable
Build:			# Kernel build version number
Menuconfig:		# Run uboot and kernel menuconfig
Compiler:		# GNU Compiler Collection
Caching on:		# Ccache

Customize
Defconfig:		# User defconfig
Name:			# Name of _defconfig (must be placed in defconfig dir.)

User options
Logging:		# Logging > output/logs/$board-*.log (Menu interface only)
Verbosity:		# Verbose
Devel Rootfs:		# Developer rootfs tarball
Compress img:		# Auto compress img > img.xz
User scripts:		# Review the README in the files/userscripts directory
User service:		# Create user during first boot (bypass the user information above)
```

#### Customize image
* custom.txt
```sh
# Boot Partition: true false
ENABLE_VFAT="false"

# Root Filesystem Types: ext4 btrfs xfs
FSTYPE="ext4"

# UEFI Options: true false
ENABLE_EFI="false"

# Shrink Image (EXT4 Only): true false
ENABLE_SHRINK="true"

# Petitboot (ODROID): true false
ENABLE_PETITBOOT="false"

# Compression Types: xz zst
IMG_COMPRESSION="xz"
```
#### User defconfig

```sh
# config placement: defconfig/$NAME_defconfig
The config menu will append _defconfig to the end of the name in the
userdata.txt file.
```
#### User patches

```sh
Patches "-p1" placed in patches/userpatches are applied during compilation.
```
#### Amlogic eMMC boot (u-boot only)

```sh
# change from 0 to 1
EMMC=0
```

## Usage
* Review the [Wiki](https://github.com/pyavitz/debian-image-builder/wiki)
#### /boot/useraccount.txt
* Headless: ENABLE="true" and fill in the variables (recommended)
* Headful: ENABLE="false" and get prompted to create a user account
```sh
ENABLE="false"			# Enable service
NAME=""				# Your name
USERNAME=""			# Username
PASSWORD=""			# Password
```

#### /boot/credentials.txt
```sh
Set to ENABLE="true" and input your wifi information.
ENABLE="false"			# Enable service

SSID=""				# Service set identifier
PASSKEY=""			# Wifi password
COUNTRYCODE=""			# Your country code

# set static ip (ifupdown)
MANUAL="false"			# Set to true to enable a static ip
IPADDR=""			# Static ip address
NETMASK=""			# Your Netmask
GATEWAY=""			# Your Gateway
NAMESERVERS=""			# Your preferred dns

# set static ip (network-manager)
MANUAL="false"			# Set to true to enable a static ip
IPADDR=""			# Static ip address
GATEWAY=""			# Your Gateway
DNS=""				# Your preferred dns

# change hostname
CHANGE="false"			# Set to true to change hostname
HOSTNAME="$CURRENT"		# Hostname

For headless use: ssh user@ipaddress
```
#### System settings (`menu-config`)
<img src="https://i.imgur.com/oKDPNA1.png" alt="Menu Interface: System Settings" />

---

### Support

Should you come across any bugs, feel free to either open an issue on GitHub or talk with us directly by joining our channel on Libera; [`#arm-img-builder`](irc://irc.libera.chat/#arm-img-builder) or [Discord](https://discord.gg/mypJ7NW8BG)
