# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_RV64I) += \
	keystone-k1-pwm-backlight.dtbo \
	keystone-k1-qspi.dtbo

targets += dtbs dtbs_install
targets += $(dtbo-y)

always-y	:= $(dtbo-y)
clean-files	:= *.dtbo
