PROJECT_DIR = $(CURDIR)
SDK_DIR ?= $(CURDIR)/../../../..

USER_CSRC := main.c
USER_CSRC += $(wildcard  src/*.c)

USER_INCLUDE += $(PROJECT_DIR) \
				$(PROJECT_DIR)/inc \
				$(PROJECT_DIR)/common

include $(SDK_DIR)/tools/build/makeall.mk

USR_BOOT_DIR		?= /mnt/d/tftpboot

# 设置启动镜像名
USER_BOOT_IMAGE    ?= openamp_core0

config_pd2008_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pd2008_aarch64_test_openamp_core0

config_pd2008_aarch32:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pd2008_aarch32_test_openamp_core0

config_pd1904_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pd1904_aarch64_dsk_openamp_core0

config_pd1904_aarch32:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pd1904_aarch32_dsk_openamp_core0

config_pe2202_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2202_aarch64_demo_openamp_core0

config_pe2202_aarch32:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2202_aarch32_demo_openamp_core0

config_pe2204_demo_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2204_aarch64_demo_openamp_core0

config_pe2204_demo_aarch32:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2204_aarch32_demo_openamp_core0

config_pe2204_phytiumpi_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2204_aarch64_phytiumpi_openamp_core0

config_pe2204_phytiumpi_aarch32:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2204_aarch32_phytiumpi_openamp_core0

config_pd2308_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pd2308_aarch64_demo_openamp_core0

config_pd2408_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pd2408_aarch64_test_a_openamp_core0

# 完成编译
image:
	make clean
	make all -j
	cp ./$(IMAGE_OUT_NAME).elf $(USR_BOOT_DIR)/$(USER_BOOT_IMAGE).elf
	
scp:
	scp /mnt/d/tftpboot/openamp_core0.elf root@192.168.4.51:/lib/firmware/