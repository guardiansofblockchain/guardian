#!/usr/bin/env python3
"""
Export an RGB565 .h array back to PNG. Uses the first 320x170 = 54400 pixels.

Usage:
  python3 tools/header_to_png.py src/media/images_320_170.h --out-dir out
  python3 tools/header_to_png.py src/media/images_320_170.h --out-dir out --swap
  python3 tools/header_to_png.py src/media/images_320_170.h --out-dir out --both
     -> name.png (no swap) and name_swap.png (with swap)
  python3 tools/header_to_png.py ... --bgr
     -> interpret the pixels as BGR565 (R and B swapped)
"""
from __future__ import print_function
import re
import sys
import os

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install Pillow", file=sys.stderr)
    sys.exit(1)

WIDTH = 320
HEIGHT = 170
PIXELS = 320 * 170


def rgb565_to_rgb(pixel, bgr565=False):
    """RGB565 (uint16) -> (r, g, b) 0-255. bgr565=True uses BGR order (some converters)."""
    if bgr565:
        b5 = (pixel >> 11) & 0x1F
        g6 = (pixel >> 5) & 0x3F
        r5 = pixel & 0x1F
    else:
        r5 = (pixel >> 11) & 0x1F
        g6 = (pixel >> 5) & 0x3F
        b5 = pixel & 0x1F
    r = (r5 << 3) | (r5 >> 2)
    g = (g6 << 2) | (g6 >> 4)
    b = (b5 << 3) | (b5 >> 2)
    return (r, g, b)


def extract_array(content, name):
    pattern = r"const unsigned short " + re.escape(name) + r"\[[^\]]+\]\s*PROGMEM\s*=\s*\{\s*(.*?)\};"
    m = re.search(pattern, content, re.DOTALL)
    if not m:
        return None
    # Keep values followed by a comma (pixel data). Hex in comments like "// 0x0010 (16)" drew vertical lines.
    hex_values = re.findall(r"(0x[0-9A-Fa-f]{4})\s*,", m.group(1))
    return [int(h, 16) for h in hex_values]


def array_to_png(arr, out_path, swap_bytes=False, bgr565=False, stride_340=False):
    """stride_340: arrays of 0xFC58 (64600) store 340 pixels per row. We draw 320x170."""
    pixels_rgb = []
    if stride_340 and len(arr) >= 340 * 170:
        for row in range(HEIGHT):
            for col in range(WIDTH):
                i = row * 340 + col
                p = arr[i] if i < len(arr) else 0
                if swap_bytes:
                    p = ((p & 0xFF) << 8) | (p >> 8)
                pixels_rgb.append(rgb565_to_rgb(p, bgr565=bgr565))
    else:
        for i in range(PIXELS):
            p = arr[i] if i < len(arr) else 0
            if swap_bytes:
                p = ((p & 0xFF) << 8) | (p >> 8)
            pixels_rgb.append(rgb565_to_rgb(p, bgr565=bgr565))
    im = Image.new("RGB", (WIDTH, HEIGHT))
    im.putdata(pixels_rgb)
    im.save(out_path)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    default_h = os.path.join(project_root, "src", "media", "images_guardian_320_170.h")
    out_dir = project_root
    swap_bytes = False
    both = False
    bgr565 = False

    if len(sys.argv) >= 2 and not sys.argv[1].startswith("--"):
        h_path = sys.argv[1]
    else:
        h_path = default_h

    if "--out-dir" in sys.argv:
        i = sys.argv.index("--out-dir")
        if i + 1 < len(sys.argv):
            out_dir = sys.argv[i + 1]
    if "--swap" in sys.argv:
        swap_bytes = True
    if "--both" in sys.argv:
        both = True
    if "--bgr" in sys.argv:
        bgr565 = True

    if not os.path.isfile(h_path):
        print("File not found:", h_path, file=sys.stderr)
        sys.exit(1)

    with open(h_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    if "guardian" in h_path.lower():
        screens = [("guardianSetupScreen", "guardian_setup_from_h"), ("guardianInitScreen", "guardian_init_from_h")]
    else:
        screens = [
            ("setupModeScreen", "nerd_setupMode"),
            ("initScreen", "nerd_init"),
            ("MinerScreen", "nerd_miner"),
            ("minerClockScreen", "nerd_minerClock"),
            ("globalHashScreen", "nerd_globalHash"),
            ("priceScreen", "nerd_price"),
            ("ChristmasScreen", "nerd_christmas"),
        ]

    written = 0
    for name, base in screens:
        arr = extract_array(content, name)
        if not arr:
            continue
        # Arrays of 64600 values (0xFC58) are stored at 340 px per row
        use_stride340 = len(arr) >= 340 * 170
        if both:
            array_to_png(arr, os.path.join(out_dir, base + ".png"), swap_bytes=False, bgr565=bgr565, stride_340=use_stride340)
            array_to_png(arr, os.path.join(out_dir, base + "_swap.png"), swap_bytes=True, bgr565=bgr565, stride_340=use_stride340)
            print("Wrote: {}.png and {}_swap.png{}".format(base, base, " (stride 340)" if use_stride340 else ""))
            written += 2
        else:
            path = os.path.join(out_dir, base + ".png")
            array_to_png(arr, path, swap_bytes=swap_bytes, bgr565=bgr565, stride_340=use_stride340)
            print("Wrote: {}{}".format(path, " (stride 340)" if use_stride340 else ""))
            written += 1

    if written == 0:
        print("No array found.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
