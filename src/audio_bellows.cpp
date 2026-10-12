/* DO-OCRACY's sound, on Bellows (bellows-embedded, the C++ port of
 * bellows.live's DSP core, vendored in third_party/bellows).
 *
 * The music is written, not generated, the way doot-doot-revolution does
 * it: each song is a form of sections, a chord per bar, a set of layers per
 * section, and a written lead line. A small conductor fills in the backing
 * (drums, a genre bass groove, voice-led pads, arps) on a 16-step grid, so
 * the arrangement builds and breathes: intro on pad and arp, the first verse
 * without snare, the second verse bigger, every chorus with everything, a
 * stripped bridge so the last chorus lands, fills at the end of sections.
 *
 *   1  lobby / attract   "Coffee and Coworking"   city pop, 112 bpm
 *   2  the night         "Hack Your Hackerspace"  synthwave, 140 bpm, two passes that build
 *   3  closing time      "Who Is Doing The Dishes" synthcore, 168 bpm, hook on a loop
 *
 * Effects are notes on their own voices on top: FM bells for clout, plucked
 * strings for karma, a VA buzzer for trouble. Same API as audio.h, so the C
 * game never knows. */
#include "bellows/config.h"
#include "bellows/core/prng.h"
#include "bellows/engines/drums.h"
#include "bellows/engines/fm.h"
#include "bellows/engines/pluck.h"
#include "bellows/engines/va.h"
#include "bellows/fx/delay.h"
#include "bellows/fx/dynamics.h"
#include "bellows/fx/modfx.h"
#include "bellows/fx/plate.h"
#include "bellows/fx/saturator.h"
#include "bellows/voicepool.h"
#include <math.h>
#include <stdint.h>

extern "C" {
#include "audio.h"
}

