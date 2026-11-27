// Polygen Modelling: mesin 3D modelling dari nol (tanpa GLM/library lain).
// Banyak objek, grup, tekstur + UV per sisi, mode objek / edit mesh / paint, undo-redo.
#include <jni.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
using std::string;
using std::vector;

// ---------- matematika ----------
struct V3 { float x, y, z; };
struct V2 { float x, y; };
static V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static V3 norm(V3 a) { float l = sqrtf(dot(a, a)); return l > 1e-8f ? a * (1.f / l) : V3{0, 1, 0}; }

static const float D2R = 0.017453292f;
static const float PI = 3.14159265f;
static V3 rotX(V3 p, float a) { float c = cosf(a), s = sinf(a); return {p.x, p.y * c - p.z * s, p.y * s + p.z * c}; }
static V3 rotY(V3 p, float a) { float c = cosf(a), s = sinf(a); return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c}; }
static V3 rotZ(V3 p, float a) { float c = cosf(a), s = sinf(a); return {p.x * c - p.y * s, p.x * s + p.y * c, p.z}; }

struct M4 { float m[16]; };  // column-major
static M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}
static M4 persp(float fov, float asp, float n, float f) {
    float t = 1.f / tanf(fov * 0.5f);
    M4 r = {};
    r.m[0] = t / asp; r.m[5] = t;
    r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
static M4 lookAt(V3 e, V3 c, V3 up) {
    V3 f = norm(c - e), s = norm(cross(f, up)), u = cross(s, f);
    M4 r = {};
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, e); r.m[13] = -dot(u, e); r.m[14] = dot(f, e); r.m[15] = 1;
    return r;
}

// ---------- data ----------
typedef std::array<float, 4> UVRect;  // u0, v0, u1, v1 (0..1, v ke bawah seperti gambar)

struct Obj {
    string name;
    vector<V3> verts, wv;  // lokal, dunia
    vector<vector<int>> faces;
    vector<UVRect> fuv;    // satu persegi UV per sisi
    V3 pos{0, 0, 0}, rot{0, 0, 0}, size{1, 1, 1};  // rot dalam derajat (XYZ)
    bool visible = true;
    V3 color{0.72f, 0.74f, 0.78f};
    bool colored = false;  // warna dipilih user
    int group = -1;        // indeks grup, -1 = root
    int tex = -1;          // indeks tekstur, -1 = tanpa tekstur
};
struct Group { string name; bool visible = true, collapsed = false; };
struct Tex {
    string name;
    int w = 16, h = 16;
    std::shared_ptr<vector<uint8_t>> px;  // RGBA, baris atas dulu
    bool dirty = true;
};

static const int MAXTEX = 8;
static const int NCOUNT = 20;  // 0..12 bentuk, 16 grup

static std::mutex M;
static int W = 1, H = 1;
static vector<Obj> objs;
static vector<Group> groups;
static vector<Tex> texs;
static int activeTex = -1;
static int curObj = -1;   // objek terpilih
static int mode = 0;      // 0 objek, 1 edit mesh, 2 paint
static int selMode = 0;   // edit: 0 vertex, 1 face
static int selV = -1, selF = -1;
static int tool = 0;      // 0 kamera, 1 geser, 2 ukuran, 3 putar
static int axisI = 1;     // 0 X, 1 Y, 2 Z, 3 N (normal face / ukuran seragam)
static int nameCount[NCOUNT] = {};
static int stateVer = 0;
static bool dirty = true;
static float yaw = 0.7f, pitch = 0.55f, camDist = 5.f;
static V3 camTarget = {0, 0.5f, 0};
static M4 curMvp;
static V3 curEye = {0, 0, 0}, camFwd = {0, 0, -1}, camRight = {1, 0, 0}, camUp = {0, 1, 0};
static GLuint prog;
static GLint uMvp, uLit, uPt, uTex, uUseTex;
// paint
static int paintTool = 0;   // 0 kuas, 1 penghapus, 2 isi, 3 pipet
static int brushSize = 2;   // piksel
static int brushArgb = (int)0xFFE85050;

static void bump() { stateVer++; dirty = true; }
static Obj* cur();

static bool objShown(const Obj& o) {
    return o.visible && (o.group < 0 || o.group >= (int)groups.size() || groups[o.group].visible);
}

// ---------- undo / redo (snapshot seluruh scene) ----------
struct Snap {
    vector<Obj> objs; vector<Group> groups; vector<Tex> texs;
    int curObj, activeTex; int nameCount[NCOUNT];
};
static vector<Snap> undoS, redoS;
static bool dragPushed = false;  // satu gestur = satu langkah undo
static const size_t MAX_HIST = 60;

static Snap takeSnap() {
    Snap s;
    s.objs = objs; s.groups = groups; s.texs = texs;
    s.curObj = curObj; s.activeTex = activeTex;
    std::copy(nameCount, nameCount + NCOUNT, s.nameCount);
    return s;
}
static void pushUndo() {  // panggil SEBELUM mengubah scene
    undoS.push_back(takeSnap());
    if (undoS.size() > MAX_HIST) undoS.erase(undoS.begin());
    redoS.clear();
}
static void beginDragEdit() { if (!dragPushed) { pushUndo(); dragPushed = true; } }
static void applySnap(Snap& s) {
    objs = std::move(s.objs); groups = std::move(s.groups); texs = std::move(s.texs);
    curObj = s.curObj; activeTex = s.activeTex;
    std::copy(s.nameCount, s.nameCount + NCOUNT, nameCount);
    for (Tex& t : texs) t.dirty = true;
    selV = selF = -1;
    if (curObj < 0 || curObj >= (int)objs.size()) { curObj = -1; if (mode == 1) mode = 0; }
    bump();
}
static void doUndo() {
    if (undoS.empty()) return;
    redoS.push_back(takeSnap());
    Snap s = std::move(undoS.back()); undoS.pop_back();
    applySnap(s);
}
static void doRedo() {
    if (redoS.empty()) return;
    undoS.push_back(takeSnap());
    Snap s = std::move(redoS.back()); redoS.pop_back();
    applySnap(s);
}

static Obj* cur() { return (curObj >= 0 && curObj < (int)objs.size()) ? &objs[curObj] : nullptr; }

static Obj clipObj;
static bool hasClip = false;
static void duplicateObj() {
    Obj* o = cur();
    if (!o) return;
    pushUndo();
    Obj c = *o;
    c.name += " copy";
    c.pos = c.pos + V3{0.5f, 0, 0.5f};
    objs.push_back(c);
    curObj = (int)objs.size() - 1;
    if (mode == 1) mode = 0;
    selV = selF = -1;
    bump();
}
static void copyObj() { Obj* o = cur(); if (o) { clipObj = *o; hasClip = true; bump(); } }
static void pasteObj() {
    if (!hasClip) return;
    pushUndo();
    Obj c = clipObj;
    c.pos = c.pos + V3{0.5f, 0, 0.5f};
    if (c.group >= (int)groups.size()) c.group = -1;
    if (c.tex >= (int)texs.size()) c.tex = -1;
    objs.push_back(c);
    curObj = (int)objs.size() - 1;
    if (mode == 1) mode = 0;
    selV = selF = -1;
    bump();
}
static const float PALETTE[8][3] = {{0.72f,0.74f,0.78f},{0.90f,0.30f,0.30f},{0.95f,0.60f,0.20f},{0.95f,0.85f,0.30f},
                                    {0.35f,0.80f,0.40f},{0.30f,0.55f,0.95f},{0.65f,0.40f,0.90f},{0.95f,0.50f,0.75f}};
static void setColorIdx(int k) {
    Obj* o = cur();
    if (!o || k < 0 || k > 7) return;
    pushUndo();
    o->color = {PALETTE[k][0], PALETTE[k][1], PALETTE[k][2]};
    o->colored = (k != 0);
    bump();
}

static V3 toWorld(const Obj& o, V3 p) {
    p = {p.x * o.size.x, p.y * o.size.y, p.z * o.size.z};
    p = rotZ(rotY(rotX(p, o.rot.x * D2R), o.rot.y * D2R), o.rot.z * D2R);
    return p + o.pos;
}
static V3 toLocal(const Obj& o, V3 w) {
    w = w - o.pos;
    w = rotX(rotY(rotZ(w, -o.rot.z * D2R), -o.rot.y * D2R), -o.rot.x * D2R);
    return {w.x / o.size.x, w.y / o.size.y, w.z / o.size.z};
}
static void refreshWorld() {
    for (Obj& o : objs) {
        o.wv.resize(o.verts.size());
        for (size_t i = 0; i < o.verts.size(); i++) o.wv[i] = toWorld(o, o.verts[i]);
    }
}

static bool toScreen(V3 p, float& sx, float& sy) {
    const float* m = curMvp.m;
    float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (w <= 0.01f) return false;
    sx = (x / w * 0.5f + 0.5f) * W;
    sy = (1.f - (y / w * 0.5f + 0.5f)) * H;
    return true;
}

static V3 rayDir(float x, float y) {
    float nx = (x / W) * 2.f - 1.f, ny = 1.f - (y / H) * 2.f;
    float th = tanf(0.45f);
    return norm(camFwd + camRight * (nx * th * ((float)W / H)) + camUp * (ny * th));
}

