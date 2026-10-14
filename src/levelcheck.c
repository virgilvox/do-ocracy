/* level_check: the eight rules of FINAL-space.md 9.2 over one level's tables.
 *
 * It reads only the level_t (the station places in lv->st, never S[]), so the
 * same file runs inside the cart (world_set_level logs every finding after a
 * rebuild) and natively in tools/smoke.c, which checks all five levels and
 * fails on any error. tools/gapcheck.py is the Python port of the same rules.
 *
 * Errors: GAP, WALL, APPROACH, SPAWN, RECT, FLOOR, STENCIL, PROP. Warnings
 * (NARROW, and NEAR for a free standing prop parked in a pinch) are reported
 * but not counted.
 *
 * Two places differ from the letter of 9.2, on purpose:
 * - Props have no collision, so a shop vac 0.2 from the lathe pinches nothing
 *   a body cares about. A prop disc is an error (PROP) only when it sits inside
 *   a solid or on a station's approach point, where a bot would stand in it;
 *   a pinch between a prop and a solid is the NEAR warning.
 * - RECT asks for 40 percent of the samples clear, not 60: spot_in tries 20
 *   points, so at 40 percent it fails one call in 27,000, and with all 23
 *   stations present the CLASSIC newbie box is 53 percent clear. */
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define GAP_MIN 0.76f
#define CLAMP_X 11.6f
#define CLAMP_ZB (-6.6f)
#define CLAMP_ZF 7.0f

typedef struct { float x, z, hw, hd; int st, ob, kind; const char *name; } cbox_t;

static const char *const OBK_NAMES[OBK_KIND_COUNT] = {
  "CLOSET", "WOODBAY", "FENCE", "LATHE", "MILL", "COUNTER", "DESKS", "WINDOW", "COFFEE",
  "LEGO", "HOST", "ISLAND", "DONBIN", "FOLD", "BOXES",
};

static cbox_t bx[ST_COUNT + MAX_OBST + MAX_PROPS];
static int bn;
static int found;
static void (*sink)(const char *line);
static const level_t *cur;

