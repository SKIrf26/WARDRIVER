# Changelog

All notable changes to this project are listed here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed
- GPS: the module now runs at its normal 1 Hz rate with the satellites-in-view sentences (GSV) switched on. Beta 1 asked for 5 Hz with GGA and RMC only, which left almost no room on a 9600 baud link and hid the satellites in view.
- GPS: the constellation setting is left as the module ships it (Beta 1 forced GPS + BeiDou + GLONASS on ATGM336H modules).
- GPS: the baud hunt also runs when bytes arrive but no sentence can be read, and now tries 9600, 38400 and 115200 baud with normal and swapped RX/TX. Before, a module at another baud was never found.

### Added
- GPS screen: SATELLITES shows "used / in view" while searching, and FIX says NO DATA or BAD BAUD when the link is the problem. The wardrive screen says "No data from the GPS - check wiring" instead of waiting.
- Serial Monitor: a "GPS:" status line every 10 seconds (pins, baud, characters, good and bad sentences, satellites used and in view).

## [Beta 1] - 2026-10-18

First public release, for the **NM-CYD-C5** (ESP32-C5, 2.8 inch touch screen) board.

### Included
- Dual-band Wi-Fi (2.4 GHz and 5 GHz, including channels 149-165 with the US country setting) and Bluetooth LE in every sweep, scanned straight through ESP-IDF with a dwell time per channel that suits the ride.
- Ride profiles: Car (sweep every 2.5 s, passive Bluetooth, slow 5 GHz radar channels on one sweep in three), Motorcycle (3.5 s) and Walk/Bike (6 s, active Bluetooth, every channel every sweep).
- Unique counting per run with a 16,384-entry table in PSRAM; a network or device is logged again when it is heard at least 10 dB stronger.
- WiGLE CSV 1.6 logs: frequency column, accuracy from HDOP, WPA3/OWE security strings, Bluetooth LE rows with manufacturer ID. Rows are written once per sweep, and only with a GPS fix and date.
- GPS: NMEA at 9600 baud, module set to position updates 5 times a second (changed to 1 Hz after release, see Unreleased) (checksummed set-up commands for common modules), GPS pins chosen on the board with presets.
- Screens: boot splash, Home radar, ride picker, wardrive dashboard (unique total, new in the last sweep, 2.4 / 5 / BLE tiles, GPS strip), two-tap STOP, Run report with personal best and lifetime totals, GPS with compass, Logs, Settings, Wi-Fi with network picker and on-screen keyboard, brightness, orientation, theme, GPS pins, WDGWars stats, Pro mode, About & safety.
- Uploads from the board to WiGLE and WDGWars over your home Wi-Fi.
- Web portal (60 s at each start, or from Settings): file list, download, download all as .zip, delete, upload to WiGLE / WDGWars, home Wi-Fi and API key set-up. Also served on your home network while the board is connected to it.
- Pro mode: Wi-Fi / BLE on-off, BLE duty, sweep interval, RSSI windows, GPS accuracy gate, skip open networks, de-duplication, channel plans, live session readout.
- Interface: anti-aliased Saira Semi Condensed type, eight accent colours plus a WDGW pink + cyan theme, flicker-free drawing, drag-to-scroll lists, pixel-art vehicles that face the way they travel, four orientations, touch calibration.
- Logs to the microSD card, or to the board's flash when there is no card.
- Illustrated user guide (PDF, normal and two booklet versions), plain-text help, build notes, code map and the full source.

### Security
- The web portal no longer sends saved passwords and API keys back to the browser. Leave a field blank to keep the saved value.

### Known limitations
- The web portal hotspot has no password (it is up for 60 seconds at a time).
- Uploads use HTTPS without checking the server certificate. Upload over a network you trust.
- A run scans only while the GPS has a fix.
- The Wi-Fi country is set at build time (`SKIRF_WIFI_COUNTRY`, default US).
- Pro mode uses its own sweep interval (8 s by default), slower than the Car profile.
- Touch calibration and settings are reset when a new image is flashed.
