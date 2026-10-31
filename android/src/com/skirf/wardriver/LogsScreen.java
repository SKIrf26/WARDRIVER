package com.skirf.wardriver;

import android.app.Dialog;
import android.content.Intent;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.json.JSONObject;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

import static com.skirf.wardriver.Ui.dp;

public final class LogsScreen extends Screen {
    public LogsScreen(MainActivity a) { super(a); }

    private LinearLayout wigleBox, wdgBox, list, upCard;
    private TextView upTitle, upSub, upStatus;
    private LinearLayout upBtn;
    private List<LogStore.Info> files = new ArrayList<LogStore.Info>();
    private boolean statsAsked;

    @Override protected View build() {
        LinearLayout col = Ui.v();

        // accounts
        LinearLayout acc = Ui.card();
        final View refresh = Ui.icon(Icon.REFRESH, Theme.MUTE, 22);
        Ui.tap(refresh, new Runnable() {
            @Override public void run() {
                refresh.animate().rotationBy(360).setDuration(600).start();
                fetchStats();
            }
        });
        acc.addView(Ui.head("Your accounts", refresh));
        LinearLayout two = Ui.h();
        two.setGravity(Gravity.TOP);
        wigleBox = Ui.v();
        wdgBox = Ui.v();
        two.addView(wigleBox, Ui.lpw(1));
        View sep = new View(a);
        sep.setBackgroundColor(Theme.LINE);
        LinearLayout.LayoutParams sp = Ui.lp(Math.max(1, dp(1)), -1);
        sp.setMargins(dp(14), 0, dp(14), 0);
        two.addView(sep, sp);
        two.addView(wdgBox, Ui.lpw(1));
        acc.addView(two);
        col.addView(acc);

        // upload card
        upCard = Ui.card();
        LinearLayout r = Ui.h();
        r.addView(Ui.icon(Icon.UPLOAD, Theme.ACC, 26));
        r.addView(Ui.space(12, 1));
        LinearLayout tx = Ui.v();
        upTitle = Ui.tv("", 17, Theme.TEXT, Theme.BOLD);
        tx.addView(upTitle);
        upSub = Ui.tv("", 13, Theme.MUTE, Theme.MED);
        upSub.setPadding(0, dp(3), 0, 0);
        tx.addView(upSub);
        r.addView(tx, Ui.lpw(1));
        upCard.addView(r);
        upStatus = Ui.tv("", 13, Theme.ACC, Theme.SEMI);
        upStatus.setPadding(0, dp(10), 0, 0);
        upStatus.setMaxLines(3);
        upCard.addView(upStatus);
        upBtn = Ui.button("Upload new logs", Icon.UPLOAD, Ui.PRIMARY, new Runnable() { @Override public void run() { uploadNew(); } });
        LinearLayout.LayoutParams bp = Ui.lp(-1, -2);
        bp.topMargin = dp(14);
        upCard.addView(upBtn, bp);
        col.addView(upCard);

        // list
        LinearLayout lh = Ui.h();
        lh.setPadding(dp(4), dp(8), dp(4), dp(10));
        lh.addView(Ui.cap("Logs on this phone"), Ui.lpw(1));
        TextView clean = Ui.tv("CLEAN UP", 12, Theme.ACC, Theme.BOLD);
        clean.setLetterSpacing(0.1f);
        clean.setPadding(dp(8), dp(4), 0, dp(4));
        Ui.tap(clean, new Runnable() { @Override public void run() { cleanUp(); } });
        lh.addView(clean);
        col.addView(lh);
        list = Ui.v();
        col.addView(list);
        TextView where = Ui.tv("Files are in Android/data/com.skirf.wardriver/files/logs. Use Save to Downloads or Share to get them off the phone.", 12, Theme.DIM, Theme.MED);
        where.setLineSpacing(0, 1.25f);
        where.setPadding(dp(4), dp(10), dp(4), 0);
        col.addView(where);
        return Ui.page(col);
    }

    @Override public void onShow() {
        wasRunning = ScanEngine.get().running;
        reload();
        if (!statsAsked) { statsAsked = true; fetchStats(); }
        tick();
    }

