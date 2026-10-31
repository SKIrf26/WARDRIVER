package com.skirf.wardriver;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import static com.skirf.wardriver.Ui.dp;

public final class DriveScreen extends Screen {
    public DriveScreen(MainActivity a) { super(a); }

    private LinearLayout banners;
    private String bannerKey = "";
    private Charts.DriveButton btn;
    private TextView heroCap, timer, heroSub, stDist, stRows, stRate;
    private final List<Tile> tiles = new ArrayList<Tile>();
    private LinearLayout tileGrid;
    private String tileKey = "";
    private Charts.Spark spark;
    private TextView sparkRight;
    private Charts.SplitBar bandBar, secBar;
    private LinearLayout bandLegend, secLegend;
    private TextView extraLine;
    private LinearLayout radios;
    private LinearLayout feedBox;
    private TextView feedCount;
    private final List<FeedRow> feedRows = new ArrayList<FeedRow>();

    @Override protected View build() {
        LinearLayout col = Ui.v();
        banners = Ui.v();
        col.addView(banners, Ui.MW);

        // ---------- hero ----------
        LinearLayout hero = Ui.card();
        hero.setPadding(dp(18), dp(18), dp(18), dp(16));
        LinearLayout top = Ui.h();
        LinearLayout left = Ui.v();
        heroCap = Ui.cap("Ready");
        left.addView(heroCap);
        timer = Ui.big("0:00", 48, Theme.HI);
        timer.setPadding(0, dp(6), 0, dp(4));
        left.addView(timer);
        heroSub = Ui.tv("", 14, Theme.MUTE, Theme.MED);
        heroSub.setMaxLines(2);
        left.addView(heroSub);
        top.addView(left, Ui.lpw(1));
        btn = new Charts.DriveButton(a);
        top.addView(btn, new LinearLayout.LayoutParams(dp(118), dp(118)));
        Ui.tap(btn, new Runnable() { @Override public void run() { a.toggleRun(); } });
        hero.addView(top);
        View d = Ui.divider();
        hero.addView(d);
        LinearLayout stats = Ui.h();
        stDist = mini(stats, Icon.ROUTE, "Distance");
        stRows = mini(stats, Icon.LOGS, "Rows saved");
        stRate = mini(stats, Icon.SPARK, "Last minute");
        hero.addView(stats);
        col.addView(hero);

        // ---------- tiles ----------
        tileGrid = Ui.v();
        col.addView(tileGrid, Ui.MW);

        // ---------- activity ----------
        LinearLayout act = Ui.card();
        sparkRight = Ui.tv("last 30 min", 12, Theme.MUTE, Theme.MED);
        act.addView(Ui.head("Finds per minute", sparkRight));
        spark = new Charts.Spark(a);
        act.addView(spark, new LinearLayout.LayoutParams(-1, dp(96)));
        col.addView(act);

        // ---------- breakdown ----------
        LinearLayout br = Ui.card();
        br.addView(Ui.head("Wi-Fi bands", null));
        bandBar = new Charts.SplitBar(a);
        br.addView(bandBar, new LinearLayout.LayoutParams(-1, dp(12)));
        bandLegend = Ui.h();
        bandLegend.setPadding(0, dp(10), 0, 0);
        br.addView(bandLegend);
        View sp = Ui.space(1, 18);
        br.addView(sp);
        br.addView(Ui.head("Wi-Fi security", null));
        secBar = new Charts.SplitBar(a);
        br.addView(secBar, new LinearLayout.LayoutParams(-1, dp(12)));
        secLegend = Ui.h();
        secLegend.setPadding(0, dp(10), 0, 0);
        br.addView(secLegend);
        br.addView(Ui.divider());
        extraLine = Ui.tv("", 14, Theme.MUTE, Theme.MED);
        extraLine.setLineSpacing(0, 1.3f);
        br.addView(extraLine);
        col.addView(br);

        // ---------- radios ----------
        LinearLayout rc = Ui.card();
        rc.addView(Ui.head("Radios", null));
        radios = Ui.v();
        rc.addView(radios);
        col.addView(rc);

        // ---------- live feed ----------
        LinearLayout fc = Ui.card();
        feedCount = Ui.tv("", 12, Theme.MUTE, Theme.MED);
        fc.addView(Ui.head("Live finds", feedCount));
        feedBox = Ui.v();
        fc.addView(feedBox);
        for (int i = 0; i < 14; i++) {
            FeedRow r = new FeedRow();
            feedRows.add(r);
            feedBox.addView(r.root);
        }
        col.addView(fc);

        return Ui.page(col);
    }

