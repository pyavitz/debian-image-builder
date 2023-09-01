# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_EXYNOS) += \
	exynos5422-odroidxu4-ads7846.dtbo \
	exynos5422-odroidxu4-hktft-cs-ogst.dtbo \
	exynos5422-odroidxu4-hktft32.dtbo \
	exynos5422-odroidxu4-hktft35.dtbo \
	exynos5422-odroidxu4-i2c0.dtbo \
	exynos5422-odroidxu4-i2c1.dtbo \
	exynos5422-odroidxu4-spidev0.dtbo \
	exynos5422-odroidxu4-sx865x-i2c1.dtbo \
	exynos5422-odroidxu4-uart0.dtbo

dtbo-$(CONFIG_ARCH_SUNXI) += \
	sun8i-h3-i2c0.dtbo \
	sun8i-h3-i2c1.dtbo \
	sun8i-h3-i2c2.dtbo \
	sun8i-h3-spi-add-cs1.dtbo \
	sun8i-h3-spi-jedec-nor.dtbo \
	sun8i-h3-spi-spidev.dtbo \
	sun8i-h3-uart1.dtbo \
	sun8i-h3-uart2.dtbo \
	sun8i-h3-uart3.dtbo \
	sunxi-h3-h5-ir-tx.dtbo \
	sunxi-h3-h5-overclock.dtbo

targets += dtbs dtbs_install
targets += $(dtbo-y)

always-y	:= $(dtbo-y)
clean-files	:= *.dtbo
