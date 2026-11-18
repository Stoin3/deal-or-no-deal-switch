#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include "platform.h"
#include "../include/stb_truetype.h"

// ------------------------------------------------------------------ shaders
static const char* VS_LIT = R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec2 aUV;
uniform mat4 uModel; uniform mat4 uVP;
out vec3 vW; out vec3 vN; out vec2 vUV;
void main(){ vec4 w = uModel*vec4(aPos,1.0); vW=w.xyz; vN=mat3(uModel)*aNrm; vUV=aUV; gl_Position=uVP*w; }
)";

static const char* FS_COMMON = R"(
uniform vec3 uCam; uniform vec3 uTint; uniform float uTime; uniform vec3 uSpotPos; uniform vec3 uSpotCol;
vec3 env(vec3 r){
  float y=r.y;
  vec3 c=mix(vec3(0.03,0.02,0.07), vec3(0.22,0.13,0.40), smoothstep(-0.3,0.7,y));
  float a=atan(r.x,r.z);
  c += pow(max(0.0, sin(a*5.0+0.5)),40.0)*smoothstep(-0.1,0.4,y)*vec3(1.5,1.4,1.3);
  c += pow(max(0.0, sin(a*11.0+1.3)),60.0)*smoothstep(-0.4,0.1,y)*(1.0-smoothstep(0.1,0.5,y))*vec3(0.6,0.8,1.6);
  c += smoothstep(0.78,1.0,y)*vec3(1.7,1.6,1.5);
  c += uTint*0.12;
  return c;
}
vec3 tonemap(vec3 c){ c = c*(2.51*c+0.03)/(c*(2.43*c+0.59)+0.14); return pow(clamp(c,0.0,1.0), vec3(1.0/2.2)); }
)";

static const char* FS_LIT = R"(
in vec3 vW; in vec3 vN; in vec2 vUV;
uniform vec4 uColor; uniform float uMetal; uniform float uGloss; uniform vec3 uEmis; uniform float uReflect;
out vec4 frag;
void main(){
  vec3 n=normalize(vN); if(!gl_FrontFacing) n=-n;
  vec3 v=normalize(uCam-vW);
  vec3 base=pow(uColor.rgb, vec3(2.2));
  vec3 Ls[3]; vec3 Cs[3];
  Ls[0]=normalize(vec3(-0.35,0.9,0.7)); Cs[0]=vec3(1.0,0.95,0.88)*1.25;
  Ls[1]=normalize(vec3(0.8,0.45,0.35)); Cs[1]=vec3(0.45,0.55,1.0)*0.55;
  Ls[2]=normalize(vec3(0.0,0.5,-1.0)); Cs[2]=(vec3(0.9,0.35,1.0)*0.8+uTint*0.5);
  vec3 diff=vec3(0.10,0.09,0.17)+uTint*0.05; vec3 spec=vec3(0.0);
  float sp=exp2(uGloss*10.0+2.0);
  for(int i=0;i<3;i++){
    float nd=max(dot(n,Ls[i]),0.0);
    diff+=Cs[i]*nd;
    spec+=Cs[i]*pow(max(dot(n,normalize(Ls[i]+v)),0.0),sp)*nd;
  }
  vec3 L=uSpotPos-vW; float d=length(L); L/=max(d,0.001); float att=1.0/(1.0+0.25*d*d);
  float nd=max(dot(n,L),0.0);
  diff+=uSpotCol*nd*att; spec+=uSpotCol*pow(max(dot(n,normalize(L+v)),0.0),sp)*att*nd;
  vec3 r=reflect(-v,n);
  float fres=pow(1.0-max(dot(n,v),0.0),5.0);
  vec3 f0=mix(vec3(0.04), base, uMetal);
  vec3 col = base*diff*(1.0-uMetal) + spec*f0*(1.0+uGloss*2.0);
  col += env(r)*(f0+(1.0-f0)*fres)*(0.25+0.75*uGloss);
  col += uEmis;
  float a=uColor.a;
  if(uReflect>0.0){ float f=clamp(1.0+vW.y*0.55,0.0,1.0); col*=uReflect*f*f; }
  frag=vec4(tonemap(col),a);
}
)";

