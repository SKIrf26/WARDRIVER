package com.skirf.wardriver;

import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import static com.skirf.wardriver.Ui.dp;

public final class MapScreen extends Screen {
    public MapScreen(MainActivity a) { super(a); }

    private MapView map;
    private TextView title, sub, tileNote;
    private FrameLayout close, followBtn, layersBtn;
    private LinearLayout legend;
    private final TextView[] legendText = new TextView[4];

    @Override protected View build() {
        FrameLayout f = new FrameLayout(a);
        f.setPadding(dp(14), dp(6), dp(14), dp(14));
        FrameLayout clip = new FrameLayout(a);
        clip.setBackground(Ui.round(Theme.CARD, 22));
        clip.setClipToOutline(true);
        map = new MapView(a);
        map.tiles = Prefs.b(Prefs.MAP_TILES);
        clip.addView(map, new FrameLayout.LayoutParams(-1, -1));
        f.addView(clip, new FrameLayout.LayoutParams(-1, -1));

        // title chip (top left)
        LinearLayout tc = Ui.h();
        tc.setBackground(Ui.round(0xE60B0D12, 16));
        tc.setPadding(dp(14), dp(10), dp(8), dp(10));
        tc.setElevation(dp(4));
        LinearLayout tx = Ui.v();
        title = Ui.tv("This run", 15, Theme.TEXT, Theme.BOLD);
        Ui.single(title);
        tx.addView(title);
        sub = Ui.tv("", 12, Theme.MUTE, Theme.MED);
        Ui.single(sub);
        sub.setPadding(0, dp(3), 0, 0);
        tx.addView(sub);
        tc.addView(tx, Ui.lp(-2, -2));
        close = Ui.roundBtn(Icon.CLOSE, 32, new Runnable() {
            @Override public void run() { map.showLive(); tick(); }
        });
        close.setElevation(0);
        LinearLayout.LayoutParams cp = Ui.lp(dp(32), dp(32));
        cp.leftMargin = dp(8);
        tc.addView(close, cp);
        FrameLayout.LayoutParams tp = new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.START);
        tp.setMargins(dp(12), dp(12), dp(72), 0);
        f.addView(tc, tp);

