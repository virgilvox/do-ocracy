# Audio map: src/audio_bellows.cpp and src/audio.h

Source of truth: `/Users/obsidian/Projects/heatsync/games/do-ocracy/src/audio_bellows.cpp` (691 lines), `/Users/obsidian/Projects/heatsync/games/do-ocracy/src/audio.h` (16 lines), engine headers under `/Users/obsidian/Projects/heatsync/games/do-ocracy/third_party/bellows/bellows/`. Line numbers below refer to those files as they stand today.

## 1. The public C API (audio.h)

```
audio.h:5-8    enum { SFX_PICKUP, SFX_DROP, SFX_TICK, SFX_GLAMOUR, SFX_KARMA, SFX_LASER, SFX_BAD,
                      SFX_JOIN, SFX_ALARM, SFX_WIN, SFX_LOSE, SFX_TALK, SFX_SLURP, SFX_COUNT };
audio.h:10     void audio_play(int sfx);
audio.h:12     void audio_music(int mode);      0 off, 1 lobby, 2 night, 3 closing time
audio.h:14     void audio_render(float *ring, uint32_t cap, uint32_t *cursor, uint32_t rate, double dt_ms);
```

Three functions, that is the whole contract between the C game and the C++ band. The game never includes a Bellows header. `audio_bellows.cpp:37-39` wraps the include in `extern "C"`.

Call sites in the game: `src/main.c:134,356,362` (audio_music 1 on title or lobby), `src/main.c:409-410` (audio_render every frame, after the two three.c renders), `src/game.c` (about 30 `audio_play` calls and `audio_music(1|2|3)` at lines 242, 966, 986, 1071). The ring is `static float audio_ring[AUDIO_CAP * 2]` with `AUDIO_CAP 8192` interleaved stereo f32 (`main.c:17-21`), advertised to the host through `info.audio_ptr`, `info.audio_cap`, `info.audio_write_ptr` and `WC_FLAG_AUDIO_F32` (`main.c:42-49`).

## 2. Build

`build.sh:62-68`: the one C++ translation unit is compiled separately with `em++ -std=c++17 -fno-exceptions -fno-rtti -I third_party/bellows -I src`, then its `.o` is linked into the cart with the C sources. Bellows is header only (`third_party/bellows/bellows/...`, vendored from github.com/virgilvox/bellowsjs at 613cd73, see `third_party/bellows/SOURCE.txt`). Nothing in the audio file touches libc++; only `<math.h>` and `<stdint.h>`. Keep it that way in any copy: the cart may import only gl, wc_* and WASI (`build.sh:76`).

Compile-time knobs (`bellows/config.h:29-39`): `BELLOWS_FAST_MATH` (0 default, 1 swaps libm for polynomials, cheaper), `BELLOWS_SAMPLE_RATE` (48000, sizes delay and pluck buffers only; runtime `Init(sr)` takes the real rate), `BELLOWS_BLOCK_SIZE` (128, used as `kBlock` at `audio_bellows.cpp:43`).

## 3. Song data format

### 3.1 Notes (lead lines)

`audio_bellows.cpp:46-48` comment, parser at `117-149` (`ParseSection`).

- Token: `NAME OCTAVE : LENGTH`, e.g. `E5:2`, `F#5:4`, `Bb4:8`. Length in sixteenths. Missing `:LENGTH` means 4 (one beat), `ParseSection:144`.
- `-:4` is a rest of 4 sixteenths (midi 0 = rest, `Note.midi` at `103`).
- `|` ends a bar: the step counter rounds up to the next multiple of 16 (`134`). Tokens before the bar line need not fill the bar; tokens that overflow push into the next bar before the round-up, so keep each bar summing to 16 or less.
- Octave: `midi = 12 * (oct + 1) + pc` (`142`), so `E5` is midi 76, `C4` is 60.
- Limits: 96 notes per section lead (`Note lead[96]` at `108`, guard at `132`). The lead of an 8-bar section with sixteenth notes would exceed this; current riffs use 2-step notes at most 8 per bar, so 64.
- Three written lines: `RIFF` (`66-70`, 8 bars), `DREAM` (`72-73`, 8 bars), `HALCYON` (`75-76`, 8 bars).

### 3.2 Chords

