# world.c technical map (DO-OCRACY)

Source: `/Users/obsidian/Projects/heatsync/games/do-ocracy/src/world.c` (879 lines), read in full, with `hud.h`, `game.h`, the LAYOUT/OBST tables in `game.c`, the camera in `main.c`, `tools/states.txt`, HANDOFF.md and three.c's `docs/r186-renderer.md`. Line numbers below are for world.c unless a file is named.

## 0. Coordinate frame, in one place

- World units: the floor is 24.6 wide (x) by 14.3 deep (z), centred on the origin. x runs from -12.3 (left wall) to +12.3 (right wall). z runs from -7.15 (back wall, brick) to +7.15 (the open front, the "cutaway"). Up is +y. The room is 3.4 units tall (walls, line 231).
- Camera (`main.c` lines 377-385): during the night at (0, 21.5, 15.0) looking at (0, 0, -1.25); on the title screen and attract it orbits gently at y 16.5, z about 12.5, looking at (2.5, 0, -0.5). Vertical FOV 39, aspect 16:9, near 0.5, far 80 (line 707). The front of the room (z positive) is nearest the camera and the camera sees over the missing front wall. The player's screen-up is -z.
- The front door is on the open front edge at x = -8 (`DOOR_X -8.0f`, `DOOR_Z 7.9f`, game.c lines 61-62). The alley door is in the right wall at z about 0.2 (line 242). Zones, left to right, back to front: wood shop (back left), laser, printing counter, electronics bench (back right), metal shop (right wall, back half), kitchen (front left), lounge (left centre front), work island (centre), Show & Tell (front window centre), Free Pile (front right).
- Floor texture mapping: `FW 1024`, `FH 596` (lines 78-79). `fx_(x) = (x + 12.3) / 24.6 * FW`, `fz_(z) = (z + 7.15) / 14.3 * FH` (lines 80-81). About 41.6 texture pixels per world unit on both axes (1024/24.6 = 41.63, 596/14.3 = 41.68), so pixels are square to within 0.1 percent. Texture y grows with world z: y 0 is the back wall, y 596 is the open front.
- Why that is the right way up: `hud_px` stores rows bottom-up (hud.h line 38) and `hud_texture()` (lines 50-57) copies row y of the HUD buffer, so image row 0 is HUD y 0. `t3_texture_new` defaults to `flip_y = true` (three.c core.c line 169), so image row 0 lands at v = 1, the plane's local +y. The floor plane is rotated rx = -pi/2 (line 229), which sends local +y to world -z. So HUD y 0 is the back wall. The same holds for the wall planes: HUD y 0 is the ceiling edge, HUD y = h is the floor line.
- Hard cap: every texture painted this way comes from the 1280 x 720 HUD buffer. `hud_texture(w, h)` reads the top-left w x h of it, so w <= 1280 and h <= 720. `hud_rect` clips to the buffer (hud.c lines 79-82); `hud_text` and `hud_circle` blend per pixel with the same bounds check (hud.c line 67). A floor texture at full HUD width would be 1280 x 744 for this aspect, which does not fit; the largest aspect-correct size is about 1232 x 716 (51 px per unit). 1024 x 596 is what ships.

## 1. Materials (lines 15-19, 21-26, 643-673, plus locals)

`mat(colour, roughness, metalness)` (line 21) makes a `t3_mesh_standard_material`. Every mesh in the room is MeshStandard except the signs (MeshBasic with a map, line 60) and the HUD quad in main.c.

Shared, file-scope (created in `world_init`, lines 643-673):

| name | colour | rough / metal | used for |
| --- | --- | --- | --- |
| m_plywood | 0xc8a165 | 0.9 / 0 | lumber rack sheets, IT_PLYWOOD |
| m_amber | C_HAZARD 0xf2ab1e | 0.45 / 0.1 | laser lid, stage top, trophy, pillow, print growth, projects |
| m_scrap | 0x9c7a48 | 0.9 / 0 | IT_SCRAP |
| m_bottle | 0x6b3f12 | 0.2 / 0 | IT_BOTTLE |
| m_label | 0xf2c94c | 0.6 / 0 | bottle label |
| m_spaghetti | 0x9b7fd0 | 0.6 / 0 | IT_SPAGHETTI |
| m_crt | 0x8b8578 | 0.7 / 0 | CRTs (Free Pile, IT_DONATION heavy) |
| m_screen | 0x1a2a2a | 0.15 / 0 | CRT glass |
| m_cardboard | 0xa0773f | 0.95 / 0 | boxes, offcut bin, donations |
| m_sawdust | 0xd8b47a | 1.0 / 0 | IT_SAWDUST disc |
| m_puddle | 0x5e93c4 | 0.05 / 0 | IT_PUDDLE disc |
| m_strip | 0x14110d | 0.5 / 0 | IT_DAISY power strips |
| m_glow | 0xff3b2a, emissive same, intensity 3 | 0.5 / 0 | laser beam, daisy chain hot spot |
| m_skin | 0xc89a6e | 0.8 / 0 | heads and hands |
| m_visor | 0x14110d | 0.2 / 0.3 | faces |
| m_pcb | 0x2f8562 | 0.5 / 0 | boards |
| m_led | 0x7fd0aa, emissive, intensity 2 | 0.4 / 0 | LEDs |
| m_char | C_CHAR 0x15120f | 0.7 / 0.2 | laser body, legs, many dark parts |
| m_steel | C_STEEL 0x3a342b | 0.9 / 0 | wall boxes, saw base, coffee cabinet |
| m_steel_hi | C_STEEL_HI 0x4a4338 | 0.6 / 0.4 | legs, rails, frames, fixtures, duct |
| m_bone | C_BONE 0xece3d3 | 0.7 / 0 | sink cabinet, pedestals |
| m_rust | C_RUST 0xc2542c | 0.8 / 0 | alley door panel, drawers, bike frame |
| m_violet | 0x6a4fa0 | 0.9 / 0 | vinyl roll |
| m_info | C_INFO 0x5e93c4 | 0.5 / 0.2 | scope, drawers, bins |
| m_plate | 0xf4f1ea | 0.3 / 0 | dishes (identity used by world_sync, line 868) |
| m_wood | 0x8a6a44 | 0.85 / 0 | two-by-fours, TV stand |
| m_dumpster | 0x2f5a46 | 0.7 / 0.3 | dumpster |
| m_fridge | 0xd8d2c4 | 0.4 / 0.1 | fridge |
| m_mate | 0x7a4a18 | 0.25 / 0 | Club-Mate bottles |

