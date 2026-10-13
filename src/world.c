/* The 3D side: a low-poly HeatSync on a Tuesday night. 108 W Main, cut away
 * like a dollhouse so the shared screen sees the whole floor at once. */
#include "game.h"
#include "hud.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

t3_camera *world_cam;
static t3_scene *SC;
static t3_light *laser_glow, *kitchen_lamp, *bench_lamp;

/* shared geometries and materials: meshes that share both are drawn as one
 * instanced draw by three.c */
static t3_geometry *g_box, *g_cyl, *g_sphere, *g_knot, *g_disc, *g_cone;
static t3_material *m_plywood, *m_amber, *m_scrap, *m_bottle, *m_label, *m_spaghetti, *m_crt, *m_screen,
                   *m_cardboard, *m_sawdust, *m_puddle, *m_strip, *m_glow, *m_skin, *m_visor, *m_pcb, *m_led,
                   *m_char, *m_steel, *m_steel_hi, *m_bone, *m_rust, *m_violet, *m_info,
                   *m_plate, *m_wood, *m_dumpster, *m_fridge, *m_mate;

static t3_material *mat(uint32_t c, float rough, float metal) {
  t3_material *m = t3_mesh_standard_material_new(c);
  m->roughness = rough;
  m->metalness = metal;
  return m;
}

static t3_mesh *part(void *parent, t3_geometry *g, t3_material *m, float sx, float sy, float sz, float x, float y, float z) {
  t3_mesh *me = t3_mesh_new(g, m);
  t3_object_set_scale(me, sx, sy, sz);
  t3_object_set_position(me, x, y, z);
  me->base.cast_shadow = true;
  me->base.receive_shadow = true;
  t3_object_add(parent, me);
  t3_release(me);
  return me;
}
#define BOX(p, m, w, h, d, x, y, z) part(p, g_box, m, w, h, d, x, y, z)
#define CYL(p, m, r, h, x, y, z) part(p, g_cyl, m, r, h, r, x, y, z)

static t3_object *group_at(void *parent, float x, float z) {
  t3_object *g = t3_group_new();
  t3_object_set_position(g, x, 0, z);
  t3_object_add(parent, g);
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
  return t;
}

static t3_mesh *sign(t3_texture *t, float w, float h, float x, float y, float z, float ry) {
  t3_material *m = t3_mesh_basic_material_new(0xffffff);
  t3_material_set_map(m, t);
  t3_mesh *me = t3_mesh_new(t3_plane_geometry_new(w, h, 1, 1), m);
  t3_object_set_position(me, x, y, z);
  t3_object_set_rotation(me, 0, ry, 0);
  t3_object_add(&SC->base, me);
  t3_release(me); t3_release(m); t3_release(t);
  return me;
}

/* ── the room ─────────────────────────────────────────────────────
 * Floor and walls are textures painted once at startup with the HUD's own
 * routines: zones you can read from across the room, worn concrete, shadows
 * pooling where the floor meets the walls. */

static t3_light *stage_lamp;

/* the floor is 24.6 x 14.3 units; the texture is 1024 x 596 pixels */
#define FW 1024
#define FH 596
static float fx_(float x) { return (x + 12.3f) / 24.6f * FW; }
static float fz_(float z) { return (z + 7.15f) / 14.3f * FH; }
static uint32_t lcg = 12345;
static int noise8(void) { lcg = lcg * 1664525u + 1013904223u; return (int)(lcg >> 24) - 128; }

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

static t3_texture *paint_floor(void) {
  hud_clear();
  /* base: sealed concrete with a slow mottle and grain */
  for (int y = 0; y < FH; y++) {
    uint8_t *row = hud_px + ((size_t)(HUD_H - 1 - y) * HUD_W) * 4;
    for (int x = 0; x < FW; x++) {
      float m = sinf(x * 0.013f) * sinf(y * 0.017f + 1.3f) * 9 + sinf(x * 0.041f + y * 0.023f) * 4;
      int g = (int)m + noise8() / 18;
      row[x * 4 + 0] = (uint8_t)(78 + g); row[x * 4 + 1] = (uint8_t)(70 + g); row[x * 4 + 2] = (uint8_t)(60 + g); row[x * 4 + 3] = 255;
    }
  }
  /* saw-cut joints every three units */
  for (float x = -12.3f + 3; x < 12.3f; x += 3) hud_rect((int)fx_(x), 0, 2, FH, 0x2a241e, 160);
  for (float z = -7.15f + 3; z < 7.15f; z += 3) hud_rect(0, (int)fz_(z), FW, 2, 0x2a241e, 160);

  /* wood shop: an OSB floor with board seams, and sawdust drifts */
  floor_rect(-12.3f, -7.15f, -6.4f, -2.6f, 0x6e5536, 255);
  for (float z = -7.15f; z < -2.6f; z += 0.6f) hud_rect(0, (int)fz_(z), (int)fx_(-6.4f), 1, 0x4a3822, 200);
  for (int i = 0; i < 2400; i++) {
    int x = (int)fx_(-12.3f) + (int)((noise8() + 128) / 256.f * (fx_(-6.4f) - fx_(-12.3f)));
    int y = (int)((noise8() + 128) / 256.f * fz_(-2.6f));
    hud_rect(x, y, 2, 1, 0xd8b47a, 90);
  }
  /* kitchen: a retro checkerboard, cream and char */
  for (float z = -0.8f; z < 7.15f; z += 0.6f) for (float x = -12.3f; x < -6.8f; x += 0.6f) {
    bool odd = ((int)((x + 12.3f) / 0.6f + (z + 0.8f) / 0.6f)) & 1;
    floor_rect(x, z, fminf(x + 0.6f, -6.8f), fminf(z + 0.6f, 7.15f), odd ? 0xcfc5b0 : 0x2a241d, 255);
  }
  /* metal shop: diamond plate */
  floor_rect(9.6f, -7.15f, 12.3f, -1.6f, 0x5d5a55, 255);
  for (int y = 0; y < (int)fz_(-1.6f); y += 8) for (int x = (int)fx_(9.6f) + (y / 8 % 2) * 6; x < FW; x += 12) {
    hud_rect(x, y, 5, 2, 0x8a8780, 255);
    hud_rect(x + 1, y + 2, 5, 1, 0x3a3834, 255);
  }
  /* electronics: an ESD mat under the bench */
  floor_rect(4.9f, -7.15f, 9.5f, -4.8f, 0x3d5566, 255);
  /* the laser's footprint: a rubber mat inside hazard tape */
  floor_rect(-6.6f, -7.15f, -2.6f, -4.6f, 0x24211e, 255);
  hazard_border(-6.6f, -7.15f, -2.6f, -4.6f, 0.22f);
  /* the lounge rug */
  floor_rect(-7.0f, 2.5f, -2.2f, 5.4f, 0x4a2f5c, 255);
  hud_frame((int)fx_(-6.8f), (int)fz_(2.7f), (int)(fx_(-2.4f) - fx_(-6.8f)), (int)(fz_(5.2f) - fz_(2.7f)), 3, 0xc98714, 200);
  for (float x = -6.6f; x < -2.4f; x += 0.4f) floor_rect(x, 2.75f, x + 0.2f, 2.85f, 0x9b7fd0, 160);
  /* show and tell: a painted stage in front of the window */
  hud_frame((int)fx_(-0.6f), (int)fz_(5.4f), (int)(fx_(4.2f) - fx_(-0.6f)), (int)(fz_(7.15f) - fz_(5.4f)), 4, C_HAZARD, 255);
  /* the door mat */
  floor_rect(-9.0f, 5.9f, -7.0f, 7.05f, 0x1d1915, 255);
  stencil(-8.0f, 6.45f, "108 W MAIN", F_MONO_S, C_ASH, 255);
  /* the walkway in from the door: dashed lines */
  for (float z = -0.5f; z < 5.8f; z += 0.5f) {
    floor_rect(-6.55f, z, -6.45f, z + 0.28f, 0xc98714, 170);
  }
  /* oil stains and coffee rings: the space has history */
  for (int i = 0; i < 14; i++) {
    int x = (noise8() + 128) * FW / 256, y = (noise8() + 128) * FH / 256;
    hud_circle(x, y, 6 + (noise8() + 128) / 20, 0x1a1612, 40);
  }
  /* zone names, stencilled on the floor */
  stencil(-9.3f, -2.95f, "WOOD SHOP", F_H3, C_BONE, 70);
  stencil(-4.6f, -4.35f, "LASER. EYES ON IT.", F_MONO_S, C_HAZARD, 200);
  stencil(1.2f, -5.35f, "3D PRINTING", F_H3, C_BONE, 60);
  stencil(7.2f, -4.45f, "ELECTRONICS", F_H3, C_BONE, 70);
  stencil(10.95f, -1.25f, "METAL SHOP", F_MONO_S, C_BONE, 140);
  stencil(-9.6f, -0.45f, "KITCHEN", F_H3, C_TAPE, 110);
  stencil(-4.6f, 2.25f, "LOUNGE", F_H3, C_BONE, 60);
  stencil(1.8f, 5.6f, "SHOW & TELL", F_H3, C_HAZARD, 220);
  stencil(10.4f, 4.4f, "FREE PILE: TAKE ANYTHING", F_MONO_S, C_BONE, 140);
  /* shadows pooling where the floor meets the walls */
  for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) {
    float wx = x * 24.6f / FW - 12.3f, wz = y * 14.3f / FH - 7.15f;
    float d = fminf(fminf(wx + 12.3f, 12.3f - wx), wz + 7.15f);
    if (d < 0.9f) shade_px(x, y, -(int)((0.9f - d) / 0.9f * 46));
  }
  return hud_texture(FW, FH);
}

