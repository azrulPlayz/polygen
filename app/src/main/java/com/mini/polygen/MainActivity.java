package com.mini.polygen;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.Toast;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity {
    private GLSurfaceView gl;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        gl = new GLSurfaceView(this) {
            @Override
            public boolean onTouchEvent(MotionEvent e) {
                final int a = e.getActionMasked();
                int n = e.getPointerCount();
                float px = e.getX(0), py = e.getY(0);
                float qx = n > 1 ? e.getX(1) : 0, qy = n > 1 ? e.getY(1) : 0;
                if (a == MotionEvent.ACTION_POINTER_UP) {
                    int keep = e.getActionIndex() == 0 ? 1 : 0;
                    px = e.getX(keep);
                    py = e.getY(keep);
                    n = 1;
                }
                final float x = px, y = py, x2 = qx, y2 = qy;
                final int cnt = n;
                queueEvent(() -> NativeLib.touch(a, x, y, x2, y2, cnt));
                return true;
            }
        };
        gl.setEGLContextClientVersion(3);
        gl.setRenderer(new GLSurfaceView.Renderer() {
            @Override public void onSurfaceCreated(GL10 g, EGLConfig c) { NativeLib.init(); }
            @Override public void onSurfaceChanged(GL10 g, int w, int h) { NativeLib.resize(w, h); }
            @Override public void onDrawFrame(GL10 g) { NativeLib.draw(); }
        });

        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.VERTICAL);
        bar.setBackgroundColor(0xCC202428);

        LinearLayout r1 = row(), r2 = row();
        r1.addView(btn("Kubus", () -> NativeLib.add(0)));
        r1.addView(btn("Bidang", () -> NativeLib.add(1)));
        r1.addView(btn("Vertex", () -> NativeLib.set(0, 0)));
        r1.addView(btn("Face", () -> NativeLib.set(0, 1)));
        r1.addView(btn("Extrude", () -> NativeLib.action(0)));
        r1.addView(btn("Hapus", () -> NativeLib.action(1)));
        r1.addView(btn("Simpan OBJ", this::export));
        r2.addView(btn("Orbit", () -> NativeLib.set(1, 0)));
        r2.addView(btn("Geser", () -> NativeLib.set(1, 1)));
        r2.addView(btn("X", () -> NativeLib.set(2, 0)));
        r2.addView(btn("Y", () -> NativeLib.set(2, 1)));
        r2.addView(btn("Z", () -> NativeLib.set(2, 2)));
        r2.addView(btn("Normal", () -> NativeLib.set(2, 3)));
        bar.addView(r1);
        bar.addView(r2);

        FrameLayout root = new FrameLayout(this);
        root.setFitsSystemWindows(true);
        root.addView(gl);
        root.addView(bar, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM));
        setContentView(root);
    }

    private LinearLayout row() {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.HORIZONTAL);
        return l;
    }

    private Button btn(String text, Runnable r) {
        Button b = new Button(this);
        b.setText(text);
        b.setTextSize(11);
        b.setAllCaps(false);
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setPadding(4, 0, 4, 0);
        b.setOnClickListener(v -> gl.queueEvent(r));
        b.setLayoutParams(new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        return b;
    }

    private void export() {
        final String path = getExternalFilesDir(null) + "/model.obj";
        final boolean ok = NativeLib.exportObj(path);
        runOnUiThread(() -> Toast.makeText(this,
                ok ? "Tersimpan: " + path : "Gagal menyimpan OBJ",
                Toast.LENGTH_LONG).show());
    }

    @Override protected void onPause() { super.onPause(); gl.onPause(); }
    @Override protected void onResume() { super.onResume(); gl.onResume(); }
}
