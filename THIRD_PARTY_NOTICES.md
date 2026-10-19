# Third-party notices

SKIrf Wardriver itself is released under the **GNU General Public License v3.0** (see [LICENSE](LICENSE)). Copyright (C) 2026 SKIrf Labs.

It is built with, and links against, the open-source components below. Their licences are all compatible with distributing a combined GPL v3 firmware. Copies of the licence texts are in the `licenses/` folder.

| Component | Used for | Licence | Copy of licence |
|---|---|---|---|
| Arduino-ESP32 core (Espressif Systems), incl. ESP-IDF components and the NimBLE Bluetooth stack | Runtime, Wi-Fi, Bluetooth LE, TLS, web server, SD and LittleFS file systems | LGPL 2.1 or later, with Apache 2.0 and other permissive parts for the underlying ESP-IDF | [licenses/LGPL-2.1.txt](licenses/LGPL-2.1.txt), [licenses/Apache-2.0.txt](licenses/Apache-2.0.txt); see the core's own repository for the full per-component list |
| LovyanGFX (lovyan03) | Display and touch driver, sprites, smooth-font rendering, the Orbitron font data | FreeBSD (BSD 2-clause); parts derived from Adafruit and TFT_eSPI (BSD / MIT) | [licenses/BSD-LovyanGFX.txt](licenses/BSD-LovyanGFX.txt) |
| XPT2046_Touchscreen (Paul Stoffregen) | Touch controller | MIT | [licenses/MIT-XPT2046_Touchscreen.txt](licenses/MIT-XPT2046_Touchscreen.txt) |
| TinyGPSPlus (Mikal Hart) | NMEA parsing | LGPL 2.1 or later | [licenses/LGPL-2.1.txt](licenses/LGPL-2.1.txt) |
| ArduinoJson (Benoit Blanchon) | Settings files and WDGWars replies | MIT | [licenses/MIT-ArduinoJson.txt](licenses/MIT-ArduinoJson.txt) |
| Saira Semi Condensed (The Saira Project Authors) | Glyph shapes drawn on screen, and the web portal's font (a subset, as WOFF2, in `SKIrf_Web.h`) | SIL Open Font License 1.1 | [licenses/OFL-1.1-Saira.txt](licenses/OFL-1.1-Saira.txt) |
| Orbitron (The Orbitron Project Authors), as shipped in LovyanGFX | The "SKIrf" logo lettering on the boot and Home screens | SIL Open Font License 1.1 | [licenses/OFL-1.1-Orbitron.txt](licenses/OFL-1.1-Orbitron.txt) |
| Material Design Icons (Pictogrammers) | Interface icon shapes | Apache 2.0 | [licenses/Apache-2.0.txt](licenses/Apache-2.0.txt) |

## About the font data
The firmware contains pre-rendered, anti-aliased glyph bitmaps generated from Saira Semi Condensed (SIL OFL 1.1) by `tools/art/mkvlw.py`. The bitmaps are derivatives of the font software and remain under the OFL; they are not sold on their own. They are in `src/SKIrf_WARDRIVER_C5_CYD/SKIrf_Fonts.h`.

## About the icons
The interface icons are 4-bit masks rendered from the Material Design Icons webfont (Apache 2.0) by `tools/art/icons.py`. They are in `src/SKIrf_WARDRIVER_C5_CYD/SKIrf_Icons.h`.

## About the vehicle art
The vehicle sprites are original pixel art drawn by `tools/art/vehicles.py` for this project and are covered by the project's GPL v3 licence.

## Trademarks
WiGLE, WDGWars, RockBase, NMTech, Espressif, Arduino and the other names that appear in this project belong to their respective owners. They are used only to describe what the firmware works with. This project is not affiliated with, endorsed by, or sponsored by any of them.

## Binary releases
A pre-built `.bin` attached to a GitHub Release contains compiled code from the components above. The library sources are available from the projects named in the table.
