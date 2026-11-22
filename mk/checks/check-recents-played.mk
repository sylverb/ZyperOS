CHECKS += check-recents-played
#| a program of its own reaches Recents and Played (src/stats.h, "a program of its own"):
#| launch.sh's run_port with stand-ins runs `zyperos.elf --port-played` as the program ends, before
#| the launcher comes back, and with POWER over it before the power-off; then the real launcher on
#| a card with your PICO-8 (stand-in files), a cart, OpenBOR, a port, PortMaster and a SNES game
#| played yesterday: A on the cart writes the request, the note (`/tmp/zyperos_port_played`) and
#| the cart's row open at 0 s, `--port-played` closes it as a quit (1h 2m, the note's clock moved
#| back) and removes the note, a second finds nothing; the cart first on Recents with "PICO-8 ·
#| today HH:MM" (its start, the card's zone, 24 and 12 hours) and "Played 1h 2m" in Covers, Grid,
#| List and Video, both themes, nothing cut; the SNES game's "SNES · yesterday 21:40"; the cart's
#| Played on the PICO-8 shelf, with no time there; a launcher that starts with the note still there
#| closes it as a quit before Recents is built; OpenBOR's module and a port on Recents the same
#| way, Splore with its own system's line, PortMaster itself no note and no row; Recents' Games not
#| counting Splore. Shots go to build-native/recents-played-shots/ (builds the host launcher)
