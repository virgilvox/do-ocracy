# DO-OCRACY handoff

Where this stands, how it works, how to change it, and what nobody has checked yet. Read with the [README](README.md), which covers the game itself.

## Status

- Playable, deployed, and tested headless and in Chromium. Not yet run on the HeatSync game station hardware.
- The October 2026 overhaul is in: the storefront room as level data with five nights (docs/design/FINAL-space.md), the front end with title, menus, modes, settings, rules flash, pause, results and credits (docs/design/FINAL-frontend.md), ten new stations, the save region, and the acceptance run (`tools/accept.sh`), which is green at this handoff.
- Live: https://virgilvox.github.io/do-ocracy/ (browser player) and https://virgilvox.github.io/do-ocracy/join.html (phone controller; the QR on the cabinet points here). The overhaul is committed but not pushed; the next push redeploys it.
- Repo: https://github.com/virgilvox/do-ocracy, branch `main`. Every push redeploys `web/` to GitHub Pages (`.github/workflows/pages.yml`).
- Commits are authored as `virgilvox <hackbuildvideo@gmail.com>` (set in the repo's git config).
- The cart: `build/do-ocracy.wasc`, copied to `web/do-ocracy.wasc` by every build. About 1.3 MB, wasmcart ABI v4, GL (WebGL2 / GLES3). It imports `wc_pad_rumble` (ABI v3 rumble), which every current wasmcart host provides. romdev's `wasm conformance` passed before the overhaul (one harmless warning, see below) and has not been rerun since.

## What it is, in one paragraph

One Tuesday night of Hack Your Hackerspace at HeatSync, three minutes long, up to eight players on pads, two keyboard seats and phones. Glamour work (laser, printers, the bench) earns CLOUT for you; janitorial work (dishes, trash, the Free Pile, fixing things, greeting newbies) earns KARMA for the space. CLUTTER at 100 brings the fire marshal, VIBE at 0 is burnout, and if the space closes every point of clout is void. Archetype bots (BUILDER, TALKER, FOUNDER) run the attract mode and keep a solo player company.

## Architecture

```
wasmcart host (RetroArch core / Node / browser)
  wc_render() each frame
    main.c     input (4 pads, 2 raw-key seats, up to 8 phones), phase flow, camera, audio pump
    game.c     the simulation: players, bots, newbies, items, stations, events, meters
    world.c    the 3D room in three.c (three.js r186 API in C); floor and walls painted at startup
    draw.c     the HUD, drawn on the CPU into a 1280x720 texture (hud.c has the primitives)
    net.c      a CLASP client over wc_peer_* (WebSocket to relay.clasp.to)
    audio_bellows.cpp  the music and effects on Bellows (bellows-embedded, C++17)
```

- Two three.c renders per frame: the scene, then a full-screen quad with the HUD texture. The renderer runs with `t3_renderer_set_exact_output(true)` so the second pass composites over the first. Tone mapping is off on purpose: it would also tint the HUD and break the GANTRY colours.
- three.c draws only material states that are in a program table. The game's table is `src/gen/hsx_gl.c`, generated from `tools/states.txt` (see "Adding a render state").
- Phones: the cart itself opens `wss://relay.clasp.to` and speaks CLASP binary frames (HELLO, SUBSCRIBE, event PUBLISH with string payloads). Nothing wraps the cart, so this works in the RetroArch core, which has Node's WebSocket inside it. Protocol is at the top of `src/net.h`. The room code comes from the relay's session id on first contact (some hosts never call `wc_set_seed`).
- Input: every input source is a seat in `seats[]` (main.c: pads 0 to 3, keyboard halves 4 and 5, phones 6 to 13). `seats_poll` reads all four pads with no connected gate (an unplugged pad reads zero and the idle rule sends its player home), both keyboard halves (raw HID keys: WASD with Space or E, Q, R, Enter; arrows with period, comma, slash, right Shift; only the browser and Node hosts deliver raw keys, RetroArch and the desktop `npx wasmcart` player turn the keyboard into pad 1) and the phones' last `I<btn>,<x>,<y>`, and computes menu nav events with stick repeat (`nav_read`). `menu_update` runs next and drives the front end, then `seats_join` does the joins and the play input. A pad or keyboard player idle for 75 seconds goes home. `rumble(seat, ...)` pulses a pad seat at the moments section 7 lists.

- Settings: `SET` (game.c) holds the front end settings. `game_apply_settings` turns `SET` and the mode into the night length, the spawn pace, the bad event weight multiplier and the printer jam chance at lobby open and at night start; the attract night ignores `SET` and plays the level as written.
- Layout: the room is data in `src/levels.c` (`LEVELS[]`, one `level_t` per night: stations, furniture, props, signs, floor accents, lighting, night script; the design is `docs/design/FINAL-space.md`). `game_set_level` copies the station places into `S[]`; collision, spawn boxes, the door and the night script read the active level `L`. Bots and newbies plan their walks on a 0.2 unit grid (`nav_plan` in game.c) so the fenced shop and the counters do not wedge them. `world.c` builds the room from the same level: the tile floor and plaster walls are painted from its accents, overlays, stencils and bays, the storefront shell from its door, the furniture, props and signs from its tables by kind; `world_set_level` drops everything a builder made (objects, textures, materials) and builds the next level without touching the six lights or the camera, so a level switch adds no render state. Every gap between pieces is either walkable (wider than 0.76) or closed.
- The laser's state is `S_IDLE`, `S_LOADED` or `S_BROKEN`; "someone is cutting right now" is the separate `G.laser_cutting` timer that drives the beam and glow. (An earlier build reused `S_BUSY` for that and overwrote the real state after every cut, so the laser never needed plywood again.)
- Bots: `bot_think()` in `game.c` scores goals by archetype, walks to a target face of the station, presses or holds A. A bot that makes no progress for 3 seconds walks to an open spot and rethinks.

### The level file

`src/levels.c` holds `LEVELS[LEVEL_COUNT]`; read its header comment before editing. The rules it states, and that `level_check` enforces:

- `ST_COUNT`, the `ST_` order and `OBK_KIND_COUNT` are fixed per build. A level can move, hide (`present` false) and relabel a station but never invent a kind; draw.c, world_sync and the three destination tables in game.c switch on station ids. Nothing goes between `ST_PRINT1` and `ST_PRINT2`.
- Absent stations are still built by world.c and parked at y -20, because world_sync dereferences the laser, printer, sink, fridge and show pointers every frame without checks.
- `ST_SWITCH` and `ST_BACKDOOR` live inside the wall planes and are skipped by collide and by the pair test.
- Axes: -z is the back wall, +z the street and the camera, -x screen left. Tiles are 0.6 units, 41 by 24 from (-12.3, -7.15); accents are whole tiles (`c, r, cw, rh, ring`), a checker is exactly 2 by 2, overlays and stencils are world units, stencils sit on white or grey tiles.
- The gap rule: two solids touch (at most 0.05 apart on both axes, or a declared nest: a printer or the cutter in its counter, the fence boxes at their corner) or leave at least 0.76 on one axis with no diagonal pinch under 0.76, unless a third box sits between them. A face 0.05 to 0.76 from a collide clamp line is a wedge unless another box fills that strip. 0.76 to 1.00 is legal and logged as NARROW.
- Every present station's approach point (its face pushed out 0.55) is inside the clamps and at least 0.38 from every other box; the 8 lobby and 8 night start places are 0.4 clear; the spawn boxes are at least 40 percent clear at a 0.6 margin; signs and props stay inside the room. The free standing props a body walks round (shop vac, tool cart, water, buckets) are 0.3 discs: an error inside a solid or on an approach point, a NEAR warning in a pinch.

`level_check(lv, out)` (src/levelcheck.c) runs on every room world.c builds and logs any finding to the host log, and `tools/smoke.c` runs it natively over all five levels (`build/smoke`, exit 1 on any error; `build/smoke --json` dumps the tables). `tools/gapcheck.py` is an independent Python port of the same rules that reads that dump; run both after every edit to levels.c (`tools/accept.sh` does).

### The front end state machine

All of it lives in `M` (`menu_t`, game.h), written by main.c and read by draw.c (`front_end`, `lobby_overlay`, `pause_overlay`, `rules_flash`, `results_screen`).

```
PH_TITLE or the attract night (G.attract)
  SCR_TITLE --20 s idle--> SCR_ATTRACT --any input--> SCR_TITLE
  SCR_TITLE / SCR_ATTRACT --A or START on a seat--> SCR_MAIN (that seat is M.owner)
  SCR_MAIN rows: PLAY -> front_end_play (PH_LOBBY, SCR_NONE; SCR_FLASH first when flash_due())
                 MODES -> SCR_MODES -> SCR_MAIN (CUSTOM: SCR_SETTINGS -> lobby)
                 SETTINGS -> SCR_SETTINGS -> SCR_MAIN
                 HOW TO PLAY -> SCR_FLASH -> SCR_MAIN
                 CREDITS -> SCR_CREDITS -> SCR_MAIN on A, B or START
PH_LOBBY   READY (START) per player, M.ready_t 3 s countdown, M.lobby_t 25 s auto start,
           hold B to leave, G.wipe_t 0.6 s tape wipe, game_start_night at the closed frame
PH_NIGHT   START pauses (G.paused, M.pauser): RESUME, HOW TO PLAY (SCR_FLASH over the pause), QUIT TO MENU
PH_RESULTS 4 s tape lock, then A or START back to the lobby, B to SCR_MAIN owned by the presser (menu_with_owner)
```

- The front end is up when `G.phase == PH_TITLE || G.attract` (`front_end_up`); never test `PH_TITLE` alone, because the attract night is a real `PH_NIGHT`. `seats_join` never seats anyone while the front end is up; seats that press A there are denied once and wait AT THE DOOR (`pending`), and `front_end_play` seats the owner first and then them. In SCR_ATTRACT any button edge (A, B, X or START) claims the menu, not only the nav bits, so a keyboard player who hits Q or R still lands on the main menu instead of watching the demo; the overlay says DEMO and PRESS A OR SPACE TO PLAY so nobody takes the bots' night for a tutorial.
- The play camera (`camera()`, main.c) looks at z -0.6, not the room's centre, so the storefront's base sits at about screen y 652 and the player cards (`CARD_Y` 660, draw.c) have the dark band under it to themselves; they must never cover the show window, the vestibule or the sidewalk. The first five seconds of a real night draw YOU ARE <seat> over every human (`players_overlay`) and pulse their card; the coach's first two lines and the first rules card name the group's or the owner's key (`group_key`, `flash_cap0`).
- The owner idle rule: 20 s without input on a menu hands the menu to the lowest pending seat, or back to the title. `new_group()` (the lobby empty for 6 s, or a night with no humans for 12 s) resets `M` and the group's `flash_seen` and `group_nights`.
- The rules flash is `flash_update` (main.c) and `rules_flash` and `flash_card` (draw.c): five cards of 2.6 s with 0.3 s wipes, A next, B skip. It holds the lobby clock and READY, takes only the owner's (or the pauser's) buttons, and sets `G.flash_seen` when it ends.
- Settings: `settings_input` and `settings_step` (main.c); `settings_pinned(row)` is shared with draw.c and dims the rows the mode decides. MUSIC and SFX reach the band through `audio_set_levels` (half music while paused). ROOM steps `SET.layout` through `LEVELS[]` and PLAY builds that level before seating anyone.
- Results: `END_*_V` picked by `G.end_variant`, the space strip, one award per player, the VERSUS frame, the CO-OP grade, LAB RECORD chips from `save_records`, confetti over the show window on a survived night. The survived eyebrow reads the level's closing hour.
- Credits: `credits_screen` (draw.c), lines from `CREDITS[]` in content.h (roles and places only), and the lab records from the save region, mirrored into `M.lab_*` every frame by `menu_update`. The title footer shows the karma record the same way.