    private TextView mini(LinearLayout parent, int icon, String label) {
        LinearLayout c = Ui.v();
        LinearLayout h = Ui.h();
        h.addView(Ui.icon(icon, Theme.MUTE, 14));
        h.addView(Ui.space(5, 1));
        TextView l = Ui.tv(label, 12, Theme.MUTE, Theme.MED);
        Ui.single(l);
        h.addView(l);
        c.addView(h);
        TextView v = Ui.big("-", 20, Theme.TEXT);
        v.setPadding(0, dp(5), 0, 0);
        c.addView(v);
        parent.addView(c, Ui.lpw(1));
        return v;
    }

    // ======================================================================
    @Override public void tick() {
        if (root == null) return;
        ScanEngine e = ScanEngine.get();
        boolean run = e.running;

        // hero
        btn.set(run, a.hasLocationPerm());
        if (run) {
            heroCap.setText(e.goodFix() ? "WARDRIVING" : "WAITING FOR GPS");
            heroCap.setTextColor(e.goodFix() ? Theme.ACC : Theme.WARN);
            timer.setText(Fmt.dur(e.elapsed()));
            String f = e.csv.fileName();
            heroSub.setText(f.length() > 0 ? f : "Opening log file...");
        } else {
            heroCap.setText(e.startMs > 0 ? "LAST RUN" : "READY");
            heroCap.setTextColor(Theme.MUTE);
            timer.setText(Fmt.dur(e.startMs > 0 ? e.elapsed() : 0));
            String s;
            if (!a.hasLocationPerm()) s = "Allow location to start";
            else if (e.startMs > 0 && RunEnd.lastSummary.length() > 0) s = RunEnd.lastSummary;
            else if (e.goodFix()) s = "GPS locked · " + e.satsUsed + " satellites. Tap to start.";
            else if (e.hasFix()) s = "GPS fix is rough (" + Fmt.acc(e.loc.getAccuracy()) + "). You can start now.";
            else s = "Tap to start. Logging begins at GPS lock.";
            heroSub.setText(s);
        }
        stDist.setText(e.startMs > 0 ? Fmt.dist(e.distanceM) : "-");
        stRows.setText(e.startMs > 0 ? Fmt.num(e.csv.rows()) : "-");
        stRate.setText(run ? Fmt.num(e.lastMinuteCount()) : "-");

        // tiles
        updateTiles(e);

        // spark
        spark.set(e.lastMinutes(30));
        sparkRight.setText(run ? "last 30 min" : e.startMs > 0 ? "last run" : "starts with a run");

        // breakdown
        bandBar.set(e.bands, new int[]{Theme.W24, Theme.W5, Theme.W6});
        legend(bandLegend, new String[]{"2.4 GHz", "5 GHz", "6 GHz"}, e.bands, new int[]{Theme.W24, Theme.W5, Theme.W6});
        int[] secCol = {Theme.BAD, Theme.WARN, Theme.NEW, Theme.W24, Theme.OK};
        secBar.set(e.sec, secCol);
        legend(secLegend, Sighting.SEC_NAMES, e.sec, secCol);
        StringBuilder x = new StringBuilder();
        x.append("Open networks: ").append(Fmt.num(e.openNets));
        if (e.strongestRssi > -200) x.append("\nStrongest: ").append(e.strongestName).append("  ").append(e.strongestRssi).append(" dBm");
        x.append("\nNever seen before on this phone: ").append(Fmt.num(e.newEver));
        extraLine.setText(x.toString());

        // radios
        updateRadios(e);

        // feed
        List<Sighting> f = e.feed(feedRows.size());
        feedCount.setText(e.totalUnique() > 0 ? Fmt.num(e.totalUnique()) + " this run" : "");
        for (int i = 0; i < feedRows.size(); i++) {
            FeedRow r = feedRows.get(i);
            if (i < f.size()) { r.root.setVisibility(View.VISIBLE); r.bind(f.get(i)); }
            else r.root.setVisibility(i == 0 && f.isEmpty() ? View.VISIBLE : View.GONE);
            if (i == 0 && f.isEmpty()) r.empty(run);
        }

        updateBanners(e);
    }

