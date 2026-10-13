# DO-OCRACY front end: final specification

Title, attract, main menu, modes, settings, rules flash, credits, lobby, night HUD declutter, pause, results. One document, implementation ready. It is the winning proposal (design-frontend-2, "game feel first") with the judged grafts from proposals 1 and 3 folded in, and every function, file, coordinate and line number checked against the source as it stands on 2026-10-12: `src/draw.c` 778 lines, `src/hud.c` 347, `src/hud.h` 64, `src/game.c` 1077, `src/game.h` 195, `src/main.c` 415, `src/content.h` 110, `src/audio.h` 16, `src/audio_bellows.cpp` 691, `tools/shot.mjs` 76.

Everything is drawn on the existing 1280x720 HUD buffer with the primitives in `hud.c` plus the set added in section 5. Fonts are only the eight already baked (`hud.c` 46 to 62). Colours are only the `C_*` tokens in `hud.h` 12 to 33 and `PCOL[]` (`game.c` 21). The HeatSync mark is drawn only by `hud_logo` with a `C_BONE` flame and is never scaled by anything but its size argument, never tinted, never redrawn.

House rules observed throughout and required of everything this document produces: no em or en dashes, no emoji, no bold for emphasis, ASCII caps in game strings (only 32..127 is baked, `hud.c` 38), roles never names, GANTRY tokens only, square corners, solid offset shadows, caution tape between things. The hyphens in DO-OCRACY, CO-OP and CLUB-MATE are compound words and stay.

---

## 0. The stance, and what this spec resolves

The room is the game, so the room is the background of every screen. The scrim drops from alpha 150 to 70, text gets a drawn shadow instead of a wall of dark, and the attract night keeps running behind the menus. Every cursor move makes a sound, every card slides and snaps, every screen change is a 0.28 s wipe, nothing blinks off and on. Text on the title drops from about 28 strings to 9. During the night the only button name on screen is the one under your own feet.

Decisions that go beyond the winning proposal, each with the reason:

1. Lobby seat cards move to the bottom band, y 580 to 700, and the lobby spawn anchor moves off the door. The play camera is a 39 degree perspective at (0, 21.5, 15) looking at (0, 0, -1.25) (`world.c` 707, `main.c` 380). Projecting with it: the back wall floor line is at screen y 216, the front wall at y 664, the door spawn (-9.2..-6.8, z 5.6) lands at (230..337, 603) with the head at y 559. The winner's cards at y 380..580 would sit on top of the lobby floor from z 0.9 to z 5.2, which is the lounge and the middle of the room where people try things. The bottom band y 580..700 covers z 5.2 and beyond (Show & Tell, the Free Pile, the door mat, the sidewalk), which the night's own card row at y 634..720 already half covers. The lobby spawn becomes x = -6.5 + (slot % 4) * 0.9, z = 1.6 - (slot / 4) * 0.9, which projects to (378..487, 452) with the head at y 400, well clear of the cards and clear of every obstacle box (nearest is the kitchen table at x -7.55, 1.05 units away). Mid-night joins keep the door anchor. The acceptance script verifies this with `tools/shot.mjs` (section 10, check L3).
2. Toasts move to the top right, two slots right aligned at x 1264, y 108 and 132, over the back wall (graft from proposals 1 and 3). `reserve(960, 104, 304, 52)` is added before world labels, and the reserve grows to the widest live toast so a long line never has a station tag settle under it.
3. CHAOS is a tri-state setting (CALM, NORMAL, CHAOS) with pace 1.4, 1.0, 0.65 on the four spawn refills and bad event weight multipliers 0.6, 1, 2 inside `fire_event`, which gets a re-roll loop in place of the `(e + 1) % 13` bump so zero weights never fire. The CHAOS mode is a preset (night 120 s, pace 0.65, badmul 2, jam chance 0.6, Dr. Strange down at start) applied at night start without rewriting `SET`.
4. RULES FLASH and COACH collapse into one HELP row (OFF, FIRST NIGHT, ALWAYS). Settings is ten rows. The NIGHT script row and the LAYOUT top down preview are fenced behind `NIGHTS[]` and `layout_count > 1`, which the layout workstream owns; today the ROOM row shows the single value 108 W MAIN with dimmed chevrons.
5. The pause overlay widens to 820 and takes the deleted how_to board's two loops and the clutter bar beside the pauser's controls strip, so pause is the one on-demand reference for both buttons and loops. The parenthetical joke on PROPOSE stays.
6. Non-owner seats that press A on a menu are not only denied; they are recorded as pending and shown in an AT THE DOOR panel on the main menu, then seated in seat order after the owner when PLAY opens the lobby. The deny joke fires once per seat.
7. CUSTOM on the mode cards opens SETTINGS with DONE returning to the lobby (`M.settings_then = TO_LOBBY`).
8. The night join strip tweens from 320x56 to 120x56 at 45 s rather than vanishing; it is hidden only when `SET.phones` is off or the relay is offline.
9. The lobby clock is its own `lobby_t` in main.c that does not advance while the flash is up or any seat is mid hold-B, and resets to 25 on every join. `G.t` is never clamped.
10. Every results row carries the seat glyph before the name, so the KEYS and PAD debug tags leave the night cards for good. Co-op results pair the S A B C letter with a tier word eyebrow (SPOTLESS, OPEN TOMORROW, BARELY, CLOSED) by vibe and clutter at close, and the banner reads THE REAL WINNER: EVERYONE WHO SWEPT.
11. RED TAG replaces an item's floor label after 25 s instead of stacking under it. One object, one label.
12. On END_SURVIVED the results fire `fx_burst(FX_CONFETTI, 1.8f, 2.0f, 6.3f, C_HAZARD)` at t 0.3, 1.1 and 1.9 so confetti falls over Show & Tell under the play camera, in addition to screen confetti on a record.

---

## 1. Files touched

| File | What changes |
|---|---|
| `src/hud.h`, `src/hud.c`, new `src/glyphs.h` | text shadow, global alpha, clip rect, glyph bitmaps, keycap and eyebrow moved in from draw.c, easing and tween, screen space particles, step dots, pictogram helpers, coloured text runs |
| `src/game.h` | `menu_t M`, `settings_t SET`, mode, screen and help enums, nav bits, new `game_t` and `player_t` fields, new prototypes |
| `src/game.c` | `game_begin_lobby`, `game_apply_settings`, `game_quit_to_title`, `game_mode_bots`, pace and bad event multipliers with the re-roll in `fire_event`, `G.paused`, group counters, coach target, team karma, award counters, lobby spawn anchor, end variant |
| `src/draw.c` | `title_screen` replaced by `front_end`; new `attract_overlay`, `menu_main`, `menu_preview`, `who_is_in`, `menu_modes`, `mode_card`, `menu_settings`, `settings_help`, `rules_flash`, `flash_card`, `credits_screen`, `lobby_overlay` rewrite, `seat_card`, `countdown`, `start_wipe`, `top_bar` and `player_cards` rewrite, `mode_panel`, `join_strip` tween, `toasts` move, `pause_overlay`, `results_screen` rewrite, `results_coop`, `draw_hud` reorder; `how_to`, `controls_card`, `join_panel` deleted |
| `src/main.c` | seat table, nav reader with repeat, `menu_update`, owner model, pad `connected` gate removed, save region, `flow()` rewrite with `lobby_t`, camera tween, pause gate, hold-B leave |
| `src/audio.h`, `src/audio_bellows.cpp` | eight new SFX, `audio_set_levels` |
| `src/content.h` | menu strings, mode table, settings tables, end variants, flash captions as runs, credits, coach steps, taglines and tips replaced, toasts trimmed, awards added |
| `tools/shot.mjs` | `SAVE=path` env writes `host.getSaveData()` at exit (12 lines) |

---

## 2. Screen map, state and timings

Phases stay `PH_TITLE, PH_LOBBY, PH_NIGHT, PH_RESULTS` (`game.h` 18). The attract night is `G.attract == true` with `G.phase == PH_NIGHT`, exactly as today (`main.c` 364 to 370). The front end is a separate layer, `M.screen`, meaningful while `G.phase == PH_TITLE || G.attract`, plus two cases drawn over other phases: the rules flash over the lobby or a paused night, and the pause overlay over the night.

```c
enum { SCR_TITLE, SCR_ATTRACT, SCR_MAIN, SCR_MODES, SCR_SETTINGS, SCR_FLASH, SCR_CREDITS, SCR_NONE };
enum { TO_MAIN, TO_LOBBY };            /* M.settings_then: where DONE goes */
```

```
                 20 s idle                any input
   SCR_TITLE  ------------->  SCR_ATTRACT -----------> SCR_TITLE
      |  A or START (any seat)        A or START: that seat owns, straight to MAIN
      v
   SCR_MAIN  (owner seat drives it)   20 s owner idle: ownership passes to the lowest pending seat,
      |                                 or, with none pending, owner released and SCR_TITLE
      | PLAY            | MODES          | SETTINGS           | HOW TO PLAY        | CREDITS
      v                 v                v                    v                    v
   PH_LOBBY          SCR_MODES        SCR_SETTINGS         SCR_FLASH            SCR_CREDITS
   owner is P1,      A pick -> MAIN   DONE -> settings_then  A next, B exit      B back
   pending seats     CUSTOM ->        (TO_MAIN or TO_LOBBY)  returns to MAIN
   seated after,     SETTINGS with    B -> MAIN (or MODES
   flash first if    then = TO_LOBBY  when then = TO_LOBBY)
   HELP says so
      | all humans READY (3 s countdown) or lobby_t runs out at 25 s
      v
   PH_NIGHT    START by any human: G.paused, pause overlay (RESUME / HOW TO PLAY / QUIT TO MENU)
      | end_night
      v
   PH_RESULTS  after 4 s: A or START -> PH_LOBBY (same settings); B -> SCR_MAIN with the presser as
               owner and every other human pending; 40 s -> lobby if humans remain else SCR_TITLE
```

Background during SCR_TITLE through SCR_CREDITS: the attract night (three bots) runs as today (`flow()` PH_TITLE starts it 1.5 s after entering PH_TITLE; `end_night` in attract returns to PH_TITLE and it loops, `game.c` 976 to 980). The menu layer is untouched by that loop. The camera orbits as today (`camera()` attract branch, `main.c` 381).

Timing table (seconds):

| Thing | Value |
|---|---|
| title idle before attract | 20 |
| owner idle in any menu before release | 20 (not while SCR_FLASH is up) |
| screen wipe | 0.28 (0.14 out, 0.14 in) |
| cursor tween | exponential, speed 18 per s, 3 px overshoot via ease back over 0.22 |
| row stagger on menu open | 0.04 per row |
| title to main logo tween | 0.35 ease out cubic |
| tagline rotation | 6, crossfade 0.3 |
| attract tip rotation | 3 |
| stick repeat | first 0.28, then 0.11, threshold 0.5 |
| flash card hold | 2.6 |
| flash card wipe | 0.30 |
| flash total (5 cards) | 14.2 |
| lobby auto start (`lobby_t`) | 25, frozen during the flash and any hold-B, reset to 25 on join |
| lobby ready countdown | 3 (3, 2, 1, GO) |
| lobby leave hold B | 1.5 (1.0 when the owner is alone: returns to the menu instead) |
| night start tape wipe | 0.3 close, 0.3 open |
| join strip wide form | 45, then 0.3 tween to the narrow form |
| results input lock | 4 |
| results timeout | 40 |
| results row stagger | 0.25 (kept) |
| results confetti (survived) | world bursts at 0.3, 1.1, 1.9 |
| deny shake | 0.2, 6 px |
| camera tween orbit to play | 0.8 ease in out |
| toast visible | 4 (kept: drawn while `toast_t > 2`) |

---

## 3. Input model

### 3.1 Seats

A seat is an input source that may or may not be a player yet. Fourteen seats:

```c
#define SEAT_PADS 4
#define SEAT_KBS 2
#define SEAT_PHONES MAX_PLAYERS
#define SEAT_COUNT (SEAT_PADS + SEAT_KBS + SEAT_PHONES)   /* 14 */
#define SEAT_PAD(i)   (i)          /* 0..3 */
#define SEAT_KB(k)    (4 + (k))    /* 4, 5 */
#define SEAT_PHONE(i) (6 + (i))    /* 6..13 */

typedef struct {
  bool present;        /* pad: always true (3.2); kb: a key was ever seen this boot; phone: registered */
  uint32_t btn, prev;  /* IN_A IN_B IN_X IN_START */
  float sx, sy;        /* -1..1, sy positive is down on screen (toward +z) */
  int dir, last_dir;   /* NAV_* direction currently held, for repeat */
  float repeat;        /* seconds until the next repeat event */
  int slot;            /* player slot or -1 */
  char name[14];       /* "PAD 1".."PAD 4", "WASD", "ARROWS", or the phone's name */
  char pid[25];        /* phones only */
  bool pending;        /* pressed A on a menu, or a phone said J while the front end was up: seated when the lobby opens */
  bool denied;         /* the deny joke has fired for this seat (once per seat, reset by new_group) */
  float hold_b;        /* lobby: seconds B has been held */
} seat_t;
static seat_t seats[SEAT_COUNT];   /* main.c */
```

`pad_slot[]`, `pad_prev[]`, `pad_gone[]`, `kb_slot[]`, `kb_prev[]` (`main.c` 140 to 148) and `phones[]` (`main.c` 31) collapse into `seats[]`. `game_on_phone` (`main.c` 275) writes `seats[SEAT_PHONE(i)]`.

### 3.2 Pads under RetroArch

Remove the `p->connected` test at `main.c` 190 and the `pad_gone` removal at 191 to 197. Poll `pad_buttons()` on all four pads every frame; an unplugged pad reports zero buttons, which is harmless. Idle removal at 75 s (`main.c` 233 to 240) stays and now covers unplugged pads. Under the libretro core only pad 0 reports connected whatever is plugged in (map-platform section 2), and the keyboard arrives as RetroPad 1 with Z as A, X as B, arrows, Enter as START, so the cabinet keyboard is just a pad and needs nothing special. Keyboard seats 4 and 5 stay for the browser host (two players on one keyboard); under RetroArch `keys[]` never fires and those seats simply never become present. `WC_FLAG_KEYBOARD` stays set for the browser's sake.

### 3.3 Nav events

```c
enum { NAV_UP = 1, NAV_DOWN = 2, NAV_LEFT = 4, NAV_RIGHT = 8, NAV_A = 16, NAV_B = 32, NAV_START = 64 };
static int nav_read(seat_t *s, float dt);   /* main.c, returns a bitmask for this frame */
```

- Direction from the stick or d-pad (`pad_buttons` folds the d-pad into the stick, `main.c` 150 to 165): `|sy| >= 0.5` beats `|sx| >= 0.5`; one direction at a time.
- A new direction emits at once and sets `repeat = 0.28`. Held: `repeat -= dt`; at zero emit again and set `repeat = 0.11`. Releasing clears `dir`.
- A, B, START are edges against `prev`.
- Phones: the four wire bits (`btn & 15`, `main.c` 311) plus the stick, so phones can drive every menu. Nothing in any menu needs X, L, R, Y or SELECT.

### 3.4 Owner, pending seats, deny

`M.owner` is a seat id or -1. On SCR_TITLE or SCR_ATTRACT, the first seat to edge A or START becomes owner and the screen goes to SCR_MAIN. While an owner exists only the owner's nav events drive the menu.

A non-owner edge of A or START on SCR_MAIN, SCR_MODES, SCR_SETTINGS or SCR_CREDITS:

1. If `!seat.denied`: `M.deny_t = 2`, deny line "ONE AT A TIME. THIS IS A DO-OCRACY." (or "JOIN IN THE LOBBY. P1 IS PICKING." when `M.screen != SCR_MAIN`), SFX_MENU_DENY, the 6 px shake; `seat.denied = true`.
2. `seat.pending = true`. The seat appears in the AT THE DOOR panel (6.3) as P2, P3... in seat order. A pending seat that presses A again does nothing (no second joke).

Pending seats never steal the menu. If the owner is idle 20 s, or a phone owner is lost to the relay (`seen > 12`), ownership passes to the lowest pending seat (its `pending` clears, `M.idle = 0`, SFX_MENU_OK, the owner chip swaps with the 0.18 s slide) and the menu stays; with no pending seat the owner is released and the screen returns to SCR_TITLE. Pending seats survive a return to the title (the title shows "N AT THE DOOR") and are cleared only by `new_group()`.

On PLAY: `game_begin_lobby()`, then `game_add_player(...)` for the owner, so the owner is always P1 (slot 0, `PCOL[0]`). Then every pending seat is seated in seat id order (pads, keyboards, phones). Everyone else joins in the lobby with A or START as today, and may still join mid-night with A as today.

On PH_RESULTS, B after 4 s makes the presser the owner and goes to SCR_MAIN with the title band already small; every other human seat becomes pending so the group re-seats itself on the next PLAY. A or START goes to the lobby as today.

### 3.5 Keyboard glyph

`glyph_a()` (`draw.c` 81) stays: "SPACE" for kb seat 0, "." for kb seat 1, "A" for pads and phones. The title CTA says PRESS A; when any `keys[]` bit has ever been set this boot (`M.kb_seen`), a second line OR SPACE ON A KEYBOARD appears under it. Under RetroArch `kb_seen` never becomes true, so the cabinet shows only PRESS A.

### 3.6 Phones on the front end

- `J` while the front end is up (`G.phase == PH_TITLE || G.attract`): register the seat (`present`, `pid`, `name`), set `pending = true`, reply `H` (hold on; the phone page already understands it). The title and menus count it in "N AT THE DOOR". A pending phone may become owner by pressing A on the title, or by being the lowest pending seat when the owner idles out.
- When the lobby opens, pending phones are seated by `game_add_player(true, -1, pid, name)` in seat order after the owner and the pending pads.
- `SET.phones == 0`: no QR anywhere, `J` is answered `F` (full), `net_update` still runs so the relay stays warm for the next group.
- `I` messages for a pending or owner phone update `seats[].btn, sx, sy` and are not forwarded to `game_player_input` until the phone has a slot.

### 3.7 What A, B and START mean per screen

| Screen | A | B | START | Stick |
|---|---|---|---|---|
| SCR_TITLE, SCR_ATTRACT | claim, open MAIN | nothing | same as A | title: nothing; attract: return to title |
| SCR_MAIN | select row | release owner, back to TITLE | same as A on PLAY | up, down rows; left, right on PLAY step the mode |
| SCR_MODES | pick, back to MAIN with PLAY focused (CUSTOM: open SETTINGS, then = TO_LOBBY) | back to MAIN | pick and PLAY | left, right |
| SCR_SETTINGS | step the value right; on RESET reset; on DONE leave to `settings_then` | leave to `settings_then` target without a second confirm | same as A on DONE | up, down rows; left, right values |
| SCR_FLASH | next card (last card: end) | end | end | nothing |
| SCR_CREDITS | back | back | back | nothing |
| PH_LOBBY | join (unseated seat); try things (seated) | drop; hold 1.5 s to leave your seat (owner alone: hold 1.0 s to return to MAIN) | READY toggle (seated); join (unseated) | walk |
| pause | select row | resume | resume (any seat) | up, down |
| PH_RESULTS after 4 s | lobby | MAIN, presser owns | lobby | nothing |

