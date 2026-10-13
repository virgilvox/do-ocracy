/* DO-OCRACY's sound, on Bellows (bellows-embedded, the C++ port of
 * bellows.live's DSP core, vendored in third_party/bellows).
 *
 * The soundtrack is a love letter to the Hackers (1995) soundtrack: Orbital's
 * dreamy arpeggios, Underworld's hypnotic sixteenths, the Prodigy's breakbeats
 * and rave stabs, and a squelching acid line under all of it. Hack the planet.
 *
 * It is written the way doot-doot-revolution writes songs: each song is a
 * form of sections, a chord per bar, a set of layers per section and a written
 * lead line; a small conductor plays drums, the acid line, arps, stabs and
 * pads from those on a 16-step grid, so the arrangement builds and drops.
 *
 *   1  lobby / attract   "Halcyon Tuesday"   ambient electronica, 120 bpm
 *   2  the night         "Hack the Planet"   acid breakbeat, 136 bpm:
 *                        filtered acid intro, four-on-the-floor build, the drop,
 *                        a pads-only breakdown, the second drop with a counter line
 *   3  closing time      "Mess the Planet"   breakbeat rush, 160 bpm
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
 * E minor with Phrygian colour (the F natural) for the riff. */

enum Layer : uint16_t {
  L_ACID = 1,    /* the 303-style line */
  L_KICK = 2,    /* four on the floor */
  L_BREAK = 4,   /* breakbeat kick and snare instead */
  L_HAT = 8,
  L_PERC = 16,   /* claps on the backbeat */
  L_PAD = 32,
  L_HYPNO = 64,  /* the repeating arpeggio */
  L_LEAD = 128,
  L_COUNTER = 256,
  L_STAB = 512,  /* rave chord stabs */
  L_ROLL = 1024, /* snare roll into the next section */
  L_SUB = 2048,  /* a soft sub instead of the acid line (lobby) */
};

/* the riff: a two-bar call, a two-bar answer, then again with a new ending */
const char* const RIFF =
  "E5:2 E5:2 G5:2 E5:2 B5:2 E5:2 A5:2 G5:2 | E5:2 E5:2 G5:2 E5:2 D6:2 B5:2 A5:2 F5:2 | "
  "E5:2 E5:2 G5:2 E5:2 C6:2 B5:2 G5:2 E5:2 | F#5:2 A5:2 D6:4 C6:2 B5:2 A5:4 | "
  "E5:2 E5:2 G5:2 E5:2 B5:2 E5:2 A5:2 G5:2 | E5:2 E5:2 G5:2 E5:2 D6:2 B5:2 A5:2 F5:2 | "
  "G5:2 E5:2 C6:2 E5:2 B5:2 G5:2 E5:2 C5:2 | D5:4 F#5:4 E5:8";
/* the breakdown: long notes over the pads, then a B major turnaround back into the drop */
const char* const DREAM =
  "G5:8 E5:8 | F#5:8 A5:8 | B5:12 G5:4 | E5:16 | G5:8 C6:8 | A5:8 F#5:8 | D#5:8 F#5:8 | B5:16";
/* the lobby: slow bells, room to breathe */
const char* const HALCYON =
  "B5:12 -:4 | G5:12 E5:4 | D6:12 -:4 | F#5:16 | B5:8 E6:8 | C6:12 -:4 | B5:8 G5:8 | A5:16";

struct SectionDef { const char* tag; int bars; const char* chords; uint16_t layers; const char* lead; };

constexpr uint16_t DROP = L_BREAK | L_HAT | L_PERC | L_ACID | L_HYPNO | L_STAB | L_LEAD;
const SectionDef NIGHT[] = {
  {"INTRO", 8, "Em Em C D Em Em C D", L_HAT | L_ACID, nullptr},
  {"BUILD", 8, "Em Em C D Em Em C D", L_KICK | L_HAT | L_PERC | L_ACID | L_HYPNO | L_ROLL, nullptr},
  {"DROP", 8, "Em Em C D Em Em C D", DROP, RIFF},
  {"DROP", 8, "Em Em C D Em Em C D", DROP, RIFF},
  {"BREAK", 8, "C D Em Em C D B B", L_PAD | L_HYPNO | L_LEAD | L_ROLL, DREAM},
  {"DROP", 8, "Em Em C D Em Em C D", DROP | L_COUNTER, RIFF},
  {"DROP", 8, "Em Em C D Em Em C D", DROP | L_COUNTER, RIFF},
};
constexpr int kNightLoop = 1;   /* back to the build, and up again */

