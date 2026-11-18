// Koffer Koorts / Case Fever - an original briefcase game show for Nintendo Switch homebrew
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "audio.h"
#include "gfx.h"
#include "platform.h"

// =====================================================================================
// globals & settings
// =====================================================================================
static Rng R;
static float g_time = 0, g_dt = 0.016f;
static bool g_quit = false;
static bool g_autoplay = false, g_fast = false;
static std::string g_shotDir;
static int g_shotCount = 0;

struct Settings {
  int lang = 0;  // 0 nl, 1 en
  bool gyro = true;
  int sens = 5;  // 1..10
  bool invX = false, invY = false;
  int volMusic = 6, volVoice = 9, volSfx = 7;
  bool subs = true;
  bool explained = false;
};
static Settings S;
static bool NL() { return S.lang == 0; }
static const char* T(const char* nl, const char* en) { return NL() ? nl : en; }

static void applyVolumes() { audio_setVolumes(S.volMusic / 10.0f, S.volVoice / 10.0f, S.volSfx / 10.0f); }

static void saveSettings() {
  FILE* f = fopen(plat_savePath("settings.txt").c_str(), "w");
  if (!f) return;
  fprintf(f, "lang=%d\ngyro=%d\nsens=%d\ninvx=%d\ninvy=%d\nmusic=%d\nvoice=%d\nsfx=%d\nsubs=%d\nexplained=%d\n", S.lang, S.gyro,
          S.sens, S.invX, S.invY, S.volMusic, S.volVoice, S.volSfx, S.subs, S.explained);
  fclose(f);
}
static void loadSettings() {
  S.lang = strcmp(plat_systemLang(), "nl") == 0 ? 0 : 1;
  FILE* f = fopen(plat_savePath("settings.txt").c_str(), "r");
  if (!f) return;
  char key[64];
  int v;
  while (fscanf(f, " %63[^=]=%d", key, &v) == 2) {
    std::string k = key;
    if (k == "lang") S.lang = v ? 1 : 0;
    else if (k == "gyro") S.gyro = v != 0;
    else if (k == "sens") S.sens = v < 1 ? 1 : (v > 10 ? 10 : v);
    else if (k == "invx") S.invX = v != 0;
    else if (k == "invy") S.invY = v != 0;
    else if (k == "music") S.volMusic = v;
    else if (k == "voice") S.volVoice = v;
    else if (k == "sfx") S.volSfx = v;
    else if (k == "subs") S.subs = v != 0;
    else if (k == "explained") S.explained = v != 0;
  }
  fclose(f);
}

// =====================================================================================
// amounts
// =====================================================================================
static const long AMOUNTS[26] = {1, 100, 500, 1000, 2500, 5000, 7500, 10000, 20000, 30000, 40000, 50000, 75000,
                                 100000, 500000, 1000000, 2500000, 5000000, 7500000, 10000000, 20000000, 30000000,
                                 40000000, 50000000, 75000000, 100000000};

static std::string fmtAmount(long cents) {
  char buf[48];
  if (cents < 100) {
    snprintf(buf, sizeof(buf), NL() ? "\xE2\x82\xAC 0,%02ld" : "\xE2\x82\xAC" "0.%02ld", cents);
    return buf;
  }
  long e = cents / 100;
  std::string d = std::to_string(e), out;
  int cnt = 0;
  for (int i = (int)d.size() - 1; i >= 0; i--) {
    out.insert(out.begin(), d[i]);
    if (++cnt % 3 == 0 && i > 0) out.insert(out.begin(), NL() ? '.' : ',');
  }
  return std::string(NL() ? "\xE2\x82\xAC " : "\xE2\x82\xAC") + out;
}

static long round2sig(double euros) {
  if (euros < 1) return 1;
  long e = (long)(euros + 0.5);
  if (e < 100) return e;
  long mag = 1;
  while (e / mag >= 100) mag *= 10;
  long r = (long)((double)e / mag + 0.5) * mag;
  if (r > 990000 && r < 1000000) r = 990000;
  if (r > 1000000) r = 1000000;
  return r;
}

// =====================================================================================
// presenter voice
// =====================================================================================
struct VGroup { std::vector<std::string> nl, en; int last = -1; };
static std::map<std::string, VGroup> V;

static void loadVoiceIndex() {
  FILE* f = fopen(plat_assetPath("voice/index.txt").c_str(), "rb");
  if (!f) { plat_log("voice index missing"); return; }
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    std::string s(line);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    size_t a = s.find('\t'), b = s.find('\t', a + 1), c = s.find('\t', b + 1);
    if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
    VGroup& g = V[s.substr(0, a)];
    g.nl.push_back(s.substr(b + 1, c - b - 1));
    g.en.push_back(s.substr(c + 1));
  }
  fclose(f);
}

static bool hasGroup(const std::string& g) { return V.find(g) != V.end(); }

static void say(const std::string& g, float gap = 0.15f) {
  if (g_fast) return;
  auto it = V.find(g);
  if (it == V.end()) { plat_log("missing voice group %s", g.c_str()); return; }
  VGroup& vg = it->second;
  int n = (int)vg.nl.size();
  int v = n <= 1 ? 0 : R.irange(n);
  if (n > 1 && v == vg.last) v = (v + 1 + R.irange(n - 1)) % n;
  vg.last = v;
  std::string file = std::string("voice/") + (NL() ? "nl/" : "en/") + g + "__" + std::to_string(v) + ".ogg";
  voice_queue(plat_assetPath(file.c_str()), NL() ? vg.nl[v] : vg.en[v], gap);
}
static void sayAmount(long cents, float gap = 0.1f) { say("amt_" + std::to_string(cents), gap); }

// =====================================================================================
// sequencer: scripted show moments
// =====================================================================================
struct Step { int type; float t; std::function<void()> fn; std::function<bool()> cond; };
static std::deque<Step> Q;
static void qCall(std::function<void()> f) { Q.push_back({0, 0, f, nullptr}); }
static void qWait(float t) { Q.push_back({1, t, nullptr, nullptr}); }
static void qVoice() { Q.push_back({2, 0, nullptr, nullptr}); }
static void qUntil(std::function<bool()> c) { Q.push_back({3, 0, nullptr, c}); }
static void qSay(const std::string& g) { qCall([g] { say(g); }); }
static void qSayAmount(long c) { qCall([c] { sayAmount(c); }); }
static void qSfx(const char* n, float v = 1) { qCall([n, v] { sfx_play(n, v); }); }

static void runQueue(float dt) {
  int guard = 0;
  while (!Q.empty() && guard++ < 200) {
    Step& s = Q.front();
    if (s.type == 0) { auto f = s.fn; Q.pop_front(); f(); continue; }
    if (s.type == 1) {
      s.t -= dt * (g_fast ? 5.0f : 1.0f);
      if (s.t <= 0) { Q.pop_front(); dt = 0; continue; }
      break;
    }
    if (s.type == 2) { if (!voice_busy()) { Q.pop_front(); continue; } break; }
    if (s.type == 3) { if (s.cond()) { Q.pop_front(); continue; } break; }
  }
}

// =====================================================================================
// scene
// =====================================================================================
static const float CW = 0.64f, CH = 0.46f, CD = 0.18f;  // case size
static const V3 STAND_C(0, 0, 2.6f);
static const float TIER_R[3] = {5.7f, 6.6f, 7.5f};
static const float TIER_H[3] = {0.30f, 0.78f, 1.26f};

static Mesh mFloor, mWall, mBeam, mTier[3], mTierLed[3], mShellBack, mShellFront, mSeam, mHandle, mLatch, mPlate,
    mInner, mPlaque, mPedestal, mPedRing, mBoothFrame, mBoothGlass, mSilBody, mSilHead, mLamp, mDesk;

struct CaseObj {
  int num = 0, amt = 0;
  bool opened = false, revealed = false;
  int owner = -1;
  float lid = 0, lidTarget = 0, hover = 0, dim = 0, flash = 0;
  V3 home; float homeYaw = 0; int tier = 0;
  V3 pos; float yaw = 0;
  bool moving = false; float moveT = 0; V3 from, to; float yawFrom = 0, yawTo = 0;
};
static CaseObj C[26];

struct CamS { V3 pos, tgt, gpos, gtgt; float rate = 2.0f; } cam;
static V3 g_tint(0, 0, 0), g_tintTarget(0, 0, 0);
static V3 g_spotPos(0, 3, 2), g_spotCol(0, 0, 0), g_spotColTarget(0, 0, 0);
static float g_energy = 1.0f;
static float g_lampBlink = 0;  // banker lamp
static float g_shake = 0;

static V3 pedestalPos(int p, int np) {
  if (np <= 1) return V3(0, 0, 1.7f);
  float span = 2.2f;
  float x = (p - (np - 1) * 0.5f) * span;
  return V3(x, 0, 1.9f);
}
static const float PED_H = 0.55f;

static void buildScene() {
  MeshBuilder b;
  b.disc(V3(0, 0, 0), 18.0f, 64);
  mFloor = b.build();
  b = MeshBuilder();
  b.cylinder(V3(0, -0.1f, 0), 16.0f, 13.0f, 96, false);
  mWall = b.build();
  b = MeshBuilder();
  b.cylinder(V3(0, -11.0f, 0), 1.3f, 11.0f, 24, false, 0.12f);
  mBeam = b.build();
  for (int t = 0; t < 3; t++) {
    float half = 0.86f;  // radians each side
    b = MeshBuilder();
    float y0 = t == 0 ? 0.0f : TIER_H[t - 1];
    b.arcTier(TIER_R[t] - 0.45f, TIER_R[t] + 0.45f, 0.0f, TIER_H[t], -half, half, 40, STAND_C);
    (void)y0;
    mTier[t] = b.build();
    b = MeshBuilder();
    b.arcTier(TIER_R[t] - 0.47f, TIER_R[t] - 0.45f, TIER_H[t] - 0.05f, TIER_H[t] - 0.015f, -half, half, 40, STAND_C);
    mTierLed[t] = b.build();
  }
  b = MeshBuilder();
  b.roundedBox(V3(0, CH / 2, -CD / 4), V3(CW / 2, CH / 2, CD / 4 - 0.003f), 0.035f, 3);
  mShellBack = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, -CH / 2, CD / 4), V3(CW / 2, CH / 2, CD / 4 - 0.003f), 0.035f, 3);
  mShellFront = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, CH / 2, 0), V3(CW / 2 + 0.006f, CH / 2 + 0.006f, 0.008f), 0.03f, 2);
  mSeam = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, CH + 0.06f, 0), V3(0.11f, 0.017f, 0.022f), 0.016f, 2);
  b.roundedBox(V3(-0.085f, CH + 0.025f, 0), V3(0.014f, 0.03f, 0.016f), 0.01f, 1);
  b.roundedBox(V3(0.085f, CH + 0.025f, 0), V3(0.014f, 0.03f, 0.016f), 0.01f, 1);
  mHandle = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(-0.21f, -0.035f, CD / 2), V3(0.035f, 0.022f, 0.012f), 0.01f, 1);
  b.roundedBox(V3(0.21f, -0.035f, CD / 2), V3(0.035f, 0.022f, 0.012f), 0.01f, 1);
  mLatch = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, -CH * 0.56f, CD / 2), V3(0.15f, 0.095f, 0.008f), 0.03f, 2);
  mPlate = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, CH / 2, 0.004f), V3(CW / 2 - 0.035f, CH / 2 - 0.035f, 0.004f), 0.02f, 1);
  mInner = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, CH / 2, 0.011f), V3(0.255f, 0.075f, 0.004f), 0.02f, 1);
  mPlaque = b.build();
  b = MeshBuilder();
  b.cylinder(V3(0, 0, 0), 0.5f, PED_H, 40, true, 0.42f);
  mPedestal = b.build();
  b = MeshBuilder();
  b.cylinder(V3(0, PED_H - 0.04f, 0), 0.435f, 0.03f, 40, false);
  b.cylinder(V3(0, 0.02f, 0), 0.505f, 0.03f, 40, false);
  mPedRing = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, 0, 0), V3(1.8f, 1.05f, 0.16f), 0.08f, 2);
  mBoothFrame = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, 0, 0.13f), V3(1.6f, 0.88f, 0.03f), 0.04f, 1);
  mBoothGlass = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, -0.62f, 0.3f), V3(0.5f, 0.32f, 0.12f), 0.22f, 3);
  b.cylinder(V3(0, -0.32f, 0.3f), 0.09f, 0.14f, 16, false);
  mSilBody = b.build();
  b = MeshBuilder();
  b.sphere(V3(0, -0.05f, 0.3f), 0.2f, 12);
  mSilHead = b.build();
  b = MeshBuilder();
  b.sphere(V3(0, 0, 0), 0.13f, 12);
  mLamp = b.build();
  b = MeshBuilder();
  b.roundedBox(V3(0, -0.98f, 0.3f), V3(1.2f, 0.1f, 0.35f), 0.05f, 1);
  mDesk = b.build();
}

