package com.skirf.wardriver;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

/** Line icons drawn in code on a 24x24 grid, so they stay sharp at any size and take any colour. */
public final class Icon extends Drawable {
    public static final int WIFI = 1, BLE = 2, BT = 3, CELL = 4, TARGET = 5, MAP = 6, LOGS = 7, COG = 8, PLAY = 9,
            STOP = 10, UPLOAD = 11, DOWNLOAD = 12, SHARE = 13, TRASH = 14, CHECK = 15, CHEVRON = 16, ALERT = 17,
            LOCK = 18, UNLOCK = 19, REFRESH = 20, LOCATE = 21, FIT = 22, CLOCK = 23, ROUTE = 24, BOLT = 25,
            CLOSE = 26, KEY = 27, EYE = 28, INFO = 29, PHONE = 30, LAYERS = 31, TROPHY = 32, COPY = 33, SPARK = 34,
            DRIVE = 35, PIN = 36, PLUS = 37, MINUS = 38, BATTERY = 39, SAT = 40;

    private final int kind;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private final RectF r = new RectF();
    private int color;
    private float stroke = 2f;

    public Icon(int kind, int color) {
        this.kind = kind;
        this.color = color;
        p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeJoin(Paint.Join.ROUND);
    }

    public Icon stroke(float s) { stroke = s; return this; }
    public void setColor(int c) { color = c; invalidateSelf(); }

    @Override
    public void draw(Canvas c) {
        Rect b = getBounds();
        float s = Math.min(b.width(), b.height()) / 24f;
        c.save();
        c.translate(b.left + (b.width() - 24 * s) / 2f, b.top + (b.height() - 24 * s) / 2f);
        c.scale(s, s);
        p.setColor(color);
        p.setStrokeWidth(stroke);
        p.setStyle(Paint.Style.STROKE);
        drawKind(c);
        c.restore();
    }

    private void line(Canvas c, float... xy) {
        path.reset();
        path.moveTo(xy[0], xy[1]);
        for (int i = 2; i + 1 < xy.length; i += 2) path.lineTo(xy[i], xy[i + 1]);
        c.drawPath(path, p);
    }

    private void closed(Canvas c, boolean fill, float... xy) {
        path.reset();
        path.moveTo(xy[0], xy[1]);
        for (int i = 2; i + 1 < xy.length; i += 2) path.lineTo(xy[i], xy[i + 1]);
        path.close();
        if (fill) { p.setStyle(Paint.Style.FILL_AND_STROKE); c.drawPath(path, p); p.setStyle(Paint.Style.STROKE); }
        else c.drawPath(path, p);
    }

    private void dot(Canvas c, float x, float y, float rad) {
        p.setStyle(Paint.Style.FILL);
        c.drawCircle(x, y, rad, p);
        p.setStyle(Paint.Style.STROKE);
    }

    private void arc(Canvas c, float cx, float cy, float rad, float start, float sweep) {
        r.set(cx - rad, cy - rad, cx + rad, cy + rad);
        c.drawArc(r, start, sweep, false, p);
    }

    private void rrect(Canvas c, float l, float t, float rt, float bt, float rad) {
        r.set(l, t, rt, bt);
        c.drawRoundRect(r, rad, rad, p);
    }

