// Polygen Modelling: mesin 3D modelling dari nol (tanpa GLM/library lain).
// Banyak objek (kubus/bidang), tiap objek punya posisi/rotasi/ukuran dan mesh sendiri.
// Mode objek (gaya Blockbench) dan mode edit mesh (gaya Blender).
#include <jni.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
using std::string;
using std::vector;

// ---------- matematika ----------
struct V3 { float x, y, z; };
static V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static V3 norm(V3 a) { float l = sqrtf(dot(a, a)); return l > 1e-8f ? a * (1.f / l) : V3{0, 1, 0}; }

static const float D2R = 0.017453292f;
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
struct Obj {
    string name;
    vector<V3> verts, wv;  // lokal, dunia
    vector<vector<int>> faces;
    V3 pos{0, 0, 0}, rot{0, 0, 0}, size{1, 1, 1};  // rot dalam derajat (XYZ)
    bool visible = true;
    V3 color{0.72f, 0.74f, 0.78f};
};

static std::mutex M;
static int W = 1, H = 1;
static vector<Obj> objs;
static int curObj = -1;   // objek terpilih
static int mode = 0;      // 0 objek, 1 edit
static int selMode = 0;   // edit: 0 vertex, 1 face
static int selV = -1, selF = -1;
static int tool = 0;      // 0 kamera, 1 geser, 2 ukuran, 3 putar
static int axisI = 1;     // 0 X, 1 Y, 2 Z, 3 N (normal face / ukuran seragam)
static int nameCount[2] = {0, 0};
static int stateVer = 0;
static bool dirty = true;
static float yaw = 0.7f, pitch = 0.55f, camDist = 5.f;
static V3 camTarget = {0, 0.5f, 0};
static M4 curMvp;
static V3 curEye = {0, 0, 0}, camFwd = {0, 0, -1}, camRight = {1, 0, 0}, camUp = {0, 1, 0};
static GLuint prog;
static GLint uMvp, uLit, uPt;

static void bump() { stateVer++; dirty = true; }
static Obj* cur() { return (curObj >= 0 && curObj < (int)objs.size()) ? &objs[curObj] : nullptr; }

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

static bool rayTri(V3 o, V3 d, V3 a, V3 b, V3 c, float& t) {
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
    return true;
}

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