static void layoutCases() {
  int idx = 0;
  int perRow[3] = {9, 9, 8};  // back, middle, front
  int tierOf[3] = {2, 1, 0};
  for (int row = 0; row < 3; row++) {
    int n = perRow[row], t = tierOf[row];
    float R0 = TIER_R[t];
    float spacing = 1.20f / R0;
    float span = (n - 1) * spacing;
    for (int i = 0; i < n; i++, idx++) {
      float a = -span / 2 + i * spacing;
      CaseObj& c = C[idx];
      c.num = idx + 1;
      c.tier = t;
      c.home = V3(STAND_C.x + sinf(a) * R0, TIER_H[t], STAND_C.z - cosf(a) * R0);
      c.homeYaw = -a;
    }
  }
}

static void resetCases() {
  int order[26];
  for (int i = 0; i < 26; i++) order[i] = i;
  for (int i = 25; i > 0; i--) { int j = R.irange(i + 1); int t = order[i]; order[i] = order[j]; order[j] = t; }
  for (int i = 0; i < 26; i++) {
    CaseObj& c = C[i];
    int num = c.num; V3 h = c.home; float hy = c.homeYaw; int tier = c.tier;
    c = CaseObj();
    c.num = num; c.home = h; c.homeYaw = hy; c.tier = tier;
    c.amt = order[i];
    c.pos = c.home; c.yaw = c.homeYaw;
  }
}

static V3 caseFront(const CaseObj& c) { return V3(sinf(c.yaw), 0, cosf(c.yaw)); }

static void moveCase(CaseObj& c, V3 to, float yaw) {
  c.moving = true; c.moveT = 0; c.from = c.pos; c.to = to; c.yawFrom = c.yaw; c.yawTo = yaw;
}

// camera presets
static void camOverview() { cam.gpos = V3(0, 3.7f, 8.6f); cam.gtgt = V3(0, 1.05f, -3.0f); cam.rate = 1.8f; }
static void camCase(int i) {
  CaseObj& c = C[i];
  V3 f = caseFront(c);
  cam.gpos = c.pos + f * 1.55f + V3(0, 0.62f, 0);
  cam.gtgt = c.pos + V3(0, 0.26f, 0);
  cam.rate = 2.6f;
}
static void camBanker() { cam.gpos = V3(0, 4.1f, -2.4f); cam.gtgt = V3(0, 4.7f, -8.4f); cam.rate = 1.6f; }

// =====================================================================================
// players, pointer & focus system
// =====================================================================================
static const unsigned PCOL[4] = {0xff4d5e, 0x3d8bff, 0xffd23d, 0x3ddc84};
struct Pointer {
  float x = 640, y = 360;
  float vis = 0, wake = 0, still = 0;
  int focus = -1;
  float rep = 0; bool navHeld = false;
};
struct Player {
  int pad = -1;
  int caseIdx = -1;
  bool dealt = false;
  long deal = 0, offer = 0, finalAmt = 0;
  int choice = -1;
  float mood = 1.0f;
  Pointer ptr;
};
static Player P[4];
static int NP = 1;
static bool MP = false;

struct Item { int id; float x, y, w, h; int owner; bool noAuto; };
static std::vector<Item> itemsPrev, itemsCur;
static int ctrlMask = 1;
static int activated[4] = {-1, -1, -1, -1};
static float g_autoTimer = 0;
static V3 g_grav[MAX_PADS];

static int HID(const char* s) {
  unsigned h = 2166136261u;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return (int)(h & 0x3fffffff) + 100000;
}

static void addItem(int id, float x, float y, float w, float h, int owner = -1, bool noAuto = false) {
  Item it = {id, x, y, w, h, owner, noAuto};
  itemsCur.push_back(it);
}
static bool eligible(const Item& it, int p) { return it.owner == -1 || it.owner == p; }
static const Item* findItem(int id, int p) {
  for (auto& it : itemsPrev) if (it.id == id && eligible(it, p)) return &it;
  return nullptr;
}
static int itemAt(float x, float y, int p) {
  int best = -1; float bestA = 1e9f;
  for (auto& it : itemsPrev) {
    if (!eligible(it, p)) continue;
    if (x >= it.x && x <= it.x + it.w && y >= it.y && y <= it.y + it.h) {
      float a = it.w * it.h;
      if (a < bestA) { bestA = a; best = it.id; }
    }
  }
  return best;
}
static bool focused(int id, int p = -1) {
  if (p >= 0) return (ctrlMask & (1 << p)) && P[p].ptr.focus == id;
  for (int i = 0; i < 4; i++) if ((ctrlMask & (1 << i)) && P[i].ptr.focus == id) return true;
  return false;
}
static bool wasActivated(int id, int owner = -1) {
  for (int i = 0; i < 4; i++) {
    if (!(ctrlMask & (1 << i))) continue;
    if (owner >= 0 && owner != i) continue;
    if (activated[i] == id) { activated[i] = -1; return true; }
  }
  return false;
}

static void navigate(int p, float dx, float dy) {
  Pointer& pt = P[p].ptr;
  const Item* cur = findItem(pt.focus, p);
  if (!cur) {
    for (auto& it : itemsPrev) if (eligible(it, p)) { pt.focus = it.id; return; }
    return;
  }
  float cx = cur->x + cur->w / 2, cy = cur->y + cur->h / 2;
  float l = sqrtf(dx * dx + dy * dy);
  float dirx = dx / l, diry = -dy / l;
  int best = -1; float bestS = 1e9f;
  for (auto& it : itemsPrev) {
    if (!eligible(it, p) || it.id == cur->id) continue;
    float vx = it.x + it.w / 2 - cx, vy = it.y + it.h / 2 - cy;
    float d = sqrtf(vx * vx + vy * vy);
    if (d < 1) continue;
    float c = (vx * dirx + vy * diry) / d;
    if (c < 0.3f) continue;
    float s = d * (1 + 2.5f * (1 - c));
    if (s < bestS) { bestS = s; best = it.id; }
  }
  if (best != -1) { pt.focus = best; sfx_play("sfx_hover", 0.6f); }
}

struct InputAgg { uint32_t held = 0, down = 0; float sx = 0, sy = 0, gdx = 0, gdy = 0; bool mouse = false; float mx = 0, my = 0; };

static void padToAgg(int pi, InputAgg& in) {
  GPad& g = plat_pad(pi);
  if (!g.connected) return;
  in.held |= g.held;
  in.down |= g.down;
  if (g.stickX * g.stickX + g.stickY * g.stickY > in.sx * in.sx + in.sy * in.sy) { in.sx = g.stickX; in.sy = g.stickY; }
  if (g.mouseAbs && (g.mouseDX != 0 || g.mouseDY != 0)) { in.mouse = true; in.mx = g.mouseX; in.my = g.mouseY; }
  if (g.gyroValid && S.gyro) {
    V3 a(g.accel[0], g.accel[1], g.accel[2]);
    if (len(a) > 0.3f) g_grav[pi] = lerp3(g_grav[pi], a, 0.08f);
    if (len(g_grav[pi]) < 0.1f) g_grav[pi] = V3(0, 0, -1);
    V3 up = norm(g_grav[pi]) * -1.0f;
    V3 w(g.gyro[0], g.gyro[1], g.gyro[2]);
    float yawRate = dot(w, up);
    float pitchRate = g.gyro[0];
    float k = 950.0f * (0.35f + S.sens * 0.17f);
    if (fabsf(yawRate) < 0.025f) yawRate = 0;
    if (fabsf(pitchRate) < 0.025f) pitchRate = 0;
    float dx = -yawRate * k * g_dt * (S.invX ? -1.f : 1.f);
    float dy = -pitchRate * k * g_dt * (S.invY ? -1.f : 1.f);
    in.gdx += dx; in.gdy += dy;
  }
}

static void updatePlayerInput(int p) {
  activated[p] = -1;
  if (!(ctrlMask & (1 << p))) return;
  InputAgg in;
  if (MP && P[p].pad >= 0) padToAgg(P[p].pad, in);
  else for (int i = 0; i < MAX_PADS; i++) padToAgg(i, in);
  Pointer& pt = P[p].ptr;
  bool moved = false;
  if (in.mouse) { pt.x = in.mx; pt.y = in.my; pt.vis = 6; moved = true; }
  else if (in.gdx != 0 || in.gdy != 0) {
    pt.x = clampf(pt.x + in.gdx, 0, 1280);
    pt.y = clampf(pt.y + in.gdy, 0, 720);
    pt.wake += fabsf(in.gdx) + fabsf(in.gdy);
    if (pt.wake > 30) pt.vis = 8;
    moved = pt.vis > 0;
  }
  pt.wake *= expf(-g_dt * 4);
  if (in.down & BTN_RECENTER) { pt.x = 640; pt.y = 360; pt.vis = 8; moved = true; }
  if (pt.vis > 0) pt.vis -= g_dt * (moved ? 0 : 0.5f);
  if (pt.vis > 0 && moved) {
    int h = itemAt(pt.x, pt.y, p);
    if (h != -1 && h != pt.focus) { pt.focus = h; sfx_play("sfx_hover", 0.5f); }
  }
  float sx = in.sx, sy = in.sy;
  if (in.held & BTN_LEFT) sx = -1;
  if (in.held & BTN_RIGHT) sx = 1;
  if (in.held & BTN_UP) sy = 1;
  if (in.held & BTN_DOWN) sy = -1;
  if (sx * sx + sy * sy > 0.3f) {
    pt.rep -= g_dt;
    if (pt.rep <= 0) {
      navigate(p, sx, sy);
      pt.rep = pt.navHeld ? 0.16f : 0.34f;
      pt.navHeld = true;
      pt.vis = 0;
    }
  } else { pt.rep = 0; pt.navHeld = false; }
  if (in.down & BTN_TRIGGER) {
    if (pt.vis > 0) { int h = itemAt(pt.x, pt.y, p); if (h != -1) { activated[p] = h; pt.focus = h; } }
    else if (findItem(pt.focus, p)) activated[p] = pt.focus;
  }
  if ((in.down & BTN_A) && findItem(pt.focus, p)) activated[p] = pt.focus;
  // autoplay for automated testing
  if (g_autoplay && !itemsPrev.empty()) {
    g_autoTimer += g_dt;
    static float autoDelay = getenv("KK_AUTODELAY") ? (float)atof(getenv("KK_AUTODELAY")) : 0.45f;
    if (g_autoTimer > autoDelay) {
      std::vector<int> cand;
      for (auto& it : itemsPrev) if (eligible(it, p) && !it.noAuto) cand.push_back(it.id);
      if (!cand.empty()) { activated[p] = cand[R.irange((int)cand.size())]; g_autoTimer = 0; }
    }
  }
}

