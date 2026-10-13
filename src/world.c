/* The 3D side: a low-poly HeatSync on a Tuesday night. 108 W Main, cut away
 * like a dollhouse so the shared screen sees the whole floor at once.
 *
 * Everything in the room is built from the active level L (src/levels.c):
 * the floor is painted from its tile accents, the walls from its bays, the
 * shell from its door, the furniture, props and signs from its tables, the
 * stations from S[]. world_set_level tears all of that down and builds it
 * again; the six lights and the camera are made once and only moved and
 * retinted, so a level switch never adds a render state. */
#include "game.h"
#include "hud.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

t3_camera *world_cam;
static t3_scene *SC;
static t3_light *hemi, *sun, *laser_glow, *kitchen_lamp, *bench_lamp, *hall_lamp;

/* shared geometries and materials: meshes that share both are drawn as one
 * instanced draw by three.c */
static t3_geometry *g_box, *g_cyl, *g_sphere, *g_knot, *g_disc, *g_cone;
static t3_material *m_plywood, *m_amber, *m_scrap, *m_bottle, *m_label, *m_spaghetti, *m_crt, *m_screen,
                   *m_cardboard, *m_sawdust, *m_puddle, *m_strip, *m_glow, *m_skin, *m_visor, *m_pcb, *m_led,
                   *m_char, *m_steel, *m_steel_hi, *m_bone, *m_rust, *m_violet, *m_info,
                   *m_plate, *m_wood,
                   /* section 4.4: the shared colours of the new room */
                   *m_green, *m_red, *m_orange, *m_blue, *m_cord, *m_bulb, *m_door, *m_white, *m_tvs, *m_grey,
                   *m_scr_g, *m_scr_b, *m_scr_a, *m_woodtop, *m_tube,
                   *m_ouch, *m_strange;   /* the two printer frames */

/* ── what a rebuild owns ───────────────────────────────────────────
 * Every root object a builder adds, every texture it paints and every
 * material it makes on the fly is listed here so world_set_level can drop
 * the whole room and build the next level without leaking. */
static t3_object *built[1024]; static int built_n;
static t3_texture *tex[64]; static int tex_n;
static t3_material *owned[96]; static int owned_n;
static t3_object *fans[8]; static int fan_n;
static t3_mesh *board_line[8], *board_corner[2];   /* the whiteboard's marker lines and its DO NOT ERASE corner */
static t3_mesh *storage_box[6], *storage_misc, *storage_tilt;   /* bankers boxes by count, the MISC label, the spilled one */
static t3_mesh *lib_book[4];                        /* books out, on the floor in front of the shelf */
static t3_object *door_hinge, *door_leaf;           /* the back door swings on its east jamb */
static t3_mesh *switch_toggle;
static t3_object *bike_hung;                        /* the flat, on the bike station's stand */
static t3_object *rack_bike[4]; static t3_material *rack_frame[4]; static int rack_n;   /* bikes in the rack, by npc.bike slot */
static t3_object *visitor[8]; static int visitor_n;

static void *keep(void *o) {
  if (o && built_n < (int)(sizeof built / sizeof built[0])) built[built_n++] = o;
  return o;
}
static t3_texture *keep_tex(t3_texture *t) {
  if (t && tex_n < (int)(sizeof tex / sizeof tex[0])) tex[tex_n++] = t3_retain(t);
  return t;
}
static t3_material *keep_mat(t3_material *m) {
  if (m && owned_n < (int)(sizeof owned / sizeof owned[0])) owned[owned_n++] = m;
  return m;
}

static t3_material *mat(uint32_t c, float rough, float metal) {
  t3_material *m = t3_mesh_standard_material_new(c);
  m->roughness = rough;
  m->metalness = metal;
  return m;
}

static t3_material *emis(uint32_t c, float k) {
  t3_material *m = mat(c, 0.4f, 0);
  m->emissive = t3_color_hex(c);
  m->emissive_intensity = k;
  return m;
}

static t3_mesh *part(void *parent, t3_geometry *g, t3_material *m, float sx, float sy, float sz, float x, float y, float z) {
  t3_mesh *me = t3_mesh_new(g, m);
  t3_object_set_scale(me, sx, sy, sz);
  t3_object_set_position(me, x, y, z);
  me->base.cast_shadow = true;
  me->base.receive_shadow = true;
  t3_object_add(parent, me);
  if (parent == &SC->base) keep(me);
  t3_release(me);
  return me;
}
#define BOX(p, m, w, h, d, x, y, z) part(p, g_box, m, w, h, d, x, y, z)
#define CYL(p, m, r, h, x, y, z) part(p, g_cyl, m, r, h, r, x, y, z)

static t3_mesh *rot(t3_mesh *m, float rx, float ry, float rz) { t3_object_set_rotation(m, rx, ry, rz); return m; }
static t3_mesh *noshadow(t3_mesh *m) { m->base.cast_shadow = false; return m; }

static t3_object *group_at(void *parent, float x, float z) {
  t3_object *g = t3_group_new();
  t3_object_set_position(g, x, 0, z);
  t3_object_add(parent, g);
  if (parent == &SC->base) keep(g);
  t3_release(g);
  return g;
}

/* a texture drawn with the HUD routines into the top-left of the buffer */
static t3_texture *hud_texture(int w, int h) {
  uint8_t *img = malloc((size_t)w * h * 4);
  for (int y = 0; y < h; y++) memcpy(img + (size_t)y * w * 4, hud_px + ((size_t)(HUD_H - 1 - y) * HUD_W) * 4, (size_t)w * 4);
  t3_texture *t = t3_texture_new(w, h, img);
  t->color_space = T3_SRGB_COLOR_SPACE;
  free(img);
  return keep_tex(t);
}

static t3_mesh *sign_on(void *parent, t3_texture *t, float w, float h, float x, float y, float z, float ry) {
  t3_material *m = t3_mesh_basic_material_new(0xffffff);
  t3_material_set_map(m, t);
  t3_mesh *me = t3_mesh_new(t3_plane_geometry_new(w, h, 1, 1), m);
  t3_object_set_position(me, x, y, z);
  t3_object_set_rotation(me, 0, ry, 0);
  t3_object_add(parent, me);
  if (parent == &SC->base) keep(me);
  t3_release(me); t3_release(m); t3_release(t);
  return me;
}
static t3_mesh *sign(t3_texture *t, float w, float h, float x, float y, float z, float ry) { return sign_on(&SC->base, t, w, h, x, y, z, ry); }

/* ── the floor ─────────────────────────────────────────────────────
 * One 0.6 unit tile grid, 41 columns by 24 rows, origin at the back left
 * corner. White VCT with whole tile rectangles of orange and red from the
 * level's accent list, concrete in the fenced shop, terrazzo in the
 * vestibule, grey mats under the thin machines. Painted into the HUD buffer
 * at 50 px per unit (30 px per tile), captured as a 1230 x 715 texture.
 * HUD y 0 is the back wall; fx_ and fz_ are never flipped. */

#define FW 1230
#define FH 715
#define TILE 0.6f
#define TCOLS 41
#define TROWS 24
#define TPX 30
static float fx_(float x) { return (x + 12.3f) / 24.6f * FW; }
static float fz_(float z) { return (z + 7.15f) / 14.3f * FH; }
static int tc_(float x) { return (int)floorf((x + 12.3f) / TILE); }
static int tr_(float z) { return (int)floorf((z + 7.15f) / TILE); }
static uint32_t lcg = 12345;
static int noise8(void) { lcg = lcg * 1664525u + 1013904223u; return (int)(lcg >> 24) - 128; }
static float fnoise(void) { return (noise8() + 128) / 256.f; }

static const uint32_t TCOL[] = { 0xd9d3c7, 0xd9672b, 0xb23a2e, 0, 0x68645f, 0x7d6e5e, 0xc8c2b6, 0xe2dcd0 };

static uint32_t shade(uint32_t c, int n) {
  int r = ((c >> 16) & 255) + n, g = ((c >> 8) & 255) + n, b = (c & 255) + n;
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void shade_px(int x, int y, int d) {
  if ((unsigned)x >= HUD_W || (unsigned)y >= HUD_H) return;
  uint8_t *p = hud_px + ((size_t)(HUD_H - 1 - y) * HUD_W + x) * 4;
  for (int k = 0; k < 3; k++) { int v = p[k] + d; p[k] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
}

static void floor_rect(float x0, float z0, float x1, float z1, uint32_t c, int a) {
  int px = (int)fx_(x0), pz = (int)fz_(z0);
  hud_rect(px, pz, (int)fx_(x1) - px, (int)fz_(z1) - pz, c, a);
}

/* diagonal hazard stripes around a footprint, the way shops mark a machine */
static void hazard_border(float x0, float z0, float x1, float z1, float w) {
  int X0 = (int)fx_(x0), Z0 = (int)fz_(z0), X1 = (int)fx_(x1), Z1 = (int)fz_(z1), W = (int)(w * FW / 24.6f);
  for (int y = Z0; y < Z1; y++) for (int x = X0; x < X1; x++) {
    bool edge = x < X0 + W || x >= X1 - W || y < Z0 + W || y >= Z1 - W;
    if (!edge) continue;
    hud_rect(x, y, 1, 1, ((x + y) / 7) & 1 ? C_HAZARD : C_TAPE, 230);
  }
}

static void stencil(float x, float z, const char *s, int font, uint32_t c, int a) {
  hud_text(font, (int)fx_(x), (int)fz_(z) - hud_font_h(font) / 2, s, c, a, AL_CENTER);
}

/* the tile map resolved from the level's accents */
static uint8_t tmap[TCOLS][TROWS];

static void resolve_tiles(const level_t *lv) {
  memset(tmap, T_WHITE, sizeof tmap);
  for (int i = 0; i < lv->accent_n; i++) {
    const accent_t *a = &lv->accents[i];
    for (int c = a->c; c < a->c + a->cw && c < TCOLS; c++)
      for (int r = a->r; r < a->r + a->rh && r < TROWS; r++) {
        bool edge = c == a->c || r == a->r || c == a->c + a->cw - 1 || r == a->r + a->rh - 1;
        if (a->ring && !edge) continue;
        tmap[c][r] = a->kind == T_CHECK ? (((c + r) & 1) ? T_ORANGE : T_RED) : a->kind;
      }
  }
}

/* distance from a world point to the nearest fence box, for the darkening along it */
static float fence_dist(const level_t *lv, float x, float z) {
  float best = 99;
  for (int i = 0; i < lv->ob_n; i++) {
    const obst_t *o = &lv->ob[i];
    if (o->kind != OBK_FENCE) continue;
    float dx = fabsf(x - o->x) - o->hw, dz = fabsf(z - o->z) - o->hd;
    if (dx < 0) dx = 0;
    if (dz < 0) dz = 0;
    float d = sqrtf(dx * dx + dz * dz);
    if (d < best) best = d;
  }
  return best;
}

/* sealed concrete: a slow mottle, grain, and a darker strip along the fence */
static void concrete_tile(int px, int py, int w, int h, const level_t *lv) {
  for (int y = py; y < py + h; y++) {
    uint8_t *row = hud_px + ((size_t)(HUD_H - 1 - y) * HUD_W) * 4;
    for (int x = px; x < px + w; x++) {
      float m = sinf(x * 0.013f) * sinf(y * 0.017f + 1.3f) * 6 + sinf(x * 0.041f + y * 0.023f) * 3;
      int g = (int)m + noise8() / 14;
      float wx = x / 50.f - 12.3f, wz = y / 50.f - 7.15f;
      if (fence_dist(lv, wx, wz) < 0.5f) g -= 18;
      row[x * 4 + 0] = (uint8_t)(104 + g); row[x * 4 + 1] = (uint8_t)(100 + g); row[x * 4 + 2] = (uint8_t)(95 + g); row[x * 4 + 3] = 255;
    }
  }
}

/* terrazzo: a warm field with light and dark chips */
static void terrazzo_tile(int px, int py, int w, int h) {
  hud_rect(px, py, w, h, 0x7d6e5e, 255);
  for (int y = py; y < py + h; y += 2) for (int x = px; x < px + w; x += 2) {
    int n = noise8() + 128;
    if (n % 9 == 0) hud_rect(x, y, 2, 2, 0xa89a86, 255);
    else if (n % 14 == 1) hud_rect(x, y, 2, 2, 0x4a3f36, 255);
  }
}

/* stains and saw joints, only inside the concrete rectangles */
static void shop_stains_and_joints(const level_t *lv) {
  for (int i = 0; i < lv->accent_n; i++) {
    const accent_t *a = &lv->accents[i];
    if (a->kind != T_GREY) continue;
    int X0 = a->c * TPX, Y0 = a->r * TPX, X1 = (a->c + a->cw) * TPX, Y1 = (a->r + a->rh) * TPX;
    for (int k = 0; k < 8; k++) {
      int x = X0 + (int)(fnoise() * (X1 - X0)), y = Y0 + (int)(fnoise() * (Y1 - Y0));
      hud_circle(x, y, 5 + (noise8() + 128) / 24, 0x2a2622, 30);
    }
    /* two saw cuts: along the c 32 column edge and the r 2 row edge */
    int jx = 32 * TPX, jy = 2 * TPX;
    if (jx > X0 && jx < X1) hud_rect(jx, Y0, 1, Y1 - Y0, 0x4a4640, 255);
    if (jy > Y0 && jy < Y1) hud_rect(X0, jy, X1 - X0, 1, 0x4a4640, 255);
  }
}

static void paint_overlay(const overlay_t *o) {
  switch (o->kind) {
    case OV_LASER_MAT:
      floor_rect(o->x0, o->z0, o->x1, o->z1, 0x24211e, 255);
      hazard_border(o->x0, o->z0, o->x1, o->z1, 0.22f);
      break;
    case OV_TAPE_LINE: hazard_border(o->x0, o->z0, o->x1, o->z1, 0.5f * (o->z1 - o->z0)); break;
    case OV_TAPE_RING: hazard_border(o->x0, o->z0, o->x1, o->z1, 0.18f); break;
    case OV_FRAME:
      hud_frame((int)fx_(o->x0), (int)fz_(o->z0), (int)(fx_(o->x1) - fx_(o->x0)), (int)(fz_(o->z1) - fz_(o->z0)), 4, C_HAZARD, 255);
      break;
    case OV_RUG:
      floor_rect(o->x0, o->z0, o->x1, o->z1, 0x4a2f5c, 255);
      hud_frame((int)fx_(o->x0 + 0.15f), (int)fz_(o->z0 + 0.15f), (int)(fx_(o->x1 - 0.15f) - fx_(o->x0 + 0.15f)),
                (int)(fz_(o->z1 - 0.15f) - fz_(o->z0 + 0.15f)), 3, C_BONE, 255);
      break;
  }
}

/* oil and coffee: the space has history. Hall only; never on the concrete or the terrazzo. */
static void stain_disc(uint32_t c, int a) {
  for (int tries = 0; tries < 8; tries++) {
    int x = (int)(fnoise() * FW), y = (int)(fnoise() * FH);
    int r = (noise8() + 128) / 20 + 6;
    int cc = x / TPX, rr = y / TPX;
    if (cc >= TCOLS || rr >= TROWS) continue;
    int k = tmap[cc][rr];
    if (k == T_GREY || k == T_TERRAZZO) continue;
    hud_circle(x, y, r, c, a);
    return;
  }
}

/* shadows pooling where the floor meets the left, right and back walls */
static void wall_pool(float band, int steps) {
  for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) {
    float wx = x / 50.f - 12.3f, wz = y / 50.f - 7.15f;
    float d = fminf(fminf(wx + 12.3f, 12.3f - wx), wz + 7.15f);
    if (d < band) shade_px(x, y, -(int)((band - d) / band * steps));
  }
}

/* HABOOB: a second wash fading over 2.5 units from the back door and the front door */
static void door_wash(const level_t *lv) {
  float bx = lv->st[ST_BACKDOOR].x, bz = -7.15f, fx = lv->door_x, fz = 7.15f;
  for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) {
    float wx = x / 50.f - 12.3f, wz = y / 50.f - 7.15f;
    float d = fminf(hypotf(wx - bx, wz - bz), hypotf(wx - fx, wz - fz));
    if (d < 2.5f) hud_rect(x, y, 1, 1, lv->light.wash_col, (int)(40 * (1 - d / 2.5f)));
  }
}