/* brick, with a char wainscot and an amber pinstripe: GANTRY on a wall */
static t3_texture *paint_brick(int w, int h, float units_w) {
  hud_clear();
  float ppu = w / units_w;
  int bh = (int)(0.21f * ppu), bw = (int)(0.5f * ppu);
  if (bh < 4) bh = 4;
  if (bw < 8) bw = 8;
  hud_rect(0, 0, w, h, 0x3d342d, 255);
  for (int row = 0, y = 0; y < h; row++, y += bh) {
    for (int x = -(row & 1) * bw / 2; x < w; x += bw) {
      int v = noise8() / 10;
      uint32_t c = ((0x74 + v) << 16) | ((0x40 + v / 2) << 8) | (0x30 + v / 2);
      if ((noise8() & 15) == 0) c = 0x5c3a2c;
      hud_rect(x + 1, y + 1, bw - 2, bh - 2, c, 255);
    }
  }
  /* a dark wainscot along the bottom, with the pinstripe */
  int wy = (int)(h * (1.0f - 0.9f / 3.4f));
  hud_rect(0, wy, w, h - wy, C_CHAR, 255);
  hud_rect(0, wy, w, 3, C_HAZARD, 255);
  /* darker toward the floor, and up near the ceiling */
  for (int y = 0; y < h; y++) {
    float t = (float)y / h;
    int d = t < 0.15f ? -(int)((0.15f - t) / 0.15f * 30) : 0;
    if (d) for (int x = 0; x < w; x++) shade_px(x, y, d);
  }
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
  t3_release(me); t3_release(m); t3_release(t);
  return me;
}

static void build_room(void) {
  lit_plane(paint_floor(), 24.6f, 14.3f, 0, 0, 0, -1.5707963f, 0);
  /* walls: brick on the inside, cut away at the front */
  BOX(&SC->base, m_steel, 24.6f, 3.4f, 0.3f, 0, 1.7f, -7.15f);
  BOX(&SC->base, m_steel, 0.3f, 3.4f, 14.3f, -12.15f, 1.7f, 0);
  BOX(&SC->base, m_steel, 0.3f, 3.4f, 14.3f, 12.15f, 1.7f, 0);
  lit_plane(paint_brick(1024, 142, 24.6f), 24.6f, 3.4f, 0, 1.7f, -6.995f, 0, 0);
  lit_plane(paint_brick(1024, 244, 14.3f), 14.3f, 3.4f, -11.995f, 1.7f, 0, 0, 1.5707963f);
  lit_plane(paint_brick(1024, 244, 14.3f), 14.3f, 3.4f, 11.995f, 1.7f, 0, 0, -1.5707963f);
  BOX(&SC->base, m_steel_hi, 24.6f, 0.12f, 0.36f, 0, 3.4f, -7.15f);
  /* front kick wall stubs, with a gap for the door at the left */
  BOX(&SC->base, m_steel, 3.0f, 0.5f, 0.25f, -10.5f, 0.25f, 7.1f);
  BOX(&SC->base, m_steel, 15.0f, 0.5f, 0.25f, 4.5f, 0.25f, 7.1f);
  /* the alley door, steel and propped open a crack */
  BOX(&SC->base, m_steel_hi, 0.1f, 2.5f, 1.5f, 11.95f, 1.25f, 0.2f);
  BOX(&SC->base, m_rust, 0.08f, 2.3f, 1.3f, 11.9f, 1.15f, 0.2f);
  BOX(&SC->base, m_char, 0.1f, 0.06f, 0.25f, 11.82f, 1.1f, -0.25f);
  /* the front door: a frame and a glass door, with PUSH on it */
  BOX(&SC->base, m_steel_hi, 0.2f, 2.6f, 0.2f, -8.9f, 1.3f, 7.1f);
  BOX(&SC->base, m_steel_hi, 0.2f, 2.6f, 0.2f, -7.1f, 1.3f, 7.1f);
  BOX(&SC->base, m_steel_hi, 2.0f, 0.2f, 0.2f, -8.0f, 2.6f, 7.1f);
  /* fluorescent strip lights high on the walls */
  t3_material *tube = mat(0xfff6e0, 0.3f, 0);
  tube->emissive = t3_color_hex(0xfff6e0); tube->emissive_intensity = 2.2f;
  for (int i = 0; i < 4; i++) {
    float x = -9.0f + i * 6.0f;
    BOX(&SC->base, m_steel_hi, 3.0f, 0.12f, 0.2f, x, 3.05f, -6.88f);
    t3_mesh *t = BOX(&SC->base, tube, 2.8f, 0.06f, 0.08f, x, 2.98f, -6.8f);
    t->base.cast_shadow = false;
  }
  for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 2; i++) {
    float z = -3.5f + i * 6.0f;
    BOX(&SC->base, m_steel_hi, 0.2f, 0.12f, 3.0f, s * 11.88f, 3.05f, z);
    t3_mesh *t = BOX(&SC->base, tube, 0.08f, 0.06f, 2.8f, s * 11.8f, 2.98f, z);
    t->base.cast_shadow = false;
  }
  t3_release(tube);
  /* the laser's exhaust duct, up to the roof */
  CYL(&SC->base, m_steel_hi, 0.26f, 1.9f, -5.6f, 2.45f, -6.5f);
  BOX(&SC->base, m_steel_hi, 0.4f, 0.3f, 0.6f, -5.6f, 1.45f, -6.35f);
}

