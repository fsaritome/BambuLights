# ESP32 Bambu Light Tower

An industrial-style stacked status tower for Bambu Lab printers, driven by an
ESP32 and addressable LEDs. The printer's state is read over its local MQTT
stream and shown on the tower, so you can tell what the machine is doing from
across the room.

This is a fork of **[judge2005/BambuLights](https://github.com/judge2005/BambuLights)**
— see [Credits](#credits).

## Status

Working today on an ESP32-WROOM-32:

- Reads printer state over local MQTT (TLS, port 8883)
- Colour, pattern and pulse rate configurable per state in a web UI
- Runtime-configurable LED count and GRB/RGB ordering
- OTA firmware and web-GUI updates once flashed
- Home Assistant auto-registration

The multi-tier tower described in [Planned](#planned) is not built yet; the
current firmware drives a single LED strip that shows one state at a time.

## What this fork changes

| Change | Why |
|---|---|
| **`wroom32` build environment** | Upstream ships `pico32`, `s3` and `c3`. The classic WROOM-32 is the same silicon as the PICO-D4 but needs its own board target. |
| **HTTPS dependencies** | Upstream's `pico32` env fetches four libraries over `git+ssh://`, which only resolves with the maintainer's own SSH keys. |
| **Larger app partitions** (`partitions_wroom32.csv`) | The firmware links to ~1.52 MB and overflows the stock 1.25 MB OTA slots. Space is taken from the oversized filesystem partition. OTA is preserved. |
| **HMS ignore-list** | Some faults latch until you intervene at the printer and say nothing about the print — a full SD card, for example. Previously these pinned the lights to the warning colour indefinitely. |
| **HMS severity precedence** | The fault loop kept whichever entry came *last* in the array rather than the most severe one, so a trailing benign fault could mask a serious one. |
| **Unknown HMS codes** are formatted as `HMS_XXXX_XXXX_XXXX_XXXX` | Previously `std::map::operator[]` silently inserted an empty entry on every unknown code. The new form matches how the Bambu wiki indexes them. |
| **Lamp state on the Info page** | The web UI was configuration-only, so there was no way to see what the lights were reacting to without a USB cable and a serial monitor. Now shows lamp state, printer state and the active fault in plain English. |
| **`local_secrets.h`** | Optional, gitignored way to bake printer details into a build without committing them. |
| **`flash.ps1`** | One-shot flash of every image in a single `esptool` call. |

## Hardware

- **ESP32-WROOM-32** devkit (any classic ESP32 module works)
- **WS2812B** addressable LEDs (GRB), or APA106 (RGB) — selectable at runtime
- **Level shifter** on the data line (74AHCT125, CD40109B or similar)

Wiring:

| Signal | Pin |
|---|---|
| LED data | **GPIO14** (set by `-D LED_PIN` in `platformio.ini`) |
| LED power | 5 V — most devkits can supply this for a short strip |
| Ground | common between ESP32 and strip |

Recommended: a 300–500 Ω resistor in series on the data line and a 1000 µF
capacitor across the strip's power input.

> **Note on GPIO14:** the ESP32 emits a brief pulse on this pin during boot, so
> expect a short flicker at power-up. Any free GPIO works; change `LED_PIN`.

## Printer setup

The firmware reads the printer's **local** MQTT stream. Contrary to a common
assumption, this does **not** require LAN-only Mode or Developer Mode on
current firmware — Bambu's Authorization Control System restricts *write*
access, while status reporting is unaffected. A cloud-connected printer works.

You need three things from the printer:

| | Where |
|---|---|
| IP address | Settings → Network |
| LAN Access Code | Settings → Network (rotates if you toggle LAN mode) |
| Serial number | Settings → Device |

If local MQTT is refused, enable LAN-only Mode and then Developer Mode
(Settings → LAN Mode, **power-cycle**, then the Developer Mode toggle appears).
X1 series needs firmware 01.08.03.00 or later for that toggle to exist at all.

## Build

Requires [PlatformIO](https://platformio.org/). Node is optional — without it
the build falls back to the pre-built web assets in `data/`.

```bash
pio run -e wroom32                      # firmware
pio run -e wroom32 --target buildfs     # web GUI
```

Re-run `buildfs` after editing anything under `web/src/`, or the device will
keep serving the old interface.

## Flash

### First time (USB)

```powershell
.\flash.ps1 -Port COM5
```

Writes bootloader, partition table, otadata, firmware and filesystem in one
call. If it reports `Wrong boot mode detected` or `No serial data received`,
put the board into download mode by hand:

1. Hold **BOOT**
2. Tap **EN**
3. Release **EN**, then **BOOT**

then run it again. The script passes `--before no_reset` by default so the
manual latch survives; pass `-AutoReset` on a board whose auto-reset circuit
works properly.

A **1 µF capacitor between EN and GND** makes the button dance unnecessary
permanently, and is worth doing on cheap devkits.

### Afterwards (OTA, no cable)

```bash
curl.exe -u update:secretsauce -F "update=@.pio/build/wroom32/firmware.bin;filename=firmware.bin" http://<device-ip>/update
curl.exe -u update:secretsauce -F "update=@.pio/build/wroom32/littlefs.bin;filename=littlefs.bin" http://<device-ip>/update
```

> **The filename matters.** The updater picks the target partition by
> filename: anything containing `littlefs` or `spiffs` goes to the filesystem,
> everything else to the app partition. Send the filesystem image under the
> wrong name and it is written to the app partition instead.
>
> Use `curl`, not PowerShell's `Invoke-WebRequest -Form` — the latter does not
> send the filename in a form the parser reads.

## Configure

On first boot the device raises an access point named like
`5FC874bambulights`, password **`secretsauce`**. Join it and give it your WiFi.
Alternatively, since the firmware runs ImprovWiFi on the serial port, open
`web/improv.html` in Chrome or Edge while the board is on USB and set
credentials over the cable.

It then appears at `http://bambulights.local`:

- **Printer** — IP, port (8883), user (`bblp`), access code, serial
- **LEDs** — LED count, GRB/RGB, and per-state colour, pattern and pulse rate
- **Homeassistant** — optional MQTT broker for HA integration
- **Info** — lamp state, printer state, active fault, and diagnostics

Give the device a DHCP reservation in your router so its address stops moving.

## Known behaviour: adding config items resets stored settings

`EEPROMConfig` checksums the whole configuration tree, so **adding or removing
any config item invalidates everything already stored** and each setting falls
back to its compiled-in default. Expect to re-enter the printer details after
a firmware update that changes the config structure.

WiFi credentials are unaffected — `AsyncWiFiManager` keeps those in NVS
separately.

## Develop the web UI without flashing

`web/server.js` is a mock: it serves the GUI and answers the same WebSocket
messages the firmware does, from canned data.

```bash
cd web && node server.js
```

Then open <http://localhost:8080/app.html>. Edit, refresh, no flash cycle.

Its mock payload must mirror the fields `WSInfoHandler::handle()` actually
sends, or rows render as a literal `...`.

## The tower

Four tiers, each an independent segment of the strip answering a different
question. Several can be lit at once, and a dark tier is itself information.

| Tier | Conditions |
|---|---|
| **Filament** | changing · runout · jam · AMS lost · damp |
| **Status** | idle · printing · paused (never dark) |
| **Finished** | ready to collect — latched until the door opens or the timeout expires |
| **System** | no WiFi · no printer · warning · error |

Tiers are not mutually exclusive. Following the usual andon convention,
**SYSTEM means "a human is needed"** and lights for anything that has stopped
the machine, including a filament runout or jam — which light FILAMENT too.
You read SYSTEM from across the room and the specific tier tells you what to
bring. Routine events (an AMS change) and advisory ones (damp filament) light
only their own tier. STATUS never reports faults, so it cannot contradict
SYSTEM.

Within a tier the most severe live condition wins.

Each condition has its own colour, pattern (**constant · pulse · blink ·
fade**) and rate, and each tier has a configurable first LED and length, all
on the Tower page. Defaults are one LED per tier.

The decision logic is in [`src/Tower.cpp`](src/Tower.cpp) and reads a plain
`Facts` struct, so it can be followed without untangling MQTT. Printer stages
(`stg_cur`) are mapped in [`src/MQTTBroker.cpp`](src/MQTTBroker.cpp); HMS
severity 1–2 becomes `error`, 3–4 `warning`.

## Known issue: OTA fails when memory is tight

The tower's configuration allocates ~86 config objects on the heap, leaving
roughly 71 KB free and a 45 KB largest block. OTA uploads sometimes fail with
a connection reset while the device stays up, apparently because the upload
buffer no longer fits alongside the TLS MQTT connection. Flashing over USB
always works. Reducing the heap cost (static rather than heap-allocated config
objects) would be the fix.

## Credits

This project is a fork of **[judge2005/BambuLights](https://github.com/judge2005/BambuLights)**
by Paul Andrews, which provides essentially all of the firmware: the MQTT
client, the state machine, the HMS decoding, the LED rendering, the
configuration system and the web UI. This fork adds WROOM-32 support and the
changes listed above.

It also depends on Paul Andrews' supporting libraries — `AsyncWiFiManager`,
`ESPConfig`, `ASyncOTAWebUpdate`, `Configs` and `ImprovWiFi`.

The original project was inspired by
[BLLEDController](https://github.com/DutchDevelop/BLLEDController).

Protocol documentation: [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI).

## Licence

The upstream project does not carry a licence file, so its terms are
unspecified and its author retains all rights by default. This fork is
published for the benefit of other Bambu owners and is not intended to claim
any ownership of that work. If you are the upstream author and would like
anything changed here, please open an issue.
