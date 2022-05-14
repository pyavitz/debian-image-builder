## Boards
```sh
Allwinner:      # NanoPi M1/NEO/NEO Plus2, OrangePi One/PC & Tritium
Amlogic:        # Le Potato, Odroid C4/N2/N2+ & Radxa Zero
Broadcom:       # Raspberry Pi 4B
Rockchip:       # NanoPC-T4, Odroid M1, Renegade & ROCKPro64
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
#### Config Menu
* Review the userdata.txt file for further options: locales, timezone and nameserver(s)
* 1 active | 0 inactive
```sh
Name:		# Your name
Username:       # Your username
Password:       # Your password
Branding:       # Set custom ASCII text banner
Hostname:       # Set custom system hostname

Distribution
Distro:         # Supported: debian, devuan and ubuntu
Release:	# Debian: bullseye, bookwork, testing, unstable and sid
		# Devuan: chimaera, daedalus, testing, unstable and ceres
		# Ubuntu: focal and jammy

U-Boot and Linux
U-Boot:         # Supported: v2022.04
Branch:         # Supported: 5.15 and current stable
Build:          # Kernel build version number
RC:             # Any x.y-rc
Menuconfig:     # Run uboot and kernel menuconfig
Compiler:       # Defaults at gcc-10 
Crosscompile:   # 1 cross compile | 0 native compile
Caching on:     # Ccache

Extra wireless support
rtl88XXau:      # Realtek 8812AU/21AU wireless support
rtl88XXbu:      # Realtek 88X2BU wireless support
rtl88XXcu:      # Realtek 8811CU/21CU wireless support
rtl8188eu:      # Realtek 8188EU wireless support

Customize
Defconfig:	# User defconfig
Name:		# Name of _defconfig (Must be placed in defconfig dir.)

User options
Logging:	# Logging > output/logs/$board-*.log (Menu interface only)
Verbosity:	# Verbose
Rootfs XL:	# Developer rootfs tarball
Compress img:	# Auto compress img > img.xz
User scripts:	# Review the README in the files/userscripts directory
User service:	# Create user during first boot (bypass the user information above)

Odroid N2/+
petitboot:	# Enable boot option
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
#### Miscellaneous
```sh
make cleanup    # Clean up image errors
make purge      # Remove sources directory
make purge-all  # Remove sources and output directory
```
#### Notes
* **Radxa Zero:** [Maskrom](https://wiki.radxa.com/Zero/dev/maskrom#Enable_maskrom) \ [Fastboot](https://wiki.radxa.com/Zero/dev/u-boot#Run_U-boot) \ [Wifi](https://github.com/pyavitz/debian-image-builder/tree/feature/patches/amlogic/rzero/wifi)

```sh
You need to erase Android from; or flash mainline u-boot to; the eMMC before booting from SD.
I found using fastboot to be the quickest and easiest method when using Linux.
```

*Put the device in maskrom mode and run `lsusb` to verify.*
```sh
Bus 001 Device 048: ID 1b8e:c003 Amlogic, Inc. GX-CHIP
```
*Run the following to flash mainline u-boot. When done. power down board, insert SD and power on.*
```sh
sudo apt update; sudo apt install -y python3-pip fastboot
sudo pip3 install pyamlboot --upgrade
wget -cq https://github.com/pyavitz/debian-image-builder/raw/feature/files/boot/rz-fastboot-loader.bin
wget -cq https://github.com/pyavitz/debian-image-builder/raw/feature/files/boot/rz-u-boot.bin
boot-g12.py rz-fastboot-loader.bin
sudo fastboot flash 0x200 rz-u-boot.bin
sudo fastboot reboot
```
* **Radxa Zero:** Rev. 1.5 LED Overlay
```sh
fdtoverlays ../amlogic/overlays/meson-g12a-gpio-10-led.dtbo
```
* **Amlogic:** G12A/B SM1 Audio
```sh
sudo amixer sset 'FRDDR_A SINK 1 SEL' 'OUT 1'
sudo amixer sset 'FRDDR_A SRC 1 EN' 'on'
sudo amixer sset 'TDMOUT_B SRC SEL' 'IN 0'
sudo amixer sset 'TOHDMITX I2S SRC' 'I2S B'
sudo amixer sset 'TOHDMITX' 'on'
sudo amixer sset 'FRDDR_B SINK 1 SEL' 'OUT 2'
sudo amixer sset 'FRDDR_B SRC 1 EN' 'on'
sudo amixer sset 'FRDDR_C SINK 1 SEL' 'OUT 3'
sudo amixer sset 'FRDDR_C SRC 1 EN' 'on'
sudo alsactl store
```

## Usage

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

# set static ip
MANUAL="n"			# Set to y to enable a static ip
IPADDR=""			# Static ip address
NETMASK=""			# Your Netmask
GATEWAY=""			# Your Gateway
NAMESERVERS=""			# Your preferred dns

For headless use: ssh user@ipaddress

Note:
You can also mount the ROOTFS partition and edit the following
files, whilst leaving rename_to_credentials.txt untouched.

/etc/opt/interfaces.manual
/etc/opt/wpa_supplicant.manual
```

#### Write to eMMC
```sh
Supported: Le Potato, NanoPi NEO Plus2, NanoPC-T4, Odroid C4/N2/N2+, Radxa Zero & Renegade
1. Attach eMMC module       # In some cases the module may need to be attached after boot
2. Boot from sdcard
3. Execute: sudo write2mmc
```
#### EEPROM

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
#### Overclocking
* **NanoPC-T4, Odroid N2/+, Radxa Zero & ROCKPro64**

If you decide to overclock the board, make sure you are using passive or active cooling.
```sh
echo "overclock=1" | sudo tee /etc/opt/overclock.txt
sudo service tweaks restart
```
#### Odroid N2 Fan Control
Install script and service
```sh
sudo wget https://raw.githubusercontent.com/pyavitz/scripts/master/fan-ctrl -P /usr/local/bin
sudo chmod +x $(command -v fan-ctrl)
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
   
A service runs 'fan-ctrl -r' during boot.
```

---

### Support

Should you come across any bugs, feel free to either open an issue on GitHub or talk with us directly by joining our channel on Libera; [`#arm-img-builder`](irc://irc.libera.chat/#arm-img-builder) or [Discord](https://discord.com/channels/898968760457781309/950159122031079524)
