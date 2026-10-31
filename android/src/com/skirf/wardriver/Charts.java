package com.skirf.wardriver;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.location.GnssStatus;
import android.view.View;

import static com.skirf.wardriver.Ui.dp;

/** Drawn views: the start button, the activity chart, split bars, the sky plot and the signal bars. */
public final class Charts {

    // ======================================================================
    /** Big round START / STOP button with a sweeping ring while running. */
    public static final class DriveButton extends View {
        private boolean running;
        private boolean ready = true;
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();
        private final Icon play, stop;
        private long t0 = System.currentTimeMillis();

        public DriveButton(Context c) {
            super(c);
            play = new Icon(Icon.PLAY, Theme.ON_ACC);
            stop = new Icon(Icon.STOP, Theme.BAD);
            setClickable(true);
        }

        public void set(boolean running, boolean ready) {
            if (this.running != running) t0 = System.currentTimeMillis();
            this.running = running;
            this.ready = ready;
            invalidate();
        }

        @Override protected void onDraw(Canvas c) {
            float w = getWidth(), h = getHeight();
            float cx = w / 2, cy = h / 2, R = Math.min(w, h) / 2 - dp(4);
            long now = System.currentTimeMillis();
            p.setStyle(Paint.Style.FILL);
            p.setShader(null);
            if (!running) {
                int acc = ready ? Theme.ACC : Theme.DIM;
                // soft glow
                float pulse = (float) (0.5 + 0.5 * Math.sin((now - t0) / 600.0));
                p.setColor(Theme.a(acc, (int) (26 + 26 * pulse)));
                c.drawCircle(cx, cy, R, p);
                p.setColor(acc);
                c.drawCircle(cx, cy, R * 0.80f, p);
                play.setColor(Theme.ON_ACC);
                int s = (int) (R * 0.62f);
                play.setBounds((int) (cx - s / 2f + R * 0.05f), (int) (cy - s / 2f), (int) (cx + s / 2f + R * 0.05f), (int) (cy + s / 2f));
                play.draw(c);
                postInvalidateDelayed(40);
            } else {
                p.setColor(Theme.a(Theme.BAD, 30));
                c.drawCircle(cx, cy, R * 0.80f, p);
                // track ring
                p.setStyle(Paint.Style.STROKE);
                p.setStrokeWidth(dp(5));
                p.setColor(Theme.a(Theme.BAD, 50));
                c.drawCircle(cx, cy, R - dp(3), p);
                // comet sweeping once every 2 s
                float ang = ((now - t0) % 2000) / 2000f * 360f;
                r.set(cx - R + dp(3), cy - R + dp(3), cx + R - dp(3), cy + R - dp(3));
                SweepGradient g = new SweepGradient(cx, cy, new int[]{Theme.a(Theme.BAD, 0), Theme.BAD}, new float[]{0.55f, 1f});
                android.graphics.Matrix m = new android.graphics.Matrix();
                m.setRotate(ang - 360f, cx, cy);
                g.setLocalMatrix(m);
                p.setShader(g);
                p.setStrokeCap(Paint.Cap.ROUND);
                c.drawArc(r, ang - 200, 200, false, p);
                p.setShader(null);
                p.setStyle(Paint.Style.FILL);
                stop.setColor(Theme.BAD);
                int s = (int) (R * 0.62f);
                stop.setBounds((int) (cx - s / 2f), (int) (cy - s / 2f), (int) (cx + s / 2f), (int) (cy + s / 2f));
                stop.draw(c);
                postInvalidateDelayed(33);
            }
        }
    }

    // ======================================================================
    /** Finds per minute, stacked by type, with the newest minute on the right. */
    public static final class Spark extends View {
        private int[][] data = new int[30][4];
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();
        private static final int[] ORDER = {0, 1, 2, 3};

        public Spark(Context c) { super(c); }

        public void set(int[][] d) { data = d; invalidate(); }

