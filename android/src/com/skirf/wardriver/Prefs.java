package com.skirf.wardriver;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.HashSet;
import java.util.Set;

/** Every setting in one place, with its default. */
public final class Prefs {
    private static SharedPreferences sp;

    // Scanning
    public static final String WIFI = "wifi";               // bool
    public static final String WIFI_INTERVAL = "wifi_iv";   // int ms
    public static final String BLE = "ble";                 // bool
    public static final String BLE_MODE = "ble_mode";       // 0 low power, 1 balanced, 2 max
    public static final String BT = "bt";                   // bool (classic)
    public static final String CELL = "cell";               // bool
    public static final String MIN_RSSI = "min_rssi";       // int dBm, -127 = off
    // Logging
    public static final String LOG_MODE = "log_mode";       // 0 once, 1 stronger, 2 every
    public static final String NEED_FIX = "need_fix";       // bool
    public static final String MAX_ACC = "max_acc";         // int metres, 0 = any
    public static final String SPLIT = "split";             // int minutes, 0 = never, -1 = 50k rows
    public static final String COPY_DL = "copy_dl";         // bool
    public static final String PREFIX = "prefix";           // string
    // GPS
    public static final String GPS_RATE = "gps_rate";       // int ms
    public static final String NET_LOC = "net_loc";         // bool
    public static final String UNITS = "units";             // 0 metric, 1 imperial
    // Run
    public static final String SCREEN_ON = "screen_on";     // bool
    public static final String WAKELOCK = "wakelock";       // bool
    public static final String AUTOSTART = "autostart";     // bool
    public static final String BUZZ_OPEN = "buzz_open";     // bool
    public static final String TICK_NEW = "tick_new";       // bool
    // Uploads
    public static final String WIGLE_NAME = "wigle_name";
    public static final String WIGLE_TOKEN = "wigle_token";
    public static final String WDG_KEY = "wdg_key";
    public static final String AUTO_UP = "auto_up";         // 0 off, 1 wigle, 2 wdg, 3 both
    public static final String UP_WIFI_ONLY = "up_wifi";    // bool
    public static final String UPLOADED = "uploaded";       // string set "w:name" / "d:name"
    // Appearance
    public static final String ACCENT = "accent";           // int index into Theme.ACCENTS
    // App state
    public static final String ONBOARDED = "onboarded";
    public static final String LAST_TAB = "last_tab";
    public static final String MAP_TILES = "map_tiles";     // bool

    public static void init(Context c) {
        sp = c.getSharedPreferences("skirf", Context.MODE_PRIVATE);
    }

    public static SharedPreferences sp() { return sp; }

    public static boolean defBool(String k) {
        switch (k) {
            case WIFI: case BLE: case NEED_FIX: case SCREEN_ON: case WAKELOCK: case UP_WIFI_ONLY: case MAP_TILES:
                return true;
            default:
                return false;
        }
    }

    public static int defInt(String k) {
        switch (k) {
            case WIFI_INTERVAL: return 2000;
            case BLE_MODE: return 2;
            case MIN_RSSI: return -127;
            case LOG_MODE: return 0;
            case MAX_ACC: return 50;
            case SPLIT: return 0;
            case GPS_RATE: return 1000;
            case UNITS: return 1;
            case AUTO_UP: return 0;
            case ACCENT: return 0;
            default: return 0;
        }
    }

    public static boolean b(String k) { return sp.getBoolean(k, defBool(k)); }
    public static int i(String k) { return sp.getInt(k, defInt(k)); }
    public static String s(String k) {
        String d = PREFIX.equals(k) ? "SKIrf" : "";
        String v = sp.getString(k, d);
        return v == null ? d : v;
    }

    public static void put(String k, boolean v) { sp.edit().putBoolean(k, v).apply(); }
    public static void put(String k, int v) { sp.edit().putInt(k, v).apply(); }
    public static void put(String k, String v) { sp.edit().putString(k, v == null ? "" : v.trim()).apply(); }

    // ---- upload bookkeeping ----
    public static boolean uploaded(char site, String file) {
        Set<String> s = sp.getStringSet(UPLOADED, null);
        return s != null && s.contains(site + ":" + file);
    }

    public static synchronized void markUploaded(char site, String file, boolean on) {
        Set<String> cur = sp.getStringSet(UPLOADED, null);
        Set<String> s = cur == null ? new HashSet<String>() : new HashSet<String>(cur);
        if (on) s.add(site + ":" + file); else s.remove(site + ":" + file);
        sp.edit().putStringSet(UPLOADED, s).apply();
    }

    public static synchronized void forgetFile(String file) {
        markUploaded('w', file, false);
        markUploaded('d', file, false);
    }

    public static boolean hasWigle() { return s(WIGLE_NAME).length() > 0 && s(WIGLE_TOKEN).length() > 0; }
    public static boolean hasWdg() { return s(WDG_KEY).length() >= 8; }
    public static boolean imperial() { return i(UNITS) == 1; }

    private Prefs() {}
}
