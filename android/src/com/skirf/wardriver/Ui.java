package com.skirf.wardriver;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.text.InputType;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/** Building blocks for every screen, styled like the board and its portal. */
public final class Ui {
    static Activity act;
    static float density = 3f;

    public static void init(Activity a) {
        act = a;
        density = a.getResources().getDisplayMetrics().density;
    }

    public static int dp(float v) { return Math.round(v * density); }

    public static final LinearLayout.LayoutParams MW = new LinearLayout.LayoutParams(-1, -2);

    public static LinearLayout.LayoutParams lp(int w, int h) { return new LinearLayout.LayoutParams(w, h); }
    public static LinearLayout.LayoutParams lpw(float weight) { return new LinearLayout.LayoutParams(0, -2, weight); }
    public static LinearLayout.LayoutParams margins(LinearLayout.LayoutParams p, int l, int t, int r, int b) {
        p.setMargins(dp(l), dp(t), dp(r), dp(b));
        return p;
    }

    // ---------------- text ----------------
    public static TextView tv(String s, float sp, int color, Typeface tf) {
        TextView t = new TextView(act);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(tf);
        t.setIncludeFontPadding(false);
        t.setLineSpacing(0, 1.12f);
        return t;
    }

    public static TextView body(String s) {
        TextView t = tv(s, 15, Theme.MUTE, Theme.MED);
        t.setLineSpacing(0, 1.25f);
        return t;
    }

    public static TextView title(String s) { return tv(s, 17, Theme.TEXT, Theme.BOLD); }

    /** Small upper-case label, like the board's section headers. */
    public static TextView cap(String s) {
        TextView t = tv(s.toUpperCase(java.util.Locale.US), 12, Theme.MUTE, Theme.SEMI);
        t.setLetterSpacing(0.12f);
        return t;
    }

    /** Big number with even-width digits. */
    public static TextView big(String s, float sp, int color) {
        TextView t = tv(s, sp, color, Theme.BOLD);
        t.setFontFeatureSettings("tnum");
        t.setMaxLines(1);
        return t;
    }

    public static void single(TextView t) {
        t.setMaxLines(1);
        t.setEllipsize(TextUtils.TruncateAt.END);
    }

    // ---------------- shapes ----------------
    public static GradientDrawable round(int color, float radiusDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(color);
        g.setCornerRadius(dp(radiusDp));
        return g;
    }

    public static GradientDrawable outline(int fill, int stroke, float radiusDp, float strokeDp) {
        GradientDrawable g = round(fill, radiusDp);
        g.setStroke(Math.max(1, dp(strokeDp)), stroke);
        return g;
    }

    public static Drawable ripple(Drawable content, float radiusDp) {
        return new RippleDrawable(ColorStateList.valueOf(Theme.a(Color.WHITE, 40)), content, round(Color.WHITE, radiusDp));
    }