A section's `chords` string is 8 space-separated chord names, one per bar: root letter, optional `#` or `b`, optional `m` for minor (`119-127`). Only root and minor flag are kept (`roots[8]`, `minor[8]` at `106-107`); sevenths etc. are ignored. `bar_ % 8` indexes them (`419-420`), so a section longer than 8 bars repeats its chords.

### 3.3 Sections and forms

```
audio_bellows.cpp:78   struct SectionDef { const char* tag; int bars; const char* chords; uint16_t layers; const char* lead; };
```

- `tag` is for humans, except that `Step:425` checks `tag[0]=='B' && tag[1]=='R'` (BREAK) to start the snare roll two bars before the end instead of one. Any new section whose tag starts with "BR" gets the 2-bar roll.
- `bars` is the section length. Chord and lead data are 8 bars; `bars` should be 8 unless you also change `roots[8]`, the `% 8` at 419-420, and the lead length.
- `layers` is a bitmask of `Layer` (`50-63`).
- `lead` is a note string or `nullptr`.

Forms (arrays of SectionDef) and their loop point:

| Form | Lines | Mode | BPM | Sections | Loop back to |
|---|---|---|---|---|---|
| `LOBBY` "Halcyon Tuesday" | 92-96 | 1 | 120 | A, B | 0 (`kLobbyLoop`) |
| `NIGHT` "Hack the Planet" | 81-90 | 2 | 136 | INTRO, BUILD, DROP, DROP, BREAK, DROP+COUNTER, DROP+COUNTER | 1 (`kNightLoop`, skips INTRO on repeat) |
| `CLOSING` "Mess the Planet" | 98-100 | 3 | 160 | RUSH | 0 (hardcoded in `Current:374`) |

BPM per mode is hardcoded in `ApplyMode:361`. Genre (patch set) per mode in `ApplyMode:363`: mode 3 is `G_RUSH`, 2 is `G_ACID`, else `G_AMBIENT` (`enum Genre` at `176`).

Parsed forms live in `night_[]`, `lobby_[]`, `closing_[]` (`628-630`), filled in `Band::Init:192-194`. `Current()` (`373-377`) picks the array, count and loop by `mode_`.

### 3.4 Layers (the conductor's instrument switches)

`enum Layer : uint16_t` at `50-63`. What each bit does in `Step()` (`404-521`):

| Bit | Name | Effect in Step | Lines |
|---|---|---|---|
| 1 | L_ACID | 303 line from `ACID_N`/`ACID_A` 16-step tables, root-relative, cutoff sweeps (intro ramps, else sine over 128 steps) | 455-473, tables 166-167 |
| 2 | L_KICK | four on the floor (`s % 4 == 0`), muted in the last beat of a roll | 433 |
| 4 | L_BREAK | breakbeat kick and snare from `BRK_K`/`BRK_S` velocity tables, extra kicks in G_RUSH, a fill in the last bar | 434-439, tables 169-170 |
| 8 | L_HAT | offbeat open hat; lobby and intro are sparser; full sixteenths otherwise | 440-448 |
| 16 | L_PERC | a clap on steps 4 and 12, built as three clap hits 10 ms apart via `Push` | 449-452 |
| 32 | L_PAD | one voice-led triad per bar on the FM pad pool | 475-478, `PadChord` 382-402 |
| 64 | L_HYPNO | the plucked arpeggio: 3-against-4 in night, slow rising four in lobby | 480-496 |
| 128 | L_LEAD | the written lead line, `lead_` Fm; counter line if L_COUNTER | 504-519 |
| 256 | L_COUNTER | counter line a diatonic third below (`DiatonicBelow` 152-162, E minor only) on `counter_` Va; also octaves the lead up on pass > 0 | 507, 513 |
| 512 | L_STAB | rave minor-seventh stabs on steps 0, 6 and (odd bars) 10 | 498-502 |
| 1024 | L_ROLL | snare roll into the next section (last bar, or last two for BR* tags) | 425-432 |
| 2048 | L_SUB | the acid Va plays a soft held sub instead of the acid line (lobby) | 455-467 |

`DROP` (`80`) is a convenience union for the full kit. 12 of 16 bits used; 4 free bits remain in `uint16_t`.