static bool rayTriB(V3 o, V3 d, V3 a, V3 b, V3 c, float& t, float& bu, float& bv) {
    V3 e1 = b - a, e2 = c - a, h = cross(d, e2);
    float det = dot(e1, h);
    if (fabsf(det) < 1e-9f) return false;
    float inv = 1.f / det;
    V3 s = o - a;
    float u = inv * dot(s, h);
    if (u < 0.f || u > 1.f) return false;
    V3 q = cross(s, e1);
    float v = inv * dot(d, q);
    if (v < 0.f || u + v > 1.f) return false;
    t = inv * dot(e2, q);
    bu = u; bv = v;
    return true;
}
static bool rayTri(V3 o, V3 d, V3 a, V3 b, V3 c, float& t) { float u, v; return rayTriB(o, d, a, b, c, t, u, v); }

// ---------- mesh ----------
static V3 faceNv(const vector<V3>& v, const vector<int>& f) {  // metode Newell
    V3 n = {0, 0, 0};
    for (size_t i = 0; i < f.size(); i++) {
        V3 a = v[f[i]], b = v[f[(i + 1) % f.size()]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return norm(n);
}

static void autoUV(Obj& o) {
    int n = (int)o.faces.size();
    o.fuv.assign(n, UVRect{{0, 0, 1, 1}});
    int cols = std::max(1, (int)ceilf(sqrtf((float)std::max(n, 1))));
    float cell = 1.f / cols, pad = cell * 0.04f;
    for (int i = 0; i < n; i++) {
        float u = (i % cols) * cell, v = (i / cols) * cell;
        o.fuv[i] = UVRect{{u + pad, v + pad, u + cell - pad, v + cell - pad}};
    }
}
static void ensureUV(Obj& o) {
    if (o.fuv.size() > o.faces.size()) o.fuv.resize(o.faces.size());
    while (o.fuv.size() < o.faces.size()) o.fuv.push_back(UVRect{{0, 0, 0.25f, 0.25f}});
}

struct FB { V3 o, u, v; float u0, u1, v0, v1; };
// Basis bidang sisi (lokal): u mengarah ke kanan jika dilihat dari luar, v ke atas.
static bool faceBasis(const Obj& o, int fi, FB& b) {
    const vector<int>& f = o.faces[fi];
    if (f.size() < 3) return false;
    V3 n = faceNv(o.verts, f), u;
    if (fabsf(n.y) < 0.9f) u = norm(cross(V3{0, 1, 0}, n));
    else { V3 x = {1, 0, 0}; u = norm(x - n * dot(x, n)); }
    V3 v = cross(n, u);
    b.o = o.verts[f[0]]; b.u = u; b.v = v;
    b.u0 = b.v0 = 1e30f; b.u1 = b.v1 = -1e30f;
    for (int i : f) {
        V3 d = o.verts[i] - b.o;
        float s = dot(d, u), t = dot(d, v);
        b.u0 = std::min(b.u0, s); b.u1 = std::max(b.u1, s);
        b.v0 = std::min(b.v0, t); b.v1 = std::max(b.v1, t);
    }
    return true;
}
static V2 uvOf(const Obj& o, int fi, const FB& b, V3 p) {
    V3 d = p - b.o;
    float s = (dot(d, b.u) - b.u0) / std::max(b.u1 - b.u0, 1e-6f);
    float t = (dot(d, b.v) - b.v0) / std::max(b.v1 - b.v0, 1e-6f);
    const UVRect& r = o.fuv[fi];
    return {r[0] + (r[2] - r[0]) * s, r[1] + (r[3] - r[1]) * (1.f - t)};
}

static bool selCenter(V3& c) {
    Obj* o = cur();
    if (!o || mode == 2) return false;
    refreshWorld();
    if (mode == 0) { c = o->pos; return true; }
    if (selMode == 0 && selV >= 0 && selV < (int)o->wv.size()) { c = o->wv[selV]; return true; }
    if (selMode == 1 && selF >= 0 && selF < (int)o->faces.size()) {
        V3 s = {0, 0, 0};
        for (int i : o->faces[selF]) s = s + o->wv[i];
        c = s * (1.f / o->faces[selF].size());
        return true;
    }
    return false;
}

static V3 axisDir() {
    if (axisI == 0) return {1, 0, 0};
    if (axisI == 2) return {0, 0, 1};
    Obj* o = cur();
    if (axisI == 3 && mode == 1 && selMode == 1 && o && selF >= 0 && selF < (int)o->faces.size())
        return faceNv(o->wv, o->faces[selF]);
    return {0, 1, 0};
}

// ---------- pembuat bentuk ----------
static const char* SHAPE_NAMES[13] = {"Kubus", "Kubus Bevel", "Piramida", "Bidang", "Lingkaran", "Silinder", "Tabung",
                                      "Kerucut", "Bola", "Ikosfer", "Oktahedron", "Dodekahedron", "Torus"};

// Balik urutan titik sisi supaya normal (Newell) menghadap keluar dari titik acuan ref(centroid).
static void orientFaces(Obj& o, int from, int to, const std::function<V3(V3)>& ref, bool outward = true) {
    for (int i = from; i < to && i < (int)o.faces.size(); i++) {
        vector<int>& f = o.faces[i];
        V3 c = {0, 0, 0};
        for (int k : f) c = c + o.verts[k];
        c = c * (1.f / f.size());
        float d = dot(faceNv(o.verts, f), c - ref(c));
        if ((d < 0) == outward) std::reverse(f.begin(), f.end());
    }
}
static V3 originRef(V3) { return {0, 0, 0}; }

static Obj buildMesh(int shape, float d, float h) {
    Obj o;
    d = std::max(0.05f, d); h = std::max(0.05f, h);
    const float r = d * 0.5f, hh = h * 0.5f;
    const int N = 16;
    auto V = [&](float x, float y, float z) { o.verts.push_back({x, y, z}); return (int)o.verts.size() - 1; };
    switch (shape) {
    case 0: {  // kubus
        const float c[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
        for (auto& p : c) V(p[0] * r, p[1] * hh, p[2] * r);
        const int f[6][4] = {{4,5,6,7},{1,0,3,2},{5,1,2,6},{0,4,7,3},{7,6,2,3},{0,1,5,4}};
        for (auto& q : f) o.faces.push_back({q[0], q[1], q[2], q[3]});
        break;
    }
    case 1: {  // kubus bevel
        float b = 0.18f * std::min(d, h);
        auto ci = [](int sx, int sy, int sz) { return (sx > 0 ? 4 : 0) + (sy > 0 ? 2 : 0) + (sz > 0 ? 1 : 0); };
        for (int c = 0; c < 8; c++) {
            int sx = (c & 4) ? 1 : -1, sy = (c & 2) ? 1 : -1, sz = (c & 1) ? 1 : -1;
            V(sx * r, sy * (hh - b), sz * (r - b));
            V(sx * (r - b), sy * hh, sz * (r - b));
            V(sx * (r - b), sy * (hh - b), sz * r);
        }
        const int ord[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int a = 0; a < 3; a++)
            for (int s = -1; s <= 1; s += 2) {
                vector<int> f;
                for (auto& q : ord) {
                    int sg[3]; sg[a] = s; sg[(a + 1) % 3] = q[0]; sg[(a + 2) % 3] = q[1];
                    f.push_back(ci(sg[0], sg[1], sg[2]) * 3 + a);
                }
                o.faces.push_back(f);
            }
        for (int a1 = 0; a1 < 3; a1++)
            for (int a2 = a1 + 1; a2 < 3; a2++) {
                int a3 = 3 - a1 - a2;
                for (int s1 = -1; s1 <= 1; s1 += 2)
                    for (int s2 = -1; s2 <= 1; s2 += 2) {
                        int g0[3], g1[3];
                        g0[a1] = g1[a1] = s1; g0[a2] = g1[a2] = s2; g0[a3] = -1; g1[a3] = 1;
                        int c0 = ci(g0[0], g0[1], g0[2]), c1 = ci(g1[0], g1[1], g1[2]);
                        o.faces.push_back({c0 * 3 + a1, c0 * 3 + a2, c1 * 3 + a2, c1 * 3 + a1});
                    }
            }
        for (int c = 0; c < 8; c++) o.faces.push_back({c * 3, c * 3 + 1, c * 3 + 2});
        break;
    }
    case 2: {  // piramida
        V(-r, -hh, -r); V(r, -hh, -r); V(r, -hh, r); V(-r, -hh, r); V(0, hh, 0);
        o.faces = {{0, 1, 2, 3}, {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}};
        break;
    }
    case 3: {  // bidang
        V(-r, 0, r); V(r, 0, r); V(r, 0, -r); V(-r, 0, -r);
        o.faces.push_back({0, 1, 2, 3});
        break;
    }
    case 4: {  // lingkaran
        vector<int> f;
        for (int i = 0; i < N; i++) { float a = 2 * PI * i / N; f.push_back(V(r * cosf(a), 0, -r * sinf(a))); }
        o.faces.push_back(f);
        break;
    }
    case 5: {  // silinder
        for (int i = 0; i < N; i++) { float a = 2 * PI * i / N; V(r * cosf(a), -hh, -r * sinf(a)); }
        for (int i = 0; i < N; i++) { float a = 2 * PI * i / N; V(r * cosf(a), hh, -r * sinf(a)); }
        vector<int> top, bot;
        for (int i = 0; i < N; i++) { top.push_back(N + i); bot.push_back(i); }
        o.faces.push_back(top); o.faces.push_back(bot);
        for (int i = 0; i < N; i++) o.faces.push_back({i, (i + 1) % N, N + (i + 1) % N, N + i});
        break;
    }
    case 6: {  // tabung (berlubang)
        float ri = r * 0.6f;
        for (int k = 0; k < 4; k++) {
            float rad = k < 2 ? r : ri, y = (k % 2 == 0) ? -hh : hh;  // luar bawah, luar atas, dalam bawah, dalam atas
            for (int i = 0; i < N; i++) { float a = 2 * PI * i / N; V(rad * cosf(a), y, -rad * sinf(a)); }
        }
        for (int i = 0; i < N; i++) o.faces.push_back({i, (i + 1) % N, N + (i + 1) % N, N + i});                  // dinding luar
        for (int i = 0; i < N; i++) o.faces.push_back({N + i, N + (i + 1) % N, 3 * N + (i + 1) % N, 3 * N + i});  // cincin atas
        for (int i = 0; i < N; i++) o.faces.push_back({i, (i + 1) % N, 2 * N + (i + 1) % N, 2 * N + i});          // cincin bawah
        for (int i = 0; i < N; i++) o.faces.push_back({2 * N + i, 2 * N + (i + 1) % N, 3 * N + (i + 1) % N, 3 * N + i});  // dinding dalam
        break;
    }
    case 7: {  // kerucut
        vector<int> base;
        for (int i = 0; i < N; i++) { float a = 2 * PI * i / N; base.push_back(V(r * cosf(a), -hh, -r * sinf(a))); }
        int apex = V(0, hh, 0);
        o.faces.push_back(base);
        for (int i = 0; i < N; i++) o.faces.push_back({i, (i + 1) % N, apex});
        break;
    }
    case 8: {  // bola
        const int rings = 8, segs = 12;
        V(0, r, 0);
        for (int j = 1; j < rings; j++) {
            float ph = PI * j / rings;
            for (int i = 0; i < segs; i++) { float a = 2 * PI * i / segs; V(r * sinf(ph) * cosf(a), r * cosf(ph), -r * sinf(ph) * sinf(a)); }
        }
        int bot = V(0, -r, 0);
        auto id = [&](int j, int i) { return 1 + (j - 1) * segs + (i % segs); };
        for (int i = 0; i < segs; i++) o.faces.push_back({0, id(1, i), id(1, i + 1)});
        for (int j = 1; j < rings - 1; j++)
            for (int i = 0; i < segs; i++) o.faces.push_back({id(j, i), id(j + 1, i), id(j + 1, i + 1), id(j, i + 1)});
        for (int i = 0; i < segs; i++) o.faces.push_back({id(rings - 1, i), bot, id(rings - 1, i + 1)});
        break;
    }
    case 9: {  // ikosfer (ikosahedron disubdivisi 1x)
        const float t = (1.f + sqrtf(5.f)) / 2.f;
        const float c[12][3] = {{0,1,t},{0,-1,t},{0,1,-t},{0,-1,-t},{1,t,0},{-1,t,0},{1,-t,0},{-1,-t,0},{t,0,1},{t,0,-1},{-t,0,1},{-t,0,-1}};
        vector<V3> base;
        for (auto& p : c) base.push_back(norm({p[0], p[1], p[2]}));
        vector<std::array<int, 3>> tris;
        for (int i = 0; i < 12; i++) for (int j = i + 1; j < 12; j++) for (int k = j + 1; k < 12; k++) {
            auto L = [&](int a, int b) { V3 e = base[a] - base[b]; return dot(e, e); };
            float e0 = L(0, 1) * 0 + 4.f / (1.f + t * t);  // panjang sisi^2 setelah dinormalkan
            if (fabsf(L(i, j) - e0) < 1e-3f && fabsf(L(j, k) - e0) < 1e-3f && fabsf(L(i, k) - e0) < 1e-3f) tris.push_back({i, j, k});
        }
        vector<V3> vs = base;
        std::map<std::pair<int, int>, int> mid;
        auto midpoint = [&](int a, int b) {
            auto key = std::make_pair(std::min(a, b), std::max(a, b));
            auto it = mid.find(key);
            if (it != mid.end()) return it->second;
            vs.push_back(norm((vs[a] + vs[b]) * 0.5f));
            return mid[key] = (int)vs.size() - 1;
        };
        for (auto& tr : tris) {
            int a = tr[0], b = tr[1], cc = tr[2];
            int ab = midpoint(a, b), bc = midpoint(b, cc), ca = midpoint(cc, a);
            o.faces.push_back({a, ab, ca}); o.faces.push_back({b, bc, ab});
            o.faces.push_back({cc, ca, bc}); o.faces.push_back({ab, bc, ca});
        }
        for (V3 p : vs) V(p.x * r, p.y * r, p.z * r);
        break;
    }
    case 10: {  // oktahedron
        V(r, 0, 0); V(-r, 0, 0); V(0, r, 0); V(0, -r, 0); V(0, 0, r); V(0, 0, -r);
        for (int sx = 0; sx < 2; sx++) for (int sy = 2; sy < 4; sy++) for (int sz = 4; sz < 6; sz++) o.faces.push_back({sx, sy, sz});
        break;
    }
    case 11: {  // dodekahedron
        const float t = (1.f + sqrtf(5.f)) / 2.f, it = 1.f / t, sc = r / sqrtf(3.f);
        vector<V3> pts;
        for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2) pts.push_back({(float)sx, (float)sy, (float)sz});
        for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) {
            pts.push_back({0, a * it, b * t}); pts.push_back({a * it, b * t, 0}); pts.push_back({b * t, 0, a * it});
        }
        for (V3 p : pts) V(p.x * sc, p.y * sc, p.z * sc);
        vector<V3> normals;
        for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) {
            normals.push_back({0, a * t, (float)b}); normals.push_back({(float)b, 0, a * t}); normals.push_back({a * t, (float)b, 0});
        }
        for (V3 n : normals) {
            n = norm(n);
            float best = -1e30f;
            for (V3 p : pts) best = std::max(best, dot(p, n));
            vector<int> idx;
            for (int i = 0; i < (int)pts.size(); i++) if (dot(pts[i], n) > best - 1e-3f) idx.push_back(i);
            V3 u = norm(cross(n, fabsf(n.y) < 0.9f ? V3{0, 1, 0} : V3{1, 0, 0})), v = cross(n, u);
            std::sort(idx.begin(), idx.end(), [&](int a, int b) {
                return atan2f(dot(pts[a], v), dot(pts[a], u)) < atan2f(dot(pts[b], v), dot(pts[b], u));
            });
            o.faces.push_back(idx);
        }
        break;
    }
    default: {  // torus
        const int SM = 16, ST = 8;
        float rho = 0.125f * d, R = 0.375f * d;
        for (int i = 0; i < SM; i++) for (int j = 0; j < ST; j++) {
            float th = 2 * PI * i / SM, ph = 2 * PI * j / ST;
            V((R + rho * cosf(ph)) * cosf(th), rho * sinf(ph), -(R + rho * cosf(ph)) * sinf(th));
        }
        for (int i = 0; i < SM; i++) for (int j = 0; j < ST; j++) {
            int i2 = (i + 1) % SM, j2 = (j + 1) % ST;
            o.faces.push_back({i * ST + j, i2 * ST + j, i2 * ST + j2, i * ST + j2});
        }
        break;
    }
    }
    // arah normal keluar
    int nf = (int)o.faces.size();
    switch (shape) {
    case 0: case 1: case 2: case 8: case 9: case 10: case 11: orientFaces(o, 0, nf, originRef); break;
    case 3: case 4: { vector<int>& f = o.faces[0]; if (faceNv(o.verts, f).y < 0) std::reverse(f.begin(), f.end()); break; }
    case 5: orientFaces(o, 0, 2, originRef); orientFaces(o, 2, nf, [](V3 c) { return V3{0, c.y, 0}; }); break;
    case 6: orientFaces(o, 0, 3 * N, [](V3 c) { return V3{0, c.y, 0}; }, true);
            orientFaces(o, 3 * N, nf, [](V3 c) { return V3{0, c.y, 0}; }, false); break;
    case 7: orientFaces(o, 0, 1, originRef); orientFaces(o, 1, nf, [](V3 c) { return V3{0, c.y, 0}; }); break;
    default: {
        float R = 0.375f * d;
        orientFaces(o, 0, nf, [R](V3 c) { V3 q = norm({c.x, 0, c.z}); return q * R; });
    }
    }
    // tabung: cincin atas/bawah ikut orientasi dinding luar menurut sumbu y; perbaiki cincin (normal vertikal)
    if (shape == 6) {
        for (int i = N; i < 3 * N; i++) {
            vector<int>& f = o.faces[i];
            V3 c = {0, 0, 0};
            for (int k : f) c = c + o.verts[k];
            float up = (c.y > 0) ? 1.f : -1.f;
            if (faceNv(o.verts, f).y * up < 0) std::reverse(f.begin(), f.end());
        }
    }
    autoUV(o);
    return o;
}