/* ── the machines and furniture ───────────────────────────────────── */

static t3_material *emis(uint32_t c, float k) {
  t3_material *m = mat(c, 0.4f, 0);
  m->emissive = t3_color_hex(c);
  m->emissive_intensity = k;
  return m;
}

static t3_mesh *rot(t3_mesh *m, float rx, float ry, float rz) { t3_object_set_rotation(m, rx, ry, rz); return m; }

static void build_stations(void) {
  station_t *s;
  t3_object *g;
  t3_material *green = mat(0x5f8a73, 0.6f, 0.3f);    /* machine-tool green */
  t3_material *red = mat(0xc23b2a, 0.6f, 0.2f);
  t3_material *glass = mat(0x1d3a3a, 0.1f, 0.2f);
  t3_material *scr_g = emis(0x7fd0aa, 1.6f), *scr_b = emis(0x8fbde6, 1.6f), *scr_a = emis(C_HAZARD, 1.6f);
  t3_material *counter = mat(0x3a342b, 0.7f, 0.1f);
  t3_material *woodtop = mat(0xa57c4f, 0.75f, 0);

  /* lumber: a rack on the left wall, sheet goods and two-by-fours */
  s = &S[ST_LUMBER]; g = s->obj = group_at(&SC->base, s->x, s->z);
  for (int k = -1; k <= 1; k++) BOX(g, m_steel_hi, 0.1f, 2.4f, 0.1f, -0.45f, 1.2f, k * 1.35f);
  for (int k = 0; k < 3; k++) BOX(g, m_steel_hi, 0.6f, 0.06f, 2.9f, -0.2f, 0.6f + k * 0.7f, 0);
  for (int k = 0; k < 5; k++) rot(BOX(g, m_plywood, 0.05f, 1.9f, 2.6f, -0.05f + k * 0.08f, 0.95f, 0), 0, 0, -0.13f);
  for (int k = 0; k < 4; k++) BOX(g, m_wood, 0.5f, 0.09f, 0.09f, -0.2f, 1.36f + (k % 2) * 0.1f, -1.0f + k * 0.6f);
  BOX(g, m_cardboard, 0.5f, 0.4f, 0.5f, 0.15f, 0.2f, 1.1f);           /* the offcut bin */

  /* PEWPEW 9000: the big laser, lid, glass, panel, e-stop */
  s = &S[ST_LASER]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_char, 2.8f, 0.9f, 1.6f, 0, 0.45f, 0);
  BOX(g, m_steel_hi, 2.8f, 0.06f, 1.6f, 0, 0.93f, 0);
  BOX(g, m_amber, 2.5f, 0.1f, 1.3f, -0.1f, 1.02f, 0);
  rot(BOX(g, glass, 2.1f, 0.04f, 1.0f, -0.1f, 1.09f, 0), 0, 0, 0);
  BOX(g, m_char, 0.45f, 0.35f, 1.2f, 1.15f, 1.12f, 0);                 /* control panel */
  BOX(g, scr_a, 0.3f, 0.02f, 0.25f, 1.15f, 1.3f, 0.2f);
  CYL(g, red, 0.07f, 0.08f, 1.15f, 1.33f, -0.25f);                     /* e-stop */
  for (int k = 0; k < 5; k++) BOX(g, m_steel, 0.02f, 0.4f, 0.06f, -1.41f, 0.45f, -0.5f + k * 0.25f);   /* vents */
  s->beam = BOX(g, m_glow, 0.04f, 0.5f, 0.04f, 0, 1.2f, 0);
  s->beam->base.visible = false;
  s->light = BOX(g, mat(0x5fb892, 0.4f, 0), 0.14f, 0.14f, 0.05f, 1.15f, 0.7f, 0.81f);

  /* the printing counter: Loafer and Dr. Strange live on it, with Sir Cutsalot */
  t3_object *pc = group_at(&SC->base, 1.2f, -6.5f);
  BOX(pc, counter, 5.6f, 0.75f, 0.9f, 0, 0.375f, 0);
  BOX(pc, woodtop, 5.7f, 0.06f, 0.95f, 0, 0.78f, 0);
  static const uint32_t frames[2] = { 0xe0602a, 0x9b7fd0 };
  static const uint32_t spools[2] = { 0x2f8562, 0xf2ab1e };
  for (int k = 0; k < 2; k++) {
    s = &S[ST_PRINT1 + k]; g = s->obj = group_at(&SC->base, s->x, s->z);
    t3_material *fm = mat(frames[k], 0.5f, 0.2f), *sp = mat(spools[k], 0.5f, 0);
    BOX(g, m_char, 0.8f, 0.1f, 0.6f, 0, 0.86f, 0);                       /* base */
    BOX(g, mat(0xb8962e, 0.4f, 0.4f), 0.6f, 0.03f, 0.5f, 0, 0.93f, 0.02f); /* the bed */
    BOX(g, fm, 0.06f, 0.75f, 0.06f, -0.37f, 1.28f, -0.18f);
    BOX(g, fm, 0.06f, 0.75f, 0.06f, 0.37f, 1.28f, -0.18f);
    BOX(g, fm, 0.8f, 0.06f, 0.06f, 0, 1.65f, -0.18f);
    BOX(g, m_steel_hi, 0.76f, 0.04f, 0.04f, 0, 1.25f, -0.12f);           /* x gantry */
    BOX(g, m_char, 0.14f, 0.16f, 0.14f, 0.08f, 1.2f, -0.06f);            /* extruder */
    part(g, g_cyl, sp, 0.18f, 0.08f, 0.18f, 0, 1.82f, -0.18f);
    rot(part(g, g_cyl, sp, 0.18f, 0.08f, 0.18f, 0, 1.82f, -0.18f), 1.5707963f, 0, 0);
    BOX(g, k ? scr_b : scr_g, 0.16f, 0.08f, 0.02f, 0.25f, 0.88f, 0.31f); /* LCD */
    s->growth = part(g, g_knot, m_amber, 0.14f, 0.14f, 0.14f, 0, 1.03f, 0.02f);
    s->growth->base.visible = false;
    s->light = BOX(g, mat(0x5fb892, 0.4f, 0), 0.06f, 0.06f, 0.03f, -0.28f, 0.88f, 0.31f);
    t3_release(fm); t3_release(sp);
  }

  /* Sir Cutsalot, the vinyl cutter nobody fights over */
  s = &S[ST_CUTSALOT]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, mat(0xd8d2c4, 0.5f, 0.1f), 1.3f, 0.26f, 0.32f, 0, 0.94f, 0);
  rot(part(g, g_cyl, m_violet, 0.07f, 1.2f, 0.07f, 0, 0.92f, 0.24f), 0, 0, 1.5707963f);
  BOX(g, m_char, 0.1f, 0.12f, 0.12f, 0.2f, 1.1f, 0.05f);
  BOX(g, scr_b, 0.14f, 0.06f, 0.02f, -0.45f, 0.98f, 0.17f);

  /* the electronics bench: monitors, a scope, a soldering station, parts drawers, a lamp */
  s = &S[ST_BENCH]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, woodtop, 3.8f, 0.08f, 1.3f, 0, 0.86f, 0);
  BOX(g, mat(0x3d5566, 0.8f, 0), 3.2f, 0.01f, 0.9f, 0.1f, 0.905f, 0.1f);     /* ESD mat */
  for (int i = 0; i < 4; i++) BOX(g, m_steel_hi, 0.08f, 0.86f, 0.08f, i < 2 ? -1.8f : 1.8f, 0.43f, i & 1 ? -0.55f : 0.55f);
  for (int k = 0; k < 2; k++) {
    float mx = -1.2f + k * 0.9f;
    BOX(g, m_char, 0.08f, 0.35f, 0.08f, mx, 1.07f, -0.45f);
    BOX(g, m_char, 0.8f, 0.5f, 0.05f, mx, 1.45f, -0.45f);
    BOX(g, k ? scr_b : scr_g, 0.72f, 0.42f, 0.01f, mx, 1.45f, -0.42f);
  }
  BOX(g, m_info, 0.55f, 0.32f, 0.38f, 0.7f, 1.06f, -0.3f);                  /* the scope */
  BOX(g, scr_g, 0.3f, 0.2f, 0.01f, 0.62f, 1.08f, -0.105f);
  BOX(g, m_rust, 0.4f, 0.18f, 0.3f, 1.35f, 0.99f, -0.2f);                   /* soldering station */
  part(g, g_cyl, m_steel_hi, 0.05f, 0.18f, 0.05f, 1.55f, 1.0f, 0.05f);
  for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++)                      /* parts drawers */
    BOX(g, (r + c) & 1 ? m_info : m_rust, 0.12f, 0.08f, 0.2f, -1.7f + c * 0.13f, 0.96f + r * 0.09f, -0.4f);
  for (int i = 0; i < 6; i++) part(g, g_sphere, m_led, 0.05f, 0.05f, 0.05f, -0.2f + i * 0.12f, 0.93f, 0.35f);
  BOX(g, m_pcb, 0.5f, 0.03f, 0.35f, -0.3f, 0.92f, 0.2f);
  rot(BOX(g, m_steel_hi, 0.04f, 0.7f, 0.04f, 1.75f, 1.25f, -0.4f), 0, 0, 0.3f);   /* desk lamp */
  BOX(g, scr_a, 0.2f, 0.08f, 0.14f, 1.6f, 1.58f, -0.3f);

  /* the kitchen: sink cabinet, faucet, dish rack; the fridge with magnets */
  s = &S[ST_SINK]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_bone, 1.1f, 0.85f, 1.75f, 0.05f, 0.425f, 0);
  BOX(g, woodtop, 1.2f, 0.06f, 1.85f, 0.05f, 0.88f, 0);
  BOX(g, m_steel_hi, 0.7f, 0.04f, 0.8f, 0.1f, 0.9f, -0.2f);
  BOX(g, m_char, 0.6f, 0.03f, 0.7f, 0.1f, 0.915f, -0.2f);
  CYL(g, m_steel_hi, 0.03f, 0.35f, -0.25f, 1.08f, -0.2f);
  BOX(g, m_steel_hi, 0.25f, 0.04f, 0.04f, -0.13f, 1.25f, -0.2f);
  for (int k = 0; k < 2; k++) BOX(g, m_steel, 0.01f, 0.7f, 0.8f, 0.61f, 0.42f, -0.43f + k * 0.86f);   /* cabinet doors */
  for (int i = 0; i < 10; i++) {
    t3_mesh *pl = part(g, g_cyl, m_plate, 0.26f, 0.035f, 0.26f, 0.1f + (i & 1) * 0.03f, 0.98f + i * 0.05f, 0.45f + (i % 3) * 0.04f);
    pl->base.visible = false;
  }

  s = &S[ST_FRIDGE]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_fridge, 1.0f, 2.1f, 0.95f, 0, 1.05f, 0);
  BOX(g, m_steel, 0.02f, 0.03f, 0.9f, 0.51f, 1.45f, 0);
  BOX(g, m_steel_hi, 0.05f, 0.5f, 0.05f, 0.53f, 1.8f, 0.35f);
  BOX(g, m_steel_hi, 0.05f, 0.7f, 0.05f, 0.53f, 0.95f, 0.35f);
  static const uint32_t mags[5] = { 0xf2ab1e, 0xc2542c, 0x5e93c4, 0x5fb892, 0x9b7fd0 };
  for (int k = 0; k < 5; k++) {
    t3_material *mm = mat(mags[k], 0.6f, 0);
    BOX(g, mm, 0.02f, 0.1f, 0.12f, 0.52f, 1.0f + (k % 3) * 0.25f, -0.3f + k * 0.13f);
    t3_release(mm);
  }
  BOX(g, mat(0xe8c43a, 0.6f, 0), 0.5f, 0.25f, 0.4f, 0, 2.23f, 0);          /* a Club-Mate crate on top */
  for (int i = 0; i < 6; i++) part(g, g_cyl, m_mate, 0.05f, 0.18f, 0.05f, -0.15f + (i % 3) * 0.15f, 2.43f, -0.08f + (i / 3) * 0.16f);
  s->light = part(g, g_sphere, mat(0x5fb892, 0.5f, 0), 0.0f, 0.0f, 0.0f, 0, 2.7f, 0);

  /* trash: a dumpster by the alley door, lid up, and the recycling */
  s = &S[ST_TRASH]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_dumpster, 1.0f, 0.95f, 0.9f, 0.1f, 0.52f, 0);
  rot(BOX(g, m_char, 1.05f, 0.05f, 0.5f, 0.1f, 1.15f, -0.38f), -1.1f, 0, 0);
  for (int k = -1; k <= 1; k += 2) CYL(g, m_char, 0.07f, 0.06f, 0.1f + k * 0.38f, 0.05f, 0.4f);
  BOX(g, mat(0x3f74a8, 0.7f, 0), 0.4f, 0.75f, 0.4f, -0.1f, 0.375f, 0.52f);

  /* the Free Pile: shelves that gave up */
  s = &S[ST_FREEPILE]; g = s->obj = group_at(&SC->base, s->x, s->z);
  for (int lvl = 0; lvl < 3; lvl++) BOX(g, m_steel_hi, 2.2f, 0.05f, 0.6f, 0, 0.35f + lvl * 0.6f, -0.2f);
  for (int k = -1; k <= 1; k += 2) BOX(g, m_steel_hi, 0.05f, 1.6f, 0.6f, k * 1.08f, 0.8f, -0.2f);
  BOX(g, m_crt, 0.6f, 0.5f, 0.55f, -0.6f, 0.62f, -0.2f);
  BOX(g, m_screen, 0.46f, 0.36f, 0.02f, -0.6f, 0.64f, 0.08f);
  BOX(g, m_cardboard, 0.6f, 0.4f, 0.45f, 0.3f, 0.6f, -0.2f);
  BOX(g, m_cardboard, 0.5f, 0.3f, 0.45f, 0.5f, 1.12f, -0.2f);
  BOX(g, m_char, 0.7f, 0.25f, 0.4f, -0.5f, 1.08f, -0.2f);
  part(g, g_knot, m_char, 0.12f, 0.06f, 0.12f, -0.1f, 1.55f, -0.2f);        /* a tangle of cables */
  BOX(g, m_crt, 0.7f, 0.6f, 0.6f, 0.6f, 0.3f, 0.45f);                        /* overflow on the floor */
  rot(BOX(g, m_cardboard, 0.5f, 0.35f, 0.4f, -0.4f, 0.18f, 0.5f), 0, 0.4f, 0);

  /* Show & Tell: the front window display, pedestals, a trophy, a LEGO wall */
  s = &S[ST_SHOW]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_char, 4.4f, 0.35f, 1.0f, 0, 0.175f, 0);
  BOX(g, m_amber, 4.5f, 0.05f, 1.1f, 0, 0.37f, 0);
  for (int k = -1; k <= 1; k++) CYL(g, m_bone, 0.22f, 0.4f + (k == 0) * 0.2f, k * 1.4f, 0.6f + (k == 0) * 0.1f, -0.1f);
  part(g, g_cone, m_amber, 0.12f, 0.2f, 0.12f, -1.4f, 0.92f, -0.1f);          /* the trophy */
  part(g, g_sphere, m_amber, 0.1f, 0.1f, 0.1f, -1.4f, 1.08f, -0.1f);
  static const uint32_t lego[4] = { 0xc23b2a, 0xf2ab1e, 0x3f74a8, 0x2f8562 };
  for (int k = 0; k < 8; k++) {
    t3_material *lm = mat(lego[k % 4], 0.5f, 0);
    BOX(g, lm, 0.22f, 0.12f, 0.14f, 1.25f + (k % 3) * 0.12f - 0.1f, 0.86f + (k / 3) * 0.12f, -0.1f + (k & 1) * 0.04f);
    t3_release(lm);
  }
  for (int k = -1; k <= 1; k += 2) BOX(g, m_steel_hi, 0.08f, 1.6f, 0.08f, k * 2.2f, 0.8f, 0.45f);   /* the window frame */
  BOX(g, m_steel_hi, 4.48f, 0.08f, 0.08f, 0, 1.6f, 0.45f);
  s->growth = part(g, g_box, m_amber, 0.42f, 0.42f, 0.42f, 0, 1.15f, -0.1f);
  s->growth->base.visible = false;

  /* the lurker couch, cushions and all */
  s = &S[ST_COUCH]; g = s->obj = group_at(&SC->base, s->x, s->z);
  t3_material *fab = mat(0x6a4fa0, 0.95f, 0), *fab2 = mat(0x7d62b5, 0.95f, 0);
  BOX(g, fab, 3.2f, 0.35f, 0.9f, 0, 0.2f, 0);
  for (int k = 0; k < 3; k++) BOX(g, fab2, 0.98f, 0.14f, 0.7f, -1.0f + k * 1.0f, 0.44f, 0.05f);
  BOX(g, fab, 3.2f, 0.65f, 0.25f, 0, 0.6f, -0.38f);
  for (int k = -1; k <= 1; k += 2) BOX(g, fab, 0.25f, 0.55f, 0.9f, k * 1.5f, 0.45f, 0);
  rot(BOX(g, m_amber, 0.35f, 0.3f, 0.12f, -1.1f, 0.65f, -0.18f), 0.2f, 0.3f, 0);   /* a throw pillow */
  t3_release(fab); t3_release(fab2);

  /* the work island: butcher block, a vise, a toolbox, a project in progress */
  s = &S[ST_TABLE]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, woodtop, 3.2f, 0.14f, 1.6f, 0, 0.9f, 0);
  for (int i = 0; i < 4; i++) BOX(g, m_steel_hi, 0.1f, 0.84f, 0.1f, i < 2 ? -1.45f : 1.45f, 0.42f, i & 1 ? -0.68f : 0.68f);
  BOX(g, m_steel_hi, 2.9f, 0.05f, 1.3f, 0, 0.25f, 0);                       /* lower shelf */
  BOX(g, m_cardboard, 0.6f, 0.3f, 0.4f, -0.8f, 0.43f, 0.2f);
  BOX(g, green, 0.3f, 0.2f, 0.25f, 1.3f, 1.07f, 0.55f);                       /* bench vise */
  BOX(g, red, 0.6f, 0.28f, 0.3f, -1.0f, 1.11f, -0.4f);                        /* toolbox */
  part(g, g_knot, m_rust, 0.15f, 0.15f, 0.15f, 0.3f, 1.15f, -0.1f);
  BOX(g, m_info, 0.3f, 0.12f, 0.2f, -0.2f, 1.03f, 0.35f);
  t3_release(green); t3_release(red); t3_release(glass); t3_release(counter); t3_release(woodtop);
  t3_release(scr_g); t3_release(scr_b); t3_release(scr_a);
}

