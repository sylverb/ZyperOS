/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8: Lexaloffle's own PICO-8, the player's copy, as the PICO-8
 * shelf's first player, with Splore, its online cart browser (Tarik's picks
 * in the native PICO-8 lab, 2026-11-19; DESIGN.md 11.48).
 *
 * PICO-8 is Lexaloffle's to sell, and its license lets no one pass it on, so
 * ZyperOS never ships, downloads or keeps a copy of it. The player brings the
 * PICO-8 for Raspberry Pi zip they bought: ZyperOS Installer takes it from
 * the computer, or the player copies it, or its two files, into Bios/PICO-8.
 * Only two of its files run here: pico8_64, its 64-bit ARM program, and
 * pico8.dat beside it. config/systems.cfg's brings line names them, so the
 * shelf's core run:ZyperOS/pico8/PICO-8.sh counts as on the card only with
 * both there (src/main.c, core_ready), and sd/pixel2/PICO-8.sh runs it as
 * OpenBOR runs: a program of its own, with ZyperOS's in-game menu over it.
 *
 * Splore is an entry of the shelf's own, first on it (pinned whatever the
 * sort), as PortMaster's is on the Ports shelf: its art is ZyperOS's
 * (res/pico8, tools/genpico8-art.py), no part of Lexaloffle's. While your
 * PICO-8 is not on the card the entry says Not installed, and A opens a page
 * that says how to add it. Nothing here touches SDL or a device: the words
 * and the zip's rules are held by `make check` (tools/pico8-check.c). */
#ifndef ZYPEROS_PICO8_H
#define ZYPEROS_PICO8_H

#include <stdbool.h>
#include <stddef.h>

#include "library.h"
#include "sysupdate.h"

/* The PICO-8 shelf's tag (config/systems.cfg) */
#define P8_TAG "PICO8"
/* The script that runs it, in the card's ZyperOS folder (mk/payload.sh: the
 * devices that run a shelf's script, the Pixel 2), and the core that names it */
#define P8_SCRIPT "pico8/PICO-8.sh"
#define P8_CORE   "run:ZyperOS/" P8_SCRIPT
/* Where the player's files go on the card, and what is kept of the zip */
#define P8_DIR     "Bios/PICO-8"
#define P8_PROGRAM "pico8_64"
#define P8_DATA    "pico8.dat"
#define P8_LICENSE "license.txt"
/* The version the files are, one line, as ZyperOS Installer and the zip's
 * import write it ("0.2.7"); none when the player copied the files alone */
#define P8_VERSION "version.txt"
/* The biggest either file may be: in 0.2.7 pico8_64 is 1.6 MB and pico8.dat
 * 11.2 MB */
#define P8_MAX_BYTES (64u << 20)

/* Splore's entry: a name no scan makes (its extension is no cart's), added to
 * the shelf's listing as PortMaster's is to the Ports shelf's */
#define SPLORE_FILE "Splore.splore"
#define SPLORE_NAME "Splore"
/* Its art beside the script: its cover in each theme, and the icon the page
 * draws */
#define P8_COVER_LIGHT "pico8/cover-light.png"
#define P8_COVER_DARK  "pico8/cover-dark.png"
#define P8_ICON        "pico8/icon.png"

/* Whether `file` (a listing's file name) is Splore's entry */
bool splore_file(const char *file);
/* Whether a listing has Splore's entry, and the entry added to one that lacks
 * it, first, so a sort that keeps it first (splore_pin) finds it there.
 * False when it could not be. */
bool splore_listing_has(const game_list *l);
bool splore_listing_add(game_list *l);
/* Splore's entry moved to the front of `n` sorted entries (a shelf's games,
 * after its folders), the rest in their order */
void splore_pin(game_entry *items, int n);

/* The page A opens on Splore while your PICO-8 is not on the card: System
 * Update's page (DESIGN.md 11.17) with its own words, Back its one key.
 * `notes` gets the points under the line, `*nnotes` how many. */
#define P8_NOTES 3
#define P8_NOTE_LEN 200
void p8_page_build(su_page *p, char notes[P8_NOTES][P8_NOTE_LEN], int *nnotes);

/* The zip's rules, pure, for the check:
 * - the version in its file name ("pico-8_0.2.7_raspi.zip" is 0.2.7), or in
 *   its manual's first lines ("PICO-8 v0.2.7"); false when neither says;
 * - whether 20 bytes or more of a file's start are a 64-bit little-endian
 *   ARM (AArch64) ELF program, as pico8_64 is;
 * - which of the zip's files is taken, by its path in the zip: "pico8_64",
 *   "pico8.dat", "license.txt" or a manual ("pico-8_manual.txt", older
 *   zips' "pico-8.txt") at the top or one folder down ("pico-8/pico8_64"),
 *   their name in Bios/PICO-8 (a manual's "" ), NULL for the rest. */
bool p8_version_from_name(const char *name, char *out, size_t n);
bool p8_version_from_manual(const char *text, size_t len, char *out, size_t n);
bool p8_elf_aarch64(const unsigned char *h, size_t n);
const char *p8_zip_member(const char *path);

/* A zip the player copied into Bios/PICO-8 by hand (Tarik's pick: the zip as
 * it comes, or its two files). The newest .zip there is read when pico8_64 is
 * not there, or older than the zip: its pico8_64 and pico8.dat, and its
 * license, go beside it, each written whole under a temporary name and then
 * renamed, and version.txt says which PICO-8. The zip stays where the player
 * put it. `card` is the card's mount point. */
typedef enum {
	P8_TAKE_NONE,       /* no zip to take, or nothing newer */
	P8_TAKE_DONE,       /* taken: `version` says which ("" unknown) */
	P8_TAKE_NOT_RASPI,  /* a zip with no AArch64 pico8_64 and pico8.dat */
	P8_TAKE_FAILED      /* the card refused a write, or the zip is damaged */
} p8_take;
p8_take p8_take_zip(const char *card, char *version, size_t vn, char *zip, size_t zn);
/* What the launcher says after a take, into `out` ("" for nothing) */
void p8_take_message(p8_take t, const char *version, const char *zip, char *out, size_t n);

#endif