Local materials, made and released inside a builder: `green` 0x5f8a73 machine-tool green, `red` 0xc23b2a, `glass` 0x1d3a3a, `scr_g` / `scr_b` / `scr_a` emissive screens (0x7fd0aa, 0x8fbde6, C_HAZARD at 1.6), `counter` 0x3a342b, `woodtop` 0xa57c4f (lines 284-289, again at 460-462), `tube` fluorescent 0xfff6e0 emissive 2.2 (line 250), printer frames 0xe0602a and 0x9b7fd0, spools 0x2f8562 and 0xf2ab1e (lines 317-318), magnets (line 386), lego (line 423), couch fabrics 0x6a4fa0 and 0x7d62b5 (line 436), bins (line 474), `mug` 0xece3d3, `blk` 0x14110d, `tvs` emissive 0x8fbde6 (line 461-462), pots and leaves (line 543), three bulb emissives at intensity 3 (line 551), per-person body colour (line 756) and floor ring (line 778).

`emis(colour, k)` (line 272) is `mat(c, 0.4, 0)` plus emissive = colour at intensity k. Note some `mat()` calls in builders are never released (lines 311, 323, 334, 392, 394, 510, 311): small leaks at startup, harmless once, but a runtime rebuild must not repeat them.

Instancing: three.c auto-instances opaque meshes that share geometry, material and parent group (renderer.c line 162). `part()` (line 28) is how every mesh is made, so sharing `g_box` + `m_steel_hi` is what keeps the draw count low. A new per-mesh material breaks a mesh out of its batch.

## 2. Geometries (lines 15, 636-641, plus one-offs)

- `g_box` unit box; `g_cyl` unit cylinder, 16 sides; `g_sphere` 16 x 12; `g_knot` torus knot (1, 0.32, 64, 8, 2, 3) used for spaghetti, cables, growth; `g_disc` cylinder 20 sides (rings, puddles, sawdust); `g_cone` 12 sides.
- `part(parent, geom, mat, sx, sy, sz, x, y, z)` scales the unit shape, so BOX(w, h, d) and CYL(r, h) are scaled unit meshes with cast and receive shadow both on.
- One-offs: plane geometry per floor, wall and sign (`t3_plane_geometry_new`, lines 62, 219); a torus for the bike wheels (line 561, released).

## 3. Lights (lines 681-705) and the render states they lock in

Six light objects, created once in `world_init`:

1. Hemisphere 0xfff2dc sky / 0x3a2e22 ground, intensity 1.7 (line 681).
2. Directional "sun" 0xffe2b8 intensity 2.8 at (-6, 16, 9), casts shadow, 2048 shadow map, ortho frustum -16..16 by -12..12, near 1 far 40, bias -0.0006 (lines 682-690). This is the only shadow caster. PCF soft is set in main.c line 106.
3. `kitchen_lamp` point 0xffb36b intensity 14, distance 9, decay 2 at (-9.6, 2.9, 2.2) (line 693).
4. `bench_lamp` point 0x8fbde6 intensity 12, distance 9, decay 2 at (6.5, 2.9, -4.6) (line 696).
5. `laser_glow` point 0xff3b2a intensity 0 (driven by world_sync line 847: 10 + 6 sin when cutting), distance 6, decay 2 at (-4.6, 1.8, -5.2) (line 699). An intensity-0 light still counts in the light-count state.
6. `stage_lamp` point 0xffd59a intensity 10, distance 7, decay 2 at (1.8, 2.8, 5.2) (line 703).

So every lit material is compiled for `l1401` (1 directional, 4 point, 0 spot, 1 hemisphere) with `s100` (one directional shadow). The state names in `tools/states.txt`:

```
standard+shadow+l1001            (older: one directional, one hemi)
basic+map+trans+nearest          the HUD quad (main.c)
standard+l1001
standard+instbig+shadow+l1001
basic+map                        the signs (line 60)
standard+shadow+l1301+s100       (older: three point lights)
standard+instbig+shadow+l1301+s100
standard+shadow+l1401+s100       current: non-instanced lit mesh receiving shadow
standard+instbig+l1401           current: instanced batch not receiving shadow
standard+instbig+shadow+l1401+s100   current: instanced batch receiving shadow
standard+map+shadow+l1401+s100   current: floor and brick planes (lit_plane, line 215)
standard+l1401                   current: non-instanced, no shadow receive
```

Rules that follow from this (HANDOFF line 82, three.c docs "State names"):

- Adding a fifth point light means a new family: `standard+shadow+l1501+s100`, `standard+instbig+shadow+l1501+s100`, `standard+instbig+l1501`, `standard+map+shadow+l1501+s100`, `standard+l1501`. Append all five to `tools/states.txt`, then `(cd third_party/three.c && npm install --ignore-scripts)` once and `REGEN_STATES=1 ./build.sh`. A spot light makes `l1411`. Dropping to three point lights lands on `l1301`, of which only two of the five variants exist in the list today (no `map`, no `instbig` without shadow, no plain). Making a point light cast shadows adds `s110` and a `depth` cube variant; do not do that on this hardware budget.
- New material features add states too: a `map` on a standard material that is instanced (`standard+map+instbig+...`), `transparent` (`trans`), `emissive_map`, `side = T3_DOUBLE_SIDE` (`ds`), vertex colours (`vcol`), `alpha_test` (`atest`). A MeshBasic with a map on a mesh that receives shadow does not add `shadow` (basic is unlit).
- The cart logs missing states through `t3_renderer_generated_missing` three times, every 120 frames (main.c lines 412-415). A missing state draws nothing for that material, so a new light or feature with no regenerated table silently blanks meshes. Always run a headless shot after any light or material change and read the log.

