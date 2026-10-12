/* DO-OCRACY's sound, on Bellows (bellows-embedded, the C++ port of
 * bellows.live's DSP core, vendored in third_party/bellows).
 *
 * The score: E minor punk, i VI III VII (Em C G D), the progression every
 * garage band finds on its own. Three intensities, picked by the game:
 *   1  lobby / coffee and coworking: 112 bpm, half-note bass, pluck arps
 *   2  the night: 150 bpm, the whole kit, eighth-note bass, offbeat stabs
 *   3  closing time: 172 bpm, all of that plus an FM lead
 * Sound effects are notes on their own voices: FM bells for clout, plucked
 * strings for karma, a VA buzzer for trouble. No samples, no files: about
 * 50 KB of wasm that sounds the same in every wasmcart host.
 *
 * Same API as before (audio.h), so the C game never knows. */
#include "bellows/config.h"
#include "bellows/core/prng.h"
#include "bellows/engines/drums.h"
#include "bellows/engines/fm.h"
#include "bellows/engines/pluck.h"
#include "bellows/engines/va.h"
#include "bellows/fx/delay.h"
#include "bellows/fx/dynamics.h"
#include "bellows/voicepool.h"
#include <math.h>
#include <stdint.h>

extern "C" {
#include "audio.h"
}

namespace {

constexpr int kBlock = BELLOWS_BLOCK_SIZE;

float Mtof(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }

using String = bellows::Pluck<70>;
using Bell = bellows::Fm;

/* Em C G D, bass roots as MIDI notes in octave 2 */
constexpr int kRoots[4] = {40, 36, 43, 38};
/* closing time lead, one note per eighth (0 = rest), over the four bars */
constexpr int kLead[32] = {
  71, 0, 71, 74, 76, 0, 74, 71,   72, 0, 72, 71, 67, 0, 64, 0,
  67, 0, 67, 71, 74, 0, 71, 67,   69, 0, 66, 69, 74, 0, 72, 71,
};

/* a tiny event queue so an effect can be a phrase, not one note */
enum Inst { I_BELL, I_PLUCK, I_BUZZ, I_KICK, I_SNARE, I_HAT, I_BUZZ_OFF };
struct Ev { int at; int inst; float freq, vel; };

class Mixer {
 public:
  void Init(float sr) {
    sr_ = sr;
    rng_seq_.Init("do-ocracy::seq");
    rng_snare_.Init("do-ocracy::kit::snare/noise");
    rng_bass_.Init("do-ocracy::bass");
    rng_str_.Init("do-ocracy::strings");
    rng_sfx_.Init("do-ocracy::sfx");
    rng_buzz_.Init("do-ocracy::buzz");

    bellows::Kick::Params kp; kp.decay = 0.32f; kp.drive = 2.6f;
    kick_.Init(sr, kp);
    bellows::Snare::Params sp; sp.tone = 0.35f; sp.decay = 0.16f; sp.snap = 0.3f;
    snare_.Init(sr, &rng_snare_, sp);
    bellows::Hat::Params hp; hp.decay = 0.05f;
    hat_.Init(sr, hp);

    bellows::Va::Params bass;
    bass.shape = 0.1f; bass.detune = 6.0f; bass.sub = 0.55f;
    bass.cutoff = 700.0f; bass.resonance = 0.3f; bass.env_amount = 1.1f;
    bass.attack = 0.003f; bass.decay = 0.12f; bass.sustain = 0.5f; bass.release = 0.06f;
    bass.f_decay = 0.1f; bass.f_sustain = 0.2f;
    bass_.Init(sr, &rng_bass_, bass);

    String::Params st;
    st.damp = 0.25f; st.pick_pos = 0.18f; st.decay = 0.9f; st.level = 0.85f;
    for (int i = 0; i < 6; ++i) stabs_.at(i).Init(sr, &rng_str_, st);
    String::Params pl;
    pl.damp = 0.15f; pl.pick_pos = 0.3f; pl.decay = 1.6f; pl.level = 0.9f;
    for (int i = 0; i < 4; ++i) sfx_pluck_.at(i).Init(sr, &rng_sfx_, pl);

    Bell::Params lead;
    lead.ops = 2; lead.algorithm = 1; lead.feedback = 0.25f; lead.brightness = 0.45f;
    lead.attack = 0.004f; lead.decay = 0.15f; lead.sustain = 0.55f; lead.release = 0.08f;
    lead_.Init(sr, lead);
    Bell::Params bell;
    bell.ops = 4; bell.algorithm = 3; bell.brightness = 0.7f;
    bell.attack = 0.001f; bell.decay = 0.5f; bell.sustain = 0.0f; bell.release = 0.4f;
    bell.m_decay = 0.3f; bell.m_sustain = 0.1f;
    for (int i = 0; i < 6; ++i) bells_.at(i).Init(sr, bell);

    bellows::Va::Params bz;
    bz.shape = 0.85f; bz.detune = 18.0f; bz.cutoff = 2400.0f; bz.resonance = 0.1f;
    bz.attack = 0.002f; bz.decay = 0.1f; bz.sustain = 0.7f; bz.release = 0.05f;
    buzz_.Init(sr, &rng_buzz_, bz);

    bellows::StereoDelay<500>::Params d;
    d.time_l = 0.3f; d.time_r = 0.2f; d.feedback = 0.35f; d.cross_feedback = 0.15f;
    d.damping = 3800.0f; d.mix = 1.0f;
    delay_.Init(sr, d);

    bellows::Limiter<>::Params lim;
    lim.ceiling_db = -1.0f; lim.release = 0.08f;
    limiter_.Init(sr, lim);
    SetMode(1);
    ready_ = true;
  }

