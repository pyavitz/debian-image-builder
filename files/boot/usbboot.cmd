setenv bootlabel "USB Boot"
usb start; if usb dev ${devnum}; then devtype=usb; run scan_dev_for_boot_part; fi