    private void legend(LinearLayout box, String[] names, int[] vals, int[] cols) {
        int total = 0;
        for (int v : vals) total += v;
        if (box.getChildCount() != names.length) {
            box.removeAllViews();
            for (int i = 0; i < names.length; i++) {
                LinearLayout it = Ui.h();
                it.addView(Ui.dot(cols[i], 8));
                it.addView(Ui.space(6, 1));
                TextView t = Ui.tv("", 12, Theme.MUTE, Theme.MED);
                Ui.single(t);
                it.addView(t);
                box.addView(it, Ui.lpw(1));
            }
        }
        for (int i = 0; i < names.length; i++) {
            TextView t = (TextView) ((LinearLayout) box.getChildAt(i)).getChildAt(2);
            String pct = total == 0 ? "" : " " + Math.round(vals[i] * 100f / total) + "%";
            t.setText(names[i] + pct);
        }
    }

    // ---------------- tiles ----------------
    private static final class Tile {
        LinearLayout root;
        TextView label, value, sub;
        View dot;
    }

    private Tile tile(String label, int color, int icon) {
        Tile t = new Tile();
        t.root = Ui.v();
        t.root.setBackground(Ui.round(Theme.CARD, 20));
        t.root.setPadding(dp(14), dp(14), dp(14), dp(14));
        LinearLayout h = Ui.h();
        ImageView ic = Ui.icon(icon, color, 18);
        h.addView(ic);
        h.addView(Ui.space(7, 1));
        t.label = Ui.cap(label);
        Ui.single(t.label);
        h.addView(t.label, Ui.lpw(1));
        t.root.addView(h);
        t.value = Ui.big("0", 36, Theme.HI);
        t.value.setPadding(0, dp(10), 0, dp(4));
        t.root.addView(t.value);
        t.sub = Ui.tv("", 13, Theme.MUTE, Theme.MED);
        Ui.single(t.sub);
        t.root.addView(t.sub);
        return t;
    }

    private void updateTiles(ScanEngine e) {
        boolean bt = Prefs.b(Prefs.BT) || e.uniq[2] > 0;
        boolean cell = Prefs.b(Prefs.CELL) || e.uniq[3] > 0;
        String key = bt + "/" + cell;
        if (!key.equals(tileKey)) {
            tileKey = key;
            tiles.clear();
            tileGrid.removeAllViews();
            tiles.add(tile("Wi-Fi", Theme.W24, Icon.WIFI));
            tiles.add(tile("Bluetooth LE", Theme.BLE, Icon.BLE));
            if (bt) tiles.add(tile("Bluetooth", Theme.BT, Icon.BT));
            if (cell) tiles.add(tile("Cell towers", Theme.CELL, Icon.CELL));
            tiles.add(tile("New to you", Theme.NEW, Icon.SPARK));
            if (tiles.size() % 2 == 1) tiles.add(tile("All-time", Theme.ACC, Icon.TROPHY));
            for (int i = 0; i < tiles.size(); i += 2) {
                LinearLayout r = Ui.h();
                r.addView(tiles.get(i).root, Ui.lpw(1));
                LinearLayout.LayoutParams p = Ui.lpw(1);
                p.leftMargin = dp(12);
                r.addView(tiles.get(i + 1).root, p);
                r.setLayoutParams(Ui.margins(Ui.lp(-1, -2), 0, 0, 0, 12));
                tileGrid.addView(r);
            }
        }
        int last = (int) (e.elapsed() / 60000);
        for (Tile t : tiles) {
            String l = t.label.getText().toString();
            if (l.equals("WI-FI")) {
                t.value.setText(Fmt.num(e.uniq[0]));
                int five = e.bands[1] + e.bands[2];
                t.sub.setText(e.uniq[0] == 0 ? stateShort(e.wifiState, Prefs.b(Prefs.WIFI))
                        : Math.round(five * 100f / Math.max(1, e.uniq[0])) + "% on 5/6 GHz");
            } else if (l.equals("BLUETOOTH LE")) {
                t.value.setText(Fmt.num(e.uniq[1]));
                t.sub.setText(e.uniq[1] == 0 ? stateShort(e.bleState, Prefs.b(Prefs.BLE)) : perMin(e, 1, last));
            } else if (l.equals("BLUETOOTH")) {
                t.value.setText(Fmt.num(e.uniq[2]));
                t.sub.setText(e.uniq[2] == 0 ? stateShort(e.btState, Prefs.b(Prefs.BT)) : perMin(e, 2, last));
            } else if (l.equals("CELL TOWERS")) {
                t.value.setText(Fmt.num(e.uniq[3]));
                t.sub.setText(e.uniq[3] == 0 ? stateShort(e.cellState, Prefs.b(Prefs.CELL)) : "WiGLE only");
            } else if (l.equals("NEW TO YOU")) {
                t.value.setText(Fmt.num(e.newEver));
                t.sub.setText("never logged before");
            } else if (l.equals("ALL-TIME")) {
                t.value.setText(Fmt.num(e.allTimeCount()));
                t.sub.setText("unique on this phone");
            }
        }
    }

