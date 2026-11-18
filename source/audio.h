#pragma once
#include <string>

void audio_init();
void audio_shutdown();
void audio_update(float dt);
void audio_setVolumes(float music, float voice, float sfx);

void music_play(const char* name, float fadeSeconds = 1.0f);  // name inside romfs/audio, "" = stop
void sfx_play(const char* name, float vol = 1.0f, float pitch = 1.0f);

void voice_queue(const std::string& file, const std::string& subtitle, float gapBefore = 0.12f);
void voice_clear();          // stop current + drop queue
bool voice_busy();           // playing or queued
float voice_level();         // 0..1 loudness of current voice (for visuals)
const std::string& voice_subtitle();
float voice_subtitleAlpha();
