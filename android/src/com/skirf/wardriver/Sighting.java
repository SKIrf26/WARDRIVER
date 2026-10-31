package com.skirf.wardriver;

import java.util.Locale;

/** One thing heard on one scan, and how it becomes a WiGLE CSV row. */
public final class Sighting {
    public static final char WIFI = 'W', BLE = 'B', BT = 'C', CELL = 'T';

    // security classes for the breakdown bar
    public static final int SEC_OPEN = 0, SEC_WEP = 1, SEC_WPA = 2, SEC_WPA2 = 3, SEC_WPA3 = 4;
    public static final String[] SEC_NAMES = {"Open", "WEP", "WPA", "WPA2", "WPA3"};

    public char type;
    public String mac;        // BSSID / BT address / cell key
    public String name;       // SSID, device name or operator
    public String auth;       // WiGLE AuthMode text
    public int channel;
    public int freq;          // MHz (Wi-Fi only)
    public int rssi;
    public int mfgr = -1;     // BLE manufacturer id
    public String cellType;   // GSM, WCDMA, LTE, NR
    public long time;         // ms UTC
    public boolean isNew;     // first time this run
    public boolean newEver;   // never seen on this phone before

    public String typeName() {
        switch (type) {
            case BLE: return "BLE";
            case BT: return "BT";
            case CELL: return cellType == null ? "CELL" : cellType;
            default: return "WIFI";
        }
    }

    // ---------------- Wi-Fi helpers ----------------
    public static int freqToChannel(int f) {
        if (f == 2484) return 14;
        if (f >= 2412 && f < 2484) return (f - 2407) / 5;
        if (f >= 5955 && f <= 7115) return (f - 5950) / 5;
        if (f == 5935) return 2;
        if (f >= 5000 && f <= 5900) return (f - 5000) / 5;
        if (f >= 4910 && f <= 4980) return (f - 4000) / 5;
        return 0;
    }

    /** 0 = 2.4 GHz, 1 = 5 GHz, 2 = 6 GHz. */
    public static int band(int f) {
        if (f < 3000) return 0;
        if (f >= 5925) return 2;
        return 1;
    }

    public static int secClass(String caps) {
        if (caps == null) return SEC_OPEN;
        String c = caps.toUpperCase(Locale.US);
        if (c.contains("SAE") || c.contains("SUITE-B") || c.contains("OWE")) return SEC_WPA3;
        if (c.contains("WPA2") || c.contains("RSN")) return SEC_WPA2;
        if (c.contains("WPA")) return SEC_WPA;
        if (c.contains("WEP")) return SEC_WEP;
        return SEC_OPEN;
    }

    public static boolean isOpen(String caps) {
        return secClass(caps) == SEC_OPEN && (caps == null || !caps.toUpperCase(Locale.US).contains("EAP"));
    }

    /** Same cleaning as the board: no control characters, commas become spaces, quotes become apostrophes. */
    public static String clean(String s) {
        if (s == null) return "";
        StringBuilder b = new StringBuilder(s.length());
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            if (ch < 0x20) continue;
            if (ch == ',') ch = ' ';
            else if (ch == '"') ch = '\'';
            b.append(ch);
        }
        return b.toString();
    }

    static String two(int v) { return v < 10 ? "0" + v : String.valueOf(v); }

    /** WiGLE time stamp, UTC, like the board writes from GPS time. */
    public static String stamp(long ms) {
        java.util.Calendar c = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone("UTC"), Locale.US);
        c.setTimeInMillis(ms);
        return c.get(java.util.Calendar.YEAR) + "-" + two(c.get(java.util.Calendar.MONTH) + 1) + "-" + two(c.get(java.util.Calendar.DAY_OF_MONTH))
                + " " + two(c.get(java.util.Calendar.HOUR_OF_DAY)) + ":" + two(c.get(java.util.Calendar.MINUTE)) + ":" + two(c.get(java.util.Calendar.SECOND));
    }

    /**
     * MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,
     * AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
     */
    public String toCsv(double lat, double lon, double alt, double acc) {
        StringBuilder b = new StringBuilder(160);
        b.append(mac).append(',');
        b.append(clean(name)).append(',');
        b.append(clean(auth)).append(',');
        b.append(stamp(time)).append(',');
        b.append(channel).append(',');
        if (type == WIFI) b.append(freq);
        b.append(',');
        b.append(rssi).append(',');
        b.append(String.format(Locale.US, "%.6f,%.6f,%.1f,%.1f", lat, lon, alt, acc));
        b.append(",,");
        if (type == BLE && mfgr >= 0) b.append(mfgr);
        b.append(',');
        b.append(typeName());
        b.append('\n');
        return b.toString();
    }

    /** Pack a MAC like aa:bb:cc:dd:ee:ff into a long, or -1. Cell keys are hashed. */
    public static long key(char type, String mac) {
        if (mac == null) return -1;
        if (type == CELL) return 0x7F00000000000000L ^ (mac.hashCode() & 0xFFFFFFFFL) ^ ((long) mac.length() << 40);
        long v = 0;
        int digits = 0;
        for (int i = 0; i < mac.length(); i++) {
            int d = Character.digit(mac.charAt(i), 16);
            if (d < 0) continue;
            v = (v << 4) | d;
            digits++;
        }
        if (digits != 12) return mac.hashCode();
        long tag = type == WIFI ? 1L : type == BLE ? 2L : 3L;
        return v | (tag << 52);
    }
}
