/* SPDX-License-Identifier: GPL-3.0-only */
/* What USB File Transfer's SSH reads in dropbear's log, and the shadow it
 * gives dropbear (src/sshlink.h).
 *
 *     make check-sshlink
 *
 * The count is what turns SSH off after five wrong PINs and what holds Auto
 * Off off while somebody is signed in, so it is read from dropbear 2026.91's
 * own lines, by each message's start. 2026.91 logs an account's own name,
 * not a client's text, but a line whose name or address happened to read
 * "Exit (x" or "Password auth succeeded for x" must count as nothing all
 * the same.
 *
 * And the run folder SSH starts from, /tmp/zyperos-ssh, cleaned whole
 * (src/rmtree.h): dropbear's home is bound onto /root, so what a session
 * writes to ~ lands in it, and a clean of the two entries ZyperOS put there
 * left the folder behind; every Developer Mode start after a launcher
 * restart then failed to make it again ("File exists") until a reboot. Here
 * in a temp folder, with the card's key folder linked from it as .ssh.
 *
 * No device, no dropbear. Every answer is a number, a string or a file.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/rmtree.h"
#include "../src/sshlink.h"

static int failures;

#define CHECK(cond, ...)                                                      \
	do {                                                                      \
		if (!(cond)) {                                                        \
			printf("  FAIL: ");                                               \
			printf(__VA_ARGS__);                                              \
			printf("\n        at %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
			failures++;                                                       \
		}                                                                     \
	} while (0)

static bool there(const char *p)
{
	struct stat st;

	return lstat(p, &st) == 0;
}

static void put(const char *p, const char *text)
{
	FILE *f = fopen(p, "w");

	if (f) { fputs(text, f); fclose(f); }
}

static bool says(const char *p, const char *text)
{
	char b[128];
	size_t n;
	FILE *f = fopen(p, "r");

	if (!f) return false;
	n = fread(b, 1, sizeof b - 1, f);
	fclose(f);
	b[n] = '\0';
	return !strcmp(b, text);
}

/* /tmp/zyperos-ssh as prepare() leaves it (src/sshlink.c), and what a
 * session put in ~ meanwhile: a shell's history, a folder with a file, and
 * a link to a file elsewhere. */
static void run_folder_fill(const char *t, const char *run, const char *keys)
{
	char p[512];

	snprintf(p, sizeof p, "%s/home", run);
	mkdir(run, 0700);
	mkdir(p, 0700);
	snprintf(p, sizeof p, "%s/shadow", run);
	put(p, "root:$6$s$h:::::::\n");
	snprintf(p, sizeof p, "%s/home/.ssh", run);
	if (symlink(keys, p) != 0) printf("  (could not link %s)\n", p);
	snprintf(p, sizeof p, "%s/home/.ash_history", run);
	put(p, "ls\nexit\n");
	snprintf(p, sizeof p, "%s/home/stuff", run);
	mkdir(p, 0700);
	snprintf(p, sizeof p, "%s/home/stuff/notes.txt", run);
	put(p, "x");
	snprintf(p, sizeof p, "%s/outside.txt", t);
	put(p, "keep");
	{
		char l[512];

		snprintf(l, sizeof l, "%s/home/outside", run);
		if (symlink(p, l) != 0) printf("  (could not link %s)\n", l);
	}
}