    private void fetchStats() {
        Uploader.fetchWigleStats(new Uploader.Done() { @Override public void done(Uploader.Result r) { drawStats(); } });
        Uploader.fetchWdgStats(new Uploader.Done() { @Override public void done(Uploader.Result r) { drawStats(); } });
        drawStats();
    }

    // ---------------- stats ----------------
    private void drawStats() {
        if (root == null) return;
        wigleBox.removeAllViews();
        wdgBox.removeAllViews();
        siteHead(wigleBox, "WiGLE");
        siteHead(wdgBox, "WDGWars");
        JSONObject w = Uploader.wigleStats;
        if (!Prefs.hasWigle()) keysMissing(wigleBox);
        else if (w != null) {
            JSONObject s = w.optJSONObject("statistics");
            if (s == null) s = w;
            String user = w.optString("user", s.optString("userName", Prefs.s(Prefs.WIGLE_NAME)));
            stat(wigleBox, "User", user);
            stat(wigleBox, "Rank", rank(s.optLong("rank", w.optLong("rank", 0))));
            stat(wigleBox, "This month", rank(s.optLong("monthRank", w.optLong("monthRank", 0))));
            stat(wigleBox, "Wi-Fi found", Fmt.num(s.optLong("discoveredWiFiGPS", s.optLong("discoveredWiFi", 0))));
            stat(wigleBox, "Bluetooth", Fmt.num(s.optLong("discoveredBtGPS", s.optLong("discoveredBt", 0))));
        } else stat(wigleBox, "", Uploader.wigleStatsErr.length() > 0 ? Uploader.wigleStatsErr : "Loading...");

        JSONObject d = Uploader.wdgStats;
        if (!Prefs.hasWdg()) keysMissing(wdgBox);
        else if (d != null) {
            stat(wdgBox, "User", d.optString("username", "?"));
            String gang = d.optString("gang", "");
            if (gang.length() > 0 && !gang.equals("null")) stat(wdgBox, "Gang", gang);
            stat(wdgBox, "Total", Fmt.num(d.optLong("total", 0)));
            stat(wdgBox, "Wi-Fi / BLE", Fmt.num(d.optLong("wifi", 0)) + " / " + Fmt.num(d.optLong("ble", 0)));
            stat(wdgBox, "Today / 7 days", Fmt.num(d.optLong("recent_today", 0)) + " / " + Fmt.num(d.optLong("recent_7d", 0)));
        } else stat(wdgBox, "", Uploader.wdgStatsErr.length() > 0 ? Uploader.wdgStatsErr : "Loading...");
    }

    private static String rank(long r) { return r > 0 ? "#" + Fmt.num(r) : "-"; }

    private void siteHead(LinearLayout box, String name) {
        TextView t = Ui.tv(name, 16, Theme.HI, Theme.BOLD);
        t.setPadding(0, 0, 0, dp(8));
        box.addView(t);
    }

    private void stat(LinearLayout box, String label, String value) {
        if (label.length() > 0) {
            TextView l = Ui.tv(label, 12, Theme.MUTE, Theme.MED);
            l.setPadding(0, dp(6), 0, dp(1));
            box.addView(l);
            TextView v = Ui.big(value, 16, Theme.TEXT);
            Ui.single(v);
            box.addView(v);
        } else {
            TextView v = Ui.tv(value, 13, Theme.MUTE, Theme.MED);
            v.setLineSpacing(0, 1.2f);
            box.addView(v);
        }
    }

    private void keysMissing(LinearLayout box) {
        TextView t = Ui.tv("No key yet", 14, Theme.MUTE, Theme.MED);
        box.addView(t);
        TextView go = Ui.tv("ADD IN SETTINGS", 12, Theme.ACC, Theme.BOLD);
        go.setLetterSpacing(0.08f);
        go.setPadding(0, dp(10), 0, dp(4));
        Ui.tap(go, new Runnable() { @Override public void run() { a.showTab(MainActivity.TAB_SETTINGS); } });
        box.addView(go);
    }

