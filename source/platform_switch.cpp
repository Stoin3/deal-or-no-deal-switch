#ifdef __SWITCH__
#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
#include "platform.h"
#include "glmini.h"

static EGLDisplay s_display = EGL_NO_DISPLAY;
static EGLContext s_context = EGL_NO_CONTEXT;
static EGLSurface s_surface = EGL_NO_SURFACE;
static GPad s_pads[MAX_PADS];
static PadState s_nx[MAX_PADS];
static const HidNpadIdType s_ids[MAX_PADS] = {HidNpadIdType_No1, HidNpadIdType_No2, HidNpadIdType_No3, HidNpadIdType_No4,
                                               HidNpadIdType_No5, HidNpadIdType_No6, HidNpadIdType_No7, HidNpadIdType_No8,
                                               HidNpadIdType_Handheld};
struct SixInfo { u32 style; HidSixAxisSensorHandle h[2]; int n; int use; };
static SixInfo s_six[MAX_PADS];
static FILE* s_log = nullptr;

void plat_log(const char* fmt, ...) {
  if (!s_log) return;
  va_list ap; va_start(ap, fmt); vfprintf(s_log, fmt, ap); va_end(ap);
  fputc('\n', s_log); fflush(s_log);
}

static void* egl_getproc(const char* n) { return (void*)eglGetProcAddress(n); }

static bool initEgl(NWindow* win) {
  s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!s_display) return false;
  eglInitialize(s_display, nullptr, nullptr);
  if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) return false;
  EGLConfig config; EGLint numConfigs = 0;
  static const EGLint attrMsaa[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                    EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
                                    EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 4, EGL_NONE};
  static const EGLint attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
  eglChooseConfig(s_display, attrMsaa, &config, 1, &numConfigs);
  if (numConfigs == 0) {
    eglChooseConfig(s_display, attr, &config, 1, &numConfigs);
    if (numConfigs == 0) return false;
  }
  s_surface = eglCreateWindowSurface(s_display, config, win, nullptr);
  if (!s_surface) return false;
  static const EGLint ctxAttr[] = {EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                   EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_NONE};
  s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctxAttr);
  if (!s_context) return false;
  eglMakeCurrent(s_display, s_surface, s_surface, s_context);
  eglSwapInterval(s_display, 1);
  return true;
}

void plat_init() {
  romfsInit();
  setInitialize();
  plInitialize(PlServiceType_User);
  mkdir("sdmc:/switch", 0777);
  mkdir("sdmc:/switch/KofferKoorts", 0777);
  s_log = fopen("sdmc:/switch/KofferKoorts/log.txt", "w");
  plat_log("Koffer Koorts start");

  padConfigureInput(8, HidNpadStyleSet_NpadStandard);
  hidSetNpadJoyHoldType(HidNpadJoyHoldType_Vertical);
  for (int i = 0; i < 8; i++) padInitialize(&s_nx[i], s_ids[i]);
  padInitialize(&s_nx[8], HidNpadIdType_Handheld);
  memset(s_six, 0, sizeof(s_six));

  NWindow* win = nwindowGetDefault();
  nwindowSetDimensions(win, 1280, 720);
  if (!initEgl(win)) plat_log("EGL init failed");
  glminiLoad(egl_getproc);
}

static void updateSixAxis(int i, u32 style) {
  SixInfo& s = s_six[i];
  if (s.style != style) {
    for (int k = 0; k < s.n; k++) hidStopSixAxisSensor(s.h[k]);
    s.n = 0; s.use = 0; s.style = style;
    HidNpadStyleTag tag = (HidNpadStyleTag)0;
    int count = 1;
    if (style & HidNpadStyleTag_NpadFullKey) tag = HidNpadStyleTag_NpadFullKey;
    else if (style & HidNpadStyleTag_NpadHandheld) tag = HidNpadStyleTag_NpadHandheld;
    else if (style & HidNpadStyleTag_NpadJoyDual) { tag = HidNpadStyleTag_NpadJoyDual; count = 2; }
    else if (style & HidNpadStyleTag_NpadJoyLeft) tag = HidNpadStyleTag_NpadJoyLeft;
    else if (style & HidNpadStyleTag_NpadJoyRight) tag = HidNpadStyleTag_NpadJoyRight;
    if (tag) {
      if (R_SUCCEEDED(hidGetSixAxisSensorHandles(s.h, count, s_ids[i], tag))) {
        s.n = count;
        s.use = count == 2 ? 1 : 0;  // dual joy-cons: aim with the right one
        for (int k = 0; k < s.n; k++) hidStartSixAxisSensor(s.h[k]);
      }
    }
  }
  GPad& p = s_pads[i];
  p.gyroValid = false;
  if (s.n > 0) {
    HidSixAxisSensorState st;
    if (hidGetSixAxisSensorStates(s.h[s.use], &st, 1) > 0) {
      const float k = 2.0f * 3.14159265f;  // hid reports revolutions per second
      p.gyro[0] = st.angular_velocity.x * k;
      p.gyro[1] = st.angular_velocity.y * k;
      p.gyro[2] = st.angular_velocity.z * k;
      p.accel[0] = st.acceleration.x;
      p.accel[1] = st.acceleration.y;
      p.accel[2] = st.acceleration.z;
      p.gyroValid = true;
    }
  }
}