  bool Ready() const { return ready_; }

  void SetMode(int m) {
    if (m == mode_) return;
    mode_ = m;
    unsigned bpm = m == 3 ? 172 : m == 2 ? 150 : 112;
    samples_per_step_ = static_cast<int>(sr_ * 60.0f / (static_cast<float>(bpm) * 4.0f) + 0.5f);
    if (m == 0) bass_.NoteOff();
  }

  void Push(int delay_ms, int inst, float freq, float vel) {
    if (nev_ >= 64) return;
    ev_[nev_++] = Ev{static_cast<int>(delay_ms * sr_ / 1000.0f), inst, freq, vel};
  }

  void Sfx(int s) {
    float r = rng_sfx_.Next();
    switch (s) {
      case SFX_PICKUP: Push(0, I_PLUCK, Mtof(76), 0.6f); Push(40, I_PLUCK, Mtof(83), 0.5f); break;
      case SFX_DROP: Push(0, I_PLUCK, Mtof(52), 0.7f); Push(0, I_HAT, 400, 0.3f); break;
      case SFX_TICK: Push(0, I_HAT, 330, 0.25f); break;
      case SFX_GLAMOUR:   /* clout: a rising bell arpeggio, a little too pleased with itself */
        Push(0, I_BELL, Mtof(76), 0.7f); Push(70, I_BELL, Mtof(80), 0.7f);
        Push(140, I_BELL, Mtof(83), 0.7f); Push(210, I_BELL, Mtof(88), 0.9f); break;
      case SFX_KARMA:     /* karma: two quiet strings, nobody notices */
        Push(0, I_PLUCK, Mtof(79), 0.6f); Push(90, I_PLUCK, Mtof(86), 0.55f); break;
      case SFX_LASER:
        Push(0, I_BELL, Mtof(96 + r * 6), 0.35f); Push(0, I_HAT, 330, 0.4f); break;
      case SFX_BAD:
        Push(0, I_BUZZ, Mtof(40), 0.8f); Push(260, I_BUZZ_OFF, 0, 0); break;
      case SFX_JOIN:
        Push(0, I_PLUCK, Mtof(64), 0.7f); Push(60, I_PLUCK, Mtof(71), 0.7f); Push(120, I_PLUCK, Mtof(76), 0.8f); break;
      case SFX_ALARM:
        for (int i = 0; i < 4; ++i) { Push(i * 140, I_BUZZ, Mtof(i & 1 ? 76 : 81), 0.55f); }
        Push(560, I_BUZZ_OFF, 0, 0); break;
      case SFX_WIN:
        Push(0, I_BELL, Mtof(64), 0.8f); Push(110, I_BELL, Mtof(67), 0.8f); Push(220, I_BELL, Mtof(71), 0.8f);
        Push(330, I_BELL, Mtof(76), 0.9f); Push(330, I_KICK, 50, 0.9f); Push(330, I_PLUCK, Mtof(52), 0.9f); break;
      case SFX_LOSE:
        Push(0, I_BUZZ, Mtof(52), 0.7f); Push(300, I_BUZZ, Mtof(51), 0.7f); Push(600, I_BUZZ, Mtof(50), 0.7f);
        Push(900, I_BUZZ, Mtof(49), 0.8f); Push(1500, I_BUZZ_OFF, 0, 0); break;
      case SFX_TALK:      /* a proposal: confident mumbling */
        for (int i = 0; i < 4; ++i) Push(i * 55, I_BELL, Mtof(62 + rng_sfx_.Next() * 12), 0.3f);
        break;
      case SFX_SLURP: Push(0, I_SNARE, 300, 0.25f); Push(80, I_PLUCK, Mtof(60), 0.5f); Push(160, I_PLUCK, Mtof(67), 0.5f); break;
    }
  }

