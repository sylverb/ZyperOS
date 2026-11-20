#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-keymap: ONE keyboard map for the handheld's buttons everywhere a
computer's keyboard stands in for them. The tables, read from their sources:

  src/device/keymap.h               desk mode and the host build (SDL scancodes)
  simulator/.../App.swift Keyboard  the Simulator (macOS key codes)
  diatom/port/desktop.c keymap[]    Diatom's own window (SDL scancodes)

Each key the Simulator or Diatom has must be the same button in keymap.h, and
keymap.h may have more only where a device has buttons the others' Pixel 2
has not (the Brick's brightness keys, [ and ], which the Simulator has too,
for a handheld that has them). The docs print the map:
docs/building.md and docs/simulator.md each name every one of keymap.h's
keys' buttons."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# macOS virtual key codes (Carbon's kVK_*) for the keys the Simulator maps, as
# SDL scancode names
MAC = {126: 'UP', 125: 'DOWN', 123: 'LEFT', 124: 'RIGHT', 7: 'X', 6: 'Z', 1: 'S', 0: 'A',
       12: 'Q', 13: 'W', 14: 'E', 15: 'R', 36: 'RETURN', 76: 'KP_ENTER', 51: 'BACKSPACE',
       53: 'ESCAPE', 46: 'M', 24: 'EQUALS', 27: 'MINUS', 35: 'P', 60: 'RSHIFT',
       33: 'LEFTBRACKET', 30: 'RIGHTBRACKET'}
IN = {'IN_UP': 'up', 'IN_DOWN': 'down', 'IN_LEFT': 'left', 'IN_RIGHT': 'right', 'IN_ACCEPT': 'a',
      'IN_BACK': 'b', 'IN_X': 'x', 'IN_Y': 'y', 'IN_L1': 'l1', 'IN_R1': 'r1', 'IN_L2': 'l2',
      'IN_R2': 'r2', 'IN_START': 'start', 'IN_SELECT': 'select', 'IN_MENU': 'menu',
      'IN_VOLUP': 'volup', 'IN_VOLDN': 'voldown', 'IN_POWER': 'power',
      'IN_BRIGHTUP': 'brightup', 'IN_BRIGHTDN': 'brightdown'}
DIATOM = {'DIATOM_BTN_UP': 'up', 'DIATOM_BTN_DOWN': 'down', 'DIATOM_BTN_LEFT': 'left',
          'DIATOM_BTN_RIGHT': 'right', 'DIATOM_BTN_A': 'a', 'DIATOM_BTN_B': 'b', 'DIATOM_BTN_X': 'x',
          'DIATOM_BTN_Y': 'y', 'DIATOM_BTN_L1': 'l1', 'DIATOM_BTN_R1': 'r1', 'DIATOM_BTN_L2': 'l2',
          'DIATOM_BTN_R2': 'r2', 'DIATOM_BTN_SELECT': 'select', 'DIATOM_BTN_START': 'start',
          'DIATOM_BTN_MENU': 'menu'}
# keymap.h's keys that only a device with such buttons uses
EXTRA = {'LEFTBRACKET': 'brightdown', 'RIGHTBRACKET': 'brightup'}

fails = []


def keymap_h():
    text = (ROOT / 'src/device/keymap.h').read_text()
    body = text[text.index('keymap_button'):text.index('keymap_hint')]
    out, pending = {}, []
    for line in body.splitlines():
        pending += re.findall(r'case SDL_SCANCODE_(\w+):', line)
        m = re.search(r'return (IN_\w+);', line)
        if m and pending:
            for k in pending:
                out[k] = IN.get(m.group(1), m.group(1))
            pending = []
    return out


def simulator():
    text = (ROOT / 'simulator/Sources/ZyperSimulator/App.swift').read_text()
    block = re.search(r'private static let map: \[UInt16: PadButton\] = \[(.*?)\]', text, re.S).group(1)
    out = {MAC[int(c)]: b for c, b in re.findall(r'(\d+): \.(\w+)', block)}
    # Right Shift is SELECT there too, by its flags (handle())
    if re.search(r'rightShift.*sim\.press\(\.select\)', text, re.S):
        out['RSHIFT'] = 'select'
    return out


def diatom():
    text = (ROOT / 'diatom/port/desktop.c').read_text()
    block = text[text.index('keymap[] = {'):]
    block = block[:block.index('};')]
    return {k: DIATOM[b] for k, b in re.findall(r'SDL_SCANCODE_(\w+),\s*(DIATOM_BTN_\w+)', block)}


def main():
    ours, sim, dia = keymap_h(), simulator(), diatom()
    if len(ours) < 20:
        fails.append(f'src/device/keymap.h: only {len(ours)} keys read')
    for name, table in (('the Simulator', sim), ("Diatom's window", dia)):
        for key, button in sorted(table.items()):
            if ours.get(key) != button:
                fails.append(f'{key}: {button} in {name}, {ours.get(key, "nothing")} in src/device/keymap.h')
    for key, button in sorted(ours.items()):
        if key not in sim and EXTRA.get(key) != button:
            fails.append(f'{key}: {button} in src/device/keymap.h, nothing in the Simulator')
    for doc in ('docs/building.md', 'docs/simulator.md'):
        text = (ROOT / doc).read_text()
        if 'src/device/keymap.h' not in text:
            fails.append(f'{doc} does not point at src/device/keymap.h, the one key map')
    for f in fails:
        print(f'  FAIL {f}')
    print(f'keymap: {len(ours)} keys in src/device/keymap.h, {len(sim)} in the Simulator, '
          f'{len(dia)} in Diatom\'s window{": the same buttons" if not fails else ""}')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