namespace {

constexpr int kBlock = BELLOWS_BLOCK_SIZE;
float Mtof(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }

/* ── the songs ─────────────────────────────────────────────────────
 * Notes are NAME+OCTAVE:LENGTH in sixteenths, '-' is a rest, '|' ends a bar.
 * Every melody is two four-bar halves that answer each other. */

enum Layer : uint16_t {
  L_BASS = 1, L_KICK = 2, L_SNARE = 4, L_HAT = 8, L_PERC = 16, L_PAD = 32, L_ARP = 64, L_LEAD = 128, L_COUNTER = 256,
};
constexpr uint16_t L_ALL = L_BASS | L_KICK | L_SNARE | L_HAT | L_PERC | L_PAD | L_ARP | L_LEAD;

/* the night: Em C G D in the verse, C G D Em in the chorus (the lift), and a
 * bridge to Am C G B that borrows the D# so the last chorus resolves */
const char* const NIGHT_VERSE =
  "E5:4 G5:2 E5:2 B4:4 E5:4 | C5:4 E5:2 G5:2 E5:4 C5:4 | B4:4 D5:2 G5:2 B5:4 A5:4 | F#5:4 A5:4 D5:8 | "
  "E5:4 G5:2 E5:2 B4:4 E5:4 | C5:4 E5:2 G5:2 C6:4 B5:4 | D5:4 G5:2 B5:2 D6:4 B5:4 | A5:4 F#5:4 D5:8";
const char* const NIGHT_HOOK =
  "G5:2 G5:2 E6:4 D6:2 C6:2 G5:4 | G5:2 B5:2 D6:4 C6:2 B5:2 G5:4 | F#5:2 A5:2 D6:4 C6:2 A5:2 F#5:4 | G5:4 B5:4 E6:8 | "
  "G5:2 G5:2 E6:4 D6:2 C6:2 G5:4 | G5:2 B5:2 D6:4 G6:2 D6:2 B5:4 | A5:2 B5:2 C6:4 B5:2 A5:2 F#5:4 | G5:8 E5:8";
const char* const NIGHT_BRIDGE =
  "A4:4 C5:4 E5:6 D5:2 | C5:4 E5:4 G5:8 | B4:4 D5:4 G5:6 F#5:2 | D#5:4 F#5:4 B5:8 | "
  "A4:4 C5:4 E5:6 G5:2 | E5:4 G5:2 C6:2 B5:4 G5:4 | D5:4 G5:4 B5:8 | B5:4 A5:4 F#5:4 D#5:4";
/* coffee and coworking: the same key, half the hurry */
const char* const LOBBY_LEAD =
  "B4:6 E5:2 G5:8 | C5:6 E5:2 G5:8 | D5:6 G5:2 B5:8 | A5:8 F#5:8 | "
  "B4:6 E5:2 G5:4 B5:4 | C5:6 E5:2 G5:4 C6:4 | D6:4 B5:4 G5:8 | F#5:8 -:8";

struct SectionDef { const char* tag; int bars; const char* chords; uint16_t layers; const char* lead; };

const SectionDef NIGHT[] = {
  {"INTRO", 4, "Em C G D", L_PAD | L_ARP, nullptr},
  {"VERSE", 8, "Em C G D Em C G D", L_BASS | L_KICK | L_HAT | L_PAD | L_LEAD, NIGHT_VERSE},
  {"CHORUS", 8, "C G D Em C G D Em", L_ALL, NIGHT_HOOK},
  {"VERSE", 8, "Em C G D Em C G D", L_BASS | L_KICK | L_SNARE | L_HAT | L_PAD | L_ARP | L_LEAD, NIGHT_VERSE},
  {"CHORUS", 8, "C G D Em C G D Em", L_ALL, NIGHT_HOOK},
  {"BRIDGE", 8, "Am C G B Am C G B", L_BASS | L_KICK | L_HAT | L_PAD | L_LEAD, NIGHT_BRIDGE},
  {"CHORUS", 8, "C G D Em C G D Em", L_ALL | L_COUNTER, NIGHT_HOOK},
};
constexpr int kNightLoop = 3;   /* after the last chorus, back to the second verse */

const SectionDef LOBBY[] = {
  {"INTRO", 4, "Em C G D", L_PAD | L_ARP, nullptr},
  {"VERSE", 8, "Em C G D Em C G D", L_BASS | L_KICK | L_HAT | L_PAD | L_ARP | L_LEAD, LOBBY_LEAD},
  {"CHORUS", 8, "C G D Em C G D Em", L_BASS | L_KICK | L_HAT | L_PERC | L_PAD | L_ARP | L_LEAD, NIGHT_HOOK},
};
constexpr int kLobbyLoop = 1;

const SectionDef CLOSING[] = {
  {"CHORUS", 8, "C G D Em C G D Em", L_ALL | L_COUNTER, NIGHT_HOOK},
  {"CHORUS", 8, "C G D Em C G D Em", L_ALL | L_COUNTER, NIGHT_HOOK},
};

/* parsed forms of the above */
struct Note { int16_t step; int8_t midi; uint8_t dur; };   /* step from the section start; midi 0 = rest */
struct Section {
  const SectionDef* def;
  int roots[8];          /* chord root pitch class per bar */
  bool minor[8];
  Note lead[96];
  int nlead;
};

int PitchClass(char c) {
  switch (c) { case 'C': return 0; case 'D': return 2; case 'E': return 4; case 'F': return 5; case 'G': return 7; case 'A': return 9; case 'B': return 11; }
  return 0;
}

void ParseSection(const SectionDef* d, Section* s) {
  s->def = d;
  const char* c = d->chords;
  for (int b = 0; b < 8 && *c; b++) {
    while (*c == ' ') c++;
    int pc = PitchClass(*c++);
    if (*c == '#') { pc = (pc + 1) % 12; c++; } else if (*c == 'b') { pc = (pc + 11) % 12; c++; }
    s->minor[b] = *c == 'm';
    while (*c && *c != ' ') c++;
    s->roots[b] = pc;
  }
  s->nlead = 0;
  if (!d->lead) return;
  const char* p = d->lead;
  int step = 0;
  while (*p && s->nlead < 96) {
    while (*p == ' ') p++;
    if (*p == '|') { step = ((step + 15) / 16) * 16; p++; continue; }
    if (!*p) break;
    int midi = 0;
    if (*p == '-') { p++; }
    else {
      int pc = PitchClass(*p++);
      if (*p == '#') { pc++; p++; } else if (*p == 'b') { pc--; p++; }
      int oct = *p++ - '0';
      midi = 12 * (oct + 1) + pc;
    }
    int dur = 4;
    if (*p == ':') { p++; dur = 0; while (*p >= '0' && *p <= '9') dur = dur * 10 + (*p++ - '0'); }
    s->lead[s->nlead++] = Note{static_cast<int16_t>(step), static_cast<int8_t>(midi), static_cast<uint8_t>(dur)};
    step += dur;
  }
}

/* E minor, for diatonic harmony (the counter line sits a third below the lead) */
int DiatonicBelow(int midi) {
  static const int kScale[7] = {4, 6, 7, 9, 11, 0, 2};   /* E F# G A B C D */
  int pc = midi % 12;
  for (int i = 0; i < 7; i++) if (kScale[i] == pc) {
    int down = kScale[(i + 5) % 7];
    int d = pc - down;
    if (d <= 0) d += 12;
    return midi - d;
  }
  return midi - 3;
}

/* ── the band ──────────────────────────────────────────────────── */

using String = bellows::Pluck<70>;
using Bell = bellows::Fm;
enum Genre { G_CITYPOP, G_SYNTHWAVE, G_SYNTHCORE };
enum Inst { I_BELL, I_PLUCK, I_BUZZ, I_KICK, I_SNARE, I_HAT, I_OHAT, I_CLAP, I_BUZZ_OFF };
struct Ev { int at; int inst; float freq, vel; };

struct Groove { int8_t step, off; };
const Groove kBassCity[] = {{0, 0}, {3, 7}, {6, 0}, {8, 12}, {11, 7}, {14, 0}};
const Groove kBassWave[] = {{0, 0}, {4, 0}, {7, 12}, {8, 0}, {12, 0}, {14, 7}};
const Groove kBassCore[] = {{0, 0}, {2, 0}, {4, 0}, {6, 0}, {8, 0}, {10, 12}, {12, 0}, {14, 0}};

class Band {
 public:
  void Init(float sr) {
    sr_ = sr;
    rng_snare_.Init("do-ocracy::kit::snare/noise");
    rng_clap_.Init("do-ocracy::kit::clap/noise");
    rng_bass_.Init("do-ocracy::bass");
    rng_str_.Init("do-ocracy::arp");
    rng_sfx_.Init("do-ocracy::sfx");
    rng_buzz_.Init("do-ocracy::buzz");
    rng_ctr_.Init("do-ocracy::counter");

    for (unsigned i = 0; i < sizeof NIGHT / sizeof NIGHT[0]; i++) ParseSection(&NIGHT[i], &night_[i]);
    for (unsigned i = 0; i < sizeof LOBBY / sizeof LOBBY[0]; i++) ParseSection(&LOBBY[i], &lobby_[i]);
    for (unsigned i = 0; i < sizeof CLOSING / sizeof CLOSING[0]; i++) ParseSection(&CLOSING[i], &closing_[i]);

    bellows::Hat::Params oh; oh.decay = 0.28f; oh.tone = 0.9f;
    ohat_.Init(sr, oh);
    bellows::Snare::Params cp; cp.tone = 0.2f; cp.snap = 0.9f; cp.decay = 0.14f;
    clap_.Init(sr, &rng_clap_, cp);

    /* the pad: a soft two-operator FM voice, a triad that clears before the
     * next bar. A detuned VA pad sounded the same and cost most of the engine's
     * CPU, and a Raspberry Pi has to run this */
    bellows::Fm::Params pad;
    pad.ops = 2; pad.algorithm = 1; pad.feedback = 0.05f; pad.brightness = 0.25f;
    pad.attack = 0.3f; pad.decay = 0.6f; pad.sustain = 0.8f; pad.release = 0.5f;
    pad.m_attack = 0.4f; pad.m_decay = 0.8f; pad.m_sustain = 0.4f; pad.m_release = 0.5f;
    pad.ratio[0] = 1.0f; pad.ratio[1] = 2.0f; pad.level[1] = 0.35f;
    for (int i = 0; i < 6; ++i) pad_.at(i).Init(sr, pad);

    String::Params arp; arp.damp = 0.45f; arp.pick_pos = 0.25f; arp.decay = 0.6f; arp.level = 0.85f;
    for (int i = 0; i < 6; ++i) arp_.at(i).Init(sr, &rng_str_, arp);

    bellows::Va::Params ctr; ctr.shape = 3.0f; ctr.cutoff = 1400.0f; ctr.detune = 4.0f; ctr.attack = 0.01f; ctr.sustain = 0.6f; ctr.release = 0.15f;
    counter_.Init(sr, &rng_ctr_, ctr);

    String::Params pl; pl.damp = 0.15f; pl.pick_pos = 0.3f; pl.decay = 1.6f; pl.level = 0.9f;
    for (int i = 0; i < 4; ++i) sfx_pluck_.at(i).Init(sr, &rng_sfx_, pl);
    Bell::Params bell;
    bell.ops = 4; bell.algorithm = 3; bell.brightness = 0.7f;
    bell.attack = 0.001f; bell.decay = 0.5f; bell.sustain = 0.0f; bell.release = 0.4f;
    bell.m_decay = 0.3f; bell.m_sustain = 0.1f;
    for (int i = 0; i < 6; ++i) bells_.at(i).Init(sr, bell);
    bellows::Va::Params bz;
    bz.shape = 0.85f; bz.detune = 18.0f; bz.cutoff = 2400.0f; bz.resonance = 0.1f;
    bz.attack = 0.002f; bz.decay = 0.1f; bz.sustain = 0.7f; bz.release = 0.05f;
    buzz_.Init(sr, &rng_buzz_, bz);

    bellows::PlateExt::Params pp; pp.decay = 0.66f; pp.damping = 0.5f; pp.mix = 1.0f; pp.predelay = 0.01f;
    plate_.Init(sr, pp);
    bellows::StereoDelay<500>::Params d;
    d.time_l = 0.26f; d.time_r = 0.39f; d.feedback = 0.32f; d.cross_feedback = 0.1f; d.damping = 3500.0f; d.mix = 1.0f;
    delay_.Init(sr, d);
    bellows::Chorus<>::Params ch; ch.rate = 2.5f; ch.depth = 0.5f; ch.mix = 0.4f;
    chorus_.Init(sr, ch);
    bellows::Saturator<>::Params sat; sat.drive = 1.5f; sat.tone = -0.3f;
    bass_sat_.Init(sr, sat);
    bellows::Compressor<>::Params cmp; cmp.threshold_db = -14.0f; cmp.ratio = 2.5f; cmp.knee_db = 6.0f; cmp.attack = 0.006f; cmp.release = 0.16f;
    comp_.Init(sr, cmp);
    bellows::Limiter<>::Params lim; lim.ceiling_db = -0.8f; lim.release = 0.08f;
    limiter_.Init(sr, lim);

    SetGenre(G_CITYPOP);
    SetMode(1);
    ready_ = true;
  }