  void Render(float* l, float* r, int n) {
    int i = 0;
    while (i < n) {
      if (mode_ && countdown_ <= 0) { Step(); countdown_ = samples_per_step_; }
      int span = n - i;
      if (mode_ && span > countdown_) span = countdown_;
      if (span > kBlock) span = kBlock;
      Fire(span);
      RenderSpan(l, r, i, span);
      i += span;
      countdown_ -= span;
    }
  }

 private:
  void Fire(int span) {
    int k = 0;
    for (int e = 0; e < nev_; ++e) {
      Ev& v = ev_[e];
      if (v.at <= 0) {
        switch (v.inst) {
          case I_BELL: bells_.NoteOn(note_id_++ & 0xffff, v.freq, v.vel, frame_); break;
          case I_PLUCK: sfx_pluck_.NoteOn(note_id_++ & 0xffff, v.freq, v.vel, frame_); break;
          case I_BUZZ: buzz_.NoteOn(v.freq, v.vel); break;
          case I_BUZZ_OFF: buzz_.NoteOff(); break;
          case I_KICK: kick_.NoteOn(v.freq, v.vel); break;
          case I_SNARE: snare_.NoteOn(v.freq, v.vel); break;
          case I_HAT: hat_.NoteOn(v.freq, v.vel); break;
        }
      } else {
        v.at -= span;
        ev_[k++] = v;
      }
    }
    nev_ = k;
  }

  void Step() {
    const int s = step_ % 16;
    const int bar = (step_ / 16) % 4;
    const int root = kRoots[bar];
    const float jitter = 0.1f * rng_seq_.Next();
    /* release before retriggering, so the bass is a line and not a drone */
    if (bass_gate_ > 0 && --bass_gate_ == 0) bass_.NoteOff();
    if (mode_ == 1) {
      if (s % 2 == 0) hat_.NoteOn(330.0f, 0.12f + jitter);
      if (s == 0 || s == 8) { bass_.NoteOn(Mtof(static_cast<float>(root)), 0.6f); bass_gate_ = 6; }
      if (s % 4 == 2) {   /* a lazy arpeggio: coffee and coworking */
        static const int kArp[4] = {12, 19, 24, 19};
        stabs_.NoteOn(note_id_++ & 0xffff, Mtof(static_cast<float>(root + 12 + kArp[(s / 4) % 4])), 0.35f, frame_);
      }
    } else {
      if (s == 0 || s == 8 || s == 10 || (mode_ == 3 && s == 14)) kick_.NoteOn(50.0f, 0.95f);
      if (s == 4 || s == 12) snare_.NoteOn(190.0f, 0.75f + jitter);
      if (s == 15 && bar == 3) snare_.NoteOn(190.0f, 0.5f);
      if (s % 2 == 0) hat_.NoteOn(330.0f, (s % 4 == 2 ? 0.32f : 0.2f) + jitter);
      if (s % 2 == 0) { bass_.NoteOn(Mtof(static_cast<float>(root + (s == 6 || s == 14 ? 12 : 0))), 0.8f); bass_gate_ = 2; }
      if (s % 4 == 2) {   /* offbeat upstrokes: root, fifth, octave */
        const float v = 0.4f + jitter;
        stabs_.NoteOn(note_id_++ & 0xffff, Mtof(static_cast<float>(root + 24)), v, frame_);
        stabs_.NoteOn(note_id_++ & 0xffff, Mtof(static_cast<float>(root + 31)), v, frame_);
        stabs_.NoteOn(note_id_++ & 0xffff, Mtof(static_cast<float>(root + 36)), v * 0.8f, frame_);
      }
      if (mode_ == 3 && s % 2 == 0) {
        const int n = kLead[(bar * 8 + s / 2) % 32];
        if (n) lead_.NoteOn(Mtof(static_cast<float>(n)), 0.55f);
        else lead_.NoteOff();
      }
    }
    ++step_;
  }

