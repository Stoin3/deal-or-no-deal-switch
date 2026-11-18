#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <memory>
#include <vector>
#include <deque>
#include "platform.h"

#define STB_VORBIS_HEADER_ONLY
#include "../include/stb_vorbis.c"

#ifdef __SWITCH__
#include <switch.h>
#include <malloc.h>
static Mutex s_mtx;
static void lockA() { mutexLock(&s_mtx); }
static void unlockA() { mutexUnlock(&s_mtx); }
#else
#include <SDL.h>
static SDL_mutex* s_mtx;
static void lockA() { SDL_LockMutex(s_mtx); }
static void unlockA() { SDL_UnlockMutex(s_mtx); }
#endif

static const int OUT_RATE = 48000;
static float approachLocal(float c, float t, float dt) { return t + (c - t) * expf(-8.0f * dt); }

struct Sample {
  std::vector<short> data;
  int channels = 1, rate = 48000;
  int frames() const { return (int)(data.size() / channels); }
};
typedef std::shared_ptr<Sample> SampleP;

enum Kind { K_MUSIC, K_SFX, K_VOICE };
struct Play {
  SampleP s;
  double pos = 0, step = 1;
  float vol = 1, gain = 1, gainTarget = 1, fadeRate = 0;
  bool loop = false, done = false, stopWhenSilent = false;
  int kind = K_SFX;
  int id = 0;
};

static std::vector<Play> s_plays;
static std::map<std::string, SampleP> s_cache;
static float s_volMusic = 0.7f, s_volVoice = 1.0f, s_volSfx = 0.8f;
static float s_duck = 1.0f;
static volatile float s_level = 0;
static int s_nextId = 1;
static int s_voiceId = 0;
static std::string s_curMusic;

struct QueuedVoice { std::string file, sub; float gap; };
static std::deque<QueuedVoice> s_vq;
static float s_gapTimer = 0;
static std::string s_subtitle;
static float s_subAlpha = 0, s_subHold = 0;

static SampleP loadOgg(const std::string& path) {
  int ch = 0, rate = 0;
  short* out = nullptr;
  int n = stb_vorbis_decode_filename(path.c_str(), &ch, &rate, &out);
  if (n <= 0 || !out) { plat_log("audio: failed to load %s", path.c_str()); return nullptr; }
  SampleP s = std::make_shared<Sample>();
  s->channels = ch; s->rate = rate;
  s->data.assign(out, out + (size_t)n * ch);
  free(out);
  return s;
}

static SampleP getCached(const char* name) {
  auto it = s_cache.find(name);
  if (it != s_cache.end()) return it->second;
  SampleP s = loadOgg(plat_assetPath((std::string("audio/") + name + ".ogg").c_str()));
  s_cache[name] = s;
  return s;
}

// ------------------------------------------------------------- mixer (audio thread)
static void mix(short* out, int frames) {
  static std::vector<float> acc;
  acc.assign((size_t)frames * 2, 0.0f);
  float lvl = 0;
  lockA();
  bool voiceActive = false;
  for (auto& p : s_plays) if (p.kind == K_VOICE && !p.done) voiceActive = true;
  float duckTarget = voiceActive ? 0.4f : 1.0f;
  for (auto& p : s_plays) {
    if (p.done || !p.s) continue;
    const Sample& s = *p.s;
    int nf = s.frames();
    if (nf < 2) { p.done = true; continue; }
    float kvol = p.kind == K_MUSIC ? s_volMusic : (p.kind == K_VOICE ? s_volVoice * 1.25f : s_volSfx);
    for (int i = 0; i < frames; i++) {
      if (p.fadeRate != 0) {
        p.gain += (p.gainTarget - p.gain) * p.fadeRate;
        if (p.stopWhenSilent && p.gain < 0.002f) { p.done = true; break; }
      }
      if (p.kind == K_MUSIC) { s_duck += (duckTarget - s_duck) * 0.00008f; }
      int i0 = (int)p.pos;
      if (i0 >= nf - 1) {
        if (p.loop) { p.pos -= (nf - 1); i0 = (int)p.pos; }
        else { p.done = true; break; }
      }
      float f = (float)(p.pos - i0);
      float l, r;
      if (s.channels == 2) {
        l = s.data[i0 * 2] * (1 - f) + s.data[i0 * 2 + 2] * f;
        r = s.data[i0 * 2 + 1] * (1 - f) + s.data[i0 * 2 + 3] * f;
      } else {
        l = r = s.data[i0] * (1 - f) + s.data[i0 + 1] * f;
      }
      float g = p.vol * p.gain * kvol * (p.kind == K_MUSIC ? s_duck : 1.0f) / 32768.0f;
      acc[i * 2] += l * g;
      acc[i * 2 + 1] += r * g;
      if (p.kind == K_VOICE) lvl += fabsf(l * g);
      p.pos += p.step;
    }
  }
  unlockA();
  s_level = s_level * 0.7f + 0.3f * fminf(1.0f, lvl / frames * 6.0f);
  for (int i = 0; i < frames * 2; i++) {
    float v = acc[i];
    v = v / (1.0f + fabsf(v) * 0.25f);  // soft clip
    int iv = (int)(v * 32767.0f);
    out[i] = (short)(iv > 32767 ? 32767 : (iv < -32768 ? -32768 : iv));
  }
}

#ifdef __SWITCH__
static Thread s_thread;
static volatile bool s_run = false;
static const int BUF_FRAMES = 1024;
static AudioOutBuffer s_bufs[3];
static void* s_bufData[3];