### The save region

`save_t` in main.c is exposed through `wc_get_info` (`save_ptr`, `save_size`, 108 bytes) and persisted by the host after each frame: magic `DOOC` (0x434f4f44), version 1, a crc32 of everything after the crc, the 16 byte `settings_t`, `nights_played`, the best karma and clout with names, the best co-op team karma, and 32 reserved bytes. `save_load` runs before `game_init`; a wrong magic, version or crc, or any field out of range, loads the defaults, so a never saved cabinet (all zero) boots to SOLO, NORMAL, bots on, phones on, music 7, sfx 8, CLASSIC TUESDAY, help FIRST NIGHT. Call `save_commit()` after any change to `SET` or the records. New fields go into `reserved` first so the size stays fixed. To test: `SAVE=out.bin SAVE_IN=in.bin node tools/shot.mjs ...` dumps the region after the run and loads one before it, and `node tools/accept/check-save.mjs out.bin mode=1 music=4` checks it.

## Build and test

Needs Emscripten (`~/emsdk`), Node 22+ (24 for romdevtools), and `git submodule update --init`, then `npm install`.

```sh
./build.sh                         # build/do-ocracy.wasc and web/do-ocracy.wasc
./build.sh smoke                   # the pipeline smoke cart (tools/pipeline-smoke.c)
JOIN_URL=http://<lan-ip>:8747/join.html ./build.sh   # QR points at a LAN phone page
REGEN_STATES=1 ./build.sh          # regenerate the render program table (see below)
npm run serve                      # web/ on port 8747, prints the LAN address
```

