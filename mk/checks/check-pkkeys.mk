CHECKS += check-pkkeys
#| the volume keys over a port: MENU+VOL is the backlight whichever device is read first, the
#| kernel's autorepeat is no step, a hold repeats on the launcher's clock, and the levels saved are
#| the ones pressed; the boost past the top (src/boost.h through tools/pkkeys.c), and its buzz as
#| it opens (`pk_buzz_pct`): MENU > Sound > Rumble's share as the launcher writes and reads the
#| word (src/rumble.c through src/db.c, a word it cannot read Full), once for each opening of
#| either boost by presses or a hold, never for a step, a cell or a fold, none at Off, 40 ms as the
#| launcher's and Diatom's, at the launcher's strength; a boost kept from before shows nothing at
#| the start (no step, no opening, no buzz, nothing moving); whose display may be borrowed
#| (`pk_looks`): a display file only once settled, seen open at a look 300 ms before with no gap
#| over 1.5 s, or a process whose master was held, and on a timeline of SDL's two looks at the
#| display as a program starts (the 1.1.0-rc.9 "kmsdrm not available"), at no start phase a copy
#| inside them, the program's own file copied within a look and 300 ms; and over your PICO-8,
#| SELECT's hotkeys by Diatom's own chord rule (tools/pkhot.c: SELECT then the button, the button
#| just before SELECT, SELECT held long then the button; not after the d-pad, not Quick Save, not
#| the kernel's repeat, not a SELECT a resync found down) and CTRL-R as portkeys writes it to its
#| uinput keyboard (tools/pkkbd.c), into a pipe; and the screenshot (tools/pkshot.c): PICO-8's own
#| 128 x 128 out of a turned and a landscape panel in every Display mode's rect (src/p8draw.h), the
#| cut rows of Zoomed and Edges Cut black, the PNG's chunks and pixels back through zlib, and the
#| shot laid in the pop-up's square with its corners rounded; what a shot asks for (`pk_shot_feel`,
#| tools/pkfeel.c), from the launcher's words through src/db.c for every Rumble, UI Sounds and
#| Audio Channels word, on the speaker, headphones and a DAC: the buzz at Rumble's share (none at
#| Off), the shutter in UI Sounds' voice (none at Off, Wood on a card that never chose), on the
#| device and the sides the launcher's own sounds would have (`uisound_device`, `uisound_sides`:
#| "usb" with a DAC, Mono Left on the speaker whole); what PICO-8 reads while portkeys holds the
#| joypad for the hotkeys (`pkhot_out`): every press at once, MENU never, a chord never (the button
#| just before SELECT taken back), SELECT alone given back as it is let go for two turns at least
#| and taken back after, the game's with the d-pad or held past 400 ms; and that pad's writes
#| (tools/pkvpad.c), one event a button that changed and a report, into a pipe
