#!/usr/bin/env python3
"""Make the landing page's short previews from the already licensed menu music.

Run with python3 tools/site-music.py. Requires ffmpeg only when regenerating.
The static site build copies the checked-in previews without audio tooling.
"""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'res/bgm'
OUT = ROOT / 'site/music'
IDS = ('starfall-island', 'napping-on-a-cloud', 'bossa-nova')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    tracks = json.loads((SOURCE / 'tracks.json').read_text())['tracks']
    records = []
    for key in IDS:
        track = next(t for t in tracks if t['id'] == key)
        source = SOURCE / track['file']
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        assert digest == track['mp3']['sha256'], f'Source changed: {source}'
        assert track['license'] == 'CC0-1.0'
        output = OUT / (key + '.mp3')
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
                        '-i', str(source), '-t', '30', '-af',
                        'afade=t=in:d=0.08,afade=t=out:st=28:d=2',
                        '-c:a', 'libmp3lame', '-b:a', '128k', '-map_metadata', '-1',
                        '-metadata', 'title=' + track['title'],
                        '-metadata', 'artist=' + track['author'], str(output)], check=True)
        records.append({'id': key, 'title': track['title'], 'author': track['author'],
                        'license': track['license'], 'page': track['page'],
                        'source': str(source.relative_to(ROOT)), 'source_sha256': digest,
                        'excerpt_seconds': 30, 'fade_out_seconds': 2,
                        'sha256': hashlib.sha256(output.read_bytes()).hexdigest()})
    (OUT / 'credits.json').write_text(json.dumps(records, indent=2) + '\n')
    (OUT / 'LICENSE-CC0-1.0.txt').write_bytes((SOURCE / 'LICENSE-CC0-1.0.txt').read_bytes())
    print(f'Wrote {len(records)} previews to {OUT}')


if __name__ == '__main__':
    main()
