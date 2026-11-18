#pragma once
#include <vector>
#include <string>
#include "glmini.h"
#include "mathx.h"

struct Vtx { float px, py, pz, nx, ny, nz, u, v; };

struct Mesh {
  GLuint vao = 0, vbo = 0, ibo = 0;
  int count = 0;
};

struct MeshBuilder {
  std::vector<Vtx> v;
  std::vector<unsigned> idx;
  void quad(V3 a, V3 b, V3 c, V3 d, V3 n, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1);
  void roundedBox(V3 center, V3 half, float radius, int seg = 3);
  void cylinder(V3 base, float r, float h, int seg, bool caps = true, float r2 = -1);
  void disc(V3 c, float r, int seg);
  void arcTier(float r0, float r1, float y0, float y1, float a0, float a1, int seg, V3 center);
  void sphere(V3 c, float r, int seg);
  Mesh build() const;
};

void gfx_init();
void gfx_beginFrame(int w, int h);
void gfx_setCamera(const M4& view, const M4& proj, V3 camPos);
const M4& gfx_viewProj();

// lighting / material
struct Material {
  Col color = {1, 1, 1, 1};
  float metal = 0, gloss = 0.3f;
  V3 emissive;
  float reflectFade = 0;  // >0 when drawing mirrored geometry for the floor reflection
};
void gfx_setLights(V3 spotPos, V3 spotColor, float time, V3 tint);
void gfx_drawMesh(const Mesh& m, const M4& model, const Material& mat);
void gfx_setMirror(bool on);  // flips winding + enables reflection fade

void gfx_drawFloor(const Mesh& m, float time, V3 tint);
void gfx_drawBackground(const Mesh& m, float time, V3 tint, float energy);
void gfx_drawBeam(const Mesh& m, const M4& model, Col color);

// text
struct Font;
void gfx_loadFont(const char* path);
float text_width(const char* utf8, float size);
// align: 0 left, 1 center, 2 right
void text_draw(const char* utf8, float x, float y, float size, Col c, int align = 0, float shadow = 2.0f);
void text_drawWrapped(const char* utf8, float x, float y, float w, float size, Col c, int align = 1, float lineH = 1.2f);
int text_wrapLines(const char* utf8, float w, float size);
void text_draw3D(const char* utf8, const M4& model, float size, Col c);  // size in world units, centered

// 2d ui (1280x720 virtual coordinates)
void ui_rect(float x, float y, float w, float h, Col c, float radius = 8, float border = 0, Col borderCol = {0, 0, 0, 0});
void ui_gradRect(float x, float y, float w, float h, Col top, Col bottom, float radius = 8);
void ui_glow(float x, float y, float w, float h, Col c, float radius, float spread);
void ui_circle(float cx, float cy, float r, Col c, float ring = 0);
void ui_flush();
void ui_setScissor(bool on, float x = 0, float y = 0, float w = 0, float h = 0);
V2 gfx_project(V3 world);  // to 1280x720 virtual space; returns x<-1000 if behind camera
