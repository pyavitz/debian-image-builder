## Supported boards
```sh
Allwinner:      # NanoPi NEO Plus2, Pine A64+ and Tritium
Amlogic:        # Le Potato and Odroid C4/N2/N2+
Broadcom:       # Raspberry Pi 4B
Rockchip:       # NanoPC-T4, Renegade and Rock64
```
### Dependencies for Ubuntu Focal AMD64/x86_64

```sh
sudo apt install \
	build-essential bison bc git dialog patch dosfstools zip unzip qemu parted \
	debootstrap qemu-user-static rsync kmod cpio flex libssl-dev libncurses5-dev \
	device-tree-compiler libfdt-dev python3-distutils python3-dev swig fakeroot \
	lzop lz4 aria2 pv toilet figlet crossbuild-essential-arm64 gcc-arm-none-eabi \
	distro-info-data lsb-release python python-dev kpartx gcc-8 gcc-9 gcc-10 make \
	gcc-8-aarch64-linux-gnu gcc-9-aarch64-linux-gnu gcc-10-aarch64-linux-gnu \
	debian-archive-keyring debian-keyring python-setuptools python3-setuptools \
	python-distutils-extra libelf-dev
```

### Docker

To build using [Docker](https://www.docker.com/), follow the install [instructions](https://docs.docker.com/engine/install/) and use our other [builder](https://github.com/pyavitz/arm-img-builder).

---

## Instructions

#### Install dependencies

```sh
make ccompile   # Cross compile
make ncompile   # Native compile
```

#### Menu interface

```sh
make config     # Create user data file
make menu       # Open menu interface
make dialogrc   # Set builder theme (optional)
```
#### Config Menu [(Odroid Options)](https://github.com/pyavitz/debian-image-builder/commit/ae51e22b5889120aac17b2e4f0e836341d05b37d)

```sh
Name:		# Whats your name?
Username:       # Your username
Password:       # Your password
Branding:       # Set ASCII text banner
Hostname:       # Set system hostname

Distribution
Distro:         # Supported: debian and ubuntu
Release:	# Debian: buster, bullseye, unstable and sid
		# Ubuntu: focal and hirsute

U-Boot and Linux
U-Boot:         # Supported: v2021.04, v2021.07
Branch:         # Supported: 5.10 (Note: If building for Odroids please review options)
Build:          # Kernel build version number
RC:             # 1 for any x.y-rc
Menuconfig:     # 1 to run uboot and kernel menuconfig
Crosscompile:   # 1 to cross compile | 0 to native compile
Caching on:     # 1 to enable ccache

Extra wireless support
rtl88XXau:      # 1 to add Realtek 8812AU/21AU wireless support
rtl88XXbu:      # 1 to add Realtek 88X2BU wireless support
rtl88XXcu:      # 1 to add Realtek 8811CU/21CU wireless support

Customize (user defconfig)
Defconfig:	# 1 to enable
Name:		# Name of _defconfig (Must be placed in defconfig dir.)
```

#### User defconfig

```sh
# config placement: defconfig/$NAME_defconfig
The config menu will append _defconfig to the end of the name
in the userdata.txt file.
```
#### Aircrack (rtl88XXau)

```sh
nano userdata.txt
# change from 0 to 1
aircrack=0
```
#### Compression (turn off)
```sh
nano userdata.txt
# change from 1 to 0
auto=1        # compresses to img.xz
```
#### Odroid N2/Plus eMMC
```sh
nano userdata.txt
# change from 0 to 1
emmc=0
```
#### Miscellaneous

```sh
make cleanup    # Clean up image errors
make purge      # Remove sources directory
make purge-all  # Remove sources and output directory
```

## Usage

#### /boot/rename_to_credentials.txt
```sh
Rename file to credentials.txt and input your wifi information.

SSID=" "			# Service set identifier
PASSKEY=" "			# Wifi password
COUNTRYCODE=" "			# Your country code

# set static ip
MANUAL=n			# Set to y to enable a static ip
IPADDR=" "			# Static ip address
NETMASK=" "			# Your Netmask
GATEWAY=" "			# Your Gateway
NAMESERVERS=" "			# Your preferred dns

For headless use: ssh user@ipaddress

Note:
You can also mount the ROOTFS partition and edit the following
files, whilst leaving rename_to_credentials.txt untouched.

/etc/opt/interfaces.manual
/etc/opt/wpa_supplicant.manual
```

#### Write to eMMC
```sh
Supported: Le Potato, Odroid C4, Odroid N2/Plus, NanoPi NEO Plus2 and NanoPC-T4
1. Attach eMMC module       # In some cases the module may need to be attached after boot
2. Boot from sdcard
3. Execute: sudo write2mmc
```
#### Deb EEPROM ([usb_storage.quirks](https://github.com/pyavitz/rpi-img-builder/issues/17))

```sh
Raspberry Pi 4B EEPROM Helper Script
Usage: deb-eeprom -h

   -U       Upgrade eeprom package
   -w       Transfer to USB	# Supported: EXT4, BTRFS and F2FS
   -u       Update script

Note:
Upon install please run 'deb-eeprom -u' before using this script.
```
#### Simple wifi helper
```sh
swh -h

   -s       Scan for SSID's
   -u       Bring up interface
   -d       Bring down interface
   -r       Restart interface
   -W       Edit wpa supplicant
   -I       Edit interfaces
```
#### CPU frequency scaling
```sh
governor -h

   -c       Conservative
   -o       Ondemand
   -p       Performance

   -r       Run
   -u       Update
   
A systemd service runs 'governor -r' during boot.
```
#### Odroid N2 Fan Control
Install script and service
```sh
sudo wget https://raw.githubusercontent.com/pyavitz/scripts/master/fan-ctrl -P /usr/local/bin
sudo chmod +x /usr/local/bin/fan-ctrl
```
```sh
sudo tee /etc/systemd/system/odroid-fan-ctrl.service <<EOF
[Unit]
Description=Odroid Fan Control
ConditionPathExists=/usr/local/bin/fan-ctrl

[Service]
ExecStart=/usr/local/bin/fan-ctrl -r &>/dev/null
Type=oneshot
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF
sudo systemctl enable odroid-fan-ctrl
```
Set trip point and run `fan-ctrl -r`
```sh
Odroid N2 Trip Point
Usage: fan-ctrl -h

   -1       65°C
   -2       55°C
   -3       45°C
   -4       35°C

   -r       Run
   -u       Update
   
A systemd service runs 'fan-ctrl -r' during boot.
```