        @Override protected void onDraw(Canvas c) {
            float w = getWidth(), h = getHeight();
            int n = data.length;
            int max = 1;
            for (int[] m : data) max = Math.max(max, m[0] + m[1] + m[2] + m[3]);
            // round the scale up to a friendly number
            int scale = max <= 5 ? 5 : max <= 10 ? 10 : max <= 25 ? 25 : max <= 50 ? 50 : max <= 100 ? 100 : ((max + 99) / 100) * 100;
            float gap = dp(3);
            float bw = (w - gap * (n - 1)) / n;
            float top = dp(16), bottom = h - dp(2);
            float avail = bottom - top;
            // guide lines
            p.setColor(Theme.LINE);
            p.setStrokeWidth(Math.max(1, dp(1)));
            c.drawLine(0, top, w, top, p);
            c.drawLine(0, top + avail / 2, w, top + avail / 2, p);
            p.setTextSize(dp(10));
            p.setTypeface(Theme.SEMI);
            p.setColor(Theme.MUTE);
            c.drawText(String.valueOf(scale), 0, top - dp(4), p);
            for (int i = 0; i < n; i++) {
                float x = i * (bw + gap);
                float y = bottom;
                int total = data[i][0] + data[i][1] + data[i][2] + data[i][3];
                if (total == 0) {
                    p.setColor(Theme.CARD2);
                    r.set(x, bottom - dp(3), x + bw, bottom);
                    c.drawRoundRect(r, dp(1.5f), dp(1.5f), p);
                    continue;
                }
                for (int t : ORDER) {
                    if (data[i][t] == 0) continue;
                    float hh = Math.max(dp(2), avail * data[i][t] / (float) scale);
                    p.setColor(colorFor(t));
                    r.set(x, Math.max(top, y - hh), x + bw, y);
                    c.drawRoundRect(r, dp(2), dp(2), p);
                    y -= hh + dp(1);
                }
            }
        }

        static int colorFor(int t) {
            switch (t) {
                case 1: return Theme.BLE;
                case 2: return Theme.BT;
                case 3: return Theme.CELL;
                default: return Theme.W24;
            }
        }
    }

    // ======================================================================
    /** One rounded bar split into coloured parts. */
    public static final class SplitBar extends View {
        private int[] values = new int[0];
        private int[] colors = new int[0];
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();
        private final Path clip = new Path();

        public SplitBar(Context c) { super(c); }

        public void set(int[] values, int[] colors) {
            this.values = values;
            this.colors = colors;
            invalidate();
        }

        @Override protected void onDraw(Canvas c) {
            float w = getWidth(), h = getHeight();
            r.set(0, 0, w, h);
            p.setColor(Theme.CARD2);
            c.drawRoundRect(r, h / 2, h / 2, p);
            int total = 0;
            for (int v : values) total += v;
            if (total == 0) return;
            c.save();
            clip.reset();
            clip.addRoundRect(r, h / 2, h / 2, Path.Direction.CW);
            c.clipPath(clip);
            float x = 0, gap = dp(2);
            for (int i = 0; i < values.length; i++) {
                if (values[i] == 0) continue;
                float ww = w * values[i] / (float) total;
                p.setColor(colors[i]);
                c.drawRect(x, 0, x + ww - gap, h, p);
                x += ww;
            }
            c.restore();
        }
    }

    // ======================================================================
    public static int consColor(int cons) {
        switch (cons) {
            case GnssStatus.CONSTELLATION_GPS: return 0xFF50AAFF;
            case GnssStatus.CONSTELLATION_GLONASS: return 0xFFFF7AC8;
            case GnssStatus.CONSTELLATION_GALILEO: return 0xFF40F096;
            case GnssStatus.CONSTELLATION_BEIDOU: return 0xFFFFB547;
            case GnssStatus.CONSTELLATION_QZSS: return 0xFFBA80FF;
            case GnssStatus.CONSTELLATION_IRNSS: return 0xFF00DCC8;
            case GnssStatus.CONSTELLATION_SBAS: return 0xFFB0B8C4;
            default: return 0xFF808998;
        }
    }

