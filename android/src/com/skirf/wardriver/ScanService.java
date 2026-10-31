package com.skirf.wardriver;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;

/** Keeps the run alive with the screen off, and shows live counts in the notification. */
public class ScanService extends Service {
    static final String STOP = "com.skirf.wardriver.STOP";
    private static final int NID = 1;
    private Notification.Builder nb;
    private boolean fg;

    public static void start(Context c) {
        Intent i = new Intent(c, ScanService.class);
        c.startForegroundService(i);
    }

    public static void stop(Context c) {
        ScanEngine.get().stop();
        c.stopService(new Intent(c, ScanService.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && STOP.equals(intent.getAction())) {
            ScanEngine.get().stop();
            stopForegroundCompat();
            stopSelf();
            return START_NOT_STICKY;
        }
        if (!fg) {
            Notification n = build("Starting...", "Waiting for GPS");
            try {
                if (Build.VERSION.SDK_INT >= 29) startForeground(NID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION);
                else startForeground(NID, n);
                fg = true;
            } catch (Throwable t) {
                // Android said no (usually location permission missing): don't run half-blind
                ScanEngine.get().stop();
                stopSelf();
                return START_NOT_STICKY;
            }
        }
        ScanEngine.get().start(this);
        App.main.removeCallbacks(update);
        App.main.postDelayed(update, 2000);
        return START_NOT_STICKY;
    }

    private Notification build(String title, String text) {
        if (nb == null) {
            Intent open = new Intent(this, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
            PendingIntent pi = PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
            Intent stop = new Intent(this, ScanService.class).setAction(STOP);
            PendingIntent ps = PendingIntent.getService(this, 1, stop, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
            nb = new Notification.Builder(this, App.CH_RUN)
                    .setSmallIcon(R.drawable.ic_stat)
                    .setOngoing(true)
                    .setOnlyAlertOnce(true)
                    .setColor(Theme.ACC)
                    .setContentIntent(pi)
                    .setCategory(Notification.CATEGORY_SERVICE)
                    .addAction(new Notification.Action.Builder(null, "Stop run", ps).build());
            if (Build.VERSION.SDK_INT >= 31) nb.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
        }
        nb.setContentTitle(title).setContentText(text);
        return nb.build();
    }

    private final Runnable update = new Runnable() {
        @Override public void run() {
            ScanEngine e = ScanEngine.get();
            if (!e.running) return;
            StringBuilder t = new StringBuilder();
            t.append("Wi-Fi ").append(Fmt.num(e.uniq[0]));
            if (Prefs.b(Prefs.BLE) || e.uniq[1] > 0) t.append("  ·  BLE ").append(Fmt.num(e.uniq[1]));
            if (Prefs.b(Prefs.BT) || e.uniq[2] > 0) t.append("  ·  BT ").append(Fmt.num(e.uniq[2]));
            if (Prefs.b(Prefs.CELL) || e.uniq[3] > 0) t.append("  ·  Cell ").append(Fmt.num(e.uniq[3]));
            String gps = e.goodFix() ? e.satsUsed + " sats" : "no GPS fix";
            String title = "Wardriving " + Fmt.dur(e.elapsed()) + "  ·  " + gps;
            NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            try { if (nm != null) nm.notify(NID, build(title, t.toString())); } catch (Throwable ignored) { }
            App.main.postDelayed(this, 2000);
        }
    };

    private void stopForegroundCompat() {
        if (Build.VERSION.SDK_INT >= 24) stopForeground(STOP_FOREGROUND_REMOVE);
        fg = false;
    }

    @Override
    public void onDestroy() {
        App.main.removeCallbacks(update);
        ScanEngine.get().stop();
        fg = false;
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }
}
