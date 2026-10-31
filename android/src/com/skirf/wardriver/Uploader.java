package com.skirf.wardriver;

import android.util.Base64;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.List;

/** WiGLE and WDGWars: uploads and account stats. All network work runs on App.net. */
public final class Uploader {
    public static final class Result {
        public boolean ok;
        public String msg = "";
        public JSONObject json;
    }

    public interface Done { void done(Result r); }
    public interface Progress { void step(String status, int index, int total); void finished(int ok, int failed, String lastError); }

    public static volatile boolean busy;
    public static volatile String status = "";

    // cached stats for the Logs screen
    public static volatile JSONObject wigleStats, wdgStats;
    public static volatile String wigleStatsErr = "", wdgStatsErr = "";
    public static volatile long wigleStatsAt, wdgStatsAt;

    private static final String UA = "SKIrf-Wardriver-Android/" + BuildInfo.APP_RELEASE;

    // ------------------------------------------------------------------
    private static String wigleAuth() {
        String raw = Prefs.s(Prefs.WIGLE_NAME) + ":" + Prefs.s(Prefs.WIGLE_TOKEN);
        return "Basic " + Base64.encodeToString(raw.getBytes(), Base64.NO_WRAP);
    }

    private static String readAll(InputStream in) throws java.io.IOException {
        if (in == null) return "";
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) > 0 && b.size() < 1 << 20) b.write(buf, 0, n);
        in.close();
        return b.toString("UTF-8");
    }

    private static String reason(int code, String body) {
        String msg = "";
        try {
            JSONObject j = new JSONObject(body);
            msg = j.optString("message", j.optString("error", ""));
        } catch (Throwable ignored) { }
        if (msg.length() == 0) {
            switch (code) {
                case 401: msg = "Key not accepted (401) - check the API name and key"; break;
                case 403: msg = "Not allowed (403) - check the key"; break;
                case 413: msg = "File too big for the server (413)"; break;
                case 429: msg = "Too many requests (429) - try again later"; break;
                default: msg = "Server answered " + code;
            }
        } else msg = msg + " (" + code + ")";
        return msg;
    }

    private static String netError(Throwable t) {
        String m = t.getMessage();
        if (t instanceof java.net.UnknownHostException) return "No internet (can't find the server)";
        if (t instanceof java.net.SocketTimeoutException) return "No reply from server (timed out)";
        return m == null ? t.getClass().getSimpleName() : m;
    }

    // ------------------------------------------------------------------
    public static Result uploadWigle(File f) {
        Result r = new Result();
        if (!Prefs.hasWigle()) { r.msg = "WiGLE API name and key are not set"; return r; }
        try {
            HttpURLConnection c = multipart("https://api.wigle.net/api/v2/file/upload", f, wigleAuth(), null);
            int code = c.getResponseCode();
            String body = readAll(code >= 400 ? c.getErrorStream() : c.getInputStream());
            c.disconnect();
            if (code >= 200 && code < 300) {
                JSONObject j = null;
                try { j = new JSONObject(body); } catch (Throwable ignored) { }
                if (j == null || j.optBoolean("success", true)) {
                    r.ok = true;
                    r.msg = "Uploaded to WiGLE";
                    r.json = j;
                } else {
                    r.msg = j.optString("message", "WiGLE said no");
                }
            } else r.msg = reason(code, body);
        } catch (Throwable t) {
            r.msg = netError(t);
        }
        return r;
    }

    public static Result uploadWdg(File f) {
        Result r = new Result();
        if (!Prefs.hasWdg()) { r.msg = "WDGWars API key is not set"; return r; }
        try {
            HttpURLConnection c = multipart("https://wdgwars.pl/api/upload-csv", f, null, Prefs.s(Prefs.WDG_KEY));
            int code = c.getResponseCode();
            String body = readAll(code >= 400 ? c.getErrorStream() : c.getInputStream());
            c.disconnect();
            if (code >= 200 && code < 300) { r.ok = true; r.msg = "Uploaded to WDGWars"; }
            else if (code == 409) { r.ok = true; r.msg = "WDGWars already has this file"; }
            else r.msg = reason(code, body);
        } catch (Throwable t) {
            r.msg = netError(t);
        }
        return r;
    }

    private static HttpURLConnection multipart(String url, File f, String auth, String apiKey) throws Exception {
        String boundary = "----SKIrf" + Long.toHexString(System.nanoTime());
        byte[] head = ("--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"file\"; filename=\"" + f.getName() + "\"\r\n"
                + "Content-Type: text/csv\r\n\r\n").getBytes("UTF-8");
        byte[] tail = ("\r\n--" + boundary + "--\r\n").getBytes("UTF-8");
        long len = head.length + f.length() + tail.length;
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(20000);
        c.setReadTimeout(180000);
        c.setDoOutput(true);
        c.setRequestMethod("POST");
        c.setFixedLengthStreamingMode(len);
        c.setRequestProperty("User-Agent", UA);
        c.setRequestProperty("Accept", "application/json");
        c.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
        if (auth != null) c.setRequestProperty("Authorization", auth);
        if (apiKey != null) c.setRequestProperty("X-API-Key", apiKey);
        OutputStream out = c.getOutputStream();
        out.write(head);
        InputStream in = new FileInputStream(f);
        byte[] buf = new byte[32768];
        int n;
        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        in.close();
        out.write(tail);
        out.flush();
        out.close();
        return c;
    }

    private static Result getJson(String url, String auth, String apiKey) {
        Result r = new Result();
        try {
            HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(15000);
            c.setReadTimeout(30000);
            c.setRequestProperty("User-Agent", UA);
            c.setRequestProperty("Accept", "application/json");
            if (auth != null) c.setRequestProperty("Authorization", auth);
            if (apiKey != null) c.setRequestProperty("X-API-Key", apiKey);
            int code = c.getResponseCode();
            String body = readAll(code >= 400 ? c.getErrorStream() : c.getInputStream());
            c.disconnect();
            if (code >= 200 && code < 300) {
                r.json = new JSONObject(body);
                r.ok = true;
            } else r.msg = reason(code, body);
        } catch (Throwable t) {
            r.msg = netError(t);
        }
        return r;
    }

    // ------------------------------------------------------------------
    public static void fetchWigleStats(final Done d) {
        App.net.post(new Runnable() {
            @Override public void run() {
                final Result r;
                if (!Prefs.hasWigle()) { r = new Result(); r.msg = "Add your WiGLE API name and key in Settings"; }
                else {
                    r = getJson("https://api.wigle.net/api/v2/stats/user", wigleAuth(), null);
                    if (r.ok && r.json != null && !r.json.optBoolean("success", true)) {
                        r.ok = false;
                        r.msg = r.json.optString("message", "WiGLE refused the key");
                    }
                }
                if (r.ok) { wigleStats = r.json; wigleStatsErr = ""; wigleStatsAt = System.currentTimeMillis(); }
                else wigleStatsErr = r.msg;
                if (d != null) App.main.post(new Runnable() { @Override public void run() { d.done(r); } });
            }
        });
    }

    public static void fetchWdgStats(final Done d) {
        App.net.post(new Runnable() {
            @Override public void run() {
                final Result r;
                if (!Prefs.hasWdg()) { r = new Result(); r.msg = "Add your WDGWars API key in Settings"; }
                else {
                    r = getJson("https://wdgwars.pl/api/me", null, Prefs.s(Prefs.WDG_KEY));
                    // the board's fix: success is "ok": true OR a username in the reply
                    if (r.ok && r.json != null && !r.json.optBoolean("ok", false) && !r.json.has("username")) {
                        r.ok = false;
                        r.msg = r.json.optString("error", "WDGWars refused the key");
                    }
                }
                if (r.ok) { wdgStats = r.json; wdgStatsErr = ""; wdgStatsAt = System.currentTimeMillis(); }
                else wdgStatsErr = r.msg;
                if (d != null) App.main.post(new Runnable() { @Override public void run() { d.done(r); } });
            }
        });
    }

    /**
     * Upload files to the chosen sites one after another. sites: bit 1 = WiGLE, bit 2 = WDGWars.
     * Files already sent to a site are skipped unless force is set.
     */
    public static void uploadMany(final List<File> files, final int sites, final boolean force, final Progress p) {
        if (busy) {
            if (p != null) p.finished(0, 0, "An upload is already running");
            return;
        }
        busy = true;
        App.net.post(new Runnable() {
            @Override public void run() {
                int ok = 0, bad = 0;
                String lastErr = "";
                int total = files.size() * ((sites & 1) != 0 && (sites & 2) != 0 ? 2 : 1);
                int idx = 0;
                for (File f : files) {
                    if (ScanEngine.get().running && f.getName().equals(ScanEngine.get().csv.fileName())) continue;
                    for (int site = 1; site <= 2; site++) {
                        if ((sites & site) == 0) continue;
                        char tag = site == 1 ? 'w' : 'd';
                        idx++;
                        if (!force && Prefs.uploaded(tag, f.getName())) continue;
                        final String st = (site == 1 ? "WiGLE" : "WDGWars") + ": " + f.getName();
                        status = st;
                        final int fi = idx, ft = total;
                        if (p != null) App.main.post(new Runnable() { @Override public void run() { p.step(st, fi, ft); } });
                        Result r = site == 1 ? uploadWigle(f) : uploadWdg(f);
                        if (r.ok) { ok++; Prefs.markUploaded(tag, f.getName(), true); }
                        else { bad++; lastErr = (site == 1 ? "WiGLE: " : "WDGWars: ") + r.msg; }
                    }
                }
                busy = false;
                status = "";
                final int fo = ok, fb = bad;
                final String fe = lastErr;
                if (p != null) App.main.post(new Runnable() { @Override public void run() { p.finished(fo, fb, fe); } });
            }
        });
    }

    private Uploader() {}
}
