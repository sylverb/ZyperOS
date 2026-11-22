CHECKS += check-shotfeel
#| a screenshot felt and heard as it is taken (DESIGN.md 9, 11.61). Over your PICO-8, portkeys's
#| shutter player (tools/pkshutter.c) on this computer's alsa-lib, into stand-ins for the system's
#| mix and the DAC's that write down what reaches them (alsa-lib's file PCM over its null one),
#| PICO-8's own stream playing beside it (tools/shotfeel-alsa.c): the shutter starts and the shot
#| goes on at once, a second shot while it sounds plays none, the mix gets the shutter's file
#| sample for sample to its end on both sides, Mono Left on the DAC the left side alone and on the
#| DAC's device only, a device that will not open plays nothing and the next shot plays again, and
#| PICO-8's stream has every frame of its tone as written (Linux with alsa-lib's headers; skipped
#| elsewhere). In a Diatom game, diatom/test/shotfeel.py against the stub core showing a frame of
#| its own (when diatom/build/desktop is there; check-diatom builds it and runs it too): two shots,
#| two buzzes at RUN rumble='s level (65% and Full), each on 40 ms and off a frame or so after,
#| none at 0; each shot says HOTKEY key=shot as it is taken and before its SHOT path=, at every
#| level; each file the core's frame pixel for pixel; and the game's sound written all through,
#| every write the stub's tone at its own level. What a shot asks for over PICO-8 from the
#| launcher's settings is `check-pkkeys`'s, the launcher's shutter `check-uisound`'s and
#| `check-resident`'s