        // controls (right)
        LinearLayout ctl = Ui.v();
        followBtn = Ui.roundBtn(Icon.LOCATE, 46, new Runnable() {
            @Override public void run() {
                if (map.isFile()) map.showLive();
                map.follow = true;
                map.invalidate();
                tick();
            }
        });
        ctl.addView(followBtn);
        ctl.addView(Ui.space(1, 10));
        ctl.addView(Ui.roundBtn(Icon.FIT, 46, new Runnable() { @Override public void run() { map.follow = false; map.fit(); tick(); } }));
        ctl.addView(Ui.space(1, 10));
        ctl.addView(Ui.roundBtn(Icon.PLUS, 46, new Runnable() { @Override public void run() { map.zoomBy(1); } }));
        ctl.addView(Ui.space(1, 10));
        ctl.addView(Ui.roundBtn(Icon.MINUS, 46, new Runnable() { @Override public void run() { map.zoomBy(-1); } }));
        ctl.addView(Ui.space(1, 10));
        layersBtn = Ui.roundBtn(Icon.LAYERS, 46, new Runnable() {
            @Override public void run() {
                map.tiles = !map.tiles;
                Prefs.put(Prefs.MAP_TILES, map.tiles);
                map.invalidate();
                Ui.toast(map.tiles ? "Street map on" : "Street map off (route and finds only, no data used)");
                tick();
            }
        });
        ctl.addView(layersBtn);
        FrameLayout.LayoutParams cpp = new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.END);
        cpp.setMargins(0, dp(12), dp(12), 0);
        f.addView(ctl, cpp);

        // legend (bottom): tap a type to hide/show it
        legend = Ui.h();
        legend.setBackground(Ui.round(0xE60B0D12, 16));
        legend.setPadding(dp(6), dp(6), dp(6), dp(6));
        legend.setElevation(dp(4));
        String[] names = {"Wi-Fi", "BLE", "BT", "Cell"};
        for (int i = 0; i < 4; i++) {
            final int t = i;
            LinearLayout it = Ui.h();
            it.setPadding(dp(10), dp(7), dp(10), dp(7));
            it.addView(Ui.dot(Charts.Spark.colorFor(i), 10));
            it.addView(Ui.space(6, 1));
            TextView l = Ui.tv(names[i], 13, Theme.TEXT, Theme.SEMI);
            Ui.single(l);
            legendText[i] = l;
            it.addView(l);
            Ui.tap(it, new Runnable() {
                @Override public void run() {
                    map.showType[t] = !map.showType[t];
                    map.invalidate();
                    tick();
                }
            });
            legend.addView(it);
        }
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(-2, -2, Gravity.BOTTOM | Gravity.END);
        lp.setMargins(0, 0, dp(12), dp(30));
        f.addView(legend, lp);

        tileNote = Ui.tv("", 12, Theme.WARN, Theme.SEMI);
        tileNote.setBackground(Ui.round(0xE60B0D12, 10));
        tileNote.setPadding(dp(10), dp(6), dp(10), dp(6));
        FrameLayout.LayoutParams np = new FrameLayout.LayoutParams(-2, -2, Gravity.BOTTOM | Gravity.START);
        np.setMargins(dp(12), 0, 0, dp(84));
        f.addView(tileNote, np);
        return f;
    }

    /** Show a saved log on the map (from the Logs tab). */
    public void showFile(String name, double[] pts) {
        view();
        map.showFile(name, pts);
        tick();
    }

    @Override public void onShow() {
        map.invalidate();
        tick();
    }

    @Override public void tick() {
        if (root == null) return;
        ScanEngine e = ScanEngine.get();
        int[] counts = new int[4];
        if (map.isFile()) {
            title.setText(map.fileName);
            close.setVisibility(View.VISIBLE);
        } else {
            title.setText(e.running ? "This run · live" : e.startMs > 0 ? "Last run" : "Map");
            close.setVisibility(View.GONE);
        }
        if (map.isFile()) {
            sub.setText("Saved log · tap ✕ to go back to live");
        } else if (e.loc == null) {
            sub.setText("Waiting for a GPS position");
        } else {
            for (int i = 0; i < e.dotN; i++) counts[e.dotType[i]]++;
            sub.setText(Fmt.num(e.dotN) + " finds · " + Fmt.dist(e.distanceM) + " driven");
        }
        for (int i = 0; i < 4; i++) {
            legendText[i].setTextColor(map.showType[i] ? Theme.TEXT : Theme.DIM);
            ((LinearLayout) legendText[i].getParent()).getChildAt(0).setAlpha(map.showType[i] ? 1f : 0.3f);
        }
        boolean showBt = Prefs.b(Prefs.BT) || counts[2] > 0 || map.isFile();
        boolean showCell = Prefs.b(Prefs.CELL) || counts[3] > 0 || map.isFile();
        ((View) legendText[2].getParent()).setVisibility(showBt ? View.VISIBLE : View.GONE);
        ((View) legendText[3].getParent()).setVisibility(showCell ? View.VISIBLE : View.GONE);
        ((Icon) ((android.widget.ImageView) followBtn.getChildAt(0)).getDrawable()).setColor(map.follow && !map.isFile() ? Theme.ACC : Theme.TEXT);
        ((Icon) ((android.widget.ImageView) layersBtn.getChildAt(0)).getDrawable()).setColor(map.tiles ? Theme.ACC : Theme.TEXT);
        String te = map.tiles ? MapView.tileError : "";
        tileNote.setText(te);
        tileNote.setVisibility(te.length() > 0 ? View.VISIBLE : View.GONE);
        if (!map.isFile()) map.invalidate();
    }
}
