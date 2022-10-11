## Boards
```sh
Allwinner:      # NanoPi M1/NEO/Plus2, OrangePi One/PC/R1 & Tritium
Amlogic:        # Le Potato, Odroid C4/HC4/N2/N2+ & Radxa Zero
Broadcom:       # Raspberry Pi 0W/3B+/4B
Rockchip:       # NanoPC-T4, Renegade, ROCK64 & ROCKPro64
Samsung:        # Odroid XU4

WIP:		# Cubietruck, Cubox-i, Banana Pi M5, NanoPi R4S/R4SE/R5S
		# Odroid C1/M1 & Pinebook Pro
```

### Dependencies for Debian Bullseye and Ubuntu Jammy Jellyfish
* **Recommended host:** Debian Bullseye

**Install options:**
* Run the `./install` script ***(recommended)***
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
#### Config Menu ([Linux kernel options](https://github.com/pyavitz/debian-image-builder/wiki/Building-vendor-kernels))
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
Network Manager		# Enable nmtui (default: ifupdown)

U-Boot and Linux
U-Boot:			# Supported: v2022.07
Branch:			# Supported: 5.15 and current stable
Build:			# Kernel build version number
Menuconfig:		# Run uboot and kernel menuconfig
Compiler:		# Defaults at gcc-10 
Crosscompile:		# 1 cross compile | 0 native compile
Caching on:		# Ccache
Standard packaging:	# If unsure, leave enabled (Review wiki for difference)

Extra wireless support
rtl88XXau:		# Realtek 8812AU/21AU wireless support
rtl88XXbu:		# Realtek 88X2BU wireless support
rtl88XXcu:		# Realtek 8811CU/21CU wireless support
rtl8188eu:		# Realtek 8188EU wireless support

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

Odroids
petitboot:		# Enable boot option
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
#### Aircrack (rtl88XXau)

```sh
# change from 0 to 1
aircrack=0
```
#### Amlogic eMMC boot (u-boot only)
```sh
# change from 0 to 1
emmc=0
```
## Usage
* Review the [Wiki](https://github.com/pyavitz/debian-image-builder/wiki)
#### /boot/rename_to_useraccount.txt
* Headless: rename file to useraccount.txt and fill in the variables
* Headful: don't rename file & get prompted to create a user account
```sh
NAME=""
USERNAME=""
PASSWORD=""
```

#### /boot/rename_to_credentials.txt
```sh
Rename file to credentials.txt and input your wifi information.

SSID=""				# Service set identifier
PASSKEY=""			# Wifi password
COUNTRYCODE=""			# Your country code

# set static ip (ifupdown)
MANUAL="n"			# Set to y to enable a static ip
IPADDR=""			# Static ip address
NETMASK=""			# Your Netmask
GATEWAY=""			# Your Gateway
NAMESERVERS=""			# Your preferred dns

# set static ip (network-manager)
MANUAL="n"			# Set to y to enable a static ip
IPADDR=""			# Static ip address
GATEWAY=""			# Your Gateway
DNS=""				# Your preferred dns

For headless use: ssh user@ipaddress
```
#### System menu config (`menu-config`)
<img src="https://i.imgur.com/xLdvDCV.png" alt="Menu Interface: System Settings" />

---

### Support

Should you come across any bugs, feel free to either open an issue on GitHub or talk with us directly by joining our channel on Libera; [`#arm-img-builder`](irc://irc.libera.chat/#arm-img-builder) or [Discord](https://discord.gg/mypJ7NW8BG)
