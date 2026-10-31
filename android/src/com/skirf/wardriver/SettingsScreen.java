package com.skirf.wardriver;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import static com.skirf.wardriver.Ui.dp;

public final class SettingsScreen extends Screen {
    public SettingsScreen(MainActivity a) { super(a); }

    private TextView batterySub, allTimeSub, wigleStatus, wdgStatus;

    @Override protected View build() {
        LinearLayout col = Ui.v();

        // ---------------- scanning ----------------
        LinearLayout sc = section(col, "Scanning", Icon.WIFI);
        toggle(sc, "Wi-Fi", "Scan for Wi-Fi networks on 2.4, 5 and 6 GHz.", Prefs.WIFI, true);
        choice(sc, "Ask for a Wi-Fi scan every", "Android may still limit this. See Phone below.", Prefs.WIFI_INTERVAL,
                new String[]{"1 s", "2 s", "5 s", "10 s", "30 s"}, new int[]{1000, 2000, 5000, 10000, 30000}, false);
        line(sc);
        toggle(sc, "Bluetooth LE", "Trackers, watches, earbuds, beacons and other low-energy devices.", Prefs.BLE, true);
        choice(sc, "Bluetooth LE scan power", "Max finds the most and uses the most battery.", Prefs.BLE_MODE,
                new String[]{"Low", "Balanced", "Max"}, new int[]{0, 1, 2}, true);
        line(sc);
        toggle(sc, "Bluetooth classic", "Headsets, car kits and speakers that are in pairing range. Slows Wi-Fi scans a little.", Prefs.BT, true);
        toggle(sc, "Cell towers", "Logs GSM, 3G, LTE and 5G towers. WiGLE takes them; WDGWars may skip them.", Prefs.CELL, true);
        line(sc);
        choice(sc, "Ignore weak signals", "Skip anything quieter than this.", Prefs.MIN_RSSI,
                new String[]{"Off", "-90", "-85", "-80"}, new int[]{-127, -90, -85, -80}, false);

        // ---------------- logging ----------------
        LinearLayout lg = section(col, "Logging", Icon.LOGS);
        toggle(lg, "Wait for GPS before logging", "Rows without a position are useless to WiGLE and WDGWars. Leave this on.", Prefs.NEED_FIX, false);
        choice(lg, "Best GPS accuracy needed", "Positions worse than this aren't logged (metres).", Prefs.MAX_ACC,
                new String[]{"10", "25", "50", "100", "Any"}, new int[]{10, 25, 50, 100, 0}, false);
        choice(lg, "Log each device", "Once per run matches the board. More rows help WiGLE place networks more exactly.", Prefs.LOG_MODE,
                new String[]{"Once", "If stronger", "Every 10 s"}, new int[]{0, 1, 2}, false);
        choice(lg, "Start a new file every", "Smaller files upload more reliably on weak signal.", Prefs.SPLIT,
                new String[]{"Never", "30 min", "1 hour", "50k rows"}, new int[]{0, 30, 60, -1}, false);
        line(lg);
        TextView pt = Ui.tv("File name starts with", 16, Theme.TEXT, Theme.SEMI);
        lg.addView(pt);
        final EditText prefix = Ui.input("SKIrf", Prefs.s(Prefs.PREFIX), false);
        LinearLayout.LayoutParams pp = Ui.lp(-1, -2);
        pp.topMargin = dp(10);
        pp.bottomMargin = dp(6);
        lg.addView(prefix, pp);
        prefix.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a1, int b, int c) { }
            @Override public void onTextChanged(CharSequence s, int a1, int b, int c) { }
            @Override public void afterTextChanged(Editable s) { Prefs.put(Prefs.PREFIX, CsvLog.cleanPrefix(s.toString())); }
        });
        line(lg);
        toggle(lg, "Copy each run to Downloads", "Saves a copy in Downloads / SKIrf Wardriver when a run ends, where any file app can see it.", Prefs.COPY_DL, false);

        // ---------------- GPS ----------------
        LinearLayout gp = section(col, "GPS and units", Icon.TARGET);
        choice(gp, "GPS updates", "Faster updates give more exact positions while driving.", Prefs.GPS_RATE,
                new String[]{"1 s", "2 s", "5 s"}, new int[]{1000, 2000, 5000}, false, new Runnable() {
                    @Override public void run() { ScanEngine.get().restartGps(); }
                });
        toggle(gp, "Use network location without GPS", "Fills gaps in tunnels and garages, but those positions can be hundreds of metres off.", Prefs.NET_LOC, false,
                new Runnable() { @Override public void run() { ScanEngine.get().restartGps(); } });
        choice(gp, "Units", null, Prefs.UNITS, new String[]{"Metric", "Imperial"}, new int[]{0, 1}, false);

        // ---------------- run ----------------
        LinearLayout rn = section(col, "During a run", Icon.DRIVE);
        toggle(rn, "Keep the screen on", "While a run is going and the app is open.", Prefs.SCREEN_ON, false,
                new Runnable() { @Override public void run() { a.applyScreenOn(); } });
        toggle(rn, "Keep scanning with the screen off", "Holds the phone awake during a run. Turn off only to save battery on short runs.", Prefs.WAKELOCK, true);
        toggle(rn, "Start a run when the app opens", "Handy with a phone mount: open the app and go.", Prefs.AUTOSTART, false);
        toggle(rn, "Buzz for new open networks", "A short vibration when an open Wi-Fi network is found.", Prefs.BUZZ_OPEN, false);

        // ---------------- uploads ----------------
        LinearLayout up = section(col, "Uploads", Icon.UPLOAD);
        up.addView(Ui.tv("WiGLE", 18, Theme.HI, Theme.BOLD));
        TextView wh = Ui.tv("On wigle.net go to Account, then Show my token. Use the API Name and API Token, not your login.", 13, Theme.MUTE, Theme.MED);
        wh.setLineSpacing(0, 1.25f);
        wh.setPadding(0, dp(6), 0, dp(10));
        up.addView(wh);
        final EditText wName = Ui.input("API Name (starts with AID)", Prefs.s(Prefs.WIGLE_NAME), false);
        up.addView(wName, Ui.MW);
        up.addView(Ui.space(1, 8));
        final EditText wTok = Ui.input("API Token", Prefs.s(Prefs.WIGLE_TOKEN), true);
        up.addView(Ui.secretInput(wTok), Ui.MW);
        wigleStatus = Ui.tv(Prefs.hasWigle() ? "Saved" : "", 13, Theme.OK, Theme.SEMI);
        wigleStatus.setPadding(0, dp(8), 0, dp(8));
        up.addView(wigleStatus);
        up.addView(Ui.row(
                Ui.button("Save", Icon.CHECK, Ui.PRIMARY, new Runnable() {
                    @Override public void run() {
                        Prefs.put(Prefs.WIGLE_NAME, wName.getText().toString());
                        Prefs.put(Prefs.WIGLE_TOKEN, wTok.getText().toString());
                        a.hideKeyboard();
                        say(wigleStatus, Prefs.hasWigle() ? "Saved" : "Cleared", Prefs.hasWigle() ? Theme.OK : Theme.MUTE);
                        Uploader.wigleStats = null;
                    }
                }),
                Ui.button("Test", Icon.REFRESH, Ui.SECONDARY, new Runnable() {
                    @Override public void run() {
                        Prefs.put(Prefs.WIGLE_NAME, wName.getText().toString());
                        Prefs.put(Prefs.WIGLE_TOKEN, wTok.getText().toString());
                        a.hideKeyboard();
                        say(wigleStatus, "Checking with WiGLE...", Theme.ACC);
                        Uploader.fetchWigleStats(new Uploader.Done() {
                            @Override public void done(Uploader.Result r) {
                                if (r.ok) {
                                    String user = r.json == null ? "" : r.json.optString("user", "");
                                    say(wigleStatus, "Works" + (user.length() > 0 ? ": signed in as " + user : ""), Theme.OK);
                                } else say(wigleStatus, r.msg, Theme.BAD);
                            }
                        });
                    }
                })));
        line(up);
        up.addView(Ui.tv("WDGWars", 18, Theme.HI, Theme.BOLD));
        TextView dh = Ui.tv("Your API key from your profile on wdgwars.pl.", 13, Theme.MUTE, Theme.MED);
        dh.setPadding(0, dp(6), 0, dp(10));
        up.addView(dh);
        final EditText dKey = Ui.input("API key", Prefs.s(Prefs.WDG_KEY), true);
        up.addView(Ui.secretInput(dKey), Ui.MW);
        wdgStatus = Ui.tv(Prefs.hasWdg() ? "Saved" : "", 13, Theme.OK, Theme.SEMI);
        wdgStatus.setPadding(0, dp(8), 0, dp(8));
        up.addView(wdgStatus);
        up.addView(Ui.row(
                Ui.button("Save", Icon.CHECK, Ui.PRIMARY, new Runnable() {
                    @Override public void run() {
                        Prefs.put(Prefs.WDG_KEY, dKey.getText().toString());
                        a.hideKeyboard();
                        say(wdgStatus, Prefs.hasWdg() ? "Saved" : Prefs.s(Prefs.WDG_KEY).length() > 0 ? "That key looks too short" : "Cleared",
                                Prefs.hasWdg() ? Theme.OK : Theme.WARN);
                        Uploader.wdgStats = null;
                    }
                }),
                Ui.button("Test", Icon.REFRESH, Ui.SECONDARY, new Runnable() {
                    @Override public void run() {
                        Prefs.put(Prefs.WDG_KEY, dKey.getText().toString());
                        a.hideKeyboard();
                        say(wdgStatus, "Checking with WDGWars...", Theme.ACC);
                        Uploader.fetchWdgStats(new Uploader.Done() {
                            @Override public void done(Uploader.Result r) {
                                if (r.ok) {
                                    String user = r.json == null ? "" : r.json.optString("username", "");
                                    say(wdgStatus, "Works" + (user.length() > 0 ? ": signed in as " + user : ""), Theme.OK);
                                } else say(wdgStatus, r.msg, Theme.BAD);
                            }
                        });
                    }
                })));
        line(up);
        choice(up, "Upload when a run ends", "Only to sites with a saved key.", Prefs.AUTO_UP,
                new String[]{"Off", "WiGLE", "WDGW", "Both"}, new int[]{0, 1, 2, 3}, false);
        toggle(up, "Only upload on Wi-Fi", "Waits for Wi-Fi so uploads don't use mobile data.", Prefs.UP_WIFI_ONLY, false);

        // ---------------- look ----------------
        LinearLayout lk = section(col, "Theme", Icon.SPARK);
        LinearLayout grid = Ui.v();
        LinearLayout row = null;
        for (int i = 0; i < Theme.ACCENTS.length; i++) {
            if (i % 5 == 0) {
                row = Ui.h();
                row.setPadding(0, dp(4), 0, dp(4));
                grid.addView(row);
            }
            row.addView(swatch(i), Ui.lpw(1));
        }
        lk.addView(grid);

        // ---------------- phone ----------------
        LinearLayout ph = section(col, "Phone", Icon.PHONE);
        link(ph, "Wi-Fi scan throttling", "Android allows about 4 Wi-Fi scans every 2 minutes. Turning the limit off catches far more networks.",
                Icon.WIFI, new Runnable() { @Override public void run() { a.throttleHelp(); } });
        batterySub = link(ph, "Don't let Android pause scanning", "", Icon.BATTERY, new Runnable() {
            @Override public void run() { a.askBatteryExemption(); }
        });
        link(ph, "App permissions", "Location, Nearby devices and Notifications.", Icon.KEY, new Runnable() {
            @Override public void run() {
                a.startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + a.getPackageName())));
            }
        });
        link(ph, "Location settings", "Make sure Location and Wi-Fi scanning are on.", Icon.PIN, new Runnable() {
            @Override public void run() { a.openLocationSettings(); }
        });

        // ---------------- data ----------------
        LinearLayout dt = section(col, "Your data", Icon.LOGS);
        allTimeSub = link(dt, "Forget everything seen before", "", Icon.SPARK, new Runnable() {
            @Override public void run() {
                Ui.confirm("Forget the all-time list?", "The \"New to you\" count starts again from zero. Your log files are not touched.",
                        "Forget", Ui.DANGER, new Runnable() {
                            @Override public void run() { ScanEngine.get().clearAllTime(); Ui.toast("All-time list cleared"); tick(); }
                        });
            }
        });
        link(dt, "Delete all logs", "Removes every log file from this phone.", Icon.TRASH, new Runnable() {
            @Override public void run() {
                Ui.confirm("Delete all logs?", "Every log file on this phone will be deleted, uploaded or not. This can't be undone.",
                        "Delete all", Ui.DANGER, new Runnable() {
                            @Override public void run() {
                                int n = 0;
                                for (LogStore.Info i : LogStore.list()) if (LogStore.delete(i)) n++;
                                Ui.toast("Deleted " + n + " log" + (n == 1 ? "" : "s"));
                            }
                        });
            }
        });

        // ---------------- about ----------------
        LinearLayout ab = section(col, "About", Icon.INFO);
        LinearLayout brand = Ui.h();
        android.widget.ImageView logo = new android.widget.ImageView(a);
        logo.setImageResource(R.drawable.logo_mark);
        brand.addView(logo, new LinearLayout.LayoutParams(dp(56), dp(50)));
        brand.addView(Ui.space(14, 1));
        LinearLayout bt = Ui.v();
        bt.addView(Ui.tv("SKIrf Wardriver", 20, Theme.TEXT, Theme.BOLD));
        TextView ver = Ui.tv("Android " + BuildInfo.VERSION, 14, Theme.MUTE, Theme.MED);
        ver.setPadding(0, dp(3), 0, 0);
        bt.addView(ver);
        brand.addView(bt, Ui.lpw(1));
        ab.addView(brand);
        TextView safe = Ui.body("This app only listens. It never connects to, or interferes with, the networks and devices it hears. "
                + "Logs use the WiGLE CSV format, the same as the SKIrf board. Don't handle the phone while driving: start the run before you set off.");
        safe.setPadding(0, dp(14), 0, dp(10));
        ab.addView(safe);
        TextView lic = Ui.tv("Saira Semi Condensed font: SIL Open Font License 1.1. Map data © OpenStreetMap contributors.", 12, Theme.DIM, Theme.MED);
        lic.setLineSpacing(0, 1.2f);
        ab.addView(lic);
        if (App.crashFile().exists()) {
            ab.addView(Ui.space(1, 12));
            ab.addView(Ui.button("Last crash report", Icon.ALERT, Ui.SECONDARY, new Runnable() {
                @Override public void run() { a.showCrash(); }
            }));
        }
        return Ui.page(col);
    }

    // ---------------- helpers ----------------
    private LinearLayout section(LinearLayout col, String title, int icon) {
        LinearLayout h = Ui.h();
        h.setPadding(dp(4), dp(10), dp(4), dp(10));
        h.addView(Ui.icon(icon, Theme.ACC, 16));
        h.addView(Ui.space(8, 1));
        h.addView(Ui.cap(title));
        col.addView(h);
        LinearLayout c = Ui.card();
        c.setPadding(dp(16), dp(10), dp(16), dp(12));
        col.addView(c);
        return c;
    }

    private void line(LinearLayout c) { c.addView(Ui.divider()); }

    private void toggle(LinearLayout c, String title, String sub, final String key, final boolean radio) {
        toggle(c, title, sub, key, radio, null);
    }

    private void toggle(LinearLayout c, String title, String sub, final String key, final boolean radio, final Runnable after) {
        final Ui.Toggle t = new Ui.Toggle(a, Prefs.b(key));
        final LinearLayout r = Ui.settingRow(title, sub, t);
        t.listener(new Ui.Toggle.Listener() {
            @Override public void changed(boolean on) {
                Prefs.put(key, on);
                if (radio && ScanEngine.get().running) Ui.toast("Takes effect on the next run");
                if (after != null) after.run();
            }
        });
        r.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { t.set(!t.on(), true); }
        });
        c.addView(r);
    }

    private void choice(LinearLayout c, String title, String sub, String key, String[] labels, int[] vals, boolean radio) {
        choice(c, title, sub, key, labels, vals, radio, null);
    }

    private void choice(LinearLayout c, String title, String sub, final String key, String[] labels, final int[] vals,
                        final boolean radio, final Runnable after) {
        LinearLayout box = Ui.v();
        box.setPadding(0, dp(8), 0, dp(8));
        box.addView(Ui.tv(title, 16, Theme.TEXT, Theme.SEMI));
        if (sub != null) {
            TextView s = Ui.tv(sub, 13, Theme.MUTE, Theme.MED);
            s.setLineSpacing(0, 1.2f);
            s.setPadding(0, dp(4), 0, 0);
            box.addView(s);
        }
        int cur = Prefs.i(key), sel = 0;
        for (int i = 0; i < vals.length; i++) if (vals[i] == cur) sel = i;
        LinearLayout seg = Ui.seg(labels, sel, new Ui.Choice() {
            @Override public void chose(int index) {
                Prefs.put(key, vals[index]);
                if (radio && ScanEngine.get().running) Ui.toast("Takes effect on the next run");
                if (after != null) after.run();
            }
        });
        LinearLayout.LayoutParams p = Ui.lp(-1, -2);
        p.topMargin = dp(10);
        box.addView(seg, p);
        c.addView(box);
    }

    private TextView link(LinearLayout c, String title, String sub, int icon, Runnable onTap) {
        LinearLayout r = Ui.h();
        r.setPadding(0, dp(10), 0, dp(10));
        r.addView(Ui.icon(icon, Theme.MUTE, 22));
        r.addView(Ui.space(14, 1));
        LinearLayout tx = Ui.v();
        tx.addView(Ui.tv(title, 16, Theme.TEXT, Theme.SEMI));
        TextView s = Ui.tv(sub, 13, Theme.MUTE, Theme.MED);
        s.setLineSpacing(0, 1.2f);
        s.setPadding(0, dp(4), 0, 0);
        tx.addView(s);
        r.addView(tx, Ui.lpw(1));
        r.addView(Ui.icon(Icon.CHEVRON, Theme.DIM, 18));
        Ui.tap(r, onTap);
        c.addView(r);
        return s;
    }

    private View swatch(final int i) {
        LinearLayout v = Ui.v();
        v.setGravity(Gravity.CENTER_HORIZONTAL);
        v.setPadding(0, dp(6), 0, dp(6));
        FrameLayout ring = new FrameLayout(a);
        boolean sel = Theme.accentIndex == i;
        ring.setBackground(Ui.outline(0, sel ? Theme.TEXT : 0, 99, sel ? 2.5f : 0));
        View fill = new View(a);
        if (i == Theme.WDGW) {
            android.graphics.drawable.GradientDrawable g = new android.graphics.drawable.GradientDrawable(
                    android.graphics.drawable.GradientDrawable.Orientation.TL_BR, new int[]{0xFFFF20A0, 0xFF00FFFF});
            g.setShape(android.graphics.drawable.GradientDrawable.OVAL);
            fill.setBackground(g);
        } else if (i == Theme.PRO) {
            android.graphics.drawable.GradientDrawable g = new android.graphics.drawable.GradientDrawable();
            g.setShape(android.graphics.drawable.GradientDrawable.OVAL);
            g.setColor(0xFF152415);
            g.setStroke(dp(4), Theme.ACCENTS[i]);
            fill.setBackground(g);
        } else {
            fill.setBackground(Ui.dot(Theme.ACCENTS[i], 10).getBackground());
        }
        FrameLayout.LayoutParams fp = new FrameLayout.LayoutParams(dp(34), dp(34), Gravity.CENTER);
        ring.addView(fill, fp);
        v.addView(ring, new LinearLayout.LayoutParams(dp(44), dp(44)));
        TextView n = Ui.tv(Theme.ACCENT_NAMES[i], 12, sel ? Theme.TEXT : Theme.MUTE, Theme.SEMI);
        n.setPadding(0, dp(6), 0, 0);
        v.addView(n);
        Ui.tap(v, new Runnable() {
            @Override public void run() {
                if (Theme.accentIndex == i) return;
                Prefs.put(Prefs.ACCENT, i);
                Theme.apply(i);
                a.rebuildUi(MainActivity.TAB_SETTINGS);
            }
        });
        return v;
    }

    private static void say(TextView t, String msg, int color) {
        t.setText(msg);
        t.setTextColor(color);
    }

    @Override public void tick() {
        if (root == null) return;
        PowerManager pm = (PowerManager) a.getSystemService(Context.POWER_SERVICE);
        boolean free = pm != null && pm.isIgnoringBatteryOptimizations(a.getPackageName());
        batterySub.setText(free ? "Done: Android won't pause the app." : "Battery saving can stop scans during long runs. Tap to allow the app to keep running.");
        batterySub.setTextColor(free ? Theme.OK : Theme.MUTE);
        allTimeSub.setText(Fmt.num(ScanEngine.get().allTimeCount()) + " devices and networks remembered on this phone.");
    }
}
