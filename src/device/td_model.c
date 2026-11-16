/* SPDX-License-Identifier: GPL-3.0-only */
/* A handheld model's parts (src/device/td_model.h). No SDL. */
#include "td_model.h"
#include "../design.h"

#include <stddef.h>

const td_part *td_part_of(const td_model *m, td_key k)
{
	int i;

	for (i = 0; m && i < m->n_parts; i++)
		if (m->parts[i].key == k) return &m->parts[i];
	return NULL;
}

bool td_spins(const td_model *m, unsigned lit)
{
	bool ledge = false, off = false;
	int i;

	for (i = 0; m && i < m->n_parts; i++) {
		if (!(lit >> m->parts[i].key & 1u)) continue;
		if (m->parts[i].face == TD_FACE_LEDGE) ledge = true;
		else off = true;
	}
	return ledge && off;
}

float td_swing(int ms)
{
	const int leg = DS_MS_HOLD_SPIN + DS_MS_SPIN;
	int at;

	if (ms <= 0) return 0.0f;
	at = ms % (2 * leg);
	/* Held on the front, turning round, held on the back, turning back */
	if (at < DS_MS_HOLD_SPIN) return 0.0f;
	if (at < leg) return ds_turn((float)(at - DS_MS_HOLD_SPIN) / DS_MS_SPIN);
	if (at < leg + DS_MS_HOLD_SPIN) return 1.0f;
	return 1.0f - ds_turn((float)(at - leg - DS_MS_HOLD_SPIN) / DS_MS_SPIN);
}

td_view td_view_mix(td_view a, td_view b, float k)
{
	return (td_view){ a.deg + (b.deg - a.deg) * k, a.s + (b.s - a.s) * k,
	                  a.pitch + (b.pitch - a.pitch) * k };
}
