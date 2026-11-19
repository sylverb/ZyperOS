################################################################################
#
# diatom: the libretro frontend, from ZyperOS's diatom/ folder
#
################################################################################

DIATOM_VERSION = local
DIATOM_SITE = /zyperos/diatom
DIATOM_SITE_METHOD = local
# The device's own port (ZYDEV_DIATOM_PORT, external.mk)
DIATOM_PORT = $(ZYDEV_DIATOM_PORT)
ifeq ($(DIATOM_PORT),brick)
# pvr-ge8300: the PowerVR's EGL and GLES headers and pkg-config files, for
# the GPU path (port/brick.c); its libraries are the player's own copy
DIATOM_DEPENDENCIES = sdl2 zlib pvr-ge8300 host-pkgconf
else
DIATOM_DEPENDENCIES = sdl2 zlib mesa3d-px2 libdrm host-pkgconf
endif
DIATOM_LICENSE = MPL-2.0
DIATOM_LICENSE_FILES = LICENSE
# Its source is public (github.com/tarikbc/ZyperOS, diatom/), so legal-info
# names it rather than packing up the copy below, which is a working tree's
DIATOM_REDISTRIBUTE = NO

# What git ignores stays behind: build products, cores, sysroots, notes
DIATOM_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = --filter=:-_.gitignore --exclude=/.claude

# Diatom's port for the device (port/brick.c or port/pixel2.c). Its build
# finds the libraries with pkg-config, which in Buildroot's PATH answers for
# this system's: SDL2, EGL and GLES on the Brick; SDL2, EGL, GLES, GBM and
# libdrm on the Pixel 2.
define DIATOM_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) PORT=$(DIATOM_PORT) CC="$(TARGET_CC)"
endef

# Nothing to install here: diatom runs from the card, beside ZyperOS, and the
# zyperos package puts it in ZyperOS's payload

$(eval $(generic-package))
