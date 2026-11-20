/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_ABOUT_H
#define ZYPEROS_ABOUT_H

#include <stdbool.h>
#include <stdint.h>
#include "menu.h"

/* About's second page. Storage is the card's filesystem, in bytes. Battery
 * capacities are the controller's microamp-hours; zero means unavailable.
 * Full/design is an estimate, not an independently measured wear reading.
 * learned: how many times the gauge measured the full charge (the Pixel 2
 * kernel's charge_full_learned, 9-px2-rk817-wear.patch). 0 is a full charge
 * that is still the design's guess, so About says it is not measured yet;
 * -1 is a gauge that does not say (the Brick, a host, an older kernel). */
typedef struct {
	bool storage_known;
	uint64_t total_bytes, used_bytes, free_bytes;
	uint64_t full_uah, design_uah;
	int learned;
} about_info;

#define ABOUT_DETAIL_ROWS 6
#define ABOUT_VALUE_LEN 48
typedef struct { char value[ABOUT_DETAIL_ROWS][ABOUT_VALUE_LEN]; } about_values;

/* statvfs's block counts, converted with overflow and consistency checks.
 * Free is space available to the application; used excludes reserved blocks. */
bool about_storage_counts(about_info *info, uint64_t unit, uint64_t blocks,
                          uint64_t free_blocks, uint64_t available_blocks);
/* Refresh both sources; a missing source clears its previous values. A NULL
 * battery directory is a host or device without charge capacity readings. */
void about_read(about_info *info, const char *card, const char *battery_dir);
/* Six read-only rows, in the approved Details order. GB is decimal; capacity
 * is rounded to mAh and grouped; the health estimate is capped at 100%. A
 * full charge never measured (learned 0) at the design's shows "Not measured
 * yet" for both health and full charge, not a 100% that means nothing. */
int about_detail_rows(const about_info *info, menu_row rows[ABOUT_DETAIL_ROWS],
                       about_values *buf);
#endif
