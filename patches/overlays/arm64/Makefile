# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_MESON) += \
	meson-aml-gpiomem.dtbo \
	meson-g12a-radxa-zero-gpio-8-led.dtbo \
	meson-g12a-radxa-zero-gpio-10-led.dtbo \
	meson-g12a-radxa-zero-i2c-ao-m0-gpioao-2-gpioao-3.dtbo \
	meson-g12a-radxa-zero-i2c-ee-m1-gpioh-6-gpioh-7.dtbo \
	meson-g12a-radxa-zero-i2c-ee-m1-gpiox-10-gpiox-11.dtbo \
	meson-g12a-radxa-zero-i2c-ee-m3-gpioa-14-gpioa-15.dtbo \
	meson-g12a-radxa-zero-pwm-c-on-gpiox-8.dtbo \
	meson-g12a-radxa-zero-pwmao-a-on-gpioao-11.dtbo \
	meson-g12a-radxa-zero-spi-spidev.dtbo \
	meson-g12a-radxa-zero-uart-ao-a-on-gpioao-0-gpioao-1.dtbo \
	meson-g12a-radxa-zero-uart-ao-b-on-gpioao-2-gpioao-3.dtbo \
	meson-g12a-radxa-zero-uart-ao-b-on-gpioao-8-gpioao-9.dtbo \
	meson-g12a-radxa-zero-uart-ee-c.dtbo \
	meson-g12b-bananapi-m2s-rtl8822cs.dtbo \
	meson-g12b-odroid-n2-canfd0.dtbo \
	meson-g12b-odroid-n2-i2c0.dtbo \
	meson-g12b-odroid-n2-i2c1.dtbo \
	meson-g12b-odroid-n2-spidev0.dtbo \
	meson-g12b-odroid-n2-spi.dtbo \
	meson-g12b-odroid-n2-uart0.dtbo \
	meson-g12b-odroid-n2-uart1.dtbo \
	meson-g12b-waveshare-cm4-io-base-usb.dtbo \
	meson-g12-gxl-cma-pool-896MB.dtbo \
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
	meson-sm1-x96-air-rtc.dtbo

dtbo-$(CONFIG_ARCH_SUNXI) += \
	sunxi-a64-overclock.dtbo \
	sunxi-h3-h5-ir-tx.dtbo \
	sunxi-h3-h5-overclock.dtbo

targets += dtbs dtbs_install
targets += $(dtbo-y)

always-y	:= $(dtbo-y)
clean-files	:= *.dtbo
