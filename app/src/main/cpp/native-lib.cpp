// Polygen Modelling: mesin modelling polygon dari nol (tanpa GLM/library lain).
#include <jni.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
using std::vector;

// ---------- matematika ----------
struct V3 { float x, y, z; };
static V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static V3 norm(V3 a) { float l = sqrtf(dot(a, a)); return l > 1e-8f ? a * (1.f / l) : V3{0, 1, 0}; }

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

// ---------- state ----------
static int W = 1, H = 1;
static vector<V3> verts;
static vector<vector<int>> faces;
static int selMode = 0;  // 0 vertex, 1 face
static int selV = -1, selF = -1;
static int tool = 0;     // 0 orbit, 1 geser
static int axisI = 1;    // 0 X, 1 Y, 2 Z, 3 normal
static int objCount = 0;
static float yaw = 0.7f, pitch = 0.5f, camDist = 7.f;
static V3 camTarget = {0, 0.5f, 0};
static M4 curMvp;
static V3 curEye = {0, 0, 0};
static bool dirty = true;
static GLuint prog;
static GLint uMvp, uLit, uPt;

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

// ---------- mesh ----------
static V3 faceN(const vector<int>& f) {  // metode Newell
    V3 n = {0, 0, 0};
    for (size_t i = 0; i < f.size(); i++) {
        V3 a = verts[f[i]], b = verts[f[(i + 1) % f.size()]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return norm(n);
}

static bool selCenter(V3& c) {
    if (selMode == 0 && selV >= 0 && selV < (int)verts.size()) { c = verts[selV]; return true; }
    if (selMode == 1 && selF >= 0 && selF < (int)faces.size()) {
        V3 s = {0, 0, 0};
        for (int i : faces[selF]) s = s + verts[i];
        c = s * (1.f / faces[selF].size());
        return true;
    }
    return false;
}

static V3 axisDir() {
    if (axisI == 0) return {1, 0, 0};
    if (axisI == 2) return {0, 0, 1};
    if (axisI == 3 && selMode == 1 && selF >= 0 && selF < (int)faces.size()) return faceN(faces[selF]);
    return {0, 1, 0};
}

static void addShape(int type) {
    dirty = true;
    int b = (int)verts.size();
    float ox = (objCount++) * 2.5f;
    if (type == 0) {
        const float c[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
        for (auto& p : c) verts.push_back({p[0] * 0.5f + ox, p[1] * 0.5f + 0.5f, p[2] * 0.5f});
        const int f[6][4] = {{4,5,6,7},{1,0,3,2},{5,1,2,6},{0,4,7,3},{7,6,2,3},{0,1,5,4}};
        for (auto& q : f) faces.push_back({b + q[0], b + q[1], b + q[2], b + q[3]});
    } else {
        verts.push_back({-1 + ox, 0, 1}); verts.push_back({1 + ox, 0, 1});
        verts.push_back({1 + ox, 0, -1}); verts.push_back({-1 + ox, 0, -1});
        faces.push_back({b, b + 1, b + 2, b + 3});
    }
}

static void extrudeFace() {
    dirty = true;
    if (selMode != 1 || selF < 0 || selF >= (int)faces.size()) return;
    vector<int> f = faces[selF];
    V3 n = faceN(f);
    vector<int> nf;
    for (int i : f) { verts.push_back(verts[i] + n * 0.5f); nf.push_back((int)verts.size() - 1); }
    size_t k = f.size();
    for (size_t i = 0; i < k; i++)
        faces.push_back({f[i], f[(i + 1) % k], nf[(i + 1) % k], nf[i]});
    faces[selF] = nf;
}

static void deleteSel() {
    dirty = true;
    if (selMode == 1 && selF >= 0 && selF < (int)faces.size()) {
        faces.erase(faces.begin() + selF);
        selF = -1;
    } else if (selMode == 0 && selV >= 0) {
        int v = selV;
        faces.erase(std::remove_if(faces.begin(), faces.end(), [v](const vector<int>& f) {
            return std::find(f.begin(), f.end(), v) != f.end();
        }), faces.end());
        selV = -1;
    }
}

static void grab(float dx, float dy) {
    V3 c;
    if (!selCenter(c)) return;
    V3 d = axisDir();
    float x0, y0, x1, y1;
    if (!toScreen(c, x0, y0) || !toScreen(c + d, x1, y1)) return;
    float ax = x1 - x0, ay = y1 - y0, l2 = ax * ax + ay * ay;
    if (l2 < 4.f) return;  // sumbu menghadap kamera
    float t = (dx * ax + dy * ay) / l2;
    dirty = true;
    if (selMode == 0) verts[selV] = verts[selV] + d * t;
    else for (int i : faces[selF]) verts[i] = verts[i] + d * t;
}

// sinar eye->p memotong permukaan lain sebelum mencapai p?
static bool occluded(V3 p, int skipFace, int skipVert) {
    V3 d = p - curEye;
    for (int i = 0; i < (int)faces.size(); i++) {
        if (i == skipFace) continue;
        const vector<int>& f = faces[i];
        if (skipVert >= 0 && std::find(f.begin(), f.end(), skipVert) != f.end()) continue;
        for (size_t k = 1; k + 1 < f.size(); k++) {
            V3 a = verts[f[0]], e1 = verts[f[k]] - a, e2 = verts[f[k + 1]] - a;
            V3 h = cross(d, e2);
            float det = dot(e1, h);
            if (fabsf(det) < 1e-9f) continue;
            float inv = 1.f / det;
            V3 sv = curEye - a;
            float u = inv * dot(sv, h);
            if (u < 0.f || u > 1.f) continue;
            V3 q = cross(sv, e1);
            float v = inv * dot(d, q);
            if (v < 0.f || u + v > 1.f) continue;
            float t = inv * dot(e2, q);
            if (t > 0.001f && t < 0.995f) return true;
        }
    }
    return false;
}

static void pick(float x, float y) {
    struct Cand { float d2; int i; V3 p; };
    vector<Cand> cs;
    int n = selMode == 0 ? (int)verts.size() : (int)faces.size();
    for (int i = 0; i < n; i++) {
        V3 p;
        if (selMode == 0) p = verts[i];
        else {
            V3 s = {0, 0, 0};
            for (int v : faces[i]) s = s + verts[v];
            p = s * (1.f / faces[i].size());
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
    dirty = true;
}

// ---------- render (OpenGL ES 3.0, VAO + VBO) ----------
static GLuint compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

static void push(vector<float>& b, V3 p, V3 n, float r, float g, float bl) {
    float a[9] = {p.x, p.y, p.z, n.x, n.y, n.z, r, g, bl};
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

static void drawB(const Batch& b, GLenum mode, float lit, float pt) {
    if (!b.n) return;
    glBindVertexArray(b.vao);
    glUniform1f(uLit, lit);
    glUniform1f(uPt, pt);
    glDrawArrays(mode, 0, b.n);
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
        push(g, {(float)i, 0, -6}, up, c ? 0.3f : 0.28f, c ? 0.5f : 0.3f, c ? 1.f : 0.28f);
        push(g, {(float)i, 0, 6}, up, c ? 0.3f : 0.28f, c ? 0.5f : 0.3f, c ? 1.f : 0.28f);
        push(g, {-6, 0, (float)i}, up, c ? 1.f : 0.28f, c ? 0.3f : 0.28f, 0.3f);
        push(g, {6, 0, (float)i}, up, c ? 1.f : 0.28f, c ? 0.3f : 0.28f, 0.3f);
    }
    upload(bGrid, g);
}

// dibangun ulang hanya saat mesh / seleksi / tool berubah
static void rebuild() {
    const V3 up = {0, 1, 0};
    vector<float> t, e, p, a;
    for (int i = 0; i < (int)faces.size(); i++) {
        const vector<int>& f = faces[i];
        if (f.size() < 3) continue;
        V3 n = faceN(f);
        bool s = (selMode == 1 && i == selF);
        float r = s ? 1.f : 0.72f, g = s ? 0.6f : 0.74f, b = s ? 0.1f : 0.78f;
        for (size_t k = 1; k + 1 < f.size(); k++) {
            push(t, verts[f[0]], n, r, g, b);
            push(t, verts[f[k]], n, r, g, b);
            push(t, verts[f[k + 1]], n, r, g, b);
        }
        for (size_t k = 0; k < f.size(); k++) {
            push(e, verts[f[k]], up, 0.05f, 0.05f, 0.07f);
            push(e, verts[f[(k + 1) % f.size()]], up, 0.05f, 0.05f, 0.07f);
        }
    }
    if (selMode == 0)
        for (int i = 0; i < (int)verts.size(); i++)
            if (i == selV) push(p, verts[i], up, 1.f, 0.6f, 0.1f);
            else push(p, verts[i], up, 1.f, 1.f, 1.f);
    V3 c;
    if (tool == 1 && selCenter(c)) {
        V3 d = axisDir();
        float cr = axisI == 0 ? 1.f : axisI == 1 ? 0.4f : axisI == 2 ? 0.3f : 1.f;
        float cg = axisI == 0 ? 0.3f : axisI == 1 ? 0.9f : axisI == 2 ? 0.5f : 0.9f;
        float cb = axisI == 0 ? 0.3f : axisI == 1 ? 0.4f : axisI == 2 ? 1.f : 0.3f;
        push(a, c - d * 4.f, up, cr, cg, cb);
        push(a, c + d * 4.f, up, cr, cg, cb);
    }
    upload(bTri, t); upload(bEdge, e); upload(bPts, p); upload(bAxis, a);
    dirty = false;
}

extern "C" {

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_init(JNIEnv*, jclass) {
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
    if (verts.empty()) addShape(0);
    dirty = true;
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_resize(JNIEnv*, jclass, jint w, jint h) {
    W = w; H = h > 0 ? h : 1;
    glViewport(0, 0, W, H);
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_draw(JNIEnv*, jclass) {
    glClearColor(0.13f, 0.14f, 0.17f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float cp = cosf(pitch);
    curEye = camTarget + V3{cp * sinf(yaw), sinf(pitch), cp * cosf(yaw)} * camDist;
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
                if (tool == 1 && selCenter(c)) grab(dx, dy);
                else {
                    yaw -= dx * 0.01f;
                    pitch = std::min(1.5f, std::max(-1.5f, pitch + dy * 0.01f));
                }
            }
            break;
        case 1: if (!moved) pick(x, y); break;
        default: break;
    }
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_add(JNIEnv*, jclass, jint type) { addShape(type); }

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_set(JNIEnv*, jclass, jint kind, jint value) {
    dirty = true;
    if (kind == 0) { selMode = value; selV = selF = -1; }
    else if (kind == 1) tool = value;
    else if (kind == 2) axisI = value;
}

JNIEXPORT void JNICALL Java_com_mini_polygen_NativeLib_action(JNIEnv*, jclass, jint code) {
    if (code == 0) extrudeFace();
    else if (code == 1) deleteSel();
}

JNIEXPORT jboolean JNICALL Java_com_mini_polygen_NativeLib_exportObj(JNIEnv* env, jclass, jstring path) {
    const char* p = env->GetStringUTFChars(path, nullptr);
    FILE* f = fopen(p, "w");
    env->ReleaseStringUTFChars(path, p);
    if (!f) return JNI_FALSE;
    for (const V3& v : verts) fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
    for (const auto& fc : faces) {
        fprintf(f, "f");
        for (int i : fc) fprintf(f, " %d", i + 1);
        fprintf(f, "\n");
    }
    fclose(f);
    return JNI_TRUE;
}

}  // extern "C"