static void addMeshInternal(int shape, float d, float h) {
    if (shape < 0 || shape > 12) shape = 0;
    Obj o = buildMesh(shape, d, h);
    o.name = string(SHAPE_NAMES[shape]) + " " + std::to_string(++nameCount[shape]);
    float miny = 1e30f;
    for (const V3& v : o.verts) miny = std::min(miny, v.y);
    o.pos = {(float)objs.size() * 1.5f, -miny, 0};
    o.tex = activeTex;
    objs.push_back(o);
    curObj = (int)objs.size() - 1;
    if (mode == 2) mode = 0;
    selV = selF = -1;
    bump();
}
static void addObjInternal(int type) { addMeshInternal(type == 0 ? 0 : 3, 1.f, 1.f); }

static void extrudeFace() {
    Obj* o = cur();
    if (!o || mode != 1 || selMode != 1 || selF < 0 || selF >= (int)o->faces.size()) return;
    pushUndo();
    ensureUV(*o);
    vector<int> f = o->faces[selF];
    UVRect src = o->fuv[selF];
    V3 n = faceNv(o->verts, f);
    vector<int> nf;
    for (int i : f) { o->verts.push_back(o->verts[i] + n * 0.5f); nf.push_back((int)o->verts.size() - 1); }
    size_t k = f.size();
    for (size_t i = 0; i < k; i++) {
        o->faces.push_back({f[i], f[(i + 1) % k], nf[(i + 1) % k], nf[i]});
        o->fuv.push_back(src);
    }
    o->faces[selF] = nf;
    bump();
}

