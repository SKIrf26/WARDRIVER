package com.skirf.wardriver;

import android.view.View;

/** One tab. Built once; tick() runs twice a second while it is on screen. */
public abstract class Screen {
    protected final MainActivity a;
    protected View root;

    protected Screen(MainActivity a) { this.a = a; }

    public View view() {
        if (root == null) root = build();
        return root;
    }

    protected abstract View build();
    public void onShow() { tick(); }
    public void onHide() { }
    public void tick() { }
}
