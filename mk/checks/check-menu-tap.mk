CHECKS += check-menu-tap
#| Diatom's own keys pressed quickly in a slow game (diatom/test/menutap.py), the stub core at 10
#| fps and the pad by script, `~` a press that went down and up between two polls: MENU tapped
#| opens the menu once and MENU held once; SELECT held and R1 tapped says the save hotkey once,
#| held together once; R1 or SELECT tapped alone is no hotkey and the game never reads it; the
#| stick's key tapped turns the stick each time; at 2x, where a turn runs four frames, a tap at any
#| of them counts. Diatom's desktop port built into diatom/build/menutap, apart from check-diatom's
#| (skips without SDL2)
