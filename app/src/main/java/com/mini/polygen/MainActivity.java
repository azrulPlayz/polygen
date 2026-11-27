package com.mini.polygen;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity {
    // tema cokelat ala Blockbench
    private static final int BG = 0xFF2B2121, TOP = 0xFF362B2A, BAR = 0xFF574645, PANEL = 0xFF473736,
            ACC = 0xFFE8706E, TXT = 0xFFF7CDB0, DIM = 0xFFB89A92, BTN = 0xFF5E4B49, ROWSEL = 0xFF6B5252,
            FIELD = 0xFF3A2D2C, ONACC = 0xFF2B2121;
    private static final int MATCH = ViewGroup.LayoutParams.MATCH_PARENT, WRAP = ViewGroup.LayoutParams.WRAP_CONTENT;
    private static final int TAB_OUT = 1, TAB_PROPS = 2, TAB_TEX = 3, TAB_UV = 4;
    private static final String[] SHAPES = {"Kubus", "Kubus Bevel", "Piramida", "Bidang", "Lingkaran", "Silinder",
            "Tabung", "Kerucut", "Bola", "Ikosfer", "Oktahedron", "Dodekahedron", "Torus"};

    // ---------- ikon yang digambar sendiri ----------
    static final int I_MENU = 0, I_SEARCH = 1, I_HOME = 2, I_UNDO = 3, I_REDO = 4, I_CUBE = 5, I_BRUSH = 6,
            I_ERASER = 7, I_BUCKET = 8, I_PIPETTE = 9, I_EYE = 10, I_EYE_OFF = 11, I_MOVE = 12, I_ROTATE = 13,
            I_SCALE = 14, I_LIST = 15, I_IMAGE = 16, I_UV = 17, I_PLUS = 18, I_FOLDER = 19, I_FOLDER_PLUS = 20,
            I_CLOSE = 21, I_DOTS = 22, I_CHEV_DOWN = 23, I_CHEV_RIGHT = 24, I_3D = 25;

    static class IconView extends View {
        private int kind;
        private int color;
        private boolean active;
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final float density;

        IconView(Context c, int kind, int color) {
            super(c);
            this.kind = kind;
            this.color = color;
            this.density = c.getResources().getDisplayMetrics().density;
        }

        void setKind(int k) { if (k != kind) { kind = k; invalidate(); } }
        void setActive(boolean a) { if (a != active) { active = a; invalidate(); } }
        void setIconColor(int c) { if (c != color) { color = c; invalidate(); } }

        private void head(Canvas cv, float tx, float ty, float dx, float dy, float len) {
            double a = Math.atan2(dy, dx);
            for (int k = -1; k <= 1; k += 2) {
                double b = a + Math.PI + k * 0.55;
                cv.drawLine(tx, ty, tx + (float) Math.cos(b) * len, ty + (float) Math.sin(b) * len, p);
            }
        }

        private void poly(Canvas cv, boolean close, float... pts) {
            Path pa = new Path();
            pa.moveTo(pts[0], pts[1]);
            for (int i = 2; i + 1 < pts.length; i += 2) pa.lineTo(pts[i], pts[i + 1]);
            if (close) pa.close();
            cv.drawPath(pa, p);
        }

        private void dot(Canvas cv, float x, float y, float r) {
            p.setStyle(Paint.Style.FILL);
            cv.drawCircle(x, y, r, p);
            p.setStyle(Paint.Style.STROKE);
        }

        private void arcArrow(Canvas cv, float cx, float cy, float s) {  // panah melengkung (undo)
            RectF oval = new RectF(cx - 0.6f * s, cy - 0.45f * s, cx + 0.6f * s, cy + 0.75f * s);
            cv.drawArc(oval, 210f, 230f, false, p);
            double th = Math.toRadians(210);
            float sx = cx + 0.6f * s * (float) Math.cos(th), sy = cy + 0.15f * s + 0.6f * s * (float) Math.sin(th);
            head(cv, sx, sy, (float) Math.sin(th), -(float) Math.cos(th), 0.45f * s);
        }

        @Override
        protected void onDraw(Canvas cv) {
            float w = getWidth(), h = getHeight(), cx = w / 2, cy = h / 2, s = Math.min(w, h) * 0.30f;
            if (active) {
                p.setStyle(Paint.Style.FILL);
                p.setColor(ACC);
                cv.drawRoundRect(new RectF(w * 0.08f, h * 0.08f, w * 0.92f, h * 0.92f), 10 * density, 10 * density, p);
            }
            p.setColor(active ? ONACC : color);
            p.setStyle(Paint.Style.STROKE);
            p.setStrokeWidth(Math.max(2f, s * 0.16f));
            p.setStrokeCap(Paint.Cap.ROUND);
            p.setStrokeJoin(Paint.Join.ROUND);
            switch (kind) {
                case I_MENU:
                    for (int i = -1; i <= 1; i++) cv.drawLine(cx - 0.8f * s, cy + i * 0.6f * s, cx + 0.8f * s, cy + i * 0.6f * s, p);
                    break;
                case I_SEARCH:
                    cv.drawCircle(cx - 0.15f * s, cy - 0.15f * s, 0.5f * s, p);
                    cv.drawLine(cx + 0.2f * s, cy + 0.2f * s, cx + 0.8f * s, cy + 0.8f * s, p);
                    break;
                case I_HOME:
                    poly(cv, false, cx - 0.8f * s, cy, cx, cy - 0.7f * s, cx + 0.8f * s, cy);
                    poly(cv, false, cx - 0.55f * s, cy - 0.1f * s, cx - 0.55f * s, cy + 0.65f * s, cx + 0.55f * s, cy + 0.65f * s, cx + 0.55f * s, cy - 0.1f * s);
                    break;
                case I_UNDO:
                    arcArrow(cv, cx, cy, s);
                    break;
                case I_REDO:
                    cv.save();
                    cv.scale(-1f, 1f, cx, cy);
                    arcArrow(cv, cx, cy, s);
                    cv.restore();
                    break;
                case I_CUBE: {
                    float[] hx = new float[6], hy = new float[6];
                    for (int i = 0; i < 6; i++) {
                        double a = Math.toRadians(-90 + 60 * i);
                        hx[i] = cx + 0.8f * s * (float) Math.cos(a);
                        hy[i] = cy + 0.8f * s * (float) Math.sin(a);
                    }
                    poly(cv, true, hx[0], hy[0], hx[1], hy[1], hx[2], hy[2], hx[3], hy[3], hx[4], hy[4], hx[5], hy[5]);
                    cv.drawLine(cx, cy, hx[1], hy[1], p);
                    cv.drawLine(cx, cy, hx[3], hy[3], p);
                    cv.drawLine(cx, cy, hx[5], hy[5], p);
                    break;
                }
                case I_BRUSH:
                    cv.drawLine(cx - 0.2f * s, cy + 0.2f * s, cx + 0.7f * s, cy - 0.7f * s, p);
                    p.setStrokeWidth(Math.max(2f, s * 0.35f));
                    cv.drawLine(cx - 0.2f * s, cy + 0.2f * s, cx - 0.35f * s, cy + 0.35f * s, p);
                    p.setStrokeWidth(Math.max(2f, s * 0.16f));
                    dot(cv, cx - 0.5f * s, cy + 0.55f * s, 0.28f * s);
                    break;
                case I_ERASER:
                    poly(cv, true, cx - 0.75f * s, cy + 0.1f * s, cx - 0.1f * s, cy - 0.65f * s, cx + 0.75f * s, cy + 0.1f * s, cx + 0.1f * s, cy + 0.65f * s);
                    cv.drawLine(cx - 0.3f * s, cy + 0.38f * s, cx + 0.45f * s, cy - 0.35f * s, p);
                    break;
                case I_BUCKET:
                    poly(cv, true, cx - 0.1f * s, cy - 0.75f * s, cx + 0.6f * s, cy - 0.05f * s, cx - 0.1f * s, cy + 0.6f * s, cx - 0.8f * s, cy - 0.05f * s);
                    dot(cv, cx + 0.75f * s, cy + 0.45f * s, 0.18f * s);
                    break;
                case I_PIPETTE:
                    cv.drawLine(cx - 0.75f * s, cy + 0.75f * s, cx + 0.25f * s, cy - 0.25f * s, p);
                    p.setStrokeWidth(Math.max(2f, s * 0.35f));
                    cv.drawLine(cx + 0.3f * s, cy - 0.3f * s, cx + 0.6f * s, cy - 0.6f * s, p);
                    break;
                case I_EYE:
                case I_EYE_OFF:
                    cv.drawOval(new RectF(cx - 0.8f * s, cy - 0.45f * s, cx + 0.8f * s, cy + 0.45f * s), p);
                    dot(cv, cx, cy, 0.2f * s);
                    if (kind == I_EYE_OFF) cv.drawLine(cx - 0.7f * s, cy + 0.6f * s, cx + 0.7f * s, cy - 0.6f * s, p);
                    break;
                case I_MOVE:
                    cv.drawLine(cx - 0.8f * s, cy, cx + 0.8f * s, cy, p);
                    cv.drawLine(cx, cy - 0.8f * s, cx, cy + 0.8f * s, p);
                    head(cv, cx + 0.8f * s, cy, 1, 0, 0.3f * s);
                    head(cv, cx - 0.8f * s, cy, -1, 0, 0.3f * s);
                    head(cv, cx, cy - 0.8f * s, 0, -1, 0.3f * s);
                    head(cv, cx, cy + 0.8f * s, 0, 1, 0.3f * s);
                    break;
                case I_ROTATE: {
                    RectF oval = new RectF(cx - 0.65f * s, cy - 0.65f * s, cx + 0.65f * s, cy + 0.65f * s);
                    cv.drawArc(oval, -60f, 290f, false, p);
                    double th = Math.toRadians(230);
                    float ex = cx + 0.65f * s * (float) Math.cos(th), ey = cy + 0.65f * s * (float) Math.sin(th);
                    head(cv, ex, ey, -(float) Math.sin(th), (float) Math.cos(th), 0.4f * s);
                    break;
                }
                case I_SCALE:
                    poly(cv, true, cx - 0.75f * s, cy - 0.75f * s, cx + 0.05f * s, cy - 0.75f * s, cx + 0.05f * s, cy + 0.05f * s, cx - 0.75f * s, cy + 0.05f * s);
                    cv.drawLine(cx - 0.1f * s, cy - 0.1f * s, cx + 0.75f * s, cy + 0.75f * s, p);
                    head(cv, cx + 0.75f * s, cy + 0.75f * s, 1, 1, 0.4f * s);
                    break;
                case I_LIST:
                    for (int i = -1; i <= 1; i++) {
                        dot(cv, cx - 0.7f * s, cy + i * 0.6f * s, 0.1f * s);
                        cv.drawLine(cx - 0.35f * s, cy + i * 0.6f * s, cx + 0.8f * s, cy + i * 0.6f * s, p);
                    }
                    break;
                case I_IMAGE:
                    cv.drawRoundRect(new RectF(cx - 0.8f * s, cy - 0.65f * s, cx + 0.8f * s, cy + 0.65f * s), 3 * density, 3 * density, p);
                    poly(cv, false, cx - 0.6f * s, cy + 0.4f * s, cx - 0.15f * s, cy - 0.1f * s, cx + 0.15f * s, cy + 0.25f * s, cx + 0.35f * s, cy + 0.05f * s, cx + 0.6f * s, cy + 0.4f * s);
                    dot(cv, cx + 0.4f * s, cy - 0.3f * s, 0.12f * s);
                    break;
                case I_UV:
                    cv.drawRect(cx - 0.75f * s, cy - 0.75f * s, cx + 0.75f * s, cy + 0.75f * s, p);
                    cv.drawRect(cx - 0.4f * s, cy - 0.4f * s, cx + 0.05f * s, cy + 0.05f * s, p);
                    cv.drawLine(cx + 0.05f * s, cy + 0.05f * s, cx + 0.5f * s, cy + 0.5f * s, p);
                    break;
                case I_PLUS:
                    cv.drawLine(cx - 0.7f * s, cy, cx + 0.7f * s, cy, p);
                    cv.drawLine(cx, cy - 0.7f * s, cx, cy + 0.7f * s, p);
                    break;
                case I_FOLDER:
                case I_FOLDER_PLUS:
                    poly(cv, true, cx - 0.8f * s, cy - 0.55f * s, cx - 0.2f * s, cy - 0.55f * s, cx, cy - 0.3f * s, cx + 0.8f * s, cy - 0.3f * s,
                            cx + 0.8f * s, cy + 0.6f * s, cx - 0.8f * s, cy + 0.6f * s);
                    if (kind == I_FOLDER_PLUS) {
                        cv.drawLine(cx - 0.25f * s, cy + 0.15f * s, cx + 0.25f * s, cy + 0.15f * s, p);
                        cv.drawLine(cx, cy - 0.1f * s, cx, cy + 0.4f * s, p);
                    }
                    break;
                case I_CLOSE:
                    cv.drawLine(cx - 0.6f * s, cy - 0.6f * s, cx + 0.6f * s, cy + 0.6f * s, p);
                    cv.drawLine(cx - 0.6f * s, cy + 0.6f * s, cx + 0.6f * s, cy - 0.6f * s, p);
                    break;
                case I_DOTS:
                    for (int i = -1; i <= 1; i++) dot(cv, cx, cy + i * 0.6f * s, 0.12f * s);
                    break;
                case I_CHEV_DOWN:
                    poly(cv, false, cx - 0.5f * s, cy - 0.2f * s, cx, cy + 0.3f * s, cx + 0.5f * s, cy - 0.2f * s);
                    break;
                case I_CHEV_RIGHT:
                    poly(cv, false, cx - 0.2f * s, cy - 0.5f * s, cx + 0.3f * s, cy, cx - 0.2f * s, cy + 0.5f * s);
                    break;
                default:  // I_3D
                    p.setStyle(Paint.Style.FILL);
                    p.setTextSize(s * 1.25f);
                    p.setTextAlign(Paint.Align.CENTER);
                    p.setFakeBoldText(true);
                    cv.drawText("3D", cx, cy + s * 0.45f, p);
                    p.setFakeBoldText(false);
                    break;
            }
        }
    }

    // ---------- editor UV ----------
    interface UvListener {
        void onSelect(int face);
        void onDragStart();
        void onChange(int face, float[] rect);
    }

    static class UvView extends View {
        Bitmap bmp;
        float[][] rects = new float[0][];
        int sel = -1;
        UvListener listener;
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private int mode;  // 0 tidak ada, 1 geser, 2 ubah ukuran
        private float lastU, lastV;

        UvView(Context c) { super(c); }

        private float side() { return Math.min(getWidth(), getHeight()); }
        private float ox() { return (getWidth() - side()) / 2f; }
        private float oy() { return (getHeight() - side()) / 2f; }

        @Override
        protected void onDraw(Canvas cv) {
            float s = side(), x0 = ox(), y0 = oy();
            p.setStyle(Paint.Style.FILL);
            p.setColor(0xFF3A2D2C);
            cv.drawRect(x0, y0, x0 + s, y0 + s, p);
            if (bmp == null) {
                p.setColor(0xFFB89A92);
                p.setTextSize(13 * getResources().getDisplayMetrics().density);
                p.setTextAlign(Paint.Align.CENTER);
                cv.drawText("Belum ada tekstur (buat di tab Tekstur)", getWidth() / 2f, getHeight() / 2f, p);
                return;
            }
            p.setFilterBitmap(false);
            cv.drawBitmap(bmp, null, new RectF(x0, y0, x0 + s, y0 + s), p);
            p.setStyle(Paint.Style.STROKE);
            p.setStrokeWidth(1f);
            p.setColor(0x88F7CDB0);
            for (int i = 0; i < rects.length; i++) {
                if (i == sel) continue;
                float[] r = rects[i];
                cv.drawRect(x0 + r[0] * s, y0 + r[1] * s, x0 + r[2] * s, y0 + r[3] * s, p);
            }
            if (sel >= 0 && sel < rects.length) {
                float[] r = rects[sel];
                RectF rr = new RectF(x0 + r[0] * s, y0 + r[1] * s, x0 + r[2] * s, y0 + r[3] * s);
                p.setStyle(Paint.Style.FILL);
                p.setColor(0x44E8706E);
                cv.drawRect(rr, p);
                p.setStyle(Paint.Style.STROKE);
                p.setStrokeWidth(3f);
                p.setColor(0xFFE8706E);
                cv.drawRect(rr, p);
                p.setStyle(Paint.Style.FILL);
                cv.drawCircle(rr.right, rr.bottom, 8f, p);
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent e) {
            if (bmp == null || listener == null) return true;
            float s = side();
            float u = (e.getX() - ox()) / s, v = (e.getY() - oy()) / s;
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: {
                    getParent().requestDisallowInterceptTouchEvent(true);
                    mode = 0;
                    if (sel >= 0 && sel < rects.length) {
                        float[] r = rects[sel];
                        float tol = 24f / s;
                        if (Math.abs(u - r[2]) < tol && Math.abs(v - r[3]) < tol) mode = 2;
                        else if (u >= r[0] && u <= r[2] && v >= r[1] && v <= r[3]) mode = 1;
                    }
                    if (mode == 0) {  // pilih sisi terkecil yang berisi titik ini
                        int best = -1;
                        float bestA = 1e9f;
                        for (int i = 0; i < rects.length; i++) {
                            float[] r = rects[i];
                            if (u >= r[0] && u <= r[2] && v >= r[1] && v <= r[3]) {
                                float a = (r[2] - r[0]) * (r[3] - r[1]);
                                if (a < bestA) { bestA = a; best = i; }
                            }
                        }
                        if (best >= 0) listener.onSelect(best);
                    } else {
                        listener.onDragStart();
                    }
                    lastU = u; lastV = v;
                    return true;
                }
                case MotionEvent.ACTION_MOVE:
                    if (mode != 0 && sel >= 0 && sel < rects.length) {
                        float[] r = rects[sel].clone();
                        float du = u - lastU, dv = v - lastV;
                        if (mode == 1) {
                            r[0] += du; r[2] += du; r[1] += dv; r[3] += dv;
                        } else {
                            r[2] = Math.max(r[0] + 0.01f, r[2] + du);
                            r[3] = Math.max(r[1] + 0.01f, r[3] + dv);
                        }
                        lastU = u; lastV = v;
                        listener.onChange(sel, r);
                    }
                    return true;
                default:
                    return true;
            }
        }
    }

    // ---------- field angka yang bisa digeser ----------
    interface NumTarget {
        float get();
        void set(float v);
    }

    private static class Binding {
        final EditText e; final NumTarget t; final String fmt;
        Binding(EditText e, NumTarget t, String fmt) { this.e = e; this.t = t; this.fmt = fmt; }
    }

    // ---------- state UI ----------
    private GLSurfaceView gl;
    private View rootView;
    private LinearLayout outlinerList, propsBox, bar2Inner, axisGroup, editGroup, paintGroup, texList, faceRow,
            outlinerPanel, texPanel, uvPanel, panelBox, coordBar, toolCol;
    private ScrollView propsScroll;
    private TextView coordLabel, outCount;
    private IconView btnUndo, btnRedo, btnMode, swatchHolder;
    private View swatch;
    private final IconView[] tabIcons = new IconView[5], toolIcons = new IconView[4], paintIcons = new IconView[4];
    private final Button[] axisBtns = new Button[4];
    private Button btnVtx, btnFace;
    private UvView uvView;
    private final List<Binding> bindings = new ArrayList<>();
    private final List<Button> faceChips = new ArrayList<>();
    private int tab = TAB_OUT;
    private boolean collapsed = false;
    private int lastVer = -1;
    private String filter = "";
    private int uvObj = -2, uvFace = 0, uvChipsKey = -1;
    private int texCounter = 0;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (NativeLib.version() != lastVer) sync();
            handler.postDelayed(this, 150);
        }
    };

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        NativeLib.load(this);  // harus pertama: pakai .so pengganti kalau ada

        gl = new GLSurfaceView(this) {
            @Override
            public boolean onTouchEvent(MotionEvent e) {
                int a = e.getActionMasked();
                if (a == MotionEvent.ACTION_DOWN) { rootView.requestFocus(); hideKeyboard(this); }
                int n = e.getPointerCount();
                float px = e.getX(0), py = e.getY(0);
                float qx = n > 1 ? e.getX(1) : 0, qy = n > 1 ? e.getY(1) : 0;
                if (a == MotionEvent.ACTION_POINTER_UP) {
                    int keep = e.getActionIndex() == 0 ? 1 : 0;
                    px = e.getX(keep);
                    py = e.getY(keep);
                    n = 1;
                }
                NativeLib.touch(a, px, py, qx, qy, n);
                return true;
            }
        };
        gl.setEGLContextClientVersion(3);
        gl.setRenderer(new GLSurfaceView.Renderer() {
            @Override public void onSurfaceCreated(GL10 g, EGLConfig c) { NativeLib.init(); }
            @Override public void onSurfaceChanged(GL10 g, int w, int h) { NativeLib.resize(w, h); }
            @Override public void onDrawFrame(GL10 g) { NativeLib.draw(); }
        });

        LinearLayout root = new LinearLayout(this);
        rootView = root;
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setFitsSystemWindows(true);
        root.setFocusableInTouchMode(true);

        root.addView(buildTopBar(), new LinearLayout.LayoutParams(MATCH, WRAP));
        root.addView(buildBar2(), new LinearLayout.LayoutParams(MATCH, dp(46)));

        FrameLayout vp = new FrameLayout(this);
        vp.addView(gl, new FrameLayout.LayoutParams(MATCH, MATCH));
        FrameLayout.LayoutParams tp = new FrameLayout.LayoutParams(WRAP, WRAP, Gravity.END | Gravity.CENTER_VERTICAL);
        tp.setMargins(0, 0, dp(4), 0);
        vp.addView(buildToolColumn(), tp);
        root.addView(vp, new LinearLayout.LayoutParams(MATCH, 0, 1f));

        root.addView(buildBottom(), new LinearLayout.LayoutParams(MATCH, WRAP));
        setContentView(root);
        applyTabs();
    }

    // ---------- bangun UI ----------
    private IconView icon(int kind, View.OnClickListener l) {
        IconView v = new IconView(this, kind, TXT);
        v.setOnClickListener(l);
        return v;
    }

    private LinearLayout buildTopBar() {
        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setBackgroundColor(TOP);
        top.setGravity(Gravity.CENTER_VERTICAL);
        top.setPadding(dp(4), dp(2), dp(4), dp(2));
        top.addView(icon(I_MENU, v -> showMainMenu()), new LinearLayout.LayoutParams(dp(48), dp(48)));
        top.addView(icon(I_SEARCH, v -> showSearch()), new LinearLayout.LayoutParams(dp(48), dp(48)));
        top.addView(icon(I_HOME, v -> { NativeLib.action(20); sync(); }), new LinearLayout.LayoutParams(dp(48), dp(48)));
        btnUndo = icon(I_UNDO, v -> { NativeLib.action(2); sync(); });
        btnRedo = icon(I_REDO, v -> { NativeLib.action(3); sync(); });
        top.addView(btnUndo, new LinearLayout.LayoutParams(dp(48), dp(48)));
        top.addView(btnRedo, new LinearLayout.LayoutParams(dp(48), dp(48)));
        top.addView(new View(this), new LinearLayout.LayoutParams(0, 1, 1f));
        btnMode = icon(I_CUBE, v -> {
            PopupMenu m = new PopupMenu(this, v);
            m.getMenu().add(0, 0, 0, "Objek");
            m.getMenu().add(0, 1, 1, "Edit mesh");
            m.getMenu().add(0, 2, 2, "Paint");
            m.setOnMenuItemClickListener(i -> { NativeLib.set(3, i.getItemId()); sync(); return true; });
            m.show();
        });
        top.addView(btnMode, new LinearLayout.LayoutParams(dp(48), dp(48)));
        return top;
    }

    private View buildBar2() {
        HorizontalScrollView hs = new HorizontalScrollView(this);
        hs.setBackgroundColor(BAR);
        hs.setHorizontalScrollBarEnabled(false);
        bar2Inner = new LinearLayout(this);
        bar2Inner.setOrientation(LinearLayout.HORIZONTAL);
        bar2Inner.setGravity(Gravity.CENTER_VERTICAL);
        bar2Inner.setPadding(dp(6), dp(3), dp(6), dp(3));
        hs.addView(bar2Inner, new ViewGroup.LayoutParams(WRAP, MATCH));

        axisGroup = new LinearLayout(this);
        axisGroup.setOrientation(LinearLayout.HORIZONTAL);
        axisGroup.setGravity(Gravity.CENTER_VERTICAL);
        axisGroup.addView(label("Sumbu", 12, DIM));
        String[] an = {"X", "Y", "Z", "N"};
        for (int i = 0; i < 4; i++) {
            final int idx = i;
            axisBtns[i] = chip(an[i], v -> { NativeLib.set(2, idx); sync(); });
            axisGroup.addView(axisBtns[i], margin(dp(40), WRAP, 0f, 2));
        }
        bar2Inner.addView(axisGroup, new LinearLayout.LayoutParams(WRAP, MATCH));

        editGroup = new LinearLayout(this);
        editGroup.setOrientation(LinearLayout.HORIZONTAL);
        editGroup.setGravity(Gravity.CENTER_VERTICAL);
        btnVtx = chip("Vertex", v -> { NativeLib.set(0, 0); sync(); });
        btnFace = chip("Face", v -> { NativeLib.set(0, 1); sync(); });
        Button ext = chip("Extrude", v -> { NativeLib.action(0); sync(); });
        Button dup = chip("Duplikat", v -> { NativeLib.action(4); sync(); });
        Button del = chip("Hapus", v -> { NativeLib.action(1); sync(); });
        editGroup.addView(btnVtx, margin(WRAP, WRAP, 0f, 2));
        editGroup.addView(btnFace, margin(WRAP, WRAP, 0f, 2));
        editGroup.addView(ext, margin(WRAP, WRAP, 0f, 2));
        bar2Inner.addView(editGroup, new LinearLayout.LayoutParams(WRAP, MATCH));
        // Duplikat & Hapus selalu ada (mode objek/edit)
        LinearLayout common = new LinearLayout(this);
        common.setOrientation(LinearLayout.HORIZONTAL);
        common.setGravity(Gravity.CENTER_VERTICAL);
        common.addView(dup, margin(WRAP, WRAP, 0f, 2));
        common.addView(del, margin(WRAP, WRAP, 0f, 2));
        common.setTag("common");
        bar2Inner.addView(common, new LinearLayout.LayoutParams(WRAP, MATCH));

        paintGroup = new LinearLayout(this);
        paintGroup.setOrientation(LinearLayout.HORIZONTAL);
        paintGroup.setGravity(Gravity.CENTER_VERTICAL);
        int[] pk = {I_BRUSH, I_ERASER, I_BUCKET, I_PIPETTE};
        for (int i = 0; i < 4; i++) {
            final int idx = i;
            paintIcons[i] = icon(pk[i], v -> { NativeLib.set(4, idx); sync(); });
            paintGroup.addView(paintIcons[i], new LinearLayout.LayoutParams(dp(42), dp(40)));
        }
        swatch = new View(this);
        swatch.setBackgroundColor(0xFFE85050);
        swatch.setOnClickListener(v -> pickBrushColor());
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(dp(36), dp(32));
        sp.setMargins(dp(8), 0, dp(8), 0);
        paintGroup.addView(swatch, sp);
        paintGroup.addView(label("Ukuran", 12, DIM));
        EditText sizeField = scrubField(0.05f, 0, new NumTarget() {
            @Override public float get() { int[] s = NativeLib.state(); return s != null && s.length > 9 ? s[9] : 2; }
            @Override public void set(float v) { NativeLib.set(5, Math.round(v)); }
        });
        paintGroup.addView(sizeField, margin(dp(54), WRAP, 0f, 2));
        bar2Inner.addView(paintGroup, new LinearLayout.LayoutParams(WRAP, MATCH));
        return hs;
    }

    private View buildToolColumn() {
        toolCol = new LinearLayout(this);
        toolCol.setOrientation(LinearLayout.VERTICAL);
        toolCol.setBackgroundColor(0x66362B2A);
        int[] kinds = {I_EYE, I_MOVE, I_SCALE, I_ROTATE};
        int[] toolIdx = {0, 1, 2, 3};  // kamera, geser, ukuran, putar
        for (int i = 0; i < 4; i++) {
            final int t = toolIdx[i];
            toolIcons[i] = icon(kinds[i], v -> { NativeLib.set(1, t); sync(); });
            toolCol.addView(toolIcons[i], new LinearLayout.LayoutParams(dp(52), dp(52)));
        }
        return toolCol;
    }

    private LinearLayout buildBottom() {
        LinearLayout bottom = new LinearLayout(this);
        bottom.setOrientation(LinearLayout.VERTICAL);
        bottom.setBackgroundColor(PANEL);

        panelBox = new LinearLayout(this);
        FrameLayout frame = new FrameLayout(this);
        frame.addView(buildOutlinerPanel(), new FrameLayout.LayoutParams(MATCH, MATCH));
        frame.addView(buildPropsPanel(), new FrameLayout.LayoutParams(MATCH, MATCH));
        frame.addView(buildTexPanel(), new FrameLayout.LayoutParams(MATCH, MATCH));
        frame.addView(buildUvPanel(), new FrameLayout.LayoutParams(MATCH, MATCH));
        FrameLayout.LayoutParams fp = null;
        bottom.addView(frame, new LinearLayout.LayoutParams(MATCH, dp(250)));
        panelHolder = frame;

        coordBar = new LinearLayout(this);
        coordBar.setOrientation(LinearLayout.HORIZONTAL);
        coordBar.setGravity(Gravity.CENTER_VERTICAL);
        coordBar.setBackgroundColor(BAR);
        coordBar.setPadding(dp(8), dp(3), dp(8), dp(3));
        coordLabel = label("Posisi", 12, DIM);
        coordBar.addView(coordLabel, new LinearLayout.LayoutParams(dp(54), WRAP));
        for (int k = 0; k < 3; k++) {
            final int kk = k;
            EditText e = scrubField(0.01f, 2, new NumTarget() {
                @Override public float get() { return transformGet(quickBase() + kk); }
                @Override public void set(float v) { transformSet(quickBase() + kk, v); }
            }, true);
            coordBar.addView(e, margin(0, WRAP, 1f, 2));
        }
        bottom.addView(coordBar, new LinearLayout.LayoutParams(MATCH, WRAP));

        LinearLayout tabs = new LinearLayout(this);
        tabs.setOrientation(LinearLayout.HORIZONTAL);
        tabs.setBackgroundColor(TOP);
        int[] kinds = {I_3D, I_LIST, I_CUBE, I_IMAGE, I_UV};
        for (int i = 0; i < 5; i++) {
            final int idx = i;
            tabIcons[i] = icon(kinds[i], v -> pickTab(idx));
            tabs.addView(tabIcons[i], new LinearLayout.LayoutParams(0, dp(52), 1f));
        }
        bottom.addView(tabs, new LinearLayout.LayoutParams(MATCH, WRAP));
        return bottom;
    }

    private FrameLayout panelHolder;

    private View buildOutlinerPanel() {
        outlinerPanel = new LinearLayout(this);
        outlinerPanel.setOrientation(LinearLayout.VERTICAL);
        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(4), 0, dp(8), 0);
        IconView plus = icon(I_PLUS, null);
        plus.setOnClickListener(v -> {
            PopupMenu m = new PopupMenu(this, v);
            m.getMenu().add(0, 0, 0, "Mesh…");
            m.getMenu().add(0, 1, 1, "Kubus");
            m.getMenu().add(0, 2, 2, "Bidang");
            m.getMenu().add(0, 3, 3, "Grup");
            m.setOnMenuItemClickListener(i -> {
                switch (i.getItemId()) {
                    case 0: showAddMesh(); break;
                    case 1: NativeLib.addObj(0); break;
                    case 2: NativeLib.addObj(1); break;
                    default: NativeLib.groupOp(0, 0, 0);
                }
                sync();
                return true;
            });
            m.show();
        });
        bar.addView(plus, new LinearLayout.LayoutParams(dp(44), dp(44)));
        bar.addView(icon(I_FOLDER_PLUS, v -> { NativeLib.groupOp(0, 0, 0); sync(); }), new LinearLayout.LayoutParams(dp(44), dp(44)));
        bar.addView(new View(this), new LinearLayout.LayoutParams(0, 1, 1f));
        outCount = label("", 13, DIM);
        bar.addView(outCount);
        ((LinearLayout) outlinerPanel).addView(bar, new LinearLayout.LayoutParams(MATCH, WRAP));
        ScrollView sv = new ScrollView(this);
        outlinerList = new LinearLayout(this);
        outlinerList.setOrientation(LinearLayout.VERTICAL);
        sv.addView(outlinerList, new ViewGroup.LayoutParams(MATCH, WRAP));
        ((LinearLayout) outlinerPanel).addView(sv, new LinearLayout.LayoutParams(MATCH, 0, 1f));
        return outlinerPanel;
    }

    private View buildPropsPanel() {
        propsScroll = new ScrollView(this);
        propsBox = new LinearLayout(this);
        propsBox.setOrientation(LinearLayout.VERTICAL);
        propsBox.setPadding(dp(8), dp(4), dp(8), dp(8));
        propsScroll.addView(propsBox, new ViewGroup.LayoutParams(MATCH, WRAP));

        LinearLayout head = new LinearLayout(this);
        head.setOrientation(LinearLayout.HORIZONTAL);
        head.addView(new View(this), new LinearLayout.LayoutParams(dp(56), 1));
        for (String s : new String[]{"X", "Y", "Z"}) {
            TextView t = label(s, 12, DIM);
            t.setGravity(Gravity.CENTER);
            head.addView(t, new LinearLayout.LayoutParams(0, WRAP, 1f));
        }
        propsBox.addView(head);
        propsBox.addView(propRow("Posisi", 0));
        propsBox.addView(propRow("Rotasi", 3));
        propsBox.addView(propRow("Ukuran", 6));
        propsBox.addView(label("Tip: geser angka ke kiri/kanan untuk mengubah, tap untuk mengetik. Tekan lama objek di Outliner untuk menu (salin, tempel, grup, warna, dll).", 11, DIM));
        return propsScroll;
    }

    private LinearLayout propRow(String name, int start) {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        r.addView(label(name, 12, DIM), new LinearLayout.LayoutParams(dp(56), WRAP));
        for (int k = 0; k < 3; k++) {
            final int idx = start + k;
            EditText e = scrubField((idx >= 3 && idx < 6) ? 0.5f : 0.01f, 2, new NumTarget() {
                @Override public float get() { return transformGet(idx); }
                @Override public void set(float v) { transformSet(idx, v); }
            });
            r.addView(e, margin(0, WRAP, 1f, 2));
        }
        return r;
    }

    private View buildTexPanel() {
        LinearLayout p = new LinearLayout(this);
        p.setOrientation(LinearLayout.VERTICAL);
        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(4), 0, dp(8), 0);
        bar.addView(icon(I_PLUS, v -> showNewTexture()), new LinearLayout.LayoutParams(dp(44), dp(44)));
        bar.addView(icon(I_IMAGE, v -> pickImage()), new LinearLayout.LayoutParams(dp(44), dp(44)));
        bar.addView(label("Tap = aktif, tekan lama = menu", 11, DIM));
        p.addView(bar, new LinearLayout.LayoutParams(MATCH, WRAP));
        ScrollView sv = new ScrollView(this);
        texList = new LinearLayout(this);
        texList.setOrientation(LinearLayout.VERTICAL);
        sv.addView(texList, new ViewGroup.LayoutParams(MATCH, WRAP));
        p.addView(sv, new LinearLayout.LayoutParams(MATCH, 0, 1f));
        texPanel = p;
        return p;
    }

    private View buildUvPanel() {
        LinearLayout p = new LinearLayout(this);
        p.setOrientation(LinearLayout.VERTICAL);
        HorizontalScrollView hs = new HorizontalScrollView(this);
        hs.setHorizontalScrollBarEnabled(false);
        faceRow = new LinearLayout(this);
        faceRow.setOrientation(LinearLayout.HORIZONTAL);
        faceRow.setPadding(dp(4), dp(3), dp(4), dp(3));
        hs.addView(faceRow, new ViewGroup.LayoutParams(WRAP, WRAP));
        p.addView(hs, new LinearLayout.LayoutParams(MATCH, WRAP));

        uvView = new UvView(this);
        uvView.listener = new UvListener() {
            @Override public void onSelect(int face) { uvFace = face; refreshUv(); }
            @Override public void onDragStart() { NativeLib.action(7); }
            @Override public void onChange(int face, float[] r) {
                int o = NativeLib.selected();
                if (o < 0) return;
                NativeLib.setFaceUV(o, face, r);
                uvView.rects[face] = r;
                uvView.invalidate();
                refreshFields();
            }
        };
        p.addView(uvView, new LinearLayout.LayoutParams(MATCH, 0, 1f));

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(4), dp(2), dp(4), dp(2));
        String[] ln = {"U0", "V0", "U1", "V1"};
        for (int k = 0; k < 4; k++) {
            final int kk = k;
            row.addView(label(ln[k], 11, DIM));
            EditText e = scrubField(0.25f, 1, new NumTarget() {
                @Override public float get() { return uvGet(kk); }
                @Override public void set(float v) { uvSet(kk, v); }
            });
            row.addView(e, margin(0, WRAP, 1f, 1));
        }
        Button auto = chip("Auto", v -> {
            int o = NativeLib.selected();
            if (o >= 0) { NativeLib.autoUV(o); sync(); }
        });
        row.addView(auto, margin(WRAP, WRAP, 0f, 2));
        p.addView(row, new LinearLayout.LayoutParams(MATCH, WRAP));
        uvPanel = p;
        return p;
    }

    // ---------- helper UI ----------
    private int dp(int v) { return (int) (v * getResources().getDisplayMetrics().density + 0.5f); }

    private LinearLayout.LayoutParams margin(int w, int h, float wt, int m) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(w, h, wt);
        p.setMargins(dp(m), dp(m), dp(m), dp(m));
        return p;
    }

    private Button chip(String text, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setTextSize(11);
        b.setAllCaps(false);
        b.setTextColor(TXT);
        b.setBackgroundColor(BTN);
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setMinHeight(0);
        b.setMinimumHeight(0);
        b.setPadding(dp(8), dp(7), dp(8), dp(7));
        b.setOnClickListener(l);
        return b;
    }

    private TextView label(String s, int size, int color) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(size);
        t.setTextColor(color);
        t.setPadding(dp(2), dp(4), dp(2), dp(2));
        return t;
    }

    private void active(Button b, boolean on) {
        b.setBackgroundColor(on ? ACC : BTN);
        b.setTextColor(on ? ONACC : TXT);
    }

    private void hideKeyboard(View v) {
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
    }

    private static String fmt(float v, String f) { return String.format(Locale.US, f, v); }

    private EditText scrubField(float step, int decimals, NumTarget t) { return scrubField(step, decimals, t, false); }

    /** Field angka: tap untuk mengetik, geser kiri/kanan untuk mengubah nilai. */
    private EditText scrubField(final float step, final int decimals, final NumTarget t, final boolean quick) {
        final EditText e = new EditText(this);
        final String f = "%." + decimals + "f";
        e.setSingleLine(true);
        e.setTextSize(13);
        e.setTextColor(TXT);
        e.setBackgroundColor(FIELD);
        e.setGravity(Gravity.CENTER);
        e.setPadding(dp(4), dp(8), dp(4), dp(8));
        e.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL | InputType.TYPE_NUMBER_FLAG_SIGNED);
        e.setImeOptions(EditorInfo.IME_ACTION_DONE);
        final Runnable commit = () -> {
            String txt = e.getText().toString().trim();
            try {
                float v = Float.parseFloat(txt);
                if (!txt.equals(fmt(t.get(), f))) { NativeLib.action(7); t.set(v); }
            } catch (NumberFormatException ignored) { }
        };
        e.setOnEditorActionListener((tv, actionId, ev) -> {
            commit.run();
            rootView.requestFocus();
            hideKeyboard(tv);
            sync();
            return true;
        });
        e.setOnFocusChangeListener((v, has) -> { if (!has) { commit.run(); } });
        final float slop = dp(8);
        final float[] st = new float[3];  // downX, downY, nilai awal
        final boolean[] scrubbing = {false};
        e.setOnTouchListener((v, ev) -> {
            switch (ev.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    st[0] = ev.getRawX(); st[1] = ev.getRawY();
                    scrubbing[0] = false;
                    st[2] = t.get();
                    return false;
                case MotionEvent.ACTION_MOVE: {
                    if (v.hasFocus() && !scrubbing[0]) return false;  // sedang mengetik: biarkan kursor
                    float dx = ev.getRawX() - st[0];
                    if (!scrubbing[0]) {
                        if (Math.abs(dx) < slop || Math.abs(dx) < Math.abs(ev.getRawY() - st[1])) return false;
                        scrubbing[0] = true;
                        v.getParent().requestDisallowInterceptTouchEvent(true);
                        NativeLib.action(7);
                    }
                    t.set(st[2] + dx * step);
                    e.setText(fmt(t.get(), f));
                    return true;
                }
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (scrubbing[0]) { scrubbing[0] = false; NativeLib.action(7); return true; }
                    return false;
                default:
                    return scrubbing[0];
            }
        });
        bindings.add(new Binding(e, t, f));
        return e;
    }

    // ---------- akses transform & UV ----------
    private float transformGet(int idx) {
        int sel = NativeLib.selected();
        if (sel < 0) return 0f;
        float[] t = NativeLib.getTransform(sel);
        return (t != null && idx >= 0 && idx < t.length) ? t[idx] : 0f;
    }

    private void transformSet(int idx, float v) {
        int sel = NativeLib.selected();
        if (sel < 0) return;
        float[] t = NativeLib.getTransform(sel);
        if (t == null || t.length < 9) return;
        t[idx] = v;
        NativeLib.setTransform(sel, t);
    }

    private int quickBase() {
        int[] s = NativeLib.state();
        int tl = (s != null && s.length > 2) ? s[2] : 1;
        return tl == 3 ? 3 : tl == 2 ? 6 : 0;
    }

    private float[] curFaceUv() {
        int o = NativeLib.selected();
        if (o < 0 || uvFace < 0 || uvFace >= NativeLib.faceCount(o)) return null;
        return NativeLib.getFaceUV(o, uvFace);
    }

    private int[] curTexSize() {
        int o = NativeLib.selected();
        if (o < 0) return null;
        int t = NativeLib.objTex(o);
        if (t < 0) return null;
        return NativeLib.texInfo(t);
    }

    private float uvGet(int k) {
        float[] r = curFaceUv();
        int[] sz = curTexSize();
        if (r == null || sz == null || r.length < 4) return 0f;
        return r[k] * ((k % 2 == 0) ? sz[0] : sz[1]);
    }

    private void uvSet(int k, float px) {
        int o = NativeLib.selected();
        float[] r = curFaceUv();
        int[] sz = curTexSize();
        if (o < 0 || r == null || sz == null || r.length < 4) return;
        r[k] = px / ((k % 2 == 0) ? sz[0] : sz[1]);
        NativeLib.setFaceUV(o, uvFace, r);
    }

    // ---------- sinkronisasi dengan mesin ----------
    private void sync() {
        lastVer = NativeLib.version();
        refreshModeUi();
        refreshVisiblePanel();
        refreshFields();
    }

    private void refreshVisiblePanel() {
        if (collapsed) return;
        if (tab == TAB_OUT) refreshOutliner();
        else if (tab == TAB_TEX) refreshTextures();
        else if (tab == TAB_UV) refreshUv();
    }

    private void pickTab(int i) {
        if (i == 0 || (i == tab && !collapsed)) collapsed = true;
        else { tab = i; collapsed = false; }
        applyTabs();
    }

    private void applyTabs() {
        panelHolder.setVisibility(collapsed ? View.GONE : View.VISIBLE);
        outlinerPanel.setVisibility(tab == TAB_OUT ? View.VISIBLE : View.GONE);
        propsScroll.setVisibility(tab == TAB_PROPS ? View.VISIBLE : View.GONE);
        texPanel.setVisibility(tab == TAB_TEX ? View.VISIBLE : View.GONE);
        uvPanel.setVisibility(tab == TAB_UV ? View.VISIBLE : View.GONE);
        tabIcons[0].setActive(collapsed);
        for (int i = 1; i < 5; i++) tabIcons[i].setActive(!collapsed && tab == i);
        refreshVisiblePanel();
    }

    private void refreshModeUi() {
        int[] s = NativeLib.state();
        if (s == null || s.length < 7) return;
        int mode = s[0];
        btnUndo.setEnabled(s[5] == 1); btnUndo.setAlpha(s[5] == 1 ? 1f : 0.35f);
        btnRedo.setEnabled(s[6] == 1); btnRedo.setAlpha(s[6] == 1 ? 1f : 0.35f);
        btnMode.setKind(mode == 2 ? I_BRUSH : I_CUBE);
        for (int i = 0; i < 4; i++) { toolIcons[i].setActive(s[2] == i); axisBtns[i].setEnabled(true); active(axisBtns[i], s[3] == i); }
        active(btnVtx, s[1] == 0);
        active(btnFace, s[1] == 1);
        axisGroup.setVisibility(mode == 2 ? View.GONE : View.VISIBLE);
        editGroup.setVisibility(mode == 1 ? View.VISIBLE : View.GONE);
        View common = bar2Inner.findViewWithTag("common");
        if (common != null) common.setVisibility(mode == 2 ? View.GONE : View.VISIBLE);
        paintGroup.setVisibility(mode == 2 ? View.VISIBLE : View.GONE);
        toolCol.setVisibility(mode == 2 ? View.GONE : View.VISIBLE);
        if (s.length >= 12) {
            for (int i = 0; i < 4; i++) paintIcons[i].setActive(s[8] == i);
            swatch.setBackgroundColor(s[10]);
        }
        coordBar.setVisibility(mode != 2 && s[4] >= 0 ? View.VISIBLE : View.GONE);
        int base = s[2] == 3 ? 3 : s[2] == 2 ? 6 : 0;
        coordLabel.setText(base == 3 ? "Rotasi" : base == 6 ? "Ukuran" : "Posisi");
    }

    private void refreshFields() {
        for (Binding b : bindings) {
            if (b.e.hasFocus()) continue;  // jangan timpa saat user mengetik
            b.e.setText(fmt(b.t.get(), b.fmt));
        }
    }

    private void refreshOutliner() {
        outlinerList.removeAllViews();
        int n = NativeLib.objCount(), sel = NativeLib.selected(), gc = NativeLib.groupCount();
        outCount.setText((sel >= 0 ? 1 : 0) + " / " + n);
        if (n == 0 && gc == 0) {
            outlinerList.addView(label("Kosong. Tekan + untuk menambah.", 13, DIM));
            return;
        }
        String q = filter.trim().toLowerCase(Locale.US);
        if (!q.isEmpty()) {
            outlinerList.addView(label("Filter: \"" + filter + "\" (ikon cari untuk mengubah)", 11, DIM));
            for (int i = 0; i < n; i++)
                if (NativeLib.objName(i).toLowerCase(Locale.US).contains(q)) outlinerList.addView(objRow(i, 0, sel));
            return;
        }
        for (int g = 0; g < gc; g++) {
            outlinerList.addView(groupRow(g));
            int[] gi = NativeLib.groupInfo(g);
            if (gi != null && gi.length >= 2 && gi[1] == 1) continue;
            for (int i = 0; i < n; i++) if (NativeLib.objGroup(i) == g) outlinerList.addView(objRow(i, 1, sel));
        }
        for (int i = 0; i < n; i++) if (NativeLib.objGroup(i) < 0) outlinerList.addView(objRow(i, 0, sel));
    }

    private View groupRow(final int g) {
        int[] gi = NativeLib.groupInfo(g);
        final boolean vis = gi == null || gi.length < 2 || gi[0] == 1;
        boolean col = gi != null && gi.length >= 2 && gi[1] == 1;
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        r.setBackgroundColor(PANEL);
        r.setPadding(dp(4), dp(1), dp(4), dp(1));
        r.addView(icon(col ? I_CHEV_RIGHT : I_CHEV_DOWN, v -> { NativeLib.groupOp(2, g, 0); sync(); }), new LinearLayout.LayoutParams(dp(34), dp(34)));
        IconView fi = new IconView(this, I_FOLDER, 0xFF9AD8F0);
        r.addView(fi, new LinearLayout.LayoutParams(dp(30), dp(30)));
        TextView name = label(NativeLib.groupName(g), 14, vis ? TXT : DIM);
        name.setPadding(dp(6), dp(10), dp(2), dp(10));
        name.setOnClickListener(v -> { NativeLib.groupOp(2, g, 0); sync(); });
        name.setOnLongClickListener(v -> { groupMenu(g); return true; });
        r.addView(name, new LinearLayout.LayoutParams(0, WRAP, 1f));
        r.addView(icon(vis ? I_EYE : I_EYE_OFF, v -> { NativeLib.groupOp(1, g, 0); sync(); }), new LinearLayout.LayoutParams(dp(38), dp(38)));
        return r;
    }

    private View objRow(final int i, int indent, int sel) {
        final boolean vis = NativeLib.objVisible(i);
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        r.setBackgroundColor(i == sel ? ROWSEL : PANEL);
        r.setPadding(dp(10 + indent * 22), dp(1), dp(4), dp(1));
        IconView ci = new IconView(this, I_CUBE, i == sel ? ACC : DIM);
        r.addView(ci, new LinearLayout.LayoutParams(dp(28), dp(28)));
        TextView name = label(NativeLib.objName(i), 14, vis ? TXT : DIM);
        name.setPadding(dp(6), dp(10), dp(2), dp(10));
        name.setOnClickListener(v -> { NativeLib.selectObj(i); sync(); });
        name.setOnLongClickListener(v -> { contextMenu(i); return true; });
        r.addView(name, new LinearLayout.LayoutParams(0, WRAP, 1f));
        r.addView(icon(vis ? I_EYE : I_EYE_OFF, v -> { NativeLib.setVisible(i, !vis); sync(); }), new LinearLayout.LayoutParams(dp(38), dp(38)));
        return r;
    }

    private void refreshTextures() {
        texList.removeAllViews();
        int n = NativeLib.texCount();
        int[] st = NativeLib.state();
        int act = (st != null && st.length > 11) ? st[11] : -1;
        if (n == 0) {
            texList.addView(label("Belum ada tekstur. Tekan + untuk membuat, atau ikon gambar untuk impor.", 13, DIM));
            return;
        }
        for (int i = 0; i < n; i++) {
            final int idx = i;
            int[] sz = NativeLib.texInfo(i);
            LinearLayout r = new LinearLayout(this);
            r.setOrientation(LinearLayout.HORIZONTAL);
            r.setGravity(Gravity.CENTER_VERTICAL);
            r.setBackgroundColor(i == act ? ROWSEL : PANEL);
            r.setPadding(dp(8), dp(4), dp(8), dp(4));
            ImageView iv = new ImageView(this);
            iv.setBackgroundColor(FIELD);
            iv.setScaleType(ImageView.ScaleType.FIT_CENTER);
            Bitmap bm = texBitmap(i);
            if (bm != null) iv.setImageBitmap(bm);
            r.addView(iv, new LinearLayout.LayoutParams(dp(44), dp(44)));
            LinearLayout col = new LinearLayout(this);
            col.setOrientation(LinearLayout.VERTICAL);
            col.addView(label(NativeLib.texName(i) + (i == act ? "  (aktif)" : ""), 14, TXT));
            col.addView(label(sz[0] + " x " + sz[1], 11, DIM));
            LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(0, WRAP, 1f);
            cp.setMargins(dp(10), 0, 0, 0);
            r.addView(col, cp);
            r.setOnClickListener(v -> { NativeLib.set(7, idx); sync(); });
            r.setOnLongClickListener(v -> { textureMenu(idx); return true; });
            texList.addView(r, new LinearLayout.LayoutParams(MATCH, WRAP));
        }
    }

    private Bitmap texBitmap(int i) {
        int[] sz = NativeLib.texInfo(i);
        int[] px = NativeLib.texPixels(i);
        if (sz == null || sz.length < 2 || sz[0] < 1 || sz[1] < 1 || px == null || px.length != sz[0] * sz[1]) return null;
        return Bitmap.createBitmap(px, sz[0], sz[1], Bitmap.Config.ARGB_8888);
    }

    private void refreshUv() {
        int o = NativeLib.selected();
        if (o != uvObj) { uvObj = o; uvFace = 0; uvChipsKey = -1; }
        int nf = o >= 0 ? NativeLib.faceCount(o) : 0;
        if (uvFace >= nf) uvFace = 0;
        int key = o * 100003 + nf;
        if (key != uvChipsKey) {
            uvChipsKey = key;
            faceRow.removeAllViews();
            faceChips.clear();
            for (int i = 0; i < nf; i++) {
                final int idx = i;
                Button b = chip(NativeLib.faceName(o, i), v -> { uvFace = idx; refreshUv(); });
                faceChips.add(b);
                faceRow.addView(b, margin(WRAP, WRAP, 0f, 2));
            }
        }
        for (int i = 0; i < faceChips.size(); i++) active(faceChips.get(i), i == uvFace);
        int t = o >= 0 ? NativeLib.objTex(o) : -1;
        uvView.bmp = t >= 0 ? texBitmap(t) : null;
        float[][] rects = new float[nf][];
        for (int i = 0; i < nf; i++) rects[i] = NativeLib.getFaceUV(o, i);
        uvView.rects = rects;
        uvView.sel = nf > 0 ? uvFace : -1;
        uvView.invalidate();
    }

    // ---------- dialog & menu ----------
    private void showMainMenu() {
        final String[] cats = {"Berkas", "Edit", "Tampilan", "Bantuan"};
        new AlertDialog.Builder(this).setItems(cats, (d, w) -> {
            switch (w) {
                case 0: fileMenu(); break;
                case 1: editMenu(); break;
                case 2: viewMenu(); break;
                default:
                    new AlertDialog.Builder(this).setTitle("Bantuan")
                            .setMessage("• Tekan lama objek di Outliner: salin, tempel, duplikat, grup, warna, tekstur.\n"
                                    + "• Geser angka di kolom koordinat/Properti untuk mengubah nilai.\n"
                                    + "• Mode Paint (ikon kanan atas): 1 jari melukis, 2 jari zoom/putar.\n"
                                    + "• Tab UV: tap sisi lalu geser kotaknya; tarik sudut kanan-bawah untuk mengubah ukuran.")
                            .setPositiveButton("OK", null).show();
            }
        }).show();
    }

    private void fileMenu() {
        final String[] it = {"Simpan OBJ (+ tekstur PNG)", "Impor gambar sebagai tekstur…", "Muat .so baru…", "Reset .so ke bawaan", "Info .so"};
        new AlertDialog.Builder(this).setTitle("Berkas").setItems(it, (d, w) -> {
            switch (w) {
                case 0: export(); break;
                case 1: pickImage(); break;
                case 2: pickSo(); break;
                case 3:
                    Toast.makeText(this, NativeLib.resetOverride(this)
                            ? "Dihapus. Tutup lalu buka ulang aplikasi."
                            : "Tidak ada .so pengganti", Toast.LENGTH_LONG).show();
                    break;
                default:
                    Toast.makeText(this, "Sedang memakai: " + (NativeLib.usingOverride() ? ".so pengganti" : ".so bawaan APK")
                            + (NativeLib.hasOverride(this) ? " (ada file pengganti tersimpan)" : ""), Toast.LENGTH_LONG).show();
            }
        }).show();
    }

    private void editMenu() {
        final String[] it = {"Undo", "Redo", "Salin", "Tempel", "Duplikat", "Hapus pilihan"};
        new AlertDialog.Builder(this).setTitle("Edit").setItems(it, (d, w) -> {
            int[] codes = {2, 3, 5, 6, 4, 1};
            NativeLib.action(codes[w]);
            sync();
        }).show();
    }

    private void viewMenu() {
        final String[] it = {"Reset kamera", "Hapus filter outliner"};
        new AlertDialog.Builder(this).setTitle("Tampilan").setItems(it, (d, w) -> {
            if (w == 0) NativeLib.action(20); else filter = "";
            sync();
        }).show();
    }

    private void showSearch() {
        final EditText e = new EditText(this);
        e.setSingleLine(true);
        e.setText(filter);
        e.setSelectAllOnFocus(true);
        new AlertDialog.Builder(this).setTitle("Cari objek")
                .setView(e)
                .setPositiveButton("Cari", (d, w) -> {
                    filter = e.getText().toString();
                    if (collapsed || tab != TAB_OUT) { collapsed = false; tab = TAB_OUT; applyTabs(); }
                    sync();
                })
                .setNeutralButton("Hapus filter", (d, w) -> { filter = ""; sync(); })
                .setNegativeButton("Batal", null)
                .show();
    }

    private void showAddMesh() {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(20), dp(8), dp(20), dp(4));
        final Spinner sp = new Spinner(this);
        sp.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, SHAPES));
        box.addView(sp, new LinearLayout.LayoutParams(MATCH, WRAP));
        final EditText dEt = new EditText(this), hEt = new EditText(this);
        for (EditText e : new EditText[]{dEt, hEt}) {
            e.setSingleLine(true);
            e.setText("1");
            e.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
        }
        TextView dl = new TextView(this), hl = new TextView(this);
        dl.setText("Diameter");
        hl.setText("Tinggi (kubus, piramida, silinder, tabung, kerucut)");
        box.addView(dl);
        box.addView(dEt, new LinearLayout.LayoutParams(MATCH, WRAP));
        box.addView(hl);
        box.addView(hEt, new LinearLayout.LayoutParams(MATCH, WRAP));
        new AlertDialog.Builder(this).setTitle("Tambah Mesh").setView(box)
                .setPositiveButton("Tambah", (d, w) -> {
                    float dia = 1f, hh = 1f;
                    try { dia = Float.parseFloat(dEt.getText().toString().trim()); } catch (NumberFormatException ignored) { }
                    try { hh = Float.parseFloat(hEt.getText().toString().trim()); } catch (NumberFormatException ignored) { }
                    NativeLib.addMesh(sp.getSelectedItemPosition(), Math.max(0.05f, dia), Math.max(0.05f, hh));
                    sync();
                })
                .setNegativeButton("Batal", null).show();
    }

    private void contextMenu(final int i) {
        NativeLib.selectObj(i);
        final boolean vis = NativeLib.objVisible(i);
        int[] st = NativeLib.state();
        final boolean canPaste = st != null && st.length >= 8 && st[7] == 1;
        final String[] items = {"Salin", canPaste ? "Tempel" : "Tempel (kosong)", "Duplikat", "Grupkan (Group Elements)",
                "Pindah ke grup…", "Tekstur…", "Ganti nama", "Warna…", vis ? "Sembunyikan" : "Tampilkan", "Hapus"};
        new AlertDialog.Builder(this)
                .setTitle(NativeLib.objName(i))
                .setItems(items, (d, w) -> {
                    switch (w) {
                        case 0: NativeLib.action(5); break;
                        case 1: NativeLib.action(6); break;
                        case 2: NativeLib.action(4); break;
                        case 3: NativeLib.groupOp(6, i, 0); break;
                        case 4: moveToGroup(i); break;
                        case 5: pickObjTexture(i); break;
                        case 6: rename(i); break;
                        case 7: pickColor(); break;
                        case 8: NativeLib.setVisible(i, !vis); break;
                        default: NativeLib.deleteObj(i);
                    }
                    sync();
                })
                .show();
    }

    private void moveToGroup(final int obj) {
        int gc = NativeLib.groupCount();
        final String[] names = new String[gc + 1];
        names[0] = "(tanpa grup)";
        for (int g = 0; g < gc; g++) names[g + 1] = NativeLib.groupName(g);
        new AlertDialog.Builder(this).setTitle("Pindah ke grup")
                .setItems(names, (d, w) -> { NativeLib.groupOp(3, obj, w - 1); sync(); }).show();
    }

    private void groupMenu(final int g) {
        int[] gi = NativeLib.groupInfo(g);
        final boolean vis = gi == null || gi.length < 2 || gi[0] == 1;
        final String[] it = {"Ganti nama", vis ? "Sembunyikan" : "Tampilkan", "Bubarkan grup (isi tetap ada)", "Hapus grup + isinya"};
        new AlertDialog.Builder(this).setTitle(NativeLib.groupName(g)).setItems(it, (d, w) -> {
            switch (w) {
                case 0: renameGroup(g); break;
                case 1: NativeLib.groupOp(1, g, 0); break;
                case 2: NativeLib.groupOp(4, g, 0); break;
                default: NativeLib.groupOp(5, g, 0);
            }
            sync();
        }).show();
    }

    private void renameGroup(final int g) {
        final EditText e = new EditText(this);
        e.setText(NativeLib.groupName(g));
        e.setSelectAllOnFocus(true);
        new AlertDialog.Builder(this).setTitle("Ganti nama grup").setView(e)
                .setPositiveButton("OK", (d, w) -> { NativeLib.renameGroup(g, e.getText().toString()); sync(); })
                .setNegativeButton("Batal", null).show();
    }

    private void pickColor() {
        final String[] names = {"Abu-abu (bawaan)", "Merah", "Oranye", "Kuning", "Hijau", "Biru", "Ungu", "Pink"};
        new AlertDialog.Builder(this)
                .setTitle("Warna objek")
                .setItems(names, (d, w) -> { NativeLib.action(10 + w); sync(); })
                .show();
    }

    private void pickObjTexture(final int obj) {
        int n = NativeLib.texCount();
        final String[] names = new String[n + 1];
        names[0] = "(tanpa tekstur)";
        for (int t = 0; t < n; t++) names[t + 1] = NativeLib.texName(t);
        new AlertDialog.Builder(this).setTitle("Tekstur objek")
                .setItems(names, (d, w) -> { NativeLib.setObjTex(obj, w - 1); sync(); }).show();
    }

    private void textureMenu(final int t) {
        final String[] it = {"Jadikan aktif", "Terapkan ke objek terpilih", "Ekspor PNG", "Hapus"};
        new AlertDialog.Builder(this).setTitle(NativeLib.texName(t)).setItems(it, (d, w) -> {
            switch (w) {
                case 0: NativeLib.set(7, t); break;
                case 1: {
                    int o = NativeLib.selected();
                    if (o >= 0) NativeLib.setObjTex(o, t);
                    else Toast.makeText(this, "Pilih objek dulu", Toast.LENGTH_SHORT).show();
                    break;
                }
                case 2: {
                    String p = exportTexture(t);
                    Toast.makeText(this, p != null ? "Tersimpan: " + p : "Gagal menyimpan PNG", Toast.LENGTH_LONG).show();
                    break;
                }
                default: NativeLib.texDelete(t);
            }
            sync();
        }).show();
    }

    private void showNewTexture() {
        final int[] sizes = {16, 32, 64, 128, 256};
        String[] names = new String[sizes.length];
        for (int i = 0; i < sizes.length; i++) names[i] = sizes[i] + " x " + sizes[i];
        new AlertDialog.Builder(this).setTitle("Tekstur baru (abu-abu)").setItems(names, (d, w) -> {
            int r = NativeLib.texCreate("texture " + (++texCounter), sizes[w], sizes[w], 0xFFCCCCCC);
            if (r < 0) Toast.makeText(this, "Gagal: maksimal 8 tekstur", Toast.LENGTH_LONG).show();
            sync();
        }).show();
    }

    private void pickBrushColor() {
        final int[] pal = {0xFF000000, 0xFF7F7F7F, 0xFFFFFFFF, 0xFFE85050, 0xFFF5A030, 0xFFF5E050, 0xFF60C060, 0xFF40C0C0,
                0xFF4080F0, 0xFF8050D0, 0xFFE060B0, 0xFF8B5A2B, 0xFFB0B0B0, 0xFF404040, 0xFF9BE8A0, 0xFFA0D8FF};
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(16), dp(8), dp(16), dp(4));
        final AlertDialog[] ref = new AlertDialog[1];
        for (int r = 0; r < 2; r++) {
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            for (int c = 0; c < 8; c++) {
                final int col = pal[r * 8 + c];
                View v = new View(this);
                v.setBackgroundColor(col);
                v.setOnClickListener(x -> {
                    NativeLib.set(6, col);
                    if (ref[0] != null) ref[0].dismiss();
                    sync();
                });
                row.addView(v, margin(0, dp(34), 1f, 2));
            }
            box.addView(row, new LinearLayout.LayoutParams(MATCH, WRAP));
        }
        final EditText hex = new EditText(this);
        hex.setSingleLine(true);
        hex.setHint("Hex, mis. FF8800");
        box.addView(hex, new LinearLayout.LayoutParams(MATCH, WRAP));
        ref[0] = new AlertDialog.Builder(this).setTitle("Warna kuas").setView(box)
                .setPositiveButton("OK", (d, w) -> {
                    String s = hex.getText().toString().trim();
                    if (s.isEmpty()) return;
                    try {
                        int c = Color.parseColor(s.startsWith("#") ? s : "#" + s);
                        NativeLib.set(6, 0xFF000000 | (c & 0xFFFFFF));
                        sync();
                    } catch (IllegalArgumentException ex) {
                        Toast.makeText(this, "Hex tidak valid", Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton("Batal", null).create();
        ref[0].show();
    }

    private void rename(final int i) {
        final EditText e = new EditText(this);
        e.setText(NativeLib.objName(i));
        e.setSelectAllOnFocus(true);
        new AlertDialog.Builder(this)
                .setTitle("Ganti nama")
                .setView(e)
                .setPositiveButton("OK", (d, w) -> { NativeLib.renameObj(i, e.getText().toString()); sync(); })
                .setNegativeButton("Batal", null)
                .show();
    }

    // ---------- berkas ----------
    private static final int REQ_SO = 77, REQ_IMG = 78;

    private void pickSo() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, REQ_SO);
    }

    private void pickImage() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("image/*");
        startActivityForResult(i, REQ_IMG);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (res != RESULT_OK || data == null || data.getData() == null) return;
        if (req == REQ_SO) {
            try (InputStream in = getContentResolver().openInputStream(data.getData())) {
                String err = NativeLib.importSo(this, in);
                Toast.makeText(this, err == null
                        ? "Berhasil. Tutup aplikasi (geser dari recent) lalu buka lagi."
                        : "Ditolak: " + err, Toast.LENGTH_LONG).show();
            } catch (Exception e) {
                Toast.makeText(this, "Gagal membaca file: " + e.getMessage(), Toast.LENGTH_LONG).show();
            }
        } else if (req == REQ_IMG) {
            try (InputStream in = getContentResolver().openInputStream(data.getData())) {
                Bitmap bm = BitmapFactory.decodeStream(in);
                if (bm == null) { Toast.makeText(this, "Bukan gambar yang valid", Toast.LENGTH_LONG).show(); return; }
                int w = bm.getWidth(), h = bm.getHeight();
                if (w > 512 || h > 512) {
                    float sc = 512f / Math.max(w, h);
                    bm = Bitmap.createScaledBitmap(bm, Math.max(1, (int) (w * sc)), Math.max(1, (int) (h * sc)), false);
                    w = bm.getWidth();
                    h = bm.getHeight();
                }
                int[] px = new int[w * h];
                bm.getPixels(px, 0, w, 0, 0, w, h);
                int r = NativeLib.texSet("gambar " + (++texCounter), w, h, px);
                Toast.makeText(this, r >= 0 ? "Tekstur ditambahkan" : "Gagal: maksimal 8 tekstur", Toast.LENGTH_LONG).show();
                sync();
            } catch (Exception e) {
                Toast.makeText(this, "Gagal membaca gambar: " + e.getMessage(), Toast.LENGTH_LONG).show();
            }
        }
    }

    private String exportTexture(int t) {
        try {
            Bitmap bm = texBitmap(t);
            if (bm == null) return null;
            File f = new File(getExternalFilesDir(null), NativeLib.texName(t).replace(' ', '_') + ".png");
            try (FileOutputStream o = new FileOutputStream(f)) {
                bm.compress(Bitmap.CompressFormat.PNG, 100, o);
            }
            return f.getAbsolutePath();
        } catch (Exception e) {
            return null;
        }
    }

    private void export() {
        final String path = getExternalFilesDir(null) + "/model.obj";
        final boolean ok = NativeLib.exportObj(path);
        int saved = 0;
        if (ok) for (int t = 0; t < NativeLib.texCount(); t++) if (exportTexture(t) != null) saved++;
        Toast.makeText(this, ok ? "Tersimpan: " + path + (saved > 0 ? " (+" + saved + " tekstur PNG)" : "") : "Gagal menyimpan OBJ",
                Toast.LENGTH_LONG).show();
    }

    @Override protected void onPause() {
        super.onPause();
        handler.removeCallbacks(poll);
        gl.onPause();
    }

    @Override protected void onResume() {
        super.onResume();
        gl.onResume();
        sync();
        handler.post(poll);
    }
}