static t3_texture *paint_floor(const level_t *lv) {
  hud_clear();
  resolve_tiles(lv);
  lcg = 12345;                                                   /* the same floor every build */
  hud_rect(0, 0, FW, FH, lv->light.grout, 255);                  /* grout */
  for (int c = 0; c < TCOLS; c++) for (int r = 0; r < TROWS; r++) {
    int k = tmap[c][r], px = c * TPX + 2, py = r * TPX + 2;
    if (k == T_GREY) { concrete_tile(px, py, TPX - 2, TPX - 2, lv); continue; }
    if (k == T_TERRAZZO) { terrazzo_tile(px, py, TPX - 2, TPX - 2); continue; }
    hud_rect(px, py, TPX - 2, TPX - 2, shade(TCOL[k], noise8() / 25), 255);
    if (k == T_WHITE || k == T_GHOST || k == T_MAT) {
      /* four VCT speckles and, on one tile in eight, a diagonal wear streak */
      for (int d = 0; d < 4; d++) hud_rect(px + (noise8() & 27), py + (noise8() & 27), 1, 1, d < 2 ? 0xc6c0b4 : 0xe4ded2, 255);
      if (k == T_WHITE && (noise8() & 7) == 0) for (int s = 0; s < 24; s++) hud_rect(px + 1 + s, py + 1 + s, 2, 1, 0x7a7367, 12);
    }
  }
  shop_stains_and_joints(lv);
  for (int i = 0; i < lv->overlay_n; i++) paint_overlay(&lv->overlays[i]);
  /* chair wheel streaks in the coworking aisle */
  for (int k = 0; k < 14; k++) {
    float x = lv->streak_box.x0 + fnoise() * (lv->streak_box.x1 - lv->streak_box.x0);
    floor_rect(x, lv->streak_box.z0, x + 0.03f, lv->streak_box.z1, 0x7a7367, 18);
  }
  for (int k = 0; k < 8; k++) stain_disc(0x5a524a, 28);
  for (int i = 0; i < lv->stencil_n; i++) {
    const stencil_t *s = &lv->stencils[i];
    stencil(s->x, s->z, s->text, s->font, s->warn ? C_HAZARD_DP : s->on_grey ? C_BONE : C_STEEL, s->warn ? 220 : s->on_grey ? 150 : 120);
  }
  wall_pool(1.2f, 28);
  if (lv->light.wash_a) { hud_rect(0, 0, FW, FH, lv->light.wash_col, lv->light.wash_a); door_wash(lv); }
  return hud_texture(FW, FH);
}

/* ── the walls ─────────────────────────────────────────────────────
 * Grey plaster, a char kick plate, the amber soffit band, pegboard bays
 * behind the wall stations. 50 px per unit on every wall. HUD y 0 is the
 * ceiling edge, y h the floor line. Bays are given in units along the wall
 * as the plane shows them (left to right from inside the room). */
typedef struct { float a0, a1; } bay_t;
enum { WALL_BACK, WALL_WEST, WALL_EAST };

static t3_texture *paint_wall(int w, int h, float units_w, const bay_t *bays, int bay_n, int side) {
  hud_clear();
  float ppu = w / units_w;
#define hy(units) (h - (int)((units) * ppu))   /* wall height to HUD y */
  /* 1. plaster with a little grain per 4 x 4 block */
  for (int y = 0; y < h; y += 4) for (int x = 0; x < w; x += 4) hud_rect(x, y, 4, 4, shade(0x6b675f, noise8() / 32), 255);
  /* 5. the darkroom panel on the back wall: plain orange, no mark */
  if (side == WALL_BACK) hud_rect(0, hy(2.3f), (int)(2.4f * ppu), hy(0.12f) - hy(2.3f), 0xd9672b, 255);
  /* 4. pegboard bays */
  for (int i = 0; i < bay_n; i++) {
    int x0 = (int)(bays[i].a0 * ppu), x1 = (int)(bays[i].a1 * ppu), y0 = hy(2.3f), y1 = hy(0.95f);
    hud_rect(x0, y0, x1 - x0, y1 - y0, 0xd9d3c7, 255);
    for (int y = y0 + 4; y < y1 - 2; y += 6) for (int x = x0 + 4; x < x1 - 2; x += 6) hud_rect(x, y, 2, 2, 0x9c968a, 140);
    hud_frame(x0, y0, x1 - x0, y1 - y0, 3, C_STEEL, 255);
  }
  /* 6. the shop's electrical panel on the back wall */
  if (side == WALL_BACK) {
    int x0 = (int)((6.3f + 12.3f) * ppu), x1 = (int)((7.3f + 12.3f) * ppu);
    hud_rect(x0, hy(1.8f), x1 - x0, hy(0.9f) - hy(1.8f), 0x4a463f, 255);
    hud_frame(x0, hy(1.8f), x1 - x0, hy(0.9f) - hy(1.8f), 2, C_STEEL, 255);
  }
  /* 2. kick plate */
  hud_rect(0, hy(0.12f), w, h - hy(0.12f), C_CHAR, 255);
  /* 3. the amber soffit band, with the hack letters sitting on it on the east wall */
  float b0 = 2.3f, b1 = 2.55f;
  if (side == WALL_EAST) { b0 = 2.2f; b1 = 2.7f; }
  hud_rect(0, hy(b1), w, hy(b0) - hy(b1), C_HAZARD, 255);
  hud_rect(0, hy(b0), w, 2, C_HAZARD_DP, 255);
  if (side == WALL_EAST) hud_text(F_H2, (int)((4.0f + 7.15f) * ppu), hy(b1) - 2, "hack", C_BONE, 255, AL_CENTER);
  /* 7. acoustic panels above the band, skipping the bays */
  for (float a = 0.7f; a + 0.8f < units_w; a += 1.4f) {
    bool over = false;
    for (int i = 0; i < bay_n; i++) if (a < bays[i].a1 && a + 0.8f > bays[i].a0) over = true;
    if (over) continue;
    hud_rect((int)(a * ppu), hy(3.25f), (int)(0.8f * ppu), hy(2.75f) - hy(3.25f), 0x4a4338, 255);
  }
  /* 9. darker at the ceiling edge and again where the wall meets the floor */
  int ky = hy(0.12f);
  for (int y = 0; y < h; y++) {
    float t = (float)y / h;
    int d = 0;
    if (t < 0.15f) d = -(int)((0.15f - t) / 0.15f * 30);
    else if (y < ky && (ky - y) < 0.12f * h) d = -(int)((1.0f - (ky - y) / (0.12f * h)) * 24);
    if (d) for (int x = 0; x < w; x++) shade_px(x, y, d);
  }
#undef hy
  return hud_texture(w, h);
}

static t3_mesh *lit_plane(t3_texture *t, float w, float h, float x, float y, float z, float rx, float ry) {
  t3_material *m = t3_mesh_standard_material_new(0xffffff);
  m->roughness = 0.9f;
  t3_material_set_map(m, t);
  t3_mesh *me = t3_mesh_new(t3_plane_geometry_new(w, h, 1, 1), m);
  t3_object_set_position(me, x, y, z);
  t3_object_set_rotation(me, rx, ry, 0);
  me->base.receive_shadow = true;
  t3_object_add(&SC->base, me);
  keep(me);
  t3_release(me); t3_release(m); t3_release(t);
  return me;
}

/* pegboard bays behind the wall stations and the wood bay, in wall units */
static int wall_bays(const level_t *lv, int side, bay_t *out) {
  int n = 0;
  if (side == WALL_BACK) {
    const st_place_t *la = &lv->st[ST_LASER];
    if (la->z - la->hd < -6.5f) out[n++] = (bay_t){ la->x - la->hw + 12.3f, la->x + la->hw + 12.3f };
    for (int i = 0; i < lv->ob_n; i++) {
      const obst_t *o = &lv->ob[i];
      if (o->kind == OBK_WOODBAY && o->z - o->hd < -5.9f) out[n++] = (bay_t){ o->x - o->hw + 12.3f, o->x + o->hw + 12.3f };
    }
  } else if (side == WALL_WEST) {
    /* the west plane runs from the front (HUD x 0) to the back */
    static const int ids[] = { ST_SEWING, ST_FREEPILE };
    for (int k = 0; k < 2; k++) {
      const st_place_t *s = &lv->st[ids[k]];
      if (s->x - s->hw < -11.3f) out[n++] = (bay_t){ 7.15f - (s->z + s->hd), 7.15f - (s->z - s->hd) };
    }
  }
  return n;
}

/* ── the shell ─────────────────────────────────────────────────────
 * A downtown storefront: glass front with mullions and a centre door in a
 * recessed vestibule, a header beam the facade sign hangs on, a sidewalk
 * slab, the back hallway recess, the laser's duct. The fence is furniture
 * (OBK_FENCE) and the back door is a station. */
static void build_room(const level_t *lv) {
  bay_t bays[6];
  int n;
  lit_plane(paint_floor(lv), 24.6f, 14.3f, 0, 0, 0, -1.5707963f, 0);
  /* walls: steel slabs with the painted plaster just inside them */
  BOX(&SC->base, m_steel, 24.6f, 3.4f, 0.3f, 0, 1.7f, -7.15f);
  BOX(&SC->base, m_steel, 0.3f, 3.4f, 14.3f, -12.15f, 1.7f, 0);
  BOX(&SC->base, m_steel, 0.3f, 3.4f, 14.3f, 12.15f, 1.7f, 0);
  n = wall_bays(lv, WALL_BACK, bays);
  lit_plane(paint_wall(1230, 170, 24.6f, bays, n, WALL_BACK), 24.6f, 3.4f, 0, 1.7f, -6.995f, 0, 0);
  n = wall_bays(lv, WALL_WEST, bays);
  lit_plane(paint_wall(715, 170, 14.3f, bays, n, WALL_WEST), 14.3f, 3.4f, -11.995f, 1.7f, 0, 0, 1.5707963f);
  n = wall_bays(lv, WALL_EAST, bays);
  lit_plane(paint_wall(715, 170, 14.3f, bays, n, WALL_EAST), 14.3f, 3.4f, 11.995f, 1.7f, 0, 0, -1.5707963f);
  /* top rails along the back and both sides: the cutaway's clean edge */
  BOX(&SC->base, m_steel_hi, 24.6f, 0.12f, 0.36f, 0, 3.4f, -7.15f);
  BOX(&SC->base, m_steel_hi, 0.36f, 0.12f, 14.3f, -12.15f, 3.4f, 0);
  BOX(&SC->base, m_steel_hi, 0.36f, 0.12f, 14.3f, 12.15f, 3.4f, 0);

  /* the storefront: kick wall stubs with an amber pinstripe, broken at the door */
  float dx = lv->door_x, dh = lv->door_hw;
  float wl = (dx - dh) - (-12.0f), wr = 12.0f - (dx + dh);
  BOX(&SC->base, m_steel, wl, 0.5f, 0.25f, -12.0f + wl / 2, 0.25f, 7.1f);
  BOX(&SC->base, m_steel, wr, 0.5f, 0.25f, 12.0f - wr / 2, 0.25f, 7.1f);
  BOX(&SC->base, m_amber, wl, 0.02f, 0.25f, -12.0f + wl / 2, 0.51f, 7.1f);
  BOX(&SC->base, m_amber, wr, 0.02f, 0.25f, 12.0f - wr / 2, 0.51f, 7.1f);
  /* door frame, vestibule posts and soffit, two glass leaves (frames only, the glass is implied) */
  for (int s = -1; s <= 1; s += 2) {
    BOX(&SC->base, m_steel_hi, 0.1f, 2.6f, 0.1f, dx + s * dh, 1.3f, 7.1f);
    BOX(&SC->base, m_steel_hi, 0.08f, 2.6f, 0.08f, dx + s * (dh + 0.06f), 1.3f, 6.5f);
    float lx = dx + s * dh * 0.5f;
    BOX(&SC->base, m_steel_hi, 0.06f, 2.4f, 0.05f, lx - s * (dh * 0.5f - 0.03f), 1.2f, 7.1f);
    BOX(&SC->base, m_steel_hi, 0.06f, 2.4f, 0.05f, lx + s * (dh * 0.5f - 0.03f), 1.2f, 7.1f);
    BOX(&SC->base, m_steel_hi, dh, 0.06f, 0.05f, lx, 2.37f, 7.1f);
    BOX(&SC->base, m_steel_hi, dh, 0.06f, 0.05f, lx, 0.03f, 7.1f);
    BOX(&SC->base, m_steel_hi, dh - 0.2f, 0.05f, 0.05f, lx, 1.05f, 7.08f);   /* the push bar */
  }
  BOX(&SC->base, m_steel_hi, dh * 2 + 0.2f, 0.12f, 0.12f, dx, 2.6f, 7.1f);
  BOX(&SC->base, m_steel, dh * 2 + 0.2f, 0.15f, 0.7f, dx, 2.7f, 6.8f);
  /* mullions every 2.2 units with a sill and a header over each window span */
  for (float x = -11.0f; x <= 11.01f; x += 2.2f) {
    if (x > dx - dh - 0.5f && x < dx + dh + 0.5f) continue;
    BOX(&SC->base, m_steel_hi, 0.08f, 2.8f, 0.08f, x, 1.4f, 7.12f);
  }
  BOX(&SC->base, m_steel_hi, wl, 0.1f, 0.1f, -12.0f + wl / 2, 0.55f, 7.12f);
  BOX(&SC->base, m_steel_hi, wr, 0.1f, 0.1f, 12.0f - wr / 2, 0.55f, 7.12f);
  BOX(&SC->base, m_steel_hi, wl, 0.14f, 0.1f, -12.0f + wl / 2, 2.8f, 7.12f);
  BOX(&SC->base, m_steel_hi, wr, 0.14f, 0.1f, 12.0f - wr / 2, 2.8f, 7.12f);
  /* the header beam, broken over the door; the facade sign hangs on it */
  float bl = (dx - dh - 0.2f) - (-12.3f), br = 12.3f - (dx + dh + 0.2f);
  BOX(&SC->base, m_steel, bl, 0.3f, 0.4f, -12.3f + bl / 2, 3.2f, 7.25f);
  BOX(&SC->base, m_steel, br, 0.3f, 0.4f, 12.3f - br / 2, 3.2f, 7.25f);
  /* the sidewalk, so the walk out to the street has something underfoot */
  BOX(&SC->base, m_steel_hi, 2.6f, 0.04f, 1.6f, dx, -0.02f, 7.9f);

  /* the back hallway recess behind the trash: jambs, a header and a dark mouth */
  const st_place_t *tr = &lv->st[ST_TRASH];
  if (tr->z - tr->hd < -6.9f) {
    BOX(&SC->base, m_steel_hi, 0.1f, 2.4f, 0.1f, tr->x - tr->hw, 1.2f, -6.9f);
    BOX(&SC->base, m_steel_hi, 0.1f, 2.4f, 0.1f, tr->x + tr->hw, 1.2f, -6.9f);
    BOX(&SC->base, m_steel_hi, tr->hw * 2 + 0.1f, 0.1f, 0.1f, tr->x, 2.4f, -6.9f);
    t3_material *grime = keep_mat(mat(C_GRIME, 1.0f, 0));
    BOX(&SC->base, grime, tr->hw * 2, 2.4f, 0.25f, tr->x, 1.2f, -6.87f);
  }
  /* the laser's exhaust duct, up to the roof */
  const st_place_t *la = &lv->st[ST_LASER];
  CYL(&SC->base, m_steel_hi, 0.26f, 1.9f, la->x - 2.2f, 2.45f, -6.5f);
  BOX(&SC->base, m_steel, 0.5f, 0.5f, 0.3f, la->x - 2.2f, 1.35f, -6.85f);
}

