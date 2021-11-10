## Boards
```sh
Allwinner:      # NanoPi NEO Plus2, Pine A64+ and Tritium
Amlogic:        # Le Potato and Odroid C4/N2/N2+
Broadcom:       # Raspberry Pi 4B
Rockchip:       # NanoPC-T4, Renegade and Rock64
```

### Dependencies for Debian Bullseye and Ubuntu Focal / Jammy Jellyfish

**Install options:**
* Run the `./install` script ***(recommended)***
* Run builder [make commands](https://github.com/pyavitz/debian-image-builder#install-dependencies) (dependency: make)
* Review [package list](https://raw.githubusercontent.com/pyavitz/debian-image-builder/feature/lib/.package.list) and install manually

### Docker (obsolete)

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
#### Config Menu [(Odroid Options)](https://github.com/pyavitz/debian-image-builder/blob/feature/patches/amlogic/odroid/README.md)

```sh
Name:		# Whats your name?
Username:       # Your username
Password:       # Your password
Branding:       # Set ASCII text banner
Hostname:       # Set system hostname

Distribution
Distro:         # Supported: debian, devuan and ubuntu
Release:	# Debian: bullseye, testing, unstable and sid
		# Devuan: chimaera, testing, unstable and ceres
		# Ubuntu: focal, impish and jammy

U-Boot and Linux
U-Boot:         # Supported: v2021.10 (Note: Use v2021.07 for Rockchip)
Branch:         # Supported: 5.15 (Note: If building for Odroids please review options)
Build:          # Kernel build version number
RC:             # 1 for any x.y-rc
Menuconfig:     # 1 to run uboot and kernel menuconfig
Compiler:       # Defaults at gcc-10
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
#### User patches

```sh
Patches "-p1" placed in patches/userpatches are applied during compilation.
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
#### Odroid N2/+ eMMC
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
Supported: Le Potato, Odroid C4/N2/N2+, NanoPi NEO Plus2 and NanoPC-T4
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
   -s       Schedutil

   -r       Run
   -u       Update
   
A systemd service runs 'governor -r' during boot.
```
#### Overclocking (NanoPC-T4 & Odroid N2/+)
By default the NanoPC-T4 and Odroid N2/+ run slightly under clocked as a safety precaution. If you decide to overclock the board, make sure you are using passive and active cooling.
```sh
echo "overclock=1" | sudo tee /etc/opt/overclock.txt
sudo service tweaks restart
```
#### Odroid N2 Fan Control
Install script and service
```sh
sudo wget https://raw.githubusercontent.com/pyavitz/scripts/master/fan-ctrl -P /usr/local/bin
sudo chmod +x /usr/local/bin/fan-ctrl
```
**Debian / Ubuntu**
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
**Devuan**
```sh
sudo wget -cq https://raw.githubusercontent.com/pyavitz/scripts/master/fan-ctrl.init -P /etc/init.d -O /etc/init.d/fan-ctrl
sudo chmod +x /etc/init.d/fan-ctrl
sudo update-rc.d fan-ctrl defaults 2
```
Set trip point
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

---

### Support

Should you come across any bugs, feel free to either open an issue on GitHub or talk with us directly by joining our channel on Libera; [`#arm-img-builder`](irc://irc.libera.chat/#arm-img-builder)
