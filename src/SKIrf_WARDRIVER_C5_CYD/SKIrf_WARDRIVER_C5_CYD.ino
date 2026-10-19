/*
 * ============================================================
 *  SKIrf WARDRIVER  -  NM-CYD-C5  -  Beta 1
 * ============================================================
 *  Copyright (C) 2026 SKIrf Labs.
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation, either version 3 of the License, or (at your
 *  option) any later version. It comes with NO WARRANTY. See ../../LICENSE.
 *  SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Board:     NM-CYD-C5 (ESP32-C5, 2.8" ST7789 + XPT2046 "cheap yellow display")
 *  Arduino:   Tools > Board "ESP32C5 Dev Module", Flash Size 8MB,
 *             Partition "8M with spiffs (3MB APP/1.5MB SPIFFS)", PSRAM Enabled
 *             (full settings and the command-line build: docs/BUILDING.md)
 *
 *  This folder must contain:
 *    SKIrf_WARDRIVER_C5_CYD.ino   (this file)
 *    SKIrf_C5_CYD_LGFX.h          (LovyanGFX panel / touch set-up)
 *    SKIrf_Sprites.h              (vehicle pixel sprites)
 *    SKIrf_Icons.h                (Material Design icon masks)
 *    SKIrf_Fonts.h                (Saira Semi Condensed smooth fonts, SIL OFL 1.1)
 *
 *  Libraries: LovyanGFX, TinyGPSPlus, ArduinoJson, XPT2046_Touchscreen
 *
 *  Display ST7789 240x320  SPI SCK=6 MOSI=7 MISO=2 CS=23 DC=24 BL=25
 *  Touch   XPT2046 CS=1     SD CS=10     GPS RX=4 TX=5 (changeable on screen)
 * ============================================================
 */

#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>
// ESP32 Arduino 3.x TLS client (Espressif package required)
#include <NetworkClientSecure.h>
#include <mbedtls/base64.h>
#define SKIRF_SECURE_CLIENT NetworkClientSecure
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_wifi.h>

// ESP32-C5: stock TFT_eSPI will not compile (no VSPI). Use LovyanGFX.
#include "SKIrf_C5_CYD_LGFX.h"

#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <Preferences.h>

#include "SKIrf_Sprites.h"
#include "SKIrf_Icons.h"
#include "SKIrf_Fonts.h"   // Saira Semi Condensed smooth fonts (SIL OFL 1.1)

// ---------------- USER CONFIG ----------------
#define SKIRF_VERSION      "Beta 1"          // shown on the boot screen
#define SKIRF_APP_RELEASE  "0.1.0-beta.1"    // written into every WiGLE CSV header
// This CYD's ST7789 shows R and B swapped on the glass. Every colour in the
// sketch goes through C565(r,g,b) below, so if a different panel ever shows
// red as blue, flip this ONE switch to 0 and all themes come out right.
#define SKIRF_PANEL_BGR    1
// Wi-Fi regulatory country for scanning. "US" unlocks 5 GHz UNII-3
// (ch 149-165) which is where most US home routers live. Change if needed.
#define SKIRF_WIFI_COUNTRY "US"
// ---------------------------------------------

#define TOUCH_CS    1
#define TOUCH_IRQ  -1
#define SD_CS      10
#define BUZZER_PIN 26
#define TFT_BL     25
#define CAL_FILE   "/wardriver_cal.json"

// ---------- Pins and types ----------
// Types first so Arduino's auto-prototyper can emit waitForTap() etc.
struct CalPoint { int rawX, rawY; };
enum Screen { HOME_SCREEN, WARDRIVING_SCREEN, WD_MODE_SCREEN, GPS_SCREEN, SETTINGS_SCREEN, BRIGHTNESS_SCREEN, ORIENTATION_SCREEN, UI_COLOR_SCREEN, FILES_SCREEN, WIFI_SCREEN, WIFI_SCAN_SCREEN, WIFI_PASS_SCREEN, DISCLAIMER_SCREEN, GPS_PINS_SCREEN, GAME_STATS_SCREEN, PRO_SCREEN, PRO_SESSION_SCREEN, SUMMARY_SCREEN };
enum WdTransport { WD_XPORT_CAR = 0, WD_XPORT_MOTO = 1, WD_XPORT_WALK = 2 };
struct Rect { int x, y, w, h; };
// Animation lanes (see ANIMATION LANES)
enum LaneScene : uint8_t { SC_NONE = 0, SC_ROAD };
enum LaneRole : uint8_t { ROLE_HOME_A = 0, ROLE_HOME_B, ROLE_WD, ROLE_SPLASH_A, ROLE_SPLASH_B, ROLE_MISC };
struct LaneActor {
  const SkiSprite* spr; float x, speed, dist; bool right, active;
  uint8_t frame; int kind; uint16_t body; unsigned long nextSpawn;
};
struct Lane {
  bool on; int x, y, w, h; uint8_t scene, role; bool treadmill, goRight;
  float scroll; uint32_t tick; LaneActor a;
};
// Wardriving dashboard layout
struct Blip { uint8_t ang, rad, kind; unsigned long t; };
struct HomeGeo { Rect scope, lane, primary, sec[3]; bool land; };
struct SetRow { const char* label; int8_t action; int8_t icon; };
struct DragState { bool down, drag; int x0, y0, s0; unsigned long last; };
typedef std::function<void(LovyanGFX&, int idx, int x, int y, int w, int h)> RowFn;
struct StatLine { const char* lab; char val[28]; uint16_t col; };
struct GpsGeom { Rect card[6]; Rect compass; Rect sig; };
struct WdLayout { Rect status, hero, band[3], prog, gps, lane, side, btnMain, btnHome; bool land; };

// ---------- Fonts and colours ----------
// ---- Type scale (one family, anti-aliased). *_CAPS-style: UI fonts render all caps.
//   F_XS 11px captions/status  F_S 13px secondary  F_M 16px buttons/rows
//   F_L 22px titles/counts      F_XL 44px hero numbers
//   F_DS 14px / F_DK 20px mixed-case data (SSIDs, passwords, file names, keys)
lgfx::PointerWrapper fwXS, fwS, fwM, fwL, fwXL, fwDS, fwDK;
lgfx::VLWfont fntXS, fntS, fntM, fntL, fntXL, fntDS, fntDK;
const lgfx::IFont* F_XS = &fntXS;
const lgfx::IFont* F_S  = &fntS;
const lgfx::IFont* F_M  = &fntM;
const lgfx::IFont* F_L  = &fntL;
const lgfx::IFont* F_XL = &fntXL;
const lgfx::IFont* F_DS = &fntDS;
const lgfx::IFont* F_DK = &fntDK;
bool fontsReady = false;
void initFonts() {
  if (fontsReady) return;
  fwXS = lgfx::PointerWrapper(VLW_XS, sizeof(VLW_XS)); fntXS.loadFont(&fwXS);
  fwS  = lgfx::PointerWrapper(VLW_S,  sizeof(VLW_S));  fntS.loadFont(&fwS);
  fwM  = lgfx::PointerWrapper(VLW_M,  sizeof(VLW_M));  fntM.loadFont(&fwM);
  fwL  = lgfx::PointerWrapper(VLW_L,  sizeof(VLW_L));  fntL.loadFont(&fwL);
  fwXL = lgfx::PointerWrapper(VLW_XL, sizeof(VLW_XL)); fntXL.loadFont(&fwXL);
  fwDS = lgfx::PointerWrapper(VLW_DS, sizeof(VLW_DS)); fntDS.loadFont(&fwDS);
  fwDK = lgfx::PointerWrapper(VLW_DK, sizeof(VLW_DK)); fntDK.loadFont(&fwDK);
  fontsReady = true;
}

// Panel-correct RGB565 from an intended on-glass colour.
constexpr uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
  return SKIRF_PANEL_BGR
    ? (uint16_t)(((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3))
    : (uint16_t)(((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t C565x(uint32_t rgb) { return C565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }

// Semantic colours (what you actually see on the glass)
static const uint16_t C_RED     = C565(255, 56, 56);
static const uint16_t C_GREEN   = C565(46, 224, 110);
static const uint16_t C_YELLOW  = C565(255, 214, 0);
static const uint16_t C_AMBER   = C565(255, 168, 0);
static const uint16_t C_ORANGE  = C565(255, 128, 0);
static const uint16_t C_CYAN    = C565(0, 214, 255);
static const uint16_t C_BLUE    = C565(48, 120, 255);
static const uint16_t C_VIOLET  = C565(170, 120, 255);
static const uint16_t C_WHITE   = 0xFFFF;
// Band colours used everywhere a capture is shown
// Band / state colours are derived from the theme accent at runtime (paletteRefresh)
uint16_t C_W24 = C565(80, 170, 255);   // 2.4 GHz
uint16_t C_W5  = C565(186, 128, 255);  // 5 GHz
uint16_t C_BLE = C565(0, 220, 200);    // Bluetooth LE
uint16_t C_OK  = C565(64, 240, 150);   // good / locked
uint16_t C_WARN = C565(255, 190, 90);  // waiting / weak
uint16_t C_BAD = C565(240, 92, 104);   // stop / error


void drawHomeScreen();
void drawWardrivingScreen();
void drawWdModeScreen();
void applyTransportProfile(int mode);
const char* wdTransportLabel(int m);
const char* wdTransportShort(int m);
const char* wdTransportTag(int m);

#define GPS_BAUD    9600
int gpsRxPin = 4;
int gpsTxPin = 5;
int gpsPinEditRx = 4;
int gpsPinEditTx = 5;
bool gpsEditFocusRx = true;
int gpsPinsScroll = 0;
#define DEBOUNCE_DELAY 160

// NM-CYD-C5: TFT + touch + SD share SPI2 (pins in SKIrf_C5_CYD_LGFX.h; TFT_eSPI is an alias there)
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS);
TinyGPSPlus gps;
// Satellites in view, from each constellation's GSV sentence (field 3). GGA only reports
// satellites USED in a fix, which is 0 until the first fix, so on its own it looks dead.
TinyGPSCustom gsvGP(gps, "GPGSV", 3), gsvGL(gps, "GLGSV", 3), gsvGB(gps, "GBGSV", 3), gsvBD(gps, "BDGSV", 3), gsvGA(gps, "GAGSV", 3), gsvGN(gps, "GNGSV", 3);
int gsvN[6] = {0, 0, 0, 0, 0, 0};
unsigned long gsvMs[6] = {0, 0, 0, 0, 0, 0};
// Link lock: once good sentences arrive on a pin/baud setting, the board stays there for good
bool gpsLinkLocked = false;
// The module's own text messages ($GPTXT / $GNTXT), e.g. ATGM336H "ANTENNA OPEN" / "ANTENNA OK"
char gpsLine[128]; int gpsLineLen = 0;
unsigned long gpsRelabelCount = 0;
// GPS bytes are moved from the UART into a 16 KB ring by a small task that runs every 5 ms,
// whatever the main loop is doing, so a slow screen, SD card or radio call can no longer
// overflow the UART and lose position sentences (about 22 s of data at 5 Hz fits).
static const uint32_t GPS_RING_SZ = 16384;
static uint8_t* gpsRing = nullptr;
static volatile uint32_t gpsRingHead = 0, gpsRingTail = 0;
volatile uint32_t gpsRingDrops = 0, gpsUartErrs = 0;
#ifndef SKIRF_HOST_PREVIEW
static SemaphoreHandle_t gpsPortMux = nullptr;
#endif
// live link health for the GPS screen (5 s window)
unsigned long gpsWinStart = 0, gpsWinPos = 0, gpsWinFix = 0, gpsWinFail0 = 0, gpsWinLost0 = 0, gpsLoopMaxMs = 0, gpsLoopLastMs = 0;
int gpsNavModeSent = -1;   // CASIC $PCAS11 dynamic model last sent (-1 = not yet on this port)
double gpsRawLat = 0, gpsRawLng = 0; unsigned long gpsMovedMs = 0;   // last time the module reported a different position
int gpsDiagFixPct = -1; unsigned long gpsDiagBad = 0, gpsDiagLost = 0, gpsDiagLag = 0;   // GGA/RMC sentences whose talker (BD, GQ, ...) TinyGPS++ would have dropped
char gpsTxt[40] = "";
char gpsAnt[8] = "";        // "OK", "OPEN", "SHORT" or "" (module does not report it)
unsigned long gpsTxtMs = 0;
int gpsInView = 0;
uint32_t gpsCharsAtOpen = 0, gpsPassAtOpen = 0;   // TinyGPS counters when the port was last (re)opened
unsigned long gpsDiagMs = 0;
HardwareSerial gpsSerial(1);
uint32_t gpsBaudNow = 9600;
uint32_t gpsChars = 0;
unsigned long gpsLastByteMs = 0;
unsigned long gpsLastHuntMs = 0;
int gpsHuntStep = 0;
bool gpsPinsSwapped = false;


bool highContrast = false;
uint16_t accentColor = C565(64, 240, 150);
// WDGWars dual theme: pink chrome + cyan text
bool wdgwTheme = false;
uint16_t wdgwChrome = C565(255, 32, 160); // hot pink
uint16_t wdgwText   = C565(0, 255, 255);  // electric cyan
int brightness = 255;
int preferredRotation = 0;  // default portrait so first-boot cal fits

// Affine touch: screen = A*rawX + B*rawY + D
float calAx = 0, calBx = 0, calDx = 0;
float calAy = 0, calBy = 0, calDy = 0;
bool calAffineOk = false;
uint16_t lgfxCal[8] = {0};
bool lgfxCalOk = false;
int calXMin = 200, calXMax = 3900;
int calYMin = 200, calYMax = 3900;
bool calLoaded = false;
Preferences calPrefs;
int calRotation = 0;

double gpsLat = 0.0, gpsLng = 0.0, gpsAlt = 0.0;
int gpsSats = 0;
bool gpsValid = false;
double gpsCourse = 0.0, gpsSpeed = 0.0;

bool wardriveActive = false;
String wardriveFile = "";
unsigned long lastWardriveScan = 0;
const unsigned long WARDRIVE_INTERVAL = 8000;

// ---------- Globals: ride profile, state ----------
// Transport profile chosen before a run (Home → WARDRIVE → picker)
int wdTransport = WD_XPORT_CAR;
unsigned long wdSweepMs = 8000;
bool wdDoWifi = true;
bool wdDoBle = true;
int  wdBleDuty = 1;          // 0 LOW  1 BAL  2 HIGH
int  wdChanFilter = 0;       // 0 ALL  1 2.4  2 5  3 1-6-11
int  wdWifiRssiMin = -98;
int  wdWifiRssiMax = -20;
int  wdBleRssiMin  = -100;
int  wdBleRssiMax  = -20;
bool wdGpsGate = true;
int  wdGpsMinSats = 4;

const char* wdTransportLabel(int m) {
  int v = (m < 0) ? wdTransport : m;
  if (v == WD_XPORT_MOTO) return "Motorcycle";
  if (v == WD_XPORT_WALK) return "Walk / bike";
  return "Car";
}
const char* wdTransportShort(int m) {
  int v = (m < 0) ? wdTransport : m;
  if (v == WD_XPORT_MOTO) return "MOTO";
  if (v == WD_XPORT_WALK) return "WALK";
  return "CAR";
}
const char* wdTransportTag(int m) {
  int v = (m < 0) ? wdTransport : m;
  if (v == WD_XPORT_MOTO) return "M";
  if (v == WD_XPORT_WALK) return "W";
  return "C";
}

int wardriveWifiCount = 0;
int wardriveWifi24Count = 0;
int wardriveWifi5Count = 0;
int wardriveBleCount = 0;
int sessionWifiTotal = 0;
int sessionWifi24Total = 0;
int sessionWifi5Total = 0;
int sessionBleTotal = 0;
int scanPhase = 0; // 0 idle 1 wait24 2 wait5 3 ble

// ---------- PRO MODE (operator console) ----------
bool proMode = false;
bool proWifiOn = true;
bool proBleOn = true;
int  proBleDuty = 1;          // 0 LOW  1 BAL  2 HIGH
int  proIntervalMs = 8000;    // sweep cadence
int  proWifiRssiMin = -95;
int  proWifiRssiMax = -25;
int  proBleRssiMin  = -100;
int  proBleRssiMax  = -25;
bool proGpsGate = false;      // log only with a fix
bool proSkipOpen = false;     // skip open ESS
bool proDedupe = true;        // unique BSSID / BLE addr per session
int  proChanMode = 0;         // 0 ALL  1 2.4  2 5  3 1-6-11
int  proAccMode = 0;          // 0 OFF  1 HDOP<=5  2 HDOP<=2  3 SATS>=6
int  proScroll = 0;
struct ProHit {
  char name[18];
  int rssi;
  char kind; // W or B
};
ProHit proHits[5];
uint8_t proHitN = 0;
uint8_t proHitHead = 0;
int  proSweepCount = 0;
unsigned long proLastSweepMs = 0;
int  proRssiSeenMin = 0;
int  proRssiSeenMax = -127;
uint16_t proCyan  = C565(0, 255, 0);     // signal green
uint16_t proAmber = C565(255, 160, 0);   // brass
uint16_t proPink  = C565(80, 136, 56);   // drab
uint16_t proText  = C565(198, 196, 176); // sand
uint16_t proSteel = C565(104, 104, 104); // faded OD
uint16_t proFrame = C565(0, 148, 0);     // olive bezel

BLEScan* pBLEScan = nullptr;

unsigned long lastGPSUpdate = 0;
int homeStatusPhase = 0;
unsigned long lastHomeStatusSwap = 0;
String homeTickerText = "";
int homeTickerX = 0;
unsigned long lastHomeTickerMove = 0;


unsigned long lastTouchTime = 0;
bool wasTouching = false;
bool touchIgnoreUntilUp = false;  // ignore held finger after navigation
int fileArmDelete = 0; // 0 none  1 selected  2 all

// Soft AP for web access (1 minute on boot)
WebServer server(80);
bool apActive = false;
bool portalCountdownActive = false;
unsigned long portalStartMs = 0;
int portalLastSec = -1;
bool returnToDevice = false;
int gameStatsScroll = 0;
unsigned long apStartTime = 0;
const unsigned long AP_DURATION = 60000;  // 1 minute
static const int proRowCount = 15;
int settingsScroll = 0;
int disclaimerScroll = 0;
int colorScroll = 0;
int fileBrowserCount = 0;
int fileBrowserScroll = 0;
int wifiScanCount = 0;
int wifiScanScroll = 0;
String wifiScanSSID[20];
int wifiScanRSSI[20];
String pendingSSID = "";
String pendingPass = "";

bool fileSelected[40];
String fileNames[40];

int uploadOneCsvToWdg(const String& path);
void doWdgUploadFromDevice();
bool tryConnectHomeWifi(uint32_t timeoutMs = 12000);
int uploadOneCsvToWigle(const String& path);
void doWigleUploadFromDevice();
void drawFilesScreen();
void drawWifiScreen();
void drawDisclaimerScreen();

void drawWifiScanScreen();
void drawWifiPassScreen();
void doWifiScan();

void loadCsvFileList();



// Home WiFi (station) credentials — saved on SD
String homeSSID = "";
String homePass = "";
bool homeWifiConnected = false;
bool userWantsHomeWifi = false;  // only connect when user taps CONNECT/JOIN
bool bootSplashActive = false;


String homeWifiIP = "";
unsigned long lastHomeWifiAttempt = 0;
const unsigned long HOME_WIFI_RETRY = 30000;
#define WIFI_CFG_FILE "/wifi_cfg.json"
bool sdReady = false;
bool logUseLittleFS = false;
String wdgApiKey = "";
String wigleApiName = "";
String wigleApiToken = "";

// Cached WDGWars /api/me stats
String wdgMeUser = "";
String wdgMeGang = "";
String wdgMeStatus = "Not loaded";
long wdgMeWifi = 0, wdgMeBle = 0, wdgMeAircraft = 0, wdgMeMesh = 0, wdgMeTotal = 0;
long wdgMeToday = 0, wdgMe7d = 0;
bool wdgMeOk = false;



Screen currentScreen = HOME_SCREEN;
Screen filesReturnTo = SETTINGS_SCREEN;   // Logs can be opened from Home or Settings


void beep(int times = 1, int duration = 45) {
  for (int i = 0; i < times; i++) {
    tone(BUZZER_PIN, 2200, duration);
    delay(duration + 12);
    noTone(BUZZER_PIN);
  }
}

void setBrightness(int val) {
  brightness = constrain(val, 12, 255);
  // Do NOT digitalWrite(BL) here — that pins the pin HIGH and kills PWM.
  tft.setBrightness((uint8_t)brightness);
#if defined(SOC_LEDC_SUPPORTED)
  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, (uint32_t)brightness);
#endif
}

uint16_t getAccent() {
  if (proMode) return proFrame;
  return wdgwTheme ? wdgwChrome : accentColor;
}
uint16_t getTextAccent() {
  if (proMode) return proText;
  return wdgwTheme ? wdgwText : accentColor;
}
uint16_t getBtnFill() { return TFT_BLACK; }  // sentinel: "normal" button
bool isPortrait() { return (tft.getRotation() % 2 == 0); }

// ================= SKIrf UI KIT =================
// Design tokens (intended on-glass colours; C565 makes them panel-correct)
// "Night drive" palette: flat surfaces, no outlines; spacing separates things.
uint16_t UI_CARD  = C565(19, 22, 29);    // flat surface
uint16_t UI_CARD2 = C565(30, 35, 45);    // raised / selected surface
uint16_t UI_LINE  = C565(37, 42, 53);    // dividers only
uint16_t UI_MUTE  = C565(128, 137, 152); // secondary text
uint16_t UI_DIM   = C565(74, 81, 94);    // tertiary text
uint16_t UI_TEXT  = C565(234, 238, 243); // primary text
uint16_t C_NEW    = C565(255, 181, 71);  // "fresh catch" badge

// ---- Palette: every neutral and state colour is derived from the accent hue so
// each theme reads as one family (tinted greys, analogous band colours).
void rgbOf565(uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t hi = (c >> 11) << 3, mi = ((c >> 5) & 63) << 2, lo = (c & 31) << 3;
  if (SKIRF_PANEL_BGR) { r = lo; g = mi; b = hi; } else { r = hi; g = mi; b = lo; }
}
void rgbToHsv(uint8_t r8, uint8_t g8, uint8_t b8, float &h, float &s, float &v) {
  float r = r8 / 255.0f, g = g8 / 255.0f, b = b8 / 255.0f;
  float mx = max(r, max(g, b)), mn = min(r, min(g, b)), d = mx - mn;
  v = mx; s = mx > 0 ? d / mx : 0;
  if (d <= 0.0001f) h = 0;
  else if (mx == r) h = 60.0f * fmodf((g - b) / d, 6.0f);
  else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
  else h = 60.0f * ((r - g) / d + 4.0f);
  if (h < 0) h += 360.0f;
}
uint16_t hsv565(float h, float s, float v) {
  h = fmodf(h + 720.0f, 360.0f);
  s = constrain(s, 0.0f, 1.0f); v = constrain(v, 0.0f, 1.0f);
  float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2.0f) - 1)), m = v - c;
  float r, g, b;
  if (h < 60) { r = c; g = x; b = 0; } else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; } else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }
  return C565((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255), (uint8_t)((b + m) * 255));
}
uint16_t getAccent();
void paletteRefresh() {
  uint8_t r, g, b;
  rgbOf565(getAccent(), r, g, b);
  float h, s, v;
  rgbToHsv(r, g, b, h, s, v);
  bool grey = s < 0.18f;          // Chrome / white accent: use a cool steel family
  if (grey) { h = 212; s = 0.30f; }
  float ts = min(s, 1.0f);
  UI_CARD  = hsv565(h, ts * 0.30f, 0.115f);
  UI_CARD2 = hsv565(h, ts * 0.30f, 0.185f);
  UI_LINE  = hsv565(h, ts * 0.26f, 0.235f);
  UI_TEXT  = hsv565(h, ts * 0.05f, 0.95f);
  UI_MUTE  = hsv565(h, ts * 0.16f, 0.62f);
  UI_DIM   = hsv565(h, ts * 0.18f, 0.40f);
  float bs = grey ? 0.50f : max(0.45f, ts * 0.80f), bv = max(0.88f, v);
  C_W24 = grey ? hsv565(205, bs, bv) : getAccent();
  C_W5  = hsv565(h + 42, bs, bv);
  C_BLE = hsv565(h - 42, bs, bv);
  C_NEW = hsv565(h, grey ? 0.12f : ts * 0.32f, 1.0f);        // pale accent badge
  C_OK  = grey ? hsv565(150, 0.55f, 0.92f) : getAccent();
  C_WARN = hsv565(38, 0.62f, 1.0f);
  C_BAD  = hsv565(354, 0.60f, 0.94f);
}

bool useChromeHud() { return true; }

// Channel-wise blend: t=0 -> a, t=255 -> b. Works for RGB or BGR packing.
uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  uint32_t hiA = a >> 11, miA = (a >> 5) & 63, loA = a & 31;
  uint32_t hiB = b >> 11, miB = (b >> 5) & 63, loB = b & 31;
  uint32_t hi = (hiA * (255 - t) + hiB * t + 127) / 255;
  uint32_t mi = (miA * (255 - t) + miB * t + 127) / 255;
  uint32_t lo = (loA * (255 - t) + loB * t + 127) / 255;
  return (uint16_t)((hi << 11) | (mi << 5) | lo);
}

uint16_t uiCardFill() {
  if (proMode) return C565(10, 20, 10);
  if (wdgwTheme) return C565(30, 12, 34);
  return UI_CARD;
}
uint16_t blendColor(uint16_t col, uint8_t lvl) { return blend565(TFT_BLACK, col, lvl); }
uint16_t uiRaisedFill() { return blend565(uiCardFill(), TFT_WHITE, 18); }
// Readable ink on top of an arbitrary fill colour
uint16_t inkOn(uint16_t bg) {
  uint32_t a = bg >> 11, b = (bg >> 5) & 63, c = bg & 31;
  uint32_t lum = a * 2 + b * 2 + c * 2;   // channel order agnostic, rough
  return lum > 150 ? C565(8, 10, 14) : UI_TEXT;
}
// Border colour for normal (non-hot) cards/buttons
uint16_t uiBorder() {
  return uiCardFill();   // v3: no outlines — kept for callers that still pass a border
}

void uiResetFont() {
  tft.setFont(F_XS);
  tft.setFont(F_XS);
  tft.setTextSize(1);
}

// ---------- Material icons (4-bit alpha masks from SKIrf_Icons.h) ----------
const SkiIcon* iconSet(int size) {
  if (size >= 26) return ICONS_26;
  if (size >= 20) return ICONS_20;
  return ICONS_14;
}
// Draw icon `id` with its top-left at x,y, anti-aliased against `bg`.
void drawIconOn(LovyanGFX &g, int id, int size, int x, int y, uint16_t fg, uint16_t bg) {
  if (id < 0 || id >= ICON_COUNT) return;
  const SkiIcon &ic = iconSet(size)[id];
  int n = ic.n;
  uint16_t lut[16];
  for (int a = 0; a < 16; a++) lut[a] = blend565(bg, fg, (uint8_t)(a * 17));
  g.startWrite();
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) {
      int k = j * n + i;
      uint8_t b = ic.a[k >> 1];
      uint8_t a = (k & 1) ? (b & 0x0F) : (b >> 4);
      if (a) g.drawPixel(x + i, y + j, lut[a]);
    }
  }
  g.endWrite();
}
void drawIcon(int id, int size, int x, int y, uint16_t fg, uint16_t bg) {
  drawIconOn(tft, id, size, x, y, fg, bg);
}

// Map a button label to a fitting icon (so every screen gets icons for free)
int iconForLabel(const char* l) {
  if (!l) return -1;
  struct M { const char* k; int id; };
  static const M m[] = {
    {"HOME", ICON_HOME}, {"BACK", ICON_BACK}, {"START", ICON_PLAY}, {"STOP", ICON_STOP},
    {"SAVE", ICON_CHECK}, {"JOIN", ICON_CHECK}, {"DONE", ICON_CHECK}, {"REFRESH", ICON_REFRESH},
    {"RESCAN", ICON_REFRESH}, {"DELETE", ICON_TRASH}, {"CONFIRM", ICON_ALERT}, {"WIGLE", ICON_UPLOAD},
    {"WDGW", ICON_UPLOAD}, {"CONNECT", ICON_WIFI}, {"FIND NET", ICON_RADAR}, {"OFFLINE", ICON_STOP}, {"SKIP", ICON_PLAY},
    {"ALL", ICON_CHECK}, {"NONE", ICON_CHECK}, {"TAP TO STOP", ICON_ALERT}, {"RESUME", ICON_PLAY},
  };
  for (const M &e : m) if (strcmp(l, e.k) == 0) return e.id;
  return -1;
}

int iconForTitle(const char* t) {
  if (!t) return ICON_RADAR;
  struct M { const char* k; int id; };
  static const M m[] = {
    {"WARDRIVER", ICON_RADAR}, {"WARDRIVING", ICON_RADAR}, {"TRANSPORT", ICON_CAR}, {"GPS", ICON_SATELLITE},
    {"SETTINGS", ICON_COG}, {"BRIGHTNESS", ICON_BRIGHTNESS}, {"UI COLOR", ICON_PALETTE},
    {"ORIENTATION", ICON_ROTATE}, {"FILES", ICON_FOLDER}, {"WIFI", ICON_WIFI}, {"SELECT WIFI", ICON_WIFI},
    {"PASSWORD", ICON_WIFI}, {"DISCLAIMER", ICON_ALERT}, {"GPS PINS", ICON_PIN}, {"WDGW STATS", ICON_TROPHY},
    {"PRO CONSOLE", ICON_SHIELD}, {"SESSION", ICON_SHIELD}, {"UPLOAD", ICON_UPLOAD}, {"WEB PORTAL", ICON_PORTAL},
    {"SESSION REPORT", ICON_TROPHY}, {"TOUCH CAL", ICON_TAP},
  };
  for (const M &e : m) if (strcmp(t, e.k) == 0) return e.id;
  return ICON_RADAR;
}

void uiDrawLabel(int cx, int cy, int maxW, const char* label,
                 uint16_t tc, uint16_t bg, bool bold = true) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tc, bg);
  tft.setTextSize(1);
  if (bold) {
    tft.setFont(F_M);
    int tw = tft.textWidth(label ? label : "");
    if (tw > maxW - 10) tft.setFont(F_S);
  } else {
    tft.setFont(F_S);
  }
  tft.drawString(label ? label : "", cx, cy);
  uiResetFont();
}

// Rounded card with a soft top highlight and optional accent rail
void uiCardOn(LovyanGFX &g, int x, int y, int w, int h, uint16_t border, bool rail, uint16_t fill) {
  (void)rail;
  // Emphasis (accent "border") is shown as a lifted surface, not an outline
  bool lift = (border == getAccent() || border == C_GREEN);
  g.fillRoundRect(x, y, w, h, 10, lift ? blend565(fill, TFT_WHITE, 16) : fill);
}
void uiCard(int x, int y, int w, int h, uint16_t border, bool rail = true) {
  uiCardOn(tft, x, y, w, h, border, rail, uiCardFill());
}

// Status pill, optional 14px icon at the left
void uiPill(int x, int y, int w, int h, const char* label,
            uint16_t border, uint16_t tc, bool lit = false, int icon = -1) {
  (void)icon; (void)lit;
  uint16_t fill = uiCardFill();
  tft.fillRoundRect(x, y, w, h, h / 2, fill);
  tft.fillCircle(x + 9, y + h / 2, 3, border);
  tft.setFont(F_XS);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(tc, fill);
  tft.drawString(label ? label : "", x + 17, y + h / 2);
  uiResetFont();
}



// Main button. fill: getBtnFill()=normal, TFT_GREEN/C_GREEN=go, C_RED=danger, C_AMBER=warn
// Turn "CHOOSE NET" into "Choose net" (keeps a few proper names intact)
void niceLabel(const char* in, char* out, size_t n) {
  struct M { const char* k; const char* v; };
  static const M m[] = {
    {"WIGLE", "WiGLE"}, {"WDGW", "WDGW"}, {"TAP TO STOP", "Tap to stop"}, {"FIND NET", "Networks"},
    {"OFFLINE", "Disconnect"}, {"BACK", "Back"}, {"HOME", "Home"}, {"CONFIRM", "Confirm"}, {"NONE", "None"},
    {"GPS", "GPS"}, {"LOGS", "Logs"}, {"DELETE", "Delete"}, {"REFRESH", "Refresh"}, {"RESCAN", "Scan again"},
  };
  for (const M &e : m) if (strcmp(in, e.k) == 0) { snprintf(out, n, "%s", e.v); return; }
  size_t i = 0;
  bool start = true;
  for (; in[i] && i + 1 < n; i++) {
    char c = in[i];
    out[i] = start ? c : (char)tolower(c);
    start = false;
  }
  out[i] = 0;
}

// Small left chevron for back / home buttons
void drawBackChevron(int x, int cy, uint16_t c) {
  for (int i = 0; i < 2; i++) {
    tft.drawLine(x + 5 + i, cy - 5, x + i, cy, c);
    tft.drawLine(x + i, cy, x + 5 + i, cy + 5, c);
  }
}

// Main button. fill: getBtnFill()=quiet, TFT_GREEN/C_GREEN=primary (accent),
// C_RED=danger, C_AMBER=attention. No outlines, no icons — type does the work.
void drawThemedButton(int x, int y, int w, int h, const char* label,
                      uint16_t fill, uint16_t textCol) {
  (void)textCol;
  const char* raw = label ? label : "";
  bool go = (fill == TFT_GREEN || fill == C_GREEN);
  bool danger = (fill == C_RED);
  bool warn = (fill == C_AMBER);
  bool nav = !go && !danger && !warn && (strcmp(raw, "BACK") == 0 || strcmp(raw, "HOME") == 0);
  uint16_t f = uiCardFill();
  if (go) f = getAccent();
  if (danger) f = C_BAD;
  if (warn) f = C_WARN;
  int r = min(h / 2, 14);
  char lab[32];
  niceLabel(raw, lab, sizeof(lab));
  tft.fillRect(x, y, w, h, TFT_BLACK);
  if (nav) {
    // ghost button: just a chevron and a word
    tft.setFont(F_M);
    int tw = tft.textWidth(lab);
    int sx = x + 10;
    drawBackChevron(sx, y + h / 2, getAccent());
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(UI_TEXT, TFT_BLACK);
    tft.drawString(lab, sx + 13, y + h / 2 + 1);
    (void)tw;
    uiResetFont();
    return;
  }
  tft.fillRoundRect(x, y, w, h, r, f);
  uint16_t tc = (go || danger || warn) ? inkOn(f) : UI_TEXT;
  if (proMode && !go && !danger && !warn) tc = proText;
  tft.setTextSize(1);
  bool loud = go || danger || warn;
  tft.setFont(loud ? F_M : F_M);
  int tw = tft.textWidth(lab);
  if (tw > w - 10) { tft.setFont(F_S); tw = tft.textWidth(lab); }
  if (tw > w - 6) { tft.setFont(F_XS); }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tc, f);
  tft.drawString(lab, x + w / 2, y + h / 2 + 1);
  uiResetFont();
}

// ---------- Title bar ----------
Rect titleStatusRect() { int w = tft.width(); return { w - 70, 12, 58, 22 }; }

void drawBrandChip(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 5, TFT_BLACK);
  tft.drawRoundRect(x, y, w, h, 5, blend565(TFT_BLACK, TFT_WHITE, 150));
  tft.setFont(F_S);
  tft.setTextSize(1);
  int skiW = tft.textWidth("SKI");
  int rfW  = tft.textWidth("rf");
  int tx0 = x + (w - skiW - rfW) / 2;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C565(255, 128, 0), TFT_BLACK);
  tft.drawString("SKI", tx0, y + h / 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("rf", tx0 + skiW, y + h / 2);
  uiResetFont();
}

// Live GPS / REC chip at the right of the title bar
bool titleHasStatus = false;
int titleStatusSig = -1;
void drawTitleStatus(bool force) {
  if (!titleHasStatus) return;
  bool blink = (millis() / 500) & 1;
  int sig = (gpsValid ? 1000 : 0) + gpsSats * 4 + (wardriveActive ? 2 : 0) + (wardriveActive && blink ? 1 : 0);
  if (!force && sig == titleStatusSig) return;
  titleStatusSig = sig;
  Rect r = titleStatusRect();
  tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  uint16_t col = gpsValid ? C_OK : (gpsSats > 0 ? C_NEW : UI_DIM);
  char b[8];
  snprintf(b, sizeof(b), "%d", gpsSats);
  tft.setFont(F_S);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(UI_MUTE, TFT_BLACK);
  int right = r.x + r.w;
  tft.drawString(b, right, r.y + r.h / 2);
  int tw = tft.textWidth(b);
  // satellite "orbit" glyph: dot with a ring
  int gx = right - tw - 10;
  tft.fillCircle(gx, r.y + r.h / 2, 3, col);
  tft.drawCircle(gx, r.y + r.h / 2, 6, blend565(TFT_BLACK, col, 110));
  if (wardriveActive) tft.fillCircle(gx - 16, r.y + r.h / 2, 4, blink ? C_BAD : blend565(TFT_BLACK, C_BAD, 70));
  uiResetFont();
}

void lanesStop();
const char* niceTitle(const char* t) {
  struct M { const char* k; const char* v; };
  static const M m[] = {
    {"WARDRIVER", "Wardriver"}, {"WARDRIVING", "Wardrive"}, {"TRANSPORT", "Pick your ride"}, {"GPS", "GPS"},
    {"SETTINGS", "Settings"}, {"BRIGHTNESS", "Brightness"}, {"UI COLOR", "Theme"}, {"ORIENTATION", "Orientation"},
    {"FILES", "Logs"}, {"WIFI", "Wi-Fi"}, {"SELECT WIFI", "Choose network"}, {"PASSWORD", "Password"},
    {"DISCLAIMER", "About & safety"}, {"GPS PINS", "GPS pins"}, {"WDGW STATS", "WDGWars stats"},
    {"PRO CONSOLE", "Pro mode"}, {"SESSION", "Live session"}, {"UPLOAD", "Upload"}, {"WEB PORTAL", "Web portal"},
    {"SESSION REPORT", "Run report"}, {"FILES", "Logs"},
  };
  for (const M &e : m) if (t && strcmp(t, e.k) == 0) return e.v;
  return t ? t : "";
}

void drawTitleBar(const char* title) {
  paletteRefresh();
  lanesStop();   // a new screen owns the display; lanes are re-armed by screens that want them
  int w = tft.width();
  const char* shown = title;
  titleHasStatus = false;
  tft.fillRect(0, 0, w, 48, TFT_BLACK);
  const char* t = niceTitle(title);
  titleHasStatus = true;
  titleStatusSig = -1;
  int rightEdge = titleStatusRect().x - 6;
  tft.setTextDatum(ML_DATUM);
  tft.setFont(F_L);
  if (tft.textWidth(t) > rightEdge - 14) tft.setFont(F_M);
  tft.setTextColor(proMode ? proText : UI_TEXT, TFT_BLACK);
  tft.drawString(t, 14, 22);
  // the signature: a short accent dash under every title
  tft.fillRoundRect(15, 37, 18, 3, 1, getAccent());
  uiResetFont();
  drawTitleStatus(true);
}

void drawBackButton(const char* label = "BACK") {
  int y = tft.height() - 38;
  drawThemedButton(5, y, 90, 32, label, getBtnFill(), getTextAccent());
}



// Section label: small caps in muted ink
void uiLabel(int x, int y, const char* s, uint16_t bg, uint16_t col = 0) {
  tft.setFont(F_XS);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col ? col : UI_MUTE, bg);
  tft.drawString(s, x, y);
}

// Right-pointing chevron used on navigation tiles
void drawChevron(int x, int cy, uint16_t c) {
  for (int i = 0; i < 2; i++) {
    tft.drawLine(x + i, cy - 5, x + 5 + i, cy, c);
    tft.drawLine(x + 5 + i, cy, x + i, cy + 5, c);
  }
}

// ---------- GPS pins screen ----------
bool gpsPinConflicts(int pin) {
  if (pin < 0) return false;
  // NM-CYD-C5 reserved: shared SPI + TFT/touch/SD
  const int bad[] = {1, 2, 6, 7, 10, 16, 17, 18, 19, 20, 22, 23, 24, 25};
  for (int b : bad) if (pin == b) return true;
  return false;
}

void loadGpsPins() {
  Preferences prefs;
  if (!prefs.begin("skirf", true)) return;
  if (prefs.isKey("gpsRx")) gpsRxPin = prefs.getInt("gpsRx", 4);
  if (prefs.isKey("gpsTx")) gpsTxPin = prefs.getInt("gpsTx", 5);
  prefs.end();
  if (gpsRxPin < -1 || gpsRxPin > 28) gpsRxPin = 4;
  if (gpsTxPin < -1 || gpsTxPin > 28) gpsTxPin = 5;

  if (gpsPinConflicts(gpsRxPin) || gpsPinConflicts(gpsTxPin)) {
    Serial.printf("GPS pins RX=%d TX=%d conflict — resetting to 4/5\n",
                  gpsRxPin, gpsTxPin);
    gpsRxPin = 4;
    gpsTxPin = 5;
    Preferences wp;
    if (wp.begin("skirf", false)) {
      wp.putInt("gpsRx", gpsRxPin);
      wp.putInt("gpsTx", gpsTxPin);
      wp.end();
    }
  }

  gpsPinEditRx = gpsRxPin;
  gpsPinEditTx = gpsTxPin;
  Serial.printf("GPS pins loaded RX=%d TX=%d\n", gpsRxPin, gpsTxPin);
}

void saveGpsPinsAndRestart() {
  gpsRxPin = gpsPinEditRx;
  gpsTxPin = gpsPinEditTx;
  if (gpsPinConflicts(gpsRxPin) || gpsPinConflicts(gpsTxPin)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_BAD, TFT_BLACK);
    tft.setFont(F_S);
    tft.drawString("Pin conflict!", tft.width()/2, tft.height()/2 - 16);
    tft.setFont(F_XS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Avoid 25/32/33/39 (touch)", tft.width()/2, tft.height()/2 + 8);
    tft.drawString("Avoid 12-15/2 (TFT) 5/18/19/23 (SD)", tft.width()/2, tft.height()/2 + 24);
    delay(2500);
    drawGpsPinsScreen();
    return;
  }
  Preferences prefs;
  if (prefs.begin("skirf", false)) {
    prefs.putInt("gpsRx", gpsRxPin);
    prefs.putInt("gpsTx", gpsTxPin);
    prefs.end();
  }
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFont(F_S);
  tft.drawString("GPS pins saved", tft.width()/2, tft.height()/2 - 12);
  char buf[40];
  snprintf(buf, sizeof(buf), "RX=%d  TX=%d", gpsRxPin, gpsTxPin);
  tft.setFont(F_XS);
  tft.drawString(buf, tft.width()/2, tft.height()/2 + 12);
  tft.drawString("Restarting...", tft.width()/2, tft.height()/2 + 28);
  beep(2, 60);
  delay(1200);
  ESP.restart();
}

void gpsPinsGeom(int &hintY, int &boxY, int &boxH, int &bw,
                 int &stepY, int &stepH, int &presetY, int &pw, int &ph,
                 int &saveY, int &saveH, int &pGap) {
  int w = tft.width();
  int h = tft.height();
  bool land = (w > h);
  saveH = 32;
  saveY = h - 38;
  bw = (w - 24) / 2;
  pw = bw;
  if (land) {
    hintY = 50;
    boxH = 34;
    boxY = hintY + 16;
    stepH = 28;
    stepY = boxY + boxH + 6;
    ph = 26;
    pGap = 4;
    presetY = stepY + stepH + 8;  // first preset button
    return;
  }
  // Portrait: even gaps in the band between title and BACK/SAVE.
  // presetY = first preset button Y so the second row cannot hit the dock.
  const int top = 54;
  const int bot = saveY - 8;
  const int hintH = 16;
  const int labelH = 14;
  boxH = 40;
  stepH = 32;
  ph = 28;
  int widgets = hintH + boxH + stepH + labelH + 2 * ph;
  int slack = (bot - top) - widgets;
  if (slack < 20) slack = 20;
  int g = slack / 5;  // hint→box, box→step, step→label, label→presets, preset row gap
  pGap = g;
  hintY = top;
  boxY = hintY + hintH + g;
  stepY = boxY + boxH + g;
  presetY = stepY + stepH + g + labelH + g;
}

static const int gpsPresetRx[4] = {4, 4, 3, 8};
static const int gpsPresetTx[4] = {5, -1, -1, 9};
static const char* gpsPresetLab[4] = {"4 / 5", "4 / -1", "3 / -1", "8 / 9"};

void gpsPinsClampEdit() {
  if (gpsPinEditRx < -1) gpsPinEditRx = 28;
  if (gpsPinEditRx > 28) gpsPinEditRx = -1;
  if (gpsPinEditTx < -1) gpsPinEditTx = 28;
  if (gpsPinEditTx > 28) gpsPinEditTx = -1;
}

void drawGpsPinsScreen() {
  int w = tft.width();
  int h = tft.height();
  int hintY, boxY, boxH, bw, stepY, stepH, presetY, pw, ph, saveY, saveH, pGap;
  gpsPinsGeom(hintY, boxY, boxH, bw, stepY, stepH, presetY, pw, ph, saveY, saveH, pGap);

  tft.fillScreen(TFT_BLACK);
  drawTitleBar("GPS PINS");

  bool rxBad = gpsPinConflicts(gpsPinEditRx);
  bool txBad = gpsPinConflicts(gpsPinEditTx);
  tft.setFont(F_XS);
  tft.setTextDatum(TC_DATUM);
  if (rxBad || txBad) {
    tft.setTextColor(C_BAD, TFT_BLACK);
    tft.drawString(rxBad && txBad ? "RX+TX reserved on C5" :
                   (rxBad ? "RX pin reserved on C5" : "TX pin reserved on C5"),
                   w / 2, hintY + 6);
  } else {
    tft.setTextColor(useChromeHud() ? UI_MUTE : TFT_DARKGREY, TFT_BLACK);
    tft.drawString("TX=-1 if module is NMEA-only", w / 2, hintY + 6);
  }

  uint16_t rxFill = useChromeHud() ? uiCardFill() : getBtnFill();
  uint16_t txFill = rxFill;
  uint16_t rxBd = gpsEditFocusRx ? getAccent() : (useChromeHud() ? UI_LINE : getAccent());
  uint16_t txBd = !gpsEditFocusRx ? getAccent() : (useChromeHud() ? UI_LINE : getAccent());
  if (gpsEditFocusRx) rxFill = blend565(rxFill, getAccent(), 40);
  else txFill = blend565(txFill, getAccent(), 40);
  (void)rxBd; (void)txBd;
  tft.fillRoundRect(8, boxY, bw, boxH, 12, rxFill);
  tft.fillRoundRect(16 + bw, boxY, bw, boxH, 12, txFill);

  char b1[16], b2[16];
  snprintf(b1, sizeof(b1), "RX %d", gpsPinEditRx);
  snprintf(b2, sizeof(b2), "TX %d", gpsPinEditTx);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, rxFill);
  tft.setFont(F_M);
  tft.drawString(b1, 8 + bw / 2, boxY + boxH / 2);
  tft.setTextColor(TFT_WHITE, txFill);
  tft.drawString(b2, 16 + bw + bw / 2, boxY + boxH / 2);

  int stepW = (w - 24) / 2;
  drawThemedButton(8, stepY, stepW, stepH, "-", getBtnFill(), TFT_WHITE);
  drawThemedButton(16 + stepW, stepY, stepW, stepH, "+", getBtnFill(), TFT_WHITE);

  tft.setFont(F_XS);
  tft.setTextColor(useChromeHud() ? UI_MUTE : TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  int labelY = (w < h) ? ((stepY + stepH + presetY) / 2) : (presetY - 8);
  tft.drawString("C5 presets", w / 2, labelY);

  for (int i = 0; i < 4; i++) {
    int col = i % 2, row = i / 2;
    int x = 8 + col * (pw + 8);
    int y = presetY + row * (ph + pGap);
    bool on = (gpsPinEditRx == gpsPresetRx[i] && gpsPinEditTx == gpsPresetTx[i]);
    drawThemedButton(x, y, pw, ph, gpsPresetLab[i], on ? TFT_GREEN : getBtnFill(), TFT_WHITE);
  }

  uiResetFont();
  drawThemedButton(8 + 100, saveY, w - 16 - 100, saveH, "SAVE", TFT_GREEN, TFT_WHITE);
  drawBackButton("BACK");
  tft.setFont(F_XS);
}

// GPS pins touch uses the same geom as drawGpsPinsScreen.



// ---------- Saved settings (NVS) + touch calibration ----------
void drawSlider(int x, int y, int w, int h, int minV, int maxV, int val, uint16_t color) {
  tft.fillRect(x - 12, y - 14, w + 24, h + 28, TFT_BLACK);
  int fillW = map(val, minV, maxV, 0, w);
  fillW = constrain(fillW, 0, w);
  tft.fillRoundRect(x, y, w, h, h / 2, C565(30, 33, 42));
  tft.drawRoundRect(x, y, w, h, h / 2, UI_LINE);
  if (fillW > h / 2) {
    // gradient fill: dim at the left, full accent at the knob
    for (int i = 0; i < fillW; i++) {
      uint16_t c = blend565(blend565(TFT_BLACK, color, 90), color, (uint8_t)(i * 255 / max(1, fillW)));
      int inset = (i < h / 2) ? (h / 2 - (int)sqrtf((float)(h / 2) * (h / 2) - (float)(h / 2 - i) * (h / 2 - i))) : 0;
      tft.drawFastVLine(x + i, y + 2 + inset, h - 4 - 2 * inset, c);
    }
  }
  int kx = x + fillW;
  tft.fillCircle(kx, y + h / 2, h / 2 + 5, TFT_BLACK);
  tft.fillCircle(kx, y + h / 2, h / 2 + 3, color);
  tft.fillCircle(kx, y + h / 2, h / 2 - 2, UI_TEXT);
}

// v1/v2 saved the raw "pure" accents; map them onto the refined v3 palette
extern const uint16_t colorOptions[];
uint16_t migrateAccent(uint16_t v) {
  static const uint16_t old[8] = { C565(0, 255, 0), C565(0, 255, 255), C565(255, 255, 0), C565(255, 160, 0),
                                   C565(255, 0, 0), C565(255, 0, 255), C565(0, 0, 255), C565(255, 255, 255) };
  for (int i = 0; i < 8; i++) if (v == old[i]) return colorOptions[i];
  return v;
}

bool loadCalibration() {
  calPrefs.begin("skirf_cal", true);
  // ST7789 + touch pipeline change — old affine is wrong; force new 3-point cal
  if (calPrefs.getInt("uiVer", 0) < 6) {
    Serial.println("Cal invalidated (uiVer<6) — fresh calibration required");
    calPrefs.end();
    return false;
  }
  if (calPrefs.getBool("ok", false)) {
    lgfxCalOk = calPrefs.getBool("lgfxok", false);
    for (int i = 0; i < 8; i++) {
      char k[8]; snprintf(k, sizeof(k), "lc%d", i);
      lgfxCal[i] = (uint16_t)calPrefs.getUShort(k, 0);
    }
    calAffineOk = calPrefs.getBool("affine", false);
    calAx = calPrefs.getFloat("ax", 0);
    calBx = calPrefs.getFloat("bx", 0);
    calDx = calPrefs.getFloat("dx", 0);
    calAy = calPrefs.getFloat("ay", 0);
    calBy = calPrefs.getFloat("by", 0);
    calDy = calPrefs.getFloat("dy", 0);
    calXMin = calPrefs.getInt("xMin", 200);
    calXMax = calPrefs.getInt("xMax", 3900);
    calYMin = calPrefs.getInt("yMin", 200);
    calYMax = calPrefs.getInt("yMax", 3900);
    brightness = calPrefs.getInt("bri", 255);
    highContrast = false; // contrast option removed
    accentColor = migrateAccent(calPrefs.getUShort("acc", accentColor));
    wdgwTheme = calPrefs.getBool("wdgw", false);
    proMode = calPrefs.getBool("pro", false);
    proWifiOn = calPrefs.getBool("pWifi", true);
    proBleOn = calPrefs.getBool("pBle", true);
    proBleDuty = calPrefs.getInt("pDuty", 1);
    proIntervalMs = calPrefs.getInt("pInt", 8000);
    proWifiRssiMin = calPrefs.getInt("pWmin", -95);
    proWifiRssiMax = calPrefs.getInt("pWmax", -25);
    proBleRssiMin = calPrefs.getInt("pBmin", -100);
    proBleRssiMax = calPrefs.getInt("pBmax", -25);
    proGpsGate = calPrefs.getBool("pGps", false);
    proSkipOpen = calPrefs.getBool("pOpen", false);
    proDedupe = calPrefs.getBool("pDed", true);
    proChanMode = calPrefs.getInt("pCh", 0);
    proAccMode = calPrefs.getInt("pAcc", 0);
    preferredRotation = calPrefs.getInt("rot", 0);
    calRotation = calPrefs.getInt("calRot", preferredRotation);
    calPrefs.end();
    Serial.printf("Cal NVS affine=%d\n", (int)calAffineOk);
    return true;
  }
  calPrefs.end();

  if (!sdReady) {
    if (!SD.begin(SD_CS)) return false;
    sdReady = true;
  }
  if (!SD.exists(CAL_FILE)) return false;
  File f = SD.open(CAL_FILE, FILE_READ);
  if (!f) return false;
  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();
  calAffineOk = doc["affine"] | false;
  calAx = doc["ax"] | 0.0;
  calBx = doc["bx"] | 0.0;
  calDx = doc["dx"] | 0.0;
  calAy = doc["ay"] | 0.0;
  calBy = doc["by"] | 0.0;
  calDy = doc["dy"] | 0.0;
  calXMin = doc["xMin"] | 200;
  calXMax = doc["xMax"] | 3900;
  calYMin = doc["yMin"] | 200;
  calYMax = doc["yMax"] | 3900;
  brightness = doc["brightness"] | 255;
  highContrast = false; // contrast option removed
  accentColor = migrateAccent(doc["accentColor"] | accentColor);
  wdgwTheme = doc["wdgwTheme"] | false;
  preferredRotation = doc["rotation"] | 0;
  calRotation = doc["calRotation"] | preferredRotation;
  Serial.println("Cal loaded from SD");
  return true;
}

void saveCalibration() {
  calPrefs.begin("skirf_cal", false);
  calPrefs.putBool("lgfxok", lgfxCalOk);
  for (int i = 0; i < 8; i++) {
    char k[8]; snprintf(k, sizeof(k), "lc%d", i);
    calPrefs.putUShort(k, lgfxCal[i]);
  }
  calPrefs.putBool("affine", calAffineOk);
  calPrefs.putFloat("ax", calAx);
  calPrefs.putFloat("bx", calBx);
  calPrefs.putFloat("dx", calDx);
  calPrefs.putFloat("ay", calAy);
  calPrefs.putFloat("by", calBy);
  calPrefs.putFloat("dy", calDy);
  calPrefs.putInt("xMin", calXMin);
  calPrefs.putInt("xMax", calXMax);
  calPrefs.putInt("yMin", calYMin);
  calPrefs.putInt("yMax", calYMax);
  calPrefs.putInt("bri", brightness);
  calPrefs.putBool("hc", highContrast);
  calPrefs.putUShort("acc", accentColor);
  calPrefs.putBool("wdgw", wdgwTheme);
  // clear flags left by themes that were removed before Beta 1
  calPrefs.putBool("lcars", false);
  calPrefs.putBool("kitt", false);
  calPrefs.putBool("jaws", false);
  calPrefs.putBool("matrix", false);
  calPrefs.putBool("pro", proMode);
  calPrefs.putBool("pWifi", proWifiOn);
  calPrefs.putBool("pBle", proBleOn);
  calPrefs.putInt("pDuty", proBleDuty);
  calPrefs.putInt("pInt", proIntervalMs);
  calPrefs.putInt("pWmin", proWifiRssiMin);
  calPrefs.putInt("pWmax", proWifiRssiMax);
  calPrefs.putInt("pBmin", proBleRssiMin);
  calPrefs.putInt("pBmax", proBleRssiMax);
  calPrefs.putBool("pGps", proGpsGate);
  calPrefs.putBool("pOpen", proSkipOpen);
  calPrefs.putBool("pDed", proDedupe);
  calPrefs.putInt("pCh", proChanMode);
  calPrefs.putInt("pAcc", proAccMode);
  calPrefs.putInt("rot", preferredRotation);
  calPrefs.putInt("calRot", calRotation);
  calPrefs.putBool("ok", true);
  calPrefs.putInt("uiVer", 6);
  calPrefs.end();
  Serial.println("Cal saved to NVS (affine)");

  if (!sdReady) {
    if (SD.begin(SD_CS)) sdReady = true;
  }
  if (!sdReady) return;
  if (SD.exists(CAL_FILE)) SD.remove(CAL_FILE);
  File f = SD.open(CAL_FILE, FILE_WRITE);
  if (!f) return;
  StaticJsonDocument<384> doc;
  doc["affine"] = calAffineOk;
  doc["ax"] = calAx; doc["bx"] = calBx; doc["dx"] = calDx;
  doc["ay"] = calAy; doc["by"] = calBy; doc["dy"] = calDy;
  doc["xMin"] = calXMin; doc["xMax"] = calXMax;
  doc["yMin"] = calYMin; doc["yMax"] = calYMax;
  doc["brightness"] = brightness;
  doc["highContrast"] = highContrast;
  doc["accentColor"] = accentColor;
  doc["wdgwTheme"] = wdgwTheme;
  doc["rotation"] = preferredRotation;
  doc["calRotation"] = calRotation;
  serializeJson(doc, f);
  f.close();
  Serial.println("Cal saved to SD");
}

void drawCrosshair(int x, int y, uint16_t color) {
  tft.drawFastHLine(x - 12, y, 24, color);
  tft.drawFastVLine(x, y - 12, 24, color);
  tft.drawCircle(x, y, 6, color);
}

bool c5RawTouch(int &rx, int &ry) {
  lgfx::touch_point_t tp;
  if (tft.getTouchRaw(&tp, 1) < 1) return false;
  if (tp.x < 10 || tp.y < 10 || tp.x > 4090 || tp.y > 4090) return false;
  rx = tp.x; ry = tp.y;
  return true;
}

CalPoint waitForTap() {
  int rx, ry;
  while (c5RawTouch(rx, ry)) delay(10);
  delay(80);
  while (true) {
    if (c5RawTouch(rx, ry)) {
      delay(25);
      int ax, ay, n = 0;
      long sx = 0, sy = 0;
      unsigned long t0 = millis();
      while (millis() - t0 < 200) {
        if (c5RawTouch(ax, ay)) { sx += ax; sy += ay; n++; }
        delay(8);
      }
      if (n >= 2) { rx = sx / n; ry = sy / n; }
      while (c5RawTouch(ax, ay)) delay(8);
      delay(100);
      return {rx, ry};
    }
    delay(8);
  }
}


bool computeAffine(const float sx[3], const float sy[3],
                   const float rx[3], const float ry[3]) {
  float det = rx[0]*(ry[1]-ry[2]) + rx[1]*(ry[2]-ry[0]) + rx[2]*(ry[0]-ry[1]);
  if (fabsf(det) < 1e-3f) {
    Serial.println("Affine: degenerate points");
    return false;
  }
  calAx = (sx[0]*(ry[1]-ry[2]) + sx[1]*(ry[2]-ry[0]) + sx[2]*(ry[0]-ry[1])) / det;
  calBx = (sx[0]*(rx[2]-rx[1]) + sx[1]*(rx[0]-rx[2]) + sx[2]*(rx[1]-rx[0])) / det;
  calDx = (sx[0]*(rx[1]*ry[2]-rx[2]*ry[1]) + sx[1]*(rx[2]*ry[0]-rx[0]*ry[2]) + sx[2]*(rx[0]*ry[1]-rx[1]*ry[0])) / det;
  calAy = (sy[0]*(ry[1]-ry[2]) + sy[1]*(ry[2]-ry[0]) + sy[2]*(ry[0]-ry[1])) / det;
  calBy = (sy[0]*(rx[2]-rx[1]) + sy[1]*(rx[0]-rx[2]) + sy[2]*(rx[1]-rx[0])) / det;
  calDy = (sy[0]*(rx[1]*ry[2]-rx[2]*ry[1]) + sy[1]*(rx[2]*ry[0]-rx[0]*ry[2]) + sy[2]*(rx[0]*ry[1]-rx[1]*ry[0])) / det;
  calAffineOk = true;
  Serial.printf("Affine X: %.6f %.6f %.2f\n", calAx, calBx, calDx);
  Serial.printf("Affine Y: %.6f %.6f %.2f\n", calAy, calBy, calDy);
  return true;
}

void runCalibration() {
  delay(300);
  int dumpX, dumpY;
  while (c5RawTouch(dumpX, dumpY)) delay(10);

  if (preferredRotation < 0 || preferredRotation > 3) preferredRotation = 0;
  tft.setRotation(preferredRotation);
  touch.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFont(F_S);
  tft.drawString("TOUCH CAL", tft.width()/2, tft.height()/2 - 12);
  tft.setFont(F_XS);
  tft.drawString("Tap the four corners", tft.width()/2, tft.height()/2 + 10);
  delay(700);

  uint16_t cal[8] = {0};
  tft.calibrateTouch(cal, TFT_WHITE, TFT_BLACK, 20);
  memcpy(lgfxCal, cal, sizeof(lgfxCal));
  lgfxCalOk = true;
  tft.setTouchCalibrate(lgfxCal);
  calAffineOk = false;
  calRotation = preferredRotation;
  saveCalibration();
  calLoaded = true;

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setFont(F_S);
  tft.drawString("Cal saved", tft.width()/2, tft.height()/2);
  tft.setFont(F_XS);
  tft.drawString("Restarting...", tft.width()/2, tft.height()/2 + 22);
  beep(2, 70);
  delay(1200);
  ESP.restart();
}



unsigned long lastUiTouchMs = 0;


bool getTouchMapped(int &tx, int &ty) {
  tft.endWrite();
  if (lgfxCalOk) {
    uint16_t lx, ly;
    // drop a stale sample after a redraw
    tft.getTouch(&lx, &ly);
    delay(2);
    if (tft.getTouch(&lx, &ly)) {
      tx = (int)lx;
      ty = (int)ly;
      return true;
    }
    return false;
  }
  float rawX = 0, rawY = 0;
  bool got = false;
  {
    tft.endWrite();
    lgfx::touch_point_t tp;
    int xs[9], ys[9], hits = 0;
    for (int k = 0; k < 9 && hits < 9; k++) {
      if (tft.getTouchRaw(&tp, 1) < 1) { delayMicroseconds(200); continue; }
      if (tp.x < 10 || tp.y < 10 || tp.x > 4090 || tp.y > 4090) continue;
      xs[hits] = tp.x; ys[hits] = tp.y; hits++;
      delayMicroseconds(250);
    }
    if (hits >= 3) {
      for (int a = 0; a < hits - 1; a++)
        for (int b = a + 1; b < hits; b++) {
          if (xs[b] < xs[a]) { int u = xs[a]; xs[a] = xs[b]; xs[b] = u; }
          if (ys[b] < ys[a]) { int u = ys[a]; ys[a] = ys[b]; ys[b] = u; }
        }
      rawX = (float)xs[hits / 2];
      rawY = (float)ys[hits / 2];
      got = true;
    }
  }
  if (!got && touch.touched()) {
    TS_Point p = touch.getPoint();
    rawX = p.x; rawY = p.y;
    got = (p.x > 10 && p.y > 10);
  }
  if (!got) return false;

  int w = tft.width();
  int h = tft.height();

  if (calAffineOk) {
    float fx = calAx * rawX + calBx * rawY + calDx;
    float fy = calAy * rawX + calBy * rawY + calDy;
    tx = constrain((int)(fx + 0.5f), 0, w - 1);
    ty = constrain((int)(fy + 0.5f), 0, h - 1);
  } else {
    // Uncalibrated fallback — still usable enough to hit big targets
    int left = calXMin, right = calXMax, top = calYMin, bot = calYMax;
    if (left == right) { left = 200; right = 3900; }
    if (top == bot) { top = 200; bot = 3900; }
    tx = constrain(map((int)rawX, left, right, 0, w - 1), 0, w - 1);
    ty = constrain(map((int)rawY, top, bot, 0, h - 1), 0, h - 1);
  }
  return true;
}



// ---------- GPS serial: module set-up and reading ----------
// Send "$<body>*CS\r\n" with a correct NMEA checksum
void gpsSendNmea(const char* body) {
  uint8_t cs = 0;
  for (const char* p = body; *p; p++) cs ^= (uint8_t)*p;
  char tail[8];
  snprintf(tail, sizeof(tail), "*%02X\r\n", cs);
  gpsSerial.print('$');
  gpsSerial.print(body);
  gpsSerial.print(tail);
  delay(25);
}

// Optional: tune the module (sentences / rate). OFF by default: every common module
// (ATGM336H, NEO-6M/M8N, PA6H) already sends GGA, GSA, GSV and RMC at 1 Hz and 9600
// baud from the factory, which is all the wardriver needs. Sending set-up commands
// meant for one maker to another maker's module can stop it talking until it is
// powered off, so the board only listens unless you turn this on.
#define SKIRF_GPS_CONFIGURE 0
void gpsApplyConfig() {
#if SKIRF_GPS_CONFIGURE
  gpsSendNmea("PCAS03,1,0,1,1,1,0,0,0");   // ATGM336H / CASIC: GGA GSA GSV RMC
  gpsSendNmea("PCAS02,1000");              // 1 Hz
#endif
}

static void gpsPumpUart() {
  int n;
  while ((n = gpsSerial.available()) > 0) {
    uint8_t tmp[128];
    int k = gpsSerial.read(tmp, n < 128 ? n : 128);
    if (k <= 0) break;
    for (int i = 0; i < k; i++) {
      uint32_t h = gpsRingHead, nh = (h + 1) % GPS_RING_SZ;
      if (nh == gpsRingTail) { gpsRingDrops++; continue; }
      gpsRing[h] = tmp[i];
      gpsRingHead = nh;
    }
  }
}
#ifndef SKIRF_HOST_PREVIEW
static void gpsPumpTask(void*) {
  for (;;) {
    if (xSemaphoreTake(gpsPortMux, pdMS_TO_TICKS(50)) == pdTRUE) {
      gpsPumpUart();
      xSemaphoreGive(gpsPortMux);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
#endif

void gpsOpenPort(int rx, int tx, uint32_t baud) {
  if (!gpsRing) {
#ifndef SKIRF_HOST_PREVIEW
    gpsRing = (uint8_t*)heap_caps_malloc(GPS_RING_SZ, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);   // keep internal RAM for Wi-Fi, BLE and the SD card
#endif
    if (!gpsRing) gpsRing = (uint8_t*)malloc(GPS_RING_SZ);
#ifndef SKIRF_HOST_PREVIEW
    gpsPortMux = xSemaphoreCreateMutex();
    xTaskCreate(gpsPumpTask, "gpsPump", 3072, nullptr, 5, nullptr);   // above loop(): never starved
#endif
  }
#ifndef SKIRF_HOST_PREVIEW
  xSemaphoreTake(gpsPortMux, portMAX_DELAY);
#endif
  gpsSerial.end();
  delay(30);
  gpsSerial.setRxBufferSize(4096);
  gpsSerial.onReceiveError([](hardwareSerial_error_t) { gpsUartErrs++; });   // FIFO / buffer overflow, framing
  gpsSerial.begin(baud, SERIAL_8N1, rx, tx);
  gpsRingTail = gpsRingHead;
  gpsNavModeSent = -1;
#ifndef SKIRF_HOST_PREVIEW
  xSemaphoreGive(gpsPortMux);
#endif
  gpsBaudNow = baud;
  gpsChars = 0;
  gpsLastByteMs = millis();
  gpsCharsAtOpen = gps.charsProcessed();
  gpsPassAtOpen = gps.passedChecksum();
  Serial.printf("GPS UART RX=%d TX=%d baud=%lu\n", rx, tx, (unsigned long)baud);
  delay(80);
  if (baud == 9600 && rx == gpsRxPin) gpsApplyConfig();
}

// TinyGPS++ only accepts GGA / RMC from the talkers GP, GN, GA, GB and GL. A GPS + BeiDou
// module (ATGM336H and friends) labels a sentence "BD" whenever BeiDou alone carries the fix,
// and some firmware flips between GN, GP and BD from one second to the next. Those sentences
// used to be thrown away silently, so the position and speed froze and the fix kept dropping.
// Relabel them as GN (fixing the checksum) before the parser sees them.
static void gpsFeedLine(char* s, int len) {
  if (len >= 7 && s[0] == '$' && (!strncmp(s + 3, "GGA", 3) || !strncmp(s + 3, "RMC", 3) || !strncmp(s + 3, "GSA", 3))) {
    bool known = s[1] == 'G' && strchr("PNABL", s[2]) != NULL;
    char* star = strchr(s, '*');
    if (!known && star && star - s > 6 && star[1] && star[2]) {
      auto hx = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1)); };
      int h1 = hx(star[1]), h2 = hx(star[2]);
      if (h1 >= 0 && h2 >= 0) {
        uint8_t cs = (uint8_t)(h1 * 16 + h2);
        cs ^= (uint8_t)s[1] ^ (uint8_t)s[2] ^ (uint8_t)'G' ^ (uint8_t)'N';
        s[1] = 'G'; s[2] = 'N';
        static const char HX[] = "0123456789ABCDEF";
        star[1] = HX[cs >> 4]; star[2] = HX[cs & 15];
        gpsRelabelCount++;
      }
    }
  }
  if (len >= 7 && s[0] == '$') {
    bool isGGA = !strncmp(s + 3, "GGA", 3), isRMC = !strncmp(s + 3, "RMC", 3);
    if (isGGA || isRMC) {
      int want = isGGA ? 6 : 2, commas = 0; const char* q = s;
      while (*q && commas < want) { if (*q == ',') commas++; q++; }
      gpsWinPos++;
      if (commas == want && (isGGA ? (*q > '0' && *q <= '9') : *q == 'A')) gpsWinFix++;
    }
  }
  for (int i = 0; i < len; i++) gps.encode(s[i]);
  gps.encode('\r'); gps.encode('\n');
}

void readGPS() {
#ifdef SKIRF_HOST_PREVIEW
  if (gpsRing) gpsPumpUart();
#endif
  {   // loop health: the longest gap between two readGPS() calls
    unsigned long now = millis();
    if (gpsLoopLastMs && now - gpsLoopLastMs > gpsLoopMaxMs) gpsLoopMaxMs = now - gpsLoopLastMs;
    gpsLoopLastMs = now;
    if (now - gpsWinStart >= 5000) {
      unsigned long lost = gpsRingDrops + gpsUartErrs;
      gpsDiagFixPct = gpsWinPos ? (int)(gpsWinFix * 100 / gpsWinPos) : -1;
      gpsDiagBad = gps.failedChecksum() - gpsWinFail0;
      gpsDiagLost = lost - gpsWinLost0;
      gpsDiagLag = gpsLoopMaxMs;
      gpsWinStart = now; gpsWinPos = gpsWinFix = 0; gpsWinFail0 = gps.failedChecksum(); gpsWinLost0 = lost; gpsLoopMaxMs = 0;
    }
  }
  while (gpsRing && gpsRingTail != gpsRingHead) {
    char c = (char)gpsRing[gpsRingTail];
    gpsRingTail = (gpsRingTail + 1) % GPS_RING_SZ;
    gpsChars++;
    gpsLastByteMs = millis();
    if (gpsChars < 400) Serial.write(c);   // first bytes after each (re)open, for diagnosis
    if (c == '$') { gpsLineLen = 0; gpsLine[gpsLineLen++] = c; continue; }
    if (c == '\n' || c == '\r') {
      if (gpsLineLen > 6 && gpsLine[0] == '$') {
        gpsLine[gpsLineLen] = 0;
        if (!strncmp(gpsLine + 3, "TXT", 3)) {           // $xxTXT,nn,nn,nn,text*CS
          const char* s = gpsLine; int commas = 0;
          while (*s && commas < 4) { if (*s == ',') commas++; s++; }
          int k = 0; while (s[k] && s[k] != '*' && k < (int)sizeof(gpsTxt) - 1) { gpsTxt[k] = s[k]; k++; } gpsTxt[k] = 0;
          gpsTxtMs = millis();
          if (strstr(gpsTxt, "ANTENNA OPEN")) strcpy(gpsAnt, "OPEN");
          else if (strstr(gpsTxt, "ANTENNA SHORT")) strcpy(gpsAnt, "SHORT");
          else if (strstr(gpsTxt, "ANTENNA OK")) strcpy(gpsAnt, "OK");
          Serial.printf("GPS module says: %s\n", gpsTxt);
        }
        gpsFeedLine(gpsLine, gpsLineLen);
      }
      gpsLineLen = 0;
    } else if (gpsLineLen > 0 && gpsLineLen < (int)sizeof(gpsLine) - 1) gpsLine[gpsLineLen++] = c;
    else if (gpsLineLen > 0) { gps.encode('$'); gpsLineLen = 0; }   // over-long line: count it as garbage, start over
    else gps.encode(c);   // stray bytes between sentences still count, so a wrong baud is spotted
  }
  if (gps.location.isValid() && gps.location.age() < 4000) {
    gpsLat = gps.location.lat();
    gpsLng = gps.location.lng();
    if (gpsLat != gpsRawLat || gpsLng != gpsRawLng) { gpsRawLat = gpsLat; gpsRawLng = gpsLng; gpsMovedMs = millis(); }
    gpsValid = true;
  } else if (!gps.location.isValid() || gps.location.age() > 8000) {
    gpsValid = false;
  }
  if (gps.satellites.isValid()) {
    int s = (int)gps.satellites.value();
    if (s >= 0 && s <= 36) gpsSats = s;
  }
  {
    TinyGPSCustom* g5[6] = { &gsvGP, &gsvGL, &gsvGB, &gsvBD, &gsvGA, &gsvGN };
    unsigned long now = millis();
    int sum = 0;
    for (int i = 0; i < 6; i++) {
      if (g5[i]->isUpdated()) { gsvN[i] = atoi(g5[i]->value()); gsvMs[i] = now; }
      if (gsvMs[i] && now - gsvMs[i] < 5000) sum += gsvN[i];
    }
    gpsInView = sum;
  }
  if (millis() - gpsDiagMs > 10000) {
    gpsDiagMs = millis();
    if (gpsAnt[0]) Serial.printf("GPS antenna: %s\n", gpsAnt);
    Serial.printf("GPS: RX=%d TX=%d baud=%lu last byte %lus ago chars=%lu ok=%lu bad=%lu relabelled=%lu used=%d view=%d fix=%d hdop=%.1f | 5s: fix %d%% bad %lu lost %lu lag %lums moved %lus ago\n",
                  gpsPinsSwapped ? gpsTxPin : gpsRxPin, gpsPinsSwapped ? gpsRxPin : gpsTxPin,
                  (unsigned long)gpsBaudNow, (millis() - gpsLastByteMs) / 1000, (unsigned long)gps.charsProcessed(),
                  (unsigned long)gps.passedChecksum(), (unsigned long)gps.failedChecksum(), gpsRelabelCount,
                  gpsSats, gpsInView, (int)gpsValid, gps.hdop.isValid() ? gps.hdop.hdop() : 0.0,
                  gpsDiagFixPct, gpsDiagBad, gpsDiagLost, gpsDiagLag, gpsMovedMs ? (millis() - gpsMovedMs) / 1000 : 0);
  }
  if (gps.course.isValid()) gpsCourse = gps.course.deg();
  if (gps.speed.isValid()) gpsSpeed = gps.speed.mph();
  if (gps.altitude.isValid()) gpsAlt = gps.altitude.meters();

  // Hunt for the right baud / pin order only when the link is silent, or when bytes
  // arrive but not one sentence has passed its checksum (a module at another baud
  // produces exactly that). A module that is talking sensibly is never touched.
  if (!gpsLinkLocked && gps.passedChecksum() > gpsPassAtOpen + 2) {
    gpsLinkLocked = true;
    Serial.printf("GPS link locked: RX=%d TX=%d baud=%lu\n", gpsPinsSwapped ? gpsTxPin : gpsRxPin, gpsPinsSwapped ? gpsRxPin : gpsTxPin, (unsigned long)gpsBaudNow);
  }
  // Dynamic model for CASIC modules (ATGM336H and other AT6558 boards). Many ship in vehicle
  // mode, which holds the position still at walking speed, so the fix updates in bursts.
  // Portable on the GPS page, walking or vehicle during a run to match the ride. Not saved in
  // the module (no $PCAS00), and modules of other makes ignore the sentence.
  if (gpsLinkLocked && gpsTxPin >= 0) {
    int want = wardriveActive ? (wdTransport == WD_XPORT_WALK ? 2 : 3) : 0;
    if (want != gpsNavModeSent) {
      char cmd[16]; snprintf(cmd, sizeof(cmd), "PCAS11,%d", want);
      gpsSendNmea(cmd);
      gpsNavModeSent = want;
      Serial.printf("GPS: dynamic model $%s (%s)\n", cmd, want == 0 ? "portable" : (want == 2 ? "walking" : "vehicle"));
    }
  }
  if (!gpsLinkLocked && !gpsValid && (millis() - gpsLastHuntMs > 8000)) {
    gpsLastHuntMs = millis();
    bool quiet = (millis() - gpsLastByteMs > 4000);
    bool garbled = (gps.charsProcessed() - gpsCharsAtOpen > 600) && (gps.passedChecksum() == gpsPassAtOpen);
    Serial.printf("GPS hunt valid=%d sats=%d view=%d chars=%lu baud=%lu swapped=%d quiet=%d garbled=%d\n",
                  (int)gpsValid, gpsSats, gpsInView, (unsigned long)gpsChars,
                  (unsigned long)gpsBaudNow, (int)gpsPinsSwapped, (int)quiet, (int)garbled);
    if (quiet || garbled) {
      static const uint32_t bauds[3] = { 9600, 38400, 115200 };
      gpsHuntStep = (gpsHuntStep + 1) % 6;            // 9600, 38400, 115200, then the same with RX/TX swapped
      if (gpsTxPin < 0 && gpsHuntStep >= 3) gpsHuntStep = 0;   // nothing to swap with an RX-only module
      gpsPinsSwapped = gpsHuntStep >= 3;
      uint32_t bd = bauds[gpsHuntStep % 3];
      if (gpsPinsSwapped) gpsOpenPort(gpsTxPin, gpsRxPin, bd);
      else gpsOpenPort(gpsRxPin, gpsTxPin, bd);
    }
  }
}

// ================= CAPTURE ENGINE =================
// Dual-band Wi-Fi via ESP-IDF directly (tuned per-channel dwell), async BLE,
// a session-wide unique table, and batched WiGLE 1.6 CSV writes.

void performScan();
void updateWardrivingScreen();
void drawWardrivingScreen();
void drawWdModeScreen();
void applyTransportProfile(int mode);
void drawProScreen();
void drawProSessionScreen();
void activateProRow(int i);

// Per-transport radio timing (set by applyTransportProfile)
uint16_t wdActMinMs = 40, wdActMaxMs = 90, wdPassiveMs = 110;
uint8_t  wdBleSec = 1;
bool     wdBleActive = false;
bool     wdSkipDfsMost = true;   // CAR: skip passive DFS on 2 of 3 sweeps

void applyTransportProfile(int mode) {
  if (mode < WD_XPORT_CAR) mode = WD_XPORT_CAR;
  if (mode > WD_XPORT_WALK) mode = WD_XPORT_WALK;
  wdTransport = mode;
  wdDoWifi = true;
  wdDoBle = true;
  wdChanFilter = 0;
  wdWifiRssiMax = -20;
  wdBleRssiMax = -20;
  wdGpsGate = true;

  // Speed vs dwell: faster platforms sweep more often with shorter channel
  // dwell; walk/bike dwells longer for a complete dual-band + BLE pass.
  if (mode == WD_XPORT_CAR) {
    wdSweepMs = 2500;  wdActMinMs = 40; wdActMaxMs = 90;  wdPassiveMs = 110;
    wdBleDuty = 0;     wdBleSec = 1;    wdBleActive = false; wdSkipDfsMost = true;
    wdWifiRssiMin = -92; wdBleRssiMin = -88; wdGpsMinSats = 4;
  } else if (mode == WD_XPORT_MOTO) {
    wdSweepMs = 3500;  wdActMinMs = 50; wdActMaxMs = 110; wdPassiveMs = 120;
    wdBleDuty = 1;     wdBleSec = 1;    wdBleActive = false; wdSkipDfsMost = true;
    wdWifiRssiMin = -94; wdBleRssiMin = -92; wdGpsMinSats = 4;
  } else {
    wdSweepMs = 6000;  wdActMinMs = 70; wdActMaxMs = 160; wdPassiveMs = 200;
    wdBleDuty = 2;     wdBleSec = 2;    wdBleActive = true;  wdSkipDfsMost = false;
    wdWifiRssiMin = -98; wdBleRssiMin = -100; wdGpsMinSats = 5;
  }
  Serial.printf("Transport %s: sweep=%lums dwell=%u-%u/%u ble=%us wifi>=%d ble>=%d\n",
                wdTransportShort(-1), wdSweepMs, wdActMinMs, wdActMaxMs, wdPassiveMs,
                wdBleSec, wdWifiRssiMin, wdBleRssiMin);
}

// ---------- Session unique table (open addressing, PSRAM when present) ----------
struct SeenSlot { uint32_t h; int8_t best; };
SeenSlot* seenTab = nullptr;
uint32_t seenCap = 0, seenUsed = 0;
bool seenRelogDb = true;   // re-log a known MAC when RSSI improves >= 10 dB

void seenInit() {
  if (!seenTab) {
    uint32_t cap = 16384;
    size_t bytes = cap * sizeof(SeenSlot);
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM)
    if (psramFound()) seenTab = (SeenSlot*)ps_malloc(bytes);
#endif
    if (!seenTab) { cap = 4096; bytes = cap * sizeof(SeenSlot); seenTab = (SeenSlot*)malloc(bytes); }
    seenCap = seenTab ? cap : 0;
  }
  if (seenTab) memset(seenTab, 0, seenCap * sizeof(SeenSlot));
  seenUsed = 0;
}

static uint32_t macHash(const char* m, uint8_t kind) {
  uint32_t h = 2166136261u ^ kind;
  while (m && *m) { h ^= (uint8_t)toupper(*m++); h *= 16777619u; }
  return h ? h : 1;
}

// 0 = brand new, 1 = known but much stronger now, 2 = duplicate
static int seenCheck(const char* mac, uint8_t kind, int rssi) {
  if (!seenTab || !seenCap) return 0;
  if (seenUsed > (seenCap * 7) / 8) {   // table nearly full: start fresh
    memset(seenTab, 0, seenCap * sizeof(SeenSlot));
    seenUsed = 0;
  }
  uint32_t h = macHash(mac, kind);
  uint32_t mask = seenCap - 1;
  uint32_t i = h & mask;
  while (seenTab[i].h) {
    if (seenTab[i].h == h) {
      if (rssi >= seenTab[i].best + 10) { seenTab[i].best = (int8_t)rssi; return 1; }
      return 2;
    }
    i = (i + 1) & mask;
  }
  seenTab[i].h = h;
  seenTab[i].best = (int8_t)constrain(rssi, -127, 0);
  seenUsed++;
  return 0;
}

// ---------- Session stats ----------
unsigned long wdStartMs = 0;
unsigned long wdSweepStartMs = 0;
unsigned long wdPhaseStartMs = 0;
unsigned long wdLastSweepMs = 0;      // duration of the last full sweep
int  sweepNew24 = 0, sweepNew5 = 0, sweepNewBle = 0;
uint32_t rowsLogged = 0;
bool wdWaitingGps = false;
double wdDistM = 0;
double wdLastLat = 0, wdLastLng = 0;
bool wdHaveLast = false;
int  wdBestRssi = -127;
char wdBestName[24] = "";
uint32_t recBestSession = 0, recLifetime = 0, recSessions = 0;
bool wdNewRecord = false;
uint32_t wdLastSessionTotal = 0;
unsigned long wdLastSessionMs = 0;

uint32_t sessionUnique() { return (uint32_t)(sessionWifi24Total + sessionWifi5Total + sessionBleTotal); }

void loadRecords() {
  Preferences p;
  if (!p.begin("skirf_rec", true)) return;
  recBestSession = p.getUInt("best", 0);
  recLifetime = p.getUInt("life", 0);
  recSessions = p.getUInt("sess", 0);
  p.end();
}
void saveRecords() {
  Preferences p;
  if (!p.begin("skirf_rec", false)) return;
  p.putUInt("best", recBestSession);
  p.putUInt("life", recLifetime);
  p.putUInt("sess", recSessions);
  p.end();
}

// ---------- Storage ----------
bool tryMountSd() {
  if (sdReady) return true;
  SPI.begin(6, 2, 7, SD_CS);  // SCK MISO MOSI CS  NM-CYD-C5
  delay(30);
  const uint32_t speeds[] = { 16000000, 8000000, 4000000, 1000000, 400000 };
  for (uint32_t hz : speeds) {
    if (SD.begin(SD_CS, SPI, hz)) {
      sdReady = true;
      Serial.printf("SD OK @ %lu Hz\n", (unsigned long)hz);
      return true;
    }
    SD.end();
    delay(20);
  }
  Serial.println("SD FAIL — logs will use LittleFS so scans can still run");
  return false;
}

bool openLogFS() {
  if (tryMountSd()) { logUseLittleFS = false; return true; }
  if (LittleFS.begin(true)) { logUseLittleFS = true; return true; }
  return false;
}

// The filesystem that holds wardrive logs (SD, or internal flash fallback)
fs::FS& logFs() {
  if (!logUseLittleFS && sdReady) return SD;
  return LittleFS;
}

File openLogFile(const char* path, const char* mode) {
  return logFs().open(path, mode);
}

// ---------- WiGLE 1.6 CSV writer (batched) ----------
static char wdLogBuf[4096];
static size_t wdLogLen = 0;

bool wdLogFailed = false;          // last write to the log did not reach the card (shown on the run screen)
static const char WD_CSV_HEADER[] =
  "WigleWifi-1.6,appRelease=" SKIRF_APP_RELEASE ",model=NM-CYD-C5,release=" SKIRF_APP_RELEASE
  ",device=SKIrf-Wardriver,display=ST7789,board=ESP32-C5,brand=SKIrf,star=Sol,body=3,subBody=0\n"
  "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n";

static bool wdWriteOnce(const uint8_t* data, size_t len, const char* mode) {
  tft.endWrite();                  // the card shares the display's SPI bus: make sure the display let go of it
  File f = openLogFile(wardriveFile.c_str(), mode);
  if (!f) return false;
  size_t w = f.write(data, len);
  f.close();
  return w == len;
}

// Write to the run's log. On a failure: remount the card and try again; if the card still
// refuses, carry on in the board's flash (same file name) so the run is not lost.
static bool wdWriteLog(const uint8_t* data, size_t len, const char* mode) {
  if (wardriveFile == "") return false;
  if (wdWriteOnce(data, len, mode)) { wdLogFailed = false; return true; }
  Serial.printf("Log write failed on %s (free heap %u, largest block %u) - remounting\n",
                logUseLittleFS ? "flash" : "SD", (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  if (!logUseLittleFS) {
    SD.end(); sdReady = false; delay(20);
    if (tryMountSd() && wdWriteOnce(data, len, mode)) { wdLogFailed = false; Serial.println("Log write OK after SD remount"); return true; }
    if (LittleFS.begin(true)) {
      logUseLittleFS = true;
      bool fresh = !LittleFS.exists(wardriveFile);
      if (fresh && strcmp(mode, FILE_WRITE) != 0) wdWriteOnce((const uint8_t*)WD_CSV_HEADER, strlen(WD_CSV_HEADER), FILE_WRITE);
      if (wdWriteOnce(data, len, mode)) {
        wdLogFailed = false;
        Serial.printf("SD keeps failing - this run continues in the board's flash: %s\n", wardriveFile.c_str());
        return true;
      }
    }
  }
  wdLogFailed = true;
  return false;
}

void wdLogFlush() {
  if (!wdLogLen) return;
  if (wdWriteLog((const uint8_t*)wdLogBuf, wdLogLen, FILE_APPEND)) wdLogLen = 0;
  else if (wdLogLen > sizeof(wdLogBuf) - 512) wdLogLen = 0;   // keep the rows for the next try unless the buffer is full
}

static void wdLogAppend(const char* line) {
  size_t n = strlen(line);
  if (wdLogLen + n >= sizeof(wdLogBuf)) wdLogFlush();
  if (n >= sizeof(wdLogBuf)) return;
  memcpy(wdLogBuf + wdLogLen, line, n);
  wdLogLen += n;
}

static void csvClean(const char* in, char* out, size_t outSz) {
  size_t o = 0;
  for (const char* p = in ? in : ""; *p && o + 1 < outSz; p++) {
    char c = *p;
    if ((uint8_t)c < 0x20) continue;     // drop control chars
    if (c == ',') c = ' ';
    if (c == '"') c = '\'';
    out[o++] = c;
  }
  out[o] = 0;
}

int wifiChanToFreq(int ch) {
  if (ch >= 1 && ch <= 13) return 2407 + ch * 5;
  if (ch == 14) return 2484;
  if (ch >= 32) return 5000 + ch * 5;
  return 0;
}

// Writes one observation. Only called with a valid GPS fix + UTC date.
void logWigleRow(const char* mac, const char* ssid, const char* authMode,
                 int channel, int rssi, const char* type, int mfgr = -1) {
  if (!wardriveActive || wardriveFile == "") return;
  if (!gpsValid || !gps.date.isValid() || gps.date.year() < 2020) return;
  char firstSeen[32];
  snprintf(firstSeen, sizeof(firstSeen), "%04d-%02d-%02d %02d:%02d:%02d",
           gps.date.year(), gps.date.month(), gps.date.day(),
           gps.time.hour(), gps.time.minute(), gps.time.second());
  float acc = gps.hdop.isValid() ? (float)gps.hdop.hdop() * 4.0f : 10.0f;
  if (acc < 2.0f) acc = 2.0f;
  char clean[40];
  csvClean(ssid, clean, sizeof(clean));
  bool ble = (type[0] == 'B');
  char freq[12] = "";
  if (!ble) snprintf(freq, sizeof(freq), "%d", wifiChanToFreq(channel));
  char mf[12] = "";
  if (ble && mfgr >= 0) snprintf(mf, sizeof(mf), "%d", mfgr);
  char line[256];
  snprintf(line, sizeof(line), "%s,%s,%s,%s,%d,%s,%d,%.6f,%.6f,%.1f,%.1f,,%s,%s\n",
           mac, clean, authMode, firstSeen, ble ? 0 : channel, freq, rssi,
           gpsLat, gpsLng, gpsAlt, acc, mf, type);
  wdLogAppend(line);
  rowsLogged++;
}

static const char* wigleAuthStr(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN:            return "[ESS]";
    case WIFI_AUTH_WEP:             return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:         return "[WPA-PSK-TKIP][ESS]";
    case WIFI_AUTH_WPA2_PSK:        return "[WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "[WPA-PSK-CCMP][WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "[WPA2-EAP-CCMP][ESS]";
    case WIFI_AUTH_WPA3_PSK:        return "[RSN-SAE-CCMP][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "[WPA2-PSK-CCMP][RSN-SAE-CCMP][ESS]";
    case WIFI_AUTH_OWE:             return "[RSN-OWE-CCMP][ESS]";
    case WIFI_AUTH_WAPI_PSK:        return "[WAPI-PSK][ESS]";
    case WIFI_AUTH_WPA3_ENT_192:    return "[RSN-EAP-SUITE-B-192][ESS]";
    default:                        return "[WPA2-EAP-CCMP][ESS]";
  }
}

static bool channelIs5GHz(int ch) { return ch >= 32; }

static bool proAcceptRssi(int rssi, bool ble) {
  int mn = ble ? wdBleRssiMin : wdWifiRssiMin;
  int mx = ble ? wdBleRssiMax : wdWifiRssiMax;
  if (proMode) {
    int pmn = ble ? proBleRssiMin : proWifiRssiMin;
    int pmx = ble ? proBleRssiMax : proWifiRssiMax;
    if (pmn > mn) mn = pmn;
    if (pmx < mx) mx = pmx;
  }
  if (mn > mx) { int t = mn; mn = mx; mx = t; }
  return rssi >= mn && rssi <= mx;
}

static bool proChanOk(int ch) {
  int mode = wdChanFilter;
  if (proMode && proChanMode != 0) mode = proChanMode;
  if (mode == 1) return ch > 0 && ch < 32;
  if (mode == 2) return ch >= 32;
  if (mode == 3) return ch == 1 || ch == 6 || ch == 11;
  return true;
}

static bool proGpsAccOk() {
  if (wdGpsGate) {
    if (!gpsValid) return false;
    if (gpsSats < wdGpsMinSats) return false;
  }
  if (!proMode) return true;
  if (proGpsGate && !gpsValid) return false;
  if (proAccMode == 0) return true;
  if (!gpsValid) return false;
  if (proAccMode == 3) return gpsSats >= 6;
  if (!gps.hdop.isValid()) return false;
  float hd = gps.hdop.value() / 100.0f;
  if (proAccMode == 1) return hd <= 5.0f;
  if (proAccMode == 2) return hd <= 2.0f;
  return true;
}

static void proPushHit(const char* name, int rssi, char kind) {
  ProHit &h = proHits[proHitHead];
  memset(h.name, 0, sizeof(h.name));
  strncpy(h.name, (name && name[0]) ? name : "--", sizeof(h.name) - 1);
  h.rssi = rssi;
  h.kind = kind;
  proHitHead = (proHitHead + 1) % 5;
  if (proHitN < 5) proHitN++;
}

// Decide whether an observation is written: new MAC, or a much stronger
// re-sighting (better location for WiGLE). PRO + DEDUPE OFF logs everything.
static int wdObserve(const char* mac, uint8_t kind, int rssi) {
  int s = seenCheck(mac, kind, rssi);
  if (proMode && !proDedupe) return (s == 0) ? 0 : 1;
  if (proMode && proDedupe) return s == 0 ? 0 : 2;
  return s;
}

void scopeAddBlip(const char* mac, int rssi, uint8_t kind);

// ---------- Wi-Fi: direct ESP-IDF scan ----------
volatile bool wdWifiScanDone = false;
volatile bool wdBleScanDone = false;
bool wdScanEvtRegistered = false;

static void wdScanDoneEvt(void*, esp_event_base_t, int32_t, void*) { wdWifiScanDone = true; }
static void wdBleDoneCb(BLEScanResults) { wdBleScanDone = true; }

bool wdStartWifiScan() {
  if (!wdScanEvtRegistered) {
    if (esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wdScanDoneEvt, NULL) == ESP_OK)
      wdScanEvtRegistered = true;
  }
  wifi_scan_config_t c;
  memset(&c, 0, sizeof(c));
  c.show_hidden = true;
  c.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  c.scan_time.active.min = wdActMinMs;
  c.scan_time.active.max = wdActMaxMs;
  c.scan_time.passive = wdPassiveMs;
  int mode = (proMode && proChanMode != 0) ? proChanMode : wdChanFilter;
  const uint16_t all24 = 0x3FFE;                         // ch 1-13
  const uint32_t nonDfs5 = 0x1Eu | (0x1Fu << 21);        // 36-48 + 149-165
  if (mode == 1) { c.channel_bitmap.ghz_2_channels = all24; c.channel_bitmap.ghz_5_channels = 1; }
  else if (mode == 2) { c.channel_bitmap.ghz_2_channels = 1; c.channel_bitmap.ghz_5_channels = 0; }
  else if (mode == 3) { c.channel_bitmap.ghz_2_channels = (1 << 1) | (1 << 6) | (1 << 11); c.channel_bitmap.ghz_5_channels = 1; }
  else if (wdSkipDfsMost && (proSweepCount % 3) != 0) {
    // Fast sweep: every 2.4 channel + the busy non-DFS 5 GHz blocks.
    // Every third sweep is a full sweep that includes passive DFS channels.
    c.channel_bitmap.ghz_2_channels = all24;
    c.channel_bitmap.ghz_5_channels = nonDfs5;
  }
  wdWifiScanDone = false;
  esp_err_t e = esp_wifi_scan_start(&c, false);
  if (e != ESP_OK) {
    Serial.printf("scan_start err %d\n", (int)e);
    return false;
  }
  return true;
}

void wdHarvestWifi() {
  wifi_ap_record_t r;
  char mac[18];
  int seen24 = 0, seen5 = 0;
  while (esp_wifi_scan_get_ap_record(&r) == ESP_OK) {
    int ch = r.primary;
    int rssi = r.rssi;
    bool is5 = channelIs5GHz(ch);
    if (!proChanOk(ch)) continue;
    if (!proAcceptRssi(rssi, false)) continue;
    if (proMode && proSkipOpen && r.authmode == WIFI_AUTH_OPEN) continue;
    if (is5) seen5++; else seen24++;
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             r.bssid[0], r.bssid[1], r.bssid[2], r.bssid[3], r.bssid[4], r.bssid[5]);
    int s = wdObserve(mac, is5 ? 2 : 1, rssi);
    if (s == 2) continue;
    const char* ssid = (const char*)r.ssid;
    logWigleRow(mac, ssid, wigleAuthStr(r.authmode), ch, rssi, "WIFI");
    if (s == 0) {
      if (is5) { sessionWifi5Total++; sweepNew5++; } else { sessionWifi24Total++; sweepNew24++; }
      sessionWifiTotal++;
      proPushHit(ssid[0] ? ssid : mac, rssi, 'W');
      scopeAddBlip(mac, rssi, is5 ? 2 : 1);
      if (rssi > wdBestRssi) { wdBestRssi = rssi; strncpy(wdBestName, ssid[0] ? ssid : mac, sizeof(wdBestName) - 1); }
    }
    if (rssi < proRssiSeenMin) proRssiSeenMin = rssi;
    if (rssi > proRssiSeenMax) proRssiSeenMax = rssi;
  }
  esp_wifi_clear_ap_list();
  wardriveWifi24Count = seen24;
  wardriveWifi5Count = seen5;
  wardriveWifiCount = seen24 + seen5;
}

// ---------- BLE: async scan ----------
bool wdStartBleScan() {
  if (!pBLEScan) return false;
  int duty = proMode ? proBleDuty : wdBleDuty;
  pBLEScan->setActiveScan(proMode ? (duty == 2) : wdBleActive);
  pBLEScan->setInterval(80);
  pBLEScan->setWindow(duty == 0 ? 60 : 79);    // near 100% duty while BLE owns the radio
  wdBleScanDone = false;
  uint32_t sec = proMode ? (duty == 2 ? 2 : 1) : wdBleSec;
  return pBLEScan->start(sec, wdBleDoneCb, false);
}

void wdHarvestBle() {
  if (!pBLEScan) return;
  BLEScanResults* found = pBLEScan->getResults();
  int seen = 0;
  if (found) {
    int count = found->getCount();
    for (int i = 0; i < count; i++) {
      BLEAdvertisedDevice d = found->getDevice(i);
      int rssi = d.getRSSI();
      if (!proAcceptRssi(rssi, true)) continue;
      seen++;
      String addr = d.getAddress().toString();
      int s = wdObserve(addr.c_str(), 3, rssi);
      if (s == 2) continue;
      String name = d.haveName() ? String(d.getName().c_str()) : String("");
      int mfgr = -1;
      if (d.haveManufacturerData()) {
        String md = d.getManufacturerData();
        if (md.length() >= 2) mfgr = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
      }
      logWigleRow(addr.c_str(), name.c_str(), "Misc [LE]", 0, rssi, "BLE", mfgr);
      if (s == 0) {
        sessionBleTotal++;
        sweepNewBle++;
        proPushHit(name.length() ? name.c_str() : addr.c_str(), rssi, 'B');
        scopeAddBlip(addr.c_str(), rssi, 3);
      }
      if (rssi < proRssiSeenMin) proRssiSeenMin = rssi;
      if (rssi > proRssiSeenMax) proRssiSeenMax = rssi;
    }
  }
  pBLEScan->clearResults();
  wardriveBleCount = seen;
}

// ---------- Session control ----------
void startWardriving() {
  if (wardriveActive) return;
  if (apActive) stopPortalAP();

  seenInit();
  if (!openLogFS()) Serial.println("No SD and no LittleFS — scanning anyway, no file");

  // Unique, sortable file name. Never append to an older session's file.
  char base[48];
  snprintf(base, sizeof(base), "/SKIrf_WD_%s_%04d%02d%02d_%02d%02d%02d",
           wdTransportTag(-1),
           gps.date.isValid() && gps.date.year() > 2000 ? gps.date.year() : 2026,
           gps.date.month() > 0 ? gps.date.month() : 1,
           gps.date.day() > 0 ? gps.date.day() : 1,
           gps.time.hour(), gps.time.minute(), gps.time.second());
  String fn = String(base) + ".csv";
  for (int n = 2; n < 100 && logFs().exists(fn); n++) fn = String(base) + "_" + String(n) + ".csv";
  wardriveFile = fn;

  // Create the log (with its header) before the radios start: the card is set up while
  // internal memory is still free, and a card problem shows at once.
  if (wdWriteLog((const uint8_t*)WD_CSV_HEADER, strlen(WD_CSV_HEADER), FILE_WRITE))
    Serial.printf("Wardrive started: %s (%s)\n", wardriveFile.c_str(), logUseLittleFS ? "LittleFS" : "SD");
  else
    Serial.println("CSV not created - UI will still show live counts");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(60);
  esp_wifi_set_ps(WIFI_PS_NONE);
#if defined(WIFI_BAND_MODE_AUTO)
  WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
#endif
  esp_wifi_set_country_code(SKIRF_WIFI_COUNTRY, false);

  if (pBLEScan == nullptr) {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
  }

  wardriveActive = true;
  sessionWifiTotal = sessionWifi24Total = sessionWifi5Total = sessionBleTotal = 0;
  wardriveWifiCount = wardriveWifi24Count = wardriveWifi5Count = wardriveBleCount = 0;
  sweepNew24 = sweepNew5 = sweepNewBle = 0;
  proSweepCount = 0;
  proRssiSeenMin = 0;
  proRssiSeenMax = -127;
  proHitN = 0;
  proHitHead = 0;
  rowsLogged = 0;
  wdDistM = 0;
  wdHaveLast = false;
  wdBestRssi = -127;
  wdBestName[0] = 0;
  wdNewRecord = false;
  wdLogLen = 0;
  wdStartMs = millis();
  wdSweepStartMs = 0;
  lastWardriveScan = 0;
  scanPhase = 0;

  beep(2, 70);
}

void stopWardriving() {
  if (!wardriveActive) return;
  // Finish whatever radio work is in flight, then flush
  if (scanPhase == 1) esp_wifi_scan_stop();
  if (scanPhase == 3 && pBLEScan) pBLEScan->stop();
  scanPhase = 0;
  wdLogFlush();
  wardriveActive = false;

  uint32_t total = sessionUnique();
  wdLastSessionTotal = total;
  wdLastSessionMs = millis() - wdStartMs;
  loadRecords();
  recSessions++;
  recLifetime += total;
  wdNewRecord = (total > recBestSession && total > 0);
  if (wdNewRecord) recBestSession = total;
  saveRecords();
  if (!homeWifiConnected) WiFi.mode(WIFI_OFF);
  beep(1, 90);
  Serial.printf("Wardrive stopped: %lu unique, %lu rows\n", (unsigned long)total, (unsigned long)rowsLogged);
}

// Called ~1 Hz: accumulate distance travelled while recording
void wdTrackDistance() {
  if (!wardriveActive || !gpsValid) return;
  if (!wdHaveLast) { wdLastLat = gpsLat; wdLastLng = gpsLng; wdHaveLast = true; return; }
  double d = TinyGPSPlus::distanceBetween(wdLastLat, wdLastLng, gpsLat, gpsLng);
  if (d > 4.0 && d < 500.0) {           // ignore jitter and teleports
    wdDistM += d;
    wdLastLat = gpsLat; wdLastLng = gpsLng;
  } else if (d >= 500.0) {
    wdLastLat = gpsLat; wdLastLng = gpsLng;
  }
}

static void wdFinishSweep() {
  wdLogFlush();
  lastWardriveScan = millis();
  wdLastSweepMs = lastWardriveScan - wdSweepStartMs;
  proLastSweepMs = lastWardriveScan;
  proSweepCount++;
  scanPhase = 0;
  Serial.printf("Sweep %d: 2.4=%d 5=%d BLE=%d  new %d/%d/%d  %lums\n", proSweepCount,
                wardriveWifi24Count, wardriveWifi5Count, wardriveBleCount,
                sweepNew24, sweepNew5, sweepNewBle, wdLastSweepMs);
  if (sweepNew24 + sweepNew5 + sweepNewBle > 0 && currentScreen == WARDRIVING_SCREEN) {
    tone(BUZZER_PIN, 3200, 8);   // tiny tick when the sweep found new stuff
  }
  if (currentScreen == WARDRIVING_SCREEN) updateWardrivingScreen();
}

void pumpWardriveScan() {
  if (!wardriveActive) { scanPhase = 0; return; }
  unsigned long now = millis();
  bool doWifi = wdDoWifi && (!proMode || proWifiOn);
  bool doBle  = wdDoBle  && (!proMode || proBleOn);

  if (scanPhase == 0) {
    unsigned long period = wdSweepMs;
    if (proMode && proIntervalMs > 0) period = (unsigned long)proIntervalMs;
    if (wdSweepStartMs && now - wdSweepStartMs < period) return;
    if (!proGpsAccOk()) {
      // No logging without a usable fix — wait (radios idle) and say so
      if (!wdWaitingGps) {
        wdWaitingGps = true;
        if (currentScreen == WARDRIVING_SCREEN) updateWardrivingScreen();
      }
      return;
    }
    if (wdWaitingGps) wdWaitingGps = false;
    if (!doWifi && !doBle) return;
    wdSweepStartMs = now;
    wdPhaseStartMs = now;
    sweepNew24 = sweepNew5 = sweepNewBle = 0;
    if (doWifi && wdStartWifiScan()) scanPhase = 1;
    else if (doBle && wdStartBleScan()) scanPhase = 3;
    else { wdSweepStartMs = now; return; }
    if (currentScreen == WARDRIVING_SCREEN) updateWardrivingScreen();
    return;
  }

  if (scanPhase == 1) {
    if (!wdWifiScanDone) {
      if (now - wdPhaseStartMs < 20000) return;
      esp_wifi_scan_stop();           // stuck scan: abandon and move on
      Serial.println("Wi-Fi scan timeout");
    }
    wdHarvestWifi();
    wdLogFlush();
    if (doBle && wdStartBleScan()) {
      scanPhase = 3;
      wdPhaseStartMs = millis();
      if (currentScreen == WARDRIVING_SCREEN) updateWardrivingScreen();
      return;
    }
    wardriveBleCount = 0;
    wdFinishSweep();
    return;
  }

  if (scanPhase == 3) {
    uint32_t limit = (uint32_t)(proMode ? 2 : wdBleSec) * 1000UL + 2500UL;
    if (!wdBleScanDone) {
      if (now - wdPhaseStartMs < limit) return;
      if (pBLEScan) pBLEScan->stop();
    }
    wdHarvestBle();
    wdFinishSweep();
  }
}

void performScan() {
  wdSweepStartMs = 0;
  scanPhase = 0;
}


// ================= ANIMATION LANES =================
// Each lane is rendered into an offscreen canvas and pushed in one SPI burst:
// no flicker, no erase trails, and nothing ever paints over the UI.

LGFX_Sprite laneCanvas(&tft);
Lane lanes[2];
unsigned long laneLastFrame = 0;

// Vehicle body colours (intended on-glass RGB)
static const uint32_t CAR_BODIES[] = {
  0xDC1E28, 0x2C6EE6, 0xC0C6CE, 0xEEF0F4, 0x2A2C32, 0xF8C814,
  0x1EAA5A, 0xFF7814, 0x00AAB4, 0x8C46DC
};

void laneSetupActor(Lane &L);

void lanesStop() {
  for (int i = 0; i < 2; i++) lanes[i].on = false;
}

bool laneCanvasFor(int w, int h) {
  if (laneCanvas.width() == w && laneCanvas.height() == h && laneCanvas.getBuffer()) return true;
  laneCanvas.deleteSprite();
  laneCanvas.setColorDepth(16);
  laneCanvas.setPsram(false);
  if (laneCanvas.createSprite(w, h)) return true;
  laneCanvas.setPsram(true);          // internal RAM tight (Wi-Fi + BLE up): use PSRAM
  return laneCanvas.createSprite(w, h) != nullptr;
}

void laneConfigure(int idx, int x, int y, int w, int h, uint8_t scene, uint8_t role, bool treadmill, bool goRight) {
  if (idx < 0 || idx > 1) return;
  Lane &L = lanes[idx];
  L.on = (w > 20 && h > 12 && scene != SC_NONE);
  L.x = x; L.y = y; L.w = w; L.h = h;
  L.scene = scene; L.role = role; L.treadmill = treadmill; L.goRight = goRight;
  L.scroll = 0; L.tick = 0;
  L.a.active = false;
  L.a.nextSpawn = millis() + (idx ? 900 : 200);
  if (treadmill) laneSetupActor(L);
}

// Pick what drives through this lane
const SkiSprite* laneVehicleFor(uint8_t role, int &kind) {
  if (role == ROLE_WD) {
    if (wdTransport == WD_XPORT_MOTO) { kind = 4; return &SPR_MOTO; }
    if (wdTransport == WD_XPORT_WALK) { kind = 5; return &SPR_CYCLIST; }
    static const SkiSprite* cars4[] = { &SPR_SEDAN, &SPR_SPORTS, &SPR_PICKUP, &SPR_VAN };
    kind = (int)random(0, 4);
    return cars4[kind];
  }
  static const SkiSprite* all6[] = { &SPR_SEDAN, &SPR_SPORTS, &SPR_PICKUP, &SPR_VAN, &SPR_MOTO, &SPR_CYCLIST };
  int r = (int)random(0, 10);
  kind = (r < 7) ? (r % 4) : (r == 7 ? 4 : (r == 8 ? 5 : (int)random(0, 4)));
  return all6[kind];
}

void laneSetupActor(Lane &L) {
  LaneActor &a = L.a;
  a.active = (L.scene == SC_ROAD);
  a.frame = 0;
  a.dist = 0;
  a.right = L.goRight;
  if (!a.active) return;
  a.spr = laneVehicleFor(L.role, a.kind);
  if (a.spr->h > L.h - 2) { a.spr = &SPR_SEDAN; a.kind = 0; }   // never clip the roof off
  if (wdgwTheme) a.body = random(0, 2) ? wdgwChrome : wdgwText;
  else a.body = C565x(CAR_BODIES[random(0, sizeof(CAR_BODIES) / sizeof(CAR_BODIES[0]))]);
  if (a.kind == 4) a.speed = 4.4f;
  else if (a.kind == 5) a.speed = 2.0f;
  else a.speed = 3.0f + random(0, 12) / 10.0f;
  if (L.treadmill) a.x = (L.w - a.spr->w) / 2.0f;
  else a.x = a.right ? -a.spr->w - 2.0f : (float)L.w + 2.0f;
}

// ---------- Sprite blit (4-bit palette, optional body recolour) ----------
void blitSkiSprite(LovyanGFX &g, const SkiSprite &s, int frame, int dx, int dy, bool faceRight, uint16_t body) {
  uint16_t pal[16];
  for (int i = 0; i < 16; i++) pal[i] = C565x(s.pal[i]);
  if ((s.flags & SPR_RECOLOR) && body) {
    pal[1] = blend565(body, TFT_WHITE, 120);
    pal[2] = body;
    pal[3] = blend565(body, TFT_BLACK, 90);
    pal[4] = blend565(body, TFT_BLACK, 170);
  }
  int bpr = (s.w + 1) / 2;
  const uint8_t* base = s.data + (size_t)(frame % s.frames) * s.h * bpr;
  for (int y = 0; y < s.h; y++) {
    const uint8_t* row = base + y * bpr;
    int runStart = -1;
    uint8_t runIdx = 0;
    for (int x = 0; x <= s.w; x++) {
      uint8_t idx = 0;
      if (x < s.w) {
        uint8_t b = row[x >> 1];
        idx = (x & 1) ? (b & 0x0F) : (b >> 4);
      }
      if (idx != runIdx || x == s.w) {
        if (runIdx && runStart >= 0) {
          int len = x - runStart;
          int px = faceRight ? (dx + runStart) : (dx + s.w - x);
          g.drawFastHLine(px, dy + y, len, pal[runIdx]);
        }
        runIdx = idx;
        runStart = x;
      }
    }
  }
}


// ---------- Scene backgrounds ----------
void laneDrawRoad(LovyanGFX &g, Lane &L, int groundY) {
  g.fillScreen(TFT_BLACK);
  uint16_t edge = blend565(TFT_BLACK, getAccent(), 60);
  g.drawFastHLine(0, groundY + 1, L.w, blend565(TFT_BLACK, UI_LINE, 200));
  // lane dashes: move only when the vehicle is on the treadmill
  int s = ((int)L.scroll) % 22;
  int off = L.a.right ? (22 - s) % 22 : s;   // scenery streams opposite to travel
  for (int x = -22 + off; x < L.w; x += 22) g.drawFastHLine(x, groundY + 3, 11, edge);
}






// ---------- One frame ----------
void laneRender(Lane &L) {
  if (!laneCanvasFor(L.w, L.h)) return;
  LGFX_Sprite &g = laneCanvas;
  LaneActor &a = L.a;
  int groundY = L.h - 4;
  laneDrawRoad(g, L, groundY);
  if (a.active && a.spr) {
    const SkiSprite &s = *a.spr;
    // sprites face right; mirror for left-bound traffic so the nose always leads
    blitSkiSprite(g, s, a.frame, (int)a.x, groundY - s.h + 2, a.right, a.body);
  }
  g.pushSprite(L.x, L.y);
}

void laneStep(Lane &L) {
  LaneActor &a = L.a;
  L.tick++;
  bool moving = L.treadmill ? (wardriveActive || currentScreen != WARDRIVING_SCREEN) : true;
  if (L.treadmill) {
    if (!a.active) laneSetupActor(L);
    if (moving) {
      float spd = a.speed;
      if (gpsValid && gpsSpeed > 3.0) spd *= constrain((float)(gpsSpeed / 30.0), 0.7f, 2.2f);
      L.scroll += spd;
      if (L.scroll > 1000000.0f) L.scroll = 0;
      a.dist += spd;
    }
  } else if (!a.active) {
    if ((long)(millis() - a.nextSpawn) >= 0) laneSetupActor(L);
  } else {
    a.x += a.right ? a.speed : -a.speed;
    a.dist += a.speed;
    int aw = a.spr ? a.spr->w : 70;
    if ((a.right && a.x > L.w + 2) || (!a.right && a.x < -aw - 2)) {
      a.active = false;
      a.nextSpawn = millis() + random(500, 2400);
      if (L.role != ROLE_HOME_A && L.role != ROLE_HOME_B && L.role != ROLE_SPLASH_A && L.role != ROLE_SPLASH_B) L.goRight = !L.goRight;
    }
  }
  // wheels turn with distance travelled (clockwise when heading right)
  if (a.active && a.spr) a.frame = moving ? ((int)(a.dist / 4.0f)) % a.spr->frames : 0;
}

// Called from loop(): advances and draws every active lane
void scopeTick();
extern bool scopeOn;
void updateCarAnimation() {
  if (currentScreen != HOME_SCREEN) scopeOn = false;
  scopeTick();
  if (proMode) return;
  if (!lanes[0].on && !lanes[1].on) return;
  unsigned long now = millis();
  unsigned long frameMs = (currentScreen == WARDRIVING_SCREEN) ? 50 : 40;
  if (!gpsValid) frameMs = 100;   // 10 fps while the GPS hunts for its first fix: less display-bus noise near the antenna
  if (now - laneLastFrame < frameMs) return;
  laneLastFrame = now;
  for (int i = 0; i < 2; i++) {
    if (!lanes[i].on) continue;
    laneStep(lanes[i]);
    laneRender(lanes[i]);
  }
}

// Scene for a lane given the theme and its role on screen
uint8_t laneSceneFor(uint8_t role) {
  if (proMode) return SC_NONE;
  return SC_ROAD;
}

const char* settingsLabels[] = {
  "BRIGHTNESS",
  "UI COLOR",
  "ORIENTATION",
  "WEB PORTAL",
  "FILES",
  "WIFI",
  "DISCLAIMER",
  "GPS PINS",
  "TOUCH CAL",
  "WDGW STATS",
  "PRO MODE"
};
const int settingsCount = 11;

// Grid geometry for settings icons (portrait & landscape)
void settingsIconGeom(int &cols, int &rows, int &cellW, int &cellH,
                      int &gridLeft, int &gridTop, int &iconR, int &maxScroll) {
  int w = tft.width();
  int h = tft.height();
  bool landscape = (w > h);
  int listTop = 54;
  int listBottom = (h - 50); // keep clear of HOME
  int viewH = listBottom - listTop;

  if (landscape) {
    cols = 4;
    rows = 3;
  } else {
    // 3 x 4 = 12 slots — all 11 items stay above HOME
    cols = 3;
    rows = 4;
  }
  cellW = (w - 8) / cols;
  cellH = viewH / rows;
  int needH = ((settingsCount + cols - 1) / cols) * cellH;
  maxScroll = needH - viewH;
  if (maxScroll < 0) maxScroll = 0;

  gridLeft = 4;
  gridTop = listTop;
  // Landscape: smaller icons so label text is not crushed
  if (landscape) {
    iconR = min(cellW / 3, cellH / 3);
    if (iconR < 11) iconR = 11;
    if (iconR > 18) iconR = 18;
  } else {
    iconR = min(cellW, cellH) / 3;
    if (iconR < 12) iconR = 12;
    if (iconR > 26) iconR = 26;
  }
}

bool settingsIconHit(int tx, int ty, int &outIndex) {
  int w = tft.width();
  int h = tft.height();
  // Never treat HOME (bottom-left) as an icon
  if (ty >= h - 48 && tx <= 100) return false;

  int cols, rows, cellW, cellH, gridLeft, gridTop, iconR, maxScroll;
  settingsIconGeom(cols, rows, cellW, cellH, gridLeft, gridTop, iconR, maxScroll);
  int listTop = 54;
  int listBottom = h - 50;
  int hitTop = listTop;
  int hitBot = listBottom;
  if (ty < hitTop || ty > hitBot) return false;
  if (tx < gridLeft || tx > gridLeft + cols * cellW) return false;

  int localY = ty - gridTop + settingsScroll;
  int row = localY / cellH;
  int col = (tx - gridLeft) / cellW;
  if (col < 0 || col >= cols || row < 0) return false;
  int idx = row * cols + col;
  if (idx < 0 || idx >= settingsCount) return false;
  outIndex = idx;
  return true;
}



// ================= SCREENS: HOME / TRANSPORT / WARDRIVING / REPORT =================
#include <functional>

// Paint a region through an offscreen canvas (flicker-free). Falls back to
// drawing straight on the panel if the canvas can't be allocated.
void paintRegion(int x, int y, int w, int h, std::function<void(LovyanGFX&, int, int)> fn) {
  LGFX_Sprite c(&tft);
  c.setColorDepth(16);
  bool ok = (w > 0 && h > 0) && c.createSprite(w, h);
  if (!ok && w > 0 && h > 0) { c.setPsram(true); ok = c.createSprite(w, h); }
  if (ok) {
    c.fillScreen(TFT_BLACK);
    fn(c, 0, 0);
    c.pushSprite(x, y);
    c.deleteSprite();
  } else {
    tft.fillRect(x, y, w, h, TFT_BLACK);
    fn(tft, x, y);
  }
}

void fmtNum(uint32_t v, char* out, size_t n) {
  char tmp[16];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)v);
  int len = strlen(tmp), o = 0;
  for (int i = 0; i < len && o + 1 < (int)n; i++) {
    out[o++] = tmp[i];
    int rem = len - i - 1;
    if (rem > 0 && rem % 3 == 0 && o + 1 < (int)n) out[o++] = ',';
  }
  out[o] = 0;
}

void fmtDur(unsigned long ms, char* out, size_t n) {
  unsigned long s = ms / 1000;
  if (s >= 3600) snprintf(out, n, "%lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  else snprintf(out, n, "%02lu:%02lu", s / 60, s % 60);
}

int transportIcon(int m) {
  if (m == WD_XPORT_MOTO) return ICON_MOTO;
  if (m == WD_XPORT_WALK) return ICON_BIKE;
  return ICON_CAR;
}

// ================= HOME: radar scope + one primary action =================
// Shared home geometry (draw + touch)
void homeGeo(HomeGeo &G) {
  int W = tft.width(), H = tft.height();
  G.land = W > H;
  if (!G.land) {
    G.scope = { W / 2 - 76, 36, 152, 152 };
    G.lane = { 0, 189, W, 38 };
    G.primary = { 12, 232, W - 24, 40 };
    int w0 = 58, w1 = 62, w2 = W - 24 - 12 - w0 - w1;   // GPS | Logs | Settings
    G.sec[0] = { 12, 280, w0, 34 };
    G.sec[1] = { 12 + w0 + 6, 280, w1, 34 };
    G.sec[2] = { 12 + w0 + w1 + 12, 280, w2, 34 };
  } else {
    G.scope = { 8, 54, 156, 156 };
    G.lane = { 0, 0, 0, 0 };
    G.primary = { 176, 44, W - 190, 44 };
    for (int i = 0; i < 3; i++) G.sec[i] = { 176, 98 + i * 44, W - 190, 38 };
  }
}
// legacy hooks
void homeBtnGeom(int &btnH, int &gap, int &top, int &blockH) {
  HomeGeo G; homeGeo(G);
  btnH = G.primary.h; gap = 8; top = G.primary.y; blockH = G.sec[2].y + G.sec[2].h - top;
}
// 0 = primary (wardrive), 1 = GPS, 2 = Logs, 3 = Settings, -1 = nothing
int homeTileHit(int tx, int ty) {
  HomeGeo G; homeGeo(G);
  auto in = [&](const Rect &r, int p) { return tx >= r.x - p && tx <= r.x + r.w + p && ty >= r.y - p && ty <= r.y + r.h + p; };
  if (in(G.primary, 5)) return 0;
  for (int i = 0; i < 3; i++) if (in(G.sec[i], 3)) return i + 1;
  return -1;
}

// ---------- Radar scope ----------
Blip scopeBlips[28];
uint8_t scopeBlipHead = 0;
bool scopeOn = false;
Rect scopeRect = { 0, 0, 0, 0 };
LGFX_Sprite scopeCanvas(&tft);
unsigned long scopeLastFrame = 0;

void scopeAddBlip(const char* mac, int rssi, uint8_t kind) {
  uint32_t h = 2166136261u;
  for (const char* p = mac; p && *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  Blip &b = scopeBlips[scopeBlipHead];
  b.ang = (uint8_t)(h & 0xFF);
  // strong signals sit near the centre, weak ones out at the rim
  int r = map(constrain(rssi, -95, -35), -35, -95, 30, 250);
  b.rad = (uint8_t)constrain(r + (int)((h >> 8) % 20) - 10, 20, 255);
  b.kind = kind;
  b.t = millis() | 1;
  scopeBlipHead = (scopeBlipHead + 1) % 28;
}

uint16_t blipColor(uint8_t kind) { return kind == 3 ? C_BLE : (kind == 2 ? C_W5 : C_W24); }

void scopeRender() {
  int S = scopeRect.w;
  if (scopeCanvas.width() != S || !scopeCanvas.getBuffer()) {
    scopeCanvas.deleteSprite();
    scopeCanvas.setColorDepth(16);
    scopeCanvas.setPsram(false);
    if (!scopeCanvas.createSprite(S, S)) { scopeCanvas.setPsram(true); if (!scopeCanvas.createSprite(S, S)) return; }
  }
  LGFX_Sprite &g = scopeCanvas;
  uint16_t acc = getAccent();
  int c = S / 2, R = S / 2 - 3;
  unsigned long now = millis();
  g.fillScreen(TFT_BLACK);
  // phosphor disc + rings
  g.fillCircle(c, c, R, blend565(TFT_BLACK, acc, 16));
  g.drawCircle(c, c, R, blend565(TFT_BLACK, acc, 140));
  g.drawCircle(c, c, R - 1, blend565(TFT_BLACK, acc, 50));
  g.drawCircle(c, c, R * 2 / 3, blend565(TFT_BLACK, acc, 64));
  g.drawCircle(c, c, R / 3, blend565(TFT_BLACK, acc, 64));
  g.drawFastHLine(c - R + 8, c, 2 * R - 16, blend565(TFT_BLACK, acc, 30));
  g.drawFastVLine(c, c - R + 8, 2 * R - 16, blend565(TFT_BLACK, acc, 30));
  for (int d = 0; d < 360; d += 15) {
    float a = d * DEG_TO_RAD;
    int len = (d % 90 == 0) ? 7 : 3;
    g.drawLine(c + (int)((R - len) * cosf(a)), c + (int)((R - len) * sinf(a)),
               c + (int)(R * cosf(a)), c + (int)(R * sinf(a)), blend565(TFT_BLACK, acc, d % 90 == 0 ? 110 : 55));
  }
  // sweep: faster while recording
  float period = wardriveActive ? 2600.0f : 5200.0f;
  float sweep = fmodf(now / period * 360.0f, 360.0f);
  for (int k = 26; k >= 0; k--) {
    float a = (sweep - k * 1.6f) * DEG_TO_RAD;
    uint8_t lvl = (uint8_t)(k == 0 ? 255 : 150 - k * 5);
    g.drawLine(c, c, c + (int)((R - 2) * cosf(a)), c + (int)((R - 2) * sinf(a)), blend565(TFT_BLACK, acc, k == 0 ? 255 : lvl * 2 / 3));
  }
  // blips glow when the arm passes, then fade over ~14 s
  for (int i = 0; i < 28; i++) {
    Blip &b = scopeBlips[i];
    if (!b.t) continue;
    unsigned long age = now - b.t;
    if (age > 14000) { b.t = 0; continue; }
    float ba = b.ang * (360.0f / 256.0f);
    float since = fmodf(sweep - ba + 360.0f, 360.0f);     // degrees since the arm passed
    float glow = 1.0f - since / 360.0f;
    float life = 1.0f - age / 14000.0f;
    uint8_t lvl = (uint8_t)(40 + 215 * glow * life);
    float rr = b.rad / 255.0f * (R - 6);
    int bx = c + (int)(rr * cosf(ba * DEG_TO_RAD));
    int by = c + (int)(rr * sinf(ba * DEG_TO_RAD));
    uint16_t col = blendColor(blipColor(b.kind), lvl);
    if (age < 1600) g.drawCircle(bx, by, 3 + age / 250, blend565(TFT_BLACK, C_NEW, (uint8_t)(255 - age * 255 / 1600)));
    g.fillCircle(bx, by, 2, col);
  }
  // centre readout
  g.fillCircle(c, c, 30, TFT_BLACK);
  g.drawCircle(c, c, 30, blend565(TFT_BLACK, acc, 90));
  g.setTextDatum(MC_DATUM);
  if (wardriveActive) {
    char n[16];
    fmtNum(sessionUnique(), n, sizeof(n));
    g.setFont(F_L);
    if (g.textWidth(n) > 56) g.setFont(F_M);
    g.setTextColor(UI_TEXT);
    g.drawString(n, c, c - 4);
    g.setFont(F_XS);
    g.setTextColor(UI_MUTE);
    g.drawString("unique", c, c + 16);
  } else {
    g.setFont(F_S);
    g.setTextColor(UI_TEXT);
    g.drawString(gpsValid ? "Ready" : "No fix", c, c - 5);
    g.setFont(F_XS);
    g.setTextColor(UI_MUTE);
    char b[16];
    snprintf(b, sizeof(b), "%d sats", gpsSats);
    g.drawString(b, c, c + 11);
  }
  g.pushSprite(scopeRect.x, scopeRect.y);
}

void scopeTick() {
  if (!scopeOn) return;
  unsigned long now = millis();
  if (now - scopeLastFrame < 66) return;
  scopeLastFrame = now;
  scopeRender();
}

// ---------- Home chrome ----------
String homeStatusLast = "";

void drawHomeStatusBar() {
  if (currentScreen != HOME_SCREEN) return;
  bool wifiOn = (WiFi.status() == WL_CONNECTED);
  homeWifiConnected = wifiOn;
  if (wifiOn) homeWifiIP = WiFi.localIP().toString();
  char st[40];
  snprintf(st, sizeof(st), "%d sats  %s%s", gpsSats, sdReady ? "SD" : (logUseLittleFS ? "Flash" : "No SD"), wifiOn ? "  LAN" : "");
  String sig = String(st) + (gpsValid ? "L" : "-");
  if (homeStatusLast == sig) return;
  homeStatusLast = sig;
  int w = tft.width();
  tft.fillRect(w - 130, 4, 126, 24, TFT_BLACK);
  tft.setFont(F_XS);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(UI_MUTE, TFT_BLACK);
  tft.drawString(st, w - 12, 16);
  int tw = tft.textWidth(st);
  uint16_t col = gpsValid ? C_OK : (gpsSats > 0 ? C_NEW : UI_DIM);
  tft.fillCircle(w - 12 - tw - 8, 16, 3, col);
  uiResetFont();
}

void updateHomeStatusBar(bool force = false) {
  if (currentScreen != HOME_SCREEN) return;
  if (force) homeStatusLast = "";
  if (!force && millis() - lastHomeStatusSwap < 500) return;
  lastHomeStatusSwap = millis();
  drawHomeStatusBar();
}

void homeHint(int i, char* out, size_t n) {
  char a[16];
  if (i == 0) {
    bool land = tft.width() > tft.height();
    if (wardriveActive) { fmtNum(sessionUnique(), a, sizeof(a)); snprintf(out, n, land ? "%s" : "Recording  %s", a); }
    else snprintf(out, n, land ? "Start" : "Start wardrive");
  } else if (i == 1) {
    snprintf(out, n, "%d", gpsValid ? 1 : 0);
  } else {
    if (recBestSession > 0) { fmtNum(recBestSession, a, sizeof(a)); snprintf(out, n, "Best run %s", a); }
    else snprintf(out, n, "2.4 + 5 GHz + BLE");
  }
}

void drawHomePrimary() {
  HomeGeo G; homeGeo(G);
  char lab[40];
  homeHint(0, lab, sizeof(lab));
  Rect r = G.primary;
  uint16_t f = wardriveActive ? C_BAD : getAccent();
  uint16_t ink = inkOn(f);
  tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
  tft.fillRoundRect(r.x, r.y, r.w, r.h, r.h / 2, f);
  // play triangle / rec dot as the only glyph on the screen
  int gx = r.x + 22, gy = r.y + r.h / 2;
  if (wardriveActive) tft.fillCircle(gx, gy, 5, ink);
  else tft.fillTriangle(gx - 4, gy - 7, gx - 4, gy + 7, gx + 7, gy, ink);
  tft.setFont(F_M);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ink, f);
  tft.drawString(lab, r.x + r.w / 2 + 8, gy + 1);
  uiResetFont();
}

void drawHomeTile(int i) {   // kept name: i=0 primary, others secondary
  HomeGeo G; homeGeo(G);
  if (i == 0) { drawHomePrimary(); return; }
}

void drawHomeScreen() {
  paletteRefresh();
  lanesStop();
  scopeOn = false;
  tft.fillScreen(TFT_BLACK);
  HomeGeo G; homeGeo(G);
  int w = tft.width();
  {
    // wordmark, same voice as the boot screen
    tft.setFont(&fonts::Orbitron_Light_24);
    tft.setTextDatum(ML_DATUM);
    int sx = 12;
    tft.setTextColor(C565(255, 128, 0), TFT_BLACK);
    tft.drawString("SKI", sx, 16);
    int skiW = tft.textWidth("SKI");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("rf", sx + skiW, 16);
    if (proMode) {
      int x2 = sx + skiW + tft.textWidth("rf") + 8;
      tft.setFont(F_XS);
      tft.setTextColor(proCyan, TFT_BLACK);
      tft.drawString("PRO", x2, 18);
    }
    uiResetFont();
  }
  (void)w;
  drawHomePrimary();
  const char* sec[3] = { "GPS", "LOGS", "SETTINGS" };
  for (int i = 0; i < 3; i++) drawThemedButton(G.sec[i].x, G.sec[i].y, G.sec[i].w, G.sec[i].h, sec[i], getBtnFill(), TFT_WHITE);
  // caption under the scope (best run / capabilities)
  char cap[40];
  homeHint(2, cap, sizeof(cap));
  homeStatusLast = "";
  if (true) drawHomeStatusBar();
  scopeRect = G.scope;
  scopeOn = true;
  scopeRender();
  if (G.land) {
    tft.setFont(F_XS);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(UI_DIM, TFT_BLACK);
    tft.drawString(cap, G.scope.x + G.scope.w / 2, G.scope.y + G.scope.h + 6 > tft.height() - 12 ? tft.height() - 12 : G.scope.y + G.scope.h + 2);
    uiResetFont();
  } else {
    laneConfigure(0, G.lane.x, G.lane.y, G.lane.w, G.lane.h, laneSceneFor(ROLE_HOME_B), ROLE_HOME_B, false, false);
  }
}

// ---------- Transport picker ----------
void wdModeTileRect(int i, int &x, int &y, int &w, int &h) {
  int W = tft.width(), H = tft.height();
  if (W > H) { w = 96; h = 140; x = 10 + i * (w + 6); y = 54; }
  else { w = W - 20; h = 68; x = 10; y = 54 + i * (h + 6); }
}
// transport picker geometry (shared with the touch handler)
void wdModeBtnGeom(int &btnH, int &gap, int &top, int &blockH) {
  int x, y, w, h; wdModeTileRect(0, x, y, w, h);
  btnH = h; gap = 6; top = y; blockH = 3 * h + 12;
}

const SkiSprite* transportSprite(int m) {
  if (m == WD_XPORT_MOTO) return &SPR_MOTO;
  if (m == WD_XPORT_WALK) return &SPR_CYCLIST;
  return &SPR_SEDAN;
}

void drawWdModeScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("TRANSPORT");
  bool land = (tft.width() > tft.height());
  const char* labels[3] = { "Car", "Motorcycle", "Walk or bike" };
  const char* specA[3]  = { "2.5 s sweeps", "3.5 s sweeps", "6 s sweeps" };
  const char* specB[3]  = { "Passive BLE", "Balanced BLE", "Active BLE" };
  for (int i = 0; i < 3; i++) {
    int x, y, w, h; wdModeTileRect(i, x, y, w, h);
    bool sel = (wdTransport == i);
    uint16_t fill = sel ? uiRaisedFill() : uiCardFill();
    tft.fillRoundRect(x, y, w, h, 12, fill);
    const SkiSprite* s = transportSprite(i);
    uint16_t body = wdgwTheme ? wdgwChrome : (getAccent());
    int pw = land ? w - 12 : 92, ph = land ? 50 : h - 12;
    int px = x + 6, py = y + 6;
    paintRegion(px, py, pw, ph, [&](LovyanGFX &g, int bx, int by) {
      g.fillRect(bx, by, pw, ph, fill);
      g.drawFastHLine(bx + 6, by + ph - 6, pw - 12, blend565(fill, TFT_WHITE, 30));
      blitSkiSprite(g, *s, 0, bx + (pw - s->w) / 2, by + ph - 6 - s->h + 1, true, body);
    });
    if (!land) {
      int tx = x + 108;
      tft.setTextDatum(TL_DATUM);
      tft.setFont(F_L);
      if (tft.textWidth(labels[i]) > x + w - tx - 8) tft.setFont(F_M);
      tft.setTextColor(UI_TEXT, fill);
      tft.drawString(labels[i], tx, y + 10);
      tft.setFont(F_XS);
      tft.setTextColor(UI_MUTE, fill);
      tft.drawString(specA[i], tx, y + 38);
      tft.setTextColor(UI_DIM, fill);
      String sb = specB[i];
      while (sb.length() > 1 && tft.textWidth(sb) > x + w - tx - 6) sb = sb.substring(0, sb.length() - 1);
      tft.drawString(sb, tx, y + 51);
      if (sel) tft.fillCircle(x + w - 14, y + 14, 4, getAccent());
    } else {
      tft.setTextDatum(MC_DATUM);
      tft.setFont(F_M);
      const char* sl = (i == 2) ? "Walk/bike" : (i == 1 ? "Moto" : "Car");
      tft.setTextColor(UI_TEXT, fill);
      tft.drawString(sl, x + w / 2, y + 78);
      tft.setFont(F_XS);
      tft.setTextColor(UI_MUTE, fill);
      const char* sw[3] = { "2.5 s sweeps", "3.5 s sweeps", "6 s sweeps" };
      tft.drawString(sw[i], x + w / 2, y + 100);
      if (sel) tft.fillCircle(x + w / 2, y + h - 14, 4, getAccent());
    }
    uiResetFont();
  }
  drawBackButton("HOME");
  if (true) {
    tft.setFont(F_XS);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(UI_DIM, TFT_BLACK);
    tft.drawString("Tap a ride to start", tft.width() - 14, tft.height() - 22);
    uiResetFont();
  }
}

// ---------- Wardriving dashboard ----------
unsigned long wdStopArmMs = 0;
unsigned long wdUiLastTick = 0;
int wdBtnSig = -1;

void wdLayout(WdLayout &L) {
  int W = tft.width(), H = tft.height();
  L.land = (W > H);
  if (!L.land) {
    L.hero = { 0, 50, W, 74 };
    L.status = { 14, 126, W - 28, 18 };
    L.prog = { 14, 148, W - 28, 2 };
    int bw = (W - 28) / 3;
    for (int i = 0; i < 3; i++) L.band[i] = { 14 + i * bw, 156, bw, 46 };
    L.gps = { 14, 205, W - 28, 14 };
    L.lane = { 0, 224, W, 42 };
    L.side = { 0, 0, 0, 0 };
    L.btnMain = { 14, H - 46, 140, 40 };
    L.btnHome = { 160, H - 46, W - 166, 40 };
  } else {
    int lw = 196;
    L.hero = { 0, 48, lw + 8, 58 };
    L.status = { 14, 108, lw - 14, 16 };
    L.prog = { 14, 127, lw - 14, 2 };
    int bw = (lw - 14) / 3;
    for (int i = 0; i < 3; i++) L.band[i] = { 14 + i * bw, 134, bw, 44 };
    L.gps = { 14, 183, lw - 14, 14 };
    L.lane = { 210, 52, W - 214, 70 };
    L.side = { 214, 128, W - 220, 70 };
    L.btnMain = { 14, H - 38, lw - 14, 34 };
    L.btnHome = { 214, H - 38, W - 220, 34 };
  }
}

const char* wdStatusText() {
  static char st[36];
  if (!wardriveActive) return "READY";
  if (wdLogFailed) return "LOG NOT SAVING";
  if (wdWaitingGps || !proGpsAccOk()) return gpsValid ? "WAIT GPS SATS" : "WAITING FOR GPS";
  if (scanPhase == 1) {
    int mode = (proMode && proChanMode != 0) ? proChanMode : wdChanFilter;
    snprintf(st, sizeof(st), "%s", mode == 1 ? "SCAN 2.4 GHz" : (mode == 2 ? "SCAN 5 GHz" : "SCAN 2.4 + 5 GHz"));
    return st;
  }
  if (scanPhase == 3) return "SCAN BLE";
  return "RECORDING";
}

uint16_t wdStatusColor() {
  if (!wardriveActive) return UI_MUTE;
  if (wdLogFailed) return C_BAD;
  if (wdWaitingGps || !proGpsAccOk()) return C_WARN;
  return C_OK;
}

void wdDrawStatus(LovyanGFX &g, int bx, int by, int w, int h) {
  uint16_t sc = wdStatusColor();
  bool blink = (millis() / 450) & 1;
  int cy = by + h / 2;
  if (wardriveActive && (blink || wdWaitingGps)) g.fillCircle(bx + 4, cy, 3, sc);
  else g.drawCircle(bx + 4, cy, 3, sc);
  // sentence-case state, e.g. "Scanning 2.4 + 5 GHz"
  const char* raw = wdStatusText();
  String s2 = raw;
  if (s2 == "READY") s2 = String(wdTransportLabel(-1)) + " profile, ready";
  else if (s2 == "RECORDING") s2 = "Recording";
  else if (s2 == "SCAN BLE") s2 = "Scanning Bluetooth";
  else if (s2.startsWith("SCAN")) { s2.replace("SCAN", "Scanning"); s2.replace("GHZ", "GHz"); }
  else if (s2 == "WAITING FOR GPS") s2 = gpsLinkState() == 2 ? "No data from the GPS - check wiring" : "Waiting for a GPS fix";
  else if (s2 == "WAIT GPS SATS") s2 = "Waiting for more satellites";
  g.setFont(F_XS);
  g.setTextDatum(ML_DATUM);
  g.setTextColor(wardriveActive ? UI_TEXT : UI_MUTE, TFT_BLACK);
  g.drawString(s2.c_str(), bx + 14, cy);
  char t[16];
  if (wardriveActive) fmtDur(millis() - wdStartMs, t, sizeof(t));
  else snprintf(t, sizeof(t), "--:--");
  g.setTextDatum(MR_DATUM);
  g.setFont(F_S);
  g.setTextColor(wardriveActive ? UI_TEXT : UI_DIM, TFT_BLACK);
  g.drawString(t, bx + w, cy);
}

void wdDrawHero(LovyanGFX &g, int bx, int by, int w, int h, bool land) {
  char n[16];
  fmtNum(sessionUnique(), n, sizeof(n));
  g.setTextDatum(TL_DATUM);
  g.setFont(F_XL);
  if (g.textWidth(n) > w - 96) g.setFont(F_L);
  g.setTextColor(wardriveActive ? UI_TEXT : UI_DIM, TFT_BLACK);
  g.drawString(n, bx + 12, by + (land ? 4 : 8));
  g.setFont(F_XS);
  g.setTextColor(UI_MUTE, TFT_BLACK);
  g.drawString(land ? "unique" : (wardriveActive || sessionUnique() ? "unique networks + devices" : "nothing captured yet"), bx + 14, by + (land ? 44 : 52));
  // the fresh-catch badge
  int nw = sweepNew24 + sweepNew5 + sweepNewBle;
  char p[12];
  snprintf(p, sizeof(p), "+%d", nw);
  g.setFont(F_L);
  int pw = g.textWidth(p) + 18, ph = 28;
  int px = bx + w - 14 - pw, py = by + (land ? 6 : 10);
  if (nw > 0) {
    g.fillRoundRect(px, py, pw, ph, ph / 2, C_NEW);
    g.setTextDatum(MC_DATUM);
    g.setTextColor(inkOn(C_NEW), C_NEW);
    g.drawString(p, px + pw / 2, py + ph / 2 + 1);
  } else {
    g.fillRoundRect(px, py, pw, ph, ph / 2, uiCardFill());
    g.setTextDatum(MC_DATUM);
    g.setTextColor(UI_DIM, uiCardFill());
    g.drawString(p, px + pw / 2, py + ph / 2 + 1);
  }
  g.setFont(F_XS);
  g.setTextDatum(TR_DATUM);
  g.setTextColor(UI_DIM, TFT_BLACK);
  g.drawString("last sweep", bx + w - 14, py + ph + 4);
}

void wdDrawBand(LovyanGFX &g, int bx, int by, int w, int h, int i) {
  const char* lab[3] = { "2.4 GHz", "5 GHz", w >= 66 ? "Bluetooth" : "BLE" };
  const uint16_t col[3] = { C_W24, C_W5, C_BLE };
  long tot[3] = { sessionWifi24Total, sessionWifi5Total, sessionBleTotal };
  int nw[3] = { sweepNew24, sweepNew5, sweepNewBle };
  long mx = max(1L, max(tot[0], max(tot[1], tot[2])));
  bool live = (scanPhase == 1 && i < 2) || (scanPhase == 3 && i == 2);
  g.setFont(F_XS);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(col[i], TFT_BLACK);
  g.drawString(lab[i], bx, by);
  if (live && wardriveActive) g.fillCircle(bx + g.textWidth(lab[i]) + 6, by + 5, 2, col[i]);
  char nb[14];
  fmtNum((uint32_t)tot[i], nb, sizeof(nb));
  g.setFont(w >= 66 ? F_L : F_M);
  if (g.textWidth(nb) > w - 8) g.setFont(F_M);
  g.setTextColor(UI_TEXT, TFT_BLACK);
  g.drawString(nb, bx - 1, by + 13);
  // proportion bar
  int bw = w - 10;
  g.fillRoundRect(bx, by + h - 4, bw, 3, 1, UI_CARD);
  int fw = (int)(bw * (tot[i] / (float)mx));
  if (fw > 2) g.fillRoundRect(bx, by + h - 4, fw, 3, 1, col[i]);
  if (nw[i] > 0 && w >= 66) {
    char p[10];
    snprintf(p, sizeof(p), "+%d", nw[i] > 999 ? 999 : nw[i]);
    g.setFont(F_XS);
    g.setTextDatum(TR_DATUM);
    g.setTextColor(C_NEW, TFT_BLACK);
    g.drawString(p, bx + w - 10, by + 16);
  }
}

void wdDrawProgress(LovyanGFX &g, int bx, int by, int w, int h) {
  g.fillRect(bx, by, w, h, UI_CARD);
  if (!wardriveActive) return;
  float p = 0;
  uint16_t c = getAccent();
  unsigned long now = millis();
  if (wdWaitingGps) {
    int seg = w / 4, pos = (int)((now / 12) % (w + seg)) - seg;
    int x0 = max(0, pos), x1 = min(w, pos + seg);
    if (x1 > x0) g.fillRect(bx + x0, by, x1 - x0, h, C_NEW);
    return;
  }
  if (scanPhase == 1) {
    unsigned long est = wdLastSweepMs ? (unsigned long)(wdLastSweepMs * 0.75f) : 3000UL;
    p = min(1.0f, (now - wdPhaseStartMs) / (float)max(500UL, est)) * 0.75f;
  } else if (scanPhase == 3) {
    unsigned long bl = (unsigned long)(proMode ? 2 : wdBleSec) * 1000UL;
    p = 0.75f + min(1.0f, (now - wdPhaseStartMs) / (float)bl) * 0.25f;
    c = C_BLE;
  } else {
    p = 1.0f;
    c = blend565(UI_CARD, getAccent(), 110);
  }
  int fw = (int)(w * p);
  if (fw > 0) g.fillRect(bx, by, fw, h, c);
}

void wdDrawGps(LovyanGFX &g, int bx, int by, int w, int h) {
  uint16_t col = gpsValid ? C_OK : (gpsSats > 0 ? C_NEW : UI_DIM);
  int cy = by + h / 2;
  g.fillCircle(bx + 3, cy, 3, col);
  char b[48];
  if (gpsValid && w < 200) snprintf(b, sizeof(b), "%d sats  locked", gpsSats);
  else if (gpsValid) snprintf(b, sizeof(b), "%d sats   %.5f, %.5f", gpsSats, gpsLat, gpsLng);
  else snprintf(b, sizeof(b), "%d sats   logging pauses until GPS locks", gpsSats);
  g.setFont(F_XS);
  g.setTextDatum(ML_DATUM);
  g.setTextColor(UI_MUTE, TFT_BLACK);
  String bs = b;
  int room = w - 16 - (gpsValid ? 50 : 0);
  while (bs.length() > 1 && g.textWidth(bs.c_str()) > room) bs = bs.substring(0, bs.length() - 1);
  g.drawString(bs.c_str(), bx + 12, cy);
  if (gpsValid) {
    snprintf(b, sizeof(b), "%.0f mph", gpsSpeed);
    g.setTextDatum(MR_DATUM);
    g.setTextColor(UI_TEXT, TFT_BLACK);
    g.drawString(b, bx + w, cy);
  }
}

void wdDrawSide(LovyanGFX &g, int bx, int by, int w, int h) {
  char b[24];
  g.setFont(F_XS);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(UI_DIM, TFT_BLACK);
  g.drawString("Distance", bx, by + 2);
  g.drawString("Sweep", bx, by + 36);
  snprintf(b, sizeof(b), "%.1f mi", wdDistM / 1609.344);
  g.setFont(F_M);
  g.setTextColor(UI_TEXT, TFT_BLACK);
  g.drawString(b, bx, by + 13);
  if (wdLastSweepMs) snprintf(b, sizeof(b), "%.1fs", wdLastSweepMs / 1000.0f);
  else snprintf(b, sizeof(b), "--");
  g.drawString(b, bx, by + 47);
  (void)w; (void)h;
}

void drawLastHits(int x, int y, int w, int maxRows) {
  paintRegion(x, y, w, maxRows * 14 + 2, [&](LovyanGFX &g, int bx, int by) {
    g.setFont(F_XS);
    g.setTextDatum(TL_DATUM);
    if (proHitN == 0) {
      g.setTextColor(proText, TFT_BLACK);
      g.drawString("LAST HITS  --", bx, by + 2);
      return;
    }
    int shown = (proHitN < maxRows) ? proHitN : maxRows;
    for (int i = 0; i < shown; i++) {
      int idx = (proHitHead + 5 - 1 - i) % 5;
      ProHit &hit = proHits[idx];
      int yy = by + i * 14 + 2;
      g.setTextColor(hit.kind == 'B' ? C_BLE : proCyan, TFT_BLACK);
      g.drawString(hit.kind == 'B' ? "BLE" : "WIFI", bx, yy);
      g.setTextColor(TFT_WHITE, TFT_BLACK);
      g.drawString(hit.name, bx + 30, yy);
      char rs[8];
      snprintf(rs, sizeof(rs), "%d", hit.rssi);
      g.setTextDatum(TR_DATUM);
      g.setTextColor(proAmber, TFT_BLACK);
      g.drawString(rs, bx + w, yy);
      g.setTextDatum(TL_DATUM);
    }
  });
}

void wdDrawButtons(bool force) {
  WdLayout L; wdLayout(L);
  bool armed = wardriveActive && wdStopArmMs && (millis() - wdStopArmMs < 3000);
  int sig = (wardriveActive ? 1 : 0) + (armed ? 2 : 0);
  if (!force && sig == wdBtnSig) return;
  wdBtnSig = sig;
  tft.fillRect(L.btnMain.x, L.btnMain.y, L.btnMain.w, L.btnMain.h, TFT_BLACK);
  if (!wardriveActive) drawThemedButton(L.btnMain.x, L.btnMain.y, L.btnMain.w, L.btnMain.h, "START", C_GREEN, TFT_WHITE);
  else if (armed) drawThemedButton(L.btnMain.x, L.btnMain.y, L.btnMain.w, L.btnMain.h, "TAP TO STOP", C_AMBER, TFT_WHITE);
  else drawThemedButton(L.btnMain.x, L.btnMain.y, L.btnMain.w, L.btnMain.h, "STOP", C_RED, TFT_WHITE);
  drawThemedButton(L.btnHome.x, L.btnHome.y, L.btnHome.w, L.btnHome.h, "HOME", getBtnFill(), getTextAccent());
}

void wdPaintDynamic(bool all) {
  WdLayout L; wdLayout(L);
  paintRegion(L.status.x, L.status.y, L.status.w, L.status.h, [&](LovyanGFX &g, int bx, int by) { wdDrawStatus(g, bx, by, L.status.w, L.status.h); });
  paintRegion(L.prog.x, L.prog.y, L.prog.w, L.prog.h, [&](LovyanGFX &g, int bx, int by) { wdDrawProgress(g, bx, by, L.prog.w, L.prog.h); });
  if (!all) return;
  paintRegion(L.hero.x, L.hero.y, L.hero.w, L.hero.h, [&](LovyanGFX &g, int bx, int by) { wdDrawHero(g, bx, by, L.hero.w, L.hero.h, L.land); });
  for (int i = 0; i < 3; i++)
    paintRegion(L.band[i].x, L.band[i].y, L.band[i].w, L.band[i].h, [&](LovyanGFX &g, int bx, int by) { wdDrawBand(g, bx, by, L.band[i].w, L.band[i].h, i); });
  paintRegion(L.gps.x, L.gps.y, L.gps.w, L.gps.h, [&](LovyanGFX &g, int bx, int by) { wdDrawGps(g, bx, by, L.gps.w, L.gps.h); });
  if (L.land) paintRegion(L.side.x, L.side.y, L.side.w, L.side.h, [&](LovyanGFX &g, int bx, int by) { wdDrawSide(g, bx, by, L.side.w, L.side.h); });
  if (proMode) {
    if (L.land) drawLastHits(L.lane.x + 2, L.lane.y + 4, L.lane.w - 4, 4);
    else drawLastHits(L.lane.x + 10, L.lane.y + 1, L.lane.w - 20, 3);
  }
}

void drawWardrivingScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("WARDRIVING");
  WdLayout L; wdLayout(L);
  wdPaintDynamic(true);
  wdBtnSig = -1;
  wdDrawButtons(true);
  if (!proMode) {
    // the selected vehicle "drives" on a treadmill while recording, parks when stopped
    bool right = true;
    laneConfigure(0, L.lane.x, L.lane.y, L.lane.w, L.lane.h, laneSceneFor(ROLE_WD), ROLE_WD, true, right);
  }
}

void updateWardrivingScreen() {
  if (currentScreen != WARDRIVING_SCREEN) return;
  wdPaintDynamic(true);
  wdDrawButtons(false);
}

// 4 Hz light refresh: clock, progress, blinking dot, stop-arm timeout
void wdUiTick() {
  if (currentScreen != WARDRIVING_SCREEN) return;
  unsigned long now = millis();
  if (now - wdUiLastTick < 250) return;
  wdUiLastTick = now;
  wdPaintDynamic(false);
  static unsigned long lastGpsPaint = 0;
  if (now - lastGpsPaint > 1000) {
    lastGpsPaint = now;
    WdLayout L; wdLayout(L);
    paintRegion(L.gps.x, L.gps.y, L.gps.w, L.gps.h, [&](LovyanGFX &g, int bx, int by) { wdDrawGps(g, bx, by, L.gps.w, L.gps.h); });
    if (L.land) paintRegion(L.side.x, L.side.y, L.side.w, L.side.h, [&](LovyanGFX &g, int bx, int by) { wdDrawSide(g, bx, by, L.side.w, L.side.h); });
  }
  if (wdStopArmMs && now - wdStopArmMs >= 3000) { wdStopArmMs = 0; }
  wdDrawButtons(false);
}

bool wardriveTap(int tx, int ty) {
  if (currentScreen != WARDRIVING_SCREEN) return false;
  WdLayout L; wdLayout(L);
  const int pad = 6;
  auto in = [&](const Rect &r) { return tx >= r.x - pad && tx <= r.x + r.w + pad && ty >= r.y - pad && ty <= r.y + r.h + pad; };
  lastUiTouchMs = millis();
  if (in(L.btnMain)) {
    if (!wardriveActive) {
      wdStopArmMs = 0;
      startWardriving();
      if (lanes[0].on) laneSetupActor(lanes[0]);
      wdPaintDynamic(true);
      wdDrawButtons(true);
      return true;
    }
    if (wdStopArmMs && millis() - wdStopArmMs < 3000) {
      wdStopArmMs = 0;
      stopWardriving();
      currentScreen = SUMMARY_SCREEN;
      drawSummaryScreen();
      return true;
    }
    wdStopArmMs = millis();      // first tap arms; second tap within 3 s stops
    beep(1, 25);
    wdDrawButtons(true);
    return true;
  }
  if (in(L.btnHome)) {
    currentScreen = HOME_SCREEN;   // recording keeps running in the background
    drawHomeScreen();
    beep(1, 40);
    return true;
  }
  return false;
}

// ---------- Session report ----------
void drawStatTile(int x, int y, int w, int h, const char* label, const char* value, uint16_t col, int icon) {
  (void)icon;
  uint16_t fill = uiCardFill();
  tft.fillRoundRect(x, y, w, h, 10, fill);
  tft.setFont(F_XS);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col == UI_MUTE ? UI_MUTE : col, fill);
  tft.drawString(label, x + 9, y + (h < 40 ? 4 : 7));
  tft.setTextDatum(BL_DATUM);
  tft.setFont(F_M);
  if (tft.textWidth(value) > w - 12) tft.setFont(F_S);
  tft.setTextColor(UI_TEXT, fill);
  tft.drawString(value, x + 8, y + h - (h < 40 ? 4 : 6));
  uiResetFont();
}

Rect summaryDoneRect() {
  int W = tft.width(), H = tft.height();
  if (W > H) return { 8, H - 38, W - 16, 32 };
  return { 8, H - 48, W - 16, 42 };
}

void drawSummaryScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("SESSION REPORT");
  int W = tft.width(), H = tft.height();
  bool land = W > H;
  char n[16], a[16], b[16], c[16];
  int y = 54;
  if (wdNewRecord) {
    tft.fillRoundRect(8, y, W - 16, 24, 12, C_NEW);
    drawIcon(ICON_TROPHY, 14, 18, y + 5, inkOn(C_NEW), C_NEW);
    tft.setTextDatum(ML_DATUM);
    tft.setFont(F_M);
    tft.setTextColor(inkOn(C_NEW), C_NEW);
    tft.drawString("New personal best", 38, y + 13);
    uiResetFont();
    y += 28;
  }
  Rect dr = summaryDoneRect();
  if (land) {
    // hero on the left, 2x3 stat grid on the right
    int heroW = 122, heroH = dr.y - 6 - y;
    uint16_t fill = TFT_BLACK;
    tft.setFont(F_XS);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(UI_MUTE, fill);
    tft.drawString("Unique this run", 12, y + 6);
    fmtNum(wdLastSessionTotal, n, sizeof(n));
    tft.setFont(F_XL);
    if (tft.textWidth(n) > heroW - 10) tft.setFont(F_L);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(UI_TEXT, fill);
    tft.drawString(n, 16, y + heroH - 22);
    uiResetFont();
    fmtNum(recBestSession, a, sizeof(a));
    char bl[24];
    snprintf(bl, sizeof(bl), "Best %s", a);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(UI_DIM, fill);
    tft.drawString(bl, 18, y + heroH - 7);
    int gx = 8 + heroW + 6, gw = W - 8 - gx, gap = 4;
    int tw = (gw - gap) / 2, th = (heroH - 2 * gap) / 3;
    fmtNum(sessionWifi24Total, a, sizeof(a));
    fmtNum(sessionWifi5Total, b, sizeof(b));
    fmtNum(sessionBleTotal, c, sizeof(c));
    drawStatTile(gx, y, tw, th, "2.4 GHz", a, C_W24, -1);
    drawStatTile(gx + tw + gap, y, tw, th, "5 GHz", b, C_W5, -1);
    drawStatTile(gx, y + th + gap, tw, th, "BLE", c, C_BLE, -1);
    fmtDur(wdLastSessionMs, a, sizeof(a));
    drawStatTile(gx + tw + gap, y + th + gap, tw, th, "Time", a, UI_MUTE, -1);
    snprintf(b, sizeof(b), "%.1f mi", wdDistM / 1609.344);
    float mins = wdLastSessionMs / 60000.0f;
    if (mins > 0.1f) snprintf(c, sizeof(c), "%.0f/min", wdLastSessionTotal / mins); else snprintf(c, sizeof(c), "--");
    drawStatTile(gx, y + 2 * (th + gap), tw, th, "Distance", b, UI_MUTE, -1);
    drawStatTile(gx + tw + gap, y + 2 * (th + gap), tw, th, "Rate", c, UI_MUTE, -1);
    drawThemedButton(dr.x, dr.y, dr.w, dr.h, "DONE", C_GREEN, TFT_WHITE);
    return;
  }
  // big total
  int heroH = 70;
  uint16_t fill = TFT_BLACK;
  fmtNum(wdLastSessionTotal, n, sizeof(n));
  tft.setFont(F_XL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(UI_TEXT, fill);
  tft.drawString(n, 12, y + 6);
  tft.setFont(F_XS);
  tft.setTextColor(UI_MUTE, fill);
  tft.drawString("unique this run", 14, y + 50);
  uiResetFont();
  y += heroH + 6;
  // stat tiles
  int cols = 3, gap = 4;
  int tw = (W - 16 - gap * (cols - 1)) / cols, th = 44;
  fmtNum(sessionWifi24Total, a, sizeof(a));
  fmtNum(sessionWifi5Total, b, sizeof(b));
  fmtNum(sessionBleTotal, c, sizeof(c));
  drawStatTile(8, y, tw, th, "2.4 GHz", a, C_W24, -1);
  drawStatTile(8 + tw + gap, y, tw, th, "5 GHz", b, C_W5, -1);
  drawStatTile(8 + 2 * (tw + gap), y, tw, th, "BLE", c, C_BLE, -1);
  y += th + gap;
  fmtDur(wdLastSessionMs, a, sizeof(a));
  snprintf(b, sizeof(b), "%.1f mi", wdDistM / 1609.344);
  float mins = wdLastSessionMs / 60000.0f;
  if (mins > 0.1f) snprintf(c, sizeof(c), "%.0f/min", wdLastSessionTotal / mins);
  else snprintf(c, sizeof(c), "--");
  drawStatTile(8, y, tw, th, "Time", a, UI_MUTE, -1);
  drawStatTile(8 + tw + gap, y, tw, th, "Distance", b, UI_MUTE, -1);
  drawStatTile(8 + 2 * (tw + gap), y, tw, th, "Rate", c, UI_MUTE, -1);
  y += th + 6;
  // records line
  fmtNum(recBestSession, a, sizeof(a));
  fmtNum(recLifetime, b, sizeof(b));
  char line[64];
  snprintf(line, sizeof(line), "Best %s    lifetime %s    %lu run%s", a, b, (unsigned long)recSessions, recSessions == 1 ? "" : "s");
  Rect d = summaryDoneRect();
  tft.setFont(F_XS);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(UI_MUTE, TFT_BLACK);
  int ly = min(y + 6, d.y - 10);
  tft.drawString(line, W / 2, ly);
  if (wdBestName[0] && !land && ly + 14 < d.y - 4) {
    char bl[48];
    snprintf(bl, sizeof(bl), "Strongest: %.20s (%d dBm)", wdBestName, wdBestRssi);
    tft.setTextColor(UI_DIM, TFT_BLACK);
    tft.drawString(bl, W / 2, ly + 13);
  }
  uiResetFont();
  drawThemedButton(d.x, d.y, d.w, d.h, "DONE", C_GREEN, TFT_WHITE);
}

// ================= GPS / NAVIGATION =================
// 0 fix, 1 searching (talking, no fix), 2 no data on the wire, 3 data but unreadable (baud / noise)
int gpsLinkState() {
  if (gpsValid) return 0;
  if (millis() - gpsLastByteMs > 6000) return 2;
  if (gps.charsProcessed() > 400 && gps.passedChecksum() == 0) return 3;
  return 1;
}
int satsToBars(int sats) {
  if (sats <= 0) return 0;
  if (sats <= 2) return 1;
  if (sats <= 4) return 2;
  if (sats <= 6) return 3;
  if (sats <= 8) return 4;
  return 5;
}

uint16_t barColor(int level) {
  if (level <= 1) return C_BAD;
  if (level == 2) return C_WARN;
  if (level == 3) return C_WARN;
  if (level == 4) return C565(150, 230, 60);
  return C_OK;
}

const char* courseLabel(double c) {
  static const char* n[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
  int i = (int)((c + 11.25) / 22.5) % 16;
  if (i < 0) i += 16;
  return n[i];
}

void gpsLayout(GpsGeom &G) {
  int W = tft.width(), H = tft.height();
  if (W > H) {
    int cw = 96;
    G.card[0] = { 8, 52, cw, 40 };      G.card[1] = { 8 + cw + 6, 52, cw, 40 };
    G.card[2] = { 8, 96, cw, 40 };      G.card[3] = { 8 + cw + 6, 96, cw, 40 };
    G.card[4] = { 8, 140, cw, 40 };     G.card[5] = { 8 + cw + 6, 140, cw, 40 };
    G.compass = { 216, 52, 96, 96 };
    G.sig = { 214, 152, 100, 46 };
  } else {
    int cw = (W - 16 - 6) / 2;
    G.card[0] = { 8, 52, cw, 44 };      G.card[1] = { 8 + cw + 6, 52, cw, 44 };
    G.card[2] = { 8, 100, cw, 44 };     G.card[3] = { 8 + cw + 6, 100, cw, 44 };
    G.card[4] = { 8, 148, cw, 40 };     G.card[5] = { 8 + cw + 6, 148, cw, 40 };
    G.compass = { W - 8 - 92, 192, 92, 86 };
    G.sig = { 8, 194, W - 16 - 100, 82 };
  }
}

void gpsCardValue(int i, char* v, size_t n, uint16_t &col) {
  col = gpsValid ? UI_TEXT : UI_DIM;
  switch (i) {
    case 0:
      if (gpsInView > 0 && !gpsValid) snprintf(v, n, "%d / %d", gpsSats, gpsInView);
      else snprintf(v, n, "%d", gpsSats);
      col = UI_TEXT; break;
    case 1:
      if (gpsValid) { snprintf(v, n, gps.hdop.isValid() && gps.hdop.hdop() < 2.0 ? "3D LOCK" : "LOCKED"); col = getAccent(); }
      else {
        int ls = gpsLinkState();
        bool antBad = gpsAnt[0] && strcmp(gpsAnt, "OK");
        snprintf(v, n, "%s", ls == 2 ? "NO DATA" : (ls == 3 ? "BAD BAUD" : (antBad ? "NO ANTENNA" : "SEARCH")));
        col = (ls >= 2 || antBad) ? C_BAD : C_NEW;
      }
      break;
    case 2: if (gpsValid) snprintf(v, n, "%.0f ft", gpsAlt * 3.28084); else snprintf(v, n, "---"); break;
    case 3: if (gpsValid) snprintf(v, n, "%.1f mph", gpsSpeed); else snprintf(v, n, "---"); break;
    case 4: if (gpsValid) snprintf(v, n, "%.6f", gpsLat); else snprintf(v, n, "---"); break;
    default: if (gpsValid) snprintf(v, n, "%.6f", gpsLng); else snprintf(v, n, "---"); break;
  }
}

void gpsDrawCard(LovyanGFX &g, int bx, int by, int w, int h, int i) {
  static const char* lab[6] = { "Satellites", "Fix", "Altitude", "Speed", "Latitude", "Longitude" };
  const char* lab0 = (i == 0 && gpsInView > 0 && !gpsValid) ? "Used / in view" : lab[i];
  char v[24]; uint16_t col;
  gpsCardValue(i, v, sizeof(v), col);
  if (i == 1) {
    int ls = gpsLinkState();
    if (gpsValid) snprintf(v, sizeof(v), "%s", gps.hdop.isValid() && gps.hdop.hdop() < 2.0 ? "3D lock" : "Locked");
    else snprintf(v, sizeof(v), "%s", ls == 2 ? "No data" : (ls == 3 ? "Bad data" : ((gpsAnt[0] && strcmp(gpsAnt, "OK")) ? "No antenna" : "Searching")));
  }
  uint16_t fill = uiCardFill();
  g.fillRoundRect(bx, by, w, h, 10, fill);
  g.setFont(F_XS);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(UI_MUTE, fill);
  g.drawString(lab0, bx + 10, by + 7);
  g.setTextDatum(BL_DATUM);
  g.setFont(i >= 4 ? F_M : F_L);
  if (g.textWidth(v) > w - 16) g.setFont(F_M);
  g.setTextColor(col == UI_TEXT || col == UI_DIM ? col : col, fill);
  g.drawString(v, bx + 9, by + h - 5);
  if (i == 0) {
    int bars = satsToBars(gpsValid ? gpsSats : max(gpsSats, gpsInView / 2));
    for (int k = 0; k < 5; k++) {
      int bh = 4 + k * 3, xx = bx + w - 34 + k * 5, yy = by + h - 8 - bh;
      g.fillRect(xx, yy, 3, bh, k < bars ? getAccent() : blend565(fill, TFT_WHITE, 30));
    }
  }
}

double gpsSmoothCourse = 0;

void gpsDrawCompass(LovyanGFX &g, int bx, int by, int w, int h) {
  int r = min(w, h) / 2 - 10;
  int cx = bx + w / 2, cy = by + h / 2;
  uint16_t acc = getAccent();
  g.fillCircle(cx, cy, r + 2, uiCardFill());
  g.drawCircle(cx, cy, r + 2, blend565(TFT_BLACK, acc, 120));
  for (int d = 0; d < 360; d += 30) {
    float a = d * DEG_TO_RAD;
    int len = (d % 90 == 0) ? 6 : 3;
    g.drawLine(cx + (int)((r - len) * sinf(a)), cy - (int)((r - len) * cosf(a)),
               cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), d % 90 == 0 ? UI_MUTE : UI_DIM);
  }
  g.setFont(F_XS);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(getAccent());
  g.drawString("N", cx, cy - r - 7 < by + 4 ? by + 4 : cy - r - 7);
  g.setTextColor(UI_MUTE);
  g.drawString("S", cx, min(by + h - 4, cy + r + 7));
  g.drawString("E", min(bx + w - 3, cx + r + 7), cy);
  g.drawString("W", max(bx + 3, cx - r - 7), cy);
  bool moving = gpsValid && gpsSpeed >= 0.6;
  if (moving) {
    double diff = gpsCourse - gpsSmoothCourse;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;
    gpsSmoothCourse += diff * 0.5;
    while (gpsSmoothCourse < 0) gpsSmoothCourse += 360;
    while (gpsSmoothCourse >= 360) gpsSmoothCourse -= 360;
  }
  float rad = gpsSmoothCourse * DEG_TO_RAD;
  if (!moving) {
    g.fillCircle(cx, cy, 4, UI_DIM);
    g.drawCircle(cx, cy, 8, UI_LINE);
    return;
  }
  int L = r - 7;
  int tipX = cx + (int)(L * sinf(rad)), tipY = cy - (int)(L * cosf(rad));
  int tailX = cx - (int)((L - 6) * sinf(rad)), tailY = cy + (int)((L - 6) * cosf(rad));
  float p = rad + 1.5708f;
  int wx = (int)(7 * sinf(p)), wy = (int)(-7 * cosf(p));
  int mx = cx - (int)(4 * sinf(rad)), my = cy + (int)(4 * cosf(rad));
  g.fillTriangle(tipX, tipY, tailX + wx, tailY + wy, mx, my, acc);
  g.fillTriangle(tipX, tipY, tailX - wx, tailY - wy, mx, my, blend565(acc, TFT_BLACK, 90));
  g.fillCircle(cx, cy, 2, TFT_WHITE);
}

void gpsDrawSig(LovyanGFX &g, int bx, int by, int w, int h) {
  bool land = tft.width() > tft.height();
  char b[24];
  bool moving = gpsValid && gpsSpeed >= 0.6;
  g.setFont(F_XS);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(UI_MUTE, TFT_BLACK);
  g.drawString("Heading", bx + 4, by + 4);
  if (moving) snprintf(b, sizeof(b), "%.0f %s", gpsCourse, courseLabel(gpsCourse));
  else snprintf(b, sizeof(b), gpsValid ? "Parked" : "--");
  g.setFont(land ? F_M : F_L);
  if (g.textWidth(b) > w - 8) g.setFont(F_M);
  g.setTextColor(moving ? UI_TEXT : UI_DIM, TFT_BLACK);
  g.drawString(b, bx + 3, by + 18);
  if (!land) {
    g.setFont(F_XS);
    g.setTextColor(UI_DIM, TFT_BLACK);
    int ls = gpsLinkState();
    if (ls == 2) { g.setTextColor(C_BAD, TFT_BLACK); snprintf(b, sizeof(b), "No data on RX %d", gpsPinsSwapped ? gpsTxPin : gpsRxPin); }
    else if (ls == 3) { g.setTextColor(C_BAD, TFT_BLACK); snprintf(b, sizeof(b), "Unreadable: baud?"); }
    else if (ls == 1) {
      if (gpsInView) snprintf(b, sizeof(b), "%d in view, no fix", gpsInView);
      else if (gpsSats) snprintf(b, sizeof(b), "%d used, no fix yet", gpsSats);   // module sends no GSV list
      else snprintf(b, sizeof(b), "Data OK, no sats yet");
    }
    else if (gpsDiagFixPct >= 0) {
      if (gpsDiagFixPct < 90) g.setTextColor(C_NEW, TFT_BLACK);
      snprintf(b, sizeof(b), "Fix %d%%  HDOP %.1f", gpsDiagFixPct, gps.hdop.isValid() ? gps.hdop.hdop() : 0.0);
    }
    else if (gps.hdop.isValid()) snprintf(b, sizeof(b), "HDOP %.1f", gps.hdop.hdop());
    else snprintf(b, sizeof(b), "HDOP --");
    g.drawString(b, bx + 4, by + h - 28);
    g.setTextColor(UI_DIM, TFT_BLACK);
    if (!gpsValid && gpsAnt[0] && strcmp(gpsAnt, "OK")) { g.setTextColor(C_BAD, TFT_BLACK); snprintf(b, sizeof(b), "Antenna %s", gpsAnt); }
    else if (!gpsValid && gpsAnt[0]) snprintf(b, sizeof(b), "Antenna OK");
    else if (gpsValid && gpsDiagFixPct >= 0) {   // link health: bad sentences, lost bytes, longest loop gap
      if (gpsDiagBad || gpsDiagLost || gpsDiagLag > 1000) {
        g.setTextColor(C_NEW, TFT_BLACK);
        snprintf(b, sizeof(b), "Bad %lu Lost %lu %lums", gpsDiagBad, gpsDiagLost, gpsDiagLag);
      } else {   // how long since the module last reported a new position
        unsigned long ago = gpsMovedMs ? (millis() - gpsMovedMs) / 100 : 0;
        if (ago >= 30) g.setTextColor(C_NEW, TFT_BLACK);
        snprintf(b, sizeof(b), "Moved %lu.%lu sec ago", ago / 10, ago % 10);
      }
    }
    else if (gps.time.isValid()) snprintf(b, sizeof(b), "%02d:%02d:%02d UTC", gps.time.hour(), gps.time.minute(), gps.time.second());
    else snprintf(b, sizeof(b), "--:--:-- UTC");
    g.drawString(b, bx + 4, by + h - 15);
  }
}

void drawGPSScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("GPS");
  GpsGeom G; gpsLayout(G);
  for (int i = 0; i < 6; i++) gpsDrawCard(tft, G.card[i].x, G.card[i].y, G.card[i].w, G.card[i].h, i);
  paintRegion(G.sig.x, G.sig.y, G.sig.w, G.sig.h, [&](LovyanGFX &g, int bx, int by) { gpsDrawSig(g, bx, by, G.sig.w, G.sig.h); });
  paintRegion(G.compass.x, G.compass.y, G.compass.w, G.compass.h, [&](LovyanGFX &g, int bx, int by) { gpsDrawCompass(g, bx, by, G.compass.w, G.compass.h); });
  drawBackButton("HOME");
}

void updateGPSScreen() {
  if (currentScreen != GPS_SCREEN) return;
  static String last[6];
  static int lastCourse = -999;
  static int lastSig = -1;
  GpsGeom G; gpsLayout(G);
  for (int i = 0; i < 6; i++) {
    char v[24]; uint16_t col;
    gpsCardValue(i, v, sizeof(v), col);
    String key = String(v) + (i == 0 ? String(satsToBars(gpsValid ? gpsSats : max(gpsSats, gpsInView / 2))) : String(""));
    if (key == last[i]) continue;
    last[i] = key;
    paintRegion(G.card[i].x, G.card[i].y, G.card[i].w, G.card[i].h, [&](LovyanGFX &g, int bx, int by) { gpsDrawCard(g, bx, by, G.card[i].w, G.card[i].h, i); });
  }
  bool moving = gpsValid && gpsSpeed >= 0.6;
  int cKey = moving ? (int)(gpsCourse / 3) : -1;
  int sKey = (gpsValid ? 1 : 0) + (gps.time.isValid() ? gps.time.second() * 2 : 0) + (moving ? (int)gpsCourse * 1000 : 0)
           + gpsLinkState() * 200 + gpsInView * 7 + (int)gpsAnt[0] * 3
           + gpsDiagFixPct * 13 + (int)gpsDiagBad * 17 + (int)gpsDiagLost * 19 + (int)(gpsDiagLag / 50) * 23
           + (gpsMovedMs ? (int)((millis() - gpsMovedMs) / 400) * 29 : 0);
  if (cKey != lastCourse || moving) {
    lastCourse = cKey;
    paintRegion(G.compass.x, G.compass.y, G.compass.w, G.compass.h, [&](LovyanGFX &g, int bx, int by) { gpsDrawCompass(g, bx, by, G.compass.w, G.compass.h); });
  }
  if (sKey != lastSig) {
    lastSig = sKey;
    paintRegion(G.sig.x, G.sig.y, G.sig.w, G.sig.h, [&](LovyanGFX &g, int bx, int by) { gpsDrawSig(g, bx, by, G.sig.w, G.sig.h); });
  }
}


// ---------- Settings list + UI colors ----------

void settingsGeom(int &listTop, int &listBottom, int &btnH, int &rowH, int &btnW, int &ax) {
  int w = tft.width();
  int h = tft.height();
  bool landscape = (w > h);
  listTop = 52;
  // Portrait: tighter rows so one more full button fits above HOME
  btnH = landscape ? 38 : 36;
  rowH = landscape ? 44 : 40;
  int rawBottom = h - (landscape ? 44 : 42);
  // Only whole rows — no partial/empty slot at top or bottom
  int visibleRows = (rawBottom - listTop) / rowH;
  if (visibleRows < 1) visibleRows = 1;
  if (visibleRows > settingsCount) visibleRows = settingsCount;
  listBottom = listTop + visibleRows * rowH;
  btnW = w - 58;
  ax = w - 36;
}

// ST7789 CYD is typically BGR: R and B bits are swapped on the glass.
// Each value is the RGB565 word that *looks like* the name on this panel.



// ================= DRAG-TO-SCROLL LISTS =================
// One component for every long list: drag with a finger, tap to pick (on
// release, so a drag never triggers a row), slim scrollbar on the right.


void listPaint(int top, int bot, int rowH, int count, int &scroll, RowFn row) {
  int w = tft.width(), vh = bot - top;
  int contentH = count * rowH;
  int maxS = max(0, contentH - vh);
  scroll = constrain(scroll, 0, maxS);
  paintRegion(0, top, w, vh, [&](LovyanGFX &g, int bx, int by) {
    int first = scroll / rowH;
    for (int i = first; i < count; i++) {
      int y = by + i * rowH - scroll;
      if (y > by + vh) break;
      row(g, i, bx + 10, y, w - 26, rowH);
    }
    if (maxS > 0) {
      int th = max(18, vh * vh / max(1, contentH));
      int ty = by + (vh - th) * scroll / maxS;
      g.fillRoundRect(bx + w - 6, by, 3, vh, 1, UI_CARD);
      g.fillRoundRect(bx + w - 6, ty, 3, th, 1, UI_MUTE);
    }
  });
}

// Returns the tapped row index on a tap-release inside the list, else -1.
int listDragTouch(DragState &d, bool touching, int tx, int ty, int top, int bot, int rowH, int count,
                  int &scroll, std::function<void()> repaint) {
  if (touching && !d.down) {
    if (ty < top || ty > bot) return -1;          // buttons outside the list are handled elsewhere
    d.down = true; d.drag = false; d.x0 = tx; d.y0 = ty; d.s0 = scroll;
  }
  if (!d.down) return -1;
  if (touching) {
    int dy = ty - d.y0;
    if (!d.drag && abs(dy) > 10) d.drag = true;
    if (d.drag) {
      int maxS = max(0, count * rowH - (bot - top));
      int ns = constrain(d.s0 - dy, 0, maxS);
      if (ns != scroll && millis() - d.last > 35) { scroll = ns; repaint(); d.last = millis(); }
    }
    return -1;
  }
  d.down = false;
  if (d.drag) return -1;
  int idx = (d.y0 - top + scroll) / rowH;
  return (idx >= 0 && idx < count) ? idx : -1;
}

void listHint(const char* s) {
  tft.setFont(F_XS);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(UI_DIM, TFT_BLACK);
  tft.drawString(s, tft.width() - 14, tft.height() - 22);
  uiResetFont();
}

// ---------- Theme ----------
const uint16_t colorOptions[] = {
  C565(64, 240, 150),  // Mint
  C565(64, 208, 255),  // Ice
  C565(255, 210, 70),  // Gold
  C565(255, 138, 60),  // Ember
  C565(255, 72, 96),   // Siren
  C565(236, 96, 232),  // Neon
  C565(104, 140, 255), // Cobalt
  C565(226, 230, 238), // Chrome
  C565(255, 0, 255)    // WDGW (pink + cyan)
};
const char* colorNames[] = { "Mint", "Ice", "Gold", "Ember", "Siren", "Neon", "Cobalt", "Chrome", "WDGW" };
const int colorCount = 9;
DragState themeDrag = {};
void themeGeom(int &top, int &bot) { top = 54; bot = tft.height() - 46; }

void themePaintList() {
  int top, bot; themeGeom(top, bot);
  listPaint(top, bot, 44, colorCount, colorScroll, [&](LovyanGFX &g, int i, int x, int y, int w, int h) {
    bool selected = (i == 8) ? wdgwTheme : (!wdgwTheme && accentColor == colorOptions[i]);
    uint16_t fill = selected ? uiRaisedFill() : uiCardFill();
    int rh = h - 6;
    g.fillRoundRect(x, y + 3, w, rh, rh / 2, fill);
    int cx = x + 22, cy = y + 3 + rh / 2;
    uint16_t a = (i == 8) ? wdgwChrome : colorOptions[i];
    g.fillCircle(cx, cy, 8, a);
    if (i == 8) g.fillArc(cx, cy, 8, 0, 270, 90, wdgwText);
    g.setFont(F_M);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(UI_TEXT, fill);
    g.drawString(colorNames[i], x + 42, cy + 1);
    if (i == 8) {
      g.setFont(F_XS);
      g.setTextDatum(MR_DATUM);
      g.setTextColor(UI_DIM, fill);
      g.drawString("pink + cyan", x + w - (selected ? 34 : 16), cy + 1);
    }
    if (selected) drawIconOn(g, ICON_CHECK, 14, x + w - 26, cy - 7, getAccent(), fill);
  });
}

void drawUIColorScreen() {
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("UI COLOR");
  themePaintList();
  drawBackButton("BACK");
  listHint("Drag to scroll");
}

void themeTap(int i) {
  if (i == 8) applyWdgwTheme(); else applyUIColor(colorOptions[i]);
}

// ---------- Logs (files) ----------
DragState filesDrag = {};
int filesScrollPx = 0;
void filesListGeom(int &top, int &bot, int &rowH) { top = 70; bot = tft.height() - 82; rowH = 36; }

void filesPaintList() {
  int top, bot, rowH; filesListGeom(top, bot, rowH);
  if (fileBrowserCount == 0) {
    paintRegion(0, top, tft.width(), bot - top, [&](LovyanGFX &g, int bx, int by) {
      g.setFont(F_S);
      g.setTextDatum(MC_DATUM);
      g.setTextColor(UI_DIM, TFT_BLACK);
      g.drawString("Start a wardrive to create logs", bx + tft.width() / 2, by + (bot - top) / 2);
    });
    return;
  }
  listPaint(top, bot, rowH, fileBrowserCount, filesScrollPx, [&](LovyanGFX &g, int row, int x, int y, int w, int h) {
    bool sel = fileSelected[row];
    uint16_t bg = sel ? uiRaisedFill() : uiCardFill();
    int rh = h - 4;
    g.fillRoundRect(x, y + 2, w, rh, rh / 2, bg);
    int cx = x + 18, cy = y + 2 + rh / 2;
    if (sel) {
      g.fillCircle(cx, cy, 7, getAccent());
      drawIconOn(g, ICON_CHECK, 14, cx - 7, cy - 7, inkOn(getAccent()), getAccent());
    } else {
      g.drawCircle(cx, cy, 6, UI_DIM);
    }
    String name = fileNames[row];
    int ic = ICON_FOLDER;
    if (name.startsWith("SKIrf_WD_C_")) ic = ICON_CAR;
    else if (name.startsWith("SKIrf_WD_M_")) ic = ICON_MOTO;
    else if (name.startsWith("SKIrf_WD_W_")) ic = ICON_BIKE;
    drawIconOn(g, ic, 14, x + 34, cy - 7, UI_MUTE, bg);
    String shown = name;
    if (shown.startsWith("SKIrf_WD_")) shown = shown.substring(11);
    if (shown.endsWith(".csv")) shown = shown.substring(0, shown.length() - 4);
    g.setFont(F_S);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(UI_TEXT, bg);
    int maxW = w - 62;
    while (shown.length() > 1 && g.textWidth(shown.c_str()) > maxW) shown = shown.substring(0, shown.length() - 1);
    g.drawString(shown.c_str(), x + 56, cy + 1);
  });
}

void drawFilesScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("FILES");
  int w = tft.width();
  int h = tft.height();
  int selN = filesSelectedCount();
  char sum[40];
  if (fileBrowserCount == 0) snprintf(sum, sizeof(sum), "No logs on the %s yet", logUseLittleFS ? "flash" : "SD card");
  else if (selN) snprintf(sum, sizeof(sum), "%d of %d selected", selN, fileBrowserCount);
  else snprintf(sum, sizeof(sum), "%d runs  -  tap to select", fileBrowserCount);
  tft.setFont(F_XS);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(selN ? getAccent() : UI_MUTE, TFT_BLACK);
  tft.drawString(sum, 14, 60);
  uiResetFont();
  filesPaintList();
  int gap = 6, dw = (w - 24 - 2 * gap) / 3, dy = h - 76, bh = 32;
  bool allOn = (fileBrowserCount > 0 && selN == fileBrowserCount);
  drawThemedButton(12, dy, dw, bh, allOn ? "NONE" : "ALL", getBtnFill(), TFT_WHITE);
  drawThemedButton(12 + dw + gap, dy, dw, bh, "WIGLE", selN ? C_GREEN : getBtnFill(), TFT_WHITE);
  drawThemedButton(12 + 2 * (dw + gap), dy, dw, bh, "WDGW", getBtnFill(), TFT_WHITE);
  const char* delLab = (fileArmDelete == 1) ? "CONFIRM" : "DELETE";
  drawThemedButton(8 + 100, h - 38, w - 16 - 100, 32, delLab, fileArmDelete == 1 ? C_AMBER : getBtnFill(), TFT_WHITE);
  drawBackButton("BACK");
}

// ---------- Choose network (Wi-Fi scan) ----------
DragState wifiDrag = {};
int wifiScrollPx = 0;
void wifiListGeom(int &top, int &bot, int &rowH) { top = 54; bot = tft.height() - 48; rowH = 40; }

void drawRssiBarsOn(LovyanGFX &g, int x, int y, int rssi, uint16_t bg) {
  int lvl = rssi >= -55 ? 4 : (rssi >= -67 ? 3 : (rssi >= -78 ? 2 : 1));
  for (int k = 0; k < 4; k++) {
    int bh = 3 + k * 3;
    g.fillRect(x + k * 4, y + 12 - bh, 3, bh, k < lvl ? getAccent() : blend565(bg, TFT_WHITE, 34));
  }
}

void wifiPaintList() {
  int top, bot, rowH; wifiListGeom(top, bot, rowH);
  if (wifiScanCount == 0) {
    paintRegion(0, top, tft.width(), bot - top, [&](LovyanGFX &g, int bx, int by) {
      g.setFont(F_S);
      g.setTextDatum(MC_DATUM);
      g.setTextColor(UI_DIM, TFT_BLACK);
      g.drawString("No networks found - scan again", bx + tft.width() / 2, by + (bot - top) / 2);
    });
    return;
  }
  listPaint(top, bot, rowH, wifiScanCount, wifiScrollPx, [&](LovyanGFX &g, int idx, int x, int y, int w, int h) {
    uint16_t fill = uiCardFill();
    int rh = h - 6;
    g.fillRoundRect(x, y + 3, w, rh, rh / 2, fill);
    int cy = y + 3 + rh / 2;
    drawRssiBarsOn(g, x + 14, cy - 7, wifiScanRSSI[idx], fill);
    String ss = wifiScanSSID[idx];
    bool hidden = ss.length() == 0;
    g.setFont(hidden ? F_S : F_DS);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(hidden ? UI_DIM : UI_TEXT, fill);
    if (hidden) ss = "Hidden network";
    int maxW = w - 40 - 44;
    while (ss.length() > 1 && g.textWidth(ss.c_str()) > maxW) ss = ss.substring(0, ss.length() - 1);
    g.drawString(ss.c_str(), x + 38, cy + 1);
    char rb[12];
    snprintf(rb, sizeof(rb), "%d", wifiScanRSSI[idx]);
    g.setFont(F_XS);
    g.setTextDatum(MR_DATUM);
    g.setTextColor(UI_MUTE, fill);
    g.drawString(rb, x + w - 14, cy + 1);
  });
}

void drawWifiScanScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("SELECT WIFI");
  wifiPaintList();
  int w = tft.width(), h = tft.height();
  int navW = (w - 24) / 2;
  drawThemedButton(8, h - 40, navW, 34, "BACK", getBtnFill(), getTextAccent());
  drawThemedButton(16 + navW, h - 40, navW, 34, "RESCAN", getBtnFill(), getTextAccent());
}

// ---------- About & safety ----------
DragState discDrag = {};
int discScrollPx = 0;
static const char* DISC_LINES[] = {
  "Photosensitivity / epilepsy", "",
  "A very small percentage of", "people may experience seizures", "or blackouts when exposed to",
  "certain light patterns or", "flashing lights, including on", "screen displays.", "",
  "This device shows animated", "graphics, moving elements,", "status updates and colour", "changes that could trigger",
  "discomfort in sensitive users.", "",
  "If you or anyone in your family", "has an epileptic condition,", "consult a physician before use.", "",
  "Stop immediately if you feel", "dizziness, altered vision, eye", "or muscle twitches, loss of", "awareness, disorientation, any",
  "involuntary movement, or", "convulsions.", "",
  "Use in a well-lit area and take", "regular breaks. You assume all", "risk related to continued use.", "",
  "This firmware is for education", "and authorized testing only.", "Follow your local laws.",
};
static const int DISC_N = sizeof(DISC_LINES) / sizeof(DISC_LINES[0]);
void discGeom(int &top, int &bot, int &rowH) { top = 54; bot = tft.height() - 46; rowH = 16; }

void discPaintList() {
  int top, bot, rowH; discGeom(top, bot, rowH);
  listPaint(top, bot, rowH, DISC_N, discScrollPx, [&](LovyanGFX &g, int i, int x, int y, int w, int h) {
    if (!DISC_LINES[i][0]) return;
    g.setFont(F_XS);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(i == 0 ? C_WARN : UI_TEXT, TFT_BLACK);
    g.drawString(DISC_LINES[i], x + 4, y + h / 2);
    (void)w;
  });
}

void drawDisclaimerScreen() {
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("DISCLAIMER");
  discPaintList();
  drawBackButton("BACK");
  listHint("Drag to scroll");
}

// ---------- WDGWars stats ----------
DragState statsDrag = {};
StatLine statLines[12];
int statN = 0;

void statsBuild() {
  statN = 0;
  if (!wdgMeOk) {
    statLines[statN] = { "Couldn't load stats", "", C_BAD }; statN++;
    String s = wdgMeStatus;
    while (s.length() && statN < 12) {
      int take = min(26, (int)s.length());
      String bit = s.substring(0, take);
      int sp = bit.lastIndexOf(' ');
      if (sp > 8 && (int)s.length() > 26) { bit = s.substring(0, sp); s = s.substring(sp + 1); }
      else s = s.substring(bit.length());
      StatLine &L = statLines[statN++];
      L.lab = ""; strncpy(L.val, bit.c_str(), 27); L.val[27] = 0; L.col = UI_MUTE;
    }
    return;
  }
  StatLine &u = statLines[statN++];
  u.lab = nullptr; strncpy(u.val, wdgMeUser.c_str(), 27); u.val[27] = 0; u.col = getAccent();
  if (wdgMeGang.length()) {
    StatLine &gg = statLines[statN++];
    gg.lab = nullptr; strncpy(gg.val, wdgMeGang.c_str(), 27); gg.val[27] = 0; gg.col = UI_MUTE;
  }
  struct { const char* lab; long v; uint16_t c; } st[] = {
    {"Wi-Fi", wdgMeWifi, UI_TEXT}, {"Bluetooth", wdgMeBle, UI_TEXT}, {"Aircraft", wdgMeAircraft, UI_TEXT},
    {"Mesh", wdgMeMesh, UI_TEXT}, {"Total", wdgMeTotal, getAccent()}, {"Today", wdgMeToday, C_NEW}, {"Last 7 days", wdgMe7d, C_NEW},
  };
  for (auto &e : st) {
    StatLine &L = statLines[statN++];
    L.lab = e.lab;
    fmtNum((uint32_t)max(0L, e.v), L.val, sizeof(L.val));
    L.col = e.c;
  }
}

void statsGeom(int &top, int &bot, int &rowH) { top = 54; bot = tft.height() - 46; rowH = 38; }

void statsPaintList() {
  int top, bot, rowH; statsGeom(top, bot, rowH);
  listPaint(top, bot, rowH, statN, gameStatsScroll, [&](LovyanGFX &g, int i, int x, int y, int w, int h) {
    StatLine &L = statLines[i];
    uint16_t bg = uiCardFill();
    int rh = h - 6;
    int cy = y + 3 + rh / 2;
    if (!wdgMeOk) {
      g.setFont(i == 0 ? F_M : F_XS);
      g.setTextDatum(ML_DATUM);
      g.setTextColor(i == 0 ? C_BAD : UI_MUTE, TFT_BLACK);
      g.drawString(i == 0 ? L.lab : L.val, x + 4, cy);
      return;
    }
    g.fillRoundRect(x, y + 3, w, rh, rh / 2, bg);
    if (!L.lab) {
      g.setFont(F_M);
      g.setTextDatum(MC_DATUM);
      g.setTextColor(L.col, bg);
      g.drawString(L.val, x + w / 2, cy + 1);
    } else {
      g.setFont(F_S);
      g.setTextDatum(ML_DATUM);
      g.setTextColor(UI_MUTE, bg);
      g.drawString(L.lab, x + 16, cy + 1);
      g.setFont(F_M);
      g.setTextDatum(MR_DATUM);
      g.setTextColor(L.col, bg);
      g.drawString(L.val, x + w - 16, cy + 1);
    }
  });
}

void drawGameStatsScreen(bool doFetch) {
  int w = tft.width(), h = tft.height();
  if (doFetch) {
    tft.fillScreen(TFT_BLACK);
    drawTitleBar("WDGW STATS");
    tft.setTextDatum(MC_DATUM);
    tft.setFont(F_M);
    tft.setTextColor(UI_TEXT, TFT_BLACK);
    tft.drawString("Loading WDGWars...", w / 2, h / 2 - 8);
    tft.setFont(F_XS);
    tft.setTextColor(UI_MUTE, TFT_BLACK);
    tft.drawString("Needs Wi-Fi and an API key", w / 2, h / 2 + 14);
    uiResetFont();
    String err;
    fetchWdgGameStats(err);
  }
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("WDGW STATS");
  statsBuild();
  statsPaintList();
  drawThemedButton(8 + 100, h - 38, w - 16 - 100, 32, "REFRESH", getBtnFill(), TFT_WHITE);
  drawBackButton("BACK");
}

// ---------- Pro console ----------
DragState proDrag = {};
void proGeom(int &top, int &bot, int &rowH) { top = 54; bot = tft.height() - 46; rowH = 40; }

void proRowText(int i, const char* &lab, char* val, size_t n) {
  static const char* labs[15] = {
    "Wi-Fi scan", "BLE scan", "BLE duty", "Sweep interval",
    "Wi-Fi RSSI min", "Wi-Fi RSSI max", "BLE RSSI min", "BLE RSSI max",
    "GPS gate", "GPS accuracy", "Skip open APs", "Dedupe BSSID",
    "Channels", "Live session", "Exit Pro mode"
  };
  lab = labs[i];
  switch (i) {
    case 0: snprintf(val, n, "%s", proWifiOn ? "On" : "Off"); break;
    case 1: snprintf(val, n, "%s", proBleOn ? "On" : "Off"); break;
    case 2: snprintf(val, n, "%s", proBleDuty == 0 ? "Low" : (proBleDuty == 2 ? "High" : "Bal")); break;
    case 3: snprintf(val, n, "%d s", proIntervalMs / 1000); break;
    case 4: snprintf(val, n, "%d", proWifiRssiMin); break;
    case 5: snprintf(val, n, "%d", proWifiRssiMax); break;
    case 6: snprintf(val, n, "%d", proBleRssiMin); break;
    case 7: snprintf(val, n, "%d", proBleRssiMax); break;
    case 8: snprintf(val, n, "%s", proGpsGate ? "Fix only" : "Always"); break;
    case 9: snprintf(val, n, "%s", proAccMode == 1 ? "HDOP 5" : (proAccMode == 2 ? "HDOP 2" : (proAccMode == 3 ? "6 sats" : "Off"))); break;
    case 10: snprintf(val, n, "%s", proSkipOpen ? "Yes" : "No"); break;
    case 11: snprintf(val, n, "%s", proDedupe ? "On" : "Off"); break;
    case 12: snprintf(val, n, "%s", proChanMode == 1 ? "2.4" : (proChanMode == 2 ? "5" : (proChanMode == 3 ? "1/6/11" : "All"))); break;
    case 13: snprintf(val, n, "Open"); break;
    default: snprintf(val, n, " "); break;
  }
}

void proPaintList() {
  int top, bot, rowH; proGeom(top, bot, rowH);
  listPaint(top, bot, rowH, proRowCount, proScroll, [&](LovyanGFX &g, int i, int x, int y, int w, int h) {
    const char* lab; char val[16];
    proRowText(i, lab, val, sizeof(val));
    uint16_t bg = uiCardFill();
    int rh = h - 6, cy = y + 3 + rh / 2;
    g.fillRoundRect(x, y + 3, w, rh, rh / 2, bg);
    g.setFont(F_S);
    g.setTextDatum(ML_DATUM);
    g.setTextColor(i == 14 ? C_BAD : UI_TEXT, bg);
    g.drawString(lab, x + 16, cy + 1);
    g.setFont(F_M);
    g.setTextDatum(MR_DATUM);
    g.setTextColor(getAccent(), bg);
    g.drawString(val, x + w - 16, cy + 1);
  });
}

void drawProScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("PRO CONSOLE");
  proPaintList();
  drawBackButton("BACK");
  listHint("Tap a row to change it");
}

// ---------- Dispatcher (called every loop) ----------
void scrollListsTouch(bool touching, int tx, int ty) {
  int top, bot, rowH, idx;
  switch (currentScreen) {
    case UI_COLOR_SCREEN:
      themeGeom(top, bot);
      idx = listDragTouch(themeDrag, touching, tx, ty, top, bot, 44, colorCount, colorScroll, themePaintList);
      if (idx >= 0) { beep(1, 25); themeTap(idx); touchIgnoreUntilUp = true; }
      break;
    case FILES_SCREEN:
      filesListGeom(top, bot, rowH);
      idx = listDragTouch(filesDrag, touching, tx, ty, top, bot, rowH, fileBrowserCount, filesScrollPx, filesPaintList);
      if (idx >= 0) { fileSelected[idx] = !fileSelected[idx]; fileArmDelete = 0; beep(1, 18); drawFilesScreen(); }
      break;
    case WIFI_SCAN_SCREEN:
      wifiListGeom(top, bot, rowH);
      idx = listDragTouch(wifiDrag, touching, tx, ty, top, bot, rowH, wifiScanCount, wifiScrollPx, wifiPaintList);
      if (idx >= 0) {
        pendingSSID = wifiScanSSID[idx];
        pendingPass = "";
        currentScreen = WIFI_PASS_SCREEN;
        drawWifiPassScreen();
        beep(1, 30);
        touchIgnoreUntilUp = true;
      }
      break;
    case DISCLAIMER_SCREEN:
      discGeom(top, bot, rowH);
      listDragTouch(discDrag, touching, tx, ty, top, bot, rowH, DISC_N, discScrollPx, discPaintList);
      break;
    case GAME_STATS_SCREEN:
      statsGeom(top, bot, rowH);
      listDragTouch(statsDrag, touching, tx, ty, top, bot, rowH, statN, gameStatsScroll, statsPaintList);
      break;
    case PRO_SCREEN:
      proGeom(top, bot, rowH);
      idx = listDragTouch(proDrag, touching, tx, ty, top, bot, rowH, proRowCount, proScroll, proPaintList);
      if (idx >= 0) { activateProRow(idx); touchIgnoreUntilUp = true; }
      break;
    default: break;
  }
}

// ---------- Theme apply, file list, Wi-Fi scan ----------
void applyUIColor(uint16_t c) {
  accentColor = c;
  wdgwTheme = false;
  saveCalibration();
  beep(2, 60);
  drawUIColorScreen();
}

void applyWdgwTheme() {
  wdgwTheme = true;
  accentColor = wdgwChrome;
  saveCalibration();
  beep(2, 60);
  drawUIColorScreen();
}






void showWdgStatusScreen(const char* line1, const char* line2 = "", const char* line3 = "",
                          int progress = -1, int progressMax = 0, const char* title = "UPLOAD") {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar(title && title[0] ? title : "UPLOAD");
  int w = tft.width();
  int h = tft.height();
  bool land = w > h;
  int cw = min(280, w - 24), chh = land ? 150 : 176;
  int cx = (w - cw) / 2, cy = 52 + ((h - 52 - 8) - chh) / 2;
  uiCard(cx, cy, cw, chh, getAccent(), false);
  uint16_t fill = uiCardFill();
  int icon = iconForTitle(title && title[0] ? title : "UPLOAD");
  // icon in a tinted ring
  int ix = w / 2, iy = cy + (land ? 28 : 32);
  tft.fillCircle(ix, iy, 20, blend565(fill, getAccent(), 50));
  drawIcon(icon, 26, ix - 13, iy - 13, getAccent(), blend565(fill, getAccent(), 50));
  int ty = iy + 34;
  tft.setTextDatum(MC_DATUM);
  tft.setFont(F_M);
  if (tft.textWidth(line1) > cw - 16) tft.setFont(F_S);
  tft.setTextColor(UI_TEXT, fill);
  tft.drawString(line1, w / 2, ty);
  uiResetFont();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_OK, fill);
  if (line2 && line2[0]) tft.drawString(line2, w / 2, ty + 20);
  tft.setTextColor(UI_MUTE, fill);
  if (line3 && line3[0]) {
    String l3 = line3;
    int maxc = (cw - 16) / 6;
    if ((int)l3.length() > maxc) l3 = l3.substring(0, maxc - 1) + "~";
    tft.drawString(l3, w / 2, ty + 34);
  }
  if (progress >= 0 && progressMax > 0) {
    int barW = cw - 32, barH = 10;
    int bx = cx + 16, by = cy + chh - 30;
    tft.fillRoundRect(bx, by, barW, barH, 5, C565(30, 33, 42));
    int fw = (int)((long)barW * progress / progressMax);
    fw = constrain(fw, 0, barW);
    if (fw > 6) tft.fillRoundRect(bx, by, fw, barH, 5, getAccent());
    char pct[24];
    snprintf(pct, sizeof(pct), "%d / %d", progress, progressMax);
    tft.setTextColor(UI_TEXT, fill);
    tft.drawString(pct, w / 2, by + barH + 9);
  }
  uiResetFont();
}

// On-device WDGWars upload (needs home WiFi + API key)

void loadCsvFileList() {
  fileBrowserCount = 0;
  for (int i = 0; i < 40; i++) { fileSelected[i] = false; fileNames[i] = ""; }
  File root = logFs().open("/");
  if (!root) return;
  File f = root.openNextFile();
  while (f && fileBrowserCount < 40) {
    if (!f.isDirectory()) {
      String name = f.name();
      if (name.startsWith("/")) name = name.substring(1);
      String lower = name; lower.toLowerCase();
      if (lower.endsWith(".csv")) {
        fileNames[fileBrowserCount] = name;
        fileSelected[fileBrowserCount] = false;
        fileBrowserCount++;
      }
    }
    f = root.openNextFile();
  }
  root.close();
  fileBrowserScroll = 0; filesScrollPx = 0;
}



void doWifiScan() {
  wifiScanCount = 0;
  wifiScanScroll = 0; wifiScrollPx = 0;
  showWdgStatusScreen("Scanning WiFi...", "Please wait", "", -1, 0, "WIFI");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(80);
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;
  if (n > 20) n = 20;
  for (int i = 0; i < n; i++) {
    wifiScanSSID[i] = WiFi.SSID(i);
    wifiScanRSSI[i] = WiFi.RSSI(i);
  }
  wifiScanCount = n;
  WiFi.scanDelete();
}

void drawRssiBars(int x, int y, int rssi, uint16_t bg) {
  int lvl = rssi >= -55 ? 4 : (rssi >= -67 ? 3 : (rssi >= -78 ? 2 : 1));
  uint16_t on = lvl >= 3 ? C_OK : (lvl == 2 ? C_WARN : C_BAD);
  for (int k = 0; k < 4; k++) {
    int bh = 3 + k * 3;
    tft.fillRect(x + k * 4, y + 12 - bh, 3, bh, k < lvl ? on : blend565(bg, UI_LINE, 220));
  }
}


// Keyboard layers: 0=lower, 1=UPPER, 2=symbols
// ---------- Big-key password keyboard (resistive-friendly) ----------
// Modes: 0=letters, 1=numbers, 2=symbols
// Letter pages: 0=A-M, 1=N-Z (lower/upper via kbUpperCase)
int kbMode = 0;
int kbPage = 0;
bool kbUpperCase = false;

static const char* kbLettersLo[2] = {
  "abcdefghijklm",
  "nopqrstuvwxyz"
};
static const char* kbLettersUp[2] = {
  "ABCDEFGHIJKLM",
  "NOPQRSTUVWXYZ"
};
static const char* kbNumbers = "1234567890";
static const char* kbSymbols = "!@#$%&*()-_=+.?";

// Fills geometry for current screen size
void wifiPassKbGeom(int &fieldY, int &fieldH, int &gridY, int &keyH, int &keyGap,
                    int &actY, int &actH, int &joinY, int &joinH, int &cols) {
  int w = tft.width();
  int h = tft.height();
  bool land = (w > h);
  cols = land ? 7 : 7;
  if (land) {
    fieldY = 40;
    fieldH = 22;
    gridY = 64;
    keyGap = 3;
    actH = 28;
    joinH = 30;
    joinY = h - joinH - 2;
    actY = joinY - actH - 3;
    // 2 key rows between gridY and actY
    int gridH = actY - gridY - 2;
    keyH = (gridH - keyGap) / 2;
    if (keyH < 28) keyH = 28;
    if (keyH > 40) keyH = 40;
  } else {
    fieldY = 66;
    fieldH = 28;
    gridY = 100;
    keyGap = 4;
    actH = 32;
    joinH = 36;
    joinY = h - joinH - 6;
    actY = joinY - actH - 6;
    int gridH = actY - gridY - 4;
    keyH = (gridH - keyGap) / 2;
    if (keyH < 34) keyH = 34;
    if (keyH > 48) keyH = 48;
  }
}

const char* wifiPassActiveKeys() {
  if (kbMode == 1) return kbNumbers;
  if (kbMode == 2) return kbSymbols;
  return kbUpperCase ? kbLettersUp[kbPage & 1] : kbLettersLo[kbPage & 1];
}

void drawWifiPassScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("PASSWORD");
  int w = tft.width();
  int h = tft.height();
  bool land = (w > h);

  int fieldY, fieldH, gridY, keyH, keyGap, actY, actH, joinY, joinH, cols;
  wifiPassKbGeom(fieldY, fieldH, gridY, keyH, keyGap, actY, actH, joinY, joinH, cols);

  // SSID caption (portrait has room above the field)
  if (!land) {
    tft.setTextDatum(ML_DATUM);
    tft.setFont(F_XS);
    tft.setTextColor(UI_MUTE, TFT_BLACK);
    String title = pendingSSID.length() ? pendingSSID : "SSID";
    if ((int)title.length() > 30) title = title.substring(0, 30);
    drawIcon(ICON_WIFI, 14, 8, fieldY - 16, getAccent(), TFT_BLACK);
    tft.drawString(title, 26, fieldY - 9);
  }
  // Password field
  uint16_t ff = uiCardFill();
  tft.fillRoundRect(6, fieldY, w - 12, fieldH, fieldH / 2, ff);
  tft.setFont(F_DS);
  tft.setTextDatum(ML_DATUM);
  String shown = pendingPass;
  int maxShow = land ? 36 : 24;
  if ((int)shown.length() > maxShow) shown = shown.substring(shown.length() - maxShow);
  if (shown.length()) {
    tft.setTextColor(UI_TEXT, ff);
    tft.drawString(shown, 13, fieldY + fieldH / 2 + 1);
    int cx = 13 + tft.textWidth(shown) + 2;
    tft.fillRect(cx, fieldY + 6, 2, fieldH - 12, getAccent());
  } else {
    tft.setTextColor(UI_DIM, ff);
    tft.setFont(F_XS);
    tft.drawString("Type the Wi-Fi password", 13, fieldY + fieldH / 2 + 1);
  }
  uiResetFont();

  // Key grid: 2 rows x cols large keys
  const char* keys = wifiPassActiveKeys();
  int nkeys = strlen(keys);
  int keyW = (w - 8) / cols;
  int startX = (w - keyW * cols) / 2;
  for (int i = 0; i < nkeys && i < cols * 2; i++) {
    int r = i / cols;
    int c = i % cols;
    int x = startX + c * keyW;
    int y = gridY + r * (keyH + keyGap);
    uint16_t kf = blend565(uiCardFill(), TFT_WHITE, 10);
    tft.fillRoundRect(x + 2, y, keyW - 4, keyH, 8, kf);
    char ch[2] = { keys[i], 0 };
    tft.setTextDatum(MC_DATUM);
    tft.setFont(F_DK);
    tft.setTextColor(UI_TEXT, kf);
    tft.drawString(ch, x + keyW / 2, y + keyH / 2);
  }
  uiResetFont();

  // Mode / edit row: SPC | ABC | 123 | # | < | > | BKSP
  const char* labs[7] = { "SPC", "ABC", "123", "#+=", "<", ">", "DEL" };
  if (kbMode == 0) labs[1] = kbUpperCase ? "abc" : "ABC";
  int nAct = 7;
  int aw = (w - 10) / nAct;
  for (int i = 0; i < nAct; i++) {
    int x = 5 + i * aw;
    bool on = (i == 1 && kbMode == 0 && kbUpperCase) || (i == 2 && kbMode == 1) || (i == 3 && kbMode == 2) ||
              (i == 4 && kbMode == 0 && kbPage == 0) || (i == 5 && kbMode == 0 && kbPage == 1);
    uint16_t af = on ? blend565(uiCardFill(), getAccent(), 70) : uiCardFill();
    tft.fillRoundRect(x, actY, aw - 2, actH, actH / 2, af);
    tft.setTextDatum(MC_DATUM);
    tft.setFont(F_XS);
    tft.setTextColor(on ? UI_TEXT : UI_MUTE, af);
    tft.drawString(labs[i], x + (aw - 2) / 2, actY + actH / 2);
  }

  int jw = (w - 18) / 2;
  drawThemedButton(6, joinY, jw, joinH, "JOIN", C_GREEN, TFT_BLACK);
  drawThemedButton(12 + jw, joinY, jw, joinH, "BACK", getBtnFill(), getTextAccent());
}






// ---------- Wi-Fi screen ----------
void drawWifiScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("WIFI");
  int w = tft.width();
  int h = tft.height();
  bool landscape = (w > h);

  bool connected = (WiFi.status() == WL_CONNECTED);
  homeWifiConnected = connected;
  if (connected) homeWifiIP = WiFi.localIP().toString();

  int navH = landscape ? 36 : 34;
  int navY = h - navH - 6;
  int btnH = landscape ? 40 : 44;
  int btnY = navY - btnH - 8;
  int btnW = (w - 24) / 2;
  int gap = 8;
  int x0 = 8;
  int x1 = x0 + btnW + gap;

  // Saved-network card
  int cy = 54, ch = btnY - 8 - cy;
  if (ch > 110) ch = 110;
  uint16_t fill = uiCardFill();
  uiCard(8, cy, w - 16, ch, connected ? C_OK : uiBorder(), false);
  int icx = 8 + 30, icy = cy + (landscape ? ch / 2 : 34);
  uint16_t ic = connected ? C_OK : UI_MUTE;
  tft.fillCircle(icx, icy, 20, blend565(fill, ic, 45));
  drawIcon(ICON_WIFI, 26, icx - 13, icy - 13, ic, blend565(fill, ic, 45));
  tft.setFont(F_XS);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(UI_MUTE, fill);
  tft.setFont(F_XS);
  tft.drawString("Saved network", 62, cy + 12);
  uiResetFont();
  String ssidShow = homeSSID.length() ? homeSSID : "(none saved)";
  tft.setFont(homeSSID.length() ? F_DK : F_M);
  while (ssidShow.length() > 1 && tft.textWidth(ssidShow) > w - 80) ssidShow = ssidShow.substring(0, ssidShow.length() - 1);
  tft.setTextColor(homeSSID.length() ? UI_TEXT : UI_DIM, fill);
  tft.drawString(ssidShow, 62, cy + 25);
  uiResetFont();
  const char* st = connected ? "Connected" : "Offline";
  uint16_t sc = connected ? C_OK : C_BAD;
  tft.setFont(F_XS);
  tft.setTextDatum(TL_DATUM);
  tft.fillCircle(66, cy + 62, 3, sc);
  tft.setTextColor(sc, fill);
  tft.drawString(st, 74, cy + 58);
  if (connected) {
    tft.setTextColor(UI_TEXT, fill);
    tft.drawString(homeWifiIP, 74 + tft.textWidth(st) + 10, cy + 58);
  }
  if (!landscape && ch >= 90) {
    tft.setTextColor(UI_DIM, fill);
    tft.drawString(connected ? "Uploads ready" : "Connect to upload logs", 20, cy + ch - 18);
  }
  uiResetFont();

  uint16_t connCol = connected ? getBtnFill() : C_OK;
  uint16_t discCol = connected ? C_BAD : getBtnFill();
  drawThemedButton(x0, btnY, btnW, btnH, "CONNECT", connCol, TFT_WHITE);
  drawThemedButton(x1, btnY, btnW, btnH, "OFFLINE", discCol, TFT_WHITE);

  int navW = (w - 24) / 2;
  drawThemedButton(8, navY, navW, navH, "BACK", getBtnFill(), getTextAccent());
  drawThemedButton(16 + navW, navY, navW, navH, "FIND NET", getBtnFill(), getTextAccent());
}

// ---------- Logs: select, delete, upload from the board ----------
int filesSelectedCount() {
  int n = 0;
  for (int i = 0; i < fileBrowserCount; i++) if (fileSelected[i]) n++;
  return n;
}

void filesGeom(int &listTop, int &listBottom, int &rowH, int &rowX, int &rowW,
               int &ax, int &dockY, int &viewRows) {
  int w = tft.width();
  int h = tft.height();
  listTop = 70;
  listBottom = h - 82;
  rowH = 36;
  rowX = 10;
  rowW = w - 26;
  ax = w;
  dockY = h - 76;
  viewRows = max(1, (listBottom - listTop - 6) / rowH);
}


void filesShowMsg(const char* a, const char* b) {
  showWdgStatusScreen(a, b, "Returning...", -1, 0, "FILES");
  delay(2500);
  loadCsvFileList();
  drawFilesScreen();
}

void filesDeleteSelected() {
  int n = 0;
  for (int i = 0; i < fileBrowserCount; i++) {
    if (!fileSelected[i]) continue;
    String path = "/" + fileNames[i];
    if (logFs().exists(path)) { logFs().remove(path); n++; }
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "Removed %d file(s)", n);
  filesShowMsg("Delete selected", buf);
}

void filesDeleteAll() {
  int n = 0;
  for (int i = 0; i < fileBrowserCount; i++) {
    String path = "/" + fileNames[i];
    if (logFs().exists(path)) { logFs().remove(path); n++; }
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "Removed %d file(s)", n);
  filesShowMsg("Delete all CSV", buf);
}

void filesUploadSelected(bool doWigle, bool doWdg) {
  if (WiFi.status() != WL_CONNECTED) {
    if (homeSSID.length() == 0 || !tryConnectHomeWifi(15000)) {
      filesShowMsg("No internet WiFi", "Set home WiFi first");
      return;
    }
  }
  int ok = 0, fail = 0, skip = 0, done = 0;
  for (int i = 0; i < fileBrowserCount; i++) {
    if (!fileSelected[i]) continue;
    done++;
    String path = "/" + fileNames[i];
    showWdgStatusScreen("Uploading...", fileNames[i].c_str(), "", done - 1, max(1, fileBrowserCount), "FILES");
    if (doWigle) {
      if (wigleApiName.length() < 1) fail++;
      else if (uploadOneCsvToWigle(path) == 1) ok++; else fail++;
    }
    if (doWdg) {
      if (wdgApiKey.length() < 8) fail++;
      else {
        int r = uploadOneCsvToWdg(path);
        if (r == 1) ok++; else if (r == 2) skip++; else fail++;
      }
    }
  }
  if (done == 0) { filesShowMsg("Nothing selected", "Tap files to select"); return; }
  char res[48];
  snprintf(res, sizeof(res), "OK:%d Skip:%d Fail:%d", ok, skip, fail);
  showWdgStatusScreen("Upload done", res, "Returning...", done, done, "FILES");
  delay(5000);
  loadCsvFileList();
  drawFilesScreen();
}

void doWdgUploadFromDevice() {
  if (wdgApiKey.length() < 8) {
    showWdgStatusScreen("No API key", "Set key in web portal", "Settings > OPEN PORTAL");
    delay(2500);
    drawSettingsScreen();
    return;
  }

  // Ensure WiFi
  if (WiFi.status() != WL_CONNECTED) {
    showWdgStatusScreen("Connecting WiFi...", homeSSID.c_str(), "Please wait");
    if (homeSSID.length() == 0 || !tryConnectHomeWifi(15000)) {
      showWdgStatusScreen("No internet WiFi", "Set home WiFi in portal", "or move in range");
      delay(2500);
      drawSettingsScreen();
      return;
    }
  }

  showWdgStatusScreen("Uploading CSVs...", WiFi.localIP().toString().c_str(), "Do not power off");

  String names[40];
  int n = 0;
  File root = logFs().open("/");
  if (root) {
    File f = root.openNextFile();
    while (f && n < 40) {
      if (!f.isDirectory()) {
        String name = f.name();
        if (name.startsWith("/")) name = name.substring(1);
        String lower = name; lower.toLowerCase();
        if (lower.endsWith(".csv")) names[n++] = name;
      }
      f = root.openNextFile();
    }
    root.close();
  }

  if (n == 0) {
    showWdgStatusScreen("No CSV files", "Run a wardrive first", "");
    delay(2000);
    drawSettingsScreen();
    return;
  }

  int ok = 0, skip = 0, fail = 0;
  for (int i = 0; i < n; i++) {
    char line2[40];
    snprintf(line2, sizeof(line2), "File %d of %d", i + 1, n);
    showWdgStatusScreen("WDGWars upload", line2, names[i].c_str(), i, n);
    int r = uploadOneCsvToWdg("/" + names[i]);
    if (r == 1) ok++;
    else if (r == 2) skip++;
    else fail++;
    // show progress after file completes
    showWdgStatusScreen("WDGWars upload", line2, names[i].c_str(), i + 1, n);
  }

  char result[64];
  snprintf(result, sizeof(result), "OK:%d  Skip:%d  Fail:%d", ok, skip, fail);
  showWdgStatusScreen("Upload finished", result, "Read results — returning soon", n, n);
  beep(ok > 0 ? 2 : 1, 60);
  delay(7000);
  drawSettingsScreen();
}



// ---------- WDGWars WDGW Stats (GET /api/me) ----------
bool fetchWdgGameStats(String &errOut) {
  errOut = "";
  wdgMeOk = false;
  if (wdgApiKey.length() < 8) {
    errOut = "No WDG API key — set in web portal";
    wdgMeStatus = errOut;
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (homeSSID.length() == 0) {
      errOut = "No home WiFi — set in portal";
      wdgMeStatus = errOut;
      return false;
    }
    if (!tryConnectHomeWifi(15000)) {
      errOut = "WiFi connect failed";
      wdgMeStatus = errOut;
      return false;
    }
  }

  SKIRF_SECURE_CLIENT client;
  client.setInsecure();
  client.setTimeout(25000);
  if (!client.connect("wdgwars.pl", 443)) {
    errOut = "TLS to wdgwars.pl failed";
    wdgMeStatus = errOut;
    return false;
  }

  client.print("GET /api/me HTTP/1.1\r\n");
  client.print("Host: wdgwars.pl\r\n");
  client.print("User-Agent: SKIrf-Wardriver/1.0\r\n");
  client.print("Accept: application/json\r\n");
  client.print("X-API-Key: " + wdgApiKey + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.flush();

  unsigned long t0 = millis();
  while (client.connected() && !client.available() && millis() - t0 < 20000) {
    delay(10);
    yield();
  }
  if (!client.available()) {
    client.stop();
    errOut = "No response from server";
    wdgMeStatus = errOut;
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  Serial.println(statusLine);

  int contentLen = -1;
  bool chunked = false;
  t0 = millis();
  while (millis() - t0 < 10000) {
    if (!client.available()) {
      if (!client.connected()) break;
      delay(5);
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    String low = line;
    low.toLowerCase();
    if (low.startsWith("content-length:")) {
      contentLen = low.substring(15).toInt();
    } else if (low.indexOf("chunked") >= 0) {
      chunked = true;
    }
  }

  String body;
  body.reserve(contentLen > 0 ? (size_t)contentLen + 16 : 1024);
  t0 = millis();

  if (chunked) {
    while (millis() - t0 < 15000) {
      while (!client.available() && client.connected() && millis() - t0 < 15000) {
        delay(5);
        yield();
      }
      if (!client.available()) break;
      String hexLen = client.readStringUntil('\n');
      hexLen.trim();
      int cl = (int)strtol(hexLen.c_str(), nullptr, 16);
      if (cl <= 0) break;
      int got = 0;
      while (got < cl && millis() - t0 < 15000) {
        if (!client.available()) { delay(2); yield(); continue; }
        char c = (char)client.read();
        if (body.length() < 8192) body += c;
        got++;
      }
      client.readStringUntil('\n');
    }
  } else {
    while (millis() - t0 < 15000) {
      while (client.available()) {
        char c = (char)client.read();
        if (body.length() < 8192) body += c;
        if (contentLen > 0 && (int)body.length() >= contentLen) goto body_done;
      }
      if (!client.connected() && !client.available()) break;
      delay(5);
      yield();
    }
  }
body_done:
  client.stop();

  Serial.printf("WDG /api/me body len=%u contentLen=%d chunked=%d\n",
                (unsigned)body.length(), contentLen, (int)chunked);

  int brace = body.indexOf('{');
  if (brace < 0) {
    errOut = statusLine.length() ? statusLine : "No JSON in response";
    wdgMeStatus = errOut;
    return false;
  }
  body = body.substring(brace);
  int last = body.lastIndexOf('}');
  if (last > 0) body = body.substring(0, last + 1);

  DynamicJsonDocument doc(4096);
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    errOut = String("JSON: ") + e.c_str();
    Serial.printf("JSON fail tail: %s\n",
                  body.substring(body.length() > 100 ? body.length() - 100 : 0).c_str());
    wdgMeStatus = errOut;
    return false;
  }

  if (doc["ok"] != !doc.containsKey("username")) {
    const char* er = doc["error"] | "API rejected key";
    errOut = er;
    wdgMeStatus = errOut;
    return false;
  }

  wdgMeUser = doc["username"] | "?";
  wdgMeGang = doc["gang"] | "";
  wdgMeWifi = doc["wifi"] | 0;
  wdgMeBle = doc["ble"] | 0;
  wdgMeAircraft = doc["aircraft"] | 0;
  wdgMeMesh = doc["mesh"] | 0;
  wdgMeTotal = doc["total"] | 0;
  wdgMeToday = doc["recent_today"] | 0;
  wdgMe7d = doc["recent_7d"] | 0;
  wdgMeOk = true;
  wdgMeStatus = "OK";
  errOut = "";
  return true;
}



// ---------- Settings actions, Pro session, brightness ----------
void activateSettingsItem(int i) {
  if (i < 0 || i >= settingsCount) return;
  beep(1, 35);
  touchIgnoreUntilUp = true;
  if (i == 0) { currentScreen = BRIGHTNESS_SCREEN; drawBrightnessScreen(); }
  else if (i == 1) { currentScreen = UI_COLOR_SCREEN; colorScroll = 0; drawUIColorScreen(); }
  else if (i == 2) { currentScreen = ORIENTATION_SCREEN; drawOrientationScreen(); }
  else if (i == 3) {
    startPortalAP();
    runPortalCountdown();  // non-blocking; loop ticks until done
    touchIgnoreUntilUp = true;
  }
  else if (i == 4) {
    loadCsvFileList();
    currentScreen = FILES_SCREEN;
    drawFilesScreen();
  }
  else if (i == 5) {
    currentScreen = WIFI_SCREEN;
    drawWifiScreen();
  }
  else if (i == 6) {
    disclaimerScroll = 0; discScrollPx = 0;
    currentScreen = DISCLAIMER_SCREEN;
    drawDisclaimerScreen();
  }
  else if (i == 7) {
    gpsPinEditRx = gpsRxPin;
    gpsPinEditTx = gpsTxPin;
    gpsEditFocusRx = true;
    gpsPinsScroll = 0;
    currentScreen = GPS_PINS_SCREEN;
    drawGpsPinsScreen();
  }
  else if (i == 8) { runCalibration(); }
  else if (i == 9) {
    currentScreen = GAME_STATS_SCREEN;
    gameStatsScroll = 0;
    drawGameStatsScreen(true);  // fetch on open
  }
  else if (i == 10) {
    if (!proMode) {
      proMode = true;
      saveCalibration();
    }
    proScroll = 0;
    currentScreen = PRO_SCREEN;
    drawProScreen();
  }
}



void drawProSessionScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("SESSION");
  int w = tft.width();
  int h = tft.height();
  const int top = 52, bot = h - 44;
  tft.fillRoundRect(4, top, w - 8, bot - top, 6, C565(4, 14, 4));
  const int lines = 13;
  int rowH = (bot - top - 8) / lines;
  if (rowH > 22) rowH = 22;
  bool big = rowH >= 17;
  int y = top + 4 + rowH / 2;
  uint16_t bg = C565(4, 14, 4);
  auto line = [&](const char* k, const char* v, uint16_t vc) {
    if (big) tft.setFont(F_S); else tft.setFont(F_XS);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(proText, bg);
    tft.drawString(k, 12, y);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(vc ? vc : proCyan, bg);
    tft.drawString(v, w - 12, y);
    y += rowH;
  };
  char b[40];
  snprintf(b, sizeof(b), "%d", proSweepCount);
  line("SWEEPS", b, proCyan);
  if (proLastSweepMs) snprintf(b, sizeof(b), "%lus ago  (%.1fs)", (millis() - proLastSweepMs) / 1000, wdLastSweepMs / 1000.0f);
  else snprintf(b, sizeof(b), "--");
  line("LAST SWEEP", b, TFT_WHITE);
  snprintf(b, sizeof(b), "%d", sessionWifi24Total);
  line("UNIQUE 2.4", b, C_W24);
  snprintf(b, sizeof(b), "%d", sessionWifi5Total);
  line("UNIQUE 5", b, C_W5);
  snprintf(b, sizeof(b), "%d", sessionBleTotal);
  line("UNIQUE BLE", b, C_BLE);
  snprintf(b, sizeof(b), "%lu", (unsigned long)rowsLogged);
  line("CSV ROWS", b, proCyan);
  snprintf(b, sizeof(b), "%d / %d / %d", wardriveWifi24Count, wardriveWifi5Count, wardriveBleCount);
  line("LAST 2.4/5/BLE", b, TFT_WHITE);
  if (proRssiSeenMax > -127) snprintf(b, sizeof(b), "%d .. %d", proRssiSeenMin, proRssiSeenMax);
  else snprintf(b, sizeof(b), "--");
  line("RSSI SEEN", b, proAmber);
  line("GPS", gpsValid ? "LOCK" : "SEARCH", gpsValid ? C_OK : C_BAD);
  snprintf(b, sizeof(b), "%d", gpsSats);
  line("SATS", b, gpsSats >= 4 ? C_OK : C_WARN);
  line("WIFI RADIO", proWifiOn ? "ARM" : "SAFE", proWifiOn ? C_OK : C_BAD);
  line("BLE RADIO", proBleOn ? "ARM" : "SAFE", proBleOn ? C_OK : C_BAD);
  String fn = wardriveFile.length() ? wardriveFile : String("--");
  if (fn.startsWith("/")) fn = fn.substring(1);
  if (fn.length() > 24) fn = fn.substring(fn.length() - 24);
  line("LOG", fn.c_str(), UI_MUTE);
  uiResetFont();
  drawBackButton("BACK");
}

void proCycleRssi(int &v, bool isMin) {
  static const int steps[] = { -100, -95, -90, -85, -80, -70, -60, -50, -40, -30, -20, -10 };
  int n = 12;
  int idx = 0;
  for (int i = 0; i < n; i++) if (steps[i] == v) { idx = i; break; }
  idx = (idx + 1) % n;
  v = steps[idx];
  if (isMin && proWifiRssiMin > proWifiRssiMax) { /* allow, filter swaps */ }
  (void)isMin;
}

void activateProRow(int i) {
  beep(1, 25);
  if (i == 0) proWifiOn = !proWifiOn;
  else if (i == 1) proBleOn = !proBleOn;
  else if (i == 2) { proBleDuty = (proBleDuty + 1) % 3; }
  else if (i == 3) {
    if (proIntervalMs <= 3000) proIntervalMs = 5000;
    else if (proIntervalMs <= 5000) proIntervalMs = 8000;
    else if (proIntervalMs <= 8000) proIntervalMs = 12000;
    else proIntervalMs = 3000;
  }
  else if (i == 4) proCycleRssi(proWifiRssiMin, true);
  else if (i == 5) proCycleRssi(proWifiRssiMax, false);
  else if (i == 6) proCycleRssi(proBleRssiMin, true);
  else if (i == 7) proCycleRssi(proBleRssiMax, false);
  else if (i == 8) proGpsGate = !proGpsGate;
  else if (i == 9) proAccMode = (proAccMode + 1) % 4;
  else if (i == 10) proSkipOpen = !proSkipOpen;
  else if (i == 11) proDedupe = !proDedupe;
  else if (i == 12) proChanMode = (proChanMode + 1) % 4;
  else if (i == 13) {
    currentScreen = PRO_SESSION_SCREEN;
    drawProSessionScreen();
    return;
  }
  else if (i == 14) {
    proMode = false;
    saveCalibration();
    currentScreen = SETTINGS_SCREEN;
    drawSettingsScreen();
    return;
  }
  saveCalibration();
  drawProScreen();
}




void brightGeom(int &cardY, int &cardH, int &sliderY, int &presetY, int &presetH) {
  bool land = tft.width() > tft.height();
  if (land) { cardY = 54; cardH = 50; sliderY = 124; presetY = 156; presetH = 32; }
  else { cardY = 58; cardH = 62; sliderY = 150; presetY = 196; presetH = 36; }
}
static const int brightPresets[3] = { 30, 140, 255 };

void drawBrightnessValue() {
  int w = tft.width();
  int cardY, cardH, sliderY, presetY, presetH;
  brightGeom(cardY, cardH, sliderY, presetY, presetH);
  int cw = min(200, w - 40);
  int cx = (w - cw) / 2;
  uiCard(cx, cardY, cw, cardH, getAccent(), false);
  uint16_t fill = uiCardFill();
  drawIcon(ICON_BRIGHTNESS, 26, cx + 14, cardY + (cardH - 26) / 2, getAccent(), fill);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", (brightness * 100 + 127) / 255);
  tft.setTextDatum(MC_DATUM);
  tft.setFont(F_XL);
  if (cardH < 56) tft.setFont(F_L);
  tft.setTextColor(UI_TEXT, fill);
  tft.drawString(buf, cx + cw / 2 + 16, cardY + cardH / 2 + 1);
  uiResetFont();
}

// ================= SETTINGS: grouped list, drag to scroll =================
// Each row maps to an action index understood by activateSettingsItem():
// 0 bright 1 theme 2 orientation 3 portal 4 logs 5 wifi 6 about 7 gps pins 8 touch cal 9 stats 10 pro
static const SetRow SET_ROWS[] = {
  { "Display", -1, -1 },
  { "Brightness", 0, ICON_BRIGHTNESS }, { "Theme", 1, ICON_PALETTE }, { "Orientation", 2, ICON_ROTATE }, { "Touch calibration", 8, ICON_TAP },
  { "Connections", -1, -1 },
  { "Wi-Fi", 5, ICON_WIFI }, { "Web portal", 3, ICON_PORTAL }, { "GPS pins", 7, ICON_PIN },
  { "Your data", -1, -1 },
  { "Logs", 4, ICON_FOLDER }, { "WDGWars stats", 9, ICON_TROPHY },
  { "Advanced", -1, -1 },
  { "Pro mode", 10, ICON_SHIELD }, { "About & safety", 6, ICON_ALERT },
};
static const int SET_ROW_N = sizeof(SET_ROWS) / sizeof(SET_ROWS[0]);
static const int SET_HDR_H = 24, SET_ITEM_H = 40;

int setRowH(int i) { return SET_ROWS[i].action < 0 ? SET_HDR_H : SET_ITEM_H; }
void settingsViewport(int &top, int &bot) { top = 50; bot = tft.height() - 46; }
int settingsContentH() { int t = 0; for (int i = 0; i < SET_ROW_N; i++) t += setRowH(i); return t + 6; }
int settingsMaxScroll() { int top, bot; settingsViewport(top, bot); return max(0, settingsContentH() - (bot - top)); }

void settingsValue(int action, char* out, size_t n) {
  out[0] = 0;
  switch (action) {
    case 0: snprintf(out, n, "%d%%", (brightness * 100 + 127) / 255); break;
    case 1: {
      const char* nm = wdgwTheme ? "WDGW" : "Solid";
      if (!wdgwTheme) {
        for (int i = 0; i < 8; i++) if (colorOptions[i] == accentColor) nm = colorNames[i];
      }
      snprintf(out, n, "%s", nm);
      break;
    }
    case 2: { const char* o[4] = { "Portrait", "Landscape", "Portrait 180", "Landscape 270" }; snprintf(out, n, "%s", o[preferredRotation & 3]); break; }
    case 3: snprintf(out, n, "192.168.4.1"); break;
    case 4: snprintf(out, n, "%s", sdReady ? "SD card" : "Flash"); break;
    case 5: snprintf(out, n, "%s", WiFi.status() == WL_CONNECTED ? "Connected" : (homeSSID.length() ? homeSSID.c_str() : "Not set")); break;
    case 7: snprintf(out, n, "%d / %d", gpsRxPin, gpsTxPin); break;
    case 10: snprintf(out, n, "%s", proMode ? "On" : "Off"); break;
    default: break;
  }
}

void settingsDrawList() {
  int top, bot;
  settingsViewport(top, bot);
  int w = tft.width();
  int maxS = settingsMaxScroll();
  settingsScroll = constrain(settingsScroll, 0, maxS);
  paintRegion(0, top, w, bot - top, [&](LovyanGFX &g, int bx, int by) {
    int y = by - settingsScroll + 2;
    uint16_t acc = getAccent();
    for (int i = 0; i < SET_ROW_N; i++) {
      const SetRow &r = SET_ROWS[i];
      int rh = setRowH(i);
      if (y + rh < by || y > by + (bot - top)) { y += rh; continue; }
      if (r.action < 0) {
        g.setFont(F_XS);
        g.setTextDatum(BL_DATUM);
        g.setTextColor(UI_DIM, TFT_BLACK);
        g.drawString(r.label, bx + 16, y + rh - 5);
      } else {
        uint16_t f = uiCardFill();
        // rows in a section share one rounded surface
        bool first = (i == 0) || SET_ROWS[i - 1].action < 0;
        bool last = (i == SET_ROW_N - 1) || SET_ROWS[i + 1].action < 0;
        int x0 = bx + 10, ww = w - 26;
        g.fillRoundRect(x0, y, ww, rh, 10, f);
        if (!first) g.fillRect(x0, y, ww, 10, f);
        if (!last) g.fillRect(x0, y + rh - 10, ww, 10, f);
        if (!first) g.drawFastHLine(x0 + 36, y, ww - 46, UI_LINE);
        drawIconOn(g, r.icon, 14, x0 + 12, y + (rh - 14) / 2, acc, f);
        g.setFont(F_M);
        g.setTextDatum(ML_DATUM);
        g.setTextColor(r.action == 10 && proMode ? proCyan : UI_TEXT, f);
        g.drawString(r.label, x0 + 36, y + rh / 2 + 1);
        int labW = g.textWidth(r.label);
        char v[24];
        settingsValue(r.action, v, sizeof(v));
        g.setFont(F_XS);
        g.setTextDatum(MR_DATUM);
        g.setTextColor(UI_MUTE, f);
        String vs = v;
        int room = ww - 36 - labW - 34;
        if (g.textWidth(vs.c_str()) > room) vs = "";   // never show a clipped value
        if (vs.length()) g.drawString(vs.c_str(), x0 + ww - 22, y + rh / 2 + 1);
        int cx = x0 + ww - 14, cy = y + rh / 2;
        g.drawLine(cx, cy - 4, cx + 4, cy, UI_DIM);
        g.drawLine(cx + 4, cy, cx, cy + 4, UI_DIM);
      }
      y += rh;
    }
    // slim scroll indicator
    if (maxS > 0) {
      int vh = bot - top;
      int th = max(18, vh * vh / settingsContentH());
      int ty = by + (vh - th) * settingsScroll / maxS;
      g.fillRoundRect(bx + w - 6, by, 3, vh, 1, UI_CARD);
      g.fillRoundRect(bx + w - 6, ty, 3, th, 1, UI_MUTE);
    }
  });
}

int settingsRowAt(int ty) {
  int top, bot;
  settingsViewport(top, bot);
  if (ty < top || ty > bot) return -1;
  int y = top + 2 - settingsScroll;
  for (int i = 0; i < SET_ROW_N; i++) {
    int rh = setRowH(i);
    if (ty >= y && ty < y + rh) return SET_ROWS[i].action;
    y += rh;
  }
  return -1;
}

void drawSettingsScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("SETTINGS");
  settingsDrawList();
  drawBackButton("HOME");
  int w = tft.width();
  tft.setFont(F_XS);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(UI_DIM, TFT_BLACK);
  tft.drawString("Drag to scroll", w - 14, tft.height() - 22);
  uiResetFont();
}

// Drag-to-scroll + tap-on-release handling for the settings list
void settingsTouch(bool touching, int tx, int ty) {
  static bool down = false, drag = false;
  static int y0 = 0, x0 = 0, s0 = 0;
  static unsigned long lastDraw = 0;
  if (touching && !down) { down = true; drag = false; x0 = tx; y0 = ty; s0 = settingsScroll; }
  if (touching && down) {
    int top, bot;
    settingsViewport(top, bot);
    int dy = ty - y0;
    if (!drag && abs(dy) > 10 && y0 >= top && y0 <= bot) drag = true;
    if (drag) {
      int ns = constrain(s0 - dy, 0, settingsMaxScroll());
      if (ns != settingsScroll && millis() - lastDraw > 35) {
        settingsScroll = ns;
        settingsDrawList();
        lastDraw = millis();
      }
    }
    return;
  }
  if (!touching && down) {
    down = false;
    if (drag) return;
    int h = tft.height();
    lastTouchTime = millis();
    if (y0 >= h - 46 && x0 <= 110) {
      currentScreen = HOME_SCREEN;
      drawHomeScreen();
      beep(1, 40);
      touchIgnoreUntilUp = true;
      return;
    }
    int act = settingsRowAt(y0);
    if (act >= 0) {
      if (act == 4) filesReturnTo = SETTINGS_SCREEN;
      activateSettingsItem(act);
      touchIgnoreUntilUp = true;
    }
  }
}

void drawBrightnessScreen() {
  lanesStop();
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("BRIGHTNESS");
  int w = tft.width();
  int cardY, cardH, sliderY, presetY, presetH;
  brightGeom(cardY, cardH, sliderY, presetY, presetH);
  drawBrightnessValue();
  int sw = min(280, w - 40);
  drawSlider(20, sliderY, sw, 14, 12, 255, brightness, getAccent());
  // presets
  const char* lab[3] = { "NIGHT", "DAY", "MAX" };
  int gap = 6, pw = (w - 16 - 2 * gap) / 3;
  for (int i = 0; i < 3; i++) {
    bool on = abs(brightness - brightPresets[i]) < 8;
    drawThemedButton(8 + i * (pw + gap), presetY, pw, presetH, lab[i], on ? C_GREEN : getBtnFill(), TFT_WHITE);
  }
  tft.setFont(F_XS);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(UI_DIM, TFT_BLACK);
  tft.drawString("Drag or tap a preset", w - 8, tft.height() - 22);
  uiResetFont();
  drawBackButton("BACK");
}

// CYD standard rotations: 0=0°, 1=90°, 2=180°, 3=270°
void orientationBtnGeom(int &btnW, int &btnH, int &gap, int &startY, int &col0, int &col1) {
  int w = tft.width();
  int h = tft.height();
  bool landscape = (w > h);
  btnW = landscape ? 130 : 100;
  btnH = landscape ? 36 : 40;
  gap = landscape ? 10 : 12;
  startY = landscape ? 58 : 70;
  col0 = (w / 2) - btnW - gap / 2;
  col1 = (w / 2) + gap / 2;
}

// ---------- Orientation ----------
void drawOrientationScreen() {
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("ORIENTATION");
  int w = tft.width();
  int h = tft.height();

  int btnW, btnH, gap, startY, col0, col1;
  orientationBtnGeom(btnW, btnH, gap, startY, col0, col1);

  const char* labels[4] = { "0 deg", "90 deg", "180 deg", "270 deg" };
  // rot index: 0,1,2,3
  for (int i = 0; i < 4; i++) {
    int col = i % 2;
    int row = i / 2;
    int x = (col == 0) ? col0 : col1;
    int y = startY + row * (btnH + gap);

    bool sel = (preferredRotation == i);
    uint16_t fill = sel ? TFT_GREEN : getBtnFill();
    drawThemedButton(x, y, btnW, btnH, labels[i], fill, sel ? TFT_BLACK : TFT_WHITE);
  }

  tft.setFont(F_XS);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  int noteY = startY + 2 * (btnH + gap) + 6;
  if (noteY > h - 55) noteY = h - 55;
  tft.drawString("Change forces recalibration", w / 2, noteY);

  drawBackButton("BACK");
}

void applyOrientation(int rot) {
  if (rot < 0) rot = 0;
  if (rot > 3) rot = 3;
  preferredRotation = rot;
  tft.setRotation(preferredRotation);
  touch.setRotation(0);
  saveCalibration();
  beep(2, 70);
  runCalibration();  // recalibrate in THIS orientation
}







// ---------- Home WiFi config ----------
void loadHomeWifi() {
  if (!logFs().exists(WIFI_CFG_FILE)) return;
  File f = logFs().open(WIFI_CFG_FILE, FILE_READ);
  if (!f) return;
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, f)) { f.close(); return; }
  f.close();
  homeSSID = doc["ssid"] | "";
  homePass = doc["pass"] | "";
  wdgApiKey = doc["wdg_key"] | "";
  wigleApiName = doc["wigle_name"] | "";
  wigleApiToken = doc["wigle_token"] | "";
  Serial.printf("Home WiFi loaded: %s\n", homeSSID.c_str());
}

void saveHomeWifi() {
  if (logFs().exists(WIFI_CFG_FILE)) logFs().remove(WIFI_CFG_FILE);
  File f = logFs().open(WIFI_CFG_FILE, FILE_WRITE);
  if (!f) return;
  StaticJsonDocument<512> doc;
  doc["ssid"] = homeSSID;
  doc["pass"] = homePass;
  doc["wdg_key"] = wdgApiKey;
  doc["wigle_name"] = wigleApiName;
  doc["wigle_token"] = wigleApiToken;
  serializeJson(doc, f);
  f.close();
  Serial.println("Home WiFi / WDG key saved");
}

bool tryConnectHomeWifi(uint32_t timeoutMs) {
  if (homeSSID.length() == 0) {
    Serial.println("WiFi: no SSID saved");
    return false;
  }
  if (wardriveActive) {
    Serial.println("WiFi: blocked while wardriving");
    return false;
  }

  // SoftAP on same radio often blocks STA join — tear it down first
  if (apActive) {
    Serial.println("WiFi: stopping portal AP before STA connect");
    stopPortalAP();
    delay(200);
  }

  homeSSID.trim();
  homePass.trim();  // accidental spaces from keyboard kill WPA

  Serial.printf("WiFi: connecting to '%s' (pass len=%d)\n",
                homeSSID.c_str(), homePass.length());

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);   // STA only — more reliable than AP_STA for join
  delay(100);
  WiFi.setSleep(false);

  // Empty pass = open network
  if (homePass.length() == 0) {
    WiFi.begin(homeSSID.c_str());
  } else {
    WiFi.begin(homeSSID.c_str(), homePass.c_str());
  }

  uint32_t start = millis();
  wl_status_t last = WL_IDLE_STATUS;
  while (millis() - start < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st != last) {
      last = st;
      Serial.printf("WiFi status=%d\n", (int)st);
    }
    if (st == WL_CONNECTED) break;
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
      // brief pause then one retry
      delay(400);
      WiFi.disconnect(true);
      delay(100);
      if (homePass.length() == 0) WiFi.begin(homeSSID.c_str());
      else WiFi.begin(homeSSID.c_str(), homePass.c_str());
      // don't spin forever on hard fail — allow rest of timeout for retry
    }
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    homeWifiConnected = true;
    userWantsHomeWifi = true;
    homeWifiIP = WiFi.localIP().toString();
    Serial.printf("WiFi OK IP=%s RSSI=%d\n", homeWifiIP.c_str(), WiFi.RSSI());
    return true;
  }

  homeWifiConnected = false;
  homeWifiIP = "";
  Serial.printf("WiFi FAILED status=%d SSID='%s'\n", (int)WiFi.status(), homeSSID.c_str());
  WiFi.disconnect(true);
  return false;
}

void maintainHomeWifi() {
  // No auto-reconnect. Home WiFi is opt-in only (Settings > WIFI > CONNECT / JOIN).
  // Boot / portal use SoftAP. If user disconnected, stay offline.
  if (apActive) return;
  if (!userWantsHomeWifi) {
    if (WiFi.status() == WL_CONNECTED) {
      // Shouldn't happen after DISCONNECT, but keep flags honest
    } else {
      homeWifiConnected = false;
      homeWifiIP = "";
    }
    return;
  }
  // User wants WiFi: only refresh status, never silent WiFi.begin()
  if (WiFi.status() == WL_CONNECTED) {
    homeWifiConnected = true;
    homeWifiIP = WiFi.localIP().toString();
  } else {
    homeWifiConnected = false;
    homeWifiIP = "";
    // Lost connection — do not auto-retry; user must CONNECT again
    userWantsHomeWifi = false;
  }
}

// ---------- Soft AP + SD file portal ----------

// Minimal ZIP (store / no compression) for downloading all CSVs
void zipWrite16(uint8_t* buf, uint16_t v) {
  buf[0] = v & 0xFF; buf[1] = (v >> 8) & 0xFF;
}
void zipWrite32(uint8_t* buf, uint32_t v) {
  buf[0] = v & 0xFF; buf[1] = (v >> 8) & 0xFF;
  buf[2] = (v >> 16) & 0xFF; buf[3] = (v >> 24) & 0xFF;
}

uint32_t zipCrc32Update(uint32_t crc, uint8_t b) {
  crc ^= b;
  for (int i = 0; i < 8; i++)
    crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320UL : (crc >> 1);
  return crc;
}

uint32_t zipCrc32(File &f) {
  uint32_t crc = 0xFFFFFFFF;
  f.seek(0);
  while (f.available()) {
    crc = zipCrc32Update(crc, (uint8_t)f.read());
  }
  f.seek(0);
  return crc ^ 0xFFFFFFFF;
}

void handleDownloadAll() {
  if (!openLogFS()) {
    server.send(500, "text/plain", "SD error");
    return;
  }

  // Collect CSV filenames
  String names[40];
  int nFiles = 0;
  File root = logFs().open("/");
  if (root) {
    File f = root.openNextFile();
    while (f && nFiles < 40) {
      if (!f.isDirectory()) {
        String name = f.name();
        if (name.startsWith("/")) name = name.substring(1);
        String lower = name;
        lower.toLowerCase();
        if (lower.endsWith(".csv")) {
          names[nFiles++] = name;
        }
      }
      f = root.openNextFile();
    }
    root.close();
  }

  if (nFiles == 0) {
    server.send(404, "text/plain", "No CSV files found");
    return;
  }

  // Build ZIP in memory is too large — stream in chunks via a temp approach:
  // Write zip to /tmp_all.zip on SD then stream it.
  const char* zipPath = "/SKIrf_WD_ALL.zip";
  if (logFs().exists(zipPath)) logFs().remove(zipPath);
  File zf = logFs().open(zipPath, FILE_WRITE);
  if (!zf) {
    server.send(500, "text/plain", "Cannot create zip");
    return;
  }

  // Track central directory entries
  struct CDEntry {
    String name;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
  };
  CDEntry* cds = new CDEntry[nFiles];
  if (!cds) { zf.close(); server.send(500, "text/plain", "OOM"); return; }

  for (int i = 0; i < nFiles; i++) {
    String path = "/" + names[i];
    File src = logFs().open(path, FILE_READ);
    if (!src) continue;

    uint32_t sz = src.size();
    uint32_t crc = zipCrc32(src);
    uint32_t localOff = zf.size();

    // Local file header
    uint8_t lh[30];
    memset(lh, 0, 30);
    zipWrite32(lh + 0, 0x04034b50);
    zipWrite16(lh + 8, 0);      // method = store
    zipWrite16(lh + 14, 0);     // crc later? use data descriptor free - put crc now
    zipWrite32(lh + 14, crc);
    zipWrite32(lh + 18, sz);    // compressed
    zipWrite32(lh + 22, sz);    // uncompressed
    zipWrite16(lh + 26, names[i].length());
    zf.write(lh, 30);
    zf.print(names[i]);

    // File data
    uint8_t buf[256];
    while (src.available()) {
      int n = src.read(buf, sizeof(buf));
      if (n > 0) zf.write(buf, n);
    }
    src.close();

    cds[i].name = names[i];
    cds[i].crc = crc;
    cds[i].size = sz;
    cds[i].offset = localOff;
  }

  uint32_t cdOffset = zf.size();
  uint32_t cdSize = 0;

  for (int i = 0; i < nFiles; i++) {
    if (cds[i].name.length() == 0) continue;
    uint8_t ch[46];
    memset(ch, 0, 46);
    zipWrite32(ch + 0, 0x02014b50);
    zipWrite16(ch + 10, 0); // method store
    zipWrite32(ch + 16, cds[i].crc);
    zipWrite32(ch + 20, cds[i].size);
    zipWrite32(ch + 24, cds[i].size);
    zipWrite16(ch + 28, cds[i].name.length());
    zipWrite32(ch + 42, cds[i].offset);
    zf.write(ch, 46);
    zf.print(cds[i].name);
    cdSize += 46 + cds[i].name.length();
  }

  // End of central directory
  uint8_t eocd[22];
  memset(eocd, 0, 22);
  zipWrite32(eocd + 0, 0x06054b50);
  zipWrite16(eocd + 8, nFiles);
  zipWrite16(eocd + 10, nFiles);
  zipWrite32(eocd + 12, cdSize);
  zipWrite32(eocd + 16, cdOffset);
  zf.write(eocd, 22);
  zf.close();
  delete[] cds;

  // Stream zip to client
  File out = logFs().open(zipPath, FILE_READ);
  if (!out) {
    server.send(500, "text/plain", "Zip open failed");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"SKIrf_WD_ALL.zip\"");
  server.streamFile(out, "application/zip");
  out.close();
  // keep zip on SD for re-download, or remove:
  // logFs().remove(zipPath);
}


String urlEncode(const String& s) {
  String out;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += c;
    else if (c == ' ') out += '+';
    else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}



// ---------- WDGWars CSV upload ----------
// POST https://wdgwars.pl/api/upload-csv  Header X-API-Key  multipart field "file"
String lastWdgError = "";

int uploadOneCsvToWdg(const String& path) {
  lastWdgError = "";
  if (wdgApiKey.length() < 8) { lastWdgError = "API key missing/too short"; return -1; }
  if (WiFi.status() != WL_CONNECTED) { lastWdgError = "WiFi not connected"; return -2; }

  File f = logFs().open(path, FILE_READ);
  if (!f) { lastWdgError = "Cannot open " + path; return -3; }
  size_t fsize = f.size();

  String boundary = "SKIrfWDG7a3f";
  String fname = path;
  if (fname.startsWith("/")) fname = fname.substring(1);

  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fname + "\"\r\n";
  head += "Content-Type: text/csv\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  size_t totalLen = head.length() + fsize + tail.length();

  SKIRF_SECURE_CLIENT client;
  client.setInsecure();
  client.setTimeout(25000);

  Serial.printf("WDG connecting... keylen=%d file=%s size=%u\n", wdgApiKey.length(), fname.c_str(), (unsigned)fsize);

  if (!client.connect("wdgwars.pl", 443)) {
    f.close();
    lastWdgError = "TLS connect to wdgwars.pl:443 failed";
    Serial.println(lastWdgError);
    return -5;
  }

  client.print("POST /api/upload-csv HTTP/1.1\r\n");
  client.print("Host: wdgwars.pl\r\n");
  client.print("User-Agent: SKIrf-Wardriver/1.0\r\n");
  client.print("X-API-Key: " + wdgApiKey + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(totalLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(head);

  uint8_t buf[512];
  while (f.available()) {
    int n = f.read(buf, sizeof(buf));
    if (n > 0) {
      size_t w = client.write(buf, n);
      if (w != (size_t)n) {
        // keep going; some stacks report partial
      }
    }
    yield();
  }
  f.close();
  client.print(tail);
  client.flush();

  unsigned long t0 = millis();
  while (client.connected() && !client.available() && millis() - t0 < 45000) {
    delay(15);
    yield();
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  String bodyPeek = "";
  t0 = millis();
  while (millis() - t0 < 2000) {
    while (client.available()) {
      char c = client.read();
      if (bodyPeek.length() < 200) bodyPeek += c;
    }
    if (!client.connected() && !client.available()) break;
    delay(10);
  }
  client.stop();

  Serial.println("WDG status: " + statusLine);
  Serial.println("WDG body: " + bodyPeek);

  if (statusLine.indexOf("200") >= 0 || statusLine.indexOf("201") >= 0 || statusLine.indexOf("202") >= 0) {
    lastWdgError = "OK " + statusLine;
    return 1;
  }
  // 409 = often "already imported" / duplicate content — treat as soft success
  if (statusLine.indexOf("409") >= 0) {
    lastWdgError = "Already on WDGWars (409 Conflict) — not an auth error";
    return 2;  // soft OK
  }
  lastWdgError = statusLine.length() ? statusLine : "No HTTP response";
  if (bodyPeek.length()) {
    bodyPeek.replace("\n", " ");
    lastWdgError += " | " + bodyPeek.substring(0, 120);
  }
  return 0;
}

String uploadAllCsvToWdg() {
  if (wdgApiKey.length() < 8)
    return "FAIL: No API key saved. Enter and SAVE KEY first.";
  if (WiFi.status() != WL_CONNECTED)
    return "FAIL: Not connected to home WiFi (need internet). Status=" + String((int)WiFi.status());

  String names[40];
  int n = 0;
  File root = logFs().open("/");
  if (!root) return "FAIL: SD card error";
  File f = root.openNextFile();
  while (f && n < 40) {
    if (!f.isDirectory()) {
      String name = f.name();
      if (name.startsWith("/")) name = name.substring(1);
      String lower = name; lower.toLowerCase();
      if (lower.endsWith(".csv")) names[n++] = name;
    }
    f = root.openNextFile();
  }
  root.close();

  if (n == 0) return "FAIL: No CSV files on SD card.";

  int ok = 0, skip = 0, fail = 0;
  String details = "";
  for (int i = 0; i < n; i++) {
    Serial.printf("Uploading %d/%d: %s\n", i + 1, n, names[i].c_str());
    int r = uploadOneCsvToWdg("/" + names[i]);
    if (r == 1) {
      ok++;
      details += "<br>OK: " + names[i];
    } else if (r == 2) {
      skip++;
      details += "<br>SKIP (already uploaded): " + names[i];
    } else {
      fail++;
      details += "<br>FAIL: " + names[i] + " — " + lastWdgError;
    }
  }

  String summary = "Done: " + String(ok) + " new, " + String(skip) + " already on server, " +
                   String(fail) + " failed (of " + String(n) + ").";
  summary += details;
  return summary;
}

void handleWdgPage() {
  String html;
  html += F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>WDGWars</title><style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px}");
  html += F("input,button{width:95%;padding:12px;margin:8px 0;font-size:15px;border-radius:8px;border:1px solid #0f0;background:#111;color:#fff}");
  html += F("button{background:#0220;color:#0f0} a{color:#0ff}");
  html += F(".ok{color:#0f0}.warn{color:#fd0}.info{color:#0ff}");
  html += F("#spin{display:none;color:#fd0;margin:12px 0}</style></head><body>");
  html += F("<h2>WDGWars Upload</h2>");

  if (wdgApiKey.length() >= 8) {
    html += F("<p class='ok'>API key saved (");
    html += String(wdgApiKey.length());
    html += F(" chars)</p>");
  } else {
    html += F("<p class='warn'>No API key saved yet</p>");
  }

  if (WiFi.status() == WL_CONNECTED) {
    html += F("<p class='info'>WiFi: connected — ");
    html += WiFi.localIP().toString();
    html += F("</p>");
  } else {
    html += F("<p class='warn'>WiFi: NOT connected — uploads need home WiFi + internet</p>");
  }

  html += F("<p style='color:#888'>Paste API key from your wdgwars.pl profile, save, then upload.</p>");
  html += F("<form method='POST' action='/wdg-save'>");
  html += F("<label>API Key</label><br>");
  html += secretInput("key", wdgApiKey.length() > 0, false);
  html += F("<br><button type='submit'>SAVE KEY</button></form>");

  html += F("<form method='POST' action='/wdg-upload' onsubmit=\"document.getElementById('spin').style.display='block';this.querySelector('button').disabled=true;this.querySelector('button').innerText='UPLOADING...';return true;\">");
  html += F("<button type='submit'>UPLOAD ALL CSV TO WDGWARS</button></form>");
  html += F("<div id='spin' style='display:none;margin:16px 0;padding:12px;border:1px solid #0f0;border-radius:8px'>");
  html += F("<p style='color:#fd0'>Uploading to WDGWars…</p>");
  html += F("<p style='color:#888'>Progress depends on file size and network. Keep this page open.</p>");
  html += F("<div style='height:14px;background:#222;border-radius:7px;overflow:hidden'><div style='height:100%;width:40%;background:#0f0;animation:pulse 1.2s infinite alternate'></div></div>");
  html += F("<style>@keyframes pulse{from{width:15%}to{width:90%}}</style></div>");
  html += F("<p><a href='/'>Back to files</a></p></body></html>");
  server.send(200, "text/html", html);
}

// Web portal: escape text placed inside an HTML attribute or element.
String htmlEsc(const String &s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += F("&amp;"); else if (c == '<') o += F("&lt;"); else if (c == '>') o += F("&gt;");
    else if (c == '\'') o += F("&#39;"); else if (c == '"') o += F("&quot;"); else o += c;
  }
  return o;
}
// Saved secrets are never sent back to the browser: the field shows a
// placeholder, and leaving it blank keeps the stored value.
String secretInput(const char* name, bool saved, bool password) {
  String h = F("<input name='");
  h += name;
  h += password ? F("' type='password'") : F("'");
  h += F(" autocomplete='off' placeholder='");
  h += saved ? F("saved - leave blank to keep") : F("not set");
  h += F("'>");
  return h;
}

void handleWdgSave() {
  String msg;
  if (server.hasArg("key") && server.arg("key").length()) {
    wdgApiKey = server.arg("key");
    wdgApiKey.trim();
    saveHomeWifi();
    msg = "API key saved (" + String(wdgApiKey.length()) + " characters).";
    Serial.println(msg);
  } else {
    msg = "No key received.";
  }

  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Key Saved</title><style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;margin-top:40px} a{color:#0ff}</style></head><body>");
  html += F("<h2>Saved</h2><p>");
  html += msg;
  html += F("</p><p><a href='/wdg'>Back to WDGWars</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleWdgUpload() {
  String result = uploadAllCsvToWdg();
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Upload Result</title><style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px;text-align:left;line-height:1.5}");
  html += F("a{color:#0ff} h2{text-align:center;color:#fff}");
  html += F(".box{border:1px solid #0f0;border-radius:10px;padding:14px;margin:12px 0;background:#0a0a0a}</style></head><body>");
  html += F("<h2>WDGWars Result</h2>");
  html += F("<div class='box'><p>");
  html += result;
  html += F("</p></div>");
  html += F("<p style='color:#888;text-align:center'>Results stay on this page — review OK / Skip / Fail, then go back.</p>");
  html += F("<p style='text-align:center'><a href='/wdg'>Back to WDGWars</a> · <a href='/'>Files</a></p></body></html>");
  server.send(200, "text/html", html);
}


// ---------- Wigle CSV upload ----------
String lastWigleError = "";

String wigleBasicAuthHeader() {
  String userpass = wigleApiName + ":" + wigleApiToken;
  size_t olen = 0;
  size_t inlen = userpass.length();
  size_t outlen = ((inlen + 2) / 3) * 4 + 8;
  unsigned char* out = (unsigned char*)malloc(outlen);
  if (!out) return "";
  memset(out, 0, outlen);
  int ret = mbedtls_base64_encode(out, outlen, &olen, (const unsigned char*)userpass.c_str(), inlen);
  String b64 = "";
  if (ret == 0 && olen > 0) b64 = String((const char*)out);
  free(out);
  if (b64.length() == 0) return "";
  return "Basic " + b64;
}

int uploadOneCsvToWigle(const String& path) {
  lastWigleError = "";
  if (wigleApiName.length() < 1 || wigleApiToken.length() < 4) {
    lastWigleError = "Wigle API name/token missing";
    return -1;
  }
  if (WiFi.status() != WL_CONNECTED) {
    lastWigleError = "WiFi not connected";
    return -2;
  }
  File f = logFs().open(path, FILE_READ);
  if (!f) { lastWigleError = "Cannot open file"; return -3; }
  size_t fsize = f.size();
  String boundary = "SKIrfWigle9b2";
  String fname = path;
  if (fname.startsWith("/")) fname = fname.substring(1);
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fname + "\"\r\n";
  head += "Content-Type: text/csv\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  size_t totalLen = head.length() + fsize + tail.length();
  String auth = wigleBasicAuthHeader();
  if (auth.length() < 10) { f.close(); lastWigleError = "Base64 auth failed"; return -4; }

  SKIRF_SECURE_CLIENT client;
  client.setInsecure();
  client.setTimeout(25000);
  Serial.printf("Wigle connect %s size=%u\n", fname.c_str(), (unsigned)fsize);
  if (!client.connect("api.wigle.net", 443)) {
    f.close();
    lastWigleError = "TLS connect api.wigle.net failed";
    return -5;
  }
  client.print("POST /api/v2/file/upload HTTP/1.1\r\n");
  client.print("Host: api.wigle.net\r\n");
  client.print("User-Agent: SKIrf-Wardriver/1.0\r\n");
  client.print("Authorization: " + auth + "\r\n");
  client.print("Accept: application/json\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(totalLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(head);
  uint8_t buf[512];
  while (f.available()) {
    int n = f.read(buf, sizeof(buf));
    if (n > 0) client.write(buf, n);
    yield();
  }
  f.close();
  client.print(tail);
  client.flush();

  unsigned long t0 = millis();
  while (client.connected() && !client.available() && millis() - t0 < 60000) {
    delay(15); yield();
  }
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  String bodyPeek = "";
  t0 = millis();
  while (millis() - t0 < 3000) {
    while (client.available()) {
      char c = client.read();
      if (bodyPeek.length() < 240) bodyPeek += c;
    }
    if (!client.connected() && !client.available()) break;
    delay(10);
  }
  client.stop();
  Serial.println("Wigle status: " + statusLine);
  Serial.println("Wigle body: " + bodyPeek);
  if (statusLine.indexOf("200") >= 0) { lastWigleError = "OK"; return 1; }
  lastWigleError = statusLine.length() ? statusLine : "No HTTP response";
  if (bodyPeek.length()) { bodyPeek.replace("\n", " "); lastWigleError += " | " + bodyPeek.substring(0, 100); }
  return 0;
}

void doWigleUploadFromDevice() {
  if (wigleApiName.length() < 1 || wigleApiToken.length() < 4) {
    showWdgStatusScreen("No Wigle credentials", "Set in web portal", "OPEN PORTAL > WIGLE");
    delay(2500); drawSettingsScreen(); return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    showWdgStatusScreen("Connecting WiFi...", homeSSID.c_str(), "Please wait");
    if (homeSSID.length() == 0 || !tryConnectHomeWifi(15000)) {
      showWdgStatusScreen("No internet WiFi", "Set home WiFi first", "");
      delay(2500); drawSettingsScreen(); return;
    }
  }
  showWdgStatusScreen("Wigle uploading...", WiFi.localIP().toString().c_str(), "Do not power off");
  String names[40]; int n = 0;
  File root = logFs().open("/");
  if (root) {
    File f = root.openNextFile();
    while (f && n < 40) {
      if (!f.isDirectory()) {
        String name = f.name();
        if (name.startsWith("/")) name = name.substring(1);
        String lower = name; lower.toLowerCase();
        if (lower.endsWith(".csv")) names[n++] = name;
      }
      f = root.openNextFile();
    }
    root.close();
  }
  if (n == 0) {
    showWdgStatusScreen("No CSV files", "Run a wardrive first", "");
    delay(2000); drawSettingsScreen(); return;
  }
  int ok = 0, fail = 0;
  for (int i = 0; i < n; i++) {
    char line2[40];
    snprintf(line2, sizeof(line2), "File %d of %d", i + 1, n);
    showWdgStatusScreen("Wigle upload", line2, names[i].c_str(), i, n);
    if (uploadOneCsvToWigle("/" + names[i]) == 1) ok++; else fail++;
    showWdgStatusScreen("Wigle upload", line2, names[i].c_str(), i + 1, n);
  }
  char result[64];
  snprintf(result, sizeof(result), "OK:%d  Fail:%d", ok, fail);
  showWdgStatusScreen("Wigle finished", result, "Read results — returning soon", n, n);
  beep(ok > 0 ? 2 : 1, 60);
  delay(7000);
  drawSettingsScreen();
}

// ---------- Web portal pages (WiGLE, home Wi-Fi, file list, downloads) ----------
void handleWiglePage() {
  String html;
  html += F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Wigle</title><style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px}");
  html += F("input,button{width:95%;padding:12px;margin:8px 0;font-size:15px;border-radius:8px;border:1px solid #0f0;background:#111;color:#fff}");
  html += F("button{background:#0220;color:#0f0} a{color:#0ff} .ok{color:#0f0} .warn{color:#fd0}</style></head><body>");
  html += F("<h2>Wigle Upload</h2>");
  html += F("<p style='color:#888'>Get API Name + Token from wigle.net/account (not your login password).</p>");
  if (wigleApiName.length() > 0 && wigleApiToken.length() > 0) html += F("<p class='ok'>Credentials saved</p>");
  else html += F("<p class='warn'>No Wigle credentials saved</p>");
  html += F("<form method='POST' action='/wigle-save'>");
  html += F("<label>API Name</label><br><input name='name' autocomplete='off' value='");
  html += htmlEsc(wigleApiName);
  html += F("'><br><label>API Token</label><br>");
  html += secretInput("token", wigleApiToken.length() > 0, true);
  html += F("<br><button type='submit'>SAVE CREDENTIALS</button></form>");
  html += F("<form method='POST' action='/wigle-upload' onsubmit=\"document.getElementById('spin').style.display='block';this.querySelector('button').disabled=true;this.querySelector('button').innerText='UPLOADING...';return true;\">");
  html += F("<button type='submit'>UPLOAD ALL CSV TO WIGLE</button></form>");
  html += F("<div id='spin' style='display:none;margin:16px 0;padding:12px;border:1px solid #0af;border-radius:8px'>");
  html += F("<p style='color:#fd0'>Uploading to Wigle… keep this page open.</p>");
  html += F("<div style='height:14px;background:#222;border-radius:7px;overflow:hidden'><div style='height:100%;width:40%;background:#0af;animation:pulse 1.2s infinite alternate'></div></div>");
  html += F("<style>@keyframes pulse{from{width:15%}to{width:90%}}</style></div>");
  html += F("<p><a href='/'>Back to files</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleWigleSave() {
  if (server.hasArg("name")) wigleApiName = server.arg("name");
  if (server.hasArg("token") && server.arg("token").length()) wigleApiToken = server.arg("token");
  wigleApiName.trim(); wigleApiToken.trim();
  saveHomeWifi();
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Saved</title><style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;margin-top:40px}a{color:#0ff}</style></head><body>");
  html += F("<h2>Saved</h2><p>Wigle API name + token stored on SD.</p>");
  html += F("<p><a href='/wigle'>Back</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleWigleUpload() {
  String result;
  if (wigleApiName.length() < 1 || wigleApiToken.length() < 4)
    result = "FAIL: Save Wigle API name and token first.";
  else if (WiFi.status() != WL_CONNECTED)
    result = "FAIL: Need home WiFi with internet.";
  else {
    String names[40]; int n = 0;
    File root = logFs().open("/");
    if (root) {
      File f = root.openNextFile();
      while (f && n < 40) {
        if (!f.isDirectory()) {
          String name = f.name();
          if (name.startsWith("/")) name = name.substring(1);
          String lower = name; lower.toLowerCase();
          if (lower.endsWith(".csv")) names[n++] = name;
        }
        f = root.openNextFile();
      }
      root.close();
    }
    if (n == 0) result = "FAIL: No CSV files.";
    else {
      int ok = 0, fail = 0; String details;
      for (int i = 0; i < n; i++) {
        if (uploadOneCsvToWigle("/" + names[i]) == 1) { ok++; details += "<br>OK: " + names[i]; }
        else { fail++; details += "<br>FAIL: " + names[i] + " — " + lastWigleError; }
      }
      result = "Done: " + String(ok) + " ok, " + String(fail) + " failed." + details;
    }
  }
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Wigle Result</title><style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px;line-height:1.5}");
  html += F("a{color:#0ff} h2{text-align:center;color:#fff}");
  html += F(".box{border:1px solid #0af;border-radius:10px;padding:14px;margin:12px 0;background:#0a0a0a}</style></head><body>");
  html += F("<h2>Wigle Result</h2><div class='box'><p>");
  html += result;
  html += F("</p></div>");
  html += F("<p style='color:#888;text-align:center'>Results stay here so you can check failures before leaving.</p>");
  html += F("<p style='text-align:center'><a href='/wigle'>Back to Wigle</a> · <a href='/'>Files</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleWifiConfig() {
  String html;
  html += F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Home WiFi</title><style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px}");
  html += F("input,button{width:95%;padding:12px;margin:8px 0;font-size:16px;border-radius:8px;border:1px solid #0f0;background:#111;color:#fff}");
  html += F("button{background:#0220;color:#0f0}");
  html += F("a{color:#0ff}</style></head><body>");
  html += F("<h2>Home WiFi Setup</h2>");
  html += F("<p style='color:#888'>When this network is in range, the wardriver will auto-connect so you can reach it on your LAN.</p>");
  html += F("<form method='POST' action='/wifi-save'>");
  html += F("<label>SSID</label><br><input name='ssid' value='");
  html += htmlEsc(homeSSID);
  html += F("'><br><label>Password</label><br>");
  html += secretInput("pass", homePass.length() > 0, true);
  html += F("<br><button type='submit'>SAVE & CONNECT</button></form>");
  html += F("<p><a href='/'>Back to files</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleWifiSave() {
  String oldSSID = homeSSID;
  if (server.hasArg("ssid")) homeSSID = server.arg("ssid");
  homeSSID.trim();
  // Blank password keeps the saved one for the same network; a new network
  // with a blank password is treated as an open network.
  if (server.hasArg("pass") && server.arg("pass").length()) homePass = server.arg("pass");
  else if (homeSSID != oldSSID) homePass = "";
  saveHomeWifi();

  bool ok = tryConnectHomeWifi(15000);

  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Saved</title><style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;margin-top:40px}</style></head><body>");
  if (ok) {
    html += F("<h2>Connected!</h2><p>IP: ");
    html += homeWifiIP;
    html += F("</p><p>Open http://");
    html += homeWifiIP;
    html += F("/ on your home network.</p>");
  } else {
    html += F("<h2>Saved</h2><p>Could not connect right now (out of range or wrong password). Credentials stored — will retry when network is seen.</p>");
  }
  html += F("<p><a href='/' style='color:#0ff'>Back</a></p></body></html>");
  server.send(200, "text/html", html);
}


bool isCsvName(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".csv");
}

bool safeCsvPath(String& name) {
  name.replace("..", "");
  name.replace("\\", "/");
  while (name.indexOf("//") >= 0) name.replace("//", "/");
  if (name.startsWith("/")) name = name.substring(1);
  if (name.length() == 0 || name.indexOf('/') >= 0) return false;  // root only, no subdirs
  if (!isCsvName(name)) return false;
  return true;
}

void handleRoot() {
  if (!openLogFS()) {
    server.send(500, "text/plain", "SD card error");
    return;
  }

  String html;
  html.reserve(4096);
  html += F("<!DOCTYPE html><html><head>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>SKIrf WARDRIVER</title>");
  html += F("<style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px}");
  html += F("h1{color:#fff;text-align:center;letter-spacing:1px}");
  html += F("table{width:100%;border-collapse:collapse;margin-top:16px}");
  html += F("th,td{border-bottom:1px solid #033;padding:10px 6px;text-align:left}");
  html += F("th{color:#0ff}");
  html += F("a.dl{color:#0ff;text-decoration:none;margin-right:12px}");
  html += F("a.del{color:#f55;text-decoration:none}");
  html += F(".meta{color:#666;font-size:12px;text-align:center;margin-top:20px}");
  html += F(".empty{color:#888;text-align:center;margin-top:30px}");
  html += F("</style></head><body>");
  html += F("<h1>SKIrf WARDRIVER</h1>");
  html += F("<p style='text-align:center;color:#888'>Wardrive CSV logs — select files to upload</p>");
  html += F("<form id='sel' method='POST'>");
  html += F("<table><tr><th><input type='checkbox' id='all' onclick=\"document.querySelectorAll('input.f').forEach(c=>c.checked=this.checked)\"></th>");
  html += F("<th>File</th><th>Size</th><th>Actions</th></tr>");

  File root = logFs().open("/");
  int count = 0;
  if (root) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        String name = f.name();
        if (name.startsWith("/")) name = name.substring(1);
        if (!isCsvName(name)) {
          f = root.openNextFile();
          continue;
        }
        size_t sz = f.size();
        String enc = urlEncode(name);

        html += "<tr><td><input class='f' type='checkbox' name='f' value='";
        html += name;
        html += "'></td><td>";
        html += name;
        html += "</td><td>";
        if (sz < 1024) { html += String(sz); html += " B"; }
        else { html += String(sz / 1024.0, 1); html += " KB"; }
        html += "</td><td>";
        html += "<a class='dl' href='/download?file=";
        html += enc;
        html += "'>Download</a>";
        html += "<a class='del' href='/delete?file=";
        html += enc;
        html += "' onclick=\"return confirm('Delete ";
        html += name;
        html += "?');\">Delete</a>";
        html += "</td></tr>";
        count++;
      }
      f = root.openNextFile();
    }
    root.close();
  }

  html += F("</table>");
  if (count == 0) {
    html += F("<p class='empty'>No .csv wardrive logs found</p>");
  }

  // Selected-file upload actions
  html += F("<div style='text-align:center;margin-top:18px'>");
  html += F("<p style='color:#888;margin:8px 0'>Selected files:</p>");
  html += F("<button type='submit' formaction='/upload-selected' name='target' value='wigle' onclick=\"this.innerText='UPLOADING WIGLE...';this.disabled=0;\"");
  html += F(" style='padding:12px 16px;margin:5px;border:2px solid #0af;border-radius:10px;background:#0220;color:#fff;font-size:14px'>");
  html += F("UPLOAD SELECTED → WIGLE</button>");
  html += F("<button type='submit' formaction='/upload-selected' name='target' value='wdg' onclick=\"this.innerText='UPLOADING WDG...';\"");
  html += F(" style='padding:12px 16px;margin:5px;border:2px solid #f0f;border-radius:10px;background:#0220;color:#fff;font-size:14px'>");
  html += F("UPLOAD SELECTED → WDGWARS</button>");
  html += F("<button type='submit' formaction='/upload-selected' name='target' value='all' onclick=\"this.innerText='UPLOADING ALL...';\"");
  html += F(" style='padding:12px 16px;margin:5px;border:2px solid #0f0;border-radius:10px;background:#0220;color:#fff;font-size:14px'>");
  html += F("UPLOAD SELECTED → ALL LOGGED-IN</button>");
  html += F("<br>");
  html += F("<button type='submit' formaction='/delete-selected' onclick=\"return confirm('Delete selected CSV files?');\"");
  html += F(" style='padding:12px 16px;margin:5px;border:2px solid #f55;border-radius:10px;background:#200;color:#f88;font-size:14px'>");
  html += F("DELETE SELECTED CSV</button>");
  html += F("</div></form>");

  html += F("<div style='text-align:center;margin-top:16px'>");
  html += F("<a href='/downloadall' style='display:inline-block;padding:12px 22px;margin:6px;border:2px solid #0ff;");
  html += F("border-radius:10px;color:#fff;text-decoration:none;background:#0220;font-size:15px'>");
  html += F("DOWNLOAD ALL .CSV (.ZIP)</a></div>");
  html += F("<div style='text-align:center;margin-top:10px'>");
  html += F("<a href='/delete-all-csv' onclick=\"return confirm('Delete ALL CSV wardrive logs on the SD card? This cannot be undone.');\"");
  html += F(" style='display:inline-block;padding:12px 22px;margin:6px;border:2px solid #f55;");
  html += F("border-radius:10px;color:#f88;text-decoration:none;background:#200;font-size:15px'>");
  html += F("DELETE ALL CSV</a></div>");
  html += F("<div style='text-align:center;margin-top:10px'>");
  html += F("<a href='/wifi' style='display:inline-block;padding:12px 22px;margin:6px;border:2px solid #fd0;");
  html += F("border-radius:10px;color:#fff;text-decoration:none;background:#0220;font-size:15px'>");
  html += F("HOME WIFI SETUP</a></div>");
  html += F("<div style='text-align:center;margin-top:10px'>");
  html += F("<a href='/wigle' style='display:inline-block;padding:12px 16px;margin:5px;border:2px solid #0af;");
  html += F("border-radius:10px;color:#fff;text-decoration:none;background:#0220;font-size:14px'>WIGLE API KEY</a>");
  html += F("<a href='/wdg' style='display:inline-block;padding:12px 16px;margin:5px;border:2px solid #f0f;");
  html += F("border-radius:10px;color:#fff;text-decoration:none;background:#0220;font-size:14px'>WDGWARS API KEY</a></div>");
  if (homeWifiConnected) {
    html += F("<p style='text-align:center;color:#0f0'>LAN: http://");
    html += homeWifiIP;
    html += F("/</p>");
  }
  html += F("<div style='text-align:center;margin-top:12px'>");
  html += F("<a href='/return' style='display:inline-block;padding:14px 28px;border:2px solid #0f0;");
  html += F("border-radius:10px;color:#fff;text-decoration:none;background:#0220;font-size:16px'>");
  html += F("RETURN TO WARDRIVER</a></div>");
  html += F("<p class='meta'>AP stays open while you are connected &middot; 192.168.4.1</p>");
  html += F("<p class='meta'>Uploads need Home WiFi with internet (set above)</p>");
  html += F("</body></html>");

  server.send(200, "text/html", html);
}


void handleUploadSelected() {
  String target = server.hasArg("target") ? server.arg("target") : "";
  target.toLowerCase();

  // Collect selected filenames (checkboxes name="f")
  String files[40];
  int n = 0;
  for (int i = 0; i < server.args() && n < 40; i++) {
    if (server.argName(i) == "f") {
      String nm = server.arg(i);
      if (safeCsvPath(nm)) files[n++] = nm;
    }
  }

  String result;
  if (n == 0) {
    result = "No files selected. Go back and check one or more CSVs.";
  } else if (target != "wigle" && target != "wdg" && target != "all") {
    result = "Unknown target.";
  } else {
    // Need internet — try home WiFi (AP_STA keeps portal up)
    if (WiFi.status() != WL_CONNECTED) {
      if (homeSSID.length() == 0 || !tryConnectHomeWifi(15000)) {
        result = "FAIL: Need home WiFi with internet for uploads. Configure HOME WIFI SETUP first.";
      }
    }

    if (result.length() == 0 && WiFi.status() == WL_CONNECTED) {
      bool doWigle = (target == "wigle" || target == "all");
      bool doWdg   = (target == "wdg"   || target == "all");

      if (doWigle && (wigleApiName.length() < 1 || wigleApiToken.length() < 4)) {
        result += "Wigle: no API credentials saved.<br>";
        doWigle = false;
      }
      if (doWdg && wdgApiKey.length() < 8) {
        result += "WDGWars: no API key saved.<br>";
        doWdg = false;
      }
      if (!doWigle && !doWdg && target == "all") {
        result += "Not logged in to Wigle or WDGWars. Save API keys first.";
      } else if (!doWigle && target == "wigle") {
        result += "Save Wigle API name + token first.";
      } else if (!doWdg && target == "wdg") {
        result += "Save WDGWars API key first.";
      } else {
        for (int i = 0; i < n; i++) {
          String path = "/" + files[i];
          result += "<b>" + files[i] + "</b><br>";
          if (doWigle) {
            int r = uploadOneCsvToWigle(path);
            result += r == 1 ? "&nbsp;&nbsp;Wigle: OK<br>" : ("&nbsp;&nbsp;Wigle: FAIL — " + lastWigleError + "<br>");
          }
          if (doWdg) {
            int r = uploadOneCsvToWdg(path);
            if (r == 1) result += "&nbsp;&nbsp;WDGWars: OK<br>";
            else if (r == 2) result += "&nbsp;&nbsp;WDGWars: SKIP (already on server)<br>";
            else result += "&nbsp;&nbsp;WDGWars: FAIL — " + lastWdgError + "<br>";
          }
          result += "<br>";
        }
      }
    }
  }

  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Upload Result</title><style>");
  html += F("body{background:#000;color:#0f0;font-family:monospace;margin:16px;line-height:1.5}");
  html += F("a{color:#0ff} h2{color:#fff;text-align:center}");
  html += F(".box{border:1px solid #0f0;border-radius:10px;padding:14px;background:#0a0a0a}</style></head><body>");
  html += F("<h2>Upload Result</h2><div class='box'><p>");
  html += result;
  html += F("</p></div>");
  html += F("<p style='text-align:center;color:#888'>Page stays open so you can review results.</p>");
  html += F("<p style='text-align:center'><a href='/'>Back to files</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleDownload() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }
  String name = server.arg("file");
  if (!safeCsvPath(name)) {
    server.send(403, "text/plain", "Only .csv wardrive logs can be downloaded");
    return;
  }
  name = "/" + name;

  if (!logFs().exists(name)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File f = logFs().open(name, FILE_READ);
  if (!f) {
    server.send(500, "text/plain", "Open failed");
    return;
  }

  String fname = name;
  if (fname.startsWith("/")) fname = fname.substring(1);

  server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
  server.streamFile(f, "text/csv");
  f.close();
}


void handleDeleteSelected() {
  int deleted = 0;
  for (int i = 0; i < server.args(); i++) {
    if (server.argName(i) != "f") continue;
    String nm = server.arg(i);
    if (!safeCsvPath(nm)) continue;
    String path = "/" + nm;
    if (logFs().exists(path)) {
      logFs().remove(path);
      deleted++;
      Serial.println("Deleted CSV: " + path);
    }
  }
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Deleted</title><style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;margin-top:40px}a{color:#0ff}</style></head><body>");
  html += F("<h2>Deleted selected</h2><p>");
  html += String(deleted);
  html += F(" CSV file(s) removed.</p>");
  html += F("<p><a href='/'>Back to files</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleDeleteAllCsv() {
  int deleted = 0;
  File root = logFs().open("/");
  if (root) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        String name = f.name();
        if (name.startsWith("/")) name = name.substring(1);
        if (isCsvName(name)) {
          String path = "/" + name;
          f.close();
          if (logFs().remove(path)) {
            deleted++;
            Serial.println("Deleted CSV: " + path);
          }
          f = root.openNextFile();
          continue;
        }
      }
      f = root.openNextFile();
    }
    root.close();
  }
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Deleted</title><style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;margin-top:40px}a{color:#0ff}</style></head><body>");
  html += F("<h2>Delete all CSV</h2><p>");
  html += String(deleted);
  html += F(" CSV file(s) removed.</p>");
  html += F("<p style='color:#888'>Config files (WiFi keys, calibration) were not touched.</p>");
  html += F("<p><a href='/'>Back to files</a></p></body></html>");
  server.send(200, "text/html", html);
}

void handleDelete() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }
  String name = server.arg("file");
  if (!safeCsvPath(name)) {
    server.send(403, "text/plain", "Only .csv wardrive logs can be deleted");
    return;
  }
  name = "/" + name;

  if (logFs().exists(name)) {
    logFs().remove(name);
    Serial.println("Deleted CSV: " + name);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleReturn() {
  returnToDevice = true;
  String html = F(
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Returning</title>"
    "<style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;margin-top:60px}</style>"
    "</head><body><h2>Returning to device...</h2>"
    "<p>AP will close. Use the CYD screen.</p></body></html>"
  );
  server.send(200, "text/html", html);
}

// ---------- Web portal hotspot + 60 s countdown screen ----------
void startPortalAP() {
  // Pure AP during portal so 192.168.4.1 always works for clients.
  // (Connecting home STA at the same time was breaking the portal page.)
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);

  // Force classic SoftAP subnet
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  bool ok = WiFi.softAP("SKIrf_WARDRIVER", nullptr, 1, 0, 4);
  if (!ok) {
    Serial.println("SoftAP failed");
    apActive = false;
    return;
  }
  // The GPS antenna sits a few centimetres from the radio and is trying to get its first
  // fix during this minute: a full-power hotspot right next to it slows that down a lot.
  // A phone at arm's length does not need more than this.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  delay(200);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP started: SKIrf_WARDRIVER  IP=");
  Serial.println(ip);

  // Clear old routes then register
  server.stop();
  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.on("/upload-selected", HTTP_POST, handleUploadSelected);
  server.on("/downloadall", handleDownloadAll);
  server.on("/delete", handleDelete);
  server.on("/delete-selected", HTTP_POST, handleDeleteSelected);
  server.on("/delete-all-csv", handleDeleteAllCsv);
  server.on("/return", handleReturn);
  server.on("/wifi", handleWifiConfig);
  server.on("/wifi-save", HTTP_POST, handleWifiSave);
  server.on("/wdg", handleWdgPage);
  server.on("/wdg-save", HTTP_POST, handleWdgSave);
  server.on("/wdg-upload", HTTP_POST, handleWdgUpload);
  server.on("/wigle", handleWiglePage);
  server.on("/wigle-save", HTTP_POST, handleWigleSave);
  server.on("/wigle-upload", HTTP_POST, handleWigleUpload);
  // Captive-portal style: any unknown path -> main page
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  apActive = true;
  apStartTime = millis();
  homeWifiConnected = false;  // STA not used while pure AP is up
  homeWifiIP = "";

  Serial.println("Portal AP ready — open http://192.168.4.1");
  // SoftAP only here — home STA is manual via Settings > WIFI
  loadHomeWifi();
  userWantsHomeWifi = false;
}

void portalSkipRect(int &bx, int &by, int &bw, int &bh) {
  int w = tft.width(), h = tft.height();
  bw = (w > h) ? min(280, w - 16) : min(200, w - 20);
  bh = (w > h) ? 50 : 48;
  bx = (w - bw) / 2;
  by = h - 58;
}

void portalRingGeom(int &cx, int &cy, int &r) {
  int w = tft.width(), h = tft.height();
  if (w > h) { cx = w - 64; cy = 112; r = 40; }
  else { cx = w / 2; cy = 230; r = 30; }
}

void portalDrawRing(LovyanGFX &g, int cx, int cy, int r, int secsLeft) {
  uint16_t acc = getAccent();
  g.fillCircle(cx, cy, r + 2, TFT_BLACK);
  g.fillArc(cx, cy, r, r - 6, 0, 360, C565(30, 33, 42));
  float frac = constrain(secsLeft / (AP_DURATION / 1000.0f), 0.0f, 1.0f);
  if (frac > 0.0f) g.fillArc(cx, cy, r, r - 6, 270, 270 + (int)(360 * frac), secsLeft <= 10 ? C_WARN : acc);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", secsLeft < 0 ? 0 : secsLeft);
  g.setTextDatum(MC_DATUM);
  g.setFont(F_L);
  g.setTextColor(UI_TEXT);
  g.drawString(buf, cx, cy - 2);
  g.setFont(F_XS);
  g.setTextColor(UI_MUTE);
  g.drawString("sec", cx, cy + 13);
}

void drawPortalCountdown(int secsLeft) {
  tft.fillScreen(TFT_BLACK);
  drawTitleBar("WEB PORTAL");
  int w = tft.width();
  int h = tft.height();
  bool landscape = (w > h);
  int cardX = 8, cardY = landscape ? 54 : 98, cardW = landscape ? w - 130 : w - 16, cardH = landscape ? 116 : 92;
  uiCard(cardX, cardY, cardW, cardH, getAccent(), false);
  uint16_t fill = uiCardFill();
  int rowY = cardY + 12;
  drawIcon(ICON_WIFI, 20, cardX + 10, rowY, getAccent(), fill);
  tft.setFont(F_XS);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(UI_MUTE, fill);
  tft.setFont(F_XS);
  tft.drawString("1. Join this Wi-Fi", cardX + 38, rowY);
  uiResetFont();
  tft.setFont(F_M);
  tft.setTextColor(UI_TEXT, fill);
  tft.drawString("SKIrf_WARDRIVER", cardX + 38, rowY + 10);
  uiResetFont();
  rowY += landscape ? 52 : 42;
  drawIcon(ICON_PORTAL, 20, cardX + 10, rowY, getAccent(), fill);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(UI_MUTE, fill);
  tft.setFont(F_XS);
  tft.drawString("2. Open in a browser", cardX + 38, rowY);
  uiResetFont();
  tft.setFont(F_M);
  tft.setTextColor(C_CYAN, fill);
  tft.drawString("192.168.4.1", cardX + 38, rowY + 10);
  uiResetFont();
  int cx, cy, r;
  portalRingGeom(cx, cy, r);
  portalDrawRing(tft, cx, cy, r, secsLeft);
  if (landscape) {
    tft.setFont(F_XS);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(UI_DIM, TFT_BLACK);
    tft.drawString("closes in", cx, cy - r - 10);
    uiResetFont();
  }
  int bx, by, bw, bh;
  portalSkipRect(bx, by, bw, bh);
  drawThemedButton(bx, by, bw, bh, "SKIP", getBtnFill(), getTextAccent());
  if (!landscape) laneConfigure(0, 0, 50, w, 44, laneSceneFor(ROLE_HOME_A), ROLE_HOME_A, false, true);
}

void portalUpdateSecondsOnly(int secsLeft) {
  int cx, cy, r;
  portalRingGeom(cx, cy, r);
  int s = r * 2 + 6;
  paintRegion(cx - s / 2, cy - s / 2, s, s, [&](LovyanGFX &g, int bx, int by) {
    portalDrawRing(g, bx + s / 2, by + s / 2, r, secsLeft);
  });
}

// NON-BLOCKING: start portal UI then return; tickPortalCountdown() runs from loop()
void runPortalCountdown() {
  if (!apActive) return;
  portalStartMs = millis();
  portalLastSec = 60;
  portalCountdownActive = true;
  drawPortalCountdown(60);
  Serial.println("Portal countdown (non-blocking) start");
}

void tickPortalCountdown() {
  if (!portalCountdownActive) return;
  if (!apActive) {
    // SoftAP died — leave portal UI
    portalCountdownActive = false;
    currentScreen = HOME_SCREEN;
    drawHomeScreen();
    return;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - portalStartMs;

  // ONLY way out (besides web /return): full 60s timeout
  // Touch SKIP disabled — was false-triggering and locking the UI on this CYD
  if (elapsed >= AP_DURATION) {
    portalCountdownActive = false;
    stopPortalAP();
    Serial.println("Portal timeout -> home");
    delay(50);
    currentScreen = HOME_SCREEN;
    drawHomeScreen();
    Serial.println("Home drawn");
    touchIgnoreUntilUp = true;
    return;
  }

  int secsLeft = (int)((AP_DURATION - elapsed + 999UL) / 1000UL);
  if (secsLeft < 0) secsLeft = 0;
  if (secsLeft != portalLastSec) {
    portalLastSec = secsLeft;
    portalUpdateSecondsOnly(secsLeft);
    Serial.printf("Portal %d\n", secsLeft);
  }

  // Serve web clients; do not call softAPgetStationNum here
  server.handleClient();

  if (returnToDevice) {
    returnToDevice = false;
    portalCountdownActive = false;
    stopPortalAP();
    currentScreen = HOME_SCREEN;
    drawHomeScreen();
    touchIgnoreUntilUp = true;
    return;
  }

  // SKIP: only the drawn button, only after 2s, only a real mapped tap.
  // Ghost IRQ / raw "touched" used to end the portal in ~1 second.
  if (elapsed < 2000) return;

  static unsigned long lastPoll = 0;
  static unsigned long downSince = 0;
  if (now - lastPoll < 40) return;
  lastPoll = now;

  int tx = 0, ty = 0;
  if (!getTouchMapped(tx, ty)) {
    downSince = 0;
    return;
  }

  int h = tft.height();
  int bx, by, btnW, btnH;
  portalSkipRect(bx, by, btnW, btnH);
  bool onSkip = (tx >= bx && tx <= bx + btnW && ty >= by - 4 && ty <= h - 1);
  if (!onSkip) {
    downSince = 0;
    return;
  }

  if (downSince == 0) downSince = now;
  if (now - downSince < 120) return;

  downSince = 0;
  portalCountdownActive = false;
  stopPortalAP();
  touchIgnoreUntilUp = true;
  Serial.println("Portal SKIP -> home");
  currentScreen = HOME_SCREEN;
  drawHomeScreen();
}

void stopPortalAP() {
  if (!apActive) return;
  WiFi.softAPdisconnect(true);
  apActive = false;
  if (homeWifiConnected && WiFi.status() == WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    Serial.println("AP stopped — home WiFi still up");
  } else {
    server.stop();
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);          // radio silent until a run or Wi-Fi screen needs it (GPS-friendly)
    Serial.println("AP stopped, Wi-Fi radio off");
  }
}

void handlePortal() {
  // Serve web UI while AP is up OR while connected to home WiFi
  if (apActive || homeWifiConnected) {
    server.handleClient();
  }
  if (returnToDevice) {
    returnToDevice = false;
    stopPortalAP();
    currentScreen = HOME_SCREEN;
    drawHomeScreen();
    return;
  }
  // Auto-stop only when not in UI countdown (tickPortalCountdown owns that)
  if (apActive && !portalCountdownActive &&
      millis() - apStartTime >= AP_DURATION) {
    stopPortalAP();
  }
}

// ---------- Boot splash ----------
void drawSplashText() {
  int w = tft.width(), h = tft.height();
  bool portrait = h > w;
  int cy = portrait ? h / 2 - 4 : h / 2;
  uint16_t acc = getAccent();
  // faint radar rings behind the wordmark
  for (int r = 26; r <= 96; r += 18) tft.drawCircle(w / 2, cy - 6, r, blend565(TFT_BLACK, acc, (uint8_t)(70 - r / 2)));
  tft.drawFastHLine(w / 2 - 100, cy - 6, 200, blend565(TFT_BLACK, acc, 30));
  tft.setFont(&fonts::Orbitron_Light_32);
  int skiW = tft.textWidth("SKI"), rfW = tft.textWidth("rf");
  int x0 = (w - skiW - rfW) / 2;
  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(C565(255, 128, 0));
  tft.drawString("SKI", x0, cy + 4);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("rf", x0 + skiW, cy + 4);
  tft.setFont(F_M);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(UI_TEXT);
  tft.drawString("W A R D R I V E R", w / 2, cy + 12);
  int lw = 150;
  for (int i = 0; i < lw; i++) {
    int d = abs(i - lw / 2);
    tft.drawPixel((w - lw) / 2 + i, cy + 36, blend565(TFT_BLACK, acc, (uint8_t)(255 - d * 255 / (lw / 2))));
  }
  uiResetFont();
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(UI_MUTE);
  tft.drawString(SKIRF_VERSION "   ESP32-C5  2.4 + 5 GHz  BLE", w / 2, cy + 43);
  uiResetFont();
}

void runSplash(unsigned long ms) {
  paletteRefresh();
  tft.fillScreen(TFT_BLACK);
  int w = tft.width(), h = tft.height();
  bool portrait = h > w;
  drawSplashText();
  bootSplashActive = true;
  int ya = portrait ? 34 : 2, yb = portrait ? h - 84 : h - 46;
  laneConfigure(0, 0, ya, w, 44, laneSceneFor(ROLE_SPLASH_A), ROLE_SPLASH_A, false, true);
  laneConfigure(1, 0, yb, w, 44, laneSceneFor(ROLE_SPLASH_B), ROLE_SPLASH_B, false, false);
  for (int i = 0; i < 2; i++) {
    if (!lanes[i].on) continue;
    if (lanes[i].scene != SC_ROAD) continue;
    laneSetupActor(lanes[i]);
    lanes[i].a.speed *= 1.7f;
    lanes[i].a.x += lanes[i].a.right ? 30 : -30;
  }
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    updateCarAnimation();
    delay(4);
  }
  lanesStop();
  bootSplashActive = false;
}

// ---------- setup() ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== WARDRIVER BOOT ===");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // TFT FIRST
  tft.init();
  initFonts();
  tft.setRotation(0);
  digitalWrite(TFT_BL, HIGH);
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
  Serial.printf("Display %dx%d rot=%d\n", tft.width(), tft.height(), tft.getRotation());

  // Touch + SD share the TFT SPI bus (SCK=6 MOSI=7 MISO=2)
  touch.begin();
  touch.setRotation(0);

  delay(80);
  openLogFS();          // SD if present, else internal flash
  loadHomeWifi();

  loadGpsPins();
  gpsLastHuntMs = millis();
  gpsOpenPort(gpsRxPin, gpsTxPin, 9600);

  calLoaded = loadCalibration();
  if (calLoaded && lgfxCalOk) tft.setTouchCalibrate(lgfxCal);
  applyTransportProfile(WD_XPORT_CAR);
  // Respect saved orientation (0/1/2/3). Do NOT force portrait — that
  // overwrote landscape choices after orientation + calibration.
  if (preferredRotation < 0 || preferredRotation > 3) preferredRotation = 0;
  tft.setRotation(preferredRotation);
  touch.setRotation(0);  // raw coords; affine maps to screen
  Serial.printf("Boot rotation=%d  %dx%d  calLoaded=%d\n",
                preferredRotation, tft.width(), tft.height(), (int)calLoaded);

  // Hold BOOT (GPIO0) during power-up → force touch calibration
  pinMode(0, INPUT_PULLUP);
  delay(20);
  if (digitalRead(0) == LOW) {
    Serial.println("BOOT held — forcing touch calibration");
    calLoaded = false;
  }

  if (calLoaded) {
    setBrightness(brightness);
  } else {
    // First boot only: prefer a portrait rotation for easier first cal
    preferredRotation = 0;
    tft.setRotation(0);
    if (tft.width() >= tft.height()) {
      for (int r = 0; r < 4; r++) {
        tft.setRotation(r);
        if (tft.height() > tft.width()) {
          preferredRotation = r;
          break;
        }
      }
    }
    touch.setRotation(0);
    runCalibration();  // saves preferredRotation + restarts
  }
  tft.setRotation(preferredRotation);
  touch.setRotation(0);

  loadRecords();
  randomSeed(esp_random());
  runSplash(2800);

  // 1-minute web portal AP — NON-BLOCKING (tickPortalCountdown in loop)
  startPortalAP();
  runPortalCountdown();

  // Only force calibration if none is saved
  if (!calLoaded) {
    Serial.println("No calibration saved — running touch cal...");
    runCalibration();  // saves + restarts
  }

  if (!portalCountdownActive) {
    currentScreen = HOME_SCREEN;
    drawHomeScreen();
    Serial.println("=== READY ===");
  } else {
    Serial.println("=== READY (portal active) ===");
  }
}

// ---------- loop(): touch dispatch for every screen ----------
void loop() {
  // Serial: press 'c' + Enter to force touch calibration anytime
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'c' || c == 'C') {
      Serial.println("Serial: force calibration");
      runCalibration();
    }
  }

  // Portal countdown must run even if WiFi/touch is slow elsewhere
  if (portalCountdownActive) {
    readGPS();            // keep the GPS link drained while the portal is up
    tickPortalCountdown();
    if (portalCountdownActive) updateCarAnimation();
    delay(10);
    return;
  }

  readGPS();
  handlePortal();
  maintainHomeWifi();

  if (wardriveActive && (millis() - lastUiTouchMs > 350)) pumpWardriveScan();

  static unsigned long last1Hz = 0;
  if (millis() - last1Hz >= 1000) {
    last1Hz = millis();
    wdTrackDistance();
    // Wi-Fi off whenever nothing uses it: the ESP32-C5's radio sits next to the GPS antenna,
    // and an idle-but-on radio is still noise the GPS has to dig through for its first fix.
    {
      bool wifiScreen = (currentScreen == WIFI_SCREEN || currentScreen == WIFI_SCAN_SCREEN || currentScreen == WIFI_PASS_SCREEN || currentScreen == GAME_STATS_SCREEN);
      if (!wardriveActive && !apActive && !homeWifiConnected && !wifiScreen && WiFi.getMode() != WIFI_OFF) {
        WiFi.mode(WIFI_OFF);
        Serial.println("Wi-Fi radio off (idle)");
      }
    }
    // live hints on the Home tiles (REC count, GPS lock)
    if (currentScreen == HOME_SCREEN) {
      static String lastHints = "";
      char h0[40], h1[40];
      homeHint(0, h0, sizeof(h0));
      h1[0] = 0;
      String hs = String(h0) + "|" + h1;
      if (hs != lastHints) {
        if (lastHints.length()) drawHomePrimary();
        lastHints = hs;
      }
    }
  }

  if (millis() - lastGPSUpdate >= 400) {
    if (currentScreen == GPS_SCREEN) updateGPSScreen();
    lastGPSUpdate = millis();
  }

  if (currentScreen == HOME_SCREEN) updateHomeStatusBar(false);
  if (currentScreen == WARDRIVING_SCREEN) wdUiTick();
  drawTitleStatus(false);

  // Sprite lanes (self-throttled to ~20-25 fps)
  updateCarAnimation();

  int tx = 0, ty = 0;
  bool touching = getTouchMapped(tx, ty);

  // After navigation, ignore until the finger fully lifts.
  // Do NOT time out while still down — that was "tap goes so fast it reverts".
  static unsigned long lastLiftMs = 0;
  if (touchIgnoreUntilUp) {
    if (!touching) {
      touchIgnoreUntilUp = false;
      lastLiftMs = millis();
    } else {
      touching = false;
    }
  }
  // Dead time after a lift so the same tap cannot chain into the next page
  if (touching && lastLiftMs && (millis() - lastLiftMs < 220)) {
    touching = false;
  }

  static unsigned long touchDownAt = 0;
  static bool holdFired = false;
  if (touching && !wasTouching) {
    touchDownAt = millis();
    holdFired = false;
  }
  bool edgePress = touching && !wasTouching;
  // holdPress only for continuous controls (brightness slider)
  bool holdPress = touching && wasTouching && !holdFired && (millis() - touchDownAt > 50);
  if (holdPress) holdFired = true;

  int debounceNeed = (currentScreen == WIFI_PASS_SCREEN) ? 20 : DEBOUNCE_DELAY;
  // Navigation = rising edge only (one action per tap)
  bool menuPress = edgePress;
  if (currentScreen == BRIGHTNESS_SCREEN)
    menuPress = edgePress || holdPress;

  if (currentScreen == SETTINGS_SCREEN) settingsTouch(touching, tx, ty);
  scrollListsTouch(touching, tx, ty);

  if (currentScreen == WIFI_PASS_SCREEN) {
    static bool kbDown = false;
    static int kbDownX = 0, kbDownY = 0;
    int w = tft.width();
    int h = tft.height();
    int fieldY, fieldH, gridY, keyH, keyGap, actY, actH, joinY, joinH, cols;
    wifiPassKbGeom(fieldY, fieldH, gridY, keyH, keyGap, actY, actH, joinY, joinH, cols);
    int keyW = (w - 8) / cols;

    if (touching && !kbDown) {
      kbDown = true;
      kbDownX = tx;
      kbDownY = ty;
    }
    if (!touching && kbDown) {
      kbDown = false;
      int x = kbDownX, y = kbDownY;
      bool acted = false;
      const char* keys = wifiPassActiveKeys();
      int nkeys = strlen(keys);
      int startX = (w - keyW * cols) / 2;
      for (int i = 0; i < nkeys && i < cols * 2; i++) {
        int r = i / cols, c = i % cols;
        int kx = startX + c * keyW;
        int ky = gridY + r * (keyH + keyGap);
        if (x >= kx && x < kx + keyW && y >= ky && y < ky + keyH) {
          if (pendingPass.length() < 63) pendingPass += keys[i];
          drawWifiPassScreen();
          beep(1, 10);
          acted = true;
          break;
        }
      }
      if (!acted) {
        int nAct = 7;
        int aw = (w - 10) / nAct;
        for (int i = 0; i < nAct; i++) {
          int ax0 = 5 + i * aw;
          if (x >= ax0 && x < ax0 + aw && y >= actY && y < actY + actH) {
            if (i == 0) { if (pendingPass.length() < 63) pendingPass += ' '; }
            else if (i == 1) {
              if (kbMode != 0) { kbMode = 0; kbPage = 0; }
              else kbUpperCase = !kbUpperCase;
            } else if (i == 2) { kbMode = 1; kbPage = 0; }
            else if (i == 3) { kbMode = 2; kbPage = 0; }
            else if (i == 4) { if (kbMode == 0) kbPage = 0; }
            else if (i == 5) { if (kbMode == 0) kbPage = 1; }
            else if (i == 6) { if (pendingPass.length()) pendingPass.remove(pendingPass.length() - 1); }
            drawWifiPassScreen();
            beep(1, 12);
            acted = true;
            break;
          }
        }
      }
      if (!acted && y >= joinY && y <= h - 1) {
        int jw = (w - 18) / 2;
        // Dead zone in the middle so JOIN cannot fire BACK
        if (x >= 6 && x <= 6 + jw - 8) {
          homeSSID = pendingSSID;
          homePass = pendingPass;
          saveHomeWifi();
          showWdgStatusScreen("Connecting...", homeSSID.c_str(), "Please wait", -1, 0, "WIFI");
          bool ok = tryConnectHomeWifi(18000);
          if (ok) {
            showWdgStatusScreen("Connected!", WiFi.localIP().toString().c_str(), "Saved as home WiFi", -1, 0, "WIFI");
            beep(2, 40);
            delay(2200);
            currentScreen = WIFI_SCREEN;
            drawWifiScreen();
          } else {
            char st[24];
            snprintf(st, sizeof(st), "status=%d", (int)WiFi.status());
            showWdgStatusScreen("Failed", st, "Check SSID/pass", -1, 0, "WIFI");
            delay(2800);
            drawWifiPassScreen();
          }
          touchIgnoreUntilUp = true;
        } else if (x >= 6 + jw + 8) {
          currentScreen = WIFI_SCAN_SCREEN;
          drawWifiScanScreen();
          beep(1, 25);
          touchIgnoreUntilUp = true;
        }
      }
    }
    if (!touching) kbDown = false;
  }

  if (menuPress && (millis() - lastTouchTime >= (unsigned long)debounceNeed)) {
    {
      lastTouchTime = millis();
      int w = tft.width();
      int h = tft.height();

      if (currentScreen == HOME_SCREEN) {
        int tile = homeTileHit(tx, ty);
        if (tile == 0) {
          // mid-session: jump straight back to the live dashboard
          currentScreen = wardriveActive ? WARDRIVING_SCREEN : WD_MODE_SCREEN;
          if (wardriveActive) drawWardrivingScreen(); else drawWdModeScreen();
          beep(1, 35);
          touchIgnoreUntilUp = true;
        } else if (tile == 1) {
          currentScreen = GPS_SCREEN;
          drawGPSScreen();
          beep(1, 35);
          touchIgnoreUntilUp = true;
        } else if (tile == 2) {
          loadCsvFileList();
          filesReturnTo = HOME_SCREEN;
          currentScreen = FILES_SCREEN;
          drawFilesScreen();
          beep(1, 35);
          touchIgnoreUntilUp = true;
        } else if (tile == 3) {
          currentScreen = SETTINGS_SCREEN;
          settingsScroll = 0;
          drawSettingsScreen();
          beep(1, 35);
          touchIgnoreUntilUp = true;
        }
      }
      else if (currentScreen == SUMMARY_SCREEN) {
        Rect d = summaryDoneRect();
        if (ty >= d.y - 8) {
          currentScreen = WARDRIVING_SCREEN;
          drawWardrivingScreen();
          beep(1, 35);
          touchIgnoreUntilUp = true;
        }
      }
      else if (currentScreen == WD_MODE_SCREEN) {
        int h = tft.height();
        lastUiTouchMs = millis();
        if (ty >= h - 48 && tx <= 110) {
          currentScreen = HOME_SCREEN;
          drawHomeScreen();
          beep(1, 35);
          touchIgnoreUntilUp = true;
        } else {
          int picked = -1;
          for (int i = 0; i < 3; i++) {
            int x, y, bw, bh;
            wdModeTileRect(i, x, y, bw, bh);
            if (tx >= x - 4 && tx <= x + bw + 4 && ty >= y - 3 && ty <= y + bh + 3) { picked = i; break; }
          }
          {
            if (picked >= 0) {
              applyTransportProfile(picked);
              currentScreen = WARDRIVING_SCREEN;
              drawWardrivingScreen();
              beep(2, 35);
              touchIgnoreUntilUp = true;
            }
          }
        }
      }
      else if (currentScreen == WARDRIVING_SCREEN) {
        wardriveTap(tx, ty);
      }
      else if (currentScreen == GPS_SCREEN) {
        if (tx >= 2 && tx <= 110 && ty >= h-50 && ty <= h-1) {
          currentScreen = HOME_SCREEN;
          drawHomeScreen();
          beep(1, 35);
        }
      }
      else if (currentScreen == WIFI_SCAN_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        int navW = (w - 24) / 2;
        int navY = h - 40;

        if (tx >= 8 && tx <= 8 + navW && ty >= navY && ty <= navY + 34) {
          currentScreen = WIFI_SCREEN;
          drawWifiScreen();
          beep(1, 25);
        } else if (tx >= 16 + navW && tx <= w - 8 && ty >= navY && ty <= navY + 34) {
          doWifiScan();
          drawWifiScanScreen();
          beep(1, 25);
        }
      }
      else if (currentScreen == WIFI_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        bool landscape = (w > h);
        int navH = landscape ? 36 : 34;
        int navY = h - navH - 6;
        int btnH = landscape ? 40 : 44;
        int btnY = navY - btnH - 8;
        int btnW = (w - 24) / 2;
        int gap = 8;
        int x0 = 8;
        int x1 = x0 + btnW + gap;
        int navW = (w - 24) / 2;
        int pad = 8;

        // BACK
        if (tx >= 0 && tx <= 8 + navW + pad && ty >= navY - pad && ty <= h - 1) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 30);
          touchIgnoreUntilUp = true;
        }
        // CHOOSE NET → scan / pick network
        else if (tx >= 16 + navW - pad && tx <= w - 1 && ty >= navY - pad && ty <= h - 1) {
          doWifiScan();
          currentScreen = WIFI_SCAN_SCREEN;
          drawWifiScanScreen();
          beep(1, 30);
          touchIgnoreUntilUp = true;
        }
        // CONNECT
        else if (tx >= x0 - pad && tx <= x0 + btnW + pad && ty >= btnY - pad && ty <= btnY + btnH + pad) {
          if (homeSSID.length() == 0) {
            showWdgStatusScreen("No saved WiFi", "Use CHOOSE NET", "or set in portal", -1, 0, "WIFI");
            delay(2500);
            drawWifiScreen();
          } else {
            showWdgStatusScreen("Connecting...", homeSSID.c_str(), "Please wait", -1, 0, "WIFI");
            bool ok = tryConnectHomeWifi(15000);
            if (ok) {
              showWdgStatusScreen("Connected!", WiFi.localIP().toString().c_str(), "You can upload files", -1, 0, "WIFI");
              beep(2, 40);
              delay(2500);
            } else {
              char st[28];
              snprintf(st, sizeof(st), "WiFi status=%d", (int)WiFi.status());
              showWdgStatusScreen("Connect failed", st, "SSID/pass / range / 2.4GHz", -1, 0, "WIFI");
              beep(1, 80);
              delay(2800);
            }
            drawWifiScreen();
          }
          touchIgnoreUntilUp = true;
        }
        // DISCONNECT
        else if (tx >= x1 - pad && tx <= x1 + btnW + pad && ty >= btnY - pad && ty <= btnY + btnH + pad) {
          userWantsHomeWifi = false;
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          homeWifiConnected = false;
          homeWifiIP = "";
          beep(1, 30);
          drawWifiScreen();
          touchIgnoreUntilUp = true;
        }
      }
      

      else if (currentScreen == GAME_STATS_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        if (tx >= 5 && tx <= 100 && ty >= h - 40 && ty <= h - 4) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 30);
          touchIgnoreUntilUp = true;
        } else if (tx >= 8 + 100 && ty >= h - 40 && ty <= h - 4) {
          beep(1, 25);
          touchIgnoreUntilUp = true;
          gameStatsScroll = 0;
          drawGameStatsScreen(true);
        }
      }

      else if (currentScreen == PRO_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        if (ty >= h - 48 && tx <= 110) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 30);
          touchIgnoreUntilUp = true;
        }
      }

      else if (currentScreen == PRO_SESSION_SCREEN) {
        int h = tft.height();
        if (ty >= h - 48 && tx <= 110) {
          currentScreen = PRO_SCREEN;
          drawProScreen();
          beep(1, 25);
          touchIgnoreUntilUp = true;
        }
      }

      else if (currentScreen == DISCLAIMER_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        if (tx >= 8 && tx <= 118 && ty >= h - 40 && ty <= h - 6) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 30);
        }
      }
      else if (currentScreen == FILES_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        int listTop, listBottom, rowH, rowX, rowW, ax, dockY, viewRows;
        filesGeom(listTop, listBottom, rowH, rowX, rowW, ax, dockY, viewRows);
        int gap = 6;
        int dw = (w - 24 - 2 * gap) / 3;
        int y1 = dockY;
        int bh = 32;

        if (false) {
        } else if (ty >= y1 && ty <= y1 + bh) {
          int slot = -1;
          if (tx >= 8 && tx < 12 + dw + gap / 2) slot = 0;
          else if (tx < 12 + 2 * dw + gap + gap / 2) slot = 1;
          else if (tx <= w - 8) slot = 2;
          if (slot == 0) {
            bool allOn = (fileBrowserCount > 0 && filesSelectedCount() == fileBrowserCount);
            for (int i = 0; i < fileBrowserCount; i++) fileSelected[i] = !allOn;
            fileArmDelete = 0;
            drawFilesScreen();
            beep(1, 22);
          } else if (slot == 1) {
            fileArmDelete = 0;
            filesUploadSelected(true, false);
            beep(1, 35);
          } else if (slot == 2) {
            fileArmDelete = 0;
            filesUploadSelected(false, true);
            beep(1, 35);
          }
        } else if (ty >= h - 40 && tx >= 8 + 100) {
          if (filesSelectedCount() == 0) {
            filesShowMsg("Nothing selected", "Tap files or ALL first");
          } else if (fileArmDelete != 1) {
            fileArmDelete = 1;
            drawFilesScreen();
            beep(2, 30);
          } else {
            fileArmDelete = 0;
            filesDeleteSelected();
            beep(1, 35);
          }
        } else if (ty >= h - 40 && tx <= 100) {
          fileArmDelete = 0;
          currentScreen = filesReturnTo;
          if (filesReturnTo == HOME_SCREEN) drawHomeScreen(); else drawSettingsScreen();
          beep(1, 30);
        }
      }

      else if (currentScreen == GPS_PINS_SCREEN) {
        int w = tft.width();
        int h = tft.height();
        int hintY, boxY, boxH, bw, stepY, stepH, presetY, pw, ph, saveY, saveH, pGap;
        gpsPinsGeom(hintY, boxY, boxH, bw, stepY, stepH, presetY, pw, ph, saveY, saveH, pGap);
        int stepW = (w - 24) / 2;

        if (tx >= 8 + 100 && ty >= saveY && ty <= saveY + saveH) {
          saveGpsPinsAndRestart();
        } else if (tx >= 5 && tx <= 100 && ty >= h - 40 && ty <= h - 2) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 40);
        } else if (tx >= 8 && tx <= 8 + bw && ty >= boxY && ty <= boxY + boxH) {
          gpsEditFocusRx = true;
          drawGpsPinsScreen();
          beep(1, 20);
        } else if (tx >= 16 + bw && tx <= 16 + 2 * bw && ty >= boxY && ty <= boxY + boxH) {
          gpsEditFocusRx = false;
          drawGpsPinsScreen();
          beep(1, 20);
        } else if (tx >= 8 && tx <= 8 + stepW && ty >= stepY && ty <= stepY + stepH) {
          if (gpsEditFocusRx) gpsPinEditRx--; else gpsPinEditTx--;
          gpsPinsClampEdit();
          drawGpsPinsScreen();
          beep(1, 15);
        } else if (tx >= 16 + stepW && tx <= 16 + 2 * stepW && ty >= stepY && ty <= stepY + stepH) {
          if (gpsEditFocusRx) gpsPinEditRx++; else gpsPinEditTx++;
          gpsPinsClampEdit();
          drawGpsPinsScreen();
          beep(1, 15);
        } else {
          for (int i = 0; i < 4; i++) {
            int col = i % 2, row = i / 2;
            int x = 8 + col * (pw + 8);
            int y = presetY + row * (ph + pGap);
            if (tx >= x && tx <= x + pw && ty >= y && ty <= y + ph) {
              gpsPinEditRx = gpsPresetRx[i];
              gpsPinEditTx = gpsPresetTx[i];
              drawGpsPinsScreen();
              beep(1, 25);
              break;
            }
          }
        }
      }

      else if (currentScreen == BRIGHTNESS_SCREEN) {
        int cardY, cardH, sliderY, presetY, presetH;
        brightGeom(cardY, cardH, sliderY, presetY, presetH);
        if (tx >= 5 && tx <= 95 && ty >= h-40 && ty <= h-5) {
          saveCalibration();
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 60);
        } else if (ty >= presetY - 3 && ty <= presetY + presetH + 3) {
          int gap = 6, pw = (w - 16 - 2 * gap) / 3;
          int i = (tx - 8) / (pw + gap);
          if (i >= 0 && i < 3) {
            setBrightness(brightPresets[i]);
            drawBrightnessScreen();
            beep(1, 25);
          }
        }
      }
      
      else if (currentScreen == UI_COLOR_SCREEN) {
        if (tx >= 5 && tx <= 110 && ty >= h - 45 && ty <= h - 2) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 60);
        }
      }
      else if (currentScreen == ORIENTATION_SCREEN) {
        int btnW, btnH, gap, startY, col0, col1;
        orientationBtnGeom(btnW, btnH, gap, startY, col0, col1);
        bool hit = false;
        for (int i = 0; i < 4; i++) {
          int col = i % 2;
          int row = i / 2;
          int x = (col == 0) ? col0 : col1;
          int y = startY + row * (btnH + gap);
          int pad = 10;
          if (tx >= x - pad && tx <= x + btnW + pad && ty >= y - pad && ty <= y + btnH + pad) {
            applyOrientation(i);  // 0,90,180,270 -> rot 0..3
            hit = true;
            break;
          }
        }
        if (!hit && tx >= 5 && tx <= 95 && ty >= h-40 && ty <= h-5) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
          beep(1, 60);
        }
      }
    }
  }

  // Wardrive buttons are edge-only (see menuPress). No hold-repeat.


  // Continuous brightness slider drag (while finger is down)
  if (touching && currentScreen == BRIGHTNESS_SCREEN) {
    int w = tft.width();
    int sw = min(280, w - 40);
    int cardY, cardH, sliderY, presetY, presetH;
    brightGeom(cardY, cardH, sliderY, presetY, presetH);
    if (tx >= 8 && tx <= w - 8 && ty >= sliderY - 22 && ty <= sliderY + 30) {
      int v = map(constrain(tx, 20, 20 + sw), 20, 20 + sw, 12, 255);
      if (abs(v - brightness) >= 2) {
        setBrightness(v);
        drawBrightnessValue();
        drawSlider(20, sliderY, sw, 14, 12, 255, brightness, getAccent());
      }
    }
  }

  // Emergency: hold a finger ~3s on an empty part of HOME → touch calibration
  static unsigned long homeHoldStart = 0;
  int rawX = 0, rawY = 0;
  bool rawDown = c5RawTouch(rawX, rawY);
  if (touchIgnoreUntilUp) {
    homeHoldStart = 0;
  } else if (currentScreen == HOME_SCREEN && rawDown) {
    if (homeHoldStart == 0) homeHoldStart = millis();
    else if (millis() - homeHoldStart > 3000) {
      homeHoldStart = 0;
      touchIgnoreUntilUp = true;
      beep(3, 40);
      runCalibration();
      touchIgnoreUntilUp = true;
    }
  } else {
    homeHoldStart = 0;
  }

  wasTouching = touching;
  delay(1);
}