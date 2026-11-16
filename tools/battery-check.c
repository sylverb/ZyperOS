/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-battery
 *
 * A slow combined cache misses plug/unplug deadlines. An unconditional dirty
 * result wastes draws at every poll. Reading capacity on each charger poll
 * wastes gauge reads. Exercise the production cache against those failures.
 * This models published kernel readings, not hardware propagation or SDL
 * presentation time. No device, SDL, radio commands or real-time sleeps.
 * Then the rule both devices read the charger by (src/charging_rule.h): the
 * cable's online decides, the battery's status only where there is none. */
#include "../src/battery_poll.h"
#include "../src/charging_rule.h"

#include <stdio.h>

static int failures;
static unsigned transitions, longest_ms;
static int capacity, capacity_reads, charger_reads;
static bool online, capacity_ok, charger_ok;

#define CHECK(cond, what) do { \
	if (!(cond)) { printf("  FAIL: %s (line %d)\n", what, __LINE__); failures++; } \
} while (0)

/* Only the hardware read is substituted. Failed reads leave outputs alone,
 * as platform reads can; stale charging must not suppress a warning forever. */
static bool read_battery(int *pct, bool *charging)
{
	bool ok = true;
	if (pct) {
		capacity_reads++;
		if (capacity_ok) *pct = capacity;
		else ok = false;
	}
	if (charging) {
		charger_reads++;
		if (charger_ok) *charging = online;
		else ok = false;
	}
	return ok;
}

static void reset(void)
{
	capacity = 50;
	online = false;
	capacity_ok = charger_ok = true;
	capacity_reads = charger_reads = 0;
}

/* A change can arrive at any millisecond relative to the existing input
 * loop. Cover every phase, both directions, and the uint32_t clock wrap.
 * The dirty result is the request to draw; a stable state must not emit it. */
static void deadlines(unsigned step, uint32_t start)
{
	for (unsigned phase = 1; phase <= 250; phase++) {
		for (int plugged = 0; plugged <= 1; plugged++) {
			battery_poll b = { 0 };
			unsigned at = 0, draws = 0;
			reset();
			online = !plugged;
			battery_poll_update(&b, start, read_battery);
			for (unsigned t = step; t <= 1500; t += step) {
				if (t >= phase) online = plugged;
				if (battery_poll_update(&b, start + t, read_battery)) {
					draws++;
					if (!at) at = t;
				}
			}
			CHECK(at >= phase && at - phase <= 250,
			      "plug/unplug redraw missed the 250 ms deadline");
			CHECK(draws == 1, "one cable change must request exactly one redraw");
			CHECK(b.charging == (bool)plugged, "cached charger state is stale");
			CHECK(capacity_reads == 1, "charger polling read capacity again");
			transitions++;
			if (at >= phase && at - phase > longest_ms) longest_ms = at - phase;
		}
	}
}

static void stable(void)
{
	for (int plugged = 0; plugged <= 1; plugged++) {
		battery_poll b = { 0 };
		unsigned draws = 0;
		reset();
		online = plugged;
		CHECK(battery_poll_update(&b, 0, read_battery), "initial battery was not drawn");
		for (unsigned t = 1; t <= 10000; t++) {
			draws += battery_poll_update(&b, t, read_battery);
			/* The corner and Auto Off can ask at the same time. */
			draws += battery_poll_update(&b, t, read_battery);
		}
		CHECK(draws == 0, "stable battery requested extra redraws");
		CHECK(capacity_reads == 3, "capacity did not keep its five-second interval");
		CHECK(charger_reads > 3 && charger_reads <= 51,
		      "charger reads must be frequent and bounded to five per second");
	}
}

static void capacity_and_errors(void)
{
	battery_poll b = { 0 };
	reset();
	battery_poll_update(&b, 0, read_battery);
	capacity = 2;
	CHECK(!battery_poll_update(&b, 4999, read_battery), "capacity changed before five seconds");
	CHECK(b.pct == 50, "capacity cache was bypassed");
	CHECK(battery_poll_update(&b, 5000, read_battery), "low capacity did not request a redraw");
	CHECK(b.valid && b.pct == 2 && !b.charging, "low battery reading was lost");
	online = true;
	CHECK(battery_poll_update(&b, 5200, read_battery), "plug did not clear the low battery mark");
	CHECK(b.charging && b.pct == 2, "plug changed the cached capacity");
	charger_ok = false;
	CHECK(battery_poll_update(&b, 5400, read_battery), "failed read retained charging");
	CHECK(!b.charging && b.valid && b.pct == 2, "failed charger read hid the low battery");
	CHECK(!battery_poll_update(&b, 5600, read_battery), "repeated failure requested another redraw");
	charger_ok = true;
	CHECK(battery_poll_update(&b, 5800, read_battery), "charger recovery did not request a redraw");
	capacity_ok = false;
	CHECK(battery_poll_update(&b, 10000, read_battery), "missing capacity did not clear the mark");
	CHECK(!b.valid && b.charging, "missing capacity blocked the charger read");
	online = false;
	CHECK(battery_poll_update(&b, 10200, read_battery), "capacity failure blocked unplug");
	CHECK(!b.charging, "capacity failure left Auto Off on the charger");
	capacity_ok = true;
	CHECK(battery_poll_update(&b, 15000, read_battery), "capacity recovery did not request a redraw");
	CHECK(b.valid && b.pct == 2, "capacity did not recover");
}

/* The charger-online rule. Online 0 is off the charger whatever the status
 * says; online 1 is on it whatever the status says, as rk817 says
 * Discharging on the cable with a full battery (the Pixel 2's 2026-11-04);
 * with no online answer, Charging or Full is on it and anything else is not. */
static void charger_rule(void)
{
	CHECK(!charging_from(0, "Charging\n"), "online 0 but Charging: on the charger");
	CHECK(!charging_from(0, "Full\n"), "online 0 but Full: on the charger");
	CHECK(!charging_from(0, NULL), "online 0, no status: on the charger");
	CHECK(charging_from(1, "Discharging\n"), "online 1 but Discharging: off the charger");
	CHECK(charging_from(1, "Full\n"), "online 1 and Full: off the charger");
	CHECK(charging_from(1, NULL), "online 1, no status: off the charger");
	CHECK(charging_from(-1, "Charging\n"), "no online, Charging: off the charger");
	CHECK(charging_from(-1, "Full"), "no online, Full: off the charger");
	CHECK(!charging_from(-1, "Discharging\n"), "no online, Discharging: on the charger");
	CHECK(!charging_from(-1, "Not charging\n"), "no online, Not charging: on the charger");
	CHECK(!charging_from(-1, ""), "no online, an empty status: on the charger");
	CHECK(!charging_from(-1, NULL), "no online, no status: on the charger");
	CHECK(charging_from(2, "Charging\n") && !charging_from(2, "Unknown\n"),
	      "online neither 0 nor 1: the status speaks, as with none");
}

int main(void)
{
	puts("battery: the charger's online decides, the status only without it");
	charger_rule();
	puts("battery: plug/unplug deadlines at 8 ms and 16 ms, including clock wrap");
	deadlines(8, 0);
	deadlines(16, 0);
	deadlines(8, UINT32_MAX - 99);
	deadlines(16, UINT32_MAX - 99);
	printf("  %u transitions; longest redraw request delay: %u ms\n", transitions, longest_ms);
	puts("battery: stable state, bounded reads, capacity changes and read failures");
	stable();
	capacity_and_errors();
	if (failures) { printf("%d check(s) failed\n", failures); return 1; }
	puts("ok: every battery check passed");
	return 0;
}