static bool selCenter(V3& c) {
    Obj* o = cur();
    if (!o) return false;
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

static void addObjInternal(int type) {
    Obj o;
    int ti = type == 0 ? 0 : 1;
    o.name = string(ti == 0 ? "Kubus " : "Bidang ") + std::to_string(++nameCount[ti]);
    if (type == 0) {
        const float c[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
        for (auto& p : c) o.verts.push_back({p[0] * 0.5f, p[1] * 0.5f, p[2] * 0.5f});
        const int f[6][4] = {{4,5,6,7},{1,0,3,2},{5,1,2,6},{0,4,7,3},{7,6,2,3},{0,1,5,4}};
        for (auto& q : f) o.faces.push_back({q[0], q[1], q[2], q[3]});
        o.pos = {(float)objs.size() * 1.5f, 0.5f, 0};
    } else {
        o.verts = {{-0.5f, 0, 0.5f}, {0.5f, 0, 0.5f}, {0.5f, 0, -0.5f}, {-0.5f, 0, -0.5f}};
        o.faces.push_back({0, 1, 2, 3});
        o.pos = {(float)objs.size() * 1.5f, 0, 0};
    }
    objs.push_back(o);
    curObj = (int)objs.size() - 1;
    selV = selF = -1;
    bump();
}

static void extrudeFace() {
    Obj* o = cur();
    if (!o || mode != 1 || selMode != 1 || selF < 0 || selF >= (int)o->faces.size()) return;
    vector<int> f = o->faces[selF];
    V3 n = faceNv(o->verts, f);
    vector<int> nf;
    for (int i : f) { o->verts.push_back(o->verts[i] + n * 0.5f); nf.push_back((int)o->verts.size() - 1); }
    size_t k = f.size();
    for (size_t i = 0; i < k; i++)
        o->faces.push_back({f[i], f[(i + 1) % k], nf[(i + 1) % k], nf[i]});
    o->faces[selF] = nf;
    bump();
}

static void deleteObjAt(int i) {
    if (i < 0 || i >= (int)objs.size()) return;
    objs.erase(objs.begin() + i);
    if (curObj == i) curObj = -1;
    else if (curObj > i) curObj--;
    if (curObj < 0) mode = 0;
    selV = selF = -1;
    bump();
}

static void deleteSel() {
    if (mode == 0) { deleteObjAt(curObj); return; }
    Obj* o = cur();
    if (!o) return;
    if (selMode == 1 && selF >= 0 && selF < (int)o->faces.size()) {
        o->faces.erase(o->faces.begin() + selF);
        selF = -1;
    } else if (selMode == 0 && selV >= 0) {
        int v = selV;
        o->faces.erase(std::remove_if(o->faces.begin(), o->faces.end(), [v](const vector<int>& f) {
            return std::find(f.begin(), f.end(), v) != f.end();
        }), o->faces.end());
        selV = -1;
    }
    bump();
}

// ---------- interaksi ----------
static void grab(float dx, float dy) {
    Obj* o = cur();
    if (!o) return;
    if (mode == 0 && tool == 3) {  // putar
        float a = dx * 0.5f;
        if (axisI == 0) o->rot.x += a;
        else if (axisI == 2) o->rot.z += a;
        else o->rot.y += a;
        bump();
        return;
    }
    if (mode == 0 && tool == 2 && axisI == 3) {  // ukuran seragam
        float s = 1.f + (dx - dy) * 0.005f;
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
        if (!o.visible) continue;
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
        if (!o.visible) continue;
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

static void push(vector<float>& b, V3 p, V3 n, V3 c) {
    float a[9] = {p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z};
    b.insert(b.end(), a, a + 9);
}

struct Batch { GLuint vao = 0, vbo = 0; GLsizei n = 0; };
static Batch bGrid, bTri, bEdge, bPts, bAxis;

static void upload(Batch& b, const vector<float>& d) {
    if (!b.vao) {
        glGenVertexArrays(1, &b.vao);
        glGenBuffers(1, &b.vbo);
        glBindVertexArray(b.vao);
        glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, 36, (const void*)(size_t)(i * 12));
        }
    }
    glBindVertexArray(b.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(d.size() * sizeof(float)), d.empty() ? nullptr : d.data(), GL_DYNAMIC_DRAW);
    b.n = (GLsizei)(d.size() / 9);
}

static void drawB(const Batch& b, GLenum m, float lit, float pt) {
    if (!b.n) return;
    glBindVertexArray(b.vao);
    glUniform1f(uLit, lit);
    glUniform1f(uPt, pt);
    glDrawArrays(m, 0, b.n);
}

static const char* VS = R"(#version 300 es
uniform mat4 u_mvp;
uniform float u_pt;
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec3 a_col;
out vec3 v_nrm;
out vec3 v_col;
void main() {
  v_nrm = a_nrm;
  v_col = a_col;
  gl_Position = u_mvp * vec4(a_pos, 1.0);
  gl_PointSize = u_pt;
}
)";
static const char* FS = R"(#version 300 es
precision mediump float;
uniform float u_lit;
in vec3 v_nrm;
in vec3 v_col;
out vec4 frag;
void main() {
  float l = 1.0;
  if (u_lit > 0.5) {
    vec3 L = normalize(vec3(0.4, 0.9, 0.5));
    l = 0.35 + 0.65 * abs(dot(normalize(v_nrm), L));
  }
  frag = vec4(v_col * l, 1.0);
}
)";

static void buildGrid() {
    const V3 up = {0, 1, 0};
    vector<float> g;
    for (int i = -6; i <= 6; i++) {
        bool c = (i == 0);
        V3 cz = c ? V3{0.3f, 0.5f, 1.f} : V3{0.28f, 0.28f, 0.3f};
        V3 cx = c ? V3{1.f, 0.3f, 0.3f} : V3{0.28f, 0.28f, 0.3f};
        push(g, {(float)i, 0, -6}, up, cz); push(g, {(float)i, 0, 6}, up, cz);
        push(g, {-6, 0, (float)i}, up, cx); push(g, {6, 0, (float)i}, up, cx);
    }
    upload(bGrid, g);
}

