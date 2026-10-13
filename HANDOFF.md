# DO-OCRACY handoff

Where this stands, how it works, how to change it, and what nobody has checked yet. Read with the [README](README.md), which covers the game itself.

## Status

- Playable, deployed, and tested headless and in Chromium. Not yet run on the HeatSync game station hardware.
- Live: https://virgilvox.github.io/do-ocracy/ (browser player) and https://virgilvox.github.io/do-ocracy/join.html (phone controller; the QR on the cabinet points here).
- Repo: https://github.com/virgilvox/do-ocracy, branch `main`. Every push redeploys `web/` to GitHub Pages (`.github/workflows/pages.yml`).
- Commits are authored as `virgilvox <hackbuildvideo@gmail.com>` (set in the repo's git config).
- The cart: `build/do-ocracy.wasc`, copied to `web/do-ocracy.wasc` by every build. About 1.1 MB, wasmcart ABI v4, GL (WebGL2 / GLES3), passes romdev 0.150's `wasm conformance` (one harmless warning, see below).

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
- Input: pads map A/B/X/Start. Keyboard seats read raw HID keys (WASD + Space/E, Q, R, Enter; arrows + period, comma, slash); only the browser player delivers raw keys. RetroArch and the desktop `npx wasmcart` player turn the keyboard into pad 1 instead. Phones send `I<btn>,<x>,<y>`. A pad or keyboard player idle for 75 seconds goes home; the press that clocks you in never also starts the night. Every input source is a seat in `seats[]` (main.c: pads 0 to 3, keyboard halves 4 and 5, phones 6 to 13); `seats_poll` reads all four pads with no connected gate (an unplugged pad reads zero and the idle rule sends its player home) and computes menu nav events with stick repeat, and `seats_join` does the joins and play input. `menu_update` (main.c) runs between them and drives the front end in `M`: the title, the attract screen after 20 idle seconds, the owner seat that claims the main menu, the deny line and AT THE DOOR pending seats (seated after the owner on PLAY), the owner idle handover, the rules flash (SCR_FLASH, `flash_update` in main.c, `rules_flash` and `flash_card` in draw.c: five cards of 2.6 s with 0.3 s wipes, A next, B skip; it opens from HOW TO PLAY and on PLAY when `flash_due()`, holds the lobby clock and READY, and takes only the owner's buttons; ending sets `G.flash_seen`), and placeholder MODES, SETTINGS and CREDITS screens that return on any button. Joining is gated to the lobby and the night. In the lobby START toggles READY, all humans ready runs a 3 s countdown (`M.ready_t`), `M.lobby_t` auto starts at 25 s (reset on every join, held while anyone holds B), and `begin_night` sets the 0.6 s tape wipe (`G.wipe_t`) with `game_start_night` at the closed frame. draw.c draws all of it in `front_end` and `lobby_overlay`; the camera blends orbit and play over 0.8 s by `M.cam_blend`. tools/scripts/title.json walks title, menu, lobby and READY; tools/scripts/flash.json plays HOW TO PLAY from the menu (card k is up at about frame 266 + 174 k). The night HUD (draw.c `top_bar`, `mode_panel`, `join_strip`, `player_cards`, `station_labels`, `floor_tags`, `coach_step`) follows FINAL-frontend.md 6.9; `coach_step` sets `G.coach_st` each frame for the ash guides.
- Settings and save: `SET` (game.c) holds the front end settings; main.c keeps them in a 16 byte block inside the `save_t` save region exposed through `wc_get_info` (magic DOOC, version 1, crc32), and any mismatch, including the all zero region of a never saved cart, loads the defaults. `game_apply_settings` turns `SET` and the mode into the night length, the spawn pace, the bad event weight multiplier and the printer jam chance at lobby open and at night start; the attract night ignores `SET` and plays the level as written.
- Layout: the room is data in `src/levels.c` (`LEVELS[]`, one `level_t` per night: stations, furniture, props, signs, floor accents, lighting, night script; the design is `docs/design/FINAL-space.md`). `game_set_level` copies the station places into `S[]`; collision, spawn boxes, the door and the night script read the active level `L`. Bots and newbies plan their walks on a 0.2 unit grid (`nav_plan` in game.c) so the fenced shop and the counters do not wedge them. `world.c` builds the room from the same level: the tile floor and plaster walls are painted from its accents, overlays, stencils and bays, the storefront shell from its door, the furniture, props and signs from its tables by kind; `world_set_level` drops everything a builder made (objects, textures, materials) and builds the next level without touching the six lights or the camera, so a level switch adds no render state. Every gap between pieces is either walkable (wider than 0.76) or closed.
- The laser's state is `S_IDLE`, `S_LOADED` or `S_BROKEN`; "someone is cutting right now" is the separate `G.laser_cutting` timer that drives the beam and glow. (An earlier build reused `S_BUSY` for that and overwrote the real state after every cut, so the laser never needed plywood again.)
- Bots: `bot_think()` in `game.c` scores goals by archetype, walks to a target face of the station, presses or holds A. A bot that makes no progress for 3 seconds walks to an open spot and rethinks.

## Build and test

Needs Emscripten (`~/emsdk`), Node 22+ (24 for romdevtools), and `git submodule update --init`, then `npm install`.

```sh
./build.sh                         # build/do-ocracy.wasc and web/do-ocracy.wasc
./build.sh smoke                   # the pipeline smoke cart (tools/smoke.c)
JOIN_URL=http://<lan-ip>:8747/join.html ./build.sh   # QR points at a LAN phone page
REGEN_STATES=1 ./build.sh          # regenerate the render program table (see below)
npm run serve                      # web/ on port 8747, prints the LAN address
```

Testing that was done for this handoff:

```sh
node tools/shot.mjs build/do-ocracy.wasc build/shots/s 60,600 [script.json]     # headless GL screenshots, scripted pads
WAV=out.wav node tools/shot.mjs ...                                              # plus the audio
PACE=1 node tools/shot.mjs build/do-ocracy.wasc build/shots/e 600 & node tools/phone-sim.mjs ROOM NAME   # a fake phone over the real relay
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

- Words: `src/content.h` has every joke, tagline, proposal, project, donation and award. Event banners and their one-line explanations are `EVENTS[]` in `game.c`.
- Night length: `night_seconds` in `config.json` (180). The clock always runs 7 to 11 PM.
- Phone page URL: `controller_url` in `config.json`, or `JOIN_URL=` at build time.
- Music: songs are data at the top of `src/audio_bellows.cpp` in doot-doot-revolution's note format (`E5:2 G5:2 | ...`, lengths in sixteenths). Sections choose layers with the `L_*` bitmask. The acid line and breakbeat are 16-step arrays. Stem gains in `RenderSpan` were set from measured levels; Bellows engines differ by about 10x in native loudness, so measure after changing a patch (render a WAV and compare RMS per stem).
- Layout: move a station or a piece of furniture in `LEVELS[0]` (src/levels.c); the models follow (station groups turn by `ry`, absent stations park at y -20). Keep every gap either wider than 0.76 or closed, or bots and players get wedged. Re-run a full night (`node tools/shot.mjs build/do-ocracy.wasc build/shots/x 400,2600,11000 tools/scripts/solo-full.json`; `solo.json` is the same without the keepalive presses, so the idle pad goes home at 75 s) and look for anyone standing still.
- Floor and walls: `paint_floor(L)` and `paint_wall()` in `world.c`. The floor is a 41 x 24 grid of 0.6 unit tiles painted at 50 px per unit into a 1230 x 715 texture (`FW`, `FH`; nothing else reads them); accents are whole tiles in `LEVELS[].accents`, overlays and stencils are world units. World coordinates map to the floor texture with `fx_()` and `fz_()`; HUD y 0 is the back wall, never flip them. The white tile is 0xd9d3c7 and must not get lighter, or it clips under the lights (the brightest floor pixel in a night shot is about 239; the only pixels at 255 are the emissive tubes).
- Adding a render state: when a new material, light count or texture use appears, three.c logs the missing state (the cart prints it to the host log every 120 frames, three times). Append it to `tools/states.txt`, install three.c's tools once (`cd third_party/three.c && npm install --ignore-scripts`), and run `REGEN_STATES=1 ./build.sh`. Light counts are in the state name (`l1401` is one hemisphere, four point lights, one directional), so adding a light means new states.

## Not verified yet

- The actual game station. RetroArch is not installed on the development Mac and the SDL native runner was not set up, so the RetroArch path was checked by reading the core's source (GL context, per-frame Node pump, peer dial-out), not by running it.
- Performance on a Raspberry Pi. On an Apple Silicon Mac the music costs about 1 ms of CPU per frame and a full night runs at several hundred frames per second headless. A Pi will be several times slower; the expensive parts are the lit, shadowed scene (four point lights, a 2048 shadow map, soft PCF), the CPU-drawn HUD uploaded every frame, and the music (the pad and acid line). If it struggles: drop the shadow map to 1024 and PCF to plain, remove one or two point lights (and their states), redraw the HUD only when something changed, and cut the pad to two voices.
- The music by ear. It was checked by measurement only (per-stem RMS, section loudness curve, no clipping). Previews are in `docs/music-*.m4a`.
- Real phones on HeatSync's Wi-Fi. A simulated phone over the public relay and a phone-sized Chromium page both worked.

## Known limits and risks

- The relay handshake gives up after 10 seconds without a WELCOME and redials. Phone players are not timed out while the cart itself is offline.

- Phones depend on the public relay `relay.clasp.to`. If it is down, the screen says PHONES OFFLINE and pads keep working. A self-hosted CLASP router would need a rebuild with its domain in the manifest (`--ws <host>` in `build.sh`) and in `config.json`.
- TLS needs a correct clock. A Pi without network time will fail the `wss://` handshake.
- The RetroArch menu pauses the cart; the relay may drop the socket. The cart reconnects and keeps its room code, and phone seats survive because their timeout only counts frames that run.
- Save states restore the cart's memory but not the live socket. Avoid them on this game.
- Under RetroArch the keyboard is RetroPad 1 with RetroArch's bindings (Z does it, X drops), not the two keyboard seats.
- romdev conformance warns `deterministic-unflagged`: the cart exports `wc_set_seed` without `WC_FLAG_DETERMINISTIC`. That is deliberate; it does not support seeded replay because of the network, and the room code does not rely on the seed.

## House rules this project follows

- HeatSync material: only general, public hackerspace culture and HeatSync lore. Nothing personal, nothing from the private governance or conflict material, no names. Roles only.
- Voice: no em or en dashes, no emoji, no bold for emphasis in prose, none of the banned filler words. `hackerspace-forge`'s `scripts/lint_deliverable.py` checks the README, HANDOFF and in-game text; it reports zero findings at this handoff.
- Visuals: GANTRY tokens (char, bone, hazard amber as a fill, rust, live teal), square corners, solid offset shadows, caution tape between things. The HeatSync mark is drawn from its official paths in its two colours, never altered.
- The 3D printers are Ouch and Dr. Strange (public tool names; the old joke names are retired). BobRoss is the paper printer.

## Ideas not done

- A lighter render mode chosen from the host's hardware thread count or a config flag, for a Pi.
- A hall of fame: the save region has the record fields; nothing writes them yet.
- Phone-only extras: a buzz on your own events (the `Z` message exists but is unused), or a vote between nights.
- More events and stations from the archive: the vending machine, the haboob cleanup, the forge, the sewing machines.