  bool Ready() const { return ready_; }

  /* patches per genre, after doot-doot-revolution's racks */
  void SetGenre(Genre g) {
    genre_ = g;
    bellows::Va::Params bass;
    bass.attack = 0.003f; bass.decay = 0.24f; bass.sustain = 0.45f; bass.release = 0.08f; bass.f_decay = 0.12f; bass.f_sustain = 0.2f;
    bellows::Fm::Params lead;
    lead.ops = 4; lead.algorithm = 4; lead.attack = 0.005f; lead.release = 0.12f;
    bellows::Kick::Params kp;
    bellows::Snare::Params sp;
    bellows::Hat::Params hp;
    switch (g) {
      case G_CITYPOP:
        bass.shape = 2.0f; bass.cutoff = 560.0f; bass.resonance = 0.3f; bass.env_amount = 1.2f; bass.sub = 0.4f;
        lead.ratio[0] = 1; lead.ratio[1] = 2; lead.ratio[2] = 3; lead.ratio[3] = 1; lead.feedback = 0.12f; lead.brightness = 0.4f; lead.decay = 0.6f; lead.sustain = 0.45f;
        kp.decay = 0.3f; kp.drive = 1.6f; kp.pitch_decay = 0.06f;
        sp.decay = 0.16f; sp.tone = 0.5f; sp.snap = 0.1f;
        hp.decay = 0.05f; hp.tone = 1.0f;
        break;
      case G_SYNTHWAVE:
        bass.shape = 0.0f; bass.cutoff = 520.0f; bass.resonance = 0.5f; bass.env_amount = 1.6f; bass.sub = 0.5f; bass.drift = 0.2f;
        lead.ratio[0] = 1; lead.ratio[1] = 2.5f; lead.ratio[2] = 5; lead.ratio[3] = 7; lead.feedback = 0.28f; lead.brightness = 0.5f; lead.decay = 0.35f; lead.sustain = 0.55f;
        kp.decay = 0.35f; kp.pitch_decay = 0.08f; kp.drive = 2.0f; kp.click_tune = 6.0f;
        sp.decay = 0.19f; sp.tone = 0.6f; sp.snap = 0.08f;
        hp.decay = 0.06f; hp.tone = 1.1f;
        break;
      case G_SYNTHCORE:
        bass.shape = 0.0f; bass.cutoff = 720.0f; bass.resonance = 0.38f; bass.env_amount = 1.5f; bass.sub = 0.6f; bass.detune = 8.0f;
        lead.ratio[0] = 1; lead.ratio[1] = 2; lead.ratio[2] = 3.5f; lead.ratio[3] = 7; lead.feedback = 0.32f; lead.brightness = 0.6f; lead.decay = 0.3f; lead.sustain = 0.6f;
        kp.decay = 0.3f; kp.pitch_decay = 0.05f; kp.drive = 2.2f; kp.click_tune = 8.0f;
        sp.decay = 0.17f; sp.tone = 0.6f; sp.snap = 0.06f;
        hp.decay = 0.05f; hp.tone = 1.2f;
        break;
    }
    bass_.Init(sr_, &rng_bass_, bass);
    lead_.Init(sr_, lead);
    kick_.Init(sr_, kp);
    snare_.Init(sr_, &rng_snare_, sp);
    hat_.Init(sr_, hp);
  }

