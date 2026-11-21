/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Host checks for the GKD Pixel 2's battery level at a start: rk817_rest_uv
 * and rk817_start_from, taken out of the kernel patch as they stand
 * (patches/linux/9-px2-rk817-start-soc.patch) into rule.c, with
 * RK817_START_CHARGING_UA as it is now and rk817_start_level
 * (patches/linux/9-px2-rk817-wear3-charger-start.patch), the full charge a
 * start measures (rk817_learn_fcc and
 * RK817_LEARN_MAX_SOC, patches/linux/9-px2-rk817-wear.patch), the empty end
 * (rk817_empty_due and the RK817_EMPTY_ limits,
 * patches/linux/9-px2-rk817-wear2-empty.patch) and ZyperOS's BATT_EMPTY_PCT
 * (src/status.h), and the Pixel 2's OCV table, internal resistance, design
 * minimum and charge's end current, taken out of
 * its device tree (dts/rk3326s-gkd-pixel2.dts) into battery.h. plastron's
 * `make start-soc-check` makes both and runs this; no Docker, no kernel, no
 * device. Every check states the rule it holds, so a failure says which one
 * broke.
 */
#include <stdbool.h>
#include <stdio.h>

typedef long long s64;

static s64 div_s64(s64 dividend, int divisor)
{
	return dividend / divisor;
}

#include "rule.c"
#include "battery.h"

#ifndef PX2_R_UOHM
#error "no factory-internal-resistance-micro-ohms found in the Pixel 2's device tree"
#endif
#ifndef PX2_VMIN_UV
#error "no voltage-min-design-microvolt found in the Pixel 2's device tree"
#endif
#ifndef PX2_TERM_UA
#error "no charge-term-current-microamp found in the Pixel 2's device tree"
#endif
#ifndef ZYPEROS_BATT_EMPTY_PCT
#error "no BATT_EMPTY_PCT found in ZyperOS's src/status.h"
#endif

static int fails, checks;

#define CHECK(cond, ...) do { \
	checks++; \
	if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

#define N_OCV ((int)(sizeof(PX2_OCV) / sizeof(PX2_OCV[0])))

/*
 * The level the OCV table gives for a voltage, as the kernel reads it
 * (power_supply_ocv2cap_simple, fixp_linear_interpolate): whole points,
 * cut, not rounded
 */
static int ocv2cap(int uv)
{
	int i, high, low, x0, y0, x1, y1;

	for (i = 0; i < N_OCV; i++)
		if (uv > PX2_OCV[i][0])
			break;
	if (i == 0)
		high = low = i;
	else if (i == N_OCV)
		high = low = i - 1;
	else
		high = (low = i) - 1;
	x0 = PX2_OCV[low][0];
	y0 = PX2_OCV[low][1];
	x1 = PX2_OCV[high][0];
	y1 = PX2_OCV[high][1];
	if (y0 == y1 || uv == x0)
		return y0;
	if (x1 == x0 || uv == x1)
		return y1;
	return y0 + (y1 - y0) * (uv - x0) / (x1 - x0);
}

/* The voltage at rest for a whole level, from the table */
static int rest_at(int pct)
{
	int i;

	for (i = 0; i + 1 < N_OCV; i++)
		if (PX2_OCV[i + 1][1] <= pct) {
			int v1 = PX2_OCV[i][0], c1 = PX2_OCV[i][1];
			int v2 = PX2_OCV[i + 1][0], c2 = PX2_OCV[i + 1][1];

			return v2 + (int)((s64)(v1 - v2) * (pct - c2) / (c1 - c2));
		}
	return PX2_OCV[N_OCV - 1][0];
}

/* The sample a start takes with `ua` flowing (+ into the battery), the
 * battery at rest at `rest_uv`, its path's resistance `r_uohm` */
static int sampled(int rest_uv, int ua, int r_uohm)
{
	return rest_uv + (int)((s64)ua * r_uohm / 1000000);
}

/* One 8 s update at the empty end, as rk817_empty_cal makes it: the battery
 * at rest at `rest_true`, `ua` flowing (+ into it) through a path of
 * `r_uohm`, read and corrected with the device tree's resistance, the
 * counter at `counter` (thousandths of a percent) */
