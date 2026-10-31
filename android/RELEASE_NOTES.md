# SKIrf Wardriver for the NM-CYD-C5 - Beta 1

First public release for the **NM-CYD-C5** board (ESP32-C5, 2.8 inch touch screen).

**SKIrf Wardriver** turns the board into a pocket dual-band wardriver. With a GPS module fitted it logs the Wi-Fi networks (2.4 GHz and 5 GHz) and Bluetooth LE devices around you in the WiGLE CSV format, and uploads your runs to WiGLE and WDGWars from the board.

## Highlights
- 2.4 GHz + 5 GHz Wi-Fi and Bluetooth LE in every sweep, with live counters for each band
- Pick your ride: Car (sweep every 2.5 s), Motorcycle or Walk/Bike profiles tune the scanner to your speed
- Unique counting per run; re-logged only when you get 10 dB closer, for better WiGLE placement
- WiGLE CSV 1.6 logs, written only with a GPS fix, to the microSD card or the board's flash
- Live dashboard, Home radar, Run report with personal best and lifetime totals, GPS screen with compass
- Upload to WiGLE and WDGWars from the board, and WDGWars stats on screen
- Web portal at each start, in the app's look (Logs, Upload, Setup tabs): download one log or a .zip, delete, upload to WiGLE / WDGWars, save Wi-Fi and API keys; the hotspot stays open while you use it
- Settings > Storage: card status, a card test, and where logs are saved (Card only / Auto / Flash); run report says where each log went
- Pro mode operator console
- Eight accent colours plus a WDGW theme, four orientations, drag-to-scroll lists
- Illustrated user guide included (PDF, normal and booklet), plain-text help and the full source code

## Updated build (20 Oct 2026)
This replaces the first Beta 1 image. Fixes since then:
- GPS: works with GPS + BeiDou modules such as the ATGM336H (position and speed no longer freeze), reads the module in the background, sets the module's motion mode to the ride, and keeps Wi-Fi off near the antenna so the fix holds
- Uploads to WiGLE and WDGWars work (the secure connection now has the memory it needs) and failures say why
- Wi-Fi scan and join work after a run; keys are stored on the card and in the board and checked after saving
- Logs: the run's file stays open for the whole run, rows are buffered and never dropped, logs in the board's flash are never deleted automatically, and lists / uploads show logs on the card and in flash together
- New web portal design; Settings > Storage

## New: SKIrf Wardriver for Android (1.0 Beta 1)
A phone-only wardriver: no board needed. It uses the phone's Wi-Fi, Bluetooth LE, classic Bluetooth, cell towers and GPS, writes the same WiGLE CSV logs as the board, and uploads to WiGLE and WDGWars. Drive, Map, GPS, Logs and Settings screens in the board's look, a live map with your route, satellite sky plot, auto-upload, and scanning with the screen off. Android 8.0 or newer.

Install `SKIrf_Wardriver_1.0_Beta_1.apk` from the files below: open it on the phone and allow the install (Play Protect may ask: More details > Install anyway). For far more Wi-Fi networks, turn off Developer options > Wi-Fi scan throttling (the app shows how).

## Updated build (31 Oct 2026)
- Wi-Fi and Bluetooth now scan together (on-chip memory freed; WIFI OFF / BT OFF show the reason on the run screen if a radio can't start)
- Bluetooth starts before Wi-Fi at run start, and the stack restarts once per run if it stalls
- WDGWars stats load (the reply check was inverted)
- The web portal also opens on your home network, at the address on the Wi-Fi screen
- Logs on the card and in flash are listed together; flash logs are never deleted automatically; MOVE TO CARD verifies every byte
- With a card fitted, logs default to the card only

## Install
Download `SKIrf_WARDRIVER_C5_CYD_Beta_1.bin` below and write it at `0x0`:

```
esptool --chip esp32c5 --port YOUR_PORT --baud 460800 write-flash 0x0 SKIrf_WARDRIVER_C5_CYD_Beta_1.bin
```

Use the board's left USB-C port. This resets the settings saved in the board (you will calibrate the touch screen again); files on the microSD card stay. Updates are done the same way, over USB.

## Files
| File | Purpose |
|---|---|
| `SKIrf_WARDRIVER_C5_CYD_Beta_1.bin` | Complete image, write at `0x0` |
| `SKIrf_Wardriver_1.0_Beta_1.apk` | Android app (phone-only wardriver), Android 8.0+ |
| `SHA256SUMS.txt` | Checksums |
| `SKIrf_WARDRIVER_C5_CYD_Beta_1_User_Guide.pdf` (+ two booklet versions) | User guide |

## Please know
This is a **beta**. A run scans only while the GPS has a fix.

After flashing, set your GPS pins again (Settings > GPS pins) if your module is not on the default RX 4 / TX 5. The web portal hotspot (`SKIrf_WARDRIVER`) has no password and is up for 60 seconds at a time. The Wi-Fi country is US by default (change `SKIRF_WIFI_COUNTRY` and build your own elsewhere). Sound needs a small passive buzzer on IO26.

Use it responsibly and within your local laws, and never operate the screen while driving. See the README.

Please report problems with the **Bug report** form on the Issues tab, with the Serial Monitor output if you can.
