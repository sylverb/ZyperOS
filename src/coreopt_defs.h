/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_COREOPT_DEFS_H
#define ZYPEROS_COREOPT_DEFS_H

/* Core option definitions for the Core Settings page: what a core offers
 * (key, description, default, allowed values), from Diatom's OPTIONS lines,
 * a cache under the player's userdata, or a small seed for blueMSX so MSX
 * mapper and sound cart are configurable before a first launch. The optional
 * whitelist (config/coreopts-show.cfg) can later hide keys; an absent file
 * or an empty section for a core shows every definition. No SDL. */

#include <stdbool.h>
#include <stddef.h>

#define CODEFS_MAX       96
#define CODEFS_KEY_MAX   80
#define CODEFS_VAL_MAX   128
#define CODEFS_DESC_MAX  160
#define CODEFS_VALUES_MAX 64
#define CODEFS_PIPE_MAX  512

typedef struct {
	char key[CODEFS_KEY_MAX];
	char desc[CODEFS_DESC_MAX];
	char deflt[CODEFS_VAL_MAX];
	char value[CODEFS_VAL_MAX];   /* last known / current when from OPTIONS */
	char pipe[CODEFS_PIPE_MAX]; /* values joined by |, as Diatom emits */
	char vals[CODEFS_VALUES_MAX][CODEFS_VAL_MAX];
	int  nvals;
} codef;

typedef struct {
	codef opt[CODEFS_MAX];
	int   n;
} codefs;

/* Parse one Diatom OPTION line into `o`. False if it is not an OPTION line
 * or lacks a key. */
bool codef_parse_line(const char *line, codef *o);

/* Split `pipe` ("a|b|c") into `o->vals` / `o->nvals`; copies into `o->pipe`. */
void codef_set_values(codef *o, const char *pipe);

/* Read a cache file of OPTION lines into `out` (cleared first). */
bool codefs_load_file(codefs *out, const char *path);

/* Write `in` as OPTION lines. */
bool codefs_save_file(const codefs *in, const char *path);

/* Path `$userdata/coreopt-defs/<core>.txt` into `out`. */
void codefs_cache_path(const char *userdata, const char *core,
                       char *out, size_t n);

/* Drop every cached defs file under `$userdata/coreopt-defs/`: MENU >
 * Language changed, and the OPTION lines' desc were for the old tongue. */
void codefs_cache_clear(const char *userdata);

/* blueMSX seed: cartmapper + additional_cart, so MSX works with an empty
 * cache. False for any other core name. */
bool codefs_seed(codefs *out, const char *core);

/* Whitelist: keys listed under `core=<name>` in the show file. `n_allow` 0
 * means show every definition (absent file, unknown core, or empty section). */
#define CODEFS_ALLOW_MAX 64
int codefs_load_allow(const char *path, const char *core,
                      char allow[][CODEFS_KEY_MAX], int max);

/* Copy defs from `in` that the whitelist keeps into `out`. `n_allow` 0 keeps
 * every one. */
void codefs_filter(const codefs *in, codefs *out,
                   const char allow[][CODEFS_KEY_MAX], int n_allow);

/* Index of `key` in `o->vals`, or -1. */
int codef_value_index(const codef *o, const char *value);

/* Step `cur` by `dir` (+1 / -1) round the values; returns the new index. */
int codef_value_step(const codef *o, int cur, int dir);

/* Database key "coreopt.<tag>.<option>" into `out`. */
void codef_db_key(const char *tag, const char *option, char *out, size_t n);

#endif