    // ---------------- list ----------------
    private void reload() {
        if (root == null) return;
        files = LogStore.list();
        list.removeAllViews();
        if (files.isEmpty()) {
            LinearLayout e = Ui.card();
            e.setGravity(Gravity.CENTER_HORIZONTAL);
            e.setPadding(dp(18), dp(28), dp(18), dp(28));
            e.addView(Ui.icon(Icon.LOGS, Theme.DIM, 40));
            TextView t = Ui.tv("No logs yet", 18, Theme.TEXT, Theme.BOLD);
            t.setPadding(0, dp(12), 0, dp(6));
            e.addView(t);
            TextView s = Ui.tv("Each run saves one WiGLE-format CSV file here.", 14, Theme.MUTE, Theme.MED);
            s.setGravity(Gravity.CENTER);
            e.addView(s);
            list.addView(e);
        }
        for (final LogStore.Info i : files) list.addView(row(i));
        // count rows in the background
        for (final LogStore.Info i : files) {
            if (i.rows >= 0) continue;
            App.io.post(new Runnable() {
                @Override public void run() {
                    LogStore.count(i);
                    App.main.post(new Runnable() { @Override public void run() { refreshRowText(i); } });
                }
            });
        }
        updateUploadCard();
    }

    private View row(final LogStore.Info i) {
        LinearLayout r = Ui.h();
        r.setTag(i.name);
        r.setBackground(Ui.ripple(Ui.round(Theme.CARD, 18), 18));
        r.setPadding(dp(14), dp(14), dp(12), dp(14));
        r.setLayoutParams(Ui.margins(Ui.lp(-1, -2), 0, 0, 0, 8));
        boolean live = LogStore.isLive(i);
        android.widget.FrameLayout ic = new android.widget.FrameLayout(a);
        ic.setBackground(Ui.round(Theme.a(live ? Theme.BAD : Theme.ACC, 34), 12));
        android.widget.ImageView iv = new android.widget.ImageView(a);
        iv.setImageDrawable(new Icon(live ? Icon.SPARK : Icon.LOGS, live ? Theme.BAD : Theme.ACC));
        iv.setPadding(dp(9), dp(9), dp(9), dp(9));
        ic.addView(iv, new android.widget.FrameLayout.LayoutParams(-1, -1));
        r.addView(ic, new LinearLayout.LayoutParams(dp(42), dp(42)));
        r.addView(Ui.space(12, 1));
        LinearLayout tx = Ui.v();
        TextView n = Ui.tv(niceName(i), 16, Theme.TEXT, Theme.SEMI);
        Ui.single(n);
        tx.addView(n);
        TextView s = Ui.tv(subText(i), 13, Theme.MUTE, Theme.MED);
        s.setTag("sub");
        Ui.single(s);
        s.setPadding(0, dp(4), 0, 0);
        tx.addView(s);
        r.addView(tx, Ui.lpw(1));
        LinearLayout chips = Ui.v();
        chips.setGravity(Gravity.END);
        if (live) chips.addView(Ui.chip("RECORDING", Theme.BAD));
        else {
            chips.addView(miniChip("WiGLE", i.wigle));
            View g = Ui.space(1, 4);
            chips.addView(g);
            chips.addView(miniChip("WDGW", i.wdg));
        }
        r.addView(chips);
        Ui.tap(r, new Runnable() { @Override public void run() { openFile(i); } });
        return r;
    }

    private TextView miniChip(String name, boolean done) {
        TextView t = Ui.tv((done ? "✓ " : "") + name, 11, done ? Theme.OK : Theme.DIM, Theme.BOLD);
        t.setLetterSpacing(0.05f);
        t.setPadding(dp(8), dp(3), dp(8), dp(3));
        t.setBackground(Ui.round(done ? Theme.a(Theme.OK, 30) : Theme.CARD2, 99));
        return t;
    }

    private static String niceName(LogStore.Info i) {
        return new SimpleDateFormat("EEE d MMM · HH:mm", Locale.getDefault()).format(new Date(i.modified));
    }

    private static String subText(LogStore.Info i) {
        String rows = i.rows < 0 ? "counting..." : Fmt.num(i.rows) + " rows";
        return rows + " · " + LogStore.size(i.size) + " · " + i.name;
    }

    private void refreshRowText(LogStore.Info i) {
        for (int k = 0; k < list.getChildCount(); k++) {
            View v = list.getChildAt(k);
            if (i.name.equals(v.getTag())) {
                TextView s = (TextView) v.findViewWithTag("sub");
                if (s != null) s.setText(subText(i));
            }
        }
    }