    public static String consName(int cons) {
        switch (cons) {
            case GnssStatus.CONSTELLATION_GPS: return "GPS";
            case GnssStatus.CONSTELLATION_GLONASS: return "GLONASS";
            case GnssStatus.CONSTELLATION_GALILEO: return "Galileo";
            case GnssStatus.CONSTELLATION_BEIDOU: return "BeiDou";
            case GnssStatus.CONSTELLATION_QZSS: return "QZSS";
            case GnssStatus.CONSTELLATION_IRNSS: return "NavIC";
            case GnssStatus.CONSTELLATION_SBAS: return "SBAS";
            default: return "Other";
        }
    }

    /** Where the satellites are in the sky: centre is straight up, the edge is the horizon. */
    public static final class SkyView extends View {
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private float heading = Float.NaN;

        public SkyView(Context c) { super(c); }

        public void heading(float deg) { heading = deg; }

        @Override protected void onMeasure(int ws, int hs) {
            int w = MeasureSpec.getSize(ws);
            setMeasuredDimension(w, Math.min(w, dp(340)));
        }

        @Override protected void onDraw(Canvas c) {
            ScanEngine e = ScanEngine.get();
            float w = getWidth(), h = getHeight();
            float cx = w / 2, cy = h / 2, R = Math.min(w, h) / 2 - dp(16);
            p.setStyle(Paint.Style.FILL);
            p.setShader(new android.graphics.RadialGradient(cx, cy, R, Theme.a(Theme.ACC, 22), Theme.a(Theme.ACC, 4), Shader.TileMode.CLAMP));
            c.drawCircle(cx, cy, R, p);
            p.setShader(null);
            p.setStyle(Paint.Style.STROKE);
            p.setStrokeWidth(Math.max(1, dp(1)));
            p.setColor(Theme.LINE);
            c.drawCircle(cx, cy, R, p);
            c.drawCircle(cx, cy, R * 2 / 3f, p);
            c.drawCircle(cx, cy, R / 3f, p);
            c.drawLine(cx - R, cy, cx + R, cy, p);
            c.drawLine(cx, cy - R, cx, cy + R, p);
            p.setStyle(Paint.Style.FILL);
            p.setTextSize(dp(12));
            p.setTypeface(Theme.BOLD);
            p.setTextAlign(Paint.Align.CENTER);
            String[] lab = {"N", "E", "S", "W"};
            for (int i = 0; i < 4; i++) {
                double a = Math.toRadians(i * 90 - 90);
                float x = (float) (cx + Math.cos(a) * (R + dp(9)));
                float y = (float) (cy + Math.sin(a) * (R + dp(9))) + dp(4);
                p.setColor(i == 0 ? Theme.ACC : Theme.MUTE);
                c.drawText(lab[i], x, y, p);
            }
            // heading line
            if (!Float.isNaN(heading)) {
                double a = Math.toRadians(heading - 90);
                p.setStrokeWidth(dp(2));
                p.setColor(Theme.a(Theme.ACC, 120));
                c.drawLine(cx, cy, (float) (cx + Math.cos(a) * R), (float) (cy + Math.sin(a) * R), p);
            }
            p.setTextSize(dp(9));
            for (int i = 0; i < e.satN; i++) {
                if (e.satElev[i] <= 0 && e.satAz[i] == 0) continue;
                float rr = R * (90 - Math.max(0, e.satElev[i])) / 90f;
                double a = Math.toRadians(e.satAz[i] - 90);
                float x = (float) (cx + Math.cos(a) * rr), y = (float) (cy + Math.sin(a) * rr);
                int col = consColor(e.satCons[i]);
                float size = dp(5) + Math.min(dp(5), e.satCn0[i] / 50f * dp(5));
                if (e.satUsed[i]) {
                    p.setStyle(Paint.Style.FILL);
                    p.setColor(Theme.a(col, 60));
                    c.drawCircle(x, y, size + dp(4), p);
                    p.setColor(col);
                    c.drawCircle(x, y, size, p);
                } else {
                    p.setStyle(Paint.Style.STROKE);
                    p.setStrokeWidth(dp(1.5f));
                    p.setColor(Theme.a(col, 170));
                    c.drawCircle(x, y, size, p);
                }
                p.setStyle(Paint.Style.FILL);
                p.setColor(Theme.MUTE);
                c.drawText(String.valueOf(e.satSvid[i]), x, y + size + dp(10), p);
            }
            p.setTextAlign(Paint.Align.LEFT);
            if (e.satN == 0) {
                p.setTextAlign(Paint.Align.CENTER);
                p.setTextSize(dp(14));
                p.setColor(Theme.MUTE);
                c.drawText("Looking for satellites...", cx, cy - dp(8), p);
                p.setTextAlign(Paint.Align.LEFT);
            }
        }
    }

