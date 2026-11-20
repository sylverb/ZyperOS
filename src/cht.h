/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CHT_H
#define ZYPEROS_CHT_H

/* A .cht file read: RetroArch's cheat format, which libretro's cheat list
 * (libretro-database's cht/ folder) and NextUI's cards both use.
 *
 *     cheats = 2
 *
 *     cheat0_desc = "Infinite Lives"
 *     cheat0_code = "7E0DBE:63"
 *     cheat0_enable = false
 *
 *     cheat1_desc = "Start on Any World"
 *     cheat1_code = "DD62-6F0D+DD6B-6D6D"
 *     cheat1_enable = false
 *
 * The older files have those three keys a cheat. RetroArch's own cheat
 * search writes the newer ones, which add a handler and the address, value
 * and the like of a cheat RetroArch pokes into memory itself every frame
 * (handler 1). Those have no code for a core, and a poke every frame is a
 * cost per frame, so they are left out; every other key is skipped.
 *
 * Our own reader, written from the format: key = value a line, the value
 * quoted or not, `#` starting a comment line, keys in any order. A file with
 * no `cheats =` line counts up to its last cheat. A cheat with no code is
 * left out; one with no description is given none here, and its list names
 * it by its place.
 *
 * Pure: no SDL, no card, so tools/cheats-check.c holds it to the cases. */
#include <stddef.h>

/* The most cheats one file gives. libretro's longest file has about 900. */
#define CHT_MAX      1024
/* A description longer is cut, at a character's start */
#define CHT_DESC_MAX 128

/* Each cheat that has a code, in its file's order (its number), with its
 * description ("" for none) and code; both are good until cht_parse returns */
typedef void (*cht_fn)(const char *desc, const char *code, void *ctx);

/* Read `len` bytes of `text`, a file's whole text, calling `fn` for each
 * cheat. How many it gave, or -1 when the text has no cheat key at all (not
 * a .cht). */
int cht_parse(const char *text, size_t len, cht_fn fn, void *ctx);

#endif
