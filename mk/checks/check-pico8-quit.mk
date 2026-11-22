CHECKS += check-pico8-quit
#| MENU, then X over your PICO-8 ends it at once, as Quit ends a game in Diatom: launch.sh's
#| run_port with stand-ins, a port script that execs its program (`sd/pixel2/PICO-8.sh` execs
#| pico8_64, `OpenBOR.sh` OpenBOR, so the group's leader is the program and nothing else is in the
#| group) TERMed within a second of the in-game menu's Quit (USR2) and of POWER (USR1), never only
#| by the whole group's TERM 2 s later, ending by its own TERM with status 0 and the shelf back or
#| the handheld powering off; a script that runs its program as a child and goes on after it (a
#| PortMaster port's) has its program TERMed at once as before and goes on to its lines after the
#| game. Linux only, as `port_end` reads /proc; elsewhere it says it skipped, and
#| `PICO8_QUIT_DOCKER=1` runs it in tools/p8fx-gl.Dockerfile's Ubuntu 24.04
