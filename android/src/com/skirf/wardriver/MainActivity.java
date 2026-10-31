package com.skirf.wardriver;

import android.Manifest;
import android.app.Activity;
import android.app.Dialog;
import android.bluetooth.BluetoothAdapter;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import static com.skirf.wardriver.Ui.dp;

public class MainActivity extends Activity {
    public static final int TAB_DRIVE = 0, TAB_MAP = 1, TAB_GPS = 2, TAB_LOGS = 3, TAB_SETTINGS = 4;
    private static final int REQ_PERMS = 1;

    private FrameLayout rootFrame, content;
    private LinearLayout nav;
    private final Screen[] screens = new Screen[5];
    public MapScreen map;
    private int tab = -1;
    private TextView gpsPill, recPill;
    private View welcome;
    private boolean resumed, pendingStart, autoStartDone;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Ui.init(this);
        buildShell(Prefs.i(Prefs.LAST_TAB));
        if (!Prefs.b(Prefs.ONBOARDED)) showWelcome();
        else if (App.crashFile().exists()) App.main.postDelayed(new Runnable() { @Override public void run() { showCrash(); } }, 600);
    }

    // ======================================================================
    //  Shell: header, content, bottom bar
    // ======================================================================
    private void buildShell(int startTab) {
        Window w = getWindow();
        w.setStatusBarColor(Theme.BG);
        w.setNavigationBarColor(Theme.BG);
        rootFrame = new FrameLayout(this);
        rootFrame.setBackgroundColor(Theme.BG);
        LinearLayout col = Ui.v();
        col.addView(header(), Ui.MW);
        content = new FrameLayout(this);
        col.addView(content, new LinearLayout.LayoutParams(-1, 0, 1f));
        nav = navBar();
        col.addView(nav, Ui.MW);
        rootFrame.addView(col, new FrameLayout.LayoutParams(-1, -1));
        FrameLayout toastLayer = new FrameLayout(this);
        rootFrame.addView(toastLayer, new FrameLayout.LayoutParams(-1, -1));
        Ui.toastLayer = toastLayer;
        setContentView(rootFrame);

        screens[TAB_DRIVE] = new DriveScreen(this);
        screens[TAB_MAP] = map = new MapScreen(this);
        screens[TAB_GPS] = new GpsScreen(this);
        screens[TAB_LOGS] = new LogsScreen(this);
        screens[TAB_SETTINGS] = new SettingsScreen(this);
        tab = -1;
        showTab(Math.max(0, Math.min(4, startTab)));
    }

    /** Rebuild every screen (after a theme change). */
    public void rebuildUi(int startTab) {
        for (Screen s : screens) if (s != null) s.onHide();
        Ui.toastLayer = null;
        Ui.resetToast();
        buildShell(startTab);
        if (welcome != null) { welcome = null; showWelcome(); }
    }

    private View header() {
        LinearLayout h = Ui.h();
        h.setPadding(dp(16), dp(12), dp(14), dp(10));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.logo_mark);
        logo.setAdjustViewBounds(true);
        h.addView(logo, new LinearLayout.LayoutParams(dp(38), dp(34)));
        h.addView(Ui.space(10, 1));
        LinearLayout words = Ui.v();
        LinearLayout wm = Ui.h();
        TextView a = Ui.tv("SKIrf", 20, Theme.ACC, Theme.BOLD);
        TextView b = Ui.tv(" WARDRIVER", 20, Theme.TEXT, Theme.BOLD);
        b.setLetterSpacing(0.04f);
        wm.addView(a);
        wm.addView(b);
        words.addView(wm);
        h.addView(words, Ui.lpw(1));
        recPill = Ui.chip("REC", Theme.BAD);
        h.addView(recPill);
        h.addView(Ui.space(6, 1));
        gpsPill = Ui.chip("GPS", Theme.WARN);
        h.addView(gpsPill);
        Ui.tap(gpsPill, new Runnable() { @Override public void run() { showTab(TAB_GPS); } });
        Ui.tap(recPill, new Runnable() { @Override public void run() { showTab(TAB_DRIVE); } });
        return h;
    }

    private LinearLayout navBar() {
        LinearLayout n = Ui.h();
        n.setBackgroundColor(Theme.BG);
        n.setPadding(dp(6), dp(8), dp(6), dp(10));
        View topLine = new View(this);
        String[] names = {"Drive", "Map", "GPS", "Logs", "Settings"};
        int[] icons = {Icon.DRIVE, Icon.MAP, Icon.TARGET, Icon.LOGS, Icon.COG};
        for (int i = 0; i < 5; i++) {
            final int idx = i;
            LinearLayout it = Ui.v();
            it.setGravity(Gravity.CENTER_HORIZONTAL);
            FrameLayout pill = new FrameLayout(this);
            ImageView iv = new ImageView(this);
            iv.setImageDrawable(new Icon(icons[i], Theme.MUTE));
            pill.addView(iv, new FrameLayout.LayoutParams(dp(24), dp(24), Gravity.CENTER));
            LinearLayout.LayoutParams pp = new LinearLayout.LayoutParams(dp(60), dp(32));
            pp.gravity = Gravity.CENTER_HORIZONTAL;
            it.addView(pill, pp);
            TextView t = Ui.tv(names[i], 12, Theme.MUTE, Theme.SEMI);
            t.setPadding(0, dp(5), 0, 0);
            t.setGravity(Gravity.CENTER);
            Ui.single(t);
            LinearLayout.LayoutParams tp = new LinearLayout.LayoutParams(-1, -2);
            tp.gravity = Gravity.CENTER_HORIZONTAL;
            it.addView(t, tp);
            Ui.tap(it, new Runnable() { @Override public void run() { showTab(idx); } });
            n.addView(it, Ui.lpw(1));
        }
        n.setElevation(dp(2));
        return n;
    }

    public void showTab(int t) {
        if (t == tab) {
            View v = screens[t].view();
            if (v instanceof ScrollView) ((ScrollView) v).smoothScrollTo(0, 0);
            return;
        }
        if (tab >= 0) screens[tab].onHide();
        tab = t;
        Prefs.put(Prefs.LAST_TAB, t);
        content.removeAllViews();
        View v = screens[t].view();
        if (v.getParent() != null) ((FrameLayout) v.getParent()).removeView(v);
        v.setAlpha(0f);
        content.addView(v, new FrameLayout.LayoutParams(-1, -1));
        v.animate().alpha(1f).setDuration(160).start();
        screens[t].onShow();
        for (int i = 0; i < nav.getChildCount(); i++) {
            LinearLayout it = (LinearLayout) nav.getChildAt(i);
            boolean on = i == t;
            FrameLayout pill = (FrameLayout) it.getChildAt(0);
            pill.setBackground(on ? Ui.round(Theme.a(Theme.ACC, 40), 16) : null);
            ((Icon) ((ImageView) pill.getChildAt(0)).getDrawable()).setColor(on ? Theme.ACC : Theme.MUTE);
            ((TextView) it.getChildAt(1)).setTextColor(on ? Theme.TEXT : Theme.MUTE);
        }
        applyScreenOn();
    }

    // ======================================================================
    //  Ticking
    // ======================================================================
    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!resumed) return;
            try {
                if (tab >= 0) screens[tab].tick();
                headerTick();
            } catch (Throwable t) {
                // a drawing slip must never take the run down with it
                android.util.Log.e("SKIrf", "tick", t);
            }
            App.main.postDelayed(this, 500);
        }
    };

    private boolean blink;

    private void headerTick() {
        ScanEngine e = ScanEngine.get();
        if (!hasLocationPerm()) Ui.chipColor(gpsPill, "GPS OFF", Theme.BAD);
        else if (!e.locationServicesOn()) Ui.chipColor(gpsPill, "LOCATION OFF", Theme.BAD);
        else if (e.goodFix()) Ui.chipColor(gpsPill, e.satsUsed + " SATS", Theme.OK);
        else if (e.hasFix()) Ui.chipColor(gpsPill, "ROUGH FIX", Theme.WARN);
        else Ui.chipColor(gpsPill, "NO FIX", Theme.WARN);
        if (e.running) {
            blink = !blink;
            recPill.setVisibility(View.VISIBLE);
            Ui.chipColor(recPill, (blink ? "● " : "○ ") + Fmt.dur(e.elapsed()), Theme.BAD);
        } else recPill.setVisibility(View.GONE);
        applyScreenOn();
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        ScanEngine.get().uiAttach(this);
        App.main.removeCallbacks(tick);
        App.main.post(tick);
        if (tab >= 0) screens[tab].onShow();
        if (!autoStartDone && Prefs.b(Prefs.AUTOSTART) && Prefs.b(Prefs.ONBOARDED) && !ScanEngine.get().running && hasLocationPerm()) {
            autoStartDone = true;
            App.main.postDelayed(new Runnable() { @Override public void run() { startRun(); } }, 800);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        resumed = false;
        App.main.removeCallbacks(tick);
        ScanEngine.get().uiDetach();
    }

    @Override
    public void onBackPressed() {
        if (welcome != null) { super.onBackPressed(); return; }
        if (tab != TAB_DRIVE) { showTab(TAB_DRIVE); return; }
        if (ScanEngine.get().running) { moveTaskToBack(true); return; }
        super.onBackPressed();
    }

    public void applyScreenOn() {
        boolean on = ScanEngine.get().running && Prefs.b(Prefs.SCREEN_ON);
        if (on) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    public void hideKeyboard() {
        View f = getCurrentFocus();
        InputMethodManager im = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (f != null && im != null) { im.hideSoftInputFromWindow(f.getWindowToken(), 0); f.clearFocus(); }
    }

    // ======================================================================
    //  Run control
    // ======================================================================
    public void toggleRun() {
        final ScanEngine e = ScanEngine.get();
        if (e.running) {
            Ui.confirm("Stop this run?", Fmt.num(e.csv.rows()) + " rows saved in " + Fmt.dur(e.elapsed()) + ". The log is closed and kept.",
                    "Stop run", Ui.DANGER, new Runnable() {
                        @Override public void run() {
                            ScanService.stop(MainActivity.this);
                            Ui.toast("Run saved");
                            App.main.postDelayed(new Runnable() {
                                @Override public void run() { if (tab >= 0) screens[tab].onShow(); }
                            }, 600);
                        }
                    });
        } else startRun();
    }

    public void startRun() {
        if (ScanEngine.get().running) return;
        if (!hasLocationPerm()) { pendingStart = true; requestPerms(); return; }
        if (!ScanEngine.get().locationServicesOn()) {
            Ui.confirm("Location is off", "Android gives no GPS and no Wi-Fi scan results while Location is off. Turn it on, then tap start again.",
                    "Turn on", Ui.PRIMARY, new Runnable() { @Override public void run() { openLocationSettings(); } });
            return;
        }
        if (!Prefs.b(Prefs.WIFI) && !Prefs.b(Prefs.BLE) && !Prefs.b(Prefs.BT) && !Prefs.b(Prefs.CELL)) {
            Ui.info("Nothing to scan", "Every scanner is off. Turn on Wi-Fi or Bluetooth in Settings → Scanning.");
            return;
        }
        try {
            ScanService.start(this);
        } catch (Throwable t) {
            Ui.info("Couldn't start", "Android wouldn't start the scanning service: " + t.getMessage());
            return;
        }
        Ui.toast(ScanEngine.get().goodFix() ? "Run started" : "Run started · logging begins at GPS lock");
        if (tab != TAB_DRIVE) showTab(TAB_DRIVE);
        App.main.postDelayed(new Runnable() { @Override public void run() { if (tab >= 0) screens[tab].tick(); } }, 300);
    }

    // ======================================================================
    //  Permissions and system screens
    // ======================================================================
    public boolean hasLocationPerm() {
        return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    public void requestPerms() {
        List<String> p = new ArrayList<String>();
        p.add(Manifest.permission.ACCESS_FINE_LOCATION);
        p.add(Manifest.permission.ACCESS_COARSE_LOCATION);
        if (Build.VERSION.SDK_INT >= 31) {
            p.add(Manifest.permission.BLUETOOTH_SCAN);
            p.add(Manifest.permission.BLUETOOTH_CONNECT);
        }
        if (Build.VERSION.SDK_INT >= 33) {
            p.add(Manifest.permission.POST_NOTIFICATIONS);
            p.add(Manifest.permission.NEARBY_WIFI_DEVICES);
        }
        List<String> need = new ArrayList<String>();
        for (String s : p) if (checkSelfPermission(s) != PackageManager.PERMISSION_GRANTED) need.add(s);
        if (need.isEmpty()) { afterPerms(); return; }
        // Asked before and refused for good: only the settings page can change it now
        if (!hasLocationPerm() && Prefs.sp().getBoolean("asked_loc", false)
                && !shouldShowRequestPermissionRationale(Manifest.permission.ACCESS_FINE_LOCATION)) {
            Ui.confirm("Allow location in Settings", "Location access was turned down. Open the app's settings, tap Permissions → Location, and choose \"Allow only while using the app\" with Precise location on.",
                    "Open settings", Ui.PRIMARY, new Runnable() {
                        @Override public void run() {
                            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + getPackageName())));
                        }
                    });
            return;
        }
        Prefs.sp().edit().putBoolean("asked_loc", true).apply();
        requestPermissions(need.toArray(new String[0]), REQ_PERMS);
    }

    @Override
    public void onRequestPermissionsResult(int code, String[] perms, int[] res) {
        super.onRequestPermissionsResult(code, perms, res);
        if (code != REQ_PERMS) return;
        afterPerms();
    }

    private void afterPerms() {
        ScanEngine.get().restartGps();
        if (welcome != null) closeWelcome();
        if (hasLocationPerm()) {
            if (pendingStart) { pendingStart = false; startRun(); }
        } else {
            pendingStart = false;
            Ui.toast("Location is needed to log anything");
        }
        if (tab >= 0) screens[tab].tick();
    }

    public void openLocationSettings() {
        try { startActivity(new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)); }
        catch (Throwable t) { startActivity(new Intent(Settings.ACTION_SETTINGS)); }
    }

    public void openWifiSettings() {
        try {
            if (Build.VERSION.SDK_INT >= 29) startActivity(new Intent(Settings.Panel.ACTION_WIFI));
            else startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
        } catch (Throwable t) { startActivity(new Intent(Settings.ACTION_SETTINGS)); }
    }

    @SuppressWarnings("MissingPermission")
    public void enableBluetooth() {
        if (Build.VERSION.SDK_INT >= 31 && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPerms();
            return;
        }
        try { startActivity(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)); }
        catch (Throwable t) {
            try { startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)); } catch (Throwable ignored) { }
        }
    }

    public void askBatteryExemption() {
        try {
            startActivity(new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:" + getPackageName())));
        } catch (Throwable t) {
            try { startActivity(new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)); } catch (Throwable ignored) { }
        }
    }

    /** -1 unknown, 0 off, 1 on */
    private int throttleState() {
        try {
            return Settings.Global.getInt(getContentResolver(), "wifi_scan_throttle_enabled", 1) == 0 ? 0 : 1;
        } catch (Throwable t) { return -1; }
    }

    public void throttleHelp() {
        LinearLayout c = Ui.v();
        c.addView(Ui.tv("Unlock more Wi-Fi scans", 22, Theme.TEXT, Theme.BOLD));
        int st = throttleState();
        TextView now = Ui.chip(st == 0 ? "THROTTLING IS OFF ON THIS PHONE" : st == 1 ? "THROTTLING IS ON" : "STATE UNKNOWN",
                st == 0 ? Theme.OK : Theme.WARN);
        LinearLayout.LayoutParams np = Ui.lp(-2, -2);
        np.topMargin = dp(10);
        c.addView(now, np);
        TextView m = Ui.body("Android lets an app scan for Wi-Fi about 4 times every 2 minutes. At driving speed that misses most networks. "
                + "The limit can be switched off in Developer options:");
        m.setPadding(0, dp(12), 0, dp(12));
        c.addView(m);
        String[] steps = {
                "Open Settings → About phone (on some phones: Software information).",
                "Tap Build number 7 times until it says you're a developer.",
                "Go back to Settings → System → Developer options.",
                "Scroll to Networking and turn off Wi-Fi scan throttling."};
        for (int i = 0; i < steps.length; i++) {
            LinearLayout r = Ui.h();
            r.setGravity(Gravity.TOP);
            r.setPadding(0, dp(6), 0, dp(6));
            TextView n = Ui.tv(String.valueOf(i + 1), 13, Theme.ON_ACC, Theme.BOLD);
            n.setGravity(Gravity.CENTER);
            n.setBackground(Ui.round(Theme.ACC, 99));
            r.addView(n, new LinearLayout.LayoutParams(dp(24), dp(24)));
            r.addView(Ui.space(12, 1));
            TextView s = Ui.tv(steps[i], 15, Theme.TEXT, Theme.MED);
            s.setLineSpacing(0, 1.25f);
            r.addView(s, Ui.lpw(1));
            c.addView(r);
        }
        TextView note = Ui.tv("It's a normal Android setting and safe to change. It applies to every app until you turn it back on.", 13, Theme.MUTE, Theme.MED);
        note.setLineSpacing(0, 1.25f);
        note.setPadding(0, dp(12), 0, dp(18));
        c.addView(note);
        final Dialog[] d = new Dialog[1];
        c.addView(Ui.button("Open Developer options", Icon.COG, Ui.PRIMARY, new Runnable() {
            @Override public void run() {
                d[0].dismiss();
                try { startActivity(new Intent(Settings.ACTION_APPLICATION_DEVELOPMENT_SETTINGS)); }
                catch (Throwable t) {
                    Ui.toast("Developer options aren't on yet. Do steps 1 and 2 first.");
                    try { startActivity(new Intent(Settings.ACTION_DEVICE_INFO_SETTINGS)); } catch (Throwable ignored) { }
                }
            }
        }));
        d[0] = Ui.sheet(c);
        d[0].show();
    }

    // ======================================================================
    //  Crash report
    // ======================================================================
    public void showCrash() {
        final File f = App.crashFile();
        if (!f.exists()) return;
        final StringBuilder sb = new StringBuilder();
        try {
            BufferedReader r = new BufferedReader(new FileReader(f));
            String line;
            while ((line = r.readLine()) != null && sb.length() < 20000) sb.append(line).append('\n');
            r.close();
        } catch (Throwable ignored) { }
        LinearLayout c = Ui.v();
        c.addView(Ui.tv("The app stopped last time", 22, Theme.TEXT, Theme.BOLD));
        TextView m = Ui.body("Sorry about that. Your logs up to that moment were saved. Sending this report helps get it fixed.");
        m.setPadding(0, dp(10), 0, dp(12));
        c.addView(m);
        TextView rep = Ui.tv(sb.toString(), 11, Theme.MUTE, Theme.MONO);
        rep.setBackground(Ui.round(Theme.BG, 12));
        rep.setPadding(dp(12), dp(12), dp(12), dp(12));
        rep.setMaxLines(14);
        rep.setTextIsSelectable(true);
        c.addView(rep);
        c.addView(Ui.space(1, 14));
        final Dialog[] d = new Dialog[1];
        c.addView(Ui.row(
                Ui.button("Copy", Icon.COPY, Ui.SECONDARY, new Runnable() {
                    @Override public void run() {
                        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                        if (cm != null) cm.setPrimaryClip(ClipData.newPlainText("SKIrf crash", sb.toString()));
                        Ui.toast("Copied");
                    }
                }),
                Ui.button("Share", Icon.SHARE, Ui.SECONDARY, new Runnable() {
                    @Override public void run() {
                        Intent s = new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, sb.toString())
                                .putExtra(Intent.EXTRA_SUBJECT, "SKIrf Wardriver crash report");
                        startActivity(Intent.createChooser(s, "Send crash report"));
                    }
                })));
        c.addView(Ui.space(1, 8));
        c.addView(Ui.button("Dismiss", 0, Ui.PRIMARY, new Runnable() {
            @Override public void run() { f.delete(); d[0].dismiss(); }
        }));
        d[0] = Ui.sheet(c);
        d[0].show();
    }

    // ======================================================================
    //  Welcome
    // ======================================================================
    private void showWelcome() {
        LinearLayout col = Ui.v();
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        col.setPadding(dp(24), dp(40), dp(24), dp(28));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.logo_mark);
        logo.setAdjustViewBounds(true);
        col.addView(logo, new LinearLayout.LayoutParams(dp(150), dp(132)));
        LinearLayout wm = Ui.h();
        wm.setGravity(Gravity.CENTER);
        wm.addView(Ui.tv("SKIrf", 32, Theme.ACC, Theme.BOLD));
        TextView w2 = Ui.tv(" WARDRIVER", 32, Theme.TEXT, Theme.BOLD);
        w2.setLetterSpacing(0.04f);
        wm.addView(w2);
        wm.setPadding(0, dp(18), 0, 0);
        col.addView(wm);
        TextView tag = Ui.body("Log Wi-Fi, Bluetooth and cell towers as you drive, then upload to WiGLE and WDGWars.");
        tag.setGravity(Gravity.CENTER);
        tag.setTextSize(16);
        tag.setPadding(0, dp(10), 0, dp(26));
        col.addView(tag);

        LinearLayout card = Ui.card();
        card.addView(Ui.head("The app will ask for", null));
        permRow(card, Icon.PIN, Theme.ACC, "Location", "Stamps every find with where you were. Android also only shares Wi-Fi scans with apps that have it. Choose Precise.");
        permRow(card, Icon.BLE, Theme.BLE, "Nearby devices", "To scan for Bluetooth devices.");
        permRow(card, Icon.INFO, Theme.WARN, "Notifications", "Shows the run with a Stop button, and tells you when uploads finish.");
        col.addView(card);
        TextView note = Ui.tv("Nothing leaves your phone until you upload. The app only listens and never connects to what it hears.", 13, Theme.MUTE, Theme.MED);
        note.setGravity(Gravity.CENTER);
        note.setLineSpacing(0, 1.25f);
        note.setPadding(dp(8), dp(6), dp(8), dp(22));
        col.addView(note);
        col.addView(Ui.button("Get started", Icon.CHECK, Ui.PRIMARY, new Runnable() {
            @Override public void run() { requestPerms(); }
        }), Ui.MW);
        TextView skip = Ui.tv("NOT NOW", 13, Theme.MUTE, Theme.BOLD);
        skip.setLetterSpacing(0.1f);
        skip.setPadding(dp(16), dp(18), dp(16), dp(10));
        Ui.tap(skip, new Runnable() { @Override public void run() { closeWelcome(); } });
        col.addView(skip);

        ScrollView sv = new ScrollView(this);
        sv.setFillViewport(true);
        sv.setBackgroundColor(Theme.BG);
        sv.addView(col, new FrameLayout.LayoutParams(-1, -2));
        sv.setClickable(true);
        welcome = sv;
        rootFrame.addView(sv, new FrameLayout.LayoutParams(-1, -1));
    }

    private void permRow(LinearLayout c, int icon, int color, String title, String text) {
        LinearLayout r = Ui.h();
        r.setGravity(Gravity.TOP);
        r.setPadding(0, dp(8), 0, dp(8));
        FrameLayout ic = new FrameLayout(this);
        ic.setBackground(Ui.round(Theme.a(color, 36), 12));
        ImageView iv = new ImageView(this);
        iv.setImageDrawable(new Icon(icon, color));
        iv.setPadding(dp(9), dp(9), dp(9), dp(9));
        ic.addView(iv, new FrameLayout.LayoutParams(-1, -1));
        r.addView(ic, new LinearLayout.LayoutParams(dp(40), dp(40)));
        r.addView(Ui.space(14, 1));
        LinearLayout tx = Ui.v();
        tx.addView(Ui.tv(title, 16, Theme.TEXT, Theme.SEMI));
        TextView s = Ui.tv(text, 14, Theme.MUTE, Theme.MED);
        s.setLineSpacing(0, 1.22f);
        s.setPadding(0, dp(4), 0, 0);
        tx.addView(s);
        r.addView(tx, Ui.lpw(1));
        c.addView(r);
    }

    private void closeWelcome() {
        Prefs.put(Prefs.ONBOARDED, true);
        if (welcome == null) return;
        final View w = welcome;
        welcome = null;
        w.animate().alpha(0).setDuration(220).withEndAction(new Runnable() {
            @Override public void run() { rootFrame.removeView(w); }
        }).start();
    }
}