    public static void tap(View v, final Runnable r) {
        v.setClickable(true);
        v.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View view) {
                view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                r.run();
            }
        });
    }

    // ---------------- layout ----------------
    public static LinearLayout v() {
        LinearLayout l = new LinearLayout(act);
        l.setOrientation(LinearLayout.VERTICAL);
        return l;
    }

    public static LinearLayout h() {
        LinearLayout l = new LinearLayout(act);
        l.setOrientation(LinearLayout.HORIZONTAL);
        l.setGravity(Gravity.CENTER_VERTICAL);
        return l;
    }

    public static View space(int w, int h) {
        View s = new View(act);
        s.setLayoutParams(new LinearLayout.LayoutParams(dp(w), dp(h)));
        return s;
    }

    public static View grow() {
        View s = new View(act);
        s.setLayoutParams(new LinearLayout.LayoutParams(0, 1, 1f));
        return s;
    }

    public static View divider() {
        View d = new View(act);
        d.setBackgroundColor(Theme.LINE);
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, Math.max(1, dp(1)));
        p.setMargins(0, dp(12), 0, dp(12));
        d.setLayoutParams(p);
        return d;
    }

    /** Rounded card: the main surface on every screen. */
    public static LinearLayout card() {
        LinearLayout c = v();
        c.setBackground(round(Theme.CARD, 20));
        c.setPadding(dp(16), dp(16), dp(16), dp(16));
        c.setLayoutParams(margins(lp(-1, -2), 0, 0, 0, 12));
        return c;
    }

    /** A scrolling page with the standard side gutter. */
    public static ScrollView page(LinearLayout content) {
        ScrollView sv = new ScrollView(act);
        sv.setFillViewport(true);
        sv.setVerticalScrollBarEnabled(false);
        sv.setOverScrollMode(View.OVER_SCROLL_IF_CONTENT_SCROLLS);
        content.setPadding(dp(14), dp(6), dp(14), dp(28));
        sv.addView(content, new FrameLayout.LayoutParams(-1, -2));
        return sv;
    }

    /** Card header: label on the left, optional small text on the right. */
    public static LinearLayout head(String label, View right) {
        LinearLayout r = h();
        r.addView(cap(label), lpw(1));
        if (right != null) r.addView(right);
        r.setPadding(0, 0, 0, dp(12));
        return r;
    }

    public static ImageView icon(int kind, int color, int sizeDp) {
        ImageView iv = new ImageView(act);
        iv.setImageDrawable(new Icon(kind, color));
        iv.setLayoutParams(new LinearLayout.LayoutParams(dp(sizeDp), dp(sizeDp)));
        return iv;
    }

    public static View dot(int color, int sizeDp) {
        View d = new View(act);
        GradientDrawable g = new GradientDrawable();
        g.setShape(GradientDrawable.OVAL);
        g.setColor(color);
        d.setBackground(g);
        d.setLayoutParams(new LinearLayout.LayoutParams(dp(sizeDp), dp(sizeDp)));
        return d;
    }

    /** Pill-shaped status chip. */
    public static TextView chip(String text, int color) {
        TextView t = tv(text, 12, color, Theme.SEMI);
        t.setLetterSpacing(0.06f);
        t.setPadding(dp(10), dp(6), dp(10), dp(6));
        t.setBackground(round(Theme.a(color, 34), 99));
        t.setGravity(Gravity.CENTER);
        single(t);
        return t;
    }

    public static void chipColor(TextView t, String text, int color) {
        t.setText(text);
        t.setTextColor(color);
        t.setBackground(round(Theme.a(color, 34), 99));
    }

    // ---------------- buttons ----------------
    public static final int PRIMARY = 0, SECONDARY = 1, DANGER = 2, GHOST = 3;

    public static LinearLayout button(String text, int iconKind, int style, Runnable onTap) {
        LinearLayout b = h();
        b.setGravity(Gravity.CENTER);
        b.setMinimumHeight(dp(50));
        b.setPadding(dp(14), dp(12), dp(14), dp(12));
        int bg, fg;
        switch (style) {
            case PRIMARY: bg = Theme.ACC; fg = Theme.ON_ACC; break;
            case DANGER: bg = Theme.a(Theme.BAD, 40); fg = Theme.BAD; break;
            case GHOST: bg = Theme.a(Theme.CARD2, 0); fg = Theme.TEXT; break;
            default: bg = Theme.CARD2; fg = Theme.TEXT;
        }
        b.setBackground(ripple(round(bg, 14), 14));
        if (iconKind > 0) {
            ImageView iv = icon(iconKind, fg, 20);
            b.addView(iv);
            b.addView(space(8, 1));
        }
        TextView t = tv(text.toUpperCase(java.util.Locale.US), 14, fg, Theme.BOLD);
        t.setLetterSpacing(0.08f);
        single(t);
        b.addView(t);
        if (onTap != null) tap(b, onTap);
        return b;
    }

    /** Two or three buttons side by side. */
    public static LinearLayout row(View... views) {
        LinearLayout r = h();
        for (int i = 0; i < views.length; i++) {
            LinearLayout.LayoutParams p = lpw(1);
            if (i > 0) p.leftMargin = dp(8);
            r.addView(views[i], p);
        }
        return r;
    }

    /** Round icon-only button for map controls and toolbars. */
    public static FrameLayout roundBtn(int iconKind, int sizeDp, Runnable onTap) {
        FrameLayout f = new FrameLayout(act);
        f.setBackground(ripple(round(Theme.a(Theme.CARD2, 235), sizeDp / 2f), sizeDp / 2f));
        ImageView iv = new ImageView(act);
        iv.setImageDrawable(new Icon(iconKind, Theme.TEXT));
        int pad = dp(sizeDp * 0.26f);
        iv.setPadding(pad, pad, pad, pad);
        f.addView(iv, new FrameLayout.LayoutParams(-1, -1));
        f.setLayoutParams(new LinearLayout.LayoutParams(dp(sizeDp), dp(sizeDp)));
        f.setElevation(dp(4));
        if (onTap != null) tap(f, onTap);
        return f;
    }

    // ---------------- inputs ----------------
    public static EditText input(String hint, String value, boolean secret) {
        EditText e = new EditText(act);
        e.setHint(hint);
        e.setText(value);
        e.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        e.setTypeface(Theme.MED);
        e.setTextColor(Theme.TEXT);
        e.setHintTextColor(Theme.DIM);
        e.setSingleLine(true);
        e.setBackground(round(Theme.BG, 12));
        e.setPadding(dp(14), dp(13), dp(14), dp(13));
        e.setInputType(secret ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                : InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        e.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING);
        return e;
    }

    /** Input with an eye button that shows or hides a secret. */
    public static LinearLayout secretInput(final EditText e) {
        LinearLayout r = h();
        r.setBackground(round(Theme.BG, 12));
        e.setBackground(null);
        r.addView(e, lpw(1));
        final ImageView eye = icon(Icon.EYE, Theme.MUTE, 22);
        eye.setPadding(0, 0, 0, 0);
        LinearLayout.LayoutParams p = lp(dp(44), dp(44));
        r.addView(eye, p);
        eye.setPadding(dp(11), dp(11), dp(11), dp(11));
        tap(eye, new Runnable() {
            boolean shown;
            @Override public void run() {
                shown = !shown;
                int sel = e.getSelectionEnd();
                e.setInputType(shown ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                        : InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
                e.setTypeface(Theme.MED);
                e.setSelection(Math.max(0, Math.min(sel, e.length())));
                ((Icon) eye.getDrawable()).setColor(shown ? Theme.ACC : Theme.MUTE);
            }
        });
        return r;
    }

    // ---------------- toggle ----------------
    public static final class Toggle extends View {
        public interface Listener { void changed(boolean on); }
        private boolean on;
        private float pos;
        private Listener l;
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rr = new RectF();

        public Toggle(Context c, boolean on) {
            super(c);
            this.on = on;
            pos = on ? 1 : 0;
            setLayoutParams(new LinearLayout.LayoutParams(dp(52), dp(32)));
            setClickable(true);
            setOnClickListener(new OnClickListener() {
                @Override public void onClick(View v) { set(!Toggle.this.on, true); }
            });
        }

        public void listener(Listener l) { this.l = l; }
        public boolean on() { return on; }

        public void set(boolean v, boolean fromUser) {
            if (v == on) return;
            on = v;
            if (fromUser) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
            animate().setDuration(0);
            final float target = v ? 1 : 0;
            final float from = pos;
            final long t0 = System.currentTimeMillis();
            post(new Runnable() {
                @Override public void run() {
                    float k = Math.min(1f, (System.currentTimeMillis() - t0) / 140f);
                    pos = from + (target - from) * (1 - (1 - k) * (1 - k));
                    invalidate();
                    if (k < 1) postOnAnimation(this);
                }
            });
            if (fromUser && l != null) l.changed(v);
        }

        @Override protected void onDraw(Canvas c) {
            float w = getWidth(), h = getHeight();
            rr.set(0, 0, w, h);
            p.setColor(Theme.mix(Theme.LINE, Theme.ACC, pos));
            c.drawRoundRect(rr, h / 2, h / 2, p);
            float r = h / 2 - dp(4);
            float cx = h / 2 + (w - h) * pos;
            p.setColor(Theme.mix(Theme.MUTE, Theme.ON_ACC, pos));
            c.drawCircle(cx, h / 2, r, p);
        }
    }

    // ---------------- segmented choice ----------------
    public interface Choice { void chose(int index); }

    public static LinearLayout seg(String[] labels, int selected, final Choice cb) {
        final LinearLayout box = h();
        box.setBackground(round(Theme.BG, 14));
        box.setPadding(dp(4), dp(4), dp(4), dp(4));
        for (int i = 0; i < labels.length; i++) {
            final int idx = i;
            final TextView t = tv(labels[i], 14, Theme.MUTE, Theme.BOLD);
            t.setGravity(Gravity.CENTER);
            t.setPadding(dp(4), dp(11), dp(4), dp(11));
            single(t);
            box.addView(t, lpw(1));
            tap(t, new Runnable() {
                @Override public void run() {
                    segSelect(box, idx);
                    cb.chose(idx);
                }
            });
        }
        segSelect(box, selected);
        return box;
    }

    public static void segSelect(LinearLayout box, int sel) {
        for (int i = 0; i < box.getChildCount(); i++) {
            TextView t = (TextView) box.getChildAt(i);
            boolean on = i == sel;
            t.setTextColor(on ? Theme.TEXT : Theme.MUTE);
            if (on) {
                GradientDrawable g = round(Theme.CARD2, 11);
                t.setBackground(new android.graphics.drawable.LayerDrawable(new Drawable[]{g, new Underline()}));
            } else t.setBackground(null);
        }
    }

    /** Accent bar under the selected segment. */
    static final class Underline extends Drawable {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        @Override public void draw(Canvas c) {
            p.setColor(Theme.ACC);
            android.graphics.Rect b = getBounds();
            float w = b.width() * 0.34f;
            RectF r = new RectF(b.centerX() - w / 2, b.bottom - dp(4), b.centerX() + w / 2, b.bottom - dp(1.5f));
            c.drawRoundRect(r, dp(2), dp(2), p);
        }
        @Override public void setAlpha(int a) { }
        @Override public void setColorFilter(android.graphics.ColorFilter cf) { }
        @Override public int getOpacity() { return android.graphics.PixelFormat.TRANSLUCENT; }
    }

    // ---------------- settings rows ----------------
    /** Title + optional explanation on the left, control on the right. */
    public static LinearLayout settingRow(String title, String sub, View control) {
        LinearLayout r = h();
        r.setPadding(0, dp(6), 0, dp(6));
        LinearLayout tx = v();
        TextView t = tv(title, 16, Theme.TEXT, Theme.SEMI);
        tx.addView(t);
        if (sub != null && sub.length() > 0) {
            TextView s = tv(sub, 13, Theme.MUTE, Theme.MED);
            s.setLineSpacing(0, 1.2f);
            s.setPadding(0, dp(4), 0, 0);
            tx.addView(s);
        }
        r.addView(tx, lpw(1));
        if (control != null) {
            r.addView(space(12, 1));
            r.addView(control);
        }
        return r;
    }

    // ---------------- sheet (bottom dialog) ----------------
    public static Dialog sheet(View content) {
        final Dialog d = new Dialog(act);
        d.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout box = v();
        box.setBackground(round(Theme.CARD, 26));
        box.setPadding(dp(18), dp(10), dp(18), dp(22));
        View grab = new View(act);
        grab.setBackground(round(Theme.LINE, 3));
        LinearLayout.LayoutParams gp = lp(dp(40), dp(5));
        gp.gravity = Gravity.CENTER_HORIZONTAL;
        gp.bottomMargin = dp(14);
        box.addView(grab, gp);
        ScrollView sv = new ScrollView(act);
        sv.setVerticalScrollBarEnabled(false);
        sv.addView(content);
        box.addView(sv, new LinearLayout.LayoutParams(-1, -2));
        FrameLayout outer = new FrameLayout(act);
        outer.setPadding(dp(8), 0, dp(8), dp(8));
        outer.addView(box, new FrameLayout.LayoutParams(-1, -2));
        d.setContentView(outer);
        Window w = d.getWindow();
        if (w != null) {
            w.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            w.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            w.setGravity(Gravity.BOTTOM);
            w.setDimAmount(0.7f);
            w.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            w.setWindowAnimations(android.R.style.Animation_InputMethod);
            w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }
        d.setCanceledOnTouchOutside(true);
        return d;
    }

    /** Title, message and up to two buttons. */
    public static Dialog confirm(String title, String msg, String yes, int yesStyle, final Runnable onYes) {
        LinearLayout c = v();
        c.addView(tv(title, 22, Theme.TEXT, Theme.BOLD));
        TextView m = body(msg);
        m.setPadding(0, dp(10), 0, dp(20));
        c.addView(m);
        final Dialog[] d = new Dialog[1];
        LinearLayout no = button("Cancel", 0, SECONDARY, new Runnable() { @Override public void run() { d[0].dismiss(); } });
        LinearLayout ok = button(yes, 0, yesStyle, new Runnable() {
            @Override public void run() { d[0].dismiss(); if (onYes != null) onYes.run(); }
        });
        c.addView(row(no, ok));
        d[0] = sheet(c);
        d[0].show();
        return d[0];
    }

    public static void info(String title, String msg) {
        LinearLayout c = v();
        c.addView(tv(title, 22, Theme.TEXT, Theme.BOLD));
        TextView m = body(msg);
        m.setPadding(0, dp(10), 0, dp(20));
        c.addView(m);
        final Dialog[] d = new Dialog[1];
        c.addView(button("OK", 0, PRIMARY, new Runnable() { @Override public void run() { d[0].dismiss(); } }));
        d[0] = sheet(c);
        d[0].show();
    }

    // ---------------- toast ----------------
    static FrameLayout toastLayer;
    private static TextView toastView;
    private static final Runnable hideToast = new Runnable() {
        @Override public void run() {
            if (toastView != null) toastView.animate().alpha(0).translationY(dp(12)).setDuration(180).start();
        }
    };

    static void resetToast() { toastView = null; }

    public static void toast(String msg) {
        if (toastLayer == null) return;
        if (toastView == null) {
            toastView = tv("", 15, Theme.TEXT, Theme.SEMI);
            toastView.setBackground(round(0xF0262B36, 14));
            toastView.setPadding(dp(16), dp(13), dp(16), dp(13));
            toastView.setElevation(dp(8));
            toastView.setMaxLines(4);
            FrameLayout.LayoutParams p = new FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM);
            p.setMargins(dp(14), 0, dp(14), dp(84));
            toastLayer.addView(toastView, p);
            toastView.setOnTouchListener(new View.OnTouchListener() {
                @Override public boolean onTouch(View v, MotionEvent e) { hideToast.run(); return false; }
            });
        }
        toastView.setText(msg);
        toastView.removeCallbacks(hideToast);
        toastView.setAlpha(0);
        toastView.setTranslationY(dp(12));
        toastView.animate().alpha(1).translationY(0).setDuration(180).start();
        toastView.postDelayed(hideToast, Math.min(7000, 2200 + msg.length() * 45L));
    }

    private Ui() {}
}
