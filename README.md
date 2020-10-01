## Supported boards
```sh
Allwinner:      # NanoPi NEO Plus2, Orange Pi R1, Pine A64+ and Tritium
Amlogic:        # Le Potato, Odroid C4 and Odroid N2
broardcom:      # Raspberry Pi 4B
Rockchip:       # NanoPC-T4, Renegade and Rock64
```
### Dependencies for Debian Buster AMD64/x86_64 cross compile

```sh
sudo apt install build-essential bison bc git dialog patch dosfstools zip unzip qemu parted \ 
                 debootstrap qemu-user-static rsync kmod cpio flex libssl-dev libncurses5-dev \
                 device-tree-compiler libfdt-dev python3-distutils python3-dev swig fakeroot \
                 lzop lz4 aria2 pv toilet crossbuild-essential-arm64 gcc-arm-none-eabi
                 
Orange Pi R1 - sudo apt install -y crossbuild-essential-armhf
```
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
#### Config Menu

```sh
Username:       # Your username
Password:       # Your password
Branding:       # Set ASCII text banner
Hostname:       # Set the system's host name
Debian:         # Supported: buster and unstable
Branch:         # Supported: 5.8.y and above
Mainline:       # 1 for any x.y-rc
Menuconfig:     # 1 to run uboot and kernel menuconfig
Crosscompile:   # 1 to cross compile | 0 to native compile
```
#### User defconfig
```sh
nano userdata.txt
# place config in defconfig directory
custom_defconfig=1
MYCONFIG="nameofyour_defconfig"
```
#### Odroid N2 eMMC
```sh
nano userdata.txt
# change from 0 to 1
emmc=1
```
#### Miscellaneous

```sh
make cleanup    # Clean up image errors
make purge      # Remove sources directory
```

## Usage

#### Write to eMMC
```sh
Supported: Le Potato, Odroid C4, Odroid N2 and NanoPi NEO Plus2
1. Attach eMMC module       # In some cases the module may need to be attached after boot
2. Boot from sdcard
3. Execute: sudo write2mmc
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

#### Install wifi drivers
```sh
wifidrv -h

   -1     rtl8812au (aircrack)
   -2     rtl88x2bu (cilynx)
   -3     rtl8821cu (brektrou)
   -u     update script
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

