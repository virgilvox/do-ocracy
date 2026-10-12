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
                   *m_char, *m_steel, *m_steel_hi, *m_bone, *m_slab, *m_rust, *m_live, *m_violet, *m_info,
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

/* the stuff every hackerspace has: shelving, bins, plants, string lights */
static void build_props(void) {
  t3_material *bins[4] = { mat(C_RUST, 0.7f, 0), mat(C_INFO, 0.7f, 0), mat(C_HAZARD, 0.6f, 0), mat(0x2f8562, 0.7f, 0) };
  /* shelving units along the back wall, full of labelled bins */
  for (int u = 0; u < 2; u++) {
    float sx = u == 0 ? -10.4f : 7.6f;
    for (int lvl = 0; lvl < 3; lvl++) BOX(&SC->base, m_steel_hi, 1.6f, 0.06f, 0.55f, sx, 0.4f + lvl * 0.75f, -6.7f);
    BOX(&SC->base, m_steel_hi, 0.06f, 2.0f, 0.55f, sx - 0.8f, 1.0f, -6.7f);
    BOX(&SC->base, m_steel_hi, 0.06f, 2.0f, 0.55f, sx + 0.8f, 1.0f, -6.7f);
    for (int lvl = 0; lvl < 3; lvl++) for (int b = 0; b < 3; b++)
      BOX(&SC->base, bins[(lvl + b + u) & 3], 0.42f, 0.3f, 0.4f, sx - 0.5f + b * 0.5f, 0.6f + lvl * 0.75f, -6.65f);
  }
  /* a rug under the lurker couch, and one by the front door */
  t3_mesh *rug = BOX(&SC->base, mat(0x4a2f5c, 0.95f, 0), 4.2f, 0.02f, 2.4f, -4.0f, 0.01f, 2.0f);
  rug->base.cast_shadow = false;
  t3_mesh *mat_ = BOX(&SC->base, mat(0x3a2e22, 0.95f, 0), 2.0f, 0.02f, 1.0f, -8.0f, 0.01f, 6.4f);
  mat_->base.cast_shadow = false;
  /* painted walkway lanes: amber floor lines, the way a shop marks its aisles */
  t3_material *lane = mat(0x7a5a24, 0.9f, 0);
  struct { float x, z, w, d; } L[] = {
    { -1.5f, -3.7f, 15.0f, 0.08f }, { -1.5f, 4.6f, 15.0f, 0.08f }, { -9.2f, 0.45f, 0.08f, 8.3f }, { 6.2f, 0.45f, 0.08f, 8.3f },
  };
  for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++) {
    t3_mesh *l = BOX(&SC->base, lane, L[i].w, 0.012f, L[i].d, L[i].x, 0.006f, L[i].z);
    l->base.cast_shadow = false;
  }
  /* plants: somebody's pothos, somehow still alive */
  t3_material *pot = mat(0x9a3f20, 0.8f, 0), *leaf = mat(0x3f7a3a, 0.85f, 0);
  float pots[][2] = { { -11.5f, -5.9f }, { 11.4f, -2.2f }, { -11.5f, 6.4f }, { 5.9f, 6.5f } };
  for (int i = 0; i < 4; i++) {
    part(&SC->base, g_cyl, pot, 0.28f, 0.45f, 0.28f, pots[i][0], 0.225f, pots[i][1]);
    part(&SC->base, g_sphere, leaf, 0.45f, 0.4f, 0.45f, pots[i][0], 0.7f, pots[i][1]);
    part(&SC->base, g_sphere, leaf, 0.3f, 0.35f, 0.3f, pots[i][0] + 0.2f, 0.95f, pots[i][1] + 0.1f);
  }
  /* string lights along the back wall */
  t3_material *bulb[3] = { mat(0xffd27a, 0.4f, 0), mat(0xff8a5c, 0.4f, 0), mat(0x8fd6ff, 0.4f, 0) };
  for (int i = 0; i < 3; i++) { bulb[i]->emissive = bulb[i]->color; bulb[i]->emissive_intensity = 3.0f; }
  for (int i = 0; i < 40; i++) {
    float x = -11.8f + i * 0.6f;
    float sag = 0.18f * sinf((i % 8) / 8.f * 3.14159f);
    t3_mesh *b = part(&SC->base, g_sphere, bulb[i % 3], 0.07f, 0.07f, 0.07f, x, 3.15f - sag, -6.95f);
    b->base.cast_shadow = false;
  }
  for (int i = 0; i < 4; i++) t3_release(bins[i]);
  for (int i = 0; i < 3; i++) t3_release(bulb[i]);
  t3_release(lane); t3_release(pot); t3_release(leaf);
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
  sign(hud_texture(512, 256), 2.8f, 1.4f, 12.08f, 1.9f, -2.6f, -1.5708f);

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
  sign(hud_texture(512, 256), 4.0f, 2.0f, 9.6f, 1.9f, -6.98f, 0);

  /* the OPEN sign by the front door */
  hud_clear();
  hud_rect(0, 0, 256, 128, 0x14110d, 255);
  hud_frame(4, 4, 248, 120, 6, 0xff4f8a, 255);
  hud_text(F_H1, 128, 22, "OPEN", 0xff7fae, 255, AL_CENTER);
  sign(hud_texture(256, 128), 1.6f, 0.8f, -11.95f, 2.2f, 5.0f, 1.5708f);

  /* caution tape on the floor around the laser */
  hud_clear();
  hud_tape(0, 0, 512, 32, 0);
  t3_texture *tape = hud_texture(512, 32);
  t3_retain(tape); t3_retain(tape);
  t3_mesh *a = sign(tape, 3.6f, 0.22f, -5.0f, 0.012f, -4.35f, 0);
  t3_object_set_rotation(a, -1.5708f, 0, 0);
  t3_mesh *b = sign(tape, 2.6f, 0.22f, -7.0f, 0.012f, -5.7f, 0);
  t3_object_set_rotation(b, -1.5708f, 0, 1.5708f);
  t3_mesh *c = sign(tape, 2.6f, 0.22f, -3.0f, 0.012f, -5.7f, 0);
  t3_object_set_rotation(c, -1.5708f, 0, 1.5708f);
}