static void deleteObjAt(int i) {
    if (i < 0 || i >= (int)objs.size()) return;
    pushUndo();
    objs.erase(objs.begin() + i);
    if (curObj == i) curObj = -1;
    else if (curObj > i) curObj--;
    if (curObj < 0 && mode == 1) mode = 0;
    selV = selF = -1;
    bump();
}

static void deleteSel() {
    if (mode == 0) { deleteObjAt(curObj); return; }
    Obj* o = cur();
    if (!o || mode != 1) return;
    ensureUV(*o);
    if (selMode == 1 && selF >= 0 && selF < (int)o->faces.size()) {
        pushUndo();
        o->faces.erase(o->faces.begin() + selF);
        o->fuv.erase(o->fuv.begin() + selF);
        selF = -1;
    } else if (selMode == 0 && selV >= 0) {
        pushUndo();
        int v = selV;
        vector<vector<int>> nfaces;
        vector<UVRect> nuv;
        for (size_t i = 0; i < o->faces.size(); i++) {
            if (std::find(o->faces[i].begin(), o->faces[i].end(), v) != o->faces[i].end()) continue;
            nfaces.push_back(o->faces[i]);
            nuv.push_back(o->fuv[i]);
        }
        o->faces.swap(nfaces);
        o->fuv.swap(nuv);
        selV = -1;
    }
    bump();
}

// ---------- grup ----------
static int addGroupInternal() {
    Group g;
    g.name = "group " + std::to_string(++nameCount[16]);
    groups.push_back(g);
    return (int)groups.size() - 1;
}
static void deleteGroup(int g, bool withChildren) {
    if (g < 0 || g >= (int)groups.size()) return;
    pushUndo();
    if (withChildren) {
        vector<Obj> keep;
        int newCur = -1;
        for (int i = 0; i < (int)objs.size(); i++) {
            if (objs[i].group == g) continue;
            if (i == curObj) newCur = (int)keep.size();
            keep.push_back(objs[i]);
        }
        objs.swap(keep);
        curObj = newCur;
    }
    for (Obj& o : objs) { if (o.group == g) o.group = -1; else if (o.group > g) o.group--; }
    groups.erase(groups.begin() + g);
    if (curObj < 0 && mode == 1) mode = 0;
    selV = selF = -1;
    bump();
}
static int groupOp(int op, int a, int b) {
    switch (op) {
    case 0: { pushUndo(); int g = addGroupInternal(); bump(); return g; }
    case 1: if (a >= 0 && a < (int)groups.size()) { pushUndo(); groups[a].visible = !groups[a].visible; bump(); } break;
    case 2: if (a >= 0 && a < (int)groups.size()) { groups[a].collapsed = !groups[a].collapsed; bump(); } break;
    case 3: if (a >= 0 && a < (int)objs.size() && b >= -1 && b < (int)groups.size()) { pushUndo(); objs[a].group = b; bump(); } break;
    case 4: deleteGroup(a, false); break;
    case 5: deleteGroup(a, true); break;
    case 6: if (a >= 0 && a < (int)objs.size()) { pushUndo(); int g = addGroupInternal(); objs[a].group = g; bump(); return g; } break;
    default: break;
    }
    return -1;
}

// ---------- tekstur ----------
static int texAdd(const string& name, int w, int h, const vector<uint8_t>& rgba) {
    if ((int)texs.size() >= MAXTEX || w < 1 || h < 1 || w > 512 || h > 512) return -1;
    pushUndo();
    Tex t;
    t.name = name.empty() ? "texture" : name;
    t.w = w; t.h = h;
    t.px = std::make_shared<vector<uint8_t>>(rgba);
    texs.push_back(t);
    activeTex = (int)texs.size() - 1;
    for (Obj& o : objs) if (o.tex < 0) o.tex = activeTex;
    bump();
    return activeTex;
}
static void texDeleteAt(int i) {
    if (i < 0 || i >= (int)texs.size()) return;
    pushUndo();
    texs.erase(texs.begin() + i);
    for (Obj& o : objs) { if (o.tex == i) o.tex = -1; else if (o.tex > i) o.tex--; }
    if (activeTex == i) activeTex = texs.empty() ? -1 : std::min(i, (int)texs.size() - 1);
    else if (activeTex > i) activeTex--;
    bump();
}

// ---------- paint ----------
static int lastTex = -1, lastFace = -1, lastPx = 0, lastPy = 0;
static bool strokeFirst = true;

static bool hitFace(float x, float y, int& oi, int& fi, V3& localP) {
    refreshWorld();
    V3 d = rayDir(x, y);
    float best = 1e30f;
    bool hit = false;
    for (int j = 0; j < (int)objs.size(); j++) {
        const Obj& o = objs[j];
        if (!objShown(o)) continue;
        for (int i = 0; i < (int)o.faces.size(); i++) {
            const vector<int>& f = o.faces[i];
            for (size_t k = 1; k + 1 < f.size(); k++) {
                float t, u, v;
                if (rayTriB(curEye, d, o.wv[f[0]], o.wv[f[k]], o.wv[f[k + 1]], t, u, v) && t > 0.f && t < best) {
                    best = t; hit = true; oi = j; fi = i;
                    V3 a = o.verts[f[0]], b = o.verts[f[k]], c = o.verts[f[k + 1]];
                    localP = a + (b - a) * u + (c - a) * v;
                }
            }
        }
    }
    return hit;
}