The acceptance run, green at this handoff:

```sh
tools/accept.sh                    # build, then every check below; prints GREEN or RED and the shots to read
NOBUILD=1 tools/accept.sh          # reuse build/do-ocracy.wasc
```

It does, in order: `build/smoke` (level_check over the five levels, natively), `tools/gapcheck.py` (the Python port over `build/smoke --json`), `tools/lint-content.py` (dashes and emoji anywhere, bytes outside printable ASCII and lower case in game strings, glyph rows of 16, flash captions of six words at most, toasts and awards up to 64 characters, banners up to 48), then the scripted sessions: `tools/accept/idle.json` (title, attract and back), `claim.json` (claim, deny, MODES, SETTINGS, PLAY, flash, lobby, READY, a CO-OP night with a pause, results, B to the menu; it also dumps the save region and a WAV), `second-night.json` (the same group again: no flash, no coach), `regroup.json` (a solo player idles out, the group resets to the title and the next PLAY plays the flash again), `attract.json` (30 s untouched: the title's PRESS A OR SPACE, the DEMO tag, A from the demo to the main menu, PLAY, the first rules card, READY, YOU ARE PAD 1 and the coach's first line in the night), `idle-owner.json` and `idle-owner2.json` (the owner idle rule with and without a seat at the door), `lobby-geom.json` (lobby geometry and the owner's hold B to the menu), `credits.json`, and the five nights in `tools/scripts/level-k.json`. Each log is grepped for the level_check errors, BOT_STILL (a bot that moved less than 0.5 in 10 s outside a hold, printed by game.c) and missing or generated render states; the save region goes through `tools/accept/check-save.mjs` and the WAV through `tools/wavpeak.mjs` (any full scale sample fails). At the end of each night game.c prints VISITS (bot visits per station) and a NOVISIT line for each station no bot went near; those are notes, not failures. The screenshots land in `build/shots/acc-*.png`; check them against the tables in FINAL-frontend.md section 10 and FINAL-space.md section 13 by eye. The runs share the GPU and run slower together than one after another, so the default is one at a time; the whole run takes about twenty minutes on an Apple Silicon Mac.

Other tools:

```sh
node tools/shot.mjs build/do-ocracy.wasc build/shots/s 60,600 [script.json]     # headless GL screenshots, scripted pads
WAV=out.wav node tools/shot.mjs ...                                              # plus the audio
PACE=1 node tools/shot.mjs build/do-ocracy.wasc build/shots/e 600 & node tools/phone-sim.mjs ROOM NAME   # a fake phone over the real relay
SAVE=out.bin SAVE_IN=in.bin node tools/shot.mjs ...                              # dump the save region after the run, load one before it
node tools/wavpeak.mjs out.wav                                                   # peak, RMS, full scale samples
npx romdevtools   # then POST /tool/loadMedia {platform:'wasmcart', path} and /tool/wasm {op:'conformance'}
```

Script files for `shot.mjs` are arrays of `{from, to, pad, buttons:["A"], lx, ly}`. Without `PACE=1` the run is deterministic (seed 7) and as fast as the machine allows; with it, frames are real-time so the relay can answer.

The Playwright checks used during development (browser player with two keyboard players; browser player plus `join.html` in a phone-sized page) live in `build/pw/` and are not committed. They need a Chromium from `~/Library/Caches/ms-playwright`.

## Deploying

- Web: push to `main`. The Pages workflow publishes `web/`.
- Game station (Batocera, RetroArch, wasmcart-libretro core): install the core once with the core repo's `frontend/batocera/install.sh`, then copy `web/do-ocracy.wasc` to `/userdata/roms/wasmcart/` and restart EmulationStation.
- Recommended in `/userdata/system/batocera.conf`:
  ```
  wasmcart.gfxbackend=gl
  wasmcart.rewind=0
  ```
  The cart needs a GL context; if someone sets the global backend to Vulkan the wasmcart system would inherit it. Rewind would snapshot the whole cart memory every frame.

## How to change things

- Words: `src/content.h` has every joke, tagline, proposal, project, donation, toast, award, menu line and the event banners (`EVENT_TEXT`, with the effect for each row in `EVENTS[]` in game.c). Run `python3 -I tools/lint-content.py` after editing; the toast pill cuts at 64 characters.
- Night length: each level's `night.len` (CLASSIC 180 s; `night_seconds` in `config.json` overrides level 0's), and the NIGHT LENGTH setting (SHORT 2:00, NORMAL is the level's own, LONG, MARATHON). The wall clock runs from `clock_h0` for `clock_span` hours (CLASSIC 7 PM to 10 PM, OPEN HOUSE noon to 4 PM, LATE NIGHT 10 PM to 2 AM).
- Phone page URL: `controller_url` in `config.json`, or `JOIN_URL=` at build time.
- Music: songs are data at the top of `src/audio_bellows.cpp` in doot-doot-revolution's note format (`E5:2 G5:2 | ...`, lengths in sixteenths). Sections choose layers with the `L_*` bitmask. The acid line and breakbeat are 16-step arrays. Stem gains in `RenderSpan` were set from measured levels; Bellows engines differ by about 10x in native loudness, so measure after changing a patch (render a WAV and compare RMS per stem).
- Layout: move a station or a piece of furniture in `LEVELS[0]` (src/levels.c); the models follow (station groups turn by `ry`, absent stations park at y -20). Keep every gap either wider than 0.76 or closed (run `build/smoke` and `tools/gapcheck.py`), or bots and players get wedged. Re-run a full night (`node tools/shot.mjs build/do-ocracy.wasc build/shots/x 400,2600,11000 tools/scripts/solo-full.json`; `solo.json` is the same without the keepalive presses, so the idle pad goes home at 75 s) and look for anyone standing still.
- New stations: all ten are live on level 0 (FINAL-space.md 5.2 to 5.13): VENDING, WHITEBOARD, SEWING, BATHROOM, MEMBER STORAGE, WELDING BAY, COFFEE AND BOOKS, the back door, the light switch and the BIKE STATION. Each one is a case in `find_target`, `act`, `goal_valid` and `choose_goal` in `game.c`, a clutter line in `game_update`, a model in `build_stations` and a status line in `draw.c`. World_sync keeps pointers for the vending panel, the needle lamp, the bathroom tube, the forge lamp and the alley light (`s->light`, each with its own emissive material), the whiteboard's lines and corner (`board_line`, `board_corner`), the storage boxes (`storage_box`, `storage_misc`, `storage_tilt`), the books out (`lib_book`), the door hinge (`door_hinge`, the TURN OFF GAS sign is a child of `door_leaf`), the switch toggle, the hung bike and the four rack bikes (`rack_bike`, shown per `npc.bike`). The switch writes the sun and the pendant emissive every frame from `G.lights_on`; `game_lights_dark()` says whether the level runs the DARK preset. EVENTS lists all 25 rows and every effect is written; `event_ready` only gives zero weight to rows whose station the level lacks. A bot's own goal wins a tie in `find_target` (0.4 bias), and the at hand rule in `bot_think` skips GRAB PLYWOOD, the switch and the sleeper, because the bike station stands beside the scrap bays. IT_MOVEBOX, PR_SLEEPER and PR_VISITOR are placed by MOVE NIGHT, LATE NIGHT and OPEN HOUSE.
- Levels: `levels_init` (src/levels.c) copies level 0 into 1 to 4 and applies each night's section 6 deltas (`open_house`, `haboob_night`, `move_night`, `late_night`) with small helpers that move, drop and add obstacles, props, signs, accents, overlays and stencils, then set the lighting preset (DUST, DARK), the night_t column and the event weight column. A night_t gap of 0 means none (LATE has no donations). `lighting_t.open_e` dims the OPEN neon (0.2 at LATE). SG_LEAN is a tile leaning on the floor and SG_OLD a tile crossed with tape (MOVE NIGHT). The level is built on PLAY (`front_end_play`), the attract night always rebuilds level 0, the lobby shows the level card (`level_card`, draw.c) and SETTINGS the ROOM map, both painted by `tile_thumb` from the level's accents and footprints. tools/scripts/level-k.json (k 0 to 4) claims the menu, steps ROOM k times in SETTINGS, plays, skips the flash and readies; the night is under way by frame 2600. Every plane a builder makes releases its own geometry reference (`sign_on`, `lit_plane`); a missing release leaked about 29 KB per level switch until the ten switch soak caught it.
- Floor and walls: `paint_floor(L)` and `paint_wall()` in `world.c`. The floor is a 41 x 24 grid of 0.6 unit tiles painted at 50 px per unit into a 1230 x 715 texture (`FW`, `FH`, which were 1024 x 596 before the overhaul; nothing outside world.c may read them, and `fx_()` and `fz_()` are the only conversion); accents are whole tiles in `LEVELS[].accents`, overlays and stencils are world units. World coordinates map to the floor texture with `fx_()` and `fz_()`; HUD y 0 is the back wall, never flip them. The white tile is 0xd9d3c7 and must not get lighter, or it clips under the lights (the brightest floor pixel in a night shot is about 239; the only pixels at 255 are the emissive tubes).
- Adding a render state: when a new material, light count or texture use appears, three.c logs the missing state (the cart prints it to the host log every 120 frames, three times). Append it to `tools/states.txt`, install three.c's tools once (`cd third_party/three.c && npm install --ignore-scripts`), and run `REGEN_STATES=1 ./build.sh`. Light counts are in the state name (`l1401` is one hemisphere, four point lights, one directional), so adding a light means new states.

- Adding a level: raise `LEVEL_COUNT` in game.h, add a row to `META[]` in levels.c with a name, a blurb and a delta function that edits a copy of level 0 with the helpers (`move_ob`, `drop_ob`, `add_ob`, `place`, `add_prop`, `drop_props`, `add_sign`, `add_accent`, `drop_accent`, `add_overlay`, `add_stencil`, `set_weights`), set its lighting preset, its `night_t` column and its 25 event weights (keep E_NONE rows at or below a quarter of the total, and a zero on any row whose station the level hides). Then run `build/smoke` and `tools/gapcheck.py`, add `tools/scripts/level-k.json` (copy level-4.json with one more RIGHT on the ROOM row) and a line in `tools/accept.sh`, and look at the night. The ROOM row, the map, the lobby level card and `level_check` pick it up from `LEVELS[]`. Do not add lights: levels move, retint and dim the six that exist.
- Adding a station: append the id before `ST_COUNT` (never between the printers), give every level a place in `LEVELS[0].st` (present false where it does not belong), then write its cases in `find_target`, `act`, `goal_valid` and `choose_goal` (game.c), its clutter line in `game_update`, its model in `build_stations` (world.c; any emissive part gets its own `keep_mat(emis(...))`, and models go through the `part` and `group_at` helpers so a level switch frees them), any per frame state in `world_sync`, and its status line and label height in draw.c. If a bot should use it, add a WANT with a low base. Run smoke and gapcheck for the new box.
- Adding a menu row: append the label to `MENU_ROWS[]` (content.h), a `case` in `menu_select` (main.c) that opens its screen, a `SCR_` value before `SCR_NONE` in game.h, an input function called from the default branch of `menu_update`, a drawing function called from the switch in `front_end` (draw.c), and a preview card in `menu_preview`. The rows are 64 px apart from y 200, so a sixth row still fits above the footer; a seventh needs the pitch reduced.
- Adding a flash card: add a caption row to `FLASH_CAPS[]` (content.h, six words at most, the lint checks), a `case` in `flash_card` (draw.c) that draws its picture inside the 680 x 300 picture area (clipped at 300, 150) with the `hud_pict_*` helpers, and an entry in the `beat[]` and `sfx[]` tables of `flash_update` (main.c); then raise the last card index (`M.flash_step >= 4` in `flash_next`, the `> 4` clamp in `flash_update` and the step count passed to `hud_steps` in `rules_flash`). Each card is 2.6 s plus a 0.3 s wipe, so the lobby clock holds 2.9 s longer.

## Not verified yet

- The actual game station. RetroArch is not installed on the development Mac, so the RetroArch path was checked by reading the core's source (GL context, per-frame Node pump, peer dial-out), not by running it. The front end adds two things to check there by hand (FINAL-frontend.md section 10, the cabinet check): with only the keyboard, Z claims the menu as PAD 1; with a USB pad in port 2, 3 or 4 and nothing in port 1, its A claims the menu. The headless rig marks every pad connected, so it cannot prove either. Rumble was never felt on a real pad.
- Performance on a Raspberry Pi. On an Apple Silicon Mac the music costs about 1 ms of CPU per frame and a full night runs at about 150 frames per second headless. A Pi will be several times slower; the expensive parts are the lit, shadowed scene (four point lights, a 2048 shadow map, soft PCF), the CPU-drawn HUD uploaded every frame (the front end adds full screen scrims and caution tape strips, which `hud_tape` blends per pixel), the 1230 x 715 floor texture and the music. If it struggles: drop the shadow map to 1024 and PCF to plain, remove one or two point lights (and their states), redraw the HUD only when something changed, and cut the pad to two voices.
- The music and the new menu sounds by ear. They were checked by measurement only: a WAV of a menu session and a whole CO-OP night peaks at -1.5 dBFS with no full scale sample, under the limiter's -0.8 dBFS ceiling. Previews of the songs are in `docs/music-*.m4a`.
- Real phones on HeatSync's Wi-Fi, and a phone as the menu owner. A simulated phone over the public relay and a phone-sized Chromium page both worked before the overhaul; the AT THE DOOR path for phones (J during the front end, H reply) was checked in code only.
- The acceptance tables by pixel. `tools/accept.sh` checks logs, the save region and the audio; the screenshots against the frame tables of the two specs were read by eye, not probed, and some frames differ from the spec's because the scripts run through the real flow (the comments in `tools/accept.sh` list them).
- A level switch soak since D10. The ten switch soak (levels 0 and 3, heap within 0.005 percent) used a temporary cart hook, which is not in the tree.

## Known limits and risks

- The relay handshake gives up after 10 seconds without a WELCOME and redials. Phone players are not timed out while the cart itself is offline.
- Phones depend on the public relay `relay.clasp.to`. If it is down, the screen says PHONES OFFLINE and pads keep working. A self-hosted CLASP router would need a rebuild with its domain in the manifest (`--ws <host>` in `build.sh`) and in `config.json`.
- TLS needs a correct clock. A Pi without network time will fail the `wss://` handshake.
- The RetroArch menu pauses the cart; the relay may drop the socket. The cart reconnects and keeps its room code, and phone seats survive because their timeout only counts frames that run.
- Save states restore the cart's memory but not the live socket. Avoid them on this game.
- Under RetroArch the keyboard is RetroPad 1 with RetroArch's bindings (Z does it, X drops), not the two keyboard seats.
- The menu owner handover on idle means a stranger who pressed A once can inherit the menu after 20 s. That is the intended party behaviour; a cabinet left alone still returns to the title.
- The save region is 108 bytes and the libretro core copies it every frame. Keep it small; new fields go into `reserved[32]`.
- The headless runs are not exactly repeatable once the relay answers: the room code draws from the sim's random stream when the session arrives, so bot behaviour after that point can differ between two runs of the same cart. Compare shots by eye, or block the network.
- Bots use the planner on a 0.2 unit grid with 0.30 clearance; a level with a corridor under 0.8 would be unplannable there. The gap rule keeps every level above that.
- With two bots and 23 stations some stations go a night without a bot visit (the VISITS and NOVISIT lines in the log). The janitorial wants are tuned low on purpose (FINAL-space.md risk 2).
- romdev conformance warns `deterministic-unflagged`: the cart exports `wc_set_seed` without `WC_FLAG_DETERMINISTIC`. That is deliberate; it does not support seeded replay because of the network, and the room code does not rely on the seed.

## House rules this project follows

- HeatSync material: only general, public hackerspace culture and HeatSync lore. Nothing personal, nothing from the private governance or conflict material, no names. Roles only.
- Voice: no em or en dashes, no emoji, no bold for emphasis in prose, none of the banned filler words. `hackerspace-forge`'s `scripts/lint_deliverable.py` checks the README, HANDOFF and in-game text for voice, and `tools/lint-content.py` checks the repo for dashes, emoji, non ASCII and lower case game strings and the text budgets; both report zero findings at this handoff. Game strings are ASCII caps.
- Visuals: GANTRY tokens (char, bone, hazard amber as a fill, rust, live teal), square corners, solid offset shadows, caution tape between things. The HeatSync mark is drawn from its official paths in its two colours, never altered.
- Station names are the lab's public tool names: LARGE 80W LASER, OUCH and DR. STRANGE (the 3D printers), SIR CUTSALOT (the vinyl cutter). The old joke names are retired. BobRoss is the paper printer.

## Ideas not done

- A lighter render mode chosen from the host's hardware thread count or a config flag, for a Pi.
- A fuller hall of fame: the save region keeps the best karma, clout and co-op team and the nights played (CREDITS and the title footer show them); a per level table would need the reserved bytes.
- Phone-only extras: a buzz on your own events (the `Z` message exists but is unused), or a vote between nights.
- More nights: a class night (seats in rows, one instructor), a build night before a maker faire, a power outage.