The 16-step pattern tables (`164-170`) are shared by every section; they are not per-section data. To get a different groove per song, add a second table set and select it by `genre_`.

## 4. The band: engines and voices

Members at `609-626`:

| Member | Type | Role | Patch set at |
|---|---|---|---|
| `kick_` | `bellows::Kick` | kick, both music and SFX `I_KICK` | `SetGenre` 275, 283 |
| `snare_` | `bellows::Snare` | snare, rolls, fills, `I_SNARE` | 276, 284 |
| `clap_` | `bellows::Snare` | clap (snappy snare, `tone .2 snap .9`) | 198-199 |
| `hat_`, `ohat_` | `bellows::Hat` | closed and open hat | 277, 285, 196-197 |
| `acid_` | `bellows::Va` (saw, ladder, filter env) | acid line or lobby sub; params kept in `acid_p_` and re-sent every step | 257-264, 455-462 |
| `buzz_` | `bellows::Va` (square, detuned 18 cents) | SFX buzzer (`I_BUZZ`) | 229-232 |
| `counter_` | `bellows::Va` (sine shape 3) | counter line | 219-220 |
| `pad_` | `VoicePool<Fm, 6>` 2-op FM | chord pad | 201-207 |
| `stab_` | `VoicePool<Fm, 4>` 2-op FM, feedback .45 | rave stabs | 209-214 |
| `hypno_` | `VoicePool<Pluck<70>, 6>` | arpeggio | 216-217 |
| `sfx_pluck_` | `VoicePool<Pluck<70>, 4>` | SFX strings (`I_PLUCK`) | 222-223 |
| `bells_` | `VoicePool<Fm, 6>` 4-op algorithm 3 | SFX bells (`I_BELL`) | 224-228 |
| `lead_` | `bellows::Fm` 2-op, mono | lead | 266-287 |
| `plate_` | `Plate<>` | the shared reverb bus | 234-235 |
| `delay_` | `StereoDelay<500>` | the shared tempo delay bus, retimed per mode | 236-238, 365-369 |
| `chorus_` | `Chorus<>` | insert on the lead only | 239-240 |
| `acid_sat_` | `Saturator<2>` | insert on the acid line only | 241-242 |
| `comp_`, `limiter_` | `Compressor<>`, `Limiter<>` | master bus | 243-246 |

Seven `bellows::Rng` instances (`609`) seeded with string labels (`184-190`); `Rng::Init(const char*)` hashes the label (`core/prng.h:86`), so the sound is deterministic per label. Every noise-using engine (Snare, Pluck, Va, Modal) needs its own Rng pointer passed to `Init`.

Engine API shape (all engines): `Init(sr[, rng], Params)`, `SetParams(Params)`, `NoteOn(freq_hz, vel)`, `NoteOff()`, `Process(l, r, from, to)` which adds into the buffers, `Active()`. Frequencies are Hz, so `Mtof` (`44`) converts midi. `VoicePool<V, N>` (`voicepool.h`) adds a `note_id` and a `frame` argument to `NoteOn` and steals free, then oldest released, then oldest held; `NoteOff(note_id)` releases by id. Pool voices are addressed by `at(i)` for `Init`.

Params that matter for new patches:

