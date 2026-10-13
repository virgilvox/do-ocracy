# DO-OCRACY simulation map: game.c and game.h

A line-by-line technical map of the simulation in `/Users/obsidian/Projects/heatsync/games/do-ocracy/src/game.c` (1077 lines) and `src/game.h` (195 lines), plus every place in `main.c`, `world.c` and `draw.c` that reaches into the simulation's tables and state. Line numbers refer to the files as they stand at this handoff. Read this before touching game.c; it is written so an implementer does not have to rediscover any of it.

Sections 1 to 19 describe what exists. Section 20 lists every hard-coded coordinate. Sections 21 to 23 say exactly what must change for (a) multiple layouts and nights, (b) difficulty and mode settings, and (c) new stations and events. Section 24 is the list of facts that are easy to get wrong.

House rules for anything written into this project: no em or en dashes, no emoji, no bold for emphasis in prose. HeatSync material is public hackerspace culture and public HeatSync lore only; roles, never names. Visuals follow GANTRY (char, bone, hazard amber as fill, rust, live teal, square corners, solid offset shadows, caution tape). The HeatSync mark is never redrawn or recoloured.

## 1. Files and who owns what

| File | Role | Reads from game.c | Writes into game.c state |
| --- | --- | --- | --- |
| `src/game.h` | All types, enums, globals, prototypes | | |
| `src/game.c` | The simulation: tables, players, bots, newbies, items, stations, events, meters, night flow | | everything |
| `src/content.h` | Every string table (projects, donations, proposals, awards). `COUNT(a)` macro lives here (line 6) | | |
| `src/main.c` | wasmcart entry points, input from pads, keyboard seats and phones, phase flow (`flow()`), camera, config.json | `G.phase`, `G.t`, `G.attract`, `G.night_len`, `G.clutter`, `G.vibe`, `P[]`, `human_count()`, `player_count()` | `G.phase`, `G.t`, `G.attract` (via `flow()` and `game_add_bot`), `G.night_len` (config), `G.relay`, `G.room`, `P[].prev`, `P[].seen` |
| `src/world.c` | three.c scene: room, station models, furniture, props, lights, item and person models, per-frame sync | `S[]` (positions, state, timer, count, smell, obj, light, beam, growth), `OBST[]`, `P[]`, `N[]`, `I[]`, `G.laser_cutting` | `S[i].obj`, `S[i].light`, `S[i].beam`, `S[i].growth` (set in `build_stations`) |
| `src/draw.c` | HUD on a 1280x720 CPU texture | `S[]`, `I[]`, `N[]`, `P[]`, all of `G`, `PROPOSALS`, `TAGLINES`, `ATTRACT_TIPS`, `AWARD_*` | nothing |
| `src/hud.h` | GANTRY colour tokens `C_*`, font enum `F_*`, drawing primitives | | |
| `src/audio.h` | `SFX_*` enum, `audio_play(int)`, `audio_music(int mode)` with 0 off, 1 lobby, 2 night, 3 closing | | |
| `config.json` | `controller_url`, `relay`, `night_seconds` (180) | | |

Init order in `main.c wc_init()` (lines 101 to 135): `hud_init()`, `game_init()`, `load_config()`, `world_init(scene)`. `world_init` reads `S[i].x, S[i].z` and `OBST[]` when building models, so `game_init` must have filled `S[]` from `LAYOUT[]` first. Frame order in `wc_render` (lines 393 to 404): `read_input(dt)`, `net_update(dt)`, `game_update(dt)`, `flow(dt)`, `world_sync(dt)`, render, `draw_hud(dt)`.

## 2. Coordinate frame and room constants

The room is a flat plane, 24.6 units wide (x) by 14.3 units deep (z). From `world.c` line 77 and the mapping functions at lines 80 and 81:

- x runs from -12.3 (left wall) to +12.3 (right wall).
- z runs from -7.15 (back wall, far from the camera) to +7.15 (front, the window and door side, nearest the camera).
- y is up. Players stand at y 0, carried items float at y 1.75 (world.c `world_sync`).
- The camera in play (`main.c camera()` line 380) sits at (0, 21.5, 15) and looks at (0, 0, -1.25), so on screen -x is left and +z is down.
- The floor texture is 1024 x 596 pixels; `fx_(x) = (x + 12.3) / 24.6 * FW`, `fz_(z) = (z + 7.15) / 14.3 * FH`.

Walkable area, from `collide()` (game.c lines 203 to 229):

| Constraint | Line | Value |
| --- | --- | --- |
| Left wall | 204 | x >= -11.6 |
| Right wall | 205 | x <= 11.6 |
| Back wall | 206 | z >= -6.6 |
| Front kick wall | 207 | z <= 7.0 unless within 0.9 of DOOR_X, where the doorway continues |
| Absolute front | 208 | z <= 8.5 (the sidewalk outside the door) |

Door: `DOOR_X -8.0f`, `DOOR_Z 7.9f` (lines 61, 62). Front left. The front wall gap is x in [-8.9, -7.1] (world.c builds the frame posts at -8.9 and -7.1).

`REACH 0.85f` (line 63): how close to a station's box a player must stand to act on it. Players collide with radius 0.38 (line 721), newbies 0.36 (line 766), dropped items 0.2 (line 274). The minimum walkable gap between two boxes is therefore 0.76; HANDOFF's rule is every gap wider than 0.76 or closed.

## 3. game.h: enums

| Enum | Line | Values | Notes |
| --- | --- | --- | --- |
| `MAX_PLAYERS` | 14 | 8 | Also the phone slot count in main.c |
| `MAX_ITEMS` | 15 | 64 | `spawn_item` returns -1 when full; callers mostly ignore that |
| `MAX_NPCS` | 16 | 8 | newbies |
| phase | 18 | `PH_TITLE, PH_LOBBY, PH_NIGHT, PH_RESULTS` | |
| bot archetype | 19 | `BOT_NONE, BOT_BUILDER, BOT_TALKER, BOT_FOUNDER` | stored in `player_t.bot`; 0 means human |
| end reason | 20 | `END_NONE, END_SURVIVED, END_FIRE, END_BURNOUT` | |
| buttons | 23 to 26 | `IN_A 1, IN_B 2, IN_X 4, IN_START 8` | Phones send `btn & 15` (main.c line 311): exactly these four bits exist on the wire |
| item type | 28 to 35 | `IT_NONE, IT_PLYWOOD, IT_PROJECT, IT_SCRAP, IT_BOTTLE, IT_SPAGHETTI, IT_DONATION, IT_SAWDUST, IT_PUDDLE, IT_DAISY, IT_TYPES` | first six are carried, last three are cleaned in place |
| station | 37 to 40 | `ST_LUMBER, ST_LASER, ST_PRINT1, ST_PRINT2, ST_CUTSALOT, ST_BENCH, ST_SINK, ST_FRIDGE, ST_TRASH, ST_FREEPILE, ST_SHOW, ST_COUCH, ST_TABLE, ST_COUNT` | order matters: `ST_PRINT1 <= k <= ST_PRINT2` loops appear in game.c 564, 1007, 1060 and draw.c 179 and world.c 320, 853 |
| station state | 43 | `S_IDLE, S_LOADED, S_BUSY, S_DONE, S_BROKEN` | |
| obstacle | 152 | `OB_SAW, OB_SHELF, OB_COFFEE, OB_KTABLE, OB_TV, OB_CTABLE, OB_ISLAND, OB_LATHE, OB_MILL, OB_COUNT` | |
| fx | 190 | `FX_SPARK, FX_KARMA, FX_CONFETTI, FX_DUST, FX_STEAM, FX_JOIN` | draw.c particles |

## 4. game.h: structs, field by field

### item_t (lines 45 to 56)

| Field | Meaning |
| --- | --- |
| `alive` | slot in use |
| `type` | `IT_*` |
| `x, z, y` | world position; `y` is 0 on the floor |
| `holder` | -1 on the floor; 0 to 7 a player slot; 100 + n held by newbie n (game.c line 824). Any `holder >= 0` means "not on the floor" |
| `age` | seconds on the floor; past 25 the item is red tagged (weight x1.5, draw.c RED TAG) |
| `value` | clout for a project |
| `name` | index into `LASER_PROJECTS`, `PRINT_PROJECTS`, `BENCH_PROJECTS`, `STICKERS` or `DONATIONS` |
| `kind` | project source: 0 laser, 1 print, 2 bench, 3 sticker. For a donation `world_item_obj` is passed `heavy` in this argument |
| `heavy` | donation flag from `DONATIONS[name].heavy`; slows the carrier to 0.55x |
| `obj` | three.c group |

### station_t (lines 58 to 69)

| Field | Meaning |
| --- | --- |
| `label` | from `LAYOUT` |
| `x, z, hw, hd` | centre and half extents of the footprint box |
| `solid` | set true in `game_init` (line 933) and never read anywhere. `collide()` blocks on every station regardless |
| `state` | `S_*`; meaning depends on the station, see section 7 |
| `timer` | printers: seconds left in the print; Show & Tell: pedestal spin time |
| `dust` | laser only, 0 to 100 |
| `count` | sink: dirty dishes 0 to 10; printers: 1 means this print will jam |
| `smell` | fridge only |
| `proj_name` | declared, never used |
| `obj` | model group (world.c) |
| `light, beam, growth` | world.c meshes: status lamp, laser beam, print growth / Show & Tell pedestal |

### player_t (lines 71 to 109)

