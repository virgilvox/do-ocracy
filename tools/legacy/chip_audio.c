/* A tiny chip synth: square, triangle and noise voices with pitch slides,
 * plus a looping bass and drum pattern for the night. */
#include "audio.h"
#include <math.h>
#include <string.h>

enum { W_SQ, W_TRI, W_NOISE, W_SAW };

typedef struct {
  int on, wave;
  float freq, slide, vol, decay, t, len, duty, pan;
  uint32_t lfsr;
} voice_t;

#define NV 16
static voice_t v[NV];
static int music_mode;
static double mclock;      /* seconds into the loop */
static int last_step = -1;

static void voice(int wave, float f, float slide, float vol, float len, float duty) {
  int k = 0;
  for (int i = 0; i < NV; i++) if (!v[i].on) { k = i; break; } else if (v[i].t > v[k].t) k = i;
  v[k] = (voice_t){ 1, wave, f, slide, vol, 0, 0, len, duty, 0.5f, 0xACE1u };
}

void audio_play(int s) {
  switch (s) {
    case SFX_PICKUP: voice(W_SQ, 520, 900, 0.18f, 0.07f, 0.25f); break;
    case SFX_DROP: voice(W_SQ, 300, -900, 0.16f, 0.08f, 0.5f); break;
    case SFX_TICK: voice(W_TRI, 880, 0, 0.08f, 0.03f, 0.5f); break;
    case SFX_GLAMOUR:
      voice(W_SQ, 523, 0, 0.16f, 0.12f, 0.25f); voice(W_SQ, 659, 0, 0.12f, 0.2f, 0.25f);
      voice(W_SQ, 784, 0, 0.12f, 0.3f, 0.125f); voice(W_TRI, 1046, 0, 0.18f, 0.45f, 0.5f); break;
    case SFX_KARMA: voice(W_TRI, 660, 0, 0.22f, 0.12f, 0.5f); voice(W_TRI, 990, 0, 0.18f, 0.25f, 0.5f); break;
    case SFX_LASER: voice(W_SAW, 1800, -2400, 0.10f, 0.18f, 0.5f); voice(W_NOISE, 4000, 0, 0.04f, 0.15f, 0.5f); break;
    case SFX_BAD: voice(W_SQ, 140, -40, 0.2f, 0.35f, 0.5f); voice(W_SQ, 147, -40, 0.2f, 0.35f, 0.5f); break;
    case SFX_JOIN: voice(W_SQ, 392, 0, 0.15f, 0.08f, 0.25f); voice(W_SQ, 784, 0, 0.15f, 0.16f, 0.25f); break;
    case SFX_ALARM: voice(W_SQ, 880, -300, 0.14f, 0.25f, 0.5f); voice(W_SQ, 660, 300, 0.10f, 0.25f, 0.5f); break;
    case SFX_WIN:
      for (int i = 0; i < 4; i++) voice(W_SQ, 392.f * powf(1.26f, (float)i), 0, 0.12f, 0.15f + 0.12f * i, 0.25f);
      voice(W_TRI, 196, 0, 0.3f, 0.9f, 0.5f); break;
    case SFX_LOSE: voice(W_SQ, 330, -200, 0.16f, 0.9f, 0.5f); voice(W_TRI, 110, -50, 0.3f, 1.0f, 0.5f); break;
    case SFX_TALK: voice(W_SQ, 300 + (float)(mclock * 997.0 - floor(mclock * 997.0)) * 300, 0, 0.07f, 0.05f, 0.5f); break;
    case SFX_SLURP: voice(W_NOISE, 2000, 0, 0.08f, 0.25f, 0.5f); voice(W_TRI, 220, 600, 0.12f, 0.25f, 0.5f); break;
  }
}

void audio_music(int mode) { if (mode != music_mode) { music_mode = mode; last_step = -1; } }

/* E minor punk: E E G A, E E D B */
static const float BASS[16] = { 82.4f, 82.4f, 0, 82.4f, 98.0f, 0, 110.0f, 0, 82.4f, 82.4f, 0, 82.4f, 73.4f, 0, 61.7f, 0 };
static const uint8_t KICK[16] = { 1,0,0,0, 0,0,1,0, 1,0,1,0, 0,0,0,0 };
static const uint8_t SNARE[16] = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,1 };
static const float LEAD[32] = {
  0,0,0,0, 0,0,0,0, 659,0,587,0, 494,0,0,0,
  0,0,0,0, 0,0,0,0, 659,0,740,0, 784,0,740,0 };

static void music_step(int step) {
  int s = step & 15;
  float vol = music_mode == 1 ? 0.5f : 1.f;
  if (BASS[s] > 0) voice(W_TRI, BASS[s] * 2, 0, 0.22f * vol, 0.16f, 0.5f);
  if (KICK[s]) voice(W_TRI, 140, -500, 0.35f * vol, 0.12f, 0.5f);
  if (SNARE[s]) voice(W_NOISE, 3000, 0, 0.12f * vol, 0.1f, 0.5f);
  if (s % 2 == 1 && music_mode >= 2) voice(W_NOISE, 8000, 0, 0.03f * vol, 0.03f, 0.5f);
  if (music_mode == 3 && LEAD[step & 31] > 0) voice(W_SQ, LEAD[step & 31], 0, 0.06f, 0.18f, 0.25f);
}

static float sample(voice_t *x, float rate) {
  float ph = x->t * x->freq;
  float f = ph - floorf(ph), s = 0;
  switch (x->wave) {
    case W_SQ: s = f < x->duty ? 1.f : -1.f; break;
    case W_TRI: s = 4.f * fabsf(f - 0.5f) - 1.f; break;
    case W_SAW: s = 2.f * f - 1.f; break;
    case W_NOISE: {
      static float hold; static float acc;
      acc += x->freq / rate;
      if (acc >= 1) { acc -= 1; x->lfsr = (x->lfsr >> 1) ^ (-(x->lfsr & 1u) & 0xB400u); hold = (x->lfsr & 1) ? 1.f : -1.f; }
      s = hold; break;
    }
  }
  float env = 1.f - x->t / x->len;
  if (x->t < 0.004f) env *= x->t / 0.004f;
  return s * x->vol * env * env;
}

void audio_render(float *ring, uint32_t cap, uint32_t *cursor, uint32_t rate, double dt_ms) {
  static double frac;
  double want = rate * dt_ms / 1000.0 + frac;
  int n = (int)want;
  frac = want - n;
  if (n > (int)cap / 2) n = (int)cap / 2;
  float bpm = music_mode == 3 ? 172.f : music_mode == 2 ? 150.f : 120.f;
  double step_len = 60.0 / bpm / 4.0;
  for (int i = 0; i < n; i++) {
    if (music_mode) {
      mclock += 1.0 / rate;
      int step = (int)(mclock / step_len);
      if (step != last_step) { last_step = step; music_step(step); }
    }
    float mix = 0;
    for (int k = 0; k < NV; k++) {
      voice_t *x = &v[k];
      if (!x->on) continue;
      mix += sample(x, (float)rate);
      x->t += 1.f / rate;
      x->freq += x->slide / rate;
      if (x->freq < 20) x->freq = 20;
      if (x->t >= x->len) x->on = 0;
    }
    mix = tanhf(mix * 0.9f);
    uint32_t c = *cursor;
    ring[c * 2] = mix; ring[c * 2 + 1] = mix;
    *cursor = (c + 1) % cap;
  }
}
