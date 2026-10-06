# Font DM Sans v Guardianu

Guardian může používat **DM Sans** (Regular) pro text na obrazovkách, aby odpovídal designu z Figmy.

## Jak přidat DM Sans do projektu

1. **Python a fonttools**
   ```bash
   pip install fonttools
   ```

2. **Stažení fontu**  
   Stáhni [DM Sans na Google Fonts](https://fonts.google.com/specimen/DM+Sans) (Download family), rozbal ZIP a zkopíruj **DMSans-Regular.ttf** do složky:
   ```
   tools/fonts/DMSans-Regular.ttf
   ```
   (Skript může zkusit stáhnout font sám; pokud to selže, použij ruční stažení.)

3. **Generování C hlavičky**
   Z kořene projektu:
   ```bash
   python3 tools/prepare_dmsans_font.py
   ```
   Tím se vytvoří **src/media/DMSans_subset.h** (pole `DMSans_Regular_subset` v PROGMEM). Skript udělá subset fontu (ASCII + Latin-1), aby byl soubor menší.

4. **Zapnutí DM Sans ve firmware**  
   Po vygenerování `src/media/DMSans_subset.h` se při sestavení s `GUARDIAN` automaticky načte DM Sans (včetně popisků na mining obrazovce). Znovu sestav a nahraj:
   ```bash
   pio run -e guardian -t upload
   ```
   Pokud hlavička neexistuje, firmware použije výchozí font (DigitalNumbers pro čísla, FreeSans pro popisky).

## Rozsah znaků

Subset obsahuje ASCII (U+0020–007F) a Latin-1 (U+00A0–00FF), tedy číslice, písmena, mezery a běžnou interpunkci včetně háčků a čárek. To stačí pro hashrate, block templates, čas, teplotu a popisky na mining obrazovce.

## Velikost

Subset fontu má typicky řádově desítky až nízké stovky KB. Celý TTF bez subsetu je větší; pokud nechceš instalovat fonttools, skript použije celý soubor (varování v konzoli).