| Field | Meaning |
| --- | --- |
| `active, phone` | seat in use; joined from a phone |
| `bot` | `BOT_*`, 0 for a human |
| `goal, goal_ref` | bot goal: a `W_*` kind and its reference (item, station or newbie index) |
| `think` | seconds until the bot rethinks |
| `stuck, stuck_total, side, lastx, lastz, escape, esc_x, esc_z` | bot unsticking state |
| `talk_in` | TALKER bot: seconds to the next proposal |
| `pad, kb` | local pad index or -1; keyboard seat 0 or 1 or -1 (set by main.c) |
| `has_goal, goal_x, goal_z, goal_st, goal_label` | carried item destination for the HUD arrows (set every frame in `player_update`) |
| `pid[25]` | phone id |
| `name[14]` | up to 13 chars |
| `color` | `PCOL[slot]` |
| `x, z, face, speed_mul` | position, facing (radians, `atan2f(sx, sy)`), `speed_mul` unused |
| `bob` | walk cycle |
| `hold` | item index or -1 |
| `work, work_need, work_kind, work_ref` | hold-A progress on the current candidate |
| `btn, prev` | current and previous button bits |
| `sx, sy` | stick -1..1 (sy positive is +z, toward the camera) |
| `seen` | phone: seconds since last message |
| `boost, owes_bottle` | Club-Mate: speed time left; drops a bottle when it ends |
| `bubble, bubble_text` | proposal speech bubble time and `PROPOSALS` index |
| `flash, pop[48], pop_color` | score popup |
| `clout, karma, proposals, greeted, mates, tasks` | the per-night tallies the results screen uses |
| `idle` | seconds without input; main.c removes pad and keyboard players past 75 |
| `prompt[48], px, pz` | the action prompt under the player; `px` is reused as "is karma" (1 or 0), `pz` unused |
| `obj, body, carry_marker` | model; `carry_marker` unused |
| `status_t` | unused |

### npc_t (lines 111 to 121)

| Field | Meaning |
| --- | --- |
| `alive` | |
| `state` | 0 entering, 1 waiting to be greeted, 2 helping (a member), 3 leaving (life over), 4 sulking out (ignored) |
| `x, z, tx, tz` | position and walk target |
| `patience` | 20 seconds of waiting, only drains in `PH_NIGHT` |
| `life` | 40 seconds as a member |
| `work` | multi-purpose timer: entry timeout, mess cleaning progress, wander timer |
| `target` | item index being walked to |
| `hold` | item index carried |
| `bob, obj, body` | |

### game_t (lines 123 to 143)

| Field | Meaning |
| --- | --- |
| `phase, end` | `PH_*`, `END_*` |
| `t` | seconds in this phase |
| `night_len` | seconds per night; 180 unless `config.json` says otherwise (main.c clamps to 20..3600, line 93) |
| `clutter, vibe` | the two meters, 0..100 |
| `fire_warn` | seconds clutter has been at or above 100; 5 ends the night |
| `next_event, next_newbie, next_dish, next_donation` | spawn countdowns (rush-scaled seconds) |
| `event` | last event index into `EVENTS[]`, -1 none |
| `event_t` | unused |
| `heat` | seconds of 118-degree slowdown left |
| `landlord` | seconds until the landlord checks clutter |
| `laser_cutting` | seconds of beam glow left; set to 0.1 each frame someone holds A on a loaded laser |
| `ticker[96], ticker_sub[96], ticker_t, ticker_color` | the event banner |
| `toast_n` | unused |
| `room[8], join_url[160], relay[128]` | phone networking |
| `attract` | the demo night is running |
| `shake` | decremented, never set |
| `nights` | nights started since boot |

Globals (lines 145 to 164): `G`, `P[MAX_PLAYERS]`, `I[MAX_ITEMS]`, `N[MAX_NPCS]`, `S[ST_COUNT]`, `OBST[OB_COUNT]` (const), `toast_text[5][96]`, `toast_t[5]`, `toast_col[5]`, `PCOL[8]`.

Prototypes (lines 167 to 193):

```c
void game_init(void);                                   // game.c 929
void game_update(float dt);                             // game.c 991
void game_player_input(int slot, uint32_t btn, float sx, float sy);   // 288
int  game_add_player(bool phone, int pad, const char *pid, const char *name);   // 233
void game_remove_player(int slot);                      // 279
void game_start_night(void);                            // 944
int  game_add_bot(int archetype);                       // 497
void game_remove_bots(void);                            // 507
int  human_count(void);                                 // 495
void game_reset_world(void);                            // 942 (just clear_world)
void game_toast(const char *msg, uint32_t color);       // 84
const char *item_name(const item_t *it);                // 113
uint32_t rnd(void); float frnd(void); void rnd_seed(uint32_t);   // 24 to 26, xorshift32
int player_count(void);                                 // 82
void world_init(t3_scene *); void world_sync(float dt);
t3_object *world_item_obj(int type, int kind);
t3_object *world_person(uint32_t color, bool newbie, t3_mesh **body);
void world_free(t3_object *o);
void fx_burst(int kind, float x, float y, float z, uint32_t color);
void draw_hud(float dt); bool project(float x, float y, float z, int *sx, int *sy);
```

## 5. LAYOUT: the stations (game.c lines 33 to 47)

`static const struct { const char *label; float x, z, hw, hd; } LAYOUT[ST_COUNT]`. Copied into `S[]` by `game_init` (lines 930 to 934). Extents below are centre plus or minus half extents.

| Index | Label | Centre (x, z) | hw, hd | Footprint x | Footprint z | Zone, facing |
| --- | --- | --- | --- | --- | --- | --- |
| ST_LUMBER | LUMBER | (-11.0, -4.6) | 0.55, 1.5 | -11.55 to -10.45 | -6.1 to -3.1 | left wall, wood shop; approach from +x |
| ST_LASER | PEWPEW 9000 | (-4.6, -5.95) | 1.4, 0.8 | -6.0 to -3.2 | -6.75 to -5.15 | back wall; approach from +z |
| ST_PRINT1 | LOAFER | (-0.6, -6.35) | 0.55, 0.45 | -1.15 to -0.05 | -6.8 to -5.9 | back wall printing counter |
| ST_PRINT2 | DR. STRANGE | (1.0, -6.35) | 0.55, 0.45 | 0.45 to 1.55 | -6.8 to -5.9 | back wall printing counter |
| ST_CUTSALOT | SIR CUTSALOT | (3.0, -6.4) | 0.75, 0.38 | 2.25 to 3.75 | -6.78 to -6.02 | back wall, vinyl cutter |
| ST_BENCH | ELECTRONICS | (7.2, -6.0) | 1.9, 0.7 | 5.3 to 9.1 | -6.7 to -5.3 | back wall right, ESD mat |
| ST_SINK | SINK | (-11.0, 1.9) | 0.6, 0.9 | -11.6 to -10.4 | 1.0 to 2.8 | left wall, kitchen |
| ST_FRIDGE | FRIDGE | (-11.0, 4.4) | 0.55, 0.5 | -11.55 to -10.45 | 3.9 to 4.9 | left wall, kitchen |
| ST_TRASH | TRASH | (11.1, 1.6) | 0.65, 0.55 | 10.45 to 11.75 | 1.05 to 2.15 | right wall by the alley door |
| ST_FREEPILE | FREE PILE | (10.4, 5.5) | 1.2, 0.8 | 9.2 to 11.6 | 4.7 to 6.3 | front right corner |
| ST_SHOW | SHOW & TELL | (1.8, 6.3) | 2.2, 0.5 | -0.4 to 4.0 | 5.8 to 6.8 | front window |
| ST_COUCH | LURKER COUCH | (-4.6, 3.6) | 1.6, 0.45 | -6.2 to -3.0 | 3.15 to 4.05 | lounge; blocks only, no actions, no label |
| ST_TABLE | WORK ISLAND | (3.4, 0.6) | 1.6, 0.8 | 1.8 to 5.0 | -0.2 to 1.4 | centre; blocks only, no actions, no label |

COUCH and TABLE are stations only so that `collide()` and `clear_spot()` see them and world.c builds them in `build_stations`. `find_target` has no case for them (`default: break`, line 345) and draw.c skips them (line 177).

## 6. OBST: furniture that only blocks (game.c lines 49 to 59)

`const obst_t OBST[OB_COUNT]`, `typedef struct { float x, z, hw, hd; } obst_t` (game.h 153). Used by `clear_spot()`, `collide()` and world.c `build_furniture()`.

| Index | Centre (x, z) | hw, hd | Footprint x | Footprint z | What |
| --- | --- | --- | --- | --- | --- |
| OB_SAW | (-8.0, -5.7) | 0.7, 0.55 | -8.7 to -7.3 | -6.25 to -5.15 | table saw, wood shop |
| OB_SHELF | (-11.55, -2.0) | 0.35, 0.9 | -11.9 to -11.2 | -2.9 to -1.1 | parts shelving, left wall |
| OB_COFFEE | (-11.15, -0.2) | 0.5, 0.6 | -11.65 to -10.65 | -0.8 to 0.4 | coffee and coworking |
| OB_KTABLE | (-8.3, 2.4) | 0.75, 0.65 | -9.05 to -7.55 | 1.75 to 3.05 | kitchen table |
| OB_TV | (-4.6, 6.25) | 0.9, 0.2 | -5.5 to -3.7 | 6.05 to 6.45 | lounge TV on the front wall |
| OB_CTABLE | (-2.75, 3.55) | 0.25, 0.25 | -3.0 to -2.5 | 3.3 to 3.8 | side table by the couch |
| OB_ISLAND | (-2.0, -1.8) | 1.3, 0.6 | -3.3 to -0.7 | -2.4 to -1.2 | assembly table, centre left |
| OB_LATHE | (11.15, -5.2) | 0.55, 1.1 | 10.6 to 11.7 | -6.3 to -4.1 | metal shop, right wall |
| OB_MILL | (11.1, -2.9) | 0.55, 0.55 | 10.55 to 11.65 | -3.45 to -2.35 | metal shop, right wall |

