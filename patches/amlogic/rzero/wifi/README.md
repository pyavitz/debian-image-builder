### Wifi Cap SDIO Speed

If having wifi problems, copy the following patch into patches/userpatches before building the kernel.

---

Alternatively, you can add the following script to the img and see if it resolves the problem.

```sh
sudo mkdir -p /etc/initramfs/post-update.d/
sudo wget -cq https://raw.githubusercontent.com/pyavitz/debian-image-builder/feature/files/boot/99-sdio-speed -P /etc/initramfs/post-update.d/
sudo chmod +x /etc/initramfs/post-update.d/99-sdio-speed
sudo chown root:root /etc/initramfs/post-update.d/99-sdio-speed
sudo /etc/initramfs/post-update.d/99-sdio-speed
```