    // ---------------- upload ----------------
    private int sitesWanted() {
        int s = 0;
        if (Prefs.hasWigle()) s |= 1;
        if (Prefs.hasWdg()) s |= 2;
        return s;
    }

    private List<File> notUploaded(int sites) {
        List<File> out = new ArrayList<File>();
        for (LogStore.Info i : files) {
            if (LogStore.isLive(i)) continue;
            if (((sites & 1) != 0 && !i.wigle) || ((sites & 2) != 0 && !i.wdg)) out.add(i.file);
        }
        return out;
    }

    private void updateUploadCard() {
        int sites = sitesWanted();
        if (sites == 0) {
            upTitle.setText("Add your keys to upload");
            upSub.setText("WiGLE and WDGWars keys go in Settings → Uploads");
            upBtn.setVisibility(View.GONE);
            return;
        }
        int n = notUploaded(sites).size();
        upTitle.setText(n == 0 ? "All caught up" : n + " log" + (n == 1 ? "" : "s") + " to upload");
        String to = (sites & 1) != 0 && (sites & 2) != 0 ? "WiGLE and WDGWars" : (sites & 1) != 0 ? "WiGLE" : "WDGWars";
        upSub.setText("Sends to " + to);
        upBtn.setVisibility(n == 0 && !Uploader.busy ? View.GONE : View.VISIBLE);
        upBtn.setAlpha(Uploader.busy ? 0.5f : 1f);
    }

    private void uploadNew() {
        if (Uploader.busy) { Ui.toast("Upload already running"); return; }
        int sites = sitesWanted();
        List<File> todo = notUploaded(sites);
        if (todo.isEmpty()) { Ui.toast("Nothing new to upload"); return; }
        if (!RunEnd.online(a, false)) { Ui.toast("No internet connection"); return; }
        runUpload(todo, sites, false);
    }

    void runUpload(List<File> todo, int sites, boolean force) {
        upStatus.setTextColor(Theme.ACC);
        upStatus.setText("Starting upload...");
        Uploader.uploadMany(todo, sites, force, new Uploader.Progress() {
            @Override public void step(String status, int index, int total) {
                upStatus.setTextColor(Theme.ACC);
                upStatus.setText("Uploading " + index + " of " + total + " · " + status);
                updateUploadCard();
            }
            @Override public void finished(int ok, int failed, String lastError) {
                if (failed == 0) {
                    upStatus.setTextColor(Theme.OK);
                    upStatus.setText(ok == 0 ? "Nothing needed sending" : "Done: " + ok + " upload" + (ok == 1 ? "" : "s") + " worked");
                    Uploader.fetchWigleStats(new Uploader.Done() { @Override public void done(Uploader.Result r) { drawStats(); } });
                    Uploader.fetchWdgStats(new Uploader.Done() { @Override public void done(Uploader.Result r) { drawStats(); } });
                } else {
                    upStatus.setTextColor(Theme.BAD);
                    upStatus.setText(ok + " worked, " + failed + " failed. " + lastError);
                }
                reload();
            }
        });
        updateUploadCard();
    }

    private void cleanUp() {
        final int sites = sitesWanted();
        final List<LogStore.Info> del = new ArrayList<LogStore.Info>();
        for (LogStore.Info i : files) {
            if (LogStore.isLive(i)) continue;
            boolean done = sites != 0 && ((sites & 1) == 0 || i.wigle) && ((sites & 2) == 0 || i.wdg);
            if (done) del.add(i);
        }
        if (del.isEmpty()) {
            Ui.info("Nothing to clean up", "Clean up deletes logs that are already uploaded to every site you have a key for. None are yet.");
            return;
        }
        Ui.confirm("Delete " + del.size() + " uploaded log" + (del.size() == 1 ? "" : "s") + "?",
                "These are already on " + ((sites & 1) != 0 && (sites & 2) != 0 ? "WiGLE and WDGWars" : (sites & 1) != 0 ? "WiGLE" : "WDGWars")
                        + ". They will be removed from this phone. This can't be undone.",
                "Delete", Ui.DANGER, new Runnable() {
                    @Override public void run() {
                        int n = 0;
                        for (LogStore.Info i : del) if (LogStore.delete(i)) n++;
                        Ui.toast("Deleted " + n + " log" + (n == 1 ? "" : "s"));
                        reload();
                    }
                });
    }

