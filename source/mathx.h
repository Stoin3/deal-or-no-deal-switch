#pragma once
#include <math.h>
#include <string.h>

static const float PI = 3.14159265358979f;

struct V2 { float x, y; };
struct V3 {
  float x, y, z;
  V3() : x(0), y(0), z(0) {}
  V3(float a, float b, float c) : x(a), y(b), z(c) {}
  V3 operator+(const V3& o) const { return V3(x + o.x, y + o.y, z + o.z); }
  V3 operator-(const V3& o) const { return V3(x - o.x, y - o.y, z - o.z); }
  V3 operator*(float s) const { return V3(x * s, y * s, z * s); }
  V3 operator*(const V3& o) const { return V3(x * o.x, y * o.y, z * o.z); }
  V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float len(V3 a) { return sqrtf(dot(a, a)); }
inline V3 norm(V3 a) { float l = len(a); return l > 1e-8f ? a * (1.0f / l) : V3(0, 1, 0); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline V3 lerp3(V3 a, V3 b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float sat(float v) { return clampf(v, 0, 1); }
inline float smooth(float t) { t = sat(t); return t * t * (3 - 2 * t); }
inline float easeOutBack(float t) { t = sat(t); float c1 = 1.70158f, c3 = c1 + 1; return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2); }
inline float easeOutCubic(float t) { t = sat(t); return 1 - powf(1 - t, 3); }
inline float approach(float cur, float target, float rate, float dt) { return target + (cur - target) * expf(-rate * dt); }
inline V3 approach3(V3 c, V3 t, float rate, float dt) { float k = expf(-rate * dt); return t + (c - t) * k; }

struct M4 {
  float m[16];  // column major
  static M4 id() { M4 r; memset(r.m, 0, sizeof(r.m)); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
  M4 operator*(const M4& b) const {
    M4 r;
    for (int c = 0; c < 4; c++)
      for (int rr = 0; rr < 4; rr++) {
        float s = 0;
        for (int k = 0; k < 4; k++) s += m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
      }
    return r;
  }
  V3 xform(V3 p, float w = 1) const {
    return V3(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12] * w,
              m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13] * w,
              m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14] * w);
  }
  void xform4(V3 p, float out[4]) const {
    for (int i = 0; i < 4; i++) out[i] = m[i] * p.x + m[4 + i] * p.y + m[8 + i] * p.z + m[12 + i];
  }
};
inline M4 translate(V3 t) { M4 r = M4::id(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
inline M4 scale(V3 s) { M4 r = M4::id(); r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
inline M4 rotY(float a) { M4 r = M4::id(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
inline M4 rotX(float a) { M4 r = M4::id(); float c = cosf(a), s = sinf(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
inline M4 rotZ(float a) { M4 r = M4::id(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r; }
inline M4 perspective(float fovy, float aspect, float n, float f) {
  M4 r; memset(r.m, 0, sizeof(r.m));
  float t = 1.0f / tanf(fovy / 2);
  r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
  return r;
}
inline M4 ortho(float l, float r_, float b, float t, float n, float f) {
  M4 r = M4::id();
  r.m[0] = 2 / (r_ - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (f - n);
  r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(f + n) / (f - n);
  return r;
}
inline M4 lookAt(V3 eye, V3 at, V3 up) {
  V3 f = norm(at - eye), s = norm(cross(f, up)), u = cross(s, f);
  M4 r = M4::id();
  r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
  r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
  r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
  r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
  return r;
}

struct Col { float r, g, b, a; };
inline Col rgba(float r, float g, float b, float a = 1) { Col c = {r, g, b, a}; return c; }
inline Col hexc(unsigned h, float a = 1) { return rgba(((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f, a); }
inline Col withA(Col c, float a) { c.a = a; return c; }
inline Col mixc(Col a, Col b, float t) { return rgba(lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t), lerpf(a.a, b.a, t)); }

// small deterministic rng
struct Rng {
  unsigned long long s;
  explicit Rng(unsigned long long seed = 88172645463325252ULL) : s(seed ? seed : 1) {}
  unsigned next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (unsigned)(s >> 11); }
  float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
  float range(float a, float b) { return a + (b - a) * f(); }
  int irange(int n) { return n > 0 ? (int)(next() % (unsigned)n) : 0; }
};
