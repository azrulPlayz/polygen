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
    public static final String API_TAG = "POLYGEN_API_2";
    private static boolean loaded = false;
    private static boolean fromOverride = false;

    private static File overrideFile(Context c) { return new File(c.getFilesDir(), "libpolygen_override.so"); }

    /** Panggil sekali di awal (sebelum fungsi native mana pun). Pakai .so pengganti kalau ada dan cocok, kalau tidak yang bawaan APK. */
    public static synchronized void load(Context ctx) {
        if (loaded) return;
        File o = overrideFile(ctx);
        if (o.exists()) {
            try {
                // .so pengganti lama (API lain) akan membuat aplikasi crash saat fungsi baru dipanggil: cek dulu
                if (verify(o) == null) {
                    System.load(o.getAbsolutePath());
                    fromOverride = true;
                    loaded = true;
                    return;
                }
            } catch (Throwable t) {
                // jatuh ke bawah: buang
            }
            o.setWritable(true);
            o.delete();  // rusak / versi lama: buang, pakai bawaan
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
    /** {mode(0 objek,1 edit,2 paint), selMode, tool, axis, selected, canUndo, canRedo, canPaste,
     *   paintTool(0 kuas,1 hapus,2 isi,3 pipet), brushSize, brushColorARGB, activeTex} */
    public static native int[] state();

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
    /** shape: 0 kubus, 1 kubus bevel, 2 piramida, 3 bidang, 4 lingkaran, 5 silinder, 6 tabung, 7 kerucut,
     *  8 bola, 9 ikosfer, 10 oktahedron, 11 dodekahedron, 12 torus */
    public static native void addMesh(int shape, float diameter, float height);

    // alat
    /** kind: 0 seleksi edit (0 vertex, 1 face), 1 tool (0 kamera, 1 geser, 2 ukuran, 3 putar),
     *  2 sumbu (0 X, 1 Y, 2 Z, 3 N), 3 mode (0 objek, 1 edit, 2 paint),
     *  4 alat paint (0 kuas, 1 hapus, 2 isi, 3 pipet), 5 ukuran kuas (px), 6 warna kuas (ARGB), 7 tekstur aktif */
    public static native void set(int kind, int value);
    /** code: 0 extrude, 1 hapus pilihan, 2 undo, 3 redo, 4 duplikat, 5 salin, 6 tempel,
     *  7 mulai gestur edit baru (gabung undo), 10..17 warna objek (0 abu bawaan), 20 reset kamera */
    public static native void action(int code);
    public static native boolean exportObj(String path);

    // grup
    public static native int groupCount();
    public static native String groupName(int g);
    /** {visible, collapsed} */
    public static native int[] groupInfo(int g);
    public static native int objGroup(int i);
    /** op: 0 grup baru, 1 toggle tampil grup a, 2 toggle lipat grup a, 3 pindah objek a ke grup b (-1 = root),
     *  4 hapus grup a (isi ke root), 5 hapus grup a beserta isi, 6 grupkan objek a ke grup baru. Mengembalikan indeks grup baru atau -1 */
    public static native int groupOp(int op, int a, int b);
    public static native void renameGroup(int g, String name);

    // tekstur & UV
    public static native int texCount();
    public static native String texName(int i);
    /** {lebar, tinggi} */
    public static native int[] texInfo(int i);
    /** piksel ARGB, baris atas dulu */
    public static native int[] texPixels(int i);
    /** Mengembalikan indeks tekstur baru, atau -1 (penuh / ukuran tidak valid, maksimal 8 tekstur dan 512x512) */
    public static native int texCreate(String name, int w, int h, int argb);
    public static native int texSet(String name, int w, int h, int[] argb);
    public static native void texDelete(int i);
    public static native int objTex(int i);
    public static native void setObjTex(int i, int t);
    public static native int faceCount(int obj);
    public static native String faceName(int obj, int face);
    /** {u0, v0, u1, v1} dalam 0..1 */
    public static native float[] getFaceUV(int obj, int face);
    public static native void setFaceUV(int obj, int face, float[] uv);
    public static native void autoUV(int obj);
}