static uint32_t anyDown() {
  uint32_t d = 0;
  for (int i = 0; i < MAX_PADS; i++) if (plat_pad(i).connected) d |= plat_pad(i).down;
  return d;
}

static void endFrameItems() {
  itemsPrev.swap(itemsCur);
  itemsCur.clear();
  for (int p = 0; p < 4; p++) {
    if (!(ctrlMask & (1 << p))) continue;
    if (!findItem(P[p].ptr.focus, p)) {
      P[p].ptr.focus = -1;
      for (auto& it : itemsPrev) if (eligible(it, p)) { P[p].ptr.focus = it.id; break; }
    }
  }
}

// =====================================================================================
// game state
// =====================================================================================
enum Mode { M_TITLE, M_SETTINGS, M_HOWTO, M_LOBBY, M_GAME };
enum Phase { P_BUSY, P_PICK_OWN, P_PICK_OPEN, P_OFFER, P_SWAP, P_MP_CHOOSE, P_RESULTS };
static Mode mode = M_TITLE;
static Phase phase = P_BUSY;
static int roundIdx = 0, toOpen = 0, turn = 0;
static std::vector<int> schedule;
static long curOffer = 0;
static std::vector<long> offerHistory;
static bool showOffer = false; static float offerT = 0;
static bool showMpPanels = false, mpRevealChoices = false;
static int streak = 0; static bool streakSaid = false;
static float phaseTime = 0, idleTime = 0; static bool idleSaid = false;
static int howtoStep = 0;
static bool swapped = false;
static int lastCaseIdx = -1;
static std::string banner, banner2;
static unsigned bannerCol = 0xffffff;
static float titleT = 0;
static float confetti = 0;
static int winnerIdx = -1;
static bool lobbyJoined[4];

static void setPhase(Phase ph) { phase = ph; phaseTime = 0; idleTime = 0; idleSaid = false; }

static int remainingBoard() {
  int n = 0;
  for (auto& c : C) if (!c.opened && c.owner < 0) n++;
  return n;
}
static bool amountInPlay(int ai) {
  for (auto& c : C) if (c.amt == ai) return !c.revealed;
  return false;
}
static double expectedValue() {
  double s = 0; int n = 0;
  for (auto& c : C) if (!c.revealed) { s += AMOUNTS[c.amt] / 100.0; n++; }
  return n ? s / n : 0;
}
static long bankerOffer(int rIdx, float mood) {
  static const float F[] = {0.16f, 0.26f, 0.36f, 0.46f, 0.56f, 0.66f, 0.76f, 0.86f, 0.94f, 0.98f};
  float f = F[rIdx < 9 ? rIdx : 9] * R.range(0.9f, 1.1f) * mood;
  if (f > 1.02f) f = 1.02f;
  return round2sig(expectedValue() * f) * 100;
}

static void startRound();
static void bankerCall();
static void finalSwap();
static void revealOwnAndFinish(bool afterDeal);
static void mpOffers();
static void mpEnd();
static void showResults();

// ---------------------------------------------------------------- opening a case
static void openCase(int ci) {
  setPhase(P_BUSY);
  CaseObj& c = C[ci];
  c.opened = true;
  lastCaseIdx = ci;
  int amt = c.amt;
  long cents = AMOUNTS[amt];
  sfx_play("sfx_select");
  sfx_play("sfx_whoosh", 0.6f);
  qCall([ci] { camCase(ci); });
  if (R.f() < 0.6f) qSay("open_" + std::to_string(c.num));
  qWait(0.5f);
  qVoice();
  // suspense
  int remainingHigh = 0;
  for (int a = 20; a < 26; a++) if (amountInPlay(a)) remainingHigh++;
  bool drum = (toOpen == 1 && roundIdx >= 2) || R.f() < 0.15f;
  if (drum) qSfx("sfx_drumroll", 0.8f);
  if (R.f() < 0.55f || drum) { qSay("suspense"); qVoice(); }
  if (drum) qWait(0.9f);
  qCall([ci] { sfx_play("sfx_open"); C[ci].lidTarget = 1; });
  qWait(0.55f);
  // determine reaction tier relative to what's left
  int below = 0, n = 0;
  for (int a = 0; a < 26; a++) if (amountInPlay(a)) { n++; if (a < amt) below++; }
  float frac = n > 1 ? (float)below / (n - 1) : 0.5f;
  float score = frac * 0.6f + amt / 25.0f * 0.4f;
  int tier = score < 0.25f ? 0 : (score < 0.42f ? 1 : (score < 0.58f ? 2 : (score < 0.75f ? 3 : 4)));
  qCall([ci, tier] {
    C[ci].revealed = true;
    C[ci].flash = 1;
    if (tier <= 1) { sfx_play("sfx_good"); if (tier == 0) sfx_play("sfx_applause", 0.5f); }
    else if (tier == 2) sfx_play("sfx_neutral");
    else { sfx_play("sfx_bad"); sfx_play("sfx_aww", 0.7f); g_shake = tier == 4 ? 0.6f : 0.3f; }
  });
  qSayAmount(cents);
  qVoice();
  static const char* REACT[] = {"react_great", "react_good", "react_meh", "react_bad", "react_awful"};
  if (tier <= 1) streak++; else { streak = 0; streakSaid = false; }
  bool sayStreak = streak >= 3 && !streakSaid;
  if (sayStreak) streakSaid = true;
  if (sayStreak) qSay("streak");
  else if (R.f() < 0.85f || tier >= 3) qSay(REACT[tier]);
  qWait(0.3f);
  qVoice();
  qCall([ci] { C[ci].dim = 1; camOverview(); });
  toOpen--;
  qCall([] {
    if (MP) turn = (turn + 1) % NP;
    if (toOpen <= 0 || (MP && remainingBoard() == 0)) {
      if (MP) mpOffers(); else bankerCall();
    } else {
      if (MP) {
        if (R.f() < 0.55f) say("mp_turn_" + std::to_string(turn + 1));
      } else if (R.f() < 0.5f) say("left_" + std::to_string(toOpen < 5 ? toOpen : 5));
      setPhase(P_PICK_OPEN);
    }
  });
}

// ---------------------------------------------------------------- flow: solo
static void beginGame(bool multi, int players) {
  MP = multi;
  NP = multi ? players : 1;
  resetCases();
  for (int i = 0; i < 4; i++) {
    int pad = P[i].pad;
    Pointer ptr = P[i].ptr;
    P[i] = Player();
    P[i].pad = multi ? pad : -1;
    P[i].ptr = ptr;
    P[i].mood = R.range(0.92f, 1.08f);
  }
  roundIdx = 0; turn = 0; streak = 0; streakSaid = false; swapped = false;
  offerHistory.clear(); showOffer = false; showMpPanels = false; mpRevealChoices = false;
  winnerIdx = -1; confetti = 0;
  mode = M_GAME;
  setPhase(P_BUSY);
  Q.clear();
  voice_clear();
  music_play("music_tension", 2.0f);
  camOverview();
  g_tintTarget = V3(0, 0, 0);
  // opening schedule
  schedule.clear();
  int boardCount = 26 - NP;
  static const int base[] = {6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1};
  int sum = 0;
  int target = MP ? boardCount : boardCount - 1;  // solo: keep one case for the final swap
  for (int i = 0; i < 12 && sum < target; i++) {
    int k = base[i];
    if (sum + k > target) k = target - sum;
    schedule.push_back(k);
    sum += k;
  }
  sfx_play("sfx_applause", 0.7f);
  if (!MP) {
    qSay("welcome");
    qVoice();
    if (!S.explained) {
      for (int i = 1; i <= 5; i++) qSay("rules" + std::to_string(i));
      qVoice();
      qCall([] { S.explained = true; saveSettings(); });
    }
    qSay("pick_own");
    qCall([] { setPhase(P_PICK_OWN); });
  } else {
    qSay("mp_welcome");
    qVoice();
    if (!S.explained) { qSay("mp_rules"); qVoice(); }
    qCall([] { turn = 0; say("mp_pick_1"); setPhase(P_PICK_OWN); });
  }
}

static void pickOwn(int ci) {
  setPhase(P_BUSY);
  int p = MP ? turn : 0;
  CaseObj& c = C[ci];
  c.owner = p;
  P[p].caseIdx = ci;
  sfx_play("sfx_select");
  sfx_play("sfx_whoosh", 0.7f);
  V3 to = pedestalPos(p, NP) + V3(0, PED_H, 0);
  moveCase(c, to, 0);
  if (!MP) {
    qSay("own_" + std::to_string(c.num));
    qVoice();
    qCall([] { startRound(); });
  } else {
    qSay("own_" + std::to_string(c.num));
    qVoice();
    qCall([] {
      turn++;
      if (turn < NP) { say("mp_pick_" + std::to_string(turn + 1)); setPhase(P_PICK_OWN); }
      else { turn = 0; startRound(); }
    });
  }
}

static void startRound() {
  if (roundIdx >= (int)schedule.size()) {
    if (MP) mpEnd(); else finalSwap();
    return;
  }
  toOpen = schedule[roundIdx];
  camOverview();
  music_play("music_tension", 1.5f);
  g_tintTarget = V3(0, 0, 0);
  say("round_" + std::to_string(toOpen < 6 ? toOpen : 6));
  if (MP) say("mp_turn_" + std::to_string(turn + 1));
  setPhase(P_PICK_OPEN);
}

static void bankerCall() {
  setPhase(P_BUSY);
  music_play("music_banker", 1.0f);
  g_tintTarget = V3(0.6f, 0.05f, 0.1f);
  if (amountInPlay(25) && roundIdx >= 2 && R.f() < 0.4f) { qSay("million_alive"); qVoice(); }
  qCall([] { camBanker(); sfx_play("sfx_phone", 0.8f); g_lampBlink = 3.0f; });
  qWait(1.2f);
  qSay("banker_call");
  qVoice();
  qCall([] {
    curOffer = bankerOffer(roundIdx, 1.0f);
    showOffer = true; offerT = 0;
    sfx_play("sfx_sting", 0.8f);
    camOverview();
    cam.gpos = V3(0, 3.2f, 6.8f);
  });
  qSay("offer_pre");
  qCall([] { sayAmount(curOffer); });
  qVoice();
  qCall([] {
    double ratio = curOffer / 100.0 / (expectedValue() + 0.01);
    if (R.f() < 0.6f) say(ratio < 0.45 ? "offer_low" : (ratio < 0.8 ? "offer_fair" : "offer_high"));
    say("offer_q");
    setPhase(P_OFFER);
  });
}

