<details>
<summary><h3>Boards</h3></summary>

```py
(*) Work in progress

# ALLWINNER
BananaPi BPI-M2-Zero
BananaPi BPI-M4-Zero
FriendlyElec NanoPi M1
KickPi K2B
OrangePi 3 LTS

# AMLOGIC
BananaPi BPI-CM4
BananaPi BPI-M2S
H96-MAX X3
Libre Computer Le Potato
Odroid C4
Odroid N2
Odroid N2L
Odroid N2+
Radxa Zero
X96-AIR	GBIT / QCOM

# ROCKCHIP
FriendlyElec NanoPi R3S / LTS
Radxa Zero 3W

# SAMSUNG
Odroid XU4

# SPACEMIT
BananaPi BPI-F3
```
</details>

### Supported Hosts: Debian Trixie and Ubuntu Noble Numbat
* **Debian Trixie** (arm arm64 riscv) (testing)
* **Ubuntu Noble Numbat** (arm arm64 riscv) (recommended)

**Install options:**
* Run the `./install.sh` script (recommended)
* Run builder [make commands](https://github.com/pyavitz/debian-image-builder#install-dependencies) (dependency: make)

---

## Instructions

#### Install dependencies

```sh
make ccompile			# x86_64
make ncompile			# aarch64
```

#### Menu interface

```sh
make config				# Create user data file
make menu				# Open menu interface
make dialogrc			# Set builder theme (optional)
```
#### Miscellaneous
```sh
make clean				# Clean up image errors
make purge				# Remove sources directory
make purge-all			# Remove sources and output directory
```
#### Config Menu
* [Profile options](https://github.com/pyavitz/debian-image-builder/commit/1c3e78652c4dfa06c1bb64d31e60f8dfb5145bec)

---

* Review the userdata.txt file for further options: locales, timezone and nameserver(s)
* 1 active | 0 inactive
```sh
Name:					# Your name
Username:				# Your username
Password:				# Your password
Hostname:				# Set custom system hostname

Distribution
Distro:					# Supported: debian, devuan and ubuntu
Release:				# Debian: bookworm, trixie, testing, unstable and sid
						# Devuan: daedalus, excalibur, testing, unstable and ceres
						# https://www.devuan.org/os/announce/excalibur-usrmerge-announce-2024-02-20.html
						# Kali: kali-last-snapshot and kali-rolling
						# Ubuntu: focal, jammy, noble, etc ...
Network Manager			# 1 networkmanager | 0 ifupdown

U-Boot and Linux
U-Boot:					# Default: v2025.01
Branch:					# Default: 6.12.y
Build:					# Kernel build version number
Menuconfig:				# Run uboot and kernel menuconfig
Compiler:				# GNU Compiler Collection / Clang
Ccache:					# Compiler cache

Customize
Defconfig:				# User defconfig
Name:					# Name of _defconfig (must be placed in defconfig dir.)

User options
Logging:				# Logging > output/logs/$board-*.log (Menu interface only)
Verbosity:				# Verbose
Devel Rootfs:			# Developer rootfs tarball
Compress img:			# Auto compress img > img.xz
User scripts:			# Review the README in the files/userscripts directory
User service:			# Create user during first boot (bypass the user information above)
```

#### Customize image
* custom.txt
```sh
# Boot Partition
ENABLE_VFAT="false"

# Root Filesystem Types: ext4 btrfs xfs
FSTYPE="ext4"

# UEFI Options (WIP)
ENABLE_EFI="false"

# Image Size: 4096MB
IMGSIZE="4096MB"

# Shrink Image
ENABLE_SHRINK="true"

# Petitboot (AML ODROID) (not recommended)
ENABLE_PETITBOOT="false"

# Compression Types: xz zst
IMG_COMPRESSION="xz"
```
#### User patches

```sh
Patches "-p1" placed in patches/userpatches are applied during compilation.
```

## Usage
* Review the [Wiki](https://github.com/pyavitz/debian-image-builder/wiki)
#### /boot/useraccount.txt
* Headless: ENABLE="true" and fill in the variables (recommended)
* Headful: ENABLE="false" and get prompted to create a user account
```sh
ENABLE="false"			# Enable service
NAME=""					# Your name
USERNAME=""				# Username
PASSWORD=""				# Password
```

#### /boot/credentials.txt
```sh
Set to ENABLE="true" and input your wifi information.
ENABLE="false"			# Enable service

SSID=""					# Service set identifier
PASSKEY=""				# Wifi password
COUNTRYCODE=""			# Your country code

# set static ip (ifupdown)
MANUAL="false"			# Set to true to enable a static ip
IPADDR=""				# Static ip address
NETMASK=""				# Your Netmask
GATEWAY=""				# Your Gateway
NAMESERVERS=""			# Your preferred dns

# set static ip (network-manager)
MANUAL="false"			# Set to true to enable a static ip
IPADDR=""				# Static ip address
GATEWAY=""				# Your Gateway
DNS=""					# Your preferred dns

# change hostname
HOSTNAME="$CURRENT"		# Hostname

For headless use: ssh user@ipaddress
```
#### System settings (`menu-config`)
<img src="https://i.imgur.com/oKDPNA1.png" alt="Menu Interface: System Settings" />

---

### Support

Should you come across any bugs, feel free to either open an issue on GitHub or talk with us directly by joining our channel on Libera; [`#arm-img-builder`](irc://irc.libera.chat/#arm-img-builder) or [Discord](https://discord.gg/mypJ7NW8BG)
