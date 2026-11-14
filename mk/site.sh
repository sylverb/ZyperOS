#!/bin/sh
# Lay out ZyperOS's web page, site/index.html and its 3D Pixel 2 (pixel2.js),
# and its blog (site/blog/, made into pages by mk/blog.py), beside the
# pictures they show, ready to serve. Cloudflare Pages runs it on
# every change to main that touches the page or its pictures (build command
# `sh mk/site.sh _site`, output `_site`) and serves the result at
# https://zyperos.com; run it by hand to look at the page first:
#
#   sh mk/site.sh [OUT]        OUT defaults to build/site
#   npx http-server build/site -a 127.0.0.1 -c-1
# Use a server with HTTP Range support to seek through the music previews.
#
# The pictures stay where the README keeps them: the lockups and Zyper that
# tools/genzyper.py writes, and the screenshots. One copy of each in the
# repository, so the page and the README never show different ones.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/site}
rm -rf "$OUT"
mkdir -p "$OUT/img/dark" "$OUT/models" "$OUT/art"
mkdir -p "$OUT/legal"
for name in LICENSE NOTICE TRADEMARK.md THIRD-PARTY-LICENSES.md; do
	cp "$ROOT/$name" "$OUT/legal/${name%.md}.txt"
done
cp -R "$ROOT/site/music" "$OUT/music"
cp -R "$ROOT/site/scenes" "$OUT/scenes"
cp -R "$ROOT/site/tip" "$OUT/tip"
cp -R "$ROOT/site/download" "$OUT/download"
# Every file at the top of site/: the page, its script, the 404 page, the
# icons and social card tools/site-art.sh draws, robots.txt, llms.txt, the
# web manifest, the IndexNow key, Cloudflare's _headers, and the blog's
# style sheet and script (site/blog/, the posts, is a folder: mk/blog.py
# reads it)
for f in "$ROOT"/site/*; do
	if [ -f "$f" ]; then cp "$f" "$OUT/"; fi
done
cp "$ROOT/site/models/pixel2.glb" "$ROOT/site/models/pixel2-poster.png" \
   "$ROOT/site/models/pixel2-music.png" "$OUT/models/"
cp "$ROOT/res/web/icon.svg" "$ROOT/res/web/zyper.svg" "$OUT/img/"
# Every screenshot in the light theme; dark/ only for the light-and-dark section
for f in lockup lockup-light systems gallery shelf shelf-n64 shelf-snes favorites info cheevos cheevo-notice menu scraper \
         mp4 storage nowplaying portal portmaster battery gamemenu-shader controls-hotkeys media folders list es-shelf info-rename display theme \
         shaders screensaver play-phone-side player2-hotspot hare charging heat tour-language tour-home \
         rename usb-ask update ports-controls cheevos-waiting topbar storage-system storage-music eric-reinsmidt; do
	cp "$ROOT/res/readme/$f.png" "$OUT/img/"
done
for f in shelf-n64 systems menu; do
	cp "$ROOT/res/readme/dark/$f.png" "$OUT/img/dark/"
done
# Reuse the original art and its exact frame timings. Keep the full source
# catalogue and unselected atlases out of the deployed site.
python3 - "$ROOT" "$OUT" <<'PY'
import json
import hashlib
import re
import shutil
import sys
from pathlib import Path
root, out = map(Path, sys.argv[1:])
source = root / 'res/system-art/original/animations'
keys = set(re.findall(r'data-art="([A-Z0-9]+)"', (root / 'site/index.html').read_text()))
fields = ('key', 'atlas', 'cols', 'duration_ms', 'frames')
assets = [a for a in json.loads((source / 'catalogue.json').read_text())['assets'] if a['key'] in keys]
assert {a['key'] for a in assets} == keys
for asset in assets:
    for name in (asset['atlas'], asset['still']):
        shutil.copyfile(source / name, out / 'art' / name)
for name in ('MUSE-still.png', 'FAVORITES-still.png'):
    shutil.copyfile(source / name, out / 'art' / name)
(out / 'art/catalogue.json').write_text(json.dumps([{k: a[k] for k in fields} for a in assets], separators=(',', ':')) + '\n')
# Returning visitors must receive the device code with the motion control,
# even while the CDN/browser still caches the previous unversioned module.
def version(path):
    return hashlib.sha256((out / path).read_bytes()).hexdigest()[:12]
device_script = out / 'pixel2.js'
device_script.write_text(device_script.read_text().replace('"./models/pixel2.glb"',
                         '"./models/pixel2.glb?v=' + version('models/pixel2.glb') + '"'))
for filename in ('landing.js', 'player-two.js'):
    script = out / filename
    script.write_text(script.read_text().replace("import('./pixel2.js')", "import('./pixel2.js?v=" + version('pixel2.js') + "')"))
page = out / 'index.html'
page.write_text(re.sub(r'(?:landing\.(?:css|js)|features\.(?:css|js)|music\.js|player-two\.(?:css|js)|models/pixel2-(?:poster|music)\.png|img/[\w/-]+\.png)(?=")',
                       lambda match: match[0] + '?v=' + version(match[0]), page.read_text()))
tip = out / 'tip/index.html'
tip_text = tip.read_text().replace('href="/blog.css"',
    'href="/blog.css?v=' + hashlib.sha1((out / 'blog.css').read_bytes()).hexdigest()[:8] + '"')
tip.write_text(re.sub(r'/(?:tip\.(?:css|js)|models/pixel2-poster\.png)(?=")',
                     lambda match: match[0] + '?v=' + version(match[0][1:]), tip_text))
download = out / 'download/index.html'
download.write_text(re.sub(r'/(?:download\.css|downloads\.js|models/pixel2-poster\.png)(?=")',
                          lambda match: match[0] + '?v=' + version(match[0][1:]), download.read_text()))
PY
# Zyper's photo, in The name
cp "$ROOT/res/readme/zyper-photo.jpg" "$OUT/img/"
# The blog (mk/blog.py, python3's standard library): the posts in site/blog/
# as pages under /blog/, its index and RSS feed, the sitemap, and a Blog
# section at the end of llms.txt. The landing page's sitemap line is dated by
# its last commit (a shallow clone may only know the newest commit; today's
# date if git knows none); each post by its own updated date.
day=$(git -C "$ROOT" log -1 --format=%cs -- site/index.html 2>/dev/null || true)
[ -n "$day" ] || day=$(date -u +%Y-%m-%d)
python3 "$ROOT/mk/blog.py" --out "$OUT" --home-lastmod "$day"
# One shared, static header on the landing page, standalone pages and blog.
python3 "$ROOT/mk/site_header.py" --out "$OUT"
echo "site: $OUT"
