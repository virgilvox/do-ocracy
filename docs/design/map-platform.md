# wasmcart platform map for HeatSync carts

What a cart author needs to know to build a second cart next to DO-OCRACY, with the exact names and paths. Everything here was read from the checked-out sources on 2026-10-12, and the split-screen recipe in section 3 was built and screenshotted through the headless rig, not inferred.

Reference trees (read only, not part of either game):

- wasmcart: `/Users/obsidian/Projects/heatsync/games/_ref/wasmcart` (`include/wasmcart.h`, `include/wc_cart.h`, `include/wc_gl.h`, `SPEC.md`, `docs/*.md`, `src/CartHost.js`)
- three.c: `/Users/obsidian/Projects/heatsync/games/_ref/three.c` (`include/three.h`, `src/renderer.c`, `src/renderer_gen.inc`, `tools/gen-project-table.mjs`, `docs/r186-renderer.md`, `docs/perf.md`)
- game station core: `/Users/obsidian/Projects/heatsync/games/_ref/wasmcart-libretro` (`src/libretro.c`, `README.md`, `frontend/batocera/README.md`)
- the existing cart: `/Users/obsidian/Projects/heatsync/games/do-ocracy` (`build.sh`, `src/main.c`, `tools/shot.mjs`, `tools/states.txt`, `tools/smoke.c`, `HANDOFF.md`)

The game's own submodules are the same repos at pinned commits: `third_party/three.c` is `monteslu/three.c` at `bd2f28c` (v0.186.3-15, three.js r186 API, T3_VERSION 0.186.4) and `third_party/wasmcart` is `wasmcart/wasmcart` at `c38b39d` (package version 0.34.2, ABI v4).

## 1. The cart ABI surface and lifecycle

A cart is one `.wasm` plus assets, zipped into a `.wasc` by `node third_party/wasmcart/bin/wasmcart-pack.js`. The host (browser, Node, RetroArch core) instantiates it and talks through three exports and a handful of imports.

### Exports the cart provides

| Export | Attribute to use | When the host calls it |
| --- | --- | --- |
| `wc_info_t *wc_get_info(void)` | `__attribute__((export_name("wc_get_info")))` | first, before anything else; returns the struct with every buffer address |
| `void wc_init(void)` | `export_name("wc_init")` | once, after the host has written `wc_host_info_t`, loaded save data into the save region, and (under RetroArch) after the GL context exists (`opts.defer_init = true` in `libretro.c`) |
| `void wc_render(void)` | `export_name("wc_render")` | once per frame; pads, time, keys are written before, audio and (2D) framebuffer read after |
| `void wc_set_seed(uint32_t)` | optional | before `wc_init`, fresh entropy normally, pinned seed under a deterministic run |
| `wc_on_suspend`, `wc_on_resume`, `wc_on_focus_lost`, `wc_on_focus_gained` | optional | lifecycle; the host stops calling `wc_render` while suspended (the RetroArch menu is a suspend) |
| `wc_alloc`, `wc_free` | provided by `wasmcart.h` as weak exports | the host writes `glGetString` results, text and peer payloads through them; nothing to do |

`wc_cart.h` offers `WC_EXPORT` (wc_get_info only), `WC_EXPORT_INIT`, `WC_EXPORT_RENDER`, `WC_CART_BUFFERS` and `WC_FILL_INFO(flags)`. DO-OCRACY does not use the macros; `src/main.c` fills `wc_info_t` by hand (lines 37 to 52). Either is fine. If you use `WC_FILL_INFO` note it sets `gpu_api = 0` and `save_ptr = 0`, so a GL cart overrides `wc_info.gpu_api = 1` after the macro, and a cart with save data sets `save_ptr`/`save_size` after it.

### `wc_info_t` fields a GL cart must set

```c
info.version = WC_ABI_VERSION;        /* 4; v1 to v3 carts are refused at load */
info.width = 1280; info.height = 720;
info.fb_ptr = (uint32_t)(uintptr_t)framebuffer;   /* a 4-word dummy for a GL cart */
info.audio_ptr = ring; info.audio_cap = AUDIO_CAP; info.audio_write_ptr = &cursor;
info.input_ptr = pads;                 /* wc_pad_t[4] */
info.time_ptr = &tm;                   /* wc_time_t */
info.host_info_ptr = &host_info;       /* wc_host_info_t, host writes before wc_init */
info.flags = WC_FLAG_AUDIO_F32 | ...;  /* capability flags, see below */
info.gpu_api = 1;                      /* 1 = WebGL2 / GLES3; a cart importing from the "gl" module is a GL cart */
info.audio_sample_rate = 0;            /* 0 = host decides (48000 under RetroArch) */
```

Flags (`wasmcart.h` lines 316 to 329): `WC_FLAG_AUDIO_F32` (float ring), `WC_FLAG_NET_PEER` (wc_peer_* imports; needs a manifest `--ws` grant too), `WC_FLAG_POINTER`, `WC_FLAG_KEYBOARD` (raw keys; see section 2 for why a pad game should not set it), `WC_FLAG_DEBUG`, `WC_FLAG_DETERMINISTIC`, `WC_FLAG_HOST_THREADS` (asks for `hardware_threads` in a 24 byte host info struct).

### Frame order inside `wc_render` (what DO-OCRACY does, `src/main.c` lines 387 to 415)

