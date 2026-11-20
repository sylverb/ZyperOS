/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot screen in the theme the player chose (DESIGN.md 11.21), and at
 * the brightness the player chose.
 *
 * The boot animation plays from the moment the panel lights, before the card
 * with the settings is mounted, so the choice reaches it another way: the
 * launcher keeps it in theme.txt on the boot partition, U-Boot's boot.scr
 * (plastron's board/zyperos/boot-slots.cmd) reads that file and puts its two
 * words on the kernel's line, and the kernel hands a word without a dot to
 * init as a variable of its environment, which plastron's splash reads:
 *
 *   zytheme=light|dark|auto-1260-420
 *                             the Appearance setting (src/theme.h's words),
 *                             Auto with its hours since 1.1.0: dark from the
 *                             first minute of the day to the second, the
 *                             Night Hours (night.from, night.until). Plain
 *                             "auto" is a 1.0.5 launcher's, 7:00 to 19:00
 *                             light, until this launcher writes the file
 *   zyzone=America/New_York   the clock's zone, for Auto's hours; "-" when
 *                             the zone has a character a kernel line or
 *                             U-Boot could not carry
 *   zylang=pt-BR              MENU > Language's code (src/i18n.h), for the
 *                             first start's line under the animation
 *                             (DESIGN.md 11.20); "en" for any other word
 *   zybright=74               the backlight's own value at the player's
 *                             brightness (the device's ladder,
 *                             plat_brightness_raw), which the splash sets
 *                             before its first frame; "-" when none was
 *                             ever kept, and the panel lights as the
 *                             kernel has it
 *
 * The hours ride in zytheme's own word, not a word of their own: boot.scr
 * imports four words by name, and System Update does not write boot.scr, so a
 * fifth would never reach a card that has an older one. A splash from before
 * 1.1.0 (a slot fallen back to) reads "auto-..." as no word it knows: Beard.
 *
 * A boot.scr from before the language carries the first two only: System
 * Update does not write boot.scr, so a card that had 1.0.0 keeps one. The
 * language reaches the splash on such a card from the card itself, where
 * plastron's init reads it when the copy starts (src/i18n.h, LANG_FILE).
 *
 * Nothing here touches a file: main.c reads the kernel's line and the
 * settings, the device file writes theme.txt (plat_boot_theme), and
 * tools/boottheme-check.c holds the words. */
#ifndef ZYPEROS_BOOTTHEME_H
#define ZYPEROS_BOOTTHEME_H

#include <stddef.h>

#define BOOTTHEME_MAX 160

/* The four words for `appearance` ("light", "dark", "auto"; anything else
 * is light, as the launcher reads it) with Auto's hours `night_from` and
 * `night_until` (minutes into the day, the Night Hours: "auto-1260-420"; a
 * minute off the day is 21:00 or 7:00, as they ship), `zone`, `lang` (a
 * language's code, "en" for one that is none) and `bright` (the backlight's
 * value, "-" for one below 1): in `line` as a kernel line has them, one
 * space between, and in `file` as theme.txt holds them, a line each, for
 * U-Boot's env import. */
void boottheme_words(const char *appearance, int night_from, int night_until, const char *zone,
                     const char *lang, int bright, char *line, size_t n, char *file, size_t fn);

/* The words this start's kernel line carried, as boottheme_words writes
 * `line`; "" when it carried none (an older boot.scr, or no theme.txt). A
 * line from a boot.scr that carries no zylang, or no zybright, has the words
 * it does carry: boottheme_same then leaves the others out of the
 * comparison. */
void boottheme_from_cmdline(const char *cmdline, char *line, size_t n);

/* Whether `want` (boottheme_words' line) is what `said` (the kernel line's)
 * already says: every word `said` has, as a boot.scr from before the
 * language or the brightness could not carry those (each word after the
 * theme and the zone is left out where `said` has none of it). */
int boottheme_same(const char *want, const char *said);

#endif
