# Changelog

All notable changes to this project are listed here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed
- Bluetooth problems are now shown steadily on the run screen as BT OFF: <reason> (start failed / host not ready / scan refused / scan stuck, with the free on-chip RAM), and the board restarts the Bluetooth stack once per run before giving up.
- Bluetooth could count 0 for a whole run: the Bluetooth controller was started after Wi-Fi, when too little on-chip RAM was left for it, and both a failed start and refused scans were ignored. Bluetooth now starts before Wi-Fi at the beginning of a run, failures are checked, and the run screen alternates its status with BLUETOOTH NOT SCANNING when Bluetooth is not working (the Serial Monitor prints the free RAM).
- WDGWars stats always said the API key was rejected: the check on the /api/me reply was inverted, so every successful reply ({"ok": true, "username": ...}) was treated as a refusal. Stats now load; a real refusal shows the server's error or HTTP status.
- The web portal did not open on the home network: its server only ran while the board's own hotspot was up, and Wi-Fi switched off 20 s after leaving the Wi-Fi screens. The portal now also starts whenever the board joins your home Wi-Fi, at the address shown on the Wi-Fi screen ("Portal: http://..."); Wi-Fi stays on for 2 minutes after joining and while the page is open. Routes are registered once and the server restarts cleanly after Wi-Fi restarts.
- Logs could be lost: when the card came back, logs saved in the board's flash were copied to the card and the flash copy deleted after a quick size check, so a copy that never really reached the card took the log with it. The board now never deletes a flash log by itself. MOVE TO CARD copies, re-mounts the card, compares every byte, and only then deletes the flash copy, and says what failed if it could not.
- The Logs list, the portal, uploads, downloads and deletes now cover logs on the card and in the board's flash together; logs only in flash are tagged FLASH. A log no longer disappears from view when the board switches stores.
- Card writes during a run went straight from the PSRAM row buffer to the card driver, which cannot DMA from PSRAM and needs on-chip bounce memory that Wi-Fi and Bluetooth have taken; the card test (on-chip buffer, radios off) passed while runs failed. Run writes now go through a 512-byte on-chip buffer, one sector at a time.
- Runs could lose their rows even though the card tested fine: the log file was re-opened for every batch, and opening a file on the card needs internal RAM that Wi-Fi and Bluetooth use up during a run, so the writes failed mid-run (and the old 4 KB buffer then dropped the rows). The run's file is now opened once, before the radios start, and kept open; rows wait in a 512 KB PSRAM buffer and leave it only once they are written (flushed after every sweep); at the end of a run the radios are switched off before the last write, with a card remount if needed.
- With a card fitted and no saved choice (first start, or after flashing), logs now default to Card only, which never uses the board's flash. Auto and Flash are still in Settings > Storage.
- A failed card write during a run now remounts the card at a lower clock (16 > 8 > 4 MHz) twice before giving up on the card, instead of going straight to the board's flash.
- GPS could stop getting a fix after an upload from the board: the home Wi-Fi connection stayed up (now at full transmit power) a few centimetres from the GPS antenna. The connection now drops 20 s after leaving the Wi-Fi screens, like the idle radio already did; uploads join the network again by themselves. The GPS screen says "Wi-Fi on: slows the fix" while the radio is on and there is no fix.
- Logs could go missing from the Logs list, the portal and uploads: the board listed only the first 40 files on the card in the card's own order, so new runs could fall off the end, and a run that had to fall back to the board's flash was invisible once the card was back. The list now holds 120 logs, newest first; logs saved in flash are moved onto the card as soon as it works again; and a run that logged nothing (no GPS fix) no longer leaves an empty file behind.
- Uploads failed with "memory allocation failed": a TLS connection needs 40-50 KB and the chip's internal RAM had about 45 KB left with Wi-Fi, the display and the SD card running. mbedTLS now takes its larger blocks (512 bytes and up) from the 8 MB PSRAM, as ESP-IDF's CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC would.
- Uploads now say why they failed: Wi-Fi dropped, the server name could not be looked up (no internet), the secure connection failed (with the TLS error and free memory), or the server's own reply (for example "401 Unauthorized - <message>"). WiGLE replies of 200 with "success":false now count as failures. The secure connection is retried once. The reason shows on the board's upload screens (wrapped over up to three lines) and under each file in the portal's Upload tab.
- Wi-Fi scans came back empty, joining the home network failed and uploads failed after a run: the Bluetooth stack stayed up after the run and kept its memory, while Wi-Fi is fully shut down when idle and has to start again for every scan or join. Bluetooth is now shut down when a run ends, and every scan, join and upload starts the Wi-Fi with full power (the portal hotspot runs at 8.5 dBm), 2.4 + 5 GHz and the Wi-Fi country set.
- Wi-Fi screens say why a scan or join failed (network not found, password rejected, no answer, Wi-Fi did not start) instead of a status number; the portal's upload report includes the reason too.
- Saved Wi-Fi password and API keys could be lost: the portal said "saved" even when the card write failed (the card shares the display's SPI bus, and the portal screen is animating), and opening the hotspot re-read the file over the keys in memory. Keys are now written to the card and read back, and also kept in the board's NVS; loading falls back to NVS, the portal reports a real failure, and opening the hotspot no longer overwrites keys in memory.
- Web portal: a network name or WiGLE API name being typed is no longer wiped by the page's 8-second refresh before you can save it.
- Web portal: the hotspot stays open while the page is in use, instead of closing 60 seconds after it started.
- Web portal: uploads no longer cut the phone off without a result. The board now runs the upload itself (hotspot off, join home Wi-Fi, progress on its screen, Wi-Fi off again) and the portal's Upload tab shows the result next time. Saving home Wi-Fi no longer drops the hotspot to test it.
- Logs: a run's CSV file and header are now written before the Wi-Fi and Bluetooth radios start, while internal memory is free. A failed write remounts the microSD card and tries again; if the card still refuses, the run carries on in the board's flash under the same file name instead of losing rows. The GPS buffer moved to PSRAM to leave internal memory for the card. The run screen shows LOG NOT SAVING (red) if nothing can be written, and the Serial Monitor prints the free memory when a write fails.
- GPS: position no longer sticks while walking on ATGM336H and other CASIC modules. Many ship set for vehicles, which holds the position still at low speed, so it updated in bursts. When the module's RX is wired, the board sends one $PCAS11 sentence (not saved in the module): portable on the GPS screen, walking for a Walk/Bike run, vehicle for Car and Motorcycle.
- GPS: data is read from the module by a background task every 5 ms into a 16 KB buffer, so a slow screen update or SD card call can no longer make the board drop position sentences. Before, the position could freeze for 20-30 seconds and then jump.
- GPS: position and speed now keep updating with GPS + BeiDou modules such as the ATGM336H. These modules label a position sentence "BD" (or GP / GN) depending on which satellites carry the fix, and the GPS library threw away every BD sentence without a word, so the position froze, the speed stayed 0 and the fix kept dropping out. The board now relabels those sentences before reading them. The Serial Monitor's "GPS:" line counts them as "relabelled".
- GPS screen: modules that send no satellites-in-view list (only GGA and RMC) now show the satellites used, with signal bars, instead of 0.
- GPS: the board no longer sends set-up commands to the module; it only listens. Beta 1 sent commands for several module makers (rate, sentences, constellations), and on the NM-CYD-C5 GPS module that stopped it talking after a few seconds. The module's factory output (1 Hz, 9600 baud, GGA, GSA, GSV and RMC) is all the wardriver needs. The old behaviour can be turned back on in the source (SKIRF_GPS_CONFIGURE).
- GPS: the baud hunt also runs when bytes arrive but no sentence can be read, and now tries 9600, 38400 and 115200 baud with normal and swapped RX/TX. Before, a module at another baud was never found.

- GPS: faster first fix. The Wi-Fi radio is now switched off whenever nothing uses it (after the start-up portal, after a run, and away from the Wi-Fi screens), the start-up hotspot runs at low power (8.5 dBm), and the screen animations slow to 10 frames a second until the GPS has a fix. The radio and display bus sit a few centimetres from the GPS antenna, and their noise was slowing or blocking the first fix.

- GPS: once good sentences arrive the board stays on that pin and baud setting. Before, a pause of a few seconds made it switch to another speed even though it had already found the module, so the screen flipped between "No data" and "Data OK".
- GPS: satellites in view are also read from $GNGSV sentences.

### Added
- TEST CARD now has a second phase that writes the way a run does: Wi-Fi and Bluetooth on, file kept open, rows from the PSRAM buffer, flushed per batch.
- Run journal on the card (SKIrf_runlog.txt): one line per run with file, storage, mode, rows, bytes, writes, failures, unsaved bytes and the last error. Settings > Storage shows the last run's result; the portal's Setup tab can download the journal.
- Settings > Storage: the microSD card's state (found, used / total, clock) and the board flash's (logs and space), a TEST CARD button (fresh mount, writes 64 KB, reads it back, compares, deletes; reports PASS with speeds or what failed), SAVE LOGS TO: Auto (card, flash if the card fails) / Card only / Flash only (kept across restarts), and MOVE TO CARD for logs saved in flash.
- Run report: says where the log went ("206 rows saved to microSD", "... to board flash", "No log: the GPS had no fix", or "Log NOT saved").
- Web portal redesigned to match the board: one page with Logs, Upload and Setup tabs, in the board's theme colours and font. Shows log count and size, storage use, GPS and your records; logs are listed by date with their ride, and can be selected to upload, download as one .zip, or delete. Confirmations and messages appear in the page instead of browser pop-ups.
- GPS screen: "Moved N sec ago" shows how long ago the module last reported a new position; latitude and longitude show 6 decimals.
- GPS screen: link health for the last 5 seconds: Fix % (position sentences that carried a fix), HDOP, bad sentences, lost bytes and the longest main-loop delay. The same numbers are on the Serial Monitor's "GPS:" line.
- GPS screen: shows the module's antenna report when it sends one (ATGM336H and others report ANTENNA OK / OPEN / SHORT); FIX says NO ANTENNA when the antenna is open or shorted.
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
