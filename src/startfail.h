/* SPDX-License-Identifier: GPL-3.0-only */
/* What the shelf says when a core would not load.
 *
 * A core a person added (bsnes, MAME 2010: Davistoa, on Discord, 2026-11-18)
 * that the system's loader refuses sent the game straight back to the shelf
 * with nothing said; the log had Diatom's "dlopen <path>: <dlerror()>" and
 * "No such file or directory" for a file that was there. Diatom now says why
 * in the ERROR it answers the RUN with (diatom/src/corefail.h, read by
 * plat_resident_start_error), and this puts it into the player's words under
 * the game's name, as an empty game file or a wrong BIOS is (gamefile.h,
 * biosfile.h):
 *
 *   bsnes_libretro.so could not be loaded: it is not a 64-bit ARM Linux build
 *   bsnes_libretro.so could not be loaded: it needs libfoo.so.2, which
 *     ZyperOS does not have
 *   ... it needs libstdc++.so.6 (GLIBCXX_3.4.30), which ZyperOS does not have
 *   ... it calls _ZN5bsnes3fooEv, which ZyperOS does not have
 *   bsnes_libretro.so could not be loaded
 *
 * The names are the files' and the function's, never translated, and cut at
 * 40 characters with an ellipsis (a mangled C++ name has no place to break
 * a line). The log has the system's own words, whole. */
#ifndef ZYPEROS_STARTFAIL_H
#define ZYPEROS_STARTFAIL_H

#include <stdbool.h>
#include <stddef.h>

/* A core that would not load, as Diatom says it in its ERROR before RUNNING
 * (code core_load, diatom/src/corefail.h): why= machine, library, version,
 * symbol or other, the name dlerror gave and the version it asked for, and
 * the core's path. The launcher's reader of Diatom's answers keeps it
 * (plat_resident_start_error, platform.h). */
typedef struct {
	char code[24];      /* "core_load" */
	char why[16];
	char name[128];
	char version[64];
	char core[256];
} plat_start_error;

/* The words for `e`, in the language that is on; false, and "" in `out`, for
 * an error that is not a core that would not load (the log's to say) */
bool startfail_message(const plat_start_error *e, char *out, size_t n);

#endif