static void report(bool error, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void report(bool error, const char *fmt, ...) {
  char line[200];
  va_list ap; va_start(ap, fmt);
  int n = snprintf(line, sizeof line, "%s ", cur->name);
  vsnprintf(line + n, sizeof line - n, fmt, ap);
  va_end(ap);
  if (sink) sink(line);
  if (error) found++;
}

static bool solid_station(const level_t *lv, int i) { return lv->st[i].present && i != ST_SWITCH && i != ST_BACKDOOR; }

/* a station sitting inside a counter, or two fence boxes at their corner */
static bool nested(const cbox_t *a, const cbox_t *b) {
  if (a->kind == OBK_FENCE && b->kind == OBK_FENCE) return true;
  const cbox_t *s = a->st >= 0 ? a : b, *o = a->st >= 0 ? b : a;
  if (s->st < 0 || o->ob < 0 || o->kind != OBK_COUNTER) return false;
  return fabsf(s->x - o->x) <= o->hw && fabsf(s->z - o->z) <= o->hd;
}

static float axis_gap(float c0, float h0, float c1, float h1) { return fabsf(c0 - c1) - h0 - h1; }

static float box_dist(float x, float z, const cbox_t *b) {
  float dx = fmaxf(0, fabsf(x - b->x) - b->hw), dz = fmaxf(0, fabsf(z - b->z) - b->hd);
  return sqrtf(dx * dx + dz * dz);
}

/* the segment from p0 to p1 passes through box b (slab test) */
static bool seg_hits(float x0, float z0, float x1, float z1, const cbox_t *b) {
  float t0 = 0, t1 = 1, d[2] = { x1 - x0, z1 - z0 }, o[2] = { x0, z0 };
  float lo[2] = { b->x - b->hw, b->z - b->hd }, hi[2] = { b->x + b->hw, b->z + b->hd };
  for (int k = 0; k < 2; k++) {
    if (fabsf(d[k]) < 1e-6f) { if (o[k] < lo[k] || o[k] > hi[k]) return false; continue; }
    float a = (lo[k] - o[k]) / d[k], c = (hi[k] - o[k]) / d[k];
    if (a > c) { float t = a; a = c; c = t; }
    if (a > t0) t0 = a;
    if (c < t1) t1 = c;
    if (t0 > t1) return false;
  }
  return true;
}

static bool inside_any(float x, float z, int skip, float grow) {
  for (int k = 0; k < bn; k++) {
    if (k == skip) continue;
    if (fabsf(x - bx[k].x) <= bx[k].hw + grow && fabsf(z - bx[k].z) <= bx[k].hd + grow) return true;
  }
  return false;
}

/* clear_spot of game.c over the level's own boxes (props are not solids) */
static bool clear_spot_lv(float x, float z, float m, int solids) {
  for (int k = 0; k < solids; k++) {
    const cbox_t *b = &bx[k];
    if (b->st >= 0) { if (box_dist(x, z, b) < m) return false; }
    else if (fabsf(x - b->x) - b->hw < m && fabsf(z - b->z) - b->hd < m) return false;
  }
  return true;
}

static void approach(const st_place_t *s, float m, float *x, float *z) {
  switch (s->face) {
    case FACE_NZ: *x = s->x; *z = s->z - s->hd - m; break;
    case FACE_PX: *x = s->x + s->hw + m; *z = s->z; break;
    case FACE_NX: *x = s->x - s->hw - m; *z = s->z; break;
    default: *x = s->x; *z = s->z + s->hd + m; break;
  }
}

static uint8_t tmap[41][24];
static void resolve(const level_t *lv) {
  memset(tmap, T_WHITE, sizeof tmap);
  for (int i = 0; i < lv->accent_n; i++) {
    const accent_t *a = &lv->accents[i];
    for (int c = a->c; c < a->c + a->cw && c < 41; c++)
      for (int r = a->r; r < a->r + a->rh && r < 24; r++) {
        bool edge = c == a->c || r == a->r || c == a->c + a->cw - 1 || r == a->r + a->rh - 1;
        if (a->ring && !edge) continue;
        tmap[c][r] = a->kind == T_CHECK ? T_RED : a->kind;
      }
  }
}

static int sample_seed;
static float srnd(void) { sample_seed = sample_seed * 1103515245 + 12345; return (float)((sample_seed >> 8) & 0xffff) / 65536.f; }

int level_check(const level_t *lv, void (*out)(const char *line)) {
  cur = lv; sink = out; found = 0; bn = 0;
  for (int i = 0; i < ST_COUNT; i++) if (solid_station(lv, i)) {
    const st_place_t *s = &lv->st[i];
    bx[bn++] = (cbox_t){ s->x, s->z, s->hw, s->hd, i, -1, -1, s->label };
  }
  for (int i = 0; i < lv->ob_n; i++) {
    const obst_t *o = &lv->ob[i];
    bx[bn++] = (cbox_t){ o->x, o->z, o->hw, o->hd, -1, i, o->kind, OBK_NAMES[o->kind] };
  }
  int solids = bn;
  /* rule 8 (part): the free standing props that bodies walk around are 0.3 discs in the pair test */
  for (int i = 0; i < lv->prop_n; i++) {
    const prop_t *p = &lv->props[i];
    if (p->kind == PR_SHOPVAC || p->kind == PR_TOOLCART || p->kind == PR_WATER || p->kind == PR_BUCKET)
      bx[bn++] = (cbox_t){ p->x, p->z, 0.3f, 0.3f, -1, -1, -1, "PROP" };
  }

  /* 1: every pair of solids */
  for (int a = 0; a < bn; a++)
    for (int b = a + 1; b < bn; b++) {
      const cbox_t *A = &bx[a], *B = &bx[b];
      float gx = axis_gap(A->x, A->hw, B->x, B->hw), gz = axis_gap(A->z, A->hd, B->z, B->hd);
      bool prop = a >= solids || b >= solids;
      if (gx <= 0.05f && gz <= 0.05f) {
        /* a prop whose disc lies wholly inside a box is stored under it (the bucket under the HYH table) */
        const cbox_t *P0 = a >= solids ? A : B, *O = a >= solids ? B : A;
        bool stored = prop && fabsf(P0->x - O->x) + P0->hw <= O->hw + 0.01f && fabsf(P0->z - O->z) + P0->hd <= O->hd + 0.01f;
        if (gx < -0.05f && gz < -0.05f && !nested(A, B) && !stored)
          report(true, "%s overlap %s and %s (%.2f, %.2f)", prop ? "PROP" : "GAP", A->name, B->name, gx, gz);
        continue;
      }
      float cx = fmaxf(gx, 0), cz = fmaxf(gz, 0);
      if (gx < GAP_MIN && gz < GAP_MIN && hypotf(cx, cz) < GAP_MIN) {
        bool third = false;
        for (int k = 0; k < bn && !third; k++) if (k != a && k != b && seg_hits(A->x, A->z, B->x, B->z, &bx[k])) third = true;
        if (!third) report(!prop, "%s %s and %s gap %.2f, %.2f", prop ? "NEAR" : "GAP", A->name, B->name, gx, gz);
      } else if (!prop && ((gx < 0 && gz >= GAP_MIN && gz < 1.0f) || (gz < 0 && gx >= GAP_MIN && gx < 1.0f)))
        report(false, "NARROW %s and %s gap %.2f", A->name, B->name, gx < 0 ? gz : gx);
    }

  /* 2: a strip between a box face and a clamp line that a body could wedge in */
  for (int a = 0; a < solids; a++) {
    const cbox_t *A = &bx[a];
    struct { float d, mx, mz; const char *wall; } s[4] = {
      { (A->x - A->hw) + CLAMP_X, (A->x - A->hw - CLAMP_X) / 2, A->z, "WEST" },
      { CLAMP_X - (A->x + A->hw), (A->x + A->hw + CLAMP_X) / 2, A->z, "EAST" },
      { (A->z - A->hd) - CLAMP_ZB, A->x, (A->z - A->hd + CLAMP_ZB) / 2, "BACK" },
      { CLAMP_ZF - (A->z + A->hd), A->x, (A->z + A->hd + CLAMP_ZF) / 2, "FRONT" },
    };
    for (int k = 0; k < 4; k++) {
      if (s[k].d <= 0.06f || s[k].d >= GAP_MIN) continue;   /* 0.06: a face set at 0.05 is touching, float or not */
      if (k == 3 && fabsf(A->x - lv->door_x) < A->hw + lv->door_hw) continue;   /* the door slot is open to the street */
      if (inside_any(s[k].mx, s[k].mz, a, 0.05f)) continue;
      report(true, "WALL %s %.2f from the %s clamp", A->name, s[k].d, s[k].wall);
    }
  }

  /* 3: every present station with a face can be reached */
  for (int i = 0; i < ST_COUNT; i++) {
    const st_place_t *s = &lv->st[i];
    if (!s->present || s->face == FACE_NONE || i == ST_SWITCH || i == ST_BACKDOOR) continue;
    float x, z; approach(s, 0.55f, &x, &z);
    bool ok = x >= -CLAMP_X && x <= CLAMP_X && z >= CLAMP_ZB && z <= 8.5f;
    for (int k = 0; k < solids && ok; k++) {
      if (bx[k].st == i) continue;
      cbox_t me = { s->x, s->z, s->hw, s->hd, i, -1, -1, s->label };
      if (nested(&me, &bx[k])) continue;
      if (box_dist(x, z, &bx[k]) < 0.38f) ok = false;
    }
    if (!ok) report(true, "APPROACH %s at %.2f, %.2f", s->label, x, z);
    for (int k = solids; k < bn; k++)
      if (box_dist(x, z, &bx[k]) < 0.05f) report(true, "PROP disc at %.2f, %.2f stands on the %s approach", bx[k].x, bx[k].z, s->label);
  }

  /* 4: the lobby and night start places (game_add_player and game_start_night) */
  for (int i = 0; i < 16; i++) {
    int k = i % 8;
    float x = i < 8 ? lv->lobby_x + (k % 4) * 1.1f : lv->start_x + (k % 4) * 1.2f;
    float z = i < 8 ? lv->lobby_z + (k / 4) * 0.9f : lv->start_z + (k / 4) * 0.9f;
    for (int b = 0; b < solids; b++) if (box_dist(x, z, &bx[b]) < 0.4f) {
      report(true, "SPAWN %s %d at %.2f, %.2f inside %s", i < 8 ? "lobby" : "start", k, x, z, bx[b].name);
      break;
    }
  }

  /* 5: the boxes the simulation picks random clear points from */
  const box_t *rect[4] = { &lv->spot_box, &lv->newbie_box, &lv->wander_box, &lv->doormat_box };
  static const char *const RN[4] = { "spot_box", "newbie_box", "wander_box", "doormat_box" };
  for (int r = 0; r < 4; r++) {
    int ok = 0; sample_seed = 12345 + r;
    for (int n = 0; n < 200; n++) {
      float x = rect[r]->x0 + srnd() * (rect[r]->x1 - rect[r]->x0), z = rect[r]->z0 + srnd() * (rect[r]->z1 - rect[r]->z0);
      if (clear_spot_lv(x, z, 0.6f, solids)) ok++;
    }
    if (ok < 80) report(true, "RECT %s only %d of 200 clear", RN[r], ok);
  }

  /* 6: accents on the grid */
  for (int i = 0; i < lv->accent_n; i++) {
    const accent_t *a = &lv->accents[i];
    if (a->c + a->cw > 41 || a->r + a->rh > 24 || !a->cw || !a->rh) report(true, "FLOOR accent %d at c%d r%d runs off the grid", i, a->c, a->r);
    if (a->kind == T_CHECK && (a->cw != 2 || a->rh != 2)) report(true, "FLOOR checker %d is %dx%d", i, a->cw, a->rh);
  }

  /* 7: stencils on plain floor */
  resolve(lv);
  for (int i = 0; i < lv->stencil_n; i++) {
    const stencil_t *s = &lv->stencils[i];
    int c = (int)floorf((s->x + 12.3f) / 0.6f), r = (int)floorf((s->z + 7.15f) / 0.6f);
    int t = c >= 0 && c < 41 && r >= 0 && r < 24 ? tmap[c][r] : -1;
    if (t != T_WHITE && t != T_GREY) report(true, "STENCIL %s at c%d r%d sits on tile kind %d", s->text, c, r, t);
  }

  /* 8: signs and props inside the room */
  for (int i = 0; i < lv->sign_n; i++) {
    const sign_t *s = &lv->signs[i];
    if (s->kind == SG_GAS) continue;   /* panel local: build_signs offsets it by the back door */
    if (fabsf(s->x) > 12.3f || s->z < -7.15f || s->z > 8.5f || s->y < 0 || s->y > 3.4f)
      report(true, "PROP sign %d (%s) at %.2f, %.2f, %.2f is outside the room", i, s->text ? s->text : "", s->x, s->y, s->z);
  }
  for (int i = 0; i < lv->prop_n; i++) {
    const prop_t *p = &lv->props[i];
    if (fabsf(p->x) > 12.3f || p->z < -7.15f || p->z > 8.5f || p->y < 0 || p->y > 3.4f)
      report(true, "PROP prop %d kind %d at %.2f, %.2f, %.2f is outside the room", i, p->kind, p->x, p->y, p->z);
  }
  char line[96];
  snprintf(line, sizeof line, "level_check %s: %d", lv->name, found);
  if (out) out(line);
  return found;
}