static void putPx(Tex& T, int x, int y, int argb, bool erase) {
    if (x < 0 || y < 0 || x >= T.w || y >= T.h) return;
    uint8_t* p = &(*T.px)[(size_t)(y * T.w + x) * 4];
    if (erase) { p[0] = p[1] = p[2] = p[3] = 0; return; }
    p[0] = (argb >> 16) & 0xff; p[1] = (argb >> 8) & 0xff; p[2] = argb & 0xff; p[3] = (argb >> 24) & 0xff;
}
static void stamp(Tex& T, int cx, int cy, bool erase) {
    int s = std::max(1, brushSize), x0 = cx - s / 2, y0 = cy - s / 2;
    for (int y = y0; y < y0 + s; y++) for (int x = x0; x < x0 + s; x++) putPx(T, x, y, brushArgb, erase);
}
static void lineStamp(Tex& T, int x0, int y0, int x1, int y1, bool erase) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int guard = 0; guard < 4096; guard++) {
        stamp(T, x0, y0, erase);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void floodFill(Tex& T, int sx, int sy, int rx0, int ry0, int rx1, int ry1) {
    rx0 = std::max(0, rx0); ry0 = std::max(0, ry0); rx1 = std::min(T.w, rx1); ry1 = std::min(T.h, ry1);
    if (sx < rx0 || sx >= rx1 || sy < ry0 || sy >= ry1) return;
    auto getc = [&](int x, int y) { const uint8_t* p = &(*T.px)[(size_t)(y * T.w + x) * 4]; return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; };
    uint32_t target = getc(sx, sy);
    uint32_t fill = (uint32_t)((brushArgb >> 16) & 0xff) << 24 | (uint32_t)((brushArgb >> 8) & 0xff) << 16 | (uint32_t)(brushArgb & 0xff) << 8 | (uint32_t)((brushArgb >> 24) & 0xff);
    if (target == fill) return;
    vector<std::pair<int, int>> st;
    st.push_back({sx, sy});
    while (!st.empty()) {
        auto [x, y] = st.back(); st.pop_back();
        if (x < rx0 || x >= rx1 || y < ry0 || y >= ry1 || getc(x, y) != target) continue;
        putPx(T, x, y, brushArgb, false);
        st.push_back({x + 1, y}); st.push_back({x - 1, y}); st.push_back({x, y + 1}); st.push_back({x, y - 1});
    }
}

static void paintAt(float x, float y, bool begin) {
    int oi, fi;
    V3 lp;
    if (!hitFace(x, y, oi, fi, lp)) { lastTex = -1; return; }
    Obj& o = objs[oi];
    if (o.tex < 0 || o.tex >= (int)texs.size()) return;
    ensureUV(o);
    FB b;
    if (!faceBasis(o, fi, b)) return;
    V2 uv = uvOf(o, fi, b, lp);
    Tex& T = texs[o.tex];
    int px = std::min(T.w - 1, std::max(0, (int)floorf(uv.x * T.w)));
    int py = std::min(T.h - 1, std::max(0, (int)floorf(uv.y * T.h)));
    if (paintTool == 3) {  // pipet
        const uint8_t* p = &(*T.px)[(size_t)(py * T.w + px) * 4];
        if (p[3] > 0) brushArgb = (int)((uint32_t)p[3] << 24 | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]);
        stateVer++;
        return;
    }
    beginDragEdit();  // satu goresan = satu langkah undo
    if (T.px.use_count() > 1) T.px = std::make_shared<vector<uint8_t>>(*T.px);
    if (paintTool == 2) {
        if (begin) {
            const UVRect& r = o.fuv[fi];
            floodFill(T, px, py, (int)floorf(r[0] * T.w), (int)floorf(r[1] * T.h), (int)ceilf(r[2] * T.w), (int)ceilf(r[3] * T.h));
        }
    } else {
        bool erase = (paintTool == 1);
        if (!begin && lastTex == o.tex && lastFace == fi) lineStamp(T, lastPx, lastPy, px, py, erase);
        else stamp(T, px, py, erase);
    }
    lastTex = o.tex; lastFace = fi; lastPx = px; lastPy = py;
    T.dirty = true;
    stateVer++;  // geometri tidak berubah: tidak perlu rebuild
}

// ---------- interaksi ----------
static void grab(float dx, float dy) {
    Obj* o = cur();
    if (!o) return;
    if (mode == 0 && tool == 3) {  // putar
        float a = dx * 0.5f;
        beginDragEdit();
        if (axisI == 0) o->rot.x += a;
        else if (axisI == 2) o->rot.z += a;
        else o->rot.y += a;
        bump();
        return;
    }
    if (mode == 0 && tool == 2 && axisI == 3) {  // ukuran seragam
        float s = 1.f + (dx - dy) * 0.005f;
        beginDragEdit();
        o->size = {std::max(0.01f, o->size.x * s), std::max(0.01f, o->size.y * s), std::max(0.01f, o->size.z * s)};
        bump();
        return;
    }
    V3 c;
    if (!selCenter(c)) return;
    V3 d = axisDir();
    float x0, y0, x1, y1;
    if (!toScreen(c, x0, y0) || !toScreen(c + d, x1, y1)) return;
    float ax = x1 - x0, ay = y1 - y0, l2 = ax * ax + ay * ay;
    if (l2 < 4.f) return;  // sumbu menghadap kamera
    float t = (dx * ax + dy * ay) / l2;
    beginDragEdit();
    if (mode == 0) {
        if (tool == 1) o->pos = o->pos + d * t;
        else if (tool == 2) {
            float* sz[3] = {&o->size.x, &o->size.y, &o->size.z};
            int ai = axisI < 3 ? axisI : 1;
            *sz[ai] = std::max(0.01f, *sz[ai] + t);
        }
    } else if (tool == 1) {
        V3 delta = d * t;
        if (selMode == 0) o->verts[selV] = toLocal(*o, o->wv[selV] + delta);
        else for (int i : o->faces[selF]) o->verts[i] = toLocal(*o, o->wv[i] + delta);
    }
    bump();
}

static bool occluded(V3 p, int skipFace, int skipVert) {
    V3 d = p - curEye;
    for (int j = 0; j < (int)objs.size(); j++) {
        const Obj& o = objs[j];
        if (!objShown(o)) continue;
        for (int i = 0; i < (int)o.faces.size(); i++) {
            const vector<int>& f = o.faces[i];
            if (j == curObj) {
                if (i == skipFace) continue;
                if (skipVert >= 0 && std::find(f.begin(), f.end(), skipVert) != f.end()) continue;
            }
            for (size_t k = 1; k + 1 < f.size(); k++) {
                float t;
                if (rayTri(curEye, d, o.wv[f[0]], o.wv[f[k]], o.wv[f[k + 1]], t) && t > 0.001f && t < 0.995f)
                    return true;
            }
        }
    }
    return false;
}

static void pickObject(float x, float y) {
    refreshWorld();
    V3 d = rayDir(x, y);
    float best = 1e30f;
    int bi = -1;
    for (int j = 0; j < (int)objs.size(); j++) {
        const Obj& o = objs[j];
        if (!objShown(o)) continue;
        for (const auto& f : o.faces)
            for (size_t k = 1; k + 1 < f.size(); k++) {
                float t;
                if (rayTri(curEye, d, o.wv[f[0]], o.wv[f[k]], o.wv[f[k + 1]], t) && t > 0.f && t < best) {
                    best = t; bi = j;
                }
            }
    }
    curObj = bi;
    selV = selF = -1;
    bump();
}

static void pickElement(float x, float y) {
    Obj* o = cur();
    if (!o) return;
    refreshWorld();
    struct Cand { float d2; int i; V3 p; };
    vector<Cand> cs;
    int n = selMode == 0 ? (int)o->wv.size() : (int)o->faces.size();
    for (int i = 0; i < n; i++) {
        V3 p;
        if (selMode == 0) p = o->wv[i];
        else {
            V3 s = {0, 0, 0};
            for (int v : o->faces[i]) s = s + o->wv[v];
            p = s * (1.f / o->faces[i].size());
        }
        float sx, sy;
        if (!toScreen(p, sx, sy)) continue;
        float d2 = (sx - x) * (sx - x) + (sy - y) * (sy - y);
        if (d2 < 70.f * 70.f) cs.push_back({d2, i, p});
    }
    std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
    int bi = -1;
    for (const Cand& c : cs) {
        bool occ = selMode == 0 ? occluded(c.p, -1, c.i) : occluded(c.p, c.i, -1);
        if (!occ) { bi = c.i; break; }
    }
    if (selMode == 0) selV = bi; else selF = bi;
    bump();
}

// ---------- render (OpenGL ES 3.0, VAO + VBO) ----------
static GLuint compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

// format vertex: pos(3) nrm(3) col(3) uv(2)+tint(1) = 12 float
static void push(vector<float>& b, V3 p, V3 n, V3 c, float u = 0, float v = 0, float tint = 0) {
    float a[12] = {p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z, u, v, tint};
    b.insert(b.end(), a, a + 12);
}

struct Batch { GLuint vao = 0, vbo = 0; GLsizei n = 0; };
static Batch bGrid, bEdge, bPts, bAxis, bGz1, bGz2;
static Batch bTri[MAXTEX + 1];  // 0 = tanpa tekstur, k = tekstur k-1
static vector<GLuint> texGL;

static void upload(Batch& b, const vector<float>& d) {
    if (!b.vao) {
        glGenVertexArrays(1, &b.vao);
        glGenBuffers(1, &b.vbo);
        glBindVertexArray(b.vao);
        glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, 48, (const void*)(size_t)(i * 12));
        }
    }
    glBindVertexArray(b.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(d.size() * sizeof(float)), d.empty() ? nullptr : d.data(), GL_DYNAMIC_DRAW);
    b.n = (GLsizei)(d.size() / 12);
}

static void drawB(const Batch& b, GLenum m, float lit, float pt) {
    if (!b.n) return;
    glBindVertexArray(b.vao);
    glUniform1f(uLit, lit);
    glUniform1f(uPt, pt);
    glDrawArrays(m, 0, b.n);
}


