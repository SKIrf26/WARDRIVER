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
- 60-second web portal at each start: download (or all as .zip), delete, upload, Wi-Fi and API key set-up
- Pro mode operator console
- Eight accent colours plus a WDGW theme, four orientations, drag-to-scroll lists
- Illustrated user guide included (PDF, normal and booklet), plain-text help and the full source code

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
This is a **beta**. A run scans only while the GPS has a fix. The web portal hotspot (`SKIrf_WARDRIVER`) has no password and is up for 60 seconds at a time. The Wi-Fi country is US by default (change `SKIRF_WIFI_COUNTRY` and build your own elsewhere). Sound needs a small passive buzzer on IO26.

Use it responsibly and within your local laws, and never operate the screen while driving. See the README.

Please report problems with the **Bug report** form on the Issues tab, with the Serial Monitor output if you can.