## 4. paint_floor() (lines 110-184), in order

Painting is into the HUD buffer with the HUD primitives, then captured by `hud_texture(FW, FH)` (line 183) with `color_space = sRGB` (line 54), mipmapped, linear filtered, clamp. The texture is on a MeshStandard with roughness 0.9 (`lit_plane`, lines 215-226), receive shadow on, cast off.

Helpers: `floor_rect(x0, z0, x1, z1, colour, alpha)` (line 91) maps world rects through fx_/fz_. `hazard_border(x0, z0, x1, z1, w)` (line 97) paints a diagonal amber/black stripe band of width w world units inside the rect, stripe period 7 px, alpha 230. `stencil(x, z, text, font, colour, alpha)` (line 106) centres text at a world point. `shade_px(x, y, d)` (line 85) adds d to RGB. `noise8()` (line 83) is an LCG returning -128..127, seeded 12345 at line 82 and shared with paint_brick, so the order of painting fixes the noise.

1. Base (113-120): concrete, RGB (78, 70, 60) plus a slow sine mottle (amplitude 9 and 4) plus noise/18. Dark warm grey.
2. Saw-cut joints (122-123): 2 px lines at 0x2a241e alpha 160 every 3 world units from -12.3 + 3 in x and -7.15 + 3 in z.
3. Wood shop (126-132): OSB 0x6e5536 over x -12.3..-6.4, z -7.15..-2.6; board seams every 0.6 in z; 2400 random 2 x 1 sawdust flecks 0xd8b47a alpha 90 across the same rect.
4. Kitchen (134-137): 0.6-unit checkerboard, 0xcfc5b0 cream and 0x2a241d char, over x -12.3..-6.8, z -0.8..7.15. This is the only square-tile floor in the current build and its 0.6 pitch (25 px) is a sensible tile pitch to keep.
5. Metal shop (139-143): diamond plate 0x5d5a55 over x 9.6..12.3, z -7.15..-1.6, with 5 x 2 highlight dashes every 12 px, offset per row.
6. Electronics (145): an ESD-blue rect 0x3d5566 over x 4.9..9.5, z -7.15..-4.8 (the bench also has a 3D ESD mat, line 348).
7. Laser mat (147-148): 0x24211e rubber over x -6.6..-2.6, z -7.15..-4.6, then `hazard_border` 0.22 wide.
8. Lounge rug (150-152): 0x4a2f5c over x -7.0..-2.2, z 2.5..5.4; a 3 px amber frame inset 0.2; a row of 0x9b7fd0 dashes along z 2.75..2.85.
9. Show & Tell stage (154): a 4 px C_HAZARD frame over x -0.6..4.2, z 5.4..7.15.
10. Door mat (156-157): 0x1d1915 over x -9..-7, z 5.9..7.05, stencil "108 W MAIN" in F_MONO_S C_ASH.
11. Walkway (159-161): dashed amber line at x -6.55..-6.45 from z -0.5 to 5.8, dash 0.28 every 0.5.
12. Stains (163-166): 14 random dark circles alpha 40.
13. Zone stencils (168-176): WOOD SHOP (-9.3, -2.95) F_H3 bone 70; "LASER. EYES ON IT." (-4.6, -4.35) mono hazard 200; 3D PRINTING (1.2, -5.35) bone 60; ELECTRONICS (7.2, -4.45) bone 70; METAL SHOP (10.95, -1.25) mono bone 140; KITCHEN (-9.6, -0.45) F_H3 C_TAPE 110; LOUNGE (-4.6, 2.25) bone 60; SHOW & TELL (1.8, 5.6) hazard 220; "FREE PILE: TAKE ANYTHING" (10.4, 4.4) mono bone 140.
14. Wall shadow pooling (178-182): darkens up to 46 within 0.9 units of the left, right and back walls. The front edge is deliberately not darkened (d ignores 7.15 - wz).

Every rectangle is a literal coordinate. Nothing in paint_floor reads LAYOUT or OBST.

## 5. paint_brick(w, h, units_w) (lines 187-213)

- `ppu = w / units_w` pixels per unit; brick 0.5 x 0.21 units (bw, bh, minimum 8 x 4 px). Mortar base 0x3d342d; each brick 0x74xxxx reddish with noise/10 variation, 1 px inset, one in 16 is a dark 0x5c3a2c brick; alternating rows offset half a brick.
- Wainscot (203-205): the bottom 0.9 of 3.4 units (`wy = h * (1 - 0.9/3.4)`) is C_CHAR with a 3 px C_HAZARD pinstripe along its top. Remember HUD y grows toward the floor on these planes.
- Shading (207-211): only the top 15 percent (ceiling) is darkened by up to 30. The comment also promises darker toward the floor; that part is not implemented.
- Calls (234-236): back wall 1024 x 142 for 24.6 units (41.6 ppu, bricks 20 x 8 px); each side wall 1024 x 244 for 14.3 units (71.6 ppu, bricks 35 x 15 px). The side walls are therefore sharper than the back wall. 142 and 244 are 3.4 units at those ppu.

## 6. build_room() (lines 228-268)