## 7. Station state machines

Every station starts `S_IDLE` with `timer 0, dust 0, count 0, smell false` (`clear_world`, line 925). `game_start_night` then sets `S[ST_SINK].count = 3` (line 952).

### Laser (ST_LASER)

States used: `S_IDLE`, `S_LOADED`, `S_BROKEN`. `S_BUSY` and `S_DONE` are never set on the laser; "cutting now" is `G.laser_cutting > 0` (set at line 670 while someone holds A on a loaded laser, decays at line 1023).

- IDLE, player carrying IT_PLYWOOD within REACH: `W_LOAD` (tap), plywood consumed, state LOADED (lines 311, 415 to 419).
- LOADED, empty hands: `W_CUT`, hold 2.4 s (line 70). On completion (lines 420 to 430): `make_project(p, 0)` gives the player a laser project worth 900 to 1199; a scrap spawns at (s.x + rnd(-1..1), s.z + hd + 0.6); `dust += 26..38`; state IDLE; if `dust >= 100` state BROKEN with a rust ticker and SFX_BAD.
- BROKEN: `W_CLEAN_LASER`, hold 2.0 s, dust 0, IDLE, +80 karma (431 to 434). Adds 6 clutter while broken (line 1059).
- Event E_HABOOB adds 45 dust and breaks it only if it is IDLE at that moment (895 to 896). If it is LOADED, dust sits at or above 100 and the next cut breaks it.
- Bots: BUILDER rarely, FOUNDER and TALKER chase it (section 11).

### Printers (ST_PRINT1 LOAFER, ST_PRINT2 DR. STRANGE)

States: IDLE, BUSY, DONE, BROKEN.

- IDLE: `W_PRINT` (tap), state BUSY, `timer 14`, `count = 1` with 30 percent chance (the print will jam) (435 to 439).
- BUSY: ticked in `game_update` (1007 to 1022) with unscaled dt. If `count` and `timer < 7`: BROKEN, spaghetti spawns at (s.x + rnd(-0.5..0.5), s.z + 1.0), toast, SFX_BAD. Else at `timer <= 0`: DONE.
- DONE: `W_TAKE` (tap), `make_project(p, 1)` worth 600 to 799, IDLE (440 to 444).
- BROKEN: `W_FIX`, hold 2.4 s, IDLE, +80 karma, `FIXED_PRINTER` toast (445 to 450). Adds 4 clutter each while broken (1060).
- Event E_STRANGE breaks PRINT2 directly and drops spaghetti (906 to 909). E_SPAGHETTI drops spaghetti by a random printer without breaking it (911 to 915).
- world.c `world_sync` scales the growth mesh with `1 - timer / 14` (the 14 is duplicated there, line 855) and draw.c shows `PRINTING %ds`.

### Sir Cutsalot (ST_CUTSALOT), Bench (ST_BENCH)

Stateless. `W_STICKER` hold 1.4 s gives a sticker worth 150; `W_BENCH` hold 3.2 s gives a bench project worth 450 to 599 (lines 451, 452). No mess, no failure. Bench holding sparks blue at (bench.x + 0.8, 1.0, bench.z) (663).

### Lumber (ST_LUMBER)

Stateless. `W_LUMBER` (tap) gives IT_PLYWOOD; 45 percent chance to spawn sawdust at (s.x + hw + 0.6 + rnd(0..1), s.z + rnd(-1.2..1.2)) (453 to 457).

### Sink (ST_SINK)

`count` 0 to 10 dirty dishes. `W_DISH` hold 0.7 s per dish, +15 karma each (458 to 460). The night adds one dish every 6 rush-seconds (1042), tacos add 5 (901). Each dish adds 2.5 clutter (1057). Bots only consider it at count >= 2 (572). `goal_taken` lets several bots share it (533).

### Fridge (ST_FRIDGE)

`smell` flag. If smelling: `W_FRIDGE` hold 2.2 s, +90 karma, toast (466 to 470); adds 12 clutter (1058). If not smelling and the player has no boost: `W_MATE` (tap) gives `boost 7` seconds of 1.5x speed, `mates++`, `owes_bottle` (461 to 465). When the boost ends a bottle spawns behind the player (728 to 734). Set by event E_FRIDGE (910).

### Trash (ST_TRASH), Free Pile (ST_FREEPILE), Show & Tell (ST_SHOW)

Deposit targets, stateless except `S[ST_SHOW].timer` (4 s pedestal spin after a show, line 394, decays at 1024). Rules are in `find_target` lines 305 to 310 and `act` lines 388 to 413; see section 9.

## 8. Items

### Spawning and dying

```c
static int spawn_item(int type, float x, float z, int kind, int name);   // 150
static void kill_item(int i);                                            // 164
static void give(player_t *p, int item);                                 // 174
static void drop(player_t *p);                                           // 268
```

`spawn_item` takes the first dead slot, zeroes it, sets `holder -1`, fills `heavy` for donations, and asks `world_item_obj(type, type == IT_DONATION ? heavy : kind)` for a model. Returns -1 when all 64 slots are used; most callers ignore that (a `give(p, -1)` is harmless, line 175). `kill_item` clears the holder's `hold`, any newbie's `target` or `hold`, frees the model. `drop` places the item 0.7 ahead of the player and runs `collide` with radius 0.2.

### Classes

- Carried: IT_PLYWOOD, IT_PROJECT, IT_SCRAP, IT_BOTTLE, IT_SPAGHETTI, IT_DONATION.
- Cleaned in place (`is_mess`, line 130): IT_SAWDUST, IT_PUDDLE, IT_DAISY.
- Junk (`is_junk`, line 131): IT_SCRAP, IT_BOTTLE, IT_SPAGHETTI, IT_DONATION.

### Clutter weight (`weight()`, lines 133 to 148)

| Type | Weight | Red tagged (age > 25) |
| --- | --- | --- |
| IT_PLYWOOD | 2 | 3 |
| IT_SCRAP | 4 | 6 |
| IT_BOTTLE | 3 | 4.5 |
| IT_SPAGHETTI | 4 | 6 |
| IT_DONATION light | 9 | 13.5 |
| IT_DONATION heavy | 14 | 21 |
| IT_SAWDUST | 5 | 7.5 |
| IT_PUDDLE | 7 | 10.5 |
| IT_DAISY | 24 | 36 |
| IT_PROJECT | 0 | 0 |

Only items with `holder < 0` count (line 1056). `age` only advances with `holder < 0` (1026).

### Where a carried item goes

Three places encode the destination and they must agree:

1. `find_target` when holding (lines 302 to 313): TRASH accepts junk or plywood (plywood pays nothing); FREE PILE accepts donation, scrap or plywood; SHOW accepts a project; LASER accepts plywood when IDLE.
2. `player_update` HUD arrow (686 to 697): project to SHOW, donation to FREE PILE, junk to TRASH, plywood to LASER if IDLE else to FREE PILE with label "LASER BUSY: FREE PILE".
3. `choose_goal` for bots when holding (540 to 547): project to SHOW, donation to FREE PILE, plywood to LASER if IDLE (else no goal, and `bot_think` line 598 presses B to drop it), everything else to TRASH.

Newbies (members) carry junk to TRASH and donations to FREE PILE (`npc_update` line 790).

### item_name (113 to 128)

Projects resolve through `kind` to `LASER_PROJECTS`, `PRINT_PROJECTS`, `BENCH_PROJECTS`, `STICKERS`; donations through `DONATIONS[name].name`; the rest are fixed strings. draw.c `floor_tags` (243 to 270) labels messes SWEEP ME / MOP ME / UNPLUG ME, projects by name, donations by name plus TO FREE PILE, other junk TRASH ME, plywood nothing.

## 9. Work kinds, hold times, actions and scoring

### Work kinds (line 65)

`W_NONE, W_PICK, W_DEPOSIT, W_LOAD, W_CUT, W_TAKE, W_CLEAN_LASER, W_PRINT, W_FIX, W_STICKER, W_BENCH, W_LUMBER, W_DISH, W_MATE, W_FRIDGE, W_FLOOR, W_GREET`

### hold_time (lines 68 to 80)

```c
static float hold_time(int kind, int ref);
```

| Kind | Seconds | Kind | Seconds |
| --- | --- | --- | --- |
| W_CUT | 2.4 | W_DISH | 0.7 |
| W_CLEAN_LASER | 2.0 | W_FRIDGE | 2.2 |
| W_FIX | 2.4 | W_FLOOR daisy | 2.0 |
| W_STICKER | 1.4 | W_FLOOR puddle | 1.5 |
| W_BENCH | 3.2 | W_FLOOR sawdust | 1.2 |
| everything else | 0 (a tap) | | |

### find_target (lines 293 to 355)

```c
typedef struct { int kind, ref; float d; const char *label; bool karma; } cand_t;
static void consider(cand_t *best, int kind, int ref, float d, const char *label, bool karma);
static cand_t find_target(player_t *p);
```

Returns the nearest valid action. Holding: only the four deposit or load options. Empty handed, in order: floor items within 1.15 (messes become W_FLOOR, others W_PICK), then stations within REACH by a `switch (i)` on the station index, then waiting newbies within 1.6 (distance biased by -0.5 so a newbie wins ties). The `label` becomes the prompt under the player; `karma` colours it teal.