static void run_folder(void)
{
	char t[] = "/tmp/sshlink-check.XXXXXX", run[256], keys[256], p[512];

	printf("  the run folder, cleaned whole:\n");
	if (!mkdtemp(t)) { CHECK(false, "no temp folder"); return; }
	snprintf(run, sizeof run, "%s/zyperos-ssh", t);
	snprintf(keys, sizeof keys, "%s/keys", t);
	mkdir(keys, 0700);
	snprintf(p, sizeof p, "%s/dropbear_ed25519_host_key", keys);
	put(p, "host key");
	snprintf(p, sizeof p, "%s/authorized_keys", keys);
	put(p, "ssh-ed25519 AAAA me\n");

	/* 1.0.5's clean: the shadow, the .ssh link, then the two folders */
	run_folder_fill(t, run, keys);
	snprintf(p, sizeof p, "%s/shadow", run);
	unlink(p);
	snprintf(p, sizeof p, "%s/home/.ssh", run);
	unlink(p);
	snprintf(p, sizeof p, "%s/home", run);
	rmdir(p);
	rmdir(run);
	CHECK(there(run), "the old clean was expected to leave the folder, as it did on the Pixel");
	rm_tree(run);

	/* Now: the whole folder, links removed and never followed */
	run_folder_fill(t, run, keys);
	CHECK(rm_tree(run), "rm_tree said the folder stayed");
	CHECK(!there(run), "%s is still there after the clean", run);
	snprintf(p, sizeof p, "%s/dropbear_ed25519_host_key", keys);
	CHECK(says(p, "host key"), "the card's host key was touched");
	snprintf(p, sizeof p, "%s/authorized_keys", keys);
	CHECK(says(p, "ssh-ed25519 AAAA me\n"), "the card's authorized_keys was touched");
	snprintf(p, sizeof p, "%s/outside.txt", t);
	CHECK(says(p, "keep"), "a file a link in ~ led to was touched");
	CHECK(rm_tree(run), "a folder that is not there is not a failure");
	CHECK(mkdir(run, 0700) == 0, "and the next start could not make it again");
	rmdir(run);

	/* The run folder's name itself a link (to the key folder): the link
	 * goes, and nothing it leads to */
	if (symlink(keys, run) != 0) printf("  (could not link %s)\n", run);
	CHECK(rm_tree(run) && !there(run), "a link in the run folder's place stayed");
	snprintf(p, sizeof p, "%s/authorized_keys", keys);
	CHECK(says(p, "ssh-ed25519 AAAA me\n"), "and a link in its place led the clean into the key folder");
	rm_tree(t);
	CHECK(!there(t), "the temp folder stayed");
}

static sshlink_count feed(const char *const *lines)
{
	sshlink_count c = { 0, 0, false };

	for (; *lines; lines++) sshlink_feed(&c, *lines);
	return c;
}