- Floor: `lit_plane(paint_floor(), 24.6, 14.3, 0, 0, 0, -pi/2, 0)` (229).
- Wall slabs (231-233): m_steel boxes 0.3 thick: back at z -7.15 (spans z -7.3..-7.0), left at x -12.15, right at x +12.15, all 3.4 tall centred y 1.7. Brick planes just inside them: back at z -6.995; left at x -11.995 rotated ry +pi/2; right at x 11.995 rotated ry -pi/2 (234-236).
- Top rail along the back wall (237): m_steel_hi 24.6 x 0.12 x 0.36 at y 3.4.
- Front kick wall stubs (239-240): m_steel 0.5 tall at z 7.1: x -12..-9 and x -3..12. The gap -9..-3 is the storefront: the door frame fills -8.9..-7.1, the rest is open glass in the fiction (nothing is modelled there; the Show & Tell window frame at lines 429-430 is at x 1.8 +/- 2.2 in front of the right stub, not in the gap).
- Alley door (242-244): right wall at z 0.2, 1.5 wide, 2.5 tall, steel_hi slab plus a rust panel and a char handle, at x 11.95.
- Front door (246-248): two steel_hi posts at x -8.9 and -7.1, z 7.1, 2.6 tall, and a header at y 2.6. No glass and no PUSH text despite the comment at 245.
- Fluorescent fixtures (250-264): on the back wall four 3.0-wide fixtures at x = -9, -3, 3, 9 (y 3.05, z -6.88) with emissive tubes 2.8 x 0.06 x 0.08 at y 2.98, z -6.8, cast_shadow off; on each side wall two at z = -3.5 and 2.5 (x +/-11.88 fixture, +/-11.8 tube). The tubes glow but do not light anything; the lighting is section 3.
- Laser exhaust duct (266-267): cylinder r 0.26 h 1.9 at (-5.6, 2.45, -6.5) and a box at (-5.6, 1.45, -6.35).

## 7. build_stations() (lines 281-456)

Each station does `s = &S[ST_X]; g = s->obj = group_at(&SC->base, s->x, s->z)`, so station position is data-driven from `S[]`, which `game_init` copies from `LAYOUT[]` (game.c lines 930-934). Model geometry inside each group is hard-coded and sized to the LAYOUT half extents by hand; the builders never read `hw`/`hd`. Current LAYOUT (game.c 33-47): LUMBER (-11, -4.6) 0.55 x 1.5; PEWPEW 9000 (-4.6, -5.95) 1.4 x 0.8; LOAFER (-0.6, -6.35) 0.55 x 0.45; DR. STRANGE (1.0, -6.35) 0.55 x 0.45; SIR CUTSALOT (3.0, -6.4) 0.75 x 0.38; ELECTRONICS (7.2, -6.0) 1.9 x 0.7; SINK (-11, 1.9) 0.6 x 0.9; FRIDGE (-11, 4.4) 0.55 x 0.5; TRASH (11.1, 1.6) 0.65 x 0.55; FREE PILE (10.4, 5.5) 1.2 x 0.8; SHOW & TELL (1.8, 6.3) 2.2 x 0.5; LURKER COUCH (-4.6, 3.6) 1.6 x 0.45; WORK ISLAND (3.4, 0.6) 1.6 x 0.8.

- ST_LUMBER (292-297): three uprights 2.4 tall at local x -0.45, z -1.35/0/1.35; three shelves; five plywood sheets leaned at rz -0.13; four two-by-fours; a cardboard offcut bin at (0.15, 0.2, 1.1).
- ST_LASER (300-311): char body 2.8 x 0.9 x 1.6; steel_hi rim; amber lid 2.5 x 0.1 x 1.3 at y 1.02; glass 2.1 x 0.04 x 1.0; control panel at local x 1.15 with an amber screen; red e-stop cylinder; five vent slats on the left face; `s->beam` (m_glow, hidden until cutting) and `s->light` (a status lamp, colour driven by world_sync line 850).
- Printing counter (314-316): a group at a hard-coded (1.2, -6.5), 5.6 x 0.75 x 0.9 counter with a wood top. This is the one station model whose position is not derived from LAYOUT. It must become `(S[ST_PRINT1].x + S[ST_CUTSALOT].x) / 2` or a LAYOUT entry of its own if layouts move.
- ST_PRINT1 and ST_PRINT2 (319-336): per printer, base at y 0.86 (sits on the counter), bed, two frame posts and a top bar in the printer's frame colour, gantry, extruder, two spool discs, an LCD, `s->growth` (amber knot, hidden) and `s->light`.
- ST_CUTSALOT (339-343): a pale body on the counter, a violet vinyl roll, a head, a blue LCD.
- ST_BENCH (346-365): wood top 3.8 x 1.3 at y 0.86 with a 3D ESD mat; four legs; two monitors; a scope (m_info with a green screen); soldering station; 12 parts drawers; six LEDs; a PCB; a desk lamp (steel rod, amber emissive head).
- ST_SINK (368-379): bone cabinet 1.1 x 0.85 x 1.75, wood top, steel basin, faucet, two cabinet doors on the +x face, ten `m_plate` dishes hidden by default (world_sync shows `count` of them, lines 863-869; identity is the `m_plate` material pointer).
- ST_FRIDGE (381-394): fridge box 1.0 x 2.1 x 0.95, door seam and two handles on +x, five magnets, a Club-Mate crate on top with six bottles, and `s->light` a zero-scale sphere at y 2.7 that world_sync scales up as the smell bulb (lines 871-874).
- ST_TRASH (397-401): dumpster 1.0 x 0.95 x 0.9, a lid rotated open, two wheels, a blue recycling bin at +z.
- ST_FREEPILE (404-414): three shelves and two sides, a CRT with glass, two boxes, a char box, a cable knot, a CRT and a turned box on the floor at +z.
- ST_SHOW (417-432): char plinth 4.4 x 0.35 x 1.0 with an amber top, three bone pedestals (centre one taller), a trophy (cone and sphere), eight lego bricks, a window frame (two posts at local x +/-2.2, z 0.45, and a header at y 1.6), `s->growth` an amber cube hidden until `timer > 0` (world_sync lines 876-878).
- ST_COUCH (435-442): violet base 3.2 x 0.35 x 0.9, three cushions, a back, two arms, an amber pillow.
- ST_TABLE (445-453): wood top 3.2 x 0.14 x 1.6 at y 0.9, four legs, lower shelf, a box, a green vise, a red toolbox, a rust knot, a blue box.

## 8. build_furniture() (lines 459-539)

