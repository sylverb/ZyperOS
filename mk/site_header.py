#!/usr/bin/env python3
"""Render the shared header in every HTML page after the site build."""

import argparse
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
MARKER = re.compile(r'<!-- site-header active="([A-Za-z]*)" current="(page|true|location)" -->')


def render(out):
    template = (ROOT / 'site/_includes/header.html').read_text(encoding='utf-8')
    labels = set(re.findall(r'data-site-link="([^"]+)"', template))
    versions = {kind: hashlib.sha256((out / ('header.' + kind)).read_bytes()).hexdigest()[:12]
                for kind in ('css', 'js')}
    assets = ('<link rel="stylesheet" href="/header.css?v=%s">\n'
              '<script src="/header.js?v=%s" defer></script>') % (versions['css'], versions['js'])
    pages = sorted(out.rglob('*.html'))
    for page in pages:
        text = page.read_text(encoding='utf-8')
        markers = list(MARKER.finditer(text))
        if len(markers) != 1 or text.count('</head>') != 1:
            raise ValueError('%s needs one site-header marker and one head' % page.relative_to(out))
        active, current = markers[0].groups()
        if active and active not in labels:
            raise ValueError('%s: unknown active header link %s' % (page.relative_to(out), active))
        header = template
        if active:
            header = header.replace('data-site-link="%s"' % active,
                                    'data-site-link="%s" aria-current="%s"' % (active, current))
        text = MARKER.sub(lambda match: header, text)
        page.write_text(text.replace('</head>', assets + '\n</head>'), encoding='utf-8')
    print('header: %d pages' % len(pages))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    render(parser.parse_args().out)
