################################################################################
#
# monitor-news2
#
################################################################################

MONITOR_NEWS2_VERSION = local
# o .mk e lido mesmo com o pacote desligado; sem o fallback o parse quebra
MONITOR_NEWS2_SITE = $(or $(call qstrip,$(BR2_PACKAGE_MONITOR_NEWS2_SRCDIR)),$(TOPDIR)/dl)
MONITOR_NEWS2_SITE_METHOD = local
MONITOR_NEWS2_LICENSE = Proprietary
MONITOR_NEWS2_SUPPORTS_IN_SOURCE_BUILD = NO
MONITOR_NEWS2_DEPENDENCIES = qt6base qt6declarative

# build/ e .qtcreator/ sao artefatos do Qt Creator no PC
MONITOR_NEWS2_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = --exclude /build --exclude /.qtcreator

# MONITOR_NEWS2_HARDWARE: no equipamento os sinais vêm do MAX30102, não do mock
MONITOR_NEWS2_CONF_OPTS = -DQT_HOST_PATH=$(HOST_DIR) -DMONITOR_NEWS2_HARDWARE=ON

ifeq ($(BR2_PACKAGE_MONITOR_NEWS2_SENSOR_IIO),y)
MONITOR_NEWS2_CONF_OPTS += -DMONITOR_NEWS2_SENSOR_BACKEND=iio
# sem udev/mdev ninguem carrega o modulo pelo device tree: o init script faz o modprobe
define MONITOR_NEWS2_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(MONITOR_NEWS2_PKGDIR)/S30max30102 \
		$(TARGET_DIR)/etc/init.d/S30max30102
endef
else
MONITOR_NEWS2_CONF_OPTS += -DMONITOR_NEWS2_SENSOR_BACKEND=i2cdev
endif

# os testes (tst_*) rodam no PC; no alvo so o executavel
MONITOR_NEWS2_BUILD_OPTS = --target monitor-news2

define MONITOR_NEWS2_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/buildroot-build/monitor-news2 \
		$(TARGET_DIR)/root/monitor-news2
endef

$(eval $(cmake-package))
