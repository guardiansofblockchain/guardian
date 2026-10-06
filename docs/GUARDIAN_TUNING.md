# Guardian – vytížení chipu a těžba

## Co je nastaveno pro vyšší vytížení

Při sestavení s **GUARDIAN** (env `guardian`) platí:

1. **CPU 240 MHz**  
   V `setup()` se pro ESP32-S3 volá `setCpuFrequencyMhz(240)` – maximální frekvence pro tento chip. Po startu uvidíš v sériovém výstupu řádek `CPU: 240 MHz`.  
   *Kde:* `src/NerdMinerV2.ino.cpp` (podmínka `CONFIG_IDF_TARGET_ESP32S3` a `NERDMINERV2`).

2. **Větší mining batch (Guardian), cca +20 %**  
   V `src/mining.cpp` jsou pro `GUARDIAN` nastaveny mírně větší joby (~+20 % oproti výchozím):
   - **NONCE_PER_JOB_SW** ≈ 4915 (výchozí 4096)
   - **NONCE_PER_JOB_HW** ≈ 19660 (výchozí 16×1024)  

   Každý mining task dělá o trochu víc práce v jednom kuse, méně času se tráví přepínáním a frontou – chip zůstane víc vytížený, hashrate i teplota mohou mírně stoupnout.

## Další možnosti (ruční úpravy)

- **Frekvence:** Na ESP32-S3 je 240 MHz maximum; nižší hodnoty (160, 80 MHz) by snížily spotřebu a teplotu.
- **Batch:** Pro větší vytížení můžeš u `#ifdef GUARDIAN` v `src/mining.cpp` dál zvedat konstanty (např. 8192 / 32*1024 nebo víc). Příliš velké joby můžou zvyšovat latenci odevzdávání shareů.
- **Platformio:** V `platformio.ini` má env `guardian` už `board_build.f_cpu = 240000000L` – build je cílen na 240 MHz; volání `setCpuFrequencyMhz(240)` v kódu to jen ověří/zajistí za běhu.

Teplota kolem 35 °C při 250 KH/s je u ESP32-S3 s 240 MHz v pohodě; po těchto úpravách může hashrate i teplota mírně stoupnout.
