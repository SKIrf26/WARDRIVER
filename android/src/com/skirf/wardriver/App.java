package com.skirf.wardriver;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;

public class App extends Application {
    public static final String CH_RUN = "run";
    public static final String CH_ALERT = "alert";
    public static App self;
    public static Handler main, io, net;

    @Override
    public void onCreate() {
        super.onCreate();
        self = this;
        Prefs.init(this);
        Theme.load(this);
        main = new Handler(Looper.getMainLooper());
        HandlerThread t = new HandlerThread("skirf-io");
        t.start();
        io = new Handler(t.getLooper());
        HandlerThread n = new HandlerThread("skirf-net");
        n.start();
        net = new Handler(n.getLooper());
        installCrashLog();
        makeChannels();
    }

    /** Where run logs live: Android/data/com.skirf.wardriver/files/logs */
    public static File logDir() {
        File d = self.getExternalFilesDir("logs");
        if (d == null) d = new File(self.getFilesDir(), "logs");
        if (!d.exists()) d.mkdirs();
        return d;
    }

    public static File crashFile() { return new File(self.getFilesDir(), "last_crash.txt"); }

    private void installCrashLog() {
        final Thread.UncaughtExceptionHandler prev = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread th, Throwable e) {
                try {
                    // keep whatever was logged so far
                    ScanEngine.get().emergencyFlush();
                } catch (Throwable ignored) { }
                try {
                    StringWriter sw = new StringWriter();
                    PrintWriter pw = new PrintWriter(sw);
                    pw.println("SKIrf Wardriver " + BuildInfo.VERSION + " on " + Build.MANUFACTURER + " " + Build.MODEL
                            + ", Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")");
                    pw.println("Thread: " + th.getName() + "  Time: " + new java.util.Date());
                    e.printStackTrace(pw);
                    pw.flush();
                    FileWriter fw = new FileWriter(crashFile());
                    fw.write(sw.toString());
                    fw.close();
                } catch (Throwable ignored) { }
                if (prev != null) prev.uncaughtException(th, e);
            }
        });
    }

    private void makeChannels() {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm == null) return;
        NotificationChannel run = new NotificationChannel(CH_RUN, "Wardrive running", NotificationManager.IMPORTANCE_LOW);
        run.setDescription("Shows while a run is logging, with live counts and a Stop button.");
        run.setShowBadge(false);
        nm.createNotificationChannel(run);
        NotificationChannel al = new NotificationChannel(CH_ALERT, "Run finished and uploads", NotificationManager.IMPORTANCE_DEFAULT);
        al.setDescription("Run summaries and upload results.");
        nm.createNotificationChannel(al);
    }
}
