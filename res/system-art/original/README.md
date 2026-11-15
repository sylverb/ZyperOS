# Original ZyperOS system art

The set contains 60 designs: 49 systems and shelves, PortMaster, two generic folders, and eight card folders.
Two of them, Recents and the SG-1000, are placeholders drawn by scripts (see Placeholders).
Every animated design has a 256 × 256 atlas and a separate still image.
The folders also have standalone PNGs for their actual display sizes.

## View

Open `gallery.html` through a local HTTP server to view the exported animations.
The gallery includes all 60 designs, pause, and a dark background.
It starts paused when the system requests reduced motion.

```sh
python3 -m http.server 8881 --bind 127.0.0.1 --directory res/system-art/original
```

Then open `http://127.0.0.1:8881/gallery.html`.
`index.html` runs the source animation renderer for editing and export.
`source-review.html` shows the generated source illustrations.
`utility-review.html` shows the PortMaster covers and the folders at list size.
`card-folders-review.html` shows the eight card folders at 256, 56, and 40 pixels on light and dark backgrounds.

## Artwork and motion

The built-in image tool generated the hardware illustrations.
The selected source files contain no pixels from another artist's work or from a photograph.
The source images remain unchanged in `source/`.
`manifest.json` identifies selected sources and superseded drafts.

The GBA startup uses original wordmark geometry and a measured 3900 ms sequence.
Other handhelds show their own startup where one exists.
Game Gear, Lynx, and Java ME have documented startup exceptions.
The selected hardware uses its real display or power behavior in those cases.
The symbolic icons use two distinct object poses, with 400 ms per pose.
The heart beats, the Arcade button presses, and MP4 moves its music note beside a video symbol.
The old moving reflection sweeps are removed.

Home consoles now load their real cartridges or discs.
The added mechanisms include the NES front door and the Sega CD flap and tray.
Virtual Boy uses its underside slot. MSX2 opens its cartridge cover.
The AES cartridge fully clears its slot before insertion.
The PlayStation lid follows its fixed rear hinge and projected top plane.
The 3DO tray moves along the case depth direction without changing shape.
The SNES cartridge enters behind the slot edge.
Cases, controllers, and cables remain fixed.

The renderer separates raster size from physical screen shape.
For example, Game Gear retains its 160 × 144 raster on a 4:3 display.
The other displays keep their verified proportions.
Each animated layer stays inside its declared region.

## Sources and records

- `prompts.json` records the first batch and its corrections.
- `groups/consoles.json` records 16 home consoles.
- `groups/handhelds.json` records 12 handhelds and the GBA startup work.
- `groups/arcade.json` records 14 arcade, computer, software, and shelf images.
- `groups/console-lids.json` records five disc and front-loading mechanisms.
- `groups/console-cartridges.json` records 12 cartridge-loading mechanisms.
- `groups/utility.json` records PortMaster and both folder designs.
- `groups/card-folders.json` records the eight static card folders, full prompts, source hashes, and size reviews.
- `manifest.json` records the selected PNGs and source hashes.
- `animations/catalogue.json` records every exported atlas, still, frame duration, and source hash.

Each group record includes full prompts, hardware references, corrections, and perspective checks.
Startup marks, hardware names, and hardware designs remain associated with their respective makers.
The new hardware illustrations do not make those marks original ZyperOS work.
The startup sequences are visual reconstructions.
No boot ROM, captured video, or sound ships in this set.

## Export and checks

`export.js` samples each reviewed animation into at most 64 unique poses.
The output uses transparent PNG atlases and explicit frame durations.
Identical frames share one tile.
The source images and animation code have separate hashes.

Run `ZyperArtExporter.batch(keys)` on `index.html` through the browser tool.
Save its JSON result, then run:

```sh
python3 tools/systemart-original-pack.py /path/to/export.json
/usr/bin/python3 tools/systemart-original-check.py
/usr/bin/python3 tools/systemart-symbol-pixels-check.py
```

Run `check.js` through the Playwright `browser_run_code` tool against `index.html`.
The browser check covers every system, fixed case pixels, moving regions, clipping, loop return, controls, narrow layout, and reduced motion.
The PNG check covers full catalogue coverage, every unique pose, alpha, source hashes, renderer hashes, dimensions, and frame budgets.