static void takeDeal() {
  setPhase(P_BUSY);
  voice_clear();
  Q.clear();
  P[0].dealt = true;
  P[0].deal = curOffer;
  offerHistory.push_back(curOffer);
  sfx_play("sfx_fanfare");
  sfx_play("sfx_applause", 0.8f);
  confetti = 4;
  g_tintTarget = V3(0.5f, 0.4f, 0.05f);
  qSay("take");
  qVoice();
  qCall([] { showOffer = false; revealOwnAndFinish(true); });
}

static void playOn() {
  setPhase(P_BUSY);
  voice_clear();
  Q.clear();
  offerHistory.push_back(curOffer);
  sfx_play("sfx_select");
  qSay("playon");
  qCall([] { showOffer = false; g_tintTarget = V3(0, 0, 0); });
  qVoice();
  qCall([] { roundIdx++; startRound(); });
}

static void finalSwap() {
  setPhase(P_BUSY);
  camOverview();
  music_play("music_tension", 1.0f);
  qSay("final_intro");
  qVoice();
  qSay("swap_q");
  qCall([] { setPhase(P_SWAP); });
}

static int lastBoardCase() {
  for (int i = 0; i < 26; i++) if (!C[i].opened && C[i].owner < 0) return i;
  return -1;
}

static void doSwap(bool swap) {
  setPhase(P_BUSY);
  voice_clear();
  Q.clear();
  sfx_play("sfx_select");
  qSay(swap ? "swap" : "keep");
  if (swap) {
    int other = lastBoardCase();
    int mine = P[0].caseIdx;
    if (other >= 0) {
      swapped = true;
      qCall([other, mine] {
        CaseObj& a = C[other]; CaseObj& b = C[mine];
        V3 pa = a.pos; float ya = a.yaw;
        moveCase(a, b.pos, b.yaw);
        moveCase(b, pa, ya);
        a.owner = 0; b.owner = -1;
        P[0].caseIdx = other;
        sfx_play("sfx_whoosh");
      });
      qWait(1.4f);
    }
  }
  qVoice();
  qCall([] { revealOwnAndFinish(false); });
}

static void revealOwnAndFinish(bool afterDeal) {
  setPhase(P_BUSY);
  int ci = P[0].caseIdx;
  long own = AMOUNTS[C[ci].amt];
  qCall([ci] { camCase(ci); music_play("music_tension", 1.0f); });
  qSay("reveal_own");
  qSfx("sfx_drumroll", 0.9f);
  qWait(1.6f);
  qVoice();
  qCall([ci] { C[ci].opened = true; C[ci].lidTarget = 1; sfx_play("sfx_open"); });
  qWait(0.5f);
  qCall([ci, own, afterDeal] {
    C[ci].revealed = true; C[ci].flash = 1;
    bool good = afterDeal ? (P[0].deal >= own) : (own >= 5000000);
    sfx_play(good ? "sfx_fanfare" : "sfx_bad");
    if (good) { sfx_play("sfx_applause", 0.8f); confetti = 4; } else sfx_play("sfx_aww", 0.7f);
  });
  qSayAmount(own);
  qVoice();
  if (afterDeal) qCall([own] { say(P[0].deal >= own ? "verdict_good" : "verdict_bad"); });
  qVoice();
  if (!afterDeal) {
    // also open the last other case for closure
    int other = -1;
    for (int i = 0; i < 26; i++) if (!C[i].opened && i != ci) other = i;
    if (other >= 0) qCall([other] { C[other].opened = true; C[other].lidTarget = 1; C[other].revealed = true; sfx_play("sfx_open", 0.6f); });
  } else {
    // open everything that's left so the player sees where the big money was
    qCall([] { for (auto& c : C) if (!c.opened) { c.opened = true; c.lidTarget = 1; c.revealed = true; } sfx_play("sfx_open", 0.6f); });
  }
  qWait(0.8f);
  qCall([] { showResults(); });
}

static void showResults() {
  setPhase(P_BUSY);
  camOverview();
  music_play("music_menu", 2.0f);
  g_tintTarget = V3(0.3f, 0.25f, 0.05f);
  if (!MP) {
    long won = P[0].dealt ? P[0].deal : AMOUNTS[C[P[0].caseIdx].amt];
    P[0].finalAmt = won;
    say(won >= 10000000 ? "win_big" : (won >= 500000 ? "win_mid" : "win_small"));
  }
  say("goodbye");
  setPhase(P_RESULTS);
}

// ---------------------------------------------------------------- flow: party mode
static bool allChosen() {
  for (int p = 0; p < NP; p++) if (!P[p].dealt && P[p].choice < 0) return false;
  return true;
}
static int activePlayers() { int n = 0; for (int p = 0; p < NP; p++) if (!P[p].dealt) n++; return n; }

static void mpOffers() {
  setPhase(P_BUSY);
  if (activePlayers() == 0) { roundIdx++; startRound(); return; }
  music_play("music_banker", 1.0f);
  g_tintTarget = V3(0.6f, 0.05f, 0.1f);
  qCall([] { camBanker(); sfx_play("sfx_phone", 0.8f); g_lampBlink = 3.0f; });
  qWait(1.2f);
  qSay("mp_offer_all");
  qVoice();
  qCall([] {
    for (int p = 0; p < NP; p++) {
      P[p].choice = -1;
      if (!P[p].dealt) P[p].offer = bankerOffer(roundIdx, P[p].mood * R.range(0.95f, 1.05f));
    }
    showMpPanels = true; mpRevealChoices = false;
    camOverview();
    cam.gpos = V3(0, 4.4f, 9.6f);
    sfx_play("sfx_sting", 0.7f);
    setPhase(P_MP_CHOOSE);
    for (int p = 0; p < NP; p++) {
      if (P[p].dealt) continue;
      say("mp_offer_" + std::to_string(p + 1));
      sayAmount(P[p].offer);
    }
    say("mp_choose_now");
  });
}

static void mpResolve() {
  setPhase(P_BUSY);
  qWait(0.4f);
  qSay("mp_reveal_choices");
  qSfx("sfx_drumroll", 0.8f);
  qWait(1.8f);
  qVoice();
  qCall([] {
    mpRevealChoices = true;
    bool any = false;
    for (int p = 0; p < NP; p++) {
      if (P[p].dealt || P[p].choice != 0) continue;
      any = true;
      say("mp_took_" + std::to_string(p + 1));
    }
    if (any) { sfx_play("sfx_fanfare", 0.8f); sfx_play("sfx_applause", 0.6f); }
    else { say("mp_all_playon"); sfx_play("sfx_aww", 0.4f); }
  });
  qVoice();
  qWait(1.0f);
  qCall([] {
    for (int p = 0; p < NP; p++) {
      if (P[p].dealt || P[p].choice != 0) continue;
      P[p].dealt = true; P[p].deal = P[p].offer;
    }
    showMpPanels = false;
    g_tintTarget = V3(0, 0, 0);
    roundIdx++;
    if (activePlayers() == 0 || remainingBoard() == 0) mpEnd();
    else startRound();
  });
}

static void mpEnd() {
  setPhase(P_BUSY);
  showMpPanels = false;
  camOverview();
  music_play("music_tension", 1.0f);
  qSay("mp_end");
  qVoice();
  for (int p = 0; p < NP; p++) {
    int ci = P[p].caseIdx;
    qCall([ci] { camCase(ci); });
    qSay("mp_case_" + std::to_string(p + 1));
    qWait(0.6f);
    qVoice();
    qCall([ci] { C[ci].opened = true; C[ci].lidTarget = 1; sfx_play("sfx_open"); });
    qWait(0.5f);
    qCall([ci] { C[ci].revealed = true; C[ci].flash = 1; sfx_play(C[ci].amt >= 18 ? "sfx_good" : "sfx_neutral"); });
    qSayAmount(AMOUNTS[C[ci].amt]);
    qVoice();
    qWait(0.4f);
  }
  qCall([] {
    camOverview();
    long best = -1; int bi = -1; bool tie = false;
    for (int p = 0; p < NP; p++) {
      P[p].finalAmt = P[p].dealt ? P[p].deal : AMOUNTS[C[P[p].caseIdx].amt];
      if (P[p].finalAmt > best) { best = P[p].finalAmt; bi = p; tie = false; }
      else if (P[p].finalAmt == best) tie = true;
    }
    winnerIdx = tie ? -1 : bi;
    sfx_play("sfx_fanfare"); sfx_play("sfx_applause"); confetti = 5;
    if (tie) say("mp_tie"); else say("mp_winner_" + std::to_string(bi + 1));
  });
  qVoice();
  qCall([] { showResults(); });
}

// =====================================================================================
// rendering helpers
// =====================================================================================
static Material mat(unsigned hex, float metal, float gloss, V3 emis = V3(0, 0, 0), float a = 1) {
  Material m; m.color = hexc(hex, a); m.metal = metal; m.gloss = gloss; m.emissive = emis; return m;
}

static void drawCase(CaseObj& c, bool refl) {
  float lift = c.hover * (0.07f + 0.02f * sinf(g_time * 6));
  float sc = c.owner >= 0 ? 1.15f : 1.0f;
  M4 base = translate(c.pos + V3(0, lift, 0)) * rotY(c.yaw) * scale(V3(sc, sc, sc));
  float dimK = 1.0f - 0.45f * c.dim;
  unsigned bodyHex = c.owner >= 0 ? 0xe9b949 : 0xc8ccd6;
  if (MP && c.owner >= 0) bodyHex = PCOL[c.owner];
  Material body = mat(bodyHex, 0.92f, 0.78f);
  body.color.r *= dimK; body.color.g *= dimK; body.color.b *= dimK;
  float glow = c.hover * (0.55f + 0.45f * sinf(g_time * 8));
  body.emissive = V3(0.10f, 0.28f, 0.55f) * glow + V3(1.0f, 0.85f, 0.4f) * (c.flash * 0.6f);
  gfx_drawMesh(mShellBack, base, body);
  gfx_drawMesh(mSeam, base, mat(0x23232b, 0.3f, 0.4f));
  gfx_drawMesh(mHandle, base, mat(0x1d1d24, 0.2f, 0.5f));
  float ang = -c.lid * 2.0f;  // ~115 degrees
  M4 lidM = base * translate(V3(0, CH, 0)) * rotX(ang);
  gfx_drawMesh(mShellFront, lidM, body);
  gfx_drawMesh(mLatch, lidM, mat(0xf0c75e, 1.0f, 0.85f));
  gfx_drawMesh(mPlate, lidM, mat(0xf4f1ea, 0.0f, 0.35f, V3(0.05f, 0.05f, 0.05f)));
  if (c.lid > 0.01f) {
    gfx_drawMesh(mInner, base, mat(0x401028, 0.0f, 0.15f));
    Material pl = mat(0xf2c94c, 0.9f, 0.7f, V3(0.25f, 0.18f, 0.02f) * c.flash);
    if (c.amt >= 19) pl = mat(0xff5a3c, 0.6f, 0.7f, V3(0.3f, 0.05f, 0.02f) * c.flash);
    else if (c.amt <= 12) pl = mat(0x4fa3ff, 0.6f, 0.7f, V3(0.02f, 0.1f, 0.3f) * c.flash);
    gfx_drawMesh(mPlaque, base, pl);
  }
  if (refl) return;
  char num[8];
  snprintf(num, sizeof(num), "%d", c.num);
  text_draw3D(num, lidM * translate(V3(0, -CH * 0.56f, CD / 2 + 0.011f)), 0.15f, hexc(0x14193a));
  if (c.lid > 0.05f) {
    std::string a = fmtAmount(AMOUNTS[c.amt]);
    float w1 = text_width(a.c_str(), 1.0f);
    float sz = fminf(0.09f, 0.46f / fmaxf(w1, 0.01f));
    text_draw3D(a.c_str(), base * translate(V3(0, CH / 2, 0.018f)), sz, hexc(0x111111));
  }
}

