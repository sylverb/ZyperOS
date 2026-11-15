#!/usr/bin/env python3
"""The SG-1000's placeholder art, drawn by this script; Python 3.9+ and Pillow 9.1+.

A placeholder until the SG-1000 has a design made the way the others were
(../README.md): ZyperOS's own drawing, pixel by pixel on a 128-pixel grid,
from nothing but simple shapes. No pixel of another artist's work or of a
photograph went into it. A dark, angular console with a red stripe along its
front and a cartridge standing in the slot on its top, and its joystick
controller in front of it, wired to it, the way the other consoles stand.
Three poses: at rest, then the joystick rocks left with button 1 pressed and
right with button 2 pressed, and rests again.

    python3 res/system-art/original/placeholders/sg1000.py
    python3 res/system-art/original/placeholders/sg1000.py --preview out.png

The first writes, under res/system-art/original:

  source/SG1000.png             the drawing, its three poses side by side,
                                128 square each
  animations/SG1000.png         the poses at the 256-pixel cell, every pixel
  animations/SG1000-still.png   doubled: its atlas, and the rest pose its still
  animations/SG1000.json        its frame table, as the others' are
  animations/catalogue.json     every design again, with SG1000's
  manifest.json, source-index.json  the source recorded beside the others'

and tools/systemart.py then compiles it for the launcher. --preview writes
only a 4x enlargement to look at.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

HERE = Path(__file__).resolve().parent
ART = HERE.parent
KEY, NAME = "SG1000", "SG-1000"
GRID, CELL = 128, 256
# The loop: each pose and how long it shows, rest first (the still)
# pose: (stick's lean in grid pixels, the button held down or None)
POSES = [(0, None), (-3, 1), (3, 2)]
FRAMES = [(0, 1400), (1, 300), (0, 200), (2, 300)]

# The palette: the console's dark plastic in three faces, a blue-grey as the
# other consoles' is so it reads on Salt & Pepper's ground too, its red, the
# cartridge's grey, the white of its lettering
OUTLINE = (14, 14, 24, 255)
EDGE = (26, 25, 37, 255)           # an inner fold between two faces
TOP = (64, 63, 82, 255)
TOP_LIGHT = (96, 95, 116, 255)
TOP_DARK = (48, 47, 64, 255)
TOP_LIGHT_BAND = (73, 72, 92, 255)
FRONT = (50, 49, 66, 255)
FRONT_DARK = (37, 36, 50, 255)
RED = (208, 38, 48, 255)
RED_LIGHT = (238, 92, 92, 255)
RED_DARK = (146, 24, 34, 255)
WHITE = (234, 234, 238, 255)
GREY = (150, 150, 162, 255)
GREY_DARK = (96, 96, 108, 255)
CART = (78, 78, 96, 255)
CART_LIGHT = (108, 108, 128, 255)
CART_DARK = (56, 56, 72, 255)
LABEL = (236, 230, 214, 255)
SKY = (70, 120, 196, 255)
HILLS = (60, 150, 84, 255)
SUN = (246, 206, 70, 255)
CABLE = (70, 69, 88, 255)
KNOB = (44, 43, 58, 255)
KNOB_LIGHT = (116, 115, 136, 255)

# A 3x5 face for the little lettering
GLYPHS = {
    "S": ("111", "100", "111", "001", "111"),
    "G": ("111", "100", "101", "101", "111"),
    "-": ("000", "000", "111", "000", "000"),
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
}


def text(img, x, y, s, color):
    for ch in s:
        for row, bits in enumerate(GLYPHS[ch]):
            for col, bit in enumerate(bits):
                if bit == "1":
                    img.putpixel((x + col, y + row), color)
        x += 4


def outlined(layer):
    """The layer with a one-pixel dark ring round everything it holds"""
    out = layer.copy()
    alpha = layer.getchannel("A")
    w, h = layer.size
    for y in range(h):
        for x in range(w):
            if alpha.getpixel((x, y)):
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and alpha.getpixel((nx, ny)):
                    out.putpixel((x, y), OUTLINE)
                    break
    return out


def layer():
    image = Image.new("RGBA", (GRID, GRID))
    return image, ImageDraw.Draw(image)


def console():
    img, d = layer()
    # The top, seen from above and in front: narrower at the back, its sides
    # angled in, lighter toward the front edge where the light falls
    d.polygon([(30, 34), (104, 34), (120, 60), (8, 60)], fill=TOP)
    d.polygon([(14, 51), (114, 51), (120, 60), (8, 60)], fill=TOP_LIGHT_BAND)
    # its rear edge, a lighter lip where the light catches it
    d.line([(31, 34), (103, 34)], fill=TOP_LIGHT)
    d.line([(30, 35), (104, 35)], fill=TOP_LIGHT)
    # the front edge of the top, caught by the light too
    d.line([(9, 60), (119, 60)], fill=TOP_LIGHT)
    # vents on the left of the top, shorter as they go back
    for y in (40, 44, 48, 52, 56):
        left = 30 - (y - 34) * 22 // 26
        d.line([(left + 8, y), (58 + (y - 34) // 3, y)], fill=TOP_DARK)
    # The front: a plain face under the top, the red stripe along it
    d.rectangle([8, 61, 120, 78], fill=FRONT)
    d.line([(8, 61), (120, 61)], fill=EDGE)
    d.rectangle([8, 63, 120, 65], fill=RED)
    d.line([(8, 63), (120, 63)], fill=RED_LIGHT)
    d.line([(8, 65), (120, 65)], fill=RED_DARK)
    # a darker plinth it stands on, set back a little
    d.rectangle([11, 79, 117, 82], fill=FRONT_DARK)
    d.line([(8, 78), (120, 78)], fill=EDGE)
    # its name, in white, on the front
    text(img, 46, 69, "SG-1000", WHITE)
    # the power switch at the left, a grey slide in a dark slot
    d.rectangle([15, 68, 28, 74], fill=OUTLINE)
    d.rectangle([16, 69, 21, 73], fill=GREY)
    d.line([(16, 69), (21, 69)], fill=WHITE)
    d.rectangle([22, 69, 27, 73], fill=FRONT_DARK)
    # the controller's socket, where its cable comes in
    d.rectangle([78, 69, 86, 74], fill=OUTLINE)
    d.rectangle([79, 70, 85, 73], fill=GREY_DARK)
    for x in (80, 82, 84):
        d.point((x, 71), fill=OUTLINE)
    # the pause button at the right, red, the console's own
    d.rectangle([97, 68, 111, 74], fill=OUTLINE)
    d.rectangle([98, 69, 110, 73], fill=RED)
    d.line([(98, 69), (110, 69)], fill=RED_LIGHT)
    d.line([(98, 73), (110, 73)], fill=RED_DARK)
    # The slot on top, right of center: a raised frame round a dark opening
    d.polygon([(68, 39), (100, 39), (103, 49), (65, 49)], fill=TOP_LIGHT)
    d.line([(65, 49), (103, 49)], fill=EDGE)
    d.rectangle([69, 41, 99, 45], fill=OUTLINE)
    return outlined(img)


def cartridge():
    img, d = layer()
    # A cartridge standing in the slot, its foot hidden in the opening
    d.rectangle([71, 14, 97, 43], fill=CART)
    d.line([(71, 14), (97, 14)], fill=CART_LIGHT)
    d.line([(71, 14), (71, 43)], fill=CART_LIGHT)
    d.line([(97, 15), (97, 43)], fill=CART_DARK)
    # its top, a sliver of it seen from above
    d.rectangle([72, 12, 96, 13], fill=CART_LIGHT)
    # grip ridges near the top
    for x in range(75, 95, 3):
        d.line([(x, 16), (x, 18)], fill=CART_DARK)
    # its label: paper, a red band, and a simple picture of hills and a sun
    d.rectangle([74, 21, 94, 38], fill=LABEL)
    d.rectangle([74, 21, 94, 24], fill=RED)
    d.rectangle([76, 26, 92, 36], fill=SKY)
    d.polygon([(76, 36), (81, 30), (85, 34), (88, 31), (92, 36)], fill=HILLS)
    d.rectangle([87, 27, 89, 29], fill=SUN)
    img = outlined(img)
    # the foot goes into the slot: nothing of it shows under the frame's edge
    ImageDraw.Draw(img).rectangle([0, 44, GRID - 1, GRID - 1], fill=(0, 0, 0, 0))
    return img


def controller(held=None):
    img, d = layer()
    # The joystick's base: a box, its top lit, its front in shadow
    d.polygon([(12, 96), (60, 96), (64, 104), (8, 104)], fill=TOP)
    d.line([(13, 96), (59, 96)], fill=TOP_LIGHT)
    d.rectangle([8, 104, 64, 114], fill=FRONT)
    d.line([(8, 104), (64, 104)], fill=EDGE)
    d.line([(8, 105), (64, 105)], fill=RED)
    d.rectangle([10, 113, 62, 115], fill=FRONT_DARK)
    # its two buttons, 1 and 2, on the right of the top
    for cx, label in ((42, "1"), (52, "2")):
        if held == int(label):
            # pressed: sunk a pixel, in shadow, no light on it
            d.ellipse([cx - 3, 98, cx + 3, 102], fill=RED_DARK)
            d.line([(cx - 1, 98), (cx + 1, 98)], fill=OUTLINE)
        else:
            d.ellipse([cx - 3, 97, cx + 3, 102], fill=RED)
            d.line([(cx - 1, 98), (cx + 1, 98)], fill=RED_LIGHT)
            d.line([(cx - 2, 102), (cx + 2, 102)], fill=RED_DARK)
        text(img, cx - 1, 107, label, WHITE)
    # the stick's collar on the left of the top
    d.ellipse([18, 97, 30, 103], fill=TOP_DARK)
    return outlined(img)


def stick(lean=0):
    img, d = layer()
    # the shaft from its collar, its top leaning `lean` pixels, then the
    # ball on it, a pixel lower when it leans
    drop = 1 if lean else 0
    d.line([(24, 100), (24 + lean, 88 + drop)], fill=KNOB, width=3)
    d.line([(23, 100), (23 + lean, 88 + drop)], fill=KNOB_LIGHT)
    d.ellipse([19 + lean, 80 + drop, 29 + lean, 90 + drop], fill=KNOB)
    d.ellipse([21 + lean, 82 + drop, 24 + lean, 85 + drop], fill=KNOB_LIGHT)
    d.point((22 + lean, 83 + drop), fill=WHITE)
    return outlined(img)


def cable():
    img, d = layer()
    # from the back of the joystick's base, round and up into the socket
    d.line([(60, 97), (60, 92), (62, 89), (66, 87), (78, 87), (81, 85), (82, 81), (82, 75)],
           fill=CABLE, width=2)
    return outlined(img)


def draw(pose=0):
    """A pose of the design, 128 square, transparent round it"""
    lean, held = POSES[pose]
    image = Image.new("RGBA", (GRID, GRID))
    for part in (console(), cartridge(), cable(), controller(held), stick(lean)):
        image.alpha_composite(part)
    return image


def png(image):
    stream = io.BytesIO()
    image.save(stream, format="PNG", optimize=True, compress_level=9)
    return stream.getvalue()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def motion_bounds(cells):
    """Where any pose differs from the rest pose, four pixels wider each way"""
    box = None
    for cell in cells[1:]:
        diff = ImageChops.difference(cell, cells[0]).getbbox(alpha_only=False)
        if diff:
            box = diff if box is None else (min(box[0], diff[0]), min(box[1], diff[1]),
                                            max(box[2], diff[2]), max(box[3], diff[3]))
    x0, y0 = max(0, box[0] - 4), max(0, box[1] - 4)
    x1, y1 = min(CELL, box[2] + 4), min(CELL, box[3] + 4)
    return [x0, y0, x1 - x0, y1 - y0]


def write_records(poses):
    animations = ART / "animations"
    source_name = f"source/{KEY}.png"
    renderer_name = f"placeholders/{Path(__file__).name}"
    strip = Image.new("RGBA", (GRID * len(poses), GRID))
    for i, pose in enumerate(poses):
        strip.paste(pose, (i * GRID, 0))
    cells = [pose.resize((CELL, CELL), Image.Resampling.NEAREST) for pose in poses]
    atlas = Image.new("RGBA", (CELL * len(cells), CELL))
    for i, cell in enumerate(cells):
        atlas.paste(cell, (i * CELL, 0))
    source, atlas_png, still_png = png(strip), png(atlas), png(cells[0])
    (ART / source_name).write_bytes(source)
    (animations / f"{KEY}.png").write_bytes(atlas_png)
    (animations / f"{KEY}-still.png").write_bytes(still_png)
    # The frame table, in the order tools/systemart-original-pack.py writes one
    meta = {
        "key": KEY, "name": NAME,
        "description": "Placeholder drawn by script: dark angular console with a red stripe and a "
                       "cartridge in the top slot; its joystick rocks and its buttons press",
        "cell": CELL, "cols": len(cells), "rows": 1, "unique": len(cells),
        "duration_ms": sum(ms for _, ms in FRAMES),
        "kind": "system", "static": False, "placeholder": True, "expected_poses": len(cells),
        "motion_bounds": motion_bounds(cells),
        "frames": [{"tile": t, "ms": ms} for t, ms in FRAMES],
        "still_time_ms": 0,
        "decoded_atlas_bytes": atlas.width * atlas.height * 4,
        "inputs": [source_name],
        "renderers": [renderer_name],
        "input_sha256": {source_name: sha(source)},
        "renderer_sha256": {renderer_name: sha(Path(__file__).read_bytes())},
        "atlas": f"{KEY}.png", "still": f"{KEY}-still.png",
        "atlas_sha256": sha(atlas_png), "still_sha256": sha(still_png),
    }
    (animations / f"{KEY}.json").write_text(json.dumps(meta, indent=2) + "\n")
    # The catalogue again, the way the pack writes it
    assets = [json.loads(p.read_text()) for p in sorted(animations.glob("*.json")) if p.name != "catalogue.json"]
    catalogue = {"schema": 1, "cell": CELL, "assets": assets,
                 "total_png_bytes": sum(p.stat().st_size for p in animations.glob("*.png")),
                 "largest_decoded_atlas_bytes": max(a["decoded_atlas_bytes"] for a in assets)}
    (animations / "catalogue.json").write_text(json.dumps(catalogue, indent=2) + "\n")
    # The manifest: the source among the others, and the counts
    manifest_path = ART / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["selected_sources"][KEY] = source_name
    manifest["sources"] = sorted([s for s in manifest["sources"] if s["file"] != source_name] +
                                 [{"file": source_name, "selected": True, "sha256": sha(source),
                                   "bytes": len(source)}], key=lambda s: s["file"])
    manifest.setdefault("placeholders", {})[KEY] = {
        "name": NAME, "renderer": renderer_name,
        "method": "Drawn by script on a 128-pixel grid from simple shapes, every pixel doubled "
                  "for the 256-pixel cell; three poses; no image generation, no other artist's "
                  "work, no photograph",
    }
    shelves = sum(1 for a in assets if a["kind"] == "system")    # Favorites and MP4 too
    manifest["system_count"] = manifest["verification"]["systems"] = shelves
    manifest["design_count"] = manifest["verification"]["designs"] = len(assets)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    index_path = ART / "source-index.json"
    index = [e for e in json.loads(index_path.read_text()) if e["key"] != KEY]
    index.append({"key": KEY, "name": NAME, "file": source_name})
    index_path.write_text(json.dumps(index, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preview", type=Path, help="write only a 4x enlargement of the poses here")
    args = parser.parse_args()
    poses = [draw(i) for i in range(len(POSES))]
    if args.preview:
        strip = Image.new("RGBA", (GRID * len(poses), GRID))
        for i, pose in enumerate(poses):
            strip.paste(pose, (i * GRID, 0))
        strip.resize((strip.width * 4, GRID * 4), Image.Resampling.NEAREST).save(args.preview)
        return 0
    write_records(poses)
    print(f"{KEY}: drawn; source, atlas, still and records written")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