    private static String stateShort(String st, boolean enabled) {
        if (!enabled) return "Off in settings";
        if (st == null || st.length() == 0) return ScanEngine.get().running ? "Starting" : "Ready";
        return st;
    }

    private static String perMin(ScanEngine e, int t, int cur) {
        int sum = 0;
        for (int m = Math.max(0, cur - 4); m <= cur && m < ScanEngine.BIN_MAX; m++) sum += e.bins[m][t];
        return "+" + sum + " in 5 min";
    }

    // ---------------- radios ----------------
    private void updateRadios(ScanEngine e) {
        boolean run = e.running;
        String[][] rows = new String[5][];
        int[] cols = new int[5];
        // GPS
        String gps;
        int gc;
        if (!a.hasLocationPerm()) { gps = "No permission"; gc = Theme.BAD; }
        else if (!e.locationServicesOn()) { gps = "Location is off"; gc = Theme.BAD; }
        else if (e.goodFix()) { gps = e.satsUsed + " of " + e.satsInView + " sats · " + Fmt.acc(e.loc.getAccuracy()); gc = Theme.OK; }
        else if (e.hasFix()) { gps = "Rough fix · " + Fmt.acc(e.loc.getAccuracy()); gc = Theme.WARN; }
        else { gps = "Searching · " + e.satsInView + " in view"; gc = Theme.WARN; }
        rows[0] = new String[]{"GPS", gps};
        cols[0] = gc;
        // Wi-Fi
        String w;
        int wc;
        if (!Prefs.b(Prefs.WIFI)) { w = "Off in settings"; wc = Theme.DIM; }
        else if (!run) { w = "Ready"; wc = Theme.MUTE; }
        else if (e.wifiThrottled()) { w = "Limited by Android · " + e.wifiPerTwoMin + " scans / 2 min"; wc = Theme.WARN; }
        else if ("Scanning".equals(e.wifiState)) { w = "Scanning · " + e.wifiPerTwoMin + " scans / 2 min"; wc = Theme.OK; }
        else { w = e.wifiState.length() == 0 ? "Starting" : e.wifiState; wc = e.wifiState.contains("off") ? Theme.BAD : Theme.WARN; }
        rows[1] = new String[]{"Wi-Fi", w};
        cols[1] = wc;
        rows[2] = new String[]{"Bluetooth LE", radioText(Prefs.b(Prefs.BLE), run, e.bleState, e.bleResults)};
        cols[2] = radioCol(Prefs.b(Prefs.BLE), run, e.bleState);
        rows[3] = new String[]{"Bluetooth", radioText(Prefs.b(Prefs.BT), run, e.btState, e.btResults)};
        cols[3] = radioCol(Prefs.b(Prefs.BT), run, e.btState);
        rows[4] = new String[]{"Cell towers", radioText(Prefs.b(Prefs.CELL), run, e.cellState, e.cellResults)};
        cols[4] = radioCol(Prefs.b(Prefs.CELL), run, e.cellState);
        if (radios.getChildCount() != rows.length) {
            radios.removeAllViews();
            for (int i = 0; i < rows.length; i++) {
                LinearLayout r = Ui.h();
                r.setPadding(0, dp(7), 0, dp(7));
                r.addView(Ui.dot(Theme.MUTE, 9));
                r.addView(Ui.space(10, 1));
                TextView n = Ui.tv("", 15, Theme.TEXT, Theme.SEMI);
                r.addView(n, Ui.lp(dp(108), -2));
                TextView s = Ui.tv("", 14, Theme.MUTE, Theme.MED);
                s.setGravity(Gravity.END);
                s.setMaxLines(2);
                r.addView(s, Ui.lpw(1));
                radios.addView(r);
            }
        }
        for (int i = 0; i < rows.length; i++) {
            LinearLayout r = (LinearLayout) radios.getChildAt(i);
            ((GradientDrawable) r.getChildAt(0).getBackground()).setColor(cols[i]);
            ((TextView) r.getChildAt(2)).setText(rows[i][0]);
            ((TextView) r.getChildAt(3)).setText(rows[i][1]);
        }
    }

