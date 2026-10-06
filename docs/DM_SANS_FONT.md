# DM Sans in Guardian

Guardian can use **DM Sans** (Regular) for on-screen text so it matches the Figma design.

DM Sans is Copyright 2014 The DM Sans Project Authors and is under the SIL Open Font License 1.1. The license is [fonts/OFL.txt](../fonts/OFL.txt). Keep that file with the font and with `src/media/DMSans_subset.h` whenever you redistribute them. The font is not sold on its own, and this subset stays under the same license.

## How to add DM Sans

1. **Python and fonttools**
   ```bash
   pip install fonttools
   ```

2. **Download the font**
   Download [DM Sans on Google Fonts](https://fonts.google.com/specimen/DM+Sans) (Download family), unzip it, and copy **DMSans-Regular.ttf** to:
   ```
   tools/fonts/DMSans-Regular.ttf
   ```
   The script can try to download the font itself. If that fails, download it by hand.

3. **Generate the C header**
   From the project root:
   ```bash
   python3 tools/prepare_dmsans_font.py
   ```
   This writes **src/media/DMSans_subset.h** (the `DMSans_Regular_subset` array in PROGMEM). The script subsets the font (ASCII + Latin-1) so the file stays smaller.

4. **Enable DM Sans in the firmware**
   After `src/media/DMSans_subset.h` exists, a `GUARDIAN` build loads DM Sans automatically, including the mining-screen labels. Build and flash again:
   ```bash
   pio run -e guardian -t upload
   ```
   If the header is missing, the firmware falls back to the default fonts (DigitalNumbers for figures, FreeSans for labels).

## Character coverage

The subset contains ASCII (U+0020-007F) and Latin-1 (U+00A0-00FF): digits, letters, spaces, and common punctuation, including Latin-1 accents. That covers hashrate, block templates, time, temperature, and the mining-screen labels.

## Size

The subset is typically tens to a few hundred KB. A full TTF without a subset is larger. If fonttools is not installed, the script embeds the whole file and prints a warning.
