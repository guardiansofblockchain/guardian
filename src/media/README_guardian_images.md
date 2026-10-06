# Guardian – obrázky pro displej

Při sestavení s **GUARDIAN=1** se používají jen tyto bitmapy. Ostatní logika zůstává beze změny.

## Soubory a význam

| Soubor | Kde se zobrazí |
|--------|----------------|
| **guardian_setup.png** | Setup obrazovka – WiFi konfigurace (jméno sítě Guardian, heslo GOB, WAITING CONFIG). Zobrazuje se, dokud někdo nenastaví WiFi. |
| **guardian_init.png** | Init obrazovka – zobrazí se krátce při startu zařízení (boot). |
| **tools/figma/miner_bg_320.png** | Pozadí mining obrazovky (Figma 752:609). Živá čísla kreslí firmware. |
| **tools/figma/clock_bg_320.png** | Hodiny (Figma 752:976). Čas, cena, hashrate a výška bloku kreslí firmware. |
| **tools/figma/price_bg_320.png** | Cena BTC (Figma 752:787). Částka, hashrate a výška bloku kreslí firmware. |
| **tools/figma/global_bg_320.png** | Global stats (Figma 752:1239). Obtížnost, fee, blok, zbytek do halvingu a hashrate kreslí firmware. |

Oba obrázky musí být **320 × 170 pixelů**.

---

## Příkazy (od kořene projektu)

**1. Převod obrázků do hlavičky**  
Oba PNG dej do kořene projektu (vedle `platformio.ini`), pak:

```bash
# pokud ještě nemáš aktivované venv a Pillow:
python3 -m venv .venv
source .venv/bin/activate
pip install Pillow

# převod (1. = setup, 2. = init):
python3 tools/png_to_guardian_header.py guardian_setup.png guardian_init.png
```

**2. Build a nahrání na zařízení**

```bash
pio run -e guardian -t upload
```

---

## Zpětný export .h → PNG (reverse)

Z aktuálního `images_guardian_320_170.h` můžeš zpětně vyrobit PNG (např. pro úpravy v editoru):

```bash
python3 tools/header_to_png.py
```

V kořeni projektu se vytvoří `guardian_setup_from_h.png` a `guardian_init_from_h.png`. Ze souboru `images_320_170.h` (NerdMiner) zkusíš stejný skript s cestou k souboru a `--out-dir` (viz `python3 tools/header_to_png.py --help` / docstring ve skriptu).

---

## Shrnutí

- **guardian_setup.png** (1. argument) → obrazovka s WiFi jménem a heslem (setup).
- **guardian_init.png** (2. argument) → obrazovka při startu (init).
- Při čekání na konfiguraci (Guardian) monitor nepřepisuje setup obrazovku mining UI.
