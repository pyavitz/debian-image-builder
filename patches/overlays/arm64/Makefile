# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_MESON) += \
	meson-g12a-radxa-zero-gpio-8-led.dtbo \
	meson-g12a-radxa-zero-gpio-10-led.dtbo \
	meson-g12-bananapi-rtl8822cs.dtbo \
	meson-g12b-odroid-n2-i2c0.dtbo \
	meson-g12b-odroid-n2-i2c1.dtbo \
	meson-g12b-odroid-n2-spidev0.dtbo \
	meson-g12b-odroid-n2-spi.dtbo \
	meson-g12b-odroid-n2-uart0.dtbo \
	meson-g12b-odroid-n2-uart1.dtbo \
	meson-g12-gxl-cma-pool-896MB.dtbo \
	meson-sm1-bananapi-uartA.dtbo \
	meson-sm1-bananapi-uartAO_B.dtbo \
	meson-sm1-bananapi-uartB.dtbo \
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