/* furniture that blocks the way but does nothing. Collision comes from the
 * level (L->ob in game.c); these models still stand where the old room put
 * them until build_furniture is rebuilt as a loop over L->ob by kind. */
typedef struct { float x, z; } old_obst_t;
enum { OB_SAW, OB_SHELF, OB_COFFEE, OB_KTABLE, OB_TV, OB_CTABLE, OB_ISLAND, OB_LATHE, OB_MILL, OB_COUNT };
static const old_obst_t OBST[OB_COUNT] = {
  [OB_SAW]    = { -8.0f, -5.7f },    /* table saw */
  [OB_SHELF]  = { -11.55f, -2.0f },  /* parts shelving */
  [OB_COFFEE] = { -11.15f, -0.2f },  /* coffee cabinet */
  [OB_KTABLE] = { -8.3f, 2.4f },     /* kitchen table */
  [OB_TV]     = { -4.6f, 6.25f },    /* the lounge TV */
  [OB_CTABLE] = { -2.75f, 3.55f },   /* side table */
  [OB_ISLAND] = { -2.0f, -1.8f },    /* assembly table */
  [OB_LATHE]  = { 11.15f, -5.2f },
  [OB_MILL]   = { 11.1f, -2.9f },
};

static void build_furniture(void) {
  t3_material *green = mat(0x5f8a73, 0.6f, 0.3f), *woodtop = mat(0xa57c4f, 0.75f, 0);
  t3_material *red = mat(0xc23b2a, 0.6f, 0.2f), *mug = mat(0xece3d3, 0.5f, 0), *blk = mat(0x14110d, 0.3f, 0.2f);
  t3_material *tvs = emis(0x8fbde6, 1.2f);
  const old_obst_t *o;
  t3_object *g;

  o = &OBST[OB_SAW]; g = group_at(&SC->base, o->x, o->z);                   /* the table saw */
  BOX(g, m_steel, 0.7f, 0.8f, 0.6f, 0, 0.4f, 0);
  BOX(g, m_steel_hi, 1.4f, 0.05f, 1.1f, 0, 0.83f, 0);
  rot(part(g, g_cyl, red, 0.18f, 0.02f, 0.18f, 0, 0.88f, 0), 1.5707963f, 0, 0);
  BOX(g, m_char, 0.06f, 0.08f, 1.1f, 0.35f, 0.9f, 0);
  BOX(g, m_amber, 0.2f, 0.1f, 0.12f, 0, 0.6f, 0.31f);

  o = &OBST[OB_SHELF]; g = group_at(&SC->base, o->x, o->z);                 /* parts shelving */
  t3_material *bins[4] = { mat(C_RUST, 0.7f, 0), mat(C_INFO, 0.7f, 0), mat(C_HAZARD, 0.6f, 0), mat(0x2f8562, 0.7f, 0) };
  for (int lvl = 0; lvl < 4; lvl++) BOX(g, m_steel_hi, 0.55f, 0.05f, 1.8f, 0, 0.3f + lvl * 0.55f, 0);
  for (int k = -1; k <= 1; k += 2) BOX(g, m_steel_hi, 0.55f, 2.0f, 0.05f, 0, 1.0f, k * 0.9f);
  for (int lvl = 0; lvl < 4; lvl++) for (int b = 0; b < 4; b++)
    BOX(g, bins[(lvl + b) & 3], 0.4f, 0.24f, 0.38f, 0.05f, 0.45f + lvl * 0.55f, -0.65f + b * 0.43f);
  for (int i = 0; i < 4; i++) t3_release(bins[i]);

  o = &OBST[OB_COFFEE]; g = group_at(&SC->base, o->x, o->z);                /* coffee and coworking */
  BOX(g, m_steel, 0.9f, 0.85f, 1.15f, 0.05f, 0.425f, 0);
  BOX(g, woodtop, 1.0f, 0.06f, 1.2f, 0.05f, 0.88f, 0);
  BOX(g, blk, 0.4f, 0.5f, 0.35f, 0, 1.16f, -0.25f);
  CYL(g, blk, 0.09f, 0.18f, 0.12f, 1.0f, -0.25f);
  for (int k = 0; k < 3; k++) CYL(g, mug, 0.05f, 0.1f, 0.2f, 0.96f, 0.15f + k * 0.13f);

  o = &OBST[OB_KTABLE]; g = group_at(&SC->base, o->x, o->z);                /* the kitchen table */
  CYL(g, woodtop, 0.65f, 0.06f, 0, 0.78f, 0);
  CYL(g, m_steel_hi, 0.06f, 0.75f, 0, 0.38f, 0);
  CYL(g, m_steel_hi, 0.3f, 0.04f, 0, 0.02f, 0);
  for (int k = 0; k < 3; k++) {
    float a = k * 2.094f + 0.5f;
    CYL(g, red, 0.17f, 0.06f, cosf(a) * 0.85f, 0.5f, sinf(a) * 0.55f);
    CYL(g, m_steel_hi, 0.03f, 0.48f, cosf(a) * 0.85f, 0.24f, sinf(a) * 0.55f);
  }
  part(g, g_cyl, m_mate, 0.05f, 0.18f, 0.05f, 0.15f, 0.9f, 0.1f);
  CYL(g, mug, 0.05f, 0.1f, -0.2f, 0.86f, -0.1f);

  o = &OBST[OB_TV]; g = group_at(&SC->base, o->x, o->z);                    /* the lounge TV */
  BOX(g, m_wood, 1.6f, 0.45f, 0.4f, 0, 0.225f, 0);
  BOX(g, blk, 1.5f, 0.85f, 0.08f, 0, 0.95f, -0.02f);
  BOX(g, tvs, 1.4f, 0.75f, 0.01f, 0, 0.95f, -0.065f);
  BOX(g, m_char, 0.3f, 0.12f, 0.25f, 0.5f, 0.51f, 0);                        /* a game console, of course */

  o = &OBST[OB_CTABLE]; g = group_at(&SC->base, o->x, o->z);                /* side table with a lamp */
  BOX(g, woodtop, 0.5f, 0.05f, 0.5f, 0, 0.55f, 0);
  BOX(g, m_char, 0.4f, 0.52f, 0.4f, 0, 0.26f, 0);
  CYL(g, m_steel_hi, 0.03f, 0.35f, 0.08f, 0.75f, 0.05f);
  part(g, g_cone, emis(0xffd27a, 0.8f), 0.15f, 0.18f, 0.15f, 0.08f, 0.98f, 0.05f);
  part(g, g_cyl, m_mate, 0.05f, 0.18f, 0.05f, -0.12f, 0.67f, -0.1f);

  o = &OBST[OB_ISLAND]; g = group_at(&SC->base, o->x, o->z);                /* the assembly table, with a robot on it */
  BOX(g, woodtop, 2.6f, 0.08f, 1.2f, 0, 0.86f, 0);
  for (int i = 0; i < 4; i++) BOX(g, m_steel_hi, 0.08f, 0.82f, 0.08f, i < 2 ? -1.2f : 1.2f, 0.41f, i & 1 ? -0.5f : 0.5f);
  BOX(g, m_steel, 0.5f, 0.35f, 0.4f, -0.5f, 1.08f, 0);                       /* robot body */
  part(g, g_sphere, m_steel_hi, 0.16f, 0.16f, 0.16f, -0.5f, 1.38f, 0);
  part(g, g_sphere, m_led, 0.04f, 0.04f, 0.04f, -0.44f, 1.4f, 0.14f);
  for (int k = -1; k <= 1; k += 2) rot(CYL(g, m_char, 0.1f, 0.06f, -0.5f + k * 0.28f, 0.98f, 0.12f), 1.5707963f, 0, 0);
  BOX(g, m_pcb, 0.4f, 0.03f, 0.3f, 0.5f, 0.92f, 0.1f);
  for (int k = 0; k < 3; k++) BOX(g, k & 1 ? m_info : m_rust, 0.2f, 0.12f, 0.18f, 0.9f, 0.96f, -0.3f + k * 0.22f);

  o = &OBST[OB_LATHE]; g = group_at(&SC->base, o->x, o->z);                 /* the lathe */
  BOX(g, m_char, 0.6f, 0.75f, 2.1f, 0.05f, 0.375f, 0);
  BOX(g, green, 0.4f, 0.12f, 2.0f, 0, 0.82f, 0);                             /* bed */
  BOX(g, green, 0.5f, 0.5f, 0.5f, 0, 1.1f, -0.75f);                          /* headstock */
  rot(CYL(g, m_steel_hi, 0.16f, 0.18f, 0, 1.08f, -0.42f), 1.5707963f, 0, 0); /* chuck */
  BOX(g, green, 0.35f, 0.32f, 0.3f, 0, 1.02f, 0.75f);                         /* tailstock */
  BOX(g, green, 0.35f, 0.15f, 0.35f, -0.05f, 0.95f, 0.1f);                    /* carriage */

  o = &OBST[OB_MILL]; g = group_at(&SC->base, o->x, o->z);                  /* the mill (not a Bridgeport) */
  BOX(g, green, 0.5f, 1.0f, 0.5f, 0.15f, 0.5f, 0);
  BOX(g, green, 0.3f, 1.4f, 0.3f, 0.25f, 1.5f, 0);
  BOX(g, green, 0.6f, 0.4f, 0.45f, 0, 2.1f, 0);
  BOX(g, m_steel_hi, 0.4f, 0.08f, 0.9f, -0.15f, 1.05f, 0);
  CYL(g, m_steel_hi, 0.05f, 0.3f, -0.2f, 1.75f, 0);

  t3_release(green); t3_release(woodtop); t3_release(red); t3_release(mug); t3_release(blk); t3_release(tvs);
}