    private static String radioText(boolean on, boolean run, String st, int results) {
        if (!on) return "Off in settings";
        if (!run) return "Ready";
        if (st == null || st.length() == 0) return "Starting";
        if (st.equals("Scanning") || st.equals("Searching") || st.equals("Listening")) return st + " · " + Fmt.num(results) + " heard";
        return st;
    }

    private static int radioCol(boolean on, boolean run, String st) {
        if (!on) return Theme.DIM;
        if (!run) return Theme.MUTE;
        if (st.equals("Scanning") || st.equals("Searching") || st.equals("Listening") || st.equals("Already scanning")) return Theme.OK;
        if (st.contains("off") || st.contains("No ") || st.contains("Not supported")) return Theme.BAD;
        return Theme.WARN;
    }

    // ---------------- feed rows ----------------
    private final class FeedRow {
        final LinearLayout root;
        final FrameLayout badge;
        final ImageView badgeIcon;
        final TextView name, sub, rssi, newTag;
        final Charts.Bars bars;

        FeedRow() {
            root = Ui.h();
            root.setPadding(0, dp(8), 0, dp(8));
            badge = new FrameLayout(a);
            badgeIcon = new ImageView(a);
            badgeIcon.setPadding(dp(8), dp(8), dp(8), dp(8));
            badge.addView(badgeIcon, new FrameLayout.LayoutParams(-1, -1));
            root.addView(badge, new LinearLayout.LayoutParams(dp(38), dp(38)));
            root.addView(Ui.space(12, 1));
            LinearLayout mid = Ui.v();
            LinearLayout nameRow = Ui.h();
            name = Ui.tv("", 15, Theme.TEXT, Theme.SEMI);
            Ui.single(name);
            nameRow.addView(name, new LinearLayout.LayoutParams(0, -2, 1f));
            newTag = Ui.tv("NEW", 10, Theme.ON_ACC, Theme.BOLD);
            newTag.setLetterSpacing(0.1f);
            newTag.setBackground(Ui.round(Theme.NEW, 6));
            newTag.setPadding(dp(6), dp(2), dp(6), dp(2));
            LinearLayout.LayoutParams np = Ui.lp(-2, -2);
            np.leftMargin = dp(6);
            nameRow.addView(newTag, np);
            mid.addView(nameRow);
            sub = Ui.tv("", 12, Theme.MUTE, Theme.MED);
            Ui.single(sub);
            sub.setPadding(0, dp(4), 0, 0);
            mid.addView(sub);
            root.addView(mid, Ui.lpw(1));
            root.addView(Ui.space(10, 1));
            LinearLayout right = Ui.v();
            right.setGravity(Gravity.END);
            bars = new Charts.Bars(a);
            right.addView(bars, new LinearLayout.LayoutParams(dp(22), dp(14)));
            rssi = Ui.tv("", 11, Theme.MUTE, Theme.SEMI);
            rssi.setPadding(0, dp(4), 0, 0);
            right.addView(rssi);
            root.addView(right);
        }