/* ── the stations ──────────────────────────────────────────────────
 * Each group stands at S[i] and turns by S[i].ry; an absent station is built
 * anyway and parked at y -20 so world_sync keeps its pointers. */
static t3_object *station_group(station_t *s) {
  t3_object *g = s->obj = group_at(&SC->base, s->x, s->z);
  t3_object_set_rotation(g, 0, s->ry, 0);
  if (!s->present) t3_object_set_position(g, s->x, -20, s->z);
  return g;
}

static void grey_chair(t3_object *g, float x, float z, float ry);

/* a bicycle along local x: two wheels 0.9 apart, a frame, a saddle and bars */
static t3_object *bike_model(t3_object *parent, float x, float y, float z, float ry, t3_material *frame) {
  t3_object *b = t3_group_new();
  t3_object_set_position(b, x, y, z);
  t3_object_set_rotation(b, 0, ry, 0);
  t3_object_add(parent, b);
  t3_release(b);
  for (int k = -1; k <= 1; k += 2) {
    rot(noshadow(part(b, g_disc, m_char, 0.3f, 0.05f, 0.3f, k * 0.45f, 0.3f, 0)), 1.5707963f, 0, 0);
    rot(noshadow(part(b, g_disc, m_steel_hi, 0.06f, 0.035f, 0.06f, k * 0.45f, 0.3f, 0)), 1.5707963f, 0, 0);
  }
  BOX(b, frame, 0.6f, 0.05f, 0.05f, 0.02f, 0.62f, 0);                                     /* top tube */
  rot(BOX(b, frame, 0.6f, 0.05f, 0.05f, 0.05f, 0.45f, 0), 0, 0, 0.55f);                   /* down tube */
  BOX(b, frame, 0.05f, 0.45f, 0.05f, -0.2f, 0.5f, 0);                                     /* seat tube */
  rot(BOX(b, frame, 0.45f, 0.05f, 0.05f, -0.3f, 0.32f, 0), 0, 0, -0.1f);                 /* chain stay */
  BOX(b, m_char, 0.2f, 0.04f, 0.08f, -0.22f, 0.75f, 0);                                   /* saddle */
  BOX(b, frame, 0.05f, 0.4f, 0.05f, 0.38f, 0.5f, 0);                                      /* fork */
  BOX(b, m_char, 0.03f, 0.03f, 0.42f, 0.38f, 0.74f, 0);                                   /* bars */
  return b;
}

static t3_texture *plaque(int w, int h, uint32_t bg, uint32_t fg, int font, const char *text);

/* Silhouettes first (FINAL-space.md section 10): one dominant colour, one
 * tall element, fewer small parts. Every piece is a shared geometry in a
 * shared material so the meshes keep instancing. world_sync finds the sink's
 * plates by the m_plate material and the printers' growth, lamp and OUT OF
 * ORDER tag by pointer; keep those when a model changes. */
