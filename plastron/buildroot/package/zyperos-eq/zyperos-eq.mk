################################################################################
#
# zyperos-eq: ZyperOS's equalizer for ports, a LADSPA plugin
#
################################################################################

ZYPEROS_EQ_VERSION = 1.0
ZYPEROS_EQ_SITE = $(BR2_EXTERNAL_PX2_PATH)/package/zyperos-eq/src
ZYPEROS_EQ_SITE_METHOD = local
# The plugin is ZyperOS's; ladspa.h is the LADSPA 1.1 API header, as
# alsa-lib carries it
ZYPEROS_EQ_LICENSE = GPL-3.0 (plugin), LGPL-2.1+ (ladspa.h)
ZYPEROS_EQ_LICENSE_FILES = LICENSE
ZYPEROS_EQ_DEPENDENCIES = alsa-lib

# The five bands are ZyperOS's src/eqdsp.h, the same file MP4, the menu
# music and Diatom shape their sound with
define ZYPEROS_EQ_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -O2 -fPIC -shared -Wall -Wextra \
		-I/zyperos/src -o $(@D)/zyperos_eq.so $(@D)/zyperos_eq.c -lm
endef

# Where alsa-lib's ladspa PCM looks (sd/zyperos/eq-port.sh names it)
define ZYPEROS_EQ_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/zyperos_eq.so $(TARGET_DIR)/usr/lib/ladspa/zyperos_eq.so
endef

$(eval $(generic-package))