---

## 4. Settings, modes and persistence

```c
enum { MODE_SOLO, MODE_COOP, MODE_VERSUS, MODE_CHAOS, MODE_CUSTOM, MODE_COUNT };
enum { CHAOS_CALM, CHAOS_NORMAL, CHAOS_CHAOS };
enum { HELP_OFF, HELP_FIRST, HELP_ALWAYS };

typedef struct {
  uint8_t mode;        /* MODE_* */
  uint8_t night_len;   /* index into NIGHT_LENS: 0 SHORT 120, 1 NORMAL 180, 2 LONG 300, 3 MARATHON 600 */
  uint8_t bots;        /* 1 on: fill when exactly one human is seated */
  uint8_t chaos;       /* CHAOS_* */
  uint8_t phones;      /* 1 on */
  uint8_t music, sfx;  /* 0..10 */
  uint8_t layout;      /* index into LAYOUTS[] (owned by the layout workstream; 0 is today's room) */
  uint8_t help;        /* HELP_* */
  uint8_t pad_[7];
} settings_t;          /* 16 bytes */
extern settings_t SET;  /* defined in game.c, written by main.c */

#define SAVE_MAGIC 0x434f4f44u   /* 'DOOC' */
#define SAVE_VERSION 1
typedef struct {
  uint32_t magic, version, crc;   /* crc32 of everything after crc */
  settings_t set;
  uint32_t nights_played;
  int32_t best_karma;  char best_karma_name[14];
  int32_t best_clout;  char best_clout_name[14];
  int32_t best_team;               /* co-op team karma */
  uint8_t reserved[32];
} save_t;
static save_t SAVE;              /* main.c, static so the address is stable */
```

In `wc_get_info` (`main.c` 37 to 51), after the other fields: `info.save_ptr = (uint32_t)(uintptr_t)&SAVE; info.save_size = sizeof SAVE;`. In `wc_init` (`main.c` 101) before `game_init()`: `save_load()` checks magic, version and crc; on any mismatch (including all zero, a never saved cart) it writes defaults: mode SOLO, night_len 1, bots 1, chaos NORMAL, phones 1, music 7, sfx 8, layout 0, help FIRST. `save_commit()` recomputes the crc; it is called after every settings change and after results update a record. The host persists the region after the frame (RetroArch SRAM under `/userdata/saves/wasmcart/<cart>.srm`, browser storage, `CartHost.getSaveData()` in Node). `tools/shot.mjs` never passes save data, so headless runs always start at defaults.

Settings reach the sim through `game_apply_settings()` (section 8.4), called when the lobby opens and again in `game_start_night`. `G.night_len` comes from `NIGHT_LENS[SET.night_len].secs`, overriding `config.json night_seconds` (`main.c` 89 to 93), which remains the default for index 1 when present and in range.

Mode pins and presets, resolved by `game_apply_settings()`:

| Mode | Bots | Pace, bad events, clock | Scoring | Pinned rows (drawn dimmed with "(MODE)") |
|---|---|---|---|---|
| SOLO | always BUILDER and TALKER when one human | by SET | ranked clout and karma | BOTS |
| CO-OP | BUILDER and TALKER when one human and `SET.bots` | by SET | team karma, tier letter | none |
| VERSUS | BUILDER and FOUNDER as rivals when one human and `SET.bots`; none otherwise | by SET | ranked clout, void on a lost night | none |
| CHAOS | as SOLO | night 120, pace 0.65, badmul 2, jam 0.6, Dr. Strange broken at start; `SET` untouched | ranked | NIGHT LENGTH, CHAOS |
| CUSTOM | by SET (one human: BUILDER and TALKER) | by SET | ranked | none |

Session state that is not saved: `G.flash_seen` (the flash has played for this group), `G.group_nights` (nights this group has played), the pending and denied flags. All reset in `new_group()`, which runs when `flow()` enters PH_TITLE from an empty lobby, an empty night or empty results; that is how a cabinet knows a new group has walked up.

HELP semantics:

| HELP | Flash on PLAY | Coach during the night |
|---|---|---|
| OFF | never (HOW TO PLAY still works) | never |
| FIRST NIGHT | when `!G.flash_seen` | steps 1 to 3 on `G.group_nights == 1`; steps 4 and 5 only if `!G.flash_seen` |
| ALWAYS | every PLAY | every night, all five steps |

Volumes: `audio_set_levels(float music, float sfx)` with `SET.music / 10.f` and `SET.sfx / 10.f`, called at init and on every change of those rows.

---

## 5. New hud.c primitives

All added to `hud.h`. Signatures are final; bodies are described where not obvious. The clip rect and the alpha multiplier live inside `blend()` (`hud.c` 66) and the `hud_rect` fast path (`hud.c` 79); nothing else writes pixels.

```c
/* a coloured run of text; a caption is an array of runs, terminated by s == NULL */
typedef struct { const char *s; uint32_t c; } run_t;

/* text with a 3 px solid shadow in C_SHADOW at 80 percent of a, then the text */
int  hud_text_sh(int font, int x, int y, const char *s, uint32_t rgb, int a, int align);
/* runs drawn left to right; total width measured with hud_text_w for align; shadowed */
int  hud_text_runs(int font, int x, int y, const run_t *runs, int a, int align);

/* global alpha multiplier applied inside blend(): a = a * mul / 255; default 255 */
void hud_set_alpha(int mul);

/* clip rect applied inside blend() and the hud_rect fast path; hud_clip_clear resets to the screen */
void hud_clip(int x, int y, int w, int h);
void hud_clip_clear(void);

/* moved from draw.c 72 to 91, unchanged bodies */
void hud_eyebrow(int x, int y, const char *s, uint32_t c, int align);
int  hud_keycap(int x, int y, const char *k, uint32_t fill, uint32_t ink);

/* easing: t in 0..1 */
float hud_ease_out(float t);      /* 1 - (1-t)^3 */
float hud_ease_inout(float t);    /* t<.5 ? 4t^3 : 1 - (-2t+2)^3 / 2 */
float hud_ease_back(float t);     /* 1 + 2.70158*(t-1)^3 + 1.70158*(t-1)^2 (overshoots to about 1.1) */
/* exponential tween: cur += (target - cur) * (1 - exp(-speed * dt)); snaps when within 0.5 */
void  hud_tween(float *cur, float target, float speed, float dt);

/* 16x16 one-bit glyphs drawn at integer scale s (16*s px square) */
enum { GL_CLOUT, GL_KARMA, GL_PAD, GL_KEYS, GL_PHONE, GL_BOT, GL_MARSHAL, GL_HEART,
       GL_BROOM, GL_HAND, GL_CHECK, GL_NOTE, GL_SPEAKER, GL_COUNT };
void hud_glyph(int x, int y, int id, int s, uint32_t rgb, int a);

/* a row of step dots: n dots, 10 px, pitch 22, centred on cx; dot k < step filled C_STEEL_HI,
   k == step is a 60 px bar filling by frac in C_HAZARD, k > step hollow 2 px C_STEEL */
void hud_steps(int cx, int y, int n, int step, float frac);

/* pictograms for the rules flash and the mode cards (screen space, px) */
void hud_pict_person(int x, int y, int h, uint32_t body, bool newbie);   /* head disc r h/6 at top, body rect; newbie draws grey 0x8b8578 */
void hud_pict_station(int x, int y, int w, int h, const char *label, uint32_t edge);   /* char box, 4 px edge, label F_MONO_S */
void hud_pict_item(int x, int y, int kind, uint32_t c);   /* 0 plywood (bone 28x18), 1 project (amber 22x22), 2 scrap (ash 16x16), 3 daisy (fault zigzag 40x12) */

/* screen space particles, separate from the world FX in draw.c */
enum { FX2_SPARK, FX2_CONFETTI, FX2_RING };
void fx2_burst(int kind, float x, float y, uint32_t c);
void fx2_draw(float dt);
```

Glyph data (`glyphs.h`, parsed once in `hud_init` into `uint16_t rows[GL_COUNT][16]`; `#` is set). Every row is exactly 16 characters; the lint script checks this.

```c
static const char *const GLYPH_SRC[GL_COUNT][16] = {
{ /* GL_CLOUT: a four point sparkle */
".......#........", ".......#........", "......###.......", "......###.......",
".....#####......", "#...#######...#.", ".##.#######.##..", "..###########...",
"..###########...", ".##.#######.##..", "#...#######...#.", ".....#####......",
"......###.......", "......###.......", ".......#........", ".......#........" },
{ /* GL_KARMA: the plus */
"................", "................", "......####......", "......####......",
"......####......", "......####......", "..############..", "..############..",
"..############..", "..############..", "......####......", "......####......",
"......####......", "......####......", "................", "................" },
{ /* GL_PAD */
"................", "................", "................", "...##########...",
"..############..", ".##############.", ".###.######.###.", ".#.#.######.#.#.",
".###.######.###.", ".##############.", ".#####....#####.", ".####......####.",
".###........###.", "................", "................", "................" },
{ /* GL_KEYS */
"................", "................", "................", "################",
"#..............#", "#.#.#.#.#.#.#..#", "#..............#", "#.#.#.#.#.#.#..#",
"#..............#", "#..#.#.#.#.#...#", "#..............#", "#...########...#",
"#..............#", "################", "................", "................" },
{ /* GL_PHONE */
"................", ".....######.....", "....#......#....", "....#.####.#....",
"....#......#....", "....#......#....", "....#......#....", "....#......#....",
"....#......#....", "....#......#....", "....#......#....", "....#......#....",
"....#......#....", "....#..##..#....", "....#......#....", ".....######....." },
{ /* GL_BOT */
".......#........", ".......#........", "......###.......", "..############..",
"..#..........#..", "..#..##..##..#..", "..#..##..##..#..", "..#..........#..",
"..#..........#..", "..#.########.#..", "..#..........#..", "..############..",
"....#......#....", "....#......#....", "...###....###...", "................" },
{ /* GL_MARSHAL: helmet with a brim */
"................", ".....######.....", "....########....", "...##########...",
"...###.##.###...", "...###.##.###...", "...##########...", "...##########...",
"################", "#..............#", "################", "................",
"................", "................", "................", "................" },
{ /* GL_HEART */
"................", "................", "...###....###...", "..#####..#####..",
".######..######.", ".##############.", ".##############.", ".##############.",
"..############..", "...##########...", "....########....", ".....######.....",
"......####......", ".......##.......", "................", "................" },
{ /* GL_BROOM */
"............##..", "...........##...", "..........##....", ".........##.....",
"........##......", ".......##.......", "......##........", ".....##.........",
"....####........", "...######.......", "..########......", ".#########......",
".#########......", ".#.#.#.#.#......", ".#.#.#.#.#......", "................" },
{ /* GL_HAND: a wave */
"......#.#.......", "....#.#.#.#.....", "....#.#.#.#.....", "....#.#.#.#.....",
"....#######.....", "..#.#######.....", "..#.#######.....", "..#########.....",
"...########.....", "...########.....", "....######......", "....######......",
".....####.......", ".....####.......", "................", "................" },
{ /* GL_CHECK */
"................", "................", "..............##", ".............##.",
"............##..", "...........##...", "..........##....", ".........##.....",
"##......##......", ".##....##.......", "..##..##........", "...####.........",
"....##..........", "................", "................", "................" },
{ /* GL_NOTE */
"................", "........####....", "........#...##..", "........#.....#.",
"........#.....#.", "........#....#..", "........#.......", "........#.......",
"........#.......", "........#.......", "....#####.......", "..#######.......",
".########.......", ".#######........", "..#####.........", "................" },
{ /* GL_SPEAKER */
"................", "................", ".......#........", "......##........",
".....###...#....", "....####....#...", "#######..#...#..", "#######..#.#..#.",
"#######..#.#..#.", "#######..#.#..#.", "#######..#...#..", "....####....#...",
".....###...#....", "......##........", ".......#........", "................" },
};
```

`hud_glyph` draws each set bit as `hud_rect(x + c*s, y + r*s, s, s, rgb, a)`; at scale 1 use `blend` directly.

Screen particles: `fx2_t { float x, y, vx, vy, life, max, size; uint32_t c; int kind; }`, ring of 240. FX2_SPARK: 12 squares 3 px, speed 180 to 420 px/s in a random direction, gravity 900 px/s^2, life 0.35 to 0.5. FX2_CONFETTI: 60 strips 6x3 px alternating width, from (x, y) spread 400 px wide, vy 80 to 220 downward, vx drift, gravity 300, drag 1.5/s, life 2.0 to 2.6, palette hazard, live, violet, info, rust, bone. FX2_RING: 16 dots 4 px outward at 260 px/s, life 0.4. Fade over the last 30 percent of life, like `fx_draw` (`draw.c` 140 to 170).

`hud_text_runs`: sum `hud_text_w(font, run.s)` over the runs for the total width, place the start by `align`, then for each run `hud_text_sh(font, x, y, run.s, run.c, a, AL_LEFT)` and advance by its width. Spaces live inside the run strings (a trailing space on a run is measured by `hud_text_w` as the advance of the space glyph, which is what we want).

---

## 6. Screens

Coordinates are HUD pixels, origin top left. `hud_text` y is the top of the line box (`hud.c` 143 to 149), so a 40 px F_H2 in a 56 px row sits at y plus 6. In the wireframes a row of `=` is caution tape and `#` is a solid fill.

### 6.1 SCR_TITLE (idle)

```
0    100   200   300   400   500   600   700   800   900   1000  1100  1200 1280
+========================================================================================+ 0..24 tape, scroll 30 px/s
|                                                                                        |
|                       (the room: bots playing, camera orbiting, scrim 70)              |
|                                                                                        |
|----------------------------------------------------------------------------------------| 190  2 px C_STEEL_HI rule
|                                                                                        |     band C_CHAR alpha 150, y 190..430
|        [ MARK 180 ]     HEATSYNC LABS ARCADE                                           | 222  eyebrow at (356,222)
|        (150,210)        DO-OCRACY                                                       | 248  F_TITLE at (350,248) bone, shadow
|                         THE LASER IS BOOKED. THE MOP IS FREE.                          | 392  F_H3 hazard at (356,392)
|----------------------------------------------------------------------------------------| 428  rule
|                                                                                        |
|                              ######  PRESS A  ######                                   | 500..556  hazard block 420x56, x 430..850
|                              OR SPACE ON A KEYBOARD                                    | 566  F_MONO_S ash, only when kb_seen
|                                  2 AT THE DOOR                                         | 586  F_MONO_S ash, only when pending > 0
|                                                            +----------------------+    | 560  phone module (1040,560) 216x136
|                                                            | [QR 96]  PHONES      |    |       eyebrow at (1160,576)
|                                                            |          KQRT        |    |       code F_H2 hazard at (1158,596)
|                                                            +----------------------+    |
| LAB RECORD  1,240 KARMA  BY PAD 1                          HEATSYNC LABS. MESA. SINCE 2009. | 690 F_MONO_S: left ash at (24,690), right smoke right aligned at 1256
+----------------------------------------------------------------------------------------+ 720
```

