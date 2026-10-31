package com.skirf.wardriver;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.location.Location;
import android.net.Uri;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.Locale;

import static com.skirf.wardriver.Ui.dp;

public final class GpsScreen extends Screen {
    public GpsScreen(MainActivity a) { super(a); }

    private TextView state, stateSub, lat, lon, coordNote;
    private TextView spd, spdU, alt, altU, hdg, hdgU, acc, accU;
    private Charts.SkyView sky;
    private Charts.SnrView snr;
    private LinearLayout consBox;

    @Override protected View build() {
        LinearLayout col = Ui.v();

        // status
        LinearLayout st = Ui.card();
        LinearLayout r = Ui.h();
        r.addView(Ui.icon(Icon.SAT, Theme.ACC, 30));
        r.addView(Ui.space(14, 1));
        LinearLayout tx = Ui.v();
        state = Ui.tv("SEARCHING", 26, Theme.WARN, Theme.BOLD);
        state.setLetterSpacing(0.04f);
        tx.addView(state);
        stateSub = Ui.tv("", 14, Theme.MUTE, Theme.MED);
        stateSub.setPadding(0, dp(4), 0, 0);
        tx.addView(stateSub);
        r.addView(tx, Ui.lpw(1));
        st.addView(r);
        col.addView(st);

        // coordinates
        LinearLayout co = Ui.card();
        co.addView(Ui.head("Position", null));
        lat = Ui.big("-", 26, Theme.HI);
        lon = Ui.big("-", 26, Theme.HI);
        lon.setPadding(0, dp(6), 0, 0);
        co.addView(lat);
        co.addView(lon);
        coordNote = Ui.tv("", 13, Theme.MUTE, Theme.MED);
        coordNote.setPadding(0, dp(8), 0, dp(14));
        co.addView(coordNote);
        co.addView(Ui.row(
                Ui.button("Copy", Icon.COPY, Ui.SECONDARY, new Runnable() { @Override public void run() { copy(); } }),
                Ui.button("Open map", Icon.MAP, Ui.SECONDARY, new Runnable() { @Override public void run() { openMap(); } })));
        col.addView(co);

        // motion tiles
        LinearLayout t1 = Ui.h();
        TextView[] a1 = tile(t1, "Speed", Icon.BOLT, true);
        TextView[] a2 = tile(t1, "Altitude", Icon.SPARK, false);
        spd = a1[0]; spdU = a1[1]; alt = a2[0]; altU = a2[1];
        t1.setLayoutParams(Ui.margins(Ui.lp(-1, -2), 0, 0, 0, 12));
        col.addView(t1);
        LinearLayout t2 = Ui.h();
        TextView[] a3 = tile(t2, "Heading", Icon.LOCATE, true);
        TextView[] a4 = tile(t2, "Accuracy", Icon.TARGET, false);
        hdg = a3[0]; hdgU = a3[1]; acc = a4[0]; accU = a4[1];
        t2.setLayoutParams(Ui.margins(Ui.lp(-1, -2), 0, 0, 0, 12));
        col.addView(t2);

        // sky plot
        LinearLayout sk = Ui.card();
        sk.addView(Ui.head("Satellites in the sky", Ui.tv("filled = used for the fix", 12, Theme.MUTE, Theme.MED)));
        sky = new Charts.SkyView(a);
        sk.addView(sky, Ui.MW);
        consBox = Ui.v();
        consBox.setPadding(0, dp(12), 0, 0);
        sk.addView(consBox);
        col.addView(sk);

        // signal bars
        LinearLayout sn = Ui.card();
        sn.addView(Ui.head("Signal strength (dB-Hz)", null));
        snr = new Charts.SnrView(a);
        sn.addView(snr, new LinearLayout.LayoutParams(-1, dp(130)));
        TextView hint = Ui.tv("30+ is good. Under 20 means the sky is blocked: try a window or the dashboard.", 13, Theme.MUTE, Theme.MED);
        hint.setPadding(0, dp(12), 0, 0);
        hint.setLineSpacing(0, 1.25f);
        sn.addView(hint);
        col.addView(sn);
        return Ui.page(col);
    }

    private TextView[] tile(LinearLayout row, String label, int icon, boolean first) {
        LinearLayout t = Ui.v();
        t.setBackground(Ui.round(Theme.CARD, 20));
        t.setPadding(dp(14), dp(14), dp(14), dp(14));
        LinearLayout h = Ui.h();
        h.addView(Ui.icon(icon, Theme.MUTE, 16));
        h.addView(Ui.space(7, 1));
        h.addView(Ui.cap(label));
        t.addView(h);
        LinearLayout v = Ui.h();
        v.setGravity(android.view.Gravity.BOTTOM);
        TextView big = Ui.big("-", 32, Theme.HI);
        v.addView(big);
        TextView unit = Ui.tv("", 14, Theme.MUTE, Theme.SEMI);
        unit.setPadding(dp(6), 0, 0, dp(4));
        v.addView(unit);
        v.setPadding(0, dp(10), 0, 0);
        t.addView(v);
        LinearLayout.LayoutParams p = Ui.lpw(1);
        if (!first) p.leftMargin = dp(12);
        row.addView(t, p);
        return new TextView[]{big, unit};
    }

