CHECKS += check-port-menu
#| the in-game menu over a program of its own, OpenBOR, a PortMaster port (tools/fixtures/ports'
#| Star Farm) and PortMaster itself, a game's tabs since 1.1.0-rc.8 (src/game_menu.h's
#| `gm_tab_rows`, `gm_ctltab`): the real launcher in desk mode's dummy window as portkeys starts it
#| (`--port-menu TAG GAME`), as the host's own profile and as the Pixel 2 (`--device` after the
#| game): the Game tab first, Continue and Quit; R1 the Display tab, Screen Colors alone where the
#| display has a table and nothing as the host, nothing of the program's own picture; R1 the
#| Controls tab, All Controls alone, A opening the program's read-only pages (their heading and
#| page count drawn), B back on the row; X from the Controls tab Quit (11); the next menu opening
#| where the last was left (`/tmp/zyperos-port-menu-mem`), B Continue (10); OpenBOR still starting
#| (`--waiting`) Keep Waiting and Quit; every wait a wait for an event (tools/deskwalk.py)