  /* 0 silence, 1 lobby, 2 night, 3 closing time */
  void SetMode(int m) {
    if (m == mode_) return;
    pending_mode_ = m;
    if (mode_ <= 0 || m == 0) ApplyMode();   /* from silence or into it: now; otherwise at the next bar */
  }

  void Push(int delay_ms, int inst, float freq, float vel) {
    if (nev_ >= 96) return;
    ev_[nev_++] = Ev{static_cast<int>(delay_ms * sr_ / 1000.0f), inst, freq, vel};
  }

  void Sfx(int s) {
    float r = rng_sfx_.Next();
    switch (s) {
      case SFX_PICKUP: Push(0, I_PLUCK, Mtof(76), 0.6f); Push(40, I_PLUCK, Mtof(83), 0.5f); break;
      case SFX_DROP: Push(0, I_PLUCK, Mtof(52), 0.7f); Push(0, I_HAT, 400, 0.3f); break;
      case SFX_TICK: Push(0, I_HAT, 330, 0.22f); break;
      case SFX_GLAMOUR:   /* clout: a rising bell arpeggio, a little too pleased with itself */
        Push(0, I_BELL, Mtof(76), 0.7f); Push(70, I_BELL, Mtof(80), 0.7f);
        Push(140, I_BELL, Mtof(83), 0.7f); Push(210, I_BELL, Mtof(88), 0.9f); break;
      case SFX_KARMA:     /* karma: two quiet strings, nobody notices */
        Push(0, I_PLUCK, Mtof(79), 0.6f); Push(90, I_PLUCK, Mtof(86), 0.55f); break;
      case SFX_LASER:
        Push(0, I_BELL, Mtof(96 + r * 6), 0.3f); Push(0, I_HAT, 330, 0.35f); break;
      case SFX_BAD:
        Push(0, I_BUZZ, Mtof(40), 0.8f); Push(260, I_BUZZ_OFF, 0, 0); break;
      case SFX_JOIN:
        Push(0, I_PLUCK, Mtof(64), 0.7f); Push(60, I_PLUCK, Mtof(71), 0.7f); Push(120, I_PLUCK, Mtof(76), 0.8f); break;
      case SFX_ALARM:
        for (int i = 0; i < 4; ++i) Push(i * 140, I_BUZZ, Mtof(i & 1 ? 76 : 81), 0.5f);
        Push(560, I_BUZZ_OFF, 0, 0); break;
      case SFX_WIN: {     /* C, D, Em: the jingle resolves home with a crash */
        const int up[3][3] = {{60, 64, 67}, {62, 66, 69}, {64, 67, 71}};
        for (int c = 0; c < 3; ++c) for (int n = 0; n < 3; ++n) Push(c * 260 + n * 70, I_BELL, Mtof(static_cast<float>(up[c][n] + 12)), 0.65f);
        Push(780, I_BELL, Mtof(88), 0.9f); Push(780, I_KICK, 50, 0.95f); Push(780, I_OHAT, 330, 0.7f); Push(780, I_PLUCK, Mtof(40), 0.9f);
        break;
      }
      case SFX_LOSE:
        Push(0, I_BUZZ, Mtof(52), 0.7f); Push(300, I_BUZZ, Mtof(51), 0.7f); Push(600, I_BUZZ, Mtof(50), 0.7f);
        Push(900, I_BUZZ, Mtof(49), 0.8f); Push(1500, I_BUZZ_OFF, 0, 0); break;
      case SFX_TALK:      /* a proposal: confident mumbling */
        for (int i = 0; i < 4; ++i) Push(i * 55, I_BELL, Mtof(62 + rng_sfx_.Next() * 12), 0.28f);
        break;
      case SFX_SLURP: Push(0, I_SNARE, 300, 0.2f); Push(80, I_PLUCK, Mtof(60), 0.5f); Push(160, I_PLUCK, Mtof(67), 0.5f); break;
    }
  }