static void build_stations(void) {
  station_t *s;
  t3_object *g;

  /* scrap bays: four open bays, green, yellow, orange, red, with plywood leaning in the first three */
  s = &S[ST_LUMBER]; g = station_group(s);
  {
    t3_material *bay[4] = { m_green, m_cord, m_orange, m_red };
    BOX(g, m_steel, 1.76f, 0.06f, 0.9f, 0, 0.03f, 0);                                   /* base */
    BOX(g, m_steel, 1.76f, 1.3f, 0.04f, 0, 0.65f, -0.43f);                              /* back panel */
    for (int k = 0; k < 5; k++) BOX(g, m_steel, 0.04f, 1.2f, 0.9f, -0.84f + k * 0.42f, 0.6f, 0);   /* dividers */
    for (int b = 0; b < 4; b++) {
      float bx = -0.63f + b * 0.42f;
      BOX(g, bay[b], 0.38f, 0.1f, 0.9f, bx, 1.25f, 0);                                  /* the coloured header */
      BOX(g, bay[b], 0.38f, 0.14f, 0.06f, bx, 0.13f, 0.42f);                            /* and front lip */
      if (b < 3) rot(BOX(g, m_plywood, 0.02f, 1.1f, 0.8f, bx, 0.6f, 0), 0, 0, -0.13f);  /* a sheet leaning in */
    }
    BOX(g, m_cardboard, 0.3f, 0.2f, 0.2f, 0.63f, 0.16f, 0.1f);                          /* the offcut box in bay 4 */
    BOX(g, m_red, 0.1f, 0.14f, 0.01f, 0.63f, 0.2f, 0.21f);                              /* with its red tag */
  }

  /* the big red laser: body, amber lid, glass, panel, e-stop, chiller */
  s = &S[ST_LASER]; g = station_group(s);
  BOX(g, m_char, 2.8f, 0.12f, 1.6f, 0, 0.06f, 0);                                       /* skirt */
  BOX(g, m_red, 2.8f, 0.9f, 1.6f, 0, 0.45f, 0);                                         /* body */
  BOX(g, m_steel_hi, 2.8f, 0.05f, 1.6f, 0, 0.925f, 0);                                  /* rim */
  BOX(g, m_amber, 2.3f, 0.1f, 1.3f, -0.25f, 1.02f, 0);                                  /* lid */
  BOX(g, m_screen, 1.9f, 0.04f, 1.0f, -0.25f, 1.08f, 0);                                /* glass */
  BOX(g, m_char, 0.5f, 0.3f, 0.25f, 1.15f, 1.05f, 0.5f);                                /* control panel */
  noshadow(BOX(g, m_scr_a, 0.3f, 0.18f, 0.02f, 1.15f, 1.07f, 0.63f));                   /* its screen, toward the aisle */
  CYL(g, m_red, 0.06f, 0.05f, 1.15f, 1.225f, 0.3f);                                     /* e-stop */
  BOX(g, m_steel, 0.5f, 0.5f, 0.5f, -1.8f, 0.25f, 0.2f);                                /* the chiller */
  s->beam = BOX(g, m_glow, 0.04f, 0.5f, 0.04f, 0, 1.2f, 0);
  s->beam->base.visible = false;
  s->light = BOX(g, keep_mat(mat(0x5fb892, 0.4f, 0)), 0.14f, 0.14f, 0.05f, 1.15f, 0.7f, 0.81f);

  /* the printers stand on the printing counter (an OBK_COUNTER obstacle); each wears an OUT OF ORDER tag while broken */
  {
    t3_material *frame[2] = { m_ouch, m_strange };
    t3_material *spool[2] = { m_pcb, m_amber };
    for (int k = 0; k < 2; k++) {
      s = &S[ST_PRINT1 + k]; g = station_group(s);
      BOX(g, m_char, 0.8f, 0.1f, 0.6f, 0, 0.86f, 0);                                    /* base */
      BOX(g, m_woodtop, 0.6f, 0.03f, 0.5f, 0, 0.93f, 0.02f);                            /* the bed */
      BOX(g, frame[k], 0.06f, 0.75f, 0.06f, -0.37f, 1.28f, -0.18f);
      BOX(g, frame[k], 0.06f, 0.75f, 0.06f, 0.37f, 1.28f, -0.18f);
      BOX(g, frame[k], 0.8f, 0.06f, 0.06f, 0, 1.65f, -0.18f);
      BOX(g, m_steel_hi, 0.76f, 0.04f, 0.04f, 0, 1.25f, -0.12f);                        /* x gantry */
      BOX(g, m_char, 0.14f, 0.16f, 0.14f, 0.08f, 1.2f, -0.06f);                         /* extruder */
      rot(part(g, g_cyl, spool[k], 0.18f, 0.08f, 0.18f, 0, 1.82f, -0.18f), 1.5707963f, 0, 0);
      noshadow(BOX(g, k ? m_scr_b : m_scr_g, 0.16f, 0.08f, 0.02f, 0.25f, 0.88f, 0.31f)); /* LCD */
      s->growth = part(g, g_knot, m_amber, 0.14f, 0.14f, 0.14f, 0, 1.03f, 0.02f);
      s->growth->base.visible = false;
      s->light = BOX(g, keep_mat(mat(0x5fb892, 0.4f, 0)), 0.06f, 0.06f, 0.03f, -0.28f, 0.88f, 0.31f);
      s->beam = sign_on(g, plaque(96, 56, C_BONE, C_CHAR, F_MONO_S, "OUT OF\nORDER"), 0.24f, 0.14f, 0.12f, 1.08f, 0.34f, 0);
      s->beam->base.visible = false;
    }
  }

  /* the vinyl cutter nobody fights over, on the craft counter (the heat press and the Cameo are the counter's) */
  s = &S[ST_CUTSALOT]; g = station_group(s);
  BOX(g, m_red, 1.2f, 0.3f, 0.5f, 0, 0.9f, 0);
  rot(part(g, g_cyl, m_violet, 0.07f, 1.0f, 0.07f, 0, 1.05f, 0.2f), 0, 0, 1.5707963f);
  BOX(g, m_char, 0.1f, 0.12f, 0.12f, 0.2f, 1.1f, 0.05f);
  noshadow(BOX(g, m_scr_b, 0.14f, 0.06f, 0.02f, -0.45f, 0.98f, 0.26f));

  /* the electronics bench, long axis local x (the group turns by ry so that is world z):
   * white top on an ESD mat, two monitors facing the aisle, a scope, the iron, one drawer
   * unit, the instrument rack against the wall and a red lift table at the back end */
  s = &S[ST_BENCH]; g = station_group(s);
  BOX(g, m_white, 3.2f, 0.08f, 1.3f, -0.3f, 0.86f, 0);
  BOX(g, m_info, 2.9f, 0.01f, 1.0f, -0.3f, 0.905f, 0);                                   /* ESD mat */
  for (int i = 0; i < 4; i++) BOX(g, m_steel_hi, 0.08f, 0.86f, 0.08f, i < 2 ? -1.8f : 1.2f, 0.43f, i & 1 ? -0.55f : 0.55f);
  for (int k = 0; k < 2; k++) {
    float mx = -1.3f + k * 0.8f;
    BOX(g, m_char, 0.08f, 0.22f, 0.08f, mx, 1.0f, -0.4f);
    BOX(g, m_char, 0.5f, 0.32f, 0.04f, mx, 1.27f, -0.4f);
    noshadow(BOX(g, m_tvs, 0.42f, 0.26f, 0.01f, mx, 1.27f, -0.375f));
  }
  BOX(g, m_info, 0.5f, 0.32f, 0.38f, 0.4f, 1.06f, -0.3f);                                /* the scope */
  noshadow(BOX(g, m_scr_g, 0.3f, 0.2f, 0.01f, 0.35f, 1.08f, -0.105f));
  BOX(g, m_rust, 0.3f, 0.12f, 0.25f, 0.95f, 0.96f, 0.1f);                                /* the iron's stand */
  CYL(g, m_steel_hi, 0.03f, 0.2f, 1.0f, 1.1f, 0.1f);
  BOX(g, m_pcb, 0.5f, 0.03f, 0.35f, -0.6f, 0.92f, 0.2f);
  for (int i = 0; i < 3; i++) noshadow(part(g, g_sphere, m_led, 0.05f, 0.05f, 0.05f, -0.2f + i * 0.14f, 0.93f, 0.35f));
  BOX(g, m_info, 1.2f, 0.5f, 0.6f, -1.1f, 0.25f, 0.1f);                                  /* the drawer unit */
  for (int r = 0; r < 3; r++) BOX(g, m_char, 1.1f, 0.02f, 0.01f, -1.1f, 0.12f + r * 0.13f, -0.2f);
  BOX(g, m_steel_hi, 3.6f, 1.4f, 0.3f, -0.1f, 1.6f, 0.5f);                               /* the rack on the wall side */
  for (int k = 0; k < 4; k++) {
    float rx = -1.5f + k * 0.95f;
    BOX(g, m_steel, 0.7f, 0.22f, 0.04f, rx, 1.2f + (k & 1) * 0.55f, 0.33f);              /* an instrument */
    noshadow(part(g, g_sphere, m_led, 0.025f, 0.025f, 0.025f, rx + 0.25f, 1.2f + (k & 1) * 0.55f, 0.36f));
  }
  BOX(g, m_red, 0.6f, 0.5f, 0.5f, 1.6f, 0.25f, 0);                                       /* the lift table */
  rot(BOX(g, m_steel_hi, 0.04f, 0.7f, 0.04f, 1.1f, 1.25f, -0.4f), 0, 0, 0.3f);            /* desk lamp */
  noshadow(BOX(g, m_scr_a, 0.2f, 0.08f, 0.14f, 0.95f, 1.58f, -0.3f));

  /* the kitchen: sink cabinet, faucet, dishes, and the wire rack with the bottles and the MSDS binder */
  s = &S[ST_SINK]; g = station_group(s);
  BOX(g, m_bone, 1.1f, 0.85f, 1.2f, 0.05f, 0.425f, -0.3f);
  BOX(g, m_white, 1.2f, 0.06f, 1.3f, 0.05f, 0.88f, -0.3f);
  BOX(g, m_steel_hi, 0.7f, 0.04f, 0.8f, 0.1f, 0.9f, -0.4f);
  BOX(g, m_char, 0.6f, 0.03f, 0.7f, 0.1f, 0.915f, -0.4f);
  CYL(g, m_steel_hi, 0.03f, 0.35f, -0.25f, 1.08f, -0.4f);
  BOX(g, m_steel_hi, 0.25f, 0.04f, 0.04f, -0.13f, 1.25f, -0.4f);
  for (int k = 0; k < 2; k++) BOX(g, m_steel, 0.01f, 0.7f, 0.55f, 0.61f, 0.42f, -0.6f + k * 0.6f);   /* cabinet doors */
  for (int i = 0; i < 10; i++) {
    t3_mesh *pl = part(g, g_cyl, m_plate, 0.26f, 0.035f, 0.26f, 0.1f + (i & 1) * 0.03f, 0.98f + i * 0.05f, 0.1f + (i % 3) * 0.04f);
    pl->base.visible = false;
  }
  for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.03f, 1.5f, 0.03f, k & 1 ? -0.06f : -0.54f, 0.75f, k < 2 ? 0.47f : 0.83f);   /* the wire rack */
  for (int k = 0; k < 3; k++) BOX(g, m_steel_hi, 0.5f, 0.02f, 0.4f, -0.3f, 0.5f + k * 0.5f, 0.65f);
  {
    t3_material *bot[3] = { m_info, m_bone, m_amber };
    for (int k = 0; k < 3; k++) CYL(g, bot[k], 0.05f, 0.22f, -0.42f + k * 0.13f, 1.62f, 0.65f);
  }
  BOX(g, m_bone, 0.25f, 0.3f, 0.06f, -0.3f, 1.16f, 0.72f);                               /* the MSDS binder */

  /* the snack fridge: a dark box with a glass door on the aisle side and the snack box on top */
  s = &S[ST_FRIDGE]; g = station_group(s);
  BOX(g, m_char, 1.0f, 1.9f, 0.95f, 0, 0.95f, 0);
  noshadow(BOX(g, m_screen, 0.02f, 1.4f, 0.7f, 0.505f, 1.0f, 0));
  BOX(g, m_steel_hi, 0.04f, 0.6f, 0.04f, 0.53f, 1.0f, 0.4f);                              /* handle */
  {
    t3_material *can[3] = { m_red, m_info, m_amber };
    for (int r = 0; r < 3; r++) {
      noshadow(BOX(g, m_bone, 0.04f, 0.02f, 0.66f, 0.515f, 0.5f + r * 0.42f, 0));         /* a shelf behind the glass */
      for (int k = 0; k < 4; k++) noshadow(CYL(g, can[(r + k) % 3], 0.04f, 0.12f, 0.5f, 0.57f + r * 0.42f, -0.24f + k * 0.16f));
    }
  }
  BOX(g, m_cardboard, 0.5f, 0.3f, 0.4f, 0, 2.05f, 0);                                     /* the snack box */
  s->light = part(g, g_sphere, keep_mat(mat(0x5fb892, 0.5f, 0)), 0.0f, 0.0f, 0.0f, 0, 2.45f, 0);

  /* trash: the wheelbarrow and two bins in the hallway recess */
  s = &S[ST_TRASH]; g = station_group(s);
  rot(BOX(g, m_steel, 0.9f, 0.3f, 0.6f, 0, 0.45f, 0.1f), -0.15f, 0, 0);
  for (int k = -1; k <= 1; k += 2) rot(CYL(g, m_wood, 0.025f, 0.9f, k * 0.32f, 0.5f, -0.3f), 1.2f, 0, 0);
  rot(part(g, g_disc, m_char, 0.2f, 0.06f, 0.2f, 0, 0.2f, 0.45f), 0, 0, 1.5707963f);
  CYL(g, m_char, 0.25f, 0.7f, -0.55f, 0.35f, -0.25f);
  CYL(g, m_info, 0.25f, 0.7f, 0.55f, 0.35f, -0.25f);

  /* the free stuff shelf: three boards on posts against the wall, tear apart bins in front, a FREE sign */
  s = &S[ST_FREEPILE]; g = station_group(s);
  for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.04f, 1.6f, 0.04f, k & 1 ? -0.1f : -0.5f, 0.8f, k < 2 ? -0.88f : 0.88f);
  for (int lvl = 0; lvl < 3; lvl++) BOX(g, m_steel_hi, 0.4f, 0.04f, 1.8f, -0.3f, 0.3f + lvl * 0.5f, 0);
  BOX(g, m_crt, 0.4f, 0.36f, 0.42f, -0.3f, 0.5f, -0.5f);
  noshadow(BOX(g, m_screen, 0.01f, 0.3f, 0.3f, -0.095f, 0.5f, -0.5f));
  BOX(g, m_cardboard, 0.35f, 0.3f, 0.5f, -0.3f, 0.47f, 0.35f);
  BOX(g, m_cardboard, 0.35f, 0.25f, 0.45f, -0.3f, 0.945f, -0.4f);
  part(g, g_knot, m_char, 0.1f, 0.05f, 0.1f, -0.3f, 0.92f, 0.3f);                        /* a tangle of cables */
  BOX(g, m_steel, 0.5f, 0.35f, 0.4f, 0.25f, 0.175f, -0.45f);                             /* tear apart bins */
  BOX(g, m_steel, 0.5f, 0.35f, 0.4f, 0.25f, 0.175f, 0.45f);
  sign_on(g, plaque(128, 48, C_BONE, C_CHAR, F_H3, "FREE"), 0.5f, 0.19f, -0.3f, 1.45f, 0.91f, 0);

  /* the show window: the display shelf, three pedestals, the robot, the LED cube, the marquee,
   * the DONATE screens, a tombstone and the track lights; a low bone board so the camera still sees the shelf */
  s = &S[ST_SHOW]; g = station_group(s);
  BOX(g, m_char, 7.5f, 0.5f, 0.6f, 0, 0.25f, 0);
  BOX(g, m_bone, 7.5f, 0.45f, 0.05f, 0, 0.72f, 0.275f);
  for (int k = -1; k <= 1; k++) CYL(g, m_bone, 0.22f, 0.4f + (k == 0) * 0.2f, k * 1.4f, 0.74f + (k == 0) * 0.1f, -0.05f);
  part(g, g_cone, m_amber, 0.12f, 0.2f, 0.12f, -1.4f, 1.04f, -0.05f);                   /* the trophy */
  part(g, g_sphere, m_amber, 0.1f, 0.1f, 0.1f, -1.4f, 1.2f, -0.05f);
  BOX(g, m_steel, 0.25f, 0.4f, 0.2f, 2.2f, 0.7f, -0.05f);                                /* the toy robot */
  part(g, g_sphere, m_steel, 0.12f, 0.12f, 0.12f, 2.2f, 1.0f, -0.05f);
  noshadow(part(g, g_sphere, m_led, 0.03f, 0.03f, 0.03f, 2.2f, 1.02f, 0.07f));
  BOX(g, m_screen, 0.3f, 0.3f, 0.3f, 2.9f, 0.65f, 0);                                    /* the LED cube */
  for (int k = 0; k < 8; k++) noshadow(BOX(g, m_led, 0.04f, 0.04f, 0.04f, 2.9f + (k & 1 ? 0.15f : -0.15f), 0.65f + (k & 2 ? 0.15f : -0.15f), (k & 4 ? 0.15f : -0.15f)));
  BOX(g, m_char, 0.9f, 0.25f, 0.06f, -2.4f, 0.9f, 0.1f);                                 /* the marquee */
  noshadow(BOX(g, m_scr_a, 0.8f, 0.1f, 0.01f, -2.4f, 0.9f, 0.135f));
  noshadow(BOX(g, m_tvs, 0.3f, 0.3f, 0.03f, -3.2f, 0.7f, 0.1f));                          /* the DONATE screens */
  noshadow(BOX(g, m_tvs, 0.3f, 0.3f, 0.03f, 3.4f, 0.7f, 0.1f));
  BOX(g, m_steel, 0.3f, 0.4f, 0.08f, -0.7f, 0.7f, -0.1f);                                /* the tombstone */
  noshadow(BOX(g, m_steel_hi, 7.0f, 0.03f, 0.03f, 0, 2.7f, -0.1f));                       /* the track light rail */
  for (int k = -1; k <= 1; k++) noshadow(CYL(g, m_steel_hi, 0.05f, 0.12f, k * 2.4f, 2.62f, -0.1f));
  s->growth = part(g, g_box, m_amber, 0.42f, 0.42f, 0.42f, 0, 1.4f, -0.05f);
  s->growth->base.visible = false;

  /* the couch, four cushions and all, along the station's long axis (ry turns it to face the laser) */
  s = &S[ST_COUCH]; g = station_group(s);
  BOX(g, m_rust, 2.4f, 0.35f, 0.9f, 0, 0.2f, 0);
  for (int k = 0; k < 4; k++) BOX(g, k & 1 ? m_amber : m_rust, 0.55f, 0.2f, 0.75f, -0.87f + k * 0.58f, 0.45f, 0.05f);
  BOX(g, m_rust, 2.4f, 0.5f, 0.2f, 0, 0.6f, -0.35f);
  for (int k = -1; k <= 1; k += 2) BOX(g, m_bone, 0.2f, 0.55f, 0.9f, k * 1.1f, 0.45f, 0);
  rot(BOX(g, m_amber, 0.35f, 0.3f, 0.12f, -0.8f, 0.65f, -0.18f), 0.2f, 0.3f, 0);         /* a throw pillow */

  /* the HYH table: plywood on trestles, the QR signs, a bell jar, six chairs tucked in */
  s = &S[ST_TABLE]; g = station_group(s);
  BOX(g, m_plywood, 3.6f, 0.1f, 1.5f, 0, 0.9f, 0);
  for (int i = 0; i < 2; i++) {
    float tx = i ? 1.3f : -1.3f;
    rot(BOX(g, m_wood, 0.05f, 0.9f, 0.05f, tx, 0.45f, 0), 0.25f, 0, 0);
    rot(BOX(g, m_wood, 0.05f, 0.9f, 0.05f, tx, 0.45f, 0), -0.25f, 0, 0);
    BOX(g, m_wood, 0.05f, 0.05f, 0.6f, tx, 0.5f, 0);
  }
  BOX(g, m_bone, 0.25f, 0.25f, 0.02f, -1.2f, 1.08f, -0.5f);                              /* QR signs */
  BOX(g, m_char, 0.17f, 0.17f, 0.01f, -1.2f, 1.08f, -0.485f);
  BOX(g, m_bone, 0.25f, 0.25f, 0.02f, 1.2f, 1.08f, -0.5f);
  BOX(g, m_char, 0.17f, 0.17f, 0.01f, 1.2f, 1.08f, -0.485f);
  BOX(g, m_info, 0.4f, 0.3f, 0.3f, -0.9f, 0.15f, 0.2f);                                  /* a blue bin under it */
  part(g, g_sphere, m_screen, 0.12f, 0.12f, 0.12f, 0.4f, 1.07f, 0.2f);                   /* a bell jar */
  BOX(g, m_pcb, 0.4f, 0.03f, 0.3f, -0.3f, 0.97f, 0.3f);
  for (int k = 0; k < 3; k++) { grey_chair(g, -1.2f + k * 1.2f, 0.55f, 3.1415927f); grey_chair(g, -1.2f + k * 1.2f, -0.55f, 0); }

  /* the vending machine against the west wall: its lit front is the silhouette */
  s = &S[ST_VEND]; g = station_group(s);
  BOX(g, m_steel, 0.6f, 1.9f, 1.0f, 0, 0.95f, 0);
  s->light = noshadow(BOX(g, keep_mat(emis(0x8fbde6, 1.2f)), 0.02f, 1.2f, 0.7f, 0.31f, 1.15f, -0.05f));   /* front panel, +x */
  for (int k = 0; k < 6; k++)                                                            /* the drinks, two rows of three */
    noshadow(CYL(g, m_bottle, 0.05f, 0.18f, 0.37f, k < 3 ? 1.45f : 1.05f, -0.25f + (k % 3) * 0.2f));
  BOX(g, m_char, 0.02f, 0.1f, 0.06f, 0.31f, 1.05f, 0.4f);                                 /* coin slot */
  noshadow(CYL(g, m_screen, 0.08f, 0.12f, 0, 1.96f, 0.25f));                              /* the honour jar */
  noshadow(BOX(g, m_bone, 0.1f, 0.08f, 0.005f, 0, 1.97f, 0.25f));                         /* and the note in it */

  /* the rolling whiteboard: the eight marker lines come first after the frame
   * (children 4..11) so world_sync can show the first count of them */
  s = &S[ST_BOARD]; g = station_group(s);
  BOX(g, m_white, 1.3f, 0.9f, 0.04f, 0, 1.4f, 0);
  BOX(g, m_steel_hi, 0.04f, 1.9f, 0.04f, -0.6f, 0.95f, -0.04f);
  BOX(g, m_steel_hi, 0.04f, 1.9f, 0.04f, 0.6f, 0.95f, -0.04f);
  BOX(g, m_steel_hi, 1.3f, 0.04f, 0.3f, 0, 0.1f, -0.04f);                                 /* foot bar */
  for (int k = 0; k < 8; k++) {
    board_line[k] = noshadow(BOX(g, k & 1 ? m_rust : m_info, 0.9f - (k % 3) * 0.15f, 0.03f, 0.012f, -0.15f - (k % 3) * 0.075f, 1.72f - 0.1f * k, 0.026f));
    board_line[k]->base.visible = false;
  }
  board_corner[0] = noshadow(BOX(g, m_rust, 0.3f, 0.2f, 0.012f, 0.45f, 1.72f, 0.026f));  /* DO NOT ERASE */
  board_corner[1] = sign_on(g, plaque(64, 32, C_RUST, C_BONE, F_MONO_S, "DO NOT\nERASE"), 0.26f, 0.13f, 0.45f, 1.72f, 0.034f, 0);
  BOX(g, m_char, 1.36f, 0.04f, 0.06f, 0, 1.87f, 0);                                     /* a dark frame, so white reads on white tile */
  BOX(g, m_char, 1.36f, 0.04f, 0.06f, 0, 0.93f, 0);
  BOX(g, m_char, 0.04f, 0.94f, 0.06f, -0.66f, 1.4f, 0);
  BOX(g, m_char, 0.04f, 0.94f, 0.06f, 0.66f, 1.4f, 0);
  noshadow(BOX(g, m_char, 1.0f, 0.03f, 0.06f, 0, 0.9f, 0.05f));                          /* marker tray */
  for (int k = 0; k < 4; k++)
    rot(noshadow(part(g, g_disc, m_char, 0.05f, 0.03f, 0.05f, k & 1 ? 0.6f : -0.6f, 0.05f, k & 2 ? 0.1f : -0.18f)), 0, 0, 1.5707963f);

  /* sewing: a white table along the west wall, the green Consew with its needle lamp, a second machine, the thread rack */
  s = &S[ST_SEWING]; g = station_group(s);
  BOX(g, m_white, 0.8f, 0.05f, 1.4f, 0, 0.76f, 0);
  for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.04f, 0.74f, 0.04f, k & 1 ? 0.35f : -0.35f, 0.37f, k & 2 ? 0.65f : -0.65f);
  BOX(g, m_green, 0.25f, 0.35f, 0.5f, -0.05f, 0.96f, -0.3f);                             /* the Consew */
  BOX(g, m_char, 0.1f, 0.3f, 0.1f, 0.03f, 0.93f, -0.05f);                                /* its arm and needle bar */
  rot(part(g, g_disc, m_steel_hi, 0.12f, 0.03f, 0.12f, -0.05f, 1.02f, -0.57f), 1.5707963f, 0, 0);   /* hand wheel */
  s->light = noshadow(part(g, g_sphere, keep_mat(emis(0xfff0d0, 0.4f)), 0.03f, 0.03f, 0.03f, 0.1f, 0.86f, -0.05f));
  BOX(g, m_bone, 0.2f, 0.3f, 0.4f, -0.05f, 0.93f, 0.35f);                                /* the second machine */
  {
    t3_material *th[6] = { m_rust, m_info, m_cord, m_green, m_violet, m_bone };
    BOX(g, m_wood, 0.06f, 0.04f, 0.6f, -0.3f, 1.25f, 0);
    for (int k = 0; k < 6; k++) noshadow(CYL(g, th[k], 0.02f, 0.1f, -0.3f, 1.32f, -0.25f + k * 0.1f));
  }
  grey_chair(g, 0.3f, 0, -1.5707963f);

  /* the bathroom: a bone door in the back wall, a frame, the handle, the tube over it */
  s = &S[ST_BATH]; g = station_group(s);
  BOX(g, m_bone, 0.9f, 2.1f, 0.04f, 0, 1.05f, -0.11f);                                  /* just proud of the painted wall at z -6.995 */
  BOX(g, m_char, 0.08f, 2.2f, 0.1f, -0.49f, 1.1f, -0.1f);
  BOX(g, m_char, 0.08f, 2.2f, 0.1f, 0.49f, 1.1f, -0.1f);
  BOX(g, m_char, 1.06f, 0.08f, 0.1f, 0, 2.2f, -0.1f);
  BOX(g, m_steel_hi, 0.12f, 0.04f, 0.06f, 0.32f, 1.0f, -0.07f);                          /* handle */
  s->light = noshadow(BOX(g, keep_mat(emis(0xfff6e0, 0)), 0.6f, 0.05f, 0.05f, 0, 2.32f, -0.08f));

  /* member storage on the west wall: lockers, bankers boxes on top by count, the one that gave up */
  s = &S[ST_STORAGE]; g = station_group(s);
  BOX(g, m_steel, 0.6f, 1.9f, 1.2f, 0, 0.95f, 0);
  noshadow(BOX(g, m_char, 0.01f, 1.85f, 0.02f, 0.305f, 0.95f, 0));                       /* door seams, a 2 x 3 grid */
  for (int r = 1; r < 3; r++) noshadow(BOX(g, m_char, 0.01f, 0.02f, 1.18f, 0.305f, 0.03f + r * 0.633f, 0));
  for (int k = 0; k < 6; k++) noshadow(BOX(g, m_amber, 0.06f, 0.06f, 0.06f, 0.33f, 0.4f + (k / 2) * 0.633f, k & 1 ? 0.1f : -0.1f));   /* padlocks */
  BOX(g, m_grey, 0.6f, 0.03f, 1.2f, 0, 1.915f, 0);                                       /* the shelf on top */
  for (int k = 0; k < 6; k++) {
    storage_box[k] = BOX(g, m_cardboard, 0.4f, 0.3f, 0.3f, 0, 2.08f + (k / 3) * 0.31f, -0.4f + (k % 3) * 0.4f);
    storage_box[k]->base.visible = false;
  }
  storage_misc = noshadow(BOX(g, m_bone, 0.01f, 0.06f, 0.15f, 0.205f, 2.39f, 0.4f));    /* MISC, on the top box */
  storage_misc->base.visible = false;
  storage_tilt = rot(BOX(g, m_cardboard, 0.4f, 0.3f, 0.3f, 0.6f, 0.17f, 0.3f), 0, 0.3f, 0.4f);
  storage_tilt->base.visible = false;

  /* the welding bay on the east wall: table, two cylinders chained up, the blue curtain, the MIG on its cart */
  s = &S[ST_FORGE]; g = station_group(s);
  BOX(g, m_steel_hi, 0.7f, 0.08f, 1.0f, -0.15f, 0.85f, 0.15f);
  for (int k = 0; k < 4; k++) BOX(g, m_char, 0.05f, 0.81f, 0.05f, k & 1 ? 0.15f : -0.45f, 0.405f, k & 2 ? 0.6f : -0.3f);
  for (int k = 0; k < 2; k++) CYL(g, m_steel_hi, 0.14f, 1.3f, 0.34f, 0.65f, -0.68f + k * 0.3f);
  BOX(g, m_rust, 0.03f, 0.03f, 0.6f, 0.44f, 1.05f, -0.53f);                              /* the chain */
  BOX(g, m_blue, 0.05f, 1.4f, 1.2f, 0.46f, 1.0f, 0.15f);                                 /* curtain, wall side */
  BOX(g, m_char, 0.35f, 0.04f, 0.4f, -0.2f, 0.3f, -0.6f);                                /* the cart */
  BOX(g, m_red, 0.3f, 0.5f, 0.4f, -0.2f, 0.57f, -0.6f);                                  /* the MIG */
  part(g, g_sphere, m_char, 0.14f, 0.14f, 0.14f, -0.25f, 0.99f, 0.45f);                  /* helmet */
  s->light = noshadow(part(g, g_sphere, keep_mat(emis(0xffb36b, 0)), 0.08f, 0.08f, 0.08f, -0.15f, 0.95f, 0.1f));

  /* coffee and books: an open black shelf (no top board, so the top row reads from above), books out on the floor */
  s = &S[ST_LIBRARY]; g = station_group(s);
  BOX(g, m_char, 1.2f, 1.6f, 0.03f, 0, 0.8f, 0.16f);
  for (int k = -1; k <= 1; k += 2) BOX(g, m_char, 0.03f, 1.6f, 0.35f, k * 0.585f, 0.8f, 0);
  {
    t3_material *sp[5] = { m_rust, m_info, m_bone, m_violet, m_green };
    for (int r = 0; r < 3; r++) BOX(g, m_char, 1.2f, 0.03f, 0.35f, 0, 0.05f + r * 0.5f, 0);
    for (int k = 0; k < 20; k++) {
      int r = k / 7, c = k % 7;
      noshadow(BOX(g, sp[k % 5], 0.03f + (k % 3) * 0.02f, 0.22f + (k % 2) * 0.04f, 0.18f, -0.45f + c * 0.15f, 0.19f + r * 0.5f, -0.02f));
    }
  }
  for (int k = 0; k < 4; k++) {
    lib_book[k] = rot(noshadow(BOX(g, m_bone, 0.22f, 0.04f, 0.3f, -0.4f + k * 0.27f, 0.02f + (k & 1) * 0.04f, -0.6f)), 0, (k - 1.5f) * 0.4f, 0);
    lib_book[k]->base.visible = false;
  }
  noshadow(BOX(g, m_bone, 0.01f, 0.3f, 0.2f, -0.605f, 1.2f, 0));                         /* the clipboard */

  /* the green TURN OFF GAS door: a hinge group at the east jamb, the leaf swings into the room */
  s = &S[ST_BACKDOOR]; g = station_group(s);
  BOX(g, m_steel_hi, 0.08f, 2.5f, 0.1f, -s->hw, 1.25f, 0);
  BOX(g, m_steel_hi, 0.08f, 2.5f, 0.1f, s->hw, 1.25f, 0);
  BOX(g, m_steel_hi, s->hw * 2 + 0.08f, 0.08f, 0.1f, 0, 2.5f, 0);
  s->light = noshadow(BOX(g, keep_mat(emis(0xffd9a8, 0)), 0.9f, 2.2f, 0.01f, 0.05f, 1.1f, -0.035f));   /* the alley light */
  sign_on(g, plaque(64, 32, C_BONE, C_FAULT, F_MONO_S, "EXIT"), 0.3f, 0.15f, 0.05f, 2.68f, 0.06f, 0);
  door_hinge = t3_group_new();
  t3_object_set_position(door_hinge, s->hw, 0, 0);
  t3_object_add(g, door_hinge); t3_release(door_hinge);
  door_leaf = t3_group_new();
  t3_object_set_position(door_leaf, -0.5f, 0, 0);
  t3_object_add(door_hinge, door_leaf); t3_release(door_leaf);
  BOX(door_leaf, m_door, 1.0f, 2.4f, 0.08f, 0, 1.2f, 0);
  BOX(door_leaf, m_char, 0.06f, 0.06f, 0.2f, -0.38f, 1.05f, 0.06f);                     /* handle */

  /* the light switch on the west wall: a plate and a toggle */
  s = &S[ST_SWITCH]; g = station_group(s);
  noshadow(BOX(g, m_bone, 0.03f, 0.18f, 0.12f, 0.13f, 1.3f, 0));
  switch_toggle = noshadow(BOX(g, m_char, 0.03f, 0.06f, 0.03f, 0.155f, 1.3f, 0));

  /* the bike station: a black filing cabinet, a stand, and a bike on it while one has a flat */
  s = &S[ST_BIKE]; g = station_group(s);
  BOX(g, m_char, 0.9f, 1.1f, 0.6f, 0, 0.55f, -0.05f);
  for (int k = 0; k < 3; k++) noshadow(BOX(g, m_steel_hi, 0.2f, 0.03f, 0.01f, 0, 0.3f + k * 0.33f, 0.255f));   /* drawer pulls */
  BOX(g, m_steel_hi, 0.04f, 1.2f, 0.04f, 0.35f, 0.6f, 0.4f);
  BOX(g, m_steel_hi, 0.12f, 0.08f, 0.12f, 0.35f, 1.22f, 0.4f);
  bike_hung = bike_model(g, -0.05f, 0.62f, 0.42f, 0, m_rust);
  bike_hung->visible = false;
}