    private void drawKind(Canvas c) {
        switch (kind) {
            case WIFI:
            case DRIVE:
                arc(c, 12, 19, 4.5f, 225, 90);
                arc(c, 12, 19, 9f, 225, 90);
                arc(c, 12, 19, 13.5f, 228, 84);
                dot(c, 12, 19, 1.6f);
                break;
            case BLE:
                line(c, 7, 7, 17, 17, 12, 21.5f, 12, 2.5f, 17, 7, 7, 17);
                break;
            case BT:
                line(c, 8, 7.5f, 16, 15.5f, 12, 20, 12, 4, 16, 8.5f, 8, 16.5f);
                dot(c, 4, 12, 1.4f);
                dot(c, 20, 12, 1.4f);
                break;
            case CELL:
                line(c, 12, 9, 7.5f, 21.5f);
                line(c, 12, 9, 16.5f, 21.5f);
                line(c, 9, 17, 15, 17);
                dot(c, 12, 8, 1.6f);
                arc(c, 12, 8, 4.5f, 135, 90);
                arc(c, 12, 8, 4.5f, -45, 90);
                arc(c, 12, 8, 8.5f, 145, 70);
                arc(c, 12, 8, 8.5f, -35, 70);
                break;
            case TARGET:
                c.drawCircle(12, 12, 7, p);
                line(c, 12, 1.5f, 12, 5);
                line(c, 12, 19, 12, 22.5f);
                line(c, 1.5f, 12, 5, 12);
                line(c, 19, 12, 22.5f, 12);
                dot(c, 12, 12, 2.2f);
                break;
            case SAT:
                closed(c, false, 9, 9, 15, 15, 12.5f, 17.5f, 6.5f, 11.5f);
                line(c, 9, 9, 5.5f, 5.5f);
                closed(c, false, 5.5f, 5.5f, 2.5f, 8.5f, 4.5f, 10.5f, 7.5f, 7.5f);
                line(c, 15, 15, 18.5f, 18.5f);
                closed(c, false, 18.5f, 18.5f, 21.5f, 15.5f, 19.5f, 13.5f, 16.5f, 16.5f);
                arc(c, 14.5f, 9.5f, 3, -60, 120);
                break;
            case MAP:
                closed(c, false, 3, 6.5f, 9, 3.5f, 15, 6.5f, 21, 3.5f, 21, 17.5f, 15, 20.5f, 9, 17.5f, 3, 20.5f);
                line(c, 9, 3.5f, 9, 17.5f);
                line(c, 15, 6.5f, 15, 20.5f);
                break;
            case LOGS:
                closed(c, false, 6, 2.5f, 14.5f, 2.5f, 19, 7, 19, 21.5f, 6, 21.5f);
                line(c, 14.5f, 2.5f, 14.5f, 7, 19, 7);
                line(c, 9, 12, 16, 12);
                line(c, 9, 16, 16, 16);
                break;
            case COG: {
                c.drawCircle(12, 12, 3, p);
                c.drawCircle(12, 12, 7, p);
                for (int i = 0; i < 8; i++) {
                    double a = Math.PI / 4 * i;
                    float x1 = (float) (12 + Math.cos(a) * 7), y1 = (float) (12 + Math.sin(a) * 7);
                    float x2 = (float) (12 + Math.cos(a) * 10), y2 = (float) (12 + Math.sin(a) * 10);
                    p.setStrokeWidth(stroke * 1.6f);
                    c.drawLine(x1, y1, x2, y2, p);
                    p.setStrokeWidth(stroke);
                }
                break;
            }
            case PLAY:
                closed(c, true, 8, 5, 19, 12, 8, 19);
                break;
            case STOP:
                p.setStyle(Paint.Style.FILL_AND_STROKE);
                rrect(c, 6, 6, 18, 18, 2);
                p.setStyle(Paint.Style.STROKE);
                break;
            case UPLOAD:
                line(c, 12, 15, 12, 3.5f);
                line(c, 7, 8.5f, 12, 3.5f, 17, 8.5f);
                line(c, 4, 15, 4, 20, 20, 20, 20, 15);
                break;
            case DOWNLOAD:
                line(c, 12, 3.5f, 12, 15);
                line(c, 7, 10, 12, 15, 17, 10);
                line(c, 4, 15, 4, 20, 20, 20, 20, 15);
                break;
            case SHARE:
                c.drawCircle(18, 5.5f, 2.5f, p);
                c.drawCircle(6, 12, 2.5f, p);
                c.drawCircle(18, 18.5f, 2.5f, p);
                line(c, 8.2f, 10.8f, 15.8f, 6.7f);
                line(c, 8.2f, 13.2f, 15.8f, 17.3f);
                break;
            case TRASH:
                line(c, 4, 6.5f, 20, 6.5f);
                line(c, 9, 6.5f, 9, 3.5f, 15, 3.5f, 15, 6.5f);
                line(c, 6, 6.5f, 7, 21, 17, 21, 18, 6.5f);
                line(c, 10, 10.5f, 10, 17);
                line(c, 14, 10.5f, 14, 17);
                break;
            case CHECK:
                line(c, 5, 12.5f, 10, 17.5f, 19.5f, 7);
                break;
            case CHEVRON:
                line(c, 9, 5, 16, 12, 9, 19);
                break;
            case ALERT:
                closed(c, false, 12, 3, 22, 20.5f, 2, 20.5f);
                line(c, 12, 9.5f, 12, 14);
                dot(c, 12, 17.2f, 1.3f);
                break;
            case LOCK:
                rrect(c, 5, 10.5f, 19, 21, 2);
                arc(c, 12, 8, 4, 180, 180);
                line(c, 8, 8, 8, 10.5f);
                line(c, 16, 8, 16, 10.5f);
                break;
            case UNLOCK:
                rrect(c, 5, 10.5f, 19, 21, 2);
                arc(c, 12, 7, 4, 180, 160);
                line(c, 8, 7, 8, 10.5f);
                break;
            case REFRESH:
                arc(c, 12, 12, 8, -60, 300);
                line(c, 16, 2.5f, 16.2f, 5.6f, 13, 6.5f);
                break;
            case LOCATE:
                closed(c, false, 21, 3, 3, 10.5f, 11, 13, 13.5f, 21);
                break;
            case FIT:
                line(c, 3, 8, 3, 3, 8, 3);
                line(c, 16, 3, 21, 3, 21, 8);
                line(c, 21, 16, 21, 21, 16, 21);
                line(c, 8, 21, 3, 21, 3, 16);
                dot(c, 12, 12, 2.2f);
                break;
            case CLOCK:
                c.drawCircle(12, 12, 9, p);
                line(c, 12, 7, 12, 12, 15.5f, 14);
                break;
            case ROUTE:
                c.drawCircle(6, 18, 2.5f, p);
                c.drawCircle(18, 6, 2.5f, p);
                line(c, 8.5f, 18, 15, 18);
                path.reset();
                path.moveTo(15, 18);
                path.cubicTo(20, 18, 20, 12, 15, 12);
                path.lineTo(9, 12);
                path.cubicTo(4, 12, 4, 6, 9, 6);
                path.lineTo(15.5f, 6);
                c.drawPath(path, p);
                break;
            case BOLT:
                closed(c, false, 13.5f, 2.5f, 5, 13.5f, 11.5f, 13.5f, 10.5f, 21.5f, 19, 10.5f, 12.5f, 10.5f);
                break;
            case CLOSE:
                line(c, 6, 6, 18, 18);
                line(c, 18, 6, 6, 18);
                break;
            case KEY:
                c.drawCircle(8, 15.5f, 4.5f, p);
                line(c, 11.2f, 12.3f, 20, 3.5f);
                line(c, 16.5f, 7, 19, 9.5f);
                line(c, 14, 9.5f, 16, 11.5f);
                break;
            case EYE:
                path.reset();
                path.moveTo(2, 12);
                path.quadTo(12, 2, 22, 12);
                path.quadTo(12, 22, 2, 12);
                c.drawPath(path, p);
                c.drawCircle(12, 12, 3, p);
                break;
            case INFO:
                c.drawCircle(12, 12, 9.5f, p);
                line(c, 12, 11, 12, 16.5f);
                dot(c, 12, 7.8f, 1.3f);
                break;
            case PHONE:
                rrect(c, 6.5f, 2.5f, 17.5f, 21.5f, 2.5f);
                line(c, 10.5f, 18.5f, 13.5f, 18.5f);
                break;
            case LAYERS:
                closed(c, false, 12, 3, 21.5f, 8, 12, 13, 2.5f, 8);
                line(c, 2.5f, 12, 12, 17, 21.5f, 12);
                line(c, 2.5f, 16, 12, 21, 21.5f, 16);
                break;
            case TROPHY:
                line(c, 7, 3.5f, 17, 3.5f, 17, 10);
                path.reset();
                path.moveTo(7, 3.5f);
                path.lineTo(7, 10);
                path.cubicTo(7, 16, 17, 16, 17, 10);
                c.drawPath(path, p);
                arc(c, 4.5f, 8, 2.5f, 90, 180);
                arc(c, 19.5f, 8, 2.5f, -90, 180);
                line(c, 12, 15, 12, 19);
                line(c, 8, 20.5f, 16, 20.5f);
                break;
            case COPY:
                rrect(c, 8.5f, 8.5f, 20.5f, 20.5f, 2);
                line(c, 15.5f, 5, 15.5f, 3.5f, 3.5f, 3.5f, 3.5f, 15.5f, 5, 15.5f);
                break;
            case SPARK:
                line(c, 3, 17, 8, 11, 12, 14, 20, 5);
                line(c, 15, 5, 20, 5, 20, 10);
                break;
            case PIN:
                path.reset();
                path.moveTo(12, 21.5f);
                path.cubicTo(6, 15, 5, 12, 5, 9.5f);
                path.cubicTo(5, 5.5f, 8, 2.5f, 12, 2.5f);
                path.cubicTo(16, 2.5f, 19, 5.5f, 19, 9.5f);
                path.cubicTo(19, 12, 18, 15, 12, 21.5f);
                c.drawPath(path, p);
                c.drawCircle(12, 9.5f, 2.5f, p);
                break;
            case PLUS:
                line(c, 12, 5, 12, 19);
                line(c, 5, 12, 19, 12);
                break;
            case MINUS:
                line(c, 5, 12, 19, 12);
                break;
            case BATTERY:
                rrect(c, 2.5f, 7, 19, 17, 2);
                line(c, 21.5f, 10.5f, 21.5f, 13.5f);
                line(c, 9.5f, 9.5f, 7.5f, 12.5f, 11.5f, 11.5f, 9.5f, 14.5f);
                break;
            default:
                c.drawCircle(12, 12, 8, p);
        }
    }

    @Override public void setAlpha(int a) { p.setAlpha(a); }
    @Override public void setColorFilter(ColorFilter cf) { p.setColorFilter(cf); }
    @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
}