static void build_room(void) {
  /* floor: a checker of worn tiles */
  t3_material *f1 = mat(0x2f2920, 0.95f, 0), *f2 = mat(0x2a241d, 0.95f, 0);
  for (int i = 0; i < 13; i++) for (int j = 0; j < 8; j++) {
    t3_mesh *t = BOX(&SC->base, (i + j) & 1 ? f1 : f2, 2.0f, 0.1f, 2.0f, -12 + 1 + i * 2.0f, -0.05f, -7 + 1 + j * 2.0f);
    t->base.cast_shadow = false;
  }
  /* walls, cut away at the front */
  BOX(&SC->base, m_steel, 24.6f, 3.4f, 0.3f, 0, 1.7f, -7.15f);
  BOX(&SC->base, m_steel, 0.3f, 3.4f, 14.3f, -12.15f, 1.7f, 0);
  BOX(&SC->base, m_steel, 0.3f, 3.4f, 14.3f, 12.15f, 1.7f, 0);
  BOX(&SC->base, m_steel_hi, 24.6f, 0.12f, 0.36f, 0, 3.4f, -7.15f);
  /* front kick wall stubs, with a gap for the door at the left */
  BOX(&SC->base, m_steel, 3.0f, 0.5f, 0.25f, -10.5f, 0.25f, 7.1f);
  BOX(&SC->base, m_steel, 15.0f, 0.5f, 0.25f, 4.5f, 0.25f, 7.1f);
  /* the alley door on the right wall */
  BOX(&SC->base, m_rust, 0.12f, 2.4f, 1.3f, 12.0f, 1.2f, -0.6f);
  /* the front door frame (left) */
  BOX(&SC->base, m_steel_hi, 0.2f, 2.6f, 0.2f, -8.9f, 1.3f, 7.1f);
  BOX(&SC->base, m_steel_hi, 0.2f, 2.6f, 0.2f, -7.1f, 1.3f, 7.1f);
  BOX(&SC->base, m_steel_hi, 2.0f, 0.2f, 0.2f, -8.0f, 2.6f, 7.1f);
  /* ceiling duct for the laser */
  CYL(&SC->base, m_steel_hi, 0.28f, 1.6f, -5.6f, 2.6f, -6.2f);
}