// ---------- gizmo panah/cincin ----------
static bool gizmoOn(V3& c) {
    return tool >= 1 && (mode == 0 || (mode == 1 && tool == 1)) && selCenter(c);
}
static void gizmoBasis(int a, V3& d, V3& s, V3& t) {
    V3 ax[3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
    d = ax[a];
    V3 u = a == 1 ? V3{1.f, 0.f, 0.f} : V3{0.f, 1.f, 0.f};
    s = norm(cross(d, u));
    t = cross(d, s);
}
static V3 gizmoPoint(V3 c, int a, float L) {
    V3 d, s, t;
    gizmoBasis(a, d, s, t);
    if (tool == 3) return c + (s * 0.7071f + t * 0.7071f) * L;
    return c + d * (L * 1.05f);
}
static int gizmoPick(float x, float y) {
    V3 c;
    if (!gizmoOn(c)) return -1;
    float L = camDist * 0.16f, best = std::max(56.f, 0.05f * (float)H);
    int bi = -1;
    for (int a = 0; a < 3; a++) {
        float sx, sy;
        if (!toScreen(gizmoPoint(c, a, L), sx, sy)) continue;
        float d = hypotf(sx - x, sy - y);
        if (d < best) { best = d; bi = a; }
    }
    return bi;
}
static void drawGizmo() {
    vector<float> ln, tr;
    V3 c;
    if (gizmoOn(c)) {
        float L = camDist * 0.16f;
        V3 up{0.f, 1.f, 0.f};
        V3 cols[3] = {{1.f, 0.3f, 0.3f}, {0.4f, 0.9f, 0.4f}, {0.3f, 0.5f, 1.f}};
        for (int a = 0; a < 3; a++) {
            V3 d, s, t;
            gizmoBasis(a, d, s, t);
            V3 col = cols[a];
            if (tool == 3) {
                const int N = 40;
                for (int i = 0; i < N; i++) {
                    float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
                    push(ln, c + (s * cosf(a0) + t * sinf(a0)) * L, up, col);
                    push(ln, c + (s * cosf(a1) + t * sinf(a1)) * L, up, col);
                }
            } else {
                V3 base = c + d * (L * 0.8f), tip = c + d * (L * 1.15f);
                push(ln, c, up, col);
                push(ln, base, up, col);
                const int N = 10;
                float r = L * 0.07f;
                for (int i = 0; i < N; i++) {
                    float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
                    V3 p0 = base + (s * cosf(a0) + t * sinf(a0)) * r, p1 = base + (s * cosf(a1) + t * sinf(a1)) * r;
                    push(tr, p0, d, col);
                    push(tr, p1, d, col);
                    push(tr, tip, d, col);
                }
            }
        }
    }
    upload(bGz1, ln);
    upload(bGz2, tr);
    glLineWidth(3.f);
    drawB(bGz1, GL_LINES, 0.f, 1.f);
    drawB(bGz2, GL_TRIANGLES, 0.f, 1.f);
}

static const char* VS = R"(#version 300 es
uniform mat4 u_mvp;
uniform float u_pt;
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec3 a_col;
layout(location = 3) in vec3 a_uv;
out vec3 v_nrm;
out vec3 v_col;
out vec3 v_uv;
void main() {
  v_nrm = a_nrm;
  v_col = a_col;
  v_uv = a_uv;
  gl_Position = u_mvp * vec4(a_pos, 1.0);
  gl_PointSize = u_pt;
}
)";
static const char* FS = R"(#version 300 es
precision mediump float;
uniform float u_lit;
uniform float u_useTex;
uniform sampler2D u_tex;
in vec3 v_nrm;
in vec3 v_col;
in vec3 v_uv;
out vec4 frag;
void main() {
  vec3 c = v_col;
  if (u_useTex > 0.5) {
    vec4 t = texture(u_tex, v_uv.xy);
    if (t.a < 0.05) discard;
    c = mix(t.rgb, v_col, v_uv.z);
  }
  float l = 1.0;
  if (u_lit > 0.5) {
    vec3 L = normalize(vec3(0.4, 0.9, 0.5));
    l = 0.35 + 0.65 * abs(dot(normalize(v_nrm), L));
  }
  frag = vec4(c * l, 1.0);
}
)";

static void buildGrid() {
    const V3 up = {0, 1, 0};
    vector<float> g;
    for (int i = -6; i <= 6; i++) {
        bool c = (i == 0);
        V3 cz = c ? V3{0.3f, 0.5f, 1.f} : V3{0.35f, 0.29f, 0.28f};
        V3 cx = c ? V3{1.f, 0.3f, 0.3f} : V3{0.35f, 0.29f, 0.28f};
        push(g, {(float)i, 0, -6}, up, cz); push(g, {(float)i, 0, 6}, up, cz);
        push(g, {-6, 0, (float)i}, up, cx); push(g, {6, 0, (float)i}, up, cx);
    }
    upload(bGrid, g);
}

static void syncTextures() {
    while (texGL.size() > texs.size()) { GLuint id = texGL.back(); glDeleteTextures(1, &id); texGL.pop_back(); }
    while (texGL.size() < texs.size()) {
        GLuint id = 0;
        glGenTextures(1, &id);
        texGL.push_back(id);
        texs[texGL.size() - 1].dirty = true;
    }
    for (size_t i = 0; i < texs.size(); i++) {
        Tex& t = texs[i];
        if (!t.dirty || !t.px) continue;
        glBindTexture(GL_TEXTURE_2D, texGL[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.px->data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        t.dirty = false;
    }
}

// dibangun ulang hanya saat objek / seleksi / tool berubah
static void rebuild() {
    refreshWorld();
    const V3 up = {0, 1, 0};
    const V3 orange = {1.f, 0.6f, 0.1f};
    const V3 blue = {0.55f, 0.72f, 1.f};
    vector<float> t[MAXTEX + 1], e, p, a;
    for (int oi = 0; oi < (int)objs.size(); oi++) {
        Obj& o = objs[oi];
        if (!objShown(o)) continue;
        ensureUV(o);
        bool selO = (oi == curObj);
        int bucket = (o.tex >= 0 && o.tex < (int)texs.size() && o.tex < MAXTEX) ? o.tex + 1 : 0;
        bool selTint = selO && mode == 0;
        V3 base = (selTint && !o.colored) ? blue : o.color;
        V3 ec = (selTint) ? orange : V3{0.05f, 0.05f, 0.07f};
        for (int i = 0; i < (int)o.faces.size(); i++) {
            const vector<int>& f = o.faces[i];
            if (f.size() < 3) continue;
            V3 n = faceNv(o.wv, f);
            bool selFace = selO && mode == 1 && selMode == 1 && i == selF;
            V3 col = selFace ? orange : (bucket && selTint ? blue : base);
            float tint = selFace ? 0.55f : (bucket && selTint ? 0.35f : 0.f);
            FB fb;
            bool hasB = bucket > 0 && faceBasis(o, i, fb);
            vector<V2> uvs(f.size(), V2{0, 0});
            if (hasB) for (size_t k = 0; k < f.size(); k++) uvs[k] = uvOf(o, i, fb, o.verts[f[k]]);
            for (size_t k = 1; k + 1 < f.size(); k++) {
                size_t ids[3] = {0, k, k + 1};
                for (size_t q : ids) push(t[bucket], o.wv[f[q]], n, col, uvs[q].x, uvs[q].y, tint);
            }
            for (size_t k = 0; k < f.size(); k++) {
                push(e, o.wv[f[k]], up, ec);
                push(e, o.wv[f[(k + 1) % f.size()]], up, ec);
            }
        }
    }
    Obj* so = cur();
    if (so && mode == 1 && selMode == 0)
        for (int i = 0; i < (int)so->wv.size(); i++)
            push(p, so->wv[i], up, i == selV ? orange : V3{1.f, 1.f, 1.f});
    V3 c;
    if (tool >= 1 && selCenter(c)) {
        V3 d = axisDir();
        V3 col = axisI == 0 ? V3{1.f, 0.3f, 0.3f} : axisI == 1 ? V3{0.4f, 0.9f, 0.4f}
               : axisI == 2 ? V3{0.3f, 0.5f, 1.f} : V3{1.f, 0.9f, 0.3f};
        push(a, c - d * 4.f, up, col);
        push(a, c + d * 4.f, up, col);
    }
    for (int k = 0; k <= MAXTEX; k++) upload(bTri[k], t[k]);
    upload(bEdge, e); upload(bPts, p); upload(bAxis, a);
    dirty = false;
}

// Penanda versi API: dibaca aplikasi sebelum memuat .so pengganti (harus sama dengan NativeLib.API_TAG).
extern "C" __attribute__((used, visibility("default"))) const char POLYGEN_API_TAG[] = "POLYGEN_API_2";

static void resetCamera() { yaw = 0.7f; pitch = 0.55f; camDist = 5.f; camTarget = {0, 0.5f, 0}; }

// ---------- JNI ----------
extern "C" {

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_init(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    // konteks GL baru (mis. setelah onResume): semua objek GL lama sudah hilang
    bGrid = bEdge = bPts = bAxis = bGz1 = bGz2 = Batch();
    for (auto& b : bTri) b = Batch();
    texGL.clear();
    for (Tex& t : texs) t.dirty = true;
    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, VS));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, FS));
    glLinkProgram(prog);
    uMvp = glGetUniformLocation(prog, "u_mvp");
    uLit = glGetUniformLocation(prog, "u_lit");
    uPt = glGetUniformLocation(prog, "u_pt");
    uTex = glGetUniformLocation(prog, "u_tex");
    uUseTex = glGetUniformLocation(prog, "u_useTex");
    buildGrid();
    if (objs.empty()) addObjInternal(0);
    dirty = true;
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_resize(JNIEnv*, jclass, jint w, jint h) {
    std::lock_guard<std::mutex> lk(M);
    W = w; H = h > 0 ? h : 1;
    glViewport(0, 0, W, H);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_draw(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    glClearColor(0.17f, 0.13f, 0.13f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float cp = cosf(pitch);
    curEye = camTarget + V3{cp * sinf(yaw), sinf(pitch), cp * cosf(yaw)} * camDist;
    camFwd = norm(camTarget - curEye);
    camRight = norm(cross(camFwd, V3{0, 1, 0}));
    camUp = cross(camRight, camFwd);
    curMvp = mul(persp(0.9f, (float)W / H, 0.1f, 100.f), lookAt(curEye, camTarget, {0, 1, 0}));
    glUseProgram(prog);
    glUniformMatrix4fv(uMvp, 1, GL_FALSE, curMvp.m);
    syncTextures();
    if (dirty) rebuild();

    glLineWidth(1.f);
    glUniform1f(uUseTex, 0.f);
    drawB(bGrid, GL_LINES, 0.f, 1.f);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.f, 1.f);
    for (int k = 0; k <= MAXTEX; k++) {
        if (!bTri[k].n) continue;
        if (k > 0 && (size_t)(k - 1) < texGL.size()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texGL[k - 1]);
            glUniform1i(uTex, 0);
            glUniform1f(uUseTex, 1.f);
        } else {
            glUniform1f(uUseTex, 0.f);
        }
        drawB(bTri[k], GL_TRIANGLES, 1.f, 1.f);
    }
    glUniform1f(uUseTex, 0.f);
    glDisable(GL_POLYGON_OFFSET_FILL);

    glLineWidth(2.f);
    drawB(bEdge, GL_LINES, 0.f, 1.f);

    glDisable(GL_DEPTH_TEST);
    drawB(bPts, GL_POINTS, 0.f, 14.f);
    glLineWidth(3.f);
    drawB(bAxis, GL_LINES, 0.f, 1.f);
    drawGizmo();
    glBindVertexArray(0);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_touch(JNIEnv*, jclass, jint act,
        jfloat x, jfloat y, jfloat x2, jfloat y2, jint n) {
    std::lock_guard<std::mutex> lk(M);
    static float lx, ly, sx, sy, pd, pmx, pmy;
    static bool moved, multi, onGizmo;
    switch (act) {
        case 0:
            lx = sx = x; ly = sy = y; moved = false; multi = false;
            dragPushed = false; strokeFirst = true; lastTex = -1;
            onGizmo = false;
            if (mode != 2 && n < 2) {
                int g = gizmoPick(x, y);
                if (g >= 0) { onGizmo = true; if (g != axisI) { axisI = g; bump(); } }
            }
            break;
        case 5:
            pd = hypotf(x2 - x, y2 - y);
            pmx = (x + x2) * 0.5f; pmy = (y + y2) * 0.5f;
            moved = true; multi = true;
            break;
        case 6: lx = x; ly = y; break;
        case 2:
            if (n >= 2) {
                float d = hypotf(x2 - x, y2 - y);
                if (pd > 1.f && d > 1.f) camDist = std::min(40.f, std::max(2.f, camDist * pd / d));
                pd = d;
                if (mode == 2) {  // paint: dua jari = putar kamera
                    float mx = (x + x2) * 0.5f, my = (y + y2) * 0.5f;
                    yaw -= (mx - pmx) * 0.01f;
                    pitch = std::min(1.5f, std::max(-1.5f, pitch + (my - pmy) * 0.01f));
                    pmx = mx; pmy = my;
                }
            } else if (mode == 2) {
                if (!multi) { paintAt(x, y, strokeFirst); strokeFirst = false; }
                lx = x; ly = y;
            } else {
                float dx = x - lx, dy = y - ly;
                lx = x; ly = y;
                if (fabsf(x - sx) + fabsf(y - sy) > 16.f) moved = true;
                V3 c;
                bool canEdit = tool >= 1 && (mode == 0 || tool == 1) && selCenter(c);
                if (canEdit) grab(dx, dy);
                else {
                    yaw -= dx * 0.01f;
                    pitch = std::min(1.5f, std::max(-1.5f, pitch + dy * 0.01f));
                }
            }
            break;
        case 1:
            dragPushed = false; lastTex = -1;
            if (mode == 2) { if (!multi && strokeFirst) paintAt(x, y, true); }
            else if (!moved && !onGizmo) { if (mode == 0) pickObject(x, y); else pickElement(x, y); }
            strokeFirst = true;
            break;
        default: break;
    }
}

JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_version(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    return stateVer;
}

JNIEXPORT jintArray JNICALL Java_com_mini_polygen_NativeLib_state(JNIEnv* env, jclass) {
    std::lock_guard<std::mutex> lk(M);
    jint s[12] = {mode, selMode, tool, axisI, curObj, undoS.empty() ? 0 : 1, redoS.empty() ? 0 : 1,
                  hasClip ? 1 : 0, paintTool, brushSize, brushArgb, activeTex};
    jintArray a = env->NewIntArray(12);
    env->SetIntArrayRegion(a, 0, 12, s);
    return a;
}

JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_objCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    return (jint)objs.size();
}