static void drawObjects(bool refl) {
  // stands
  for (int t = 0; t < 3; t++) {
    gfx_drawMesh(mTier[t], M4::id(), mat(0x1b1834, 0.3f, 0.6f));
    float pulse = 0.7f + 0.3f * sinf(g_time * 2 + t);
    V3 led = lerp3(V3(0.2f, 0.45f, 1.2f), g_tint * 2.0f + V3(0.2f, 0.1f, 0.2f), 0.5f) * pulse;
    gfx_drawMesh(mTierLed[t], M4::id(), mat(0x000000, 0, 0, led));
  }
  // pedestals
  for (int p = 0; p < NP; p++) {
    V3 pp = pedestalPos(p, NP);
    M4 m = translate(pp);
    gfx_drawMesh(mPedestal, m, mat(0x24203d, 0.6f, 0.7f));
    Col pc = hexc(MP ? PCOL[p] : 0xffc94a);
    gfx_drawMesh(mPedRing, m, mat(0, 0, 0, V3(pc.r, pc.g, pc.b) * (1.2f + 0.4f * sinf(g_time * 3 + p))));
  }
  // banker booth
  {
    M4 bm = translate(V3(0, 4.7f, -8.6f));
    gfx_drawMesh(mBoothFrame, bm, mat(0x15131f, 0.7f, 0.6f));
    float calling = g_lampBlink > 0 ? 1.0f : 0.0f;
    V3 glassE = lerp3(V3(0.9f, 0.55f, 0.25f), V3(1.6f, 0.3f, 0.2f), calling) * (0.9f + 0.1f * sinf(g_time * 3));
    gfx_drawMesh(mBoothGlass, bm, mat(0x000000, 0, 0.2f, glassE));
    gfx_drawMesh(mSilBody, bm, mat(0x050505, 0, 0.1f));
    gfx_drawMesh(mSilHead, bm, mat(0x050505, 0, 0.1f));
    gfx_drawMesh(mDesk, bm, mat(0x0b0a10, 0.3f, 0.5f));
    float blink = (g_lampBlink > 0 && fmodf(g_time, 0.5f) < 0.25f) ? 3.0f : 0.25f;
    gfx_drawMesh(mLamp, bm * translate(V3(0, 1.22f, 0)), mat(0x300000, 0, 0.8f, V3(1.0f, 0.1f, 0.05f) * blink));
  }
  // cases (closed ones first so the transparent text renders over solids)
  for (auto& c : C) drawCase(c, refl);
}

static void drawWorld() {
  int w = plat_width(), h = plat_height();
  gfx_beginFrame(w, h);
  V3 shake(0, 0, 0);
  if (g_shake > 0) shake = V3(R.range(-1, 1), R.range(-1, 1), 0) * (g_shake * 0.04f);
  M4 view = lookAt(cam.pos + shake, cam.tgt, V3(0, 1, 0));
  M4 proj = perspective(38.0f * PI / 180.0f, (float)w / (float)h, 0.1f, 80.0f);
  gfx_setCamera(view, proj, cam.pos);
  gfx_setLights(g_spotPos, g_spotCol, g_time, g_tint);
  gfx_drawBackground(mWall, g_time, g_tint, g_energy);
  gfx_setMirror(true);
  drawObjects(true);
  gfx_setMirror(false);
  gfx_drawFloor(mFloor, g_time, g_tint);
  drawObjects(false);
  // light beams
  for (int i = 0; i < 6; i++) {
    float x = -7.5f + i * 3.0f;
    float sway = sinf(g_time * 0.5f + i * 1.3f) * 0.35f;
    M4 m = translate(V3(x, 11.0f, -2.0f + (i % 2) * 2.5f)) * rotZ(sway) * rotX(0.15f * cosf(g_time * 0.4f + i));
    Col bc = (i % 3 == 0) ? rgba(0.55f, 0.3f, 1.0f, 0.10f) : (i % 3 == 1 ? rgba(0.3f, 0.7f, 1.0f, 0.09f) : rgba(1.0f, 0.85f, 0.6f, 0.08f));
    bc.r += g_tint.x * 0.2f; bc.g += g_tint.y * 0.2f; bc.b += g_tint.z * 0.2f;
    gfx_drawBeam(mBeam, m, bc);
  }
}

// ---------------------------------------------------------------- UI widgets
static bool button(const char* key, float x, float y, float w, float h, const char* label, unsigned hex, int owner = -1,
                   float size = 30, bool noAuto = false, bool enabled = true) {
  int id = HID(key) + (owner >= 0 ? owner * 7919 : 0);
  if (enabled) addItem(id, x, y, w, h, owner, noAuto);
  bool f = enabled && focused(id, owner);
  Col c = hexc(hex);
  if (!enabled) c = hexc(0x444455);
  float pop = f ? 1.0f : 0.0f;
  float g = 4 * pop;
  if (f) ui_glow(x - g, y - g, w + 2 * g, h + 2 * g, withA(c, 0.8f), h * 0.4f, 22);
  ui_rect(x - g, y - g, w + 2 * g, h + 2 * g, f ? hexc(0xffffff) : rgba(1, 1, 1, 0.25f), h * 0.42f + g);
  ui_gradRect(x - g + 3, y - g + 3, w + 2 * g - 6, h + 2 * g - 6, mixc(c, hexc(0xffffff), 0.25f), mixc(c, hexc(0x000000), 0.35f), h * 0.42f + g - 3);
  text_draw(label, x + w / 2, y + h / 2, size + pop * 2, enabled ? hexc(0xffffff) : hexc(0x8888aa), 1, 2.5f);
  return enabled && wasActivated(id, owner);
}

static void drawBoard() {
  for (int side = 0; side < 2; side++) {
    float x = side == 0 ? 14.0f : 1280 - 14.0f - 176.0f;
    for (int i = 0; i < 13; i++) {
      int ai = side * 13 + i;
      float y = 66.0f + i * 45.5f;
      bool live = amountInPlay(ai);
      // flash newly revealed
      float fl = 0;
      for (auto& c : C) if (c.amt == ai && c.revealed) fl = c.flash;
      Col top = side == 0 ? hexc(0x4f8dff) : hexc(0xffcf4a);
      Col bot = side == 0 ? hexc(0x1d3fa8) : hexc(0xd27a00);
      if (!live) { top = hexc(0x262636, 0.85f); bot = hexc(0x1a1a26, 0.85f); }
      if (fl > 0) { top = mixc(top, hexc(0xffffff), fl); bot = mixc(bot, hexc(0xffffff), fl * 0.6f); }
      if (live) ui_glow(x, y, 176, 38, withA(top, 0.25f), 14, 10);
      ui_rect(x, y, 176, 38, live ? rgba(1, 1, 1, 0.35f) : rgba(1, 1, 1, 0.06f), 19);
      ui_gradRect(x + 2, y + 2, 172, 34, top, bot, 17);
      std::string s = fmtAmount(AMOUNTS[ai]);
      text_draw(s.c_str(), x + 88, y + 19, 23, live ? hexc(0xffffff) : hexc(0x5a5a70), 1, live ? 2.0f : 0.0f);
      if (!live) ui_rect(x + 20, y + 18, 136, 2.5f, hexc(0xff4d5e, 0.55f), 1);
    }
  }
}

static void drawBanner() {
  if (banner.empty()) return;
  float w = fmaxf(text_width(banner.c_str(), 28) + 70, 300);
  float x = 640 - w / 2;
  Col bc = hexc(bannerCol);
  ui_glow(x, 10, w, 46, withA(bc, 0.35f), 23, 16);
  ui_rect(x, 10, w, 46, hexc(0x0c0a1c, 0.85f), 23, 2.5f, withA(bc, 0.9f));
  text_draw(banner.c_str(), 640, 33, 28, hexc(0xffffff), 1);
  if (!banner2.empty()) text_draw(banner2.c_str(), 640, 70, 20, hexc(0xc8c8e8), 1);
}

static void drawSubtitles() {
  float a = voice_subtitleAlpha();
  if (!S.subs || a < 0.02f || voice_subtitle().empty()) return;
  const std::string& s = voice_subtitle();
  float w = 820;
  int lines = text_wrapLines(s.c_str(), w - 110, 25);
  float h = 26.0f + lines * 30.0f;
  float x = 640 - w / 2, y = 672 - h;
  ui_rect(x, y, w, h, hexc(0x07061a, 0.78f * a), 18, 2, hexc(0x8a6cff, 0.6f * a));
  float lv = voice_level();
  float mx = x + 40, my = y + h / 2;
  ui_circle(mx, my, 19 + lv * 10, hexc(0xffc94a, 0.18f * a));
  ui_rect(mx - 7, my - 16, 14, 22, hexc(0xffc94a, a), 7);
  ui_circle(mx, my - 2, 11, hexc(0xffc94a, a), 2.5f);
  ui_rect(mx - 1.5f, my + 9, 3, 7, hexc(0xffc94a, a), 1);
  ui_rect(mx - 7, my + 15, 14, 3, hexc(0xffc94a, a), 1.5f);
  text_drawWrapped(s.c_str(), x + 75, y + 14 + 15, w - 100, 25, hexc(0xffffff, a), 0, 1.2f);
}

static void drawHints() {
  const char* h = T("Wijs + ZL/ZR of joystick + A: kiezen     L/R: aanwijzer centreren     B: overslaan",
                    "Point + ZL/ZR or stick + A: choose     L/R: recenter pointer     B: skip");
  text_draw(h, 640, 703, 15, hexc(0x9a96c0, 0.85f), 1, 0);
}

static void drawCursors() {
  for (int p = 0; p < 4; p++) {
    if (!(ctrlMask & (1 << p))) continue;
    Pointer& pt = P[p].ptr;
    if (pt.vis <= 0 || !S.gyro) continue;
    Col c = MP ? hexc(PCOL[p]) : hexc(0x6ff3ff);
    float pulse = 1 + 0.08f * sinf(g_time * 8);
    ui_glow(pt.x - 14, pt.y - 14, 28, 28, withA(c, 0.6f), 14, 14);
    ui_circle(pt.x, pt.y, 15 * pulse, rgba(1, 1, 1, 0.95f), 4);
    ui_circle(pt.x, pt.y, 6, c);
    if (MP) {
      char t[4]; snprintf(t, sizeof(t), "%d", p + 1);
      text_draw(t, pt.x + 20, pt.y - 18, 20, c, 0, 2);
    }
  }
}

