1.配置驱动时需要将如下定义置y
CONFIG_BT=y
CONFIG_BT_RFCOMM=y

2.在config文件中增加
CONFIG_SKW_BT=m

3.参考makefile和Kconfig文件配置

4.生成skwbt.ko

5.蓝牙配置文件*.nvbin目前放在/lib/firmware目录下，可根据实际情况更改