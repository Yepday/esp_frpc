#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp_frpc

EXTRA_COMPONENT_DIRS = $(IDF_PATH)/examples/common_components/protocol_examples_common

include $(IDF_PATH)/make/project.mk

# NVS相关目标
.PHONY: nvs-generate nvs-flash nvs-clean nvs-test

# 生成NVS镜像
nvs-generate:
	@echo "生成NVS镜像..."
	@python tools/generate_nvs_data.py

# 测试NVS镜像生成
nvs-test:
	@echo "测试NVS镜像生成..."
	@python tools/generate_nvs_data.py --test

# 烧录NVS镜像
nvs-flash: nvs-generate
	@echo "烧录NVS镜像..."
	@$(IDF_PATH)/components/esptool_py/esptool/esptool.py --port $(PORT) write_flash 0x9000 nvs.bin

# 烧录NVS镜像（使用SDK中的esptool）
nvs-flash-sdk: nvs-generate
	@echo "烧录NVS镜像（使用SDK中的esptool）..."
	@$(IDF_PATH)/components/esptool_py/esptool/esptool.py --port /dev/ttyUSB0 write_flash 0x9000 nvs.bin

# 清理NVS文件
nvs-clean:
	@echo "清理NVS文件..."
	@rm -f nvs.bin
	@rm -f nvs.csv