static void drawConfetti() {
  if (confetti <= 0) return;
  for (int i = 0; i < 90; i++) {
    float seed = i * 12.9898f;
    float fx = fmodf(sinf(seed) * 43758.5453f, 1.0f); if (fx < 0) fx += 1;
    float sp = 120 + fmodf(fabsf(cosf(seed) * 9999.0f), 160.0f);
    float t = 5 - confetti;
    float y = -20 + fmodf(t * sp + i * 37.0f, 760.0f);
    float x = fx * 1280 + sinf(t * 3 + i) * 30;
    unsigned cols[] = {0xff4d5e, 0x3d8bff, 0xffd23d, 0x3ddc84, 0xff8af0, 0xffffff};
    ui_rect(x, y, 10, 6 + 4 * sinf(t * 6 + i), hexc(cols[i % 6], fminf(1.0f, confetti)), 2);
  }
}

// ---------------------------------------------------------------- screens
static void caseItems(int owner) {
  for (int i = 0; i < 26; i++) {
    CaseObj& c = C[i];
    if (c.opened || c.owner >= 0 || c.moving) continue;
    V2 a = gfx_project(c.pos + V3(0, 0.0f, 0));
    V2 b = gfx_project(c.pos + V3(0, CH + 0.08f, 0));
    float hgt = fabsf(a.y - b.y);
    float wdt = hgt * 1.5f;
    addItem(1000 + i, b.x - wdt / 2, b.y, wdt, hgt, owner);
  }
}

static void gameUI() {
  drawBoard();
  // status banner
  banner.clear(); banner2.clear(); bannerCol = 0x8a6cff;
  int cp = MP ? turn : 0;
  if (phase == P_PICK_OWN) {
    banner = MP ? std::string(T("Speler ", "Player ")) + std::to_string(turn + 1) + T(": kies je eigen koffer", ": pick your own case")
                : T("Kies je eigen koffer", "Pick your own case");
    banner2 = T("Deze koffer blijft dicht tot het einde", "This case stays closed until the end");
    if (MP) bannerCol = PCOL[turn];
  } else if (phase == P_PICK_OPEN) {
    char b[160];
    snprintf(b, sizeof(b), T("Ronde %d  \xE2\x80\xA2  open nog %d koffer%s", "Round %d  \xE2\x80\xA2  open %d more case%s"), roundIdx + 1,
             toOpen, toOpen == 1 ? "" : (NL() ? "s" : "s"));
    banner = b;
    if (MP) { banner2 = std::string(T("Speler ", "Player ")) + std::to_string(turn + 1) + T(" is aan de beurt", "'s turn"); bannerCol = PCOL[turn]; }
    else banner2 = T("Lage bedragen openen is goed nieuws!", "Opening low amounts is good news!");
  } else if (phase == P_SWAP) {
    banner = T("Houden of ruilen?", "Keep or swap?");
  }
  drawBanner();

  if (phase == P_PICK_OWN || phase == P_PICK_OPEN) {
    ctrlMask = MP ? (1 << cp) : 1;
    caseItems(-1);
    for (int i = 0; i < 26; i++) {
      if (wasActivated(1000 + i)) {
        if (phase == P_PICK_OWN) pickOwn(i); else openCase(i);
        break;
      }
    }
  }

  // offer card (solo)
  if (showOffer) {
    offerT += g_dt;
    float k = easeOutBack(offerT * 2.0f);
    float w = 600 * k, h = 330 * k;
    float x = 640 - w / 2, y = 300 - h / 2;
    ui_glow(x, y, w, h, hexc(0xff3b5c, 0.45f), 30, 40);
    ui_rect(x, y, w, h, hexc(0x0d0a20, 0.94f), 30, 3, hexc(0xff6b81));
    if (k > 0.9f) {
      text_draw(T("DE BANKIER BIEDT", "THE BANKER OFFERS"), 640, y + 44, 30, hexc(0xff9db0), 1);
      float cnt = sat(offerT * 1.4f - 0.3f);
      long shown = (long)(curOffer * easeOutCubic(cnt));
      shown = cnt >= 1 ? curOffer : round2sig(shown / 100.0) * 100;
      std::string a = fmtAmount(shown < 1 ? 1 : shown);
      ui_glow(640 - 220, y + 78, 440, 90, hexc(0xffc94a, 0.25f), 40, 30);
      text_draw(a.c_str(), 640, y + 122, 74, hexc(0xffd75e), 1, 3);
      if (!offerHistory.empty()) {
        std::string hs = T("Vorige biedingen: ", "Previous offers: ");
        for (size_t i = 0; i < offerHistory.size(); i++) { if (i) hs += "  "; hs += fmtAmount(offerHistory[i]); }
        text_draw(hs.c_str(), 640, y + 182, 18, hexc(0xaaaacc), 1, 0);
      }
      if (phase == P_OFFER) {
        ctrlMask = 1;
        if (button("take", x + 40, y + h - 110, 245, 78, T("BOD AANNEMEN", "TAKE IT"), 0x22b45a, -1, 28)) takeDeal();
        else if (button("playon", x + w - 285, y + h - 110, 245, 78, T("DOORSPELEN", "PLAY ON"), 0xe0324f, -1, 28)) playOn();
      }
    }
  }

  if (phase == P_SWAP) {
    ctrlMask = 1;
    float x = 640 - 280, y = 420;
    ui_rect(x - 20, y - 70, 600, 190, hexc(0x0d0a20, 0.9f), 26, 2, hexc(0x8a6cff));
    text_draw(T("Je eigen koffer houden, of ruilen?", "Keep your own case, or swap?"), 640, y - 34, 26, hexc(0xffffff), 1);
    if (button("keep", x, y, 260, 84, T("HOUDEN", "KEEP"), 0xe9b949, -1, 32)) doSwap(false);
    else if (button("swap", x + 300, y, 260, 84, T("RUILEN", "SWAP"), 0x3d8bff, -1, 32)) doSwap(true);
  }

  // party offer panels
  if (showMpPanels) {
    int act = 0;
    ctrlMask = 0;
    for (int p = 0; p < NP; p++) if (!P[p].dealt && phase == P_MP_CHOOSE) ctrlMask |= 1 << p;
    float pw = 250, gap = 16;
    float total = NP * pw + (NP - 1) * gap;
    float x0 = 640 - total / 2;
    for (int p = 0; p < NP; p++) {
      float x = x0 + p * (pw + gap), y = 318;
      Col pc = hexc(PCOL[p]);
      ui_glow(x, y, pw, 258, withA(pc, 0.3f), 22, 18);
      ui_rect(x, y, pw, 258, hexc(0x0d0a20, 0.93f), 22, 3, pc);
      char t[48]; snprintf(t, sizeof(t), "%s %d", T("SPELER", "PLAYER"), p + 1);
      text_draw(t, x + pw / 2, y + 28, 26, pc, 1);
      if (P[p].dealt && !(mpRevealChoices && P[p].choice == 0)) {
        text_draw(T("Heeft al een deal:", "Already dealt:"), x + pw / 2, y + 90, 20, hexc(0xaaaacc), 1, 0);
        text_draw(fmtAmount(P[p].deal).c_str(), x + pw / 2, y + 130, 34, hexc(0xffd75e), 1);
        continue;
      }
      act++;
      text_draw(T("Bod:", "Offer:"), x + pw / 2, y + 66, 20, hexc(0xaaaacc), 1, 0);
      text_draw(fmtAmount(P[p].offer).c_str(), x + pw / 2, y + 104, 36, hexc(0xffd75e), 1);
      if (mpRevealChoices) {
        bool took = P[p].choice == 0;
        ui_rect(x + 20, y + 160, pw - 40, 80, hexc(took ? 0x22b45a : 0xe0324f), 20);
        text_draw(took ? T("AANGENOMEN!", "TAKEN!") : T("SPEELT DOOR", "PLAYS ON"), x + pw / 2, y + 200, 26, hexc(0xffffff), 1);
      } else if (P[p].choice >= 0) {
        ui_rect(x + 20, y + 160, pw - 40, 80, hexc(0x2a2840), 20);
        text_draw(T("Keuze vastgelegd", "Choice locked in"), x + pw / 2, y + 190, 21, hexc(0xffffff), 1);
        text_draw(T("(geheim!)", "(secret!)"), x + pw / 2, y + 218, 17, hexc(0xaaaacc), 1, 0);
      } else if (phase == P_MP_CHOOSE) {
        if (button("mtake", x + 14, y + 148, pw - 28, 46, T("AANNEMEN", "TAKE IT"), 0x22b45a, p, 23)) { P[p].choice = 0; sfx_play("sfx_lock"); }
        else if (button("mplay", x + 14, y + 202, pw - 28, 46, T("DOORSPELEN", "PLAY ON"), 0xe0324f, p, 23)) { P[p].choice = 1; sfx_play("sfx_lock"); }
      }
    }
    if (phase == P_MP_CHOOSE && allChosen()) {
      voice_clear();
      mpResolve();
    }
    (void)act;
  }

  if (phase == P_RESULTS) {
    ctrlMask = 0;
    if (MP) { for (int p = 0; p < NP; p++) ctrlMask |= 1 << p; } else ctrlMask = 1;
    float w = 640, h = MP ? 150 + NP * 60 : 330;
    float x = 640 - w / 2, y = 330 - h / 2;
    ui_glow(x, y, w, h, hexc(0xffc94a, 0.4f), 30, 40);
    ui_rect(x, y, w, h, hexc(0x0d0a20, 0.94f), 30, 3, hexc(0xffc94a));
    if (!MP) {
      text_draw(T("JE GAAT NAAR HUIS MET", "YOU'RE GOING HOME WITH"), 640, y + 46, 28, hexc(0xffe9a8), 1);
      text_draw(fmtAmount(P[0].finalAmt).c_str(), 640, y + 116, 78, hexc(0xffd75e), 1, 3);
      std::string d = std::string(T("In je koffer zat: ", "Your case held: ")) + fmtAmount(AMOUNTS[C[P[0].caseIdx].amt]);
      text_draw(d.c_str(), 640, y + 176, 22, hexc(0xccccee), 1, 0);
      if (P[0].dealt) {
        std::string d2 = std::string(T("Je verkocht hem voor: ", "You sold it for: ")) + fmtAmount(P[0].deal);
        text_draw(d2.c_str(), 640, y + 206, 22, hexc(0xccccee), 1, 0);
      }
    } else {
      text_draw(winnerIdx >= 0 ? (std::string(T("SPELER ", "PLAYER ")) + std::to_string(winnerIdx + 1) + T(" WINT!", " WINS!")).c_str()
                               : T("GELIJKSPEL!", "IT'S A TIE!"),
                640, y + 44, 40, winnerIdx >= 0 ? hexc(PCOL[winnerIdx]) : hexc(0xffffff), 1);
      std::vector<int> order;
      for (int p = 0; p < NP; p++) order.push_back(p);
      for (size_t i = 0; i < order.size(); i++)
        for (size_t j = i + 1; j < order.size(); j++)
          if (P[order[j]].finalAmt > P[order[i]].finalAmt) { int t = order[i]; order[i] = order[j]; order[j] = t; }
      for (size_t i = 0; i < order.size(); i++) {
        int p = order[i];
        float ry = y + 96 + i * 60;
        ui_rect(x + 40, ry - 24, w - 80, 50, withA(hexc(PCOL[p]), 0.25f), 16);
        char t[96]; snprintf(t, sizeof(t), "%d. %s %d", (int)i + 1, T("Speler", "Player"), p + 1);
        text_draw(t, x + 66, ry, 26, hexc(PCOL[p]), 0);
        std::string how = P[p].dealt ? T("(deal)", "(deal)") : T("(koffer)", "(case)");
        text_draw((fmtAmount(P[p].finalAmt) + "  " + how).c_str(), x + w - 66, ry, 26, hexc(0xffffff), 2);
      }
    }
    float by = y + h - 96;
    if (button("again", x + 40, by, 270, 70, T("NOG EEN KEER", "PLAY AGAIN"), 0x22b45a, -1, 26)) {
      if (MP) beginGame(true, NP); else beginGame(false, 1);
    } else if (button("menu", x + w - 310, by, 270, 70, T("HOOFDMENU", "MAIN MENU"), 0x6a4cff, -1, 26, true)) {
      mode = M_TITLE; MP = false; NP = 1; Q.clear(); voice_clear(); resetCases(); music_play("music_menu");
      g_tintTarget = V3(0, 0, 0); confetti = 0;
    }
  }
  if (phase == P_BUSY) ctrlMask = MP ? 0 : 1;  // nothing focusable while the show runs
}