    // ---------------- file sheet ----------------
    private void openFile(final LogStore.Info i) {
        final boolean live = LogStore.isLive(i);
        LinearLayout c = Ui.v();
        c.addView(Ui.tv(niceName(i), 22, Theme.TEXT, Theme.BOLD));
        TextView fn = Ui.tv(i.name + " · " + LogStore.size(i.size), 13, Theme.MUTE, Theme.MED);
        fn.setPadding(0, dp(4), 0, dp(14));
        c.addView(fn);
        final LinearLayout counts = Ui.h();
        counts.setBackground(Ui.round(Theme.BG, 16));
        counts.setPadding(dp(14), dp(12), dp(14), dp(12));
        c.addView(counts);
        fillCounts(counts, i);
        if (i.rows < 0 || live) {
            App.io.post(new Runnable() {
                @Override public void run() {
                    LogStore.count(i);
                    App.main.post(new Runnable() { @Override public void run() { fillCounts(counts, i); refreshRowText(i); } });
                }
            });
        }
        final TextView status = Ui.tv("", 14, Theme.ACC, Theme.SEMI);
        status.setPadding(0, dp(12), 0, 0);
        status.setMaxLines(4);
        c.addView(status);
        c.addView(Ui.space(1, 12));
        final Dialog[] d = new Dialog[1];

        if (live) {
            TextView note = Ui.body("This is the run that's recording now. Stop the run to upload, share or delete it.");
            note.setPadding(0, 0, 0, dp(14));
            c.addView(note);
        }
        LinearLayout up1 = Ui.button(i.wigle ? "WiGLE again" : "Upload WiGLE", Icon.UPLOAD, i.wigle ? Ui.SECONDARY : Ui.PRIMARY, new Runnable() {
            @Override public void run() { uploadOne(i, 1, status); }
        });
        LinearLayout up2 = Ui.button(i.wdg ? "WDGW again" : "Upload WDGW", Icon.UPLOAD, i.wdg ? Ui.SECONDARY : Ui.PRIMARY, new Runnable() {
            @Override public void run() { uploadOne(i, 2, status); }
        });
        LinearLayout map = Ui.button("Map", Icon.MAP, Ui.SECONDARY, new Runnable() {
            @Override public void run() {
                d[0].dismiss();
                showOnMap(i);
            }
        });
        LinearLayout share = Ui.button("Share", Icon.SHARE, Ui.SECONDARY, new Runnable() {
            @Override public void run() { share(i); }
        });
        LinearLayout save = Ui.button("Save to Downloads", Icon.DOWNLOAD, Ui.SECONDARY, new Runnable() {
            @Override public void run() { saveDl(i, status); }
        });
        LinearLayout del = Ui.button("Delete", Icon.TRASH, Ui.DANGER, new Runnable() {
            @Override public void run() {
                Ui.confirm("Delete this log?", i.name + " will be removed from this phone."
                        + (!i.wigle && !i.wdg ? " It hasn't been uploaded anywhere." : "") + " This can't be undone.",
                        "Delete", Ui.DANGER, new Runnable() {
                            @Override public void run() {
                                if (LogStore.delete(i)) { d[0].dismiss(); Ui.toast("Deleted"); reload(); }
                                else Ui.toast("Couldn't delete it");
                            }
                        });
            }
        });
        if (!live) {
            c.addView(Ui.row(up1, up2));
            c.addView(Ui.space(1, 8));
        }
        c.addView(Ui.row(map, share));
        if (!live) {
            c.addView(Ui.space(1, 8));
            c.addView(save);
            c.addView(Ui.space(1, 8));
            c.addView(del);
        }
        d[0] = Ui.sheet(c);
        d[0].show();
    }