static const char* FS_FLOOR = R"(
in vec3 vW; in vec3 vN; in vec2 vUV;
out vec4 frag;
void main(){
  vec3 v=normalize(uCam-vW);
  vec2 p=vW.xz-vec2(0.0,2.0);
  float r=length(p);
  vec3 c=vec3(0.015,0.012,0.03);
  // tiles
  vec2 g=abs(fract(vW.xz*0.5)-0.5);
  float line=smoothstep(0.485,0.5,max(g.x,g.y));
  c+=line*vec3(0.05,0.04,0.12);
  // rings under the stands
  for(int i=0;i<3;i++){ float rr=5.0+float(i)*0.9; c+=exp(-pow((r-rr)*9.0,2.0))*vec3(0.15,0.25,0.7)*(0.6+0.4*sin(uTime*2.0+float(i))); }
  // center glow + spot
  c+=exp(-dot(vW.xz-vec2(0.0,1.2),vW.xz-vec2(0.0,1.2))*0.35)*vec3(0.35,0.18,0.6);
  vec3 L=uSpotPos-vW; float d=length(L);
  c+=uSpotCol*0.25/(1.0+0.3*d*d);
  c+=uTint*0.04*exp(-r*0.1);
  // sweeping light pools
  for(int i=0;i<4;i++){
    float fi=float(i);
    vec2 lp=vec2(sin(uTime*0.4+fi*1.7)*5.0, 1.0+cos(uTime*0.33+fi*2.3)*4.0);
    c+=exp(-dot(vW.xz-lp,vW.xz-lp)*0.8)*vec3(0.25,0.2,0.45)*0.6;
  }
  float fres=pow(1.0-max(v.y,0.0),3.0);
  float a=mix(0.72,0.45,fres);
  frag=vec4(tonemap(c),a);
}
)";

static const char* FS_BG = R"(
in vec3 vW; in vec3 vN; in vec2 vUV;
uniform float uEnergy;
out vec4 frag;
void main(){
  vec2 uv=vUV;
  vec3 c=mix(vec3(0.02,0.01,0.05), vec3(0.07,0.03,0.14), uv.y);
  float cols=48.0;
  float cell=floor(uv.x*cols);
  float fx=fract(uv.x*cols);
  float colLine=smoothstep(0.40,0.5,fx)-smoothstep(0.5,0.60,fx);
  float pulse=0.5+0.5*sin(uTime*2.2-uv.y*7.0+cell*0.9);
  c+=colLine*pulse*mix(vec3(0.25,0.45,1.0),uTint,0.35)*uEnergy*smoothstep(0.1,0.9,uv.y)*0.8;
  float strip=exp(-pow((uv.y-0.13)*50.0,2.0))+exp(-pow((uv.y-0.88)*50.0,2.0));
  float chase=0.6+0.4*sin(uv.x*120.0-uTime*6.0);
  c+=strip*chase*mix(vec3(0.7,0.3,1.0),uTint,0.5)*uEnergy*1.4;
  // dotted bulbs
  vec2 q=vec2(fract(uv.x*160.0)-0.5, (uv.y-0.5)*20.0);
  float bulb=exp(-dot(q,q)*30.0)*(0.5+0.5*sin(uTime*3.0+floor(uv.x*160.0)*1.3));
  c+=bulb*vec3(1.0,0.8,0.4)*0.5*uEnergy;
  frag=vec4(tonemap(c),1.0);
}
)";

static const char* FS_BEAM = R"(
in vec3 vW; in vec3 vN; in vec2 vUV;
uniform vec4 uColor;
out vec4 frag;
void main(){
  vec3 v=normalize(uCam-vW);
  vec3 n=normalize(vN);
  float edge=pow(abs(dot(n,v)),2.0);
  float a=edge*(1.0-vUV.y*0.85)*uColor.a;
  frag=vec4(uColor.rgb*a,1.0);
}
)";

static const char* VS_TEXT3D = R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=2) in vec2 aUV;
uniform mat4 uMVP; out vec2 vUV;
void main(){ vUV=aUV; gl_Position=uMVP*vec4(aPos,1.0); }
)";
static const char* FS_TEXT3D = R"(#version 330 core
in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor; out vec4 frag;
void main(){ float a=texture(uTex,vUV).r; frag=vec4(uColor.rgb,uColor.a*a); }
)";

