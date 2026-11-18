#pragma once
void game_init();
void game_frame(float dt);  // update + render one frame
void game_shutdown();
bool game_wantsQuit();
