package com.skirf.wardriver;

import java.util.Locale;

/** Number, time and unit formatting. */
public final class Fmt {
    public static String num(long v) { return String.format(Locale.US, "%,d", v); }

    public static String dur(long ms) {
        long s = Math.max(0, ms / 1000);
        long h = s / 3600, m = (s / 60) % 60, sec = s % 60;
        if (h > 0) return String.format(Locale.US, "%d:%02d:%02d", h, m, sec);
        return String.format(Locale.US, "%d:%02d", m, sec);
    }

    public static String dist(double m) {
        if (Prefs.imperial()) {
            double mi = m / 1609.344;
            return mi < 0.1 ? String.format(Locale.US, "%.0f ft", m * 3.28084) : String.format(Locale.US, "%.1f mi", mi);
        }
        return m < 1000 ? String.format(Locale.US, "%.0f m", m) : String.format(Locale.US, "%.1f km", m / 1000);
    }

    public static String speed(float mps) {
        if (Prefs.imperial()) return String.format(Locale.US, "%.0f", mps * 2.23694f);
        return String.format(Locale.US, "%.0f", mps * 3.6f);
    }

    public static String speedUnit() { return Prefs.imperial() ? "mph" : "km/h"; }

    public static String alt(double m) {
        if (Prefs.imperial()) return String.format(Locale.US, "%.0f", m * 3.28084);
        return String.format(Locale.US, "%.0f", m);
    }

    public static String altUnit() { return Prefs.imperial() ? "ft" : "m"; }

    public static String acc(float m) {
        if (Prefs.imperial()) return String.format(Locale.US, "±%.0f ft", m * 3.28084);
        return String.format(Locale.US, "±%.0f m", m);
    }

    public static String coord(double v, boolean lat) {
        char h = lat ? (v >= 0 ? 'N' : 'S') : (v >= 0 ? 'E' : 'W');
        return String.format(Locale.US, "%.6f° %c", Math.abs(v), h);
    }

    public static String compass(float deg) {
        String[] d = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        return d[(int) Math.floor(((deg % 360) + 382.5) / 45) % 8];
    }

    public static String ago(long ms) {
        long s = ms / 1000;
        if (s < 60) return "just now";
        if (s < 3600) return (s / 60) + " min ago";
        if (s < 86400) return (s / 3600) + " h ago";
        return (s / 86400) + " d ago";
    }

    private Fmt() {}
}