JNIEXPORT jstring JNICALL Java_com_mini_polygen_NativeLib_objName(JNIEnv* env, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    return env->NewStringUTF(i >= 0 && i < (int)objs.size() ? objs[i].name.c_str() : "");
}

JNIEXPORT jboolean JNICALL Java_com_mini_polygen_NativeLib_objVisible(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    return (i >= 0 && i < (int)objs.size() && objs[i].visible) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_selected(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    return curObj;
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_selectObj(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size()) return;
    curObj = i;
    selV = selF = -1;
    bump();
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_setVisible(JNIEnv*, jclass, jint i, jboolean v) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size()) return;
    pushUndo();
    objs[i].visible = (v == JNI_TRUE);
    bump();
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_deleteObj(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    deleteObjAt(i);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_renameObj(JNIEnv* env, jclass, jint i, jstring s) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size()) return;
    const char* c = env->GetStringUTFChars(s, nullptr);
    string n = c ? c : "";
    env->ReleaseStringUTFChars(s, c);
    if (!n.empty() && n != objs[i].name) { pushUndo(); objs[i].name = n; }
    bump();
}

JNIEXPORT jfloatArray JNICALL Java_com_mini_polygen_NativeLib_getTransform(JNIEnv* env, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    float t[9] = {0, 0, 0, 0, 0, 0, 1, 1, 1};
    if (i >= 0 && i < (int)objs.size()) {
        const Obj& o = objs[i];
        float v[9] = {o.pos.x, o.pos.y, o.pos.z, o.rot.x, o.rot.y, o.rot.z, o.size.x, o.size.y, o.size.z};
        for (int k = 0; k < 9; k++) t[k] = v[k];
    }
    jfloatArray a = env->NewFloatArray(9);
    env->SetFloatArrayRegion(a, 0, 9, t);
    return a;
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_setTransform(JNIEnv* env, jclass, jint i, jfloatArray arr) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size()) return;
    float t[9];
    env->GetFloatArrayRegion(arr, 0, 9, t);
    beginDragEdit();  // scrub angka = satu langkah undo; Java memanggil action(7) untuk memulai langkah baru
    Obj& o = objs[i];
    o.pos = {t[0], t[1], t[2]};
    o.rot = {t[3], t[4], t[5]};
    o.size = {std::max(0.01f, t[6]), std::max(0.01f, t[7]), std::max(0.01f, t[8])};
    bump();
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_addObj(JNIEnv*, jclass, jint type) {
    std::lock_guard<std::mutex> lk(M);
    pushUndo();
    addObjInternal(type);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_addMesh(JNIEnv*, jclass, jint shape, jfloat d, jfloat h) {
    std::lock_guard<std::mutex> lk(M);
    pushUndo();
    addMeshInternal(shape, d, h);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_set(JNIEnv*, jclass, jint kind, jint value) {
    std::lock_guard<std::mutex> lk(M);
    if (kind == 0) { selMode = value; selV = selF = -1; }
    else if (kind == 1) tool = value;
    else if (kind == 2) axisI = value;
    else if (kind == 3) {
        if (value == 1 && !cur()) return;  // mode edit butuh objek terpilih
        mode = value;
        selV = selF = -1;
    }
    else if (kind == 4) paintTool = std::min(3, std::max(0, (int)value));
    else if (kind == 5) brushSize = std::min(64, std::max(1, (int)value));
    else if (kind == 6) brushArgb = value;
    else if (kind == 7) { if (value >= -1 && value < (int)texs.size()) activeTex = value; }
    bump();
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_action(JNIEnv*, jclass, jint code) {
    std::lock_guard<std::mutex> lk(M);
    if (code == 0) extrudeFace();
    else if (code == 1) deleteSel();
    else if (code == 2) doUndo();
    else if (code == 3) doRedo();
    else if (code == 4) duplicateObj();
    else if (code == 5) copyObj();
    else if (code == 6) pasteObj();
    else if (code == 7) dragPushed = false;  // akhiri/mulai gestur edit (untuk menggabung undo)
    else if (code == 20) { resetCamera(); bump(); }
    else if (code >= 10 && code <= 17) setColorIdx(code - 10);
}

// ----- grup -----
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_groupCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    return (jint)groups.size();
}
JNIEXPORT jstring JNICALL Java_com_mini_polygen_NativeLib_groupName(JNIEnv* env, jclass, jint g) {
    std::lock_guard<std::mutex> lk(M);
    return env->NewStringUTF(g >= 0 && g < (int)groups.size() ? groups[g].name.c_str() : "");
}
JNIEXPORT jintArray JNICALL Java_com_mini_polygen_NativeLib_groupInfo(JNIEnv* env, jclass, jint g) {
    std::lock_guard<std::mutex> lk(M);
    jint s[2] = {1, 0};
    if (g >= 0 && g < (int)groups.size()) { s[0] = groups[g].visible ? 1 : 0; s[1] = groups[g].collapsed ? 1 : 0; }
    jintArray a = env->NewIntArray(2);
    env->SetIntArrayRegion(a, 0, 2, s);
    return a;
}
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_objGroup(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    return (i >= 0 && i < (int)objs.size()) ? objs[i].group : -1;
}
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_groupOp(JNIEnv*, jclass, jint op, jint a, jint b) {
    std::lock_guard<std::mutex> lk(M);
    return groupOp(op, a, b);
}
JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_renameGroup(JNIEnv* env, jclass, jint g, jstring s) {
    std::lock_guard<std::mutex> lk(M);
    if (g < 0 || g >= (int)groups.size()) return;
    const char* c = env->GetStringUTFChars(s, nullptr);
    string n = c ? c : "";
    env->ReleaseStringUTFChars(s, c);
    if (!n.empty() && n != groups[g].name) { pushUndo(); groups[g].name = n; bump(); }
}

// ----- tekstur & UV -----
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_texCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    return (jint)texs.size();
}
JNIEXPORT jstring JNICALL Java_com_mini_polygen_NativeLib_texName(JNIEnv* env, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    return env->NewStringUTF(i >= 0 && i < (int)texs.size() ? texs[i].name.c_str() : "");
}
JNIEXPORT jintArray JNICALL Java_com_mini_polygen_NativeLib_texInfo(JNIEnv* env, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    jint s[2] = {0, 0};
    if (i >= 0 && i < (int)texs.size()) { s[0] = texs[i].w; s[1] = texs[i].h; }
    jintArray a = env->NewIntArray(2);
    env->SetIntArrayRegion(a, 0, 2, s);
    return a;
}
JNIEXPORT jintArray JNICALL Java_com_mini_polygen_NativeLib_texPixels(JNIEnv* env, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)texs.size() || !texs[i].px) return env->NewIntArray(0);
    const Tex& t = texs[i];
    vector<jint> a((size_t)t.w * t.h);
    const vector<uint8_t>& p = *t.px;
    for (size_t k = 0; k < a.size(); k++)
        a[k] = (jint)((uint32_t)p[k * 4 + 3] << 24 | (uint32_t)p[k * 4] << 16 | (uint32_t)p[k * 4 + 1] << 8 | p[k * 4 + 2]);
    jintArray r = env->NewIntArray((jsize)a.size());
    env->SetIntArrayRegion(r, 0, (jsize)a.size(), a.data());
    return r;
}
static string jstr(JNIEnv* env, jstring s) {
    const char* c = env->GetStringUTFChars(s, nullptr);
    string n = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return n;
}
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_texCreate(JNIEnv* env, jclass, jstring name, jint w, jint h, jint argb) {
    std::lock_guard<std::mutex> lk(M);
    if (w < 1 || h < 1 || w > 512 || h > 512) return -1;
    vector<uint8_t> px((size_t)w * h * 4);
    for (size_t k = 0; k < (size_t)w * h; k++) {
        px[k * 4] = (argb >> 16) & 0xff; px[k * 4 + 1] = (argb >> 8) & 0xff; px[k * 4 + 2] = argb & 0xff; px[k * 4 + 3] = (argb >> 24) & 0xff;
    }
    return texAdd(jstr(env, name), w, h, px);
}
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_texSet(JNIEnv* env, jclass, jstring name, jint w, jint h, jintArray arr) {
    std::lock_guard<std::mutex> lk(M);
    if (w < 1 || h < 1 || w > 512 || h > 512 || env->GetArrayLength(arr) != w * h) return -1;
    vector<jint> a((size_t)w * h);
    env->GetIntArrayRegion(arr, 0, w * h, a.data());
    vector<uint8_t> px((size_t)w * h * 4);
    for (size_t k = 0; k < a.size(); k++) {
        uint32_t c = (uint32_t)a[k];
        px[k * 4] = (c >> 16) & 0xff; px[k * 4 + 1] = (c >> 8) & 0xff; px[k * 4 + 2] = c & 0xff; px[k * 4 + 3] = (c >> 24) & 0xff;
    }
    return texAdd(jstr(env, name), w, h, px);
}
JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_texDelete(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    texDeleteAt(i);
}
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_objTex(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    return (i >= 0 && i < (int)objs.size()) ? objs[i].tex : -1;
}
JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_setObjTex(JNIEnv*, jclass, jint i, jint t) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size() || t < -1 || t >= (int)texs.size()) return;
    pushUndo();
    objs[i].tex = t;
    bump();
}
JNIEXPORT jint JNICALL Java_com_mini_polygen_NativeLib_faceCount(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    return (i >= 0 && i < (int)objs.size()) ? (jint)objs[i].faces.size() : 0;
}
JNIEXPORT jstring JNICALL Java_com_mini_polygen_NativeLib_faceName(JNIEnv* env, jclass, jint i, jint f) {
    std::lock_guard<std::mutex> lk(M);
    string s = "Sisi " + std::to_string(f + 1);
    if (i >= 0 && i < (int)objs.size() && f >= 0 && f < (int)objs[i].faces.size() && objs[i].faces[f].size() >= 3) {
        V3 n = faceNv(objs[i].verts, objs[i].faces[f]);
        if (n.x > 0.9f) s = "Timur"; else if (n.x < -0.9f) s = "Barat";
        else if (n.y > 0.9f) s = "Atas"; else if (n.y < -0.9f) s = "Bawah";
        else if (n.z > 0.9f) s = "Selatan"; else if (n.z < -0.9f) s = "Utara";
    }
    return env->NewStringUTF(s.c_str());
}
JNIEXPORT jfloatArray JNICALL Java_com_mini_polygen_NativeLib_getFaceUV(JNIEnv* env, jclass, jint i, jint f) {
    std::lock_guard<std::mutex> lk(M);
    float r[4] = {0, 0, 1, 1};
    if (i >= 0 && i < (int)objs.size()) {
        ensureUV(objs[i]);
        if (f >= 0 && f < (int)objs[i].fuv.size()) for (int k = 0; k < 4; k++) r[k] = objs[i].fuv[f][k];
    }
    jfloatArray a = env->NewFloatArray(4);
    env->SetFloatArrayRegion(a, 0, 4, r);
    return a;
}
JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_setFaceUV(JNIEnv* env, jclass, jint i, jint f, jfloatArray arr) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size()) return;
    ensureUV(objs[i]);
    if (f < 0 || f >= (int)objs[i].fuv.size()) return;
    float r[4];
    env->GetFloatArrayRegion(arr, 0, 4, r);
    beginDragEdit();
    for (int k = 0; k < 4; k++) objs[i].fuv[f][k] = std::min(1.f, std::max(0.f, r[k]));
    bump();
}
JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_autoUV(JNIEnv*, jclass, jint i) {
    std::lock_guard<std::mutex> lk(M);
    if (i < 0 || i >= (int)objs.size()) return;
    pushUndo();
    autoUV(objs[i]);
    bump();
}

