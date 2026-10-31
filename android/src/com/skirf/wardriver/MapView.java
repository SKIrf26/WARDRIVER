package com.skirf.wardriver;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.location.Location;
import android.util.LruCache;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashSet;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import static com.skirf.wardriver.Ui.dp;

/**
 * Slippy map: dark OpenStreetMap tiles (optional), the route driven and a dot for every find.
 * Shows the live run, or a saved log when one is loaded.
 */
public final class MapView extends View {
    // view state in Web-Mercator units (0..1 across the world)
    private double cx = 0.5, cy = 0.5;
    private double zoom = 3;
    public boolean follow = true;
    public boolean tiles = true;
    public boolean[] showType = {true, true, true, true};
    private boolean placed;

    // a saved log to show instead of the live run
    private double[] filePts;
    public String fileName;

    private final Paint tileP = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private final float tileScale;
    private float[] ptBuf = new float[4096];

    private static final LruCache<String, Bitmap> cache = new LruCache<String, Bitmap>(24 * 1024 * 1024) {
        @Override protected int sizeOf(String k, Bitmap b) { return b.getByteCount(); }
    };
    private static final HashSet<String> loading = new HashSet<String>();
    private static final HashSet<String> failed = new HashSet<String>();
    private static final ExecutorService pool = Executors.newFixedThreadPool(3);
    public static String tileError = "";

    private final ScaleGestureDetector scaler;
    private final GestureDetector gest;