- `Va::Params` (`engines/va.h:16-30`): `shape` 0 saw, 1 square, 2 triangle, 3 sine (`BlepShape`, `dsp/oscillators.h:11`); `detune` cents between the two oscillators; `sub` square sub level; `cutoff`, `resonance`, `filter_type` (0 ladder, 1 SVF); `env_amount` octaves of filter envelope; amp ADSR `attack decay sustain release`; filter ADSR `f_attack f_decay f_sustain f_release`; `drift`; `pan`; `vel_level`, `vel_filter`.
- `Fm::Params` (`engines/fm.h:105-115`): `ops` 2, 4 or 6; `algorithm` one-based (2 ops: 1 serial, 2 parallel; 4 ops: TX81Z 1..8); `feedback`; `brightness` (velocity to mod depth); carrier ADSR and modulator `m_*` ADSR; `ratio[6]`, `level[6]`, `fixed_hz[6]`.
- `Kick::Params` (`drums.h:21-26`): `click_tune`, `pitch_decay`, `decay`, `drive`. `NoteOn(freq, vel)`: freq is the body pitch (50 Hz used).
- `Snare::Params` (`drums.h:78-82`): `tone` (0 body, 1 noise), `decay`, `snap`. `NoteOn(freq, vel)`: freq is the body pitch (190 Hz used).
- `Hat::Params` (`drums.h:149-152`): `decay`, `tone`. `NoteOn(freq, vel)`: six square oscillators at `freq * kRatios` (330 Hz used).
- `Pluck<kMinFreqHz>::Params` (`pluck.h:18-24`): `damp`, `pick_pos`, `excite_type`, `decay`, `level`. The template arg sizes the delay line: `Pluck<70>` is used here to keep each voice small (Pluck<20> is 16 KB per voice).
- Not used here but in the vendored tree and worth it for a combat game: `bellows::Modal` (`engines/modal.h:107`, `Params` at `60-66`: `material` 0 bar, 1 membrane, 2 bell, 3 glass, 4 wood; `decay`, `brightness`, `strike_hardness`, `level`; `Init(sr, Rng*, Params)`, `NoteOn(freq, vel)`), `bellows::NoiseGen` (`dsp/noise.h:12`, white, pink, brown, velvet, crackle), `bellows::WestCoast` (`engines/westcoast.h`, folded triangle, good for sirens and lasers), `bellows::Formant` (`engines/formant.h`, vowels, good for a crowd), `bellows::Flanger`, `Tremolo`, `AutoPan`, `RingMod` in `fx/modfx.h`, `bellows::Gate` in `fx/dynamics.h`. Preset tables in `presets/instruments.h` (`kModalPresetParams` at 187: marimba, vibraphone, glockenspiel, tubular bells, kalimba, steel drum, woodblock, timpani).

## 5. How SFX are defined and triggered

Flow: `audio_play(sfx)` (`657-660`) calls `band.Sfx(sfx)` if the band is initialised, else queues up to 16 in `pending_sfx` (`650-651`) and replays them at first `audio_render` (`668-673`).

`Band::Sfx(int)` (`305-339`) is a switch over the `SFX_*` enum. Each case is a list of `Push(delay_ms, inst, freq_hz, vel)` calls (`300-303`). `Push` converts ms to samples and appends to `ev_[96]` (`634`); events over 96 are dropped silently (`301`). Events are a tiny per-SFX sequencer: a jingle is a few pushes with increasing delays.

Instruments an event may name are `enum Inst` (`177`): `I_BELL` (bells_ pool), `I_PLUCK` (sfx_pluck_ pool), `I_BUZZ` and `I_BUZZ_OFF` (the mono buzz_ Va; a buzz needs an explicit OFF event or it sustains forever since `sustain = 0.7`), `I_KICK`, `I_SNARE` (freq 0 means 190 Hz), `I_HAT`, `I_OHAT`, `I_CLAP` (freq ignored, fixed 1400 Hz). Dispatch is in `Fire(span)` (`523-545`), which fires every due event at the start of each span and decrements the rest by `span` samples. Timing resolution is therefore one span (at most 128 samples, about 2.7 ms at 48 kHz).

Note that the drums are shared with the music: an SFX kick (`SFX_WIN:328`) retriggers the same `kick_` the beat uses. That is fine for one-shots but a dense SFX layer in a combat game should get its own drum instances.

SFX mix levels: `RenderSpan:594-596` (bells 0.13 dry, plucks 1.1 dry, buzz 0.22 dry). These sit above the music and are not scaled by `m` (the music mute), so SFX play in mode 0.

The current table of 13 effects (`308-337`) and what the game uses them for: PICKUP (item grabbed), DROP (item put down or wasted), TICK (UI, station work), GLAMOUR (clout, rising bells), KARMA (quiet strings), LASER (random high bell plus hat), BAD (low buzz), JOIN (three plucks, a player joined), ALARM (four alternating buzzes), WIN (C D Em bell jingle plus kick, open hat, low pluck), LOSE (descending buzz), TALK (four random bells, a proposal), SLURP (snare plus two plucks).

## 6. The render loop and CPU

Entry: `audio_render(ring, cap, cursor, rate, dt_ms)` (`667-691`).

