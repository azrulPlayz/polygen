package com.mini.polygen;

/** Jembatan JNI ke mesin modelling C++ (native-lib.cpp). Semua dipanggil dari thread GL. */
public final class NativeLib {
    static { System.loadLibrary("polygen"); }

    public static native void init();
    public static native void resize(int w, int h);
    public static native void draw();
    /** act: 0 down, 1 up, 2 move, 5 pointer down, 6 pointer up */
    public static native void touch(int act, float x, float y, float x2, float y2, int n);
    /** type: 0 kubus, 1 bidang */
    public static native void add(int type);
    /** kind: 0 mode seleksi (0 vertex, 1 face), 1 tool (0 orbit, 1 grab), 2 sumbu (0 X, 1 Y, 2 Z, 3 normal) */
    public static native void set(int kind, int value);
    /** code: 0 extrude, 1 hapus */
    public static native void action(int code);
    public static native boolean exportObj(String path);
}
