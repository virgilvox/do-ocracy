# DO-OCRACY space: final design (floor, room, levels, stations)

Status: implementation ready. Synthesised from design-space-2 (the winner, readability first) with the grafts the judges asked for from design-space-1 (storefront shell, sign set, concrete shop, validator, 22 row event table, E_DRAIN, E_FLAT, trash as the long walk) and design-space-3 (ST_BACKDOOR and HABOOB NIGHT, IT_MOVEBOX, ST_SWITCH, bikes in the rack, LATE NIGHT and OPEN HOUSE content, station detail, bathroom dirtiness, VCT speckle, the library passive, derived light positions, per level clock). Every coordinate below was run through scratchpad/check/space_check.py (gap, diagonal pinch, approach point, spawn row, accent bounds, stencil on white); the output is pasted in section 11 and the three fixes the judges required are applied (host desk at x 2.6..3.6, no mark on the darkroom wall, show window red row at r21).

Line numbers refer to the current sources: src/world.c (879 lines), src/game.c (1077), src/game.h (195), src/draw.c (778), src/main.c (415), as the maps describe them. Function names were checked against the files on disk.

House rules in force for every string, comment and this document: no em or en dashes, no emoji, no bold for emphasis, roles only, public HeatSync lore only, GANTRY tokens from hud.h (C_CHAR, C_BONE, C_HAZARD as a fill for attention, C_RUST, C_LIVE, square corners, solid offset shadows, hud_tape), the HeatSync mark drawn only by hud_logo at its official colours and never recoloured.

Contents

0. Decisions in one screen
1. The room as a space
2. The floor: grid, colours, painter, accents, overlays, stencils, zones
3. CLASSIC TUESDAY geometry: stations, obstacles, props, signs, anchors
4. Walls, storefront shell, lighting, materials
5. Stations and elements: changed and new, items, events
6. Levels: table, deltas, night scripts, lighting presets, event weights
7. Data structures (C)
8. What moves from hard coded to data, file by file
9. Rebuild path and the validator
10. Model polish list with geometry
11. Wireframes: tile maps, the level card, the HUD label budget
12. Work breakdown (numbered, files, functions, line estimates)
13. Acceptance checks (scripted shots)
14. Risks
15. Facts an implementer must not get wrong

## 0. Decisions in one screen

1. Room shell fixed at 24.6 x 14.3; camera (main.c 377-385), collide clamps (game.c 204-208) and shadow frustum do not change per level. The front door moves to the centre: door_x 0.0, door_z 7.9, door_hw 1.1.
2. Floor: one 0.6 unit tile grid, 41 columns by 24 rows, origin at the back left corner (-12.3, -7.15). Texture 1230 x 715 (50 px per unit, 30 px per tile). White VCT 0xd9d3c7 with speckle; whole tile rectangles of orange 0xd9672b and red 0xb23a2e in runways, a border ring, blocks under stations, a 2 x 2 checker and stripes; concrete per tile in the fenced shop; terrazzo in the vestibule; grey mats under thin machines. Amber C_HAZARD never as a tile.
3. Zones read from the accent rectangle on each station's approach side (red or orange underfoot means stand here), from orange station tiles on the walls, from furniture silhouettes and shadows, from six floor stencils per level, and from the pendants over the two social spots.
4. 23 station ids (13 existing plus ST_VEND, ST_BOARD, ST_SEWING, ST_BATH, ST_STORAGE, ST_FORGE, ST_LIBRARY, ST_BACKDOOR, ST_SWITCH, ST_BIKE), all appended after ST_TABLE; absent stations are built, parked at y -20 with present false, so world_sync keeps its unguarded pointers.
5. Five selectable levels: CLASSIC TUESDAY, OPEN HOUSE, HABOOB NIGHT, MOVE NIGHT, LATE NIGHT. Each is a complete level_t in memory (geometry, floor, props, signs, lighting, night script). LEVELS[1..4] are built from LEVELS[0] plus deltas in levels_init.
6. Light count unchanged: 1 hemisphere, 1 directional with the only shadow, 4 point lights, every lit material stays l1401+s100. Levels move, retint and dim lights; nothing adds a state. Signs stay basic+map. No transparency, no fog, no spot, no point shadows, no map on an instanced standard material.
7. Point light positions derive from stations at rebuild: kitchen lamp from ST_SINK, bench lamp from ST_BENCH, hall lamp over ST_TABLE, laser glow from ST_LASER.
8. Gap rule: every pair of solids is either closed (gap at most 0.05 on both axes, or filled by a third box) or at least 0.76 apart on one axis with no diagonal pinch (corner to corner at least 0.76). level_check enforces it in debug builds and in tools/smoke.c; narrow corridors from 0.76 to 1.00 are logged as warnings and listed in 11.4.
9. Station labels are level data. The base level shows the public tool names LARGE 80W LASER, OUCH, DR. STRANGE, SIR CUTSALOT; coach, how_to and the E_STRANGE banner read S[].label through one %s. The README says the two joke names (PEWPEW 9000, LOAFER) are retired.
10. Trash is a wheelbarrow and two bins in a back hallway recess, labelled TRASH (ALLEY, 225 FT); the green TURN OFF GAS door beside it is a station (ST_BACKDOOR) that the desert blows open.

## 1. The room as a space

Orientation (space brief): -z is the back wall (north, the alley), +z is the street and the camera, -x is screen left (west), +x is screen right (east). The camera looks in from Main Street over the missing front wall, so what it sees first is the storefront band.

Three bands, clean to dirty, compressed into the 24.6 x 14.3 shell:

Front band, z +3.6 to +7.15 (street side). Glass front with mullions, the centre doors at x 0 in a recessed vestibule of terrazzo tile with the lit facade sign above, a show window either side: west window the CRT wall and yellow LEGO crates (scenery obstacle OBK_WINDOW), east window the robot, LED cube, marquee and DONATE screens (this is ST_SHOW, the Show & Tell deposit). Host desk and front computer just inside, east of the door. The HYH table on trestles in the middle (ST_TABLE). LEGO table and Coffee and Books (ST_LIBRARY, OBK_COFFEE) on the west. The coworking row of plywood desks and monitors down the east wall under the hack letters, pendants on yellow cords over it, the long red runway tile in its aisle.

Middle band, z -3.6 to +3.6 (the bays). West wall, front to back: vending machine, sewing tables with the red and orange checker patch, member storage lockers and bankers boxes, the light switch plate, the free stuff shelf behind grey tear apart bins. Craft counter with the vinyl cutter at mid depth west. The whiteboard by the HYH table. The second table (class seating) east of centre. The tall donation bin between the wood tools and electronics. Electronics bench along the east wall with the orange band in its aisle.

Back band, z -7.15 to -3.6 (dirty end). West corner: the snack fridge and the kitchen sink under the orange darkroom wall panel, the supply closet and stairs block with its UPSTAIRS: LIMITED ACCESS sign, the small bathroom door in the back wall with the first aid box beside it, the retro couch facing the laser. Centre: the red 80W laser on its rubber mat with the duct up the wall and the BE NICE. BE MAKING. CLEAN UP. triptych above, the laser scrap bays painted green, yellow, orange, red (the plywood source), the bike station cabinet, the wood tools bay on white tables with orange edge banding. East: the fenced machine shop with its concrete floor, green lathe, Blue Mill, caution tape at the electrical panel, open on its east end; the welding bay behind a tape ring on the east wall; the back hallway recess with the wheelbarrow and bins; the green TURN OFF GAS door at x 11.0.

What changes for play: players spawn into the vestibule and see the whole hall; trash is a long walk to the back right (the chore the wiki describes, and funnier for it); the free shelf is west at mid depth so junk and donations cross the hall in opposite directions; the laser stays at the back; printers sit on a counter at mid depth east so the printing loop is short; the two social spots (coworking, HYH table) are lit by pendants so the eye lands there first.

## 2. The floor

### 2.1 Grid

- TILE 0.6 world units. Origin at the back left corner (-12.3, -7.15). 41 columns (c 0..40), 24 rows (r 0..23; row 23 is 0.5 deep and runs under the front wall).
- Column c covers x in [-12.3 + 0.6 c, -12.3 + 0.6 (c + 1)); row r covers z in [-7.15 + 0.6 r, -7.15 + 0.6 (r + 1)). `tc_(x) = (int)floorf((x + 12.3f) / 0.6f)`, `tr_(z) = (int)floorf((z + 7.15f) / 0.6f)`.
- Anchors: c0 x -12.3, c5 -9.3, c10 -6.3, c15 -3.3, c20 -0.3, c25 2.7, c30 5.7, c35 8.7, c40 11.7. r0 z -7.15, r4 -4.75, r8 -2.35, r12 0.05, r16 2.45, r20 4.85, r22 6.05, r23 6.65.
- Texture: FW 1230, FH 715 (world.c 78-79). Exactly 50 px per unit on both axes and 30 px per tile, inside the 1280 x 720 HUD buffer, aspect correct (1230 / 24.6 = 715 / 14.3 = 50). fx_ and fz_ keep their form; nothing else reads FW or FH. Tile pixel rect: px = 30 c, py = 30 r, 30 x 30. HUD y 0 is the back wall; never flip fx_/fz_.
- Grout: 2 px on the left and top edge of every tile (tile painted at px + 2, py + 2, 28 x 28). Grout 0x9c968a (ash); HABOOB NIGHT uses 0x8e887c. Black grout reads as a wire grid from this camera.

### 2.2 Tile kinds and colours

| id | use | colour | notes |
| --- | --- | --- | --- |
| T_WHITE | the floor | 0xd9d3c7 | plus one noise8() / 25 per tile (so +/- 5, never per pixel); four 1 px VCT speckle dots per tile, two 0xc6c0b4 and two 0xe4ded2, positions from noise8(); one tile in eight gets a diagonal wear streak (24 steps of 2 x 1 px, 0x7a7367 alpha 12). Albedo 0.85 is the most the lights allow before clipping |
| T_ORANGE | accent | 0xd9672b | the real VCT orange. Not C_HAZARD, not the mark's 0xf99a1c |
| T_RED | accent | 0xb23a2e | VCT red, a touch darker than C_RUST so rust props read on it |
| T_CHECK | accent | red and orange alternating by (c + r) parity | the 2 x 2 patch by the sewing tables |
| T_GREY | concrete, machine shop | base RGB 104, 100, 95 | per tile: the concrete recipe (sine mottle amplitude 6 and 3, noise8() / 14, eight stain discs alpha 30 over the whole shop rect, two 1 px saw joints 0x4a4640 along c 32 and r 2 edges); grout stays so the grid shows; darken 18 within 0.5 units of the fence |
| T_TERRAZZO | vestibule | 0x7d6e5e | 2 x 2 px speckles: one in nine pixels 0xa89a86, one in fourteen 0x4a3f36 |
| T_MAT | grey mat under thin machines | 0xc8c2b6 | under the printing counter, the craft counter and the electronics bench so footprints read |
| T_GHOST | newer tiles | 0xe2dcd0 | MOVE NIGHT, where a replaced accent block used to be |

Accent tiles get the same +/- 5 per tile noise so they look laid, not painted. Amber C_HAZARD appears on the floor only as tape: the laser mat border, the shop tape line, the welding bay ring, the show window frame.

### 2.3 Painter algorithm (replaces paint_floor, world.c 110-185)

```c
#define TILE 0.6f
#define TCOLS 41
#define TROWS 24
enum { T_WHITE, T_ORANGE, T_RED, T_CHECK, T_GREY, T_TERRAZZO, T_MAT, T_GHOST };
typedef struct { uint8_t kind, c, r, cw, rh, ring; } accent_t;      /* ring 1: one tile border only */
static const uint32_t TCOL[] = { 0xd9d3c7, 0xd9672b, 0xb23a2e, 0, 0x68645f, 0x7d6e5e, 0xc8c2b6, 0xe2dcd0 };

static int tc_(float x) { return (int)floorf((x + 12.3f) / TILE); }
static int tr_(float z) { return (int)floorf((z + 7.15f) / TILE); }

static t3_texture *paint_floor(const level_t *L) {
  static uint8_t map[TCOLS][TROWS];
  memset(map, T_WHITE, sizeof map);
  for (int i = 0; i < L->accent_n; i++) {                       /* 1. resolve the tile map */
    const accent_t *a = &L->accents[i];
    for (int c = a->c; c < a->c + a->cw && c < TCOLS; c++)
      for (int r = a->r; r < a->r + a->rh && r < TROWS; r++) {
        bool edge = c == a->c || r == a->r || c == a->c + a->cw - 1 || r == a->r + a->rh - 1;
        if (a->ring && !edge) continue;
        map[c][r] = a->kind == T_CHECK ? (((c + r) & 1) ? T_ORANGE : T_RED) : a->kind;
      }
  }
  lcg = 12345;                                                   /* 2. same seed every build */
  hud_rect(0, 0, FW, FH, L->light.grout, 255);                   /* 3. grout */
  for (int c = 0; c < TCOLS; c++) for (int r = 0; r < TROWS; r++) {
    int k = map[c][r], px = c * 30 + 2, py = r * 30 + 2;
    if (k == T_GREY) { concrete_tile(px, py, 28, 28, L); continue; }
    if (k == T_TERRAZZO) { terrazzo_tile(px, py, 28, 28); continue; }
    hud_rect(px, py, 28, 28, shade(TCOL[k], noise8() / 25), 255);
    if (k == T_WHITE || k == T_GHOST || k == T_MAT) {
      for (int d = 0; d < 4; d++) hud_rect(px + (noise8() & 27), py + (noise8() & 27), 1, 1, d < 2 ? 0xc6c0b4 : 0xe4ded2, 255);
      if (k == T_WHITE && (noise8() & 7) == 0) for (int s = 0; s < 24; s++) hud_rect(px + 1 + s, py + 1 + s, 2, 1, 0x7a7367, 12);
    }
  }
  shop_stains_and_joints(L);                                      /* 4. only inside T_GREY rects */
  for (int i = 0; i < L->overlay_n; i++) paint_overlay(&L->overlays[i]);   /* 5. world unit overlays, 2.4 */
  for (int k = 0; k < 14; k++) {                                  /* 6. chair wheel streaks in the coworking aisle */
    float x = L->streak_box.x0 + frnd() * (L->streak_box.x1 - L->streak_box.x0);
    floor_rect(x, L->streak_box.z0, x + 0.03f, L->streak_box.z1, 0x7a7367, 18);
  }
  for (int k = 0; k < 8; k++) stain_disc(0x5a524a, 28);          /* 7. eight stains, hall only */
  for (int i = 0; i < L->stencil_n; i++) {                        /* 8. stencils, 2.5 */
    const stencil_t *s = &L->stencils[i];
    stencil(s->x, s->z, s->text, s->font, s->warn ? C_HAZARD_DP : s->on_grey ? C_BONE : C_STEEL, s->warn ? 220 : s->on_grey ? 150 : 120);
  }
  wall_pool(1.2f, 28);                                            /* 9. left, right, back only; never the front */
  if (L->light.wash_a) { hud_rect(0, 0, FW, FH, L->light.wash_col, L->light.wash_a); door_wash(L); }   /* 10. HABOOB */
  return keep_tex(hud_texture(FW, FH));
}
```

Where `shade(col, n)` adds n to each channel clamped, `concrete_tile` paints the recipe in 2.2 inside the tile rect, `door_wash` paints a second wash of alpha 40 fading linearly to 0 over 2.5 units from the back door (11.05, -7.15) and the front door (0.0, 7.15), and `wall_pool` is the loop at world.c 178-182 with 1.2 units and 28 steps. Keep floor_rect, hazard_border, stencil, shade_px and noise8 (world.c 83-108). Delete the concrete base, saw cut joints, OSB, sawdust flecks, kitchen checker, diamond plate, ESD rect, lounge rug dashes and the dashed walkway (world.c 113-161). Paint the floor before the walls so the shared LCG keeps the tiles stable across levels.

### 2.4 Overlays (world units, per level)

```c
enum { OV_LASER_MAT, OV_TAPE_LINE, OV_TAPE_RING, OV_FRAME, OV_RUG };
typedef struct { uint8_t kind; float x0, z0, x1, z1; } overlay_t;
```

| kind | what | how |
| --- | --- | --- |
| OV_LASER_MAT | rubber mat under the laser | floor_rect(rect, 0x24211e, 255) then hazard_border(rect, 0.22) exactly as world.c 147-148; derived at levels_init from S[ST_LASER] grown 0.2 on x and 0.25 on +z |
| OV_TAPE_LINE | caution tape across a strip (shop fence front, shop electrical panel) | hazard_border(x0, z0, x1, z1, 0.5 * (z1 - z0)) on a strip 0.3 deep so the stripes fill it |
| OV_TAPE_RING | tape around a hazard (welding bay, MOVE NIGHT box stacks) | hazard_border(rect, 0.18) |
| OV_FRAME | the show window stage frame | 4 px C_HAZARD outline, as world.c 154 |
| OV_RUG | a rug under a floor couch | solid 0x4a2f5c with a 3 px C_BONE frame inset 0.15; used only when a level puts the couch on the open floor (none of the five do; kept for authors) |

Rule for authors: snap overlay rects to the tile grid unless the overlay is tape; tape may sit off grid because it is a thing laid on top.

### 2.5 Stencils

- At most eight per level. Zone names in F_H3 C_STEEL alpha 120 on white; warnings in F_MONO C_HAZARD_DP alpha 220 on white; the one name on concrete (MACHINE SHOP) in F_H3 C_BONE alpha 150. The validator requires every stencil centre to be on a T_WHITE or T_GREY tile.
- Safety stencils are flat and carry no punchline: LASER. EYES ON IT. and HOT WORK OUTSIDE.
- The orange wall tiles carry the station names, so floor stencils are zone names, warnings and the MOVE NIGHT memory lines only.

### 2.6 How zones read without coloured floor areas

