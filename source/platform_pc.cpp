// PC test build (SDL2). Keyboard + mouse simulate a controller with a gyro pointer.
#ifndef __SWITCH__
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "platform.h"
#include "glmini.h"
#include "../include/stb_image_write.h"

static SDL_Window* s_win;
static SDL_GLContext s_ctx;
static GPad s_pads[MAX_PADS];
static int s_kbPad = 0;  // which pad the keyboard drives (F1..F4)
static int s_w = 1280, s_h = 720;
static SDL_GameController* s_gc[4];

void plat_log(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr); fflush(stderr);
}

static void* sdl_getproc(const char* n) { return SDL_GL_GetProcAddress(n); }

void plat_init() {
  SDL_SetMainReady();
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
  const char* hidden = getenv("KK_HIDDEN");
  s_win = SDL_CreateWindow("Koffer Koorts (PC test)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, s_w, s_h,
                           SDL_WINDOW_OPENGL | (hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
  s_ctx = SDL_GL_CreateContext(s_win);
  if (!s_ctx) {
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    s_ctx = SDL_GL_CreateContext(s_win);
  }
  if (!s_ctx) { plat_log("GL context failed: %s", SDL_GetError()); exit(1); }
  SDL_GL_SetSwapInterval(hidden ? 0 : 1);
  glminiLoad(sdl_getproc);
  s_pads[0].connected = true;
  s_pads[0].kind = PAD_KEYBOARD;
}

bool plat_frame() {
  for (int i = 0; i < MAX_PADS; i++) { s_pads[i].down = 0; s_pads[i].mouseDX = s_pads[i].mouseDY = 0; }
  SDL_Event e;
  static uint32_t clickHeld = 0;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) return false;
    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
      SDL_Keycode k = e.key.keysym.sym;
      if (k >= SDLK_F1 && k <= SDLK_F4) {
        s_kbPad = k - SDLK_F1;
        s_pads[s_kbPad].connected = true;
        s_pads[s_kbPad].kind = PAD_KEYBOARD;
      }
    }
    if (e.type == SDL_MOUSEMOTION) {
      s_pads[s_kbPad].mouseDX += e.motion.xrel;
      s_pads[s_kbPad].mouseDY += e.motion.yrel;
      s_pads[s_kbPad].mouseAbs = true;
      s_pads[s_kbPad].mouseX = e.motion.x * 1280.0f / s_w;
      s_pads[s_kbPad].mouseY = e.motion.y * 720.0f / s_h;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) clickHeld |= (e.button.button == SDL_BUTTON_LEFT ? BTN_TRIGGER : BTN_RECENTER);
    if (e.type == SDL_MOUSEBUTTONUP) clickHeld &= ~(e.button.button == SDL_BUTTON_LEFT ? BTN_TRIGGER : BTN_RECENTER);
    if (e.type == SDL_CONTROLLERDEVICEADDED) {
      for (int i = 0; i < 4; i++) if (!s_gc[i]) { s_gc[i] = SDL_GameControllerOpen(e.cdevice.which);
        if (s_gc[i]) SDL_GameControllerSetSensorEnabled(s_gc[i], SDL_SENSOR_GYRO, SDL_TRUE);
        break; }
    }
  }
  const Uint8* ks = SDL_GetKeyboardState(nullptr);
  GPad& p = s_pads[s_kbPad];
  uint32_t prev = p.held;
  p.held = clickHeld;
  if (ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_SPACE]) p.held |= BTN_A;
  if (ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_BACKSPACE]) p.held |= BTN_B;
  if (ks[SDL_SCANCODE_X]) p.held |= BTN_X;
  if (ks[SDL_SCANCODE_Y]) p.held |= BTN_Y;
  if (ks[SDL_SCANCODE_Z]) p.held |= BTN_TRIGGER;
  if (ks[SDL_SCANCODE_R]) p.held |= BTN_RECENTER;
  if (ks[SDL_SCANCODE_P]) p.held |= BTN_PLUS;
  if (ks[SDL_SCANCODE_M]) p.held |= BTN_MINUS;
  if (ks[SDL_SCANCODE_UP]) p.held |= BTN_UP;
  if (ks[SDL_SCANCODE_DOWN]) p.held |= BTN_DOWN;
  if (ks[SDL_SCANCODE_LEFT]) p.held |= BTN_LEFT;
  if (ks[SDL_SCANCODE_RIGHT]) p.held |= BTN_RIGHT;
  p.stickX = (ks[SDL_SCANCODE_D] ? 1.f : 0.f) - (ks[SDL_SCANCODE_A] ? 1.f : 0.f);
  p.stickY = (ks[SDL_SCANCODE_W] ? 1.f : 0.f) - (ks[SDL_SCANCODE_S] ? 1.f : 0.f);
  p.down = p.held & ~prev;
  // game controllers -> pads 4..7
  for (int i = 0; i < 4; i++) {
    GPad& g = s_pads[4 + i];
    if (!s_gc[i]) { g.connected = false; continue; }
    g.connected = true; g.kind = PAD_PRO;
    uint32_t pv = g.held; g.held = 0;
    auto B = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(s_gc[i], b) != 0; };
    if (B(SDL_CONTROLLER_BUTTON_A)) g.held |= BTN_A;
    if (B(SDL_CONTROLLER_BUTTON_B)) g.held |= BTN_B;
    if (B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) g.held |= BTN_RECENTER;
    if (SDL_GameControllerGetAxis(s_gc[i], SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000 ||
        SDL_GameControllerGetAxis(s_gc[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) g.held |= BTN_TRIGGER;
    if (B(SDL_CONTROLLER_BUTTON_START)) g.held |= BTN_PLUS;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_UP)) g.held |= BTN_UP;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) g.held |= BTN_DOWN;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) g.held |= BTN_LEFT;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) g.held |= BTN_RIGHT;
    g.down = g.held & ~pv;
    g.stickX = SDL_GameControllerGetAxis(s_gc[i], SDL_CONTROLLER_AXIS_LEFTX) / 32767.f;
    g.stickY = -SDL_GameControllerGetAxis(s_gc[i], SDL_CONTROLLER_AXIS_LEFTY) / 32767.f;
    float gy[3];
    g.gyroValid = SDL_GameControllerGetSensorData(s_gc[i], SDL_SENSOR_GYRO, gy, 3) == 0;
    if (g.gyroValid) { g.gyro[0] = gy[0]; g.gyro[1] = -gy[2]; g.gyro[2] = gy[1]; g.accel[0] = 0; g.accel[1] = 0; g.accel[2] = -1; }
  }
  SDL_GL_GetDrawableSize(s_win, &s_w, &s_h);
  return true;
}

void plat_swap() { SDL_GL_SwapWindow(s_win); }
void plat_shutdown() { SDL_GL_DeleteContext(s_ctx); SDL_DestroyWindow(s_win); SDL_Quit(); }
int plat_width() { return s_w; }
int plat_height() { return s_h; }
GPad& plat_pad(int i) { return s_pads[i]; }
const char* plat_systemLang() { const char* l = getenv("KK_LANG"); return l ? l : "nl"; }
std::string plat_assetPath(const char* rel) {
  const char* base = getenv("KK_ROMFS");
  return std::string(base ? base : "romfs") + "/" + rel;
}
std::string plat_savePath(const char* name) { return std::string("kk_") + name; }
void plat_showControllerApplet(int) {}
double plat_time() { return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency(); }
void plat_setPadLabel(int, const char*) {}
void plat_screenshot(const char* path) {
  std::vector<unsigned char> px(s_w * s_h * 3), flip(s_w * s_h * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, s_w, s_h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  for (int y = 0; y < s_h; y++) memcpy(&flip[y * s_w * 3], &px[(s_h - 1 - y) * s_w * 3], s_w * 3);
  stbi_write_png(path, s_w, s_h, 3, flip.data(), s_w * 3);
}

int main(int argc, char** argv);
#endif