static bool update(int *run, int rest_true, int ua, int r_uohm, int counter,
		   bool plugged_in, bool held)
{
	int rest = rk817_rest_uv(sampled(rest_true, ua, r_uohm), ua, PX2_R_UOHM);

	return rk817_empty_due(run, plugged_in, held, rest, counter, ocv2cap(rest) * 1000);
}

/* `n` updates the same; how many of them set the counter */
static int updates(int *run, int n, int rest_true, int ua, int r_uohm, int counter,
		   bool plugged_in, bool held)
{
	int due = 0;

	while (n-- > 0)
		due += update(run, rest_true, ua, r_uohm, counter, plugged_in, held);
	return due;
}

int main(void)
{
	int rest49 = rest_at(49), s;

	printf("start-soc-check: the device tree\n");
	CHECK(PX2_R_UOHM == 111000, "the Pixel 2's internal resistance is 111 mOhm (%d uOhm)",
	      PX2_R_UOHM);
	CHECK(N_OCV == 21 && PX2_OCV[0][1] == 100 && PX2_OCV[N_OCV - 1][1] == 0,
	      "the OCV table runs from 100 to 0%% in 21 steps (%d, %d to %d)",
	      N_OCV, PX2_OCV[0][1], PX2_OCV[N_OCV - 1][1]);
	CHECK(ocv2cap(3848000) == 50 && ocv2cap(4163000) == 85,
	      "the table reads 3.848 V as 50%% and 4.163 V as 85%% (%d, %d)",
	      ocv2cap(3848000), ocv2cap(4163000));
	CHECK(ocv2cap(rest49) == 49, "49%% at rest is %d uV, which reads 49%% (%d)",
	      rest49, ocv2cap(rest49));

	printf("start-soc-check: the voltage at rest\n");
	CHECK(rk817_rest_uv(3800000, 500000, 0) == 3800000,
	      "without a resistance, the sample as it is");
	CHECK(rk817_rest_uv(3800000, 500000, -22) == 3800000,
	      "without a resistance in the device tree (-EINVAL), the sample as it is");
	CHECK(rk817_rest_uv(3800000, 0, PX2_R_UOHM) == 3800000,
	      "with no current, the sample as it is");
	CHECK(rk817_rest_uv(3900000, 1000000, PX2_R_UOHM) == 3900000 - 111000,
	      "1 A into the battery: 111 mV under the sample (%d)",
	      rk817_rest_uv(3900000, 1000000, PX2_R_UOHM));
	CHECK(rk817_rest_uv(3800000, -300000, PX2_R_UOHM) == 3800000 + 33300,
	      "300 mA out of the battery: 33.3 mV over the sample (%d)",
	      rk817_rest_uv(3800000, -300000, PX2_R_UOHM));
	CHECK(rk817_rest_uv(3800000, -5000000, 1000000) == 3800000 + 5000000,
	      "5 A through 1 Ohm does not overflow (%d)",
	      rk817_rest_uv(3800000, -5000000, 1000000));

	printf("start-soc-check: where a start's level comes from\n");
	CHECK(rk817_start_from(true, 0, 2000000) == RK817_START_NEW,
	      "a battery new to the gauge: the sample, whatever the current");
	CHECK(rk817_start_from(false, 0, 0) == RK817_START_COUNTER,
	      "a restart: the counter");
	CHECK(rk817_start_from(false, 20, -500000) == RK817_START_COUNTER,
	      "20 min off: the counter");
	CHECK(rk817_start_from(false, 20, 2000000) == RK817_START_COUNTER,
	      "20 min off, a charger's start: the counter");
	CHECK(rk817_start_from(false, 30, 0) == RK817_START_VOLTAGE,
	      "30 min off, no current: the sample");
	CHECK(rk817_start_from(false, 120, -400000) == RK817_START_VOLTAGE,
	      "2 h off, a start from POWER under the SoC's 400 mA: the sample");
	CHECK(rk817_start_from(false, 2550, -2000000) == RK817_START_VOLTAGE,
	      "2550 min off (OFF_CNT's most), 2 A out: still the sample");
	CHECK(RK817_START_CHARGING_UA == 300000, "a charger's start is over 300 mA in (%d uA)",
	      RK817_START_CHARGING_UA);
	CHECK(RK817_START_CHARGING_UA == PX2_TERM_UA,
	      "300 mA is the charger's own end of a charge, the device tree's charge-term-current (%d uA)",
	      PX2_TERM_UA);
	CHECK(rk817_start_from(false, 30, RK817_START_CHARGING_UA) == RK817_START_VOLTAGE,
	      "30 min off, 100 mA in: the sample (the limit itself)");
	CHECK(rk817_start_from(false, 30, RK817_START_CHARGING_UA + 172) == RK817_START_CHARGER,
	      "30 min off, one step of the ADC (172 uA) over 100 mA in: the counter");
	CHECK(rk817_start_from(false, 120, 1500000) == RK817_START_CHARGER,
	      "2 h off, a charger's start at 1.5 A: the counter");

	printf("start-soc-check: starts of a battery at rest at 49%%\n");
	/* A start from POWER: the SoC draws as it starts (the figure is an
	 * example; the start's log line gives the real one) */
	s = sampled(rest49, -250000, PX2_R_UOHM);
	CHECK(ocv2cap(s) == 42, "from POWER at 250 mA out, the sample alone reads 42%% (%d)",
	      ocv2cap(s));
	CHECK(ocv2cap(rk817_rest_uv(s, -250000, PX2_R_UOHM)) == 49,
	      "from POWER at 250 mA out, the voltage at rest reads 49%% (%d)",
	      ocv2cap(rk817_rest_uv(s, -250000, PX2_R_UOHM)));
	/* A charger's start: the sample alone reads 20 points high with the
	 * device tree's resistance, more with the board's path added */
	s = sampled(rest49, 1500000, PX2_R_UOHM);
	CHECK(ocv2cap(s) >= 69, "a charger's start at 1.5 A, the sample alone reads %d%%, 20 points high",
	      ocv2cap(s));
	s = sampled(rest49, 1500000, 2 * PX2_R_UOHM);
	CHECK(ocv2cap(s) >= 85, "with the path's resistance twice the cell's, %d%%: the report's 85%% is within reach",
	      ocv2cap(s));
	CHECK(rk817_start_from(false, 120, 1500000) == RK817_START_CHARGER,
	      "so a charger's start keeps the counter instead");

	printf("start-soc-check: the full charge a start measures (9-px2-rk817-wear.patch)\n");
	CHECK(RK817_LEARN_MAX_SOC == 50000, "a start measures at 50%% or under at rest (%d)",
	      RK817_LEARN_MAX_SOC);
	CHECK(rk817_learn_fcc(1730, 1730 - 980, 30000, 1730) == 1565,
	      "980 mAh out since full, to 30%% at rest, measures 1400 mAh; the full charge goes half way to it, 1565 (%d)",
	      rk817_learn_fcc(1730, 1730 - 980, 30000, 1730));
	CHECK(rk817_learn_fcc(1730, 1730 - 1211, 30000, 1730) == 1730,
	      "a battery as new, 1211 mAh out to 30%%, measures 1730: no change (%d)",
	      rk817_learn_fcc(1730, 1730 - 1211, 30000, 1730));
	CHECK(rk817_learn_fcc(1565, 1565 - 1300, 30000, 1730) == 1648,
	      "a measure over the design's (1857 mAh) counts as the design's: 1565 goes half way to 1730 (%d)",
	      rk817_learn_fcc(1565, 1565 - 1300, 30000, 1730));
	CHECK(rk817_learn_fcc(1730, 1730 - 1500, 30000, 1730) == 0,
	      "a measure over 6/5 of the design's (2142 mAh) is not taken");
	CHECK(rk817_learn_fcc(1730, 1730 - 500, 30000, 1730) == 0,
	      "a measure under half the design's (714 mAh) is not taken");
	CHECK(rk817_learn_fcc(1730, 1730 - 700, 50000, 1730) == 1565,
	      "50%% at rest, the limit itself: 700 mAh out measures 1400 (%d)",
	      rk817_learn_fcc(1730, 1730 - 700, 50000, 1730));
	CHECK(rk817_learn_fcc(1730, 1730 - 700, 51000, 1730) == 0,
	      "51%% at rest: too little out to measure");
	CHECK(rk817_learn_fcc(1730, 100, 0, 1730) == 0,
	      "0%% at rest: the table cannot see the end, not measured");
	CHECK(rk817_learn_fcc(1730, 1800, 30000, 1730) == 0 &&
	      rk817_learn_fcc(1730, 1730, 30000, 1730) == 0,
	      "nothing out since full, or more left than full: not measured");
	CHECK(rk817_learn_fcc(1730, -1, 30000, 1730) == 0,
	      "no remaining capacity read: not measured");
	{
		/* A worn battery that holds 1400 mAh, measured at 30% at rest
		 * after each full charge, from the design's 1730 */
		int fcc = 1730, n;

		for (n = 0; n < 5; n++)
			fcc = rk817_learn_fcc(fcc, fcc - 980, 30000, 1730);
		CHECK(fcc >= 1400 && fcc <= 1415,
		      "five measures of a battery that holds 1400 mAh settle within 1%% of it (%d)", fcc);
	}
	{
		/* The level at rest as the start reads it: a battery that holds
		 * 1400 mAh, 980 out, rests at the table's 30% */
		int rest = ocv2cap(rest_at(30)) * 1000;

		CHECK(rest == 30000 && rk817_learn_fcc(1730, 1730 - 980, rest, 1730) == 1565,
		      "the OCV table's 30%% (%d uV) gives the same measure (%d)",
		      rest_at(30), rk817_learn_fcc(1730, 1730 - 980, rest, 1730));
	}

	printf("start-soc-check: the empty end (9-px2-rk817-wear2-empty.patch)\n");
	CHECK(RK817_EMPTY_REST_UV == 3550000 && RK817_EMPTY_GAP_SOC == 10000 &&
	      RK817_EMPTY_UPDATES == 3,
	      "near empty is under 3.55 V at rest, 10 points under the counter, 3 updates in a row (%d, %d, %d)",
	      RK817_EMPTY_REST_UV, RK817_EMPTY_GAP_SOC, RK817_EMPTY_UPDATES);
	CHECK(PX2_OCV[N_OCV - 2][0] == 3643000 && PX2_OCV[N_OCV - 2][1] == 5 &&
	      PX2_OCV[N_OCV - 1][0] == 3444000,
	      "the table ends at 5%% for 3.643 V and 0%% for 3.444 V, as the patch says (%d, %d)",
	      PX2_OCV[N_OCV - 2][0], PX2_OCV[N_OCV - 1][0]);
	CHECK(ocv2cap(RK817_EMPTY_REST_UV) == 2 && ocv2cap(RK817_EMPTY_REST_UV - 1) <= ZYPEROS_BATT_EMPTY_PCT,
	      "under 3.55 V at rest the table reads %d%% or less, at ZyperOS's BATT_EMPTY_PCT (%d), so the level set powers off",
	      ocv2cap(RK817_EMPTY_REST_UV - 1), ZYPEROS_BATT_EMPTY_PCT);
	CHECK(RK817_EMPTY_REST_UV - PX2_VMIN_UV >= 100000 && RK817_EMPTY_REST_UV > PX2_OCV[N_OCV - 1][0],
	      "3.55 V is %d mV over the design's minimum (%d mV) and over the table's 0%%",
	      (RK817_EMPTY_REST_UV - PX2_VMIN_UV) / 1000, PX2_VMIN_UV / 1000);
	{
		int run = 0, due;

		/* A worn battery at its real end: at rest at 2%, the counter
		 * still at 32%, a game drawing 400 mA */
		due = updates(&run, 2, rest_at(2), -400000, PX2_R_UOHM, 32000, false, false);
		CHECK(due == 0, "a real empty: not on the first two updates");
		CHECK(update(&run, rest_at(2), -400000, PX2_R_UOHM, 32000, false, false),
		      "a real empty: the third in a row sets the counter to the voltage's level");
		CHECK(ocv2cap(rk817_rest_uv(sampled(rest_at(2), -400000, PX2_R_UOHM), -400000, PX2_R_UOHM)) == 2,
		      "a real empty: the level set is 2%%, where ZyperOS powers off");
		CHECK(run == 0 && updates(&run, 2, rest_at(2), -400000, PX2_R_UOHM, 32000, false, false) == 0,
		      "after it, the run starts again from nothing");

		/* The same on a worn path: twice the device tree's resistance */
		run = 0;
		due = updates(&run, 3, rest_at(1), -500000, 2 * PX2_R_UOHM, 32000, false, false);
		CHECK(due == 1, "a real empty on a path twice the device tree's, at 500 mA: set on the third");

		/* Spikes: one or two updates under, then one over */
		run = 0;
		due = updates(&run, 1, rest_at(2), -400000, PX2_R_UOHM, 60000, false, false);
		due += updates(&run, 1, rest_at(40), -400000, PX2_R_UOHM, 60000, false, false);
		due += updates(&run, 2, rest_at(2), -400000, PX2_R_UOHM, 60000, false, false);
		due += updates(&run, 1, rest_at(40), -400000, PX2_R_UOHM, 60000, false, false);
		due += updates(&run, 2, rest_at(2), -400000, PX2_R_UOHM, 60000, false, false);
		CHECK(due == 0, "spikes: one, two and two updates under 3.55 V with one over between, never three in a row: nothing set");
		/* A sag the resistance does not explain: 1.5 A through four
		 * times the device tree's, at 40% at rest, for two updates */
		run = 0;
		due = updates(&run, 2, rest_at(40), -1500000, 4 * PX2_R_UOHM, 60000, false, false);
		due += updates(&run, 1, rest_at(40), -300000, PX2_R_UOHM, 60000, false, false);
		CHECK(due == 0, "a sag of two updates at 40%% (1.5 A, four times the resistance), then a light load: nothing set");

		/* A sag under load at mid charge: the voltage at rest stays
		 * over 3.55 V, however long */
		run = 0;
		due = updates(&run, 20, rest_at(50), -1000000, 2 * PX2_R_UOHM, 90000, false, false);
		CHECK(due == 0, "50%% at rest, 1 A through twice the device tree's resistance, 20 updates: nothing set");
		due = updates(&run, 20, rest_at(20), -1200000, 2 * PX2_R_UOHM, 60000, false, false);
		CHECK(due == 0, "20%% at rest, 1.2 A through twice the device tree's resistance (%d mV read), 20 updates: nothing set",
		      sampled(rest_at(20), -1200000, 2 * PX2_R_UOHM) / 1000);

		/* The gap: 10 points or more */
		run = 0;
		due = updates(&run, 6, rest_at(2), 0, PX2_R_UOHM, 11999, false, false);
		CHECK(due == 0, "a counter under 10 points over the voltage's 2%%: nothing set");
		due = updates(&run, 3, rest_at(2), 0, PX2_R_UOHM, 12000, false, false);
		CHECK(due == 1, "10 points over: set on the third");
		run = 0;
		due = updates(&run, 6, rest_at(2), -400000, PX2_R_UOHM, 3000, false, false);
		CHECK(due == 0, "a counter that is right (3%% against 2%%): nothing set");

		/* Never on the cable, never held at the charge limit */
		run = 0;
		due = updates(&run, 6, rest_at(2), 500000, PX2_R_UOHM, 60000, true, false);
		CHECK(due == 0, "on the cable: nothing set");
		due = updates(&run, 6, rest_at(2), -100000, PX2_R_UOHM, 60000, true, true);
		CHECK(due == 0, "held at the charge limit: nothing set");
		run = 0;
		due = updates(&run, 2, rest_at(2), -400000, PX2_R_UOHM, 60000, false, false);
		due += updates(&run, 1, rest_at(2), 500000, PX2_R_UOHM, 60000, true, false);
		due += updates(&run, 2, rest_at(2), -400000, PX2_R_UOHM, 60000, false, false);
		CHECK(due == 0, "two updates, the cable in and out, two more: the cable starts the run again");
	}
	{
		/* The learn at the empty end: a battery that holds 1200 mAh,
		 * counted from the design's 1730 since a full charge. At rest at
		 * 2%, 1176 mAh went out; the counter says 554 mAh are left. */
		int fcc = 1730, left = 1730 - 1176, run = 0;

		CHECK(updates(&run, 3, rest_at(2), -400000, PX2_R_UOHM, left * 100000 / fcc, false, false) == 1 &&
		      rk817_learn_fcc(fcc, left, 2000, 1730) == 1465,
		      "the empty end of a battery that holds 1200 mAh: 1176 mAh out to 2%% at rest measures 1200, the full charge goes half way, 1465 (%d)",
		      rk817_learn_fcc(fcc, left, 2000, 1730));
		fcc = 1465;
		left = fcc - 1176;
		CHECK(updates(&run, 3, rest_at(2), -400000, PX2_R_UOHM, left * 100000 / fcc, false, false) == 1 &&
		      rk817_learn_fcc(fcc, left, 2000, 1730) == 1333,
		      "the next empty end, the counter still %d%%: 1333 (%d)",
		      left * 100 / fcc, rk817_learn_fcc(fcc, left, 2000, 1730));
		fcc = 1333;
		left = fcc - 1176;
		CHECK(updates(&run, 6, rest_at(2), -400000, PX2_R_UOHM, left * 100000 / fcc, false, false) == 0,
		      "at 1333 the counter is under 10 points over at the empty end (%d%%): left to the start's measure and the launcher's voltage rule",
		      left * 100 / fcc);
		CHECK(rk817_learn_fcc(1730, 554, 0, 1730) == 0,
		      "an empty end at 0%% at rest sets the counter but does not measure");
	}

	printf("start-soc-check: a charger's start (9-px2-rk817-wear3-charger-start.patch)\n");
	{
		/* A player's Pixel 2 on ZyperOS 1.0.6 (its logs, 2026-11-20), each
		 * start as the kernel said it, oldest first: the minutes off, the
		 * sample in mV and mA, the counter's level then, and the level the
		 * start must begin from. Its starts from POWER read +108 to +114
		 * mA with no cable in (the launcher said "off the charger" at the
		 * one that read 0%), so each after 30 minutes or more off was a
		 * charger's start under the old 100 mA, kept the counter, and the
		 * counter's fall while off piled up. */
		static const struct {
			const char *log;
			int off_min, mv, ma, counter, want_from, want_pct;
		} z[] = {
			{ "log.9", 30, 4132, 109, 38, RK817_START_VOLTAGE, 81 },
			{ "log.8", 0, 4128, 108, 37, RK817_START_COUNTER, 37 },
			{ "log.7", 640, 4114, 113, 0, RK817_START_VOLTAGE, 79 },
			{ "log.6", 0, 4328, 1796, 0, RK817_START_COUNTER, 0 },
			{ "log.5", 0, 4295, 110, 100, RK817_START_COUNTER, 100 },
			{ "log.4", 40, 4287, 114, 93, RK817_START_VOLTAGE, 94 },
			{ "log.3", 0, 4280, 113, 93, RK817_START_COUNTER, 93 },
			{ "log.2", 260, 4271, 110, 64, RK817_START_VOLTAGE, 93 },
			{ "log.1", 280, 4266, 110, 38, RK817_START_VOLTAGE, 93 },
			{ "log", 70, 4270, 113, 30, RK817_START_VOLTAGE, 93 },
		};
		int i;

		for (i = 0; i < (int)(sizeof z / sizeof z[0]); i++) {
			int ua = z[i].ma * 1000;
			int rest = rk817_rest_uv(z[i].mv * 1000, ua, PX2_R_UOHM);
			enum rk817_start_from from = rk817_start_from(false, z[i].off_min, ua);
			/* What was kept before the power-off is not in the log: the
			 * counter's level stands in for it, which only a charger's
			 * start would take */
			int soc = rk817_start_level(from, ocv2cap(rest) * 1000, z[i].counter * 1000,
						    z[i].counter * 1730 / 100, 1730);

			CHECK(from == (enum rk817_start_from)z[i].want_from && (soc + 500) / 1000 == z[i].want_pct,
			      "the player's %s, %d min off, %d mV at %d mA, counter %d%%: %s, %d%% (%d%%)",
			      z[i].log, z[i].off_min, z[i].mv, z[i].ma, z[i].counter,
			      z[i].want_from == RK817_START_VOLTAGE ? "from the voltage" : "the counter",
			      z[i].want_pct, (soc + 500) / 1000);
		}
		CHECK(rk817_start_from(false, 640, 113000) == RK817_START_VOLTAGE &&
		      ocv2cap(rk817_rest_uv(4114000, 113000, PX2_R_UOHM)) > ZYPEROS_BATT_EMPTY_PCT,
		      "the start that read 0%% and powered off reads 79%%, over ZyperOS's BATT_EMPTY_PCT (%d%%)",
		      ZYPEROS_BATT_EMPTY_PCT);
	}
	CHECK(RK817_START_CHARGING_UA >= 2 * 114000,
	      "300 mA is over twice the most a start from POWER read with no cable in (+114 mA)");
	CHECK(rk817_start_from(false, 120, -55000) == RK817_START_VOLTAGE,
	      "another Pixel 2's start from POWER, -55 mA, 2 h off: from the voltage, as before");
	CHECK(rk817_start_from(false, 30, 1796000) == RK817_START_CHARGER &&
	      rk817_start_from(false, 30, 1500000) == RK817_START_CHARGER,
	      "a cable's start at 1796 mA (the player's log.6) or 1.5 A after 30 min off: a charger's start");
	s = sampled(rest49, RK817_START_CHARGING_UA, 2 * PX2_R_UOHM);
	CHECK(rk817_rest_uv(s, RK817_START_CHARGING_UA, PX2_R_UOHM) - rest49 <= 33300,
	      "at 300 mA, a path twice the device tree's resistance leaves 33 mV at most in the voltage at rest (%d uV)",
	      rk817_rest_uv(s, RK817_START_CHARGING_UA, PX2_R_UOHM) - rest49);
	{
		/* The 1.0.5 report (85% when turned off, 49% two hours later): a
		 * cable's start at 1.5 A after 2 h off, at 49% at rest, the path's
		 * resistance twice the device tree's. Kept before the power-off:
		 * 49%, 848 mAh of 1730. The counter fell about 100 mA for the two
		 * hours off, 200 mAh, to 37%. */
		int ua = 1500000, left = 848, fcc = 1730;
		int rest = rk817_rest_uv(sampled(rest49, ua, 2 * PX2_R_UOHM), ua, PX2_R_UOHM);
		enum rk817_start_from from = rk817_start_from(false, 120, ua);
		int counter = (left - 200) * 100000 / fcc;
		int soc = rk817_start_level(from, ocv2cap(rest) * 1000, counter, left, fcc);

		CHECK(from == RK817_START_CHARGER && ocv2cap(rest) >= 65,
		      "the 1.0.5 case: a charger's start, its sample still reading high (%d%%)", ocv2cap(rest));
		CHECK((soc + 500) / 1000 == 49,
		      "the 1.0.5 case: the level kept before the power-off, 49%%, not the sample's %d%% nor the drifted counter's %d%% (%d%%)",
		      ocv2cap(rest), (counter + 500) / 1000, (soc + 500) / 1000);
	}
	CHECK(rk817_start_level(RK817_START_CHARGER, 85000, 2000, 1038, 1730) == 60000,
	      "a cable's start after six hours off: the level kept (1038 of 1730 mAh, 60%%), not the counter 35 points under it");
	CHECK(rk817_start_level(RK817_START_CHARGER, 85000, 37000, -1, 1730) == 37000,
	      "a charger's start with no level kept: the counter's");
	CHECK(rk817_start_level(RK817_START_CHARGER, 85000, 37000, 1900, 1730) == 100000,
	      "a level kept over the full charge reads 100%%");
	CHECK(rk817_start_level(RK817_START_CHARGER, 85000, 37000, 800, 0) == 37000,
	      "with no full charge to read it against, the counter's");
	CHECK(rk817_start_level(RK817_START_COUNTER, 85000, 37000, 1038, 1730) == 37000,
	      "a start after a short time off keeps the counter, as before");
	CHECK(rk817_start_level(RK817_START_VOLTAGE, 85000, 37000, 1038, 1730) == 85000 &&
	      rk817_start_level(RK817_START_NEW, 85000, 37000, 1038, 1730) == 85000,
	      "a start from the voltage, and a battery new to the gauge: the voltage's");
	CHECK(rk817_start_level(RK817_START_CHARGER, 0, 0, 65535, 1730) == 100000,
	      "the largest level kept (65535 mAh) does not overflow");

	printf("start-soc-check: %d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}
