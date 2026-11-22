/* SPDX-License-Identifier: GPL-3.0-only */
/* The pop-up over a Famicom Disk System game as L1 or R1 moves its disk.
 *
 * FCEUmm turns an .fds game's disk with L1 while it is out of the drive,
 * and puts it in or takes it out with R1 (DESIGN.md 11.28); it says each in
 * its own English, which Diatom reads (diatom/src/fdsdisk.h) and sends as a
 * DISK line: state=selected|in|out|eject-first, and disk=, of=, side=. The
 * launcher's pop-up says it in the player's language, two lines as every
 * pop-up over a game has (popup.txt): what happened, then to what.
 *
 *   L1, the disk out   Disk side / Disk 1 · Side B
 *   L1, the disk in    Disk side / Eject it first with R1
 *   R1                 Disk in / Disk 1 · Side B, or Disk out / ...
 *
 * Plain C, no SDL: the words alone, for main.c's on_disk and the shots. */
#ifndef ZYPEROS_DISKPOPUP_H
#define ZYPEROS_DISKPOPUP_H

#include <stdbool.h>
#include <stddef.h>

/* The pop-up's two lines for a DISK line's state, disk and side: the head
 * in `*head`, the body written to `body`. False for a state that is none of
 * the four, or a disk or side that is no disk's (nothing to say). */
bool disk_popup_words(const char *state, int disk, char side, const char **head,
                      char *body, size_t n);

#endif