1. `dt = tm.delta_ms / 1000`, clamped (the host already clamps at 250 ms; DO-OCRACY clamps again at 50 ms; a 0 delta on the first frame becomes 1/60).
2. Read `pads[]` (and keys), update game state.
3. `t3_renderer_render` the world, then the HUD pass.
4. Fill the audio ring with `dt` worth of stereo frames.
5. Every 120 frames, log `t3_renderer_generated_missing(R)` so a missing render state shows up in the host log.

`wc_time_t` has `time_ms`, `delta_ms` (doubles) and `frame` (u32). The host rebases the clock across a suspend, so the first frame after the RetroArch menu closes gets a normal delta.

### Build and link shape

From `build.sh`: `emcc -O2 -DNDEBUG -flto -msimd128 -msse2 -std=gnu11 -DT3_WASMCART -DWC_USE_GL ... -sSTANDALONE_WASM=1 -sEXPORTED_FUNCTIONS='["_wc_init","_wc_render","_wc_get_info"]' -sERROR_ON_UNDEFINED_SYMBOLS=0 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 --no-entry`. Emscripten 6.0.12 is at `~/emsdk`. Because undefined symbols are allowed, every missing symbol silently becomes an `env` import; `node third_party/three.c/wasm/check-imports.mjs build/x.wasm` fails the build if the cart imports anything but `gl.*`, `env.wc_*`, `env.emscripten_notify_memory_growth` or WASI. Keep that check.

### Manifest (`wasmcart-pack`)

`--name`, `--version`, `--width`, `--height`, `--players 1..4`, `--controls` (advisory token list: `dpad,a,b,x,y,l,r,start,select,left_stick,right_stick,left_trigger,right_trigger,l3,r3`), `--ws <domain>` (the only network grant), `--assets <dir>`. The manifest never gates GL, pointer or keyboard; `wc_info_t.flags` and the import section are the truth. Assets are read with `wc_asset_size(path, len)` / `wc_load_asset(path, len, dest, max)` (`WC_ASSET_SIZE("x")`, `WC_LOAD_ASSET("x", dst, n)` helpers for literals). DO-OCRACY loads `config.json` and the four TTF fonts this way (`src/hud.c` line 24).

## 2. The input model

### Pads

`wc_pads[0..3]`, 20 bytes each (`wc_pad_t`): `uint32_t buttons`, `int16_t left_x, left_y, right_x, right_y` (-32768..32767, y positive is down), `int16_t left_trigger, right_trigger` (0..32767, `WC_TRIGGER_MAX`), `uint8_t connected`. Every host normalises to the W3C Standard Gamepad layout: `WC_BTN_A` is south (Xbox A, PlayStation Cross, Nintendo B), `WC_BTN_B` east, `WC_BTN_X` west, `WC_BTN_Y` north, `WC_BTN_L`/`WC_BTN_R` bumpers, `WC_BTN_START`, `WC_BTN_SELECT`, `WC_BTN_UP/DOWN/LEFT/RIGHT` d-pad, `WC_BTN_L3`/`WC_BTN_R3` stick clicks. Bits 14 to 20 (`GUIDE`, `MISC1`, `PADDLE1..4`, `TOUCHPAD`) exist but the libretro core never sets them. Triggers have no button bit; read the analog value and threshold it yourself (`WC_TRIGGER_PRESSED` is 3277, about 10 percent).

Deadzone: apply your own (DO-OCRACY uses 0.2 after dividing by 32767, `src/main.c` line 161). The d-pad should drive the same axis as the left stick so either works.

Things that differ per host, all verified in source:

- RetroArch core (`src/libretro.c` line 1169): `pads[p].connected = (p == 0) ? 1 : 0`. Only port 0 is ever reported connected, whatever is plugged in. A cart that gates joining on `connected` lets only one controller in under RetroArch. DO-OCRACY does this (`src/main.c` line 190: `p->connected ? pad_buttons(...) : 0`), so pads 2 to 4 cannot clock in on the game station; fix it there too. Rule for every cart: poll all four pads every frame, seat a player on an edge of A or Start, never on `connected`.
- Headless rig (`tools/shot.mjs` line 45): all four pads are `connected: true`.
- Browser and `npx wasmcart` desktop host: a pad is connected when a device is present; the keyboard is mapped onto pad 0 (`wasmcart-native/src/main.c` `poll_keyboard_as_pad`).
- RetroArch SNES ordering is translated by the core: RetroPad B (south) becomes `WC_BTN_A`, RetroPad A (east) becomes `WC_BTN_B`, Y becomes `WC_BTN_X`, X becomes `WC_BTN_Y`. Sticks and L2/R2 come through `RETRO_DEVICE_ANALOG` unchanged. The keyboard reaches a cart under RetroArch only as RetroPad 1 with RetroArch's own binds (defaults: arrows, Z = south = `WC_BTN_A`, X = east = `WC_BTN_B`, A = west, S = north, Enter = Start, right Shift = Select, Q/W = L/R).

### Keyboard

`WC_FLAG_KEYBOARD` gives a `uint8_t keys[32]` HID scancode bitmask at `keys_ptr` (`WC_KEY_IS_DOWN(keys, WC_KEY_W)`), plus optional `wc_kb_on_down(keycode, mods)` / `wc_kb_on_up`. Only the browser and Node hosts write it; `libretro.c` never polls `RETRO_DEVICE_KEYBOARD`, so under RetroArch the array stays zero. Setting the flag also makes the browser host stop mapping the keyboard onto pad 1 (`docs/input.md` line 274), so a cart that sets it must implement its own keyboard seat or it loses keyboard play in the browser. For a pad-first split-screen game: do not set `WC_FLAG_KEYBOARD`; the keyboard then arrives as pad 1 on every host. Text entry (names) would use `wc_text_input_begin` / `wc_on_text`, not scancodes.