static const char* VS_UI = R"(#version 330 core
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aCol;
layout(location=3) in vec4 aP; layout(location=4) in float aMode;
uniform mat4 uProj;
out vec2 vUV; out vec4 vCol; out vec4 vP; out float vMode;
void main(){ vUV=aUV; vCol=aCol; vP=aP; vMode=aMode; gl_Position=uProj*vec4(aPos,0.0,1.0); }
)";
static const char* FS_UI = R"(#version 330 core
in vec2 vUV; in vec4 vCol; in vec4 vP; in float vMode;
uniform sampler2D uTex;
out vec4 frag;
float sdRound(vec2 p, vec2 b, float r){ vec2 q=abs(p)-b+r; return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r; }
void main(){
  if(vMode<0.5){ float d=sdRound(vUV,vP.xy,vP.z); frag=vec4(vCol.rgb,vCol.a*clamp(0.5-d,0.0,1.0)); }
  else if(vMode<1.5){ float a=texture(uTex,vUV).r; frag=vec4(vCol.rgb,vCol.a*a); }
  else if(vMode<2.5){ float d=sdRound(vUV,vP.xy,vP.z); float a=exp(-max(d,0.0)/max(vP.w,1.0)*3.0); frag=vec4(vCol.rgb,vCol.a*a); }
  else { float d=abs(sdRound(vUV,vP.xy,vP.z))-vP.w*0.5; frag=vec4(vCol.rgb,vCol.a*clamp(0.5-d,0.0,1.0)); }
}
)";

struct Prog {
  GLuint id = 0;
  std::map<std::string, GLint> u;
  GLint loc(const char* n) {
    auto it = u.find(n);
    if (it != u.end()) return it->second;
    GLint l = glGetUniformLocation(id, n);
    u[n] = l;
    return l;
  }
};

static GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log); plat_log("shader error: %s", log); }
  return s;
}

static Prog makeProg(const char* vs, const char* fs, bool common) {
  Prog p;
  std::string f = std::string("#version 330 core\n") + (common ? FS_COMMON : "") + fs;
  if (!common) f = fs;
  GLuint v = compile(GL_VERTEX_SHADER, vs), fr = compile(GL_FRAGMENT_SHADER, f.c_str());
  p.id = glCreateProgram();
  glAttachShader(p.id, v);
  glAttachShader(p.id, fr);
  glLinkProgram(p.id);
  GLint ok = 0;
  glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
  if (!ok) { char log[2048]; glGetProgramInfoLog(p.id, 2048, nullptr, log); plat_log("link error: %s", log); }
  glDeleteShader(v);
  glDeleteShader(fr);
  return p;
}

static Prog s_lit, s_floor, s_bg, s_beam, s_text3d, s_ui;
static M4 s_view, s_proj, s_vp;
static V3 s_cam;
static V3 s_spotPos, s_spotCol, s_tint;
static float s_time;
static bool s_mirror = false;
static int s_fbw = 1280, s_fbh = 720;

// ------------------------------------------------------------------ mesh building
void MeshBuilder::quad(V3 a, V3 b, V3 c, V3 d, V3 n, float u0, float v0, float u1, float v1) {
  unsigned base = (unsigned)v.size();
  Vtx q[4] = {{a.x, a.y, a.z, n.x, n.y, n.z, u0, v1}, {b.x, b.y, b.z, n.x, n.y, n.z, u1, v1},
              {c.x, c.y, c.z, n.x, n.y, n.z, u1, v0}, {d.x, d.y, d.z, n.x, n.y, n.z, u0, v0}};
  for (auto& x : q) v.push_back(x);
  unsigned id[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
  for (unsigned i : id) idx.push_back(i);
}

static void axisPts(float h, float r, int seg, std::vector<float>& out) {
  out.clear();
  for (int k = 0; k <= seg; k++) { float t = (float)k / seg; out.push_back(-(h - r) - r * sinf((1 - t) * PI / 2)); }
  for (int k = 0; k <= seg; k++) { float t = (float)k / seg; out.push_back((h - r) + r * sinf(t * PI / 2)); }
}

void MeshBuilder::roundedBox(V3 c, V3 h, float r, int seg) {
  r = fminf(r, fminf(h.x, fminf(h.y, h.z)) * 0.999f);
  V3 inner(h.x - r, h.y - r, h.z - r);
  struct Face { V3 n, u, v; };
  Face faces[6] = {{V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1)}, {V3(-1, 0, 0), V3(0, 0, 1), V3(0, 1, 0)},
                   {V3(0, 1, 0), V3(0, 0, 1), V3(1, 0, 0)}, {V3(0, -1, 0), V3(1, 0, 0), V3(0, 0, 1)},
                   {V3(0, 0, 1), V3(1, 0, 0), V3(0, 1, 0)}, {V3(0, 0, -1), V3(0, 1, 0), V3(1, 0, 0)}};
  std::vector<float> pu, pv;
  for (auto& f : faces) {
    float hu = fabsf(dot(f.u, h)), hv = fabsf(dot(f.v, h)), hn = fabsf(dot(f.n, h));
    axisPts(hu, r, seg, pu);
    axisPts(hv, r, seg, pv);
    unsigned base = (unsigned)v.size();
    int nu = (int)pu.size(), nv = (int)pv.size();
    for (int j = 0; j < nv; j++)
      for (int i = 0; i < nu; i++) {
        V3 p = f.n * hn + f.u * pu[i] + f.v * pv[j];
        V3 in(clampf(p.x, -inner.x, inner.x), clampf(p.y, -inner.y, inner.y), clampf(p.z, -inner.z, inner.z));
        V3 n = norm(p - in);
        V3 q = in + n * r + c;
        Vtx x = {q.x, q.y, q.z, n.x, n.y, n.z, (pu[i] / hu) * 0.5f + 0.5f, 1 - ((pv[j] / hv) * 0.5f + 0.5f)};
        v.push_back(x);
      }
    for (int j = 0; j < nv - 1; j++)
      for (int i = 0; i < nu - 1; i++) {
        unsigned a = base + j * nu + i, b = a + 1, cc = a + nu + 1, d = a + nu;
        idx.push_back(a); idx.push_back(b); idx.push_back(cc);
        idx.push_back(a); idx.push_back(cc); idx.push_back(d);
      }
  }
}

