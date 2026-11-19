/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_KIT_H
#define ZYPEROS_KIT_H

/* A test kit's own code (docs/building.md, "Test kits"). The device build
 * (mk/cross.mk) defines ZYPEROS_KIT_BUILD, and links the device's kit file,
 * src/device/<id>_kit.c, only for a VERSION with "-kit" in it: a kit such as
 * v1.0.7-kit.1, which tools/release.sh makes from main with its version and
 * notes alone. A release's VERSION never has it, so a release compiles none
 * of this, and the shared files' hooks are under #ifdef ZYPEROS_KIT_BUILD
 * (make check-kitbuild). The host's build is never a kit. */

#ifndef ZYPEROS_KIT_BUILD
#error "src/kit.h is a kit's alone: include it under #ifdef ZYPEROS_KIT_BUILD"
#endif

#include "app.h"
#include "hwreport.h"

/* The kit's page, Y on MENU > About (src/about_screen.c). Weak: a device
 * with no kit file links none, and Y does nothing there. */
void kit_page(app *a) __attribute__((weak));

/* The kit's facts for the log pack's hardware report, read after the
 * device's own (its plat_hw_facts) */
void kit_hw_facts(hw_facts *f);

#endif
