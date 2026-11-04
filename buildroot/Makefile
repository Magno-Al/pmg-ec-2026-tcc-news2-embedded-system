TOPDIR    := $(CURDIR)
BR_DIR    := $(TOPDIR)/buildroot
BUILD_DIR := $(TOPDIR)/build
DL_DIR    ?= $(HOME)/buildroot/dl
DEFCONFIG ?= rpi3_news2_defconfig

EXTERNAL_ARGS := BR2_EXTERNAL=$(TOPDIR)/external
BR_MAKE := $(MAKE) -C $(BR_DIR) O=$(BUILD_DIR) $(EXTERNAL_ARGS)
export BR2_DL_DIR = $(DL_DIR)

.PHONY: all save flash check-submodule

all: $(BUILD_DIR)/.config
	@$(BR_MAKE) all
	@ls -lh $(BUILD_DIR)/images/sdcard.img

$(BUILD_DIR)/.config: check-submodule
	@mkdir -p $(BUILD_DIR) $(DL_DIR)
	@$(BR_MAKE) $(DEFCONFIG)

check-submodule:
	@test -f $(BR_DIR)/Makefile || { \
	  echo "ERRO: submodule vazio. Rode: git submodule update --init --recursive"; \
	  exit 1; }

save:
	@$(BR_MAKE) savedefconfig BR2_DEFCONFIG=$(TOPDIR)/external/configs/$(DEFCONFIG)
	@echo ">> defconfig salvo em external/configs/$(DEFCONFIG)"

flash:
	@test -n "$(SD)" || { echo "Uso: make flash SD=/dev/sdX"; exit 1; }
	@$(TOPDIR)/scripts/flash-sd.sh $(BUILD_DIR)/images/sdcard.img $(SD)

%:
	@if [ ! -f $(BUILD_DIR)/.config ]; then $(MAKE) $(BUILD_DIR)/.config; fi
	@$(BR_MAKE) $@