| zone | floor cue | furniture cue | wall or sign cue |
| --- | --- | --- | --- |
| Laser | char mat with hazard border, red 4 x 2 stand here block in front | red machine body, silver duct up the wall | orange LASER CUTTER tile, BE NICE. BE MAKING. CLEAN UP. triptych, extinguisher |
| Scrap bays and bike station | red 3 x 1 in front of the bays | four bays painted green, yellow, orange, red; black cabinet with a bike stand | PARKING PASSES clipboard sign is on the lockers; the bays' coloured fronts are the sign |
| Wood tools | one tile orange ring around the bay | white tables with orange edge banding, bandsaw, drill press | orange WOOD TOOLS tile |
| Printers | grey mat under the counter, red 3 x 2 in front | black steel shelf counter with spools on top | orange 3D PRINTER tile on the counter's back board |
| Electronics | orange 1 x 6 aisle band, grey mat under the bench | long white top bench along the east wall, cool bench lamp | orange ELECTRONICS tile, pegboard sign with a tape strip |
| Coworking | red 1 x 7 aisle runway, chair wheel streaks | plywood desks with monitors, five chairs, pendants on yellow cords | hack letters on the amber band |
| HYH table | orange L around it | plywood trestle table, QR signs, orange bucket | hall pendant above, the whiteboard beside it |
| Craft corner | orange 5 x 1 stripe, grey mat under the counter | bone counter with orange edge banding, red cutter, heat press | orange VINYL STATION tile |
| Sewing | 2 x 2 red and orange checker | white table, green Consew, thread rack | orange SEWING tile |
| Member storage | white | grey lockers with amber padlocks, bankers boxes | PARKING PASSES 7 DAYS / RED TAGS 14 sign |
| Kitchen | red 2 x 3 in front of the sink | bone sink cabinet, snack fridge, wire rack, closet block | orange DARKROOM wall panel with the DARKROOM IN USE sign, KITCHEN tile, warm kitchen lamp |
| Machine shop | concrete tiles, tape line at the fence, CAUTION tape line under the panel | 1.1 tall steel half wall with posts, green lathe, blue mill, red tool cart | CAUTION sign above the panel, MACHINE SHOP stencil on the concrete |
| Welding bay, trash, back door | tape ring, orange 3 x 2 in front of the recess and the door | cylinders, blue curtain, wheelbarrow and bins in a dark recess, green door | TURN OFF GAS on the door, EXIT above the recess |
| Free stuff shelf | orange 2 x 3 in front | grey bins in front of a shelf | FREE STUFF tile |
| Show window | red 12 x 1 stand here row at r21, amber frame | east display shelf with robot, LED cube, marquee | OPEN sign, DONATE screens, facade sign above |
| Door | terrazzo vestibule tiles | mullions, posts, two door leaves, sidewalk slab, A-frame outside | 108 on the east leaf, WIFI: ASK THE HOST and EXIT on the header, facade sign |
| Coffee and Books | white | small table with Keurig and carafe, black bookshelf | COFFEE AND BOOKS tile |
| Vending | white | lit front panel, honour jar on top | none (the lit front is the sign) |

Floor colour is always on the approach side of a station. That is the one rule a player learns in a night: red or orange under your feet means you are standing where the work happens.

## 3. CLASSIC TUESDAY geometry (level 0)

Axes: -z back wall, +z street and camera, -x west. Walkable x -11.6..11.6, z -6.6..7.0, doorway to z 8.5 within 1.1 of door_x 0.0. Every box below passed space_check.py (section 11.4): pairs touch (gap at most 0.05), are separated by at least 0.76 on one axis with no diagonal pinch, or are closed by a third box.

### 3.1 Stations (23)

Centre x, z; half extents hw, hd; ry rotates the model group (collision boxes are given already rotated); face is the approach side bots and newbies use (section 7.3). Labels are the base level's; other levels may override.

| id | label | x | z | hw | hd | ry | face | footprint x | footprint z | note |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ST_LUMBER | SCRAP BAYS | -0.2 | -6.5 | 0.9 | 0.5 | 0 | +z | -1.1..0.7 | -7.0..-6.0 | plywood source; touches the bike cabinet |
| ST_LASER | LARGE 80W LASER | -3.4 | -5.95 | 1.4 | 0.8 | 0 | +z | -4.8..-2.0 | -6.75..-5.15 | back wall |
| ST_PRINT1 | OUCH | 7.0 | -3.1 | 0.55 | 0.45 | 0 | +z | 6.45..7.55 | -3.55..-2.65 | nested in the printing counter |
| ST_PRINT2 | DR. STRANGE | 8.6 | -3.1 | 0.55 | 0.45 | 0 | +z | 8.05..9.15 | -3.55..-2.65 | the one that is down |
| ST_CUTSALOT | SIR CUTSALOT | -8.0 | -1.8 | 0.75 | 0.5 | 0 | +z | -8.75..-7.25 | -2.3..-1.3 | nested in the craft counter (hd matches it) |
| ST_BENCH | ELECTRONICS | 10.9 | 0.1 | 0.7 | 1.9 | pi/2 | -x | 10.2..11.6 | -1.8..2.0 | east wall, long axis z |
| ST_SINK | SINK | -11.0 | -5.1 | 0.6 | 0.9 | 0 | +x | -11.6..-10.4 | -6.0..-4.2 | touches the fridge; its back strip is filled by the fridge |
| ST_FRIDGE | SNACK FRIDGE | -11.0 | -6.5 | 0.55 | 0.5 | 0 | +x | -11.55..-10.45 | -7.0..-6.0 | back west corner |
| ST_TRASH | TRASH (ALLEY, 225 FT) | 9.85 | -6.6 | 0.6 | 0.5 | 0 | +z | 9.25..10.45 | -7.1..-6.1 | wheelbarrow and bins in the hallway recess; touches the mill and the door |
| ST_FREEPILE | FREE STUFF | -11.1 | -2.4 | 0.5 | 1.0 | 0 | +x | -11.6..-10.6 | -3.4..-1.4 | west wall |
| ST_SHOW | SHOW WINDOW | 5.75 | 6.75 | 3.75 | 0.3 | 0 | -z | 2.0..9.5 | 6.45..7.05 | east display window |
| ST_COUCH | COUCH | -7.6 | -4.3 | 0.45 | 1.2 | pi/2 | none | -8.05..-7.15 | -5.5..-3.1 | by the laser, facing it |
| ST_TABLE | HYH TABLE | -0.5 | 2.6 | 1.9 | 0.8 | 0 | none | -2.4..1.4 | 1.8..3.4 | the work island |
| ST_VEND | VENDING | -11.3 | 4.3 | 0.3 | 0.5 | 0 | +x | -11.6..-11.0 | 3.8..4.8 | west wall |
| ST_BOARD | WHITEBOARD | -4.4 | 1.6 | 0.7 | 0.15 | 0 | +z | -5.1..-3.7 | 1.45..1.75 | rolling board by the table |
| ST_SEWING | SEWING | -11.2 | 2.2 | 0.4 | 0.8 | 0 | +x | -11.6..-10.8 | 1.4..3.0 | west wall |
| ST_BATH | BATHROOM | -6.9 | -6.85 | 0.5 | 0.3 | 0 | +z | -7.4..-6.4 | -7.15..-6.55 | a door in the back wall |
| ST_STORAGE | MEMBER STORAGE | -11.3 | 0.0 | 0.3 | 0.6 | 0 | +x | -11.6..-11.0 | -0.6..0.6 | lockers and bankers boxes |
| ST_FORGE | WELDING BAY | 11.4 | -4.2 | 0.5 | 0.8 | 0 | -x | 10.9..11.9 | -5.0..-3.4 | east wall, behind tape |
| ST_LIBRARY | COFFEE AND BOOKS | -10.2 | 6.7 | 0.6 | 0.35 | 0 | -z | -10.8..-9.6 | 6.35..7.05 | front west corner, touches the coffee cabinet and the window |
| ST_BACKDOOR | BACK DOOR | 11.05 | -6.95 | 0.55 | 0.2 | 0 | +z | 10.5..11.6 | -7.15..-6.75 | in the wall plane; touches the trash |
| ST_SWITCH | (no label) | -12.1 | -1.0 | 0.15 | 0.25 | 0 | +x | -12.25..-11.95 | -1.25..-0.75 | inside the west wall between the free shelf and the lockers |
| ST_BIKE | BIKE STATION | 1.15 | -6.7 | 0.45 | 0.35 | 0 | +z | 0.7..1.6 | -7.05..-6.35 | black filing cabinet with a stand; touches the scrap bays |

ST_ enum order: the ten new ids are appended after ST_TABLE and before ST_COUNT in the order above (ST_VEND, ST_BOARD, ST_SEWING, ST_BATH, ST_STORAGE, ST_FORGE, ST_LIBRARY, ST_BACKDOOR, ST_SWITCH, ST_BIKE). Nothing goes between ST_PRINT1 and ST_PRINT2 (range loops at game.c 564, 1007, 1060; draw.c 179, 193; world.c 320, 853). ST_SWITCH and ST_BACKDOOR lie inside the wall planes (behind the collide clamps) so they block nothing; the validator skips them in the pair test and only checks their approach points.

Approach points (face pushed out 0.55) and the tile each lands on, from the checker:

| station | point | tile | station | point | tile |
| --- | --- | --- | --- | --- | --- |
| ST_LUMBER | (-0.20, -5.45) | c20 r2 red | ST_BOARD | (-4.40, 2.30) | c13 r15 white |
| ST_LASER | (-3.40, -4.60) | c14 r4 red | ST_SEWING | (-10.25, 2.20) | c3 r15 checker |
| ST_PRINT1 | (7.00, -2.10) | c32 r8 red | ST_BATH | (-6.90, -6.00) | c9 r1 white |
| ST_PRINT2 | (8.60, -2.10) | c34 r8 red | ST_STORAGE | (-10.45, 0.00) | c3 r11 white |
| ST_CUTSALOT | (-8.00, -0.75) | c7 r10 orange | ST_FORGE | (10.35, -4.20) | c37 r4 white |
| ST_BENCH | (9.65, 0.10) | c36 r12 orange | ST_LIBRARY | (-10.20, 5.80) | c3 r21 white |
| ST_SINK | (-9.85, -5.10) | c4 r3 red | ST_BACKDOOR | (11.05, -6.20) | c38 r1 orange |
| ST_FRIDGE | (-9.90, -6.50) | c4 r1 white | ST_SWITCH | (-11.40, -1.00) | c1 r10 white |
| ST_TRASH | (9.85, -5.55) | c36 r2 orange | ST_BIKE | (1.15, -5.80) | c22 r2 white |
| ST_FREEPILE | (-10.05, -2.40) | c3 r7 orange | ST_SHOW | (5.75, 5.90) | c30 r21 red |
| ST_VEND | (-10.45, 4.30) | c3 r19 white | | | |

### 3.2 Obstacles (15, collision plus a model keyed by kind)

| kind | x | z | hw | hd | footprint x | footprint z | what |
| --- | --- | --- | --- | --- | --- | --- | --- |
| OBK_CLOSET | -8.75 | -6.75 | 0.55 | 0.4 | -9.3..-8.2 | -7.15..-6.35 | supply closet and stairs block, UPSTAIRS: LIMITED ACCESS |
| OBK_WOODBAY | 3.6 | -6.5 | 1.2 | 0.55 | 2.4..4.8 | -7.05..-5.95 | bandsaw, drill press, disc sander on white tables with orange edges; touches the fence |
| OBK_FENCE | 4.88 | -5.8 | 0.08 | 1.35 | 4.8..4.96 | -7.15..-4.45 | shop west half wall |
| OBK_FENCE | 7.1 | -4.5 | 2.3 | 0.08 | 4.8..9.4 | -4.58..-4.42 | shop front half wall; the two fence boxes overlap at the corner by design |
| OBK_LATHE | 6.15 | -6.3 | 1.15 | 0.55 | 5.0..7.3 | -6.85..-5.75 | green lathe inside the fence |
| OBK_MILL | 8.7 | -6.4 | 0.55 | 0.55 | 8.15..9.25 | -6.95..-5.85 | the Blue Mill; touches the trash |
| OBK_COUNTER | 7.8 | -3.1 | 1.6 | 0.5 | 6.2..9.4 | -3.6..-2.6 | printing shelf; both printers nest inside it |
| OBK_COUNTER | -8.0 | -1.8 | 1.4 | 0.5 | -9.4..-6.6 | -2.3..-1.3 | craft counter; the cutter nests inside it |
| OBK_DESKS | 11.0 | 4.9 | 0.6 | 2.1 | 10.4..11.6 | 2.8..7.0 | coworking row, closes the front right corner |
| OBK_WINDOW | -5.8 | 6.75 | 3.8 | 0.3 | -9.6..-2.0 | 6.45..7.05 | west display shelf: CRT wall and yellow crates; touches the library |
| OBK_COFFEE | -11.2 | 6.3 | 0.4 | 0.7 | -11.6..-10.8 | 5.6..7.0 | coffee cabinet, closes the front left corner |
| OBK_LEGO | -7.0 | 4.6 | 1.1 | 0.6 | -8.1..-5.9 | 4.0..5.2 | LEGO table |
| OBK_HOST | 3.1 | 5.25 | 0.5 | 0.35 | 2.6..3.6 | 4.9..5.6 | host desk and waiver computer (moved east from x 2.2 so night slot 7 at (2.1, 5.2) is clear by 0.5) |
| OBK_ISLAND | 4.6 | 1.0 | 1.6 | 0.7 | 3.0..6.2 | 0.3..1.7 | second table, class seating |
| OBK_DONBIN | 4.4 | -3.0 | 0.3 | 0.3 | 4.1..4.7 | -3.3..-2.7 | the tall donation bin between electronics and the wood tools |

Wall sequences (so a reader can see each gap is at least 0.8 or closed):