void MeshBuilder::cylinder(V3 b, float r, float h, int seg, bool caps, float r2) {
  if (r2 < 0) r2 = r;
  unsigned base = (unsigned)v.size();
  float slope = (r - r2) / h;
  for (int i = 0; i <= seg; i++) {
    float a = (float)i / seg * 2 * PI;
    float cx = sinf(a), cz = cosf(a);
    V3 n = norm(V3(cx, slope, cz));
    Vtx lo = {b.x + cx * r, b.y, b.z + cz * r, n.x, n.y, n.z, (float)i / seg, 1};
    Vtx hi = {b.x + cx * r2, b.y + h, b.z + cz * r2, n.x, n.y, n.z, (float)i / seg, 0};
    v.push_back(lo); v.push_back(hi);
  }
  for (int i = 0; i < seg; i++) {
    unsigned a = base + i * 2;
    idx.push_back(a); idx.push_back(a + 2); idx.push_back(a + 3);
    idx.push_back(a); idx.push_back(a + 3); idx.push_back(a + 1);
  }
  if (caps) {
    disc(V3(b.x, b.y + h, b.z), r2, seg);
    unsigned cb = (unsigned)v.size();
    Vtx c = {b.x, b.y, b.z, 0, -1, 0, 0.5f, 0.5f};
    v.push_back(c);
    for (int i = 0; i <= seg; i++) {
      float a = (float)i / seg * 2 * PI;
      Vtx e = {b.x + sinf(a) * r, b.y, b.z + cosf(a) * r, 0, -1, 0, 0, 0};
      v.push_back(e);
    }
    for (int i = 0; i < seg; i++) { idx.push_back(cb); idx.push_back(cb + 2 + i); idx.push_back(cb + 1 + i); }
  }
}

void MeshBuilder::disc(V3 c, float r, int seg) {
  unsigned cb = (unsigned)v.size();
  Vtx ce = {c.x, c.y, c.z, 0, 1, 0, 0.5f, 0.5f};
  v.push_back(ce);
  for (int i = 0; i <= seg; i++) {
    float a = (float)i / seg * 2 * PI;
    Vtx e = {c.x + sinf(a) * r, c.y, c.z + cosf(a) * r, 0, 1, 0, 0.5f + sinf(a) * 0.5f, 0.5f + cosf(a) * 0.5f};
    v.push_back(e);
  }
  for (int i = 0; i < seg; i++) { idx.push_back(cb); idx.push_back(cb + 1 + i); idx.push_back(cb + 2 + i); }
}

void MeshBuilder::arcTier(float r0, float r1, float y0, float y1, float a0, float a1, int seg, V3 c) {
  auto P = [&](float a, float r, float y) { return V3(c.x + sinf(a) * r, y, c.z - cosf(a) * r); };
  for (int i = 0; i < seg; i++) {
    float aa = a0 + (a1 - a0) * i / seg, ab = a0 + (a1 - a0) * (i + 1) / seg;
    float am = (aa + ab) * 0.5f;
    // top
    quad(P(aa, r0, y1), P(ab, r0, y1), P(ab, r1, y1), P(aa, r1, y1), V3(0, 1, 0));
    // front (inner radius faces the centre)
    V3 nf = V3(-sinf(am), 0, cosf(am));
    quad(P(aa, r0, y0), P(ab, r0, y0), P(ab, r0, y1), P(aa, r0, y1), nf, 0, 0, 1, 1);
    // back
    quad(P(ab, r1, y0), P(aa, r1, y0), P(aa, r1, y1), P(ab, r1, y1), nf * -1.0f);
  }
  V3 ns0 = V3(-cosf(a0), 0, -sinf(a0)), ns1 = V3(cosf(a1), 0, sinf(a1));
  quad(P(a0, r1, y0), P(a0, r0, y0), P(a0, r0, y1), P(a0, r1, y1), ns0);
  quad(P(a1, r0, y0), P(a1, r1, y0), P(a1, r1, y1), P(a1, r0, y1), ns1);
}

