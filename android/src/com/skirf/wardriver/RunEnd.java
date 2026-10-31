package com.skirf.wardriver;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;

import java.io.File;
import java.util.List;

/** What happens after a run stops: copy to Downloads, auto-upload, and a summary notification. */
public final class RunEnd {
    public static String lastSummary = "";

    public static boolean online(Context c, boolean wifiOnly) {
        ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) return false;
        Network n = cm.getActiveNetwork();
        if (n == null) return false;
        NetworkCapabilities nc = cm.getNetworkCapabilities(n);
        if (nc == null || !nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) return false;
        return !wifiOnly || nc.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) || nc.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET);
    }

    static void finished(final Context c, final List<File> files, final int rows, long ms) {
        if (c == null) return;
        String name = files.isEmpty() ? "" : files.get(files.size() - 1).getName();
        lastSummary = rows == 0 ? "Run ended - nothing was logged"
                : Fmt.num(rows) + " rows saved" + (files.size() > 1 ? " in " + files.size() + " files" : " to " + name);
        notify(c, "Run saved", lastSummary + " (" + Fmt.dur(ms) + ")");
        if (files.isEmpty()) return;

        if (Prefs.b(Prefs.COPY_DL)) {
            App.io.post(new Runnable() {
                @Override public void run() {
                    for (File f : files) LogStore.copyToDownloads(f);
                }
            });
        }
        int auto = Prefs.i(Prefs.AUTO_UP);
        int sites = 0;
        if ((auto == 1 || auto == 3) && Prefs.hasWigle()) sites |= 1;
        if ((auto == 2 || auto == 3) && Prefs.hasWdg()) sites |= 2;
        if (sites == 0) return;
        if (!online(c, Prefs.b(Prefs.UP_WIFI_ONLY))) {
            notify(c, "Upload waiting", Prefs.b(Prefs.UP_WIFI_ONLY)
                    ? "Not on Wi-Fi. Open Logs and tap Upload new when you are."
                    : "No internet. Open Logs and tap Upload new later.");
            return;
        }
        Uploader.uploadMany(files, sites, false, new Uploader.Progress() {
            @Override public void step(String status, int index, int total) { }
            @Override public void finished(int ok, int failed, String lastError) {
                if (failed == 0) RunEnd.notify(c, "Uploaded", ok + " upload" + (ok == 1 ? "" : "s") + " finished");
                else RunEnd.notify(c, "Upload problem", failed + " failed. " + lastError);
            }
        });
    }

    static void notify(Context c, String title, String text) {
        NotificationManager nm = (NotificationManager) c.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm == null) return;
        Intent open = new Intent(c, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pi = PendingIntent.getActivity(c, 2, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification n = new Notification.Builder(c, App.CH_ALERT)
                .setSmallIcon(R.drawable.ic_stat)
                .setContentTitle(title)
                .setContentText(text)
                .setStyle(new Notification.BigTextStyle().bigText(text))
                .setColor(Theme.ACC)
                .setContentIntent(pi)
                .setAutoCancel(true)
                .build();
        try { nm.notify((int) (System.currentTimeMillis() & 0xFFFF) + 100, n); } catch (Throwable ignored) { }
    }

    private RunEnd() {}
}