### Rumble

Always available, cart driven, no flag: `wc_pad_has_rumble(pad)`, `wc_pad_rumble(pad, low, high, duration_ms)` (0..1 each, duration capped at `WC_RUMBLE_MAX_MS` 5000), `wc_pad_rumble_stop(pad)`. Under RetroArch the core maps low/high to `RETRO_RUMBLE_STRONG`/`WEAK` and counts the duration down in frames at 60 fps (`libretro.c` line 484). Headless and keyboard: silent no-ops, `has_rumble` returns 0. Sustained rumble is re-armed every frame; a short pulse (60 to 120 ms) per hit is the cheap, safe pattern.

### Pointer, wheel, input actions

`WC_FLAG_POINTER` gives `wc_pointer_t[10]` (slot 0 mouse, 1 to 9 touches) and `wc_wheel_t`; the libretro core feeds slot 0 from the RetroArch pointer. Not needed for a pad game. The Steam Input style action API (`wc_action_set_new`, `wc_action_new`, `wc_action_digital`, `wc_action_analog`, `wc_action_label`) is optional and every host stubs it; the labels it returns are only useful on hosts that know the device, so a cabinet cart should draw its own glyphs.

### Time, determinism

`delta_ms` is host clamped (250 ms). `shot.mjs` loads with `deterministic: { seed: 7 }` which pins `wc_set_seed` and a fixed 60 Hz step unless `PACE=1`; export `wc_set_seed` and route all randomness through it (DO-OCRACY's `rnd_seed`) and screenshots are reproducible. Do not set `WC_FLAG_DETERMINISTIC` unless the cart really is replayable (DO-OCRACY is not, because of the network; a battle game without network can be, and then romdev's conformance stops warning `deterministic-unflagged`).

## 3. Rendering with three.c, and the split-screen recipe

### What three.c is

`three.h` is the three.js r186 API in C99 with reference counting (`*_new` returns an owned reference, containers retain, `t3_release` drops). It renders with the GLSL programs three.js r186's node renderer generates, so it only draws material states that are in a program table (section 4). It targets exactly the GLES 3.0 / WebGL2 surface wasmcart guarantees.

What it has: `t3_scene`, `t3_perspective_camera_new(fov, aspect, near, far)`, `t3_orthographic_camera_new(l, r, t, b, near, far)`, `t3_camera_update_projection_matrix`; meshes (`t3_mesh_new`, `t3_mesh_new_multi` with geometry groups), `t3_instanced_mesh_new(geo, mat, count)` with `t3_instanced_mesh_set_matrix_at` / `set_color_at`, `t3_skinned_mesh_new` with skeletons and `t3_animation_mixer`, `t3_sprite_new`, `t3_points_new`, `t3_line_segments_new`, `t3_lod_new`; geometries (box, plane, sphere, cylinder, cone, torus, torus knot, lathe, polyhedra, ring, circle, `t3_geometry_new` with your own attributes and `t3_geometry_set_named_attribute`); materials (basic, lambert, phong, standard, physical, normal, line, sprite, points) with `map`, `normal_map`, `ao_map`, `emissive_map`, `roughness_map`, `metalness_map`, `alpha_map`, `env_map`, `light_map`, blending modes, `depth_test`/`depth_write`, `alpha_test`, `side`; lights (ambient, hemisphere, directional, point, spot) with shadows (`t3_renderer_set_shadow_map(R, true, T3_PCF_SOFT_SHADOW_MAP)`, `light->base.cast_shadow`, `light->shadow->map_width`, `->camera`, `->bias`); textures (`t3_texture_new` from RGBA8, `t3_data_texture_new` for CPU-painted buffers, `t3_texture_load_memory` through stb_image, cube maps, `t3_texture_from_gl`); render targets (`t3_render_target_new`, `t3_renderer_set_render_target`, depth textures, MSAA samples); fog (`scene->fog = t3_fog_linear(...)`); scene background texture, environment maps and PMREM; `t3_raycaster_*`; glTF 2.0 loading (`t3_gltf_parse`) with animations; `t3_renderer_set_viewport` / `set_scissor` / `set_scissor_test` (pixels, y from the top, as three.js); `t3_renderer_set_auto_clear`, `t3_renderer_clear`, `t3_renderer_set_clear_color`; `t3_renderer_set_tone_mapping`; `t3_renderer_set_antialias(samples)`; `t3_renderer_set_auto_instancing` (on by default: opaque meshes sharing geometry and material become one instanced draw); `t3_renderer_info(R)` with `calls`, `triangles`, `skipped`, optional `gpu_ms` via `t3_renderer_set_gpu_timing`.