    @Override public void tick() {
        if (root == null) return;
        ScanEngine e = ScanEngine.get();
        Location l = e.loc;
        if (!a.hasLocationPerm()) {
            state.setText("NO PERMISSION");
            state.setTextColor(Theme.BAD);
            stateSub.setText("Allow location access to use GPS");
        } else if (!e.locationServicesOn()) {
            state.setText("LOCATION OFF");
            state.setTextColor(Theme.BAD);
            stateSub.setText("Turn on Location in quick settings");
        } else if (e.goodFix()) {
            state.setText(e.locIsGps ? "LOCKED" : "NETWORK FIX");
            state.setTextColor(e.locIsGps ? Theme.OK : Theme.WARN);
            String ttff = e.firstFixMs > 0 ? " · first fix in " + (e.firstFixMs / 1000) + " s" : "";
            stateSub.setText(e.satsUsed + " of " + e.satsInView + " satellites used" + ttff);
        } else if (e.hasFix()) {
            state.setText("ROUGH FIX");
            state.setTextColor(Theme.WARN);
            stateSub.setText("Accuracy " + Fmt.acc(l.getAccuracy()) + " is worse than your limit in Settings");
        } else {
            state.setText("SEARCHING");
            state.setTextColor(Theme.WARN);
            long s = e.gpsStartedAt > 0 ? (android.os.SystemClock.elapsedRealtime() - e.gpsStartedAt) / 1000 : 0;
            stateSub.setText(e.satsInView + " satellites in view · " + s + " s");
        }
        if (l != null) {
            lat.setText(Fmt.coord(l.getLatitude(), true));
            lon.setText(Fmt.coord(l.getLongitude(), false));
            long age = e.fixAge() / 1000;
            coordNote.setText((age <= 2 ? "Updated just now" : "Updated " + age + " s ago") + (e.locIsGps ? "" : " · from the mobile network"));
            spd.setText(l.hasSpeed() ? Fmt.speed(l.getSpeed()) : "-");
            alt.setText(l.hasAltitude() ? Fmt.alt(l.getAltitude()) : "-");
            hdg.setText(l.hasBearing() && l.hasSpeed() && l.getSpeed() > 0.8f ? String.format(Locale.US, "%.0f°", l.getBearing()) : "-");
            hdgU.setText(l.hasBearing() && l.hasSpeed() && l.getSpeed() > 0.8f ? Fmt.compass(l.getBearing()) : "");
            acc.setText(l.hasAccuracy() ? Fmt.acc(l.getAccuracy()).substring(1).split(" ")[0] : "-");
            accU.setText(Fmt.altUnit());
            sky.heading(l.hasBearing() && l.hasSpeed() && l.getSpeed() > 0.8f ? l.getBearing() : Float.NaN);
        } else {
            lat.setText("-");
            lon.setText("-");
            coordNote.setText("No position yet");
            spd.setText("-"); alt.setText("-"); hdg.setText("-"); hdgU.setText(""); acc.setText("-");
        }
        spdU.setText(Fmt.speedUnit());
        altU.setText(Fmt.altUnit());
        sky.invalidate();
        snr.invalidate();
        updateCons(e);
    }

    private void updateCons(ScanEngine e) {
        int[] inView = new int[8], used = new int[8];
        for (int i = 0; i < e.satN; i++) {
            int c = Math.max(0, Math.min(7, e.satCons[i]));
            inView[c]++;
            if (e.satUsed[i]) used[c]++;
        }
        consBox.removeAllViews();
        LinearLayout row = null;
        int k = 0;
        for (int c = 1; c < 8; c++) {
            if (inView[c] == 0) continue;
            if (k % 2 == 0) {
                row = Ui.h();
                row.setPadding(0, dp(4), 0, dp(4));
                consBox.addView(row);
            }
            LinearLayout it = Ui.h();
            it.addView(Ui.dot(Charts.consColor(c), 10));
            it.addView(Ui.space(8, 1));
            TextView n = Ui.tv(Charts.consName(c), 14, Theme.TEXT, Theme.SEMI);
            it.addView(n);
            TextView v = Ui.tv("  " + used[c] + "/" + inView[c], 14, Theme.MUTE, Theme.MED);
            it.addView(v);
            row.addView(it, Ui.lpw(1));
            k++;
        }
        if (k % 2 == 1 && row != null) row.addView(Ui.space(1, 1), Ui.lpw(1));
    }

    private void copy() {
        Location l = ScanEngine.get().loc;
        if (l == null) { Ui.toast("No position yet"); return; }
        String s = String.format(Locale.US, "%.6f, %.6f", l.getLatitude(), l.getLongitude());
        ClipboardManager cm = (ClipboardManager) a.getSystemService(Context.CLIPBOARD_SERVICE);
        if (cm != null) cm.setPrimaryClip(ClipData.newPlainText("Position", s));
        Ui.toast("Copied " + s);
    }

    private void openMap() {
        Location l = ScanEngine.get().loc;
        if (l == null) { Ui.toast("No position yet"); return; }
        String q = String.format(Locale.US, "%.6f,%.6f", l.getLatitude(), l.getLongitude());
        try {
            a.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("geo:" + q + "?q=" + q)));
        } catch (Throwable t) {
            Ui.toast("No map app found");
        }
    }
}