    /** Signal strength of each satellite, strongest first. */
    public static final class SnrView extends View {
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();

        public SnrView(Context c) { super(c); }

        @Override protected void onDraw(Canvas c) {
            ScanEngine e = ScanEngine.get();
            float w = getWidth(), h = getHeight();
            int n = e.satN;
            Integer[] idx = new Integer[n];
            for (int i = 0; i < n; i++) idx[i] = i;
            final float[] cn = e.satCn0;
            java.util.Arrays.sort(idx, new java.util.Comparator<Integer>() {
                @Override public int compare(Integer a, Integer b) { return Float.compare(cn[b], cn[a]); }
            });
            int show = Math.min(n, 32);
            float labelH = dp(14);
            float top = dp(6), bottom = h - labelH;
            p.setColor(Theme.LINE);
            p.setStrokeWidth(Math.max(1, dp(1)));
            for (int g = 0; g <= 2; g++) {
                float y = bottom - (bottom - top) * g / 2f;
                c.drawLine(0, y, w, y, p);
            }
            if (show == 0) return;
            float gap = dp(3);
            float bw = Math.min(dp(18), (w - gap * (show - 1)) / show);
            p.setTextSize(dp(9));
            p.setTypeface(Theme.SEMI);
            p.setTextAlign(Paint.Align.CENTER);
            for (int k = 0; k < show; k++) {
                int i = idx[k];
                float x = k * (bw + gap);
                float frac = Math.max(0.03f, Math.min(1f, e.satCn0[i] / 50f));
                float y = bottom - (bottom - top) * frac;
                int col = consColor(e.satCons[i]);
                r.set(x, y, x + bw, bottom);
                if (e.satUsed[i]) {
                    p.setStyle(Paint.Style.FILL);
                    p.setShader(new LinearGradient(0, y, 0, bottom, col, Theme.a(col, 90), Shader.TileMode.CLAMP));
                    c.drawRoundRect(r, dp(3), dp(3), p);
                    p.setShader(null);
                } else {
                    p.setStyle(Paint.Style.STROKE);
                    p.setStrokeWidth(dp(1.2f));
                    p.setColor(Theme.a(col, 160));
                    r.inset(dp(0.6f), dp(0.6f));
                    c.drawRoundRect(r, dp(3), dp(3), p);
                }
                p.setStyle(Paint.Style.FILL);
                p.setColor(Theme.MUTE);
                c.drawText(String.valueOf(e.satSvid[i]), x + bw / 2, h - dp(2), p);
            }
            p.setTextAlign(Paint.Align.LEFT);
        }
    }

    // ======================================================================
    /** Four little signal bars for a list row. */
    public static final class Bars extends View {
        private int level;   // 0..4
        private int color = Theme.ACC;
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();

        public Bars(Context c) { super(c); }

        public void set(int rssi, int color) {
            level = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : rssi >= -88 ? 1 : 0;
            this.color = color;
            invalidate();
        }

        @Override protected void onDraw(Canvas c) {
            float w = getWidth(), h = getHeight();
            float bw = w / 4f - dp(1.5f);
            for (int i = 0; i < 4; i++) {
                float bh = h * (i + 1) / 4f;
                float x = i * (bw + dp(2));
                r.set(x, h - bh, x + bw, h);
                p.setColor(i < level || (i == 0 && level == 0) ? color : Theme.LINE);
                if (level == 0 && i == 0) p.setColor(Theme.a(color, 120));
                c.drawRoundRect(r, dp(1), dp(1), p);
            }
        }
    }

    private Charts() {}
}
