package com.skirf.wardriver;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;

/** Colours and fonts, matching the board and its web portal. */
public final class Theme {
    public static final String[] ACCENT_NAMES = {"Mint", "Ice", "Gold", "Ember", "Siren", "Neon", "Cobalt", "Chrome", "WDGW", "Pro"};
    public static final int[] ACCENTS = {
            0xFF40F096, 0xFF40D0FF, 0xFFFFD246, 0xFFFF8A3C, 0xFFFF4860,
            0xFFEC60E8, 0xFF688CFF, 0xFFE2E6EE, 0xFFFF20A0, 0xFF3FBF3F};
    public static final int WDGW = 8, PRO = 9;

    // Surface tokens (changed by the theme)
    public static int BG, CARD, CARD2, LINE, TEXT, MUTE, DIM, ACC, ON_ACC, HI;
    // Meaning colours (fixed)
    public static final int W24 = 0xFF50AAFF;   // 2.4 GHz
    public static final int W5 = 0xFFBA80FF;    // 5 GHz
    public static final int W6 = 0xFFFF7AC8;    // 6 GHz
    public static final int BLE = 0xFF00DCC8;   // Bluetooth LE
    public static final int BT = 0xFF7A9CFF;    // Bluetooth classic
    public static final int CELL = 0xFFFFA800;  // cell towers
    public static final int OK = 0xFF40F096;
    public static final int WARN = 0xFFFFBE5A;
    public static final int BAD = 0xFFF05C68;
    public static final int NEW = 0xFFFFB547;

    public static Typeface MED, SEMI, BOLD, MONO;
    public static int accentIndex;

    public static void load(Context c) {
        try {
            MED = Typeface.createFromAsset(c.getAssets(), "fonts/SairaSemiCondensed-Medium.ttf");
            SEMI = Typeface.createFromAsset(c.getAssets(), "fonts/SairaSemiCondensed-SemiBold.ttf");
            BOLD = Typeface.createFromAsset(c.getAssets(), "fonts/SairaSemiCondensed-Bold.ttf");
        } catch (Throwable t) {
            MED = Typeface.DEFAULT;
            SEMI = Typeface.DEFAULT_BOLD;
            BOLD = Typeface.DEFAULT_BOLD;
        }
        MONO = Typeface.MONOSPACE;
        apply(Prefs.i(Prefs.ACCENT));
    }

    public static void apply(int idx) {
        if (idx < 0 || idx >= ACCENTS.length) idx = 0;
        accentIndex = idx;
        ACC = ACCENTS[idx];
        ON_ACC = 0xFF000000;
        if (idx == PRO) {
            BG = 0xFF050A05; CARD = 0xFF0D170D; CARD2 = 0xFF152415; LINE = 0xFF1F331F;
            TEXT = 0xFFC6C4B0; MUTE = 0xFF80896E; DIM = 0xFF4A5640; HI = 0xFFC6C4B0;
        } else {
            BG = 0xFF000000; CARD = 0xFF13161D; CARD2 = 0xFF1E232D; LINE = 0xFF252A35;
            TEXT = 0xFFEAEEF3; MUTE = 0xFF808998; DIM = 0xFF4A515E;
            HI = idx == WDGW ? 0xFF00FFFF : TEXT;
        }
    }

    /** Colour with a new alpha (0-255). */
    public static int a(int color, int alpha) {
        return (color & 0x00FFFFFF) | (Math.max(0, Math.min(255, alpha)) << 24);
    }

    public static int mix(int c1, int c2, float t) {
        int r = (int) (Color.red(c1) + (Color.red(c2) - Color.red(c1)) * t);
        int g = (int) (Color.green(c1) + (Color.green(c2) - Color.green(c1)) * t);
        int b = (int) (Color.blue(c1) + (Color.blue(c2) - Color.blue(c1)) * t);
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    public static int typeColor(char t) {
        switch (t) {
            case Sighting.BLE: return BLE;
            case Sighting.BT: return BT;
            case Sighting.CELL: return CELL;
            default: return W24;
        }
    }

    private Theme() {}
}
