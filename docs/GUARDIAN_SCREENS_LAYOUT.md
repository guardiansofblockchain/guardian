# Variable layout on the 4 screens (T-Display 320x170)

This document describes how the Miner, Miner Clock, Global Hash, and Price screens work in stock NerdMiner, and where each variable is drawn. Use it to prepare your own backgrounds in Figma, then adjust positions in code (or leave them) and switch the bitmap.

The `guardian` build already replaces these four screens in `src/drivers/displays/tDisplayDriver.cpp`. The coordinates below are the stock NerdMiner layout.

Resolution: **320 x 170** px. Coordinates are pixels. The origin (0,0) is the **top left** corner.

## 1. Fonts

The firmware draws text with **two systems**:

### A) OpenFontRender + the "Digital Numbers" font

- **File:** `src/media/myFonts.h`, the `DigitalNumbers` array (from the [Digital Numbers font](https://github.com/s-a/digital-numbers-font)).
- **Use:** numbers: hashrate, million hashes, block height, mining time, temperature, clock, and so on.
- **API:** `render.setFontSize(4-35)` (size in px), then:
  - **`render.drawString(text, x, y, color)`**: text **aligned left** (x,y is the top left).
  - **`render.rdrawString(text, x, y, color)`**: text **aligned right** (x is the right edge of the text, y is the baseline / top of the line).

### B) TFT_eSPI Free Fonts (GFXFF)

- **File:** `src/media/Free_Fonts.h` (and the fonts in the TFT_eSPI library).
- **Use:** strings: BTC price, HH:MM time, labels (half hour fee, difficulty, remaining blocks).
- **API:** `background.setFreeFont(...)`, `background.setTextDatum(...)`, `background.drawString(text, x, y, GFXFF)`.
- **Datum (alignment):**
  - **TL_DATUM**: x,y is top left.
  - **TR_DATUM**: x,y is top right.
  - **MC_DATUM**: x,y is the center.

Fonts used from Free_Fonts:

| Macro | Font |
|-------|------|
| FSSB9 | FreeSansBold 9pt: small bold text (price, time in the header) |
| FSS9 | FreeSans 9pt: labels (fee, difficulty) |
| FF23 | FreeSansBold 18pt: large time on the Clock screen |
| FF24 | FreeSansBold 24pt: large BTC price on the Price screen |
| FONT2 | built-in GLCD 2: small system text (remaining blocks) |

## 2. Data source (monitor.h)

Every value comes from:

- **`getMiningData(mElapsed)`** returns `mining_data` (Miner screen)
- **`getClockData(mElapsed)`** returns `clock_data` (Clock and Price screens)
- **`getCoinData(mElapsed)`** returns `coin_data` (Global Hash screen)

Structures, shortened:

```c
// mining_data: Miner
String completedShares, totalMHashes, totalKHashes, currentHashRate;
String templates, bestDiff, timeMining, valids, temp, currentTime;

// clock_data: Clock / Price
String completedShares, totalKHashes, currentHashRate, btcPrice, blockHeight, currentTime, currentDate;

// coin_data: Global Hash
String btcPrice, currentTime, halfHourFee, netwrokDifficulty, globalHashRate, blockHeight;
float progressPercent;   // 0-100
String remainingBlocks;
```

## 3. Screen 1: Miner (tDisplay_MinerScreen)

- **Background:** `MinerScreen` from `images_320_170.h` (320x170).
- **Code:** `src/drivers/displays/tDisplayDriver.cpp`, `tDisplay_MinerScreen()`.

| Variable | Data field | Font / size | Position (x, y) | Color | Alignment |
|----------|------------|-------------|-----------------|-------|-----------|
| Hashrate | currentHashRate | DigitalNumbers 35 | (118, 114) | BLACK | **right** (rdrawString) |
| Million hashes | totalMHashes | DigitalNumbers 18 | (268, 138) | BLACK | **right** |
| Block templates | templates | DigitalNumbers 18 | (186, 20) | 0xDEDB | left |
| Best diff | bestDiff | DigitalNumbers 18 | (186, 48) | 0xDEDB | left |
| 32bit shares | completedShares | DigitalNumbers 18 | (186, 76) | 0xDEDB | left |
| Mining time | timeMining | DigitalNumbers 14 | (315, 104) | 0xDEDB | **right** |
| Valid blocks | valids | DigitalNumbers 24 | (285, 56) | 0xDEDB | left |
| Temperature | temp | DigitalNumbers 10 | (239, 1) | BLACK | **right** |
| (small glyph) | "0" | DigitalNumbers 4 | (244, 3) | BLACK | **right** |
| Clock | currentTime | DigitalNumbers 10 | (286, 1) | BLACK | **right** |

Note: the Miner screen also calls `printPoolData()` or `printMemPoolFees()`. In `tDisplayDriver.cpp` for T_DISPLAY that may draw nothing else. It depends on the implementation.

## 4. Screen 2: Miner Clock (tDisplay_ClockScreen)

- **Background:** `minerClockScreen` from `images_320_170.h`.
- **Code:** `tDisplay_ClockScreen()`.

| Variable | Data field | Font / size | Position (x, y) | Color | Alignment |
|----------|------------|-------------|-----------------|-------|-----------|
| Hashrate | currentHashRate | DigitalNumbers 25 | (94, 129) | BLACK | **right** |
| BTC price | btcPrice | FSSB9 (FreeSansBold 9pt) | (202, 3) | BLACK | TL_DATUM |
| Block height | blockHeight | DigitalNumbers 18 | (254, 140) | BLACK | **right** |
| Clock | currentTime | FF23 (FreeSansBold 18pt), size 2 | (130, 50) | 0xDEDB / BLACK | GFXFF (default datum) |

## 5. Screen 3: Global Hash (tDisplay_GlobalHashScreen)

- **Background:** `globalHashScreen` from `images_320_170.h`.
- **Code:** `tDisplay_GlobalHashScreen()`.

| Variable | Data field | Font / size | Position (x, y) | Color | Alignment |
|----------|------------|-------------|-----------------|-------|-----------|
| BTC price | btcPrice | FSSB9 | (198, 3) | BLACK | TL_DATUM |
| Clock | currentTime | FSSB9 | (268, 3) | BLACK | TL_DATUM |
| Last pool block | halfHourFee | FSS9, TR_DATUM | (302, 52) | 0x9C92 | right |
| Difficulty | netwrokDifficulty | FSS9, TR_DATUM | (302, 88) | 0x9C92 | right |
| Global hashrate | globalHashRate | DigitalNumbers 17 | (274, 145) | BLACK | **right** |
| Block height | blockHeight | DigitalNumbers 28 | (140, 104) | 0xDEDB | **right** |
| Progress bar | progressPercent | fillRect(2, 149, x2, 168, 0xDEDB), x2 = 2 + 138*progressPercent/100 | | | |
| Remaining blocks | remainingBlocks | FONT2, MC_DATUM | (72, 159) | BLACK | center |

## 6. Screen 4: Price (tDisplay_BTCprice)

- **Background:** `priceScreen` from `images_320_170.h`.
- **Code:** `tDisplay_BTCprice()`.

| Variable | Data field | Font / size | Position (x, y) | Color | Alignment |
|----------|------------|-------------|-----------------|-------|-----------|
| Hashrate | currentHashRate | DigitalNumbers 25 | (94, 129) | BLACK | **right** |
| Block height | blockHeight | DigitalNumbers 18 | (254, 138) | WHITE | **right** |
| Clock | currentTime | FSSB9 | (222, 3) | BLACK | TL_DATUM |
| BTC price | btcPrice | FF24 (FreeSansBold 24pt), TR_DATUM | (300, 58) | 0xDEDB / BLACK | right |

## 7. Custom Figma background

1. **Background**
   Export four 320x170 images from Figma (miner, minerClock, globalHash, price). Add them to the project (for example as Guardian bitmaps) and, in the four functions in `tDisplayDriver.cpp`, change the image source:
   - `MinerScreen` becomes your bitmap (for example from `images_guardian_320_170.h`),
   - `minerClockScreen` becomes your minerClock,
   - and the same for the other two.

2. **Text positions**
   If the number and text blocks sit somewhere else in Figma, change the `(x, y)` coordinates of `drawString` / `rdrawString` in `tDisplayDriver.cpp` using the tables above.
   - **rdrawString(x, y)**: the right edge of the text is at x, the baseline is about y.
   - **drawString(x, y)** with TL_DATUM: top left at (x,y).
   - TR_DATUM: top right at (x,y).
   - MC_DATUM: center of the text at (x,y).

3. **Colors**
   Colors are RGB565 (hex). Common values: `TFT_BLACK`, `TFT_WHITE`, `0xDEDB` (light gray), `0x9C92` (another gray). You can switch them to Figma colors converted to RGB565.

4. **Font sizes**
   `render.setFontSize(4-35)` for DigitalNumbers. Free Fonts depend on the chosen font and `setTextSize(1)` or `2`. Shrink or grow them to fit the layout.

5. **Progress bar (Global Hash only)**
   The rectangle runs from (2, 149) to (2 + 138*progressPercent/100, 168). To move or resize it, change those constants in `tDisplay_GlobalHashScreen()`.

Guardian-specific bitmaps and coordinates belong under `#ifdef GUARDIAN` in copies of these four functions.

## 8. Guardian checklist for a custom background

- **Display:** `T_DISPLAY` (LilyGo T-Display S3), driver in `src/drivers/displays/tDisplayDriver.cpp`.
- **Four functions to edit:**
  `tDisplay_MinerScreen`, `tDisplay_ClockScreen`, `tDisplay_GlobalHashScreen`, `tDisplay_BTCprice`.
- In each function:
  1. `background.pushImage(0, 0, width, height, SCREEN_ARRAY)`: replace `SCREEN_ARRAY` with your array from your `.h` (for example `images_guardian_320_170.h`).
  2. Every `render.setFontSize(...)` and `drawString` / `rdrawString` with (x, y): match the Figma layout.
  3. Optionally change colors (RGB565) and the width and height of the Global Hash progress bar.
- The data does not change. It still comes from `getMiningData()`, `getClockData()`, and `getCoinData()` in `monitor.cpp` / `monitor.h`.