static void build_stations(void) {
  station_t *s;
  t3_object *g;

  /* lumber rack: sheets of plywood leaning on the left wall */
  s = &S[ST_LUMBER]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_steel_hi, 0.1f, 2.0f, 2.6f, -0.55f, 1.0f, 0);
  for (int i = 0; i < 6; i++) {
    t3_mesh *sh = BOX(g, m_plywood, 0.05f, 1.8f, 2.4f, -0.4f + i * 0.09f, 0.9f, 0);
    t3_object_set_rotation(sh, 0, 0, -0.12f);
  }

  /* PEWPEW 9000: the big laser */
  s = &S[ST_LASER]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_char, 2.7f, 0.95f, 1.6f, 0, 0.475f, 0);
  BOX(g, m_steel_hi, 2.7f, 0.08f, 1.6f, 0, 0.99f, 0);
  t3_mesh *lid = BOX(g, m_amber, 2.5f, 0.18f, 1.4f, 0, 1.12f, 0.02f);
  (void)lid;
  BOX(g, m_char, 0.5f, 0.3f, 0.3f, 1.0f, 1.3f, -0.6f);
  s->beam = BOX(g, m_glow, 0.04f, 0.5f, 0.04f, 0, 1.2f, 0);
  s->beam->base.visible = false;
  s->light = BOX(g, mat(0x5fb892, 0.4f, 0), 0.14f, 0.14f, 0.05f, 1.1f, 0.8f, 0.81f);

  /* the two 3D printers, each adopted and named: Loafer and Dr. Strange */
  static const uint32_t frames[2] = { 0xc2542c, 0x9b7fd0 };
  for (int k = 0; k < 2; k++) {
    s = &S[ST_PRINT1 + k]; g = s->obj = group_at(&SC->base, s->x, s->z);
    BOX(g, m_slab, 1.2f, 0.7f, 1.0f, 0, 0.35f, 0);
    t3_material *fm = mat(frames[k], 0.6f, 0.2f);
    BOX(g, fm, 0.08f, 1.1f, 0.08f, -0.5f, 1.25f, 0);
    BOX(g, fm, 0.08f, 1.1f, 0.08f, 0.5f, 1.25f, 0);
    BOX(g, fm, 1.08f, 0.08f, 0.08f, 0, 1.8f, 0);
    BOX(g, m_steel_hi, 0.8f, 0.04f, 0.7f, 0, 0.74f, 0);
    BOX(g, m_char, 0.18f, 0.18f, 0.18f, 0.1f, 1.6f, 0.05f);
    s->growth = part(g, g_knot, m_amber, 0.18f, 0.18f, 0.18f, 0, 0.9f, 0);
    s->growth->base.visible = false;
    s->light = BOX(g, mat(0x5fb892, 0.4f, 0), 0.12f, 0.12f, 0.05f, 0.42f, 0.55f, 0.51f);
    t3_release(fm);
  }

  /* Sir Cutsalot, the vinyl cutter nobody fights over */
  s = &S[ST_CUTSALOT]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_steel, 1.6f, 0.75f, 0.8f, 0, 0.375f, 0);
  BOX(g, m_bone, 1.5f, 0.3f, 0.35f, 0, 0.9f, -0.1f);
  CYL(g, m_violet, 0.12f, 1.4f, 0, 0.95f, 0.22f);

  /* electronics bench: scopes, a soldering station, blinkenlights */
  s = &S[ST_BENCH]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_wood, 3.8f, 0.1f, 1.4f, 0, 0.85f, 0);
  for (int i = 0; i < 4; i++) BOX(g, m_steel_hi, 0.08f, 0.85f, 0.08f, i < 2 ? -1.8f : 1.8f, 0.425f, i & 1 ? -0.6f : 0.6f);
  BOX(g, m_info, 0.8f, 0.5f, 0.5f, -1.1f, 1.15f, -0.3f);
  BOX(g, m_char, 0.6f, 0.36f, 0.04f, -1.1f, 1.17f, -0.04f);
  BOX(g, m_rust, 0.5f, 0.25f, 0.4f, 0.2f, 1.03f, -0.3f);
  for (int i = 0; i < 6; i++) part(g, g_sphere, m_led, 0.06f, 0.06f, 0.06f, 0.8f + i * 0.15f, 0.95f, 0.2f);
  BOX(g, m_pcb, 0.6f, 0.04f, 0.4f, 1.1f, 0.92f, 0.1f);

  /* the kitchen: sink with a stack of dishes, and the fridge */
  s = &S[ST_SINK]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_bone, 1.2f, 0.9f, 1.6f, 0, 0.45f, 0);
  BOX(g, m_steel_hi, 0.8f, 0.06f, 1.0f, 0.05f, 0.93f, 0);
  for (int i = 0; i < 10; i++) {
    t3_mesh *pl = part(g, g_cyl, m_plate, 0.28f, 0.04f, 0.28f, 0.05f + (i & 1) * 0.04f, 1.0f + i * 0.06f, -0.1f + (i % 3) * 0.06f);
    pl->base.visible = false;
  }
  s = &S[ST_FRIDGE]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_fridge, 1.1f, 2.1f, 1.0f, 0, 1.05f, 0);
  BOX(g, m_steel_hi, 0.05f, 0.6f, 0.06f, 0.45f, 1.3f, 0.52f);
  for (int i = 0; i < 4; i++) {
    part(g, g_cyl, m_mate, 0.06f, 0.3f, 0.06f, -0.3f + i * 0.2f, 2.25f, 0);
  }
  s->light = part(g, g_sphere, mat(0x5fb892, 0.5f, 0), 0.0f, 0.0f, 0.0f, 0, 2.6f, 0);

  /* trash by the alley door, and the Free Pile */
  s = &S[ST_TRASH]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_dumpster, 1.3f, 1.0f, 1.1f, 0, 0.5f, 0);
  BOX(g, m_char, 1.35f, 0.08f, 1.15f, 0, 1.04f, 0);
  s = &S[ST_FREEPILE]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_crt, 0.8f, 0.7f, 0.8f, -0.4f, 0.35f, -0.3f);
  BOX(g, m_screen, 0.6f, 0.5f, 0.05f, -0.4f, 0.38f, 0.11f);
  BOX(g, m_cardboard, 0.7f, 0.5f, 0.6f, 0.45f, 0.25f, 0.2f);
  BOX(g, m_cardboard, 0.5f, 0.4f, 0.5f, 0.3f, 0.7f, 0.1f);
  t3_mesh *wart = BOX(g, m_char, 0.6f, 0.3f, 0.5f, -0.2f, 0.85f, -0.2f);
  t3_object_set_rotation(wart, 0, 0.5f, 0.2f);
  BOX(g, m_slab, 0.9f, 0.4f, 0.4f, 0.2f, 0.2f, -0.6f);

  /* show and tell: the LEGO window display at the front */
  s = &S[ST_SHOW]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_char, 4.0f, 0.4f, 1.1f, 0, 0.2f, 0);
  BOX(g, m_amber, 4.1f, 0.06f, 1.2f, 0, 0.43f, 0);
  s->growth = part(g, g_box, m_amber, 0.5f, 0.5f, 0.5f, 0, 0.75f, 0);
  s->growth->base.visible = false;

  /* the lurker couch */
  s = &S[ST_COUCH]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_violet, 3.2f, 0.5f, 1.0f, 0, 0.25f, 0);
  BOX(g, m_violet, 3.2f, 0.7f, 0.25f, 0, 0.6f, -0.4f);
  BOX(g, m_violet, 0.25f, 0.65f, 1.0f, -1.5f, 0.45f, 0);
  BOX(g, m_violet, 0.25f, 0.65f, 1.0f, 1.5f, 0.45f, 0);

  /* the big worktable, always covered in somebody's project */
  s = &S[ST_TABLE]; g = s->obj = group_at(&SC->base, s->x, s->z);
  BOX(g, m_wood, 3.0f, 0.12f, 1.6f, 0, 0.9f, 0);
  for (int i = 0; i < 4; i++) BOX(g, m_steel_hi, 0.1f, 0.9f, 0.1f, i < 2 ? -1.4f : 1.4f, 0.45f, i & 1 ? -0.7f : 0.7f);
  BOX(g, m_cardboard, 0.5f, 0.3f, 0.4f, -0.8f, 1.11f, 0.1f);
  part(g, g_knot, m_rust, 0.15f, 0.15f, 0.15f, 0.6f, 1.15f, -0.2f);
  BOX(g, m_info, 0.3f, 0.12f, 0.2f, 0.1f, 1.02f, 0.3f);
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
  m_slab = mat(C_SLAB_HI, 0.8f, 0.1f);
  m_rust = mat(C_RUST, 0.8f, 0);
  m_live = mat(C_LIVE, 0.8f, 0);
  m_violet = mat(0x6a4fa0, 0.9f, 0);
  m_info = mat(C_INFO, 0.5f, 0.2f);
  m_plate = mat(0xf4f1ea, 0.3f, 0);
  m_wood = mat(0x8a6a44, 0.85f, 0);
  m_dumpster = mat(0x2f5a46, 0.7f, 0.3f);
  m_fridge = mat(0xd8d2c4, 0.4f, 0.1f);
  m_mate = mat(0x7a4a18, 0.25f, 0);

  build_room();
  build_stations();
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
  t3_object_set_position(kitchen_lamp, -9.5f, 3.0f, 3.5f);
  t3_object_add(&SC->base, kitchen_lamp);
  bench_lamp = t3_point_light_new(0x8fbde6, 12.0f, 9.0f, 2.0f);
  t3_object_set_position(bench_lamp, 8.5f, 3.0f, -4.5f);
  t3_object_add(&SC->base, bench_lamp);
  laser_glow = t3_point_light_new(0xff3b2a, 0.0f, 6.0f, 2.0f);
  t3_object_set_position(laser_glow, -5.0f, 1.8f, -5.0f);
  t3_object_add(&SC->base, laser_glow);

  world_cam = t3_perspective_camera_new(38, 1280.f / 720.f, 0.5f, 80);
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
  laser_glow->intensity = L->state == S_BUSY ? 10.0f + 6.0f * sinf(t * 40) : 0.0f;
  L->beam->base.visible = L->state == S_BUSY;
  if (L->state == S_BUSY) t3_object_set_position(L->beam, sinf(t * 7) * 0.9f, 1.2f, cosf(t * 5) * 0.5f);
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