        void bind(Sighting s) {
            int col;
            int ic;
            switch (s.type) {
                case Sighting.BLE: col = Theme.BLE; ic = Icon.BLE; break;
                case Sighting.BT: col = Theme.BT; ic = Icon.BT; break;
                case Sighting.CELL: col = Theme.CELL; ic = Icon.CELL; break;
                default: col = Sighting.band(s.freq) == 0 ? Theme.W24 : Sighting.band(s.freq) == 1 ? Theme.W5 : Theme.W6;
                    ic = Sighting.isOpen(s.auth) ? Icon.UNLOCK : Icon.WIFI;
            }
            badge.setBackground(Ui.round(Theme.a(col, 38), 12));
            badgeIcon.setImageDrawable(new Icon(ic, col));
            boolean hidden = s.name == null || s.name.length() == 0;
            name.setText(hidden ? (s.type == Sighting.WIFI ? "(hidden network)" : s.mac) : s.name);
            name.setTextColor(hidden ? Theme.MUTE : Theme.TEXT);
            String detail;
            if (s.type == Sighting.WIFI) {
                detail = s.mac + " · ch " + s.channel + " · " + Sighting.SEC_NAMES[Sighting.secClass(s.auth)];
            } else if (s.type == Sighting.CELL) {
                detail = s.typeName() + " · " + s.mac;
            } else {
                detail = (hidden ? "" : s.mac + " · ") + s.typeName() + (s.mfgr >= 0 ? " · " + Vendors.ble(s.mfgr) : "");
            }
            sub.setText(detail);
            newTag.setVisibility(s.newEver ? View.VISIBLE : View.GONE);
            bars.set(s.rssi, col);
            rssi.setText(s.rssi + " dBm");
            bars.setVisibility(View.VISIBLE);
        }

        void empty(boolean run) {
            badge.setBackground(Ui.round(Theme.CARD2, 12));
            badgeIcon.setImageDrawable(new Icon(Icon.SPARK, Theme.MUTE));
            name.setText(run ? "Listening..." : "Nothing yet");
            name.setTextColor(Theme.MUTE);
            sub.setText(run ? "New finds appear here as they're logged" : "Start a run to see finds here live");
            newTag.setVisibility(View.GONE);
            bars.setVisibility(View.GONE);
            rssi.setText("");
        }
    }