1. Lazy init on first call: `band.Init(rate)`, apply `pending_mode` (default 1, `649`), replay queued SFX (`668-673`). All engine init happens here, on the first frame, with the host's real sample rate.
2. Sample budget: `want = rate * dt_ms / 1000 + frac` with the fraction carried across frames (`653, 674-676`), clamped to half the ring (`677`), so a long stall produces at most `cap / 2` = 4096 samples.
3. Renders in chunks of up to 4096 into the static `L[4096], R[4096]` (`652, 678-681`), then interleaves into the ring and advances `*cursor` modulo `cap` (`682-688`). The host reads behind the cursor; the cart never checks the read position.

`Band::Render(l, r, n)` (`341-353`): walks the request in spans. Each span is cut at the next sequencer step boundary (`countdown_`) and at `kBlock` (128). For each span: `Step()` when a step boundary arrives (only when `mode_ > 0`), `Fire(span)` for due SFX events, `RenderSpan`.

`RenderSpan(l, r, from, n)` (`562-607`): clears the reverb and delay send buffers (`rl_ rr_ yl_ yr_`), then for each part calls `Mix(part, dry, rev, dly, pan, ...)` (`547-558`), which renders the part into scratch `dl_ dr_`, pans with a constant-gain-per-side law, and accumulates into the output and the two sends. The acid line and the lead bypass `Mix` for their inserts (saturator `571-579`, chorus `584-591`). Then plate and delay process their send buffers in place (`597-598`) and are summed back at 0.7 and 0.6 (`599-602`), then compressor, limiter and a 0.92 trim (`603-605`). `frame_` advances by `n` (`606`) and is the clock the VoicePool uses for steal order.

`m` (`564`) mutes every music part when `mode_ <= 0` but still processes them, so mode 0 costs the same CPU as music.

Timing: `samples_per_step_ = sr * 60 / (bpm * 4)` (`362`), one step per sixteenth. Mode changes are queued in `pending_mode_` and applied at the next bar (`step_ % 16 == 0`, `405`), except from or into silence which is immediate (`297`). `ApplyMode` resets section, bar, step and pass to 0 (`359`) and retimes the delay to the new tempo (`365-369`).

CPU notes (from HANDOFF.md:87 and the source):

- Measured about 1 ms per frame on Apple Silicon for the whole band; a Pi is several times slower.
- Expensive parts in order: the 6-voice FM pad (`201-207`, the comment at 201 says a detuned VA pad was worse), the acid Va with saturator (oversampled x2, `241`), the two 6-voice pluck pools, the plate reverb. Cheap: drums, hats, the 2-op lead, the delay.
- Every part is processed every span even when silent, because `VoicePool::Process` skips inactive voices but mono engines (`Va`, `Fm`, drums) are processed unconditionally; their `Process` loops are cheap when the envelope is at zero but not free. A combat cart on a Pi should guard with `if (part.Active())` in `Mix`.
- The acid `SetParams` is called every step (`462`), which recomputes filter coefficients; harmless at 16 steps per bar.
- Reductions if a target struggles: pad pool 6 to 2 or 3 (`615`, `207`), `Saturator<2>` to `Saturator<1>` (`624`), `BELLOWS_FAST_MATH=1` in `build.sh:64`, drop `plate_` in favour of a longer delay.
- Memory: `Plate<>` holds its whole tank as a member array (`fx/plate.h:398`), `StereoDelay<500>` two 24k-sample rings, each `Pluck<70>` a delay line sized for 70 Hz; `Band band` is a static global (`648`), so it all lives in the wasm data segment. The cart links with 64 MB initial memory (`build.sh:73`), so this is fine.

## 7. Recipes

### 7a. Add new SFX (menu move, menu confirm, fanfare, crowd, robot combat impacts)

Two files always change: `audio.h` enum and `Band::Sfx` switch. Keep `SFX_COUNT` last.