    private void fillCounts(LinearLayout box, LogStore.Info i) {
        box.removeAllViews();
        String[] names = {"Wi-Fi", "BLE", "BT", "Cell"};
        int[] cols = {Theme.W24, Theme.BLE, Theme.BT, Theme.CELL};
        LinearLayout tot = Ui.v();
        tot.addView(Ui.tv("Rows", 12, Theme.MUTE, Theme.MED));
        tot.addView(Ui.big(i.rows < 0 ? "..." : Fmt.num(i.rows), 20, Theme.HI));
        box.addView(tot, Ui.lpw(1.2f));
        for (int k = 0; k < 4; k++) {
            if (k >= 2 && i.byType[k] == 0) continue;
            LinearLayout v = Ui.v();
            LinearLayout h = Ui.h();
            h.addView(Ui.dot(cols[k], 7));
            h.addView(Ui.space(5, 1));
            h.addView(Ui.tv(names[k], 12, Theme.MUTE, Theme.MED));
            v.addView(h);
            v.addView(Ui.big(i.rows < 0 ? "..." : Fmt.num(i.byType[k]), 20, Theme.TEXT));
            box.addView(v, Ui.lpw(1));
        }
    }

    private void uploadOne(final LogStore.Info i, final int site, final TextView status) {
        if (Uploader.busy) { status.setText("Another upload is running"); return; }
        if ((site == 1 && !Prefs.hasWigle()) || (site == 2 && !Prefs.hasWdg())) {
            status.setTextColor(Theme.WARN);
            status.setText("Add your " + (site == 1 ? "WiGLE API name and key" : "WDGWars API key") + " in Settings → Uploads first");
            return;
        }
        status.setTextColor(Theme.ACC);
        status.setText("Uploading to " + (site == 1 ? "WiGLE" : "WDGWars") + "...");
        List<File> one = new ArrayList<File>();
        one.add(i.file);
        Uploader.uploadMany(one, site, true, new Uploader.Progress() {
            @Override public void step(String s, int index, int total) { }
            @Override public void finished(int ok, int failed, String lastError) {
                if (failed == 0 && ok > 0) {
                    status.setTextColor(Theme.OK);
                    status.setText("Uploaded to " + (site == 1 ? "WiGLE" : "WDGWars"));
                } else {
                    status.setTextColor(Theme.BAD);
                    status.setText(lastError.length() > 0 ? lastError : "Upload failed");
                }
                reload();
            }
        });
    }

    private void showOnMap(final LogStore.Info i) {
        Ui.toast("Loading map...");
        App.io.post(new Runnable() {
            @Override public void run() {
                final double[] pts = LogStore.points(i.file, 200000);
                App.main.post(new Runnable() {
                    @Override public void run() {
                        if (pts.length == 0) { Ui.toast("No positions in this log"); return; }
                        a.showTab(MainActivity.TAB_MAP);
                        a.map.showFile(niceName(i), pts);
                    }
                });
            }
        });
    }

    private void share(LogStore.Info i) {
        try {
            Intent s = new Intent(Intent.ACTION_SEND);
            s.setType("text/csv");
            s.putExtra(Intent.EXTRA_STREAM, LogProvider.uri(i.file));
            s.putExtra(Intent.EXTRA_SUBJECT, "Wardrive log " + i.name);
            s.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            s.setClipData(android.content.ClipData.newRawUri(i.name, LogProvider.uri(i.file)));
            a.startActivity(Intent.createChooser(s, "Share " + i.name));
        } catch (Throwable t) {
            Ui.toast("Couldn't share: " + t.getMessage());
        }
    }

    private void saveDl(final LogStore.Info i, final TextView status) {
        if (android.os.Build.VERSION.SDK_INT < 29 && !ScanEngine.has(a, android.Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
            a.requestPermissions(new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, 7);
            status.setText("Allow storage, then tap Save again");
            return;
        }
        status.setTextColor(Theme.ACC);
        status.setText("Saving...");
        App.io.post(new Runnable() {
            @Override public void run() {
                final String err = LogStore.copyToDownloads(i.file);
                App.main.post(new Runnable() {
                    @Override public void run() {
                        if (err == null) { status.setTextColor(Theme.OK); status.setText("Saved to Downloads / SKIrf Wardriver"); }
                        else { status.setTextColor(Theme.BAD); status.setText("Couldn't save: " + err); }
                    }
                });
            }
        });
    }

    private boolean wasRunning;

    @Override public void tick() {
        if (root == null) return;
        boolean run = ScanEngine.get().running;
        if (run != wasRunning) {
            wasRunning = run;
            App.main.postDelayed(new Runnable() { @Override public void run() { reload(); } }, 800);
        }
        if (Uploader.busy) upBtn.setAlpha(0.5f);
        else updateUploadCard();
    }
}