Each piece is `group_at(&SC->base, OBST[i].x, OBST[i].z)`: position data-driven from `OBST[]` (game.c 49-59), geometry hard-coded. OBST: SAW (-8.0, -5.7) 0.7 x 0.55; SHELF (-11.55, -2.0) 0.35 x 0.9; COFFEE (-11.15, -0.2) 0.5 x 0.6; KTABLE (-8.3, 2.4) 0.75 x 0.65; TV (-4.6, 6.25) 0.9 x 0.2; CTABLE (-2.75, 3.55) 0.25 x 0.25; ISLAND (-2.0, -1.8) 1.3 x 0.6; LATHE (11.15, -5.2) 0.55 x 1.1; MILL (11.1, -2.9) 0.55 x 0.55.

- OB_SAW (466-471): steel base, steel_hi table 1.4 x 1.1, a red blade disc on edge, a fence, an amber switch.
- OB_SHELF (473-479): four shelves 0.55 x 1.8, two ends, 16 bins cycling rust / info / hazard / green.
- OB_COFFEE (481-486): steel cabinet with wood top, a black coffee machine, a carafe, three mugs.
- OB_KTABLE (488-498): round wood top r 0.65 on a pedestal, three red stools on a 0.85 x 0.55 ellipse, a Mate bottle and a mug.
- OB_TV (500-504): wood stand, black bezel, emissive blue screen, a console.
- OB_CTABLE (506-511): side table, a lamp with an emissive cone shade, a Mate bottle. The `emis()` at 510 is never released.
- OB_ISLAND (513-521): assembly table 2.6 x 1.2, a robot (steel body, sphere head, LED eye, two wheels), a PCB, three parts boxes.
- OB_LATHE (523-529): char base 0.6 x 0.75 x 2.1, green bed, headstock, chuck disc, tailstock, carriage.
- OB_MILL (531-536): green column pieces, a steel_hi table, a spindle.

## 9. build_props() (lines 542-575), all hard-coded coordinates

- Five potted plants at (-11.5, -6.7), (9.0, 6.5), (-11.5, 6.4), (-0.9, 6.4), (5.0, -6.7): a pot cylinder and two leaf spheres each.
- Forty string-light bulbs along the back wall at x = -11.8 + 0.6 i, y 3.25 minus a 0.18 sag every 8 bulbs, z -6.95, cycling three emissive colours (intensity 3), cast_shadow off. Decorative only.
- A bike on the right wall at (11.85, 1.7, 3.4): two torus wheels rotated to face -x, rust frame tubes, a char saddle.

## 10. build_signs() (lines 577-632), all hard-coded

Each sign is painted into the HUD buffer and hung with `sign(texture, w, h, x, y, z, ry)` (line 59): MeshBasic, unlit, no shadow, so signs never darken with the room.

- Lockup 1024 x 256 (6.4 x 1.6 world) on the back wall at (-1.2, 2.45, -6.98): the HeatSync mark via `hud_logo` at its official size and colours, wordmark in F_LOGO, address line, a tape strip. The mark is only ever drawn through `hud_logo`; never repaint it.
- Whiteboard 512 x 256 (2.8 x 1.4) at (-9.0, 1.9, -6.98): "HYH TUESDAY 7PM" and the floor-cleaning loop, DO NOT ERASE.
- Manifesto 512 x 256 (2.4 x 1.2) on the right wall at (11.98, 2.45, 5.5), ry -pi/2.
- Pegboard 512 x 256 (3.6 x 1.8) at (7.2, 2.15, -6.98) behind the bench, eight tool silhouettes, every third one an empty outline.
- OPEN sign 256 x 128 (1.4 x 0.7) on the left wall at (-11.97, 2.3, 6.3), ry +pi/2, pink neon colours (0xff4f8a, 0xff7fae: not GANTRY tokens, a deliberate neon prop).

## 11. world_init() order (lines 634-710)

geometries, materials, build_room, build_stations, build_furniture, build_props, build_signs, then the six lights, then the camera. `world_init` runs once from `wc_init` (main.c line 115), before the title screen. There is no teardown or rebuild path today.

## 12. Dynamic objects and world_sync()

- `world_item_obj(type, kind)` (714-752): a group per item, added to the scene root; models per IT_ type (plywood sheet; project variants 0 laser box, 1 print knot, 2 bench PCB, 3 sticker; scrap; bottle; spaghetti; donation box or heavy CRT; sawdust and puddle discs; daisy chain of three strips and a red hot spot). game.c line 158 calls it from `spawn_item`.
- `world_person(colour, newbie, &body)` (754-792): body cylinder, torso sphere, head, visor, cap for members, two legs, two shoulder pivots with arm and hand, and for members a floor ring in the player colour (emissive 0.9). The whole group is scaled 1.25. `world_sync` relies on child order: legs at n-6 and n-5, arm pivots at n-4 and n-3, ring and hole last (lines 815-825). Add children before the legs or update those indices.
- `world_free(o)` (794): `t3_object_remove` from the scene root. Called by game.c for items, players and newbies.
- `world_sync(dt)` (806-879): positions and bob for players and newbies, swings legs and arms (arms raised when carrying, jitter when working); items hover at y 1.75 and spin when held; laser: `laser_glow->intensity`, beam visibility and wander, status lamp colour by state; printers: growth knot scale from `timer / 14`, spin, lamp colour (fault red, done amber, busy info blue, idle live teal); sink: shows `count` plates by matching `m_plate`; fridge: smell bulb scale and green glow; Show & Tell: spinning growth while `timer > 0`. Everything it touches is reached through `S[i].obj/light/beam/growth`, so a rebuilt station keeps working as long as those four pointers are set.

## 13. What is data-driven and what is hard-coded (the coupling list)

Data-driven already: station and furniture positions (`S[]` from `LAYOUT[]`, `OBST[]`), collision for both (game.c `collide`, lines 203-216, and `clear_spot`, 194-200), HUD station labels and edges (draw.c `station_labels`, reads `S[]`; pay class and label height keyed by station index, lines 177-215).

Hard-coded to this room, grouped by file:

- world.c: FW/FH and the fx_/fz_ constants (77-81, 98); every rectangle and stencil in paint_floor (113-182); wall and door geometry (229-248); fixture positions (252-263); the duct (266-267); the printing counter group (314); plant, bulb and bike positions (544-560); every sign position (585-630); all six light positions (681-705) including `laser_glow` which is not tied to `S[ST_LASER]`; the camera (707-709).
- game.c: `collide` wall clamps -11.6 / 11.6 / -6.6 / 7.0 / 8.5 and the door slot (204-208); `DOOR_X`, `DOOR_Z` (61-62); player spawn around the door (254); puddle spawn box (-9.5..-7.5, -3..0) at 891; the opening scrap at (-6.5, -4.0) at 963; donations on the door mat (863, 5.7..6.3); newbie entry and exit at the door (748, 780, 797).
- main.c: camera height, distance and look target (380-381), tuned to a 24.6 x 14.3 floor.
- draw.c: nothing positional, but the station index switch assumes the ST_ enum (one laser, two printers, one of each other kind).

## 14. Specification: the new floor (the explicit ask)

Goal: a mostly white square-tile floor, with rectangular arrangements of orange and red tiles, built from whole tiles on one grid, with the zones still readable from the camera.

Grid:

- Tile pitch TILE = 0.6 world units (41 columns across 24.6; 23.83 rows across 14.3, so the front row is partial, which is fine because the front is open). At FW 1024 that is 25 px per tile; raise FW/FH to 1232 x 716 if 30 px tiles are wanted (still inside the 1280 x 720 buffer; update lines 78-79 only, fx_/fz_ scale with them). Keep 1024 x 596 unless a shot shows the grout aliasing.
- Tile origin at the back-left corner (x -12.3, z -7.15), so the grid lines up with the back wall and the kitchen checker's old 0.6 pitch. Column c covers x in [-12.3 + 0.6 c, -12.3 + 0.6 (c + 1)), row r covers z in [-7.15 + 0.6 r, ...). Write helpers `tile_px(c, r)` returning the pixel rect, and `tile(c, r, colour)` that paints the tile inset 1 px on each side so the grout shows.
- Grout: paint the whole floor first in grout colour, then tiles. Grout 0x9c968a (ash-grey, not black: black grout on white reads as a grid of wires at this camera distance).
- White tile: not pure white. Use 0xd9d3c7 as the base with a per-tile noise of +/-5 (one `noise8() / 25` per tile, not per pixel) and a faint per-tile diagonal wear streak at alpha 12 on one in six tiles. Pure 0xffffff or C_BONE will clip under the current lights: the sun (2.8) and hemisphere (1.7) together give roughly 1.27 x albedo on a horizontal surface before the point lights, so an albedo over 0.78 blows out to flat white and the shadows vanish into it. Either keep the tile at or below 0xd9d3c7 or drop the sun to 2.0 and the hemisphere to 1.2 and re-check the walls. Do one shot and look at the floor under the kitchen lamp (intensity 14 at 2.9 high) before settling.
- Orange tile: 0xd9672b. Red tile: 0xb23a2e. Both are within the GANTRY family (rust is 0xc2542c; these sit either side of it) and neither is the mark's own orange 0xf99a1c, which stays reserved for the mark. Give these the same +/-5 per-tile noise so they look like VCT, not paint. Do not use C_HAZARD amber as a tile colour; amber stays for the hazard border, frames and stencils so it keeps meaning "attention".
- Rectangles of colour per zone, in tile coordinates (col, row, cols, rows), each a solid block or a one-tile border ring, so every arrangement reads as a rectangle made of square tiles:
  - Wood shop (cols 0..9, rows 0..7): a red border ring one tile wide around the saw and lumber area, white inside, plus a 2 x 2 orange block at the saw's out-feed (cols 6..7, rows 2..3). The old OSB and sawdust flecks go away; sawdust is already a 3D item.
  - Laser (cols 9..16, rows 0..4): keep the rubber mat (line 147) and the hazard border (148) exactly as now; they are objects on top of the tile, and the hazard tape is the one place amber belongs on the floor. Paint tiles under it anyway so the mat edge is crisp.
  - Printing counter (cols 17..25, rows 0..3): a solid orange 9 x 2 strip along the counter's front edge (rows 2..3).
  - Electronics (cols 28..36, rows 0..4): a 1-tile red ring around the bench footprint with a white interior; drop the ESD-blue floor rect at line 145 (the bench still has its 3D mat).
  - Metal shop (cols 36..40, rows 0..9): alternating single orange and white tiles in a 2-column strip along the wall (cols 39..40), the shop's "edge of the machine aisle"; drop the diamond plate (139-143).
  - Kitchen (cols 0..8, rows 10..23): a red and orange checker is too busy; use a 2-tile-wide red border around the kitchen with a single orange tile at each corner, white inside.
  - Lounge (cols 9..17, rows 16..21): keep the rug (150-152) as a flat overlay but change it to a solid 0x4a2f5c with a bone frame, and paint a 1-tile orange ring under its edge so the rug sits in a rectangle.
  - Work island and assembly table (cols 12..26, rows 7..14): a sparse pattern, one 2 x 2 orange block at each end of the island, so the centre stays mostly white and uncluttered; this is where the players spend their time and where the HUD labels sit.
  - Show & Tell (cols 19..27, rows 21..23): a solid red 9 x 1 row at row 21 (the back edge of the stage) and keep the amber frame (154).
  - Free Pile (cols 35..40, rows 19..23): a 2-wide orange L in the corner.
  - Door mat (156-157): keep, in 0x1d1915, but align it to the tile grid (cols 5..8, rows 22..23).
  - Walkway (159-161): remove. On a white floor the dashed line fights the HUD arrows the game already draws for carried items.