| Station | Condition | Kind | Label | Karma |
| --- | --- | --- | --- | --- |
| ST_LUMBER | always | W_LUMBER | GRAB PLYWOOD | no |
| ST_LASER | LOADED | W_CUT | CUT | no |
| ST_LASER | BROKEN | W_CLEAN_LASER | CLEAN THE LASER | yes |
| ST_PRINT1/2 | IDLE | W_PRINT | START A PRINT | no |
| ST_PRINT1/2 | DONE | W_TAKE | TAKE THE PRINT | no |
| ST_PRINT1/2 | BROKEN | W_FIX | UNCLOG IT | yes |
| ST_CUTSALOT | always | W_STICKER | CUT A STICKER | no |
| ST_BENCH | always | W_BENCH | SOLDER SOMETHING | no |
| ST_SINK | count > 0 | W_DISH | WASH A DISH | yes |
| ST_FRIDGE | smell | W_FRIDGE | DEAL WITH THE FRIDGE | yes |
| ST_FRIDGE | no smell, boost <= 0 | W_MATE | DRINK A CLUB-MATE | no |
| holding, ST_TRASH | junk or plywood | W_DEPOSIT | THROW OUT | yes |
| holding, ST_FREEPILE | donation, scrap, plywood | W_DEPOSIT | FREE PILE IT | yes |
| holding, ST_SHOW | project | W_DEPOSIT | SHOW IT OFF | no |
| holding, ST_LASER IDLE | plywood | W_LOAD | LOAD THE LASER | no |
| floor item | mess | W_FLOOR | UNPLUG IT or CLEAN UP | yes |
| floor item | other | W_PICK | PICK UP | yes if junk |
| newbie state 1 | within 1.6 | W_GREET | SAY HI | yes |

### act (lines 380 to 488) and the scoring tables

```c
static void act(player_t *p, int kind, int ref);
static void karma(player_t *p, int v);              // 373: karma += v, tasks++, teal burst, popup, SFX_KARMA
static int project_value(int kind);                 // 357
static int make_project(player_t *p, int kind);     // 366
```

Clout (only when deposited at SHOW, line 392):

| Source | kind | Value |
| --- | --- | --- |
| Laser | 0 | 900 + rnd(0..299) |
| Print | 1 | 600 + rnd(0..199) |
| Bench | 2 | 450 + rnd(0..149) |
| Sticker | 3 | 150 |

Showing off also gives VIBE +1, confetti, a toast, SFX_GLAMOUR, and an "OVER 9000" ticker when a player crosses 9000 (400 to 403). Clout is never zeroed on a lost night in game.c; the "all clout is void" rule is applied only by draw.c `results_screen` sorting by karma instead of clout when `G.end != END_SURVIVED` (draw.c 672 to 677).

Karma:

| Action | Karma | Line |
| --- | --- | --- |
| Trash scrap | 40 | 405 |
| Trash bottle | 30 | 405 |
| Trash spaghetti | 50 | 405 |
| Trash donation | 50 | 405 |
| Trash plywood | 0, "WASTED PLYWOOD" popup, SFX_DROP | 406 |
| Free pile donation | 90, plus toast | 408, 410 |
| Free pile scrap | 40 | 408 |
| Free pile plywood | 20 | 408 |
| Clean laser | 80 | 433 |
| Fix printer | 80 | 447 |
| Wash a dish | 15 | 459 |
| Fridge | 90 | 468 |
| Floor sawdust | 40 | 474 |
| Floor puddle | 50 | 474 |
| Floor daisy chain | 120, plus toast | 474, 475 |
| Greet a newbie | 100, VIBE +8, newbie becomes a member for 40 s | 478 to 485 |

Proposals (X button, lines 678 to 682): `bubble 2.4`, random `PROPOSALS` line, `proposals++`, a "+0 CLOUT" popup in smoke grey, SFX_TALK. Zero effect on anything else.

## 10. Meters and end conditions (game_update lines 1054 to 1076)

Clutter target `c` each frame: sum of `weight()` over floor items, plus `sink.count * 2.5`, plus 12 if the fridge smells, plus 6 if the laser is broken, plus 4 per broken printer. `G.clutter += (c - G.clutter) * min(1, dt * 3)` (1061): a smoothed follow, about a third of a second to settle.

Vibe (starts 70, max 100):

- clutter > 60: `vibe -= dt * (clutter - 60) / 40 * 2` (so -2 per second at clutter 100) (1062).
- clutter < 25: `vibe += dt * 0.15` (1063).
- always: `vibe -= dt * 0.12` (1064); over a 180 s night that is 21.6 points of entropy.
- discrete: show off +1, greet +8, newbie leaves -12, tacos +10, coffee +6, landlord -15 or +10, each newbie cleanup +1.

Fire: when `clutter >= 100`, `fire_warn` accumulates (first frame fires the FIRE MARSHAL ticker, SFX_ALARM); below 100 it decays at 0.5 per second (1065 to 1068). draw.c flashes the frame red and counts down `5 - fire_warn`.

End (1074 to 1076): `fire_warn >= 5` gives END_FIRE; `vibe <= 0` gives END_BURNOUT; `t >= night_len` gives END_SURVIVED. Closing-time warning and music mode 3 at `night_len - 30` (1070 to 1073), not in attract.

## 11. Rush and the spawn timers (game_update lines 1036 to 1045)

```c
float rush = (1 + 0.8f * G.t / G.night_len) * (1 + 0.12f * (player_count() - 1));
dt *= rush;   // the four spawn timers below run on scaled time
```

So at the end of a night with 8 players, spawns run 1.8 x 1.84 = 3.3 times faster than at the start alone. `G.heat` is decremented by `dt / rush`, which is real time. After the four timers, `dt /= rush` restores real time for the landlord, meters and end checks. Bots count as players for rush.

| Timer | Start value (game_start_night 950) | Refill | Effect |
| --- | --- | --- | --- |
| next_event | 14 | 17 + rnd(0..8) | `fire_event()` |
| next_newbie | 6 | 13 + rnd(0..7) | `spawn_newbie()` |
| next_dish | 8 | 6 | sink count +1 up to 10 |
| next_donation | 24 | 24 + rnd(0..10) | `spawn_donation()` |

`G.landlord` runs on real time (1046 to 1052): at zero, clutter > 50 gives VIBE -15 and a rust ticker, otherwise VIBE +10 and a teal ticker.

In `PH_TITLE` (1028 to 1033) a newbie spawns every 9 s. In practice `PH_TITLE` lasts 1.5 s before main.c starts the attract night, so this branch almost never fires.

## 12. EVENTS (game.c lines 844 to 918)

```c
enum { E_NONE, E_HEAT, E_HABOOB, E_TACOS, E_LANDLORD, E_DAISY, E_DONATION, E_STRANGE, E_FRIDGE, E_SPAGHETTI, E_COFFEE };
static const struct { const char *text, *sub; int effect; int weight; } EVENTS[13];
static void fire_event(void);        // 878
static void spawn_donation(void);    // 861
static void floor_spot(float *x, float *z);   // 869
```

| # | Banner | Sub line | Effect | Weight | What happens (fire_event switch, 888 to 917) |
| --- | --- | --- | --- | --- | --- |
| 0 | LINUX NIGHT: CANCELED. | NOTHING HAPPENS. AS USUAL. | E_NONE | 1 | nothing |
| 1 | QUIET NIGHT. NOTHING HAPPENS. | ENJOY IT. | E_NONE | 1 | nothing |
| 2 | 5TH THURSDAY?! IT'S BLASPHEMY! | IT IS TUESDAY. NOTHING HAPPENS. | E_NONE | 1 | nothing |
| 3 | IT IS 118 OUTSIDE. | EVERYONE SLOWS DOWN. THE AC LEAKS: MOP IT UP. | E_HEAT | 3 | `heat 12` (0.7x speed, orange tint); puddle at x -9.5..-7.5, z -3..0 (kitchen side); second puddle at a random floor spot |
| 4 | HABOOB! | DUST ON THE FLOOR AND IN THE LASER. SWEEP IT. | E_HABOOB | 3 | laser dust +45 (breaks it if IDLE); three sawdust at random floor spots with dust bursts |
| 5 | SOMEBODY BROUGHT TACOS. | VIBE UP. SO ARE THE DISHES. | E_TACOS | 3 | VIBE +10; sink +5 (cap 10) |
| 6 | LANDLORD WALKTHROUGH IN 10 SECONDS. | GET CLUTTER UNDER 50 OR LOSE VIBE. | E_LANDLORD | 3 | `landlord 10` |
| 7 | POWER STRIP DAISY CHAIN DETECTED. | UNPLUG IT. THE FIRE MARSHAL HATES THESE. | E_DAISY | 3 | one IT_DAISY at a random floor spot (24 clutter) |
| 8 | DONATION AT THE FRONT DOOR. | CARRY IT TO THE FREE PILE. | E_DONATION | 3 | two donations on the door mat |
| 9 | ANY PARTICULAR REASON WHY DR. STRANGE IS DOWN? | THE PRINTER JAMMED. HOLD YOUR BUTTON ON IT TO FIX. | E_STRANGE | 2 | PRINT2 BROKEN, spaghetti in front of it |
| 10 | MYSTERY FOOD IN THE FRIDGE. | SOMEONE HAS TO DEAL WITH IT. HOLD ON THE FRIDGE. | E_FRIDGE | 3 | fridge smell |
| 11 | PRINT LEFT RUNNING. | SPAGHETTI ON THE FLOOR. TRASH IT. | E_SPAGHETTI | 2 | spaghetti in front of a random printer |
| 12 | COFFEE AND COWORKING RAN LONG. | GOOD VIBES. | E_COFFEE | 2 | VIBE +6 |