  void Render(float* l, float* r, int n) {
    int i = 0;
    while (i < n) {
      if (mode_ > 0 && countdown_ <= 0) { Step(); countdown_ = samples_per_step_; }
      int span = n - i;
      if (mode_ > 0 && span > countdown_) span = countdown_;
      if (span > kBlock) span = kBlock;
      Fire(span);
      RenderSpan(l, r, i, span);
      i += span;
      if (mode_ > 0) countdown_ -= span;
    }
  }

 private:
  void ApplyMode() {
    mode_ = pending_mode_;
    pending_mode_ = -1;
    section_ = 0; bar_ = 0; step_ = 0; pass_ = 0; countdown_ = 0;
    if (mode_ == 0) { bass_.NoteOff(); lead_.NoteOff(); counter_.NoteOff(); PadRelease(); return; }
    unsigned bpm = mode_ == 3 ? 168 : mode_ == 2 ? 140 : 112;
    samples_per_step_ = static_cast<int>(sr_ * 60.0f / (static_cast<float>(bpm) * 4.0f) + 0.5f);
    SetGenre(mode_ == 3 ? G_SYNTHCORE : mode_ == 2 ? G_SYNTHWAVE : G_CITYPOP);
    last_chord_ = -1;
  }

  const Section& Current(int* count, int* loop) const {
    if (mode_ == 3) { *count = 2; *loop = 0; return closing_[section_]; }
    if (mode_ == 2) { *count = sizeof NIGHT / sizeof NIGHT[0]; *loop = kNightLoop; return night_[section_]; }
    *count = sizeof LOBBY / sizeof LOBBY[0]; *loop = kLobbyLoop; return lobby_[section_];
  }

  void PadRelease() { for (int i = 0; i < npad_; i++) pad_.NoteOff(pad_ids_[i]); npad_ = 0; }