- Zone stencils (168-176): keep every label but change colours. Bone at alpha 60 disappears on white. Use C_STEEL at alpha 120 for the F_H3 names and C_HAZARD_DP 0xc98714 at alpha 220 for the two mono warnings (laser, free pile), and keep "SHOW & TELL" in C_HAZARD on the red row. Put each stencil on a white run, not across a red block.
- Joints (122-123) go: the tile grout replaces them. Stains (163-166): keep but reduce to 8 and alpha 28; on white they are the "history" the comment wants, at 40 they look like holes.
- Wall pooling (178-182): keep, but the loop reads the 0.9 unit band and 46 steps; on a white floor that is a visible grey wedge. Halve it to 28 and widen to 1.2 so it reads as ambient occlusion.
- Zone readability check: the zones are now told apart by their coloured rectangles (red for the two shops, orange for the maker counter, red ring for the bench, red border for the kitchen), by the stencils, and by the furniture. Take a `tools/shot.mjs` frame at 600 and confirm that each zone name is legible and no coloured block sits under a HUD label row (the station labels float at y 1.3 to 2.4 so they project above the floor; the floor blocks are not under them in screen space except the Show & Tell row, which is why it is one tile deep).
- Keep `hazard_border`, `floor_rect`, `stencil` and the `noise8` LCG as they are; add `tile()` and `tile_block(c0, r0, cols, rows, colour, ring)` next to them. Do not change the FW/FH/fx_/fz_ contract (any other code that reads the floor texture does not exist, but the HANDOFF documents them).

## 15. Specification: layouts built from data at runtime

What exists: positions for stations and furniture are already tables. What is missing: a room descriptor, a floor zone descriptor, prop and sign and light positions, and a rebuild path.

1. Add to game.h a layout struct and make `LAYOUT`/`OBST` members of it:

```c
typedef struct { float w, d; float door_x; float alley_z; } room_t;
typedef struct { int kind; float x0, z0, x1, z1; } zone_t;      /* kind: floor arrangement and stencil */
typedef struct { int kind; float x, z, ry; } prop_t;             /* plant, bulbs, bike, sign id, lamp id */
typedef struct {
  const char *name;
  room_t room;
  struct { const char *label; float x, z, hw, hd; } st[ST_COUNT];
  obst_t ob[OB_COUNT];
  const zone_t *zones; int zone_n;
  const prop_t *props; int prop_n;
  float lights[4][3];                 /* the four point light positions, in the same order as today */
  float cam_look_z;
} layout_t;
extern const layout_t LAYOUTS[]; extern int layout_n; extern const layout_t *L;
```

   `game_init` copies `L->st` into `S[]` (game.c 930-934) and the collision code reads `L->ob` instead of `OBST`. ST_COUNT and OB_COUNT stay fixed per build: draw.c (lines 177-215) and game.c switch on station ids, and world_sync (845-878) assumes one laser, two printers, a sink, a fridge and a show table. A layout can move things and change the room's shape; it cannot add station kinds without touching those three files. Say so in the layout file header.

2. Replace the room constants. In world.c the pairs 24.6 / 12.3 and 14.3 / 7.15 appear at lines 77-81, 98, 113-123, 178-181, 229-237, 252-263; replace with `L->room.w` and `L->room.d` and keep FW/FH as the texture size (the fx_/fz_ formulas become `(x + w/2) / w * FW`). In game.c `collide` (204-208) the clamps become `+/-(w/2 - 0.7)`, `-(d/2 - 0.55)`, `d/2 - 0.15` and the door slot uses `L->room.door_x`; `DOOR_X`/`DOOR_Z` become `L->room.door_x` and `d/2 + 0.75`. The puddle box at 891 and the opening scrap at 963 should use `clear_spot` instead of literals. main.c `camera` (380-381) should fit the room: `y = 1.5 * d + 0.05`, `z = 1.05 * d`, look at `(0, 0, L->cam_look_z)`; the current numbers are 21.5 and 15.0 for d 14.3, which those formulas reproduce.

3. Floor from zones. `paint_floor` becomes: grout, white tiles over the whole room, then for each `zone_t` a `tile_block` arrangement chosen by `kind` (ZONE_SHOP_RED_RING, ZONE_ORANGE_STRIP, ZONE_RED_BORDER, ZONE_ORANGE_CORNER, ZONE_MAT_HAZARD, ZONE_RUG, ZONE_STAGE, ZONE_DOORMAT), and a stencil whose text and colour come from a small table indexed by `kind`. The zone rects are world units in the layout and get snapped to the tile grid with `floorf((x + w/2) / TILE)`.

4. Rebuild path. Keep a static `t3_object *built[]` list of every root-level object world.c creates (the floor and wall planes, the station and furniture groups, the prop meshes, the sign meshes, the printing counter group) and a list of the textures it made. `world_rebuild(const layout_t *)` removes each from `&SC->base` with `t3_object_remove` (which is what `world_free` does today, line 795) and releases any material or texture it owns (the leaks noted in section 1 at lines 311, 323, 334, 392, 394, 510 must be captured and released), then runs the five build_* functions again. The six lights and the camera are created once; the rebuild only moves them (`t3_object_set_position`) from `L->lights`. Nothing in the light count changes, so no new render states are needed for a layout switch. Player, newbie and item objects are owned by game.c and survive; call `game_reset_world()` first so items are gone and nobody is standing inside a moved table.

5. Where to switch: the start screen's option (the user wants modes and settings there) sets `L` before `game_start_night()`; main.c calls `world_rebuild(L)` when the chosen layout differs from the built one. `world_init` builds `LAYOUTS[0]` so attract mode works unchanged.

6. Validation rule from the HANDOFF (line 80): every gap between pieces is either wider than 0.76 or closed. Add a debug check at rebuild that walks pairs of boxes in the layout and logs any gap in (0.05, 0.76); run a full scripted night per layout and look for anyone standing still.

## 16. Specification: making the room read as the lab and look nicer

Constraints first. The only public facts about the real building the project may lean on: 108 W Main St in downtown Mesa, a storefront on Main Street, about 3,200 square feet, continuous since 2009, laser cutter since 2011, wood shop built out across 2012 and 2013, 501(c)(3), member run, HYH on Tuesdays. There is no public floor plan in the skills or the repo. Do not claim or draw a real floor plan, do not name rooms after people, and keep the fiction to "a downtown storefront hackerspace" with the streets logic a storefront has: the glass front and door on the street, the clean and social zones near the front where passers-by see them, the loud and dusty shop at the back, the alley door at the back or side for the dumpster and deliveries. The current LAYOUT already follows that logic; keep it and tighten the cues.