void MeshBuilder::sphere(V3 c, float r, int seg) {
  unsigned base = (unsigned)v.size();
  for (int j = 0; j <= seg; j++) {
    float t = (float)j / seg * PI;
    for (int i = 0; i <= seg * 2; i++) {
      float p = (float)i / (seg * 2) * 2 * PI;
      V3 n(sinf(t) * sinf(p), cosf(t), sinf(t) * cosf(p));
      Vtx x = {c.x + n.x * r, c.y + n.y * r, c.z + n.z * r, n.x, n.y, n.z, (float)i / (seg * 2), (float)j / seg};
      v.push_back(x);
    }
  }
  int w = seg * 2 + 1;
  for (int j = 0; j < seg; j++)
    for (int i = 0; i < seg * 2; i++) {
      unsigned a = base + j * w + i, b = a + w, cc = b + 1, d = a + 1;
      idx.push_back(a); idx.push_back(b); idx.push_back(cc);
      idx.push_back(a); idx.push_back(cc); idx.push_back(d);
    }
}

Mesh MeshBuilder::build() const {
  Mesh m;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glGenBuffers(1, &m.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vtx), v.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &m.ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)24);
  glBindVertexArray(0);
  m.count = (int)idx.size();
  return m;
}

// ------------------------------------------------------------------ font
struct Font {
  GLuint tex = 0;
  int tw = 2048, th = 1024;
  float bakeSize = 84;
  std::map<int, stbtt_packedchar> glyphs;
  float ascent = 0, descent = 0;
};
static Font s_font;

static int nextCodepoint(const char*& s) {
  unsigned char c = (unsigned char)*s;
  if (!c) return 0;
  int cp, n;
  if (c < 0x80) { cp = c; n = 1; }
  else if ((c >> 5) == 6) { cp = c & 31; n = 2; }
  else if ((c >> 4) == 14) { cp = c & 15; n = 3; }
  else { cp = c & 7; n = 4; }
  s++;
  for (int i = 1; i < n && *s; i++, s++) cp = (cp << 6) | ((unsigned char)*s & 63);
  return cp;
}

