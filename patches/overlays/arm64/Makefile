# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_MESON) += \
	meson-g12a-radxa-zero-gpio-8-led.dtbo \
	meson-g12a-radxa-zero-gpio-10-led.dtbo \
	meson-g12a-radxa-zero-i2c-ao-m0-gpioao-2-gpioao-3.dtbo \
	meson-g12a-radxa-zero-i2c-ee-m1-gpioh-6-gpioh-7.dtbo \
	meson-g12a-radxa-zero-i2c-ee-m1-gpiox-10-gpiox-11.dtbo \
	meson-g12a-radxa-zero-i2c-ee-m3-gpioa-14-gpioa-15.dtbo \
	meson-g12a-radxa-zero-pwm-c-on-gpiox-8.dtbo \
	meson-g12a-radxa-zero-pwmao-a-on-gpioao-11.dtbo \
	meson-g12a-radxa-zero-rtc-ds1307.dtbo \
	meson-g12a-radxa-zero-spi-spidev.dtbo \
	meson-g12a-radxa-zero-uart-ao-a-on-gpioao-0-gpioao-1.dtbo \
	meson-g12a-radxa-zero-uart-ao-b-on-gpioao-2-gpioao-3.dtbo \
	meson-g12a-radxa-zero-uart-ao-b-on-gpioao-8-gpioao-9.dtbo \
	meson-g12a-radxa-zero-uart-ee-c.dtbo \
	meson-g12b-aml-gpiomem.dtbo \
	meson-g12b-bananapi-cm4-gpu.dtbo \
	meson-g12b-bananapi-cm4-pwm-gpioh-5-fan.dtbo \
	meson-g12b-bananapi-cm4-wifi-freq-200mhz.dtbo \
	meson-g12b-bananapi-m2s-rtl8822cs.dtbo \
	meson-g12b-odroid-n2-canfd0.dtbo \
	meson-g12b-odroid-n2-i2c0.dtbo \
	meson-g12b-odroid-n2-i2c1.dtbo \
	meson-g12b-odroid-n2-spidev0.dtbo \
	meson-g12b-odroid-n2-spi.dtbo \
	meson-g12b-odroid-n2-uart0.dtbo \
	meson-g12b-odroid-n2-uart1.dtbo \
	meson-g12b-rpi-cm4-io-j17-fan.dtbo \
	meson-g12b-rpi-cm4-io-rtc-pcf85063a.dtbo \
	meson-g12-gxl-cma-pool-896MB.dtbo \
	meson-g12-pwm-gpiox-5-fan.dtbo \
	meson-gxl-s905x-libretech-cc-i2c-ao.dtbo \
	meson-sm1-aml-gpiomem.dtbo \
	meson-sm1-bananapi-m5-rtl8822cs.dtbo \
	meson-sm1-bananapi-uartA.dtbo \
	meson-sm1-bananapi-uartA_cts_rts.dtbo \
	meson-sm1-bananapi-uartAO_B.dtbo \
	meson-sm1-odroid-c4-canfd0.dtbo \
	meson-sm1-odroid-c4-i2c0.dtbo \
	meson-sm1-odroid-c4-i2c1.dtbo \
	meson-sm1-odroid-c4-rtl8822cs.dtbo \
	meson-sm1-odroid-c4-spidev0.dtbo \
	meson-sm1-odroid-c4-uart0.dtbo \
	meson-sm1-odroid-c4-uart1.dtbo \
	meson-sm1-odroid-c4-uart2.dtbo \
	meson-sm1-odroid-c4-uart3.dtbo \
	meson-sm1-x96-air-rtc.dtbo \
	meson-usb-host-mode.dtbo

dtbo-$(CONFIG_ARCH_SUNXI) += \
	sunxi-a64-mbus-devfreq-driver.dtbo \
	sunxi-a64-overclock.dtbo \
	sunxi-h3-h5-ir-tx.dtbo \
	sunxi-h3-h5-overclock.dtbo \
	sunxi-h5-i2c0.dtbo \
	sunxi-h5-i2c1.dtbo \
	sunxi-h5-i2c2.dtbo \
	sunxi-h5-mbus-devfreq-driver.dtbo \
	sunxi-h616-pg-17-18-i2c3.dtbo \
	sunxi-h616-pg-15-16-i2c4.dtbo \
	sunxi-h616-disable-uart1.dtbo \
	sunxi-h616-disable-uart5.dtbo \
	sunxi-h616-spi1-cs0-cs1-spidev.dtbo \
	sunxi-h616-spi1-cs0-spidev.dtbo \
	sunxi-h616-spi1-cs1-spidev.dtbo \
	sunxi-h618-bananapi-m4-usb-wifi-fixup.dtbo \
	sunxi-h618-bananapi-m4-wifi-bt.dtbo \
	sunxi-usb-host-mode.dtbo \
	sunxi-usb-otg-mode.dtbo

targets += dtbs dtbs_install
targets += $(dtbo-y)

always-y	:= $(dtbo-y)
clean-files	:= *.dtbo