- Background: the attract night is running. Scrim `hud_rect(0,0,1280,720,C_CHAR,70)`. Band `hud_rect(0,190,1280,240,C_CHAR,150)` with 2 px `C_STEEL_HI` rules at y 190 and 428.
- Mark: `hud_logo(150, 210, 180, C_BONE)`. Never anything else.
- Title: `hud_text_sh(F_TITLE, 350, 248, "DO-OCRACY", C_BONE, 255, AL_LEFT)`. The block (mark plus title) is about 1000 px wide and sits centred.
- Tagline: one line, `TAGLINES[(int)(T / 6.0f) % COUNT(TAGLINES)]`, crossfading over 0.3 s (alpha down then up on the swap). The 14 lines in section 8.6.
- CTA: `hud_rect(436,506,420,56,C_SHADOW,255); hud_rect(430,500,420,56,C_HAZARD,255); hud_text(F_H2, 640, 506, "PRESS A", C_TAPE, 255, AL_CENTER)`. Pulse: the block grows 0 to 8 px in width and height about its centre on `0.5 + 0.5*sinf(T*4.4)` (1.4 s period). It never blinks off.
- Phone module: `hud_panel(1040,560,216,136,C_SLAB,C_STEEL_HI)`, `hud_qr(1052,572,96,G.join_url)`, eyebrow PHONES, code. Hidden entirely when `!net_online()` or `SET.phones == 0`.
- "N AT THE DOOR" counts pending seats of every kind.
- Footer left: LAB RECORD line only when `SAVE.best_karma > 0`: "LAB RECORD  %d KARMA  BY %s" where the name is the seat or phone name (never a real person's name typed by us). Footer right: "HEATSYNC LABS. MESA. SINCE 2009."
- String count: eyebrow, title, tagline, PRESS A, (kb hint), (at the door), PHONES, code, record, footer: 8 to 10.
- Input: any seat A or START: that seat is owner; SFX_MENU_OK; transition to SCR_MAIN. Idle 20 s with no input from anyone: SCR_ATTRACT.

Transition title to main (0.35 s, `M.t` since the change):
- `k = hud_ease_out(min(1, M.t / 0.35))`.
- Band alpha 150 to 0, rules fade with it. Tagline and CTA alpha 255 to 0 over the first 0.15 s.
- Mark position and size lerp from (150, 210, 180) to (32, 40, 64). Title F_TITLE fades out over 0.15 s while F_H2 DO-OCRACY at (108, 52) fades in over the last 0.2 s (there is no font scaling; the swap is hidden by the fade and the moving mark).
- Menu rows enter from the left (x minus 60) with alpha 0 to 255, each row delayed 0.04 s after the previous, each over 0.2 s ease out.

### 6.2 SCR_ATTRACT

The living demo. No scrim. The room HUD is shown in a reduced form so a passerby sees a game being played.

```
+========================================================================================+ 0..24 tape
| [MARK 64] DO-OCRACY                                                                    | mark (32,40), F_H2 at (108,52) bone, shadow
|                                                                                        |
|            (bots playing: small name tags, pops, goal chevrons, station names at 150)  |
|                                                                                        |
|                                                                 +----------------+     | 620..664 hazard block 216x44 at (1040,620)
|                                                                 |    PRESS A     |     |   F_H3 tape ink, same pulse as 6.1
|                                                                 +----------------+     |
|########################################################################################| 680..720 tape black bar
|                 3. GET A QUOTE. IT IS $4,000.                                          | 686 F_MONO ash centred, ATTRACT_TIPS every 3 s
+----------------------------------------------------------------------------------------+
```

- Drawn under the front end layer by `attract_overlay()`: `goal_guides` for bots too (a flag lifts the `p->bot` filter at `draw.c` 293 in attract), `station_labels` with names at alpha 150 and no status lines, `players_overlay` bot variant (14 px tags at alpha 170), pops, `fx_draw`.
- Tips: `ATTRACT_TIPS[(int)(T / 3.0f) % COUNT]` in order; the five floor loop lines stay first and in sequence. The `$4,000` and `1,716` numbers are the only numbers on the attract screen.
- Any seat input (any nav bit) returns to SCR_TITLE with the scrim fading in over 0.3 s. If that input was A or START, the seat becomes owner and the screen continues straight into SCR_MAIN (the mark animates from the corner, where it already is, so only the rows slide in).

### 6.3 SCR_MAIN

```
+========================================================================================+ 0..24 tape
| [MARK 64] DO-OCRACY                                                                    | mark (32,40) 64; F_H2 (108,52) bone
|           HEATSYNC LABS ARCADE                                                         | eyebrow (108,100) ash
|                                                                                        |
|   +------------------------+                 +------------------------------------+   | 200  rows x 96 w 520 h 56 pitch 64
|   |## PLAY      < SOLO >  #|                 | PREVIEW (700,200) 540x320           |   |      focused row: hazard block, 6 px shadow, text C_TAPE
|   +------------------------+                 |  hud_panel C_SLAB / C_STEEL_HI       |   |
|     MODES                 >                  |                                     |   | 264  unfocused: F_H2 bone, shadow; chevron only when focused
|     SETTINGS                                 |  [GL 32]  SOLO WITH BOTS            |   | 328
|     HOW TO PLAY                              |  YOU PLUS TWO MEMBERS.              |   | 392
|     CREDITS                                  |  ONE OF THEM MOSTLY TALKS.          |   | 456
|                                              |  BOTS: BUILDER, TALKER              |   |
|                                              |  CLOCK: NORMAL 3:00                 |   |
|                                              |  SCORE: CLOUT AND KARMA             |   |
|                                              +------------------------------------+   | 520
|                                              | AT THE DOOR                         |   | 536  who_is_in (700,536) 540x96
|                                              | [#][GL] P2 PAD 1   [#][GL] P3 WASD  |   |      chips 128x20 at (712 + k*132, 576 + (k/4)*24)
|                                              +------------------------------------+   | 632
|          ONE AT A TIME. THIS IS A DO-OCRACY.                                           | 630  deny line, F_MONO ink-err centred on x 356, 2 s, 6 px shake
|   [A] SELECT   [B] BACK   [<] [>] CHANGE                              ## P1  PAD 1 ##  | 672  footer keycaps from x 96; owner chip right aligned to 1184
+----------------------------------------------------------------------------------------+
```

Rows: `MENU_ROWS[] = { "PLAY", "MODES", "SETTINGS", "HOW TO PLAY", "CREDITS" }`. Row rect `(96, 200 + i*64, 520, 56)`. Label `hud_text_sh(F_H2, 120, y + 6, ...)`. The PLAY row carries a value: the mode short name (`MODES[SET.mode].short_name`: SOLO, CO-OP, VERSUS, CHAOS, CUSTOM) right aligned at x 576 in F_MONO_L with 14 px chevrons either side (`hud_chevron` left at x 456 and right at x 596, y centre); LEFT and RIGHT on PLAY step the mode with the value sliding 12 px in the direction and snapping back over 0.18 s (ease back) and SFX_MENU_MOVE. Stepping onto CUSTOM here does not open settings; it just selects the mode.

Cursor: `M.cur_y` tweens to the focused row's y with `hud_tween(&M.cur_y, target, 18, dt)`; the hazard block is drawn at `M.cur_y` plus a 3 px overshoot in the move direction scaled by `1 - hud_ease_back(min(1, M.snap_t / 0.22))` where `M.snap_t` resets to 0 on each move. The focused row text is `C_TAPE`; because the block moves ahead of the focus change, the text colour swaps when the block covers 50 percent of the row.

Preview panel (700,200) 540x320 per focused row:
- PLAY: the mode card content for `SET.mode` (glyph at scale 2 at (724, 224), name F_H3 at (772, 232), blurb F_MONO wrapped at 480 px from (724, 280) lh 26, three rule lines F_MONO_S from (724, 430) lh 22; the CLOCK rule is formatted with `NIGHT_LENS[SET.night_len].name`). If the mode is VERSUS, the seated count (owner plus pending) is 1 and `SET.bots == 0`, a fourth line NEEDS 2 PLAYERS OR BOTS in `C_INK_ERR` at (724, 496). PLAY still works; the lobby adds people.
- MODES: five 92x120 mini cards in a row at y 300 showing glyph and short name, the current one framed hazard.
- SETTINGS: four summary lines F_MONO at (724, 240) lh 30: "NIGHT  NORMAL 3:00", "BOTS ON   CHAOS NORMAL   PHONES ON", "ROOM  108 W MAIN", "MUSIC 7   SFX 8".
- HOW TO PLAY: flash card 1 picture (6.6) animating in a loop inside the panel at 0.78 scale (origin (740, 230), a 2.6 s local clock).
- CREDITS: `hud_logo(880, 280, 160, C_BONE)` and the eyebrow SINCE 2009 under it.

AT THE DOOR panel `who_is_in()`: drawn only when any seat is pending. `hud_panel(700,536,540,96,C_SLAB,C_STEEL_HI)`, eyebrow AT THE DOOR at (712,548). One chip per pending seat in seat id order: chip k at `(712 + (k % 4) * 132, 576 + (k / 4) * 24)`, 128x20: a 12x20 block in `PCOL[(k + 1) % 8]` (the colour that seat will get as P(k+2) if nobody leaves), the seat glyph at scale 1 (GL_PAD, GL_KEYS, GL_PHONE) at x+16, then "P2  PAD 1" in F_MONO_S bone from x+36, clipped with `hud_clip` to the chip. Up to 8 chips in two rows. A chip arrives with the 0.25 s ease back drop used by lobby cards.

Footer: `hud_keycap(96, 672, glyph_a of the owner, C_HAZARD, C_TAPE)` then SELECT F_MONO_S bone at +8; `[B] BACK`; two chevrons and CHANGE. Owner chip: `hud_rect` in `PCOL[0]` with "P1  PAD 1" (the seat name) in F_MONO_S `C_TAPE`, right edge at 1184. Phone owners show their phone name.

Select (A on a row): SFX_MENU_OK; the row flashes `C_BONE` for 80 ms; `fx2_burst(FX2_SPARK, 616, y+28, C_HAZARD)`; then:
- PLAY: `front_end_play()` (section 8.3).
- MODES: SCR_MODES. SETTINGS: SCR_SETTINGS with `M.settings_then = TO_MAIN`. HOW TO PLAY: SCR_FLASH with `M.flash_return = SCR_MAIN`. CREDITS: SCR_CREDITS.
B: release owner, SFX_MENU_BACK, screen to SCR_TITLE (rows slide out left, band fades back in: the 6.1 transition reversed over 0.35 s). Pending seats stay pending.

Deny (3.4): `M.deny_t = 2`, the line in F_MONO `C_INK_ERR` centred on x 356 at y 630, SFX_MENU_DENY, and the menu column shakes `6 * sinf(M.deny_t * 60) * max(0, M.deny_t - 1.8) / 0.2` px horizontally for the first 0.2 s.

### 6.4 SCR_MODES

```
+========================================================================================+
| [MARK 64] DO-OCRACY           MODES                                                    | F_H2 at (108,52); eyebrow MODES at (108,100)
|                                                                                        |
|   +--------+  +--------+  +========+  +--------+  +--------+                           | cards w 224 h 400 at y 180, gap 16, x0 48
|   |########|  |########|  |########|  |########|  |########|  <- 8 px colour band      | focused card: y 160 h 440, 10 px shadow, 3 px hazard frame
|   | [GL]   |  | [GL]   |  | [GL]   |  | [GL]   |  | [GL]   |                           |
|   | SOLO   |  | CO-OP  |  | VERSUS |  | CHAOS  |  | CUSTOM |  <- F_H3 name, 2 lines   |
|   | WITH   |  | VS THE |  | CLOUT  |  | NIGHT  |  | YOUR   |                           |
|   | BOTS   |  | CLOCK  |  | RACE   |  |        |  | RULES  |                           |
|   | 1 PLYR |  | 1 TO 8 |  | 2 TO 8 |  | 1 TO 8 |  | 1 TO 8 |  <- F_MONO_S players     |
|   |        |  |        |  |        |  |        |  |        |                           |
|   | blurb  |  | blurb  |  | blurb  |  | blurb  |  | blurb  |  <- F_MONO_S wrap 3 lines |
|   |        |  |        |  |        |  |        |  |        |                           |
|   | rule 1 |  | rule 1 |  | rule 1 |  | rule 1 |  | rule 1 |  <- 3 rule lines, glyphs  |
|   | rule 2 |  | rule 2 |  | rule 2 |  | rule 2 |  | rule 2 |                           |
|   | rule 3 |  | rule 3 |  | rule 3 |  | rule 3 |  | rule 3 |                           |
|   +--------+  +--------+  +========+  +--------+  +--------+                           | 580 (620 for the lifted card)
|                                                                                        |
|   [A] PICK   [B] BACK   [<] [>] LOOK                               ## P1  PAD 1 ##     | 660
+----------------------------------------------------------------------------------------+
```

Card geometry: `x = 48 + i*240`, unfocused `(x, 180, 224, 400)`, focused `(x, 160, 224, 440)` with `hud_rect` shadow offset 10 and `hud_frame` 3 px `C_HAZARD`; unfocused frame 2 px `C_STEEL_HI`. The lift is tweened: each card keeps `M.lift[i]` 0..1 tweened at speed 16; y, h and shadow interpolate on it. Content positions relative to the card top: band `(x, y, 224, 8)` in the mode colour; glyph at `(x+16, y+24)` scale 2 in the mode colour; name F_H3 at `(x+16, y+66)` and `(x+16, y+96)` for the second line; players line F_MONO_S at `(x+16, y+134)` ash, or `C_INK_ERR` when the seated count (owner plus pending) cannot satisfy it (VERSUS with one seat and `SET.bots == 0`); blurb `hud_text_wrap(F_MONO_S, x+16, y+164, 192, blurb, C_BONE, 255, 20)`; rules from `y+300` lh 24 in F_MONO_S with a 16 px glyph at `x+16` and text at `x+40`. The CLOCK rule substitutes `NIGHT_LENS[SET.night_len].name` for `%s`.

Mode table: section 8.6. Blurbs are at most 50 chars (three F_MONO_S lines at 192 px is about 60). Rules at most 22 chars.

Input: LEFT and RIGHT move `M.col` with wrap, SFX_MENU_MOVE, snap. A: `SET.mode = M.col`, `save_commit()`, SFX_MENU_OK, the card flashes bone 80 ms and `fx2_burst(FX2_RING, cx, cy, color)`, then SCR_MAIN with PLAY focused; except CUSTOM, which after the same pick goes to SCR_SETTINGS with `M.settings_then = TO_LOBBY`. START: pick and go straight to PLAY (CUSTOM: same as A). B: SCR_MAIN without change.

### 6.5 SCR_SETTINGS

```
+========================================================================================+
| [MARK 64] DO-OCRACY           SETTINGS                                                 |
|                                                                                        |
|  +--------------------------------------------------+   +---------------------------+ | panel (96,140) 720x500; right panel (840,140) 400x500
|  | NIGHT LENGTH              < NORMAL 3:00 >        | 176 |  N I G H T  L E N G T H   | | rows pitch 44 from y 176, label x 120, value right at 776
|  | BOTS                           < ON >            | 220 |                           | |
|  | CHAOS                        < NORMAL >          | 264 |  HOW LONG UNTIL 11 PM.    | | settings_help: eyebrow at (864,176), F_MONO wrap 352
|  | PHONES                         < ON >            | 308 |  THE DISHES DO NOT CARE.  | |   at (864,210) lh 26
|  | MUSIC                 #######... 7               | 352 |                           | |
|  | SFX                   ########.. 8               | 396 |  (ROOM row focused and    | |
|  | ROOM                     108 W MAIN              | 440 |   layout_count > 1: the   | |
|  | HELP                    < FIRST NIGHT >          | 484 |   top down map instead)   | |
|  | RESET TO DEFAULTS                                | 528 |                           | |
|  | DONE                                             | 572 +---------------------------+ |
|  +--------------------------------------------------+                                   |
|   [<] [>] CHANGE   [A] PICK   [B] BACK                                 ## P1  PAD 1 ##| 660
+----------------------------------------------------------------------------------------+
```

Ten rows, `(120, 176 + i*44, 672, 40)`. Focused row: hazard block `(104, y-2, 704, 44)` with 4 px shadow, label and value in `C_TAPE`; unfocused: label F_MONO bone, value F_MONO ash. Values right aligned at x 776 with chevrons at 740 and 788 when the row has choices (hidden at the ends of non wrapping lists; drawn `C_SMOKE` when the list has one entry). Volume rows draw ten 24x14 ticks from x 540 (pitch 26) filled `C_LIVE` up to the value, `C_STEEL` beyond, and the number at 776.

Row behaviour (LEFT and RIGHT, A steps right):
- NIGHT LENGTH: `NIGHT_LENS[]`, no wrap.
- BOTS, PHONES: toggle.
- CHAOS: `CHAOS_NAMES[]` CALM, NORMAL, CHAOS, no wrap.
- MUSIC, SFX: 0..10, no wrap; each change calls `audio_set_levels` and plays SFX_MENU_MOVE so the change is audible. Changing MUSIC also moves the music level at once (the band keeps playing).
- ROOM: index into `LAYOUTS[]` by `layout_count`. Until the layout workstream lands, `layout_count` is 1, the value reads `ROOM_NAMES[0]` = "108 W MAIN", the chevrons are `C_SMOKE`, and LEFT and RIGHT do nothing. When `layout_count > 1`, LEFT and RIGHT step and the right panel shows the top down map: scale 14.3 px per unit, `px = 864 + (x + 12.3) * 14.3`, `py = 200 + (z + 7.15) * 14.3` so the back wall is at the top, stations as 2 px frames in their pay colour, obstacles as `C_STEEL` fills, the door as a bone gap, the name F_H3 at (864,420) and blurb F_MONO_S wrapped at 352 from (864,452).
- HELP: `HELP_NAMES[]` OFF, FIRST NIGHT, ALWAYS, no wrap.
- RESET TO DEFAULTS: A resets every setting (not records), SFX_MENU_BACK, every value slides in from the right 12 px.
- DONE: A or START leaves to `M.settings_then`: TO_MAIN returns to SCR_MAIN with SETTINGS focused; TO_LOBBY calls `front_end_play()`.
- Pinned rows (section 4 table) are drawn in `C_SMOKE` with the value and a small "(MODE)" suffix; LEFT and RIGHT show the deny line "PINNED BY THE MODE. PICK CUSTOM." without changing anything.

Every change: `save_commit()`, value slide 12 px and snap (0.18 s ease back), SFX_MENU_MOVE. UP and DOWN: cursor with the same tween as SCR_MAIN. B: leave to the `settings_then` target (TO_MAIN: SCR_MAIN; TO_LOBBY: SCR_MODES).

A NIGHT script row is not drawn until `NIGHTS[]` exists (the layout workstream owns night scripts); when it does, it is inserted after ROOM as the eleventh row and the panel grows to 544 tall.

`settings_help(row)`: the right panel when no map is shown. Eyebrow with the row label at (864,176) ash; one line wrapped at 352 px in F_MONO bone from (864,210), lh 26:

| Row | Line |
|---|---|
| NIGHT LENGTH | HOW LONG UNTIL 11 PM. THE DISHES DO NOT CARE. |
| BOTS | MEMBERS FILL EMPTY SEATS. ONE OF THEM TALKS. |
| CHAOS | HOW OFTEN THINGS GO WRONG. CALM IS SUSPICIOUS. |
| PHONES | SCAN THE CODE TO JOIN. NO APP. NO ACCOUNT. |
| MUSIC | THE BAND PLAYS LIVE. IT CANNOT BE FIRED. |
| SFX | BEEPS. THE GOOD KIND. |
| ROOM | 108 W MAIN. THE ONLY ROOM. FOR NOW. |
| HELP | THE RULES FLASH AND THE COACH. FIRST NIGHT IS THE DEFAULT. |
| RESET TO DEFAULTS | BACK TO HOW IT WAS. LIKE THE WOOD SHOP. |
| DONE | BACK TO THE MENU. (TO_MAIN) or OFF TO THE LOBBY. (TO_LOBBY) |

### 6.6 SCR_FLASH: the rules flash

Five cards, each one picture and one caption of at most six words, auto advancing every 2.6 s, A advances, B exits. Drawn over whatever is behind (the title room, the lobby, or a paused night) on a 120 alpha scrim.

```
+========================================================================================+
|                                                                                        |
|            +==================================================================+        | 110  card (260,110) 760x440, hud_panel C_SLAB / C_STEEL_HI
|            |==================== caution tape 24 px ===========================|        |      tape inside the card top, phase T*30
|            |                                                                  |        |
|            |   picture area (300,150) 680x300, clipped                        |        |
|            |                                                                  |        |
|            |                                                                  |        |
|            |                                                                  |        |
|            |                                                                  |        |
|            |                   HOLD A. DO THE THING.                          |        | 482  caption runs F_H2 centred at x 640
|            |                      o  o  [====]  o  o                          |        | 528  hud_steps(640, 528, 5, step, frac)
|            +==================================================================+        | 550
|                            [A] NEXT     [B] SKIP                                       | 580  keycaps centred; last card reads [A] DONE
+----------------------------------------------------------------------------------------+
```

State in `M`: `flash_step 0..4`, `flash_t` seconds in the card, `flash_wipe` 0..0.3 during a wipe, `flash_fired` (the card's one-shot beat has fired), `flash_return` screen or SCR_NONE (back to lobby or paused night).

Captions as colour runs (`FLASH_CAPS[5]` in content.h, drawn with `hud_text_runs(F_H2, 640, 482, FLASH_CAPS[step], a, AL_CENTER)`):

| Card | Runs | Words |
|---|---|---|
| 1 | "HOLD A. " `C_HAZARD`, "DO THE THING." `C_BONE` | 5 |
| 2 | "FOLLOW THE " `C_BONE`, "ARROW." `C_HAZARD` | 3 |
| 3 | "AMBER" `C_HAZARD`, " IS YOURS. " `C_BONE`, "TEAL" `C_LIVE`, " IS OURS." `C_BONE` | 6 |
| 4 | "CLUTTER" `C_RUST`, " 100: " `C_BONE`, "FIRE MARSHAL." `C_FAULT` | 4 |
| 5 | "SAY HI" `C_LIVE`, " OR THEY LEAVE." `C_BONE` | 5 |

Card pictures. Coordinates relative to the picture area origin (300, 150); `lt` is `flash_t`; `L(a,b)` means `clamp((lt-a)/(b-a), 0, 1)`; the owner's glyph is `glyph_a` of the owner seat (or the pauser) or "A". `hud_clip(300,150,680,300)` wraps every picture so slides never leave the card.

1. HOLD A. DO THE THING.
   - `hud_pict_station(240, 20, 200, 100, "LASER", C_HAZARD)`; `hud_pict_person(340, 150, 72, PCOL[0], false)`.
   - Prompt box as in play at `(240, 236, 200, 32)`: char box, 2 px hazard frame, `hud_keycap(246, 240, glyph)`, "CUT" F_MONO hazard. Fill: `done = L(0.6, 2.0)`; `hud_rect(240, 236, 200*done, 32, C_HAZARD, 90)` and the 6 px bottom bar, exactly the `players_overlay` look (`draw.c` 342 to 363).
   - At `lt >= 2.0`: pop "+1040 CLOUT" F_H2 hazard rising 40 px over 0.5 s above the station and `fx2_burst(FX2_SPARK, 640, 220, C_HAZARD)` once (`flash_fired`), SFX_GLAMOUR.
   - Keycap pulses (fill `C_BONE` for 60 ms every 0.5 s) while `done < 1`, so the player sees "held".

2. FOLLOW THE ARROW.
   - `hud_pict_station(480, 40, 160, 100, "SHOW & TELL", C_HAZARD)` with a pulsing 5 px outline in `PCOL[0]` (`0.5+0.5*sinf(T*6)` alpha 160..240), the `goal_guides` look.
   - Person at `x = 80 + 320 * hud_ease_inout(L(0.4, 1.8))`, y 150, carrying `hud_pict_item(x+20, 120, 1, C_HAZARD)` above the head.
   - Chevrons: from the person to x 470 every 44 px, phase `fmodf(T*2.5,1)*44`, `hud_chevron(cx, 190, 1, 0, 22, 6, PCOL[0], 230)`, fading in over the first 40 px.
   - At `lt >= 1.85` the item snaps into the box (drawn at (540, 60)), `fx2_burst(FX2_CONFETTI, 560, 60, C_HAZARD)` once, SFX_GLAMOUR.

3. AMBER IS YOURS. TEAL IS OURS.
   - Left half (0..330): 8 px rail `C_HAZARD` at x 20 from y 20 to 260; "CLOUT" F_H1 hazard at (44, 20); `hud_glyph(44, 100, GL_CLOUT, 3, C_HAZARD, 255)`; `hud_pict_item(120, 110, 1, C_HAZARD)`; "+900" F_MONO_L hazard at (44, 170) counting `(int)(900 * L(0.3, 1.5))`; FOR YOU F_MONO_S ash at (44, 220).
   - Right half (350..680): rail `C_LIVE` at x 350; "KARMA" F_H1 live at (374, 20); `hud_glyph(374, 100, GL_KARMA, 3, C_LIVE, 255)`; `hud_glyph(440, 100, GL_BROOM, 3, C_LIVE, 255)`; "+40" counting the same way; FOR THE SPACE F_MONO_S ash at (374, 220).

4. CLUTTER 100: FIRE MARSHAL.
   - `hud_eyebrow(40, 60, "CLUTTER", C_BONE, AL_LEFT)`; meter `hud_bar(40, 90, 600, 40, v/100, fill, C_CHAR)` with `v = 100 * L(0.3, 1.9)`, fill hazard, rust above 55, fault above 80 (the `top_bar` rule, `draw.c` 496).
   - Floor line at y 220: as `v` passes 15, 35, 55, 75 a `hud_pict_item` appears (scrap, scrap, daisy, scrap) at x 60, 200, 340, 520, each dropping in from y 200 over 0.15 s.
   - At `v >= 100`: `hud_glyph(600, 60, GL_MARSHAL, 4, C_FAULT, 255)` stamps in (scale 5 for the first 0.1 s then 4), the card gets a 12 px `C_FAULT` frame at 2 Hz for 0.6 s (the fire flash look, `draw.c` 503), SFX_ALARM once. At `lt >= 2.3` the whole picture dims to alpha 120: the lights went out.

5. SAY HI OR THEY LEAVE.
   - Newbie `hud_pict_person(480, 150, 72, 0, true)` with the play overlay (`draw.c` 277 to 281): `hud_circle(504, 120, 20, C_CHAR, 235)`, `hud_ring(504, 120, 20, 5, frac, frac < 0.35 ? C_FAULT : C_LIVE, 255)` where `frac = 1 - 0.7 * L(0, 1.6)`, "?" F_H3 bone.
   - Player `hud_pict_person(x, 150, 72, PCOL[0], false)` with `x = 120 + 300 * hud_ease_inout(L(0.4, 1.6))`.
   - At `lt >= 1.7`: `hud_glyph(440, 90, GL_HAND, 3, PCOL[0], 255)` waves (x wobble 4 px at 8 Hz for 0.5 s), the ring fills to 1 in `C_BONE`, "?" is replaced by `GL_CHECK` scale 2 in `C_LIVE`, pop "+100 KARMA" F_H2 live rising, `fx2_burst(FX2_RING, 504, 120, C_LIVE)` once, SFX_KARMA.

Wipe between cards (0.3 s): the outgoing picture and caption slide left 60 px and fade over 0.15 s, the incoming slide in from +60 px over 0.15 s; the card frame, tape and steps stay still; SFX_CARD at the start of the wipe. Step dots update at the midpoint.

Input: A (owner, or the pauser when over a paused night): skip to the next card (starts the wipe at once; on the last card ends the flash). B: end the flash. Ending: the card slides down 40 px and fades over 0.25 s, SFX_MENU_BACK; `M.screen = M.flash_return`; `G.flash_seen = true`.

Where it runs: from SCR_MAIN (return SCR_MAIN); when the lobby opens with HELP ALWAYS, or HELP FIRST NIGHT and `!G.flash_seen` (return SCR_NONE; `lobby_t` does not advance while `M.screen == SCR_FLASH`); from the pause overlay HOW TO PLAY (return SCR_NONE with `G.paused` still true, the pause menu redraws). During the lobby flash other seats may still join with A (their input goes through `seats_join()`, only the owner's goes to the flash).

### 6.7 SCR_CREDITS

```
+========================================================================================+
| [MARK 64] DO-OCRACY           CREDITS                                                  |
|                                                                                        |
|                                [ MARK 200 ]  (540,150)                                 |
|                                                                                        |
|                  A GAME ABOUT A HACKERSPACE, MADE AT ONE.                              | 380 F_H3 bone centred
|                  HEATSYNC LABS. 108 W MAIN ST, MESA. SINCE 2009.                       | 420 F_MONO ash, lh 30
|                  ENGINE: THREE.C ON WASMCART.  NOISES: BELLOWS.                        | 450
|                  FONTS: ARCHIVO BLACK, IBM PLEX MONO.                                  | 480
|                  THE FLOOR LOOP HAS NEVER EXITED.                                      | 510 F_MONO hazard
|                                                                                        |
|   +--------------------------------------------------------------------------------+   | 560 record strip (96,560) 1088x56, C_SLAB
|   | LAB RECORDS   KARMA 1,240 PAD 1   CLOUT 9,400 WASD   TEAM 3,120   NIGHTS 41   |   |    F_MONO_S, from SAVE
|   +--------------------------------------------------------------------------------+   |
|   [B] BACK                                                                             | 660
+----------------------------------------------------------------------------------------+
```

Lines come from `CREDITS[]` in content.h. Roles only; no personal names ever appear here. A, B or START returns to SCR_MAIN.

### 6.8 Lobby (PH_LOBBY)

The room is live, players walk and try things as today. The lobby overlay replaces the how_to and controls cards with big seat cards along the bottom band. The camera is the play camera (not the orbit), reached by the 0.8 s tween started on PLAY.

```
+----------------------------------------------------------------------------------------+
| [M] 7:00 PM     DOORS OPEN  | LOBBY   SOLO WITH BOTS      NORMAL 3:00  | [QR] KQRT    | 6..62  clock panel (16,6) 300x56; lobby panel (330,6) 600x56; join strip (944,6) 320x56
|  DOORS OPEN IN 0:18 |=========================== caution tape shrinking from the right ====| 72..96  chip (16,72) 180x24 then tape to 1264
|  [A] TRY THINGS  [B] DROP  [X] PROPOSE  [START] READY                  A NEWBIE WALKED IN  | 108  hint strip at (16,108); toasts right aligned at 1264, y 108 and 132
|                                                                                        |
|                 (players at x -6.5..-3.8, z 1.6: screen (378..487, 452), walking)        |
|                                                                                        |
|                              WAITING ON ROBOT MOM                                      | 540 F_MONO ash centred (status; not a deny)
|   +--------------+    +--------------+    +--------------+    +--------------+         | 580  4 cards 280x120, gap 16, x0 56, pitch 296
|   |##############|    |##############|    |##############|    |##############|         |      8 px colour band (empty: C_STEEL)
|   | [PAD] P1     |    | [PHONE] P2   |    | [BOT]        |    | [QR 96] PHONES|        |      glyph scale 2 at (x+16,y+20); name F_H3 at (x+56,y+22)
|   | PAD 1        |    | ROBOT MOM    |    | BUILDER      |    |         KQRT  |        |      QR card: QR at (x+12,y+12), eyebrow (x+120,y+24), code F_H2 (x+120,y+44)
|   | [v] READY    |    | PRESS START  |    | BOT          |    |               |        | y+88 state line F_MONO; READY live with GL_CHECK; PRESS START ash
|   +--------------+    +--------------+    +--------------+    +--------------+         | 700
+----------------------------------------------------------------------------------------+
```

- Top bar: the clock panel shows 7:00 PM F_H2 and DOORS OPEN F_MONO_S ash right aligned at 306 (replaces CLOCKING IN at `draw.c` 492). The meters panel is replaced in the lobby by the lobby panel: eyebrow LOBBY at (346, 12), mode name F_H3 at (346, 30) (SOLO WITH BOTS, CO-OP VS THE CLOCK, VERSUS CLOUT RACE, CHAOS NIGHT, CUSTOM YOUR RULES), `NIGHT_LENS[].name` F_MONO_S ash right aligned at 914, y 36. Join strip at (944, 6) in its wide 320x56 form when phones are on and online.
- Countdown: `left = lobby_t`. Chip `hud_rect(16, 72, 180, 24, C_CHAR, 255)` with "DOORS OPEN IN 0:18" F_MONO_S bone; tape `hud_tape(196, 72, (int)(1068 * left / 25), 24, T*30)`. While the flash is up or any seat is mid hold-B the tape holds still. When every human is READY the tape is replaced by the 3 s countdown: F_TITLE digits 3, 2, 1 centred at (640, 280) each dropping 20 px and fading in over 0.2 s with SFX_COUNTDOWN, then GO with SFX_GO and the night start wipe.
- Hint strip at (16, 108): `hud_keycap` with the owner's glyph then TRY THINGS, `[B] DROP`, `[X] PROPOSE`, `[START] READY`, F_MONO_S bone. TRY THINGS says that nothing counts yet without a sentence. The strip ends well before x 850 so it never meets a toast.
- Cards: `n = max(4, player_count + bots_to_add + 1)`; when `n <= 4` four cards 280x120 at y 580 with x0 56 and pitch 296; when `n > 4` eight cards 140x120 at y 580, x0 52, pitch 148 (glyph scale 1 at (x+8,y+16), name F_MONO_S at (x+30,y+18), state F_MONO_S at y+88). Order: P1 first (the owner), then by slot. Bots that will be added at night start (section 4 table) are previewed as cards at alpha 180 with GL_BOT and the state BOT. The first empty card holds the QR and code when phones are on and online; further empty cards say PRESS A pulsing (the CTA pulse) with the keycap.
- Placement: in PH_LOBBY `draw_hud` reserves `(0, 570, 1280, 150)` instead of the night's bottom 86, so station name tags (Show & Tell at (722, 585), the Free Pile at (1124, 574)) settle upward and never sit under a card.
- Join: the new card drops in from y 540 over 0.25 s ease back, SFX_JOIN; the world FX_JOIN burst already fires (`game.c` 261). `lobby_t` resets to 25.
- READY: START toggles `P[i].ready` (new field); the card frame becomes 3 px `C_LIVE`, `GL_CHECK` stamps in (scale 3 for 0.1 s, then 2), SFX_READY. All humans ready and at least one human: the 3 s countdown begins; any un-ready cancels it. Owner START while others are not ready shows the status line WAITING ON %s (the first un-ready name) for 2 s at y 540. `lobby_t` runs out: the night starts regardless (kept behaviour, now visible as the tape).
- Leave: a seated seat holds B; `seat.hold_b += dt`; a `C_RUST` `hud_ring(x+w-24, y+100, 12, 3, hold_b/1.5, C_RUST, 255)` fills in the card's lower right corner; release cancels (`hold_b = 0`); at 1.5 s `game_remove_player(slot)`, toast "%s WENT HOME." in `C_ASH`, the card drops 20 px and fades, SFX_MENU_BACK. If the owner leaves, ownership passes to the lowest seated seat. `lobby_t` holds while any `hold_b > 0`. Special case: when the owner is the only seated human (bots previews do not count), the ring fills over 1.0 s with the word MENU in F_MONO_S under it, and completion calls `front_end_back_to_menu()`: remove the owner's player, `G.phase = PH_TITLE; G.t = 0`, `M.screen = SCR_MAIN`, the owner keeps the seat, the camera tweens back to orbit. So a solo player can change mode without emptying the lobby for 6 s.
- The night style player cards strip is not drawn in the lobby. Toasts: two slots right aligned at 1264, y 108 and 132 (6.9).

Night start wipe (0.6 s): two 24 px tape bars at y 0 and y 696 grow to 360 px tall toward the middle over 0.3 s (`hud_rect` `C_TAPE` with a `hud_tape` strip at the leading edge), hold one frame, then open over 0.3 s; `game_start_night()` fires at the closed frame; SFX_ALARM as today (`game.c` 967). The wipe is driven by `G.wipe_t`, set to 0.6 by main.c and counted down there.

### 6.9 Night HUD: stays, goes, on demand

```
+----------------------------------------------------------------------------------------+
| [M] 2:41     8:12 PM | CLUTTER  [bar.........] 64 [H]  VIBE [bar......] 71 [<3] | CO-OP TEAM KARMA 1,240 |[QR]| 6..62
|  |====hour ticks====|  |                                                       |                        |    |
|                 [1 CLOUT]  GRAB PLYWOOD AT THE RACK                        A NEWBIE WALKED IN. SAY HI.| 68 slot bar; toast 1 at y 108
|                                                                                 DONATED: A CRT          | 132 toast 2
|            (room: goal chevrons, alarm badges, station names at 150, prompts)          |
|                                                                                        |
|   +----------+ +----------+ +----------+                                               | 658 player cards, centred, 210x50
|   |# PAD 1 [p]| |# WASD [k]| |# BUILDER [b]|                                           |     name F_MONO_S; seat glyph top right
|   |# * 1040  + 320| ...                                                                |     GL_CLOUT then clout, GL_KARMA then karma, F_MONO_L
+----------------------------------------------------------------------------------------+
```

Clock panel (16,6) 300x56: mark 50 at (20,9); countdown "2:41" F_H2 bone at (74,10) (promoted; blinks to hazard at 1.25 Hz in the last 30 s as today); the clock "8:12 PM" F_MONO_S ash right aligned at 306, y 12 (demoted); night bar (76,54) 232x5 with hour ticks kept (`draw.c` 487 to 491). The word LEFT is gone.

Meters panel (330,6) 600x56: CLUTTER eyebrow at (346,12) bone, value F_MONO_S right aligned at 596; `hud_glyph(604, 10, GL_MARSHAL, 1, c, a)` where `c` is `C_SMOKE`, `C_RUST` above 55, `C_FAULT` pulsing (alpha 150..255 at 4 Hz) above 80; bar (346,32) 270x16 with the existing fill rule. "FIRE MARSHAL IN N" stays as the value text in `C_FAULT` when `fire_warn > 0`; it is a countdown, not a rule. VIBE eyebrow at (640,12), value right at 898, `hud_glyph(906, 10, GL_HEART, 1, vibe < 25 ? C_FAULT : C_LIVE, 255)`, bar (640,32) 274x16. The two rule strings "100 = FIRE MARSHAL" and "0 = BURNOUT" are deleted; `meter()` (`draw.c` 464) loses the `rule` argument and gains a glyph id.

Right slot (944,6) 320x56, by mode:
- CO-OP: `mode_panel` 196x56 at (944,6): eyebrow TEAM KARMA and the total F_H2 live. VERSUS: eyebrow LEADER and the leading name plus clout in F_H3 hazard. In these two modes the join strip is always its narrow 120x56 form at (1144,6).
- SOLO, CHAOS, CUSTOM: the join strip alone. It starts wide: `hud_panel(944,6,320,56)`, QR 48 at (949,10), eyebrow PHONES JOIN at (1006,12), code F_H3 hazard at (1004,30). At `G.t >= 45` its x and width tween over 0.3 s (`hud_ease_inout`) to the narrow form `hud_panel(1144,6,120,56)`: QR 48 at (1150,10), code F_MONO_S hazard at (1204,16), eyebrow JOIN at (1204,38). The text swaps at the midpoint. "ANY TIME" is deleted. The strip is hidden entirely when `SET.phones == 0` or `!net_online()`; the "PHONES OFFLINE / GRAB A CONTROLLER." variant is deleted.

Slot bar at y 68 keeps the chain danger > ticker > coach (`draw.c` 773). Danger strings shorten to CLUTTER! with CLEAN. THE MARSHAL IS COUNTING. and VIBES! with SAY HI. CLEAN UP. Coach gating per section 4 (HELP). Steps of 8 s from `COACH_STEPS[]` (8.6); steps 1 to 3 set `G.coach_st` to ST_LUMBER, ST_LASER, ST_SHOW and `goal_guides` draws its chevrons in `C_ASH` at alpha 150 from every human without `has_goal` toward that station; steps 4 and 5 set `G.coach_st = -1`.

Toasts: `toasts(1264, 108)`: two slots, slot i right aligned at x 1264, y 108 + i*24; pill `hud_rect(1264 - w - 18, y, w + 18, 21, C_CHAR, a*210/255)`, 4 px colour edge on the pill's left, text F_MONO_S bone. Drawn for only two slots while `toast_t > 2` with the same 1 s fade in. Before world labels `draw_hud` calls `reserve(960, 104, 304, 52)` and, if the widest live toast is wider than 286 px, `reserve(1264 - w - 18, 104, w + 18, 52)` too. Toast strings: 44 chars or fewer for every new line, 48 is the hard cap (the drawer cuts at 48 with no ellipsis); section 8.6 lists the trimmed set. The "%s SHOWED OFF: %s" toast becomes "%s: %s" so a 13 char name plus a 32 char project fits.

| Element | Today (`draw.c`) | Decision |
|---|---|---|
| station name tags (11) | always alpha 240 (231) | stays; alpha 150 unless alarm, ready, or a human within 3 units of the footprint |
| station status lines (18) | alarm, ready or near 2.2 (216 to 225) | alarm and ready only, plus near 1.4; the nine idle strings PLYWOOD FOR THE LASER, ALWAYS FREE. STICKERS, CLEAN. FOR NOW, JUNK GOES HERE, DONATIONS GO HERE, CLUB-MATE: SPEED BOOST, BRING PROJECTS HERE, START A PRINT, HOLD TO SOLDER are deleted; the prompt under the feet already names the action |
| "!" alarm badge (235 to 240) | | stays |
| goal guides (290 to 319) | humans only | stays; bots too in attract; coach target in ash |
| floor mess ring (252 to 254) | | stays |
| SWEEP ME / MOP ME / UNPLUG ME (255) | always | only on group night 1; afterwards `hud_glyph(GL_BROOM)` scale 1 in the ring colour at the ring top. UNPLUG ME stays always: the daisy chain is a safety line |
| project names on the floor (257) | | stays, hazard |
| donation name plus TO FREE PILE (259, 260) | two tags | name only |
| TRASH ME (262) | | deleted; the goal line and chevrons take over at pickup |
| RED TAG (264 to 267) | a second block under the item | replaces the item's label: when `red`, `label_av(sx, sy - 20, "RED TAG", C_BONE, C_FAULT, F_MONO_S)` is drawn instead of the name, SWEEP ME or broom glyph; the mess ring stays. One label per item |
| newbie ring and "?" (277 to 280) | | stays |
| NEWBIE: SAY HI / LEAVING! SAY HI (281) | always | SAY HI only under 35 percent patience, `C_INK_ERR` |
| MEMBER (283) | | deleted; the body colour change carries it |
| human name tags (328 to 330) | | stays |
| bot name tags | full tag | 14 px tag at alpha 170 |
| goal line, held item name (332 to 339) | | stays |
| prompt box with keycap and fill (342 to 363) | | stays, the one place a button name appears in play |
| pops (364 to 369) | | stays |
| proposal bubbles (370 to 378) | unlimited | one on screen at a time (the newest `bubble` wins) |
| player cards (507 to 530) | name, source tag, two numbers, two eyebrows | name; `GL_PAD / GL_KEYS / GL_PHONE / GL_BOT` glyph at scale 1 at (x+cw-24, y+6) in `C_SMOKE` instead of the text tag; `GL_CLOUT` scale 1 at (x+14, y+22) then the clout number at x+34 in F_MONO_L hazard; `GL_KARMA` scale 1 then the karma number right aligned at x+cw-10 in `C_INK_OK`; the floating CLOUT and KARMA eyebrows (528, 529) are deleted. In CO-OP the clout number is dimmed `C_SMOKE` |
| toasts (422 to 432) | 3 slots at x 16, y 620 | 2 slots, right aligned at 1264, y 108 and 132, with the reserve |
| join strip (408 to 419) | always, 320 wide | wide 45 s then narrow, or narrow always in CO-OP and VERSUS; hidden when phones off or offline |
| heat wash, fire frame (503, 504) | | stays |
| controls card, how_to board (578 to 623) | lobby and title | deleted; the pause overlay holds both |
| join_panel (384 to 406) | title and lobby | deleted; the title phone module and the lobby QR card replace it |

### 6.10 Pause overlay

START by any human during PH_NIGHT (not in attract) sets `G.paused = true` and records `M.pauser`. `game_update` is not called while paused (`wc_render`), `world_sync` and the HUD still run so the room stays lit; toasts and tickers freeze with it.

```
+----------------------------------------------------------------------------------------+
|                         scrim C_CHAR 160                                               |
|      +==================================================================================+ 170  panel (230,170) 820x380 C_SLAB / C_STEEL_HI
|      |===================== caution tape inside the top 24 px ==========================|
|      |  PAUSED                            |  HOW IT WORKS                                | 210  eyebrows at (254,210) and (652,210)
|      |  ## RESUME ##                      |  [LUMBER] > [PEWPEW 9000] > [SHOW & TELL]    | 240  rows (254, 240+i*56, 360, 48) F_H2; chips row 1 at y 240
|      |                                    |  +900 CLOUT                                  | 274  F_MONO_S hazard
|      |  HOW TO PLAY                       |  [MESS] > [TRASH]   [?] > [HI]               | 300  chips row 2 (live)
|      |                                    |  +40 KARMA                                   | 334  F_MONO_S live
|      |  QUIT TO MENU                      |  CLUTTER                               100   | 372  eyebrow at (652,372); "100" F_MONO_S ash right at 952
|      |                                    |  [############################........] [H]  | 392  hud_bar(652,392,300,16) live value; GL_MARSHAL at (960,392) C_FAULT
|      |  [A] DO IT   [B] DROP              |                                              | 420  controls strip for the pauser's seat, its own glyphs
|      |  [X] PROPOSE (DOES NOTHING)   [START] PAUSE                                       | 452
|      |  ANYONE CAN RESUME.                |                                              | 492  F_MONO_S ash
|      +==================================================================================+ 550
+----------------------------------------------------------------------------------------+
```

Divider 2 px `C_STEEL` at x 632 from y 200 to 520. Right column x 652..1026. The two loops are `flow_chip` and `flow_arrow` rows (`draw.c` 563 to 575, kept): row 1 LUMBER, arrow, PEWPEW 9000, arrow, SHOW & TELL in `C_HAZARD` (364 px); row 2 MESS, arrow, TRASH, a 14 px gap, "?", arrow, HI in `C_LIVE`. The clutter bar shows the live `G.clutter` so the pauser sees where the night stands. This is the one on-demand place for both buttons and loops, which is why it has the one parenthetical joke.

Any seat's START or B resumes. The pauser's nav drives the rows: RESUME; HOW TO PLAY (SCR_FLASH over the pause, `flash_return` SCR_NONE, `G.paused` stays true); QUIT TO MENU (ends the night without results: `game_quit_to_title()`, SCR_MAIN with the pauser as owner, every other human pending, music 1).

### 6.11 Results

```
+========================================================================================+ tape 0..24, scroll 20 px/s
|   11:00 PM. LIGHTS OUT.                                                                 | 48  eyebrow ink-ok (ink-err on a lost night)
|   THE SPACE SURVIVES ANOTHER TUESDAY.                                                   | 70  F_H1 bone (F_H2 if wider than 1160)
|   CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES.                                        | 150 F_MONO ash (ink-err on a lost night)
|   +--------------------------------------------------------------------------------+   | 186 space strip (64,186) 1152x44 C_SLAB
|   | [H] CLUTTER AT CLOSE [bar 240] 48   [<3] VIBE AT CLOSE [bar 240] 61   NEWBIES 3 OF 5 |   F_MONO_S, two 240x12 bars, GL_MARSHAL and GL_HEART
|   +--------------------------------------------------------------------------------+   |
|   MEMBER                                   CLOUT        KARMA    AWARD                 | 246 column eyebrows (ranked modes)
|   1  [p] PAD 1                             1,040          320    JANITORIAL CO-OP     | 272 rows 1152x58 pitch 62, slide in from +80, 0.25 s stagger
|   2  [b] BUILDER                             900          120    GLAMOUR BIAS          |     rank F_H2 smoke, seat glyph scale 1 at (124,y+22), name F_H3 at 150, numbers F_MONO_L, award F_MONO_S x2
|   ...                                                                                   |
|                   ## THE REAL WINNER: PAD 1. LEAVE IT BETTER. ##                        | 600 live block, F_H3 tape ink (kept)
|   |======= RESULTS ======|  (4 s tape shrinking)   then   [A] AGAIN   [B] MENU          | 666 footer
+----------------------------------------------------------------------------------------+
```

- Headline, eyebrow and sub line come from `END_VARIANTS` (8.6) by `G.end` and `G.end_variant`, chosen once in `end_night()` by `rnd()`, never per frame.
- The space strip is new; it reads `G.clutter` and `G.vibe` preserved by `end_night` (`game.c` 983 to 985), plus `G.newbies_seen` and `G.newbies_greeted` (new counters).
- Rows: as today (sorted by clout on survival, karma otherwise, VOID in ink-err for clout on a lost night, `draw.c` 670 to 677), pitch 62 from y 272, up to 6. Every row carries the seat glyph (GL_PAD, GL_KEYS, GL_PHONE, GL_BOT) at scale 1 before the name: `hud_glyph(124 + ox, y + 22, g, 1, C_SMOKE, 255)` and the name moves from x 130 to x 150. Each row landing (`in` reaching 1) plays SFX_TICK; the first row also fires `fx2_burst(FX2_SPARK, 90, y+28, C_HAZARD)`.
- Awards: the existing six plus FREE PILE CURATOR, PRINTER WHISPERER, MARSHAL'S FAVORITE, DISH PIT, BIKESHED with the counters `shelved, fixes, unplugged, dishes` on `player_t`. Priority order stays karma, clout, then the rest (`draw.c` 686 to 694); one award per player. Full table in 8.6.
- Survived: `fx_burst(FX_CONFETTI, 1.8f, 2.0f, 6.3f, C_HAZARD)` at results t 0.3, 1.1 and 1.9 (main.c `flow()` PH_RESULTS, three one-shot flags) so confetti falls over Show & Tell under the play camera, which is active in PH_RESULTS.
- CO-OP variant (`G.mode == MODE_COOP`): no rank or CLOUT column; rows in seat order with KARMA and TASKS. Above them a team panel (64,246) 1152x100: eyebrow TEAM KARMA at (88,258) and the total F_H1 live at (88,278); at the right the tier: eyebrow with the tier word at (1192,258) right aligned, and the letter F_TITLE right aligned at 1192, y 240, in the tier colour. Tier by vibe and clutter at close: END not survived gives C CLOSED `C_FAULT`; survived with vibe at or above 60 and clutter under 40 gives S SPOTLESS `C_LIVE`; survived with vibe at or above 30 gives A OPEN TOMORROW `C_HAZARD`; otherwise B BARELY `C_RUST`. A LAB RECORD chip in hazard at (700,300) with `fx2_burst(FX2_CONFETTI, 640, 0, C_HAZARD)` every 0.5 s for 3 s when `SAVE.best_team` was beaten. Rows start at y 360. The banner reads THE REAL WINNER: EVERYONE WHO SWEPT. (36 chars).
- VERSUS: the winner's row gets a 3 px `C_HAZARD` frame and `fx2_burst(FX2_RING, 90, y+28, C_HAZARD)` on landing; on a lost night the banner is NOBODY WINS. THE FLOOR IS STILL DIRTY.
- Records: when `flow()` sees `G.phase` become PH_RESULTS it calls `save_records()`: update `SAVE.best_karma` and its name, `SAVE.best_clout` and its name (only on a survived night), `SAVE.best_team`, `SAVE.nights_played`, then `save_commit()`. A beaten personal record adds a LAB RECORD chip after the award on that row.
- Footer: for the first 4 s a tape bar `hud_tape(64, 666, (int)(1152 * (1 - G.t/4)), 24, T*20)` with the chip RESULTS; after 4 s keycaps `[A] AGAIN` and `[B] MENU` centred, with the CTA pulse, the A keycap carrying P1's glyph. A or START: lobby as today. B: SCR_MAIN with the presser as owner, other humans pending. 40 s: lobby if humans remain else `new_group()`.

---

## 7. Motion and sound

| Moment | Motion | Sound | Rumble (pads only, optional, `wc_pad_rumble`) |
|---|---|---|---|
| title idle | CTA block breathes 8 px, 1.4 s; tagline crossfade 0.3 s every 6 s | music mode 1 | |
| title to attract | scrim 70 to 0 over 0.5 s; band fades; tip bar rises from y 720 over 0.3 s | | |
| attract to title | reverse, 0.3 s | SFX_MENU_MOVE | |
| take ownership | 6.1 transition 0.35 s, rows stagger 0.04 s | SFX_MENU_OK | 0.3, 0.5, 80 ms |
| cursor move | tween speed 18, 3 px overshoot 0.22 s | SFX_MENU_MOVE | |
| value step | value slides 12 px and snaps 0.18 s | SFX_MENU_MOVE | |
| select | row flashes bone 80 ms, FX2_SPARK at the row end | SFX_MENU_OK | 0.3, 0.5, 80 ms |
| back | screen wipe right 0.28 s | SFX_MENU_BACK | |
| deny | 6 px shake 0.2 s, deny line 2 s, AT THE DOOR chip drops in | SFX_MENU_DENY | 0.6, 0.0, 60 ms |
| ownership passes on idle | owner chip slides 12 px and snaps | SFX_MENU_OK | |
| mode card focus | lift tween speed 16, shadow 6 to 10 px | SFX_MENU_MOVE | |
| mode pick | card flashes bone, FX2_RING | SFX_MENU_OK | |
| flash card wipe | 60 px slide, 0.3 s | SFX_CARD | |
| flash beats | per card in 6.6 | SFX_GLAMOUR, SFX_GLAMOUR, none, SFX_ALARM, SFX_KARMA | |
| flash end | card drops 40 px and fades 0.25 s | SFX_MENU_BACK | |
| PLAY | band and rows out 0.28 s; camera tween 0.8 s | SFX_MENU_OK; music stays mode 1 | |
| lobby join | card drops in from y 540, ease back 0.25 s | SFX_JOIN (existing) | 0.3, 0.3, 100 ms on that pad |
| lobby leave | ring fills 1.5 s, card drops 20 px and fades | SFX_MENU_BACK | |
| ready | frame to live, check stamps 0.1 s | SFX_READY | |
| countdown | F_TITLE digit drops 20 px, fades in 0.2 s | SFX_COUNTDOWN each, SFX_GO on GO | |
| night start | tape wipe 0.3 s close, 0.3 s open | SFX_ALARM (existing), music mode 2 | 0.5, 0.5, 200 ms |
| join strip narrows | x and width tween 0.3 s at 45 s | none | |
| pause | scrim and panel fade in 0.15 s | SFX_MENU_MOVE; music level halves while paused (`audio_set_levels`) | |
| results rows | slide in from +80, 0.25 s stagger (kept) | SFX_TICK per row | |
| results survived | world confetti over Show & Tell at 0.3, 1.1, 1.9 | SFX_WIN (existing) | |
| results record | FX2 confetti from the top for 3 s | | |
| results input opens | tape bar runs out at 4 s, keycaps pulse | SFX_MENU_MOVE | |

New SFX (append to `audio.h` before `SFX_COUNT`): `SFX_MENU_MOVE, SFX_MENU_OK, SFX_MENU_BACK, SFX_MENU_DENY, SFX_CARD, SFX_READY, SFX_COUNTDOWN, SFX_GO`. Recipes for `Band::Sfx` (`audio_bellows.cpp` 305 to 339) using existing instruments only; every I_BUZZ paired with I_BUZZ_OFF:

```
SFX_MENU_MOVE: Push(0, I_HAT, 900, 0.25f); Push(0, I_PLUCK, Mtof(84), 0.35f);
SFX_MENU_OK:   Push(0, I_PLUCK, Mtof(72), 0.6f); Push(50, I_PLUCK, Mtof(79), 0.6f); Push(100, I_BELL, Mtof(88), 0.5f);
SFX_MENU_BACK: Push(0, I_PLUCK, Mtof(67), 0.5f); Push(60, I_PLUCK, Mtof(62), 0.5f);
SFX_MENU_DENY: Push(0, I_BUZZ, Mtof(45), 0.5f); Push(90, I_BUZZ_OFF, 0, 0);
SFX_CARD:      Push(0, I_OHAT, 330, 0.3f); Push(0, I_PLUCK, Mtof(76), 0.4f);
SFX_READY:     Push(0, I_BELL, Mtof(79), 0.5f); Push(80, I_BELL, Mtof(86), 0.5f);
SFX_COUNTDOWN: Push(0, I_PLUCK, Mtof(69), 0.7f); Push(0, I_BELL, Mtof(81), 0.4f);
SFX_GO:        Push(0, I_BELL, Mtof(76), 0.6f); Push(70, I_BELL, Mtof(80), 0.6f); Push(140, I_BELL, Mtof(83), 0.6f); Push(210, I_BELL, Mtof(88), 0.7f); Push(210, I_OHAT, 330, 0.5f);
```

`void audio_set_levels(float music, float sfx)` stores two gains in the band; `RenderSpan` multiplies `m` (`audio_bellows.cpp` 564) by the music gain and the three SFX `Mix` lines (594 to 596: bells 0.13, plucks 1.1, buzz 0.22) by the sfx gain. Defaults 1.0 and 1.0 until `wc_init` applies `SET`. Render a WAV via `WAV=out.wav node tools/shot.mjs ...` after adding them and check RMS per stem and the limiter, as the audio map insists.

---

## 8. Code

### 8.1 game.h additions

```c
enum { SCR_TITLE, SCR_ATTRACT, SCR_MAIN, SCR_MODES, SCR_SETTINGS, SCR_FLASH, SCR_CREDITS, SCR_NONE };
enum { TO_MAIN, TO_LOBBY };
enum { MODE_SOLO, MODE_COOP, MODE_VERSUS, MODE_CHAOS, MODE_CUSTOM, MODE_COUNT };
enum { CHAOS_CALM, CHAOS_NORMAL, CHAOS_CHAOS };
enum { HELP_OFF, HELP_FIRST, HELP_ALWAYS };
enum { NAV_UP = 1, NAV_DOWN = 2, NAV_LEFT = 4, NAV_RIGHT = 8, NAV_A = 16, NAV_B = 32, NAV_START = 64 };

typedef struct { ... } settings_t;      /* section 4 */
extern settings_t SET;

typedef struct {
  int screen, prev_screen;
  int owner;            /* seat id or -1 */
  int row, col;         /* cursor */
  float cur_y, snap_t;  /* eased cursor and the overshoot clock */
  float lift[MODE_COUNT];
  float val_slide; int val_dir;   /* settings and PLAY value nudge */
  float t;              /* seconds in this screen */
  float trans;          /* -1..0 outgoing, 0..1 incoming, 1 idle */
  float idle;           /* seconds since the owner's last nav */
  float title_idle;     /* seconds with no input at all on SCR_TITLE */
  int settings_then;    /* TO_MAIN or TO_LOBBY */
  int flash_step; float flash_t, flash_wipe; bool flash_fired; int flash_return;
  float deny_t; char deny[48];
  int pause_row; int pauser;      /* seat */
  float cam_blend;      /* 0 orbit, 1 play */
  bool kb_seen;
  int pending_count;    /* mirrored from seats[] each frame for draw.c */
  int pending_seat[SEAT_COUNT];   /* seat ids in order, for the AT THE DOOR chips */
} menu_t;
extern menu_t M;       /* defined in game.c beside G, driven by main.c, drawn by draw.c */

/* player_t additions (game.h 71 to 109) */
  bool ready;                       /* lobby */
  int shelved, fixes, unplugged, dishes;   /* awards */
  int seat;                         /* seat id, for glyphs on cards and results */

/* game_t additions (game.h 123 to 143) */
  int mode;                          /* resolved from SET at lobby open */
  bool want_bots;                    /* resolved: fill when one human */
  float pace;                        /* 1.4, 1.0, 0.65: multiplies the four spawn refills */
  float badmul;                      /* 0.6, 1, 2: multiplies bad event weights */
  float jam;                         /* printer jam chance, 0.3 or 0.6 */
  bool paused;
  bool flash_seen;
  int group_nights;
  int end_variant;
  int coach_st;                      /* station the coach points at, -1 none */
  int team_karma;                    /* co-op, kept current in karma() */
  int newbies_seen, newbies_greeted;
  float wipe_t;                      /* night start tape wipe, 0.6 down to 0 */

/* game.c */
void game_begin_lobby(void);
void game_apply_settings(void);
void game_quit_to_title(void);
int  game_mode_bots(int *a, int *b);   /* which bots the current mode adds for one human; returns count */
/* draw.c */
void front_end_reset(void);   /* called by new_group(): M = defaults, screen SCR_TITLE, owner -1 */
```

### 8.2 hud.c

Section 5 is the whole list. `blend()` (`hud.c` 66) grows a clip test and the alpha multiplier; the `hud_rect` fast path (`hud.c` 79) clamps to the clip rect too. `keycap` and `eyebrow` move out of `draw.c` (72 to 91) as `hud_keycap` and `hud_eyebrow` with the same bodies; draw.c is search-replaced. `glyphs.h` holds `GLYPH_SRC`. `fx2_*` live in hud.c so results and menus share them.

### 8.3 main.c

```c
static seat_t seats[SEAT_COUNT];
static save_t SAVE;
static float lobby_t;                   /* 25 down to 0; frozen during the flash and any hold-B */
static float empty_t, ready_t;
static bool confetti_fired[3];          /* results t 0.3, 1.1, 1.9 */

static void save_load(void);            /* magic, version, crc; defaults otherwise */
static void save_commit(void);          /* recompute crc */
static void save_records(void);         /* on entering PH_RESULTS */
static uint32_t crc32(const void *p, size_t n);

static void seats_poll(float dt);       /* fills seats[].btn/sx/sy from pads (all four, no connected gate), keys[], phones */
static int  nav_read(seat_t *s, float dt);
static void seats_join(float dt);       /* PH_LOBBY and PH_NIGHT only: A or START edge on an unseated seat -> game_add_player;
                                           START edge on a seated seat -> ready toggle (lobby) or pause (night); hold-B leave (lobby) */
static void menu_update(float dt);      /* the front end state machine, sections 2, 3 and 6 */
static void menu_goto(int screen);      /* sets prev_screen, trans = -1, t = 0, SFX by direction */
static void menu_select(void);          /* A on SCR_MAIN */
static void settings_step(int row, int dir);
static void front_end_play(void);       /* PLAY: game_apply_settings(); game_begin_lobby(); seat owner; seat pending seats; flash if due; cam tween */
static void front_end_back_to_menu(void);   /* owner alone in the lobby held B: remove the owner's player, PH_TITLE, SCR_MAIN */
static void new_group(void);            /* PH_TITLE, music 1, flash_seen false, group_nights 0, pending and denied cleared, front_end_reset() */
static void begin_night(void);          /* G.wipe_t = 0.6; game_start_night() at the closed frame; group_nights++; cam target 1 */
static void flow(float dt);             /* rewritten below */
static void camera(float dt);           /* blends orbit and play by M.cam_blend */
```

`read_input` becomes `seats_poll` plus `seats_join`; the old `seat()` logic (`main.c` 170 to 183) is kept for the join edge but gated by phase: it never runs during PH_TITLE or attract, and START on a seated player no longer sets `start_pressed` (the lobby uses READY). `any_pressed` stays for results.

`menu_update(dt)`:

```
if (G.phase == PH_RESULTS) {
  if (G.t > 4) for each seat with NAV_B edge: owner = seat; every other human seat pending = true;
     game_remove_bots(); remove humans; G.phase = PH_TITLE; G.t = 0; menu_goto(SCR_MAIN);
  return;
}
if (G.paused) { pause navigation with seats[M.pauser]; any seat NAV_START or NAV_B -> G.paused = false; return; }
if (!(G.phase == PH_TITLE || G.attract) && M.screen != SCR_FLASH) return;
if (M.screen == SCR_FLASH) { advance flash_t; A/B from the owner (or pauser); return; }
any_input = any seat has any nav bit this frame
switch (M.screen) {
  SCR_TITLE:   title_idle += dt; if (any_input) title_idle = 0; if (title_idle > 20) menu_goto(SCR_ATTRACT);
               for each seat with NAV_A|NAV_START: owner = seat; seat.pending = false; menu_goto(SCR_MAIN); break;
  SCR_ATTRACT: if (any_input) { menu_goto(SCR_TITLE); if that input had A or START: owner = seat; menu_goto(SCR_MAIN); } break;
  default:     nav = nav_read(&seats[owner]); if (nav) idle = 0; else idle += dt;
               if (idle > 20) { p = lowest pending seat; if (p >= 0) { owner = p; seats[p].pending = false; idle = 0; } else { owner = -1; menu_goto(SCR_TITLE); } }
               non-owner A or START edges -> deny once per seat, pending = true;
               then per screen handling (6.3 to 6.7);
}
```

`flow(dt)` rewritten:

```
case PH_LOBBY:
  if (n == 0) { if ((empty_t += dt) > 6) new_group(); }
  else { empty_t = 0;
         bool held = M.screen == SCR_FLASH || any seat hold_b > 0;
         if (!held) lobby_t -= dt;
         if (all_humans_ready()) { if ((ready_t += dt) >= 3) begin_night(); }
         else { ready_t = 0; if (lobby_t <= 0) begin_night(); } }
case PH_NIGHT:
  if (G.attract) break;
  if (G.wipe_t > 0) { G.wipe_t -= dt; if the closed frame was just crossed: game_start_night(); }
  if (human_count() == 0) { game_remove_bots(); n = 0; }
  if (n == 0) { if ((empty_t += dt) > 12) new_group(); } else empty_t = 0;
case PH_RESULTS:
  if (G.t == 0 on entry) { save_records(); confetti_fired[] = false; }
  if (G.end == END_SURVIVED) for k in 0..2: if (!confetti_fired[k] && G.t >= 0.3 + 0.8k) { fx_burst(FX_CONFETTI, 1.8f, 2.0f, 6.3f, C_HAZARD); confetti_fired[k] = true; }
  if ((G.t > 4 && any_pressed) || G.t > 40) { game_remove_bots(); if (human_count()) { G.phase = PH_LOBBY; G.t = 0; lobby_t = 25; audio_music(1); } else new_group(); }
case PH_TITLE:
  if (G.t > 1.5f && !G.attract) { G.attract = true; add BUILDER, TALKER, FOUNDER; game_start_night(); }
```

`front_end_play()`: `game_apply_settings(); game_begin_lobby(); lobby_t = 25; slot = game_add_player(owner is phone, pad index or -1, pid or NULL, seat name); seats[owner].slot = slot; P[slot].seat = owner;` then every `pending` seat in seat id order is seated the same way and `pending` cleared; `M.screen = flash_due() ? SCR_FLASH : SCR_NONE; M.flash_return = SCR_NONE;` where `flash_due()` is `SET.help == HELP_ALWAYS || (SET.help == HELP_FIRST && !G.flash_seen)`; the camera tween begins.

`camera(dt)`: compute both the orbit pose and the play pose as today (`main.c` 377 to 385), tween `M.cam_blend` toward its target at `dt / 0.8` and interpolate position and look target with `hud_ease_inout`.

`wc_render`: `if (!G.paused) game_update(dt);` everything else as today. `wc_get_info` sets the save region. `wc_init` calls `save_load()` before `game_init()` and `audio_set_levels()` after `audio_music(1)`.

### 8.4 game.c

- `settings_t SET; menu_t M;` defined here so draw.c and main.c share them.
- `game_begin_lobby()`: the body of the `G.attract && !adding_bot` branch at 234 to 243 (remove bots, `clear_world`, attract false, PH_LOBBY, t 0, `calm_meters`, clear toasts, music 1) moved out so main.c can call it when PLAY is chosen while attract is running; `game_add_player` keeps calling it for safety. Also clears `ready` on every player and sets `G.mode = SET.mode`.
- `game_add_player` (233): the lobby position at 254 becomes `if (G.phase == PH_NIGHT) { door anchor as today } else { p->x = -6.5f + (slot % 4) * 0.9f; p->z = 1.6f - (slot / 4) * 0.9f; }`. The join toast stays for phones only ("%s JOINED FROM A PHONE"); a pad or keyboard join is announced by its card.
- `game_apply_settings()`: `G.night_len = NIGHT_LENS[SET.night_len].secs; G.pace = PACE[SET.chaos]; G.badmul = BADMUL[SET.chaos]; G.jam = 0.3f; G.want_bots = SET.bots;` then by mode: SOLO `want_bots = true`; VERSUS unchanged (rivals only if `want_bots`); CHAOS `night_len = 120; pace = 0.65f; badmul = 2; jam = 0.6f; want_bots = true`; CO-OP and CUSTOM unchanged. `SET` is never written here.
- `game_mode_bots(&a, &b)`: VERSUS gives BUILDER and FOUNDER; every other mode gives BUILDER and TALKER; returns 2.
- `game_start_night` (944): the top-up at 947 becomes `if (!G.attract && human_count() == 1 && player_count() == 1 && G.want_bots) { int a, b; game_mode_bots(&a, &b); game_add_bot(a); game_add_bot(b); }`. Initial timers at 950 multiply by `G.pace`: `14, 6, 8, 24` times pace. CHAOS mode: `S[ST_PRINT2].state = S_BROKEN` and a spaghetti at `(S[ST_PRINT2].x, S[ST_PRINT2].z + 1.0f)` after the opening mess. `G.coach_st = -1; G.team_karma = 0; G.newbies_seen = G.newbies_greeted = 0;` every player's `ready = false` and `shelved = fixes = unplugged = dishes = 0`.
- `game_update` refills (1040 to 1043): `G.next_event = (17 + frnd() * 8) * G.pace; G.next_newbie = (13 + frnd() * 7) * G.pace; G.next_dish = 6 * G.pace; G.next_donation = (24 + frnd() * 10) * G.pace;`. Nothing else in the rush block changes.
- `act` W_PRINT (436): `s->count = frnd() < G.jam ? 1 : 0;`.
- `fire_event` (878 to 885) rewritten:

```c
static bool bad_event(int eff) {
  return eff == E_HEAT || eff == E_HABOOB || eff == E_DAISY || eff == E_DONATION ||
         eff == E_STRANGE || eff == E_FRIDGE || eff == E_SPAGHETTI || eff == E_LANDLORD;
}
static void fire_event(void) {
  float w[COUNT(EVENTS)], total = 0;
  for (int i = 0; i < COUNT(EVENTS); i++) { w[i] = EVENTS[i].weight * (bad_event(EVENTS[i].effect) ? G.badmul : 1.0f); total += w[i]; }
  int e = -1;
  for (int tries = 0; tries < 8 && e < 0; tries++) {
    float r = frnd() * total; int k;
    for (k = 0; k < COUNT(EVENTS) - 1; k++) { r -= w[k]; if (r < 0) break; }
    if (w[k] <= 0) continue;                 /* a zero weight never fires */
    if (k != G.event || tries == 7) e = k;   /* avoid a repeat, but give up after eight tries */
  }
  if (e < 0) return;
  G.event = e;
  ... the existing switch (888 to 917) unchanged ...
}
```

- `karma()` (373): `G.team_karma += v`.
- `act` counters: W_DEPOSIT to ST_FREEPILE with `it->type == IT_DONATION`: `p->shelved++` (408); W_FIX: `p->fixes++` (445); W_FLOOR with `t == IT_DAISY`: `p->unplugged++` (474); W_DISH: `p->dishes++` (459); W_GREET: `G.newbies_greeted++` (478). `spawn_newbie` (740): `G.newbies_seen++`.
- `end_night(why)` (975): `G.end_variant = (int)(rnd() % END_VARIANT_COUNT[why]);`. Records are written by main.c `save_records()` on the PH_RESULTS entry frame.
- `game_quit_to_title()`: drops everything, removes bots and humans, `clear_world`, `calm_meters`, PH_TITLE, music 1, `G.paused = false`.
- `game_update`: at the top, nothing new; `G.wipe_t` is main.c's. The co-op end rule is unchanged (survive, fire, burnout); co-op grading happens in draw.c from `G.vibe`, `G.clutter` and `G.end`.
- Clout void on a lost night stays a draw.c rule (results sort), as the sim map notes; co-op ignores clout for ranking anyway.

### 8.5 draw.c

```c
static void front_end(float dt);        /* dispatch on M.screen with the wipe; draws header, screen, footer, deny */
static void header_small(void);         /* mark 64 at (32,40), DO-OCRACY F_H2, eyebrow */
static void title_idle(float k);        /* 6.1, k is the band alpha factor during the transition */
static void attract_overlay(void);      /* 6.2 world overlays for bots */
static void menu_main(void);            /* 6.3 */
static void menu_preview(int row);      /* 6.3 right panel */
static void who_is_in(void);            /* 6.3 AT THE DOOR */
static void menu_modes(void);           /* 6.4 */
static void mode_card(int i, int x, int y, int w, int h, int a);
static void menu_settings(void);        /* 6.5 */
static void settings_help(int row);     /* 6.5 right panel text */
static void layout_preview(int idx);    /* 6.5 top down map, only when layout_count > 1 */
static void rules_flash(void);          /* 6.6 */
static void flash_card(int step, float lt, int ox, int a);   /* one picture at offset ox and alpha a */
static void credits_screen(void);       /* 6.7 */
static void lobby_overlay(void);        /* 6.8 rewrite */
static void seat_card(int x, int y, int w, int h, const player_t *p, int kind);   /* kind: 0 player, 1 bot preview, 2 qr, 3 empty */
static void countdown(void);            /* 3 2 1 GO */
static void start_wipe(void);           /* the tape wipe, driven by G.wipe_t */
static void top_bar(void);              /* 6.9 rewrite */
static void mode_panel(void);           /* (944,6) by mode */
static void join_strip(float dt);       /* wide to narrow tween */
static void player_cards(void);         /* 6.9 rewrite */
static void toasts(int xr, int y);      /* right aligned, two slots */
static void pause_overlay(void);        /* 6.10 */
static void results_screen(void);       /* 6.11 rewrite */
static void results_coop(void);
static void footer_hints(int x, int y, int n, const char *const *caps, const char *const *labels, int owner_seat);
static int  seat_glyph(const player_t *p);   /* GL_BOT, GL_PHONE, GL_KEYS or GL_PAD */
```

`draw_hud` order:

```
T += dt; memcpy vignette;
if (G.phase == PH_TITLE || G.attract) {
  if (M.screen == SCR_ATTRACT) attract_overlay();      /* bot tags, guides, names at 150 */
  fx_draw(dt);
  front_end(dt);                                       /* scrim (not in attract), header, the screen, footer */
  fx2_draw(dt);
  return;
}
if (G.phase == PH_RESULTS) { fx_draw(dt); results_screen(); fx2_draw(dt); return; }
nplaced = 0;
reserve(0, 0, HUD_W, 104);
if (G.phase == PH_NIGHT) reserve(0, HUD_H - 86, HUD_W, 86); else reserve(0, 570, HUD_W, 150);
reserve(960, 104, 304, 52); if (widest live toast w > 286) reserve(1264 - w - 18, 104, w + 18, 52);
goal_guides(); station_labels(); floor_tags(); npcs_overlay(); fx_draw(dt);
top_bar();
if (G.phase == PH_LOBBY) lobby_overlay(); else { mode_panel(); join_strip(dt); }
if (!danger()) { ticker_band(); if (G.ticker_t <= 0) coach(); }
if (G.phase == PH_NIGHT) player_cards();
toasts(1264, 108);
players_overlay();
if (G.wipe_t > 0) start_wipe();
if (G.paused) pause_overlay();
if (M.screen == SCR_FLASH) rules_flash();
fx2_draw(dt);
```

The wipe helper used by `front_end`: `wipe_offset(&ox, &a)` returns `ox = -60 * (-M.trans)`, `a = 255 * (1 + M.trans)` for the outgoing half (drawing `M.prev_screen`) and `ox = 60 * (1 - M.trans)`, `a = 255 * M.trans` for the incoming half; `hud_set_alpha(a)` wraps the screen draw and is reset to 255 after. `M.trans` advances by `dt / 0.14` in each half.

### 8.6 content.h: every table in full

```c
static const char *const MENU_ROWS[] = { "PLAY", "MODES", "SETTINGS", "HOW TO PLAY", "CREDITS" };
static const char *const SETTING_ROWS[] = { "NIGHT LENGTH", "BOTS", "CHAOS", "PHONES", "MUSIC", "SFX", "ROOM", "HELP", "RESET TO DEFAULTS", "DONE" };
static const struct { const char *name; float secs; } NIGHT_LENS[] = {
  { "SHORT 2:00", 120 }, { "NORMAL 3:00", 180 }, { "LONG 5:00", 300 }, { "MARATHON 10:00", 600 } };
static const char *const CHAOS_NAMES[] = { "CALM", "NORMAL", "CHAOS" };
static const float PACE[] = { 1.4f, 1.0f, 0.65f };
static const float BADMUL[] = { 0.6f, 1.0f, 2.0f };
static const char *const HELP_NAMES[] = { "OFF", "FIRST NIGHT", "ALWAYS" };
static const char *const ONOFF[] = { "OFF", "ON" };
static const char *const ROOM_NAMES[] = { "108 W MAIN" };   /* until LAYOUTS[] exists; then LAYOUTS[i].name */
static const char *const PAUSE_ROWS[] = { "RESUME", "HOW TO PLAY", "QUIT TO MENU" };

typedef struct { const char *short_name, *name1, *name2, *players, *blurb, *rule[3]; int rule_glyph[3]; uint32_t color; int glyph; } mode_def_t;
static const mode_def_t MODES[MODE_COUNT] = {
 { "SOLO",   "SOLO",   "WITH BOTS",    "1 PLAYER", "YOU PLUS TWO MEMBERS. ONE OF THEM MOSTLY TALKS.",
   { "BOTS: BUILDER, TALKER", "CLOCK: %s", "SCORE: CLOUT AND KARMA" }, { GL_BOT, GL_HEART, GL_CLOUT }, C_HAZARD, GL_BOT },
 { "CO-OP",  "CO-OP",  "VS THE CLOCK", "1 TO 8",   "ONE TEAM. ONE KARMA TOTAL. KEEP THE LAB OPEN.",
   { "SCORE: TEAM KARMA", "GRADE: S A B C", "LOSE: EVERYONE LOSES" }, { GL_KARMA, GL_CHECK, GL_MARSHAL }, C_LIVE, GL_KARMA },
 { "VERSUS", "VERSUS", "CLOUT RACE",   "2 TO 8",   "MOST CLOUT WINS. IF THE LAB CLOSES, NOBODY WINS.",
   { "SCORE: CLOUT", "LOST NIGHT: CLOUT VOID", "ALONE? BOTS ARE RIVALS" }, { GL_CLOUT, GL_MARSHAL, GL_BOT }, C_HAZARD, GL_CLOUT },
 { "CHAOS",  "CHAOS",  "NIGHT",        "1 TO 8",   "THINGS BREAK FASTER. A PRINTER IS ALREADY DOWN.",
   { "EVENTS: MORE AND WORSE", "PRINTERS: JAM MORE", "CLOCK: SHORT 2:00" }, { GL_MARSHAL, GL_BOT, GL_HEART }, C_RUST, GL_MARSHAL },
 { "CUSTOM", "CUSTOM", "YOUR RULES",   "1 TO 8",   "YOUR SETTINGS. YOUR PROBLEM.",
   { "OPENS SETTINGS", "THEN THE LOBBY", "SCORE: RANKED" }, { GL_KEYS, GL_CHECK, GL_CLOUT }, C_VIOLET, GL_KEYS },
};
static const char *const MODE_LOBBY_NAMES[MODE_COUNT] = { "SOLO WITH BOTS", "CO-OP VS THE CLOCK", "VERSUS CLOUT RACE", "CHAOS NIGHT", "CUSTOM YOUR RULES" };

/* rules flash captions as colour runs; NULL terminated */
static const run_t FLASH_CAPS[5][5] = {
  { { "HOLD A. ", C_HAZARD }, { "DO THE THING.", C_BONE }, { NULL, 0 } },
  { { "FOLLOW THE ", C_BONE }, { "ARROW.", C_HAZARD }, { NULL, 0 } },
  { { "AMBER", C_HAZARD }, { " IS YOURS. ", C_BONE }, { "TEAL", C_LIVE }, { " IS OURS.", C_BONE }, { NULL, 0 } },
  { { "CLUTTER", C_RUST }, { " 100: ", C_BONE }, { "FIRE MARSHAL.", C_FAULT }, { NULL, 0 } },
  { { "SAY HI", C_LIVE }, { " OR THEY LEAVE.", C_BONE }, { NULL, 0 } },
};

static const char *const DENY_LINES[] = {
  "ONE AT A TIME. THIS IS A DO-OCRACY.",
  "JOIN IN THE LOBBY. P1 IS PICKING.",
  "PINNED BY THE MODE. PICK CUSTOM.",
};
static const char *const NEEDS_LINE = "NEEDS 2 PLAYERS OR BOTS";

static const char *const CREDITS[] = {
  "A GAME ABOUT A HACKERSPACE, MADE AT ONE.",
  "HEATSYNC LABS. 108 W MAIN ST, MESA. SINCE 2009.",
  "ENGINE: THREE.C ON WASMCART.  NOISES: BELLOWS.",
  "FONTS: ARCHIVO BLACK, IBM PLEX MONO.",
  "THE FLOOR LOOP HAS NEVER EXITED.",
};

static const char *const SETTINGS_HELP[] = {
  "HOW LONG UNTIL 11 PM. THE DISHES DO NOT CARE.",
  "MEMBERS FILL EMPTY SEATS. ONE OF THEM TALKS.",
  "HOW OFTEN THINGS GO WRONG. CALM IS SUSPICIOUS.",
  "SCAN THE CODE TO JOIN. NO APP. NO ACCOUNT.",
  "THE BAND PLAYS LIVE. IT CANNOT BE FIRED.",
  "BEEPS. THE GOOD KIND.",
  "108 W MAIN. THE ONLY ROOM. FOR NOW.",
  "THE RULES FLASH AND THE COACH. FIRST NIGHT IS THE DEFAULT.",
  "BACK TO HOW IT WAS. LIKE THE WOOD SHOP.",
  "BACK TO THE MENU.",            /* DONE with TO_MAIN */
};
static const char *const SETTINGS_HELP_DONE_LOBBY = "OFF TO THE LOBBY.";

static const char *const COACH_STEPS[5][2] = {
  { "1 CLOUT", "GRAB PLYWOOD AT THE RACK" },
  { "2 CLOUT", "LOAD THE LASER. HOLD TO CUT" },
  { "3 CLOUT", "CARRY IT TO SHOW & TELL" },
  { "4 KARMA", "TEAL RINGS: HOLD TO CLEAN" },
  { "5 KARMA", "A ? NEEDS A HI OR THEY LEAVE" },
};
static const int COACH_ST[5] = { ST_LUMBER, ST_LASER, ST_SHOW, -1, -1 };

static const char *const DANGER_LINES[2][2] = {
  { "CLUTTER!", "CLEAN. THE MARSHAL IS COUNTING." },
  { "VIBES!",   "SAY HI. CLEAN UP." },
};

typedef struct { const char *eyebrow, *head, *sub; } end_variant_t;
static const end_variant_t END_FIRE_V[3] = {
  { "HYH IS CANCELED", "THE FIRE MARSHAL HAD OPINIONS.", "ALL CLOUT IS VOID. THEY COUNTED THE POWER STRIPS." },
  { "CLUTTER: 100",    "THE FREE PILE WON.",             "IT WAS ALWAYS GOING TO. ALL CLOUT IS VOID." },
  { "EGRESS: BLOCKED", "NOBODY COULD REACH THE DOOR.",   "ALL CLOUT IS VOID. THE DOOR IS BEHIND A CRT." },
};
static const end_variant_t END_BURN_V[3] = {
  { "VIBE: 0", "THE SAME THREE PEOPLE DID EVERYTHING.", "THEN THEY LEFT. THE KNOWLEDGE LEFT WITH THEM." },
  { "VIBE: 0", "EVERYONE WENT HOME TO THEIR GARAGE.",   "ALL CLOUT IS VOID. THE SINE WAVE WAITS TWO YEARS." },
  { "VIBE: 0", "THE MAILING LIST WON.",                 "200 MESSAGES. NOBODY CAME IN. ALL CLOUT IS VOID." },
};
static const end_variant_t END_OK_V[2] = {
  { "11:00 PM. LIGHTS OUT.", "THE SPACE SURVIVES ANOTHER TUESDAY.", "CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES." },
  { "RENT IS PAID. BARELY.", "THE LAB OPENS TOMORROW.",             "BECAUSE SOMEBODY SWEPT. SEE YOU THURSDAY." },
};
static const int END_VARIANT_COUNT[4] = { 0, 2, 3, 3 };   /* END_NONE, END_SURVIVED, END_FIRE, END_BURNOUT */

static const char *const COOP_TIER[4] = { "CLOSED", "BARELY", "OPEN TOMORROW", "SPOTLESS" };
static const char *const COOP_LETTER[4] = { "C", "B", "A", "S" };
static const uint32_t COOP_COLOR[4] = { C_FAULT, C_RUST, C_HAZARD, C_LIVE };
static const char *const COOP_BANNER = "THE REAL WINNER: EVERYONE WHO SWEPT.";
static const char *const VERSUS_LOST_BANNER = "NOBODY WINS. THE FLOOR IS STILL DIRTY.";

/* awards: name, one-liner (one %d where the code passes one) */
static const char *const AWARD_JANITOR  = "MOST KARMA. THE LAB OPENS TOMORROW BECAUSE OF YOU.";
static const char *const AWARD_GLAMOUR  = "MOST CLOUT, LEAST KARMA. THE SPREADSHEET LOOKED FINE.";
static const char *const AWARD_TALKER   = "%d PROPOSALS. ZERO FOLLOW-UPS. A RECORD.";
static const char *const AWARD_GREETER  = "SAID HI TO %d NEWBIES. YOU ARE THE FRONT DESK NOW.";
static const char *const AWARD_MATE     = "%d CLUB-MATES. CAN HEAR COLOURS. STILL SWEEPING.";
static const char *const AWARD_LURKER   = "PAID DUES. TOUCHED NOTHING. THE LIGHTS STAYED ON.";
static const char *const AWARD_CURATOR  = "SHELVED %d DONATIONS. NONE WILL EVER LEAVE.";
static const char *const AWARD_WHISPER  = "FIXED %d PRINTERS. NOBODY SAID THANKS. WE JUST DID.";
static const char *const AWARD_MARSHAL  = "UNPLUGGED %d DAISY CHAINS. THE LAB STILL EXISTS.";
static const char *const AWARD_DISHPIT  = "%d DISHES. NOBODY DOES THE DISHES. YOU DID.";
static const char *const AWARD_BIKESHED = "ARGUED ABOUT THE COLOUR. THE SHED IS UNBUILT.";
```

Award assignment order in `results_screen` (after the existing six at `draw.c` 686 to 694, each only if the player has no award yet): FREE PILE CURATOR to the top `shelved` at or above 2; PRINTER WHISPERER to the top `fixes` at or above 1; MARSHAL'S FAVORITE to the top `unplugged` at or above 1; DISH PIT to the top `dishes` at or above 4; BIKESHED to any player with `proposals >= 5 && tasks == 0`.

Taglines (14, replaces `TAGLINES[]` at content.h 8 to 17):

```
WHOEVER DOES IT, DECIDES.
NOBODY DOES THE DISHES.
COMMUNICATION IS NOT PERMISSION.
A JANITORIAL CO-OP WITH CLASSROOMS.
THE LASER IS BOOKED. THE MOP IS FREE.
MEET ON TUESDAY. END OF DISCUSSION.
RENT IS DUE. VIBES ARE NOT A CURRENCY.
THE FREE PILE ONLY GROWS.
STEP 5: RETURN TO STEP 1.
IF THE LAB CLOSES, YOUR CLOUT IS VOID.
SOMEBODY HAS TO. IT IS YOU.
LINUX NIGHT IS CANCELED. PLAY ANYWAY.
BE EXCELLENT. ALSO SWEEP.
THE WIKI SAYS TO UPDATE THE WIKI.
```

Attract tips (13, replaces `ATTRACT_TIPS[]` at content.h 20 to 33; the five numbered lines first and in order):

```
1. WE SHOULD CLEAN THE FLOORS.
2. TESTING ENSUES.
3. GET A QUOTE. IT IS $4,000.
4. WHY DON'T WE CLEAN THEM OURSELVES?
5. RETURN TO STEP 1.
THE FLOOR LOOP HAS NEVER EXITED.
CLOUT IS FOR YOU. KARMA IS FOR THE SPACE.
THE LASER IS BOOKED. SIR CUTSALOT IS FREE.
A NEWBIE DECIDES IN FIVE MINUTES. SAY HI.
THE FIRE MARSHAL COUNTS POWER STRIPS.
NO STAFF. NO FRONT DESK. ONLY YOU.
1,716 HOURS OF CLEANING. NOT ON THE POSTER.
COORDINATE BEFORE. DO NOT APOLOGIZE AFTER.
```

Toasts trimmed to the 44 char rule (replace `NEWBIE_LEAVES`, `NEWBIE_JOINS`, `FIXED_PRINTER` at content.h 86 to 102 and add the scoring toasts at the call sites named):

| Table or call site | Line |
|---|---|
| NEWBIE_LEAVES[0] | A NEWBIE LEFT. NOBODY SAID HI. |
| NEWBIE_LEAVES[1] | A NEWBIE FOUND A PLACE WITH A FRONT DESK. |
| NEWBIE_LEAVES[2] | A NEWBIE WENT HOME TO A CLEANER GARAGE. |
| NEWBIE_LEAVES[3] | A NEWBIE SAW THE FRIDGE AND MADE A CHOICE. |
| NEWBIE_LEAVES[4] | A NEWBIE ASKED WHERE TO START. SILENCE. |
| NEWBIE_JOINS[0] | NEW MEMBER. THEY ASKED HOW TO HELP. OH NO. |
| NEWBIE_JOINS[1] | NEW MEMBER. HANDED A BROOM IN SECONDS. |
| NEWBIE_JOINS[2] | NEW MEMBER. ALREADY EYEING THE FREE PILE. |
| NEWBIE_JOINS[3] | NEW MEMBER. FUTURE BUILDER. CURRENT SWEEPER. |
| NEWBIE_JOINS[4] | NEW MEMBER. THINKS THE LASER IS FREE. |
| FIXED_PRINTER[0] (one %s) | %s LIVES. GRUB SCREW. ALWAYS. |
| FIXED_PRINTER[1] | %s IS BACK. NOBODY WILL SAY THANKS. |
| FIXED_PRINTER[2] | %s FIXED. IT WILL JAM AGAIN BY THURSDAY. |
| W_FLOOR sawdust (game.c 474) | SWEPT. THE DESERT WILL BE BACK. |
| W_FLOOR puddle | MOPPED. THE AC WILL CRY AGAIN. |
| W_FLOOR daisy (475, replaces the existing line) | UNPLUGGED. A FIRE MARSHAL RELAXES. |
| W_DEPOSIT spaghetti to TRASH (405) | SPAGHETTI TRASHED. IT WAS A BENCHY ONCE. |
| W_DEPOSIT donation to FREEPILE (410, replaces) | SHELVED. SOMEONE WILL WANT THAT. SOMEDAY. |
| W_FRIDGE (469, replaces) | FRIDGE CLEARED. DO NOT ASK. |
| OVER 9000 ticker (401, stays a ticker) | %s'S CLOUT IS OVER 9000 |
| W_GREET (485, in addition to NEWBIE_JOINS, as the toast when `greeted == 1`) | YOU SAID HI. YOU ARE THE FRONT DESK NOW. |
| W_DEPOSIT to SHOW (398) | %s: %s |
| lobby leave | %s WENT HOME. |
| newbie arrival (game.c 756, stays) | A NEWBIE WALKED IN. SOMEBODY SAY HI. |

Limits: menu labels 12 chars, settings labels 17, values 16, mode blurbs 50, mode rules 22, flash captions 6 words, deny lines 40, credits lines 48, settings help 60, results heads 47, subs 63, toasts 44 (hard cap 48), award one-liners 63 after a two digit number. Run the lint script on content.h after editing: no byte outside 32..127, no em or en dash, every glyph row 16 chars, every caption at most 6 words.

---

## 9. Work breakdown

Staged so a full scripted night runs in `tools/shot.mjs` after every stage. Line counts are estimates of new or changed lines.

| # | Stage | Files and functions | Lines |
|---|---|---|---|
| 1 | hud helpers and glyphs | `hud.h`: `run_t`, `hud_text_sh`, `hud_text_runs`, `hud_set_alpha`, `hud_clip`, `hud_clip_clear`, `hud_eyebrow`, `hud_keycap`, `hud_ease_out`, `hud_ease_inout`, `hud_ease_back`, `hud_tween`, `GL_*`, `hud_glyph`, `hud_steps`, `hud_pict_person`, `hud_pict_station`, `hud_pict_item`, `FX2_*`, `fx2_burst`, `fx2_draw`. `hud.c`: clip and alpha inside `blend` (66) and `hud_rect` (79), the bodies, `hud_init` parses `GLYPH_SRC`. New `glyphs.h`. `draw.c`: delete `eyebrow` and `keycap` (72 to 91), search-replace callers. Nothing visible changes. | 340 (glyphs.h 230) |
| 2 | settings, seats, pads | `game.h`: enums, `settings_t`, `menu_t`, `player_t` and `game_t` fields, prototypes. `game.c`: `SET`, `M`, `game_begin_lobby`, `game_apply_settings`, `game_mode_bots`, `game_quit_to_title`, pace and badmul, `fire_event` re-roll, `jam`, counters, `team_karma`, `end_variant`, lobby anchor in `game_add_player`. `main.c`: `seats[]`, `seats_poll` without the `connected` gate (190 to 197 deleted), `nav_read`, `seats_join`, `game_on_phone` on seats, `save_load`, `save_commit`, `crc32`, `wc_get_info` save region. `content.h`: `NIGHT_LENS`, `CHAOS_NAMES`, `PACE`, `BADMUL`, `HELP_NAMES`, `ONOFF`, `ROOM_NAMES`. Attract and play still work exactly as today through `seats_join`. | 420 |
| 3 | title, attract, claim gate | `draw.c`: `front_end`, `header_small`, `title_idle`, `attract_overlay`, the 6.1 transition, `title_screen` deleted, `how_to`, `controls_card`, `join_panel` deleted. `main.c`: `menu_update` SCR_TITLE and SCR_ATTRACT branches, `menu_goto`, `new_group`, `front_end_reset`, `M.kb_seen`. `content.h`: `TAGLINES`, `ATTRACT_TIPS` replaced. Cabinet check: pad 0 (the keyboard under RetroArch) and pads 1 to 3 can each claim under the libretro core. | 260 |
| 4 | main menu | `draw.c`: `menu_main`, `menu_preview`, `who_is_in`, deny line, `footer_hints`. `main.c`: `menu_select`, owner idle and handover, deny once per seat, pending, `front_end_play`, `camera` tween. `content.h`: `MENU_ROWS`, `DENY_LINES`, `NEEDS_LINE`. PLAY opens today's lobby. | 300 |
| 5 | lobby | `draw.c`: `lobby_overlay` rewrite, `seat_card`, `countdown`, `start_wipe`, lobby top bar variant, hint strip, the lobby reserve band. `main.c`: `lobby_t`, `ready_t`, READY toggle, hold-B leave with ownership handover, `front_end_back_to_menu`, `begin_night`, `G.wipe_t`, `flow()` PH_LOBBY and PH_NIGHT rewrite. `game.c`: `P[].ready`. `content.h`: `MODE_LOBBY_NAMES`. Verify the spawn and card geometry with check L3. | 300 |
| 6 | night declutter | `draw.c`: `top_bar` (clock promoted, meters with glyphs, `meter()` signature), `mode_panel`, `join_strip` tween, `player_cards` glyphs, `toasts(1264, 108)` and the reserve, `station_labels` alpha and the nine deleted strings, `floor_tags` (RED TAG replaces, TRASH ME and TO FREE PILE deleted, broom glyph), `npcs_overlay` (SAY HI only, MEMBER deleted), `players_overlay` bot tag and one bubble, `coach` gating, `goal_guides` coach target in ash, `danger` strings. `content.h`: `COACH_STEPS`, `COACH_ST`, `DANGER_LINES`, trimmed toasts. Screenshot before and after at the same seed. | 260 |
| 7 | rules flash | `draw.c`: `rules_flash`, `flash_card` 1 to 5, wipe, steps. `main.c`: SCR_FLASH input, `flash_due`, `lobby_t` hold. `content.h`: `FLASH_CAPS`. `game.c`: `G.flash_seen`. | 340 |
| 8 | modes and settings | `draw.c`: `menu_modes`, `mode_card`, `menu_settings`, `settings_help`, `layout_preview` (fenced), pinned rows, red players line. `main.c`: `settings_step`, `M.settings_then`, CUSTOM path, RESET, DONE. `audio.h`, `audio_bellows.cpp`: `audio_set_levels`. `content.h`: `MODES`, `SETTING_ROWS`, `SETTINGS_HELP`. | 340 |
| 9 | pause and results | `draw.c`: `pause_overlay` with the loops and clutter bar, `results_screen` rewrite (variants, space strip, seat glyphs, awards, VERSUS frame, footer), `results_coop`. `main.c`: pause gate in `wc_render` and `menu_update`, `save_records`, confetti one-shots, results B handover. `game.c`: `end_night` variant, award counters. `content.h`: `PAUSE_ROWS`, `END_*_V`, `COOP_*`, awards. | 360 |
| 10 | credits, SFX, save, polish | `draw.c`: `credits_screen`. `audio.h`, `audio_bellows.cpp`: eight SFX cases. `main.c`: SFX calls at every moment in section 7, rumble pulses. `tools/shot.mjs`: `SAVE=` env dump. Verify an all zero save boots to defaults; render a WAV and check the limiter. `content.h`: `CREDITS`. | 160 |
| 11 | acceptance | the scripts in section 10 as JSON files under `tools/accept/`, the lint script `tools/lint-content.py`, a full scripted night per stage. | 140 |

Total about 3220 lines. Stage 2 is the one that must land before anything else is visible; stages 3 to 5 are each playable on their own; 6 to 9 can be split across people once 5 is in.

---

## 10. Acceptance checks

`tools/shot.mjs` runs deterministic at 60 Hz with seed 7 and reports every pad as connected (its line 45), so frame N is N / 60 seconds. Script entries are `{from, to, pad, buttons, lx, ly}` with inclusive frame ranges; a press is a 6 frame hold. `ly` is -32767 for up, 32767 for down; `lx` likewise for left and right. Each check names the frame, the input before it, and what the PNG must show. Pads are 0 based in the script and named PAD 1 to PAD 4 on screen.

Script A, `accept/idle.json`: no entries until frame 1480, then `{from:1480,to:1486,pad:2,lx:-32767}`.

| Check | Frame | Must show |
|---|---|---|
| A1 | 90 | the attract night has begun (three bot name tags visible behind the band) |
| A2 | 600 | SCR_TITLE: tape, mark at (150,210), DO-OCRACY, one tagline, PRESS A block at (430,500), phone module only if the relay answered; at most 10 strings; no how_to board, no controls card, no tips bar |
| A3 | 1260 | SCR_ATTRACT: mark 64 at (32,40), bot tags at alpha 170, station names at 150, PRESS A block at (1040,620), tips bar at y 680 reading a numbered floor loop line |
| A4 | 1520 | back on SCR_TITLE after pad 3 moved the stick; no owner chip anywhere |

Script B, `accept/claim.json`:

```json
[
 {"from":300,"to":305,"pad":2,"buttons":["A"]},
 {"from":400,"to":405,"pad":0,"buttons":["A"]},
 {"from":600,"to":605,"pad":2,"ly":32767},
 {"from":700,"to":705,"pad":2,"buttons":["A"]},
 {"from":760,"to":765,"pad":2,"lx":32767},
 {"from":820,"to":825,"pad":2,"buttons":["A"]},
 {"from":900,"to":905,"pad":2,"ly":32767},
 {"from":960,"to":965,"pad":2,"ly":32767},
 {"from":1020,"to":1025,"pad":2,"buttons":["A"]},
 {"from":1080,"to":1085,"pad":2,"lx":-32767},
 {"from":1120,"to":1125,"pad":2,"ly":32767},
 {"from":1160,"to":1165,"pad":2,"ly":32767},
 {"from":1200,"to":1205,"pad":2,"ly":32767},
 {"from":1240,"to":1245,"pad":2,"ly":32767},
 {"from":1300,"to":1305,"pad":2,"lx":-32767},
 {"from":1340,"to":1345,"pad":2,"lx":-32767},
 {"from":1380,"to":1385,"pad":2,"lx":-32767},
 {"from":1440,"to":1445,"pad":2,"buttons":["B"]},
 {"from":1500,"to":1505,"pad":2,"ly":-32767},
 {"from":1540,"to":1545,"pad":2,"ly":-32767},
 {"from":1600,"to":1605,"pad":2,"buttons":["A"]},
 {"from":1700,"to":1705,"pad":0,"buttons":["A"]},
 {"from":2500,"to":2505,"pad":2,"buttons":["START"]},
 {"from":2560,"to":2565,"pad":0,"buttons":["START"]},
 {"from":3000,"to":3005,"pad":0,"buttons":["START"]},
 {"from":3100,"to":3105,"pad":2,"buttons":["START"]},
 {"from":10500,"to":10505,"pad":0,"buttons":["B"]}
]
```

| Check | Frame | Input before it | Must show |
|---|---|---|---|
| B1 | 330 | pad 3 pressed A on the title | SCR_MAIN with the row stagger finished: five rows, PLAY focused in hazard with the value SOLO, preview panel with the SOLO card, owner chip P1  PAD 3 right aligned at 1184. The attract bots still move behind |
| B2 | 420 | pad 1 pressed A | deny line ONE AT A TIME. THIS IS A DO-OCRACY. under the rows; AT THE DOOR panel at (700,536) with one chip P2  PAD 1 and the GL_PAD glyph; the cursor has not moved |
| B3 | 560 | | deny line gone (2 s elapsed); AT THE DOOR chip still there |
| B4 | 640 | DOWN | cursor on MODES with the eased block; preview shows five mini cards, SOLO framed |
| B5 | 790 | A, RIGHT | SCR_MODES with the CO-OP card lifted (y 160, 3 px hazard frame, 10 px shadow); SOLO at rest |
| B6 | 860 | A | SCR_MAIN, PLAY focused, value CO-OP, preview shows the CO-OP card with GRADE: S A B C |
| B7 | 1050 | DOWN, DOWN, A | SCR_SETTINGS, NIGHT LENGTH focused, right panel reads the NIGHT LENGTH help line, ROOM row shows 108 W MAIN with smoke chevrons, ten rows, DONE last |
| B8 | 1400 | LEFT (night SHORT), DOWN x4 (MUSIC), LEFT x3 | NIGHT LENGTH value SHORT 2:00; MUSIC row focused with 4 of 10 ticks live and the number 4 |
| B9 | 1460 | B | SCR_MAIN with SETTINGS focused; `SAVE=build/save.bin node tools/shot.mjs ...` dumps a save_t whose magic is 0x434f4f44, mode 1, night_len 0, music 4, and whose crc verifies |
| B10 | 1640 | UP x2, A (PLAY) | PH_LOBBY under the flash: card 1 HOLD A. DO THE THING. with the keycap A in the prompt box (the owner is a pad); the lobby tape at (196,72) is full width 1068 (lobby_t frozen at 25); camera is the play camera (station labels at their play positions) |
| B11 | 1760 | pad 1 pressed A during the flash | flash card 1 still up (lt 2.6 cycles at 156 frames: card 2 arrives at frame 1600 + 156 = 1756, so card 2 FOLLOW THE ARROW. is wiping in); behind it a second seat card has appeared |
| B12 | 2460 | flash ended at about frame 2452 (14.2 s) | PH_LOBBY clear: top bar with the lobby panel reading CO-OP VS THE CLOCK and SHORT 2:00; tape at full width; hint strip at (16,108); four cards at y 580: P1 PAD 3 with GL_PAD, P2 PAD 1, the QR card (if online) or PRESS A, PRESS A; no bot preview cards (two humans); players standing around (378..487, 452), none under the cards |
| B13 | 2530 | owner START | P1 card framed live with GL_CHECK and READY; status line WAITING ON PAD 1 at y 540 |
| B14 | 2650 | pad 1 START at 2560 | both READY; the F_TITLE digit 2 at (640,280) (countdown began at 2566; 2650 is 1.4 s in) |
| B15 | 2800 | countdown and wipe done at about 2746 plus 36 | PH_NIGHT: clock panel with the big countdown 1:5x and 7:0x PM small; CLUTTER and VIBE with glyphs and no rule strings; right slot: mode panel TEAM KARMA 0 at (944,6) and the narrow join strip at (1144,6) if online; player cards at y 658 with GL_PAD glyphs and GL_CLOUT, GL_KARMA before the numbers; no CLOUT or KARMA eyebrows; coach chip 1 CLOUT with ash chevrons toward LUMBER from both humans |
| B16 | 3030 | pad 1 START | pause overlay: panel (230,170) 820x380, RESUME focused, the two loop chip rows and the clutter bar on the right, the controls strip with PROPOSE (DOES NOTHING), ANYONE CAN RESUME.; the room frozen behind; music audible in the WAV |
| B17 | 3130 | pad 3 START | resumed; the clock has advanced by at most 2 frames worth across the pause |
| B18 | 10300 | the 120 s night ended at about frame 2782 + 7200 = 9982 | PH_RESULTS co-op: variant head from END_OK_V or a lost variant; space strip with the two bars; team panel TEAM KARMA total and the tier letter with its word eyebrow; two rows in seat order, each with a seat glyph before the name; banner THE REAL WINNER: EVERYONE WHO SWEPT.; footer keycaps [A] AGAIN [B] MENU pulsing (4 s passed); if survived, confetti strips visible over Show & Tell in frames 10000 to 10100 |
| B19 | 10540 | pad 1 B | SCR_MAIN with the owner chip P1  PAD 1; AT THE DOOR shows P2  PAD 3; the attract night restarting behind within 1.5 s (bots appear by 10630) |

Script C, `accept/second-night.json`: continue from B19 by re-running with PLAY at frame 10700 (A on pad 1): at 10740 the lobby opens with no flash (`flash_seen`), both cards present (owner plus the pending seat); START both at 10800 and 10860; at 11100 the night runs with no coach at all (HELP is FIRST NIGHT and `group_nights` is 2). Then with no input, players idle out at 75 s (frame 11100 + 4500): by frame 15700 the night is empty, 12 s later (16420) `new_group()` has run: SCR_TITLE, no owner chip, no AT THE DOOR line. A at 16500 on pad 0, then PLAY: the flash plays again (flash_seen reset).

Script D, `accept/idle-owner.json`: pad 1 A at 300, DOWN DOWN A at 400..520 (SETTINGS). No further input. At frame 1750 (20 s idle) SCR_TITLE with no owner; at 2960 (20 s more) SCR_ATTRACT. With a second pad pending (pad 2 A at 350): at 1750 instead the owner chip reads P1  PAD 2 and the menu stays on SETTINGS.

Script L (lobby geometry), `accept/lobby-geom.json`: pad 0 A at 300, A at 400 (PLAY with SOLO; HELP set OFF in the script by stepping SETTINGS first, or run after a flash). At the first clear lobby frame:

| Check | Must show |
|---|---|
| L1 | the P1 player's name tag is centred near (378, 400) and the feet near (378, 452); nothing of the player, the tag or the prompt box lies inside y 580..700 |
| L2 | no station name tag or floor tag is drawn inside y 580..700 (the reserve band pushed them up) |
| L3 | the four cards are at x 56, 352, 648, 944, y 580, w 280, h 120; two of them are BOT previews (BUILDER, TALKER) at alpha 180 because SOLO adds bots |
| L4 | after holding B on pad 0 from frame 700 for 60 frames (1.0 s), the game is back on SCR_MAIN with the P1  PAD 1 chip and no player in the room |

Cabinet check (manual, the headless rig cannot prove it because it marks every pad connected): on the Batocera station under the libretro core, with only the keyboard, pressing Z (RetroPad south, `WC_BTN_A`) claims the menu as PAD 1; with a USB pad in port 2, 3 or 4 and nothing in port 1, its A claims the menu as PAD 2, 3 or 4. Both must work with the `connected` gate gone.

Audio check: `WAV=build/accept.wav node tools/shot.mjs build/do-ocracy.wasc build/shots/b 330,860 tools/accept/claim.json` and confirm SFX_MENU_OK at 5.0 s, SFX_MENU_DENY at 6.7 s, SFX_MENU_MOVE at each cursor step, no limiter pumping under the lobby music.

Content lint: `python3 -I tools/lint-content.py src/content.h src/glyphs.h` fails on any byte outside 32..127 inside a string, any U+2013 or U+2014 anywhere in the repo's `src/` and `plan/` text, any glyph row not 16 chars, any `FLASH_CAPS` entry over 6 words, any toast over 48 chars.

---

## 11. Risks

1. The seat gate. Today any A or START on any seat creates a player and leaves the title (`main.c` 170 to 180, `game.c` 234 to 240, 264). If `seats_join` runs while the front end is up, the first A on the title still skips the menu. Mitigation: `seats_join` is called only when `!(G.phase == PH_TITLE || G.attract)`; check B1 proves it.
2. `game_add_bot` restores `G.phase` (`game.c` 497 to 505) so the attract bots do not flip PH_TITLE to PH_LOBBY while the menu is open; it must never touch `M`. Adding bots in `flow()` PH_TITLE while SCR_MAIN is up is normal.
3. Lobby card geometry. The projection numbers in section 0 are computed, not measured; `t3_perspective_camera_new(39, ...)` at `world.c` 707 and the `camera()` pose at `main.c` 380 are the inputs. If the real frame differs by more than 20 px, check L1 catches it; the fallback is to lower the cards to y 600 (h 100, name F_MONO_L) before moving the spawn further.
4. `hud_tape` blends per pixel (`hud.c` 110 to 120) and is slow across 1280 px; the lobby countdown tape, the title header, the card headers and the wipe leading edges are all 24 px strips. The night start wipe fills with `hud_rect` and uses tape only at the edges.
5. Owner handover on idle to a pending seat means a stranger who pressed A once can inherit the menu after 20 s. That is the intended party behaviour; a cabinet left alone still returns to the title because pending seats are cleared by `new_group()` and the lobby empties at 6 s.
6. Toast width versus the reserve: a 48 char toast at F_MONO_S is about 450 px and starts near x 814; the dynamic reserve covers it, but `settle()` only moves labels vertically in half height steps, so a back wall station tag directly under a long toast moves up to 27 px. Acceptable; the top bar reserve already does the same.
7. Save region growth. `save_t` is 108 bytes; the libretro core copies it every frame, so it must stay small. Any later field goes in `reserved[32]` first.
8. `fire_event` re-roll changes the random stream relative to today, so screenshots of event timing from before this work are not comparable; re-baseline after stage 2.
9. CO-OP results reuse `G.vibe` and `G.clutter` preserved by `end_night`; `calm_meters` must keep that order (`game.c` 983 to 985) or the tier is always SPOTLESS.
10. The flash over a paused night draws with `G.paused` true; `fx2` particles still animate because `fx2_draw` takes `dt` from `draw_hud`, which runs while paused. Intended.
11. Deleting the how_to board and controls card removes the only place ENTER and E were documented; the pause overlay's controls strip lists START for the pauser's seat, and `glyph_a` carries SPACE and "." for keyboard seats. E is not documented anywhere and does not need to be.
12. Phone owner pressing A on the title: `I` messages for a seat without a slot must update `seats[]` only; today `game_on_phone` writes `phones[k]` which indexes by slot (`main.c` 302 to 312). The seat table replaces that; a stale `k` lookup would drop the press.
13. The layout workstream may rename `LAYOUTS[].name`; the ROOM row reads `ROOM_NAMES[0]` until `layout_count` exists, and the fence is a single `#ifdef HAVE_LAYOUTS` in draw.c and main.c.

---

## 12. Facts not to get wrong

1. The attract night is `G.phase == PH_NIGHT` with `G.attract`. The menu layer must never read `G.phase == PH_TITLE` alone to decide it is on the front end; use `G.phase == PH_TITLE || G.attract` (`draw.c` 756 does this today).
2. `game_add_player` ends attract (234 to 243). The menu calls `game_begin_lobby()` itself on PLAY, and `seats_join` never runs while the front end is up.
3. `hud_px` is bottom up and the live texel array (`main.c` 121 to 124); the clip rect and alpha multiplier go inside `blend` and the `hud_rect` fast path, never into a second buffer. Never realloc or free it.
4. `draw_hud` starts every frame with the vignette memcpy (`draw.c` 755), not a clear; full screens (flash, pause) draw their own scrim.
5. Only ASCII 32..127 renders. Chevrons are `hud_chevron`, not `<` and `>` characters, wherever they are decorative; the literal `<` and `>` are fine in F_MONO when used as text.
6. `hud_text` y is the line box top; a 40 px F_H2 in a 56 px row sits at y plus 6, not centred by height. `hud_font_h` is the pixel size, not the line height.
7. `eyebrow` doubles the width of its string by inserting spaces; measure the spaced string.
8. `hud_panel` is always opaque with a 6 px shadow and a 2 px rule; translucency goes through `hud_rect` and `hud_frame`.
9. `hud_tape` blends per pixel: keep it to 24 px strips.
10. The phone wire has four button bits (`btn & 15`). Every menu, the lobby READY, pause and the flash use only the stick, A, B and START.
11. Under RetroArch only pad 0 reports connected and the keyboard is pad 1 (RetroPad 1 binds). Poll all four pads, seat on edges, never gate on `connected`.
12. The owner is a seat, not a player, until PLAY. Owner idle hands over to a pending seat or releases at 20 s; a phone owner lost to the relay does the same.
13. `SET` changes call `save_commit()` every time; the host reads the struct after the frame. All zero save data means defaults. `SET` is never written by a mode pick except `SET.mode` itself; CHAOS is a preset resolved in `game_apply_settings`.
14. `G.flash_seen`, `G.group_nights`, `pending` and `denied` are session state reset by `new_group()`, never saved.
15. The lobby clock is `lobby_t` in main.c, not `G.t`; it freezes during the flash and any hold-B, and resets to 25 on every join.
16. `end_night` picks `G.end_variant` once; results never call `rnd()`.
17. The coach guide uses `G.coach_st` and draws in `C_ASH` so it is never confused with a player's own goal chevrons in the player colour.
18. `PCOL[0]` is the UI amber and `PCOL[5]` is bone, so every player chip carries a glyph or a name, never colour alone.
19. The mark is `hud_logo` with `C_BONE` flame everywhere on the front end; it is never scaled by anything but its size argument and never tinted. `C_LOGO_SUN` is never used for UI.
20. The slot bar at y 68 is a single shared channel: danger beats ticker beats coach (`draw.c` 773). The lobby countdown uses its own chip and tape at y 72, not the slot bar.
21. Toasts: five stored (`game.c` 84 to 90), two drawn, visible only while `toast_t > 2`; right aligned at 1264 with the reserve before world labels.
22. `fire_event` re-rolls up to eight times and never fires a zero weight; the `(e + 1) % 13` bump is gone.
23. `G.pace` multiplies the four spawn refills and their initial values only; printers tick on real time before the rush block and the landlord, meters and end checks run after it, exactly as today.
24. The lobby spawn anchor is (-6.5 + 0.9 per slot, 1.6) in PH_LOBBY and the door in PH_NIGHT; night start positions at `game.c` 958 are untouched.
25. One object, one label: RED TAG replaces the label, SAY HI replaces NEWBIE: SAY HI, the prompt box is the only place a button name appears in play.
26. No em or en dashes anywhere in strings, comments, this plan or the lint; the hyphens in DO-OCRACY, CO-OP, CLUB-MATE and FOLLOW-UPS are compound words and stay.
