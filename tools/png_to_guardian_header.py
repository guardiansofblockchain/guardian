#!/usr/bin/env python3
"""
Guardian: převod PNG obrázků na hlavičkový soubor pro displej (RGB565).

Použití:
  python3 png_to_guardian_header.py guardian_setup.png guardian_init.png
  python3 png_to_guardian_header.py --black   # černý placeholder

Výstup: src/media/images_guardian_320_170.h

Soubory:
  guardian_setup.png  = obrazovka setup (WiFi jméno/heslo, WAITING CONFIG) – 1. argument
  guardian_init.png  = obrazovka při startu (init/checking) – 2. argument

Rozměr obou: 320 x 170 pixelů.
"""
from __future__ import print_function
import sys
import os

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

WIDTH = 320
HEIGHT = 170
PIXELS = WIDTH * HEIGHT  # 54400
PIXELS_HEX = "0xD480"

def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def image_to_array(path):
    if not HAS_PIL:
        print("Pro PNG potřebuješ Pillow: pip install Pillow (nebo: python3 -m venv .venv && source .venv/bin/activate && pip install Pillow)", file=sys.stderr)
        sys.exit(1)
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if w != WIDTH or h != HEIGHT:
        print("Varování: obrázek {} je {}x{}; očekáváno {}x{}. Přesnímám.".format(path, w, h, WIDTH, HEIGHT), file=sys.stderr)
        im = im.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    pixels = list(im.getdata())
    return [rgb565(r, g, b) for r, g, b in pixels]

def write_array(f, name, arr, width_name, height_name):
    f.write("const uint16_t {} = {};\n".format(width_name, WIDTH))
    f.write("const uint16_t {} = {};\n\n".format(height_name, HEIGHT))
    f.write("const unsigned short {}[{}] PROGMEM = {{\n".format(name, PIXELS_HEX))
    for i in range(0, PIXELS, 16):
        chunk = arr[i:i+16]
        line = ", ".join("0x{:04X}".format(c) for c in chunk)
        comment_offset = 0x10 + (i // 16) * 0x10
        comment_count = i + len(chunk)
        f.write("  {},   // 0x{:04X} ({})\n".format(line, comment_offset, comment_count))
    f.write("};\n\n")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    out_path = os.path.join(project_root, "src", "media", "images_guardian_320_170.h")

    if len(sys.argv) >= 2 and sys.argv[1] == "--black":
        arr = [0x0000] * PIXELS
        with open(out_path, "w") as f:
            f.write("// Guardian display images (320x170)\n")
            f.write("// Vygenerováno: python3 png_to_guardian_header.py --black\n")
            f.write("// Nahraď: python3 png_to_guardian_header.py guardian_setup.png guardian_init.png\n\n")
            write_array(f, "guardianSetupScreen", arr, "guardianSetupWidth", "guardianSetupHeight")
            write_array(f, "guardianInitScreen", arr, "guardianInitWidth", "guardianInitHeight")
        print("Zapsán černý placeholder do", out_path)
        return

    setup_path = sys.argv[1] if len(sys.argv) > 1 else None
    init_path = sys.argv[2] if len(sys.argv) > 2 else None
    if not setup_path or not os.path.isfile(setup_path):
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    setup_arr = image_to_array(setup_path)
    init_arr = image_to_array(init_path) if init_path and os.path.isfile(init_path) else [0x0000] * PIXELS

    with open(out_path, "w") as f:
        f.write("// Guardian display images (320x170)\n")
        f.write("// Vygenerováno z:\n")
        f.write("//   setup (WiFi config): {}\n".format(setup_path))
        f.write("//   init (boot):         {}\n\n".format(init_path if init_path else "(černá)"))
        write_array(f, "guardianSetupScreen", setup_arr, "guardianSetupWidth", "guardianSetupHeight")
        write_array(f, "guardianInitScreen", init_arr, "guardianInitWidth", "guardianInitHeight")

    print("Zapsáno:", out_path)

if __name__ == "__main__":
    main()
