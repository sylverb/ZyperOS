/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_MP4KEYS_H
#define ZYPEROS_MP4KEYS_H

/* MP4's settings under its own name. Until 1.1.0 the code called the player
 * Muse, the audio player it was before it played video (DESIGN.md 11.14),
 * and a handheld's database kept MP4's settings under "muse.": the mode
 * (in order, shuffle, repeat), the kind Show shows, Show's one-time All and
 * the Player Look. From 1.1.0 they are under "mp4.", and this moves a
 * handheld's database over at the first start, before anything reads them.
 *
 * Every "muse." setting moves to the same name under "mp4.", its value
 * kept. Where both are there - a step back to 1.0.6 and forward again,
 * whose start writes its own Show and once-mark under the old name - the
 * "mp4." one is the player's and stays, and the old one goes. Nothing is
 * left under "muse.", so a second start has nothing to do: no mark is
 * needed to say it was done. Returns how many moved. */

#include "db.h"

#define MP4KEYS_OLD "muse."
#define MP4KEYS_NEW "mp4."

int mp4keys_migrate(db *dev);

#endif