    // ---------------- banners ----------------
    private void updateBanners(final ScanEngine e) {
        List<String> keys = new ArrayList<String>();
        if (!a.hasLocationPerm()) keys.add("perm");
        else if (!e.locationServicesOn()) keys.add("loc");
        if ((Prefs.b(Prefs.BLE) || Prefs.b(Prefs.BT)) && btOff()) keys.add("bt");
        if (Prefs.b(Prefs.WIFI) && wifiOff()) keys.add("wifi");
        if (Build.VERSION.SDK_INT >= 31 && (Prefs.b(Prefs.BLE) || Prefs.b(Prefs.BT))
                && !ScanEngine.has(a, Manifest.permission.BLUETOOTH_SCAN) && a.hasLocationPerm()) keys.add("btperm");
        if (e.running && e.wifiThrottled()) keys.add("throttle");
        if (e.running && Prefs.b(Prefs.NEED_FIX) && !e.goodFix()) keys.add("nofix:" + (e.heardNoFix > 0));
        if (e.csv.error().length() > 0) keys.add("logerr");
        String k = keys.toString() + e.heardNoFix / 10 + e.csv.error();
        if (k.equals(bannerKey)) return;
        bannerKey = k;
        banners.removeAllViews();
        for (String key : keys) {
            if (key.equals("perm")) banner(Theme.BAD, Icon.PIN, "Location permission needed",
                    "Every log row needs a GPS position, and Android only gives Wi-Fi and Bluetooth results to apps with location access.",
                    "Allow", new Runnable() { @Override public void run() { a.requestPerms(); } });
            else if (key.equals("loc")) banner(Theme.BAD, Icon.PIN, "Location is off",
                    "Turn on Location in quick settings. Without it Android returns no Wi-Fi scans and no GPS.",
                    "Turn on", new Runnable() { @Override public void run() { a.openLocationSettings(); } });
            else if (key.equals("bt")) banner(Theme.WARN, Icon.BLE, "Bluetooth is off",
                    "Bluetooth scanning is on in settings, but Bluetooth itself is off.",
                    "Turn on", new Runnable() { @Override public void run() { a.enableBluetooth(); } });
            else if (key.equals("btperm")) banner(Theme.WARN, Icon.BLE, "Nearby devices permission is off",
                    "Android needs it before the app can scan for Bluetooth devices.",
                    "Allow", new Runnable() { @Override public void run() { a.requestPerms(); } });
            else if (key.equals("wifi")) banner(Theme.WARN, Icon.WIFI, "Wi-Fi scanning is off",
                    "Turn Wi-Fi on (you don't need to connect to anything), or turn on Wi-Fi scanning in Location settings.",
                    "Open", new Runnable() { @Override public void run() { a.openWifiSettings(); } });
            else if (key.equals("throttle")) banner(Theme.WARN, Icon.ALERT, "Android is limiting Wi-Fi scans",
                    "Apps get about 4 Wi-Fi scans every 2 minutes. Turn off \"Wi-Fi scan throttling\" in Developer options to catch many more networks.",
                    "How to fix", new Runnable() { @Override public void run() { a.throttleHelp(); } });
            else if (key.startsWith("nofix")) banner(Theme.WARN, Icon.TARGET, "Waiting for GPS lock",
                    e.heardNoFix > 0 ? Fmt.num(e.heardNoFix) + " heard so far. Logging starts as soon as the GPS fix is good enough."
                            : "Logging starts as soon as the GPS fix is good enough. Near a window or outside is fastest.",
                    null, null);
            else if (key.equals("logerr")) banner(Theme.BAD, Icon.ALERT, "Log problem", e.csv.error(), null, null);
        }
    }

    private boolean btOff() {
        BluetoothManager bm = (BluetoothManager) a.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter ad = bm == null ? null : bm.getAdapter();
        return ad != null && !ad.isEnabled();
    }

    private boolean wifiOff() {
        WifiManager w = (WifiManager) a.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        return w != null && !w.isWifiEnabled() && !w.isScanAlwaysAvailable();
    }

    private void banner(int color, int icon, String title, String text, String btnText, Runnable onTap) {
        LinearLayout b = Ui.h();
        b.setGravity(Gravity.TOP);
        b.setBackground(Ui.round(Theme.a(color, 30), 18));
        b.setPadding(dp(14), dp(14), dp(14), dp(14));
        b.setLayoutParams(Ui.margins(Ui.lp(-1, -2), 0, 0, 0, 12));
        b.addView(Ui.icon(icon, color, 22));
        b.addView(Ui.space(12, 1));
        LinearLayout tx = Ui.v();
        tx.addView(Ui.tv(title, 16, color, Theme.BOLD));
        TextView m = Ui.tv(text, 14, Theme.TEXT, Theme.MED);
        m.setLineSpacing(0, 1.25f);
        m.setPadding(0, dp(5), 0, 0);
        tx.addView(m);
        if (btnText != null) {
            TextView bt = Ui.tv(btnText.toUpperCase(java.util.Locale.US), 13, Theme.ON_ACC, Theme.BOLD);
            bt.setLetterSpacing(0.08f);
            bt.setBackground(Ui.ripple(Ui.round(color, 10), 10));
            bt.setPadding(dp(14), dp(9), dp(14), dp(9));
            LinearLayout.LayoutParams bp = Ui.lp(-2, -2);
            bp.topMargin = dp(12);
            tx.addView(bt, bp);
            Ui.tap(bt, onTap);
        }
        b.addView(tx, Ui.lpw(1));
        banners.addView(b);
    }
}