  /* smooth voice leading: each chord tone goes to the octave nearest the last voicing */
  void PadChord(int root, bool minor, float vel) {
    int tones[3] = {root, (root + (minor ? 3 : 4)) % 12, (root + 7) % 12};
    PadRelease();
    float center = pad_center_;
    float sum = 0;
    for (int k = 0; k < 3; k++) {
      int best = 0;
      float bd = 1e9f;
      for (int oct = 3; oct <= 5; oct++) {
        int m = 12 * (oct + 1) + tones[k];
        if (m < 50 || m > 76) continue;
        float d = fabsf(m - center);
        if (d < bd) { bd = d; best = m; }
      }
      if (!best) best = 60 + tones[k];
      int id = 1000 + (note_id_++ & 0xfff);
      pad_.NoteOn(id, Mtof(static_cast<float>(best)), vel, frame_);
      pad_ids_[npad_++] = id;
      sum += best;
    }
    pad_center_ = sum / 3.0f;
  }

  void Step() {
    if (pending_mode_ >= 0 && step_ % 16 == 0) ApplyMode();
    int count, loop;
    const Section* sec = &Current(&count, &loop);
    const int s = step_ % 16;
    if (s == 0 && step_ > 0) {
      if (++bar_ >= sec->def->bars) {
        bar_ = 0;
        if (++section_ >= count) { section_ = loop; pass_++; }
        sec = &Current(&count, &loop);
      }
    }
    const uint16_t lay = sec->def->layers;
    int n = 0;
    static const uint16_t kCount[7] = {L_KICK, L_SNARE, L_HAT, L_BASS, L_ARP, L_LEAD, L_PAD};
    for (int k = 0; k < 7; k++) n += (lay & kCount[k]) ? 1 : 0;
    float its = 0.18f + 0.12f * n;
    if (its < 0.28f) its = 0.28f;
    if (its > 1.0f) its = 1.0f;
    if (mode_ == 1) its *= 0.75f;                 /* the lobby never gets loud */
    const bool second = pass_ > 0 || mode_ == 3;  /* the second time through, it builds */
    const bool last_bar = bar_ == sec->def->bars - 1;
    const int root = sec->roots[bar_ % 8];
    const bool minor = sec->minor[bar_ % 8];
    const float jit = 0.06f * rng_sfx_.Next();

    /* drums */
    if (lay & L_KICK) {
      bool four = its > 0.4f && mode_ != 1;
      if ((four && s % 4 == 0) || (!four && (s == 0 || s == 8))) kick_.NoteOn(50.0f, 0.86f + 0.14f * its);
      if (mode_ == 3 && bar_ % 4 == 3 && s == 14) kick_.NoteOn(50.0f, 0.8f);
    }
    if ((lay & L_SNARE) && (s == 4 || s == 12)) snare_.NoteOn(190.0f, 0.8f + jit);
    if (last_bar && its > 0.6f && (lay & L_SNARE) && (s == 14 || s == 15)) { snare_.NoteOn(190.0f, 0.45f); if (s == 14) kick_.NoteOn(50.0f, 0.7f); }
    if (lay & L_HAT) {
      bool sixteenths = its > 0.7f || (second && mode_ == 2 && (lay & L_SNARE));
      if (s == 6 || s == 14) ohat_.NoteOn(330.0f, 0.25f + 0.15f * its);
      else if (sixteenths || s % 2 == 0) hat_.NoteOn(330.0f, (0.3f + 0.22f * its) * (s % 4 == 2 ? 1.0f : 0.75f) + jit);
    }
    if ((lay & L_PERC) && s % 4 == 2 && s != 6 && s != 14) {
      /* a clap is three snares 10 ms apart */
      float v = 0.34f + 0.24f * its;
      Push(0, I_CLAP, 0, v); Push(10, I_CLAP, 0, v * 0.8f); Push(21, I_CLAP, 0, v * 0.6f);
    }

    /* bass: the genre groove on the chord root, in its own register */
    if (bass_gate_ > 0 && --bass_gate_ == 0) bass_.NoteOff();
    if (lay & L_BASS) {
      const Groove* gr = genre_ == G_CITYPOP ? kBassCity : genre_ == G_SYNTHCORE ? kBassCore : kBassWave;
      int ng = genre_ == G_CITYPOP ? 6 : genre_ == G_SYNTHCORE ? 8 : 6;
      for (int k = 0; k < ng; k++) if (gr[k].step == s) {
        int m = 36 + ((root - 0 + 12) % 12);
        if (m > 47) m -= 12;
        bass_.NoteOn(Mtof(static_cast<float>(m + gr[k].off)), 0.72f + 0.18f * its);
        bass_gate_ = 2;
      }
    }

    /* pad: one held chord per bar, voice-led */
    int chord_id = bar_ * 100 + section_;
    if ((lay & L_PAD) && s == 0 && chord_id != last_chord_) { PadChord(root, minor, 0.62f); last_chord_ = chord_id; }
    if (!(lay & L_PAD) && npad_) PadRelease();

    /* arp: chord tones rising, eighths, sixteenths and bouncing when it is busy */
    if (lay & L_ARP) {
      bool fast = its > 0.7f;
      if (fast || s % 2 == 0) {
        int tones[4] = {root, root + (minor ? 3 : 4), root + 7, root + 12};
        int idx = fast ? s : s / 2;
        int k = fast ? ((idx / 4) % 2 ? 3 - idx % 4 : idx % 4) : idx % 4;
        int m = 64 + ((tones[k] - 4 + 24) % 24);
        arp_.NoteOn(note_id_++ & 0xfff, Mtof(static_cast<float>(m)), (fast ? 0.42f : 0.5f) + jit, frame_);
      }
    }

    /* the written lead, and on the second pass a counter line a third below */
    if (lay & L_LEAD) {
      int at = bar_ * 16 + s;
      int oct = (second && mode_ == 2 && section_ == static_cast<int>(sizeof NIGHT / sizeof NIGHT[0]) - 1) ? 12 : 0;
      if (mode_ == 1) oct = -12;
      for (int k = 0; k < sec->nlead; k++) {
        const Note& nt = sec->lead[k];
        if (nt.step == at) {
          if (nt.midi) {
            lead_.NoteOn(Mtof(static_cast<float>(nt.midi + oct)), mode_ == 1 ? 0.55f : 0.72f);
            if ((lay & L_COUNTER) || (second && mode_ == 2 && (lay & L_SNARE)))
              counter_.NoteOn(Mtof(static_cast<float>(DiatonicBelow(nt.midi) + oct - 12)), 0.5f);
          } else { lead_.NoteOff(); counter_.NoteOff(); }
          lead_off_ = nt.dur;
        }
      }
      if (lead_off_ > 0 && --lead_off_ == 0) { lead_.NoteOff(); counter_.NoteOff(); }
    } else if (s == 0) { lead_.NoteOff(); counter_.NoteOff(); }
    ++step_;
  }