const SectionDef LOBBY[] = {
  {"A", 8, "Em C G D Em C G D", L_PAD | L_HYPNO | L_LEAD, HALCYON},
  {"B", 8, "Em C G D Em C G D", L_PAD | L_HYPNO | L_LEAD | L_KICK | L_HAT | L_SUB, HALCYON},
};
constexpr int kLobbyLoop = 0;

const SectionDef CLOSING[] = {
  {"RUSH", 8, "Em Em C D Em Em C D", DROP | L_COUNTER, RIFF},
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

/* the acid line: semitones over the bar's root, accents, -1 rests.
 * The 1 is the Phrygian F; the 10 and 3 are the dirty blue notes */
const int8_t ACID_N[16] = {0, -1, 12, 0, 0, 1, -1, 12, 0, -1, 10, 0, 12, 0, 7, 3};
const uint8_t ACID_A[16] = {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0};
/* a breakbeat: kick, snare and ghost notes (velocity, 0 = none) */
const float BRK_K[16] = {1, 0, .7f, 0, 0, 0, 0, 0, 0, 0, 1, .6f, 0, 0, 0, 0};
const float BRK_S[16] = {0, 0, 0, 0, 1, 0, 0, .3f, 0, .25f, 0, 0, 1, 0, 0, .4f};

/* ── the band ──────────────────────────────────────────────────── */

using String = bellows::Pluck<70>;
using Bell = bellows::Fm;
enum Genre { G_AMBIENT, G_ACID, G_RUSH };
enum Inst { I_BELL, I_PLUCK, I_BUZZ, I_KICK, I_SNARE, I_HAT, I_OHAT, I_CLAP, I_BUZZ_OFF };
struct Ev { int at; int inst; float freq, vel; };

class Band {
 public:
  void Init(float sr) {
    sr_ = sr;
    rng_snare_.Init("do-ocracy::kit::snare/noise");
    rng_clap_.Init("do-ocracy::kit::clap/noise");
    rng_acid_.Init("do-ocracy::acid");
    rng_str_.Init("do-ocracy::hypno");
    rng_sfx_.Init("do-ocracy::sfx");
    rng_buzz_.Init("do-ocracy::buzz");
    rng_ctr_.Init("do-ocracy::counter");

    for (unsigned i = 0; i < sizeof NIGHT / sizeof NIGHT[0]; i++) ParseSection(&NIGHT[i], &night_[i]);
    for (unsigned i = 0; i < sizeof LOBBY / sizeof LOBBY[0]; i++) ParseSection(&LOBBY[i], &lobby_[i]);
    for (unsigned i = 0; i < sizeof CLOSING / sizeof CLOSING[0]; i++) ParseSection(&CLOSING[i], &closing_[i]);

    bellows::Hat::Params oh; oh.decay = 0.22f; oh.tone = 0.95f;
    ohat_.Init(sr, oh);
    bellows::Snare::Params cp; cp.tone = 0.2f; cp.snap = 0.9f; cp.decay = 0.14f;
    clap_.Init(sr, &rng_clap_, cp);

    /* the pad: a soft two-operator FM voice; a detuned VA pad cost most of the CPU */
    bellows::Fm::Params pad;
    pad.ops = 2; pad.algorithm = 1; pad.feedback = 0.05f; pad.brightness = 0.25f;
    pad.attack = 0.6f; pad.decay = 0.8f; pad.sustain = 0.85f; pad.release = 0.9f;
    pad.m_attack = 0.6f; pad.m_decay = 1.0f; pad.m_sustain = 0.4f; pad.m_release = 0.8f;
    pad.ratio[0] = 1.0f; pad.ratio[1] = 2.0f; pad.level[1] = 0.35f;
    for (int i = 0; i < 6; ++i) pad_.at(i).Init(sr, pad);

    /* rave stabs: a bright FM organ hit */
    bellows::Fm::Params st;
    st.ops = 2; st.algorithm = 1; st.feedback = 0.45f; st.brightness = 0.7f;
    st.attack = 0.002f; st.decay = 0.18f; st.sustain = 0.0f; st.release = 0.12f;
    st.ratio[0] = 1.0f; st.ratio[1] = 1.0f; st.level[1] = 0.8f;
    for (int i = 0; i < 4; ++i) stab_.at(i).Init(sr, st);

    String::Params hy; hy.damp = 0.18f; hy.pick_pos = 0.2f; hy.decay = 0.5f; hy.level = 0.9f;
    for (int i = 0; i < 6; ++i) hypno_.at(i).Init(sr, &rng_str_, hy);

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

    bellows::PlateExt::Params pp; pp.decay = 0.7f; pp.damping = 0.45f; pp.mix = 1.0f; pp.predelay = 0.012f;
    plate_.Init(sr, pp);
    bellows::StereoDelay<500>::Params d;
    d.time_l = 0.33f; d.time_r = 0.22f; d.feedback = 0.38f; d.cross_feedback = 0.15f; d.damping = 3200.0f; d.mix = 1.0f;
    delay_.Init(sr, d);
    bellows::Chorus<>::Params ch; ch.rate = 1.2f; ch.depth = 0.45f; ch.mix = 0.35f;
    chorus_.Init(sr, ch);
    bellows::Saturator<2>::Params sat; sat.drive = 2.2f; sat.tone = 0.1f;
    acid_sat_.Init(sr, sat);
    bellows::Compressor<>::Params cmp; cmp.threshold_db = -14.0f; cmp.ratio = 2.5f; cmp.knee_db = 6.0f; cmp.attack = 0.006f; cmp.release = 0.16f;
    comp_.Init(sr, cmp);
    bellows::Limiter<>::Params lim; lim.ceiling_db = -0.8f; lim.release = 0.08f;
    limiter_.Init(sr, lim);

    SetGenre(G_AMBIENT);
    SetMode(1);
    ready_ = true;
  }

  bool Ready() const { return ready_; }
  void SetLevels(float music, float sfx) { music_gain_ = music; sfx_gain_ = sfx; }

  void SetGenre(Genre g) {
    genre_ = g;
    /* the acid line: a resonant saw with a snappy filter envelope, no sustain */
    acid_p_ = bellows::Va::Params();
    acid_p_.shape = 0.0f; acid_p_.detune = 0.0f; acid_p_.resonance = g == G_AMBIENT ? 0.15f : 0.82f;
    acid_p_.cutoff = g == G_AMBIENT ? 260.0f : 400.0f; acid_p_.env_amount = g == G_AMBIENT ? 0.4f : 2.6f;
    acid_p_.attack = 0.002f; acid_p_.decay = 0.18f; acid_p_.sustain = g == G_AMBIENT ? 0.7f : 0.25f; acid_p_.release = 0.05f;
    acid_p_.f_attack = 0.001f; acid_p_.f_decay = 0.14f; acid_p_.f_sustain = 0.0f; acid_p_.f_release = 0.05f;
    acid_p_.sub = g == G_AMBIENT ? 0.8f : 0.2f;
    acid_.Init(sr_, &rng_acid_, acid_p_);

    bellows::Fm::Params lead;
    bellows::Kick::Params kp;
    bellows::Snare::Params sp;
    bellows::Hat::Params hp;
    if (g == G_AMBIENT) {
      /* Orbital: a soft glassy bell lead */
      lead.ops = 2; lead.algorithm = 1; lead.feedback = 0.0f; lead.brightness = 0.45f;
      lead.ratio[0] = 1; lead.ratio[1] = 3.5f; lead.level[1] = 0.5f;
      lead.attack = 0.01f; lead.decay = 1.2f; lead.sustain = 0.25f; lead.release = 0.8f;
      kp.decay = 0.25f; kp.drive = 1.2f; kp.pitch_decay = 0.06f;
      sp.decay = 0.16f; sp.tone = 0.5f; sp.snap = 0.2f;
      hp.decay = 0.035f; hp.tone = 1.2f;
    } else {
      /* the Prodigy: a hard, saw-ish FM lead with feedback */
      lead.ops = 2; lead.algorithm = 1; lead.feedback = g == G_RUSH ? 0.65f : 0.55f; lead.brightness = 0.6f;
      lead.ratio[0] = 1; lead.ratio[1] = 1; lead.level[1] = 0.7f;
      lead.attack = 0.003f; lead.decay = 0.25f; lead.sustain = 0.55f; lead.release = 0.08f;
      kp.decay = 0.32f; kp.pitch_decay = 0.06f; kp.drive = g == G_RUSH ? 3.0f : 2.4f; kp.click_tune = 7.0f;
      sp.decay = 0.2f; sp.tone = 0.45f; sp.snap = 0.28f;
      hp.decay = 0.04f; hp.tone = 1.25f;
    }
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
      /* the front end */
      case SFX_MENU_MOVE: Push(0, I_HAT, 900, 0.25f); Push(0, I_PLUCK, Mtof(84), 0.35f); break;
      case SFX_MENU_OK: Push(0, I_PLUCK, Mtof(72), 0.6f); Push(50, I_PLUCK, Mtof(79), 0.6f); Push(100, I_BELL, Mtof(88), 0.5f); break;
      case SFX_MENU_BACK: Push(0, I_PLUCK, Mtof(67), 0.5f); Push(60, I_PLUCK, Mtof(62), 0.5f); break;
      case SFX_MENU_DENY: Push(0, I_BUZZ, Mtof(45), 0.5f); Push(90, I_BUZZ_OFF, 0, 0); break;
      case SFX_CARD: Push(0, I_OHAT, 330, 0.3f); Push(0, I_PLUCK, Mtof(76), 0.4f); break;
      case SFX_READY: Push(0, I_BELL, Mtof(79), 0.5f); Push(80, I_BELL, Mtof(86), 0.5f); break;
      case SFX_COUNTDOWN: Push(0, I_PLUCK, Mtof(69), 0.7f); Push(0, I_BELL, Mtof(81), 0.4f); break;
      case SFX_GO:
        Push(0, I_BELL, Mtof(76), 0.6f); Push(70, I_BELL, Mtof(80), 0.6f); Push(140, I_BELL, Mtof(83), 0.6f);
        Push(210, I_BELL, Mtof(88), 0.7f); Push(210, I_OHAT, 330, 0.5f); break;
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
    if (mode_ == 0) { acid_.NoteOff(); lead_.NoteOff(); counter_.NoteOff(); PadRelease(); return; }
    float bpm = mode_ == 3 ? 160.0f : mode_ == 2 ? 136.0f : 120.0f;
    samples_per_step_ = static_cast<int>(sr_ * 60.0f / (bpm * 4.0f) + 0.5f);
    SetGenre(mode_ == 3 ? G_RUSH : mode_ == 2 ? G_ACID : G_AMBIENT);
    /* echoes in time: a dotted eighth left, an eighth right */
    bellows::StereoDelay<500>::Params d;
    float beat = 60.0f / bpm;
    d.time_l = beat * 0.75f; d.time_r = beat * 0.5f; d.feedback = mode_ == 1 ? 0.48f : 0.36f;
    d.cross_feedback = 0.15f; d.damping = 3200.0f; d.mix = 1.0f;
    delay_.SetParams(d);
    last_chord_ = -1;
  }

  const Section& Current(int* count, int* loop) const {
    if (mode_ == 3) { *count = 1; *loop = 0; return closing_[section_]; }
    if (mode_ == 2) { *count = sizeof NIGHT / sizeof NIGHT[0]; *loop = kNightLoop; return night_[section_]; }
    *count = sizeof LOBBY / sizeof LOBBY[0]; *loop = kLobbyLoop; return lobby_[section_];
  }

  void PadRelease() { for (int i = 0; i < npad_; i++) pad_.NoteOff(pad_ids_[i]); npad_ = 0; }

  /* smooth voice leading: each chord tone goes to the octave nearest the last voicing */
  void PadChord(int root, bool minor, float vel) {
    int tones[3] = {root, (root + (minor ? 3 : 4)) % 12, (root + 7) % 12};
    PadRelease();
    float center = pad_center_, sum = 0;
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
    const bool last_bar = bar_ == sec->def->bars - 1;
    const bool intro = mode_ == 2 && section_ == 0;
    const int root = sec->roots[bar_ % 8];
    const bool minor = sec->minor[bar_ % 8];
    const float jit = 0.06f * rng_sfx_.Next();
    const float soft = mode_ == 1 ? 0.6f : 1.0f;

    /* drums */
    bool rolling = (lay & L_ROLL) && bar_ >= sec->def->bars - (sec->def->tag[0] == 'B' && sec->def->tag[1] == 'R' ? 2 : 1);
    if (rolling) {
      /* a snare roll into the drop: eighths, then sixteenths, getting louder */
      int pos = (bar_ - (sec->def->bars - 2)) * 16 + s;
      float up = (bar_ == sec->def->bars - 1 ? 0.5f : 0.25f) + 0.5f * s / 16.0f;
      if (bar_ == sec->def->bars - 1 || s % 2 == 0) snare_.NoteOn(190.0f, up);
      (void)pos;
    }
    if ((lay & L_KICK) && s % 4 == 0 && !(rolling && last_bar && s >= 12)) kick_.NoteOn(50.0f, 0.95f * soft);
    if (lay & L_BREAK) {
      if (BRK_K[s] > 0) kick_.NoteOn(50.0f, BRK_K[s]);
      if (BRK_S[s] > 0) snare_.NoteOn(190.0f, BRK_S[s] + jit);
      if (genre_ == G_RUSH && (s == 6 || s == 14)) kick_.NoteOn(50.0f, 0.7f);
      if (last_bar && s >= 12) snare_.NoteOn(190.0f, 0.35f + 0.1f * (s - 12));   /* fill */
    }
    if (lay & L_HAT) {
      if (intro || mode_ == 1) {
        if (s % 4 == 2) ohat_.NoteOn(330.0f, (intro ? 0.5f : 0.3f) + jit);     /* the offbeat open hat */
        if (mode_ == 1 && s % 2 == 0) hat_.NoteOn(330.0f, 0.18f);
      } else {
        if (s % 4 == 2) ohat_.NoteOn(330.0f, 0.4f + jit);
        else hat_.NoteOn(330.0f, (s % 2 ? 0.28f : 0.4f) + jit);
      }
    }
    if ((lay & L_PERC) && (s == 4 || s == 12)) {
      /* a clap is three snares 10 ms apart */
      Push(0, I_CLAP, 0, 0.55f); Push(10, I_CLAP, 0, 0.45f); Push(21, I_CLAP, 0, 0.35f);
    }

    /* the acid line, its filter sweeping like somebody is riding the knob */
    if ((lay & L_ACID) || (lay & L_SUB)) {
      float cut;
      if (lay & L_SUB) cut = 260.0f;
      else if (intro) cut = 220.0f + 1700.0f * (bar_ * 16 + s) / 128.0f;          /* the intro opens up */
      else cut = 450.0f + 1500.0f * (0.5f + 0.5f * sinf(step_ * 6.2831853f / 128.0f));
      acid_p_.cutoff = cut;
      acid_p_.env_amount = (lay & L_SUB) ? 0.4f : (ACID_A[s] ? 3.2f : 2.0f);
      acid_.SetParams(acid_p_);
      int base = 36 + root;                     /* E2 = 40 */
      if (base > 47) base -= 12;
      if (lay & L_SUB) {
        if (s == 0) acid_.NoteOn(Mtof(static_cast<float>(base)), 0.6f);
        if (s == 12) acid_.NoteOff();
      } else if (ACID_N[s] >= 0) {
        acid_.NoteOn(Mtof(static_cast<float>(base + ACID_N[s])), ACID_A[s] ? 0.95f : 0.7f);
        acid_gate_ = (s < 15 && ACID_N[s + 1] < 0) ? 2 : 1;
      }
    }
    if (acid_gate_ > 0 && --acid_gate_ == 0 && !(lay & L_SUB)) acid_.NoteOff();

    /* pad: one held chord per bar, voice-led */
    int chord_id = bar_ * 100 + section_;
    if ((lay & L_PAD) && s == 0 && chord_id != last_chord_) { PadChord(root, minor, 0.62f); last_chord_ = chord_id; }
    if (!(lay & L_PAD) && npad_) PadRelease();

    /* the hypnotic arpeggio: three notes against four in the night,
     * a slow rising four in the lobby */
    if (lay & L_HYPNO) {
      int third = root + (minor ? 3 : 4), fifth = root + 7;
      if (mode_ == 1) {
        if (s % 2 == 0) {
          int fig[4] = {root, third, fifth, root + 12};
          int m = 72 + ((fig[(s / 2) % 4] - 0 + 12) % 12) - 12 + (s / 2 % 4 == 3 ? 12 : 0);
          hypno_.NoteOn(note_id_++ & 0xfff, Mtof(static_cast<float>(m)), 0.45f + jit, frame_);
        }
      } else {
        int fig[3] = {root + 12, fifth, third + 12};
        int m = 64 + ((fig[step_ % 3] - 4 + 24) % 24);
        float acc = s % 4 == 0 ? 0.6f : 0.42f;
        hypno_.NoteOn(note_id_++ & 0xfff, Mtof(static_cast<float>(m)), acc + jit, frame_);
      }
    }

    /* rave stabs: a minor seventh on the downbeat and the offbeat after it */
    if ((lay & L_STAB) && (s == 0 || s == 6 || (bar_ % 2 == 1 && s == 10))) {
      int ch[4] = {root, root + (minor ? 3 : 4), root + 7, root + (minor ? 10 : 11)};
      for (int k = 0; k < 4; k++) stab_.NoteOn(note_id_++ & 0xfff, Mtof(static_cast<float>(60 + (ch[k] + 8) % 12 + 4)), 0.55f, frame_);
    }

    /* the written lead, and in the second drops a counter line a third below */
    if (lay & L_LEAD) {
      int at = bar_ * 16 + s;
      int oct = (mode_ == 2 && pass_ > 0 && (lay & L_COUNTER)) ? 12 : 0;
      for (int k = 0; k < sec->nlead; k++) {
        const Note& nt = sec->lead[k];
        if (nt.step == at) {
          if (nt.midi) {
            lead_.NoteOn(Mtof(static_cast<float>(nt.midi + oct)), mode_ == 1 ? 0.5f : 0.7f);
            if (lay & L_COUNTER) counter_.NoteOn(Mtof(static_cast<float>(DiatonicBelow(nt.midi) + oct - 12)), 0.5f);
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

  /* stem gains are set from measured levels: Bellows engines differ by 10x in
   * native loudness, so these are not the reference rack's numbers */
  void RenderSpan(float* l, float* r, int from, int n) {
    for (int i = 0; i < n; ++i) { rl_[i] = rr_[i] = yl_[i] = yr_[i] = 0.0f; }
    const float g = music_gain_, m = (mode_ <= 0 ? 0.0f : 1.0f) * g;
    Mix(kick_, 0.9f * g, 0.0f, 0.0f, 0.0f, l, r, from, n);
    Mix(snare_, 2.2f * g, 0.7f * g, 0.0f, 0.0f, l, r, from, n);
    Mix(clap_, 1.0f * m, 0.9f * g, 0.0f, -0.16f, l, r, from, n);
    Mix(hat_, 8.0f * g, 0.0f, 0.0f, 0.18f, l, r, from, n);
    Mix(ohat_, 3.2f * g, 0.4f * g, 0.0f, -0.12f, l, r, from, n);
    /* the acid line through its saturator, with a little echo */
    for (int i = 0; i < n; ++i) { bl_[i] = 0.0f; br_[i] = 0.0f; }
    acid_.Process(bl_, br_, 0, n);
    acid_sat_.Process(bl_, br_, 0, n);
    const float acid_gain = mode_ == 1 ? 0.3f : 1.5f;   /* the lobby's soft sub, not an acid line */
    for (int i = 0; i < n; ++i) {
      float a = bl_[i] * m, b = br_[i] * m;
      l[from + i] += acid_gain * a; r[from + i] += acid_gain * b;
      yl_[i] += 0.2f * a * (mode_ == 1 ? 0 : 1); yr_[i] += 0.2f * b * (mode_ == 1 ? 0 : 1);
    }
    Mix(pad_, 0.022f * m, 0.08f * m, 0.0f, 0.0f, l, r, from, n);
    Mix(stab_, 0.24f * m, 0.3f * m, 0.15f * m, 0.0f, l, r, from, n);
    Mix(hypno_, 0.5f * m, 0.35f * m, 0.9f * m, -0.22f, l, r, from, n);
    /* lead through its chorus */
    for (int i = 0; i < n; ++i) { bl_[i] = 0.0f; br_[i] = 0.0f; }
    lead_.Process(bl_, br_, 0, n);
    chorus_.Process(bl_, br_, 0, n);
    for (int i = 0; i < n; ++i) {
      float a = bl_[i] * m, b = br_[i] * m;
      l[from + i] += 0.36f * a; r[from + i] += 0.36f * b;
      rl_[i] += 0.35f * a; rr_[i] += 0.35f * b; yl_[i] += 0.4f * a; yr_[i] += 0.4f * b;
    }
    Mix(counter_, 0.06f * m, 0.25f * m, 0.0f, 0.24f, l, r, from, n);
    /* effects sit above the music */
    const float e = sfx_gain_;
    Mix(bells_, 0.13f * e, 0.12f * e, 0.06f * e, 0.0f, l, r, from, n);
    Mix(sfx_pluck_, 1.1f * e, 0.5f * e, 0.0f, 0.0f, l, r, from, n);
    Mix(buzz_, 0.22f * e, 0.0f, 0.0f, 0.0f, l, r, from, n);
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

  bellows::Rng rng_snare_, rng_clap_, rng_acid_, rng_str_, rng_sfx_, rng_buzz_, rng_ctr_;
  bellows::Kick kick_;
  bellows::Snare snare_, clap_;
  bellows::Hat hat_, ohat_;
  bellows::Va acid_, buzz_, counter_;
  bellows::Va::Params acid_p_;
  bellows::VoicePool<bellows::Fm, 6> pad_;
  bellows::VoicePool<bellows::Fm, 4> stab_;
  bellows::VoicePool<String, 6> hypno_;
  bellows::VoicePool<String, 4> sfx_pluck_;
  bellows::VoicePool<Bell, 6> bells_;
  Bell lead_;
  bellows::Plate<> plate_;
  bellows::StereoDelay<500> delay_;
  bellows::Chorus<> chorus_;
  bellows::Saturator<2> acid_sat_;
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
  Genre genre_ = G_AMBIENT;
  float music_gain_ = 1.0f, sfx_gain_ = 1.0f;   /* the MUSIC and SFX settings, 0..1 */
  int mode_ = -1, pending_mode_ = -1, samples_per_step_ = 1, countdown_ = 0;
  int step_ = 0, bar_ = 0, section_ = 0, pass_ = 0, acid_gate_ = 0, lead_off_ = 0, last_chord_ = -1;
  int note_id_ = 0;
  uint32_t frame_ = 0;
  bool ready_ = false;
};

Band band;
int pending_mode = 1;
float music_level = 1.0f, sfx_level = 1.0f;
int pending_sfx[16];
int npending;
float L[4096], R[4096];
double frac;

}  // namespace

extern "C" void audio_play(int sfx) {
  if (band.Ready()) band.Sfx(sfx);
  else if (npending < 16) pending_sfx[npending++] = sfx;
}

extern "C" void audio_set_levels(float music, float sfx) {
  music_level = music; sfx_level = sfx;
  band.SetLevels(music, sfx);
}

extern "C" void audio_music(int mode) {
  pending_mode = mode;
  if (band.Ready()) band.SetMode(mode);
}

extern "C" void audio_render(float* ring, uint32_t cap, uint32_t* cursor, uint32_t rate, double dt_ms) {
  if (!band.Ready()) {
    band.Init(static_cast<float>(rate ? rate : 48000));
    band.SetMode(pending_mode);
    band.SetLevels(music_level, sfx_level);
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