/* plants, string lights, a bike on the wall */
static void build_props(void) {
  t3_material *pot = mat(0x9a3f20, 0.8f, 0), *leaf = mat(0x3f7a3a, 0.85f, 0), *leaf2 = mat(0x5a9a4a, 0.85f, 0);
  float pots[][2] = { { -11.5f, -6.7f }, { 9.0f, 6.5f }, { -11.5f, 6.4f }, { -0.9f, 6.4f }, { 5.0f, -6.7f } };
  for (int i = 0; i < 5; i++) {
    part(&SC->base, g_cyl, pot, 0.26f, 0.42f, 0.26f, pots[i][0], 0.21f, pots[i][1]);
    part(&SC->base, g_sphere, leaf, 0.42f, 0.38f, 0.42f, pots[i][0], 0.68f, pots[i][1]);
    part(&SC->base, g_sphere, leaf2, 0.28f, 0.34f, 0.28f, pots[i][0] + 0.18f, 0.92f, pots[i][1] + 0.08f);
  }
  /* string lights along the back wall */
  t3_material *bulb[3] = { emis(0xffd27a, 3.0f), emis(0xff8a5c, 3.0f), emis(0x8fd6ff, 3.0f) };
  for (int i = 0; i < 40; i++) {
    float x = -11.8f + i * 0.6f;
    float sag = 0.18f * sinf((i % 8) / 8.f * 3.14159f);
    t3_mesh *b = part(&SC->base, g_sphere, bulb[i % 3], 0.07f, 0.07f, 0.07f, x, 3.25f - sag, -6.95f);
    b->base.cast_shadow = false;
  }
  /* the bike repair corner: a bike hung on the right wall */
  t3_object *bike = group_at(&SC->base, 11.85f, 3.4f);
  t3_object_set_position(bike, 11.85f, 1.7f, 3.4f);
  t3_geometry *wheel = t3_torus_geometry_new(0.42f, 0.04f, 8, 24, 6.2831853f);
  for (int k = -1; k <= 1; k += 2) {
    t3_mesh *w = t3_mesh_new(wheel, m_char);
    t3_object_set_position(w, 0, 0, k * 0.55f);
    t3_object_set_rotation(w, 0, 1.5707963f, 0);
    t3_object_add(bike, w);
    t3_release(w);
  }
  t3_release(wheel);
  rot(BOX(bike, m_rust, 0.05f, 0.05f, 1.1f, 0, 0.15f, 0), 0.3f, 0, 0);
  rot(BOX(bike, m_rust, 0.05f, 0.6f, 0.05f, 0, 0.1f, 0.15f), 0.4f, 0, 0);
  BOX(bike, m_char, 0.1f, 0.05f, 0.25f, 0, 0.45f, -0.15f);
  for (int i = 0; i < 3; i++) t3_release(bulb[i]);
  t3_release(pot); t3_release(leaf); t3_release(leaf2);
}