1. `audio.h:6-7`: append before `SFX_COUNT`, e.g. `SFX_MENU_MOVE, SFX_MENU_OK, SFX_FANFARE, SFX_CROWD, SFX_HIT_LIGHT, SFX_HIT_HEAVY, SFX_CLANG, SFX_SAW, SFX_FLIP, SFX_KO`.
2. `audio_bellows.cpp:305-339`: add cases. Patterns that work with the existing instruments, no new engines:
   - menu move: `Push(0, I_HAT, 900, 0.25f); Push(0, I_PLUCK, Mtof(84), 0.35f);` short and high, under the music.
   - menu confirm: `Push(0, I_PLUCK, Mtof(72), 0.6f); Push(50, I_PLUCK, Mtof(79), 0.6f); Push(100, I_BELL, Mtof(88), 0.5f);`
   - menu back or cancel: `Push(0, I_PLUCK, Mtof(67), 0.5f); Push(60, I_PLUCK, Mtof(62), 0.5f);`
   - fanfare: copy the `SFX_WIN` shape (`325-330`): a chord sequence on `I_BELL` at 70 ms arpeggio spacing, 260 ms per chord, a final kick plus open hat plus low pluck. Change the chord table to the new key. Each chord is 3 pushes; the 96-event cap (`301`) allows a long fanfare but check the count.
   - heavy impact with existing kit: `Push(0, I_KICK, 38, 1.0f); Push(0, I_SNARE, 120, 0.9f); Push(0, I_BUZZ, Mtof(31), 0.9f); Push(90, I_BUZZ_OFF, 0, 0);` The buzz Va at a very low note with its 2400 Hz cutoff reads as a thud. Always pair `I_BUZZ` with `I_BUZZ_OFF`.
   - light impact: `Push(0, I_SNARE, 260, 0.5f); Push(0, I_HAT, 1200, 0.3f);`
3. For better combat sounds, add engines. Each new engine needs: a member (`609-626`), an `Rng` if it uses noise (`609`, seed label at `184-190`), `Init` in `Band::Init` (`182-251`), an `Inst` value (`177`), a `Fire` case (`528-538`), and a `Mix` line in `RenderSpan` (`593-596`). Suggested additions:
   - `bellows::VoicePool<bellows::Modal, 4> metal_;` material 2 (bell) or 0 (bar), `decay 0.6`, `strike_hardness 0.9`, for clangs: `NoteOn(Mtof(60 + r * 12), vel)` with a random pitch so repeats do not sound like a sample.
   - `bellows::Modal wood_;` material 4 for armor scrape and body hits.
   - A dedicated `bellows::Kick impact_kick_;` with `decay 0.5, drive 4.0, click_tune 9` for the heavy hit, separate from the music kick so the beat is not interrupted (`433-437` share `kick_` today).
   - A noise burst: `bellows::Va spark_;` shape 1 square, `detune 40`, `cutoff 6000`, `resonance 0.6`, `env_amount 3`, `f_decay 0.08`, `decay 0.12`, `sustain 0` for sparks and sawblade contact; or `bellows::NoiseGen` plus an envelope for a cheaper white noise hit (NoiseGen has no envelope of its own; wrap it with `bellows::Adsr` from `dsp/envelopes.h`).
   - Crowd: a crowd is a sustained bed, not a one-shot, so give it a level rather than a trigger. Add `bellows::NoiseGen crowd_;` in `kPink` or `kBrown` through a `bellows::Svf` bandpass around 800 Hz plus a slow `Tremolo` (`fx/modfx.h:263`) at 0.3 Hz, and a `float crowd_target_` that the game sets through a new C function `audio_crowd(float level)`. Ramp `crowd_gain_` toward the target per span in `RenderSpan`. For cheers, add `SFX_CROWD_CHEER` that bumps `crowd_gain_` to 1.0 for a second and fires 6 to 8 random `I_BELL`-like formant blips; `bellows::Formant` (`engines/formant.h`) with an "ah" vowel and random pitch 55 to 67 does a convincing cheer at low level.
4. Set levels by measurement, not by ear from the code: render a WAV with `WAV=out.wav node tools/shot.mjs build/<cart>.wasc ...` (`tools/shot.mjs:43-76`) and compare RMS per stem. Bellows engines differ about 10x in native loudness (`560-561`).
5. Trigger from C with `audio_play(SFX_HIT_HEAVY)`. In a split-screen cart, two players hitting on the same frame produce two events; the pools steal voices so this is fine, but the mono instruments (`buzz_`, `impact_kick_`) retrigger. Give any instrument that both players can fire a `VoicePool` or two instances.