JNIEXPORT jboolean JNICALL Java_com_mini_polygen_NativeLib_exportObj(JNIEnv* env, jclass, jstring path) {
    std::lock_guard<std::mutex> lk(M);
    const char* p = env->GetStringUTFChars(path, nullptr);
    FILE* f = fopen(p, "w");
    env->ReleaseStringUTFChars(path, p);
    if (!f) return JNI_FALSE;
    refreshWorld();
    int off = 0, vtOff = 0;
    for (Obj& o : objs) {
        if (!objShown(o)) continue;
        ensureUV(o);
        string nm = o.name;
        std::replace(nm.begin(), nm.end(), ' ', '_');
        fprintf(f, "o %s\n", nm.c_str());
        for (const V3& v : o.wv) fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
        bool useUV = o.tex >= 0;
        vector<int> vtStart(o.faces.size(), 0);
        if (useUV) {
            for (size_t i = 0; i < o.faces.size(); i++) {
                vtStart[i] = vtOff;
                FB fb;
                bool ok = faceBasis(o, (int)i, fb);
                for (int vi : o.faces[i]) {
                    V2 uv = ok ? uvOf(o, (int)i, fb, o.verts[vi]) : V2{0, 0};
                    fprintf(f, "vt %f %f\n", uv.x, 1.f - uv.y);
                    vtOff++;
                }
            }
        }
        for (size_t i = 0; i < o.faces.size(); i++) {
            fprintf(f, "f");
            for (size_t k = 0; k < o.faces[i].size(); k++) {
                if (useUV) fprintf(f, " %d/%d", o.faces[i][k] + 1 + off, vtStart[i] + (int)k + 1);
                else fprintf(f, " %d", o.faces[i][k] + 1 + off);
            }
            fprintf(f, "\n");
        }
        off += (int)o.wv.size();
    }
    fclose(f);
    return JNI_TRUE;
}

}  // extern "C"