Selection (878 to 885): weighted random over `weight`; if the pick equals the previous `G.event`, it is replaced by `(e + 1) % 13` without regard to weight (so a zero-weight event can still fire if it follows a repeat). Every event shows `ticker_sub(text, sub, C_HAZARD)` for 5.5 s and plays SFX_ALARM.

`spawn_donation` (861 to 867): random `DONATIONS` entry at x in [-8.7, -7.3], z in [5.7, 6.3] (the door mat), toast "DONATED: ...". Also called once at night start and on the donation timer.

`floor_spot` (869 to 876): up to 20 tries for x in [-10, 10], z in [-4.5, 5.5] with `clear_spot(margin 0.6)`. If all 20 fail it returns the last (unclear) spot: there is no failure path.

## 13. Newbies (lines 738 to 840)

```c
static void spawn_newbie(void);                        // 740
static bool walk(npc_t *n, float dt, float speed);     // 760, true when within 0.2 of (tx, tz)
static void npc_update(npc_t *n, float dt);            // 770
```

`spawn_newbie`: refuses if 3 or more newbies are in state 0 or 1. Spawns at the door (DOOR_X, DOOR_Z), picks a waiting spot with x in [-7, 5], z in [-0.5, 5.0] that passes `clear_spot(0.8)` (30 tries), `patience 20`, grey model (`world_person(0x8b8578, true, ...)`), toast.

State machine:

| State | Behaviour | Exit |
| --- | --- | --- |
| 0 entering | walk at 2.4 toward (tx, tz); `work` counts seconds | arrived or `work > 6` (stuck in a doorway): state 1 |
| 1 waiting | `patience -= dt` only in PH_NIGHT; draw.c shows a ? and a ring | patience <= 0: state 4, target the door, VIBE -12, `NEWBIE_LEAVES` toast, SFX_BAD. Or a player greets: state 2, `life 40` |
| 2 helping | `life -= dt`. If holding: walk to the deposit face (TRASH x - 1.1 or FREEPILE x - 1.3, same z) at 2.6, consume the item there (VIBE +1). Else pick the nearest untargeted mess or junk (another newbie's target is skipped), walk at 2.6; messes take 3.5 s of `work` then vanish (VIBE +1), junk is picked up (`holder = 100 + idx`). With nothing to do, wander at 1.6 to random spots x in [-8, 8], z in [-3, 5] every 5 s | life <= 0: state 3, one in three chance of the "FOUR YEAR SINE WAVE" toast |
| 3 leaving | walk at 2.4 to (DOOR_X, DOOR_Z + 0.5) | arrived: freed |
| 4 sulking out | walk at 3.2 to the same | arrived: freed |

Newbies collide with radius 0.36 and ignore players entirely (no player push). Items a newbie holds have `holder >= 100`, which `find_target` and bots treat as "not on the floor" and `kill_item` treats as "not a player".

## 14. Bots (lines 491 to 636)

```c
static const char *const BOT_NAMES[] = { "", "BUILDER", "TALKER", "FOUNDER" };
int game_add_bot(int arch);                 // 497: wraps game_add_player, preserves G.phase, sets bot and talk_in
void game_remove_bots(void);                // 507
static bool goal_valid(player_t *p);        // 511
static bool goal_taken(player_t *p, int kind, int ref);   // 531: another bot has it, except W_DISH
static void choose_goal(player_t *p);       // 537
static void goal_point(player_t *p, float *tx, float *tz);   // 579
static void bot_think(player_t *p, float dt);   // 593
```

`game_add_bot` sets `adding_bot` so `game_add_player` skips the join toast, SFX and the attract-ending branch, then restores `G.phase` because `game_add_player` would otherwise move PH_TITLE to PH_LOBBY (line 264).

### choose_goal scoring

Glamour weight `g`: FOUNDER 0.85, TALKER 0.55, BUILDER 0.15 (line 538). For each candidate:

```
score = base * (glam ? g : 1 - g) / (1 + distance * 0.12) + rnd(0..0.25)
```

Highest score wins, unless `goal_taken`. Holding an item short-circuits to a deposit goal (section 8).

| Candidate | base | glam | Condition |
| --- | --- | --- | --- |
| W_FLOOR daisy | 2.0 | no | |
| W_FLOOR other mess | 1.2 | no | |
| W_PICK junk | 1.0 | no | |
| W_PICK project on the floor | 2.0 | yes | |
| W_CUT | 2.2 | yes | laser LOADED |
| W_LUMBER | 1.6 | yes | laser IDLE |
| W_CLEAN_LASER | 1.4 | no | laser BROKEN |
| W_PRINT | 0.9 | yes | printer IDLE |
| W_TAKE | 1.8 | yes | printer DONE |
| W_FIX | 1.3 | no | printer BROKEN |
| W_BENCH | 0.8 | yes | always |
| W_STICKER | 0.35 | yes | always |
| W_DISH | 0.5 + count * 0.12 | no | sink count >= 2 |
| W_FRIDGE | 1.5 | no | smell |
| W_GREET | 1.6 | no | newbie waiting |

Bots never drink Club-Mate and never propose except the TALKER's timed X press (line 602: every 6 to 14 s).

### goal_valid (511 to 529)

Per kind, checks the thing still exists and the hands match. Note W_LUMBER requires the laser IDLE (a bot will not fetch plywood while it is loaded), and W_LOAD requires the laser IDLE.

### goal_point (579 to 591)

Clamps the bot's position to the station box expanded by 0.55, then forbids wall-side faces by three hard-coded tests: `s->z < -4` means a back-wall station (approach from +z), `s->x < -10` left wall (approach from +x), `s->x > 10` right wall (approach from -x). A layout that puts a station elsewhere needs this generalised.

### bot_think (593 to 636)

- Rethinks when the goal is W_NONE, invalid, or `think` ran out (3 to 5 s). If holding plywood while the laser is not IDLE, presses B to drop it first (598).
- Escape mode (603 to 610): walks straight to (esc_x, esc_z) for up to 1.5 s.
- With a goal: if `find_target` already returns the goal (ref matches, or any W_DEPOSIT / W_LOAD), it holds A for hold tasks or taps A (edge against `prev`) for instant ones. Otherwise it steers toward `goal_point`, slowing within 0.6.
- Stuck detection: moving less than 0.02 per frame accumulates `stuck`; past 0.5 s it sidesteps (`side` is random, `stuck` set to -0.6 to time the sidestep, `stuck_total += 0.5`). `stuck_total > 3` triggers escape to a random `clear_spot(0.8)` within 6 units and clears the goal.
- Writes `p->btn, p->sx, p->sy` so the rest of `player_update` treats a bot like a human.

## 15. Players

### Joining and leaving

`game_add_player(bool phone, int pad, const char *pid, const char *name)` (233 to 266). If a human joins during attract (`G.attract && !adding_bot`): bots removed, world cleared, PH_LOBBY, `calm_meters()`, toasts cleared, lobby music. Picks the first free slot, zeroes the player, lobby position `x = DOOR_X + (slot % 4) * 0.8 - 1.2` (so -9.2, -8.4, -7.6, -6.8), `z = 5.6 - (slot / 4) * 0.9`, facing pi (toward -z, into the room). Colour `PCOL[slot]`. Join toast, SFX_JOIN, FX_JOIN unless a bot. PH_TITLE becomes PH_LOBBY.

`game_remove_player(int slot)` (279): drops the carried item, frees the model.

`game_player_input(int slot, uint32_t btn, float sx, float sy)` (288): stores the frame's input; main.c calls it for pads, keyboard seats and phones.

### player_update (638 to 736)

Per frame, in order:

1. `bot_think` if a bot.
2. Edge detect A, B, X against `prev`; decay `flash` and `bubble`.
3. `find_target`; `need = hold_time`. If holding A on a hold task: reset `work` when the candidate changed, accumulate, play a tick (SFX_LASER on a cut) every quarter second with a matching particle burst, and `act()` when `work >= need`. Otherwise a tap (edge) on an instant action calls `act()`.
4. `G.laser_cutting = 0.1` while cutting (670).
5. Prompt text and karma flag for the HUD (672 to 675).
6. B drops; X proposes (677 to 682).
7. Carry destination for the HUD arrows (684 to 697).
8. Movement (699 to 720): stick normalised, dead zone 0.15, frozen while `working`. Base speed 5.0; x1.5 with boost; x0.7 during heat; x0.55 carrying a heavy donation. Facing turns at rate `dt * 14`. `bob` advances at 13 per second. `idle` resets on movement or any held button.
9. `collide(&x, &z, 0.38)`, then a soft push away from other players within distance 0.74 (722 to 726).
10. Club-Mate timer and the bottle it leaves (728 to 734).
11. The held item follows the player (735).

### collide (203 to 229)

Clamps to the room bounds (section 2), then for every station and obstacle treats the player as a circle of radius r against the box: nearest point on the box, push out along that normal; if the centre is inside the box, leave through the nearest face. Linear over `ST_COUNT + OB_COUNT` boxes, no broad phase. Called with r 0.38 for players, 0.36 for newbies, 0.2 for dropped items.

### clear_spot and box_dist

`box_dist(x, z, const station_t *s)` (186): Euclidean distance from a point to a station's box (0 inside). `clear_spot(x, z, margin)` (194): no station within `margin` by box distance, and no obstacle within `margin` on both axes. Used for newbie targets, floor spots and bot escapes.

## 16. The night flow

```c
static void clear_world(void);     // 922: kill all items, free all newbies, reset every station, clear every hold
void game_init(void);              // 929: S[] from LAYOUT, solid true, NPC indices -1, PH_TITLE, vibe 70, event -1, night_len 180 if unset
void game_reset_world(void);       // 942: clear_world
void game_start_night(void);       // 944
static void calm_meters(void);     // 970: fire_warn, heat, landlord, clutter 0, vibe 70, laser_cutting 0, ticker off
static void end_night(int why);    // 975
void game_update(float dt);        // 991
```

`game_start_night` (944 to 968): `clear_world`; if not attract and exactly one human and one player, adds BUILDER and TALKER bots (947); PH_NIGHT, END_NONE, t 0; clutter 0, vibe 70, fire_warn 0, heat 0, landlord 0; timers 14, 6, 8, 24; `nights++`; sink 3; every active player's tallies zeroed and placed at `x = -1.5 + (i % 4) * 1.2` (so -1.5, -0.3, 0.9, 2.1), `z = 4.3 + (i / 4) * 0.9` (in front of Show & Tell); two sawdust at random floor spots, one scrap at (-6.5, -4.0), one donation at the door; the opening ticker; music 1 in attract or 2 in play; SFX_ALARM in play.

`end_night(why)` (975 to 989): in attract it resets to PH_TITLE and the attract loop restarts via main.c after 1.5 s. Otherwise PH_RESULTS, `calm_meters` with clutter and vibe preserved for the results screen, lobby music, SFX_WIN or SFX_LOSE, everyone drops what they hold.

`game_update(dt)` (991 to 1077): advance `t`, toasts, ticker, shake. In PH_RESULTS only mirror `prev = btn` and return. Otherwise update players, newbies, tick the printers (unscaled), decay `laser_cutting` and the Show & Tell pedestal, age floor items. PH_TITLE: attract newbies, return. Not night: return. Night: section 11 spawns, landlord, section 10 meters, closing warning, end checks.

### main.c flow() (lines 343 to 374), the phase transitions

| Phase | Rule |
| --- | --- |
| PH_LOBBY | nobody for 6 s: PH_TITLE. Else start the night on a Start edge from a seated player (`start_pressed`) or at `G.t >= 25` |
| PH_NIGHT | attract: nothing. No humans: remove bots. Nobody for 12 s: PH_TITLE, lobby music |
| PH_RESULTS | `(G.t > 4 and any A or Start edge) or G.t > 40`: remove bots; PH_LOBBY if humans remain else PH_TITLE |
| PH_TITLE | after 1.5 s: `attract = true`, add BUILDER, TALKER, FOUNDER, `game_start_night()` |

Seating (main.c `seat()`, 170 to 183): a pad or keyboard A or Start edge joins; the press that joins never also starts the night. Pads and keyboards idle 75 s are removed (233 to 240). Phones time out after 12 s unheard while the relay is online (248 to 256); a phone that joins during PH_RESULTS is told to hold on (287).

## 17. Toasts, ticker, popups

- `game_toast(msg, color)` (84): a 5-deep stack, 6 s each, newest at index 0.
- `ticker(msg, color)` (106) and `ticker_sub(msg, sub, color)` (99): the single banner strip, 5.0 or 5.5 s. draw.c suppresses the coach card while a ticker shows and hides the ticker band entirely under `danger()` (clutter > 80 or vibe < 20).
- `pop(p, color, fmt, v)` (93): a 1.4 s popup over a player.

## 18. Attract mode

`G.attract` is true while the demo night runs. Set in main.c (368), cleared in `game_add_player` when a human joins (238) and in `end_night` (979). While attract: `game_start_night` skips the bot top-up and the alarm, uses lobby music; `game_update` skips the closing warning; `end_night` returns to PH_TITLE; draw.c shows the attract screen (`G.phase == PH_TITLE || G.attract`, draw.c 756); `camera()` orbits.

## 19. Content tables (content.h)

| Table | Count | Used by |
| --- | --- | --- |
| TAGLINES | 8 | draw.c attract |
| ATTRACT_TIPS | 12 | draw.c attract |
| PROPOSALS | 15 | player_update X, draw.c bubble |
| LASER_PROJECTS | 10 | make_project kind 0 |
| PRINT_PROJECTS | 8 | kind 1 |
| BENCH_PROJECTS | 7 | kind 2 |
| STICKERS | 4 | kind 3 |
| DONATIONS (name, heavy) | 8 | spawn_donation |
| NEWBIE_LEAVES | 4 | npc_update |
| NEWBIE_JOINS | 4 | act W_GREET |
| FIXED_PRINTER (with %s) | 3 | act W_FIX |
| AWARD_TALKER, AWARD_LURKER, AWARD_GLAMOUR, AWARD_JANITOR, AWARD_GREETER, AWARD_MATE | 1 each | draw.c results |

Awards (draw.c 686 to 694): JANITORIAL CO-OP to top karma > 0; GLAMOUR BIAS to top clout > 0 if not already awarded; TALKER at 3 or more proposals; GREETER at 2 or more greets; CLUB-MATE SOMMELIER at 3 or more mates; LURKER to the lowest `tasks + clout / 500` when 2 or more players. Sorted by clout if survived, karma otherwise.

## 20. Every hard-coded coordinate and room constant

Everything here assumes the current single layout. A layout system must own all of it.

### game.c

| Line | Constant | Meaning |
| --- | --- | --- |
| 33 to 47 | LAYOUT | 13 station boxes |
| 49 to 59 | OBST | 9 furniture boxes |
| 61, 62 | DOOR_X -8.0, DOOR_Z 7.9 | the front door |
| 63 | REACH 0.85 | |
| 204 to 208 | -11.6, 11.6, -6.6, 7.0, 0.9, 8.5 | walkable bounds and doorway width |
| 254 | DOOR_X + (slot % 4) * 0.8 - 1.2; 5.6 - (slot / 4) * 0.9 | lobby spawn by the door |
| 272, 273 | 0.7 | drop distance |
| 319 | 1.15 | pickup radius |
| 352 | 1.6, -0.5 | greet radius and bias |
| 422 | (frnd - 0.5) * 2.0, hd + 0.6 | scrap lands in front of the laser |
| 455 | hw + 0.6 + frnd, (frnd - 0.5) * 2.4 | sawdust beside the lumber rack |
| 584 | 0.55 | bot approach margin |
| 588 to 590 | z < -4, x < -10, x > 10 | wall-side tests in goal_point |
| 628 | 6 | bot escape radius |
| 660 to 663 | 1.6, 0.8, 1.2, 1.0, 0.8 | particle offsets at laser, sink, bench |
| 721, 725 | 0.38, 0.55, 0.74, 0.5 | player radius and push |
| 750 | -7 + frnd * 12, -0.5 + frnd * 5.5 | newbie waiting area |
| 766 | 0.36 | newbie radius |
| 780, 797 | DOOR_Z + 0.5 | exit target |
| 791 | x - 1.1 (trash), x - 1.3 (free pile) | newbies approach deposit stations from the left (-x) face, which assumes both are on the right wall |
| 831 | -8 + frnd * 16, -3 + frnd * 8 | member wander area |
| 863 | DOOR_X - 0.7 + frnd * 1.4, 5.7 + frnd * 0.6 | door mat for donations |
| 871 | -10 + frnd * 20, -4.5 + frnd * 10 | floor_spot area |
| 891 | -9.5 + frnd * 2, -3 + frnd * 3 | the AC leak puddle, kitchen side |
| 908, 913 | z + 1.0, (frnd - 0.5) | spaghetti in front of a printer |
| 958 | -1.5 + (i % 4) * 1.2, 4.3 + (i / 4) * 0.9 | night start positions |
| 963 | (-6.5, -4.0) | the opening scrap |

### game.h

Lines 37 to 40 (`ST_*`), 152 (`OB_*`), 153, 154 (`obst_t`, `OBST`).

### world.c

| Line | What |
| --- | --- |
| 77 to 81 | room 24.6 x 14.3, `fx_`, `fz_` |
| 110 to 185 | `paint_floor`: concrete, saw-cut joints every 3 units; OSB wood shop at x -12.3..-6.4, z -7.15..-2.6; kitchen checkerboard (0.6 tiles, cream 0xcfc5b0 and char 0x2a241d) at x -12.3..-6.8, z -0.8..7.15; diamond plate at x 9.6..12.3, z -7.15..-1.6; ESD mat x 4.9..9.5, z -7.15..-4.8; laser mat with hazard border x -6.6..-2.6, z -7.15..-4.6; lounge rug x -7.0..-2.2, z 2.5..5.4; Show & Tell stage frame x -0.6..4.2, z 5.4..7.15; door mat x -9.0..-7.0, z 5.9..7.05 with "108 W MAIN"; dashed walkway at x -6.5 from z -0.5 to 5.8; zone stencils at fixed points; wall-edge shading 0.9 deep |
| 228 to 266 | `build_room`: walls, front kick wall stubs with the door gap, alley door at x 11.95 z 0.2, front door posts at x -8.9 and -7.1, strip lights every 6 units, laser duct at (-5.6, -6.5) |
| 281 to 456 | `build_stations`: one model per `ST_*` at `S[i].x, S[i].z`; also sets `S[i].obj`, `S[i].light`, `S[i].beam` (laser), `S[i].growth` (printers, Show & Tell); a printer PC group at (1.2, -6.5) |
| 459 to 539 | `build_furniture`: one model per `OB_*` at `OBST[i].x, OBST[i].z` |
| 541 to 640 | `build_props`, `build_signs`: string lights on the back wall, a bike at (11.85, 3.4), the whiteboard, the OPEN sign by the front door, tool shadow board |
| 681 to 705 | lights: hemisphere, sun at (-6, 16, 9) with a 32 x 24 shadow camera, kitchen lamp (-9.6, 2.9, 2.2), bench lamp (6.5, 2.9, -4.6), laser glow (-4.6, 1.8, -5.2), stage lamp (1.8, 2.8, 5.2) |
| 707 to 709 | camera default (0, 19.5, 13) looking at (0, 0, 0.9); main.c overrides each frame |
| 845 to 880 | `world_sync` dereferences `S[ST_LASER].beam`, `S[ST_PRINT1 + k].growth` and `.light`, `S[ST_SINK].obj`, `S[ST_FRIDGE].light`, `S[ST_SHOW].growth` unconditionally |

### draw.c

| Line | What |
| --- | --- |
| 177 | skips ST_COUCH and ST_TABLE |
| 179 | label heights per station: fridge 2.4, lumber 2.1, printers 2.0, else 1.3 |
| 185 to 215 | per-station status text and pay colour by index |
| 224 | "ready" glow for laser LOADED and printers DONE |
| 243 to 270 | floor tags by item type |
| 287 to 316 | goal_guides uses `S[p->goal_st]` |
| 470 to 505 | clock (7 PM to 11 PM mapped over `night_len`, `clock_str` line 93), CLUTTER and VIBE meters, FIRE MARSHAL countdown, heat tint |
| 543 to 560 | coach card: five fixed steps over the first 40 s, naming LUMBER, PEWPEW 9000, SHOW & TELL |
| 578 to 625 | how_to board with fixed flow chips |
| 658 to 760 | results and awards |

### main.c

| Line | What |
| --- | --- |
| 89 to 93 | `night_seconds` from config, clamped 20 to 3600 |
| 233, 237 | 75 s idle removal |
| 347, 350 | lobby: 6 s empty, 25 s auto start |
| 356 | night: 12 s empty |
| 360 | results: 4 s minimum, 40 s maximum |
| 367 to 370 | attract after 1.5 s with three bots |
| 380, 381 | camera (0, 21.5, 15) look (0, 0, -1.25); attract orbit at y 16.5 |

## 21. Change plan (a): selectable layouts and nights

Goal: several room layouts (different station and furniture placement) and several night scripts (length, event mix, starting mess, spawn pace), selectable in the lobby.

Keep the room shell (24.6 x 14.3, walls, camera) fixed across layouts. Everything in world.c and main.c that depends on the room size then stays as is, the camera framing stays right, and only the contents move. If a layout really needs a different size, the shadow camera, `camera()`, `paint_floor` and `collide` bounds all change together.

### Data to introduce

In game.h:

```c
typedef struct { const char *label; float x, z, hw, hd; bool present; } st_place_t;
typedef struct {
  const char *name, *blurb;
  st_place_t st[ST_COUNT];
  obst_t obst[MAX_OBST]; int nobst;       /* replaces the const OBST[OB_COUNT] */
  float door_x, door_z;                   /* replaces DOOR_X, DOOR_Z */
  float xmin, xmax, zmin, zmax;           /* walkable bounds, today -11.6, 11.6, -6.6, 7.0 */
  float lobby_x, lobby_z, start_x, start_z;   /* spawn anchors, today (-9.2, 5.6) and (-1.5, 4.3) */
  float spot_x0, spot_x1, spot_z0, spot_z1;   /* floor_spot and newbie areas, today -10..10 by -4.5..5.5 */
  int floor_style;                        /* which paint_floor variant */
} layout_t;
extern const layout_t LAYOUTS[]; extern int layout_count;
extern const layout_t *L;                 /* the active layout */
extern obst_t OBST[MAX_OBST]; extern int ob_count;   /* copies of L->obst for collide and world.c */
```

`world.c build_furniture` switches on `OB_*` to pick a model, so the obstacle entries also need a `kind` field (saw, shelf, coffee, ktable, tv, ctable, island, lathe, mill) and `build_furniture` becomes a loop over `ob_count` switching on `kind`.

A night script:

```c
typedef struct {
  const char *name, *blurb;
  float len;                              /* G.night_len */
  int event_weight[COUNT(EVENTS)];        /* replaces EVENTS[i].weight */
  float first_event, event_gap, event_jitter;      /* 14, 17, 8 */
  float first_newbie, newbie_gap, newbie_jitter;   /* 6, 13, 7 */
  float dish_gap, first_donation, donation_gap, donation_jitter;   /* 6, 24, 24, 10 */
  int start_dishes, start_sawdust, start_scrap, start_donations;   /* 3, 2, 1, 1 */
  float rush_time, rush_players;          /* 0.8, 0.12 */
} night_t;
```

### game.c functions that must change

| Function | Lines | Change |
| --- | --- | --- |
| `LAYOUT`, `OBST` | 33 to 59 | become `LAYOUTS[]` entries; today's room is layout 0 verbatim |
| `DOOR_X`, `DOOR_Z` | 61, 62 | `L->door_x`, `L->door_z` at 207, 254, 748, 780, 797, 863 |
| `clear_spot` | 194 | loop to `ob_count`; skip stations with `present == false` |
| `collide` | 203 | bounds from `L`; loop `ST_COUNT + ob_count`; skip absent stations |
| `game_add_player` | 254 | lobby anchor from `L` |
| `find_target` | 300 | skip absent stations (`if (!L->st[i].present) continue;` before `box_dist`); the four deposit checks at 305 to 312 must guard on presence |
| `goal_point` | 588 to 590 | replace the three wall tests with "pick the face whose outward point is inside the walkable bounds", or precompute an `approach` side per station in the layout |
| `choose_goal` | 548 to 576 | guard every `WANT` on presence; if the laser is absent, FOUNDER and TALKER need other glamour (bench, printers) which the weights already allow |
| `spawn_newbie` | 750 | waiting area from `L` |
| `npc_update` | 791 | deposit approach via a shared `approach_point(station)` helper, not `x - 1.1` |
| `npc_update` | 831 | wander area from `L` |
| `spawn_donation` | 863 | door mat from `L` |
| `floor_spot` | 871 | area from `L`; add a return value so callers can skip when no clear spot exists |
| `fire_event` | 878 to 885 | weights from the active `night_t`; replace the repeat bump at 883 with a re-roll that respects zero weights |
| `fire_event` E_HEAT | 891 | first puddle relative to the sink or the coffee obstacle, not a fixed rectangle |
| `fire_event` E_STRANGE, E_SPAGHETTI | 906 to 915 | guard on printer presence |
| `game_init` | 930 to 934 | becomes `game_set_layout(int idx)` that copies `L->st` into `S[]`, copies `L->obst` into `OBST`, and is callable from the lobby; `game_init` calls it with 0 |
| `game_start_night` | 947 to 964 | timers, sink count, opening mess from the active `night_t`; start anchor from `L`; the opening scrap near the laser if present |
| `game_update` | 1037 to 1043 | rush coefficients and refill values from `night_t` |
| `game_update` | 1057 to 1060 | clutter contributions guard on presence (an absent sink has count 0 anyway) |

### world.c functions that must change

- `build_stations`, `build_furniture`, `build_props`, `build_signs`, `paint_floor`: today called once from `world_init`. Add `world_set_layout(void)` that removes the previous layout's groups from `SC->base` (keep them in a `t3_object *layout_root` group so one `t3_object_remove` plus `world_free` drops everything) and rebuilds from `S[]` and `OBST[]`. The floor plane's texture is rebuilt by calling `paint_floor` again with the layout's `floor_style`; the zone rectangles in `paint_floor` must be derived from station positions (laser mat around `S[ST_LASER]`, ESD mat around the bench, checkerboard around sink and fridge, stage frame around Show & Tell) or come from a per-layout zone list.
- `world_sync` (845 to 880): null-check `beam`, `growth`, `light`, `obj` for absent stations, or always build every station and park absent ones at y -10 with `present` false so collide and find_target ignore them. The second is simpler and keeps `world_sync` untouched.
- Lights (693 to 705): kitchen, bench and stage lamps follow their stations; move them in `world_set_layout`.

### draw.c

- `station_labels` (175): skip absent stations.
- `coach` (543 to 560): the five steps name LUMBER, PEWPEW 9000 and SHOW & TELL; make the text come from the layout or the night, or make it generic ("GRAB PLYWOOD FROM THE RACK").
- `how_to` (578): fixed chips; fine if every layout keeps those stations, otherwise data.
- `lobby_overlay` (647 to 656): where layout, night and mode pickers render.

### main.c

- `flow()` PH_LOBBY (346 to 351): the 25 s auto start should reset while anyone is navigating a picker.
- Input for the pickers: there are only four button bits on the phone wire (`btn & 15`), so pickers must use the stick (left and right) plus A and B, not new buttons. Pad d-pad already maps onto the stick.

### Floor tile request

The user wants floor tiles mostly white with arrangements of orange and red rectangles made of square tiles. That is a `paint_floor` concern in world.c (lines 110 to 185): replace the concrete base with a white or bone tile grid (0.6 unit tiles like the kitchen, or 1.0), then paint rectangles of hazard amber and rust tiles per zone. The zone rectangles should come from the layout (see above) so each layout has its own arrangement. Keep the hazard border around the laser and the stage frame around Show & Tell since the HUD and README refer to them.

## 22. Change plan (b): difficulty and mode settings

Add to `game_t` (game.h 123): `int layout, night, difficulty, mode;` and a `const night_t *NT;` or a `rules_t` struct that `game_start_night` resolves from difficulty. Everything below is a constant today; the table says where each one lives so a `rules_t` can own it.

| Knob | Today | Line |
| --- | --- | --- |
| Night length | 180 (config) | main.c 92 to 93, game.h `night_len`, game.c 939 |
| Clock range | 7 PM to 11 PM over `night_len` | draw.c `clock_str` 93 |
| Closing warning | 30 s before the end | game.c 1070 to 1073, draw.c `closing = left <= 30` |
| Rush curve | 0.8 over the night, 0.12 per extra player | 1037 |
| Event timing | 14, then 17 + 8 | 950, 1040 |
| Newbie timing | 6, then 13 + 7 | 950, 1041 |
| Dish timing | 8, then 6 | 950, 1042 |
| Donation timing | 24, then 24 + 10 | 950, 1043 |
| Starting dishes | 3 | 952 |
| Starting mess | 2 sawdust, 1 scrap, 1 donation | 962 to 964 |
| Newbie patience | 20 s | 753 (draw.c 278 divides by 20 for the ring) |
| Member life | 40 s | 480 |
| Newbie cleanup time | 3.5 s | 822 |
| Max waiting newbies | 3 | 743 |
| Print time | 14 s | 436, world.c 855 |
| Print fail chance | 0.3, jams at timer < 7 | 437, 1011 |
| Dust per cut | 26 + rnd 12 | 423 |
| Haboob dust | 45 | 895 |
| Fire marshal grace | 5 s | 1074, draw.c 498 |
| Vibe curve | > 60 drains, < 25 heals 0.15, entropy 0.12 | 1062 to 1064 |
| Starting vibe | 70 | 937, 949, 971 |
| Landlord timer and threshold | 10 s, clutter 50 | 903, 1049 |
| Hold times | table in section 9 | 68 to 80 |
| Project values | section 9 | 357 to 364 |
| Karma values | section 9 | 405 to 482 |
| Player speed | 5.0, boost 1.5, heat 0.7, heavy 0.55 | 705 to 708 |
| Club-Mate boost | 7 s | 462 |
| Solo bot top-up | BUILDER and TALKER | 947 |
| Attract bots | BUILDER, TALKER, FOUNDER | main.c 369 |
| Idle timeout | 75 s | main.c 233, 237 |
| Lobby auto start | 25 s | main.c 350 |

### Co-op versus competitive

Scoring is per player (`p->clout`, `p->karma` in `act`). Nothing in game.c compares players; ranking and awards are entirely draw.c `results_screen` (658 to 760), and the per-player cards are draw.c `player_cards` (507 onward). A mode therefore needs:

- `G.mode` (competitive, co-op, chaos or whatever is chosen) set in the lobby.
- game.c: in co-op, optionally a team target (total karma, or "the space survives with VIBE above N") checked in `game_update` next to the end checks (1074 to 1076), and `END_*` extended if a new outcome exists.
- draw.c: a co-op results layout (team totals, space health, one shared award set) beside the existing ranked one; `player_cards` showing team totals.
- The "clout void on a lost night" rule is enforced only by the results sort in draw.c; in a co-op mode decide whether that rule still reads correctly.

### Where settings live and persist

There is no save data today (HANDOFF lists a hall of fame as an idea). Settings can live in `G` for the session; a wasmcart save slot is the place if they should survive a reboot.

## 23. Change plan (c): new station types and new events

### A new station, step by step

1. game.h 37 to 40: add `ST_NEW` before `ST_COUNT`. If it is a printer-like timed station, do not put it between `ST_PRINT1` and `ST_PRINT2`; the `ST_PRINT1 <= k <= ST_PRINT2` loops (game.c 564, 1007, 1060; draw.c 179, 193; world.c 320, 853) would pick it up.
2. game.c 33 to 47 (or the layout entries): label, centre, half extents. Keep every gap wider than 0.76 or closed. Approach face must be inside the walkable bounds (goal_point rule).
3. game.c 65: a `W_NEW` kind. 68 to 80: its hold time if it is a hold task.
4. `find_target` 323 to 347: a `case ST_NEW` with its condition, label and karma flag. If it accepts a carried item, add the check to the holding branch (302 to 313).
5. `act` 380 to 488: what happens, including score, SFX, FX, toast.
6. `goal_valid` 511 to 529 and `choose_goal` 548 to 576: a `WANT` with base and glam so bots use it; `goal_taken` applies automatically.
7. `player_update` 686 to 697: destination arrow if the station takes a carried item. 658 to 664: a particle burst while holding, if wanted.
8. `game_update` 1007 to 1024: per-frame ticking if it has a timer; 1055 to 1060: its clutter contribution when broken or full.
9. `clear_world` 925 already resets `state, timer, dust, count, smell`; extend if the station adds fields to `station_t` (game.h 58 to 69).
10. draw.c `station_labels` 175 to 235: label height and status text, pay colour (1 clout, 2 karma), alarm condition. `coach` and `how_to` if it is part of the core loop.
11. world.c `build_stations` 281 to 456: the model, plus `s->light` if it has a status lamp and `s->growth` or `s->beam` if `world_sync` should animate it; `world_sync` 845 to 880: the animation.
12. content.h: project names if it makes projects (then `item_name` 117 and `make_project` 367 need a new `kind` value, and `world_item_obj` world.c 714 a model for it, and `project_value` 357 a price).

Candidates from HANDOFF: the vending machine (a Club-Mate source away from the fridge), the forge, the sewing machines.

### A new item type

game.h 28 to 35 (`IT_*`); game.c `item_name` 113, `is_mess` or `is_junk` 130 to 131, `weight` 133, `hold_time` W_FLOOR 77 if a mess; the three destination tables in section 8; `npc_update` 790 and 806 if newbies should handle it; world.c `world_item_obj` 714; draw.c `floor_tags` 243.

### A new event

1. game.c 844: `E_NEW` in the effect enum.
2. 845 to 859: a row with banner (keep it to about 40 chars so it fits the band), sub line (the one-sentence explanation the README promises), effect, weight. With night scripts the weight moves to `night_t.event_weight[]`.
3. 888 to 917: a `case E_NEW` in `fire_event`. Use `floor_spot` for floor messes, `spawn_item` for things, `S[...]` for station changes, `G.vibe`, `G.heat`, `G.landlord` style timers for timed effects (add a timer field to `game_t` and decrement it in `game_update` on real time, like `landlord` at 1046).
4. If the event needs a new mess or a new station state, follow the sections above.
5. The repeat guard at 883 bumps to the next index; fix it to re-roll before adding zero-weight events.

Candidates: a class in session (a zone goes busy, no actions there for 20 s), the vending machine jams, a tour group (three newbies at once), the Wi-Fi goes down (proposals do nothing, which they already do), first Friday open house (vibe up, clutter spawn up).

## 24. Facts an implementer must not get wrong

1. Axes: -z is the back wall and +z is the front (window, door, camera side). The door is front left at (-8.0, 7.9). Left is -x.
2. The room is 24.6 by 14.3; walkable x is -11.6 to 11.6 and z is -6.6 to 7.0, with the doorway continuing to z 8.5 within 0.9 of DOOR_X.
3. Player diameter is 0.76 (radius 0.38); every gap between boxes must be wider than that or closed, or players and bots wedge. Newbies use 0.36.
4. `station_t.solid` is set and never read: `collide()` blocks on every station and every obstacle, including the couch and the work island.
5. `game_init` must run before `world_init`; world.c builds models at `S[i].x, S[i].z` and `OBST[i]`. `world_sync` dereferences the laser beam, both printers' growth and light, the sink obj, the fridge light and the Show & Tell growth every frame without null checks.
6. The laser never uses `S_BUSY` or `S_DONE`. Cutting is `G.laser_cutting > 0`. An earlier build that reused `S_BUSY` broke the plywood loop; HANDOFF records it.
7. Items held by newbies have `holder = 100 + npc index`. Code that reads `holder` must treat anything `>= 0` as "not on the floor" and only `< MAX_PLAYERS` as a player.
8. `game_update` scales `dt` by `rush` for the four spawn timers (1038) and unscales it at 1045. Printers tick before that block on real time; the landlord, meters and end checks run after it on real time. New timers go in the right block.
9. "If the space closes, all clout is void" is implemented only by draw.c `results_screen` sorting by karma on a lost night. game.c never zeroes clout.
10. The clock always reads 7 PM to 11 PM across `night_len` (draw.c `clock_str`); the 30 s closing warning and 5 s fire grace are absolute seconds.
11. `fire_event`'s repeat guard bumps to `(e + 1) % 13` ignoring weight; with per-night weights, replace it with a re-roll or a zero-weight event will fire.
12. `floor_spot` returns an unclear spot after 20 failed tries; `spawn_item` returns -1 when 64 items exist and callers do not check.
13. The phone wire carries exactly four button bits (`btn & 15`: A, B, X, START). Menus must use the stick plus A and B. Keyboard seats only exist in the browser host; RetroArch turns the keyboard into pad 1.
14. `game_add_bot` saves and restores `G.phase` around `game_add_player`, which otherwise flips PH_TITLE to PH_LOBBY. `adding_bot` also suppresses the join toast and the attract-ending branch.
15. Attract flow: PH_TITLE lasts 1.5 s, then three bots play a night; `end_night` in attract returns to PH_TITLE and it loops. A human A or Start press anywhere ends it through `game_add_player`.
16. Solo play adds BUILDER and TALKER bots at night start when exactly one human and one player are seated (947). Bots count toward `player_count()` and therefore toward rush.
17. Pads and keyboards idle for 75 s are removed by main.c; phones unheard for 12 s are removed; a phone joining during results is told to wait.
18. The two 3D printers are Loafer and Dr. Strange; BobRoss is the paper printer and is not in the game. Content stays general HeatSync lore, roles only, no names.
19. Text rules: no em or en dashes, no emoji, no bold for emphasis. Banners around 40 characters, sub lines one sentence, all caps like the rest of the HUD.
20. GANTRY colours are the `C_*` tokens in hud.h; hazard amber is a fill, the mark uses `C_LOGO_SUN` and is drawn only through `hud_logo`.