bool plat_frame() {
  if (!appletMainLoop()) return false;
  for (int i = 0; i < MAX_PADS; i++) {
    PadState& nx = s_nx[i];
    padUpdate(&nx);
    GPad& p = s_pads[i];
    p.connected = padIsConnected(&nx);
    uint32_t prev = p.held;
    p.held = 0;
    if (!p.connected) { p.down = 0; p.stickX = p.stickY = 0; p.kind = PAD_NONE; updateSixAxis(i, 0); continue; }
    u32 style = padGetStyleSet(&nx);
    if (style & HidNpadStyleTag_NpadHandheld) p.kind = PAD_HANDHELD;
    else if (style & HidNpadStyleTag_NpadFullKey) p.kind = PAD_PRO;
    else if (style & HidNpadStyleTag_NpadJoyDual) p.kind = PAD_DUAL_JOYCON;
    else if (style & HidNpadStyleTag_NpadJoyLeft) p.kind = PAD_JOYCON_L;
    else if (style & HidNpadStyleTag_NpadJoyRight) p.kind = PAD_JOYCON_R;
    u64 b = padGetButtons(&nx);
    if (p.kind == PAD_JOYCON_L) {
      // single left joy-con held upright: directional buttons act as face buttons
      if (b & HidNpadButton_Right) p.held |= BTN_A;
      if (b & HidNpadButton_Down) p.held |= BTN_B;
      if (b & HidNpadButton_Up) p.held |= BTN_X;
      if (b & HidNpadButton_Left) p.held |= BTN_Y;
    } else {
      if (b & HidNpadButton_A) p.held |= BTN_A;
      if (b & HidNpadButton_B) p.held |= BTN_B;
      if (b & HidNpadButton_X) p.held |= BTN_X;
      if (b & HidNpadButton_Y) p.held |= BTN_Y;
      if (b & HidNpadButton_Up) p.held |= BTN_UP;
      if (b & HidNpadButton_Down) p.held |= BTN_DOWN;
      if (b & HidNpadButton_Left) p.held |= BTN_LEFT;
      if (b & HidNpadButton_Right) p.held |= BTN_RIGHT;
    }
    if (b & (HidNpadButton_ZL | HidNpadButton_ZR)) p.held |= BTN_TRIGGER;
    if (b & (HidNpadButton_L | HidNpadButton_R | HidNpadButton_LeftSL | HidNpadButton_LeftSR |
             HidNpadButton_RightSL | HidNpadButton_RightSR)) p.held |= BTN_RECENTER;
    if (b & HidNpadButton_Plus) p.held |= BTN_PLUS;
    if (b & HidNpadButton_Minus) p.held |= BTN_MINUS;
    p.down = p.held & ~prev;
    HidAnalogStickState l = padGetStickPos(&nx, 0), r = padGetStickPos(&nx, 1);
    float lx = l.x / 32767.0f, ly = l.y / 32767.0f, rx = r.x / 32767.0f, ry = r.y / 32767.0f;
    if (rx * rx + ry * ry > lx * lx + ly * ly) { lx = rx; ly = ry; }
    p.stickX = lx; p.stickY = ly;
    updateSixAxis(i, style);
  }
  return true;
}

void plat_swap() { eglSwapBuffers(s_display, s_surface); }

void plat_shutdown() {
  for (int i = 0; i < MAX_PADS; i++)
    for (int k = 0; k < s_six[i].n; k++) hidStopSixAxisSensor(s_six[i].h[k]);
  if (s_display) {
    eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s_context) eglDestroyContext(s_display, s_context);
    if (s_surface) eglDestroySurface(s_display, s_surface);
    eglTerminate(s_display);
  }
  if (s_log) fclose(s_log);
  plExit();
  setExit();
  romfsExit();
}

int plat_width() { return 1280; }
int plat_height() { return 720; }
GPad& plat_pad(int i) { return s_pads[i]; }

const char* plat_systemLang() {
  u64 code = 0; SetLanguage lang = SetLanguage_ENUS;
  if (R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang))) {
    if (lang == SetLanguage_NL) return "nl";
  }
  return "en";
}

std::string plat_assetPath(const char* rel) { return std::string("romfs:/") + rel; }
std::string plat_savePath(const char* name) { return std::string("sdmc:/switch/KofferKoorts/") + name; }

void plat_showControllerApplet(int maxPlayers) {
  HidLaControllerSupportArg arg;
  hidLaCreateControllerSupportArg(&arg);
  arg.hdr.player_count_min = 1;
  arg.hdr.player_count_max = (s8)maxPlayers;
  arg.hdr.enable_take_over_connection = 1;
  arg.hdr.enable_left_justify = 1;
  arg.hdr.enable_permit_joy_dual = 1;
  arg.hdr.enable_single_mode = 0;
  HidLaControllerSupportResultInfo info;
  Result rc = hidLaShowControllerSupportForSystem(&info, &arg, false);
  if (R_FAILED(rc)) rc = hidLaShowControllerSupport(&info, &arg);
  plat_log("controller applet rc=%x players=%d", rc, (int)info.player_count);
}

double plat_time() { return (double)armGetSystemTick() / (double)armGetSystemTickFreq(); }
void plat_setPadLabel(int, const char*) {}
void plat_screenshot(const char*) {}
#endif