/* ── furniture: blocks the way and does nothing ────────────────────
 * One model per OBK_ kind at each obstacle of the level. Collision is the
 * level's box; the models are sized to the section 3 footprints. */
static void grey_chair(t3_object *g, float x, float z, float ry) {
  t3_object *c = t3_group_new();
  t3_object_set_position(c, x, 0, z);
  t3_object_set_rotation(c, 0, ry, 0);
  t3_object_add(g, c);
  t3_release(c);
  BOX(c, m_char, 0.45f, 0.05f, 0.45f, 0, 0.45f, 0);
  BOX(c, m_char, 0.45f, 0.45f, 0.05f, 0, 0.7f, -0.2f);
  CYL(c, m_steel_hi, 0.03f, 0.42f, 0, 0.21f, 0);
  part(c, g_disc, m_steel_hi, 0.3f, 0.03f, 0.3f, 0, 0.015f, 0);
}

static void monitor(t3_object *g, float x, float y, float z, float ry) {
  t3_object *m = t3_group_new();
  t3_object_set_position(m, x, y, z);
  t3_object_set_rotation(m, 0, ry, 0);
  t3_object_add(g, m);
  t3_release(m);
  BOX(m, m_char, 0.08f, 0.2f, 0.08f, 0, 0.1f, 0);
  BOX(m, m_char, 0.5f, 0.32f, 0.04f, 0, 0.36f, 0);
  noshadow(BOX(m, m_tvs, 0.42f, 0.26f, 0.01f, 0, 0.36f, 0.025f));
}

static bool inside(const obst_t *o, float x, float z) { return fabsf(x - o->x) <= o->hw && fabsf(z - o->z) <= o->hd; }

static void build_furniture(const level_t *lv) {
  for (int i = 0; i < lv->ob_n; i++) {
    const obst_t *o = &lv->ob[i];
    t3_object *g = group_at(&SC->base, o->x, o->z);
    t3_object_set_rotation(g, 0, o->ry, 0);
    float w = o->hw * 2, d = o->hd * 2;
    switch (o->kind) {
      case OBK_CLOSET:                                       /* supply closet and the stairs, UPSTAIRS: LIMITED ACCESS */
        BOX(g, m_steel, w, 2.6f, d, 0, 1.3f, 0);
        BOX(g, m_steel_hi, 0.06f, 2.1f, 0.06f, -0.4f, 1.05f, o->hd + 0.02f);
        BOX(g, m_steel_hi, 0.06f, 2.1f, 0.06f, 0.4f, 1.05f, o->hd + 0.02f);
        BOX(g, m_steel_hi, 0.86f, 0.06f, 0.06f, 0, 2.1f, o->hd + 0.02f);
        BOX(g, m_char, 0.74f, 2.0f, 0.03f, 0, 1.0f, o->hd + 0.01f);
        CYL(g, m_steel_hi, 0.02f, 0.12f, 0.25f, 1.0f, o->hd + 0.05f);
        break;
      case OBK_WOODBAY: {                                    /* white tables with orange edges, bandsaw, drill press, sander */
        for (int k = -1; k <= 1; k += 2) {
          BOX(g, m_white, 1.1f, 0.05f, 1.0f, k * 0.6f, 0.73f, 0);
          BOX(g, m_orange, 1.1f, 0.06f, 0.04f, k * 0.6f, 0.73f, 0.5f);
          for (int l = 0; l < 4; l++) BOX(g, m_steel_hi, 0.05f, 0.72f, 0.05f, k * 0.6f + (l & 1 ? 0.5f : -0.5f), 0.36f, l < 2 ? -0.45f : 0.45f);
        }
        BOX(g, m_green, 0.3f, 1.4f, 0.3f, -0.85f, 0.7f, -0.2f);           /* bandsaw */
        BOX(g, m_steel_hi, 0.5f, 0.04f, 0.5f, -0.75f, 0.92f, 0);
        CYL(g, m_steel_hi, 0.05f, 1.5f, 0.85f, 0.75f, -0.25f);           /* drill press */
        BOX(g, m_green, 0.3f, 0.3f, 0.4f, 0.85f, 1.35f, -0.1f);
        part(g, g_disc, m_steel_hi, 0.22f, 0.03f, 0.22f, 0.85f, 0.95f, 0);
        BOX(g, m_char, 0.3f, 0.25f, 0.3f, -0.3f, 0.88f, 0.15f);           /* disc sander */
        rot(part(g, g_disc, m_steel_hi, 0.16f, 0.02f, 0.16f, -0.12f, 0.95f, 0.15f), 0, 0, 1.5707963f);
        BOX(g, m_info, 0.5f, 0.4f, 0.4f, 0.5f, 0.2f, 0);                  /* parts drawers */
        break;
      }
      case OBK_FENCE: {                                      /* the shop's steel half wall, posts, the tape line */
        BOX(g, m_steel, w, 1.1f, d, 0, 0.55f, 0);
        bool along_x = o->hw > o->hd;
        float len = along_x ? w : d;
        for (float a = -len / 2; a <= len / 2 + 0.01f; a += 2.0f) {
          float p = a > len / 2 ? len / 2 : a;
          BOX(g, m_steel_hi, 0.1f, 1.4f, 0.1f, along_x ? p : 0, 0.7f, along_x ? 0 : p);
        }
        BOX(g, m_steel_hi, 0.1f, 1.4f, 0.1f, along_x ? len / 2 : 0, 0.7f, along_x ? 0 : len / 2);
        if (along_x) BOX(g, m_amber, w, 0.08f, 0.03f, 0, 0.5f, o->hd + 0.015f);
        break;
      }
      case OBK_LATHE:                                        /* the green lathe, long axis x */
        BOX(g, m_char, 2.2f, 0.75f, 0.7f, 0, 0.375f, 0.05f);
        BOX(g, m_green, 2.0f, 0.12f, 0.4f, 0, 0.82f, 0);
        BOX(g, m_green, 0.5f, 0.5f, 0.5f, -0.75f, 1.1f, 0);                /* headstock */
        rot(CYL(g, m_steel_hi, 0.16f, 0.18f, -0.42f, 1.08f, 0), 0, 0, 1.5707963f);   /* chuck */
        BOX(g, m_green, 0.3f, 0.32f, 0.35f, 0.75f, 1.02f, 0);               /* tailstock */
        BOX(g, m_green, 0.35f, 0.15f, 0.35f, 0.1f, 0.95f, -0.05f);           /* carriage */
        for (int k = 0; k < 8; k++) BOX(g, m_steel_hi, 0.04f, 0.04f, 0.04f, -1.0f + k * 0.28f, 0.02f, 0.45f + (k & 1) * 0.1f);   /* chips */
        break;
      case OBK_MILL:                                         /* the Blue Mill */
        BOX(g, m_blue, 0.4f, 1.6f, 0.5f, 0.1f, 0.8f, -0.15f);
        BOX(g, m_steel_hi, 0.9f, 0.12f, 0.4f, -0.05f, 0.9f, 0.2f);
        BOX(g, m_blue, 0.35f, 0.5f, 0.4f, 0, 1.6f, 0.15f);
        CYL(g, m_steel_hi, 0.05f, 0.3f, 0, 1.2f, 0.2f);
        BOX(g, m_char, 0.2f, 0.15f, 0.08f, 0.35f, 1.45f, 0.3f);           /* the DRO */
        noshadow(part(g, g_sphere, m_led, 0.03f, 0.03f, 0.03f, 0.35f, 1.45f, 0.35f));
        break;
      case OBK_COUNTER: {
        const station_t *p1 = &S[ST_PRINT1];
        if (inside(o, p1->x, p1->z)) {                       /* the printing counter: black steel shelving, a back board */
          BOX(g, m_char, w, 0.05f, d, 0, 0.78f, 0);
          BOX(g, m_char, w, 0.04f, d, 0, 0.32f, 0);
          for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.05f, 0.8f, 0.05f, k & 1 ? o->hw - 0.05f : -o->hw + 0.05f, 0.4f, k < 2 ? -o->hd + 0.05f : o->hd - 0.05f);
          BOX(g, m_char, w, 1.1f, 0.05f, 0, 1.3f, -o->hd + 0.025f);
          t3_material *sp[4] = { m_pcb, m_amber, m_violet, m_bone };
          for (int k = 0; k < 4; k++) part(g, g_cyl, sp[k], 0.12f, 0.08f, 0.12f, (k < 2 ? -o->hw + 0.25f : o->hw - 0.25f), 0.845f, (k & 1) ? 0.25f : -0.2f);
          BOX(g, m_char, 0.3f, 0.35f, 0.3f, -0.5f, 0.515f, 0.1f);        /* a resin printer */
          BOX(g, m_bone, 0.3f, 0.3f, 0.3f, 0.5f, 0.49f, 0.1f);           /* the filament dryer */
        } else {                                             /* the craft counter: bone with an orange edge, a back board */
          BOX(g, m_bone, w, 0.75f, d, 0, 0.375f, 0);
          BOX(g, m_orange, w, 0.06f, 0.04f, 0, 0.73f, o->hd);
          BOX(g, m_bone, w, 0.9f, 0.05f, 0, 1.2f, -o->hd + 0.025f);
          BOX(g, m_char, 0.4f, 0.35f, 0.4f, 1.0f, 0.925f, 0);            /* heat press */
          BOX(g, m_steel_hi, 0.36f, 0.04f, 0.36f, 1.0f, 1.12f, 0);
          BOX(g, m_bone, 0.4f, 0.12f, 0.2f, -1.05f, 0.81f, 0.1f);        /* a Cameo */
        }
        break;
      }
      case OBK_DESKS:                                        /* the coworking row: three plywood desks with monitors */
        for (int k = -1; k <= 1; k++) {
          float z = k * 1.4f;
          BOX(g, m_plywood, 1.1f, 0.05f, 1.3f, 0, 0.73f, z);
          BOX(g, m_plywood, 1.1f, 0.7f, 0.05f, 0, 0.35f, z - 0.62f);
          BOX(g, m_plywood, 1.1f, 0.7f, 0.05f, 0, 0.35f, z + 0.62f);
          monitor(g, 0.25f, 0.76f, z, 1.5707963f);
          BOX(g, m_steel, 0.14f, 0.02f, 0.4f, -0.25f, 0.77f, z);
        }
        part(g, g_knot, m_char, 0.1f, 0.05f, 0.1f, 0, 0.1f, 0);
        break;
      case OBK_WINDOW: {                                     /* the west display window: a CRT wall and yellow crates */
        BOX(g, m_char, w, 0.5f, d, 0, 0.25f, 0);
        BOX(g, m_bone, w, 1.6f, 0.05f, 0, 1.3f, o->hd);
        for (int k = 0; k < 8; k++) {
          float x = -o->hw + 1.6f + (k % 4) * 1.0f, y = 0.68f + (k / 4) * 0.4f;
          BOX(g, m_crt, 0.42f, 0.36f, 0.4f, x, y, -0.05f);
          noshadow(BOX(g, m_tvs, 0.3f, 0.3f, 0.01f, x, y, 0.16f));
        }
        for (int k = 0; k < 3; k++) BOX(g, m_cord, 0.45f, 0.45f, 0.45f, -o->hw + 0.4f + (k == 2) * 0.5f, 0.725f + (k == 1) * 0.45f, 0);
        for (int k = 0; k < 2; k++) {
          BOX(g, m_bone, 0.3f, 0.4f, 0.02f, o->hw - 0.5f - k * 0.5f, 1.4f, o->hd - 0.04f);
          BOX(g, m_rust, 0.3f, 0.08f, 0.01f, o->hw - 0.5f - k * 0.5f, 1.5f, o->hd - 0.03f);
        }
        break;
      }
      case OBK_COFFEE:                                       /* coffee cabinet: Keurig, carafe, mugs */
        BOX(g, m_steel, w - 0.1f, 0.85f, d - 0.1f, 0, 0.425f, 0);
        BOX(g, m_woodtop, w, 0.06f, d, 0, 0.88f, 0);
        BOX(g, m_char, 0.25f, 0.3f, 0.3f, 0, 1.06f, -0.3f);
        CYL(g, m_screen, 0.07f, 0.15f, 0.05f, 0.985f, 0.05f);
        for (int k = 0; k < 3; k++) CYL(g, m_bone, 0.05f, 0.1f, 0.15f, 0.96f, 0.25f + k * 0.13f);
        break;
      case OBK_LEGO:                                         /* the LEGO table */
        BOX(g, m_white, w, 0.05f, d, 0, 0.75f, 0);
        for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.05f, 0.73f, 0.05f, k & 1 ? o->hw - 0.1f : -o->hw + 0.1f, 0.365f, k < 2 ? -o->hd + 0.1f : o->hd - 0.1f);
        BOX(g, m_cord, 0.5f, 0.4f, 0.4f, -0.5f, 0.98f, 0);
        {
          t3_material *bm[4] = { m_red, m_info, m_cord, m_green };
          for (int k = 0; k < 6; k++) BOX(g, bm[k % 4], 0.16f, 0.1f, 0.08f, 0.2f + (k % 3) * 0.22f, 0.825f, -0.2f + (k / 3) * 0.3f);
        }
        break;
      case OBK_HOST:                                         /* the host desk and the waiver computer */
        BOX(g, m_plywood, w, 0.05f, d, 0, 0.75f, 0);
        BOX(g, m_plywood, 0.05f, 0.73f, d, -o->hw + 0.05f, 0.365f, 0);
        BOX(g, m_plywood, 0.05f, 0.73f, d, o->hw - 0.05f, 0.365f, 0);
        monitor(g, -0.15f, 0.78f, -0.15f, 3.1415927f);
        BOX(g, m_bone, 0.2f, 0.01f, 0.3f, 0.25f, 0.78f, 0.1f);
        break;
      case OBK_ISLAND:                                       /* the second table, class seating */
        BOX(g, m_white, w, 0.06f, d, 0, 0.78f, 0);
        for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.06f, 0.76f, 0.06f, k & 1 ? o->hw - 0.15f : -o->hw + 0.15f, 0.38f, k < 2 ? -o->hd + 0.15f : o->hd - 0.15f);
        for (int k = 0; k < 3; k++) { grey_chair(g, -1.0f + k * 1.0f, -o->hd + 0.3f, 0); grey_chair(g, -1.0f + k * 1.0f, o->hd - 0.3f, 3.1415927f); }
        break;
      case OBK_DONBIN:                                       /* the tall donation bin */
        BOX(g, m_cardboard, 0.5f, 1.4f, 0.5f, 0, 0.7f, 0);
        BOX(g, m_char, 0.3f, 0.04f, 0.52f, 0, 1.3f, 0);
        BOX(g, m_bone, 0.3f, 0.2f, 0.01f, 0, 0.9f, 0.255f);
        break;
      case OBK_FOLD:                                         /* a folding table with grey chairs */
        BOX(g, m_white, w, 0.05f, d, 0, 0.75f, 0);
        for (int k = 0; k < 4; k++) BOX(g, m_steel_hi, 0.04f, 0.73f, 0.04f, k & 1 ? o->hw - 0.2f : -o->hw + 0.2f, 0.365f, k < 2 ? -o->hd + 0.15f : o->hd - 0.15f);
        grey_chair(g, -0.6f, -o->hd + 0.28f, 0); grey_chair(g, 0.6f, -o->hd + 0.28f, 0);
        grey_chair(g, -0.6f, o->hd - 0.28f, 3.1415927f); grey_chair(g, 0.6f, o->hd - 0.28f, 3.1415927f);
        break;
      case OBK_BOXES:                                        /* a stack of three cardboard boxes, rust tape */
        for (int k = 0; k < 3; k++) {
          rot(BOX(g, m_cardboard, 0.8f - k * 0.1f, 0.4f, 0.7f - k * 0.1f, 0, 0.2f + k * 0.4f, 0), 0, k * 0.2f - 0.2f, 0);
          BOX(g, m_rust, 0.82f - k * 0.1f, 0.04f, 0.1f, 0, 0.42f + k * 0.4f, 0);
        }
        break;
      default: BOX(g, m_steel, w, 0.8f, d, 0, 0.4f, 0); break;
    }
  }
}