static void titleUI() {
  ctrlMask = 1;
  MP = false;
  titleT += g_dt;
  float bob = sinf(g_time * 1.6f) * 6;
  ui_glow(340, 70 + bob, 600, 190, hexc(0xff3bd0, 0.25f), 60, 60);
  text_draw(T("KOFFER", "CASE"), 640, 100 + bob, 104, hexc(0xffd75e), 1, 1.6f);
  text_draw(T("KOORTS", "FEVER"), 640, 192 + bob, 104, hexc(0xff6bd6), 1, 1.6f);
  text_draw(T("De spannende koffer-spelshow", "The thrilling briefcase game show"), 640, 262, 24, hexc(0xd8d0ff), 1);
  float bx = 640 - 190, by = 300, bw = 380, bh = 54, gap = 64;
  if (button("solo", bx, by, bw, bh, T("SOLO SPELEN", "PLAY SOLO"), 0x22b45a, -1, 28)) { beginGame(false, 1); return; }
  if (button("party", bx, by + gap, bw, bh, T("FEEST  (2-4 SPELERS)", "PARTY  (2-4 PLAYERS)"), 0xff8a1f, -1, 28, true)) {
    mode = M_LOBBY; for (int i = 0; i < 4; i++) { lobbyJoined[i] = false; P[i].pad = -1; } return;
  }
  if (button("howto", bx, by + gap * 2, bw, bh, T("HOE WERKT HET?", "HOW TO PLAY"), 0x3d8bff, -1, 28, true)) {
    mode = M_HOWTO; howtoStep = 0; voice_clear(); Q.clear();
    for (int i = 1; i <= 5; i++) { int s = i; qCall([s] { howtoStep = s; say("rules" + std::to_string(s)); }); qVoice(); qWait(0.4f); }
    return;
  }
  if (button("settings", bx, by + gap * 3, bw, bh, T("INSTELLINGEN", "SETTINGS"), 0x8a6cff, -1, 28, true)) { mode = M_SETTINGS; say("settings"); return; }
#ifndef __SWITCH__
  if (button("quit", bx, by + gap * 4, bw, bh, T("AFSLUITEN", "QUIT"), 0x555577, -1, 28, true)) g_quit = true;
#else
  if (button("quit", bx, by + gap * 4, bw, bh, T("AFSLUITEN", "QUIT"), 0x555577, -1, 28, true)) g_quit = true;
#endif
  text_draw("v1.0  \xE2\x80\xA2  homebrew", 1268, 708, 14, hexc(0x77779a), 2, 0);
}

static void settingsUI() {
  ctrlMask = 1;
  float x = 300, w = 680, y = 70;
  ui_rect(x, y, w, 580, hexc(0x0d0a20, 0.92f), 26, 3, hexc(0x8a6cff));
  text_draw(T("INSTELLINGEN", "SETTINGS"), 640, y + 40, 36, hexc(0xffffff), 1);
  struct Row { const char* label; std::string value; int* iv; bool* bv; int mn, mx; };
  std::string langV = NL() ? "Nederlands" : "English";
  int langI = S.lang;
  int dummy = 0;
  Row rows[] = {
      {T("Taal", "Language"), langV, &langI, nullptr, 0, 1},
      {T("Gyro-aanwijzer", "Gyro pointer"), S.gyro ? T("Aan", "On") : T("Uit", "Off"), nullptr, &S.gyro, 0, 1},
      {T("Gyro-gevoeligheid", "Gyro sensitivity"), std::to_string(S.sens), &S.sens, nullptr, 1, 10},
      {T("Gyro horizontaal omkeren", "Invert gyro horizontal"), S.invX ? T("Ja", "Yes") : T("Nee", "No"), nullptr, &S.invX, 0, 1},
      {T("Gyro verticaal omkeren", "Invert gyro vertical"), S.invY ? T("Ja", "Yes") : T("Nee", "No"), nullptr, &S.invY, 0, 1},
      {T("Muziek", "Music"), std::to_string(S.volMusic), &S.volMusic, nullptr, 0, 10},
      {T("Presentator", "Presenter"), std::to_string(S.volVoice), &S.volVoice, nullptr, 0, 10},
      {T("Geluidseffecten", "Sound effects"), std::to_string(S.volSfx), &S.volSfx, nullptr, 0, 10},
      {T("Ondertitels", "Subtitles"), S.subs ? T("Aan", "On") : T("Uit", "Off"), nullptr, &S.subs, 0, 1},
  };
  (void)dummy;
  bool changed = false;
  for (int i = 0; i < 9; i++) {
    Row& r = rows[i];
    float ry = y + 90 + i * 50;
    text_draw(r.label, x + 40, ry + 20, 24, hexc(0xe0dcff), 0, 1);
    char k1[16], k2[16];
    snprintf(k1, sizeof(k1), "s_l%d", i); snprintf(k2, sizeof(k2), "s_r%d", i);
    bool l = button(k1, x + w - 290, ry, 50, 42, "<", 0x3a3466, -1, 26, true);
    bool rr = button(k2, x + w - 90, ry, 50, 42, ">", 0x3a3466, -1, 26, true);
    text_draw(r.value.c_str(), x + w - 165, ry + 21, 24, hexc(0xffd75e), 1);
    int d = l ? -1 : (rr ? 1 : 0);
    if (d) {
      changed = true;
      sfx_play("sfx_select", 0.7f);
      if (r.bv) *r.bv = !*r.bv;
      else if (r.iv) { *r.iv += d; if (*r.iv < r.mn) *r.iv = r.mx; if (*r.iv > r.mx) *r.iv = r.mn; }
    }
  }
  if (langI != S.lang) { S.lang = langI; voice_clear(); say("lang_set"); }
  if (changed) { applyVolumes(); saveSettings(); }
  if (button("s_back", 640 - 130, y + 520, 260, 50, T("TERUG", "BACK"), 0x6a4cff, -1, 26, true) || (anyDown() & BTN_B)) {
    sfx_play("sfx_back");
    mode = M_TITLE;
  }
}

static void howtoUI() {
  ctrlMask = 1;
  float x = 190, w = 900, y = 70;
  ui_rect(x, y, w, 520, hexc(0x0d0a20, 0.92f), 26, 3, hexc(0x3d8bff));
  text_draw(T("HOE WERKT HET?", "HOW TO PLAY"), 640, y + 40, 36, hexc(0xffffff), 1);
  const char* steps[5][2] = {
      {"26 koffers, elk met een bedrag van \xE2\x82\xAC 0,01 tot \xE2\x82\xAC 1.000.000.", "26 cases, each holding \xE2\x82\xAC" "0.01 up to \xE2\x82\xAC" "1,000,000."},
      {"Kies \xC3\xA9\xC3\xA9n koffer voor jezelf. Die blijft dicht tot het einde.", "Choose one case for yourself. It stays closed until the end."},
      {"Open de andere koffers. Lage bedragen eruit = goed nieuws!", "Open the other cases. Low amounts gone = good news!"},
      {"De bankier doet een bod: aannemen, of doorspelen?", "The banker makes an offer: take it, or play on?"},
      {"Wijs met je controller, trekker = kiezen, L/R = centreren. Joystick + A werkt ook.", "Point with your controller, trigger = choose, L/R = recenter. Stick + A works too."},
  };
  for (int i = 0; i < 5; i++) {
    float ry = y + 90 + i * 76;
    bool cur = howtoStep == i + 1;
    ui_rect(x + 30, ry, w - 60, 64, cur ? hexc(0x3d8bff, 0.35f) : hexc(0x1a1830, 0.8f), 18, cur ? 2.0f : 0.0f, hexc(0x6fb0ff));
    ui_circle(x + 70, ry + 32, 22, cur ? hexc(0xffd75e) : hexc(0x444466));
    char n[4]; snprintf(n, sizeof(n), "%d", i + 1);
    text_draw(n, x + 70, ry + 32, 26, hexc(0x14193a), 1, 0);
    text_draw(steps[i][S.lang], x + 110, ry + 32, 23, hexc(0xffffff), 0, 1);
  }
  if (button("h_back", 640 - 130, y + 460, 260, 50, T("BEGREPEN!", "GOT IT!"), 0x22b45a, -1, 26) || (anyDown() & BTN_B)) {
    voice_clear(); Q.clear(); sfx_play("sfx_back");
    mode = M_TITLE;
  }
}

static const char* padName(int kind) {
  switch (kind) {
    case PAD_HANDHELD: return T("Handheld", "Handheld");
    case PAD_PRO: return "Pro Controller";
    case PAD_DUAL_JOYCON: return T("Joy-Con paar", "Joy-Con pair");
    case PAD_JOYCON_L: return T("Linker Joy-Con", "Left Joy-Con");
    case PAD_JOYCON_R: return T("Rechter Joy-Con", "Right Joy-Con");
    case PAD_KEYBOARD: return T("Toetsenbord", "Keyboard");
  }
  return "?";
}

