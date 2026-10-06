# Layout proměnných na 4 obrazovkách (T-Display 320×170)

Tento dokument popisuje, **jak fungují** obrazovky Miner, Miner Clock, Global Hash a Price v NerdMineru a **kde se vykreslují které proměnné**. Slouží k tomu, abys mohl v Figmě připravit vlastní pozadí a v kódu jen upravit pozice (nebo je nechat) a přepnout bitmapu na svou.

Rozlišení: **320 × 170** px. Souřadnice jsou v pixelech, počátek (0,0) je **levý horní** roh.

---

## 1. Fonty

Firmware používá **dva systémy** kreslení textu:

### A) OpenFontRender + font „Digital Numbers“

- **Soubor:** `src/media/myFonts.h` – pole `DigitalNumbers` (odkaz na [Digital Numbers font](https://github.com/s-a/digital-numbers-font)).
- **Použití:** čísla – hashrate, milion hashes, block height, čas mining, teplota, hodiny atd.
- **API:** `render.setFontSize(4–35)` (velikost v px), pak:
  - **`render.drawString(text, x, y, color)`** – text **zarovnaný vlevo** (x,y = levý horní roh).
  - **`render.rdrawString(text, x, y, color)`** – text **zarovnaný vpravo** (x = pravý okraj textu, y = baseline/horní řádek).

### B) TFT_eSPI Free Fonts (GFXFF)

- **Soubor:** `src/media/Free_Fonts.h` (a fonty v knihovně TFT_eSPI).
- **Použití:** řetězce – cena BTC, čas HH:MM, popisky (half hour fee, difficulty, remaining blocks).
- **API:** `background.setFreeFont(...)`, `background.setTextDatum(...)`, `background.drawString(text, x, y, GFXFF)`.
- **Datum (zarovnání):**
  - **TL_DATUM** – x,y = top-left.
  - **TR_DATUM** – x,y = top-right.
  - **MC_DATUM** – x,y = middle-center.

Používané fonty z Free_Fonts:

| Makro | Font |
|-------|------|
| FSSB9  | FreeSansBold 9pt  – malý tučný text (cena, čas v hlavičce) |
| FSS9   | FreeSans 9pt      – popisky (fee, difficulty) |
| FF23   | FreeSansBold 18pt – velký čas na Clock screen |
| FF24   | FreeSansBold 24pt – velká cena BTC na Price screen |
| FONT2  | vestavěný GLCD 2  – malý systémový text (remaining blocks) |

---

## 2. Zdroj dat (monitor.h)

Všechny hodnoty pocházejí z:

- **`getMiningData(mElapsed)`** → `mining_data` (Miner screen)
- **`getClockData(mElapsed)`** → `clock_data` (Clock + Price screen)
- **`getCoinData(mElapsed)`** → `coin_data` (Global Hash screen)

Struktury (zkráceně):

```c
// mining_data – Miner
String completedShares, totalMHashes, totalKHashes, currentHashRate;
String templates, bestDiff, timeMining, valids, temp, currentTime;

// clock_data – Clock / Price
String completedShares, totalKHashes, currentHashRate, btcPrice, blockHeight, currentTime, currentDate;

// coin_data – Global Hash
String btcPrice, currentTime, halfHourFee, netwrokDifficulty, globalHashRate, blockHeight;
float progressPercent;   // 0–100
String remainingBlocks;
```

---

## 3. Obrazovka 1: Miner (tDisplay_MinerScreen)

- **Pozadí:** `MinerScreen` z `images_320_170.h` (320×170).
- **Kód:** `src/drivers/displays/tDisplayDriver.cpp` → `tDisplay_MinerScreen()`.

| Proměnná        | Data pole            | Font / velikost | Pozice (x, y) | Barva   | Zarovnání |
|------------------|----------------------|-----------------|---------------|---------|-----------|
| Hashrate         | currentHashRate      | DigitalNumbers 35 | (118, 114)  | BLACK   | **right** (rdrawString) |
| Million hashes   | totalMHashes         | DigitalNumbers 18 | (268, 138)  | BLACK   | **right** |
| Block templates  | templates            | DigitalNumbers 18 | (186, 20)   | 0xDEDB  | left |
| Best diff        | bestDiff             | DigitalNumbers 18 | (186, 48)   | 0xDEDB  | left |
| 32bit shares     | completedShares      | DigitalNumbers 18 | (186, 76)   | 0xDEDB  | left |
| Mining time      | timeMining           | DigitalNumbers 14 | (315, 104)  | 0xDEDB  | **right** |
| Valid blocks     | valids               | DigitalNumbers 24 | (285, 56)   | 0xDEDB  | left |
| Teplota          | temp                 | DigitalNumbers 10 | (239, 1)    | BLACK   | **right** |
| (malý znak)      | "0"                  | DigitalNumbers 4  | (244, 3)    | BLACK   | **right** |
| Hodiny           | currentTime          | DigitalNumbers 10 | (286, 1)    | BLACK   | **right** |

Pozn.: Na Miner screen se ještě volá `printPoolData()` nebo `printMemPoolFees()` – v tDisplayDriver.cpp pro T_DISPLAY to může být bez dalšího kreslení (záleží na implementaci).

---

## 4. Obrazovka 2: Miner Clock (tDisplay_ClockScreen)

- **Pozadí:** `minerClockScreen` z `images_320_170.h`.
- **Kód:** `tDisplay_ClockScreen()`.

| Proměnná    | Data pole       | Font / velikost | Pozice (x, y) | Barva  | Zarovnání |
|-------------|-----------------|-----------------|---------------|--------|-----------|
| Hashrate    | currentHashRate | DigitalNumbers 25 | (94, 129)  | BLACK  | **right** |
| BTC cena    | btcPrice        | FSSB9 (FreeSansBold 9pt) | (202, 3) | BLACK | TL_DATUM |
| Block height| blockHeight     | DigitalNumbers 18 | (254, 140) | BLACK | **right** |
| Hodiny      | currentTime     | FF23 (FreeSansBold 18pt), size 2 | (130, 50) | 0xDEDB / BLACK | GFXFF (default datum) |

---

## 5. Obrazovka 3: Global Hash (tDisplay_GlobalHashScreen)

- **Pozadí:** `globalHashScreen` z `images_320_170.h`.
- **Kód:** `tDisplay_GlobalHashScreen()`.

| Proměnná         | Data pole           | Font / velikost | Pozice (x, y) | Barva  | Zarovnání |
|------------------|---------------------|-----------------|---------------|--------|-----------|
| BTC cena         | btcPrice            | FSSB9           | (198, 3)      | BLACK  | TL_DATUM  |
| Hodiny           | currentTime         | FSSB9           | (268, 3)      | BLACK  | TL_DATUM  |
| Last pool block  | halfHourFee         | FSS9, TR_DATUM  | (302, 52)     | 0x9C92 | right     |
| Difficulty       | netwrokDifficulty   | FSS9, TR_DATUM  | (302, 88)     | 0x9C92 | right     |
| Global hashrate  | globalHashRate      | DigitalNumbers 17 | (274, 145)  | BLACK  | **right** |
| Block height     | blockHeight         | DigitalNumbers 28 | (140, 104)  | 0xDEDB | **right** |
| Progress bar     | progressPercent     | fillRect(2, 149, x2, 168, 0xDEDB), x2 = 2 + 138*progressPercent/100 |  |  |  |
| Remaining blocks | remainingBlocks     | FONT2, MC_DATUM | (72, 159)     | BLACK  | center    |

---

## 6. Obrazovka 4: Price (tDisplay_BTCprice)

- **Pozadí:** `priceScreen` z `images_320_170.h`.
- **Kód:** `tDisplay_BTCprice()`.

| Proměnná     | Data pole       | Font / velikost | Pozice (x, y) | Barva  | Zarovnání |
|--------------|-----------------|-----------------|---------------|--------|-----------|
| Hashrate     | currentHashRate | DigitalNumbers 25 | (94, 129)  | BLACK  | **right** |
| Block height | blockHeight     | DigitalNumbers 18 | (254, 138) | WHITE  | **right** |
| Hodiny       | currentTime     | FSSB9           | (222, 3)      | BLACK  | TL_DATUM  |
| BTC cena     | btcPrice        | FF24 (FreeSansBold 24pt), TR_DATUM | (300, 58) | 0xDEDB / BLACK | right |

---

## 7. Jak to „hacknout“ – vlastní pozadí z Figmy

1. **Pozadí**  
   V Figmě exportuj 4 obrázky 320×170 (miner, minerClock, globalHash, price). Přidej je do projektu (např. jako Guardian bitmapy) a v `tDisplayDriver.cpp` u těchto čtyř funkcí změň zdroj obrázku:
   - místo `MinerScreen` → tvůj bitmap (např. z `images_guardian_320_170.h`),
   - místo `minerClockScreen` → tvůj minerClock,
   - atd.

2. **Pozice textu**  
   Pokud máš v Figmě jiné umístění bloků pro čísla/text, uprav v `tDisplayDriver.cpp` příslušné souřadnice `(x, y)` u `drawString` / `rdrawString` podle tabulek výše.  
   - **rdrawString(x, y)** = pravý okraj textu je v x, baseline zhruba v y.  
   - **drawString(x, y)** s TL_DATUM = levý horní roh v (x,y).  
   - TR_DATUM = pravý horní roh v (x,y).  
   - MC_DATUM = střed textu v (x,y).

3. **Barvy**  
   Barvy jsou RGB565 (hex). Běžné: `TFT_BLACK`, `TFT_WHITE`, `0xDEDB` (světle šedá), `0x9C92` (jiná šedá). Můžeš je změnit na barvy z Figmy (převeď na RGB565).

4. **Velikosti fontů**  
   `render.setFontSize(4–35)` u DigitalNumbers; u Free Fonts záleží na zvoleném fontu a `setTextSize(1)` nebo `2`. Můžeš zmenšit/zvětšit podle layoutu.

5. **Progress bar (jen Global Hash)**  
   Obdélník od (2, 149) do (2 + 138*progressPercent/100, 168). Pokud chceš jinou pozici/šířku, uprav tyto konstanty v `tDisplay_GlobalHashScreen()`.

Když budeš chtít, můžeme konkrétně pro Guardian přidat do `#ifdef GUARDIAN` vlastní bitmapy a jen zkopírovat tyto čtyři funkce s tvými souřadnicemi a barvami.

---

## 8. Guardian – rychlý checklist pro vlastní pozadí

- **Displej:** `T_DISPLAY` (LilyGo T-Display S3), driver v `src/drivers/displays/tDisplayDriver.cpp`.
- **Čtyři funkce k úpravě:**  
  `tDisplay_MinerScreen`, `tDisplay_ClockScreen`, `tDisplay_GlobalHashScreen`, `tDisplay_BTCprice`.
- V každé funkci:
  1. `background.pushImage(0, 0, width, height, SCREEN_ARRAY)` – nahraď `SCREEN_ARRAY` svým polem z tvého `.h` (např. z `images_guardian_320_170.h`).
  2. Všechny `render.setFontSize(...)` a `drawString` / `rdrawString` s (x, y) – uprav podle layoutu z Figmy.
  3. Volitelně změň barvy (RGB565) a šířku/výšku progress baru na Global Hash.
- Data se nemění – stále `getMiningData()`, `getClockData()`, `getCoinData()` z `monitor.cpp` / `monitor.h`.