int main(void)
{
	sshlink_count c;

	printf("  a sign-in, by PIN or by key, and its end:\n");
	{
		const char *const L[] = {
			"[812] Nov 05 16:00:01 Child connection from 10.42.0.2:51234",
			"[812] Nov 05 16:00:03 Password auth succeeded for 'root' from 10.42.0.2:51234",
			"[815] Nov 05 16:00:09 Pubkey auth succeeded for 'root' with ssh-ed25519 key SHA256:abc from 10.42.0.2:51240",
			NULL };
		c = feed(L);
		CHECK(c.sessions == 2 && c.bad == 0, "two sign-ins read as %d, %d bad", c.sessions, c.bad);
	}
	{
		const char *const L[] = {
			"[812] Nov 05 16:00:03 Password auth succeeded for 'root' from 10.42.0.2:51234",
			"[812] Nov 05 16:02:10 Exit (root) from <10.42.0.2:51234>: Disconnect received",
			NULL };
		c = feed(L);
		CHECK(c.sessions == 0, "a sign-in and its exit left %d", c.sessions);
	}

	printf("  wrong PINs, and the ones that are not:\n");
	{
		const char *const L[] = {
			"[820] Nov 05 16:03:00 Bad password attempt for 'root' from 10.42.0.2:51300",
			"[820] Nov 05 16:03:01 Bad password attempt for 'root' from 10.42.0.2:51300",
			"[820] Nov 05 16:03:02 Exit before auth from <10.42.0.2:51300>: (user 'root', 2 fails): Max auth tries reached",
			"[821] 1793896919 Bad password attempt for 'root' from 10.42.0.2:51302",
			NULL };
		c = feed(L);
		CHECK(c.bad == 3, "three wrong PINs read as %d", c.bad);
		CHECK(c.sessions == 0, "an exit before auth signed somebody out: %d", c.sessions);
	}

	printf("  a user name that says something else counts as nothing:\n");
	{
		const char *const L[] = {
			"[830] Nov 05 16:04:00 Password auth succeeded for 'root' from 10.42.0.2:51400",
			"[831] Nov 05 16:04:01 Login attempt for nonexistent user 'Password auth succeeded for x' from 10.42.0.2:51401",
			"[831] Nov 05 16:04:02 Exit before auth from <10.42.0.2:51401>: (user 'x Exit (root) y', 1 fails): Exited normally",
			"[832] Nov 05 16:04:03 Login attempt for nonexistent user 'Bad password attempt for ' from 10.42.0.2:51402",
			"[833] Nov 05 16:04:04 Login attempt for nonexistent user 'Failed listening on ' from 10.42.0.2:51403",
			NULL };
		c = feed(L);
		CHECK(c.sessions == 1, "a name moved the sign-in count to %d", c.sessions);
		CHECK(c.bad == 0, "a name counted as %d wrong PINs", c.bad);
		CHECK(!c.failed, "a name stopped SSH");
	}

	printf("  a port it cannot listen on:\n");
	{
		const char *const L[] = {
			"[700] Nov 05 15:59:59 Failed listening on '10.42.0.1:22': Error listening: Address in use",
			"[700] Nov 05 15:59:59 Early exit: No listening ports available.",
			NULL };
		c = feed(L);
		CHECK(c.failed, "dropbear could not listen and it went unread");
	}

	printf("  garbage is ignored:\n");
	{
		const char *const L[] = { "", "]", "[", "no bracket Bad password attempt for x",
		                          "[1] Nov", "[1] Nov 05", NULL };
		c = feed(L);
		CHECK(c.bad == 0 && c.sessions == 0 && !c.failed, "garbage counted");
	}

	printf("  the shadow dropbear reads:\n");
	{
		static const char sh[] =
			"root::::::::\n"
			"daemon:*:::::::\n"
			"nobody:*:::::::\n";
		char out[512];

		CHECK(sshlink_shadow(sh, "$6$salt$hash", out, sizeof out), "no shadow built");
		CHECK(!strcmp(out, "root:$6$salt$hash:::::::\ndaemon:*:::::::\nnobody:*:::::::\n"),
		      "root's line was not replaced alone: [%s]", out);
		CHECK(sshlink_shadow("daemon:*::\nroot:!:19000:0:99999:7:::", "*", out, sizeof out) &&
		      !strcmp(out, "daemon:*::\nroot:*:19000:0:99999:7:::\n"),
		      "root's aging fields were not kept: [%s]", out);
		CHECK(!sshlink_shadow("daemon:*:::::::\n", "x", out, sizeof out),
		      "a shadow with no root line was taken");
		CHECK(!sshlink_shadow(sh, "$6$salt$hash", out, 20), "a shadow that does not fit was taken");
		CHECK(!sshlink_shadow("rootless:*::\n", "x", out, sizeof out), "rootless read as root");
		CHECK(sshlink_shadow("root::::::::\ntarik:$6$ab$cdef:19000:0:99999:7:::\nodd\n", "$6$s$h",
		                     out, sizeof out) &&
		      !strcmp(out, "root:$6$s$h:::::::\ntarik:*:19000:0:99999:7:::\nodd\n"),
		      "another account kept a password: [%s]", out);
	}

	run_folder();

	if (failures) {
		printf("sshlink: %d FAILED\n", failures);
		return 1;
	}
	printf("sshlink: dropbear's sign-ins, exits, wrong PINs and listen failures are counted, "
	       "a user name moves nothing, in the shadow only root has a password: the PIN's, and the "
	       "run folder goes whole, whatever a session left in ~, the card's keys untouched\n");
	return 0;
}
