# Guardian: chip load and mining

## What is set for a higher load

A **GUARDIAN** build (env `guardian`) does two things:

1. **CPU 240 MHz**
   In `setup()`, ESP32-S3 calls `setCpuFrequencyMhz(240)`, the maximum for this chip. After boot the serial log prints `CPU: 240 MHz`.
   *Where:* `src/NerdMinerV2.ino.cpp` (when `CONFIG_IDF_TARGET_ESP32S3` and `NERDMINERV2` are set).

2. **Larger mining batch (Guardian), about +20%**
   In `src/mining.cpp`, `GUARDIAN` uses slightly larger jobs (about +20% over the defaults):
   - **NONCE_PER_JOB_SW** about 4915 (default 4096)
   - **NONCE_PER_JOB_HW** about 19660 (default 16*1024)

   Each mining task does a bit more work in one piece, so less time is spent switching and queueing. The chip stays busier, and hashrate and temperature can rise a little.

## Further manual changes

- **Frequency:** 240 MHz is the ESP32-S3 maximum. Lower values (160, 80 MHz) reduce power and temperature.
- **Batch:** For a higher load, raise the constants under `#ifdef GUARDIAN` in `src/mining.cpp` (for example 8192 / 32*1024 or more). Jobs that are too large can add latency before a share is submitted.
- **PlatformIO:** The `guardian` env already sets `board_build.f_cpu = 240000000L`, so the build targets 240 MHz. The `setCpuFrequencyMhz(240)` call only confirms that at runtime.

Around 35 C at 250 KH/s is fine for an ESP32-S3 at 240 MHz. After these changes, hashrate and temperature can rise a little.
