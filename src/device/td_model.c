/* SPDX-License-Identifier: GPL-3.0-only */
/* A handheld model's parts (src/device/td_model.h). No SDL. */
#include "td_model.h"

#include <stddef.h>

const td_part *td_part_of(const td_model *m, td_key k)
{
	int i;

	for (i = 0; m && i < m->n_parts; i++)
		if (m->parts[i].key == k) return &m->parts[i];
	return NULL;
}