/* ── props: set dressing with no collision ─────────────────────────
 * One model per PR_ kind, geometry from the design's section 10. Every
 * piece is boxes and cylinders in shared materials so it instances. */
static void build_props(const level_t *lv) {
  fan_n = 0;
  for (int i = 0; i < lv->prop_n; i++) {
    const prop_t *p = &lv->props[i];
    t3_object *g = group_at(&SC->base, p->x, p->z);
    t3_object_set_position(g, p->x, p->y, p->z);
    t3_object_set_rotation(g, 0, p->ry, 0);
    switch (p->kind) {
      case PR_PENDANT:                                       /* a cord from the ceiling, a char dome, a warm bulb */
        noshadow(CYL(g, m_cord, 0.012f, 3.4f - p->y, 0, (3.4f - p->y) / 2, 0));
        noshadow(part(g, g_cone, m_char, 0.32f, 0.28f, 0.32f, 0, 0.14f, 0));
        noshadow(part(g, g_sphere, m_bulb, 0.07f, 0.07f, 0.07f, 0, -0.08f, 0));
        break;
      case PR_TUBE:                                          /* a fluorescent fixture */
        noshadow(BOX(g, m_steel_hi, 3.0f, 0.08f, 0.14f, 0, 0, 0));
        noshadow(BOX(g, m_tube, 2.8f, 0.06f, 0.08f, 0, -0.07f, 0.08f));
        break;
      case PR_FAN:                                           /* a ceiling fan, spun by world_sync */
        noshadow(CYL(g, m_steel_hi, 0.1f, 0.12f, 0, 0, 0));
        noshadow(BOX(g, m_wood, 1.8f, 0.02f, 0.16f, 0, -0.04f, 0));
        noshadow(rot(BOX(g, m_wood, 1.8f, 0.02f, 0.16f, 0, -0.04f, 0), 0, 1.5707963f, 0));
        if (fan_n < (int)(sizeof fans / sizeof fans[0])) fans[fan_n++] = g;
        break;
      case PR_EXTINGUISHER: {                                /* red cylinder on a steel bracket against the wall */
        float bz = p->z < 0 ? -0.1f : 0.1f;
        BOX(g, m_steel_hi, 0.12f, 0.3f, 0.05f, 0, 0, bz);
        CYL(g, m_red, 0.09f, 0.45f, 0, 0, 0);
        CYL(g, m_char, 0.03f, 0.1f, 0, 0.27f, 0);
        break;
      }
      case PR_FIRSTAID:                                      /* a bone box with a red cross */
        BOX(g, m_bone, 0.3f, 0.3f, 0.1f, 0, 0, 0);
        BOX(g, m_red, 0.18f, 0.05f, 0.02f, 0, 0, 0.06f);
        BOX(g, m_red, 0.05f, 0.18f, 0.02f, 0, 0, 0.06f);
        break;
      case PR_SAFETYGLASSES:                                 /* the glasses box on the fence post */
        BOX(g, m_bone, 0.5f, 0.3f, 0.06f, 0, 0, 0);
        for (int k = 0; k < 4; k++) BOX(g, m_visor, 0.08f, 0.04f, 0.03f, -0.15f + k * 0.1f, 0.05f - (k & 1) * 0.1f, 0.04f);
        break;
      case PR_SHOPVAC:                                       /* a char drum with a rust hose */
        CYL(g, m_char, 0.22f, 0.5f, 0, 0.25f, 0);
        CYL(g, m_steel_hi, 0.12f, 0.1f, 0, 0.55f, 0);
        part(g, g_knot, m_rust, 0.2f, 0.2f, 0.2f, 0.25f, 0.3f, 0.1f);
        break;
      case PR_BROOM:                                         /* leaning on whatever is nearest */
        rot(CYL(g, m_wood, 0.02f, 1.3f, 0.1f, 0.65f, 0), 0, 0, 0.15f);
        BOX(g, m_sawdust, 0.3f, 0.08f, 0.08f, 0, 0.04f, 0);
        break;
      case PR_TOOLCART:                                      /* the red rolling tool cart */
        BOX(g, m_red, 0.6f, 0.9f, 0.45f, 0, 0.5f, 0);
        for (int k = 0; k < 3; k++) BOX(g, m_char, 0.5f, 0.02f, 0.01f, 0, 0.3f + k * 0.25f, 0.23f);
        for (int k = 0; k < 4; k++) rot(part(g, g_disc, m_char, 0.05f, 0.03f, 0.05f, k & 1 ? 0.25f : -0.25f, 0.05f, k < 2 ? -0.18f : 0.18f), 0, 0, 1.5707963f);
        break;
      case PR_BUCKET: CYL(g, m_orange, 0.16f, 0.3f, 0, 0.15f, 0); break;
      case PR_WATER:                                         /* two cases of water, stacked */
        BOX(g, m_info, 0.5f, 0.4f, 0.35f, 0, 0.2f, 0);
        BOX(g, m_info, 0.5f, 0.4f, 0.35f, 0.02f, 0.6f, 0.02f);
        break;
      case PR_AFRAME:                                        /* the sidewalk sign, faces to the street and the room */
        rot(BOX(g, m_bone, 0.6f, 0.9f, 0.03f, 0, 0.45f, 0.13f), -0.3f, 0, 0);
        rot(BOX(g, m_bone, 0.6f, 0.9f, 0.03f, 0, 0.45f, -0.13f), 0.3f, 0, 0);
        BOX(g, m_char, 0.62f, 0.04f, 0.04f, 0, 0.9f, 0);
        break;
      case PR_BIKERACK:                                      /* four inverted U tubes at 0.6 pitch, a bike per slot */
        for (int k = 0; k < 4; k++) {
          float x = k * 0.6f;
          BOX(g, m_steel_hi, 0.03f, 0.8f, 0.03f, x, 0.4f, -0.25f);
          BOX(g, m_steel_hi, 0.03f, 0.8f, 0.03f, x, 0.4f, 0.25f);
          BOX(g, m_steel_hi, 0.03f, 0.03f, 0.5f, x, 0.8f, 0);
          if (rack_n < 4) {
            rack_frame[rack_n] = keep_mat(mat(0x8b8578, 0.6f, 0.2f));
            rack_bike[rack_n] = bike_model(g, x + 0.1f, 0, 0, 1.5707963f, rack_frame[rack_n]);
            rack_bike[rack_n]->visible = false;
            rack_n++;
          }
        }
        break;
      case PR_LEGO_CRATES: {                                 /* yellow crates, two and one, bricks on top */
        BOX(g, m_cord, 0.45f, 0.45f, 0.45f, -0.25f, 0.225f, 0);
        BOX(g, m_cord, 0.45f, 0.45f, 0.45f, 0.25f, 0.225f, 0);
        BOX(g, m_cord, 0.45f, 0.45f, 0.45f, 0, 0.675f, 0);
        t3_material *bm[3] = { m_rust, m_info, m_led };
        for (int k = 0; k < 12; k++) BOX(g, bm[k % 3], 0.08f, 0.05f, 0.06f, -0.4f + (k % 6) * 0.16f, k < 6 ? 0.475f : 0.925f, (k / 6) ? 0.1f : -0.15f);
        break;
      }
      case PR_CHAIR: grey_chair(g, 0, 0, 0); break;
      case PR_PLANT:                                         /* alive, or the one on the host desk */
        CYL(g, m_cardboard, 0.18f, 0.3f, 0, 0.15f, 0);
        if (p->arg) {
          part(g, g_sphere, m_scrap, 0.22f, 0.11f, 0.22f, 0, 0.32f, 0);
          part(g, g_sphere, m_scrap, 0.16f, 0.08f, 0.16f, 0.1f, 0.36f, 0.08f);
        } else {
          part(g, g_sphere, m_pcb, 0.22f, 0.22f, 0.22f, 0, 0.48f, 0);
          part(g, g_sphere, m_pcb, 0.22f, 0.22f, 0.22f, 0.12f, 0.66f, 0.06f);
        }
        break;
      case PR_HAT:                                           /* the host's captain's hat */
        part(g, g_disc, m_char, 0.14f, 0.03f, 0.14f, 0, 0.015f, 0);
        CYL(g, m_char, 0.1f, 0.1f, 0, 0.08f, 0);
        BOX(g, m_bone, 0.21f, 0.03f, 0.03f, 0, 0.05f, 0.09f);
        BOX(g, m_cord, 0.04f, 0.04f, 0.01f, 0, 0.1f, 0.1f);
        break;
      case PR_PROJECTOR:                                     /* hanging over the HYH table; no screen */
        BOX(g, m_steel, 0.3f, 0.12f, 0.3f, 0, 0, 0);
        CYL(g, m_steel_hi, 0.02f, 3.4f - p->y, 0, (3.4f - p->y) / 2, 0);
        CYL(g, m_screen, 0.04f, 0.04f, 0, 0, 0.16f);
        break;
      case PR_SLEEPER: {                                     /* LATE NIGHT: somebody on the couch, along its length */
        t3_object *o = keep(world_person(0x8b8578, true, NULL));
        t3_object_set_position(o, p->x, p->y, p->z);
        t3_object_set_rotation(o, 0, p->ry + 1.5707963f, 1.5707963f);
        break;
      }
      case PR_VISITOR:                                       /* OPEN HOUSE: a grey visitor at the LEGO table, bobbing */
        if (visitor_n < 8) {
          t3_object *o = keep(world_person(0x8b8578, true, NULL));
          t3_object_set_position(o, p->x, 0, p->z);
          t3_object_set_rotation(o, 0, p->ry, 0);
          visitor[visitor_n++] = o;
        }
        break;
      default: break;
    }
  }
}