### 7b. Add a new song or mode

A mode is: a form array, a BPM, a genre, a loop index and a number in the public API.

1. `audio.h:11` comment and the game: pick the next integer (4).
2. Write the data near `81-100`: a lead string in the note format (3.1), a `const SectionDef MYSONG[] = { {"TAG", 8, "Am Am F G Am Am F G", L_..., LEAD}, ... };` and a `constexpr int kMySongLoop`.
3. Add `Section mysong_[sizeof MYSONG / sizeof MYSONG[0]];` (`628-630`) and a `ParseSection` loop in `Band::Init` (`192-194`).
4. `Current()` (`373-377`): add `if (mode_ == 4) { *count = ...; *loop = kMySongLoop; return mysong_[section_]; }`.
5. `ApplyMode()` (`361, 363`): add the BPM and genre for the new mode. If the song needs a new patch set, add a `Genre` value (`176`) and a branch in `SetGenre` (`270-286`) covering lead, kick, snare, hat and the acid params.
6. Mode-specific behaviour is scattered on `mode_ == 1` and `mode_ == 2` tests: `418` (intro detection), `422` (soft drums in lobby), `441-443` (hat pattern), `484` (hypno figure), `507` (octave jump on second pass), `512` (lead velocity), `574, 578` (acid gain and send). Grep `mode_ ==` and decide each for the new mode; the defaults fall into the night behaviour.
7. Key: `DiatonicBelow` (`152-162`) is hardwired to E minor. A song in another key either skips `L_COUNTER` or gets a scale table parameter. The acid line is root-relative so it transposes with the chords (`463`), and `PadChord` is root-relative (`383`), as are hypno (`483-495`) and stabs (`500-501`).
8. Changing a mode mid-play: `audio_music(4)` queues to the next bar unless coming from silence. For an instant switch (round start), call `audio_music(0)` then `audio_music(4)` in the same frame: 0 applies at once (`297`), then 4 applies at once because `mode_ <= 0`.
9. New groove: `ACID_N`, `ACID_A`, `BRK_K`, `BRK_S` (`166-170`) are global. For a different beat per song, make them per-genre pointers selected in `SetGenre`.
10. Check with a WAV render per the HANDOFF (`HANDOFF.md:79, 88`): per-stem RMS, section loudness curve, no clipping at the limiter.

### 7c. Reuse this file in a second cart in a sibling folder

What is generic (copy as is):

- The whole Bellows tree: `third_party/bellows` (header only, MIT style license in `third_party/bellows/LICENSE`). Copy or symlink the directory; a relative include path `-I ../do-ocracy/third_party/bellows` also works but makes the cart depend on its sibling, so copy.
- `audio.h` shape: the three C functions and the pattern of a pending queue before init (`649-673`).
- `Mtof` (`44`), the note parser `ParseSection` (`117-149`), `PitchClass` (`112-115`), `SectionDef`/`Section`/`Note` (`78, 103-110`), `Layer` enum (`50-63`).
- The `Band` skeleton: `Init` order (rngs, parse, engines, fx, `SetGenre`, `SetMode`), `Push`/`Fire` event sequencer (`300-303, 523-545`), `Mix` (`547-558`), the send-bus and master chain in `RenderSpan` (`563-564, 597-607`), `Render`'s span walker (`341-353`), `ApplyMode`'s tempo and delay retime (`356-371`), `PadChord` voice leading (`382-402`).
- `audio_render` (`667-691`): ring writing, dt to sample conversion, chunking.
- The build step (`build.sh:62-68`): `em++ -c src/audio_bellows.cpp -std=c++17 -fno-exceptions -fno-rtti -I third_party/bellows -I src`, link the `.o` with the C objects. The cart must declare `WC_FLAG_AUDIO_F32` and the ring as in `main.c:17-21, 42-49`, and pump `audio_render` once per frame after drawing (`main.c:409-410`).

What is DO-OCRACY specific (replace):

