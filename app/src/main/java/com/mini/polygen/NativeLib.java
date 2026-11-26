package com.mini.polygen;

import android.content.Context;
import android.os.Build;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Jembatan JNI ke mesin modelling C++ (native-lib.cpp). Semua fungsi aman dipanggil dari thread mana pun. */
public final class NativeLib {
    /** Harus sama dengan POLYGEN_API_TAG di native-lib.cpp. Naikkan kalau daftar fungsi native berubah. */
    public static final String API_TAG = "POLYGEN_API_1";
    private static boolean loaded = false;
    private static boolean fromOverride = false;

    private static File overrideFile(Context c) { return new File(c.getFilesDir(), "libpolygen_override.so"); }

    /** Panggil sekali di awal (sebelum fungsi native mana pun). Pakai .so pengganti kalau ada, kalau tidak yang bawaan APK. */
    public static synchronized void load(Context ctx) {
        if (loaded) return;
        File o = overrideFile(ctx);
        if (o.exists()) {
            try {
                System.load(o.getAbsolutePath());
                fromOverride = true;
                loaded = true;
                return;
            } catch (Throwable t) {
                o.setWritable(true);
                o.delete();  // rusak: buang, pakai bawaan
            }
        }
        System.loadLibrary("polygen");
        loaded = true;
    }

    public static boolean usingOverride() { return fromOverride; }

    public static boolean hasOverride(Context ctx) { return overrideFile(ctx).exists(); }

    /** Hapus .so pengganti. Berlaku setelah aplikasi dibuka ulang. */
    public static boolean resetOverride(Context ctx) {
        File o = overrideFile(ctx);
        if (!o.exists()) return false;
        o.setWritable(true);
        return o.delete();
    }

    /** Simpan .so pengganti. Mengembalikan null kalau berhasil, atau pesan error. Berlaku setelah aplikasi dibuka ulang. */
    public static String importSo(Context ctx, InputStream in) throws IOException {
        File tmp = new File(ctx.getFilesDir(), "libpolygen_new.so");
        try (OutputStream out = new FileOutputStream(tmp)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
        String err = verify(tmp);
        if (err != null) { tmp.delete(); return err; }
        File dst = overrideFile(ctx);
        if (dst.exists()) { dst.setWritable(true); dst.delete(); }
        if (!tmp.renameTo(dst)) { tmp.delete(); return "Gagal menyimpan file"; }
        dst.setReadOnly();  // Android 14+ menolak kode yang bisa ditulis
        return null;
    }

    private static String verify(File f) throws IOException {
        ByteArrayOutputStream bo = new ByteArrayOutputStream();
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
        }
        byte[] b = bo.toByteArray();
        if (b.length < 20 || b[0] != 0x7f || b[1] != 'E' || b[2] != 'L' || b[3] != 'F')
            return "Bukan file .so (bukan ELF)";
        int machine = (b[18] & 0xff) | ((b[19] & 0xff) << 8);
        String abi = Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "";
        int want = abi.equals("arm64-v8a") ? 183 : abi.equals("armeabi-v7a") ? 40
                : abi.equals("x86_64") ? 62 : abi.equals("x86") ? 3 : -1;
        if (want > 0 && machine != want) return "Arsitektur .so salah, HP ini butuh " + abi;
        byte[] tag = API_TAG.getBytes("US-ASCII");
        if (indexOf(b, tag) < 0) return "Versi API .so tidak cocok (butuh " + API_TAG + ")";
        return null;
    }

    private static int indexOf(byte[] h, byte[] n) {
        outer:
        for (int i = 0; i + n.length <= h.length; i++) {
            for (int j = 0; j < n.length; j++) if (h[i + j] != n[j]) continue outer;
            return i;
        }
        return -1;
    }

    // render
    public static native void init();
    public static native void resize(int w, int h);
    public static native void draw();
    /** act: 0 down, 1 up, 2 move, 5 pointer down, 6 pointer up */
    public static native void touch(int act, float x, float y, float x2, float y2, int n);

    // status
    public static native int version();      // naik setiap ada perubahan
    public static native int[] state();      // {mode, selMode, tool, axis, selected, canUndo, canRedo, canPaste}

    // objek
    public static native int objCount();
    public static native String objName(int i);
    public static native boolean objVisible(int i);
    public static native int selected();
    public static native void selectObj(int i);
    public static native void setVisible(int i, boolean v);
    public static native void deleteObj(int i);
    public static native void renameObj(int i, String name);
    /** {posX, posY, posZ, rotX, rotY, rotZ, sizeX, sizeY, sizeZ} */
    public static native float[] getTransform(int i);
    public static native void setTransform(int i, float[] t);
    /** type: 0 kubus, 1 bidang */
    public static native void addObj(int type);

    // alat
    /** kind: 0 seleksi edit (0 vertex, 1 face), 1 tool (0 kamera, 1 geser, 2 ukuran, 3 putar),
     *  2 sumbu (0 X, 1 Y, 2 Z, 3 N), 3 mode (0 objek, 1 edit) */
    public static native void set(int kind, int value);
    /** code: 0 extrude, 1 hapus pilihan, 2 undo, 3 redo, 4 duplikat, 5 salin, 6 tempel,
     *  7 mulai gestur edit baru (gabung undo), 10..17 warna objek (0 abu bawaan) */
    public static native void action(int code);
    public static native boolean exportObj(String path);
}
