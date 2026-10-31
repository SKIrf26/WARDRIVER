# SKIrf Wardriver for Android

The SKIrf Wardriver as a phone app: no board needed. It logs Wi-Fi, Bluetooth LE, classic Bluetooth and cell towers with GPS positions into WiGLE CSV files (the same format the NM-CYD-C5 board writes), then uploads them to WiGLE and WDGWars.

Version 1.0 Beta 1 · Android 8.0 or newer · about 0.5 MB

## Install

Get the APK from the [Releases page](https://github.com/SKIrf26/WARDRIVER/releases) or the [web flasher](https://skirf-labs.tech/wardriver_flasher/index.html#android), and check it against `SHA256SUMS.txt`.

1. Copy `SKIrf_Wardriver_1.0_Beta_1.apk` to the phone and open it.
2. Android asks to allow installs from that app (Files, Chrome or Drive). Allow it once.
3. Play Protect may say it doesn't recognise the app. Tap **More details → Install anyway**. The app isn't on the Play Store, which is all that warning means.
4. Open SKIrf Wardriver, tap **Get started**, and allow Location (choose **Precise**), Nearby devices and Notifications.

## Get the most out of it

Android limits each app to about **4 Wi-Fi scans every 2 minutes**. That misses a lot of networks at driving speed. To lift the limit:

1. Settings → About phone → tap **Build number** 7 times.
2. Settings → System → **Developer options**.
3. Under Networking, turn off **Wi-Fi scan throttling**.

The app shows a yellow banner when Android is holding it back, and Settings → Phone walks you through the same steps.

Also worth doing: Settings → Phone → **Don't let Android pause scanning** (battery exemption), so long runs with the screen off keep going.

## Screens

- **Drive**: big start/stop button, run timer, distance, rows saved, live counts per radio, "new to you" (never logged on this phone before), finds per minute chart, Wi-Fi band and security split, radio status, and a live list of finds.
- **Map**: dark street map with your route and a dot for every find. Pinch, drag, follow, fit, tap the legend to hide a type. Opens saved logs too. Street tiles come from OpenStreetMap; the layers button turns them off.
- **GPS**: fix state, coordinates (copy / open in maps), speed, altitude, heading, accuracy, a sky plot of every satellite and their signal strengths by constellation.
- **Logs**: WiGLE and WDGWars account stats, "Upload new logs", and every log file with row counts and upload ticks. Tap a file to upload it, map it, share it, save it to Downloads or delete it. Clean up deletes logs already uploaded everywhere.
- **Settings**: everything below.

## Settings

| Group | Setting | Default |
|---|---|---|
| Scanning | Wi-Fi | On |
| | Ask for a Wi-Fi scan every 1 / 2 / 5 / 10 / 30 s | 2 s |
| | Bluetooth LE | On |
| | Bluetooth LE scan power Low / Balanced / Max | Max |
| | Bluetooth classic | Off |
| | Cell towers (GSM, 3G, LTE, 5G) | Off |
| | Ignore weak signals Off / -90 / -85 / -80 dBm | Off |
| Logging | Wait for GPS before logging | On |
| | Best GPS accuracy needed 10 / 25 / 50 / 100 m / Any | 50 m |
| | Log each device Once / If stronger / Every 10 s | Once |
| | Start a new file every Never / 30 min / 1 h / 50k rows | Never |
| | File name starts with | SKIrf |
| | Copy each run to Downloads | Off |
| GPS and units | GPS updates 1 / 2 / 5 s | 1 s |
| | Use network location without GPS | Off |
| | Units Metric / Imperial | Imperial |
| During a run | Keep the screen on | On |
| | Keep scanning with the screen off (wake lock) | On |
| | Start a run when the app opens | Off |
| | Buzz for new open networks | Off |
| Uploads | WiGLE API Name + API Token, with Save and Test | |
| | WDGWars API key, with Save and Test | |
| | Upload when a run ends Off / WiGLE / WDGW / Both | Off |
| | Only upload on Wi-Fi | On |
| Theme | Mint, Ice, Gold, Ember, Siren, Neon, Cobalt, Chrome, WDGW, Pro | Mint |
| Phone | Wi-Fi scan throttling help, battery exemption, permissions, location settings | |
| Your data | Forget the "seen before" list, delete all logs | |

## Where the logs are

`Android/data/com.skirf.wardriver/files/logs/` on the phone. Newer Android versions hide that folder from file apps, so use **Save to Downloads** (or turn on *Copy each run to Downloads*) or **Share** to get files off the phone. Uninstalling the app deletes that folder, so save anything you haven't uploaded first.

Rows look exactly like the board's:

```
WigleWifi-1.6,appRelease=1.0b1,model=Pixel 8,release=15,device=shiba,...
MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
aa:bb:cc:dd:ee:ff,Cafe Guest,[WPA2-PSK-CCMP][ESS],2026-10-31 08:40:00,36,5180,-67,41.881832,-87.623177,181.4,4.0,,,WIFI
11:22:33:44:55:66,,Misc [LE],2026-10-31 08:40:00,0,,-80,41.881832,-87.623177,181.4,4.0,,76,BLE
```

Times are UTC, like the board's GPS time.

## If it crashes

The app saves the log up to that moment and shows the crash report the next time it opens, with Copy and Share buttons. Send that text along with what you were doing.

## Building from source

The app is plain Java with no libraries, built without Gradle:

```
aapt2 (resources) → javac (Java 8) → dx (dex) → zipalign → apksigner
```

`build.sh` runs the whole chain. It expects in `tools/`:

- `android.jar` for API 34 (with `resources.arsc`), e.g. from the Android SDK `platforms/android-34/`
- `aapt2` from Android SDK build-tools
- `release.jks`, the signing key. It is **not** in the public repo: the maintainer keeps it (it comes in the private source zip). Android only installs an update over the old app when it's signed with the same key, so official builds must use it. Anyone else building the app can make their own key with `keytool -genkeypair -keystore tools/release.jks -alias skirf -keyalg RSA -validity 20000` and set `SKIRF_KS_PASS`; that build installs as a separate signature (uninstall the official app first).

and on the PATH: a JDK, `dalvik-exchange` (dx), `zipalign` and `apksigner` (Ubuntu: `apt install dalvik-exchange zipalign apksigner`).

Android Studio users can also import `src/`, `res/`, `assets/` and `AndroidManifest.xml` into a new empty-activity project (no AndroidX needed).

## Code map

| File | What it does |
|---|---|
| `ScanEngine.java` | The run: GPS + satellites, Wi-Fi scans (with throttle detection and stale-result filter), BLE, classic Bluetooth, cell towers, de-duplication, counts, map points, all-time list |
| `Cells29.java` | Android 10+ cell refresh and 5G (NR) cells |
| `Sighting.java` | One find and its WiGLE CSV row (same cleaning as the board) |
| `CsvLog.java` | Writes the CSV on a background thread, flushes every 3 s, splits files |
| `ScanService.java` | Foreground service: keeps the run going with the screen off, live notification with Stop |
| `RunEnd.java` | After a run: copy to Downloads, auto-upload, summary notification |
| `Uploader.java` | WiGLE (`/api/v2/file/upload`, `/api/v2/stats/user`) and WDGWars (`/api/upload-csv`, `/api/me`) |
| `LogStore.java`, `LogProvider.java` | Listing, counting, exporting, sharing and deleting logs |
| `MainActivity.java` | Header, tabs, welcome, permissions, start/stop, system shortcuts, crash report |
| `DriveScreen`, `MapScreen`, `GpsScreen`, `LogsScreen`, `SettingsScreen` | The five tabs |
| `MapView.java` | Dark OSM tile map with route and finds |
| `Charts.java` | Start button, finds-per-minute chart, split bars, sky plot, signal bars |
| `Ui.java`, `Icon.java`, `Theme.java` | Controls, line icons and colours shared by every screen |
| `Prefs.java` | Every setting and its default |