static void lobbyUI() {
  // join: any connected pad not yet joined that presses A / ZL / ZR / dpad-right
  int joined = 0;
  for (int i = 0; i < 4; i++) {
    if (lobbyJoined[i] && (P[i].pad < 0 || !plat_pad(P[i].pad).connected)) lobbyJoined[i] = false;
    if (lobbyJoined[i]) joined++;
  }
  bool joinedNow = false;
  for (int pi = 0; pi < MAX_PADS; pi++) {
    GPad& g = plat_pad(pi);
    if (!g.connected || !(g.down & (BTN_A | BTN_TRIGGER))) continue;
    bool already = false;
    for (int i = 0; i < 4; i++) if (lobbyJoined[i] && P[i].pad == pi) already = true;
    if (already) continue;
    for (int i = 0; i < 4; i++) if (!lobbyJoined[i]) {
      lobbyJoined[i] = true; P[i].pad = pi; joined++; joinedNow = true;
      sfx_play("sfx_lock");
      g.down &= ~(BTN_A | BTN_TRIGGER);  // consume
      break;
    }
  }
  if (joinedNow) for (int i = 0; i < 4; i++) activated[i] = -1;  // a join press never clicks a button
  // menu controlled by joined players (or everyone while nobody has joined)
  MP = joined > 0;
  ctrlMask = 0;
  for (int i = 0; i < 4; i++) if (lobbyJoined[i]) ctrlMask |= 1 << i;
  if (!ctrlMask) ctrlMask = 0;
  float x = 190, w = 900, y = 60;
  ui_rect(x, y, w, 600, hexc(0x0d0a20, 0.92f), 26, 3, hexc(0xff8a1f));
  text_draw(T("FEEST-MODUS", "PARTY MODE"), 640, y + 40, 36, hexc(0xffffff), 1);
  text_draw(T("Geef iedereen een controller of een losse Joy-Con (rechtop vasthouden, als een afstandsbediening).",
              "Give everyone a controller or a single Joy-Con (hold it upright, like a remote)."),
            640, y + 82, 19, hexc(0xd0ccef), 1, 0);
  text_draw(T("Druk op A of de trekker (ZL/ZR) om mee te doen.", "Press A or the trigger (ZL/ZR) to join."), 640, y + 108, 19, hexc(0xd0ccef), 1, 0);
  for (int i = 0; i < 4; i++) {
    float sx = x + 40 + i * 210, sy = y + 140;
    Col pc = hexc(PCOL[i]);
    ui_rect(sx, sy, 190, 200, lobbyJoined[i] ? withA(pc, 0.25f) : hexc(0x1a1830, 0.9f), 20, 3, lobbyJoined[i] ? pc : hexc(0x333355));
    char t[32]; snprintf(t, sizeof(t), "%s %d", T("SPELER", "PLAYER"), i + 1);
    text_draw(t, sx + 95, sy + 32, 24, pc, 1);
    if (lobbyJoined[i]) {
      ui_circle(sx + 95, sy + 100, 34, pc);
      text_draw(T("Klaar!", "Ready!"), sx + 95, sy + 100, 22, hexc(0xffffff), 1);
      text_draw(padName(plat_pad(P[i].pad).kind), sx + 95, sy + 165, 17, hexc(0xe0e0ff), 1, 0);
    } else {
      float a = 0.4f + 0.3f * sinf(g_time * 4 + i);
      text_draw(T("Druk op A / ZR", "Press A / ZR"), sx + 95, sy + 110, 19, hexc(0xffffff, a), 1, 0);
    }
  }
  if (joined == 0) {
    // allow pad 0 / handheld to go back or open the applet
    ctrlMask = 1; MP = false;
  }
  if (button("l_pair", x + 40, y + 370, 400, 60, T("JOY-CONS LOSKOPPELEN / KOPPELEN", "PAIR / SPLIT JOY-CONS"), 0x3d8bff, -1, 21, true)) {
    plat_showControllerApplet(4);
    for (int i = 0; i < 4; i++) { lobbyJoined[i] = false; P[i].pad = -1; }
  }
  bool canStart = joined >= 2;
  if (button("l_start", x + w - 440, y + 370, 400, 60, canStart ? T("START!", "START!") : T("MINIMAAL 2 SPELERS", "AT LEAST 2 PLAYERS"),
             0x22b45a, -1, 26, false, canStart)) {
    // compact joined players into slots 0..n-1
    int pads[4], n = 0;
    for (int i = 0; i < 4; i++) if (lobbyJoined[i]) pads[n++] = P[i].pad;
    for (int i = 0; i < 4; i++) P[i].pad = i < n ? pads[i] : -1;
    beginGame(true, n);
    return;
  }
  text_draw(T("Tip: met de knop hierboven kun je Joy-Cons los van elkaar koppelen (SL + SR indrukken).",
              "Tip: use the button above to split Joy-Cons into single controllers (press SL + SR)."),
            640, y + 470, 17, hexc(0xa8a4d0), 1, 0);
  if (button("l_back", 640 - 130, y + 520, 260, 50, T("TERUG", "BACK"), 0x6a4cff, -1, 24, true) || (joined == 0 && (anyDown() & BTN_B))) {
    sfx_play("sfx_back");
    mode = M_TITLE; MP = false;
    for (int i = 0; i < 4; i++) { lobbyJoined[i] = false; P[i].pad = -1; }
  }
}

// =====================================================================================
// main update
// =====================================================================================
void game_init() {
  R = Rng((unsigned long long)time(nullptr) * 2654435761ULL + 12345);
  g_autoplay = getenv("KK_AUTOPLAY") != nullptr;
  g_fast = getenv("KK_FAST") != nullptr;
  if (getenv("KK_SHOTS")) g_shotDir = getenv("KK_SHOTS");
  for (int i = 0; i < MAX_PADS; i++) g_grav[i] = V3(0, 0, -1);
  loadSettings();
  if (getenv("KK_LANG")) S.lang = strcmp(getenv("KK_LANG"), "nl") == 0 ? 0 : 1;
  applyVolumes();
  gfx_loadFont(plat_assetPath("font.ttf").c_str());
  loadVoiceIndex();
  buildScene();
  layoutCases();
  resetCases();
  cam.pos = V3(0, 6, 14); cam.tgt = V3(0, 1, -3);
  camOverview();
  music_play("music_menu", 1.5f);
  say("title");
  if (getenv("KK_PARTY")) {  // test hook: jump straight into party mode with N simulated players
    int n = atoi(getenv("KK_PARTY"));
    for (int i = 0; i < 4; i++) P[i].pad = i;
    beginGame(true, n < 2 ? 2 : (n > 4 ? 4 : n));
  }
}

static void updateWorld(float dt) {
  // camera
  if (mode != M_GAME) {
    float a = g_time * 0.12f;
    cam.gpos = V3(sinf(a) * 4.5f, 3.4f + sinf(g_time * 0.3f) * 0.4f, 8.8f + cosf(a) * 1.0f);
    cam.gtgt = V3(sinf(a) * 1.2f, 1.6f, -3.0f);
    cam.rate = 1.0f;
  }
  cam.pos = approach3(cam.pos, cam.gpos, cam.rate, dt);
  cam.tgt = approach3(cam.tgt, cam.gtgt, cam.rate * 1.2f, dt);
  // subtle breathing
  cam.pos.y += sinf(g_time * 0.7f) * 0.002f;
  g_tint = approach3(g_tint, g_tintTarget, 2.0f, dt);
  g_spotCol = approach3(g_spotCol, g_spotColTarget, 4.0f, dt);
  if (g_lampBlink > 0) g_lampBlink -= dt;
  if (g_shake > 0) g_shake -= dt;
  if (confetti > 0) confetti -= dt;
  // cases
  int hoverCase = -1;
  if (mode == M_GAME && (phase == P_PICK_OWN || phase == P_PICK_OPEN)) {
    int p = MP ? turn : 0;
    if (P[p].ptr.focus >= 1000 && P[p].ptr.focus < 1026) hoverCase = P[p].ptr.focus - 1000;
  }
  for (int i = 0; i < 26; i++) {
    CaseObj& c = C[i];
    c.hover = approach(c.hover, i == hoverCase ? 1.0f : 0.0f, 10, dt);
    c.lid = approach(c.lid, c.lidTarget, 5.0f, dt);
    if (c.flash > 0) c.flash = fmaxf(0, c.flash - dt * 0.7f);
    if (c.moving) {
      c.moveT += dt / 1.1f;
      float t = smooth(c.moveT);
      c.pos = lerp3(c.from, c.to, t) + V3(0, sinf(t * PI) * 1.2f, 0);
      c.yaw = lerpf(c.yawFrom, c.yawTo, t);
      if (c.moveT >= 1) { c.moving = false; c.pos = c.to; c.yaw = c.yawTo; }
    }
  }
  if (hoverCase >= 0) {
    g_spotPos = C[hoverCase].pos + V3(0, 1.2f, 1.0f);
    g_spotColTarget = V3(0.6f, 0.9f, 1.6f);
  } else if (lastCaseIdx >= 0 && mode == M_GAME && phase == P_BUSY) {
    g_spotPos = C[lastCaseIdx].pos + caseFront(C[lastCaseIdx]) * 0.9f + V3(0, 0.9f, 0);
    g_spotColTarget = V3(1.5f, 1.3f, 1.0f);
  } else g_spotColTarget = V3(0, 0, 0);
}

void game_frame(float dt) {
  g_dt = dt;
  g_time += dt;
  phaseTime += dt;
  audio_update(dt);
  if (g_fast) voice_clear();

  // skip presenter
  uint32_t d = anyDown();
  if (mode == M_GAME && (d & (BTN_B | BTN_PLUS)) && voice_busy()) voice_clear();

  for (int p = 0; p < 4; p++) updatePlayerInput(p);
  if (mode == M_GAME) {
    runQueue(dt);
    if (phase == P_PICK_OPEN || phase == P_PICK_OWN) {
      idleTime += dt;
      if (d) idleTime = 0;
      if (idleTime > 28 && !idleSaid && !voice_busy()) { idleSaid = true; say("idle"); }
    }
  } else if (mode == M_HOWTO) runQueue(dt);
  updateWorld(dt);
  drawWorld();

  switch (mode) {
    case M_TITLE: titleUI(); break;
    case M_SETTINGS: settingsUI(); break;
    case M_HOWTO: howtoUI(); break;
    case M_LOBBY: lobbyUI(); break;
    case M_GAME: gameUI(); break;
  }
  drawConfetti();
  if (mode == M_GAME) drawHints();
  drawSubtitles();
  drawCursors();
  ui_flush();
  endFrameItems();

  // automated screenshots for testing
  if (!g_shotDir.empty()) {
    static int lastKey = -1;
    static float since = 0;
    int key = (int)mode * 100 + (int)phase * 10 + (showOffer ? 1 : 0) + (showMpPanels ? 2 : 0);
    since += dt;
    if (key != lastKey) { lastKey = key; since = 0; plat_log("state %d round %d t=%.1f", key, roundIdx, g_time); }
    static int shotFor = -2;
    static float delay = getenv("KK_SHOTDELAY") ? (float)atof(getenv("KK_SHOTDELAY")) : 0.9f;
    if (since > delay && shotFor != key * 1000 + roundIdx) {
      shotFor = key * 1000 + roundIdx;
      char path[512];
      snprintf(path, sizeof(path), "%s/shot_%03d_m%d_p%d.png", g_shotDir.c_str(), g_shotCount++, (int)mode, (int)phase);
      plat_screenshot(path);
    }
    const char* maxShots = getenv("KK_MAXSHOTS");
    if (maxShots && g_shotCount >= atoi(maxShots)) g_quit = true;
  }
}

void game_shutdown() { saveSettings(); }
bool game_wantsQuit() { return g_quit; }