static void audioThread(void*) {
  for (int i = 0; i < 3; i++) {
    mix((short*)s_bufData[i], BUF_FRAMES);
    audoutAppendAudioOutBuffer(&s_bufs[i]);
  }
  while (s_run) {
    AudioOutBuffer* released = nullptr;
    u32 count = 0;
    if (R_FAILED(audoutWaitPlayFinish(&released, &count, 100000000ULL)) || !released) continue;
    mix((short*)released->buffer, BUF_FRAMES);
    audoutAppendAudioOutBuffer(released);
  }
}

void audio_init() {
  mutexInit(&s_mtx);
  if (R_FAILED(audoutInitialize())) { plat_log("audout init failed"); return; }
  audoutStartAudioOut();
  size_t size = (BUF_FRAMES * 2 * sizeof(short) + 0xfff) & ~0xfff;
  for (int i = 0; i < 3; i++) {
    s_bufData[i] = memalign(0x1000, size);
    memset(s_bufData[i], 0, size);
    s_bufs[i].next = nullptr;
    s_bufs[i].buffer = s_bufData[i];
    s_bufs[i].buffer_size = size;
    s_bufs[i].data_size = BUF_FRAMES * 2 * sizeof(short);
    s_bufs[i].data_offset = 0;
  }
  s_run = true;
  threadCreate(&s_thread, audioThread, nullptr, nullptr, 0x10000, 0x2B, -2);
  threadStart(&s_thread);
}

void audio_shutdown() {
  s_run = false;
  threadWaitForExit(&s_thread);
  threadClose(&s_thread);
  audoutStopAudioOut();
  audoutExit();
}
#else
static SDL_AudioDeviceID s_dev;
static void sdlCallback(void*, Uint8* stream, int len) { mix((short*)stream, len / 4); }
void audio_init() {
  s_mtx = SDL_CreateMutex();
  SDL_AudioSpec want = {}, have;
  want.freq = OUT_RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = sdlCallback;
  s_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (s_dev) SDL_PauseAudioDevice(s_dev, 0);
}
void audio_shutdown() { if (s_dev) SDL_CloseAudioDevice(s_dev); }
#endif

// ------------------------------------------------------------- main thread API
void audio_setVolumes(float m, float v, float s) { lockA(); s_volMusic = m; s_volVoice = v; s_volSfx = s; unlockA(); }

void music_play(const char* name, float fade) {
  if (s_curMusic == name) return;
  s_curMusic = name;
  SampleP s = name[0] ? getCached(name) : nullptr;
  lockA();
  float rate = 1.0f / (fmaxf(fade, 0.05f) * OUT_RATE) * 4.0f;
  for (auto& p : s_plays)
    if (p.kind == K_MUSIC && !p.done) { p.gainTarget = 0; p.fadeRate = rate; p.stopWhenSilent = true; }
  if (s) {
    Play p; p.s = s; p.kind = K_MUSIC; p.loop = true; p.gain = 0; p.gainTarget = 1; p.fadeRate = rate;
    p.step = (double)s->rate / OUT_RATE; p.id = s_nextId++;
    s_plays.push_back(p);
  }
  unlockA();
}

void sfx_play(const char* name, float vol, float pitch) {
  SampleP s = getCached(name);
  if (!s) return;
  lockA();
  Play p; p.s = s; p.kind = K_SFX; p.vol = vol; p.step = (double)s->rate / OUT_RATE * pitch; p.id = s_nextId++;
  s_plays.push_back(p);
  unlockA();
}

void voice_queue(const std::string& file, const std::string& sub, float gap) {
  QueuedVoice q; q.file = file; q.sub = sub; q.gap = gap;
  s_vq.push_back(q);
}

static bool voicePlaying() {
  bool r = false;
  lockA();
  for (auto& p : s_plays) if (p.id == s_voiceId && !p.done) r = true;
  unlockA();
  return r;
}

void voice_clear() {
  s_vq.clear();
  lockA();
  for (auto& p : s_plays) if (p.kind == K_VOICE && !p.done) { p.gainTarget = 0; p.fadeRate = 0.004f; p.stopWhenSilent = true; }
  s_voiceId = 0;
  unlockA();
  s_subHold = 0;
}

bool voice_busy() { return !s_vq.empty() || voicePlaying(); }
float voice_level() { return s_level; }
const std::string& voice_subtitle() { return s_subtitle; }
float voice_subtitleAlpha() { return s_subAlpha; }

void audio_update(float dt) {
  // cleanup finished
  lockA();
  for (size_t i = 0; i < s_plays.size();) {
    if (s_plays[i].done) { s_plays[i] = s_plays.back(); s_plays.pop_back(); } else i++;
  }
  unlockA();
  bool playing = voicePlaying();
  if (!playing && !s_vq.empty()) {
    s_gapTimer += dt;
    if (s_gapTimer >= s_vq.front().gap) {
      QueuedVoice q = s_vq.front();
      s_vq.pop_front();
      s_gapTimer = 0;
      SampleP s = loadOgg(q.file);
      s_subtitle = q.sub;
      s_subHold = 1.0f;
      if (s) {
        lockA();
        Play p; p.s = s; p.kind = K_VOICE; p.step = (double)s->rate / OUT_RATE; p.id = s_nextId++;
        s_voiceId = p.id;
        s_plays.push_back(p);
        unlockA();
        playing = true;
      }
    }
  } else if (!playing) {
    s_gapTimer = 0.2f;  // allow the first clip in a new queue to start promptly
  }
  if (playing) s_subHold = 1.0f;
  else s_subHold -= dt * 0.8f;
  s_subAlpha = approachLocal(s_subAlpha, s_subHold > 0 ? 1.0f : 0.0f, dt);
}
