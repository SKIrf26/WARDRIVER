package com.skirf.wardriver;

import java.util.Locale;

/** A few well-known Bluetooth company IDs, for nicer names in the live list. */
final class Vendors {
    static String ble(int id) {
        switch (id) {
            case 0x0000: return "Ericsson";
            case 0x0001: return "Nokia";
            case 0x0002: return "Intel";
            case 0x0006: return "Microsoft";
            case 0x000D: return "Texas Instruments";
            case 0x000F: return "Broadcom";
            case 0x004C: return "Apple";
            case 0x0059: return "Nordic";
            case 0x0075: return "Samsung";
            case 0x0087: return "Garmin";
            case 0x00E0: return "Google";
            default: return String.format(Locale.US, "Mfr 0x%04X", id);
        }
    }

    private Vendors() {}
}
