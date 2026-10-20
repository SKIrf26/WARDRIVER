SKIrf WARDRIVER  -  NM-CYD-C5  -  BETA 1
=======================================

Welcome. SKIrf Wardriver turns your NM-CYD-C5 board (the ESP32-C5 "cheap yellow display" with the 2.8 inch touch
screen) into a pocket wardriver. With a GPS module fitted it listens for Wi-Fi networks on 2.4 GHz and 5 GHz and for
Bluetooth LE devices, and logs each one with its position in the WiGLE CSV format. Runs can be uploaded to WiGLE and
WDGWars from the board.

It only listens. It never attacks and never joins anyone's network.


WHAT YOU NEED
-------------
  * the NM-CYD-C5 board
  * a USB-C DATA cable (some cables only charge: if the computer does not see the board, try another cable)
  * a computer (Windows, macOS or Linux)
  * a GPS module (for example an ATGM336H), 9600 baud. Nothing is logged without a GPS fix.
  * recommended: a microSD card, FAT32, 32 GB or smaller
  * optional: a small passive buzzer (for sound), a phone or laptop (for the web portal)


THE FASTEST WAY TO START
------------------------
  1. Flash the .bin file                    -> open help/2_FLASHING.txt
  2. Calibrate the touch screen (the board asks on first start: tap the marker in each corner)
  3. Wait for the 60-second web portal to finish, or tap SKIP
  4. Tap START WARDRIVE, pick your ride (CAR, MOTO or WALK/BIKE), then START
  5. Read the user guide (docs folder, PDF) for every screen and setting

Something not working?  -> open help/3_TROUBLESHOOTING.txt
Quick answers           -> open help/4_FAQ.txt


WHAT IS IN THIS FOLDER
----------------------
  READ_ME_FIRST.txt       this file
  HELP.txt                a one-page "I have this problem" finder
  README.md               the project page (shown on GitHub)
  help/                   plain-text help files (getting started, flashing, troubleshooting, FAQ)
  docs/                   the illustrated user guide (PDF) in three print formats, build notes, pictures
  firmware/               how to flash and check the release .bin
  src/                    the firmware source code
  tools/                  script that makes the release .bin from a build; art/ makes sprites, icons and fonts
  licenses/, LICENSE, THIRD_PARTY_NOTICES.md   licence information
  CHANGELOG.md, RELEASE_NOTES.md               what is in this release


THE USER GUIDE IN THREE PRINT FORMATS (docs folder)
----------------------------------------------------
  SKIrf_WARDRIVER_C5_CYD_Beta_1_User_Guide.pdf                          normal pages, US Letter
  SKIrf_WARDRIVER_C5_CYD_Beta_1_User_Guide_BOOKLET_flip-long-edge.pdf    booklet, print double-sided, "flip on long edge"
  SKIrf_WARDRIVER_C5_CYD_Beta_1_User_Guide_BOOKLET_flip-short-edge.pdf   booklet, print double-sided, "flip on short edge"
  For a booklet: print the file at 100% (actual size) on US Letter, landscape, double-sided with the setting in
  the file name, then fold the stack in half and staple along the fold.
  (If the back of every sheet comes out upside down, use the other booklet file.)


GOOD TO KNOW (THIS IS A BETA)
-----------------------------
  * This is a beta (updated build of 20 Oct 2026). Expect rough edges.
  * A run only scans while the GPS has a fix. Outdoors with a clear view of the sky, the first fix can take a few
    minutes; the screen says WAITING FOR A GPS FIX until then.
  * The web portal's hotspot SKIrf_WARDRIVER has no password and is up for 60 seconds at each start (longer while
    the page is open). Tap SKIP if you do not need it.
  * GPS on IO4 shows NO DATA on many boards: wire its TX to IO8, RX to IO9 and pick the 8 / 9 preset in
    Settings > GPS pins. Flashing resets the pins.
  * Known issue: on some boards logs written during a run do not stay on the microSD card even though
    Settings > Storage > TEST CARD passes. Until it is fixed, set Save logs to: FLASH.
  * Sound needs a small PASSIVE buzzer or piezo on IO26 and GND. Without one the board is silent.
  * The board is updated over USB.


USE IT RESPONSIBLY
------------------
  Use it within your local laws. Never join networks you are not allowed to use, and do not use the logs to
  identify, follow or harass people. Do not operate the screen while driving.

Problems or ideas: use the Issues tab of the project page and include what the Serial Monitor (115200 baud) printed.