  template <class Part>
  void Mix(Part& part, float dry, float send, float* l, float* r, int from, int n) {
    for (int i = 0; i < n; ++i) { dl_[i] = 0.0f; dr_[i] = 0.0f; }
    part.Process(dl_, dr_, 0, n);
    for (int i = 0; i < n; ++i) {
      l[from + i] += dry * dl_[i];
      r[from + i] += dry * dr_[i];
      sl_[i] += send * dl_[i];
      sr2_[i] += send * dr_[i];
    }
  }

  void RenderSpan(float* l, float* r, int from, int n) {
    for (int i = 0; i < n; ++i) { sl_[i] = 0.0f; sr2_[i] = 0.0f; }
    const float m = mode_ == 0 ? 0.0f : 0.8f;
    Mix(kick_, 0.5f, 0.0f, l, r, from, n);
    Mix(snare_, 0.3f * m + 0.1f, 0.1f, l, r, from, n);
    Mix(hat_, 0.16f, 0.0f, l, r, from, n);
    Mix(bass_, 0.34f * m, 0.0f, l, r, from, n);
    Mix(stabs_, 0.22f * m, 0.18f, l, r, from, n);
    Mix(lead_, 0.2f, 0.25f, l, r, from, n);
    Mix(bells_, 0.32f, 0.3f, l, r, from, n);
    Mix(sfx_pluck_, 0.42f, 0.2f, l, r, from, n);
    Mix(buzz_, 0.22f, 0.0f, l, r, from, n);
    delay_.Process(sl_, sr2_, 0, n);
    for (int i = 0; i < n; ++i) { l[from + i] += sl_[i]; r[from + i] += sr2_[i]; }
    limiter_.Process(l, r, from, from + n);
    frame_ += static_cast<uint32_t>(n);
  }

  bellows::Rng rng_seq_, rng_snare_, rng_bass_, rng_str_, rng_sfx_, rng_buzz_;
  bellows::Kick kick_;
  bellows::Snare snare_;
  bellows::Hat hat_;
  bellows::Va bass_, buzz_;
  bellows::VoicePool<String, 6> stabs_;
  bellows::VoicePool<String, 4> sfx_pluck_;
  bellows::VoicePool<Bell, 6> bells_;
  Bell lead_;
  bellows::StereoDelay<500> delay_;
  bellows::Limiter<> limiter_;

  float dl_[kBlock] = {}, dr_[kBlock] = {}, sl_[kBlock] = {}, sr2_[kBlock] = {};
  Ev ev_[64];
  int nev_ = 0;
  float sr_ = 48000.0f;
  int mode_ = -1, samples_per_step_ = 1, countdown_ = 0, step_ = 0, bass_gate_ = 0;
  int note_id_ = 0;
  uint32_t frame_ = 0;
  bool ready_ = false;
};

Mixer mixer;
int pending_mode = 1;
int pending_sfx[16];
int npending;
float L[4096], R[4096];
double frac;

}  // namespace

extern "C" void audio_play(int sfx) {
  if (mixer.Ready()) mixer.Sfx(sfx);
  else if (npending < 16) pending_sfx[npending++] = sfx;
}

extern "C" void audio_music(int mode) {
  pending_mode = mode;
  if (mixer.Ready()) mixer.SetMode(mode);
}

extern "C" void audio_render(float* ring, uint32_t cap, uint32_t* cursor, uint32_t rate, double dt_ms) {
  if (!mixer.Ready()) {
    mixer.Init(static_cast<float>(rate ? rate : 48000));
    mixer.SetMode(pending_mode);
    for (int i = 0; i < npending; ++i) mixer.Sfx(pending_sfx[i]);
    npending = 0;
  }
  double want = rate * dt_ms / 1000.0 + frac;
  int n = static_cast<int>(want);
  frac = want - n;
  if (n > static_cast<int>(cap / 2)) n = static_cast<int>(cap / 2);
  while (n > 0) {
    int chunk = n > 4096 ? 4096 : n;
    for (int i = 0; i < chunk; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
    mixer.Render(L, R, chunk);
    uint32_t c = *cursor;
    for (int i = 0; i < chunk; ++i) {
      ring[c * 2] = L[i];
      ring[c * 2 + 1] = R[i];
      c = (c + 1) % cap;
    }
    *cursor = c;
    n -= chunk;
  }
}