  void Fire(int span) {
    int k = 0;
    for (int e = 0; e < nev_; ++e) {
      Ev& v = ev_[e];
      if (v.at <= 0) {
        switch (v.inst) {
          case I_BELL: bells_.NoteOn(note_id_++ & 0xfff, v.freq, v.vel, frame_); break;
          case I_PLUCK: sfx_pluck_.NoteOn(note_id_++ & 0xfff, v.freq, v.vel, frame_); break;
          case I_BUZZ: buzz_.NoteOn(v.freq, v.vel); break;
          case I_BUZZ_OFF: buzz_.NoteOff(); break;
          case I_KICK: kick_.NoteOn(v.freq, v.vel); break;
          case I_SNARE: snare_.NoteOn(v.freq ? v.freq : 190.0f, v.vel); break;
          case I_HAT: hat_.NoteOn(v.freq, v.vel); break;
          case I_OHAT: ohat_.NoteOn(v.freq, v.vel); break;
          case I_CLAP: clap_.NoteOn(1400.0f, v.vel); break;
        }
      } else {
        v.at -= span;
        ev_[k++] = v;
      }
    }
    nev_ = k;
  }

  template <class Part>
  void Mix(Part& part, float dry, float rev, float dly, float pan, float* l, float* r, int from, int n) {
    for (int i = 0; i < n; ++i) { dl_[i] = 0.0f; dr_[i] = 0.0f; }
    part.Process(dl_, dr_, 0, n);
    float gl = pan > 0 ? 1.0f - pan : 1.0f, gr = pan < 0 ? 1.0f + pan : 1.0f;
    for (int i = 0; i < n; ++i) {
      float a = dl_[i] * gl, b = dr_[i] * gr;
      l[from + i] += dry * a; r[from + i] += dry * b;
      rl_[i] += rev * a; rr_[i] += rev * b;
      yl_[i] += dly * a; yr_[i] += dly * b;
    }
  }

