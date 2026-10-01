# CLAUDE.md — Agent notes (M5Stack Tab5 app-loading)

Read this first when working in this repo. It captures how this fork of the
bmorcelli Launcher is used as the **hub for loading multiple full-firmware apps
onto an M5Stack Tab5 (ESP32-P4)**, plus the non-obvious hardware quirks found
the hard way.

## TL;DR

- The Launcher (this repo) is a **chain-loader**: it installs a full-flash
  factory `.bin`, parses its embedded partition table at `0x8000`, extracts the
  app + data partitions, dynamically repartitions the 16 MB flash, sets OTA boot,
  and reboots into the app. Return to the Launcher: reset, then **tap the splash**
  (the "Press the button…" text is serial-only, not on screen) — or set
  **CFG → `[x] Boot to Launcher`** (`bootToApp` NVS flag) so it never auto-boots.
- Build/flash env: `pio run -e m5stack-tab5` → `Launcher-m5stack-tab5.bin`, flash
  at `0x0`. PlatformIO + pioarduino; **no standalone ESP-IDF needed for the
  Launcher**.
- Target device: `/dev/ttyACM0` = ESP32-P4 USB-Serial-JTAG (`303a:1001`),
  16 MB flash, ~3.8 GB FAT32 SD, A164 keyboard.

## Hardware / host setup

- User is **not** in `dialout`; a udev rule (`/etc/udev/rules.d/99-esp-tab5.rules`,
  `303a:1001 MODE=0666`) fixes port perms, but after a USB re-enum you may still
  need `sudo chmod 666 /dev/ttyACM0`.
- esptool: use the PlatformIO venv — `~/.platformio/penv/bin/python -m esptool`
  (v5, hyphenated subcommands like `write-flash`). The ESP-IDF esptool is v4
  (underscored `merge_bin`).
- **USB-JTAG writes are reliable; large reads are NOT** — `read-flash 0x0
  0x1000000` fails with "Packet content transfer stopped". Don't rely on a full
  flash backup over USB-JTAG.
- `esp-emu` (`~/.local/bin/esp-emu`) boots P4 images but does **not** model the
  Tab5 DSI/PSRAM (floods "Bus fault … unmapped 0x298…"), so it can't boot these
  display apps. Hardware is the verification surface.

## Loading apps — the workflow

Two ways to get app `.bin`s onto the device; both stream over the 115200 console
with a `READY` → chunked-`ACK` → `OK` protocol (host helpers were written under
`repos/_artifacts/`: `flash_install.py`, `sd_put.py`, `serial_*.py`).

1. **SD library (preferred).** Put full-flash factory `.bin`s at the SD root; the
   Launcher's **SD** tile lists and installs them (with data partitions). Populate
   the SD headlessly with the **`sdput`** serial command (FATFS write — clean) or
   `sdinstall` to install directly. Avoid USB-MSC for writing (see SD quirk).
2. **`flash firmware <name> <size>` serial install.** Streams an **app-only** image
   (0xE9 magic) straight into a new OTA app partition and boots it — no SD, no data
   partitions. ~126 KB/s. Large streams can stall around 115200 CDC limits (a 9 MB
   AIO stream stalled ~1.8 MB; 3–5 MB apps stream fine). Prefer SD for big apps.

Serial console commands (`help`): `nav <Next/Prev/Sel/Esc>Press` (drive the UI
headlessly), `partitions`, `partition …`, `flash firmware <name> <size>`,
`sdput <sdpath> <size>`, `sdinstall <sdpath>`, `wifi …`, `sdio …`, `version`.
Menu tile order: **SD, OTA, WUI, USB(→MSC), PMan, CFG**.

## ⚠ SD card quirk (bit me hard)

- USB-MSC (**USB** tile) exposes the SD as a host block device (`M5Stack Launcher
  SD`). Great for bulk copy, **but raw-block access + an unclean eject latches the
  SD card into a bad state**: subsequent boots print "Failed to mount SDCARD".
- A P4 **warm reset does NOT recover it** — the card stays powered (Tab5 has a
  battery), so it needs a real **power cycle** (off/on, or reseat the SD). An
  esptool `--after hard-reset` sometimes recovers it; the SW `reboot` command does
  not. Do **not** remotely `powerOff()` (`M5.Power.powerOff()`) — no guaranteed
  USB auto-wake → lockout risk.
- Prefer **`sdput`/`sdinstall` over MSC** for writing — FATFS path, no wedge.
- Improvement made here: `setupSdCard()` now retries the SPI mount 6× with a
  settle delay (was a single `SD.begin` + a `pdTICKS_TO_MS` bug), which helps the
  intermittent warm-boot mount failures — but a truly wedged card still needs a
  power cycle.

## A164 keyboard

The Tab5 keyboard is the **M5Stack A164** — STM32F030, 70-key matrix, **I²C 0x6D**
on ExtPort1 (SDA=GPIO0, SCL=GPIO1, INT=GPIO50), modes normal/HID/character. The
Launcher drives it (`boards/m5stack-tab5/interface.cpp`, `UnitTab5Keyboard`,
Normal mode); boot shows `Tab5 keyboard ready (fw 0x01)`. Known gap: the A164 is
**not polled during the early boot splash window**, so only *touch* enters the
Launcher there (keys work everywhere else). With `[x] Boot to Launcher` this is
moot.

## The apps (priority order)

- **Term_I** (`repos/Term_I`, ESP-IDF 5.4.2, P4+C6) — renamed from "Slave I"
  (all UI must read `Term_I`/`TERM_I`). Upstream **does not build** against stock
  esp_hosted 1.4.0 (missing `esp_hosted_send_priv_command`); fix documented in that
  repo's build notes / Claude memory. Reads the A164 via 0x6D in
  `hal_usb.cpp` (`tab5_keyboard_task`); the `keypad_scanner_tca8418` component is
  dead code. Validated: installs+boots via the Launcher, `M5 keyboard detected`,
  `C6 provisioned OK`.
- **Tab5_AIO** — prebuilt full-flash factory image; Launcher extracts the real
  ~9 MB app (not the 15.4 MB partition — `effectiveSdAppSize()` reads the image
  header). No data partition needed (NVS + SD).
- **tab5_loadout** — ESP-IDF; factory + app-only bins in the release. Validated
  install+boot via the Launcher.
- **HeathenHawk-Talon5** — deprioritized by the user; needs platformio.ini fixes
  (macOS `-I/-L`, missing `lib_deps`, `src/`-prefixed includes) and has **no** A164
  driver.

## Gotchas

- Building the Launcher runs `support_files/clang_format.py` as a pre-script, which
  can reformat sources in the working tree — check `git diff` before committing.
- `*.bin`, `.pio`, `build`, `include/webFiles.h` are gitignored — don't force-add.
- Tab5 board env lives in `boards/m5stack-tab5/platformio.ini`; it inherits
  platform/packages from root `[env]`.
