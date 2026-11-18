#include "platform.h"
#include "audio.h"
#include "gfx.h"
#include "game.h"

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  plat_init();
  audio_init();
  gfx_init();
  game_init();
  double last = plat_time();
  while (plat_frame()) {
    double now = plat_time();
    float dt = (float)(now - last);
    last = now;
    if (dt > 0.1f) dt = 0.1f;
    if (dt <= 0) dt = 1.0f / 60.0f;
    game_frame(dt);
    plat_swap();
    if (game_wantsQuit()) break;
  }
  game_shutdown();
  audio_shutdown();
  plat_shutdown();
  return 0;
}