  void RenderSpan(float* l, float* r, int from, int n) {
    for (int i = 0; i < n; ++i) { rl_[i] = rr_[i] = yl_[i] = yr_[i] = 0.0f; }
    const float m = mode_ <= 0 ? 0.0f : 1.0f;
    Mix(kick_, 0.88f, 0.0f, 0.0f, 0.0f, l, r, from, n);
    Mix(snare_, 2.2f, 0.9f, 0.0f, 0.0f, l, r, from, n);
    Mix(clap_, 0.9f * m, 0.8f, 0.0f, -0.16f, l, r, from, n);
    Mix(hat_, 5.0f, 0.0f, 0.0f, 0.14f, l, r, from, n);
    Mix(ohat_, 3.5f, 0.6f, 0.0f, 0.14f, l, r, from, n);
    /* bass through its saturator before the mix */
    for (int i = 0; i < n; ++i) { bl_[i] = 0.0f; br_[i] = 0.0f; }
    bass_.Process(bl_, br_, 0, n);
    bass_sat_.Process(bl_, br_, 0, n);
    for (int i = 0; i < n; ++i) { l[from + i] += 0.46f * m * bl_[i]; r[from + i] += 0.46f * m * br_[i]; }
    Mix(pad_, 0.03f * m, 0.08f * m, 0.0f, 0.0f, l, r, from, n);
    Mix(arp_, 0.55f * m, 0.55f * m, 0.9f * m, -0.24f, l, r, from, n);
    /* lead through its chorus */
    for (int i = 0; i < n; ++i) { bl_[i] = 0.0f; br_[i] = 0.0f; }
    lead_.Process(bl_, br_, 0, n);
    chorus_.Process(bl_, br_, 0, n);
    for (int i = 0; i < n; ++i) {
      float a = bl_[i] * m, b = br_[i] * m;
      l[from + i] += 0.42f * a; r[from + i] += 0.42f * b;
      rl_[i] += 0.4f * a; rr_[i] += 0.4f * b; yl_[i] += 0.45f * a; yr_[i] += 0.45f * b;
    }
    Mix(counter_, 0.1f * m, 0.45f * m, 0.0f, 0.24f, l, r, from, n);
    /* effects sit above the music */
    Mix(bells_, 0.13f, 0.12f, 0.06f, 0.0f, l, r, from, n);
    Mix(sfx_pluck_, 1.1f, 0.5f, 0.0f, 0.0f, l, r, from, n);
    Mix(buzz_, 0.22f, 0.0f, 0.0f, 0.0f, l, r, from, n);
    plate_.Process(rl_, rr_, 0, n);
    delay_.Process(yl_, yr_, 0, n);
    for (int i = 0; i < n; ++i) {
      l[from + i] += 0.7f * rl_[i] + 0.6f * yl_[i];
      r[from + i] += 0.7f * rr_[i] + 0.6f * yr_[i];
    }
    comp_.Process(l, r, from, from + n);
    limiter_.Process(l, r, from, from + n);
    for (int i = 0; i < n; ++i) { l[from + i] *= 0.92f; r[from + i] *= 0.92f; }
    frame_ += static_cast<uint32_t>(n);
  }

  bellows::Rng rng_snare_, rng_clap_, rng_bass_, rng_str_, rng_sfx_, rng_buzz_, rng_ctr_;
  bellows::Kick kick_;
  bellows::Snare snare_, clap_;
  bellows::Hat hat_, ohat_;
  bellows::Va bass_, buzz_, counter_;
  bellows::VoicePool<bellows::Fm, 6> pad_;
  bellows::VoicePool<String, 6> arp_;
  bellows::VoicePool<String, 4> sfx_pluck_;
  bellows::VoicePool<Bell, 6> bells_;
  Bell lead_;
  bellows::Plate<> plate_;
  bellows::StereoDelay<500> delay_;
  bellows::Chorus<> chorus_;
  bellows::Saturator<> bass_sat_;
  bellows::Compressor<> comp_;
  bellows::Limiter<> limiter_;

  Section night_[sizeof NIGHT / sizeof NIGHT[0]];
  Section lobby_[sizeof LOBBY / sizeof LOBBY[0]];
  Section closing_[sizeof CLOSING / sizeof CLOSING[0]];

  float dl_[kBlock] = {}, dr_[kBlock] = {}, rl_[kBlock] = {}, rr_[kBlock] = {}, yl_[kBlock] = {}, yr_[kBlock] = {};
  float bl_[kBlock] = {}, br_[kBlock] = {};
  Ev ev_[96];
  int nev_ = 0;
  int pad_ids_[8] = {};
  int npad_ = 0;
  float pad_center_ = 62.0f;
  float sr_ = 48000.0f;
  Genre genre_ = G_CITYPOP;
  int mode_ = -1, pending_mode_ = -1, samples_per_step_ = 1, countdown_ = 0;
  int step_ = 0, bar_ = 0, section_ = 0, pass_ = 0, bass_gate_ = 0, lead_off_ = 0, last_chord_ = -1;
  int note_id_ = 0;
  uint32_t frame_ = 0;
  bool ready_ = false;
};

Band band;
int pending_mode = 1;
int pending_sfx[16];
int npending;
float L[4096], R[4096];
double frac;

}  // namespace

extern "C" void audio_play(int sfx) {
  if (band.Ready()) band.Sfx(sfx);
  else if (npending < 16) pending_sfx[npending++] = sfx;
}

extern "C" void audio_music(int mode) {
  pending_mode = mode;
  if (band.Ready()) band.SetMode(mode);
}

extern "C" void audio_render(float* ring, uint32_t cap, uint32_t* cursor, uint32_t rate, double dt_ms) {
  if (!band.Ready()) {
    band.Init(static_cast<float>(rate ? rate : 48000));
    band.SetMode(pending_mode);
    for (int i = 0; i < npending; ++i) band.Sfx(pending_sfx[i]);
    npending = 0;
  }
  double want = rate * dt_ms / 1000.0 + frac;
  int n = static_cast<int>(want);
  frac = want - n;
  if (n > static_cast<int>(cap / 2)) n = static_cast<int>(cap / 2);
  while (n > 0) {
    int chunk = n > 4096 ? 4096 : n;
    for (int i = 0; i < chunk; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
    band.Render(L, R, chunk);
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
