## Debian image builder

```
Supported Boards: Tritium, Pine A64, Odroid C4, Odroid N2, Le Potato and NanoPi NEO Plus 2
```
### Dependencies for Debian Buster aarch64 system

```
sudo apt install build-essential bison bc git dialog patch dosfstools zip unzip qemu debootstrap \ 
                 qemu-user-static rsync kmod cpio flex libssl-dev libncurses5-dev parted \
                 device-tree-compiler libfdt-dev python3-distutils python3-dev swig fakeroot \
                 lzop lz4 crossbuild-essential-arm64
```
### Instructions

#### Install dependencies

```sh
make install-depends        # (cross compile)
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
Branch:         # Selected kernel branch
Mainline:       # 1 for any x.y-rc
Menuconfig:     # 1 to run kernel menuconfig
Crosscompile:   # 1 to cross compile | 0 to native compile
```
#### Miscellaneous

```sh
make cleanup    # Clean up image errors
make purge      # Remove tmp directory
```

#### User defconfig
```sh
nano userdata.txt
# place config in defconfig directory
custom_defconfig=1
MYCONFIG="nameofyour_defconfig"
```

#### Write to eMMC
```sh
Supported: Le Potato, Odroid C4 and NanoPi NEO Plus2
1. Boot from sdcard
2. Attach eMMC module (be careful)
3. Execute: sudo write_to_emmc
```

### Funding

Please [donate](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=VG8GP2SY4CEEW&item_name=Buy+me+a+beer) if you'd like to support development.