Cues that make it a storefront, cheap in states (all use existing materials and the existing light count):

- The front glass: model the storefront window in the gap x -9..-3 and across the right stub as thin m_steel_hi mullions (posts every 2 units, a sill at y 0.5, a header at y 2.8) with no glass mesh. The eye fills in the glass; a transparent material would add a `trans` state and sort problems. Put the OPEN sign on a mullion facing the camera instead of the left wall (line 630), and move the door-frame "PUSH" the comment promises into a small sign (a 128 x 64 texture, `basic+map`, no new state).
- Ceiling line: the room has no ceiling (the camera needs to see in) but the top rail exists only on the back wall (237). Add the same rail along both side walls so the cutaway has a clean edge, and a 0.4-deep header beam along the front at y 3.2 between the two front corners, broken over the door gap; it frames the storefront without blocking the view.
- Floor contrast: the white tile floor (section 14) will do most of the lifting. The sun shadow on white is where the "shadows" ask gets paid: raise `sun->shadow->bias` toward -0.0003 and keep the 2048 map; tighten the shadow camera to the room (`left/right +/-13.5`, `top/bottom +/-8.5`, lines 687) so each texel covers less floor and shadow edges sharpen. On a Pi, drop to 1024 and PCF plain (HANDOFF line 87), still no new state.
- Wall shading: implement the missing floor-side darkening in `paint_brick` (207-211): darken the bottom 12 percent above the wainscot by up to 24, so the wall meets the floor instead of floating. Lower the wainscot pinstripe to 2 px and add a second one at the top of the wall at y = 0.3 units so the brick reads as a band, which is the GANTRY wall look (char, bone, one amber line).
- Lights: keep four point lights (no state change). Move `laser_glow` to `S[ST_LASER]` plus (0, 1.8, 0.75) at build time so it follows a layout. Retint `kitchen_lamp` to 0xffc07a and lower it to 11; on a white floor 14 is a hot spot. Raise the hemisphere ground colour to 0x5a4a3a so undersides are not black against white tile.
- Props that say "hackerspace" without words: a fire extinguisher (red cylinder, steel_hi bracket) by the front door and one by the laser; a first-aid box (bone, red cross from four boxes) by the bench; a shop vac (char drum, rust hose knot) in the wood shop; a broom and dustpan leaning on the kitchen wall; a 3D-printed sign holder; a rolling tool cart (red, three drawers) in the metal shop; a pallet of water cases by the alley door (it is 118 outside, says an event); a cardboard "FREE" box at the Free Pile. All boxes and cylinders with existing materials, so they instance.
- Signs: keep the mark lockup exactly as it is. Replace the whiteboard text with fewer, larger lines (two, not five) and move the floor-cleaning loop into the attract tips where it already lives. Add one caution-tape strip (`hud_tape`) at the top of the pegboard. Keep every sign unlit (`basic+map`).
- Shadows pooling: the wall band in paint_floor (178-182) at the reduced strength plus a 1-tile grey ring (0xc8c2b6) under the back wall fixtures sells the AO cheaply; no screen-space AO exists in this renderer.
- Camera: leave the night camera; for the title screen the orbit (main.c 381) can look at the Show & Tell window (lz 4.5) so the start screen opens on the storefront instead of the middle of the floor.

Things that would add render states, listed so nobody does them by accident: a fifth point light (l1501 family); a spot light over the stage (l1411); point-light shadows (s110); a transparent window or glass (`trans`); an emissive map; a double-sided plane for a hanging banner (`ds`); instanced meshes with a texture (`map+instbig`); a normal map on the brick (`nmap`). Each is five new lines in `tools/states.txt` and a regenerate, then a shot to prove nothing went blank.

## 17. Key facts an implementer must not get wrong

1. Texture y grows with world z (back wall at HUD y 0, open front at HUD y 596) because `t3_texture_new` flips y and the floor plane is rotated -pi/2 about x. `fx_`/`fz_` already encode this; never flip them. On the wall planes HUD y 0 is the ceiling.
2. Painted textures cannot exceed 1280 x 720; `hud_texture` reads the HUD buffer. FW x FH is 1024 x 596 today and 1232 x 716 is the aspect-correct maximum.
3. The light count is baked into every state name. 1 hemisphere, 1 directional with shadow, 4 point = `l1401+s100`. Changing any count, or adding a material feature, requires new lines in `tools/states.txt` and `REGEN_STATES=1 ./build.sh`; otherwise those materials draw nothing and the cart only logs it.
4. A white floor needs a lighting budget. With the sun at 2.8 and hemisphere at 1.7 a horizontal albedo above about 0.78 clips. Use 0xd9d3c7 or darker for the white tile, or lower the lights, and check under the kitchen lamp.
5. Station and furniture positions are data (LAYOUT, OBST) but the printing counter (line 314), the lights (681-705), the signs, the props and every floor rectangle are literals. `laser_glow` does not follow `S[ST_LASER]`.
6. `world_sync` depends on `S[i].obj/light/beam/growth` pointers and on child order in `world_person` (legs at n-6, n-5; arms at n-4, n-3) and on the `m_plate` material identity for dishes. Keep those if models change.
7. `collide` in game.c (204-208) and the camera in main.c (380-381) are tuned to 24.6 x 14.3; a layout with a different room size must change them with the room.
8. Amber (C_HAZARD) is a fill for attention, not a tile colour; the mark's orange 0xf99a1c is reserved; the mark is drawn only by `hud_logo` and is never redrawn or recoloured.
9. Signs are MeshBasic and unlit; they do not get darker or lighter with the room. Keep them that way to avoid `map` on standard materials.
10. Every gap between solid pieces must be wider than 0.76 or closed, for players and bots (HANDOFF line 80).
11. No em dashes, en dashes, emoji or bold-for-emphasis in any text, code comment or in-game string; roles only, no real names, nothing from private governance.
