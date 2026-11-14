# Landing page music previews

The MP3 files are 30-second excerpts from three included menu tracks in
`res/bgm`. `credits.json` records the artists, original source pages, CC0
license, and source/output hashes. Generate them with:

```sh
python3 tools/site-music.py
```

The SVG sleeves are original illustrations made for the landing page.
They are presentation artwork, not the artists' official album covers.
The page names these as menu music previews. Audio starts only after a
visitor presses Play. Changing tracks during playback continues playback;
selecting a cover while paused leaves audio paused.

The scene uses the accurate Pixel 2 model with `now-playing.png`, a frame
from the native launcher. `capture.json` records its source and settings.
The demonstration card contains the included Starfall Island track and
the original preview sleeve. The native screenshot fixture supplies timing.
`ARTWORK.md` records the generated transparent CD and earbuds scene.