- West wall, back to front: fridge z -7.0..-6.0, sink -6.0..-4.2 (touch), free shelf -3.4..-1.4 (0.8), lockers -0.6..0.6 (0.8), sewing 1.4..3.0 (0.8), vending 3.8..4.8 (0.8), coffee 5.6..7.0 (0.8), library across the corner (touch). The switch plate sits inside the wall at z -1.25..-0.75.
- Back wall, west to east: fridge to -10.45, closet -9.3..-8.2 (1.15), bathroom -7.4..-6.4 (0.8), laser -4.8..-2.0 (1.6), scrap bays -1.1..0.7 (0.9), bike cabinet 0.7..1.6 (touch), wood bay 2.4..4.8 (0.8), fence and shop 4.8..9.4 (touch), mill to 9.25, trash 9.25..10.45 (touch), back door 10.5..11.6 (touch).
- East wall, back to front: back door to -6.75, welding bay -5.0..-3.4 (1.75 from the door box; 1.1 from the trash's corner on the diagonal), bench -1.8..2.0 (1.6), desks 2.8..7.0 (0.8), show window across the corner (0.9 from the desks).
- Front, west to east: coffee, library, west window -9.6..-2.0, door slot -1.1..1.1 (0.9 each side), east window 2.0..9.5, desks.

The machine shop is fenced west and front and open on the east: from the fence end (9.4, -4.42) to the welding bay (10.9) is 1.5 wide, and the interior strip between the front fence (z -4.58) and the mill (z -5.85) is 1.27 wide. No station is inside the fence so bots never path in; players may wander in.

### 3.3 Anchors and areas (level fields; replace the literals in sim map section 20)

| field | value | replaces |
| --- | --- | --- |
| door_x, door_z, door_hw | 0.0, 7.9, 1.1 | DOOR_X, DOOR_Z (game.c 61-62), the 0.9 at 207 |
| lobby anchor | x = -1.2 + (slot % 4) * 0.8, z = 5.0 - (slot / 4) * 0.9 | game.c 254 (rows at z 5.0 and 4.1, 0.7 clear of the table) |
| night start | x = -1.5 + (i % 4) * 1.2, z = 4.3 + (i / 4) * 0.9 | game.c 958 (rows at 4.3 and 5.2; slot 7 is 0.5 from the host desk) |
| newbie waiting box | x -7..5, z -0.5..5.0 | game.c 750 |
| member wander box | x -9..9, z -3..5 | game.c 831 |
| floor_spot box | x -10..10, z -4.5..5.5 | game.c 871 |
| door mat (donations) | x -0.7..0.7, z 5.9..6.5 | game.c 863 |
| AC puddle box | x S[ST_SINK].x + 1.2 .. + 3.2, z S[ST_SINK].z - 1.0 .. + 2.0, via clear_spot | game.c 891 |
| opening scrap | (S[ST_LASER].x + 1.0, S[ST_LASER].z + hd + 0.8) | game.c 963 |
| chair streak box | x 9.3..9.9, z 2.8..7.0 | new |

### 3.4 Props (level 0)

Kinds and geometry are in section 10. Positions:

| kind | positions (x, y, z) and notes |
| --- | --- |
| PR_PENDANT | (10.6, 2.9, 3.4), (10.6, 2.9, 5.4) over the coworking row; (-1.2, 2.9, 2.6), (0.4, 2.9, 2.6) over the HYH table; (4.6, 2.9, 1.0) over the second table; (-10.2, 2.9, -5.0) over the kitchen |
| PR_TUBE | back wall fixtures at x -9, -3, 3, 9 (world.c 252-258, now props); two inside the shop at (6.0, 3.05, -5.6) and (8.6, 3.05, -5.6). The side wall fixtures go |
| PR_FAN | (6.0, 3.3, 2.0) and (-6.0, 3.3, 0.0), spun by world_sync at 1.2 rad/s |
| PR_EXTINGUISHER | (-1.45, 1.1, 6.95) by the front door; (-1.3, 1.1, -6.95) by the laser; (10.0, 1.1, -4.48) on the fence post outside the shop |
| PR_FIRSTAID | (-6.0, 1.6, -7.0) beside the bathroom door |
| PR_SAFETYGLASSES | (9.4, 1.5, -4.6) on the fence's east end post, facing +x (outside the shop) |
| PR_SHOPVAC | (2.0, 0, -5.6) in the 0.8 pocket between the bike cabinet and the wood bay, against the wall |
| PR_BROOM | (-10.3, 0, -6.5) leaning on the fridge |
| PR_TOOLCART | (7.8, 0, -5.2) inside the fence between the lathe and the mill |
| PR_BUCKET | (6.0, 0, -4.9) and (9.0, 0, -5.4) inside the fence; (1.4, 0, 2.2) under the HYH table |
| PR_WATER | (10.9, 0, -6.0) two cases stacked beside the door box, outside the walk line (inside the trash and door footprints' shadow) |
| PR_AFRAME | (1.8, 0, 7.6) on the sidewalk |
| PR_BIKERACK | (-3.0, 0, 7.7), four slots at x -3.0, -2.4, -1.8, -1.2, outside the walkable area |
| PR_LEGO_CRATES | (-8.9, 0, 6.2) against the west window base |
| PR_CHAIR | (10.0, 0, 3.2 + 0.8 k), k 0..4, facing +x, inside the desks' approach shadow |
| PR_PLANT | arg 0 (alive) at (-9.9, 0, -3.9) and (9.8, 0, 6.6); arg 1 (dead) at (3.6, 0.8, 5.25) on the host desk: A PLANT. IT WILL DIE HERE. |
| PR_HAT | the host's captain's hat on the host desk at (2.8, 0.82, 5.25) |
| PR_PROJECTOR | (-0.5, 3.2, 1.8) hanging over the HYH table |
| PR_SLEEPER, PR_VISITOR | not in level 0 (sections 6.5 and 6.2) |

Props have no collision. Each one that could stand in a walk line (shop vac, tool cart, buckets, water, plant) is placed inside a closed pocket or an obstacle's shadow, and the validator reports PR_SHOPVAC, PR_TOOLCART, PR_WATER and PR_BUCKET as 0.3 radius discs against the gap rule.

### 3.5 Signs (level 0, all basic+map, unlit, no shadow, via sign() at world.c 59)

| kind | texture | world size | text and style | position (x, y, z), ry |
| --- | --- | --- | --- | --- |
| SG_LOCKUP | 1024 x 256 | 6.4 x 1.6 | unchanged: the mark via hud_logo, wordmark in F_LOGO, address, tape | back wall (4.4, 2.95, -6.98), above the shop |
| SG_FACADE | 1024 x 256 | 4.0 x 1.0 | bone field, the mark via hud_logo at official colours, wordmark in F_LOGO | (0, 3.0, 7.28) facing +z on the header beam; the one new mark placement |
| SG_TILE x 11 | 256 x 128 | 0.9 x 0.45 | 0xe07a2a field, name in C_CHAR F_MONO_S, two lines max | LASER CUTTER (-3.4, 2.05, -6.98); WOOD TOOLS (3.6, 2.05, -6.98); 3D PRINTER on the counter back board (7.8, 1.45, -3.55) facing +z; VINYL STATION on the craft counter back board (-8.0, 1.45, -2.25) facing +z; ELECTRONICS (11.97, 2.05, 0.1) ry -pi/2; KITCHEN (-11.97, 2.05, -5.1) ry pi/2; FREE STUFF (-11.97, 2.05, -2.4) ry pi/2; MEMBER STORAGE (-11.97, 2.05, 0.0) ry pi/2; SEWING (-11.97, 2.05, 2.2) ry pi/2; COFFEE AND BOOKS (-11.97, 2.05, 6.3) ry pi/2; MACHINE SHOP on the fence by its east end (9.0, 1.3, -4.45) facing +z |
| SG_TRIPTYCH | 3 x 128 x 128 | 0.5 each | BE NICE. / BE MAKING. / CLEAN UP. in F_MONO_L C_CHAR on C_BONE | (-4.2, 2.95, -6.98), (-3.4, 2.95, -6.98), (-2.6, 2.95, -6.98): above the amber band, not on it |
| SG_WIFI | 512 x 64 | 2.0 x 0.25 | WIFI: ASK THE HOST in 0x5fb892 (C_LIVE) on C_CHAR | door header (0, 2.75, 7.08) facing -z (into the room) |
| SG_EXIT | 128 x 64 | 0.5 x 0.25 | EXIT in C_FAULT on C_CHAR | above the front door (0, 2.95, 7.08) facing -z; above the back hallway recess (9.85, 2.55, -6.98) |
| SG_108 | 128 x 64 | 0.4 x 0.2 | 108 in C_BONE on C_CHAR | on the east door leaf (0.5, 1.6, 7.12) facing +z |
| SG_GAS | 256 x 64 | 1.0 x 0.25 | TURN OFF GAS in C_CHAR on C_BONE | child of the back door panel at panel local (0, 1.9, 0.06) |
| SG_CAUTION | 128 x 64 | 0.5 x 0.25 | CAUTION in C_CHAR on C_HAZARD | above the shop electrical panel (6.8, 2.1, -6.98); OV_TAPE_LINE (6.3, -6.75, 7.3, -6.45) below it |
| SG_DARKROOM | 256 x 64 | 1.0 x 0.25 | DARKROOM IN USE in C_CHAR on C_BONE | on the orange kitchen wall panel (-11.1, 1.95, -6.98) |
| SG_PASSES | 256 x 128 | 0.6 x 0.3 | PARKING PASSES 7 DAYS / RED TAGS 14 in C_CHAR on 0x5fb892 with a C_FAULT corner | on the lockers (-11.97, 1.5, 0.0) ry pi/2, below the MEMBER STORAGE tile |
| SG_UPSTAIRS | 256 x 64 | 0.8 x 0.2 | UPSTAIRS: LIMITED ACCESS in C_CHAR on C_HAZARD | on the closet door (-8.75, 2.0, -6.33) facing +z |
| SG_OPEN | 256 x 128 | 1.4 x 0.7 | unchanged neon pinks | on the west window header (-3.2, 2.45, 7.13) facing +z |
| SG_PEGBOARD | 512 x 256 | 3.6 x 1.8 | as today plus a hud_tape strip along its top | east wall behind the bench (11.97, 2.15, 0.1) ry -pi/2 |
| SG_WC | 64 x 64 | 0.25 x 0.25 | WC in C_CHAR on C_BONE | on the bathroom door (-6.9, 1.9, -6.8) |
| SG_PAY | 128 x 64 | 0.4 x 0.2 | PAY FOR SNACKS HERE in C_CHAR on C_BONE | on the fridge's +x face (-10.44, 1.7, -6.5) ry pi/2 |

Twenty six small draws. The whiteboard sign of world.c 597 goes (the whiteboard is a station model); the manifesto on the right wall goes (the coworking row and the hack letters take that wall). Every texture goes through keep_tex so the rebuild releases it.

## 4. Walls, storefront shell, lighting, materials

### 4.1 Walls (paint_wall replaces paint_brick, world.c 187-213)

The interior is grey plaster with pegboard bays and the amber soffit band, not brick (space brief 9). `paint_wall(int w, int h, float units_w, const bay_t *bays, int bay_n, int side)` with the same call sites (world.c 234-236) at 1230 x 170 for the back wall and 715 x 170 for each side wall (50 px per unit on all three so dots and text match; today the side walls are sharper than the back). HUD y 0 is the ceiling edge on these planes, y h the floor line.

1. Base 0x6b675f with noise8() / 32 per 4 x 4 block.
2. Kick plate: bottom 0.12 units C_CHAR.
3. Amber band: C_HAZARD fill from wall height 2.3 to 2.55 with a 2 px C_HAZARD_DP line under it. The one amber band GANTRY allows on a wall; it is the real soffit lip.
4. Pegboard bays: for each bay (a0, a1 along the wall in units): 0xd9d3c7 rect from height 0.95 to 2.3 with 2 px dots of 0x9c968a alpha 140 every 6 px, a 3 px C_STEEL frame. Back wall bays: laser (-4.8..-2.0), wood tools (2.4..4.8). West wall bays: sewing (z 1.4..3.0), jewelry-free: free shelf (-3.4..-1.4). East wall: the pegboard sign behind the bench carries that bay.
5. Darkroom panel: back wall from x -12.3 to -9.9, kick plate to band, solid 0xd9672b. No mark on it (the mark appears only at official colours: lockup and facade).
6. Shop panel: back wall x 6.3..7.3, height 0.9..1.8, a 0x4a463f box with 2 px C_STEEL outline (the electrical panel); the CAUTION sign hangs above it.
7. Acoustic panels: 0x4a4338 rects 0.8 x 0.5 at height 2.75..3.25, one every 1.4 units, skipping bay centres.
8. East wall only: "hack" in F_H2 C_BONE centred on the band at z 4.0; the band is drawn 0.5 tall there (2.2..2.7) so the letters sit on it. ASCII lowercase is baked (32..127); it is a wall sign, not HUD copy.
9. Shade: top 15 percent darkened up to 30 and the bottom 12 percent above the kick plate darkened up to 24 (the floor side darkening the old comment promised).

### 4.2 Storefront shell (build_room, world.c 228-268)

- Kick wall stubs: m_steel 0.5 tall at z 7.1 from x -12.0 to -1.1 and 1.1 to 12.0; a 0.02 tall C_HAZARD box on top of each (the amber pinstripe).
- Door frame: m_steel_hi posts 0.1 x 2.6 x 0.1 at (+/-1.1, z 7.1), header 2.4 x 0.12 x 0.12 at y 2.6. Vestibule: a second post pair at (+/-1.16, 6.5), visual only (0.08 square, no collision, outside the door slot), and a dark m_steel soffit box 2.4 x 0.15 x 0.7 at y 2.7, z 6.8. Two door leaves 1.0 x 2.4 x 0.05 m_steel_hi frames at x +/-0.5, z 7.1 (glass implied; no transparent material), the 108 sign on the east leaf.
- Mullions: m_steel_hi posts 0.08 x 2.8 x 0.08 at z 7.12, every 2.2 units from x -11.0 to -2.2 and 2.2 to 11.0 (x -11.0, -8.8, -6.6, -4.4, -2.2, 2.2, 4.4, 6.6, 8.8, 11.0); a sill 0.1 x 0.1 at y 0.5 and a header 0.1 x 0.14 at y 2.8 across each window span. No glass mesh.
- Header beam: m_steel 0.4 deep x 0.3 tall at y 3.2, z 7.25, from each corner to x +/-1.3, broken over the door; the facade sign hangs on it. Top rails: the m_steel_hi rail (world.c 237) now runs along both side walls as well as the back.
- Sidewalk: m_steel_hi slab 2.6 x 0.04 x 1.6 at (0, -0.02, 7.9) so the walk to z 8.5 has something underfoot.
- Terrazzo vestibule tiles at cols 19..21 rows 22..23 (section 2).
- Back hallway recess: back wall opening at x 9.25..10.45: two m_steel_hi posts 0.1 x 2.4 x 0.1 at the jambs, a header at y 2.4, a C_GRIME box 1.2 x 2.4 x 0.3 set 0.3 behind the wall plane (the dark hallway), the EXIT sign above. The wheelbarrow and bins (ST_TRASH) stand in its mouth, so collide never needs a hole in the back wall.
- Back door: ST_BACKDOOR's model (section 5.9), hinged on the east jamb at x 11.6. The alley door on the right wall (world.c 242-244) goes.
- Fluorescent fixtures become PR_TUBE props (3.4). The laser duct follows S[ST_LASER]: CYL r 0.26 h 1.9 m_steel_hi at (laser.x - 2.2, 2.45, -6.5) plus a 0.5 x 0.5 x 0.3 m_steel box at the wall.
- Fence: built from the two OBK_FENCE obstacles as an m_steel half wall 1.1 tall over each footprint with m_steel_hi posts 0.1 x 1.4 x 0.1 at each end and every 2.0 units, and a C_HAZARD 0.03 x 0.08 strip at y 0.5 along the front fence (the tape). From the 48 degree camera the shop reads as a room while staying open on the east.

### 4.3 Light budget (no state change)

Counts stay 1 hemisphere, 1 directional with the only shadow, 4 point: every lit material stays l1401+s100. The six light objects are created once in world_init (world.c 681-705) and only moved and retinted per level by world_apply_lighting. Order stays kitchen, bench, laser_glow, stage (renamed hall). Point light positions derive from stations at rebuild:

| light | position | TUESDAY colour, intensity, distance |
| --- | --- | --- |
| hemisphere | | sky 0xfff2dc, ground 0x5a4a3a (raised from 0x3a2e22 so undersides are not black on white), 1.4 |
| sun | (-6, 16, 9) | 0xffe2b8, 2.3; shadow camera left/right +/-13.5, top/bottom +/-8.5, near 1, far 40, map 2048, bias -0.0003, PCF soft |
| P0 kitchen | (S[ST_SINK].x + 1.4, 2.9, S[ST_SINK].z + 0.8) = (-9.6, 2.9, -4.3) | 0xffc07a, 11, distance 8, decay 2 |
| P1 bench | (S[ST_BENCH].x - 0.6, 2.9, S[ST_BENCH].z + 1.4) = (10.3, 2.9, 1.5) | 0x8fbde6, 10, distance 8 |
| P2 laser_glow | (S[ST_LASER].x, 1.8, S[ST_LASER].z + 0.75) | 0xff3b2a, idle L->light.laser_idle (0), world_sync drives 10 + 6 sin while cutting, distance 6 |
| P3 hall | (S[ST_TABLE].x, 2.9, S[ST_TABLE].z) = (-0.5, 2.9, 2.6) | 0xffd9a8, 10, distance 9 |

Sun plus hemisphere gives about 1.0 x albedo on the floor, so 0xd9d3c7 lands near 0.85 and still shows shadow. Procedure (proposal 0): the first headless shot after the floor lands samples the tile under P0 (the kitchen red block at c3 r3) and requires every channel below 250 before any tile colour is touched; if it clips, P0 drops to 9 before the tile darkens. Pi budget (platform map): shadow map 1024, PCF plain, P1 and P3 may be set to intensity 0 (an intensity 0 light still counts, so no state change).

Pendants, tubes, screens and the laser status lamp are emissive meshes and light nothing; the four point lights sit under three of the pendants so the eye connects lamp and pool.

### 4.4 Shared materials (world_init, all MeshStandard so they instance)

Create once: m_green 0x5f8a73 (lathe, Consew, scrap bay 1), m_red 0xc23b2a (laser body, extinguishers, tool cart, cutter, scrap bay 4), m_orange 0xe07a2a (buckets, table edge banding, scrap bay 3), m_blue 0x2f4f8a (the Blue Mill, the welding curtain; m_curtain is an alias), m_cord 0xd9b43a (pendant cords, LEGO crates, scrap bay 2), m_bulb (emis 0xfff0d0, intensity from the preset), m_door 0x3f7a4a (the back door panel; m_dumpster is retired), m_white 0xe8e2d6 roughness 0.6 (work table tops), m_tvs (emis 0x8fbde6 1.4, monitor and CRT screens), m_grey 0x8b8578 (visitor and sleeper bodies). Every mat() or emis() made inside a builder goes through keep_mat into owned[] so the leaks at world.c 311, 323, 334, 392, 394, 510 are captured and released by world_set_level. Sawdust changes to 0xb08a52 and the newbie body to 0x6f685c so both survive on white tile.

## 5. Stations and elements

Each new station follows the twelve steps of sim map section 23. Fields reuse station_t (state, timer, dust, count, smell) unless a field is named. Clutter contributions go in game_update beside lines 1057-1060; spawn pacing timers go in the rush scaled block (1037-1045); dirtiness, heat, gusts and cooldowns tick on real time beside landlord (1046). Labels under 16 characters, prompts under 24, toasts under 64, banners under 48, no dashes.

### 5.1 Changes to existing stations

- Labels come from L->st[i].label. Base level: SCRAP BAYS, LARGE 80W LASER, OUCH, DR. STRANGE, SIR CUTSALOT, ELECTRONICS, SINK, SNACK FRIDGE, TRASH (ALLEY, 225 FT), FREE STUFF, SHOW WINDOW, COUCH, HYH TABLE. The E_STRANGE banner becomes "ANY PARTICULAR REASON %s IS DOWN?" filled with S[ST_PRINT2].label (42 characters). draw.c coach (543-560) and how_to (578-625) read S[ST_LUMBER].label, S[ST_LASER].label and S[ST_SHOW].label through one %s each. README: the joke names are retired; this is the humor brief's rename everywhere rule.
- ST_FRIDGE keeps W_FRIDGE (smell) and W_MATE, now labelled GRAB A DRINK; L->night.fridge_locked disables W_MATE and shows status LOCKED (LATE NIGHT).
- ST_SINK gains a state: E_DRAIN sets S_BROKEN (the closet drain dried out); while broken clutter +8 and W_DISH is replaced by W_DRAIN, hold 1.5 s, label POUR SIX CUPS DOWN THE DRAIN, karma 60, S_IDLE, toast SIX CUPS. THE SEWER GAS RETREATS.
- ST_TRASH is the wheelbarrow in the recess; depositing reads as taking it out. Label TRASH (ALLEY, 225 FT) (21 characters, the one label allowed over 16 because it is the joke; draw.c label width is measured, not assumed).
- ST_SHOW approach face is -z (section 7.3); projects land on the lower shelf as today.
- ST_LUMBER sawdust spawn (game.c 455) uses the +z face: (s.x + (frnd() - 0.5) * 1.6, s.z + hd + 0.6 + frnd()).
- Printers: DR. STRANGE wears a bone OUT OF ORDER tag 0.2 x 0.12 (a box child shown while S_BROKEN).
- E_HEAT puddle box is sink relative (3.3).

### 5.2 ST_VEND, VENDING

- Model: BOX 0.6 x 1.9 x 1.0 m_steel against the west wall; front panel BOX 0.02 x 1.2 x 0.7 in scr_b emissive on the +x face (s->light); six CYL r 0.05 h 0.18 m_mate in two rows behind it; coin slot BOX 0.06 x 0.1 x 0.02 m_char; honour jar CYL r 0.08 h 0.12 m_screen on top with a bone note 0.1 x 0.08 x 0.005 inside. The lit front is its silhouette.
- find_target: empty hands, boost <= 0, IDLE: W_MATE BUY A DRINK (not karma). BROKEN: W_ROCK ROCK IT (karma), hold 1.6 s.
- act W_MATE: boost 7, mates++, owes_bottle; then one in four: state BROKEN, toast IT ATE A DOLLAR. AGAIN. ROCK IT. (C_RUST), SFX_BAD.
- act W_ROCK: IDLE, karma 35, the drink comes out (boost 7, mates++, owes_bottle), toast THE HONOUR JAR HAS A NOTE AND A WASHER., FX_SPARK bone, SFX_KARMA.
- game_update: BROKEN adds 3 clutter.
- Bots: WANT W_ROCK base 1.0, not glam, when BROKEN. Bots never buy drinks.
- draw: label VENDING, status ATE A DOLLAR on BROKEN (alarm). world_sync: panel emissive 1.2 idle; while BROKEN it flickers C_FAULT at 6 Hz: `fmodf(t, 0.17f) < 0.08f ? 1.6f : 0.2f`.
- Event E_VEND sets BROKEN: THE VENDING MACHINE ATE A DOLLAR. | ROCK IT. GENTLY. IT IS OLDER THAN YOU.

### 5.3 ST_BOARD, WHITEBOARD

- Model: m_white BOX 1.3 x 0.9 x 0.04 at y 1.4 on two m_steel_hi posts 0.04 x 1.9 x 0.04 at x +/-0.6, a foot bar 1.3 x 0.04 x 0.3, four g_disc wheels r 0.05; eight marker lines BOX 0.9 x 0.03 x 0.012 alternating m_info and m_rust at y 1.72 - 0.1 k, hidden by default; a rust 0.3 x 0.2 x 0.012 corner box top right (DO NOT ERASE) with a 64 x 32 bone sign on it.
- count = lines 0..8; dust = corner intact (1) or erased (0), set to 1 at night start.
- In player_update's X branch (game.c 678-682): after the bubble, `if (box_dist(p->x, p->z, &S[ST_BOARD]) < 3.0f && S[ST_BOARD].count < 8) S[ST_BOARD].count++`. The TALKER's timed X press does this too, which is the joke.
- find_target: count > 0: W_WIPE WIPE THE BOARD (karma), hold 1.8 s.
- act W_WIPE: karma 20 + 10 * count, toast "WHITEBOARD WIPED. %d COMMITTEES DISSOLVED." with count (snprintf; 44 characters at two digits), count 0, wiped++. One wipe in five (rnd() % 5 == 0) while dust == 1: dust 0, vibe -4, toast YOU WIPED THE CORNER. IT SAID DO NOT ERASE. DATED 2019. (56 characters).
- game_update: count * 1.5 clutter.
- Bots: WANT W_WIPE base 0.4 + 0.15 * count, not glam. TALKER only: WANT W_LOITER base 0.5 whose goal point is the board's approach point pushed 1.2 further out; goal_valid is true while the TALKER's talk_in > 0; it stands there and fills the board.
- draw: label WHITEBOARD, height 1.9, status %d LINES at count >= 4 (alarm at 8). world_sync: shows the first count line meshes (children 4..11 of the group; keep them before any other dynamic child) and hides the corner when dust == 0.

### 5.4 ST_SEWING, SEWING

- Model: m_white table BOX 1.4 x 0.05 x 0.8 at y 0.76, four m_steel_hi legs; the Consew BOX 0.5 x 0.35 x 0.25 m_green at y 0.95 with a m_char arm 0.1 x 0.3 x 0.1 and a m_steel_hi wheel disc; a second machine BOX 0.4 x 0.3 x 0.2 m_bone; a thread rack of six CYL r 0.02 h 0.1 cycling m_rust, m_info, m_cord, m_green, m_violet, m_bone; a grey chair; s->light a sphere r 0.03 (m_bulb) at the Consew's needle.
- find_target: always W_SEW SEW SOMETHING (clout), hold 1.6 s.
- act: make_project(p, 4) with SEWING_PROJECTS (humor brief 2.8) worth 300 + rnd 100, sewn++. Kind 4 needs: project_value case, item_name case, world_item_obj model (m_violet BOX 0.5 x 0.12 x 0.4 with a C_BONE strap 0.06 x 0.13 x 0.42), floor_tags unchanged.
- While anyone's work_kind == W_SEW this frame: G.sewing = 0.3 (decays on real time like laser_cutting). npc_update state 1 drains patience at 0.5 x while G.sewing > 0. First time per night: toast SOMEBODY IS SEWING. THE ROOM LOOKS FRIENDLIER.
- Bots: WANT W_SEW base 0.5, glam.
- draw: label SEWING, status SOMEBODY IS SEWING while G.sewing > 0. world_sync: needle lamp emissive 2 while G.sewing > 0 else 0.4.

### 5.5 ST_BATH, BATHROOM (with the light left on)

- Model: a door panel BOX 0.9 x 2.1 x 0.08 m_bone in the back wall slot with a m_char frame, a m_steel_hi handle, the WC sign; s->light a tube BOX 0.6 x 0.05 x 0.05 (m_bulb) over the door. A door is a silhouette nothing else in the room has.
- timer = dirtiness 0..100; count = light on (1) or off (0). Dirtiness rises 0.4 per real second and +8 whenever a newbie spawns (every newbie checks it first); every spawn sets count 1. Night start: timer = L->night.start_bath, count = timer > 0.
- find_target: count == 1 and timer < 50: W_LIGHT LIGHT OFF (karma), tap. timer >= 50: W_BATH CLEAN THE BATHROOM (karma), hold 3.0 s (the longest hold in the game).
- act W_LIGHT: count 0, karma 10, no toast. act W_BATH: karma 150 (the highest), timer 0, count 0, baths++, toast BATHROOM DONE. +KARMA. NO WITNESSES.
- game_update: clutter += timer / 12 (8 at 100); newbies in state 1 drain patience at 1.4 x while timer >= 70.
- Bots: WANT W_BATH base 1.1, not glam (FOUNDER at g 0.85 scores 0.165 and almost never goes; BUILDER goes); W_LIGHT base 0.3.
- draw: label BATHROOM, height 2.2, status LIGHT ON when count && timer < 50, NEEDS A HERO when timer >= 50 (alarm at 70). world_sync: tube emissive count ? 2.5 : 0.
- Event E_BATH: SOMEONE LEFT THE BATHROOM LIGHT ON. | AND THE REST OF IT. HOLD ON THE DOOR. sets timer = max(timer, 70) and count 1.
- Award NO WITNESSES: CLEANED THE BATHROOM %d TIMES. NOBODY SAW. EVERYONE NOTICED. (baths >= 1, after GREETER in the priority order; 62 characters at two digits).

### 5.6 ST_STORAGE, MEMBER STORAGE

- Model: lockers BOX 0.6 x 1.9 x 1.2 m_steel with a 2 x 3 grid of m_char seam boxes and six C_HAZARD padlock cubes 0.06; beside them a m_steel_hi shelf with up to six bankers boxes BOX 0.4 x 0.3 x 0.3 m_cardboard stacked two high, hidden by default; a seventh box tilted rz 0.4 on the floor at the approach face, hidden by default; a MISC label (bone 0.15 x 0.06) on the top box.
- count = boxes 0..6, +1 every L->night.storage_gap rush seconds (G.next_storage). dust = notices 0..3. smell reused: nothing. timer = spill animation clock.
- At count 6: spill: one IT_SCRAP and one light IT_DONATION spawn at clear spots within 1.5 of the +x face, count 3, timer 4.0 (the tilted box shows while timer > 0), toast A BIN GAVE UP. IT WAS LABELED MISC., SFX_DROP. E_BIN forces the spill: A STORAGE BIN GAVE UP. | IT WAS LABELED MISC. POST A NOTICE.
- find_target: count >= 4: W_NOTICE POST A NOTICE (karma), tap. Holding IT_MOVEBOX (MOVE NIGHT): W_DEPOSIT SHELVE THE BOX (karma 35).
- act W_NOTICE: dust++, karma 10, notices++. At dust 3: count 0, dust 0, karma 60, an IT_DONATION spawns at the approach face (the contents become a donation and the loop closes through the free shelf), toast THIRD AND FINAL NOTICE. FIRST AND SECOND WERE IMPLIED.
- game_update: count * 1.0 clutter.
- Bots: WANT W_NOTICE base 0.6 when count >= 4, not glam.
- draw: label MEMBER STORAGE, height 2.0, status BIN %d/6 at count >= 4 (alarm at 6). world_sync: show count box meshes; the tilted box while timer > 0.

### 5.7 ST_FORGE, WELDING BAY

- Model: welding table BOX 1.0 x 0.08 x 0.7 m_steel_hi at y 0.85 on four m_char legs; two CYL r 0.14 h 1.3 m_steel_hi cylinders chained to the wall with a rust chain box; a m_blue curtain BOX 0.05 x 1.4 x 1.2 on the wall side (a box, not a double sided plane); a MIG box 0.4 x 0.5 x 0.3 m_red on a cart; a helmet sphere r 0.14 m_char on the table; s->light a sphere r 0.08 emissive 0xffb36b at the table centre, intensity 0 idle. The tape ring on the floor is its silhouette.
- dust = heat 0..100; state S_BROKEN while TOO HOT.
- find_target: IDLE: W_FORGE WELD SOMETHING (clout), hold 2.8 s (1.4 s while G.heat > 0: it is already 118 in the alley).
- act: make_project(p, 5) with FORGE_PROJECTS worth 700 + rnd 200 (kind 5 model: g_knot scaled 0.22 in m_rust, every project is a hook), welds++, dust += 35, 40 percent an IT_SCRAP (slag) at the approach face. If G.clutter > 70 at completion: fire_warn += 1.0, toast SPARKS NEAR THE FREE PILE. THE MARSHAL FELT THAT. First weld per night: toast THE FORGE IS LIT. IT IS NOW 140 IN THE ALLEY.
- game_update (real time): dust -= 4 dt; at dust >= 100 state BROKEN; BROKEN clears when dust < 60.
- Bots: WANT W_FORGE base 1.2 glam. FOUNDER gets a second glamour target at the back, which spreads the bots out.
- draw: label WELDING BAY, height 1.6, status HOT %d while dust >= 50 (ready glow), TOO HOT while BROKEN (alarm). world_sync: lamp intensity dust / 25 plus 2 + sin while a player's work_kind is W_FORGE here; FX_SPARK bursts from player_update's tick branch (660-664) at the table top.
- Safety line stays flat: the floor stencil reads HOT WORK OUTSIDE with no punchline.
- Absent in OPEN HOUSE (no hot work with visitors in the room; parked).

### 5.8 ST_LIBRARY, COFFEE AND BOOKS

- Model: small table with a Keurig (m_char 0.25 x 0.3 x 0.3), a drip carafe (m_screen CYL r 0.07 h 0.15), three mugs; a black bookshelf BOX 1.2 x 1.6 x 0.35 m_char with three boards and twenty thin spines BOX 0.03 x 0.22 x 0.18 cycling m_rust, m_info, m_bone, m_violet, m_green; up to four books on the floor in front (BOX 0.22 x 0.04 x 0.3 m_bone, hidden by default); a clipboard (m_bone 0.2 x 0.3 x 0.01) on the wall.
- count = books out 0..4, +1 every L->night.book_gap rush seconds (G.next_book).
- find_target: count > 0: W_SHELVE SHELVE A BOOK (karma), tap. act: count--, karma 12.
- Passive (the one free vibe source, it costs standing still): player_t gets read_t and read_cool. Any human within 1.6 of the ST_LIBRARY box with no stick input accumulates read_t; at 3 continuous seconds and read_cool <= 0: vibe +2, read_cool 30, FX_KARMA teal at the shelf; first time per night: toast 40 BINDERS OF DATASHEETS. ONE HAS BEEN READ.
- game_update: count * 1.5 clutter. Bots: WANT W_SHELVE base 0.3; bots never idle there.
- draw: label COFFEE AND BOOKS, status %d BOOKS OUT when count > 0, QUIET while any read_t >= 3.

### 5.9 ST_BACKDOOR, the green TURN OFF GAS door

- Model: a hinge group at the east jamb (11.6, 0, -7.0); the panel BOX 1.0 x 2.4 x 0.08 m_door centred at hinge local (-0.5, 1.2, 0) with a m_char handle; the TURN OFF GAS sign (256 x 64, bone on green) as a child of the panel; two m_steel_hi jamb posts; a small bone EXIT 64 x 32 above the frame (the padlock code lives on it, says the wiki); s->light a warm emissive slab BOX 0.9 x 2.2 x 0.02 (emis 0xffd9a8) in the doorway at z -7.1, intensity 1.5 open, 0 closed (the alley light).
- state S_IDLE closed, S_BROKEN blown open; timer = seconds to the next gust (HABOOB NIGHT). smell unused.
- E_HABOOB and E_DOOR set S_BROKEN. While open: every 6 s one IT_SAWDUST spawns at a clear spot within x [S.x - 2.6, S.x - 0.6], z [S.z + 0.7, S.z + 2.0] (cap 4 alive within 2.6 units), FX_DUST burst; the laser gains 3 dust per 10 s (G.door_dust accumulator); a continuous FX_DUST emitter at the doorway (2 particles per frame).
- find_target: BROKEN: W_SHUT SHUT THE DOOR (karma), hold 1.2 s. act: karma 60, S_IDLE, toast DOOR SHUT. THE DESERT WAITS.
- Bots: WANT W_SHUT base 1.4, not glam.
- draw: label only while BROKEN: BLOWN OPEN (alarm). world_sync: hinge ry eases toward BROKEN ? -1.2 : 0 at 6 rad/s (the panel swings into the shop corridor; no collision change: the swung panel's free edge reaches (11.24, -6.22), inside the approach zone visually only).
- E_DOOR: THE DESERT WOULD LIKE TO COME IN. | THE BACK DOOR IS OPEN. SHUT IT.
- HABOOB NIGHT: door_open_at_start, gust timer 25 + rnd 15 s reopens it with toast THE DOOR AGAIN. THE DESERT IS PATIENT.

### 5.10 ST_SWITCH, the light switch toy

- Model: a bone plate BOX 0.12 x 0.18 x 0.03 on the west wall at (-11.95, 1.3, -1.0) with a m_char toggle 0.03 x 0.06 x 0.03. No label ever, no karma, no clout, no bot interest, excluded from collide (it lies inside the wall).
- find_target: always W_SWITCH THE LIGHTS (tap, not karma).
- act: toggles G.lights_on and sets G.lights_t. DARK preset (LATE NIGHT): while on, sun intensity 1.8 and pendant bulb emissive 2.5; after 25 s it reverts with toast SOMEONE TURNED THE LIGHTS OFF TO SAVE POWER. (46 characters). Day presets: the sun dims to 1.2 for 6 s with toast LIGHTS. SO PEOPLE KNOW WE ARE OPEN. Writing an existing light's intensity is not a state change.

### 5.11 ST_BIKE, the bike station

- Model: black filing cabinet BOX 0.9 x 1.1 x 0.6 m_char with a m_steel_hi stand (post 0.04 x 1.2 x 0.04 and a clamp box); s->growth a bike (two g_disc wheels r 0.3 h 0.03 at 0.9 apart, m_rust frame tubes 0.03 x 0.03 x 0.6 and 0.03 x 0.45 x 0.03, m_char saddle) hung on the stand, hidden unless S_BROKEN.
- E_FLAT: A BIKE ROLLED IN WITH A FLAT. | THIRD FRIDAY ENERGY. FIX IT AT THE BIKE STATION. sets S_BROKEN and shows the bike. One newbie spawn in four (rnd() % 4 == 0) does the same quietly.
- While BROKEN: clutter +3, status FLAT (alarm). find_target: W_BIKE FIX THE FLAT (karma), hold 1.6 s. act: karma 60, flats++, S_IDLE, toast FIXED A FLAT. IT IS 118. THEY RODE HERE.
- Bots: WANT W_BIKE base 0.9, not glam. Award GREASE MONKEY: FIXED %d FLATS. THE PATCH KIT IS EMPTY NOW. (flats >= 2).

### 5.12 Bikes in the rack (element)

npc_t gets `int bike` (-1 or rack slot 0..3). spawn_newbie: one newbie in L->night.bike_one_in (3 in CLASSIC, 2 in OPEN HOUSE, 0 never) arrives BY BIKE: patience is L->night.patience * 0.7 (14 at 20), a bike model (geometry as 5.11, m_grey frame) appears in the next free rack slot, bike = slot. On greet the frame mesh's material pointer is set to the newbie's body material (the member's colour; the bike stays). On leaving (state 3 or 4 reaching the door) the slot frees and the model hides. Toast once per night on the first bike arrival: A NEWBIE LOCKED UP. IT IS 118. BE FAST. The rack is a visual count of who is here. Replaces the flat patience knob.

### 5.13 PR_SLEEPER and PR_VISITOR (props with a rule)

- PR_SLEEPER (LATE NIGHT): world_person(0x8b8578, true, ...) rotated rz pi/2 at y 0.55 on the couch at (-7.6, -4.3). Tap A within 1.2 (find_target W_POKE, lowest priority, not karma): toast NO. TWO NIGHTS A WEEK MAX. THEY ROLLED OVER., vibe +1, once per 30 s (G.sleeper_cool).
- PR_VISITOR (OPEN HOUSE): three grey idle people with bob only, no collision, inside the LEGO footprint at (-7.6, 4.0), (-6.9, 4.1), (-6.3, 4.0).

### 5.14 Items

- IT_MOVEBOX (MOVE NIGHT): carried, is_junk true (TRASH accepts it for 20 karma; STORAGE pays 35 via W_DEPOSIT), weight 6, red tag 9, floor tag TO STORAGE, name MOVE BOX. LABELED MISC. Model: m_cardboard BOX 0.6 x 0.5 x 0.5 with a C_BONE label 0.2 x 0.1. Six on the floor at night start at clear spots within x -6..6, z -3..4; one more every L->night.box_gap (40 s) in the rush block. Added to the three destination tables (find_target 302-313: STORAGE first if present, else TRASH; player_update arrow 686-697: STORAGE; choose_goal 540-547: STORAGE) and npc_update 790 (members carry it to STORAGE).
- Project kinds 4 (sewing) and 5 (forge) as above; sawdust and newbie colours per 4.4.

### 5.15 The event table (25 rows; weights per level in 6.4)

Row text follows the humor brief; banners under 48, subs under 64. The effect enum: E_NONE, E_HEAT, E_HABOOB, E_TACOS, E_LANDLORD, E_DAISY, E_DONATION, E_STRANGE, E_FRIDGE, E_SPAGHETTI, E_COFFEE, E_THREAD, E_LABELS, E_PURGE, E_NOSTALGIA, E_BATH, E_FLAT, E_BIN, E_TOUR, E_DRAIN, E_DOOR, E_VEND. EVENT_ROWS 25.

| row | banner | sub | effect | what happens |
| --- | --- | --- | --- | --- |
| 0 | LINUX NIGHT: CANCELED. | AS FORETOLD. NOTHING HAPPENS. | E_NONE | nothing |
| 1 | QUIET NIGHT. | SUSPICIOUS. ENJOY IT. | E_NONE | nothing |
| 2 | A FIFTH THURSDAY?! | BLASPHEMY. ALSO IT IS TUESDAY. NOTHING HAPPENS. | E_NONE | nothing |
| 3 | IT IS 118 OUTSIDE. | THE AC IS CRYING ON THE FLOOR. MOP IT. | E_HEAT | heat 12; puddle in the sink box; second puddle at a floor spot |
| 4 | HABOOB. | THE DESERT IS NOW INSIDE. SWEEP IT BACK OUT. | E_HABOOB | laser dust + night.haboob_dust; three sawdust; back door S_BROKEN |
| 5 | SOMEBODY BROUGHT TACOS. | VIBE UP. DISHES UP. SAME NUMBER, OPPOSITE SIGN. | E_TACOS | vibe +10; sink +5 |
| 6 | LANDLORD WALKTHROUGH. TEN SECONDS. | LOOK NORMAL. CLUTTER UNDER %d. | E_LANDLORD | landlord 10; the sub takes night.landlord_limit |
| 7 | DAISY CHAIN DETECTED. | FOUR POWER STRIPS DEEP. UNPLUG IT. | E_DAISY | one IT_DAISY at a floor spot |
| 8 | DONATION ON THE DOORMAT. | NOBODY ASKED FOR IT. CARRY IT TO THE FREE PILE. | E_DONATION | two donations on the door mat |
| 9 | ANY PARTICULAR REASON %s IS DOWN? | SAME REASON AS LAST WEEK. HOLD ON IT TO FIX. | E_STRANGE | PRINT2 BROKEN with spaghetti; %s is S[ST_PRINT2].label |
| 10 | SOMETHING IN THE FRIDGE HAS A PULSE. | SOMEBODY HAS TO. HOLD ON THE FRIDGE. | E_FRIDGE | fridge smell |
| 11 | PRINT LEFT RUNNING OVERNIGHT. | IT IS SPAGHETTI NOW. TRASH IT. | E_SPAGHETTI | spaghetti by a random present printer |
| 12 | COFFEE AND COWORKING RAN LONG. | OLDER THAN THE LASER. GOOD VIBES. | E_COFFEE | vibe +6 |
| 13 | THE MAILING LIST IS ON FIRE. | 200 MESSAGES. NOBODY HERE IS IN IT. VIBE DIPS. | E_THREAD | vibe -8, halved if two humans are within 2 units of each other |
| 14 | SOMEONE LABELED EVERYTHING. | INCLUDING THE LABEL MAKER. CLUTTER EASES. | E_LABELS | G.labels_t 20: floor items weigh 0.5 x |
| 15 | FREE PILE PURGE: THIRD AND FINAL NOTICE. | THE FIRST TWO WERE IMPLIED. PILE EMPTIES IN 20. | E_PURGE | G.purge_t 20; then items within 1.5 of the free shelf die, vibe -3, toast SOMEONE WANTED THAT. TOO LATE. |
| 16 | THE WOOD SHOP USED TO BE RIGHT HERE. | IT DID. THREE TIMES. VIBE DIPS. | E_NOSTALGIA | vibe -4 |
| 17 | SOMEONE LEFT THE BATHROOM LIGHT ON. | AND THE REST OF IT. HOLD ON THE DOOR. | E_BATH | 5.5 |
| 18 | A BIKE ROLLED IN WITH A FLAT. | THIRD FRIDAY ENERGY. FIX IT AT THE BIKE STATION. | E_FLAT | 5.11 |
| 19 | A STORAGE BIN GAVE UP. | IT WAS LABELED MISC. POST A NOTICE. | E_BIN | 5.6 spill |
| 20 | A TOUR GROUP. | THREE AT ONCE. SOMEBODY SAY HI THREE TIMES. | E_TOUR | G.tour_pending 3: spawn_newbie three times bypassing max_waiting once each (cap 5) |
| 21 | THE CLOSET DRAIN DRIED OUT. | SIX CUPS OF WATER. HOLD ON THE SINK. | E_DRAIN | sink S_BROKEN (5.1) |
| 22 | THE DESERT WOULD LIKE TO COME IN. | THE BACK DOOR IS OPEN. SHUT IT. | E_DOOR | back door S_BROKEN |
| 23 | THE VENDING MACHINE ATE A DOLLAR. | ROCK IT. GENTLY. IT IS OLDER THAN YOU. | E_VEND | vending S_BROKEN |
| 24 | CAN I SLEEP AT THE LAB? | NO. NOTHING HAPPENS. VIBE HOLDS. | E_NONE | nothing |

fire_event (game.c 878-885): weights come from L->night.weight[]; the repeat guard becomes a re-roll, `do { e = weighted_pick(); } while (e == G.event && tries++ < 8);`, so a zero weight row never fires by bump. Rows whose station is absent in a level carry weight 0 in that level's column, and the case still guards on S[i].present. HABOOB NIGHT forces row 4 as the first event at t 20 (night.first_event_forced = 4, else -1).

## 6. Levels

A level binds geometry, floor, lighting and a night script. LEVELS[0] is the full table of section 3; LEVELS[1..4] are copies with the deltas below applied by code in levels_init, so each is a complete level_t in memory. The attract mode always plays level 0. Every delta set passed space_check.py (11.4).

### 6.1 Level table

| idx | name | blurb (lobby card, under 48) | lighting | clock | len |
| --- | --- | --- | --- | --- | --- |
| 0 | CLASSIC TUESDAY | AN ORDINARY OPEN NIGHT. DOORS LOCK AT TEN. | TUESDAY | 7 PM to 10 PM | 180 |
| 1 | OPEN HOUSE | FIRST SATURDAY. EVERYONE CAME. SAY HI TO ALL OF THEM. | TUESDAY, sun 3.0 | 12 PM to 4 PM | 180 |
| 2 | HABOOB NIGHT | A WALL OF DESERT. IT IS INSIDE NOW. SWEEP. | DUST | 7 PM to 10 PM | 180 |
| 3 | MOVE NIGHT | THE WOOD SHOP MOVED AGAIN. THIRD TIME. BOXES. | TUESDAY | 7 PM to 10 PM | 200 |
| 4 | LATE NIGHT | AFTER TEN. CARD ACCESS ONLY. THE FRIDGE IS AWAKE. | DARK | 10 PM to 2 AM | 150 |

### 6.2 OPEN HOUSE (level 1) deltas

| change | from | to |
| --- | --- | --- |
| ST_BOARD | (-4.4, 1.6) | (-8.6, 1.0) hw 0.7 hd 0.15: rolled to the west side |
| ST_FORGE | present | absent (parked): no hot work with visitors |
| OBK_LEGO | (-7.0, 4.6) hd 0.6 | (-7.0, 4.6) hw 1.1 hd 1.0: z 3.6..5.6, the LEGO Day footprint; 0.85 from the window shelf |
| OBK_FOLD x 2 | none | (-4.8, 1.0) hw 1.2 hd 0.5 (x -6.0..-3.6, z 0.5..1.5) and (6.4, 4.0) hw 1.2 hd 0.5 (x 5.2..7.6, z 3.5..4.5): folding tables with grey chairs |
| PR_VISITOR x 3 | none | (-7.6, 4.0), (-6.9, 4.1), (-6.3, 4.0) inside the LEGO footprint |
| PR_AFRAME | one, blank | two: (1.8, 7.6) and (-2.6, 7.6), the sign reads OPEN. COME IN. YES YOU. (128 x 160 texture) |
| PR_PENDANT | six | eight: plus (-8.4, 2.9, 4.4) and (-6.8, 2.9, 4.4) over LEGO |
| PR_CHAIR | five at the desks | plus six around the folding tables inside their footprints |
| accents | level 0 | drop the HYH L west leg (c15 r13 1 x 6); add orange 2 x 2 at (c11, r12) x -5.7..-4.5, z 0.05..1.25 under table A and orange 2 x 2 at (c30, r18) x 5.7..6.9, z 3.65..4.85 under table B; both continue under the tables as the real tiles do |
| stencils | level 0 | plus LEGO DAY. NO BABYSITTING. F_MONO_S at (-7.0, 3.1) |
| lighting | TUESDAY | sun 3.0, hemisphere sky 0xfff8ec at 1.6 (noon); the first shot checks the tile under P0; if it clips hold the sun at 2.3 and raise only the hemisphere |
| night | | 6.3 column |

### 6.3 HABOOB NIGHT (level 2) deltas

Level 0 geometry. ST_BACKDOOR starts S_BROKEN (door_open_at_start) and reopens on a gust timer 25 + rnd 15 s with toast THE DOOR AGAIN. THE DESERT IS PATIENT. PR_AFRAME absent (it blew over; nobody put it out). Two extra OV_TAPE_RING overlays around the sawdust prone spots: the laser mat grown 0.3 and the scrap bays grown 0.3. Floor: wash 0xc9a46a alpha 48 over the tiles, grout 0x8e887c, plus the door wash alpha 40 within 2.5 units of both doors. A continuous FX_DUST emitter at the open door. Player speed_mul 0.92 (player_update base speed times it). Dust per cut 36 + rnd 12. Forced first event row 4 at t 20. Landlord weight 0.

### 6.4 MOVE NIGHT (level 3) deltas

| change | from | to |
| --- | --- | --- |
| OBK_WOODBAY | (3.6, -6.5) | (-7.6, 3.4) hw 1.2 hd 0.55: the wood tools are at the front left now |
| OBK_LEGO | (-7.0, 4.6) | (3.65, -6.5) hw 1.15 hd 0.6: swapped into the back slot, touching the fence |
| ST_COUCH | (-7.6, -4.3) hw 0.45 hd 1.2 ry pi/2 | (-4.4, 6.0) hw 1.6 hd 0.45 ry 0: against the west window shelf (touching, closed) |
| OBK_ISLAND | (4.6, 1.0) hw 1.6 hd 0.7 | (7.8, -3.0) hw 1.6 hd 0.6: the old printing spot |
| OBK_COUNTER (printing) | (7.8, -3.1) | (4.6, 1.0) hw 1.6 hd 0.5 |
| ST_PRINT1, ST_PRINT2 | (7.0, -3.1), (8.6, -3.1) | (3.8, 1.0), (5.4, 1.0) |
| OBK_BOXES x 3 | none | (1.6, -1.2), (-5.6, 0.0), (8.0, 4.4), each hw 0.5 hd 0.5: stacks of three m_cardboard boxes with a rust tape line |
| IT_MOVEBOX | none | six on the floor at start within x -6..6, z -3..4; one more every 40 s (box_gap) |
| SG_TILE WOOD TOOLS | on the back wall | leaning on the floor against the moved wood bay at (-6.3, 0.3, 3.4) ry 0.2 (nobody rehung it) |
| SG_TILE 3D PRINTER | on the counter back board | moves with the counter |
| SG_TILE WOOD SHOP (OLD) | none | on the back wall where the bay was, (3.6, 2.05, -6.98), crossed with hud_tape |
| accents | level 0 | printers red 3 x 2 moves to (c27, r14) x 3.9..5.7, z 1.25..2.45 under the new counter front; T_GHOST 3 x 2 at (c32, r8) where the old red block was replaced; the wood bay ring moves to (c4, r15, 7 x 5, ring) x -9.9..-5.7, z 1.85..4.85 around the moved bay; the grey mat moves to (c25, r12, 6 x 2); the ring at (c23, r0) is dropped |
| stencils | level 0 | plus WOOD SHOP (AGAIN) F_H3 at (-7.6, 1.5); THE TABLE WAS HERE F_H3 at (4.6, 3.0) in front of the printers that replaced the class table; THE COUCH WAS HERE F_MONO_S at (-7.6, -4.3) |
| overlays | level 0 | plus OV_TAPE_RING around each box stack (box grown 0.15); OV_LASER_MAT recomputed from the laser as always |
| night start | z 4.3 and 5.2 | unchanged: the couch at z 5.55 is 1.7 east of slot 0 |

The free shelf and the HYH table do not move in this level (the judges' verified geometry); the memory stencils mark the three things that did.

### 6.5 LATE NIGHT (level 4) deltas

| change | from | to |
| --- | --- | --- |
| OBK_LEGO, OBK_HOST | present | absent (the host went home, the LEGO table is folded) |
| PR_AFRAME | present | absent (it came in at close) |
| PR_SLEEPER | none | on the couch (5.13) |
| PR_PENDANT | bulb emissive 2.5 | 5.0 |
| PR_TUBE | emissive 2.2 | 0.2 (tubes off) |
| SG_OPEN | emissive 1 | material emissive 0.2 (same texture, same state) |
| ST_FRIDGE | W_MATE on | fridge_locked: the vending machine is the only drink |
| laser_glow idle | 0 | 2.0 (laser_idle) |
| newbies | normal | newbie_until 20: none spawn after t 20, so at most one or two arrive |
| solo bot top up | BUILDER, TALKER | FOUNDER, BUILDER (game.c 947 reads L->night.solo_bots) |
| accents, stencils, overlays | level 0 | level 0 |

### 6.6 Lighting presets

```c
typedef struct {
  uint32_t sky, ground; float hemi;
  uint32_t sun_col; float sun_i, sun_x, sun_y, sun_z, bias;
  struct { uint32_t col; float i, dist; } p[4];      /* kitchen, bench, laser (idle), hall; positions derive from stations */
  float pendant_e, tube_e, laser_idle;
  uint32_t grout, wash_col; int wash_a;
  float hud_tint; uint32_t tint_col;
} lighting_t;
```

| preset | hemisphere | sun | P0 kitchen | P1 bench | P2 laser idle | P3 hall | pendant_e, tube_e | grout, wash | HUD tint |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TUESDAY | sky 0xfff2dc ground 0x5a4a3a 1.4 | 0xffe2b8 2.3 at (-6, 16, 9) bias -0.0003 | 0xffc07a 11 d 8 | 0x8fbde6 10 d 8 | 0xff3b2a 0 d 6 | 0xffd9a8 10 d 9 | 2.5, 2.2 | 0x9c968a, none | none |
| DUST | sky 0xf2d2a6 ground 0x6a5a40 1.3 | 0xf2c08a 1.9 at (-4, 14, 9) bias -0.0004 | 0xffb36b 12 d 8 | same | same | 0xffc890 9 d 9 | 2.5, 2.2 | 0x8e887c, 0xc9a46a 48 | the existing heat tint (draw.c 504) at 40 percent all night |
| DARK | sky 0x8fa0c0 ground 0x2a2420 0.45 | 0xb8c8ff 0.7 at (2, 14, 20) (street light through the glass, shadows run to the back) | 0xffc07a 16 d 7 | 0x8fbde6 12 d 7 | 0xff3b2a 2.0 d 6 | 0xffd09a 18 d 9 | 5.0, 0.2 | 0x9c968a, none | none |

OPEN HOUSE uses TUESDAY with sun 3.0 and sky 0xfff8ec 1.6. In DARK the tile reads mid grey; the pendants, the vending front, the monitors, the laser lamp and the four pools carry the scene. No fog, no fifth light.

### 6.7 Night scripts

```c
typedef struct {
  float len; int clock_h0, clock_span;                 /* 180, 19, 3: CLASSIC reads 7 PM to 10 PM */
  int weight[EVENT_ROWS]; int first_event_forced;      /* -1, or a row index fired first */
  float first_event, event_gap, event_jit;             /* 14, 17, 8 */
  float first_newbie, newbie_gap, newbie_jit, newbie_until;   /* 6, 13, 7, 0 (0: all night) */
  int max_waiting, bike_one_in; float patience, member_life;  /* 3, 3, 20, 40 */
  int greet_karma; float greet_vibe, leave_vibe;       /* 100, 8, -12 */
  float dish_gap, first_donation, donation_gap, donation_jit;  /* 6, 24, 24, 10 */
  int start_dishes, start_sawdust, start_scrap, start_donations, start_boxes; float start_bath;  /* 3, 2, 1, 1, 0, 20 + rnd 30 */
  float rush_time, rush_players;                       /* 0.8, 0.12 */
  float cut_dust, cut_dust_rnd, haboob_dust;           /* 26, 12, 45 */
  float vibe_start, vibe_entropy, vibe_heal; int landlord_limit;   /* 70, 0.12, 0.15, 50 */
  float storage_gap, book_gap, box_gap;                /* 40, 50, 40 */
  float speed_mul; bool door_open_at_start; float gust_gap, gust_jit;   /* 1.0, false, 0, 0 */
  bool fridge_locked; int solo_bots[2];                /* false, BUILDER, TALKER */
} night_t;
```

| knob | CLASSIC | OPEN HOUSE | HABOOB | MOVE | LATE |
| --- | --- | --- | --- | --- | --- |
| len, clock_h0, clock_span | 180, 19, 3 | 180, 12, 4 | 180, 19, 3 | 200, 19, 3 | 150, 22, 4 |
| first_event, gap, jit, forced | 14, 17, 8, -1 | 12, 14, 6, -1 | 20, 15, 6, row 4 | 14, 16, 8, -1 | 18, 22, 8, -1 |
| first_newbie, gap, jit, until | 6, 13, 7, 0 | 4, 8, 4, 0 | 10, 16, 8, 0 | 12, 18, 8, 0 | 8, 20, 10, 20 |
| max_waiting, bike_one_in, patience, member_life | 3, 3, 20, 40 | 5, 2, 16, 45 | 3, 3, 24, 40 | 3, 3, 18, 40 | 2, 0, 20, 60 |
| greet_karma, greet_vibe, leave_vibe | 100, 8, -12 | 120, 10, -15 | 100, 8, -12 | 100, 8, -12 | 100, 8, -12 |
| dish_gap; donations first, gap, jit | 6; 24, 24, 10 | 5; 18, 18, 8 | 6; 30, 30, 10 | 7; 30, 30, 10 | 9; 0, 0, 0 (none) |
| start dishes, sawdust, scrap, donations, boxes, bath | 3, 2, 1, 1, 0, 20 + rnd 30 | 3, 1, 1, 2, 0, 0 | 3, 4, 1, 0, 0, 20 + rnd 30 | 2, 2, 2, 1, 6, 40 | 4, 1, 1, 0, 0, 60 |
| rush_time, rush_players | 0.8, 0.12 | 0.9, 0.12 | 0.8, 0.12 | 0.8, 0.12 | 0.6, 0.12 |
| cut_dust, rnd, haboob_dust | 26, 12, 45 | 26, 12, 45 | 36, 12, 45 | 26, 12, 45 | 26, 12, 45 |
| vibe start, entropy, heal, landlord_limit | 70, 0.12, 0.15, 50 | 70, 0.10, 0.15, 55 | 70, 0.12, 0.15, 50 | 60, 0.14, 0.15, 60 | 65, 0.09, 0.25, 40 |
| storage_gap, book_gap, box_gap | 40, 50, 0 | 30, 40, 0 | 40, 50, 0 | 25, 50, 40 | 60, 60, 0 |
| speed_mul; door open at start; gust gap, jit | 1.0; no; 0 | 1.0; no; 0 | 0.92; yes; 25, 15 | 1.0; no; 0 | 1.0; no; 0 |
| fridge_locked; solo bots | no; BUILDER, TALKER | no; BUILDER, TALKER | no; BUILDER, TALKER | no; BUILDER, TALKER | yes; FOUNDER, BUILDER |

config.json night_seconds scales level 0's len only (main.c 89-93); other levels use their own len. The 30 s closing warning stays absolute. draw.c clock_str (93) becomes `mins = clock_h0 * 60 + frac * clock_span * 60` with AM/PM from the hour.

### 6.8 Event weights per level (rows of 5.15)

| row | event | CLASSIC | OPEN HOUSE | HABOOB | MOVE | LATE |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | LINUX NIGHT: CANCELED. (NONE) | 1 | 1 | 1 | 1 | 2 |
| 1 | QUIET NIGHT. (NONE) | 1 | 0 | 0 | 1 | 2 |
| 2 | A FIFTH THURSDAY?! (NONE) | 1 | 1 | 1 | 1 | 1 |
| 3 | HEAT | 3 | 2 | 4 | 2 | 1 |
| 4 | HABOOB | 3 | 1 | 6 | 2 | 1 |
| 5 | TACOS | 3 | 5 | 1 | 3 | 0 |
| 6 | LANDLORD | 3 | 1 | 0 | 4 | 0 |
| 7 | DAISY | 3 | 2 | 2 | 4 | 3 |
| 8 | DONATION | 3 | 5 | 1 | 3 | 0 |
| 9 | STRANGE | 2 | 1 | 2 | 2 | 3 |
| 10 | FRIDGE | 3 | 2 | 2 | 2 | 5 |
| 11 | SPAGHETTI | 2 | 1 | 2 | 1 | 4 |
| 12 | COFFEE | 2 | 3 | 1 | 1 | 1 |
| 13 | THREAD | 2 | 0 | 1 | 2 | 3 |
| 14 | LABELS | 1 | 1 | 1 | 3 | 1 |
| 15 | PURGE | 1 | 0 | 0 | 2 | 1 |
| 16 | NOSTALGIA | 1 | 1 | 0 | 4 | 2 |
| 17 | BATH | 2 | 3 | 2 | 2 | 3 |
| 18 | FLAT | 2 | 3 | 1 | 1 | 0 |
| 19 | BIN | 1 | 2 | 1 | 3 | 2 |
| 20 | TOUR | 0 | 4 | 0 | 0 | 0 |
| 21 | DRAIN | 2 | 1 | 3 | 2 | 2 |
| 22 | DOOR | 2 | 1 | 4 | 1 | 2 |
| 23 | VEND | 2 | 3 | 1 | 1 | 2 |
| 24 | CAN I SLEEP AT THE LAB? (NONE) | 1 | 0 | 1 | 1 | 2 |
| | column total | 47 | 44 | 38 | 49 | 43 |
| | E_NONE total (must be at most a quarter) | 4 | 2 | 3 | 4 | 7 |

LATE has no newbies after t 20, so TACOS, DONATION, FLAT and TOUR are 0 there; OPEN HOUSE has no forge, which no event touches. Every column passes the quarter rule.

## 7. Data structures (game.h)

```c
#define MAX_OBST 24
#define MAX_PROPS 48
#define MAX_SIGNS 32
#define MAX_ACCENTS 24
#define MAX_OVERLAYS 12
#define MAX_STENCILS 8
#define EVENT_ROWS 25
#define LEVEL_COUNT 5

enum { FACE_NONE, FACE_PZ, FACE_NZ, FACE_PX, FACE_NX };
enum { OBK_CLOSET, OBK_WOODBAY, OBK_FENCE, OBK_LATHE, OBK_MILL, OBK_COUNTER, OBK_DESKS, OBK_WINDOW, OBK_COFFEE,
       OBK_LEGO, OBK_HOST, OBK_ISLAND, OBK_DONBIN, OBK_FOLD, OBK_BOXES, OBK_KIND_COUNT };
enum { PR_PENDANT, PR_TUBE, PR_FAN, PR_EXTINGUISHER, PR_FIRSTAID, PR_SAFETYGLASSES, PR_SHOPVAC, PR_BROOM, PR_TOOLCART,
       PR_BUCKET, PR_WATER, PR_AFRAME, PR_BIKERACK, PR_LEGO_CRATES, PR_CHAIR, PR_PLANT, PR_HAT, PR_PROJECTOR,
       PR_SLEEPER, PR_VISITOR, PR_KIND_COUNT };
enum { SG_LOCKUP, SG_FACADE, SG_TILE, SG_TRIPTYCH, SG_WIFI, SG_EXIT, SG_108, SG_GAS, SG_CAUTION, SG_DARKROOM, SG_PASSES,
       SG_UPSTAIRS, SG_OPEN, SG_PEGBOARD, SG_WC, SG_PAY, SG_KIND_COUNT };
enum { LIGHT_TUESDAY, LIGHT_DUST, LIGHT_DARK };

typedef struct { const char *label; float x, z, hw, hd, ry; uint8_t face; bool present, always_label; } st_place_t;
typedef struct { uint8_t kind; float x, z, hw, hd, ry; } obst_t;           /* replaces game.h 153 */
typedef struct { uint8_t kind, arg; float x, y, z, ry; const char *text; } prop_t;   /* arg: plant 0 alive 1 dead; aframe text */
typedef struct { uint8_t kind; const char *text; float x, y, z, ry, w, h; } sign_t;
typedef struct { uint8_t kind, c, r, cw, rh, ring; } accent_t;
typedef struct { uint8_t kind; float x0, z0, x1, z1; } overlay_t;
typedef struct { const char *text; float x, z; uint8_t font; bool warn, on_grey; } stencil_t;
typedef struct { float x0, z0, x1, z1; } box_t;

typedef struct {
  const char *name, *blurb;
  float door_x, door_z, door_hw;
  box_t newbie_box, wander_box, spot_box, doormat_box, puddle_box, streak_box;
  float lobby_x, lobby_z, start_x, start_z;
  st_place_t st[ST_COUNT];
  obst_t ob[MAX_OBST]; int ob_n;
  prop_t props[MAX_PROPS]; int prop_n;
  sign_t signs[MAX_SIGNS]; int sign_n;
  accent_t accents[MAX_ACCENTS]; int accent_n;
  overlay_t overlays[MAX_OVERLAYS]; int overlay_n;
  stencil_t stencils[MAX_STENCILS]; int stencil_n;
  lighting_t light;
  night_t night;
} level_t;

extern level_t LEVELS[LEVEL_COUNT];
extern const level_t *L;                 /* the active level */
void levels_init(void);                  /* src/levels.c: fills LEVELS[0] from the section 3 tables, copies to 1..4, applies the section 6 deltas */
void game_set_level(int idx);            /* copies L->st into S[], parks absent stations, resets G.level */
void world_set_level(const level_t *nl); /* world.c: tear down and rebuild, 9.1 */
void world_apply_lighting(const lighting_t *lp);
```

station_t gains `uint8_t face; bool present, always_label;` copied from st_place_t. player_t gains `int sewn, notices, wiped, baths, welds, flats, shelved, fixes, unplugged, dishes; float read_t, read_cool;`. npc_t gains `int bike;`. game_t gains `int level; float sewing, door_dust, labels_t, purge_t, lights_t, sleeper_cool, next_storage, next_book, next_box, gust; int tour_pending; bool lights_on, bike_toast, sew_toast, forge_toast, read_toast;`.

LEVELS is not const because levels_init applies deltas by assignment (`LEVELS[1] = LEVELS[0]; LEVELS[1].st[ST_BOARD].x = -8.6f; ...`). The level file header says: ST_COUNT, the ST_ order and OBK_KIND_COUNT are fixed per build; draw.c station_labels, world_sync and the three destination tables switch on station ids; a level can move, hide and relabel, never invent a station kind; nothing goes between ST_PRINT1 and ST_PRINT2.

### 7.1 Faces and approach_point

```c
static void approach_point(int i, float m, float *x, float *z) {
  const station_t *s = &S[i];
  switch (s->face) {
    case FACE_PZ: *x = s->x; *z = s->z + s->hd + m; break;
    case FACE_NZ: *x = s->x; *z = s->z - s->hd - m; break;
    case FACE_PX: *x = s->x + s->hw + m; *z = s->z; break;
    case FACE_NX: *x = s->x - s->hw - m; *z = s->z; break;
    default: *x = s->x; *z = s->z + s->hd + m; break;
  }
}
```

goal_point (game.c 579-591) clamps the bot to the box grown by 0.55 as now, then snaps the clamped point onto the face line, replacing the three hard coded wall tests (588-590). npc_update's deposit walk (791) calls approach_point(i, 0.9) for TRASH, FREEPILE and STORAGE. Every face in 3.1 points into open floor; the validator confirms each approach point is inside the clamps and 0.38 clear of other boxes.

## 8. What moves from hard coded to data

### 8.1 world.c

| today | becomes |
| --- | --- |
| every rectangle and stencil in paint_floor (113-176) | L->accents, L->overlays, L->stencils, L->streak_box; paint_floor(L) |
| paint_brick (187-213) | paint_wall(w, h, units_w, bays, bay_n, side); bays derived from the present wall stations |
| printing counter group at (1.2, -6.5) (314) | an OBK_COUNTER obstacle; the printer builders no longer add a counter |
| four point light positions, hemisphere and sun (681-705) | lighting_t applied by world_apply_lighting; positions from S[ST_SINK], S[ST_BENCH], S[ST_LASER], S[ST_TABLE] |
| alley door (242-244), front door posts at -8.9 and -7.1 (246-248), kick stubs (239-240) | L->door_x, L->door_hw; the storefront shell of 4.2; ST_BACKDOOR model |
| fixtures (250-264), string lights (546-553), wall bike (558-560) | PR_TUBE props; string lights and the wall bike deleted |
| plants (544) | PR_PLANT props |
| duct (266-267) | relative to S[ST_LASER] |
| every sign position (585-630) | L->signs |
| build_furniture switch on OB_* index (459-539) | loop over L->ob_n switching on kind |
| build_stations model positions | still group_at(s->x, s->z) plus t3_object_set_rotation(g, 0, s->ry, 0); absent stations built and parked at (0, -20, 0) |
| camera (707-709) | unchanged; main.c overrides each frame |

### 8.2 game.c

| today | becomes |
| --- | --- |
| LAYOUT, OBST (33-59) | L->st, L->ob in src/levels.c; game_set_level copies L->st into S[] and is called from game_init(0) and from the lobby |
| DOOR_X, DOOR_Z, the 0.9 slot (61-62, 207) | L->door_x, L->door_z, L->door_hw |
| collide and clear_spot loops over OB_COUNT (194-229) | loop L->ob_n; skip stations with !present and ST_SWITCH, ST_BACKDOOR (they lie behind the clamps anyway) |
| lobby and night start anchors (254, 958) | L->lobby_x/z, L->start_x/z |
| newbie waiting, wander, floor_spot, door mat, puddle boxes (750, 831, 871, 863, 891) | the box_t fields; floor_spot returns bool and every caller skips on false |
| opening scrap (963) | relative to the laser |
| goal_point wall tests (588-590), npc deposit faces (791) | approach_point (7.1) |
| EVENTS[].weight and the repeat bump (845-885) | L->night.weight, the re-roll |
| spawn timers and start mess (950-964) | L->night fields |
| patience 20, max waiting 3 (743, 753), member life 40 (480) | L->night.patience, max_waiting, member_life; draw.c 278 divides the ring by L->night.patience |
| dust 26 + 12, haboob 45 (423, 895) | L->night.cut_dust, cut_dust_rnd, haboob_dust |
| vibe 70, 0.12, 0.15 (949, 1062-1064), landlord 50 (1049) | L->night.vibe_*, landlord_limit |
| rush 0.8, 0.12 (1037) | L->night.rush_time, rush_players |
| solo bots BUILDER, TALKER (947) | L->night.solo_bots |
| speed 5.0 (705) | 5.0 * L->night.speed_mul |
| greet karma 100, vibe 8, leave vibe -12 (478-485, 782) | L->night.greet_karma, greet_vibe, leave_vibe |
| G.night_len from config (main.c 93) | level 0 only |

### 8.3 draw.c and main.c

- station_labels (174): skip !present; label heights per station (fridge 2.4, lumber 2.1, printers 2.0, bathroom 2.2, storage 2.0, whiteboard 1.9, laser 1.9, forge 1.6, else 1.3); status strings of section 5; the label budget of 11.3; label text from S[i].label.
- clock_str (93): L->night.clock_h0 and clock_span; AM/PM from the hour (2 AM reads 2:00 AM).
- coach (543-560) and how_to (578-625): station names through S[].label.
- floor_tags (244): IT_MOVEBOX reads TO STORAGE.
- results_screen (658): new awards NO WITNESSES and GREASE MONKEY after GREETER; the humor brief's FREE PILE CURATOR, PRINTER WHISPERER, MARSHAL'S FAVORITE, DISH PIT with the new counters.
- main.c flow (343): when the lobby confirms a level different from the built one, call game_set_level(k), game_reset_world(), world_set_level(&LEVELS[k]) before game_start_night. The lobby picker itself is the start screen design's; it writes G.level and must reset the 25 s auto start while anyone navigates. Title orbit (381) looks at (0, 0, 4.5) so the start screen opens on the storefront.

## 9. Rebuild path and the validator

### 9.1 world_set_level

1. game_set_level(k) then game_reset_world() first (items and newbies gone, holds cleared); every active player is moved to L->lobby_* so nobody stands inside a moved table.
2. world.c keeps `static t3_object *built[256]; static int built_n; static t3_texture *tex[48]; static int tex_n; static t3_material *owned[64]; static int owned_n;`. Every root object a builder adds goes through keep(obj); every hud_texture result through keep_tex; every mat() or emis() made inside a builder through keep_mat.
3. world_set_level(nl): for each built object t3_object_remove(&SC->base, o) (what world_free does, 794-795) and release; release owned materials and textures; L = nl; rerun build_room (floor and walls repainted from L), build_stations, build_furniture, build_props, build_signs; world_apply_lighting(&L->light); in debug builds run level_check(L) and log.
4. Lights and camera are created once in world_init and never rebuilt, so a level switch adds no render state.
5. world_init builds LEVELS[0].
6. Switch between levels 0 and 3 ten times headless and watch memory and the generated_missing log (main.c 413).

### 9.2 level_check(const level_t *L) (debug world_set_level and tools/smoke.c; a non zero count fails the smoke test)

1. Collect every present station box (except ST_SWITCH and ST_BACKDOOR, which lie behind the clamps) and every obstacle box. For every pair: gaps gx and gz. Closed if both at most 0.05 (an overlap deeper than 0.05 on both axes is reported unless the pair is a declared nest: the printers in their counter, the cutter in its counter, the two fence boxes at their corner). Otherwise error GAP if gx < 0.76 and gz < 0.76 and the corner distance hypot(gx, gz) < 0.76 and no third box intersects the segment between the two centres. Warning NARROW if the open axis gap is between 0.76 and 1.00 while the other axis overlaps.
2. Wall strips: a box face between 0.05 and 0.76 from a clamp line is an error unless another box fills the strip (the fridge fills the sink's back strip; the window shelf fills the MOVE couch's front strip).
3. Every present station with a face: approach_point(i, 0.55) inside the clamps and at least 0.38 from every other box (nests excluded). Error APPROACH.
4. The 8 lobby and 8 night start positions each at least 0.4 from every box. Error SPAWN.
5. spot_box, newbie_box, wander_box, doormat_box: at least 60 percent of 200 sampled points pass clear_spot(0.6). Error RECT.
6. Every accent inside c 0..40, r 0..23; T_CHECK is exactly 2 x 2. Error FLOOR.
7. Every stencil centre on a T_WHITE or T_GREY tile. Error STENCIL.
8. Every sign and prop inside the room (x within +/-12.3, z within -7.15..8.5) with y in 0..3.4; PR_SHOPVAC, PR_TOOLCART, PR_WATER and PR_BUCKET also run through rule 1 as 0.3 radius discs. Error PROP.

tools/gapcheck.py is the Python port (the current scratchpad/check/space_check.py plus rules 5 and 8) kept beside the C data as the reference; run it on every edit to levels.c.

### 9.3 Scripted nights (tools/shot.mjs)

shot.mjs takes cart, prefix, frames and a JSON script of pad inputs (from, to, pad, buttons, lx, ly; seed 7 unless PACE is set). For each level: a script that joins pad 0, picks the level in the lobby (stick right k times, A), starts the night, then idles while three bots play; frames at 5, 60, 120 and the last second; then the results screen. Read the log for GAP, APPROACH, SPAWN, RECT, STENCIL, PROP, generated_missing, and for any bot whose position changed less than 0.5 in 10 s outside a hold (the wedge detector prints BOT_STILL slot x z).

## 10. Model polish list (silhouettes first)

Each item: one dominant colour, one tall element, fewer small parts. Geometry in world units as world.c's BOX(w, h, d) and CYL(r, h) through part(); every piece uses g_box, g_cyl, g_sphere, g_cone, g_disc or g_knot and a shared material so meshes keep instancing; cast_shadow off on every dome, bulb, tube, fan, sign and screen, on for tables, machines, shelves and people.

1. Laser (ST_LASER): body BOX 2.8 x 0.9 x 1.6 m_red at y 0.45; m_char skirt 2.8 x 0.12 x 1.6 at y 0.06; m_steel_hi rim; amber lid 2.5 x 0.1 x 1.3 at y 1.02; m_screen glass 2.1 x 0.04 x 1.0 at y 1.08; control panel m_char 0.5 x 0.3 x 0.25 at local (1.15, 1.05, 0.5) with an amber screen 0.3 x 0.18; e-stop CYL r 0.06 h 0.05 m_red; a chiller BOX 0.5 x 0.5 x 0.5 m_steel at (-1.8, 0.25, 0.2); the five vent slats go; no decal (no mark on props). s->beam and s->light unchanged. Duct per 4.2. Label height 1.9.
2. Scrap bays (ST_LUMBER): four open bays BOX 0.42 x 1.2 x 0.9 side by side at local x -0.63, -0.21, 0.21, 0.63, fronts in m_green, m_cord, m_orange, m_red; three plywood sheets 0.02 x 1.1 x 0.8 leaning rz -0.13 in bays 1 to 3; a m_cardboard offcut box 0.3 x 0.2 x 0.2 with a C_FAULT tag quad 0.1 x 0.14 (m_red thin box) in bay 4. The uprights and two by fours go.
3. Bike station (ST_BIKE): 5.11.
4. Printing counter (OBK_COUNTER kind, printing): m_char shelf BOX 3.2 x 0.75 x 1.0 with a back board 3.2 x 1.1 x 0.05 at y 1.3 (the 3D PRINTER tile hangs on it); four spools CYL r 0.12 h 0.08 on top cycling m_pcb, m_amber, m_violet, m_bone; a resin printer box 0.3 x 0.35 x 0.3 m_char and a filament dryer 0.3 x 0.3 x 0.3 m_bone below. Printers as today (frames 0xe0602a and 0x9b7fd0 promoted to shared m_ouch and m_strange, bed, gantry, growth knot, lamp), base y 0.86; DR. STRANGE's OUT OF ORDER tag (5.1).
5. Craft counter (OBK_COUNTER kind, craft) and cutter (ST_CUTSALOT): counter BOX 2.8 x 0.75 x 1.0 m_bone with a 0.06 band of m_orange along the front top edge, back board 2.8 x 0.9 x 0.05 m_bone (the VINYL STATION tile); cutter BOX 1.2 x 0.3 x 0.5 m_red with a m_char head, violet roll CYL r 0.07 h 1.0 along x at y 1.05, blue LCD; heat press BOX 0.4 x 0.35 x 0.4 m_char with a m_steel_hi platen; a Cameo BOX 0.4 x 0.12 x 0.2 m_bone.
6. Electronics bench (ST_BENCH): top BOX 1.3 x 0.08 x 3.8 m_white at y 0.86, ESD mat 1.1 x 0.01 x 3.4 m_info; two monitors (m_char bezel 0.5 x 0.32 x 0.04 with an m_tvs 0.42 x 0.26 x 0.01 face on the -x side); scope; iron stand; a rack BOX 0.3 x 1.4 x 3.6 m_steel_hi against the wall with four instrument boxes m_steel and m_led dots; the twelve drawers become one BOX 0.6 x 0.5 x 1.2 m_info with three m_char lines; a red lift table m_red 0.6 x 0.5 x 0.5 at the bench's back end inside its footprint; LEDs from six to three.
7. Coworking desks (OBK_DESKS): three desks BOX 1.1 x 0.75 x 1.3 m_plywood with a 0.03 lip at z 3.5, 4.9, 6.3; each with a monitor (bezel m_char 0.5 x 0.32 x 0.04, m_tvs face on the -x side, which the camera at 55 degrees from +z sees) and a keyboard sliver m_steel 0.4 x 0.02 x 0.14; a cable knot under one desk; five PR_CHAIR in front.
8. HYH table (ST_TABLE): plywood top BOX 3.6 x 0.1 x 1.5 m_plywood at y 0.9 on two trestles (two m_wood A-frames each of two legs 0.05 x 0.9 x 0.05 at rz +/-0.25 with a cross bar); two QR sign boxes 0.25 x 0.25 x 0.02 m_bone with a m_char inner square standing on top; an orange bucket CYL r 0.16 h 0.3 m_orange under it (PR_BUCKET); a blue bin BOX 0.4 x 0.3 x 0.3 m_info under it; a bell jar sphere r 0.12 m_screen; six grey chairs (m_steel) inside the footprint. The vise, toolbox, knot and lower shelf go.
9. Whiteboard (ST_BOARD): 5.3.
10. Kitchen (ST_SINK, ST_FRIDGE, OBK_CLOSET): sink as now (bone cabinet, m_white top, steel basin, faucet, dishes by m_plate identity, world.c 863-869); a wire rack BOX 0.5 x 1.5 x 0.4 m_steel_hi with three bottle cylinders (m_info, m_bone, m_amber) and an MSDS binder box m_bone 0.25 x 0.3 x 0.06; snack fridge: m_char box 1.0 x 1.9 x 0.95 with a m_screen glass door panel 0.7 x 1.4 x 0.02 on the +x face, three bone shelves behind it with soda cans CYL r 0.04 h 0.12 in m_red, m_info, m_amber, the snack box m_cardboard 0.5 x 0.3 x 0.4 on top with the PAY sign, smell bulb as today; the Club-Mate crate and five magnets go. Closet block: m_steel BOX 1.1 x 2.6 x 0.8 with a m_steel_hi door frame carrying the UPSTAIRS sign, a stair stub of four m_wood steps 0.9 wide behind it, PR_BROOM leaning.
11. Bathroom door (ST_BATH): 5.5. First aid box PR_FIRSTAID beside it.
12. Couch (ST_COUCH): rust base 3.2 x 0.35 x 0.9 m_rust (lengthwise along the station's ry), four cushions alternating m_rust and m_amber 0.75 x 0.2 x 0.8 at y 0.45, back 3.2 x 0.5 x 0.2 m_rust, arms m_bone; one fabric, the pillow stays amber.
13. Free stuff shelf (ST_FREEPILE): shelf BOX 1.0 x 1.6 x 0.4 m_steel_hi with three boards; two grey bins BOX 0.5 x 0.35 x 0.4 m_steel in front (Tech Tear Apart); a CRT (m_crt with a m_screen face), two boxes and a cable knot on the shelves; a FREE sign 128 x 48 bone on the shelf post.
14. Member storage (ST_STORAGE): 5.6.
15. Sewing (ST_SEWING): 5.4.
16. Vending (ST_VEND): 5.2.
17. Library (ST_LIBRARY) and coffee cabinet (OBK_COFFEE): 5.8; the cabinet is the m_steel cabinet with a wood top, Keurig, carafe and three mugs of world.c 481-486.
18. LEGO table (OBK_LEGO): m_white top 2.0 x 0.05 x 1.2 at y 0.75, four legs, a big yellow container BOX 0.5 x 0.4 x 0.4 m_cord on top, six brick boxes 0.16 x 0.1 x 0.08 in m_red, m_info, m_cord, m_green; PR_LEGO_CRATES: three BOX 0.45 cubes m_cord stacked two and one against the west window base with 12 small bricks cycling m_rust, m_info, m_led.
19. CRT wall (OBK_WINDOW): shelf BOX 7.6 x 0.5 x 0.6 m_char at y 0.25 with a m_bone back board 7.6 x 1.6 x 0.05 at y 1.3, z 7.05; eight CRTs BOX 0.42 x 0.36 x 0.4 m_crt in two rows of four with m_tvs 0.3 x 0.3 x 0.01 faces on the +z side (toward the street); three yellow cubes m_cord 0.45 with m_led strips at the west end; two posters as 0.3 x 0.4 x 0.02 m_bone boxes with a m_rust band.
20. Show window (ST_SHOW): display shelf BOX 7.5 x 0.5 x 0.6 m_char at y 0.25 with a m_bone back board 7.5 x 1.6 x 0.05 at y 1.3, z 7.05; the toy robot (m_steel body 0.25 x 0.4 x 0.2, sphere head, m_led eye); an LED cube (m_screen BOX 0.3 with eight tiny m_led cubes at its corners); a marquee BOX 0.9 x 0.25 x 0.06 m_char with a scr_a emissive strip; two DONATE screens BOX 0.3 x 0.3 x 0.03 m_tvs facing +z; a tombstone m_steel 0.3 x 0.4 x 0.08; three bone pedestals (the deposit spots, s->growth on the centre one); the trophy; three track light cans CYL r 0.05 h 0.12 m_steel_hi on a rail at y 2.7 (no light). Frame posts come from the mullion list.
21. Host desk (OBK_HOST): plywood desk 1.0 x 0.75 x 0.7 m_plywood, a monitor (m_char with m_tvs face toward -z), a clipboard m_bone, the dead plant and the captain's hat (PR_HAT: a m_char disc r 0.14 h 0.03 with a m_bone band and a m_cord badge) as props on top.
22. Second table (OBK_ISLAND): m_white top 3.2 x 0.06 x 1.4 at y 0.78 on m_steel_hi legs, six grey chairs inside the footprint; the robot and parts boxes of world.c 513-521 go to the show window.
23. Wood tools bay (OBK_WOODBAY): two white tables 1.1 x 0.75 x 1.0 m_white with m_orange edge bands; a green bandsaw (m_green column 0.3 x 1.4 x 0.3 with a m_steel_hi table 0.5 x 0.04 x 0.5 at y 0.9), a drill press (m_steel_hi column CYL r 0.05 h 1.5, head box 0.3 x 0.3 x 0.4 m_green, table disc), a disc sander (m_char box 0.3 x 0.25 x 0.3 with a g_disc on edge m_steel_hi); parts drawers BOX 0.5 x 0.4 x 0.4 m_info under one table.
24. Donation bin (OBK_DONBIN): m_cardboard BOX 0.5 x 1.4 x 0.5 with a m_char slot and a bone label.
25. Machine shop: fence per 4.2; lathe as now in m_green on a m_char base, long axis x; mill in m_blue (column 0.4 x 1.6 x 0.5, m_steel_hi table 0.9 x 0.12 x 0.4 at y 0.9, head 0.35 x 0.5 x 0.4, a DRO box with an m_led); PR_TOOLCART (m_red BOX 0.6 x 0.9 x 0.45 with three m_char drawer lines and four tiny wheels); two PR_BUCKET; the electrical panel is in the wall texture; eight m_steel_hi 0.04 chip cubes on the concrete around the lathe.
26. Welding bay (ST_FORGE): 5.7.
27. Trash (ST_TRASH): wheelbarrow tray BOX 0.9 x 0.3 x 0.6 m_steel tilted rx -0.15 at y 0.45, two m_wood handle CYL r 0.025 h 0.9 at the back, one wheel torus (the torus geometry of world.c 561, created and released as today) r 0.2 tube 0.03 m_char at the front; two bins CYL r 0.25 h 0.7 m_char and m_info behind it; the recess and EXIT sign of 4.2. The dumpster goes.
28. Back door (ST_BACKDOOR): 5.9. Water cases PR_WATER: BOX 0.5 x 0.4 x 0.35 m_info, two stacked.
29. Light switch (ST_SWITCH): 5.10.
30. Pendant (PR_PENDANT): cord CYL r 0.012 from y to 3.4 in m_cord; dome CONE r 0.32 h 0.28 m_char pointing down at y; bulb SPHERE r 0.07 m_bulb just inside the dome at y - 0.08 with emissive intensity from the preset; cast_shadow off.
31. Tube (PR_TUBE): the fixture of world.c 250-258 (m_steel_hi box 3.0 x 0.08 x 0.14 and an emissive tube 2.8 x 0.06 x 0.08) with emissive intensity from the preset's tube_e.
32. Fan (PR_FAN): hub CYL r 0.1 h 0.12 m_steel_hi at y 3.3 and four m_wood blades 0.9 x 0.02 x 0.16 at ry 0, pi/2, pi, 3 pi/2; world_sync spins the group at 1.2 rad/s; cast_shadow off.
33. Extinguisher (PR_EXTINGUISHER): CYL r 0.09 h 0.45 m_red at y 1.1 on a m_steel_hi bracket BOX 0.12 x 0.3 x 0.05 against the wall. First aid (PR_FIRSTAID): BOX 0.3 x 0.3 x 0.1 m_bone with a red cross of two BOX 0.18 x 0.05 x 0.02 and 0.05 x 0.18 x 0.02 m_red. Safety glasses (PR_SAFETYGLASSES): BOX 0.5 x 0.3 x 0.06 m_bone with four tiny m_visor boxes.
34. Shop vac (PR_SHOPVAC): CYL r 0.22 h 0.5 m_char with a g_knot hose scaled 0.2 in m_rust. Broom (PR_BROOM): CYL r 0.02 h 1.3 m_wood leaning rz 0.15 with a BOX 0.3 x 0.08 x 0.08 head in m_sawdust colour.
35. A-frame (PR_AFRAME): two BOX 0.6 x 0.9 x 0.03 m_bone leaning rz +/-0.3 meeting at y 0.9 with a m_char hinge bar; in OPEN HOUSE a 128 x 160 sign OPEN. COME IN. YES YOU. on the street face.
36. Bike rack (PR_BIKERACK): four inverted U tubes (two posts 0.03 x 0.8 x 0.03 and a top bar 0.03 x 0.03 x 0.5 m_steel_hi) at 0.6 pitch; bikes per npc.bike (5.12).
37. Chair (PR_CHAIR): m_char seat 0.45 x 0.05 x 0.45 at y 0.45, back 0.45 x 0.45 x 0.05, a CYL post, a g_disc base r 0.3 h 0.03.
38. Plant (PR_PLANT): pot m_cardboard CYL r 0.18 h 0.3; leaves two spheres r 0.22 in m_pcb (alive) or m_scrap squashed sy 0.5 (dead, arg 1).
39. Projector (PR_PROJECTOR): BOX 0.3 x 0.12 x 0.3 m_steel hanging from a m_steel_hi rod at y 3.2; no screen (it would block the view).
40. People: world_person child order unchanged (legs at n-6 and n-5, arm pivots at n-4 and n-3, ring and hole last, world.c 815-825); the newbie grey becomes 0x6f685c and the member floor ring 0.02 thicker so it survives on white tile. Sleeper and visitors per 5.13.
41. Items: sawdust disc 0xb08a52 with a 0.02 C_HAZARD_DP rim ring (a second g_disc in the same group); puddle, daisy chain, plywood unchanged; donation CRT in m_crt; sewing project, hook and move box per 5.4, 5.7, 5.14.

Draw call sanity: every new prop is boxes and cylinders in shared materials, so they fold into the existing instanced batches; the unique materials added are the ten in 4.4, all shared, none per mesh.

## 11. Wireframes

### 11.1 Level 0 floor, tile map (41 x 24, c 0..40 left to right, r 0 back wall, r 23 front)

Legend: . white VCT, R red, O orange (the 2 x 2 checker shows as alternating R and O at c2..3 r14..15), G shop concrete, M grey mat, T terrazzo, H ghost tiles (MOVE only). The first map is the floor alone; the second overlays station footprints (#) and obstacle footprints (=) so the stand here rule can be read: every red or orange block touches the approach side of its station.

```
     0         1         2         3         4
     01234567890123456789012345678901234567890
  r 0 .......................OOOOOOGGGGGGG.....
  r 1 .......................O....OGGGGGGGOOO..
  r 2 ...RR..............RRR.OOOOOOGGGGGGGOOO..
  r 3 ...RR........................GGGGGGG.....
  r 4 ...RR........RRRR........................
  r 5 .............RRRR........................
  r 6 ..OO..........................MMMMMMM....
  r 7 ..OO..........................MMMMMMM....
  r 8 ..OOMMMMMM......................RRR..MMM.
  r 9 ....MMMMMM......................RRR.OMMM.
  r10 ....OOOOO...........................OMMM.
  r11 ....................................OMMM.
  r12 ....................................OMMM.
  r13 ...............O....................OMMM.
  r14 ..RO...........O....................OMMM.
  r15 ..OR...........O.....................MMM.
  r16 ...............O.........................
  r17 ...............O....................R....
  r18 ...............OOOOOOOOO............R....
  r19 ....................................R....
  r20 ....................................R....
  r21 ........................RRRRRRRRRRRRR....
  r22 ...................TTT..............R....
  r23 ...................TTT..............R....
```

```
     0         1         2         3         4
     01234567890123456789012345678901234567890
  r 0 .###.==.##..############=========G=###...
  r 1 .###.==.....############=========G=###O..
  r 2 .###R..##...######.RRR.OOOOO=====G==OOO..
  r 3 .###R..##...######..........=GGGGGGG..###
  r 4 .###R..##....RRRR...........=========.###
  r 5 .......##....RRRR.............=======.###
  r 6 .##O...##..................==.=#####=.###
  r 7 .##O.......................==.=#####=....
  r 8 .##O=####=......................RRR..###.
  r 9 .##.=####=......................RRR.O###.
  r10 .##.OOOOO...........................O###.
  r11 .##.................................O###.
  r12 .##......................======.....O###.
  r13 ...............O.........======.....O###.
  r14 .##O........###O#######..======.....O###.
  r15 .##R...........O#######..............###.
  r16 .##............O#######..............===.
  r17 ...............O#######.............R===.
  r18 .##....====....OOOOOOOOO............R===.
  r19 .##....====.........................R===.
  r20 .......====.............===.........R===.
  r21 .==.....................===RRRRRRRRRR===.
  r22 .=###=============.TTT.##############===.
  r23 .=###=============.TTT.##############===.
```

### 11.2 OPEN HOUSE and MOVE NIGHT, with footprints

OPEN HOUSE (LEGO table grown, two folding tables with orange 2 x 2 under them, whiteboard rolled west, HYH L west leg dropped):

```
     0         1         2         3         4
     01234567890123456789012345678901234567890
  r 0 .###.==.##..############=========G=###...
  r 1 .###.==.....############=========G=###O..
  r 2 .###R..##...######.RRR.OOOOO=====G==OOO..
  r 3 .###R..##...######..........=GGGGGGG.....
  r 4 .###R..##....RRRR...........=========....
  r 5 .......##....RRRR.............=======....
  r 6 .##O...##..................==.=#####=....
  r 7 .##O.......................==.=#####=....
  r 8 .##O=####=......................RRR..###.
  r 9 .##.=####=......................RRR.O###.
  r10 .##.OOOOO...........................O###.
  r11 .##.................................O###.
  r12 .##.......=====..........======.....O###.
  r13 .....###..=====..........======.....O###.
  r14 .##O......=====.#######..======.....O###.
  r15 .##R............#######..............###.
  r16 .##.............#######..............===.
  r17 .......====.....#######......=====..R===.
  r18 .##....====....OOOOOOOOO.....=====..R===.
  r19 .##....====..................=====..R===.
  r20 .......====.............===.........R===.
  r21 .==....====.............===RRRRRRRRRR===.
  r22 .=###=============.TTT.##############===.
  r23 .=###=============.TTT.##############===.
```

MOVE NIGHT (wood bay at the front left inside its orange ring, LEGO in the back slot, couch against the west window, printing counter in the hall with its red block and mat, ghost tiles where the old red block was, three box stacks):

```
     0         1         2         3         4
     01234567890123456789012345678901234567890
  r 0 .###.==.##..############=========G=###...
  r 1 .###.==.....############=========G=###O..
  r 2 .###R.......######.RRR..=========G==OOO..
  r 3 .###R.......######..........=GGGGGGG..###
  r 4 .###R........RRRR...........=========.###
  r 5 .............RRRR.............=======.###
  r 6 .##O.......................==.=======.###
  r 7 .##O.......................==.=======....
  r 8 .##O=####=......................HHH..###.
  r 9 .##.=####=............==........HHH.O###.
  r10 .##.OOOOO.............==............O###.
  r11 .##.......==........................O###.
  r12 .##.......==.............######.....O###.
  r13 ...............O.........######.....O###.
  r14 .##O........###O#######..######.....O###.
  r15 .##ROOOOOOO....O#######....RRR.......###.
  r16 .##.O=====O....O#######..............===.
  r17 ....O=====O....O#######.............R===.
  r18 .##.O=====O....OOOOOOOOO.........==.R===.
  r19 .##.OOOOOOO......................==.R===.
  r20 ........................===......==.R===.
  r21 .==.......######........===RRRRRRRRRR===.
  r22 .=###=====######==.TTT.##############===.
  r23 .=###=============.TTT.##############===.
```

LATE NIGHT is level 0 without the LEGO table and the host desk; HABOOB NIGHT is level 0 under the dust wash.

### 11.3 The level card in the lobby and the HUD label budget

The start screen design owns the lobby picker; this document supplies what each level shows on its card. Stick left and right moves between the five cards, A confirms, B backs out (four button bits on the phone wire; the stick is the picker).

```
+--------------------------------------------------------------+
| LEVEL 1 OF 5                              < CLASSIC TUESDAY > |
|                                                              |
|  +------------------------+   AN ORDINARY OPEN NIGHT.        |
|  |  41 x 24 tile thumb    |   DOORS LOCK AT TEN.             |
|  |  (the 11.1 map at      |                                  |
|  |  4 px per tile, white  |   7 PM TO 10 PM        3:00      |
|  |  with the accents, the |   NEWBIES: STEADY                |
|  |  shop grey, furniture  |   EVENTS: THE USUAL              |
|  |  as char blocks)       |   STATIONS: 22 OF 23             |
|  +------------------------+                                  |
|                                                              |
|  [tape strip]            A: PLAY THIS NIGHT    B: BACK       |
+--------------------------------------------------------------+
```

The thumb is painted from L->accents and the footprints with hud_rect at 4 px per tile (164 x 96), so it is always true to the level. The three fact lines come from level fields: NEWBIES from newbie_gap (STEADY under 15, CROWDS under 10, FEW over 15, NONE when newbie_until > 0), EVENTS from the two highest weight rows (THE USUAL when they are HEAT and HABOOB, DUST for HABOOB, BOXES for NOSTALGIA, THE FRIDGE for FRIDGE), STATIONS from the present count.

HUD label budget (draw.c station_labels): with 23 stations the permanent name labels of today would not fit. A station's HUD name draws only when (a) it has a status (alarm or ready), or (b) a human is within 2.2 units, or (c) L->st[i].always_label is set (laser, show window, trash, free shelf: the four the coach refers to). The orange wall tiles and the floor accents carry the names the rest of the time. ST_SWITCH never draws a label; ST_BACKDOOR only while BROKEN.

```
 quiet station        near a player           alarm                 ready
 (nothing drawn)      SEWING                  BATHROOM              WELDING BAY
                      SEW SOMETHING  (prompt  NEEDS A HERO          HOT 62
                      under the player)       (C_FAULT, blinking)   (C_HAZARD glow)
```

### 11.4 Checker output: accepted narrow corridors

space_check.py reports no GAP, APPROACH, SPAWN, STENCIL or accent bound findings on any of the five levels. The corridors it logs as NARROW (0.76 to 1.00 on the open axis) are deliberate and listed so nobody tightens them by accident:

```
warn ('NARROW', 'OBK_FENCE_S', 'OBK_COUNTER_P', 0.82)
warn ('NARROW', 'OBK_FENCE_S', 'OBK_ISLAND', 0.82)
warn ('NARROW', 'OBK_LATHE', 'OBK_MILL', 0.85)
warn ('NARROW', 'OBK_WINDOW', 'OBK_LEGO', 0.85)
warn ('NARROW', 'ST_BATH', 'OBK_CLOSET', 0.8)
warn ('NARROW', 'ST_BENCH', 'OBK_DESKS', 0.8)
warn ('NARROW', 'ST_BIKE', 'OBK_LEGO', 0.9)
warn ('NARROW', 'ST_BIKE', 'OBK_WOODBAY', 0.8)
warn ('NARROW', 'ST_BOARD', 'OBK_BOXES_B', 0.95)
warn ('NARROW', 'ST_COUCH', 'OBK_COUNTER_C', 0.8)
warn ('NARROW', 'ST_CUTSALOT', 'ST_COUCH', 0.8)
warn ('NARROW', 'ST_FREEPILE', 'ST_STORAGE', 0.8)
warn ('NARROW', 'ST_LUMBER', 'ST_LASER', 0.9)
warn ('NARROW', 'ST_PRINT1', 'OBK_FENCE_S', 0.87)
warn ('NARROW', 'ST_PRINT2', 'OBK_FENCE_S', 0.87)
warn ('NARROW', 'ST_SEWING', 'ST_STORAGE', 0.8)
warn ('NARROW', 'ST_SHOW', 'OBK_DESKS', 0.9)
warn ('NARROW', 'ST_SHOW', 'OBK_HOST', 0.85)
warn ('NARROW', 'ST_SINK', 'ST_FREEPILE', 0.8)
warn ('NARROW', 'ST_VEND', 'OBK_COFFEE', 0.8)
warn ('NARROW', 'ST_VEND', 'ST_SEWING', 0.8)
```

Declared closures the checker knows about: the printers nest in the printing counter, the cutter nests in the craft counter, the two fence boxes overlap at their corner, the fridge fills the sink's back wall strip, and in MOVE NIGHT the window shelf fills the couch's front strip.

## 12. Work breakdown

Numbered in build order; each item names the files and functions touched with a line estimate (added plus changed). Items 1 to 4 leave the game playing exactly as today on level 0 apart from the room; items 5 to 12 add the new stations one per commit with a scripted night each; 13 to 16 add the levels and the lobby hook; 17 to 19 are validation and docs.

1. Types and the level file skeleton. game.h: the enums and structs of section 7, ST_ ids after ST_TABLE, W_ kinds (W_ROCK, W_WIPE, W_SEW, W_LIGHT, W_BATH, W_NOTICE, W_FORGE, W_SHELVE, W_SHUT, W_SWITCH, W_BIKE, W_DRAIN, W_LOITER, W_POKE), E_ effects, the new player_t, npc_t, game_t fields. New src/levels.c: LEVELS[0] tables of section 3 verbatim (stations, obstacles, props, signs, accents, overlays, stencils, anchors, TUESDAY lighting, CLASSIC night); levels_init; game_set_level. build.sh: add levels.c. About 520 lines.
2. Simulation reads the level. game.c: collide and clear_spot over L->ob_n with presence skips (194-229); DOOR_X/DOOR_Z/slot replaced (61-62, 207, 254, 748, 780, 797, 863); approach_point and the goal_point rewrite (579-591); npc_update deposit walk (791); spawn boxes from L (750, 831, 871, 891); floor_spot returns bool and callers skip; night_t fields replacing the literals at 423, 436, 480, 743, 753, 895, 947, 950-964, 1037, 1049, 1062-1064; fire_event weights from L and the re-roll (878-885); presence guards in find_target (300-347), choose_goal (548-576), the deposit checks (305-312), E_STRANGE and E_SPAGHETTI (906-915), the clutter sums (1057-1060). About 260 lines.
3. The floor and walls. world.c: FW/FH 1230 x 715 (78-79); tc_, tr_, shade, concrete_tile, terrazzo_tile, paint_overlay, door_wash, wall_pool; paint_floor(L) per 2.3; paint_wall per 4.1 replacing paint_brick (187-213); the back wall and side wall call sizes (234-236). About 300 lines, 120 removed.
4. The shell, lights and the rebuild path. world.c: build_room per 4.2 (door at door_x, mullions, header beam, vestibule, sidewalk, recess, fence from OBK_FENCE, duct from the laser, side rails; remove 239-264); shared materials of 4.4 in world_init (643-673); keep, keep_tex, keep_mat and the built/tex/owned lists; world_set_level per 9.1; world_apply_lighting and the station derived light positions (681-705); build_furniture as a loop switching on kind with the OBK_ models of section 10; build_props as a loop switching on PR_ kind with the geometry of section 10 (remove 544-560); build_signs from L->signs with the painters of 3.5 (remove 585-630, keep the lockup painter). main.c camera (381): title orbit looks at (0, 0, 4.5). About 700 lines, 250 removed.
5. Station models polished. world.c build_stations: laser, scrap bays, printers on the counter, cutter on the craft counter, bench, sink, snack fridge, trash wheelbarrow, free shelf, show window, couch, HYH table per section 10; ry applied to each group; absent stations parked. draw.c station_labels: heights, S[].label, the label budget of 11.3; clock_str with clock_h0 and clock_span; coach and how_to through S[].label; the newbie ring divisor. content.h: SEWING_PROJECTS and FORGE_PROJECTS from the humor brief, the new toasts, awards and the E_ table text of 5.15. About 420 lines.
6. ST_VEND and the fridge drink. game.c find_target, act, choose_goal, goal_valid, game_update clutter; fridge_locked; world.c model and the flicker in world_sync; draw.c status; E_VEND case. About 90 lines.
7. ST_BOARD. game.c X branch (678-682), find_target, act, W_LOITER for TALKER, clutter; world.c model and line visibility in world_sync; draw.c status. About 100 lines.
8. ST_SEWING. game.c find_target, act, project kind 4 in project_value (357), item_name (113), make_project (366); G.sewing decay and the patience factor in npc_update; world.c model, needle lamp, world_item_obj kind 4; draw.c status. About 90 lines.
9. ST_BATH and E_BATH. game.c dirtiness tick (real time block), spawn_newbie hooks, find_target, act, patience factor, clutter; world.c door model and tube; draw.c status; results award NO WITNESSES. About 100 lines.
10. ST_STORAGE, E_BIN and IT_MOVEBOX. game.c next_storage in the rush block, spill, notices, W_DEPOSIT for the box in the three destination tables and npc_update (790), is_junk, weight, item_name, floor tag; world.c lockers, boxes by count, tilted box, box item model; draw.c status and TO STORAGE tag. About 150 lines.
11. ST_FORGE, ST_LIBRARY, ST_BIKE and E_FLAT. game.c forge heat on real time, find_target, act, project kind 5, fire_warn rule; library next_book, W_SHELVE, the read passive in player_update; bike station state, the one in four hook in spawn_newbie, W_BIKE; world.c three models, world_item_obj kind 5, spark lamp; draw.c statuses; awards GREASE MONKEY. About 220 lines.
12. ST_BACKDOOR, ST_SWITCH, bikes in the rack, sleeper and visitors. game.c door state, sawdust spawner and laser dust while open, gust timer, W_SHUT, E_DOOR; W_SWITCH and G.lights_on timing with the sun and pendant writes in world_sync; npc.bike slots, the one in three arrival, material swap on greet, slot free on leave, the once per night toast; W_POKE for the sleeper; world.c hinge group eased in world_sync, rack and bike models, PR_SLEEPER and PR_VISITOR builders. About 230 lines.
13. The remaining events. game.c E_THREAD, E_LABELS (weight factor in weight()), E_PURGE (timer and the free shelf sweep), E_NOSTALGIA, E_TOUR (tour_pending in spawn_newbie), E_DRAIN (sink S_BROKEN, W_DRAIN), E_HABOOB also opens the door; HABOOB's forced first event; the dust emitter at the open door; speed_mul in player_update. About 120 lines.
14. Levels 1 to 4. levels.c: the deltas of 6.2 to 6.5, the DUST and DARK presets, the four night_t columns and weight columns of 6.7 and 6.8. world.c: wash and door wash in paint_floor; tube_e, pendant_e, laser_idle and the OPEN sign emissive read from the preset; MOVE NIGHT's leaning tile and WOOD SHOP (OLD) tape sign. About 260 lines.
15. The lobby hook. main.c flow (343-351): on confirm, game_set_level, game_reset_world, world_set_level when the level differs from the built one, then game_start_night; auto start reset while picking; G.level in game_t. draw.c lobby_overlay (647-656): the level card of 11.3 with the tile thumb painter (the picker chrome itself belongs to the start screen design). About 120 lines.
16. Solo bot top up and the results screen. game.c 947 reads solo_bots; draw.c results_screen adds the new awards after GREETER and the new counters. About 50 lines.
17. level_check. game.c (debug) and tools/smoke.c: the eight checks of 9.2 with logging; tools/gapcheck.py ported from scratchpad/check/space_check.py with rules 5 and 8 added. About 220 lines (C) plus 180 (Python).
18. Scripted nights. tools/shot.mjs is unchanged; add tools/scripts/level-k.json for k 0..4 (join, pick, start, idle) and a tools/nights.sh that runs the five shots and greps the log for GAP, APPROACH, SPAWN, RECT, STENCIL, PROP, BOT_STILL and generated_missing. About 80 lines.
19. Docs. README: the space section rewritten for the storefront, the five levels, the retired joke names, the gap rule; HANDOFF: the level file header rules and the FW/FH change. About 80 lines.

Total: about 4,100 lines added or changed, about 400 removed.

## 13. Acceptance checks

Every check is a headless shot with tools/shot.mjs at seed 7 and a scripted pad, read as pixels or as log lines. Frame numbers assume 60 frames per second from cart start; PH_TITLE lasts 1.5 s (90 frames) before the attract night begins.

1. Frame 1 of a fresh boot (title): the orbit opens on the storefront. The facade sign's bone field and the mark read at the top centre of the frame; the two door leaves and the terrazzo tiles are visible between the window shelves; the OPEN sign's pink is present in the left third. No pixel in the floor region reaches 250 in all three channels.
2. Frame 600 of the attract night (level 0, three bots): the floor is mostly white; the red block in front of the laser (c13 r4 4 x 2), the orange L around the HYH table, the red runway beside the desks and the 12 x 1 red row at r21 are each identifiable by sampling their tile centres (R channel above 150 and B below 90 for red; R above 190, G 80 to 130 for orange; all channels above 170 for white); the grey shop concrete at (7.1, -5.3) samples below 130 in every channel.
3. Frame 600, light clip: the tile under P0 (c3 r3, the sink's red block) samples below 250 in every channel; the hall tile under P3 (c20 r15, white) samples below 250.
4. Frame 600, HUD label budget: no more than six station name labels are drawn (count the C_ASH label boxes in the HUD layer); the four always_label names (LARGE 80W LASER, SHOW WINDOW, TRASH (ALLEY, 225 FT), FREE STUFF) are among them.
5. Frame 600, stencils: LASER. EYES ON IT. at (-3.4, -3.3) and HOT WORK OUTSIDE at (10.0, -3.0) are legible in the floor texture (OCR not required: the pixel run of C_HAZARD_DP under the projected point is at least 40 px wide); MACHINE SHOP sits on grey.
6. Scripted CLASSIC night, pad 0 joins at frame 100, A at the lobby at frame 400, 180 s night: the log holds no GAP, APPROACH, SPAWN, RECT, STENCIL, PROP or BOT_STILL line and no generated_missing line; every present station with a face receives at least one bot visit (the per station visit counter printed at end_night shows a non zero for each).
7. Scripted OPEN HOUSE night (stick right once in the lobby, A): by frame 2000 at least five newbies have spawned and at least one toast A NEWBIE LOCKED UP. IT IS 118. BE FAST. appears in the log; three grey visitors stand inside the LEGO footprint at frame 1200 (sample their body colour at the projected points); the clock reads between 12:00 PM and 1:00 PM at frame 1200.
8. Scripted HABOOB night: at frame 1300 (t 20 s after the night starts) the ticker reads HABOOB. and the back door hinge angle is below -1.0 rad (log line DOOR OPEN); the floor sample at a white tile is warmer than CLASSIC's (R minus B at least 20 higher); at least four sawdust items exist near the door by frame 2500 unless a bot shut it (the log shows DOOR SHUT or the count).
9. Scripted MOVE night (len 200): at frame 1000 the wood bay model is inside the orange ring at x -8.8..-6.4, z 2.85..3.95 (its projected centre lands on a ring row), the LEGO table is at the back, six IT_MOVEBOX items exist at frame 1000 with the floor tag TO STORAGE, the ghost tiles at c32 r8 sample lighter than the surrounding white (all channels at least 8 higher), and the leaning WOOD TOOLS tile projects at y 0.3.
10. Scripted LATE night: frame 1200 samples the white tile at (0, 0) below 120 in every channel (dark), the pendant bulbs above 240 in R and G, the laser lamp pool present (R above 150 at c14 r4); the clock reads between 10:00 PM and 11:00 PM; no newbie spawns after frame 90 + 20 x 60 of the night; the sleeper lies on the couch (grey body sample at the projected couch point at y 0.55).
11. Level switch soak: a script that picks level 0, 3, 0, 3 ten times without starting (lobby only) and then starts a CLASSIC night: no generated_missing line, resident memory after the tenth switch within 2 percent of after the first (printed by the host), and check 6 passes on the final night.
12. Smoke test: tools/smoke.c runs level_check on all five levels and returns non zero on any finding; CI fails if it does.
13. Text lint: the lint script from the humor map runs over levels.c, content.h, game.c strings and this document: no em or en dashes, no emoji, ASCII caps in game text, every toast under 64 characters, banners under 48, awards under 64 after formatting.

## 14. Risks

1. The white floor clipping. Albedo 0.85 under sun 2.3 plus hemisphere 1.4 plus four pools is at the edge; OPEN HOUSE's sun 3.0 is the first place it will show. Mitigation: check 3 runs before any tile colour changes; the fallback order is P0 to 9, then sun to 2.0, never a darker tile than 0xd9d3c7.
2. Twenty three stations in find_target and bots. More WANT candidates means more bot wandering between the back and the front. Mitigation: bases are tuned low for the janitorial stations (bathroom 1.1, notice 0.6, shelve 0.3, wipe 0.4 + 0.15 per line) and the FOUNDER's glamour targets are three (laser, forge, printers); check 6 counts visits.
3. Narrow corridors at 0.80. Eleven pairs sit between 0.80 and 0.95 (11.4); a player pushed by a soft player push (game.c 722-726) can graze them. They are all wall side corridors no bot targets; the wedge detector in check 6 is the guard. Do not shrink any of them.
4. Visual only vestibule posts and props. The second post pair at z 6.5 and the free standing props have no collision; a player can walk through a shop vac. Accepted for props placed in closed pockets; if a shot shows a bot standing in the tool cart, give PR_TOOLCART an OBK entry.
5. world_sync child order. The whiteboard's line meshes, the storage boxes and the sink plates are found by child index or material identity; any later model edit must keep those children first. The header comment in build_stations says so.
6. Rebuild leaks. The owned[] list captures the six known leaks; new builders must use keep_mat or the ten switch soak (check 11) will show growth. The soak is in the acceptance list for that reason.
7. The event re-roll and absent stations. A level that sets a non zero weight on a row whose station is absent would fire a no-op; the table in 6.8 keeps those at 0 and the case still guards on present, so the worst case is a wasted event slot.
8. Text budget. Several new toasts run 50 to 60 characters; F_MONO fits 63 in the 1152 px rows, but a label like TRASH (ALLEY, 225 FT) is 21 characters and station_labels must measure, not assume. The label budget keeps most names off screen anyway.
9. Camera and the facade sign. The header beam at y 3.2 and the sign at y 3.0 sit at the top of the frame from the night camera and could clip the top HUD row. If the frame 600 shot shows it under the meters, lower the sign to y 2.95 and keep the beam.
10. Pi performance. More instanced boxes are cheap, but 26 sign draws and the bigger floor texture add a little. The Pi fallbacks (1024 shadow map, PCF plain, P1 and P3 off) are in 4.3 and do not change states.

## 15. Facts an implementer must not get wrong

1. HUD y 0 is the back wall on the floor texture and the ceiling on the wall textures; fx_ and fz_ encode it; never flip them. FW/FH become 1230 x 715; nothing else may read them.
2. The tile grid origin is (-12.3, -7.15), pitch 0.6, 41 x 24; accents are whole tiles; the white tile is 0xd9d3c7 or darker or the floor clips.
3. Light counts stay 1 hemisphere, 1 directional with shadow, 4 point: l1401+s100. Levels move, retint and dim; never add. No fog, no spot, no transparent glass, no double sided curtain (it is a box), no map on an instanced standard material, no emissive map.
4. Amber C_HAZARD is a fill for attention: tape, frames, the wall band, the laser mat border, padlocks. Never a tile. The mark's orange 0xf99a1c is reserved and the mark is drawn only by hud_logo at its official colours, in two places: the back wall lockup and the facade sign. The darkroom wall is plain orange.
5. Every gap between solids is at least 0.76 on one axis with no diagonal pinch, or closed (touching, or filled by a third box). Run the checker on every edit; the five levels pass today.
6. Absent stations are built and parked at y -20 with present false; world_sync dereferences the laser, printers, sink, fridge and show pointers every frame without null checks.
7. The ten new station ids go after ST_TABLE; nothing between ST_PRINT1 and ST_PRINT2. ST_SWITCH and ST_BACKDOOR live inside the wall planes and are skipped by the pair test and by collide.
8. Timers in the right block: event, newbie, dish, donation, storage, book and box timers in the rush scaled block; forge heat, bathroom dirtiness, gusts, labels, purge, lights, read cooldowns, landlord, meters and end checks on real time.
9. fire_event re-rolls on a repeat; it never bumps to a neighbour. E_NONE rows stay at or below a quarter of each level's total.
10. The laser uses only S_IDLE, S_LOADED and S_BROKEN; cutting is G.laser_cutting. The sink gains S_BROKEN for the drain. The back door and the vending machine use S_BROKEN for open and jammed.
11. The three destination tables (find_target holding, player_update arrow, choose_goal holding) and npc_update must agree, including IT_MOVEBOX to STORAGE; newbies walk to approach_point, never to x - 1.1.
12. Holder values at or above 100 are newbies; only holder below MAX_PLAYERS is a player.
13. Signs are basic+map, unlit, no shadow; station tiles are signs; the HUD station name draws only on alarm, ready, proximity or always_label.
14. Station labels are level data and the base level uses the public tool names; coach, how_to and the E_STRANGE banner read S[].label; the README says the joke names are retired.
15. Strings: ASCII caps in game text (lowercase only for the hack letters on the east wall), no em or en dashes, no emoji, no bold, roles only, nothing from private governance; safety lines (laser, gas, hot work, certification) carry no punchline; toasts 63 characters, banners 47, awards 63 after formatting, one %d or %s exactly where the call site passes one.