The PNG check covers all 60 designs.
The independent visual review has no remaining material findings. It did not include the placeholders.
The review corrected controller colors, cable plugs, screen proportions, and the TurboGrafx-16 regional model.
PICO-8 and TIC-80 use isolated symbols and whole-pixel motion.
Their pixel regression check protects nearby lettering and the symbols' colors.

## Placeholders

A placeholder holds a shelf's place until it has a design made like the others. There are two.
Recents (`RECENTS`), the home's shelf of the games played last, is one.
`placeholders/recents.py` draws it with Python and Pillow on a 32-cell grid, each cell 8 pixels, from simple shapes.
It uses no image generation, no other artist's work, and no photograph.
It is a clock with ZyperOS's lime rim; its two poses, 400 ms each as Favorites' heart, move the minute hand from twelve to one.
The catalogue records the script as its input and renderer, with its hash, and marks the design `placeholder`.
Change the script, and run it into `animations/` again before the PNG check: the check compares the script's hash.

The SG-1000 (`SG1000`) is the other.
`placeholders/sg1000.py` draws it with Python and Pillow, pixel by pixel on a 128-pixel grid, from simple shapes.
It uses no image generation, no other artist's work, and no photograph.
The drawing shows a dark angular console with a red stripe, a cartridge in its top slot, and its joystick controller.
It has three poses in a 2200 ms loop: at rest, then the joystick leans left with button 1 pressed and right with button 2 pressed.
The console's colors are the same blue-grey as the other consoles, so it shows on the dark theme too.

Run the script to draw it again:

```sh
/usr/bin/python3 res/system-art/original/placeholders/sg1000.py
/usr/bin/python3 tools/systemart.py
```

The script writes `source/SG1000.png`, the three poses at 128 pixels, and the 256-pixel atlas and still, with every pixel doubled.
It also writes `animations/SG1000.json` and adds the design to `animations/catalogue.json`, `manifest.json`, and `source-index.json`.
The catalogue records the script as the renderer, with its hash, and marks the design `placeholder`.
`manifest.json` lists it under `placeholders`.
Change the script, and run it again before the PNG check: the check compares the script's hash.

## Utility files

`utility/portmaster-icon.png` is the transparent PortMaster icon.
The two `portmaster-cover-*.png` files are 512 × 656 covers for the light and dark themes.
PortMaster also has a two-frame crane animation in `animations/`.

`utility/folder-large.png` is the 256-pixel folder with a blank count label.
The small folder uses its own simple drawing.
`utility/folder-small-24.png`, `-32.png`, `-40.png`, `-48.png`, and `-64.png` support list rows.
The game count stays outside the small icon.
`utility/manifest.json` records dimensions, hashes, and the large label position.

## Card folders

The card folders use one static pose each. Their generic objects contain no brands or label text.

| Key | Folder | Artwork |
| --- | --- | --- |
| `ROMS` | ROMs | Purple game cartridge with an original label illustration |
| `MUSIC` | Music | Vinyl record with album sleeve |
| `BOOKS` | Audiobooks | Book with headphones |
| `VIDEOS` | Videos | Video tape |
| `BIOS` | BIOS | Chip |
| `SAVES` | Saves | Memory card |
| `STATES` | Save States | Floppy disk |
| `BGM` | Menu music (`Roms/BGM`) | Audio cassette |

`animations/<KEY>-still.png` contains each 256-pixel still.
`review/card-folders/` contains the 56- and 40-pixel browser exports and their hashes.
The review checks subject recognition, perspective, transparent margins, and visible glow on both backgrounds.
`all-light.png` and `all-dark.png` show all eight designs together at the three review sizes.
The Music folder uses `MUSIC`. MP4's shelf and player keep `MUSE`. Any other folder keeps the small folder (`FOLDER_SMALL`).
The File Transfer page and the Storage page show each card folder's still under its key.

## Runtime

This is the launcher's system art. `tools/systemart.py` compiles it into `res/system-icons` and `src/systemart_data.h`.
Each design keeps its 256-pixel first pose whole, with the moving region of every pose packed under it.
The launcher writes the region of the pose due into the first pose's texture.
This keeps each decoded atlas under 4 MiB; the largest is 2.1 MiB.
The 128-pixel stills serve the home's row, the Storage page and the File Transfer page.
The small folder is the launcher's `FOLDER`: a folder of no system.
The eight card folders keep their keys.
PortMaster's covers and icon are copied to `res/portmaster`.
The payload carries only the compiled files, not this source and review directory.
