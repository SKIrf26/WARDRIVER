package com.skirf.wardriver;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;

/** The run logs on the phone: listing, counting, exporting and deleting. */
public final class LogStore {

    public static final class Info {
        public File file;
        public String name;
        public long size, modified;
        public int rows = -1;                    // -1 until counted
        public int[] byType = new int[4];        // W, B, C, T
        public boolean wigle, wdg;
    }

    private static final HashMap<String, Info> counted = new HashMap<String, Info>();

    public static List<Info> list() {
        File[] fs = App.logDir().listFiles();
        ArrayList<Info> out = new ArrayList<Info>();
        if (fs == null) return out;
        Arrays.sort(fs, new Comparator<File>() {
            @Override public int compare(File a, File b) { return Long.compare(b.lastModified(), a.lastModified()); }
        });
        String live = ScanEngine.get().running ? ScanEngine.get().csv.fileName() : "";
        for (File f : fs) {
            if (!f.isFile() || !f.getName().toLowerCase().endsWith(".csv")) continue;
            Info i = new Info();
            i.file = f;
            i.name = f.getName();
            i.size = f.length();
            i.modified = f.lastModified();
            i.wigle = Prefs.uploaded('w', i.name);
            i.wdg = Prefs.uploaded('d', i.name);
            synchronized (counted) {
                Info c = counted.get(i.name);
                if (c != null && c.size == i.size && !i.name.equals(live)) {
                    i.rows = c.rows;
                    i.byType = c.byType;
                }
            }
            out.add(i);
        }
        return out;
    }

    public static boolean isLive(Info i) {
        ScanEngine e = ScanEngine.get();
        return e.running && i.name.equals(e.csv.fileName());
    }

    /** Count rows by type (I/O thread). */
    public static void count(Info i) {
        int rows = 0;
        int[] t = new int[4];
        BufferedReader r = null;
        try {
            r = new BufferedReader(new InputStreamReader(new FileInputStream(i.file), "UTF-8"), 65536);
            String line;
            int n = 0;
            while ((line = r.readLine()) != null) {
                n++;
                if (n <= 2 || line.length() < 10) continue;
                rows++;
                int c = line.lastIndexOf(',');
                String type = c >= 0 ? line.substring(c + 1).trim() : "";
                if (type.equals("WIFI")) t[0]++;
                else if (type.equals("BLE")) t[1]++;
                else if (type.equals("BT")) t[2]++;
                else t[3]++;
            }
        } catch (Throwable ignored) {
        } finally {
            try { if (r != null) r.close(); } catch (Throwable ignored) { }
        }
        i.rows = rows;
        i.byType = t;
        synchronized (counted) { counted.put(i.name, i); }
    }

    public static boolean delete(Info i) {
        if (isLive(i)) return false;
        boolean ok = i.file.delete();
        if (ok) {
            Prefs.forgetFile(i.name);
            synchronized (counted) { counted.remove(i.name); }
        }
        return ok;
    }

    /** Copy a log into Downloads/SKIrf Wardriver. Returns null when it worked, or the reason. */
    public static String copyToDownloads(File f) {
        InputStream in = null;
        OutputStream out = null;
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                ContentResolver cr = App.self.getContentResolver();
                ContentValues v = new ContentValues();
                v.put(MediaStore.MediaColumns.DISPLAY_NAME, f.getName());
                v.put(MediaStore.MediaColumns.MIME_TYPE, "text/csv");
                v.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/SKIrf Wardriver");
                Uri u = cr.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                if (u == null) return "Android refused the new file";
                out = cr.openOutputStream(u);
            } else {
                File dir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "SKIrf Wardriver");
                if (!dir.exists() && !dir.mkdirs()) return "Can't make the Downloads folder (storage permission?)";
                out = new FileOutputStream(new File(dir, f.getName()));
            }
            if (out == null) return "Android refused the new file";
            in = new FileInputStream(f);
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            out.flush();
            return null;
        } catch (Throwable t) {
            return t.getMessage() == null ? t.toString() : t.getMessage();
        } finally {
            try { if (in != null) in.close(); } catch (Throwable ignored) { }
            try { if (out != null) out.close(); } catch (Throwable ignored) { }
        }
    }

    /** Positions in a log, for the map: [lat, lon, typeIndex] triples. */
    public static double[] points(File f, int max) {
        double[] p = new double[Math.min(max, 4096) * 3];
        int n = 0;
        BufferedReader r = null;
        try {
            r = new BufferedReader(new InputStreamReader(new FileInputStream(f), "UTF-8"), 65536);
            String line;
            int ln = 0;
            while ((line = r.readLine()) != null && n / 3 < max) {
                if (++ln <= 2) continue;
                String[] c = line.split(",", -1);
                if (c.length < 14) continue;
                double lat, lon;
                try { lat = Double.parseDouble(c[7]); lon = Double.parseDouble(c[8]); } catch (Throwable t) { continue; }
                if (lat == 0 && lon == 0) continue;
                String ty = c[13].trim();
                int ti = ty.equals("WIFI") ? 0 : ty.equals("BLE") ? 1 : ty.equals("BT") ? 2 : 3;
                if (n + 3 > p.length) p = Arrays.copyOf(p, p.length * 2);
                p[n++] = lat; p[n++] = lon; p[n++] = ti;
            }
        } catch (Throwable ignored) {
        } finally {
            try { if (r != null) r.close(); } catch (Throwable ignored) { }
        }
        return Arrays.copyOf(p, n);
    }

    public static String size(long b) {
        if (b < 1024) return b + " B";
        if (b < 1024 * 1024) return String.format(java.util.Locale.US, "%.1f KB", b / 1024.0);
        return String.format(java.util.Locale.US, "%.1f MB", b / 1048576.0);
    }

    private LogStore() {}
}
