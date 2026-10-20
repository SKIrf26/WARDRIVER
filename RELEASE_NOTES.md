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
| `SHA256SUMS.txt` | Checksum |
| `SKIrf_WARDRIVER_C5_CYD_Beta_1_User_Guide.pdf` (+ two booklet versions) | User guide |

## Please know
This is a **beta**. A run scans only while the GPS has a fix.

**Known issue:** on some boards a microSD card passes the card test but logs written during a run do not stay on it. Until this is fixed, set **Settings > Storage > Save logs to: FLASH** (about 1.5 MB, enough for short runs); logs in flash show in the Logs list and the portal and upload normally.

After flashing, set your GPS pins again (Settings > GPS pins) if your module is not on the default RX 4 / TX 5. The web portal hotspot (`SKIrf_WARDRIVER`) has no password and is up for 60 seconds at a time. The Wi-Fi country is US by default (change `SKIRF_WIFI_COUNTRY` and build your own elsewhere). Sound needs a small passive buzzer on IO26.

Use it responsibly and within your local laws, and never operate the screen while driving. See the README.

Please report problems with the **Bug report** form on the Issues tab, with the Serial Monitor output if you can.