void gfx_loadFont(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { plat_log("font missing %s", path); return; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<unsigned char> data(sz);
  fread(data.data(), 1, sz, f);
  fclose(f);
  std::vector<unsigned char> bmp(s_font.tw * s_font.th);
  stbtt_pack_context pc;
  stbtt_PackBegin(&pc, bmp.data(), s_font.tw, s_font.th, 0, 2, nullptr);
  stbtt_PackSetOversampling(&pc, 1, 1);
  static int extra[] = {0x20AC, 0x2026, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x00D7};
  const int nExtra = sizeof(extra) / sizeof(extra[0]);
  std::vector<stbtt_packedchar> base(224);
  std::vector<stbtt_packedchar> ex(nExtra);
  stbtt_pack_range ranges[2] = {};
  ranges[0].font_size = s_font.bakeSize;
  ranges[0].first_unicode_codepoint_in_range = 32;
  ranges[0].num_chars = 224;
  ranges[0].chardata_for_range = base.data();
  ranges[1].font_size = s_font.bakeSize;
  ranges[1].array_of_unicode_codepoints = extra;
  ranges[1].num_chars = nExtra;
  ranges[1].chardata_for_range = ex.data();
  stbtt_PackFontRanges(&pc, data.data(), 0, ranges, 2);
  stbtt_PackEnd(&pc);
  for (int i = 0; i < 224; i++) s_font.glyphs[32 + i] = base[i];
  for (int i = 0; i < nExtra; i++) s_font.glyphs[extra[i]] = ex[i];
  stbtt_fontinfo info;
  stbtt_InitFont(&info, data.data(), 0);
  int a, d, g;
  stbtt_GetFontVMetrics(&info, &a, &d, &g);
  float sc = stbtt_ScaleForPixelHeight(&info, s_font.bakeSize);
  s_font.ascent = a * sc;
  s_font.descent = d * sc;
  glGenTextures(1, &s_font.tex);
  glBindTexture(GL_TEXTURE_2D, s_font.tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, s_font.tw, s_font.th, 0, GL_RED, GL_UNSIGNED_BYTE, bmp.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static const stbtt_packedchar* glyph(int cp) {
  auto it = s_font.glyphs.find(cp);
  if (it == s_font.glyphs.end()) {
    it = s_font.glyphs.find('?');
    if (it == s_font.glyphs.end()) return nullptr;
  }
  return &it->second;
}

float text_width(const char* s, float size) {
  float sc = size / s_font.bakeSize, w = 0;
  while (*s) {
    int cp = nextCodepoint(s);
    if (cp == '\n') break;
    const stbtt_packedchar* g = glyph(cp);
    if (g) w += g->xadvance * sc;
  }
  return w;
}

// ------------------------------------------------------------------ ui batch
struct UIV { float x, y, u, v, r, g, b, a, p0, p1, p2, p3, mode; };
static std::vector<UIV> s_ui_v;
static GLuint s_ui_vao, s_ui_vbo;
static GLuint s_t3_vao, s_t3_vbo;

static void uiQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Col ct, Col cb,
                   float p0, float p1, float p2, float p3, float mode) {
  UIV a = {x0, y0, u0, v0, ct.r, ct.g, ct.b, ct.a, p0, p1, p2, p3, mode};
  UIV b = {x1, y0, u1, v0, ct.r, ct.g, ct.b, ct.a, p0, p1, p2, p3, mode};
  UIV c = {x1, y1, u1, v1, cb.r, cb.g, cb.b, cb.a, p0, p1, p2, p3, mode};
  UIV d = {x0, y1, u0, v1, cb.r, cb.g, cb.b, cb.a, p0, p1, p2, p3, mode};
  s_ui_v.push_back(a); s_ui_v.push_back(b); s_ui_v.push_back(c);
  s_ui_v.push_back(a); s_ui_v.push_back(c); s_ui_v.push_back(d);
}

void ui_rect(float x, float y, float w, float h, Col c, float radius, float border, Col bc) {
  if (border > 0) {
    uiQuad(x, y, x + w, y + h, -w / 2, -h / 2, w / 2, h / 2, bc, bc, w / 2, h / 2, radius, 0, 0);
    x += border; y += border; w -= border * 2; h -= border * 2; radius = fmaxf(0, radius - border);
  }
  uiQuad(x, y, x + w, y + h, -w / 2, -h / 2, w / 2, h / 2, c, c, w / 2, h / 2, radius, 0, 0);
}
void ui_gradRect(float x, float y, float w, float h, Col top, Col bot, float radius) {
  uiQuad(x, y, x + w, y + h, -w / 2, -h / 2, w / 2, h / 2, top, bot, w / 2, h / 2, radius, 0, 0);
}
void ui_glow(float x, float y, float w, float h, Col c, float radius, float spread) {
  float s = spread;
  uiQuad(x - s, y - s, x + w + s, y + h + s, -w / 2 - s, -h / 2 - s, w / 2 + s, h / 2 + s, c, c, w / 2, h / 2, radius, spread, 2);
}
void ui_circle(float cx, float cy, float r, Col c, float ring) {
  float e = ring > 0 ? ring : 0;
  uiQuad(cx - r - e, cy - r - e, cx + r + e, cy + r + e, -r - e, -r - e, r + e, r + e, c, c, r, r, r, ring, ring > 0 ? 3.f : 0.f);
}

void ui_flush() {
  if (s_ui_v.empty()) return;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(s_ui.id);
  M4 p = ortho(0, 1280, 720, 0, -1, 1);
  glUniformMatrix4fv(s_ui.loc("uProj"), 1, GL_FALSE, p.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, s_font.tex);
  glUniform1i(s_ui.loc("uTex"), 0);
  glBindVertexArray(s_ui_vao);
  glBindBuffer(GL_ARRAY_BUFFER, s_ui_vbo);
  glBufferData(GL_ARRAY_BUFFER, s_ui_v.size() * sizeof(UIV), s_ui_v.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)s_ui_v.size());
  glBindVertexArray(0);
  s_ui_v.clear();
}

void ui_setScissor(bool on, float x, float y, float w, float h) {
  ui_flush();
  if (on) {
    glEnable(GL_SCISSOR_TEST);
    float sx = s_fbw / 1280.0f, sy = s_fbh / 720.0f;
    glScissor((int)(x * sx), (int)((720 - y - h) * sy), (int)(w * sx), (int)(h * sy));
  } else glDisable(GL_SCISSOR_TEST);
}

static void textRun(const char* s, float x, float baseline, float size, Col c) {
  float sc = size / s_font.bakeSize;
  float pen = x;
  while (*s) {
    int cp = nextCodepoint(s);
    if (cp == '\n') break;
    const stbtt_packedchar* g = glyph(cp);
    if (!g) continue;
    if (cp != ' ') {
      float x0 = pen + g->xoff * sc, y0 = baseline + g->yoff * sc, x1 = pen + g->xoff2 * sc, y1 = baseline + g->yoff2 * sc;
      uiQuad(x0, y0, x1, y1, g->x0 / (float)s_font.tw, g->y0 / (float)s_font.th, g->x1 / (float)s_font.tw,
             g->y1 / (float)s_font.th, c, c, 0, 0, 0, 0, 1);
    }
    pen += g->xadvance * sc;
  }
}

void text_draw(const char* s, float x, float y, float size, Col c, int align, float shadow) {
  float w = text_width(s, size);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  float baseline = y + size * 0.36f;
  if (shadow > 0) textRun(s, x + shadow * size / 40, baseline + shadow * size / 40, size, rgba(0, 0, 0, c.a * 0.55f));
  textRun(s, x, baseline, size, c);
}

static void wrapLines(const char* s, float w, float size, std::vector<std::string>& out) {
  out.clear();
  std::string line, word;
  std::string str(s);
  size_t i = 0;
  auto flushWord = [&]() {
    if (word.empty()) return;
    std::string cand = line.empty() ? word : line + " " + word;
    if (!line.empty() && text_width(cand.c_str(), size) > w) { out.push_back(line); line = word; }
    else line = cand;
    word.clear();
  };
  for (; i <= str.size(); i++) {
    char ch = i < str.size() ? str[i] : ' ';
    if (ch == ' ' || ch == '\n') {
      flushWord();
      if (ch == '\n') { out.push_back(line); line.clear(); }
    } else word += ch;
  }
  if (!line.empty()) out.push_back(line);
}

int text_wrapLines(const char* s, float w, float size) {
  std::vector<std::string> l;
  wrapLines(s, w, size, l);
  return (int)l.size();
}

void text_drawWrapped(const char* s, float x, float y, float w, float size, Col c, int align, float lineH) {
  std::vector<std::string> lines;
  wrapLines(s, w, size, lines);
  for (size_t i = 0; i < lines.size(); i++) {
    float lx = align == 1 ? x + w / 2 : (align == 2 ? x + w : x);
    text_draw(lines[i].c_str(), lx, y + i * size * lineH, size, c, align, 1.5f);
  }
}

void text_draw3D(const char* s, const M4& model, float size, Col c) {
  std::vector<Vtx> vv;
  float sc = size / s_font.bakeSize;
  float w = text_width(s, s_font.bakeSize) * sc;
  float pen = -w / 2;
  float baseline = size * 0.36f;  // centre vertically (y up)
  while (*s) {
    int cp = nextCodepoint(s);
    const stbtt_packedchar* g = glyph(cp);
    if (!g) continue;
    if (cp != ' ') {
      float x0 = pen + g->xoff * sc, x1 = pen + g->xoff2 * sc;
      float y0 = -(baseline + g->yoff * sc), y1 = -(baseline + g->yoff2 * sc);
      float u0 = g->x0 / (float)s_font.tw, v0 = g->y0 / (float)s_font.th, u1 = g->x1 / (float)s_font.tw, v1 = g->y1 / (float)s_font.th;
      Vtx a = {x0, y0, 0, 0, 0, 1, u0, v0}, b = {x1, y0, 0, 0, 0, 1, u1, v0}, cc = {x1, y1, 0, 0, 0, 1, u1, v1}, d = {x0, y1, 0, 0, 0, 1, u0, v1};
      vv.push_back(a); vv.push_back(d); vv.push_back(cc);
      vv.push_back(a); vv.push_back(cc); vv.push_back(b);
    }
    pen += g->xadvance * sc;
  }
  if (vv.empty()) return;
  glUseProgram(s_text3d.id);
  M4 mvp = s_vp * model;
  glUniformMatrix4fv(s_text3d.loc("uMVP"), 1, GL_FALSE, mvp.m);
  glUniform4f(s_text3d.loc("uColor"), c.r, c.g, c.b, c.a);
  glUniform1i(s_text3d.loc("uTex"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, s_font.tex);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(s_t3_vao);
  glBindBuffer(GL_ARRAY_BUFFER, s_t3_vbo);
  glBufferData(GL_ARRAY_BUFFER, vv.size() * sizeof(Vtx), vv.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vv.size());
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
}

// ------------------------------------------------------------------ frame / draw
void gfx_init() {
  s_lit = makeProg(VS_LIT, FS_LIT, true);
  s_floor = makeProg(VS_LIT, FS_FLOOR, true);
  s_bg = makeProg(VS_LIT, FS_BG, true);
  s_beam = makeProg(VS_LIT, FS_BEAM, true);
  s_text3d = makeProg(VS_TEXT3D, FS_TEXT3D, false);
  s_ui = makeProg(VS_UI, FS_UI, false);
  glGenVertexArrays(1, &s_ui_vao);
  glBindVertexArray(s_ui_vao);
  glGenBuffers(1, &s_ui_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, s_ui_vbo);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIV), (void*)0);
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIV), (void*)8);
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UIV), (void*)16);
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(UIV), (void*)32);
  glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(UIV), (void*)48);
  glGenVertexArrays(1, &s_t3_vao);
  glBindVertexArray(s_t3_vao);
  glGenBuffers(1, &s_t3_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, s_t3_vbo);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)24);
  glBindVertexArray(0);
}

