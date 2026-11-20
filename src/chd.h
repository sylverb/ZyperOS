/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_CHD_H
#define ZYPEROS_CHD_H

#include <stdbool.h>
#include <stdint.h>

/* MAME's compressed disc image, read one hunk at a time - enough of it to
 * reach the few sectors a RetroAchievements hash reads (src/rahash.c), and
 * no more. A CHD is a header, a map that says where each hunk is and how it
 * was packed, the packed hunks, and a metadata list (the track list, for a
 * CD).
 *
 * WHAT THIS READS: versions 3, 4 and 5; V5's Huffman-coded map; the codecs
 * chdman packs a CD or a DVD with - zlib, LZMA (the system's liblzma, which
 * FFmpeg already brings to the device), CHD's own Huffman, FLAC
 * (src/flacdec.c), and their CD forms, which pack the 2352 bytes of each
 * sector and its subcode apart.
 *
 * WHAT IT DOES NOT: zstd (`chdman -c cdzs`/`zstd`, never a default: the
 * device has no libzstd), the laserdisc codec, and a CHD that needs a parent
 * (a diff). Those open, and the hunks they would need answer NULL, so the
 * hash is refused rather than made of the wrong bytes.
 *
 * Memory: a hunk and its packed bytes (19 KB on a CD), 5 bytes a hunk for a
 * V5 map (about 190 KB for a 700 MB CD, 2 MB for a 1.8 GB DVD at 4 KB
 * hunks), never the image. */

typedef struct chd chd;

chd *chd_open(const char *path);
void chd_close(chd *c);

uint32_t chd_hunkbytes(const chd *c);
/* The bytes of one unit: a CD frame (2448: 2352 of sector and 96 of
 * subcode), a DVD's 2048-byte sector */
uint32_t chd_unitbytes(const chd *c);
uint64_t chd_logicalbytes(const chd *c);

/* Hunk n, unpacked, in the reader's own buffer: chd_hunkbytes long, good
 * until the next call. NULL past the end, for a codec this reader does not
 * have, a parent's hunk, or a hunk that does not unpack. A CD hunk's sectors
 * come back with their sync bytes put back where chdman took them out, and
 * their subcode as zeros: a hash reads neither the subcode nor the ECC. */
const uint8_t *chd_hunk(chd *c, uint32_t n);

/* A CD image's first track: the frame it starts at, its frames, and how many
 * bytes at the start of each 2448-byte frame are the sector (2352 raw, 2048
 * cooked, 2336 Mode 2). False when there is no track list: a DVD image or a
 * hard disk. */
bool chd_cd_track1(chd *c, uint32_t *start, uint32_t *frames, uint32_t *datasize);

/* Any track of a CD or GD-ROM image, as libretro-common's CHD reader
 * (chdstream) finds and places it, which is what RetroArch hands rcheevos:
 * `want` is a track's number, or CHD_TRACK_FIRST_DATA (the first that is not
 * audio) or CHD_TRACK_LAST (the last one).
 *
 *   number    the track's own number
 *   audio     an AUDIO track
 *   start     the frame of the image its first stored frame is: every track
 *             before, each padded to a multiple of 4 frames as chdman stores
 *             it, then a pregap the image holds ("V" before its type)
 *   frames    its frames from there
 *   datasize  the bytes of each frame that are the sector (2352 raw, 2048
 *             cooked, 2336 Mode 2)
 *   lba       the disc sector it starts at, chdstream's count: every track
 *             before's frames, with no padding. On a GD-ROM that is 45000
 *             for track 3, because chdman pads track 2 out to there as the
 *             .gdi it came from says (MAME's GDI reader, padframes), and the
 *             disc's file system counts its sectors from there.
 *
 * Reads the CD forms (CHT2, CHTR) and the GD-ROM's (CHGD, and the older
 * CHGT the standalone Flycast also takes). False when there is no such
 * track. */
#define CHD_TRACK_FIRST_DATA (-1)
#define CHD_TRACK_LAST       (-2)

typedef struct {
	int number;
	bool audio;
	uint32_t start, frames, datasize, lba;
} chd_track;

bool chd_cd_track(chd *c, int want, chd_track *t);

#endif
