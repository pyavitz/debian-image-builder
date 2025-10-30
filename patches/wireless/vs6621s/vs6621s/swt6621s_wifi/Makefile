# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_WLAN_VENDOR_SWT6621S) += swt6621s_wifi.o

swt6621s_wifi-objs += skw_core.o
swt6621s_wifi-objs += skw_core_dbg.o
swt6621s_wifi-objs += skw_iface.o
swt6621s_wifi-objs += skw_cfg80211.o
swt6621s_wifi-objs += skw_msg.o
swt6621s_wifi-objs += skw_rx.o
swt6621s_wifi-objs += skw_tx.o
swt6621s_wifi-objs += skw_regd.o
swt6621s_wifi-objs += skw_mlme.o
swt6621s_wifi-objs += skw_timer.o
swt6621s_wifi-objs += skw_log.o
swt6621s_wifi-objs += skw_iw.o
swt6621s_wifi-objs += skw_work.o
swt6621s_wifi-objs += skw_mbssid.o
swt6621s_wifi-objs += skw_dentry.o
swt6621s_wifi-objs += skw_util.o
swt6621s_wifi-objs += skw_config.o
swt6621s_wifi-objs += skw_recovery.o
swt6621s_wifi-objs += skw_vendor.o
swt6621s_wifi-objs += trace.o
swt6621s_wifi-objs += skw_db.o

# swt6621s_wifi-$(CONFIG_SWT6621S_CALIB_DPD) += skw_calib.o
# swt6621s_wifi-$(CONFIG_SWT6621S_EDMA) += skw_edma.o
swt6621s_wifi-$(CONFIG_SWT6621S_DFS_MASTER) += skw_dfs.o
swt6621s_wifi-$(CONFIG_SWT6621S_TDLS) += skw_tdls.o

ccflags-y += -DCONFIG_SWT6621S_STA_SME_EXT
ccflags-y += -DCONFIG_SWT6621S_SAP_SME_EXT
ccflags-y += -DCONFIG_SWT6621S_SCAN_RANDOM_MAC
ccflags-y += -DCONFIG_SWT6621S_TX_WORKQUEUE
ccflags-y += -DCONFIG_SWT6621S_HIGH_PRIORITY
ccflags-y += -DCONFIG_SWT6621S_CALIB_APPEND_BUS_ID
ccflags-y += -DCONFIG_SWT6621S_CALIB_APPEND_MODULE_ID

ifneq ($(CONFIG_SWT6621S_EXTERNAL_REGDB),y)
ccflags-y += -DCONFIG_SWT6621S_REGD_SELF_MANAGED
endif

ifeq ($(CONFIG_ARCH_ROCKCHIP),y)
ccflags-y += -DCONFIG_PLATFORM_ROCKCHIP
endif

ifneq ($(filter y,$(CONFIG_ANDROID_BINDER_IPC) $(CONFIG_SKW_ANDROID)),)
ccflags-y += -D__SKW_ANDROID__
endif

ifneq ($(skw_extra_flags),)
ccflags-y += $(skw_extra_flags)
endif

ifneq ($(skw_extra_symbols),)
KBUILD_EXTRA_SYMBOLS += $(skw_extra_symbols)
endif

ccflags-y += -I$(srctree)/include/linux/platform_data/

CFLAGS_trace.o := -I$(src)
$(obj)/skw_core.o : $(obj)/version.h

skw_abs_path := $(addprefix $(abspath $(srctree))/,$(filter-out /%,$(src)))$(filter /%,$(src))
$(obj)/version.h: $(skw_abs_path)/genver.pl
	@$(PERL) -s $(skw_abs_path)/genver.pl $@

clean-files := version.h
