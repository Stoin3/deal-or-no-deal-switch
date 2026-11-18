#pragma once
#include <stdint.h>
#include <string>

// abstract buttons
enum {
  BTN_A = 1 << 0,        // confirm (A, or the "A" equivalent)
  BTN_B = 1 << 1,        // back / skip
  BTN_X = 1 << 2,
  BTN_Y = 1 << 3,
  BTN_TRIGGER = 1 << 4,  // ZL / ZR  -> click with pointer
  BTN_RECENTER = 1 << 5, // L / R / SL / SR
  BTN_PLUS = 1 << 6,
  BTN_MINUS = 1 << 7,
  BTN_UP = 1 << 8,
  BTN_DOWN = 1 << 9,
  BTN_LEFT = 1 << 10,
  BTN_RIGHT = 1 << 11,
};

enum PadKind { PAD_NONE = 0, PAD_HANDHELD, PAD_PRO, PAD_DUAL_JOYCON, PAD_JOYCON_L, PAD_JOYCON_R, PAD_KEYBOARD };

struct GPad {
  bool connected = false;
  int kind = PAD_NONE;
  float stickX = 0, stickY = 0;  // -1..1, +y = up
  uint32_t held = 0, down = 0;
  bool gyroValid = false;
  float gyro[3] = {0, 0, 0};   // angular velocity, radians/second, controller frame
  float accel[3] = {0, 0, -1}; // gravity estimate, g units, controller frame
  // PC only: direct pointer delta in pixels (mouse)
  float mouseDX = 0, mouseDY = 0;
  bool mouseAbs = false; float mouseX = 0, mouseY = 0;
};

static const int MAX_PADS = 9;

void plat_init();
bool plat_frame();         // pump events & input, false = quit
void plat_swap();
void plat_shutdown();
int plat_width();
int plat_height();
GPad& plat_pad(int i);
const char* plat_systemLang();  // "nl" or "en"
std::string plat_assetPath(const char* rel);
std::string plat_savePath(const char* name);
void plat_showControllerApplet(int maxPlayers);  // switch: joy-con pairing applet
double plat_time();
void plat_setPadLabel(int i, const char* label);
void plat_screenshot(const char* path);
void plat_log(const char* fmt, ...);
