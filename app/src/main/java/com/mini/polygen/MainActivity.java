package com.mini.polygen;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
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
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.InputStream;
import java.util.Locale;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity {
    private static final int BG = 0xFF1B1E23, PANEL = 0xFF262A31, BTN = 0xFF343942, ACC = 0xFF3E90FF,
            TXT = 0xFFE6E8EB, DIM = 0xFF8A9099, ROWSEL = 0xFF2F4F7F;
    private static final int MATCH = ViewGroup.LayoutParams.MATCH_PARENT, WRAP = ViewGroup.LayoutParams.WRAP_CONTENT;

    private GLSurfaceView gl;
    private View rootView;
    private LinearLayout outlinerList, propsBox, propsRows, editGroup;
    private ScrollView outlinerScroll, propsScroll;
    private FrameLayout tabContent;
    private Button btnUndo, btnRedo, tabOutliner, tabProps, modeObj, modeEdit, btnVtx, btnFace;
    private TextView propsTitle, propsHint;
    private final Button[] toolBtns = new Button[4], axisBtns = new Button[4];
    private final EditText[] fields = new EditText[9];
    private final String[] shown = new String[9];
    private int tab = 0;
    private boolean collapsed = false;
    private int lastVer = -1;
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

        FrameLayout vp = new FrameLayout(this);
        vp.addView(gl, new FrameLayout.LayoutParams(MATCH, MATCH));
        FrameLayout.LayoutParams tp = new FrameLayout.LayoutParams(dp(80), WRAP, Gravity.START | Gravity.TOP);
        tp.setMargins(dp(4), dp(4), 0, dp(4));
        vp.addView(buildToolColumn(), tp);
        root.addView(vp, new LinearLayout.LayoutParams(MATCH, 0, 1f));

        root.addView(buildBottomPanel(), new LinearLayout.LayoutParams(MATCH, WRAP));
        setContentView(root);
        pickTab(0);
        pickTab(0); // dua kali: buka lagi setelah toggle pertama
    }

    // ---------- bangun UI ----------
    private LinearLayout buildTopBar() {
        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setBackgroundColor(PANEL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        top.setPadding(dp(4), dp(4), dp(4), dp(4));

        Button file = chip("File \u25BE", v -> {
            PopupMenu m = new PopupMenu(this, v);
            m.getMenu().add(0, 0, 0, "Simpan OBJ");
            m.getMenu().add(0, 1, 1, "Muat .so baru\u2026");
            m.getMenu().add(0, 2, 2, "Reset .so ke bawaan");
            m.getMenu().add(0, 3, 3, "Info .so");
            m.setOnMenuItemClickListener(i -> {
                switch (i.getItemId()) {
                    case 0: export(); break;
                    case 1: pickSo(); break;
                    case 2:
                        Toast.makeText(this, NativeLib.resetOverride(this)
                                ? "Dihapus. Tutup lalu buka ulang aplikasi."
                                : "Tidak ada .so pengganti", Toast.LENGTH_LONG).show();
                        break;
                    default:
                        Toast.makeText(this, "Sedang memakai: " + (NativeLib.usingOverride() ? ".so pengganti" : ".so bawaan APK")
                                + (NativeLib.hasOverride(this) ? " (ada file pengganti tersimpan)" : ""),
                                Toast.LENGTH_LONG).show();
                }
                return true;
            });
            m.show();
        });
        Button add = chip("Tambah \u25BE", v -> {
            PopupMenu m = new PopupMenu(this, v);
            m.getMenu().add(0, 0, 0, "Kubus");
            m.getMenu().add(0, 1, 1, "Bidang");
            m.setOnMenuItemClickListener(i -> { NativeLib.addObj(i.getItemId()); sync(); return true; });
            m.show();
        });
        btnUndo = chip("\u21B6", v -> { NativeLib.action(2); sync(); });
        btnRedo = chip("\u21B7", v -> { NativeLib.action(3); sync(); });
        top.addView(file, margin(WRAP, WRAP, 0f, 2));
        top.addView(add, margin(WRAP, WRAP, 0f, 2));
        top.addView(btnUndo, margin(WRAP, WRAP, 0f, 2));
        top.addView(btnRedo, margin(WRAP, WRAP, 0f, 2));
        top.addView(new View(this), new LinearLayout.LayoutParams(0, 1, 1f));
        modeObj = chip("Objek", v -> { NativeLib.set(3, 0); sync(); });
        modeEdit = chip("Edit", v -> { NativeLib.set(3, 1); sync(); });
        top.addView(modeObj, margin(WRAP, WRAP, 0f, 2));
        top.addView(modeEdit, margin(WRAP, WRAP, 0f, 2));
        return top;
    }

    private View buildToolColumn() {
        ScrollView sv = new ScrollView(this);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setBackgroundColor(0x99262A31);
        col.setPadding(dp(2), dp(2), dp(2), dp(2));
        sv.addView(col);

        String[] tn = {"Kamera", "Geser", "Ukuran", "Putar"};
        for (int i = 0; i < 4; i++) {
            final int idx = i;
            toolBtns[i] = chip(tn[i], v -> { NativeLib.set(1, idx); sync(); });
            col.addView(toolBtns[i], margin(MATCH, WRAP, 0f, 2));
        }
        col.addView(label("Sumbu", 10, DIM));
        String[] an = {"X", "Y", "Z", "N"};
        for (int i = 0; i < 4; i++) {
            final int idx = i;
            axisBtns[i] = chip(an[i], v -> { NativeLib.set(2, idx); sync(); });
            col.addView(axisBtns[i], margin(MATCH, WRAP, 0f, 2));
        }
        editGroup = new LinearLayout(this);
        editGroup.setOrientation(LinearLayout.VERTICAL);
        editGroup.addView(label("Edit", 10, DIM));
        btnVtx = chip("Vertex", v -> { NativeLib.set(0, 0); sync(); });
        btnFace = chip("Face", v -> { NativeLib.set(0, 1); sync(); });
        Button ext = chip("Extrude", v -> { NativeLib.action(0); sync(); });
        editGroup.addView(btnVtx, margin(MATCH, WRAP, 0f, 2));
        editGroup.addView(btnFace, margin(MATCH, WRAP, 0f, 2));
        editGroup.addView(ext, margin(MATCH, WRAP, 0f, 2));
        col.addView(editGroup, new LinearLayout.LayoutParams(MATCH, WRAP));
        Button dup = chip("Duplikat", v -> { NativeLib.action(4); sync(); });
        col.addView(dup, margin(MATCH, WRAP, 0f, 2));
        Button del = chip("Hapus", v -> { NativeLib.action(1); sync(); });
        col.addView(del, margin(MATCH, WRAP, 0f, 2));
        return sv;
    }

    private LinearLayout buildBottomPanel() {
        LinearLayout bottom = new LinearLayout(this);
        bottom.setOrientation(LinearLayout.VERTICAL);
        bottom.setBackgroundColor(PANEL);

        LinearLayout tabs = new LinearLayout(this);
        tabs.setOrientation(LinearLayout.HORIZONTAL);
        tabs.setPadding(dp(4), dp(4), dp(4), dp(2));
        tabOutliner = chip("Outliner", v -> pickTab(0));
        tabProps = chip("Properti", v -> pickTab(1));
        tabs.addView(tabOutliner, margin(0, WRAP, 1f, 2));
        tabs.addView(tabProps, margin(0, WRAP, 1f, 2));
        bottom.addView(tabs, new LinearLayout.LayoutParams(MATCH, WRAP));

        tabContent = new FrameLayout(this);
        outlinerScroll = new ScrollView(this);
        outlinerList = new LinearLayout(this);
        outlinerList.setOrientation(LinearLayout.VERTICAL);
        outlinerScroll.addView(outlinerList, new ViewGroup.LayoutParams(MATCH, WRAP));

        propsScroll = new ScrollView(this);
        propsBox = new LinearLayout(this);
        propsBox.setOrientation(LinearLayout.VERTICAL);
        propsBox.setPadding(dp(8), dp(4), dp(8), dp(8));
        propsScroll.addView(propsBox, new ViewGroup.LayoutParams(MATCH, WRAP));
        buildProps();

        tabContent.addView(outlinerScroll, new FrameLayout.LayoutParams(MATCH, MATCH));
        tabContent.addView(propsScroll, new FrameLayout.LayoutParams(MATCH, MATCH));
        bottom.addView(tabContent, new LinearLayout.LayoutParams(MATCH, dp(190)));
        return bottom;
    }

    private void buildProps() {
        propsTitle = label("", 15, TXT);
        propsHint = label("Belum ada objek terpilih. Tap objek di viewport atau di outliner.", 13, DIM);
        propsRows = new LinearLayout(this);
        propsRows.setOrientation(LinearLayout.VERTICAL);

        LinearLayout head = new LinearLayout(this);
        head.setOrientation(LinearLayout.HORIZONTAL);
        head.addView(new View(this), new LinearLayout.LayoutParams(dp(56), 1));
        for (String s : new String[]{"X", "Y", "Z"}) {
            TextView t = label(s, 12, DIM);
            t.setGravity(Gravity.CENTER);
            head.addView(t, new LinearLayout.LayoutParams(0, WRAP, 1f));
        }
        propsRows.addView(head);
        propsRows.addView(propRow("Posisi", 0));
        propsRows.addView(propRow("Rotasi", 3));
        propsRows.addView(propRow("Ukuran", 6));
        propsRows.addView(label("Tip: geser angka ke kiri/kanan untuk mengecilkan/membesarkan, tap untuk mengetik.", 11, DIM));

        propsBox.addView(propsTitle);
        propsBox.addView(propsHint);
        propsBox.addView(propsRows);
    }

    private LinearLayout propRow(String name, int start) {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        r.addView(label(name, 12, DIM), new LinearLayout.LayoutParams(dp(56), WRAP));
        for (int k = 0; k < 3; k++) {
            EditText e = makeField(start + k);
            fields[start + k] = e;
            r.addView(e, margin(0, WRAP, 1f, 2));
        }
        return r;
    }

    private EditText makeField(final int idx) {
        EditText e = new EditText(this);
        e.setSingleLine(true);
        e.setTextSize(13);
        e.setTextColor(TXT);
        e.setBackgroundColor(BG);
        e.setPadding(dp(6), dp(6), dp(6), dp(6));
        e.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | InputType.TYPE_NUMBER_FLAG_SIGNED);
        e.setImeOptions(EditorInfo.IME_ACTION_DONE);
        e.setOnEditorActionListener((tv, actionId, ev) -> {
            applyProps();
            rootView.requestFocus();
            hideKeyboard(tv);
            return true;
        });
        e.setOnFocusChangeListener((v, has) -> { if (!has) applyProps(); });
        // geser kiri/kanan di angka = ubah nilai (tap biasa tetap membuka keyboard)
        final float slop = dp(8);
        final float step = (idx >= 3 && idx < 6) ? 0.5f : 0.01f;  // per piksel: rotasi 0.5 derajat, lainnya 0.01
        final float[] st = new float[3];  // downX, downY, startValue
        final boolean[] scrubbing = {false};
        e.setOnTouchListener((v, ev) -> {
            switch (ev.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    st[0] = ev.getRawX(); st[1] = ev.getRawY();
                    scrubbing[0] = false;
                    try { st[2] = Float.parseFloat(((EditText) v).getText().toString().trim()); }
                    catch (NumberFormatException x) { st[2] = 0f; }
                    return false;
                case MotionEvent.ACTION_MOVE:
                    if (v.hasFocus() && !scrubbing[0]) return false;  // sedang mengetik: biarkan kursor
                    float dx = ev.getRawX() - st[0];
                    if (!scrubbing[0]) {
                        if (Math.abs(dx) < slop || Math.abs(dx) < Math.abs(ev.getRawY() - st[1])) return false;
                        scrubbing[0] = true;
                        v.getParent().requestDisallowInterceptTouchEvent(true);
                        NativeLib.action(7);
                        v.cancelLongPress();
                    }
                    scrub(idx, st[2] + dx * step);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (scrubbing[0]) { scrubbing[0] = false; NativeLib.action(7); return true; }
                    return false;
                default:
                    return scrubbing[0];
            }
        });
        return e;
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

    private void active(Button b, boolean on) { b.setBackgroundColor(on ? ACC : BTN); }

    private void hideKeyboard(View v) {
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
    }

    // ---------- sinkronisasi dengan mesin ----------
    private void sync() {
        lastVer = NativeLib.version();
        refreshOutliner();
        refreshProps();
        refreshModeUi();
    }

    private void pickTab(int i) {
        if (i == tab && !collapsed) collapsed = true;
        else { tab = i; collapsed = false; }
        tabContent.setVisibility(collapsed ? View.GONE : View.VISIBLE);
        outlinerScroll.setVisibility(tab == 0 ? View.VISIBLE : View.GONE);
        propsScroll.setVisibility(tab == 1 ? View.VISIBLE : View.GONE);
        active(tabOutliner, tab == 0 && !collapsed);
        active(tabProps, tab == 1 && !collapsed);
    }

    private void refreshModeUi() {
        int[] s = NativeLib.state();
        if (s == null || s.length < 5) return;
        active(modeObj, s[0] == 0);
        active(modeEdit, s[0] == 1);
        for (int i = 0; i < 4; i++) { active(toolBtns[i], s[2] == i); active(axisBtns[i], s[3] == i); }
        active(btnVtx, s[1] == 0);
        active(btnFace, s[1] == 1);
        editGroup.setVisibility(s[0] == 1 ? View.VISIBLE : View.GONE);
        boolean cu = s.length >= 7 && s[5] == 1, cr = s.length >= 7 && s[6] == 1;
        btnUndo.setEnabled(cu); btnUndo.setAlpha(cu ? 1f : 0.35f);
        btnRedo.setEnabled(cr); btnRedo.setAlpha(cr ? 1f : 0.35f);
    }

    private void refreshOutliner() {
        outlinerList.removeAllViews();
        int n = NativeLib.objCount(), sel = NativeLib.selected();
        if (n == 0) {
            outlinerList.addView(label("Kosong. Pakai menu Tambah.", 13, DIM));
            return;
        }
        for (int i = 0; i < n; i++) {
            final int idx = i;
            final boolean vis = NativeLib.objVisible(i);
            LinearLayout r = new LinearLayout(this);
            r.setOrientation(LinearLayout.HORIZONTAL);
            r.setGravity(Gravity.CENTER_VERTICAL);
            r.setBackgroundColor(i == sel ? ROWSEL : PANEL);
            r.setPadding(dp(10), dp(2), dp(4), dp(2));

            TextView name = label(NativeLib.objName(i), 14, vis ? TXT : DIM);
            name.setPadding(dp(2), dp(10), dp(2), dp(10));
            name.setOnClickListener(v -> { NativeLib.selectObj(idx); sync(); });
            name.setOnLongClickListener(v -> { contextMenu(idx); return true; });
            r.addView(name, new LinearLayout.LayoutParams(0, WRAP, 1f));

            TextView eye = label(vis ? "\u25C9" : "\u25CB", 18, vis ? TXT : DIM);
            eye.setPadding(dp(12), dp(8), dp(12), dp(8));
            eye.setOnClickListener(v -> { NativeLib.setVisible(idx, !vis); sync(); });
            r.addView(eye, new LinearLayout.LayoutParams(WRAP, WRAP));

            TextView del = label("\u2715", 16, DIM);
            del.setPadding(dp(12), dp(8), dp(12), dp(8));
            del.setOnClickListener(v -> { NativeLib.deleteObj(idx); sync(); });
            r.addView(del, new LinearLayout.LayoutParams(WRAP, WRAP));

            outlinerList.addView(r, new LinearLayout.LayoutParams(MATCH, WRAP));
        }
    }

    private void refreshProps() {
        int sel = NativeLib.selected();
        boolean has = sel >= 0;
        propsHint.setVisibility(has ? View.GONE : View.VISIBLE);
        propsRows.setVisibility(has ? View.VISIBLE : View.GONE);
        propsTitle.setVisibility(has ? View.VISIBLE : View.GONE);
        if (!has) return;
        for (EditText e : fields) if (e.hasFocus()) return; // jangan timpa saat user mengetik
        propsTitle.setText(NativeLib.objName(sel));
        float[] t = NativeLib.getTransform(sel);
        if (t == null || t.length < 9) return;
        for (int i = 0; i < 9; i++) {
            String s = String.format(Locale.US, "%.2f", t[i]);
            shown[i] = s;
            fields[i].setText(s);
        }
    }

    private void scrub(int idx, float value) {
        int sel = NativeLib.selected();
        if (sel < 0) return;
        float[] t = NativeLib.getTransform(sel);
        if (t == null || t.length < 9) return;
        t[idx] = value;
        NativeLib.setTransform(sel, t);
        float[] n = NativeLib.getTransform(sel);  // nilai setelah dibatasi mesin (ukuran min 0.01)
        String s = String.format(Locale.US, "%.2f", n[idx]);
        shown[idx] = s;
        fields[idx].setText(s);
    }

    private void applyProps() {
        int sel = NativeLib.selected();
        if (sel < 0) return;
        float[] t = NativeLib.getTransform(sel);
        if (t == null || t.length < 9) return;
        boolean changed = false;
        for (int i = 0; i < 9; i++) {
            String s = fields[i].getText().toString().trim();
            if (s.equals(shown[i])) continue;
            try {
                t[i] = Float.parseFloat(s);
                shown[i] = s;
                changed = true;
            } catch (NumberFormatException ignored) { }
        }
        if (changed) { NativeLib.action(7); NativeLib.setTransform(sel, t); }
    }

    private void contextMenu(final int i) {
        NativeLib.selectObj(i);
        final boolean vis = NativeLib.objVisible(i);
        int[] st = NativeLib.state();
        final boolean canPaste = st != null && st.length >= 8 && st[7] == 1;
        final String[] items = {"Salin", canPaste ? "Tempel" : "Tempel (kosong)", "Duplikat", "Ganti nama",
                "Warna\u2026", vis ? "Sembunyikan" : "Tampilkan", "Hapus"};
        new AlertDialog.Builder(this)
                .setTitle(NativeLib.objName(i))
                .setItems(items, (d, w) -> {
                    switch (w) {
                        case 0: NativeLib.action(5); break;
                        case 1: NativeLib.action(6); break;
                        case 2: NativeLib.action(4); break;
                        case 3: rename(i); break;
                        case 4: pickColor(); break;
                        case 5: NativeLib.setVisible(i, !vis); break;
                        default: NativeLib.deleteObj(i);
                    }
                    sync();
                })
                .show();
    }

    private void pickColor() {
        final String[] names = {"Abu-abu (bawaan)", "Merah", "Oranye", "Kuning", "Hijau", "Biru", "Ungu", "Pink"};
        new AlertDialog.Builder(this)
                .setTitle("Warna objek")
                .setItems(names, (d, w) -> { NativeLib.action(10 + w); sync(); })
                .show();
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

    private static final int REQ_SO = 77;

    private void pickSo() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, REQ_SO);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_SO || res != RESULT_OK || data == null || data.getData() == null) return;
        try (InputStream in = getContentResolver().openInputStream(data.getData())) {
            String err = NativeLib.importSo(this, in);
            Toast.makeText(this, err == null
                    ? "Berhasil. Tutup aplikasi (geser dari recent) lalu buka lagi."
                    : "Ditolak: " + err, Toast.LENGTH_LONG).show();
        } catch (Exception e) {
            Toast.makeText(this, "Gagal membaca file: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void export() {
        final String path = getExternalFilesDir(null) + "/model.obj";
        final boolean ok = NativeLib.exportObj(path);
        Toast.makeText(this, ok ? "Tersimpan: " + path : "Gagal menyimpan OBJ", Toast.LENGTH_LONG).show();
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
