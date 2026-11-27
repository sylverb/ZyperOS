CHECKS += check-mp4quit
#| MP4's Quit ends MP4 (DESIGN.md 11.14): the real launcher in desk mode's dummy window on the
#| README's media card against a stand-in daemon (`ZYPEROS_MP4_SOCKET`, check-pico8-mp4's, which
#| answers STOP here): a song played from the shelf, then paused and MENU > X, or playing and MENU >
#| Quit with A: STOP sent, the home back with nothing in its header right of the clock (no pause
#| mark, no note, no track), the power-off row (`mp4.resume`) gone, and SELECT opening MP4 on its
#| shelf again rather than a Now Playing left loaded; SELECT let go instead leaves the song heard
#| behind the home with its mark in the header and no STOP, as ever. Every wait is for an event (a
#| screen's foot drawn, a string drawn, the daemon sent the key's command, a menu row under the
#| cursor), never a time

# Pixel 2, 1.1.1-test.3: play, pause, MENU > Quit, and MP4's paused song
# still in the home's top bar with MP4 closed
check-mp4quit: native
	@python3 tools/mp4quit-check.py