// dibangun ulang hanya saat objek / seleksi / tool berubah
static void rebuild() {
    refreshWorld();
    const V3 up = {0, 1, 0};
    const V3 orange = {1.f, 0.6f, 0.1f};
    vector<float> t, e, p, a;
    for (int oi = 0; oi < (int)objs.size(); oi++) {
        const Obj& o = objs[oi];
        if (!o.visible) continue;
        bool selO = (oi == curObj);
        V3 base = (selO && mode == 0) ? V3{0.55f, 0.72f, 1.f} : o.color;
        V3 ec = (selO && mode == 0) ? orange : V3{0.05f, 0.05f, 0.07f};
        for (int i = 0; i < (int)o.faces.size(); i++) {
            const vector<int>& f = o.faces[i];
            if (f.size() < 3) continue;
            V3 n = faceNv(o.wv, f);
            V3 col = (selO && mode == 1 && selMode == 1 && i == selF) ? orange : base;
            for (size_t k = 1; k + 1 < f.size(); k++) {
                push(t, o.wv[f[0]], n, col);
                push(t, o.wv[f[k]], n, col);
                push(t, o.wv[f[k + 1]], n, col);
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
    upload(bTri, t); upload(bEdge, e); upload(bPts, p); upload(bAxis, a);
    dirty = false;
}

// Penanda versi API: dibaca aplikasi sebelum memuat .so pengganti (harus sama dengan NativeLib.API_TAG).
extern "C" __attribute__((used, visibility("default"))) const char POLYGEN_API_TAG[] = "POLYGEN_API_1";

// ---------- JNI ----------
extern "C" {

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_init(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(M);
    // konteks GL baru (mis. setelah onResume): semua objek GL lama sudah hilang
    bGrid = bTri = bEdge = bPts = bAxis = Batch();
    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, VS));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, FS));
    glLinkProgram(prog);
    uMvp = glGetUniformLocation(prog, "u_mvp");
    uLit = glGetUniformLocation(prog, "u_lit");
    uPt = glGetUniformLocation(prog, "u_pt");
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
    glClearColor(0.13f, 0.14f, 0.17f, 1.f);
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
    if (dirty) rebuild();

    glLineWidth(1.f);
    drawB(bGrid, GL_LINES, 0.f, 1.f);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.f, 1.f);
    drawB(bTri, GL_TRIANGLES, 1.f, 1.f);
    glDisable(GL_POLYGON_OFFSET_FILL);

    glLineWidth(2.f);
    drawB(bEdge, GL_LINES, 0.f, 1.f);

    glDisable(GL_DEPTH_TEST);
    drawB(bPts, GL_POINTS, 0.f, 14.f);
    glLineWidth(3.f);
    drawB(bAxis, GL_LINES, 0.f, 1.f);
    glBindVertexArray(0);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_touch(JNIEnv*, jclass, jint act,
        jfloat x, jfloat y, jfloat x2, jfloat y2, jint n) {
    std::lock_guard<std::mutex> lk(M);
    static float lx, ly, sx, sy, pd;
    static bool moved;
    switch (act) {
        case 0: lx = sx = x; ly = sy = y; moved = false; break;
        case 5: pd = hypotf(x2 - x, y2 - y); moved = true; break;
        case 6: lx = x; ly = y; break;
        case 2:
            if (n >= 2) {
                float d = hypotf(x2 - x, y2 - y);
                if (pd > 1.f && d > 1.f) camDist = std::min(40.f, std::max(2.f, camDist * pd / d));
                pd = d;
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
            if (!moved) { if (mode == 0) pickObject(x, y); else pickElement(x, y); }
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
    jint s[5] = {mode, selMode, tool, axisI, curObj};
    jintArray a = env->NewIntArray(5);
    env->SetIntArrayRegion(a, 0, 5, s);
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
    if (!n.empty()) objs[i].name = n;
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
    Obj& o = objs[i];
    o.pos = {t[0], t[1], t[2]};
    o.rot = {t[3], t[4], t[5]};
    o.size = {std::max(0.01f, t[6]), std::max(0.01f, t[7]), std::max(0.01f, t[8])};
    bump();
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_addObj(JNIEnv*, jclass, jint type) {
    std::lock_guard<std::mutex> lk(M);
    addObjInternal(type);
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
    bump();
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_action(JNIEnv*, jclass, jint code) {
    std::lock_guard<std::mutex> lk(M);
    if (code == 0) extrudeFace();
    else if (code == 1) deleteSel();
}

JNIEXPORT jboolean JNICALL Java_com_mini_polygen_NativeLib_exportObj(JNIEnv* env, jclass, jstring path) {
    std::lock_guard<std::mutex> lk(M);
    const char* p = env->GetStringUTFChars(path, nullptr);
    FILE* f = fopen(p, "w");
    env->ReleaseStringUTFChars(path, p);
    if (!f) return JNI_FALSE;
    refreshWorld();
    int off = 0;
    for (const Obj& o : objs) {
        if (!o.visible) continue;
        string nm = o.name;
        std::replace(nm.begin(), nm.end(), ' ', '_');
        fprintf(f, "o %s\n", nm.c_str());
        for (const V3& v : o.wv) fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
        for (const auto& fc : o.faces) {
            fprintf(f, "f");
            for (int i : fc) fprintf(f, " %d", i + 1 + off);
            fprintf(f, "\n");
        }
        off += (int)o.wv.size();
    }
    fclose(f);
    return JNI_TRUE;
}

}  // extern "C"
