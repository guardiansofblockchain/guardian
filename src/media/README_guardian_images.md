# Guardian display images

A **GUARDIAN=1** build uses these bitmaps. The rest of the firmware logic stays the same.

## Files

| File | Where it shows |
|------|----------------|
| **guardian_setup.png** | Setup screen: Wi-Fi configuration (SSID Guardian, password GOB, WAITING CONFIG). Stays up until Wi-Fi is configured. |
| **guardian_init.png** | Init screen: shown briefly at boot. |
| **tools/figma/miner_bg_320.png** | Mining screen background (Figma 752:609). The firmware draws the live numbers. |
| **tools/figma/clock_bg_320.png** | Clock (Figma 752:976). The firmware draws time, price, hashrate, and block height. |
| **tools/figma/price_bg_320.png** | BTC price (Figma 752:787). The firmware draws the amount, hashrate, and block height. |
| **tools/figma/global_bg_320.png** | Global stats (Figma 752:1239). The firmware draws difficulty, fee, block, blocks to the halving, and hashrate. |

Each image must be **320 x 170** pixels.

## Commands (from the project root)

**1. Convert images to a header**

Put both PNGs in the project root (next to `platformio.ini`), then:

```bash
# if the venv and Pillow are not set up yet:
python3 -m venv .venv
source .venv/bin/activate
pip install Pillow

# convert (1st = setup, 2nd = init):
python3 tools/png_to_guardian_header.py guardian_setup.png guardian_init.png
```

**2. Build and flash**

```bash
pio run -e guardian -t upload
```

## Reverse export: .h to PNG

From the current `images_guardian_320_170.h` you can write PNGs back out (for example to edit them):

```bash
python3 tools/header_to_png.py
```

That creates `guardian_setup_from_h.png` and `guardian_init_from_h.png` in the project root. For `images_320_170.h` (stock NerdMiner) pass the header path and `--out-dir` (`python3 tools/header_to_png.py --help`, or the docstring in the script).

## Summary

- **guardian_setup.png** (1st argument): screen with the Wi-Fi name and password (setup).
- **guardian_init.png** (2nd argument): screen at boot (init).
- While Guardian is waiting for configuration, the monitor does not overwrite the setup screen with the mining UI.