What it does not have: post-processing passes (bloom, SSAO) beyond what you build yourself with render targets and your own materials; a text or 2D API (DO-OCRACY paints its HUD on the CPU with stb_truetype into a data texture); a physics engine (three.c's own bench links Box2D/Box3D separately); line width above 1; `t3_renderer_compile_async` is mentioned in `docs/perf.md` but only `t3_renderer_compile(R, scene, cam)` is in `three.h`, and it is the way to pre-build every program during a loading screen so nothing hitches mid-match.

### The two-pass frame DO-OCRACY uses (`src/main.c`)

```c
t3_register_program_table(&hsx_gl, NULL);          /* before the first render */
R = t3_renderer_new(W, H);
t3_renderer_set_shadow_map(R, true, T3_PCF_SOFT_SHADOW_MAP);
t3_renderer_set_exact_output(R, true);             /* keep r186's linear target + output pass, so a second render composites */
t3_renderer_set_clear_color(R, C_CHAR, 1);
hud_tex = t3_data_texture_new(HUD_W, HUD_H, px); hud_tex->color_space = T3_SRGB_COLOR_SPACE;
/* a MeshBasicMaterial with map = hud_tex, transparent, depth_test = depth_write = false, on a 2x2 plane under an ortho camera (-1..1) */
...
t3_renderer_set_auto_clear(R, true);  t3_renderer_render(R, scene, world_cam);
draw_hud(dt); hud_tex->version++;
t3_renderer_set_auto_clear(R, false); t3_renderer_render(R, hud_scene, hud_cam);
```

Tone mapping is left off on purpose so the HUD colours are the GANTRY hex values. The data texture's `pixels` buffer is owned by the texture; DO-OCRACY paints straight into `hud_tex->pixels` (rows bottom up, GL order, no flipY) and bumps `version` every frame, which costs a 1280x720x4 byte upload per frame.

### Split-screen recipe (verified)

Test cart: `/private/tmp/claude-501/-Users-obsidian-Projects-heatsync-games/8acb29a7-95db-44ee-b7a1-0d462f690d29/scratchpad/split/split.c`, built with DO-OCRACY's `hsx_gl` table and the cached `build/obj/three_core` objects, packed with `wasmcart-pack`, run through `tools/shot.mjs`. Screenshots `scratchpad/split/shot-30.png` (two views side by side) and `shot-150.png` (four quadrants) show distinct camera views, a per-viewport clear confined by the scissor, shadows in every view from a single shadow pass, and the HUD frame composited over all of it. Materials used were `standard+shadow+l1001` and `basic+map+trans+nearest`, both already in `tools/states.txt`.

```c
/* init */
t3_renderer_set_shadow_map(R, true, T3_PCF_SHADOW_MAP);
t3_renderer_set_shadow_auto_update(R, false);   /* shadow maps drawn once per frame, not once per viewport */
t3_renderer_set_exact_output(R, true);
for (i) cam[i] = t3_perspective_camera_new(40, 1, 0.1f, 100);

/* frame */
t3_renderer_shadow_needs_update(R);              /* exactly once per frame, before the first world render */
t3_renderer_set_scissor_test(R, true);
t3_renderer_set_auto_clear(R, true);             /* clears colour + depth inside the scissor only */
for (int i = 0; i < n; i++) {
  int vx, vy, vw, vh;                            /* y measured from the TOP: player 1 is (0,0) */
  if (n == 2) { vw = W / 2; vh = H;     vx = i * vw;       vy = 0; }
  else        { vw = W / 2; vh = H / 2; vx = (i & 1) * vw; vy = (i >> 1) * vh; }
  cam[i]->aspect = (float)vw / vh; t3_camera_update_projection_matrix(cam[i]);
  t3_renderer_set_viewport(R, vx, vy, vw, vh);
  t3_renderer_set_scissor(R, vx, vy, vw, vh);
  t3_renderer_render(R, scene, cam[i]);
}
t3_renderer_set_scissor_test(R, false);
t3_renderer_set_viewport(R, 0, 0, W, H);
t3_renderer_set_auto_clear(R, false);
t3_renderer_render(R, hud_scene, hud_cam);       /* full-screen HUD quad, drawn over every view */
```

Why each line matters, from `src/renderer.c` and `src/renderer_gen.inc`:

- `t3_renderer_render` runs the shadow pass (`gen_render_shadow_maps`, `renderer_gen.inc` line 782) inside every call. With `shadow_auto_update` on, four viewports mean four shadow passes. With it off, `shadow_keep` (`renderer.c` line 726) keeps a map that was already drawn at the same size, and `t3_renderer_shadow_needs_update` marks every map stale, so one call per frame gives one shadow pass shared by all views.
- The shadow pass disables the scissor itself (`renderer_gen.inc` line 786) and sets its own viewport to the map size, and the main pass restores the renderer's viewport and scissor (`bind_target`, `renderer.c` line 598), so the scissor you set is never applied to the shadow map.
- Auto clear with the scissor on clears only the scissor box (GL `glClear` honours the scissor), including depth; that is what keeps view 2 from overwriting view 1. Without the scissor test the second render would clear the whole screen.
- The output pass (`exact_output`) also binds the renderer's viewport and scissor (`renderer.c` lines 2187 to 2188), so each view's encode lands only in its rect.
- Viewport and scissor are in pixels with y from the top (`three.h` line 934), the three.js convention; `view_gl_y` converts internally. A render target's `viewport[4]`/`scissor[4]` fields follow the same rule.
- Cameras: one scene, N `t3_camera` objects added nowhere (a camera need not be in the scene; `t3_renderer_render` updates its world matrix when it has no parent). Set `aspect` per viewport and call `t3_camera_update_projection_matrix`. Frustum culling runs per render, so each view only draws what it sees.
- Per-view HUD elements (a health bar inside each quadrant) are just drawn at the right place in the one full-screen HUD texture, or the HUD pass can itself be scissored per view if a per-player camera-space overlay is wanted.
- Draw call budget scales with the number of views, fill rate does not (the total pixel count is the same). The shadow map is the one fixed cost and the recipe pays it once.
- `t3_renderer_info(R)->calls` is reset by every render, so read it after the view whose cost you want to see.

If a render target is used instead (one FBO per player for a bezel or a screen-space effect), `t3_renderer_set_render_target(R, rt)` and `rt->viewport`/`rt->scissor` work the same way; the target's `texture` is then a map on a full-screen material. On a Pi that doubles the fill rate, so prefer the direct scissored path above.

## 4. The render-state table (`tools/states.txt`, `REGEN_STATES`)

three.c draws only material states whose program text is in a registered table. `build.sh` compiles three.c with `-DT3_TABLE_CORE` (`gen/programs_core_gl`), which contains only the renderer's internal programs (depth caster, output, backgrounds, PMREM), and the game registers its own table in `wc_init`:

```c
extern const struct t3_gen_table hsx_gl;
t3_register_program_table(&hsx_gl, NULL);   /* GL table, no WebGPU table */
```

The table is `src/gen/hsx_gl.c` (plus `hsx_wgpu.c`, generated but not compiled), produced by:

```sh
node third_party/three.c/tools/gen-project-table.mjs --name hsx --out src/gen --states tools/states.txt --cache build/t3-captures
```

`--name` must be a C identifier and becomes the symbol names `<name>_gl` and `<name>_wgpu` and the file names `<name>_gl.c`, `<name>_wgpu.c`. A new cart picks its own (`bots`, `hsb`, anything) and references that symbol in its `wc_init` and in its `extern` line in `tools/smoke.c`. The generator captures each state from three.js 0.186.1 on a mock GPU (`tools/mock-gpu.mjs`, answers in `tools/gpu-answers.json`), so it needs three.js installed inside the submodule: `(cd third_party/three.c && npm install --ignore-scripts)`. Captures are cached under `build/t3-captures` (gitignored); the emitted `src/gen/*.c` is committed, so a normal `./build.sh` never runs the generator. `REGEN_STATES=1 ./build.sh` forces it, and it also runs when `src/gen/<name>_gl.c` is missing (a fresh clone of a new cart with the generated file committed does not need it).

State name grammar (`docs/r186-renderer.md`, "State names"): `kind+feature+...+lDPSH+sDPS`. Kind is `basic`, `lambert`, `phong`, `standard`, `physical`, `sprite`, `points`, `line`. Features: `map`, `nmap`, `vcol`, `fog`, `fog2`, `trans`, `atest`, `nearest`, `rep`, `inst`, `instbig`, `icol`, `skin`, `morph`, `shadow`, `env`, `senv`, `ds`, `bs`, `tm*`, `lin`, and more (`FEATURES` in `tools/gen-programs.mjs`). `l1401` means 1 directional, 4 point, 0 spot, 1 hemisphere light in the scene (ambient is always implied; lit kinds default to `l1100`). `s100` means 1 directional caster, 0 point, 0 spot. DO-OCRACY's current list:

```
standard+shadow+l1001            standard+shadow+l1301+s100       standard+map+shadow+l1401+s100
basic+map+trans+nearest (HUD)    standard+instbig+shadow+l1301+s100   standard+l1401
standard+l1001                   standard+shadow+l1401+s100       basic+map (signs)
standard+instbig+shadow+l1001    standard+instbig+l1401
                                 standard+instbig+shadow+l1401+s100
```

Rules that follow from the grammar:

- The light vector is part of every state. Adding a point light to the scene changes `l1401` to `l1501` for every material in it and every state has to be captured again. Decide the light budget up front and keep it constant (a `laser_glow` point light at intensity 0 is DO-OCRACY's way of keeping the count fixed while a light is off). The `--minimize` option drops states another state covers (same kind, features and casters, at least as many lights of each type), so a bigger light vector folds the smaller ones.
- `instbig` appears on its own: auto instancing turns any group of opaque meshes with one geometry and one material into an instanced draw, so every lit material needs both the plain and the `+instbig` state or some of those meshes vanish depending on how many are visible.
- `shadow` is a feature of the receiving material (`receive_shadow` meshes under a casting light), so lit materials usually need both the `+shadow` and plain variants.
- `trans` (material.transparent), `nearest` (a data texture's filter), `map` (any map set), `vcol` (vertex colours), `fog` (scene fog) each change the state.

What breaks when a state is missing: the draw is skipped silently, `t3_renderer_info(R)->skipped` counts it, and `t3_renderer_generated_missing(R)` returns the missing names one per line. The object simply does not appear (a new material shows as nothing). DO-OCRACY logs the string with `wc_log` every 120 frames, three times (`src/main.c` line 414); `shot.mjs` prints `[cart] ...` lines, so the headless run shows the missing state. The fix is: append the line to `tools/states.txt`, `REGEN_STATES=1 ./build.sh`, commit the regenerated `src/gen/*.c`. During development a loading-screen `t3_renderer_compile(R, scene, cam)` call followed by a check of `generated_missing` catches a missing state on frame 1 instead of when the object first comes on screen.

## 5. Setting up a sibling cart folder

Target: `/Users/obsidian/Projects/heatsync/games/<newgame>` as its own git repo (DO-OCRACY is its own repo at `github.com/virgilvox/do-ocracy`; `games/` itself is not a repo). Commit author `virgilvox <hackbuildvideo@gmail.com>` (`git config user.name virgilvox; git config user.email hackbuildvideo@gmail.com` inside the new repo).

Recommended: the same two submodules at the same commits, and copies of the three small vendored pieces, so both carts build identically and the headless rig is the same script.

```sh
mkdir -p ~/Projects/heatsync/games/<newgame> && cd $_
git init -b main
git submodule add https://github.com/monteslu/three.c third_party/three.c
git -C third_party/three.c checkout bd2f28c72257ba022b4f61af9f905c69eeeac897
git submodule add https://github.com/wasmcart/wasmcart third_party/wasmcart
git -C third_party/wasmcart checkout c38b39d933bff214723007f0a7c0fc92c70cba40
(cd third_party/wasmcart && npm install --silent)            # gives webgl-node, yazl (pack), fflate; node >= 22
(cd third_party/three.c && npm install --ignore-scripts)      # only for REGEN_STATES (three.js 0.186.1 + webgl-node)
cp -R ../do-ocracy/third_party/bellows third_party/           # bellows-embedded, C++17 header-only, from github.com/virgilvox/bellowsjs @ 613cd73 (see SOURCE.txt)
cp ../do-ocracy/third_party/stb_truetype.h third_party/       # v1.26, public domain
cp -R ../do-ocracy/third_party/qrcodegen third_party/         # only if the cart draws a QR (Project Nayuki, MIT)
mkdir -p src/gen tools assets/fonts assets/brand build web
cp ../do-ocracy/assets/fonts/* assets/fonts/                  # ArchivoBlack, CharisSIL Bold, IBMPlexMono Bold/SemiBold, OFL.txt
cp ../do-ocracy/assets/brand/* assets/brand/                  # the official mark paths; never redrawn or recoloured
cp ../do-ocracy/tools/{shot.mjs,serve.mjs,gen-logo.mjs,smoke.c} tools/
cp ../do-ocracy/{build.sh,compile_flags.txt,package.json,.gitignore,LICENSE} .
cp -R ../do-ocracy/web/vendor web/                            # CartHostWeb + fflate for the browser player (web/index.html is per game)
cp -R ../do-ocracy/.github .                                  # the Pages workflow publishes web/
```

Then edit, in `build.sh`: `NAME=<newgame>`, the table name in the `gen-project-table.mjs` line (`--name <table>`), the `SRCS` list (drop `qrcodegen.c` if unused), `GAMEF` (drop `-DWC_USE_NET_PEER` for an offline cart), the `wasmcart-pack` line (`--name "<GAME>" --players 4 --controls dpad,left_stick,right_stick,a,b,x,y,l,r,left_trigger,right_trigger,start`, no `--ws`), and drop the `config.json` block. The three.c object cache `build/obj/three_core` is per project and gitignored; the first build compiles three.c once (about a minute). In `tools/smoke.c` change `hsx_gl` to the new table symbol. In `compile_flags.txt` drop `-DWC_USE_NET_PEER` if unused (it only serves clangd). In `package.json` change the name and description. `tools/states.txt` starts from the states the smoke cart and the first scene need (at minimum `standard+shadow+l1001`, `standard+l1001`, `standard+instbig+shadow+l1001`, `standard+instbig+l1001`, `basic+map+trans+nearest`) and grows from the missing log. `src/gen/<table>_gl.c` must be generated once (`REGEN_STATES=1 ./build.sh`, or let the missing-file rule trigger it) and committed.

Why submodules rather than copies: the headless rig imports `third_party/wasmcart/src/CartHost.js` and `third_party/wasmcart/node_modules/webgl-node`, `build.sh` imports `third_party/three.c/tools/gen-project-table.mjs` and `wasm/check-imports.mjs`, and `wasmcart-pack.js` needs wasmcart's `node_modules`. All of these are relative to the project root in `shot.mjs` (`ROOT = resolve(dirname(...), '..')`) and `build.sh` (`$ROOT/third_party/...`), so the scripts copy unchanged. Two carts sharing one checkout through a symlink would work on the Mac but breaks the GitHub Pages checkout and the fresh-clone build; submodules at pinned commits do not.

### The headless screenshot rig

```sh
node tools/shot.mjs build/<newgame>.wasc build/shots/s 60,300 [script.json]
WAV=build/out.wav node tools/shot.mjs ...        # also writes the audio the cart played
PACE=1 node tools/shot.mjs ...                   # real time, non-deterministic (only needed for network)
```

`shot.mjs` runs the cart in wasmcart's `CartHost` on `webgl-node` (the same native GL path as the station core), feeds scripted pads, and PNG-encodes `gl.readPixels` after `host.withRenderedFrame`. Script entries are `{from, to, pad, buttons: ["A"], lx, ly}` with frame ranges inclusive. Its `BTN` map covers A, B, X, Y, START, UP, DOWN, LEFT, RIGHT and only the left stick; a twin-stick cart extends it with `L: 16, R: 32, SELECT: 128, L3: 4096, R3: 8192` and passes `rightX`, `rightY`, `leftTrigger`, `rightTrigger` into the pad objects (`CartHost._writePads`, `src/CartHost.js` lines 2758 to 2767, reads exactly those names). The rig reports every pad as connected, which the station does not (section 2). Conformance: `npx romdevtools` (needs Node 24), then POST `/tool/loadMedia {platform:'wasmcart', path}` and `/tool/wasm {op:'conformance'}` as in `HANDOFF.md`.

The browser player: `npm run serve` serves `web/` on 8747; `web/index.html` loads `./vendor/wasmcart/web.js` (`CartHostWeb`) and `host.load('<cart>.wasc', { glBackend: gl, audioSampleRate })`. Every build copies the cart into `web/`, and the Pages workflow publishes `web/`.

Deploy to the station: copy `web/<newgame>.wasc` to `/userdata/roms/wasmcart/` on the Batocera box and restart EmulationStation. `batocera.conf` should carry `wasmcart.gfxbackend=gl` and `wasmcart.rewind=0` (rewind snapshots the whole 64 MB cart memory per frame).

## 6. Performance budget for a Raspberry Pi class host

The game station is Batocera on a Raspberry Pi 5 class board (the core's `frontend/batocera/README.md` was tested on a Pi 5, Mesa V3D, RetroArch 1.22.2); the exact hardware is still unconfirmed, so plan for the Pi. The cart runs as wasm inside V8 (libnode) in the core, so game logic is roughly 5 to 10 times slower than the Apple Silicon numbers in `HANDOFF.md`, and the GPU is a V3D with roughly a tenth of a desktop iGPU's fill rate.

Budget per frame at 60 fps (16.7 ms): about 6 ms CPU for the cart (logic, three.c scene traversal, audio synthesis, HUD painting) and about 8 ms GPU, leaving headroom for the core's own blit and RetroArch's frontend.

Concrete rules:

- Resolution: honour the core option. `wc_host_info_t.preferred_width/height` is the station's Internal Resolution setting (the core's default is 1920x1080, `libretro.c` line 300). Call `wc_negotiate_resolution(&host_info, &w, &h, 1280, 720)` from `wc_gl.h` in `wc_init`, create the renderer at that size and write it back into `info.width/height`, so a 960x540 or 640x360 setting really lowers the fill rate and 1080p is capped at 720p. DO-OCRACY ignores the preference and always renders 1280x720.
- Lights: one hemisphere plus one shadow-casting directional (`l1001+s100`). Point lights cost per pixel per light in every lit program; keep at most two, never with shadows (a point shadow is six depth renders). DO-OCRACY runs four point lights, a 2048 soft PCF shadow map and `T3_PCF_SOFT_SHADOW_MAP`; `HANDOFF.md` already lists dropping to 1024 and plain PCF as the first Pi fallback.
- Shadow map: 1024, `T3_PCF_SHADOW_MAP`, one pass per frame (section 3), a tight ortho shadow camera around the arena. Static arenas can skip re-rendering it entirely: leave `shadow_auto_update` off and call `t3_renderer_shadow_needs_update` only when a caster moves, or only every other frame.
- Draw calls: under 150 per view, under 400 per frame in four-way split. Share geometry and material across identical objects so auto instancing batches them (the four bots' wheels, arena tiles, debris), and chunk `t3_instanced_mesh` objects spatially because an instanced mesh is culled as one sphere.
- Geometry: low-poly. three.c's own numbers (`README.md`, 05-heavy at 0.17 ms on a desktop GPU) say the renderer is cheap on the CPU; the Pi is fill and vertex bound, so keep spheres at 16x8 segments and avoid double-sided or transparent surfaces in the arena.
- Transparency: every transparent material forces the linear target plus output pass (`exact_output` is on anyway for the HUD compositing), and transparent draws are sorted and unbatched. Particles should be a few `t3_sprite` or `t3_points` objects, not hundreds of meshes.
- HUD upload: a full 1280x720 RGBA data texture re-uploaded every frame is 3.7 MB of `glTexSubImage2D` per frame, which is significant on V3D. Use a smaller HUD texture (640x360 scaled by the quad, or 960x540), or only bump `version` on frames where the HUD changed, or split static chrome (one texture, uploaded once) from dynamic numbers (a small texture).
- Audio: Bellows at one voice per instrument and no reverb. `HANDOFF.md` measures DO-OCRACY's music at 1 ms per frame on the Mac, which is 5 to 10 ms on the Pi, which is most of the CPU budget. A battle game is better served by `wc_pcm_mixer.h` (prebaked WAV assets mixed on the CPU, dozens of microseconds) with Bellows reserved for a few one-shot synthesized hits, or by rendering the music to a loop once at init.
- Memory: `-sINITIAL_MEMORY=67108864` with growth is fine; the core keeps compiled code between runs.
- Measure on the box: the core option Performance Log (`wasmcart_perf_log`, or `WASMCART_PERF_LOG=1`) writes a per-second breakdown to `wasmcart-perf.log` beside the save directory and to RetroArch's log (`/userdata/system/logs/es_launch_stderr.log`). `t3_renderer_set_gpu_timing(R, true)` plus `t3_renderer_info(R)->gpu_ms` gives GPU time where `EXT_disjoint_timer_query` exists (uncertain on V3D; check `gpu_timer_supported`).
- Program compile hitches: `docs/perf.md` measures 10 to 36 ms per new material state on first use on a desktop; on the Pi it is worse. Call `t3_renderer_compile(R, scene, cam)` for every scene during the title or loading screen so every program is built before play.

## 7. Save data and audio

### Save data (for a hall of fame)

The contract (`SPEC.md`, "Saving is host-managed"): the cart declares a byte region with `wc_info_t.save_ptr` and `save_size`; before `wc_init` the host copies any existing save into it, and after frames the host reads it back and persists it (`CartHost.getSaveData()` in Node, browser storage in `CartHostWeb`, RetroArch SRAM under the libretro core: `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` is a core-owned buffer copied from the cart after every frame and restore, written to `/userdata/saves/wasmcart/<cart>.srm` by RetroArch at unload and on its autosave interval, `libretro.c` lines 37 to 50). The cart never writes files and never decides when; it just writes its struct in memory. `WC_FILL_INFO` sets both to 0, so set them after the macro, or by hand as DO-OCRACY does (it currently exposes no save region, which is why the hall of fame idea in `HANDOFF.md` is still open).

Pattern that survives every host:

```c
typedef struct { uint32_t magic, version, crc; uint32_t best_karma; char best_name[16]; ... } save_t;
static save_t save;           /* static, so its address is stable */
info.save_ptr = (uint32_t)(uintptr_t)&save; info.save_size = sizeof save;
/* in wc_init: if save.magic != MAGIC || crc mismatch, memset to defaults; a never-saved region is all zero */
```

Write the whole struct whenever a record changes; the host picks it up after that frame. Keep it small (a few hundred bytes), because the libretro core copies it every frame. `shot.mjs` does not pass `saveData`, so headless runs always start empty; pass `options.saveData` to `host.load` in a test variant to check loading. Save states and rewind restore cart memory wholesale (fine for an offline cart, harmful for DO-OCRACY's socket).

### Audio

Stereo ring buffer of `audio_cap` frames (a frame is a left and right sample), float32 when `WC_FLAG_AUDIO_F32` is set, else int16. The cart owns a write cursor (`audio_write_ptr`, in frames, modulo `audio_cap`); the host keeps its own read cursor and drains everything between the two after each `wc_render` (`CartHost._drainAudio`, `src/CartHost.js` line 2832; `wc_host_get_audio` in the native core, converted to int16 for `audio_batch_cb`). The rate is `host_info.audio_sample_rate` (48000 under the libretro core and the Node rig) when `info.audio_sample_rate` is 0.

Write exactly `rate * delta_ms / 1000` frames per render with a fractional carry, capped at half the ring (DO-OCRACY's `audio_render`, `src/audio_bellows.cpp` lines 667 to 691, and the timing note at the top of `include/wc_pcm_mixer.h`). A fixed 800 frames per call stutters whenever a frame is late and overflows when frames are fast. `AUDIO_CAP` 8192 is DO-OCRACY's size (170 ms at 48 kHz). For prebaked sound effects `wc_pcm_mixer.h` (`WC_PCM_MIXER_IMPLEMENTATION` in one file, `wc_mixer_init`, `wc_mixer_load_wav` from an asset, `wc_mixer_play(id, volume, loop)`, `wc_mixer_mix(ring, cap, &cursor, frames)`) does the whole job; for synthesis the Bellows copy in `third_party/bellows` (C++17, no exceptions, no RTTI, built with `em++ -std=c++17 -fno-exceptions -fno-rtti`) is what DO-OCRACY uses. Lifecycle: export `wc_on_suspend`/`wc_on_resume` to mute and un-mute if music state would otherwise glitch; the host rebases time so the ring does not need to catch up.

## 8. Things an implementer must not get wrong

1. Under the RetroArch core only pad 0 reports `connected = 1`. Never gate seating on `connected`; poll all four pads and seat on an A or Start edge. DO-OCRACY has this bug for pads 2 to 4.
2. Do not set `WC_FLAG_KEYBOARD` in a pad game: it never fires on the station and it switches off the keyboard-as-pad-1 mapping in the browser host.
3. The light vector is baked into every render state name. Fix the light budget before generating `states.txt`, and keep inert lights at intensity 0 rather than removing them.
4. Every lit material needs its plain and `+instbig` (and usually `+shadow`) variants in the table, or objects vanish depending on how many are on screen. Watch `t3_renderer_generated_missing` in the host log during the first run of every new scene.
5. Split screen: `t3_renderer_set_scissor_test(R, true)` with viewport and scissor set to the same rect, auto clear on, y from the top; `t3_renderer_set_shadow_auto_update(R, false)` at init and `t3_renderer_shadow_needs_update(R)` once per frame; scissor off, full viewport, auto clear off for the HUD pass.
6. The program table symbol name comes from `--name`; the new cart's `wc_init` and `tools/smoke.c` must reference the new symbol, and the generated `src/gen/<name>_gl.c` is committed.
7. Honour `wc_host_info_t.preferred_width/height` through `wc_negotiate_resolution` with a 1280x720 cap; the station can then run the cart at 540p or 360p when a Pi needs it.
8. Audio frames per render are time based (`rate * delta_ms / 1000` with carry), never a constant.
9. The cart may only import `gl.*`, `env.wc_*` and WASI; keep `check-imports.mjs` in the build.
10. Save region: static struct, magic and version header, treat all zero as empty; the host persists it, the cart never asks.
11. `wasmcart-pack --ws` is the only way a cart may dial out; an offline cart passes none and does not set `WC_FLAG_NET_PEER`.
12. House style in anything shipped: GANTRY tokens from `src/hud.h` (`C_CHAR 0x15120f`, `C_BONE 0xece3d3`, `C_HAZARD 0xf2ab1e` as a fill, `C_RUST 0xc2542c`, `C_LIVE 0x5fb892`, `C_LOGO_SUN 0xf99a1c` only for the mark), square corners, solid offset shadows, caution tape; the HeatSync mark comes from `assets/brand/hsl-mark-paths.json` through `tools/gen-logo.mjs` and `hud_logo`, unaltered.
