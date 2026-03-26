# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_RV64I) += \
	k1-disable-uart2.dtbo \
	k1-disable-qspi.dtbo \
	k1-bananapi-f3-i2c4-pwm7-spi3-uart5.dtbo

targets += dtbs dtbs_install
targets += $(dtbo-y)

always-y	:= $(dtbo-y)
clean-files	:= *.dtbo