    public MapView(Context c) {
        super(c);
        tileScale = Math.max(1f, getResources().getDisplayMetrics().density / 1.5f);
        // Night style: invert, turn the hue back round, then darken and cool it down.
        ColorMatrix m = new ColorMatrix(new float[]{
                -1, 0, 0, 0, 255,
                0, -1, 0, 0, 255,
                0, 0, -1, 0, 255,
                0, 0, 0, 1, 0});
        ColorMatrix hue = new ColorMatrix(new float[]{
                -0.574f, 1.43f, 0.144f, 0, 0,
                0.426f, 0.43f, 0.144f, 0, 0,
                0.426f, 1.43f, -0.856f, 0, 0,
                0, 0, 0, 1, 0});
        m.postConcat(hue);
        ColorMatrix dim = new ColorMatrix(new float[]{
                0.62f, 0, 0, 0, 0,
                0, 0.66f, 0, 0, 2,
                0, 0, 0.78f, 0, 8,
                0, 0, 0, 1, 0});
        m.postConcat(dim);
        ColorMatrix sat = new ColorMatrix();
        sat.setSaturation(0.35f);
        m.postConcat(sat);
        tileP.setColorFilter(new ColorMatrixColorFilter(m));

        scaler = new ScaleGestureDetector(c, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override public boolean onScale(ScaleGestureDetector d) {
                zoomAround(Math.log(d.getScaleFactor()) / Math.log(2), d.getFocusX(), d.getFocusY());
                return true;
            }
        });
        gest = new GestureDetector(c, new GestureDetector.SimpleOnGestureListener() {
            @Override public boolean onScroll(MotionEvent a, MotionEvent b, float dx, float dy) {
                double ws = worldPx();
                cx = clamp01(cx + dx / ws);
                cy = clamp01(cy + dy / ws);
                follow = false;
                invalidate();
                return true;
            }
            @Override public boolean onDoubleTap(MotionEvent e) {
                zoomAround(1, e.getX(), e.getY());
                return true;
            }
            @Override public boolean onDown(MotionEvent e) { return true; }
        });
    }

    private static double clamp01(double v) { return Math.max(0, Math.min(1, v)); }

    private double worldPx() { return 256.0 * tileScale * Math.pow(2, zoom); }

    public static double mx(double lon) { return (lon + 180.0) / 360.0; }

    public static double my(double lat) {
        double s = Math.sin(Math.toRadians(Math.max(-85.05, Math.min(85.05, lat))));
        return 0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI);
    }

    private void zoomAround(double dz, float fx, float fy) {
        double nz = Math.max(2, Math.min(19.5, zoom + dz));
        double ws = worldPx();
        double wx = cx + (fx - getWidth() / 2.0) / ws;
        double wy = cy + (fy - getHeight() / 2.0) / ws;
        zoom = nz;
        double ws2 = worldPx();
        cx = clamp01(wx - (fx - getWidth() / 2.0) / ws2);
        cy = clamp01(wy - (fy - getHeight() / 2.0) / ws2);
        invalidate();
    }

    public void zoomBy(double dz) { zoomAround(dz, getWidth() / 2f, getHeight() / 2f); }

    @Override public boolean onTouchEvent(MotionEvent e) {
        scaler.onTouchEvent(e);
        if (!scaler.isInProgress()) gest.onTouchEvent(e);
        if (e.getPointerCount() > 1) follow = false;
        getParent().requestDisallowInterceptTouchEvent(true);
        return true;
    }

    // ------------------------------------------------------------------
    public void showFile(String name, double[] pts) {
        fileName = name;
        filePts = pts;
        follow = false;
        fit();
        // the view may not be laid out yet: fit again once it is
        post(new Runnable() { @Override public void run() { fit(); } });
    }

    public void showLive() {
        fileName = null;
        filePts = null;
        follow = true;
        placed = false;
        invalidate();
    }

    public boolean isFile() { return filePts != null; }

    /** Zoom to show everything (the route and finds, or the loaded log). */
    public void fit() {
        double minX = 2, minY = 2, maxX = -1, maxY = -1;
        ScanEngine e = ScanEngine.get();
        if (filePts != null) {
            for (int i = 0; i + 2 < filePts.length; i += 3) {
                double x = mx(filePts[i + 1]), y = my(filePts[i]);
                minX = Math.min(minX, x); maxX = Math.max(maxX, x);
                minY = Math.min(minY, y); maxY = Math.max(maxY, y);
            }
        } else {
            for (int i = 0; i < e.trackN; i++) {
                double x = mx(e.trackLon[i]), y = my(e.trackLat[i]);
                minX = Math.min(minX, x); maxX = Math.max(maxX, x);
                minY = Math.min(minY, y); maxY = Math.max(maxY, y);
            }
            for (int i = 0; i < e.dotN; i++) {
                double x = mx(e.dotLon[i]), y = my(e.dotLat[i]);
                minX = Math.min(minX, x); maxX = Math.max(maxX, x);
                minY = Math.min(minY, y); maxY = Math.max(maxY, y);
            }
            if (e.loc != null) {
                double x = mx(e.loc.getLongitude()), y = my(e.loc.getLatitude());
                minX = Math.min(minX, x); maxX = Math.max(maxX, x);
                minY = Math.min(minY, y); maxY = Math.max(maxY, y);
            }
        }
        if (maxX < 0) { invalidate(); return; }
        cx = (minX + maxX) / 2;
        cy = (minY + maxY) / 2;
        double span = Math.max(maxX - minX, maxY - minY);
        int w = Math.max(1, getWidth()), h = Math.max(1, getHeight());
        double px = Math.min(w, h) * 0.8;
        if (span <= 0) zoom = 16;
        else zoom = Math.max(2, Math.min(18, Math.log(px / (span * 256.0 * tileScale)) / Math.log(2)));
        placed = true;
        invalidate();
    }

    // ------------------------------------------------------------------
    @Override protected void onDraw(Canvas c) {
        ScanEngine e = ScanEngine.get();
        Location me = e.loc;
        if (filePts == null && me != null && (follow || !placed)) {
            cx = mx(me.getLongitude());
            cy = my(me.getLatitude());
            if (!placed) zoom = 16;
            placed = true;
        }
        int w = getWidth(), h = getHeight();
        c.drawColor(Theme.BG);
        drawGrid(c, w, h);
        if (tiles && placed) drawTiles(c, w, h);

        double ws = worldPx();
        double ox = cx * ws - w / 2.0, oy = cy * ws - h / 2.0;

        if (filePts != null) {
            drawPoints(c, filePts, filePts.length / 3, ws, ox, oy, true, null, null, null);
        } else {
            // route
            if (e.trackN > 1) {
                path.reset();
                float lx = -1e9f, ly = -1e9f;
                boolean started = false;
                for (int i = 0; i < e.trackN; i++) {
                    float x = (float) (mx(e.trackLon[i]) * ws - ox), y = (float) (my(e.trackLat[i]) * ws - oy);
                    if (!started) { path.moveTo(x, y); started = true; lx = x; ly = y; continue; }
                    if (Math.abs(x - lx) + Math.abs(y - ly) < dp(2) && i < e.trackN - 1) continue;
                    path.lineTo(x, y);
                    lx = x; ly = y;
                }
                p.setStyle(Paint.Style.STROKE);
                p.setStrokeCap(Paint.Cap.ROUND);
                p.setStrokeJoin(Paint.Join.ROUND);
                p.setStrokeWidth(dp(7));
                p.setColor(Theme.a(Theme.ACC, 50));
                c.drawPath(path, p);
                p.setStrokeWidth(dp(3));
                p.setColor(Theme.ACC);
                c.drawPath(path, p);
            }
            drawPoints(c, null, e.dotN, ws, ox, oy, false, e.dotLat, e.dotLon, e.dotType);
            // me
            if (me != null) {
                float x = (float) (mx(me.getLongitude()) * ws - ox), y = (float) (my(me.getLatitude()) * ws - oy);
                if (me.hasAccuracy()) {
                    double mPerPx = 156543.03392 * Math.cos(Math.toRadians(me.getLatitude())) / (ws / 256.0);
                    float ar = (float) (me.getAccuracy() / mPerPx);
                    if (ar > dp(10)) {
                        p.setStyle(Paint.Style.FILL);
                        p.setColor(Theme.a(Theme.ACC, 30));
                        c.drawCircle(x, y, ar, p);
                    }
                }
                if (me.hasBearing() && me.hasSpeed() && me.getSpeed() > 1) {
                    c.save();
                    c.rotate(me.getBearing(), x, y);
                    path.reset();
                    path.moveTo(x, y - dp(15));
                    path.lineTo(x + dp(9), y + dp(10));
                    path.lineTo(x, y + dp(5));
                    path.lineTo(x - dp(9), y + dp(10));
                    path.close();
                    p.setStyle(Paint.Style.FILL);
                    p.setColor(Theme.ACC);
                    c.drawPath(path, p);
                    p.setStyle(Paint.Style.STROKE);
                    p.setStrokeWidth(dp(2));
                    p.setColor(0xFF000000);
                    c.drawPath(path, p);
                    c.restore();
                } else {
                    p.setStyle(Paint.Style.FILL);
                    p.setColor(0xFF000000);
                    c.drawCircle(x, y, dp(10), p);
                    p.setColor(Theme.ACC);
                    c.drawCircle(x, y, dp(7), p);
                }
            }
        }
        drawScale(c, w, h, ws);
        if (follow && filePts == null) postInvalidateDelayed(1000);
    }

    private void drawGrid(Canvas c, int w, int h) {
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(Math.max(1, dp(1)));
        p.setColor(Theme.a(Theme.LINE, 140));
        float step = dp(48);
        for (float x = (float) (-(cx * worldPx()) % step); x < w; x += step) c.drawLine(x, 0, x, h, p);
        for (float y = (float) (-(cy * worldPx()) % step); y < h; y += step) c.drawLine(0, y, w, y, p);
    }

    private void drawPoints(Canvas c, double[] triples, int n, double ws, double ox, double oy, boolean file,
                            double[] lat, double[] lon, byte[] type) {
        if (n == 0) return;
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeCap(Paint.Cap.ROUND);
        int w = getWidth(), h = getHeight();
        float size = (float) Math.max(dp(3), Math.min(dp(8), dp(2) * (zoom - 11)));
        // draw rarer types last so they stay visible on top of Wi-Fi
        int[] order = {0, 3, 2, 1};
        for (int t : order) {
            if (!showType[t]) continue;
            int k = 0;
            for (int i = 0; i < n; i++) {
                double la, lo;
                int ti;
                if (file) { la = triples[i * 3]; lo = triples[i * 3 + 1]; ti = (int) triples[i * 3 + 2]; }
                else { la = lat[i]; lo = lon[i]; ti = type[i]; }
                if (ti != t) continue;
                float x = (float) (mx(lo) * ws - ox), y = (float) (my(la) * ws - oy);
                if (x < -20 || y < -20 || x > w + 20 || y > h + 20) continue;
                if (k + 2 > ptBuf.length) ptBuf = java.util.Arrays.copyOf(ptBuf, ptBuf.length * 2);
                ptBuf[k++] = x;
                ptBuf[k++] = y;
            }
            if (k == 0) continue;
            int col = Charts.Spark.colorFor(t);
            p.setStrokeWidth(size + dp(3));
            p.setColor(Theme.a(0xFF000000, 150));
            c.drawPoints(ptBuf, 0, k, p);
            p.setStrokeWidth(size);
            p.setColor(col);
            c.drawPoints(ptBuf, 0, k, p);
        }
    }

    private void drawScale(Canvas c, int w, int h, double ws) {
        double lat = 0;
        double yy = cy;
        lat = Math.toDegrees(Math.atan(Math.sinh(Math.PI * (1 - 2 * yy))));
        double mPerPx = 156543.03392 * Math.cos(Math.toRadians(lat)) / (ws / 256.0);
        boolean imp = Prefs.imperial();
        double target = mPerPx * dp(90);
        double unit = imp ? 0.3048 : 1;
        double[] steps = imp ? new double[]{50, 100, 200, 500, 1000, 2640, 5280, 10560, 26400, 52800, 105600, 264000, 528000}
                : new double[]{20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000};
        double best = steps[0];
        for (double s : steps) if (s * unit <= target) best = s;
        float len = (float) (best * unit / mPerPx);
        String lab;
        if (imp) lab = best >= 5280 ? (best / 5280 == Math.floor(best / 5280) ? String.valueOf((int) (best / 5280)) : String.valueOf(best / 5280)) + " mi" : (int) best + " ft";
        else lab = best >= 1000 ? (int) (best / 1000) + " km" : (int) best + " m";
        float x0 = dp(14), y0 = h - dp(14);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeWidth(dp(4));
        p.setColor(0xAA000000);
        c.drawLine(x0, y0, x0 + len, y0, p);
        p.setStrokeWidth(dp(2));
        p.setColor(Theme.TEXT);
        c.drawLine(x0, y0, x0 + len, y0, p);
        p.setStyle(Paint.Style.FILL);
        p.setTextSize(dp(11));
        p.setTypeface(Theme.SEMI);
        p.setShadowLayer(dp(3), 0, 0, 0xFF000000);
        c.drawText(lab, x0, y0 - dp(7), p);
        if (tiles) {
            p.setTextAlign(Paint.Align.RIGHT);
            p.setTextSize(dp(10));
            p.setColor(Theme.MUTE);
            c.drawText("© OpenStreetMap contributors", w - dp(10), h - dp(10), p);
            p.setTextAlign(Paint.Align.LEFT);
        }
        p.clearShadowLayer();
    }

    // ------------------------------------------------------------------
    private void drawTiles(Canvas c, int w, int h) {
        int tz = (int) Math.max(2, Math.min(19, Math.round(zoom)));
        double ws = worldPx();
        double tilePx = ws / Math.pow(2, tz);
        double ox = cx * ws - w / 2.0, oy = cy * ws - h / 2.0;
        int n = 1 << tz;
        int x0 = (int) Math.floor(ox / tilePx), y0 = (int) Math.floor(oy / tilePx);
        int x1 = (int) Math.floor((ox + w) / tilePx), y1 = (int) Math.floor((oy + h) / tilePx);
        RectF dst = new RectF();
        for (int ty = y0; ty <= y1; ty++) {
            if (ty < 0 || ty >= n) continue;
            for (int tx = x0; tx <= x1; tx++) {
                int wx = ((tx % n) + n) % n;
                float left = (float) (tx * tilePx - ox), top = (float) (ty * tilePx - oy);
                dst.set(left, top, (float) (left + tilePx) + 0.5f, (float) (top + tilePx) + 0.5f);
                Bitmap b = tile(tz, wx, ty);
                if (b != null) { c.drawBitmap(b, null, dst, tileP); continue; }
                // stand-in from the zoom level above while this one loads
                if (tz > 2) {
                    Bitmap up = cache.get((tz - 1) + "/" + (wx / 2) + "/" + (ty / 2));
                    if (up != null) {
                        int sx = (wx % 2) * 128, sy = (ty % 2) * 128;
                        android.graphics.Rect src = new android.graphics.Rect(sx, sy, sx + 128, sy + 128);
                        c.drawBitmap(up, src, dst, tileP);
                    }
                }
            }
        }
    }

    private Bitmap tile(final int z, final int x, final int y) {
        final String k = z + "/" + x + "/" + y;
        Bitmap b = cache.get(k);
        if (b != null) return b;
        synchronized (loading) {
            if (loading.contains(k) || failed.contains(k) || loading.size() > 24) return null;
            loading.add(k);
        }
        final File disk = new File(getContext().getCacheDir(), "tiles/" + k + ".png");
        pool.execute(new Runnable() {
            @Override public void run() {
                Bitmap got = null;
                try {
                    if (disk.exists() && System.currentTimeMillis() - disk.lastModified() < 30L * 86400000L) {
                        got = BitmapFactory.decodeFile(disk.getPath());
                    }
                    if (got == null) {
                        HttpURLConnection con = (HttpURLConnection) new URL("https://tile.openstreetmap.org/" + k + ".png").openConnection();
                        con.setConnectTimeout(10000);
                        con.setReadTimeout(15000);
                        con.setRequestProperty("User-Agent", "SKIrf-Wardriver-Android/" + BuildInfo.APP_RELEASE + " (personal wardriving map)");
                        int code = con.getResponseCode();
                        if (code == 200) {
                            InputStream in = con.getInputStream();
                            ByteArrayOutputStream bo = new ByteArrayOutputStream();
                            byte[] buf = new byte[16384];
                            int n;
                            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
                            in.close();
                            byte[] bytes = bo.toByteArray();
                            got = BitmapFactory.decodeByteArray(bytes, 0, bytes.length);
                            if (got != null) {
                                disk.getParentFile().mkdirs();
                                FileOutputStream fo = new FileOutputStream(disk);
                                fo.write(bytes);
                                fo.close();
                            }
                            tileError = "";
                        } else {
                            tileError = "Map tiles: server answered " + code;
                        }
                        con.disconnect();
                    }
                } catch (Throwable t) {
                    tileError = "Map tiles need internet";
                }
                synchronized (loading) {
                    loading.remove(k);
                    if (got == null) failed.add(k);
                }
                if (got != null) {
                    cache.put(k, got);
                    postInvalidate();
                } else {
                    // try again later
                    postDelayed(new Runnable() {
                        @Override public void run() { synchronized (loading) { failed.remove(k); } }
                    }, 20000);
                }
            }
        });
        return null;
    }
}
