#!/usr/bin/env python3
"""Rebuild Guardian 320x170 backgrounds from the full Figma 632x332 exports.

One shared pipeline for both products. Guardian MAX uses its own Figma frames
(logo, GH/s, and a slightly wider global-stats row). Live numbers are erased
and redrawn by the firmware.

Erases only the glyphs the firmware redraws. Chrome (pill, wifi, icons,
units, GLOBAL STATS, bar shapes, HALVING pill, BLOCKS LEFT pill) stays.

Inputs (tools/figma):
  MINI: miner_632.png clock_632.png price_632.png global_632.png logo_guardian_mini.png
  MAX:  max_miner_632.png max_clock_632.png max_price_632.png max_global_632.png
        logo_guardian_max.png (rasterized from logo_guardian_max.svg, Figma 755:127)

Outputs:
  MINI: src/media/guardian_miner_bg.h, src/media/guardian_cycle_bg.h,
        tools/figma/<screen>_bg_320.png, tools/figma/preview_<screen>.png
  MAX:  src/media/guardian_max_bg.h,
        tools/figma/max_<screen>_bg_320.png, tools/figma/preview_max_<screen>.png

Usage:
  .venv/bin/python tools/rebuild_guardian_screens.py            # both variants
  .venv/bin/python tools/rebuild_guardian_screens.py mini
  .venv/bin/python tools/rebuild_guardian_screens.py max
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIG = os.path.join(ROOT, "tools", "figma")
FONT = os.path.join(ROOT, "tools", "fonts", "DMSans-Regular.ttf")
FONT_BOLD = os.path.join(ROOT, "fonts", "static", "DMSans-Bold.ttf")
SX = 320 / 632.0
SY = 170 / 332.0

SCREENS = ("miner", "clock", "price", "global")

YELLOW = (255, 191, 0)
CYAN = (0, 229, 207)
CARD = (23, 26, 42)

VARIANTS = {
    "mini": {
        "prefix": "",
        "logo": "logo_guardian_mini.png",
        # right edge of the big hashrate cover box; "KH/s" starts at x=423
        "hashrate_right": 416,
        # Figma boxes for the live global-stats glyphs, then device draw points
        "global_cover": {
            "diff": (36, 128, 156, 172),
            "t": (150, 140, 184, 172),
            "fee": (470, 128, 516, 172),
            "sat": (508, 138, 602, 174),
        },
        "global_draw": {"diff": (78, 68), "t": (90, 74), "fee": (256, 68), "sat": (276, 74)},
        "sample": {
            "price": "85 680 USD",
            "price_big": "85 680",
            "hashrate": "253.72",
            "hashrate_short": "253.7",
            "unit": "KH/s",
            "total": "21.9 GH",
            "diff": "0.0000",
            "block": "970197",
            "left": "79803 BLOCKS LEFT",
            "net_diff": "132.72",
            "fee": "1",
            "global": "964",
        },
    },
    "max": {
        "prefix": "max_",
        "logo": "logo_guardian_max.png",
        "sources": {
            "miner": "max_miner_632.png",
            "clock": "max_clock_632.png",
            "price": "max_price_632.png",
            "global": "max_global_632.png",
        },
        # "GH/s" starts at x=432 on the MAX frame
        "hashrate_right": 428,
        # MAX spreads the global row: difficulty sits further left, fee further right
        "global_cover": {
            "diff": (12, 128, 134, 172),
            "t": (130, 140, 158, 176),
            "fee": (500, 128, 545, 172),
            "sat": (530, 136, 625, 176),
        },
        "global_draw": {"diff": (64, 68), "t": (73, 74), "fee": (271, 68), "sat": (292, 74)},
        "sample": {
            "price": "128 397 USD",
            "price_big": "128 397",
            "hashrate": "200.7",
            "hashrate_short": "200.7",
            "unit": "GH/s",
            "total": "720.5 TH",
            "diff": "0.001",
            "block": "885790",
            "left": "163421 BLOCKS LEFT",
            "net_diff": "110.56",
            "fee": "3",
            "global": "799",
        },
    },
}


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def lerp_px(a, b, t):
    return tuple(int(a[i] * (1.0 - t) + b[i] * t) for i in range(3))


def cover_text(im, box, delta=28):
    """Replace pixels brighter than the background sampled just outside the box."""
    px = im.load()
    x0, y0, x1, y1 = box
    h = im.size[1]
    ya = max(0, y0 - 2)
    yb = min(h - 1, y1 + 1)
    span = max(1, y1 - y0)
    for y in range(y0, y1):
        t = (y - y0 + 0.5) / span
        for x in range(x0, x1):
            bg = lerp_px(px[x, ya], px[x, yb], t)
            p = px[x, y]
            if sum(p) > sum(bg) + delta:
                px[x, y] = bg


def flatten_ink(im, box, color, tol=42):
    """Turn non-bar pixels inside a flat bar into the bar color."""
    px = im.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = px[x, y]
            if any(abs(p[i] - color[i]) > tol for i in range(3)):
                px[x, y] = color


def fill_rect(im, box, color):
    px = im.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        for x in range(x0, x1):
            px[x, y] = color


def erase(im, box):
    """Paint a box with the background interpolated from the rows just outside it."""
    px = im.load()
    x0, y0, x1, y1 = box
    h = im.size[1]
    ya = max(0, y0 - 2)
    yb = min(h - 1, y1 + 1)
    span = max(1, y1 - y0)
    for y in range(y0, y1):
        t = (y - y0 + 0.5) / span
        for x in range(x0, x1):
            px[x, y] = lerp_px(px[x, ya], px[x, yb], t)


def prepare(variant, name):
    file_name = variant.get("sources", {}).get(name, name + "_632.png")
    return Image.open(os.path.join(FIG, file_name)).convert("RGB")


def stamp_logo(im, logo_file):
    """Replace the stretched screen logo with one uniform render of the component.

    The logo PNG is 350x60 on an opaque RGB(23,23,23) background which is keyed
    out, then scaled to the 242x42 the screen instances use and pasted at (18,18).
    """
    logo = Image.open(os.path.join(FIG, logo_file)).convert("RGBA")
    src = logo.load()
    for y in range(logo.size[1]):
        for x in range(logo.size[0]):
            r, g, b, a = src[x, y]
            if abs(r - 23) < 14 and abs(g - 23) < 14 and abs(b - 23) < 14:
                src[x, y] = (0, 0, 0, 0)
    logo = logo.resize((242, 42), Image.Resampling.LANCZOS)
    erase(im, (12, 10, 320, 66))
    im.paste(logo, (18, 18), logo)


def build(variant):
    """Shared erase pipeline. All boxes are in 632x332 Figma coordinates."""
    logo = variant["logo"]
    pill = (382, 26, 522, 52)

    miner = prepare(variant, "miner")
    stamp_logo(miner, logo)
    cover_text(miner, pill)
    cover_text(miner, (220, 96, 410, 120))                           # CURRENT HASHRATE
    cover_text(miner, (190, 124, variant["hashrate_right"], 190))    # big number, unit stays
    cover_text(miner, (270, 194, 370, 220))                          # total hashes
    for box in ((32, 268, 148, 308), (182, 268, 298, 308), (334, 268, 450, 308), (486, 268, 598, 308)):
        fill_rect(miner, box, CARD)                                  # card value + label rows

    clock = prepare(variant, "clock")
    stamp_logo(clock, logo)
    cover_text(clock, pill)
    cover_text(clock, (230, 96, 400, 120))                           # CURRENT TIME
    cover_text(clock, (200, 124, 420, 190))                          # time
    flatten_ink(clock, (100, 260, 270, 300), YELLOW)                 # hashrate bar
    flatten_ink(clock, (385, 260, 590, 300), CYAN)                   # block bar

    price = prepare(variant, "price")
    stamp_logo(price, logo)
    cover_text(price, (155, 122, 426, 178))                          # big price
    cover_text(price, (415, 145, 490, 190))                          # USD, redrawn larger in firmware
    flatten_ink(price, (100, 260, 270, 300), YELLOW)
    flatten_ink(price, (385, 260, 590, 300), CYAN)

    glob = prepare(variant, "global")
    stamp_logo(glob, logo)
    cover_text(glob, pill)
    cover = variant["global_cover"]
    cover_text(glob, cover["diff"])                                  # difficulty
    cover_text(glob, cover["t"])                                     # "T"
    cover_text(glob, cover["fee"])                                   # fee
    cover_text(glob, cover["sat"])                                   # "sat/vB"
    flatten_ink(glob, (30, 242, 230, 276), CYAN)                     # block bar (pill below stays)
    cover_text(glob, (40, 284, 176, 300), delta=18)                  # BLOCKS LEFT text, keeps pill
    cover_text(glob, (268, 260, 368, 286))                           # HALVING text, keeps dark pill
    flatten_ink(glob, (400, 255, 600, 296), YELLOW)                  # global hashrate bar

    frames = {"miner": miner, "clock": clock, "price": price, "global": glob}
    out = {}
    for key, im in frames.items():
        small = im.resize((320, 170), Image.Resampling.LANCZOS)
        small.save(os.path.join(FIG, variant["prefix"] + key + "_bg_320.png"))
        out[key] = small
    return out


def dump_bitmap(f, name, wname, hname, im):
    f.write("const uint16_t {} = 320;\n".format(wname))
    f.write("const uint16_t {} = 170;\n\n".format(hname))
    f.write("const unsigned short {}[0xD480] PROGMEM = {{\n".format(name))
    px = list(im.getdata())
    for i in range(0, len(px), 16):
        chunk = px[i:i + 16]
        f.write("  " + ", ".join("0x{:04X}".format(rgb565(*p)) for p in chunk) + ",\n")
    f.write("};\n\n")


def write_headers_mini(frames):
    media = os.path.join(ROOT, "src", "media")
    path = os.path.join(media, "guardian_miner_bg.h")
    with open(path, "w") as f:
        f.write("// Guardian miner background 320x170. Live numbers are drawn on top.\n\n")
        dump_bitmap(f, "guardianMinerScreen", "guardianMinerWidth", "guardianMinerHeight", frames["miner"])
    print("wrote", path)
    path = os.path.join(media, "guardian_cycle_bg.h")
    with open(path, "w") as f:
        f.write("// Guardian clock, price and global backgrounds, 320x170.\n")
        f.write("// Live numbers are drawn on top.\n\n")
        dump_bitmap(f, "guardianClockScreen", "guardianClockWidth", "guardianClockHeight", frames["clock"])
        dump_bitmap(f, "guardianPriceScreen", "guardianPriceWidth", "guardianPriceHeight", frames["price"])
        dump_bitmap(f, "guardianGlobalScreen", "guardianGlobalWidth", "guardianGlobalHeight", frames["global"])
    print("wrote", path)


def write_headers_max(frames):
    media = os.path.join(ROOT, "src", "media")
    path = os.path.join(media, "guardian_max_bg.h")
    names = [
        ("miner", "guardianMaxMinerScreen", "guardianMaxMinerWidth", "guardianMaxMinerHeight"),
        ("clock", "guardianMaxClockScreen", "guardianMaxClockWidth", "guardianMaxClockHeight"),
        ("price", "guardianMaxPriceScreen", "guardianMaxPriceWidth", "guardianMaxPriceHeight"),
        ("global", "guardianMaxGlobalScreen", "guardianMaxGlobalWidth", "guardianMaxGlobalHeight"),
    ]
    with open(path, "w") as f:
        f.write("// Guardian MAX backgrounds, 320x170. Live numbers are drawn on top.\n")
        f.write("// Same layout as Guardian MINI; only the MAX badge and GH/s differ.\n\n")
        for key, name, wname, hname in names:
            dump_bitmap(f, name, wname, hname, frames[key])
    print("wrote", path)


def paint_preview(variant, frames):
    """Approximate the firmware draw calls (same coordinates as tDisplayDriver.cpp)."""
    s = variant["sample"]
    g = variant["global_draw"]
    white = (255, 255, 255)
    muted = (153, 161, 175)
    ink = (18, 21, 35)
    pricec = (209, 213, 220)

    def text(dr, txt, xy, size, fill, anchor, bold=False):
        face = FONT_BOLD if bold else FONT
        dr.text(xy, txt, font=ImageFont.truetype(face, size), fill=fill, anchor=anchor)

    bars = [
        (s["hashrate_short"], (82, 134), 16, ink, "mt"),
        (s["unit"], (122, 140), 9, ink, "mt"),
        (s["block"], (222, 134), 16, ink, "mt"),
        ("CURRENT", (272, 135), 7, ink, "mt"),
        ("BLOCK", (272, 144), 7, ink, "mt"),
    ]
    specs = {
        "miner": [
            (s["price"], (274, 12), 13, pricec, "rt", True),
            ("CURRENT HASHRATE", (160, 50), 8, muted, "mt"),
            (s["hashrate"], (152, 60), 36, white, "mt", True),
            (s["total"], (160, 102), 7, muted, "mt"),
            ("0", (45, 139), 9, white, "mt"),
            (s["diff"], (122, 139), 9, white, "mt"),
            ("39\u00b0", (199, 139), 9, white, "mt"),
            ("12", (276, 139), 9, white, "mt"),
            ("BLOCKS", (45, 151), 7, muted, "mt"),
            ("DIFF", (122, 151), 7, muted, "mt"),
            ("TEMP", (199, 151), 7, muted, "mt"),
            ("SHARES", (276, 151), 7, muted, "mt"),
        ],
        "clock": [
            (s["price"], (274, 12), 13, pricec, "rt", True),
            ("CURRENT TIME", (160, 50), 8, muted, "mt"),
            ("19:09", (160, 60), 40, white, "mt", True),
        ] + bars,
        "price": bars,
        "global": [
            (s["price"], (274, 12), 13, pricec, "rt", True),
            (s["net_diff"], g["diff"], 18, white, "rt"),
            ("T", g["t"], 12, CYAN, "mt"),
            (s["fee"], g["fee"], 18, white, "rt"),
            ("sat/vB", g["sat"], 9, CYAN, "mt"),
            (s["block"], (43, 125), 16, ink, "mt"),
            ("CURRENT", (96, 124), 7, ink, "mt"),
            ("BLOCK", (96, 134), 7, ink, "mt"),
            (s["left"], (56, 147), 7, white, "mt"),
            ("HALVING", (160, 134), 8, white, "mt"),
            (s["global"], (230, 128), 16, ink, "rt"),
            ("EH/s", (248, 134), 8, ink, "mt"),
            ("GLOBAL", (286, 126), 7, ink, "mt"),
            ("HASHRATE", (286, 136), 7, ink, "mt"),
        ],
    }
    for key, items in specs.items():
        im = frames[key].copy()
        dr = ImageDraw.Draw(im)
        for item in items:
            text(dr, *item)
        if key == "price":
            face = ImageFont.truetype(FONT_BOLD, 40)
            num = s["price_big"]
            dr.text((160, 60), num, font=face, fill=white, anchor="mt")
            box = face.getbbox(num)
            usd_x = 160 + (box[2] - box[0]) / 2 + 6
            dr.text((usd_x, 76), "USD", font=ImageFont.truetype(FONT_BOLD, 20), fill=CYAN, anchor="lt")
        path = os.path.join(FIG, "preview_" + variant["prefix"] + key + ".png")
        im.resize((640, 340), Image.Resampling.NEAREST).save(path)
        print("wrote", path)


def run(name):
    variant = VARIANTS[name]
    print("== Guardian", name.upper())
    frames = build(variant)
    if name == "mini":
        write_headers_mini(frames)
    else:
        write_headers_max(frames)
    paint_preview(variant, frames)


if __name__ == "__main__":
    args = [a.lstrip("-") for a in sys.argv[1:]]
    if not args or "all" in args:
        args = ["mini", "max"]
    for arg in args:
        if arg not in VARIANTS:
            sys.exit("unknown variant '{}' (use mini, max or all)".format(arg))
        run(arg)