/* ── signs: painted textures, basic+map, unlit ─────────────────────── */

/* a small plaque: text centred on a field, split on a newline or where it will not fit */
static void plaque_paint(int w, int h, uint32_t bg, uint32_t fg, int font, const char *text) {
  hud_clear();
  hud_rect(0, 0, w, h, bg, 255);
  char lines[3][48]; int n = 0;
  const char *s = text;
  while (*s && n < 3) {
    int k = 0;
    while (*s && *s != '\n' && k < 47) lines[n][k++] = *s++;
    lines[n][k] = 0;
    if (*s == '\n') s++;
    n++;
  }
  if (n == 1 && hud_text_w(font, lines[0]) > w - 12) {       /* break at the middle space */
    int len = (int)strlen(lines[0]), best = -1;
    for (int i = 0; i < len; i++) if (lines[0][i] == ' ' && (best < 0 || abs(i - len / 2) < abs(best - len / 2))) best = i;
    if (best > 0) { strcpy(lines[1], lines[0] + best + 1); lines[0][best] = 0; n = 2; }
  }
  int lh = hud_font_h(font), y = (h - n * lh) / 2;
  for (int i = 0; i < n; i++) hud_text(font, w / 2, y + i * lh, lines[i], fg, 255, AL_CENTER);
}
static t3_texture *plaque(int w, int h, uint32_t bg, uint32_t fg, int font, const char *text) {
  plaque_paint(w, h, bg, fg, font, text);
  return hud_texture(w, h);
}

static void build_signs(const level_t *lv) {
  for (int i = 0; i < lv->sign_n; i++) {
    const sign_t *sg = &lv->signs[i];
    const char *t = sg->text ? sg->text : "";
    t3_texture *tx = NULL;
    switch (sg->kind) {
      case SG_LOCKUP:                                        /* the mark, then the wordmark set in Charis SIL */
        hud_clear();
        hud_rect(0, 0, 1024, 256, C_CHAR, 255);
        hud_logo(28, 18, 220, C_BONE);
        hud_text(F_LOGO, 270, 70, "HeatSync Labs", C_BONE, 255, AL_LEFT);
        hud_text(F_MONO_S, 274, 150, "108 W MAIN ST  MESA AZ  SINCE 2009", C_ASH, 255, AL_LEFT);
        hud_tape(0, 236, 1024, 20, 0);
        tx = hud_texture(1024, 256);
        break;
      case SG_FACADE:                                        /* the street sign: bone field, the mark, the wordmark */
        hud_clear();
        hud_rect(0, 0, 1024, 256, C_BONE, 255);
        hud_frame(0, 0, 1024, 256, 6, C_CHAR, 255);
        hud_logo(40, 18, 220, C_CHAR);
        hud_text(F_LOGO, 300, 60, "HeatSync Labs", C_CHAR, 255, AL_LEFT);
        hud_text(F_MONO_S, 304, 140, "A COMMUNITY WORKSHOP. OPEN TO THE PUBLIC.", C_STEEL, 255, AL_LEFT);
        hud_text(F_MONO_S, 304, 170, "108 W MAIN ST  MESA AZ", C_STEEL, 255, AL_LEFT);
        tx = hud_texture(1024, 256);
        break;
      case SG_TILE:     tx = plaque(256, 128, 0xe07a2a, C_CHAR, F_MONO_S, t); break;
      case SG_TRIPTYCH: tx = plaque(128, 128, C_BONE, C_CHAR, hud_text_w(F_MONO_L, t) <= 116 ? F_MONO_L : F_MONO, t); break;
      case SG_WIFI:     tx = plaque(512, 64, C_CHAR, C_LIVE, F_MONO, t); break;
      case SG_EXIT:     tx = plaque(128, 64, C_CHAR, C_FAULT, F_MONO_L, t); break;
      case SG_108:      tx = plaque(128, 64, C_CHAR, C_BONE, F_MONO_L, t); break;
      case SG_GAS:      tx = plaque(256, 64, C_BONE, C_CHAR, F_MONO, t); break;
      case SG_CAUTION:  tx = plaque(128, 64, C_HAZARD, C_CHAR, F_MONO, t); break;
      case SG_DARKROOM: tx = plaque(256, 64, C_BONE, C_CHAR, F_MONO, t); break;
      case SG_PASSES:
        plaque_paint(256, 128, C_LIVE, C_CHAR, F_MONO_S, t);
        hud_rect(226, 0, 30, 30, C_FAULT, 255);           /* the red tag corner */
        tx = hud_texture(256, 128);
        break;
      case SG_UPSTAIRS: tx = plaque(256, 64, C_HAZARD, C_CHAR, F_MONO_S, t); break;
      case SG_OPEN:                                          /* the neon in the window, deliberately not GANTRY */
        hud_clear();
        hud_rect(0, 0, 256, 128, 0x14110d, 255);
        hud_frame(4, 4, 248, 120, 6, 0xff4f8a, 255);
        hud_text(F_H1, 128, 22, "OPEN", 0xff7fae, 255, AL_CENTER);
        tx = hud_texture(256, 128);
        break;
      case SG_PEGBOARD: {                                    /* every tool has an outline, a third are missing */
        hud_clear();
        hud_rect(0, 0, 512, 256, 0xb08a5a, 255);
        for (int y = 8; y < 256; y += 16) for (int x = 8; x < 512; x += 16) hud_circle(x, y, 2, 0x5a4428, 255);
        static const struct { int x, y, w, h; uint32_t c; } TOOLS[] = {
          { 30, 40, 18, 120, 0xc2542c }, { 70, 50, 60, 14, 0x3a342b }, { 150, 36, 14, 100, 0x5e93c4 }, { 190, 40, 40, 40, 0x15120f },
          { 260, 50, 16, 110, 0xf2ab1e }, { 300, 40, 70, 18, 0x9b7fd0 }, { 400, 40, 20, 120, 0x15120f }, { 440, 70, 50, 50, 0xc2542c },
        };
        for (unsigned k = 0; k < sizeof TOOLS / sizeof TOOLS[0]; k++) {
          if (k % 3 == 2) hud_frame(TOOLS[k].x, TOOLS[k].y, TOOLS[k].w, TOOLS[k].h, 3, 0x2a1f12, 255);
          else hud_rect(TOOLS[k].x, TOOLS[k].y, TOOLS[k].w, TOOLS[k].h, TOOLS[k].c, 255);
        }
        hud_text(F_MONO_S, 24, 210, "IF YOU BORROW IT, PUT IT BACK.", 0x2a1f12, 255, AL_LEFT);
        hud_tape(0, 0, 512, 18, 0);
        tx = hud_texture(512, 256);
        break;
      }
      case SG_WC:       tx = plaque(64, 64, C_BONE, C_CHAR, F_MONO, t); break;
      case SG_PAY:      tx = plaque(128, 64, C_BONE, C_CHAR, F_MONO_S, t); break;
      default: break;
    }
    if (!tx) continue;
    if (sg->kind == SG_GAS && door_leaf) {
      /* panel local: a child of the door leaf, so it swings with the door */
      sign_on(door_leaf, tx, sg->w, sg->h, sg->x, sg->y, sg->z, sg->ry);
    } else if (sg->kind == SG_GAS) {
      const st_place_t *bd = &lv->st[ST_BACKDOOR];
      sign(tx, sg->w, sg->h, bd->x + sg->x, sg->y, bd->z + sg->z, sg->ry);
    } else {
      sign(tx, sg->w, sg->h, sg->x, sg->y, sg->z, sg->ry);
    }
  }
}

/* ── lighting: six lights made once, moved and retinted per level ─── */
static void world_apply_lighting(const lighting_t *lt) {
  hemi->color = t3_color_hex(lt->sky);
  hemi->ground_color = t3_color_hex(lt->ground);
  hemi->intensity = lt->hemi;
  sun->color = t3_color_hex(lt->sun_col);
  sun->intensity = lt->sun_i;
  t3_object_set_position(sun, lt->sun_x, lt->sun_y, lt->sun_z);
  sun->shadow->bias = lt->bias;
  t3_light *pl[4] = { kitchen_lamp, bench_lamp, laser_glow, hall_lamp };
  for (int i = 0; i < 4; i++) {
    pl[i]->color = t3_color_hex(lt->p[i].col);
    pl[i]->intensity = lt->p[i].i;
    pl[i]->distance = lt->p[i].dist;
  }
  laser_glow->intensity = lt->laser_idle;
  /* positions derive from the stations: the pools land where the lamps hang */
  const station_t *sk = &S[ST_SINK], *be = &S[ST_BENCH], *la = &S[ST_LASER], *ta = &S[ST_TABLE];
  t3_object_set_position(kitchen_lamp, sk->x + 1.4f, 2.9f, sk->z + 0.8f);
  t3_object_set_position(bench_lamp, be->x - 0.6f, 2.9f, be->z + 1.4f);
  t3_object_set_position(laser_glow, la->x, 1.8f, la->z + 0.75f);
  t3_object_set_position(hall_lamp, ta->x, 2.9f, ta->z);
  m_bulb->emissive_intensity = lt->pendant_e;
  m_tube->emissive_intensity = lt->tube_e;
}

static void build_all(const level_t *lv) {
  build_room(lv);
  build_stations();
  build_furniture(lv);
  build_props(lv);
  build_signs(lv);
  world_apply_lighting(&lv->light);
}

/* Drop the room and build the level that game_set_level made active. The
 * lights and the camera stay; nothing here changes a render state. Call
 * game_reset_world first so no item or newbie points at a removed model. */
void world_set_level(const level_t *nl) {
  if (nl && nl != L) L = nl;
  for (int i = 0; i < built_n; i++) t3_object_remove(&SC->base, built[i]);
  built_n = 0;
  for (int i = 0; i < owned_n; i++) t3_release(owned[i]);
  owned_n = 0;
  for (int i = 0; i < tex_n; i++) t3_release(tex[i]);
  tex_n = 0;
  fan_n = 0; rack_n = 0; visitor_n = 0;
  door_hinge = door_leaf = bike_hung = NULL; switch_toggle = storage_misc = storage_tilt = NULL;
  for (int k = 0; k < 6; k++) storage_box[k] = NULL;
  for (int k = 0; k < 4; k++) lib_book[k] = NULL;
  for (int i = 0; i < ST_COUNT; i++) { S[i].obj = NULL; S[i].light = S[i].beam = S[i].growth = NULL; }
  build_all(L);
}

void world_init(t3_scene *scene) {
  SC = scene;
  g_box = t3_box_geometry_new(1, 1, 1, 1, 1, 1);
  g_cyl = t3_cylinder_geometry_new(1, 1, 1, 16, 1, false);
  g_sphere = t3_sphere_geometry_new(1, 16, 12);
  g_knot = t3_torus_knot_geometry_new(1, 0.32f, 64, 8, 2, 3);
  g_disc = t3_cylinder_geometry_new(1, 1, 1, 20, 1, false);
  g_cone = t3_cone_geometry_new(1, 1, 12, 1, false);

  m_plywood = mat(0xc8a165, 0.9f, 0);
  m_amber = mat(C_HAZARD, 0.45f, 0.1f);
  m_scrap = mat(0x9c7a48, 0.9f, 0);
  m_bottle = mat(0x6b3f12, 0.2f, 0);
  m_label = mat(0xf2c94c, 0.6f, 0);
  m_spaghetti = mat(0x9b7fd0, 0.6f, 0);
  m_crt = mat(0x8b8578, 0.7f, 0);
  m_screen = mat(0x1a2a2a, 0.15f, 0);
  m_cardboard = mat(0xa0773f, 0.95f, 0);
  m_sawdust = mat(0xb08a52, 1.0f, 0);
  m_puddle = mat(0x5e93c4, 0.05f, 0);
  m_strip = mat(0x14110d, 0.5f, 0);
  m_glow = emis(0xff3b2a, 3);
  m_glow->roughness = 0.5f;
  m_skin = mat(0xc89a6e, 0.8f, 0);
  m_visor = mat(0x14110d, 0.2f, 0.3f);
  m_pcb = mat(0x2f8562, 0.5f, 0);
  m_led = emis(0x7fd0aa, 2);
  m_char = mat(C_CHAR, 0.7f, 0.2f);
  m_steel = mat(C_STEEL, 0.9f, 0);
  m_steel_hi = mat(C_STEEL_HI, 0.6f, 0.4f);
  m_bone = mat(C_BONE, 0.7f, 0);
  m_rust = mat(C_RUST, 0.8f, 0);
  m_violet = mat(0x6a4fa0, 0.9f, 0);
  m_info = mat(C_INFO, 0.5f, 0.2f);
  m_plate = mat(0xf4f1ea, 0.3f, 0);
  m_wood = mat(0x8a6a44, 0.85f, 0);
  /* section 4.4: the shared colours of the storefront */
  m_green = mat(0x5f8a73, 0.6f, 0.3f);
  m_red = mat(0xc23b2a, 0.6f, 0.2f);
  m_orange = mat(0xe07a2a, 0.6f, 0);
  m_blue = mat(0x2f4f8a, 0.6f, 0.2f);
  m_cord = mat(0xd9b43a, 0.7f, 0);
  m_bulb = emis(0xfff0d0, 2.5f);
  m_door = mat(0x3f7a4a, 0.7f, 0.1f);
  m_white = mat(0xe8e2d6, 0.6f, 0);
  m_tvs = emis(0x8fbde6, 1.4f);
  m_grey = mat(0x8b8578, 0.75f, 0);
  m_scr_g = emis(0x7fd0aa, 1.6f); m_scr_b = emis(0x8fbde6, 1.6f); m_scr_a = emis(C_HAZARD, 1.6f);
  m_woodtop = mat(0xa57c4f, 0.75f, 0);
  m_tube = emis(0xfff6e0, 2.2f);
  m_tube->roughness = 0.3f;
  m_ouch = mat(0xe0602a, 0.5f, 0.2f);
  m_strange = mat(0x9b7fd0, 0.5f, 0.2f);

  /* the six lights, made once: 1 hemisphere, 1 directional with the only shadow, 4 point */
  hemi = t3_hemisphere_light_new(0xfff2dc, 0x5a4a3a, 1.4f);
  t3_object_add(&SC->base, hemi);
  sun = t3_directional_light_new(0xffe2b8, 2.3f);
  t3_object_set_position(sun, -6, 16, 9);
  sun->base.cast_shadow = true;
  sun->shadow->map_width = sun->shadow->map_height = 2048;
  t3_camera *sc = sun->shadow->camera;
  sc->left = -13.5f; sc->right = 13.5f; sc->top = 8.5f; sc->bottom = -8.5f; sc->near = 1; sc->far = 40;
  t3_camera_update_projection_matrix(sc);
  sun->shadow->bias = -0.0003f;
  t3_object_add(&SC->base, sun);
  kitchen_lamp = t3_point_light_new(0xffc07a, 11.0f, 8.0f, 2.0f);
  t3_object_add(&SC->base, kitchen_lamp);
  bench_lamp = t3_point_light_new(0x8fbde6, 10.0f, 8.0f, 2.0f);
  t3_object_add(&SC->base, bench_lamp);
  laser_glow = t3_point_light_new(0xff3b2a, 0.0f, 6.0f, 2.0f);
  t3_object_add(&SC->base, laser_glow);
  hall_lamp = t3_point_light_new(0xffd9a8, 10.0f, 9.0f, 2.0f);
  t3_object_add(&SC->base, hall_lamp);

  build_all(L);

  world_cam = t3_perspective_camera_new(39, 1280.f / 720.f, 0.5f, 80);
  t3_object_set_position(world_cam, 0, 19.5f, 13.0f);
  t3_object_look_at(world_cam, 0, 0, 0.9f);
}

