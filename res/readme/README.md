# Launcher screenshots

`eric-reinsmidt.png` is a supplied portrait for the site credits, not a launcher
capture. It retains the supplied image without edits.

The screenshots in this folder show the current launcher in Beard, the light
theme: the README and the website show every screen in light. `dark/` holds
the screens the guide's and the website's light-and-dark sections show in Salt & Pepper, the
Nintendo 64 shelf, the home, the ZyperOS menu and a game's Cover and Video
page, and the top bar's shot (`topbar`, the song's note and the
headphone mark) in both themes. The 3D device on the website shows the light ones too.
The logo/wordmark files remain the branding generator's output.

These are 1024x768 frames rendered by the native ZyperOS launcher, not edited
mockups or photographs of a running device. `brick/` holds the
TrimUI Brick's, by the Pixel 2 shots' names (`--device brick`, at its own
1024x768 screen with its keys and its drawing): `tools/readme-shots.py` with its
`DEVICE` set to `brick`. The web page's device switch shows them in place of the
Pixel 2's. The `desk-` shots are desk mode's own window (docs/building.md,
"Work on the UI without a device"): the launcher drawn as the GKD Pixel 2 or
the TrimUI Brick, on the sample card `tools/desk` makes, taken with
`tools/desk shot FILE window` on a Retina display. The game in them is
`ZyperOS Test.nes`, ZyperOS's own NES test cartridge, and `desk-card` is the
top of `desk-pixel2`, cut to 4:3 for the blog's index. The three `portal` shots are the
File Transfer page (res/web) in headless Chrome, at a computer's 1280x800 and
a phone's 390x844, served by the real routes over a scratch card of freeware
names (build-native/portal-serve). The two `play-phone` shots are NES Player
2's play page (res/web/play) in headless Chrome, on a phone held upright and
on its side (390x844 and 844x390 at 3x), served by the host's p2d over a
share that p2d-feed fills with its own drawn frames: stripes and a box, no
game's picture. The source commit, command flags
and output hashes are recorded in `captures.json`, with the art each one
shows.

The capture uses temporary demonstration cards: empty game files named after
well-known games, empty music files with stand-in names, a card with no games
for the empty home, and curated Metal Gear Solid details (the synopsis is in
my words). The shots show the games' real box art, as the launcher's own
Scraper would put it on the card: covers, title screens and screenshots from
the [libretro-thumbnails](https://github.com/libretro-thumbnails) sets,
fetched once into a cache on the computer that takes the shots, outside this
repository. The Cover and Video shots (`media`, `media-look`, `media-video`)
show Metal Gear Solid's boxes, title screen and screenshot in the places the
page's fixture names; the in-game menu and Controls shots stand over a
Spyro the Dragon screenshot; the video snaps behind the play marks are two
seconds of each game's libretro screenshot. `captures.json` names every
picture used, its set and its checksum. The games and their art belong to
their owners: ZyperOS ships no games and no art, and only these composed
screenshots are in the repository.

The home screens show ZyperOS's own system art (res/system-art/original,
compiled into res/system-icons). MP4 shows clearly
named demonstration albums, a film named after Blender's freely licensed
Sintel and the public-domain Treasure Island as a book, all of them empty
files. The ScummVM shelf shows the freeware games ScummVM's own site gives
away, with their libretro covers. The made-up port behind the Ports menu and
Controls shots (tools/fixtures/ports) has no art. The games pages' hearts
mark demonstration favorites, set for those shots alone.
Achievement rows and badges, playback timing, transfer progress, the hotspot,
the Player 2 page,
the Scraper's counts, the Storage page and the System Update page use the
launcher's built-in shot fixtures and stand-in values. No ROMs, audio,
account credentials or personal save data are included or required. The
clock and battery are fixed preview values.

## Refresh

Check each image's `source_commit` in `captures.json` before capture. The feature captures
come from main at `71c6122259150af55603b3a45d326ca486cc693c`, using that
revision's `tools/readme-shots.py` and native binary. Scraper uses
`--scraper-page 1`; `--scraper 2` is the old panel. An older checkout can
produce outdated captures even after a fresh native build.

Run the commands below in a checkout that includes the recorded source
revision. For a partial refresh, merge only the refreshed image entries
into `captures.json`, including each image's source commit. Keep the other
entries and their provenance.

```sh
make native
python3 tools/readme-shots.py --art
```

Review `build/readme-shots/light/` and `dark/`. Copy the approved PNGs to this
folder and `dark/`, and merge their `manifest.json` entries into `captures.json`. Names after
the command take only those shots (`python3 tools/readme-shots.py menu`).
`--art` fetches what the cache lacks from raw.githubusercontent.com the first
time (ffmpeg makes the video snaps) and nothing after; `--art-cache` moves
the cache. Without `--art` every game wears the launcher's generated card. The page shots need Google Chrome (`--chrome` names another
Chromium, `--no-page` leaves them out), and the pages' servers listen on
ports the kernel picks while they are taken. Apart from that first fetch of the art, the launcher and the page make no
requests outside this computer for the captures. A browser's frames are not quite
repeatable: a shadow can differ by a few pixels from run to run, and the
upload's percentage by its timing.

Then rebuild the device poster and the static site:

```sh
blender --background --python tools/build-pixel2-model.py -- --render
sh mk/site.sh
```

`shaders.png` is not a launcher frame: it is Balloon Fight's picture as Diatom
draws it on the Pixel 2's 640x480 screen with each Shader, a part of it at
twice its size, side by side. The game's frame is the one Diatom saved when
the game was paused in ZyperOS's QEMU machine (docs/qemu.md), and each look
is the shader in `diatom/port/pixel2.c` drawing it there, through Mesa, as
the Pixel 2 draws it with stretch and the panel's color matrix. The game and
its picture belong to their owner.