static void build_signs(void) {
  /* the lockup: the mark, then the wordmark set in Charis SIL */
  hud_clear();
  hud_rect(0, 0, 1024, 256, C_CHAR, 255);
  hud_logo(28, 18, 220, C_BONE);
  hud_text(F_LOGO, 270, 70, "HeatSync Labs", C_BONE, 255, AL_LEFT);
  hud_text(F_MONO_S, 274, 150, "108 W MAIN ST  MESA AZ  SINCE 2009", C_ASH, 255, AL_LEFT);
  hud_tape(0, 236, 1024, 20, 0);
  sign(hud_texture(1024, 256), 6.4f, 1.6f, -1.2f, 2.45f, -6.98f, 0);

  /* the whiteboard */
  hud_clear();
  hud_rect(0, 0, 512, 256, 0xf4f1ea, 255);
  hud_frame(0, 0, 512, 256, 8, 0x8b8578, 255);
  hud_text(F_H3, 24, 18, "HYH TUESDAY 7PM", 0x2b5578, 255, AL_LEFT);
  hud_text(F_MONO, 24, 70, "1. CLEAN THE FLOORS", 0x15120f, 255, AL_LEFT);
  hud_text(F_MONO, 24, 104, "2. TESTING ENSUES", 0x15120f, 255, AL_LEFT);
  hud_text(F_MONO, 24, 138, "3. QUOTE: $4,000", 0xa52d1a, 255, AL_LEFT);
  hud_text(F_MONO, 24, 172, "4. GOTO 1", 0x15120f, 255, AL_LEFT);
  hud_text(F_MONO_S, 330, 214, "DO NOT ERASE", 0xa52d1a, 255, AL_LEFT);
  sign(hud_texture(512, 256), 2.8f, 1.4f, -9.0f, 1.9f, -6.98f, 0);

  /* the manifesto, on the right wall */
  hud_clear();
  hud_rect(0, 0, 512, 256, C_CHAR, 255);
  hud_tape(0, 0, 512, 18, 0);
  hud_text(F_H3, 22, 34, "NO ONE ELSE IS GOING", C_BONE, 255, AL_LEFT);
  hud_text(F_H3, 22, 70, "TO FIX IT, CLEAN IT,", C_BONE, 255, AL_LEFT);
  hud_text(F_H3, 22, 106, "OR REPLACE IT.", C_BONE, 255, AL_LEFT);
  hud_text(F_H2, 22, 150, "BUT YOU.", C_HAZARD, 255, AL_LEFT);
  hud_tape(0, 238, 512, 18, 24);
  sign(hud_texture(512, 256), 2.4f, 1.2f, 11.98f, 2.45f, 5.5f, -1.5708f);

  /* pegboard behind the electronics bench: every tool has an outline, half are missing */
  hud_clear();
  hud_rect(0, 0, 512, 256, 0xb08a5a, 255);
  for (int y = 8; y < 256; y += 16) for (int x = 8; x < 512; x += 16) hud_circle(x, y, 2, 0x5a4428, 255);
  static const struct { int x, y, w, h; uint32_t c; } TOOLS[] = {
    { 30, 30, 18, 120, 0xc2542c }, { 70, 40, 60, 14, 0x3a342b }, { 150, 26, 14, 100, 0x5e93c4 }, { 190, 30, 40, 40, 0x15120f },
    { 260, 40, 16, 110, 0xf2ab1e }, { 300, 30, 70, 18, 0x9b7fd0 }, { 400, 30, 20, 120, 0x15120f }, { 440, 60, 50, 50, 0xc2542c },
  };
  for (unsigned i = 0; i < sizeof TOOLS / sizeof TOOLS[0]; i++) {
    if (i % 3 == 2) hud_frame(TOOLS[i].x, TOOLS[i].y, TOOLS[i].w, TOOLS[i].h, 3, 0x2a1f12, 255);   /* the missing one */
    else hud_rect(TOOLS[i].x, TOOLS[i].y, TOOLS[i].w, TOOLS[i].h, TOOLS[i].c, 255);
  }
  hud_text(F_MONO_S, 24, 200, "IF YOU BORROW IT, PUT IT BACK.", 0x2a1f12, 255, AL_LEFT);
  sign(hud_texture(512, 256), 3.6f, 1.8f, 7.2f, 2.15f, -6.98f, 0);

  /* the OPEN sign by the front door */
  hud_clear();
  hud_rect(0, 0, 256, 128, 0x14110d, 255);
  hud_frame(4, 4, 248, 120, 6, 0xff4f8a, 255);
  hud_text(F_H1, 128, 22, "OPEN", 0xff7fae, 255, AL_CENTER);
  sign(hud_texture(256, 128), 1.4f, 0.7f, -11.97f, 2.3f, 6.3f, 1.5708f);

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
  m_sawdust = mat(0xd8b47a, 1.0f, 0);
  m_puddle = mat(0x5e93c4, 0.05f, 0);
  m_strip = mat(0x14110d, 0.5f, 0);
  m_glow = mat(0xff3b2a, 0.5f, 0);
  m_glow->emissive = t3_color_hex(0xff3b2a); m_glow->emissive_intensity = 3;
  m_skin = mat(0xc89a6e, 0.8f, 0);
  m_visor = mat(0x14110d, 0.2f, 0.3f);
  m_pcb = mat(0x2f8562, 0.5f, 0);
  m_led = mat(0x7fd0aa, 0.4f, 0);
  m_led->emissive = t3_color_hex(0x7fd0aa); m_led->emissive_intensity = 2;
  m_char = mat(C_CHAR, 0.7f, 0.2f);
  m_steel = mat(C_STEEL, 0.9f, 0);
  m_steel_hi = mat(C_STEEL_HI, 0.6f, 0.4f);
  m_bone = mat(C_BONE, 0.7f, 0);
  m_rust = mat(C_RUST, 0.8f, 0);
  m_violet = mat(0x6a4fa0, 0.9f, 0);
  m_info = mat(C_INFO, 0.5f, 0.2f);
  m_plate = mat(0xf4f1ea, 0.3f, 0);
  m_wood = mat(0x8a6a44, 0.85f, 0);
  m_dumpster = mat(0x2f5a46, 0.7f, 0.3f);
  m_fridge = mat(0xd8d2c4, 0.4f, 0.1f);
  m_mate = mat(0x7a4a18, 0.25f, 0);

  build_room();
  build_stations();
  build_furniture();
  build_props();
  build_signs();

  t3_object_add(&SC->base, t3_hemisphere_light_new(0xfff2dc, 0x3a2e22, 1.7f));
  t3_light *sun = t3_directional_light_new(0xffe2b8, 2.8f);
  t3_object_set_position(sun, -6, 16, 9);
  sun->base.cast_shadow = true;
  sun->shadow->map_width = sun->shadow->map_height = 2048;
  t3_camera *sc = sun->shadow->camera;
  sc->left = -16; sc->right = 16; sc->top = 12; sc->bottom = -12; sc->near = 1; sc->far = 40;
  t3_camera_update_projection_matrix(sc);
  sun->shadow->bias = -0.0006f;
  t3_object_add(&SC->base, sun);

  /* practical lights: warm over the kitchen, cool over the bench, and the laser's own glow */
  kitchen_lamp = t3_point_light_new(0xffb36b, 14.0f, 9.0f, 2.0f);
  t3_object_set_position(kitchen_lamp, -9.6f, 2.9f, 2.2f);
  t3_object_add(&SC->base, kitchen_lamp);
  bench_lamp = t3_point_light_new(0x8fbde6, 12.0f, 9.0f, 2.0f);
  t3_object_set_position(bench_lamp, 6.5f, 2.9f, -4.6f);
  t3_object_add(&SC->base, bench_lamp);
  laser_glow = t3_point_light_new(0xff3b2a, 0.0f, 6.0f, 2.0f);
  t3_object_set_position(laser_glow, -4.6f, 1.8f, -5.2f);
  t3_object_add(&SC->base, laser_glow);
  /* a warm spot on the Show & Tell window */
  stage_lamp = t3_point_light_new(0xffd59a, 10.0f, 7.0f, 2.0f);
  t3_object_set_position(stage_lamp, 1.8f, 2.8f, 5.2f);
  t3_object_add(&SC->base, stage_lamp);

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
    case IT_SAWDUST: part(g, g_disc, m_sawdust, 0.6f, 0.03f, 0.5f, 0, 0.02f, 0); break;
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
    t3_mesh *ring = part(g, g_disc, rm, 0.62f, 0.02f, 0.62f, 0, 0.015f, 0);
    ring->base.cast_shadow = false;
    t3_mesh *hole = part(g, g_disc, m_char, 0.48f, 0.022f, 0.48f, 0, 0.016f, 0);
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
  /* laser */
  station_t *L = &S[ST_LASER];
  bool cutting = G.laser_cutting > 0;
  laser_glow->intensity = cutting ? 10.0f + 6.0f * sinf(t * 40) : 0.0f;
  L->beam->base.visible = cutting;
  if (cutting) t3_object_set_position(L->beam, sinf(t * 7) * 0.9f, 1.2f, cosf(t * 5) * 0.5f);
  glow(L->light, L->state == S_BROKEN ? C_FAULT : L->state == S_DONE ? C_HAZARD : C_LIVE, L->state == S_BROKEN ? 2.5f * (0.6f + 0.4f * sinf(t * 10)) : 1.5f);
  /* printers */
  for (int k = 0; k < 2; k++) {
    station_t *s = &S[ST_PRINT1 + k];
    s->growth->base.visible = s->state == S_BUSY || s->state == S_DONE;
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
  /* show and tell pedestal: the last thing shown, spinning */
  station_t *sh = &S[ST_SHOW];
  sh->growth->base.visible = sh->timer > 0;
  t3_object_set_rotation(sh->growth, t, t * 1.3f, 0);
}
