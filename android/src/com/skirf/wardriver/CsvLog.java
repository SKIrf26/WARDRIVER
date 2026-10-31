package com.skirf.wardriver;

import android.os.Build;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** Writes the WiGLE CSV for a run on the I/O thread. Rows are flushed to disk every 3 seconds. */
public final class CsvLog {
    private BufferedWriter out;
    private File file;
    private final List<File> files = new ArrayList<File>();
    private volatile int rowsInFile;
    private volatile int rowsTotal;
    private volatile String error = "";
    private long fileStart;
    private String prefix = "SKIrf";
    private boolean flushQueued;

    public synchronized String fileName() { return file == null ? "" : file.getName(); }
    public int rows() { return rowsTotal; }
    public String error() { return error; }
    public synchronized List<File> files() { return new ArrayList<File>(files); }

    public static String header() {
        return "WigleWifi-1.6,appRelease=" + BuildInfo.APP_RELEASE
                + ",model=" + Sighting.clean(Build.MODEL)
                + ",release=" + Sighting.clean(Build.VERSION.RELEASE)
                + ",device=" + Sighting.clean(Build.DEVICE)
                + ",display=" + Sighting.clean(Build.DISPLAY)
                + ",board=" + Sighting.clean(Build.BOARD)
                + ",brand=" + Sighting.clean(Build.BRAND)
                + ",star=Sol,body=3,subBody=0\n"
                + "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n";
    }

    /** Opens the first file of a run (on the I/O thread). */
    public void start(final String pfx) {
        rowsTotal = 0;
        error = "";
        synchronized (this) { files.clear(); }
        App.io.post(new Runnable() {
            @Override public void run() {
                prefix = cleanPrefix(pfx);
                openNew();
            }
        });
    }

    static String cleanPrefix(String p) {
        StringBuilder b = new StringBuilder();
        if (p != null) for (int i = 0; i < p.length() && b.length() < 24; i++) {
            char c = p.charAt(i);
            if (Character.isLetterOrDigit(c) || c == '-' || c == '_') b.append(c);
        }
        return b.length() == 0 ? "SKIrf" : b.toString();
    }

    private void openNew() {
        closeFile();
        try {
            String stamp = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
            File f = new File(App.logDir(), prefix + "_" + stamp + ".csv");
            int n = 2;
            while (f.exists()) f = new File(App.logDir(), prefix + "_" + stamp + "_" + (n++) + ".csv");
            BufferedWriter w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(f), Charset.forName("UTF-8")), 65536);
            w.write(header());
            w.flush();
            synchronized (this) {
                out = w;
                file = f;
                files.add(f);
            }
            rowsInFile = 0;
            fileStart = System.currentTimeMillis();
            error = "";
        } catch (Throwable t) {
            error = "Can't create log file: " + t.getMessage();
        }
    }

    /** Queue one finished CSV line. */
    public void append(final String line) {
        App.io.post(new Runnable() {
            @Override public void run() { write(line); }
        });
    }

    private void write(String line) {
        int split = Prefs.i(Prefs.SPLIT);
        if (out != null && rowsInFile > 0) {
            boolean timeUp = split > 0 && System.currentTimeMillis() - fileStart >= split * 60000L;
            boolean full = split < 0 && rowsInFile >= 50000;
            if (timeUp || full) openNew();
        }
        if (out == null) {
            if (error.length() == 0) error = "Log file is not open";
            return;
        }
        try {
            out.write(line);
            rowsInFile++;
            rowsTotal++;
            if (!flushQueued) {
                flushQueued = true;
                App.io.postDelayed(new Runnable() {
                    @Override public void run() {
                        flushQueued = false;
                        flush();
                    }
                }, 3000);
            }
        } catch (Throwable t) {
            error = "Write failed: " + t.getMessage();
        }
    }

    private void flush() {
        try {
            synchronized (this) { if (out != null) out.flush(); }
        } catch (Throwable t) {
            error = "Write failed: " + t.getMessage();
        }
    }

    private void closeFile() {
        synchronized (this) {
            if (out != null) {
                try { out.flush(); out.close(); } catch (Throwable ignored) { }
                out = null;
                // a file with only the header is useless: remove it
                if (file != null && rowsInFile == 0) {
                    files.remove(file);
                    file.delete();
                }
            }
        }
    }

    /** Closes the run; `done` runs on the main thread with the files that hold rows. */
    public void stop(final Runnable done) {
        App.io.post(new Runnable() {
            @Override public void run() {
                closeFile();
                synchronized (CsvLog.this) { file = null; }
                if (done != null) App.main.post(done);
            }
        });
    }

    /** Called from the crash handler: push buffered rows to disk. */
    public void emergencyFlush() {
        try {
            synchronized (this) { if (out != null) out.flush(); }
        } catch (Throwable ignored) { }
    }
}
