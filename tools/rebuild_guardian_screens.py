#!/usr/bin/env python3
"""Rebuild Guardian 320x170 backgrounds from the full Figma 632 exports.

Erases only the glyphs the firmware redraws. Chrome (pill, wifi, icons,
KH/s, USD, GLOBAL STATS, bar shapes) stays in the bitmap.
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIG = os.path.join(ROOT, "tools", "figma")
FONT = os.path.join(ROOT, "tools", "fonts", "DMSans-Regular.ttf")
SX = 320 / 632.0
SY = 170 / 332.0


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


def kill_light_to(im, box, color, thresh=110):
    px = im.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b = px[x, y]
            if r >= thresh and g >= thresh and b >= thresh:
                px[x, y] = color


def prepare(name):
    im = Image.open(os.path.join(FIG, name + "_632.png")).convert("RGB")
    return im


def stamp_logo(im):
    """Replace the stretched screen logo with one uniform render of the component."""
    logo = Image.open(os.path.join(FIG, "logo_guardian_mini.png")).convert("RGBA")
    src = logo.load()
    for y in range(logo.size[1]):
        for x in range(logo.size[0]):
            r, g, b, a = src[x, y]
            if abs(r - 23) < 14 and abs(g - 23) < 14 and abs(b - 23) < 14:
                src[x, y] = (0, 0, 0, 0)
    logo = logo.resize((242, 42), Image.Resampling.LANCZOS)
    px = im.load()
    x0, y0, x1, y1 = 12, 10, 320, 66
    ya = max(0, y0 - 2)
    yb = min(im.size[1] - 1, y1 + 1)
    span = max(1, y1 - y0)
    for y in range(y0, y1):
        t = (y - y0 + 0.5) / span
        for x in range(x0, x1):
            px[x, y] = lerp_px(px[x, ya], px[x, yb], t)
    im.paste(logo, (18, 18), logo)


def build():
    miner = prepare("miner")
    stamp_logo(miner)
    cover_text(miner, (382, 26, 522, 52))
    cover_text(miner, (220, 96, 410, 120))
    cover_text(miner, (190, 124, 416, 190))
    cover_text(miner, (270, 194, 370, 220))
    card = (23, 26, 42)
    for box in ((32, 268, 148, 308), (182, 268, 298, 308), (334, 268, 450, 308), (486, 268, 598, 308)):
        fill_rect(miner, box, card)

    clock = prepare("clock")
    stamp_logo(clock)
    cover_text(clock, (382, 26, 522, 52))
    cover_text(clock, (230, 96, 400, 120))
    cover_text(clock, (200, 124, 420, 190))
    flatten_ink(clock, (100, 260, 270, 300), (255, 191, 0))
    flatten_ink(clock, (385, 260, 590, 300), (0, 229, 207))

    price = prepare("price")
    stamp_logo(price)
    cover_text(price, (155, 122, 426, 178))
    flatten_ink(price, (100, 260, 270, 300), (255, 191, 0))
    flatten_ink(price, (385, 260, 590, 300), (0, 229, 207))

    glob = prepare("global")
    stamp_logo(glob)
    cover_text(glob, (382, 26, 522, 52))
    cover_text(glob, (36, 128, 156, 172))
    cover_text(glob, (150, 140, 184, 172))
    cover_text(glob, (470, 128, 516, 172))
    cover_text(glob, (508, 138, 602, 174))
    flatten_ink(glob, (30, 242, 230, 276), (0, 229, 207))
    cover_text(glob, (40, 284, 176, 300), delta=18)
    cover_text(glob, (268, 260, 368, 286))
    flatten_ink(glob, (400, 255, 600, 296), (255, 191, 0))

    frames = {
        "miner": miner,
        "clock": clock,
        "price": price,
        "global": glob,
    }
    out = {}
    for key, im in frames.items():
        small = im.resize((320, 170), Image.Resampling.LANCZOS)
        small.save(os.path.join(FIG, key + "_bg_320.png"))
        out[key] = small
    return out


def write_headers(frames):
    media = os.path.join(ROOT, "src", "media")

    def dump(f, name, wname, hname, im):
        f.write("const uint16_t {} = 320;\n".format(wname))
        f.write("const uint16_t {} = 170;\n\n".format(hname))
        f.write("const unsigned short {}[0xD480] PROGMEM = {{\n".format(name))
        px = list(im.getdata())
        for i in range(0, len(px), 16):
            chunk = px[i:i + 16]
            f.write("  " + ", ".join("0x{:04X}".format(rgb565(*p)) for p in chunk) + ",\n")
        f.write("};\n\n")

    with open(os.path.join(media, "guardian_miner_bg.h"), "w") as f:
        f.write("// Guardian miner background 320x170. Live numbers are drawn on top.\n\n")
        dump(f, "guardianMinerScreen", "guardianMinerWidth", "guardianMinerHeight", frames["miner"])
    with open(os.path.join(media, "guardian_cycle_bg.h"), "w") as f:
        f.write("// Guardian clock, price and global backgrounds, 320x170.\n")
        f.write("// Live numbers are drawn on top.\n\n")
        dump(f, "guardianClockScreen", "guardianClockWidth", "guardianClockHeight", frames["clock"])
        dump(f, "guardianPriceScreen", "guardianPriceWidth", "guardianPriceHeight", frames["price"])
        dump(f, "guardianGlobalScreen", "guardianGlobalWidth", "guardianGlobalHeight", frames["global"])


def paint_preview(frames):
    font = lambda n: ImageFont.truetype(FONT, n)

    def text(dr, s, xy, size, fill, anchor):
        dr.text(xy, s, font=font(size), fill=fill, anchor=anchor)

    specs = {
        "miner": [
            ("85 680 USD", (261, 16), 9, (209, 213, 220), "rt"),
            ("CURRENT HASHRATE", (160, 50), 8, (153, 161, 175), "mt"),
            ("253.72", (152, 64), 30, (255, 255, 255), "mt"),
            ("18 420 MH", (160, 102), 7, (153, 161, 175), "mt"),
            ("0", (45, 139), 9, (255, 255, 255), "mt"),
            ("0.0000", (122, 139), 9, (255, 255, 255), "mt"),
            ("39\u00b0", (199, 139), 9, (255, 255, 255), "mt"),
            ("12", (276, 139), 9, (255, 255, 255), "mt"),
            ("BLOCKS", (45, 151), 7, (153, 161, 175), "mt"),
            ("DIFF", (122, 151), 7, (153, 161, 175), "mt"),
            ("TEMP", (199, 151), 7, (153, 161, 175), "mt"),
            ("SHARES", (276, 151), 7, (153, 161, 175), "mt"),
        ],
        "clock": [
            ("85 680 USD", (261, 16), 9, (209, 213, 220), "rt"),
            ("CURRENT TIME", (160, 48), 8, (153, 161, 175), "mt"),
            ("19:09", (160, 64), 30, (255, 255, 255), "mt"),
            ("253.7", (82, 134), 16, (18, 21, 35), "mt"),
            ("KH/s", (122, 140), 9, (18, 21, 35), "mt"),
            ("970197", (222, 134), 16, (18, 21, 35), "mt"),
            ("CURRENT", (272, 130), 7, (18, 21, 35), "mt"),
            ("BLOCK", (272, 140), 7, (18, 21, 35), "mt"),
        ],
        "price": [
            ("85 680", (208, 64), 24, (255, 255, 255), "rt"),
            ("253.7", (82, 134), 16, (18, 21, 35), "mt"),
            ("KH/s", (122, 140), 9, (18, 21, 35), "mt"),
            ("970197", (222, 134), 16, (18, 21, 35), "mt"),
            ("CURRENT", (272, 130), 7, (18, 21, 35), "mt"),
            ("BLOCK", (272, 140), 7, (18, 21, 35), "mt"),
        ],
        "global": [
            ("85 680 USD", (261, 16), 9, (209, 213, 220), "rt"),
            ("132.72", (78, 68), 18, (255, 255, 255), "rt"),
            ("T", (90, 74), 12, (0, 229, 207), "mt"),
            ("1", (256, 68), 18, (255, 255, 255), "rt"),
            ("sat/vB", (276, 76), 9, (0, 229, 207), "mt"),
            ("970197", (43, 125), 16, (18, 21, 35), "mt"),
            ("CURRENT", (96, 124), 7, (18, 21, 35), "mt"),
            ("BLOCK", (96, 134), 7, (18, 21, 35), "mt"),
            ("79803 BLOCKS LEFT", (56, 147), 7, (255, 255, 255), "mt"),
            ("HALVING", (160, 134), 8, (255, 255, 255), "mt"),
            ("964", (230, 128), 16, (18, 21, 35), "rt"),
            ("EH/s", (248, 134), 8, (18, 21, 35), "mt"),
            ("GLOBAL", (286, 126), 7, (18, 21, 35), "mt"),
            ("HASHRATE", (286, 136), 7, (18, 21, 35), "mt"),
        ],
    }
    probe = ImageFont.truetype(FONT, 16)
    for sample in ("253.7", "970197", "132.72", "79803 BLOCKS LEFT", "85 766 USD", "0.00015"):
        box = probe.getbbox(sample)
        print("width16", sample, box[2] - box[0] if box else None)

    for key, items in specs.items():
        im = frames[key].copy()
        dr = ImageDraw.Draw(im)
        for s, xy, size, fill, anchor in items:
            text(dr, s, xy, size, fill, anchor)
        path = os.path.join(FIG, "preview_" + key + ".png")
        im.resize((640, 340), Image.Resampling.NEAREST).save(path)
        print("wrote", path)


def math_check():
    height = 970197
    hashrate = 964122384096933800000
    difficulty = 132716002350731.3
    fee = 1
    price = 85766
    left = ((height // 210000) + 1) * 210000 - height
    prog = (210000 - left) * 100 // 210000
    eh = hashrate / 1e18
    tera = difficulty / 1e12
    print("price", price)
    print("height", height)
    print("EH {:.0f}".format(eh))
    print("T {:.2f}".format(tera))
    print("fee", fee)
    print("blocks_left", left, "progress", prog)


if __name__ == "__main__":
    math_check()
    frames = build()
    write_headers(frames)
    paint_preview(frames)