void gfx_beginFrame(int w, int h) {
  s_fbw = w; s_fbh = h;
  glViewport(0, 0, w, h);
  glDepthMask(GL_TRUE);
  glClearColor(0.01f, 0.008f, 0.025f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
}

void gfx_setCamera(const M4& view, const M4& proj, V3 cam) {
  s_view = view; s_proj = proj; s_vp = proj * view; s_cam = cam;
}
const M4& gfx_viewProj() { return s_vp; }

void gfx_setLights(V3 spotPos, V3 spotColor, float time, V3 tint) {
  s_spotPos = spotPos; s_spotCol = spotColor; s_time = time; s_tint = tint;
}

static void setCommon(Prog& p) {
  glUseProgram(p.id);
  glUniformMatrix4fv(p.loc("uVP"), 1, GL_FALSE, s_vp.m);
  glUniform3f(p.loc("uCam"), s_cam.x, s_cam.y, s_cam.z);
  glUniform3f(p.loc("uTint"), s_tint.x, s_tint.y, s_tint.z);
  glUniform1f(p.loc("uTime"), s_time);
  glUniform3f(p.loc("uSpotPos"), s_spotPos.x, s_spotPos.y, s_spotPos.z);
  glUniform3f(p.loc("uSpotCol"), s_spotCol.x, s_spotCol.y, s_spotCol.z);
}

void gfx_setMirror(bool on) {
  s_mirror = on;
  glFrontFace(on ? GL_CW : GL_CCW);
}

void gfx_drawMesh(const Mesh& m, const M4& model, const Material& mat) {
  setCommon(s_lit);
  M4 mm = s_mirror ? scale(V3(1, -1, 1)) * model : model;
  glUniformMatrix4fv(s_lit.loc("uModel"), 1, GL_FALSE, mm.m);
  glUniform4f(s_lit.loc("uColor"), mat.color.r, mat.color.g, mat.color.b, mat.color.a);
  glUniform1f(s_lit.loc("uMetal"), mat.metal);
  glUniform1f(s_lit.loc("uGloss"), mat.gloss);
  glUniform3f(s_lit.loc("uEmis"), mat.emissive.x, mat.emissive.y, mat.emissive.z);
  glUniform1f(s_lit.loc("uReflect"), s_mirror ? 0.55f : 0.0f);
  if (mat.color.a < 0.999f) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
  else glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glBindVertexArray(m.vao);
  glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void gfx_drawFloor(const Mesh& m, float time, V3 tint) {
  setCommon(s_floor);
  M4 id = M4::id();
  glUniformMatrix4fv(s_floor.loc("uModel"), 1, GL_FALSE, id.m);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(m.vao);
  glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  glDisable(GL_BLEND);
}

void gfx_drawBackground(const Mesh& m, float time, V3 tint, float energy) {
  setCommon(s_bg);
  M4 id = M4::id();
  glUniformMatrix4fv(s_bg.loc("uModel"), 1, GL_FALSE, id.m);
  glUniform1f(s_bg.loc("uEnergy"), energy);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(m.vao);
  glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void gfx_drawBeam(const Mesh& m, const M4& model, Col c) {
  setCommon(s_beam);
  glUniformMatrix4fv(s_beam.loc("uModel"), 1, GL_FALSE, model.m);
  glUniform4f(s_beam.loc("uColor"), c.r, c.g, c.b, c.a);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(m.vao);
  glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

V2 gfx_project(V3 w) {
  float c[4];
  s_vp.xform4(w, c);
  V2 r;
  if (c[3] <= 0.01f) { r.x = -10000; r.y = -10000; return r; }
  r.x = (c[0] / c[3] * 0.5f + 0.5f) * 1280.0f;
  r.y = (1.0f - (c[1] / c[3] * 0.5f + 0.5f)) * 720.0f;
  return r;
}
