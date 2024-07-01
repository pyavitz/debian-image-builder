# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_ARCH_RV64I) += \
	k1-pwm-backlight.dtbo \
	k1-qspi.dtbo \
	k1-uart2.dtbo

targets += dtbs dtbs_install
targets += $(dtbo-y)

always-y	:= $(dtbo-y)
clean-files	:= *.dtbo
