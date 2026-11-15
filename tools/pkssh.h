/* SPDX-License-Identifier: GPL-3.0-only */
/* Developer SSH while a port runs (src/sshlink.h, "over a port").
 *
 * The launcher, which runs Developer Mode's SSH everywhere else, exits for a
 * port; portkeys runs for the whole port, so it keeps SSH up meanwhile, by
 * the same rules but keys only on every link (sshlink_want_port): the cable's
 * address while its link is up, every address over Wi-Fi with SSH over
 * Wi-Fi on, nothing with Developer Mode off. zyperos.local answers as under
 * the launcher. Auto Off does not run over a port (portkeys' battery rule),
 * so nothing here holds it. */
#ifndef PKSSH_H
#define PKSSH_H

#include <sqlite3.h>
#include <stdbool.h>

/* The launcher's device database, already open (Developer Mode and SSH over
 * Wi-Fi are read from it), and its path, beside which the keys are
 * (.userdata/<device>/ssh). Ends what the launcher left running. */
void pkssh_init(sqlite3 *db, const char *db_path);

/* One look, from the main loop: what runs follows Developer Mode, the
 * cable and the radio; dropbear's log is read; mDNS answers. Returns the
 * milliseconds until the next look is due. */
int pkssh_tick(long now_ms);

/* Everything stopped, sessions too, as portkeys ends with the port */
void pkssh_stop(void);

#endif