/* ── dynamic objects ─────────────────────────────────────────────── */

t3_object *world_item_obj(int type, int kind) {
  t3_object *g = t3_group_new();
  switch (type) {
    case IT_PLYWOOD: BOX(g, m_plywood, 1.0f, 0.06f, 0.7f, 0, 0.03f, 0); break;
    case IT_PROJECT:
      if (kind == 0) { BOX(g, m_amber, 0.5f, 0.45f, 0.5f, 0, 0.225f, 0); BOX(g, m_char, 0.3f, 0.1f, 0.3f, 0, 0.5f, 0); }
      else if (kind == 1) part(g, g_knot, m_amber, 0.2f, 0.2f, 0.2f, 0, 0.28f, 0);
      else if (kind == 2) { BOX(g, m_pcb, 0.5f, 0.05f, 0.4f, 0, 0.1f, 0); part(g, g_sphere, m_led, 0.08f, 0.08f, 0.08f, 0, 0.18f, 0); }
      else if (kind == 4) { BOX(g, m_violet, 0.5f, 0.12f, 0.4f, 0, 0.07f, 0); BOX(g, m_bone, 0.06f, 0.13f, 0.42f, 0.1f, 0.075f, 0); }
      else if (kind == 5) part(g, g_knot, m_rust, 0.22f, 0.22f, 0.22f, 0, 0.3f, 0);   /* every forge project is a hook */
      else BOX(g, m_amber, 0.4f, 0.03f, 0.4f, 0, 0.1f, 0);
      break;
    case IT_SCRAP: {
      t3_mesh *a = BOX(g, m_scrap, 0.45f, 0.05f, 0.3f, 0, 0.03f, 0);
      t3_object_set_rotation(a, 0, 0.6f, 0);
      BOX(g, m_scrap, 0.25f, 0.05f, 0.2f, 0.15f, 0.08f, 0.1f);
      break;
    }
    case IT_BOTTLE:
      part(g, g_cyl, m_bottle, 0.09f, 0.4f, 0.09f, 0, 0.2f, 0);
      part(g, g_cyl, m_label, 0.095f, 0.12f, 0.095f, 0, 0.2f, 0);
      break;
    case IT_SPAGHETTI: part(g, g_knot, m_spaghetti, 0.22f, 0.08f, 0.22f, 0, 0.1f, 0); break;
    case IT_DONATION:
      if (kind) { BOX(g, m_crt, 0.8f, 0.7f, 0.8f, 0, 0.35f, 0); BOX(g, m_screen, 0.6f, 0.5f, 0.05f, 0, 0.38f, 0.41f); }
      else BOX(g, m_cardboard, 0.6f, 0.45f, 0.5f, 0, 0.225f, 0);
      break;
    case IT_MOVEBOX:
      BOX(g, m_cardboard, 0.6f, 0.5f, 0.5f, 0, 0.25f, 0);
      noshadow(BOX(g, m_bone, 0.2f, 0.1f, 0.01f, 0, 0.3f, 0.255f));
      break;
    case IT_SAWDUST:
      part(g, g_disc, m_sawdust, 0.6f, 0.03f, 0.5f, 0, 0.02f, 0);
      break;
    case IT_PUDDLE: part(g, g_disc, m_puddle, 0.7f, 0.02f, 0.55f, 0, 0.01f, 0); break;
    case IT_DAISY:
      for (int i = 0; i < 3; i++) {
        t3_mesh *st = BOX(g, m_strip, 0.55f, 0.08f, 0.16f, -0.5f + i * 0.5f, 0.04f, (i & 1) * 0.2f);
        t3_object_set_rotation(st, 0, (i & 1) ? 0.4f : -0.3f, 0);
      }
      part(g, g_sphere, m_glow, 0.07f, 0.07f, 0.07f, 0.55f, 0.1f, 0);
      break;
  }
  t3_object_add(&SC->base, g);
  t3_release(g);
  return g;
}

t3_object *world_person(uint32_t color, bool newbie, t3_mesh **body) {
  t3_object *g = t3_group_new();
  t3_material *bm = mat(color, 0.75f, 0);
  t3_mesh *b = part(g, g_cyl, bm, 0.32f, 0.75f, 0.32f, 0, 0.55f, 0);
  part(g, g_sphere, bm, 0.32f, 0.16f, 0.32f, 0, 0.92f, 0);
  part(g, g_sphere, m_skin, 0.24f, 0.24f, 0.24f, 0, 1.22f, 0);
  BOX(g, m_visor, 0.36f, 0.09f, 0.12f, 0, 1.26f, 0.19f);
  if (!newbie) {
    part(g, g_sphere, bm, 0.26f, 0.12f, 0.26f, 0, 1.4f, 0);   /* a cap, in the player colour */
    BOX(g, bm, 0.3f, 0.04f, 0.22f, 0, 1.4f, 0.2f);
  }
  /* legs then arms, last in the group so world_sync can swing them */
  part(g, g_cyl, m_char, 0.09f, 0.35f, 0.09f, -0.13f, 0.17f, 0);
  part(g, g_cyl, m_char, 0.09f, 0.35f, 0.09f, 0.13f, 0.17f, 0);
  for (int side = -1; side <= 1; side += 2) {   /* shoulder pivots, the arm hanging below */
    t3_object *sh = t3_group_new();
    t3_object_set_position(sh, side * 0.38f, 0.95f, 0);
    t3_object_add(g, sh);
    t3_release(sh);
    part(sh, g_cyl, bm, 0.07f, 0.42f, 0.07f, 0, -0.21f, 0);
    part(sh, g_sphere, m_skin, 0.08f, 0.08f, 0.08f, 0, -0.44f, 0);
  }
  if (!newbie) {
    /* a ring on the floor in the player's colour: you can always find yourself */
    t3_material *rm = mat(color, 0.4f, 0);
    rm->emissive = t3_color_hex(color); rm->emissive_intensity = 0.9f;
    t3_mesh *ring = part(g, g_disc, rm, 0.62f, 0.04f, 0.62f, 0, 0.025f, 0);
    ring->base.cast_shadow = false;
    t3_mesh *hole = part(g, g_disc, m_char, 0.46f, 0.042f, 0.46f, 0, 0.026f, 0);
    hole->base.cast_shadow = false;
    t3_release(rm);
  }
  t3_release(bm);
  t3_object_set_scale(g, 1.25f, 1.25f, 1.25f);
  if (body) *body = b;
  t3_object_add(&SC->base, g);
  t3_release(g);
  return g;
}

void world_free(t3_object *o) {
  if (o) t3_object_remove(&SC->base, o);
}

static void glow(t3_mesh *m, uint32_t c, float k) {
  if (!m) return;
  t3_material *mt = m->materials[0];
  mt->color = t3_color_hex(c);
  mt->emissive = t3_color_hex(c);
  mt->emissive_intensity = k;
}

void world_sync(float dt) {
  static float t;
  t += dt;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active || !p->obj) continue;
    float hop = fabsf(sinf(p->bob)) * 0.12f;
    t3_object_set_position(p->obj, p->x, hop, p->z);
    t3_object_set_rotation(p->obj, 0, p->face, 0);
    /* children: ... legs (n-6, n-5), arms (n-4, n-3), ring, ring hole */
    int n = p->obj->child_count;
    if (n >= 6) {
      float sw = sinf(p->bob) * 0.6f;
      t3_object_set_rotation(p->obj->children[n - 6], sw, 0, 0);
      t3_object_set_rotation(p->obj->children[n - 5], -sw, 0, 0);
      bool carry = p->hold >= 0, work = p->work > 0;
      float arm = carry ? -2.6f : work ? -1.2f + sinf(p->work * 20) * 0.4f : -sw;
      t3_object_set_rotation(p->obj->children[n - 4], arm, 0, carry ? 0.25f : 0);
      t3_object_set_rotation(p->obj->children[n - 3], carry ? arm : sw, 0, carry ? -0.25f : 0);
    }
  }
  for (int i = 0; i < MAX_NPCS; i++) {
    npc_t *n = &N[i];
    if (!n->alive || !n->obj) continue;
    float hop = fabsf(sinf(n->bob)) * 0.1f;
    t3_object_set_position(n->obj, n->x, hop, n->z);
    float dx = n->tx - n->x, dz = n->tz - n->z;
    if (dx * dx + dz * dz > 0.01f) t3_object_set_rotation(n->obj, 0, atan2f(dx, dz), 0);
  }
  for (int i = 0; i < MAX_ITEMS; i++) {
    item_t *it = &I[i];
    if (!it->alive || !it->obj) continue;
    float y = it->y;
    float spin = 0;
    if (it->holder >= 0) { y = 1.75f; spin = t * 2; }
    t3_object_set_position(it->obj, it->x, y, it->z);
    t3_object_set_rotation(it->obj, 0, spin + it->name * 0.7f, 0);
  }
  /* the ceiling fans */
  for (int i = 0; i < fan_n; i++) t3_object_set_rotation(fans[i], 0, t * 1.2f, 0);
  /* laser */
  station_t *la = &S[ST_LASER];
  bool cutting = G.laser_cutting > 0;
  laser_glow->intensity = cutting ? 10.0f + 6.0f * sinf(t * 40) : L->light.laser_idle;
  la->beam->base.visible = cutting;
  if (cutting) t3_object_set_position(la->beam, sinf(t * 7) * 0.9f, 1.2f, cosf(t * 5) * 0.5f);
  glow(la->light, la->state == S_BROKEN ? C_FAULT : la->state == S_DONE ? C_HAZARD : C_LIVE, la->state == S_BROKEN ? 2.5f * (0.6f + 0.4f * sinf(t * 10)) : 1.5f);
  /* printers */
  for (int k = 0; k < 2; k++) {
    station_t *s = &S[ST_PRINT1 + k];
    s->growth->base.visible = s->state == S_BUSY || s->state == S_DONE;
    if (s->beam) s->beam->base.visible = s->state == S_BROKEN;   /* the OUT OF ORDER tag */
    float g = s->state == S_DONE ? 1.f : 1.f - s->timer / 14.f;
    if (g < 0.05f) g = 0.05f;
    t3_object_set_scale(s->growth, 0.18f * g + 0.04f, 0.18f * g + 0.04f, 0.18f * g + 0.04f);
    t3_object_set_rotation(s->growth, 0, t, 0);
    glow(s->light, s->state == S_BROKEN ? C_FAULT : s->state == S_DONE ? C_HAZARD : s->state == S_BUSY ? C_INFO : C_LIVE,
         s->state == S_BROKEN ? 2.5f * (0.6f + 0.4f * sinf(t * 10 + k)) : 1.5f);
  }
  /* sink: one plate per dirty dish */
  t3_object *sink = S[ST_SINK].obj;
  int n = 0;
  for (int i = 0; i < sink->child_count; i++) {
    t3_object *c = sink->children[i];
    t3_mesh *m = (t3_mesh *)c;
    if (m->materials[0] == m_plate) { c->visible = n < S[ST_SINK].count; n++; }
  }
  /* fridge smell: a pulsing green bulb */
  station_t *F = &S[ST_FRIDGE];
  float sm = F->smell ? 0.22f + 0.05f * sinf(t * 6) : 0;
  t3_object_set_scale(F->light, sm, sm, sm);
  glow(F->light, 0x7fb800, 2);
  /* vending: the lit front, flickering fault red while it has eaten a dollar */
  station_t *vd = &S[ST_VEND];
  if (vd->state == S_BROKEN) glow(vd->light, C_FAULT, fmodf(t, 0.17f) < 0.08f ? 1.6f : 0.2f);
  else glow(vd->light, 0x8fbde6, 1.2f);
  /* whiteboard: one marker line per idea, the corner until somebody wipes it */
  for (int k = 0; k < 8; k++) if (board_line[k]) board_line[k]->base.visible = k < S[ST_BOARD].count;
  for (int k = 0; k < 2; k++) if (board_corner[k]) board_corner[k]->base.visible = S[ST_BOARD].dust > 0;
  /* sewing: the needle lamp brightens while somebody sews */
  glow(S[ST_SEWING].light, 0xfff0d0, G.sewing > 0 ? 2.0f : 0.4f);
  /* bathroom: the tube over the door is the light somebody left on */
  glow(S[ST_BATH].light, 0xfff6e0, S[ST_BATH].count ? 2.5f : 0);
  /* member storage: one box per count on the shelf, the spilled one on the floor for a while */
  station_t *so = &S[ST_STORAGE];
  for (int k = 0; k < 6; k++) if (storage_box[k]) storage_box[k]->base.visible = k < so->count;
  if (storage_misc) storage_misc->base.visible = so->count >= 6;
  if (storage_tilt) storage_tilt->base.visible = so->timer > 0;
  /* the forge lamp: heat, and a flicker while somebody welds */
  station_t *fo = &S[ST_FORGE];
  glow(fo->light, 0xffb36b, fo->dust / 25 + (G.welding > 0 ? 2 + sinf(t * 31) : 0));
  /* books out */
  for (int k = 0; k < 4; k++) if (lib_book[k]) lib_book[k]->base.visible = k < S[ST_LIBRARY].count;
  /* the back door swings into the room while it is blown open; the alley light comes with it */
  static float door_ang;
  station_t *bk = &S[ST_BACKDOOR];
  float want = bk->state == S_BROKEN ? 1.2f : 0, step = 6 * dt;
  door_ang += fmaxf(-step, fminf(step, want - door_ang));
  if (door_hinge) t3_object_set_rotation(door_hinge, 0, door_ang, 0);
  if (door_ang > 0.02f) glow(bk->light, 0xffd9a8, 1.5f * door_ang / 1.2f); else glow(bk->light, C_GRIME, 0);
  /* the switch flips with the lights */
  if (switch_toggle) t3_object_set_rotation(switch_toggle, 0, 0, G.lights_on ? -0.5f : 0.5f);
  bool dark = game_lights_dark();
  sun->intensity = G.lights_on ? (dark ? 1.8f : 1.2f) : L->light.sun_i;
  m_bulb->emissive_intensity = G.lights_on && dark ? 2.5f : L->light.pendant_e;
  /* the flat on the stand */
  if (bike_hung) bike_hung->visible = S[ST_BIKE].state == S_BROKEN;
  /* the rack: a bike per newbie who rode in; greeted, it takes their colour */
  for (int k = 0; k < rack_n; k++) {
    const npc_t *rider = NULL;
    for (int i = 0; i < MAX_NPCS; i++) if (N[i].alive && N[i].bike == k) rider = &N[i];
    rack_bike[k]->visible = rider != NULL;
    if (rider) rack_frame[k]->color = rider->state == 2 && rider->body ? rider->body->materials[0]->color : t3_color_hex(0x8b8578);
  }
  /* visitors bob where they stand */
  for (int k = 0; k < visitor_n; k++) {
    float x = visitor[k]->position.x, z = visitor[k]->position.z;
    t3_object_set_position(visitor[k], x, fabsf(sinf(t * 2.2f + k * 1.7f)) * 0.06f, z);
  }
  /* show window pedestal: the last thing shown, spinning */
  station_t *sh = &S[ST_SHOW];
  sh->growth->base.visible = sh->timer > 0;
  t3_object_set_rotation(sh->growth, t, t * 1.3f, 0);
}