- The header comment (`1-21`): soundtrack concept.
- Song data: `RIFF`, `DREAM`, `HALCYON` (`66-76`), `NIGHT`, `LOBBY`, `CLOSING` forms (`81-100`), loop constants (`90, 96`), the pattern tables (`166-170`).
- The `SFX_*` enum in `audio.h:5-8` and every case in `Sfx` (`305-339`); the comments on them are game lore (clout, karma, proposals).
- Rng seed labels `"do-ocracy::..."` (`184-190`): rename to the new cart so the two games do not share noise sequences by accident (cosmetic, but clean).
- `DiatonicBelow`'s E minor scale (`153`).
- Mode numbering and the per-mode branches (`293-298, 361-363, 373-377`, and the `mode_ ==` tests listed in 7b step 6).
- The `BR` tag check (`425`) if the new song has no breakdown.
- Stem gains (`565-596`) were measured for these patches; new patches need new numbers.
- Patch choices in `SetGenre` (`255-291`) and `Init` (`196-246`) are the Hackers-soundtrack sound. A combat game wants heavier drums (`Kick::drive 3 to 4`, `decay 0.4`), a darker lead, and probably no plucked arpeggio.

Recommended split for the sibling cart (so both carts can share fixes later): move the generic parts into a header pair, e.g. `src/band_core.h` (ParseSection, Mtof, SectionDef, Layer, Ev, Push/Fire, Mix, span walker) and keep a per-game `audio_<game>.cpp` holding only data, `Sfx`, `SetGenre` and `RenderSpan`'s gain table. The `Band` class today is one 466-line class (`180-646`) with data and patch choices interleaved, so the honest first step is a copy; do the split only if a third cart appears.

Two carts in one repo: each cart links its own `Band band` static (`648`), so there is no symbol clash as long as they are separate wasm builds. If a shared `build.sh` is written, the `em++` line must use the new cart's `src` dir for `-I` so it picks up the right `audio.h`.

## 8. Things an implementer must not get wrong

- `SFX_COUNT` stays last in `audio.h`; the game indexes nothing by it today but `pending_sfx` is an `int` queue and `Sfx` has no range check, so an out-of-range value just does nothing.
- Every `I_BUZZ` needs a matching `I_BUZZ_OFF` or the buzz Va sustains at 0.7 forever (`231, 319, 324, 333`).
- `Push` silently drops events past 96 (`301`). A busy combat frame with several multi-event SFX can hit this; raise `ev_[96]` and the guard together, or trim the SFX.
- Bar lines in note strings round the step up to the next multiple of 16 (`134`); a bar with more than 16 sixteenths spills into the next bar before rounding.
- `lead[96]` per section (`108`); `ParseSection` stops parsing when full (`132`), truncating the melody without warning.
- Chords are read 8 per section and indexed `bar_ % 8` (`120, 419`). `bars` other than 8 needs matching data changes.
- Audio init is lazy and happens on the first `audio_render` with the host rate (`668-673`); never call engine methods from `audio_play` or `audio_music` before `Ready()`, which the wrappers already respect (`658, 664`).
- `audio_music(n)` with the same `n` is a no-op (`295`), and a change to a nonzero mode waits for the next bar (`297, 405`). To restart a song from its top, go through mode 0 first.
- Mode 0 still processes every engine (`564`), so silence does not save CPU.
- `Mix` adds into `l`/`r`; `audio_render` zeroes `L`/`R` before each chunk (`680`). Any new direct-render path (like the acid and lead inserts at `571, 584`) must zero its scratch first.
- `Kick`, `Snare`, `Hat`, `Fm`, `Va` are mono instruments: a second `NoteOn` restarts the note. Anything two players can trigger at once goes in a `VoicePool`.
- `Pluck<70>` (`174`) cannot play below about 70 Hz; `NoteOn` clamps (`pluck.h:112-114`). `SFX_WIN` pushes `Mtof(40)` (82 Hz) which is just inside.
- Rng pointers passed to `Init` must outlive the engine; they are members here (`609`), keep them so.
- No libc++, no exceptions, no RTTI (`build.sh:64`); Bellows is written for that. Do not add `<vector>`, `<string>` or `new`.
- Stem gains are measured values (`560-561`); after any patch change, render a WAV via `tools/shot.mjs` and check RMS and the limiter before trusting the mix.
- House style for the new cart's comments and any in-game audio text: no em or en dashes, no emoji; the existing file uses box-drawing section rules (`46, 172`) which are fine.
