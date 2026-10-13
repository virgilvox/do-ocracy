/* Every screen's 2D layer, in GANTRY: char and bone, amber as a fill,
 * caution tape between things, solid offset shadows, no rounded corners.
 *
 * The rule for this file: a new player should never have to guess. Whatever
 * you hold has an arrow to where it goes; every station says what it pays;
 * every problem in the room has a marker; every prompt names your button. */
#include "game.h"
#include "hud.h"
#include "content.h"
#include "net.h"
#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float T;
static bool names_only, bot_guides;   /* attract: station names without status lines, guides for the bots */

bool project(float x, float y, float z, int *sx, int *sy) {
  t3_vec3 v = t3_vec3_apply_mat4(t3_v3(x, y, z), &world_cam->matrix_world_inverse);
  if (v.z > -0.1f) return false;
  v = t3_vec3_apply_mat4(v, &world_cam->projection_matrix);
  *sx = (int)((v.x * 0.5f + 0.5f) * HUD_W);
  *sy = (int)((0.5f - v.y * 0.5f) * HUD_H);
  return true;
}

static bool projf(float x, float y, float z, float *sx, float *sy) {
  t3_vec3 v = t3_vec3_apply_mat4(t3_v3(x, y, z), &world_cam->matrix_world_inverse);
  if (v.z > -0.1f) return false;
  v = t3_vec3_apply_mat4(v, &world_cam->projection_matrix);
  *sx = (v.x * 0.5f + 0.5f) * HUD_W;
  *sy = (0.5f - v.y * 0.5f) * HUD_H;
  return true;
}

static void label(int x, int y, const char *s, uint32_t fg, uint32_t bg, int font) {
  int w = hud_text_w(font, s), h = hud_font_h(font);
  hud_rect(x - w / 2 - 6, y - 3, w + 12, h + 8, bg, 235);
  hud_text(font, x, y, s, fg, 255, AL_CENTER);
}

/* world labels never stack: each one takes the nearest free spot above or
 * below where it wants to be, and the HUD strips are taken before any of them */
typedef struct { int x, y, w, h; } rect_t;
static rect_t placed[192];
static int nplaced;
static void reserve(int x, int y, int w, int h) { if (nplaced < 192) placed[nplaced++] = (rect_t){ x, y, w, h }; }
static bool hits(int x, int y, int w, int h) {
  for (int i = 0; i < nplaced; i++) {
    rect_t *r = &placed[i];
    if (x < r->x + r->w && x + w > r->x && y < r->y + r->h && y + h > r->y) return true;
  }
  return false;
}
static int settle(int x, int y, int w, int h) {
  static const int steps[] = { 0, 1, -1, 2, -2, 3, -3, -4, -5, -6 };   /* the far steps lift labels out of the lobby card band */
  for (unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++) {
    int ty = y + steps[i] * (h / 2 + 3);
    if (!hits(x, ty, w, h)) { reserve(x, ty, w, h); return ty; }
  }
  reserve(x, y, w, h);
  return y;
}
static void label_av(int x, int y, const char *s, uint32_t fg, uint32_t bg, int font) {
  int w = hud_text_w(font, s), h = hud_font_h(font);
  y = settle(x - w / 2 - 6, y - 3, w + 12, h + 8) + 3;
  hud_rect(x - w / 2 - 6, y - 3, w + 12, h + 8, bg, 235);
  hud_text(font, x, y, s, fg, 255, AL_CENTER);
}

/* what a player presses, named for their own input */
static const char *glyph_a(const player_t *p) { return p->kb == 0 ? "SPACE" : p->kb == 1 ? "." : "A"; }

/* the wall clock: the level says when the night starts and how many hours it spans */
static void clock_str(char *out, int n) {
  float frac = G.phase == PH_NIGHT ? G.t / G.night_len : 0;
  int mins = L->night.clock_h0 * 60 + (int)(frac * L->night.clock_span * 60);
  int h = (mins / 60) % 24, m = mins % 60;
  int h12 = h % 12 == 0 ? 12 : h % 12;
  snprintf(out, (size_t)n, "%d:%02d %s", h12, m, h < 12 ? "AM" : "PM");
}

/* ── particles: sparks, sparkles, confetti, dust, steam ─────────── */

#define NFX 360
typedef struct { float x, y, z, vx, vy, vz, life, max, size; uint32_t c; int kind; } fx_t;
static fx_t FX[NFX];
static int fx_next;

static void fx_add(int kind, float x, float y, float z, float vx, float vy, float vz, float life, float size, uint32_t c) {
  fx_t *f = &FX[fx_next];
  fx_next = (fx_next + 1) % NFX;
  *f = (fx_t){ x, y, z, vx, vy, vz, life, life, size, c, kind };
}

/* a steady trickle: n slow dust motes drifting in from a point (the open back door) */
void fx_emit(float x, float y, float z, float spread, uint32_t color, int n) {
  for (int i = 0; i < n; i++)
    fx_add(FX_DUST, x + (frnd() - 0.5f) * spread, y + (frnd() - 0.5f) * 1.4f, z + frnd() * 0.3f,
           -0.3f - frnd() * 0.6f, 0.1f + frnd() * 0.3f, 0.6f + frnd() * 0.9f, 1.0f + frnd() * 0.6f, 4 + frnd() * 4, color);
}

void fx_burst(int kind, float x, float y, float z, uint32_t color) {
  static const uint32_t CONF[6] = { C_HAZARD, C_LIVE, C_VIOLET, C_INFO, C_RUST, C_BONE };
  switch (kind) {
    case FX_SPARK:
      for (int i = 0; i < 7; i++) fx_add(kind, x, y, z, (frnd() - 0.5f) * 5, 2 + frnd() * 4, (frnd() - 0.5f) * 5, 0.35f + frnd() * 0.25f, 3, frnd() < 0.5f ? color : C_HAZARD);
      break;
    case FX_KARMA:
      for (int i = 0; i < 10; i++) fx_add(kind, x + (frnd() - 0.5f) * 0.8f, y, z + (frnd() - 0.5f) * 0.8f, 0, 1.5f + frnd() * 1.5f, 0, 0.9f + frnd() * 0.4f, 6 + frnd() * 4, color);
      break;
    case FX_CONFETTI:
      for (int i = 0; i < 46; i++) fx_add(kind, x + (frnd() - 0.5f) * 3, y, z, (frnd() - 0.5f) * 6, 5 + frnd() * 5, (frnd() - 0.5f) * 3, 1.4f + frnd() * 0.8f, 5, i % 3 == 0 ? color : CONF[rnd() % 6]);
      break;
    case FX_DUST:
      for (int i = 0; i < 6; i++) fx_add(kind, x + (frnd() - 0.5f) * 0.6f, y, z + (frnd() - 0.5f) * 0.6f, (frnd() - 0.5f) * 1.2f, 0.6f + frnd(), (frnd() - 0.5f) * 1.2f, 0.7f + frnd() * 0.4f, 8 + frnd() * 6, color);
      break;
    case FX_STEAM:
      for (int i = 0; i < 3; i++) fx_add(kind, x + (frnd() - 0.5f) * 0.5f, y, z, 0, 1.2f + frnd(), 0, 0.9f, 8, color);
      break;
    case FX_JOIN:
      for (int i = 0; i < 24; i++) {
        float a = i / 24.f * 6.2831853f;
        fx_add(kind, x, y, z, cosf(a) * 4, 2.5f, sinf(a) * 4, 0.6f, 5, color);
      }
      break;
  }
}

static void fx_draw(float dt) {
  for (int i = 0; i < NFX; i++) {
    fx_t *f = &FX[i];
    if (f->life <= 0) continue;
    f->life -= dt;
    f->x += f->vx * dt; f->y += f->vy * dt; f->z += f->vz * dt;
    if (f->kind == FX_SPARK || f->kind == FX_CONFETTI || f->kind == FX_JOIN) f->vy -= 14 * dt;
    if (f->kind == FX_CONFETTI) { f->vx *= 1 - dt * 1.5f; f->vz *= 1 - dt * 1.5f; if (f->vy < -2) f->vy = -2; }
    if (f->y < 0.02f) { f->y = 0.02f; f->vy = 0; f->vx *= 0.8f; f->vz *= 0.8f; }
    int sx, sy;
    if (!project(f->x, f->y, f->z, &sx, &sy)) continue;
    float k = f->life / f->max;
    int a = (int)(255 * (k < 0.3f ? k / 0.3f : 1));
    switch (f->kind) {
      case FX_KARMA: {      /* rising plus signs, the karma mark */
        int s = (int)f->size;
        hud_rect(sx - s / 2, sy - 1, s, 3, f->c, a);
        hud_rect(sx - 1, sy - s / 2, 3, s, f->c, a);
        break;
      }
      case FX_DUST: case FX_STEAM:
        hud_circle(sx, sy, (int)(f->size * (1.6f - k)), f->c, a / 3);
        break;
      case FX_CONFETTI:
        hud_rect(sx - 3, sy - 2, (int)f->size + ((i & 1) ? 2 : -1), 4, f->c, a);
        break;
      default:
        hud_rect(sx - 1, sy - 1, (int)f->size, (int)f->size, f->c, a);
    }
  }
}

/* ── the room: what each thing is, what it pays, what is wrong ───── */

/* The label budget (FINAL-space.md 11.3): with 23 stations the names cannot all
 * be up at once. A name draws only when the station has a status (an alarm or
 * something ready), when a human is within 2.2 units, or when the level marks
 * it always_label (the few the coach points at). The orange wall tiles and the
 * floor accents carry the names the rest of the time. */
static float label_height(int i) {
  switch (i) {
    case ST_FRIDGE: return 2.4f;
    case ST_BATH: return 2.2f;
    case ST_LUMBER: return 2.1f;
    case ST_PRINT1: case ST_PRINT2: case ST_STORAGE: return 2.0f;
    case ST_BOARD: return 2.6f;   /* above the board, so the lines show under the name */
    case ST_LASER: return 1.9f;
    case ST_FORGE: return 1.6f;
    case ST_LIBRARY: return 1.9f;
    case ST_BACKDOOR: return 2.7f;
    case ST_BIKE: return 2.4f;   /* above the stand, so the flat shows under the name */
    default: return 1.3f;
  }
}

static void station_labels(void) {
  for (int i = 0; i < ST_COUNT; i++) {
    station_t *s = &S[i];
    if (i == ST_COUCH || i == ST_TABLE || i == ST_SWITCH || !s->present || !s->label[0]) continue;
    if (i == ST_BACKDOOR && s->state != S_BROKEN) continue;
    int sx, sy;
    if (!project(s->x, label_height(i), s->z, &sx, &sy)) continue;
    char st[48] = "";
    uint32_t c = C_ASH;
    bool alarm = false;
    /* what it pays: amber stations make clout, teal ones take karma work */
    int pay = 0;   /* 1 clout, 2 karma */
    switch (i) {
      case ST_LASER:
        pay = 1;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "TOO DUSTY. HOLD TO CLEAN"); c = C_INK_ERR; alarm = true; pay = 2; }
        else if (s->state == S_LOADED) { snprintf(st, sizeof st, G.laser_cutting > 0 ? "CUTTING" : "LOADED. HOLD TO CUT"); c = C_HAZARD; }
        else snprintf(st, sizeof st, "BRING PLYWOOD  DUST %d%%", (int)s->dust);
        break;
      case ST_PRINT1: case ST_PRINT2:
        pay = 1;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "JAMMED. HOLD TO FIX"); c = C_INK_ERR; alarm = true; pay = 2; }
        else if (s->state == S_BUSY) snprintf(st, sizeof st, "PRINTING %ds", (int)ceilf(s->timer));
        else if (s->state == S_DONE) { snprintf(st, sizeof st, "READY. GRAB IT"); c = C_HAZARD; }
        break;
      case ST_LUMBER: case ST_BENCH: case ST_CUTSALOT: case ST_SHOW: pay = 1; break;
      case ST_SINK:
        pay = 2;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "DRAIN DRIED OUT"); c = C_INK_ERR; alarm = true; }
        else if (s->count) { snprintf(st, sizeof st, "%d DISHES. HOLD TO WASH", s->count); c = s->count > 5 ? C_INK_ERR : C_ASH; alarm = s->count > 5; }
        break;
      case ST_FRIDGE:
        if (s->smell) { snprintf(st, sizeof st, "SMELLS. HOLD TO CLEAN"); c = C_INK_ERR; alarm = true; pay = 2; }
        else if (L->night.fridge_locked) snprintf(st, sizeof st, "LOCKED");
        break;
      case ST_VEND:
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "ATE A DOLLAR"); c = C_INK_ERR; alarm = true; pay = 2; }
        break;
      case ST_BOARD:
        pay = 2;
        if (s->count >= 4) { snprintf(st, sizeof st, "%d LINES", s->count); c = s->count >= 8 ? C_INK_ERR : C_ASH; alarm = s->count >= 8; }
        break;
      case ST_SEWING:
        pay = 1;
        if (G.sewing > 0) snprintf(st, sizeof st, "SOMEBODY IS SEWING");
        break;
      case ST_BATH:
        pay = 2;
        if (s->timer >= 50) { snprintf(st, sizeof st, "NEEDS A HERO"); c = s->timer >= 70 ? C_INK_ERR : C_ASH; alarm = s->timer >= 70; }
        else if (s->count) snprintf(st, sizeof st, "LIGHT ON");
        break;
      case ST_STORAGE:
        pay = 2;
        if (s->count >= 4) { snprintf(st, sizeof st, "BIN %d/6", s->count); c = s->count >= 6 ? C_INK_ERR : C_ASH; alarm = s->count >= 6; }
        break;
      case ST_FORGE:
        pay = 1;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "TOO HOT"); c = C_INK_ERR; alarm = true; }
        else if (s->dust >= 50) { snprintf(st, sizeof st, "HOT %d", (int)s->dust); c = C_HAZARD; }
        break;
      case ST_LIBRARY: {
        pay = 2;
        bool quiet = false;
        for (int k = 0; k < MAX_PLAYERS; k++) if (P[k].active && P[k].read_t >= 3) quiet = true;
        if (quiet) snprintf(st, sizeof st, "QUIET");
        else if (s->count > 0) snprintf(st, sizeof st, "%d BOOKS OUT", s->count);
        break;
      }
      case ST_BACKDOOR:
        pay = 2;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "BLOWN OPEN"); c = C_INK_ERR; alarm = true; }
        break;
      case ST_BIKE:
        pay = 2;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "FLAT"); c = C_INK_ERR; alarm = true; }
        break;
      case ST_TRASH: case ST_FREEPILE: pay = 2; break;
    }
    /* the name when it matters (trouble, something ready, a person within 3
     * units, or the level keeps it up) and dimmed to 150 unless it is one of
     * the first three; the status line only for trouble, ready, or 1.4 units */
    float near_d = 99;
    for (int k = 0; k < MAX_PLAYERS; k++) {
      player_t *p = &P[k];
      if (!p->active || p->bot) continue;
      float dx = fabsf(p->x - s->x) - s->hw, dz = fabsf(p->z - s->z) - s->hd;
      float d = fmaxf(dx, dz);
      if (d < near_d) near_d = d;
    }
    bool near = near_d < 3.0f, close = near_d < 1.4f;
    bool ready = (i == ST_LASER && s->state == S_LOADED) || ((i == ST_PRINT1 || i == ST_PRINT2) && s->state == S_DONE);
    if (names_only) { st[0] = 0; alarm = false; }
    if (!(alarm || ready || close)) st[0] = 0;
    if (!(alarm || ready || near || s->always_label)) continue;
    int na = alarm || ready || near ? 240 : 150, ta = alarm || ready || near ? 255 : 150;
    uint32_t edge = pay == 1 ? C_HAZARD : pay == 2 ? C_LIVE : C_STEEL_HI;
    int w = hud_text_w(F_MONO_S, s->label);
    int sw = st[0] ? hud_text_w(F_MONO_S, st) + 12 : 0;
    int bw = (w + 16 > sw ? w + 16 : sw);
    sy = settle(sx - bw / 2, sy - 4, bw, st[0] ? 48 : 24) + 4;
    hud_rect(sx - w / 2 - 8, sy - 4, w + 16, 24, C_CHAR, na);
    hud_rect(sx - w / 2 - 8, sy - 4, 4, 24, edge, ta);
    hud_text(F_MONO_S, sx + 2, sy, s->label, C_BONE, ta, AL_CENTER);
    if (st[0]) label(sx, sy + 24, st, c, C_TAPE, F_MONO_S);
    if (alarm) {
      float pulse = 0.5f + 0.5f * sinf(T * 8);
      int r = 14 + (int)(pulse * 3);
      hud_circle(sx, sy - 26, r, C_FAULT, 255);
      hud_text(F_H3, sx, sy - 42, "!", C_BONE, 255, AL_CENTER);
    }
  }
}

/* one label per thing on the floor: a red tag wins over everything else */
static void floor_tags(void) {
  bool words = G.group_nights <= 1;   /* SWEEP ME and MOP ME on a group's first night only */
  for (int i = 0; i < MAX_ITEMS; i++) {
    item_t *it = &I[i];
    if (!it->alive || it->holder >= 0) continue;
    int sx, sy;
    if (!project(it->x, 0.45f, it->z, &sx, &sy)) continue;
    bool red = it->age > 25 && it->type != IT_PROJECT && it->type != IT_PLYWOOD;
    bool mess = it->type == IT_SAWDUST || it->type == IT_PUDDLE || it->type == IT_DAISY;
    if (mess) {
      /* a mess: a ring that says clean me */
      float pulse = 0.5f + 0.5f * sinf(T * 4 + i);
      int r = 15 + (int)(pulse * 3);
      uint32_t rc = it->type == IT_DAISY ? C_FAULT : C_LIVE;
      hud_ring(sx, sy + 8, r, 3, 1, rc, 200);
      /* a red tag below is the one label; otherwise the words or the broom */
      if (!red && it->type == IT_DAISY) label_av(sx, sy - 26, "UNPLUG ME", C_INK_ERR, C_CHAR, F_MONO_S);   /* a safety line: always */
      else if (!red && words) label_av(sx, sy - 26, it->type == IT_PUDDLE ? "MOP ME" : "SWEEP ME", C_INK_OK, C_CHAR, F_MONO_S);
      else if (!red) hud_glyph(sx - 8, sy + 8 - r - 18, GL_BROOM, 1, rc, 220);
    }
    if (red) label_av(sx, sy - 20, "RED TAG", C_BONE, C_FAULT, F_MONO_S);
    else if (it->type == IT_PROJECT) label_av(sx, sy - 20, item_name(it), C_HAZARD, C_CHAR, F_MONO_S);
    else if (it->type == IT_DONATION) label_av(sx, sy - 20, item_name(it), C_BONE, C_CHAR, F_MONO_S);
    else if (it->type == IT_MOVEBOX) label_av(sx, sy - 20, S[ST_STORAGE].present ? "TO STORAGE" : "TO THE TRASH", C_BONE, C_CHAR, F_MONO_S);
  }
}

static void npcs_overlay(void) {
  for (int i = 0; i < MAX_NPCS; i++) {
    npc_t *n = &N[i];
    if (!n->alive) continue;
    int sx, sy;
    if (!project(n->x, 2.2f, n->z, &sx, &sy)) continue;
    if (n->state <= 1) {
      float f = n->patience / L->night.patience;
      hud_circle(sx, sy, 20, C_CHAR, 235);
      hud_ring(sx, sy, 20, 5, f, f < 0.35f ? C_FAULT : C_LIVE, 255);
      hud_text(F_H3, sx, sy - 16, "?", C_BONE, 255, AL_CENTER);
      if (f < 0.35f) label_av(sx, sy + 26, "SAY HI", C_INK_ERR, C_CHAR, F_MONO_S);
    }
  }
}

/* the arrow from your hands to where the thing goes */
static void goal_guides(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active || (p->bot && !bot_guides)) continue;
    /* no goal: the coach's station, in ash, for humans only */
    bool coached = !p->has_goal && !p->bot && G.coach_st >= 0 && S[G.coach_st].present;
    if (!p->has_goal && !coached) continue;
    station_t *s = &S[coached ? G.coach_st : p->goal_st];
    uint32_t col = coached ? C_ASH : p->color;
    int ca = coached ? 150 : 230;
    /* target footprint, pulsing in the player's colour */
    float pulse = 0.5f + 0.5f * sinf(T * 6);
    float m = 0.25f + pulse * 0.12f;
    float cx[4] = { s->x - s->hw - m, s->x + s->hw + m, s->x + s->hw + m, s->x - s->hw - m };
    float cz[4] = { s->z - s->hd - m, s->z - s->hd - m, s->z + s->hd + m, s->z + s->hd + m };
    float px[4], py[4];
    bool ok = true;
    for (int k = 0; k < 4; k++) ok &= projf(cx[k], 0.05f, cz[k], &px[k], &py[k]);
    if (ok && !coached) for (int k = 0; k < 4; k++) hud_line(px[k], py[k], px[(k + 1) % 4], py[(k + 1) % 4], 5, col, 220);
    /* chevrons marching along the floor toward it */
    float dx = s->x - p->x, dz = s->z - p->z, d = sqrtf(dx * dx + dz * dz);
    float edge = fmaxf(s->hw, s->hd) + 0.4f;
    if (d > edge + 0.6f) {
      float ux = dx / d, uz = dz / d;
      float phase = fmodf(T * 2.5f, 1.0f);
      for (float t = 0.9f + phase * 0.9f; t < d - edge; t += 0.9f) {
        float ax, ay, bx, by;
        float wx = p->x + ux * t, wz = p->z + uz * t;
        if (!projf(wx, 0.06f, wz, &ax, &ay) || !projf(wx + ux * 0.3f, 0.06f, wz + uz * 0.3f, &bx, &by)) continue;
        float fade = fminf(1, (t - 0.6f) / 0.8f);
        hud_chevron(ax, ay, bx - ax, by - ay, 22, 6, col, (int)(ca * fade));
      }
    }
  }
}

static void players_overlay(void) {
  /* one proposal bubble on screen: the newest */
  int talk = -1;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && P[i].bubble > 0 && (talk < 0 || P[i].bubble > P[talk].bubble)) talk = i;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    int sx, sy;
    if (!project(p->x, 2.25f, p->z, &sx, &sy)) continue;
    /* name tag in the player's colour; a bot's is a smaller, dimmer one */
    int w = hud_text_w(F_MONO_S, p->name);
    if (p->bot) {
      hud_rect(sx - w / 2 - 5, sy + 2, w + 10, 14, p->color, 170);
      hud_text(F_MONO_S, sx, sy, p->name, C_TAPE, 170, AL_CENTER);
    } else {
      hud_rect(sx - w / 2 - 7, sy - 2, w + 14, 22, p->color, 255);
      hud_text(F_MONO_S, sx, sy, p->name, C_TAPE, 255, AL_CENTER);
    }
    /* the goal line for whatever is in your hands */
    if (p->has_goal && !p->bot) {
      int gw = hud_text_w(F_MONO_S, p->goal_label) + 18;
      hud_rect(sx - gw / 2, sy - 30, gw, 24, C_CHAR, 235);
      hud_rect(sx - gw / 2, sy - 30, gw, 3, p->color, 255);
      hud_text(F_MONO_S, sx, sy - 26, p->goal_label, C_BONE, 255, AL_CENTER);
    } else if (p->hold >= 0 && !p->bot) {
      const char *nm = item_name(&I[p->hold]);
      hud_text(F_MONO_S, sx, sy - 22, nm, I[p->hold].type == IT_PROJECT ? C_HAZARD : C_BONE, 255, AL_CENTER);
    }
    /* the action under your feet, with your own button */
    int fx, fy;
    if (p->prompt[0] && project(p->x, 0.0f, p->z, &fx, &fy) && !p->bot) {
      bool karma = p->px > 0;
      uint32_t c = karma ? C_LIVE : C_HAZARD;
      const char *g = glyph_a(p);
      int tw = hud_text_w(F_MONO, p->prompt);
      int gw = hud_text_w(F_MONO_S, g) + 14;
      if (gw < 26) gw = 26;
      int tot = gw + tw + 26;
      int bx = fx - tot / 2, by = fy + 16;
      hud_rect(bx + 4, by + 4, tot, 32, C_SHADOW, 220);
      hud_rect(bx, by, tot, 32, C_CHAR, 240);
      /* holding the button fills the prompt itself: your progress is always under your feet */
      float done = p->work > 0 && p->work_need > 0 ? fminf(1, p->work / p->work_need) : 0;
      if (done > 0) {
        hud_rect(bx, by, (int)(tot * done), 32, c, 90);
        hud_rect(bx, by + 26, (int)(tot * done), 6, c, 255);
      }
      hud_frame(bx, by, tot, 32, 2, c, 255);
      hud_keycap(bx + 6, by + 3, g, done > 0 ? C_BONE : c, C_TAPE);
      hud_text(F_MONO, bx + gw + 16, by + 3, p->prompt, done > 0 ? C_BONE : karma ? C_INK_OK : C_HAZARD, 255, AL_LEFT);
    }
    if (p->flash > 0) {
      int a = p->flash < 0.4f ? (int)(p->flash / 0.4f * 255) : 255;
      int rise = (int)((1.4f - p->flash) * 50);
      hud_text(F_H2, sx + 3, sy - 86 - rise + 3, p->pop, C_SHADOW, a, AL_CENTER);
      hud_text(F_H2, sx, sy - 86 - rise, p->pop, p->pop_color, a, AL_CENTER);
    }
    if (i == talk) {
      const char *t = PROPOSALS[p->bubble_text];
      int bw = hud_text_w(F_MONO_S, t);
      if (bw < 190) bw = 190;
      int bx = sx + 18, by = sy - 84;
      hud_panel(bx, by, bw + 20, 50, C_BONE, C_TAPE);
      hud_text(F_MONO_S, bx + 10, by + 6, "I THINK WE SHOULD", C_SMOKE, 255, AL_LEFT);
      hud_text(F_MONO_S, bx + 10, by + 26, t, C_TAPE, 255, AL_LEFT);
    }
  }
}

/* ── panels ──────────────────────────────────────────────────────── */

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static int lerpi(int a, int b, float k) { return a + (int)((b - a) * k); }
static bool phones_on(void) { return SET.phones && net_online(); }
static int seat_glyph(int seat) { return seat < 0 ? GL_BOT : seat < SEAT_KB(0) ? GL_PAD : seat < SEAT_PHONE(0) ? GL_KEYS : GL_PHONE; }

/* the phone join strip. The lobby shows it wide; in the night it starts wide
 * (SOLO, CHAOS, CUSTOM) and tweens to the narrow form at 45 s, or is narrow
 * from the start beside the mode panel (CO-OP, VERSUS). Hidden with phones
 * off or the relay down */
static void join_strip_k(float k) {
  if (!phones_on()) return;
  int x = lerpi(944, 1144, k), w = lerpi(320, 120, k);
  hud_panel(x, 6, w, 56, C_SLAB, C_STEEL_HI);
  if (k < 0.5f) {
    hud_qr(x + 5, 10, 48, G.join_url);
    hud_eyebrow(x + 62, 12, "PHONES JOIN", C_ASH, AL_LEFT);
    hud_text(F_H3, x + 60, 30 - 6, G.room, C_HAZARD, 255, AL_LEFT);
  } else {
    hud_qr(x + 6, 10, 48, G.join_url);
    hud_text(F_MONO_S, x + 60, 16, G.room, C_HAZARD, 255, AL_LEFT);
    hud_eyebrow(x + 60, 38, "JOIN", C_ASH, AL_LEFT);
  }
}
static void join_strip(void) {
  bool narrow = G.mode == MODE_COOP || G.mode == MODE_VERSUS;
  float k = narrow ? 1 : hud_ease_inout(clampf((G.t - 45) / 0.3f, 0, 1));
  join_strip_k(k);
}

/* 1240 reads 1,240 */
static void commas(char *b, int n, int v) {
  char t[16]; snprintf(t, sizeof t, "%d", v < 0 ? -v : v);
  int l = (int)strlen(t), o = 0;
  if (v < 0 && o < n - 1) b[o++] = '-';
  for (int i = 0; i < l && o < n - 1; i++) {
    b[o++] = t[i];
    if ((l - 1 - i) % 3 == 0 && i < l - 1 && o < n - 1) b[o++] = ',';
  }
  b[o] = 0;
}

/* the right slot by mode: CO-OP shows the team, VERSUS the leader */
static void mode_panel(void) {
  char b[48];
  if (G.mode == MODE_COOP) {
    hud_panel(944, 6, 196, 56, C_SLAB, C_STEEL_HI);
    hud_eyebrow(958, 12, "TEAM KARMA", C_ASH, AL_LEFT);
    commas(b, sizeof b, G.team_karma);
    hud_text(F_H2, 956, 22, b, C_LIVE, 255, AL_LEFT);
  } else if (G.mode == MODE_VERSUS) {
    int top = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && (top < 0 || P[i].clout > P[top].clout)) top = i;
    hud_panel(944, 6, 196, 56, C_SLAB, C_STEEL_HI);
    hud_eyebrow(958, 12, "LEADER", C_ASH, AL_LEFT);
    if (top >= 0) {
      char c[16]; commas(c, sizeof c, P[top].clout);
      snprintf(b, sizeof b, "%s %s", P[top].name, c);
      hud_clip(944, 6, 192, 56);
      hud_text(F_H3, 958, 30, b, C_HAZARD, 255, AL_LEFT);
      hud_clip_clear();
    }
  }
}

/* the widest toast that is showing, for the reserve */
static int toast_w(int i) {
  char b[49]; snprintf(b, sizeof b, "%s", toast_text[i]);   /* the drawer cuts at 48 */
  return hud_text_w(F_MONO_S, b) + 18;
}
static int toasts_widest(void) {
  int w = 0, shown = 0;
  for (int i = 0; i < TOASTS && shown < 2; i++) {
    if (toast_t[i] <= 2) continue;
    int tw = toast_w(i);
    if (tw > w) w = tw;
    shown++;
  }
  return w;
}

/* the feed: the two newest lines, right aligned under the top bar, gone in a few seconds */
static void toasts(int xr, int y) {
  int shown = 0;
  for (int i = 0; i < TOASTS && shown < 2; i++) {
    if (toast_t[i] <= 2) continue;
    float t = toast_t[i] - 2;
    int a = t < 1 ? (int)(t * 255) : 255;
    char b[49]; snprintf(b, sizeof b, "%s", toast_text[i]);
    int w = toast_w(i);
    int ty = y + shown * 24;
    hud_rect(xr - w, ty, w, 21, C_CHAR, a * 210 / 255);
    hud_rect(xr - w, ty, 4, 21, toast_col[i], a);
    hud_text(F_MONO_S, xr - w + 10, ty + 2, b, C_BONE, a, AL_LEFT);
    shown++;
  }
}

/* one message slot, just under the top bar, over the back wall: nothing you
 * play with is up there, so a message never covers the floor */
#define SLOT_Y 68
#define SLOT_H 32
static void slot_bar(const char *chip, uint32_t chip_c, const char *msg, uint32_t msg_c, int a, int dy) {
  /* step the fonts down until it fits on one line */
  int cf = F_H3, mf = F_MONO, max = HUD_W - 32;
  int cw = hud_text_w(cf, chip) + 28;
  int mw = msg && msg[0] ? hud_text_w(mf, msg) + 32 : 0;
  if (cw + mw > max && mw) { mf = F_MONO_S; mw = hud_text_w(mf, msg) + 28; }
  if (cw + mw > max) { cf = F_MONO_L; cw = hud_text_w(cf, chip) + 24; }
  int w = cw + mw;
  if (w > max) w = max;
  int x = HUD_W / 2 - w / 2, y = SLOT_Y + dy;
  hud_rect(x + 5, y + 5, w, SLOT_H, C_SHADOW, a);
  hud_rect(x, y, w, SLOT_H, C_CHAR, a);
  hud_rect(x, y, cw, SLOT_H, chip_c, a);
  hud_text(cf, x + cw / 2, y + (cf == F_H3 ? 2 : 1), chip, C_TAPE, a, AL_CENTER);
  if (mw) hud_text(mf, x + cw + 14, y + (mf == F_MONO ? 5 : 8), msg, msg_c, a, AL_LEFT);
  hud_frame(x, y, w, SLOT_H, 2, chip_c, a);
}

static void ticker_band(void) {
  if (G.ticker_t <= 0) return;
  float k = G.ticker_t > 5.1f ? (5.5f - G.ticker_t) / 0.4f : G.ticker_t < 0.4f ? G.ticker_t / 0.4f : 1;
  int a = (int)(255 * k);
  uint32_t c = G.ticker_color == C_FAULT ? C_FAULT : G.ticker_color == C_LIVE ? C_LIVE : C_HAZARD;
  slot_bar(G.ticker, c, G.ticker_sub, c == C_FAULT ? C_INK_ERR : C_BONE, a, (int)((k - 1) * 20));
}

/* a meter: eyebrow, the value right aligned at vx, a glyph at gx, the bar */
static void meter(int x, int y, int w, const char *name, int vx, int gx, int glyph, uint32_t gc, int ga,
                  float v, uint32_t fill, const char *right, uint32_t rc) {
  hud_eyebrow(x, y, name, C_BONE, AL_LEFT);
  hud_text(F_MONO_S, vx, y, right, rc, 255, AL_RIGHT);
  hud_glyph(gx, y - 2, glyph, 1, gc, ga);
  hud_bar(x, y + 20, w, 16, v / 100.f, fill, C_CHAR);
}

/* two compact rows: clock, meters and join on top; messages in a strip below.
 * Both sit above the room, never over it */
static void top_bar(void) {
  hud_panel(16, 6, 300, 56, C_SLAB, C_STEEL_HI);
  hud_logo(20, 9, 50, C_BONE);
  if (G.phase != PH_NIGHT) {
    hud_text(F_H2, 74, 14, "7:00 PM", C_BONE, 255, AL_LEFT);
    hud_text(F_MONO_S, 306, 10, "DOORS OPEN", C_ASH, 255, AL_RIGHT);
    return;   /* the lobby panel takes the meters' place */
  }
  /* how much night is left, big; the clock on the wall, small */
  int left = (int)ceilf(G.night_len - G.t);
  if (left < 0) left = 0;
  bool closing = left <= 30;
  char lt[24], clk[16];
  snprintf(lt, sizeof lt, "%d:%02d", left / 60, left % 60);
  hud_text(F_H2, 74, 10, lt, closing && fmodf(T, 0.8f) < 0.5f ? C_HAZARD : C_BONE, 255, AL_LEFT);
  clock_str(clk, sizeof clk);
  hud_text(F_MONO_S, 306, 12, clk, C_ASH, 255, AL_RIGHT);
  float f = G.t / G.night_len;
  int bx = 76, bw = 232, by = 54;
  hud_rect(bx, by, bw, 5, C_CHAR, 255);
  hud_rect(bx, by, (int)(bw * fminf(1, f)), 5, closing ? C_RUST : C_HAZARD, 255);
  for (int h = 1; h < 4; h++) hud_rect(bx + bw * h / 4, by, 2, 5, C_STEEL_HI, 255);

  hud_panel(330, 6, 600, 56, C_SLAB, C_STEEL_HI);
  float c = G.clutter > 100 ? 100 : G.clutter;
  uint32_t cf = c > 80 ? C_FAULT : c > 55 ? C_RUST : C_HAZARD;
  uint32_t mc = c > 80 ? C_FAULT : c > 55 ? C_RUST : C_SMOKE;
  int ma = c > 80 ? 150 + (int)(105 * (0.5f + 0.5f * sinf(T * 6.2832f * 4))) : 255;
  char r[48];
  if (G.fire_warn > 0) snprintf(r, sizeof r, "FIRE MARSHAL IN %d", (int)ceilf(5 - G.fire_warn));
  else snprintf(r, sizeof r, "%d", (int)c);
  meter(346, 12, 270, "CLUTTER", 596, 604, GL_MARSHAL, mc, ma, c, cf, r, G.fire_warn > 0 ? C_FAULT : C_ASH);
  snprintf(r, sizeof r, "%d", (int)G.vibe);
  meter(640, 12, 274, "VIBE", 898, 906, GL_HEART, G.vibe < 25 ? C_FAULT : C_LIVE, 255, G.vibe, G.vibe < 25 ? C_FAULT : C_LIVE, r, C_ASH);
  if (G.fire_warn > 0 && fmodf(T, 0.5f) < 0.25f) hud_frame(0, 0, HUD_W, HUD_H, 12, C_FAULT, 220);
  if (G.heat > 0) hud_rect(0, 0, HUD_W, HUD_H, 0xff7a1a, 28);
}

static void player_cards(void) {
  int n = player_count();
  if (!n) return;
  int cw = n > 4 ? 150 : 210, gap = 10;
  int total = n * cw + (n - 1) * gap;
  int x = HUD_W / 2 - total / 2, y = HUD_H - 62;
  bool coop = G.mode == MODE_COOP;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    hud_panel(x, y, cw, 50, C_SLAB, C_STEEL_HI);
    hud_rect(x, y, 8, 50, p->color, 255);
    hud_clip(x, y, cw - 28, 50);
    hud_text(F_MONO_S, x + 16, y + 4, p->name, C_BONE, 255, AL_LEFT);
    hud_clip_clear();
    hud_glyph(x + cw - 24, y + 6, p->bot ? GL_BOT : p->phone ? GL_PHONE : seat_glyph(p->seat), 1, C_SMOKE, 255);
    char b[24];
    hud_glyph(x + 14, y + 26, GL_CLOUT, 1, coop ? C_SMOKE : C_HAZARD, 255);
    snprintf(b, sizeof b, "%d", p->clout);
    hud_text(F_MONO_L, x + 34, y + 21, b, coop ? C_SMOKE : C_HAZARD, 255, AL_LEFT);
    snprintf(b, sizeof b, "%d", p->karma);
    int kw = hud_text(F_MONO_L, x + cw - 10, y + 21, b, C_INK_OK, 255, AL_RIGHT);
    hud_glyph(x + cw - 10 - kw - 20, y + 26, GL_KARMA, 1, C_INK_OK, 255);
    x += cw + gap;
  }
}

static bool danger(void) {
  if (G.phase != PH_NIGHT) return false;
  float c = G.clutter;
  if (!(c > 80 || G.vibe < 20)) return false;
  int a = fmodf(T, 0.6f) < 0.4f ? 255 : 150;
  int k = c > 80 ? 0 : 1;
  slot_bar(DANGER_LINES[k][0], C_FAULT, DANGER_LINES[k][1], C_INK_ERR, a, 0);
  return true;
}

/* which coach step is up, or -1. HELP OFF: never. FIRST NIGHT: steps 1 to 3
 * on the group's first night, 4 and 5 only if the rules flash was skipped
 * over. ALWAYS: every night, all five. Sets G.coach_st for the ash guides */
static int coach_step(void) {
  G.coach_st = -1;
  if (G.phase != PH_NIGHT || G.attract || G.t > 40 || SET.help == HELP_OFF) return -1;
  int k = (int)(G.t / 8);
  if (k > 4) k = 4;
  if (SET.help == HELP_FIRST) {
    if (G.group_nights > 1) return -1;
    if (k >= 3 && G.flash_seen) return -1;
  }
  G.coach_st = COACH_ST[k];
  return k;
}

/* the first forty seconds of a night: a coach walks you through it */
static void coach(void) {
  int k = coach_step();
  if (k < 0 || G.ticker_t > 0) return;   /* one thing at a time up top */
  float local = G.t - k * 8;
  int a = local < 0.3f ? (int)(local / 0.3f * 255) : 255;
  slot_bar(COACH_STEPS[k][0], k >= 3 ? C_LIVE : C_HAZARD, COACH_STEPS[k][1], C_BONE, a, 0);
}

/* ── full screens ────────────────────────────────────────────────── */

/* ── the front end: title, attract, main menu (FINAL-frontend.md 6.1 to 6.3) ── */

static const char *seat_cap(int seat, int btn) {
  static const char *const PAD[4] = { "A", "B", "X", "START" };
  static const char *const KB0[4] = { "SPACE", "Q", "R", "ENTER" };
  static const char *const KB1[4] = { ".", ",", "/", "RSHIFT" };
  return seat == SEAT_KB(0) ? KB0[btn] : seat == SEAT_KB(1) ? KB1[btn] : PAD[btn];
}

/* the call to action: a hazard block that breathes 8 px on a 1.4 s period and never blinks off */
static void cta(int x, int y, int w, int h, int font, const char *s, int a) {
  int g = (int)((0.5f + 0.5f * sinf(T * 4.4f)) * 8);
  int bx = x - g / 2, by = y - g / 2, bw = w + g, bh = h + g;
  hud_rect(bx + 6, by + 6, bw, bh, C_SHADOW, a);
  hud_rect(bx, by, bw, bh, C_HAZARD, a);
  hud_text(font, x + w / 2, y + (h - hud_font_h(font)) / 2, s, C_TAPE, a, AL_CENTER);
}

/* 0 the big title, 1 the mark in the corner; eased over 0.35 s between them */
static float mark_k(void) {
  bool now = M.screen != SCR_TITLE, before = M.prev_screen != SCR_TITLE;
  if (now == before) return now ? 1 : 0;
  float e = hud_ease_out(fminf(1, M.t / 0.35f));
  return now ? e : 1 - e;
}

/* 6.1: the band over the running room; f fades it out as the menu comes in */
static void title_idle(float f) {
  int a = (int)(255 * f), late = (int)(255 * clampf((f - 0.57f) / 0.43f, 0, 1));
  hud_rect(0, 190, HUD_W, 240, C_CHAR, (int)(150 * f));
  hud_rect(0, 190, HUD_W, 2, C_STEEL_HI, a);
  hud_rect(0, 428, HUD_W, 2, C_STEEL_HI, a);
  hud_set_alpha(a);
  hud_eyebrow(356, 222, "HEATSYNC LABS ARCADE", C_ASH, AL_LEFT);
  hud_set_alpha(255);
  hud_text_sh(F_TITLE, 350, 248, "DO-OCRACY", C_BONE, late, AL_LEFT);
  /* one tagline at a time, crossfading every 6 s */
  float ph = fmodf(T, 6.0f);
  float ta = ph < 0.15f ? ph / 0.15f : ph > 5.85f ? (6 - ph) / 0.15f : 1;
  hud_text_sh(F_H3, 356, 392, TAGLINES[(int)(T / 6.0f) % COUNT(TAGLINES)], C_HAZARD, (int)(late * ta), AL_LEFT);
  if (late <= 0) return;
  cta(430, 500, 420, 56, F_H2, "PRESS A", late);
  if (M.kb_seen) hud_text(F_MONO_S, 640, 566, "OR SPACE ON A KEYBOARD", C_ASH, late, AL_CENTER);
  if (M.pending_count) {
    char b[32]; snprintf(b, sizeof b, "%d AT THE DOOR", M.pending_count);
    hud_text(F_MONO_S, 640, 586, b, C_ASH, late, AL_CENTER);
  }
  if (phones_on()) {
    hud_set_alpha(late);
    hud_panel(1040, 540, 216, 124, C_SLAB, C_STEEL_HI);
    hud_qr(1052, 554, 96, G.join_url);
    hud_eyebrow(1160, 562, "PHONES", C_ASH, AL_LEFT);
    hud_text(F_H3, 1158, 584, G.room, C_HAZARD, 255, AL_LEFT);
    hud_set_alpha(255);
  }
  hud_text(F_MONO_S, 1256, 690, "HEATSYNC LABS. MESA. SINCE 2009.", C_SMOKE, late, AL_RIGHT);
}

/* 6.2: the bots get the overlays a player would, faded, under the front end */
static void attract_overlay(void) {
  hud_set_alpha(150);
  names_only = true; station_labels(); names_only = false;
  bot_guides = true; goal_guides(); bot_guides = false;
  hud_set_alpha(170);
  players_overlay();
  hud_set_alpha(255);
}

static void attract_screen(void) {
  float up = hud_ease_out(fminf(1, M.t / 0.3f));
  cta(1040, 620, 216, 44, F_H3, "PRESS A", 255);
  int y = 680 + (int)(40 * (1 - up));
  hud_rect(0, y, HUD_W, 40, C_TAPE, 255);
  hud_text(F_MONO, HUD_W / 2, y + 6, ATTRACT_TIPS[(int)(T / 3.0f) % COUNT(ATTRACT_TIPS)], C_ASH, 255, AL_CENTER);
}

/* rows come in from the left after the title clears, 0.04 s apart */
static float row_in(int i) {
  bool from_title = M.prev_screen == SCR_TITLE || M.prev_screen == SCR_ATTRACT;
  if (!from_title) return 1;
  return hud_ease_out(clampf((M.t - 0.15f - i * 0.04f) / 0.2f, 0, 1));
}

static void menu_preview(int row) {
  hud_panel(700, 200, 540, 320, C_SLAB, C_STEEL_HI);
  char b[64];
  switch (row) {
    case 0: {
      const mode_def_t *m = &MODES[SET.mode < MODE_COUNT ? SET.mode : 0];
      hud_glyph(724, 224, m->glyph, 2, m->color, 255);
      snprintf(b, sizeof b, "%s %s", m->name1, m->name2);
      hud_text(F_H3, 772, 232, b, C_BONE, 255, AL_LEFT);
      hud_text_wrap(F_MONO, 724, 280, 480, m->blurb, C_BONE, 255, 26);
      for (int k = 0; k < 3; k++) {
        if (strchr(m->rule[k], '%')) snprintf(b, sizeof b, m->rule[k], NIGHT_LENS[SET.night_len < COUNT(NIGHT_LENS) ? SET.night_len : 1].name);
        else snprintf(b, sizeof b, "%s", m->rule[k]);
        hud_glyph(724, 428 + k * 22, m->rule_glyph[k], 1, C_ASH, 255);
        hud_text(F_MONO_S, 748, 430 + k * 22, b, C_ASH, 255, AL_LEFT);
      }
      if (SET.mode == MODE_VERSUS && 1 + M.pending_count == 1 && !SET.bots) hud_text(F_MONO_S, 724, 496, NEEDS_LINE, C_INK_ERR, 255, AL_LEFT);
      break;
    }
    case 1:
      hud_eyebrow(724, 224, "FIVE WAYS TO RUN A TUESDAY", C_ASH, AL_LEFT);
      for (int i = 0; i < MODE_COUNT; i++) {
        int x = 724 + i * 100, y = 300;
        hud_rect(x + 4, y + 4, 92, 120, C_SHADOW, 255);
        hud_rect(x, y, 92, 120, C_CHAR, 255);
        hud_frame(x, y, 92, 120, i == SET.mode ? 3 : 1, i == SET.mode ? C_HAZARD : C_STEEL_HI, 255);
        hud_glyph(x + 30, y + 24, MODES[i].glyph, 2, MODES[i].color, 255);
        hud_text(F_MONO_S, x + 46, y + 80, MODES[i].short_name, i == SET.mode ? C_BONE : C_ASH, 255, AL_CENTER);
      }
      break;
    case 2: {
      int y = 240;
      snprintf(b, sizeof b, "NIGHT  %s", NIGHT_LENS[SET.night_len < COUNT(NIGHT_LENS) ? SET.night_len : 1].name);
      hud_text(F_MONO, 724, y, b, C_BONE, 255, AL_LEFT);
      snprintf(b, sizeof b, "BOTS %s   CHAOS %s   PHONES %s", ONOFF[SET.bots & 1], CHAOS_NAMES[SET.chaos <= CHAOS_CHAOS ? SET.chaos : 1], ONOFF[SET.phones & 1]);
      hud_text(F_MONO, 724, y + 30, b, C_BONE, 255, AL_LEFT);
      snprintf(b, sizeof b, "ROOM  %s", LEVELS[SET.layout < LEVEL_COUNT ? SET.layout : 0].name);
      hud_text(F_MONO, 724, y + 60, b, C_BONE, 255, AL_LEFT);
      snprintf(b, sizeof b, "MUSIC %d   SFX %d", SET.music, SET.sfx);
      hud_text(F_MONO, 724, y + 90, b, C_BONE, 255, AL_LEFT);
      break;
    }
    case 3: {
      /* the first card of the rules flash, looping: walk up, hold A, the thing is done */
      float lt = fmodf(T, 2.6f);
      hud_pict_station(1040, 270, 150, 110, S[ST_LASER].label, C_HAZARD);
      int px = 780 + (int)(200 * hud_ease_inout(fminf(1, lt / 1.0f)));
      hud_pict_person(px, 290, 90, C_HAZARD, false);
      if (lt > 1.1f) {
        float f = fminf(1, (lt - 1.1f) / 1.0f);
        hud_rect(980, 400, 200, 12, C_CHAR, 255);
        hud_rect(980, 400, (int)(200 * f), 12, C_HAZARD, 255);
        hud_keycap(940, 394, "A", C_HAZARD, C_TAPE);
      }
      hud_text_runs(F_H3, 970, 450, FLASH_CAPS[0], 255, AL_CENTER);
      break;
    }
    case 4:
      hud_logo(890, 250, 160, C_BONE);
      hud_eyebrow(970, 440, "SINCE 2009", C_ASH, AL_CENTER);
      break;
  }
}

/* AT THE DOOR: everyone who pressed A while somebody else was picking */
static void who_is_in(void) {
  if (!M.pending_count) return;
  hud_panel(700, 536, 540, 96, C_SLAB, C_STEEL_HI);
  hud_eyebrow(712, 548, "AT THE DOOR", C_ASH, AL_LEFT);
  for (int k = 0; k < M.pending_count && k < 8; k++) {
    int id = M.pending_seat[k];
    int x = 712 + (k % 4) * 132, y = 576 + (k / 4) * 24;
    char b[32]; snprintf(b, sizeof b, "P%d  %s", k + 2, M.seat_name[id]);
    hud_rect(x, y, 128, 20, C_CHAR, 255);
    hud_rect(x, y, 12, 20, PCOL[(k + 1) % MAX_PLAYERS], 255);
    hud_glyph(x + 16, y + 2, seat_glyph(id), 1, C_BONE, 255);
    hud_clip(x, y, 128, 20);
    hud_text(F_MONO_S, x + 36, y + 1, b, C_BONE, 255, AL_LEFT);
    hud_clip_clear();
  }
}

static int footer_hint(int x, int y, const char *cap, const char *label) {
  x += hud_keycap(x, y, cap, C_HAZARD, C_TAPE) + 8;
  x += hud_text(F_MONO_S, x, y + 4, label, C_BONE, 255, AL_LEFT);
  return x + 24;
}

static void owner_chip(int xr, int y) {
  if (M.owner < 0) return;
  char b[32]; snprintf(b, sizeof b, "P1  %s", M.seat_name[M.owner]);
  int w = hud_text_w(F_MONO_S, b) + 48;
  hud_rect(xr - w + 4, y + 4, w, 26, C_SHADOW, 255);
  hud_rect(xr - w, y, w, 26, PCOL[0], 255);
  hud_glyph(xr - w + 8, y + 5, seat_glyph(M.owner), 1, C_TAPE, 255);
  hud_text(F_MONO_S, xr - 10, y + 5, b, C_TAPE, 255, AL_RIGHT);
}

static void deny_line_at(int x, int y) {
  if (M.deny_t <= 0) return;
  int a = M.deny_t < 0.3f ? (int)(255 * M.deny_t / 0.3f) : 255;
  hud_text_sh(F_MONO, x, y, M.deny, C_INK_ERR, a, AL_CENTER);
}
static void deny_line(void) { deny_line_at(356, 630); }
static int deny_shake(void) { return M.deny_t > 1.8f ? (int)(6 * sinf(M.deny_t * 60) * (M.deny_t - 1.8f) / 0.2f) : 0; }

static void menu_main(float dt) {
  int ox = M.deny_t > 1.8f ? (int)(6 * sinf(M.deny_t * 60) * (M.deny_t - 1.8f) / 0.2f) : 0;
  float in0 = row_in(0);
  hud_set_alpha((int)(255 * in0));
  hud_eyebrow(108, 100, "HEATSYNC LABS ARCADE", C_ASH, AL_LEFT);
  hud_set_alpha(255);
  /* the cursor block eases to the row with a small overshoot */
  float target = 200 + M.row * 64;
  hud_tween(&M.cur_y, target, 18, dt);
  float over = 3 * M.cur_dir * (1 - hud_ease_back(fminf(1, M.snap_t / 0.22f)));
  int cy = (int)(M.cur_y + over);
  int ca = (int)(255 * row_in(M.row));
  int cx = 96 + ox - (int)(60 * (1 - row_in(M.row)));
  hud_rect(cx + 6, cy + 6, 520, 56, C_SHADOW, ca);
  hud_rect(cx, cy, 520, 56, M.sel_t > 0 ? C_BONE : C_HAZARD, ca);
  for (int i = 0; i < COUNT(MENU_ROWS); i++) {
    float e = row_in(i);
    int a = (int)(255 * e), x = 96 + ox - (int)(60 * (1 - e)), y = 200 + i * 64;
    bool covered = fabsf(M.cur_y - y) < 28;
    if (covered) hud_text(F_H2, x + 24, y + 6, MENU_ROWS[i], C_TAPE, a, AL_LEFT);
    else hud_text_sh(F_H2, x + 24, y + 6, MENU_ROWS[i], C_BONE, a, AL_LEFT);
    if (i == 0) {
      /* PLAY carries the mode; left and right step it */
      int slide = M.val_slide > 0 ? (int)(12 * M.val_dir * (1 - hud_ease_back(1 - M.val_slide / 0.18f))) : 0;
      const char *v = MODES[SET.mode < MODE_COUNT ? SET.mode : 0].short_name;
      uint32_t vc = covered ? C_TAPE : C_ASH;
      hud_text(F_MONO_L, x + 480 + slide, y + 11, v, vc, a, AL_RIGHT);
      if (M.row == 0) {
        int vw = hud_text_w(F_MONO_L, v);
        hud_chevron((float)(x + 480 - vw - 16), (float)(y + 28), -1, 0, 14, 4, C_TAPE, a);
        hud_chevron((float)(x + 500), (float)(y + 28), 1, 0, 14, 4, C_TAPE, a);
      }
    }
  }
  float pin = row_in(COUNT(MENU_ROWS) - 1);
  hud_set_alpha((int)(255 * pin));
  menu_preview(M.row);
  who_is_in();
  deny_line();
  /* footer: the owner's own buttons, and who is driving */
  int fx = 96;
  fx = footer_hint(fx, 672, seat_cap(M.owner, 0), "SELECT");
  fx = footer_hint(fx, 672, seat_cap(M.owner, 1), "BACK");
  if (M.row == 0) {
    hud_chevron((float)fx + 8, 685, -1, 0, 12, 4, C_HAZARD, 255);
    hud_chevron((float)fx + 26, 685, 1, 0, 12, 4, C_HAZARD, 255);
    hud_text(F_MONO_S, fx + 42, 676, "CHANGE", C_BONE, 255, AL_LEFT);
  }
  owner_chip(1184, 670);
  hud_set_alpha(255);
}


/* ── MODES (6.4): five cards, the focused one lifted ── */

static int seated_count(void) { return (M.owner >= 0) + M.pending_count; }

static void mode_card(int i, int x, int y, int w, int h, int a) {
  const mode_def_t *m = &MODES[i];
  float lf = M.lift[i];
  int sh = 6 + (int)(4 * lf);
  hud_rect(x + sh, y + sh, w, h, C_SHADOW, a);
  hud_rect(x, y, w, h, (i == M.col && M.sel_t > 0) ? C_BONE : C_SLAB, a);
  if (lf > 0.5f) hud_frame(x, y, w, h, 3, C_HAZARD, a);
  else hud_frame(x, y, w, h, 2, C_STEEL_HI, a);
  hud_rect(x, y, w, 8, m->color, a);
  hud_glyph(x + 16, y + 24, m->glyph, 2, m->color, a);
  hud_text(F_H3, x + 16, y + 66, m->name1, C_BONE, a, AL_LEFT);
  hud_text(F_H3, x + 16, y + 96, m->name2, C_BONE, a, AL_LEFT);
  bool short_of = i == MODE_VERSUS && seated_count() <= 1 && !SET.bots;
  hud_text(F_MONO_S, x + 16, y + 134, m->players, short_of ? C_INK_ERR : C_ASH, a, AL_LEFT);
  if (short_of) hud_text(F_MONO_S, x + 16, y + 152, "NEEDS A RIVAL", C_INK_ERR, a, AL_LEFT);
  hud_set_alpha(a);
  hud_text_wrap(F_MONO_S, x + 16, y + 176, w - 32, m->blurb, C_BONE, 255, 20);
  hud_set_alpha(255);
  char b[48];
  int ry = y + h - 100;
  hud_rect(x + 16, ry - 14, w - 32, 2, C_STEEL, a);
  for (int k = 0; k < 3; k++) {
    if (strchr(m->rule[k], '%')) snprintf(b, sizeof b, m->rule[k], NIGHT_LENS[SET.night_len < COUNT(NIGHT_LENS) ? SET.night_len : 1].name);
    else snprintf(b, sizeof b, "%s", m->rule[k]);
    hud_glyph(x + 12, ry + k * 26, m->rule_glyph[k], 1, m->color, a);
    hud_clip(x + 34, ry + k * 26 - 2, w - 38, 22);
    hud_text(F_MONO_S, x + 34, ry + k * 26, b, C_ASH, a, AL_LEFT);
    hud_clip_clear();
  }
}

static void menu_modes(float dt) {
  int ox = deny_shake();
  int in = (int)(255 * fminf(1, M.t / 0.14f));
  hud_set_alpha(in);
  hud_eyebrow(108, 100, "MODES", C_ASH, AL_LEFT);
  hud_set_alpha(255);
  for (int i = 0; i < MODE_COUNT; i++) {
    hud_tween(&M.lift[i], i == M.col ? 1 : 0, 16, dt);
    float lf = M.lift[i];
    int x = 48 + i * 240 + ox, y = 180 - (int)(20 * lf), h = 400 + (int)(40 * lf);
    float e = hud_ease_out(clampf((M.t - i * 0.04f) / 0.2f, 0, 1));
    mode_card(i, x, y + (int)(30 * (1 - e)), 224, h, (int)(in * e));
  }
  deny_line_at(640, 628);
  hud_set_alpha(in);
  int fx = footer_hint(64, 672, seat_cap(M.owner, 0), "PICK");
  fx = footer_hint(fx, 672, seat_cap(M.owner, 1), "BACK");
  hud_chevron((float)fx + 8, 685, -1, 0, 12, 4, C_HAZARD, 255);
  hud_chevron((float)fx + 26, 685, 1, 0, 12, 4, C_HAZARD, 255);
  hud_text(F_MONO_S, fx + 42, 676, "LOOK", C_BONE, 255, AL_LEFT);
  owner_chip(1232, 670);
  hud_set_alpha(255);
}

/* ── SETTINGS (6.5): ten rows on the left, help or the room map on the right ── */

static const char *settings_value(int row, char *b, int n) {
  switch (row) {
    case SET_ROW_NIGHT: return NIGHT_LENS[SET.night_len < COUNT(NIGHT_LENS) ? SET.night_len : 1].name;
    case SET_ROW_BOTS: return ONOFF[SET.bots & 1];
    case SET_ROW_CHAOS: return CHAOS_NAMES[SET.chaos <= CHAOS_CHAOS ? SET.chaos : 1];
    case SET_ROW_PHONES: return ONOFF[SET.phones & 1];
    case SET_ROW_MUSIC: snprintf(b, n, "%d", SET.music); return b;
    case SET_ROW_SFX: snprintf(b, n, "%d", SET.sfx); return b;
    case SET_ROW_ROOM: return LEVELS[SET.layout < LEVEL_COUNT ? SET.layout : 0].name;
    case SET_ROW_HELP: return HELP_NAMES[SET.help <= HELP_ALWAYS ? SET.help : 1];
    default: return "";
  }
}

/* -1 no chevrons, else bit 0 left shown, bit 1 right shown, bit 2 dimmed (one entry) */
static int settings_chevrons(int row) {
  int v, n;
  switch (row) {
    case SET_ROW_NIGHT: v = SET.night_len; n = COUNT(NIGHT_LENS); break;
    case SET_ROW_CHAOS: v = SET.chaos; n = COUNT(CHAOS_NAMES); break;
    case SET_ROW_HELP: v = SET.help; n = COUNT(HELP_NAMES); break;
    case SET_ROW_BOTS: case SET_ROW_PHONES: return 3;
    case SET_ROW_ROOM: return LEVEL_COUNT > 1 ? 3 : 7;
    default: return -1;
  }
  return (v > 0 ? 1 : 0) | (v < n - 1 ? 2 : 0);
}

static void layout_preview(int idx) {
  const level_t *lv = &LEVELS[idx < LEVEL_COUNT ? idx : 0];
  const float sc = 14.3f;
  #define MX(x) (864 + (int)(((x) + 12.3f) * sc))
  #define MY(z) (200 + (int)(((z) + 7.15f) * sc))
  hud_rect(MX(-12.3f), MY(-7.15f), (int)(24.6f * sc), (int)(14.3f * sc), C_CHAR, 255);
  hud_frame(MX(-12.3f) - 2, MY(-7.15f) - 2, (int)(24.6f * sc) + 4, (int)(14.3f * sc) + 4, 2, C_STEEL_HI, 255);
  for (int i = 0; i < lv->ob_n; i++) {
    const obst_t *o = &lv->ob[i];
    hud_rect(MX(o->x - o->hw), MY(o->z - o->hd), (int)(2 * o->hw * sc) + 1, (int)(2 * o->hd * sc) + 1, C_STEEL, 255);
  }
  for (int i = 0; i < ST_COUNT; i++) {
    const st_place_t *p = &lv->st[i];
    if (!p->present || i == ST_SWITCH) continue;
    bool clout = i <= ST_BENCH || i == ST_SHOW;
    hud_frame(MX(p->x - p->hw), MY(p->z - p->hd), (int)(2 * p->hw * sc) + 1, (int)(2 * p->hd * sc) + 1, 2, clout ? C_HAZARD : C_LIVE, 255);
  }
  /* the front door: a bone gap in the street wall */
  hud_rect(MX(lv->door_x - lv->door_hw), MY(7.15f) - 1, (int)(2 * lv->door_hw * sc), 5, C_BONE, 255);
  #undef MX
  #undef MY
  hud_text(F_H3, 864, 420, lv->name, C_BONE, 255, AL_LEFT);
  hud_text_wrap(F_MONO_S, 864, 456, 352, lv->blurb, C_ASH, 255, 20);
}

static void settings_help(int row) {
  hud_eyebrow(864, 176, SETTING_ROWS[row], C_ASH, AL_LEFT);
  const char *line = row == SET_ROW_DONE && M.settings_then == TO_LOBBY ? SETTINGS_HELP_DONE_LOBBY : SETTINGS_HELP[row];
  hud_text_wrap(F_MONO, 864, 210, 352, line, C_BONE, 255, 26);
  if (settings_pinned(row)) {
    char b[48]; snprintf(b, sizeof b, "SET BY %s. PICK CUSTOM TO CHANGE IT.", MODES[SET.mode].short_name);
    hud_text_wrap(F_MONO_S, 864, 300, 352, b, C_SMOKE, 255, 20);
  }
}

static void menu_settings(float dt) {
  int ox = deny_shake();
  int in = (int)(255 * fminf(1, M.t / 0.14f));
  hud_set_alpha(in);
  hud_eyebrow(108, 100, "SETTINGS", C_ASH, AL_LEFT);
  hud_rect(96 + 6, 146, 720, 500, C_SHADOW, 255);
  hud_panel(96, 140, 720, 500, C_SLAB, C_STEEL_HI);
  hud_rect(846, 146, 400, 500, C_SHADOW, 255);
  hud_panel(840, 140, 400, 500, C_SLAB, C_STEEL_HI);
  float target = 174 + M.row * 44;
  hud_tween(&M.cur_y, target, 18, dt);
  float over = 3 * M.cur_dir * (1 - hud_ease_back(fminf(1, M.snap_t / 0.22f)));
  int cy = (int)(M.cur_y + over);
  hud_rect(104 + ox + 4, cy + 4, 704, 44, C_SHADOW, 255);
  hud_rect(104 + ox, cy, 704, 44, M.sel_t > 0 ? C_BONE : C_HAZARD, 255);
  char b[16];
  for (int i = 0; i < SET_ROW_COUNT; i++) {
    int y = 176 + i * 44;
    bool on = fabsf(M.cur_y - (y - 2)) < 22, pin = settings_pinned(i);
    uint32_t lc = on ? C_TAPE : pin ? C_SMOKE : C_BONE, vc = on ? C_TAPE : pin ? C_SMOKE : C_ASH;
    hud_text(F_MONO, 120 + ox, y + 8, SETTING_ROWS[i], lc, 255, AL_LEFT);
    if (i >= SET_ROW_RESET) continue;
    bool slide_me = M.val_slide > 0 && (M.val_dir == 2 || i == M.row);
    int sl = slide_me ? (int)(12 * (M.val_dir == 2 ? 1 : M.val_dir) * (1 - hud_ease_back(1 - M.val_slide / 0.18f))) : 0;
    const char *v = settings_value(i, b, sizeof b);
    int vx = 776 + ox + sl;
    if (i == SET_ROW_MUSIC || i == SET_ROW_SFX) {
      int lv = i == SET_ROW_MUSIC ? SET.music : SET.sfx;
      for (int k = 0; k < 10; k++) hud_rect(480 + ox + k * 26, y + 13, 24, 14, k < lv ? C_LIVE : (on ? C_HAZARD_DP : C_STEEL), 255);
      hud_text(F_MONO, vx, y + 8, v, vc, 255, AL_RIGHT);
      continue;
    }
    int vw = hud_text_w(F_MONO, v);
    hud_text(F_MONO, vx, y + 8, v, vc, 255, AL_RIGHT);
    if (pin) {
      char sfx[24]; snprintf(sfx, sizeof sfx, "(%s)", MODES[SET.mode].short_name);
      hud_text(F_MONO_S, vx - vw - 10, y + 12, sfx, vc, 255, AL_RIGHT);
      continue;
    }
    int ch = settings_chevrons(i);
    if (ch < 0) continue;
    uint32_t cc = (ch & 4) ? C_SMOKE : on ? C_TAPE : C_ASH;
    if (ch & 1) hud_chevron((float)(776 + ox - vw - 16), (float)(y + 20), -1, 0, 10, 3, cc, 255);
    if (ch & 2) hud_chevron((float)(788 + ox), (float)(y + 20), 1, 0, 10, 3, cc, 255);
  }
  if (M.row == SET_ROW_ROOM && LEVEL_COUNT > 1) { hud_eyebrow(864, 176, SETTING_ROWS[SET_ROW_ROOM], C_ASH, AL_LEFT); layout_preview(SET.layout); }
  else settings_help(M.row);
  deny_line_at(456, 646);
  hud_chevron(104, 685, -1, 0, 12, 4, C_HAZARD, 255);
  hud_chevron(122, 685, 1, 0, 12, 4, C_HAZARD, 255);
  hud_text(F_MONO_S, 138, 676, "CHANGE", C_BONE, 255, AL_LEFT);
  int fx = footer_hint(138 + hud_text_w(F_MONO_S, "CHANGE") + 24, 672, seat_cap(M.owner, 0), "PICK");
  footer_hint(fx, 672, seat_cap(M.owner, 1), "BACK");
  owner_chip(1240, 670);
  hud_set_alpha(255);
}

/* CREDITS lands in a later stage; each is a
 * card that says so and returns to the menu on any button */
static void menu_unbuilt(void) {
  static const char *const NAMES[] = { "", "", "", "MODES", "SETTINGS", "HOW TO PLAY", "CREDITS" };
  int ox = (int)(60 * (1 - hud_ease_out(fminf(1, M.t / 0.14f))));
  int a = (int)(255 * fminf(1, M.t / 0.14f));
  hud_set_alpha(a);
  hud_eyebrow(108 + ox, 100, NAMES[M.screen < SCR_NONE ? M.screen : 0], C_ASH, AL_LEFT);
  hud_rect(96 + ox + 6, 206, 620, 150, C_SHADOW, 255);
  hud_panel(96 + ox, 200, 620, 150, C_SLAB, C_STEEL_HI);
  hud_rect(96 + ox, 200, 620, 8, C_HAZARD, 255);
  hud_text(F_H3, 120 + ox, 228, NAMES[M.screen < SCR_NONE ? M.screen : 0], C_BONE, 255, AL_LEFT);
  hud_text(F_MONO, 120 + ox, 276, UNBUILT_LINE, C_ASH, 255, AL_LEFT);
  footer_hint(96, 672, seat_cap(M.owner, 1), "BACK");
  deny_line();
  owner_chip(1184, 670);
  hud_set_alpha(255);
}

/* ── the rules flash (6.6): five cards, one picture and one line each ── */

static float lin(float lt, float a, float b) { return clampf((lt - a) / (b - a), 0, 1); }

/* the seat whose buttons the flash names */
static const char *flash_glyph(void) {
  int seat = M.owner;
  if (seat < 0) return "A";
  return seat_cap(seat, 0);
}

/* a score pop rising 40 px over 0.5 s */
static void flash_pop(int x, int y, const char *s, uint32_t c, float t) {
  if (t < 0) return;
  float e = hud_ease_out(fminf(1, t / 0.5f));
  hud_text_sh(F_H2, x, y - (int)(40 * e), s, c, 255, AL_CENTER);
}

/* one card's picture with its origin at (300 + ox, 150) */
static void flash_card(int step, float lt, int ox) {
  int X = 300 + ox, Y = 150;
  switch (step) {
    case 0: {   /* HOLD A. DO THE THING. */
      hud_pict_station(X + 240, Y + 20, 200, 100, "LASER", C_HAZARD);
      hud_pict_person(X + 340, Y + 150, 72, PCOL[0], false);
      float done = lin(lt, 0.6f, 2.0f);
      int bx = X + 240, by = Y + 236, bw = 200;
      hud_rect(bx + 4, by + 4, bw, 32, C_SHADOW, 220);
      hud_rect(bx, by, bw, 32, C_CHAR, 240);
      if (done > 0) {
        hud_rect(bx, by, (int)(bw * done), 32, C_HAZARD, 90);
        hud_rect(bx, by + 26, (int)(bw * done), 6, C_HAZARD, 255);
      }
      hud_frame(bx, by, bw, 32, 2, C_HAZARD, 255);
      bool pulse = done < 1 && done > 0 && fmodf(lt, 0.5f) < 0.06f;
      int kw = hud_keycap(bx + 6, by + 3, flash_glyph(), pulse || done > 0 ? C_BONE : C_HAZARD, C_TAPE);
      hud_text(F_MONO, bx + kw + 16, by + 3, "CUT", done > 0 ? C_BONE : C_HAZARD, 255, AL_LEFT);
      if (lt >= 2.0f) flash_pop(X + 525, Y + 170, "+1040 CLOUT", C_HAZARD, lt - 2.0f);
      break;
    }
    case 1: {   /* FOLLOW THE ARROW. */
      float pa = 160 + 80 * (0.5f + 0.5f * sinf(T * 6));
      hud_pict_station(X + 480, Y + 40, 160, 100, "SHOW & TELL", C_HAZARD);
      hud_frame(X + 472, Y + 32, 176, 116, 5, PCOL[0], (int)pa);
      int px = X + 80 + (int)(320 * hud_ease_inout(lin(lt, 0.4f, 1.8f)));
      hud_pict_person(px, Y + 150, 72, PCOL[0], false);
      if (lt < 1.85f) hud_pict_item(px + 20, Y + 120, 1, C_HAZARD);
      else hud_pict_item(X + 560, Y + 62, 1, C_HAZARD);   /* in the box */
      float ph = fmodf(T * 2.5f, 1) * 44;
      for (float cx = px + 40 + ph; cx < X + 470; cx += 44) {
        float fade = fminf(1, (cx - px - 40) / 40.f);
        hud_chevron(cx, (float)(Y + 190), 1, 0, 22, 6, PCOL[0], (int)(230 * fade));
      }
      break;
    }
    case 2: {   /* AMBER IS YOURS. TEAL IS OURS. */
      float f = lin(lt, 0.3f, 1.5f);
      char b[16];
      hud_rect(X + 20, Y + 20, 8, 240, C_HAZARD, 255);
      hud_text(F_H1, X + 44, Y + 20, "CLOUT", C_HAZARD, 255, AL_LEFT);
      hud_glyph(X + 44, Y + 100, GL_CLOUT, 3, C_HAZARD, 255);
      hud_pict_item(X + 120, Y + 110 + 14, 1, C_HAZARD);
      snprintf(b, sizeof b, "+%d", (int)(900 * f));
      hud_text(F_MONO_L, X + 44, Y + 170, b, C_HAZARD, 255, AL_LEFT);
      hud_eyebrow(X + 44, Y + 220, "FOR YOU", C_ASH, AL_LEFT);
      hud_rect(X + 350, Y + 20, 8, 240, C_LIVE, 255);
      hud_text(F_H1, X + 374, Y + 20, "KARMA", C_LIVE, 255, AL_LEFT);
      hud_glyph(X + 374, Y + 100, GL_KARMA, 3, C_LIVE, 255);
      hud_glyph(X + 440, Y + 100, GL_BROOM, 3, C_LIVE, 255);
      snprintf(b, sizeof b, "+%d", (int)(40 * f));
      hud_text(F_MONO_L, X + 374, Y + 170, b, C_LIVE, 255, AL_LEFT);
      hud_eyebrow(X + 374, Y + 220, "FOR THE SPACE", C_ASH, AL_LEFT);
      break;
    }
    case 3: {   /* CLUTTER 100: FIRE MARSHAL. */
      float v = 100 * lin(lt, 0.3f, 1.9f);
      bool out = lt >= 2.3f;
      if (out) hud_set_alpha(120);
      hud_eyebrow(X + 40, Y + 60, "CLUTTER", C_BONE, AL_LEFT);
      hud_bar(X + 40, Y + 90, 600, 40, v / 100, v > 80 ? C_FAULT : v > 55 ? C_RUST : C_HAZARD, C_CHAR);
      hud_rect(X + 20, Y + 238, 640, 3, C_STEEL_HI, 255);
      static const int at[4] = { 15, 35, 55, 75 }, ix[4] = { 60, 200, 340, 520 }, kind[4] = { 2, 2, 3, 2 };
      for (int k = 0; k < 4; k++) {
        if (v < at[k]) continue;
        float t0 = 0.3f + 1.6f * at[k] / 100.f;
        float d = hud_ease_out(fminf(1, (lt - t0) / 0.15f));
        hud_pict_item(X + ix[k], Y + 200 + (int)(20 * d), kind[k], kind[k] == 3 ? C_FAULT : C_RUST);
      }
      if (v >= 100) {
        float since = lt - 1.9f;
        int sc = since < 0.1f ? 5 : 4;
        hud_glyph(X + 600 - (sc - 4) * 8, Y + 40 - (sc - 4) * 8, GL_MARSHAL, sc, C_FAULT, 255);
      }
      if (out) hud_set_alpha(255);
      break;
    }
    case 4: {   /* SAY HI OR THEY LEAVE. */
      bool hi = lt >= 1.7f;
      float frac = hi ? 1 : 1 - 0.7f * lin(lt, 0, 1.6f);
      hud_pict_person(X + 480, Y + 150, 72, 0, true);
      hud_circle(X + 504, Y + 120, 20, C_CHAR, 235);
      hud_ring(X + 504, Y + 120, 20, 5, frac, hi ? C_BONE : frac < 0.35f ? C_FAULT : C_LIVE, 255);
      if (hi) hud_glyph(X + 488, Y + 104, GL_CHECK, 2, C_LIVE, 255);
      else hud_text(F_H3, X + 504, Y + 104, "?", C_BONE, 255, AL_CENTER);
      int px = X + 120 + (int)(300 * hud_ease_inout(lin(lt, 0.4f, 1.6f)));
      hud_pict_person(px, Y + 150, 72, PCOL[0], false);
      if (hi) {
        float w = lt - 1.7f;
        int wob = w < 0.5f ? (int)(4 * sinf(w * 6.2832f * 8)) : 0;
        hud_glyph(X + 440 + wob, Y + 90, GL_HAND, 3, PCOL[0], 255);
        flash_pop(X + 504, Y + 60, "+100 KARMA", C_LIVE, w);
      }
      break;
    }
  }
}

static void rules_flash(void) {
  float end = M.flash_end > 0 ? 1 - M.flash_end / 0.25f : 0;   /* 0..1 as it drops away */
  int drop = (int)(40 * hud_ease_out(end));
  int base = (int)(255 * (1 - end));
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, (int)(120 * (1 - end)));
  hud_set_alpha(base);
  int cy = 110 + drop;
  hud_rect(260 + 8, cy + 8, 760, 440, C_SHADOW, 255);
  hud_panel(260, cy, 760, 440, C_SLAB, C_STEEL_HI);
  hud_tape(262, cy + 2, 756, 24, T * 30);
  /* the wipe: out to the left over 0.15 s, in from the right over 0.15 s */
  int ox = 0, a = 255;
  if (M.flash_wipe > 0) {
    float w = 0.3f - M.flash_wipe;   /* 0..0.3 elapsed; the step changed at 0.15 */
    if (w < 0.15f) { ox = -(int)(60 * w / 0.15f); a = (int)(255 * (1 - w / 0.15f)); }
    else { ox = (int)(60 * (1 - (w - 0.15f) / 0.15f)); a = (int)(255 * (w - 0.15f) / 0.15f); }
  }
  int step = M.flash_step > 4 ? 4 : M.flash_step;
  float lt = M.flash_wipe > 0 && 0.3f - M.flash_wipe < 0.15f ? 2.6f : M.flash_t;
  hud_set_alpha(a * base / 255);
  hud_clip(300, 150 + drop, 680, 300);
  flash_card(step, lt, ox);
  if (step == 3 && lt >= 1.9f && lt < 2.5f && fmodf(lt - 1.9f, 0.5f) < 0.25f) hud_frame(260, cy, 760, 440, 12, C_FAULT, 255);
  hud_clip_clear();
  hud_text_runs(F_H2, 640 + ox, 482 + drop, FLASH_CAPS[step], 255, AL_CENTER);
  hud_set_alpha(base);
  hud_steps(640, 528 + drop, 5, step, M.flash_wipe > 0 ? 0 : fminf(1, M.flash_t / 2.6f));
  /* the keys, centred under the card */
  const char *ca = seat_cap(M.owner, 0), *cb = seat_cap(M.owner, 1);
  const char *la = step == 4 ? "DONE" : "NEXT";
  int w = hud_text_w(F_MONO_S, ca) + 14 + 8 + hud_text_w(F_MONO_S, la) + 24 + hud_text_w(F_MONO_S, cb) + 14 + 8 + hud_text_w(F_MONO_S, "SKIP");
  int x = 640 - w / 2;
  x = footer_hint(x, 572 + drop, ca, la);
  footer_hint(x, 572 + drop, cb, "SKIP");
  hud_set_alpha(255);
}

static void front_end(float dt) {
  int scrim = 70;
  if (M.screen == SCR_ATTRACT) scrim = (int)(70 * (1 - fminf(1, M.t / 0.5f)));
  else if (M.prev_screen == SCR_ATTRACT && M.t < 0.3f) scrim = (int)(70 * M.t / 0.3f);
  if (scrim > 0) hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, scrim);
  hud_tape(0, 0, HUD_W, 24, T * 30);
  float k = mark_k();
  if (k < 1) title_idle(1 - k);
  hud_logo(lerpi(150, 32, k), lerpi(210, 40, k), lerpi(180, 64, k), C_BONE);
  if (k > 0.45f) hud_text_sh(F_H2, 108, 52, "DO-OCRACY", C_BONE, (int)(255 * clampf((k - 0.45f) / 0.55f, 0, 1)), AL_LEFT);
  switch (M.screen) {
    case SCR_TITLE: break;
    case SCR_ATTRACT: attract_screen(); break;
    case SCR_MAIN: menu_main(dt); break;
    case SCR_FLASH: rules_flash(); break;
    case SCR_MODES: menu_modes(dt); break;
    case SCR_SETTINGS: menu_settings(dt); break;
    default: menu_unbuilt(); break;
  }
}

/* ── the lobby (6.8) ─────────────────────────────────────────────── */

static float card_age[MAX_PLAYERS], ready_age[MAX_PLAYERS];
static bool card_was[MAX_PLAYERS], ready_was[MAX_PLAYERS];
static float gone_t[MAX_PLAYERS]; static int gone_x[MAX_PLAYERS], gone_w[MAX_PLAYERS]; static uint32_t gone_c[MAX_PLAYERS];
static char gone_name[MAX_PLAYERS][14];

static void card_frame(int x, int y, int w, int h, uint32_t band, int a) {
  hud_rect(x + 6, y + 6, w, h, C_SHADOW, a);
  hud_rect(x, y, w, h, C_SLAB, a);
  hud_frame(x, y, w, h, 1, C_STEEL_HI, a);
  hud_rect(x, y, w, 8, band, a);
}

/* one seat card: kind 0 a player, 1 a bot that night start will add, 2 the phone code, 3 empty */
static void seat_card(int x, int y, int w, int h, int slot, int kind, int bot_arch, int num) {
  bool big = w > 200;
  char b[32];
  if (kind == 0) {
    const player_t *p = &P[slot];
    float e = hud_ease_back(fminf(1, card_age[slot] / 0.25f));
    y -= (int)(40 * (1 - e));
    int a = (int)(255 * fminf(1, card_age[slot] / 0.12f));
    card_frame(x, y, w, h, p->color, a);
    if (p->ready) hud_frame(x, y, w, h, 3, C_LIVE, a);
    snprintf(b, sizeof b, "P%d", num);
    if (big) {
      hud_glyph(x + 16, y + 20, seat_glyph(p->seat), 2, C_BONE, a);
      hud_text(F_H3, x + 56, y + 22, b, p->color, a, AL_LEFT);
      hud_clip(x, y, w - 8, h);
      hud_text(F_MONO, x + 16, y + 58, p->name, C_BONE, a, AL_LEFT);
      hud_clip_clear();
    } else {
      hud_glyph(x + 8, y + 16, seat_glyph(p->seat), 1, C_BONE, a);
      hud_text(F_MONO_S, x + 30, y + 18, b, p->color, a, AL_LEFT);
      hud_clip(x, y, w - 4, h);
      hud_text(F_MONO_S, x + 8, y + 44, p->name, C_BONE, a, AL_LEFT);
      hud_clip_clear();
    }
    int sy = y + 88;
    if (p->ready) {
      int s = ready_age[slot] < 0.1f ? 3 : 2;
      int gs = big ? s : 1;
      hud_glyph(x + 16 - (gs - 2) * 8, sy - 6 - (gs - 2) * 8, GL_CHECK, gs, C_LIVE, a);
      hud_text(big ? F_MONO : F_MONO_S, x + 16 + 16 * (big ? 2 : 1) + 8, sy, "READY", C_LIVE, a, AL_LEFT);
    } else {
      snprintf(b, sizeof b, "PRESS %s", seat_cap(p->seat, 3));
      hud_text(F_MONO_S, x + 16, sy + 4, b, C_ASH, a, AL_LEFT);
    }
    float hb = M.hold_b[slot];
    if (hb > 0) {
      hud_ring(x + w - 24, y + h - 20, 12, 3, hb, C_RUST, 255);
      if (p->seat == M.owner && human_count() == 1) hud_text(F_MONO_S, x + w - 42, y + h - 28, "MENU", C_RUST, 255, AL_RIGHT);
    }
  } else if (kind == 1) {
    card_frame(x, y, w, h, C_STEEL, 180);
    hud_glyph(x + (big ? 16 : 8), y + (big ? 20 : 16), GL_BOT, big ? 2 : 1, C_ASH, 180);
    hud_text(big ? F_H3 : F_MONO_S, x + (big ? 56 : 30), y + (big ? 22 : 18), game_bot_name(bot_arch), C_ASH, 180, AL_LEFT);
    hud_text(F_MONO_S, x + 16, y + 92, "BOT", C_SMOKE, 180, AL_LEFT);
  } else if (kind == 2) {
    card_frame(x, y, w, h, C_STEEL, 255);
    int q = big ? 96 : 72;
    hud_qr(x + 12, y + 16, q, G.join_url);
    if (big) {
      hud_eyebrow(x + 120, y + 24, "PHONES", C_ASH, AL_LEFT);
      hud_text(F_H2, x + 120, y + 44, G.room, C_HAZARD, 255, AL_LEFT);
    }
  } else {
    card_frame(x, y, w, h, C_STEEL, 255);
    int a = 150 + (int)(105 * (0.5f + 0.5f * sinf(T * 4.4f)));
    int kx = x + 16 + hud_keycap(x + 16, y + 44, "A", C_HAZARD, C_TAPE) + 8;
    if (big) hud_text(F_MONO, kx, y + 44, "TO JOIN", C_BONE, a, AL_LEFT);
  }
}

static void lobby_cards(float dt) {
  /* who is here: the owner first, then by slot */
  int order[MAX_PLAYERS], np = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && !P[i].bot && P[i].seat == M.owner && M.owner >= 0) order[np++] = i;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && !(np && order[0] == i)) order[np++] = i;
  int bots[2] = { 0, 0 }, nb = 0;
  if (human_count() == 1 && player_count() == 1 && G.want_bots) nb = game_mode_bots(&bots[0], &bots[1]);
  int n = np + nb + 1;
  bool big = n <= 4;
  int cards = big ? 4 : 8, w = big ? 280 : 140, x0 = big ? 56 : 52, pitch = big ? 296 : 148, y = 580, h = 120;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    bool on = P[i].active && !P[i].bot;
    if (on && !card_was[i]) card_age[i] = 0;
    if (on) card_age[i] += dt;
    if (!on && card_was[i] && gone_t[i] <= 0) {
      gone_t[i] = 0.3f; gone_c[i] = P[i].color; snprintf(gone_name[i], sizeof gone_name[i], "%s", P[i].name);
    }
    card_was[i] = on;
    bool r = on && P[i].ready;
    if (r && !ready_was[i]) ready_age[i] = 0;
    ready_age[i] += dt;
    ready_was[i] = r;
  }
  bool qr_done = false;
  for (int c = 0; c < cards; c++) {
    int x = x0 + c * pitch;
    if (c < np) {
      seat_card(x, y, w, h, order[c], 0, 0, order[c] + 1);
      gone_x[order[c]] = x; gone_w[order[c]] = w;
    } else if (c < np + nb) seat_card(x, y, w, h, -1, 1, bots[c - np], 0);
    else if (!qr_done && phones_on()) { seat_card(x, y, w, h, -1, 2, 0, 0); qr_done = true; }
    else seat_card(x, y, w, h, -1, 3, 0, 0);
  }
  /* a card that went home drops 20 px and fades */
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (gone_t[i] <= 0) continue;
    gone_t[i] -= dt;
    float f = clampf(gone_t[i] / 0.3f, 0, 1);
    int gy = y + (int)(20 * (1 - f)), a = (int)(255 * f);
    card_frame(gone_x[i], gy, gone_w[i], h, gone_c[i], a);
    hud_text(F_MONO, gone_x[i] + 16, gy + 58, gone_name[i], C_ASH, a, AL_LEFT);
  }
}

/* 3, 2, 1, GO: each digit drops 20 px and fades in over 0.2 s */
static void countdown(void) {
  const char *s = NULL;
  float local = 0;
  if (M.ready_t > 0 && M.ready_t < 3) { static const char *const D[3] = { "3", "2", "1" }; s = D[(int)M.ready_t]; local = fmodf(M.ready_t, 1); }
  else if (M.ready_t >= 3 && G.wipe_t > 0) { s = "GO"; local = 0.6f - G.wipe_t; }
  if (!s) return;
  float e = hud_ease_out(fminf(1, local / 0.2f));
  hud_text_sh(F_TITLE, 640, 220 + (int)(20 * e), s, C_HAZARD, (int)(255 * e), AL_CENTER);
}

/* the night start tape wipe: two bars close over 0.3 s and open over 0.3 s */
static void start_wipe(void) {
  if (G.wipe_t <= 0) return;
  float t = 0.6f - G.wipe_t;
  float f = t < 0.3f ? hud_ease_inout(t / 0.3f) : 1 - hud_ease_inout((t - 0.3f) / 0.3f);
  int h = 24 + (int)(336 * f);
  hud_rect(0, 0, HUD_W, h, C_TAPE, 255);
  hud_tape(0, h - 24, HUD_W, 24, T * 30);
  hud_rect(0, HUD_H - h, HUD_W, h, C_TAPE, 255);
  hud_tape(0, HUD_H - h, HUD_W, 24, T * 30);
}

static void lobby_overlay(float dt) {
  /* the lobby panel stands where the meters will be */
  hud_panel(330, 6, 600, 56, C_SLAB, C_STEEL_HI);
  hud_eyebrow(346, 12, "LOBBY", C_ASH, AL_LEFT);
  int mode = G.mode >= 0 && G.mode < MODE_COUNT ? G.mode : 0;
  hud_text(F_H3, 346, 26, MODE_LOBBY_NAMES[mode], C_BONE, 255, AL_LEFT);
  int li = mode == MODE_CHAOS ? 0 : SET.night_len < COUNT(NIGHT_LENS) ? SET.night_len : 1;
  hud_text(F_MONO_S, 914, 36, NIGHT_LENS[li].name, C_ASH, 255, AL_RIGHT);
  join_strip_k(0);
  /* doors open in: a chip and a tape that shrinks from the right */
  if (M.ready_t <= 0 && G.wipe_t <= 0) {
    int left = (int)ceilf(M.lobby_t);
    char b[32]; snprintf(b, sizeof b, "DOORS OPEN IN %d:%02d", left / 60, left % 60);
    hud_rect(16, 72, 180, 24, C_CHAR, 255);
    hud_text(F_MONO_S, 26, 76, b, C_BONE, 255, AL_LEFT);
    static float phase;
    if (!M.tape_held) phase += dt * 30;
    hud_tape(196, 72, (int)(1068 * M.lobby_t / 25), 24, phase);
  }
  /* the owner's buttons */
  int x = 16;
  x = footer_hint(x, 108, seat_cap(M.owner, 0), "TRY THINGS");
  x = footer_hint(x, 108, seat_cap(M.owner, 1), "DROP");
  x = footer_hint(x, 108, seat_cap(M.owner, 2), "PROPOSE");
  footer_hint(x, 108, seat_cap(M.owner, 3), "READY");
  if (M.status_t > 0) hud_text_sh(F_MONO, 640, 146, M.status, C_ASH, M.status_t < 0.3f ? (int)(255 * M.status_t / 0.3f) : 255, AL_CENTER);
  lobby_cards(dt);
  countdown();
}

/* ── results (6.11) ── */

static const end_variant_t *end_variant(void) {
  int v = G.end_variant < 0 ? 0 : G.end_variant;
  switch (G.end) {
    case END_FIRE: return &END_FIRE_V[v % 3];
    case END_BURNOUT: return &END_BURN_V[v % 3];
    default: return &END_OK_V[v % 2];
  }
}

/* one award per player, priority karma, clout, then the rest (8.6) */
static void results_awards(const int *order, int n, const char **aname, char award[][72]) {
  for (int i = 0; i < MAX_PLAYERS; i++) { aname[i] = NULL; award[i][0] = 0; }
  int top_k = -1, top_c = -1, top_t = -1, top_g = -1, top_m = -1, low = -1;
  int top_s = -1, top_f = -1, top_u = -1, top_d = -1, top_b = -1, top_l = -1;
  for (int j = 0; j < n; j++) {
    int i = order[j];
    if (top_k < 0 || P[i].karma > P[top_k].karma) top_k = i;
    if (top_c < 0 || P[i].clout > P[top_c].clout) top_c = i;
    if (top_t < 0 || P[i].proposals > P[top_t].proposals) top_t = i;
    if (top_g < 0 || P[i].greeted > P[top_g].greeted) top_g = i;
    if (top_m < 0 || P[i].mates > P[top_m].mates) top_m = i;
    if (top_s < 0 || P[i].shelved > P[top_s].shelved) top_s = i;
    if (top_f < 0 || P[i].fixes > P[top_f].fixes) top_f = i;
    if (top_u < 0 || P[i].unplugged > P[top_u].unplugged) top_u = i;
    if (top_d < 0 || P[i].dishes > P[top_d].dishes) top_d = i;
    if (top_b < 0 || P[i].baths > P[top_b].baths) top_b = i;
    if (top_l < 0 || P[i].flats > P[top_l].flats) top_l = i;
    if (low < 0 || P[i].tasks + P[i].clout / 500 < P[low].tasks + P[low].clout / 500) low = i;
  }
  #define AW(k, cond, nm, ...) if ((k) >= 0 && !aname[k] && (cond)) { aname[k] = nm; snprintf(award[k], 72, __VA_ARGS__); }
  AW(top_k, P[top_k].karma > 0, "JANITORIAL CO-OP", "%s", AWARD_JANITOR)
  AW(top_c, P[top_c].clout > 0, "GLAMOUR BIAS", "%s", AWARD_GLAMOUR)
  AW(top_t, P[top_t].proposals >= 3, "TALKER", AWARD_TALKER, P[top_t].proposals)
  AW(top_g, P[top_g].greeted >= 2, "GREETER", AWARD_GREETER, P[top_g].greeted)
  AW(top_b, P[top_b].baths >= 1, "NO WITNESSES", AWARD_WITNESS, P[top_b].baths)
  AW(top_l, P[top_l].flats >= 2, "GREASE MONKEY", AWARD_GREASE, P[top_l].flats)
  AW(top_m, P[top_m].mates >= 3, "CLUB-MATE SOMMELIER", AWARD_MATE, P[top_m].mates)
  AW(top_s, P[top_s].shelved >= 2, "FREE PILE CURATOR", AWARD_CURATOR, P[top_s].shelved)
  AW(top_f, P[top_f].fixes >= 1, "PRINTER WHISPERER", AWARD_WHISPER, P[top_f].fixes)
  AW(top_u, P[top_u].unplugged >= 1, "MARSHAL'S FAVORITE", AWARD_MARSHAL, P[top_u].unplugged)
  AW(top_d, P[top_d].dishes >= 4, "DISH PIT", AWARD_DISHPIT, P[top_d].dishes)
  for (int j = 0; j < n; j++) { int i = order[j]; AW(i, P[i].proposals >= 5 && P[i].tasks == 0, "BIKESHED", "%s", AWARD_BIKESHED) }
  if (n >= 2) AW(low, true, "LURKER", "%s", AWARD_LURKER)
  #undef AW
}

/* the co-op grade: C closed, B barely, A open tomorrow, S spotless */
static int coop_tier(void) {
  if (G.end != END_SURVIVED) return 0;
  if (G.vibe >= 60 && G.clutter < 40) return 3;
  if (G.vibe >= 30) return 2;
  return 1;
}

static void space_strip(void) {
  hud_rect(64, 186, 1152, 44, C_SLAB, 255);
  hud_frame(64, 186, 1152, 44, 1, C_STEEL_HI, 255);
  char b[32];
  int c = (int)G.clutter, v = (int)G.vibe;
  hud_glyph(82, 200, GL_MARSHAL, 1, c > 80 ? C_FAULT : C_SMOKE, 255);
  int x = 106 + hud_text(F_MONO_S, 106, 200, "CLUTTER AT CLOSE", C_ASH, 255, AL_LEFT) + 12;
  hud_bar(x, 202, 200, 12, c / 100.f, c > 80 ? C_FAULT : c > 55 ? C_RUST : C_LIVE, C_CHAR);
  snprintf(b, sizeof b, "%d", c);
  hud_text(F_MONO_S, x + 210, 200, b, C_BONE, 255, AL_LEFT);
  hud_glyph(492, 200, GL_HEART, 1, v < 25 ? C_FAULT : C_LIVE, 255);
  x = 516 + hud_text(F_MONO_S, 516, 200, "VIBE AT CLOSE", C_ASH, 255, AL_LEFT) + 12;
  hud_bar(x, 202, 200, 12, v / 100.f, v < 25 ? C_FAULT : C_LIVE, C_CHAR);
  snprintf(b, sizeof b, "%d", v);
  hud_text(F_MONO_S, x + 210, 200, b, C_BONE, 255, AL_LEFT);
  snprintf(b, sizeof b, "NEWBIES %d OF %d", G.newbies_greeted, G.newbies_seen);
  hud_text(F_MONO_S, 1196, 200, b, C_ASH, 255, AL_RIGHT);
}

static void record_chip(int x, int y) {
  hud_rect(x, y, 104, 20, C_HAZARD, 255);
  hud_text(F_MONO_S, x + 52, y + 2, "LAB RECORD", C_TAPE, 255, AL_CENTER);
}

static void results_screen(float dt) {
  static float last_t = 1e9f; static bool landed[8]; static float rec_fx;
  if (G.t < last_t) { memset(landed, 0, sizeof landed); rec_fx = 0; }
  last_t = G.t;
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, 228);
  fx_draw(dt);   /* the confetti over the show window reads through the scrim */
  hud_tape(0, 0, HUD_W, 24, T * 20);
  bool ok = G.end == END_SURVIVED, coop = G.mode == MODE_COOP, versus = G.mode == MODE_VERSUS;
  const end_variant_t *ev = end_variant();
  hud_eyebrow(64, 48, ev->eyebrow, ok ? C_INK_OK : C_INK_ERR, AL_LEFT);
  hud_text(hud_text_w(F_H1, ev->head) > 1160 ? F_H2 : F_H1, 60, 70, ev->head, C_BONE, 255, AL_LEFT);
  hud_text(F_MONO, 64, 150, ev->sub, ok ? C_ASH : C_INK_ERR, 255, AL_LEFT);
  space_strip();

  int order[MAX_PLAYERS], n = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active) order[n++] = i;
  if (!coop)
    for (int a = 1; a < n; a++) {
      int v = order[a], k = a - 1;
      while (k >= 0 && (ok ? P[order[k]].clout < P[v].clout : P[order[k]].karma < P[v].karma)) { order[k + 1] = order[k]; k--; }
      order[k + 1] = v;
    }
  else   /* co-op: seat order, humans first */
    for (int a = 1; a < n; a++) {
      int v = order[a], k = a - 1;
      while (k >= 0 && P[order[k]].bot && !P[v].bot) { order[k + 1] = order[k]; k--; }
      order[k + 1] = v;
    }
  const char *aname[MAX_PLAYERS]; char award[MAX_PLAYERS][72];
  results_awards(order, n, aname, award);

  int y0 = 272, top_y = 246;
  if (coop) {
    char b[24]; int tier = coop_tier();
    hud_rect(64, 246, 1152, 100, C_SLAB, 255);
    hud_rect(64, 246, 8, 100, C_LIVE, 255);
    hud_eyebrow(88, 258, "TEAM KARMA", C_ASH, AL_LEFT);
    commas(b, sizeof b, G.team_karma);
    hud_text(F_H1, 88, 270, b, C_LIVE, 255, AL_LEFT);
    hud_eyebrow(1100, 258, COOP_TIER[tier], COOP_COLOR[tier], AL_RIGHT);
    hud_text_sh(F_TITLE, 1192, 220, COOP_LETTER[tier], COOP_COLOR[tier], 255, AL_RIGHT);
    if (M.rec_team) {
      record_chip(700, 300);
      if (G.t < 3 && (rec_fx -= dt) <= 0) { rec_fx = 0.5f; fx2_burst(FX2_CONFETTI, 640, 0, C_HAZARD); }
    }
    y0 = 380; top_y = 354;
  }
  int rows = n > 6 ? 6 : n;
  int pitch = rows ? (592 - y0) / rows : 62;
  if (pitch > 62) pitch = 62;
  int rh = pitch - 4;
  bool big = rh >= 50;
  hud_eyebrow(150, top_y, "MEMBER", C_ASH, AL_LEFT);
  if (coop) {
    hud_eyebrow(560, top_y, "KARMA", C_INK_OK, AL_RIGHT);
    hud_eyebrow(680, top_y, "TASKS", C_ASH, AL_RIGHT);
  } else {
    hud_eyebrow(560, top_y, "CLOUT", C_HAZARD, AL_RIGHT);
    hud_eyebrow(680, top_y, "KARMA", C_INK_OK, AL_RIGHT);
  }
  hud_eyebrow(712, top_y, "AWARD", C_ASH, AL_LEFT);
  for (int j = 0; j < rows; j++) {
    int i = order[j], y = y0 + j * pitch;
    player_t *p = &P[i];
    float in = fminf(1, fmaxf(0, (G.t - 0.6f - j * 0.25f) * 4));
    if (in <= 0) continue;
    if (in >= 1 && !landed[j]) {
      landed[j] = true;
      audio_play(SFX_TICK);
      if (j == 0) fx2_burst(versus && ok ? FX2_RING : FX2_SPARK, 90, y + rh / 2, C_HAZARD);
    }
    int ox = (int)((1 - in) * 80), a = (int)(255 * in);
    hud_rect(64 + ox, y, 1152, rh, j & 1 ? C_SLAB : C_SLAB_HI, a);
    hud_rect(64 + ox, y, 8, rh, p->color, a);
    if (versus && ok && j == 0) hud_frame(64 + ox, y, 1152, rh, 3, C_HAZARD, a);
    char b[24];
    int ty = y + (rh - (big ? 40 : 26)) / 2;
    if (!coop) { snprintf(b, sizeof b, "%d", j + 1); hud_text(big ? F_H2 : F_H3, 86 + ox, ty - (big ? 2 : 0), b, C_SMOKE, a, AL_LEFT); }
    hud_glyph(124 + ox, y + rh / 2 - 8, seat_glyph(p->bot ? -1 : p->seat), 1, C_SMOKE, a);
    hud_clip(150 + ox, y, 300, rh);
    hud_text(big ? F_H3 : F_MONO, 150 + ox, y + rh / 2 - (big ? 14 : 11), p->name, C_BONE, a, AL_LEFT);
    hud_clip_clear();
    int ny = y + rh / 2 - (big ? 16 : 11), nf = big ? F_MONO_L : F_MONO;
    if (coop) {
      commas(b, sizeof b, p->karma);
      hud_text(nf, 560 + ox, ny, b, C_INK_OK, a, AL_RIGHT);
      snprintf(b, sizeof b, "%d", p->tasks);
      hud_text(nf, 680 + ox, ny, b, C_ASH, a, AL_RIGHT);
    } else {
      if (ok) commas(b, sizeof b, p->clout); else snprintf(b, sizeof b, "VOID");
      hud_text(nf, 560 + ox, ny, b, ok ? C_HAZARD : C_INK_ERR, a, AL_RIGHT);
      commas(b, sizeof b, p->karma);
      hud_text(nf, 680 + ox, ny, b, C_INK_OK, a, AL_RIGHT);
    }
    if (aname[i]) {
      int nw = hud_text(F_MONO_S, 712 + ox, y + rh / 2 - 19, aname[i], C_HAZARD, a, AL_LEFT);
      if (M.rec_karma == i || M.rec_clout == i) record_chip(712 + ox + nw + 12, y + rh / 2 - 21);
      hud_clip(712 + ox, y, 496, rh);
      hud_text(F_MONO_S, 712 + ox, y + rh / 2 + 2, award[i], C_BONE, a, AL_LEFT);
      hud_clip_clear();
    } else if (M.rec_karma == i || M.rec_clout == i) record_chip(712 + ox, y + rh / 2 - 10);
  }
  /* the banner */
  if (G.t > 2.5f) {
    char b[96]; b[0] = 0;
    uint32_t bg = C_LIVE;
    int top_k = -1;
    for (int j = 0; j < n; j++) if (top_k < 0 || P[order[j]].karma > P[top_k].karma) top_k = order[j];
    if (versus && !ok) { snprintf(b, sizeof b, "%s", VERSUS_LOST_BANNER); bg = C_FAULT; }
    else if (coop) snprintf(b, sizeof b, "%s", COOP_BANNER);
    else if (top_k >= 0 && P[top_k].karma > 0) snprintf(b, sizeof b, "THE REAL WINNER: %s. LEAVE IT BETTER.", P[top_k].name);
    if (b[0]) {
      int w = hud_text_w(F_H3, b) + 40;
      hud_rect(HUD_W / 2 - w / 2 + 6, 606, w, 44, C_SHADOW, 255);
      hud_rect(HUD_W / 2 - w / 2, 600, w, 44, bg, 255);
      hud_text(F_H3, HUD_W / 2, 606, b, C_TAPE, 255, AL_CENTER);
    }
  }
  /* the footer: the input lock as a shrinking tape, then the two choices */
  if (G.t < 4) {
    int w = (int)(1152 * (1 - G.t / 4));
    if (w > 0) hud_tape(64, 666, w, 24, T * 20);
    hud_rect(HUD_W / 2 - 60, 666, 120, 24, C_TAPE, 255);
    hud_text(F_MONO_S, HUD_W / 2, 670, "RESULTS", C_HAZARD, 255, AL_CENTER);
  } else {
    int seat = M.owner >= 0 ? M.owner : (P[0].active && !P[0].bot ? P[0].seat : SEAT_PAD(0));
    const char *ca = seat_cap(seat, 0), *cb = seat_cap(seat, 1);
    int w = hud_text_w(F_MONO_S, ca) + 22 + hud_text_w(F_MONO_S, "AGAIN") + 24 + hud_text_w(F_MONO_S, cb) + 22 + hud_text_w(F_MONO_S, "MENU");
    int pa = 170 + (int)(85 * (0.5f + 0.5f * sinf(T * 4.4f)));
    hud_set_alpha(pa);
    int x = footer_hint(HUD_W / 2 - w / 2, 666, ca, "AGAIN");
    footer_hint(x, 666, cb, "MENU");
    hud_set_alpha(255);
  }
}

/* ── the pause overlay (6.10): rows, the two loops, the clutter bar, the pauser's buttons ── */

static int loop_chip(int x, int y, const char *s, uint32_t c, int a) {
  int w = hud_text_w(F_MONO_S, s) + 16;
  hud_rect(x + 3, y + 3, w, 24, C_SHADOW, a);
  hud_rect(x, y, w, 24, C_CHAR, a);
  hud_frame(x, y, w, 24, 2, c, a);
  hud_text(F_MONO_S, x + 8, y + 4, s, C_BONE, a, AL_LEFT);
  return x + w;
}
static int loop_arrow(int x, int y, uint32_t c, int a) {
  hud_chevron((float)x + 10, (float)y + 12, 1, 0, 9, 3, c, a);
  return x + 20;
}

static void pause_overlay(void) {
  float f = hud_ease_out(fminf(1, M.pause_t / 0.15f));
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, (int)(160 * f));
  int a = (int)(255 * f);
  hud_set_alpha(a);
  hud_rect(236, 176, 820, 380, C_SHADOW, 255);
  hud_panel(230, 170, 820, 380, C_SLAB, C_STEEL_HI);
  hud_tape(232, 172, 816, 24, T * 20);
  hud_eyebrow(254, 210, "PAUSED", C_ASH, AL_LEFT);
  hud_eyebrow(652, 210, "HOW IT WORKS", C_ASH, AL_LEFT);
  hud_rect(632, 200, 2, 330, C_STEEL, 255);
  for (int i = 0; i < 3; i++) {
    int y = 232 + i * 52;
    bool on = i == M.pause_row;
    if (on) { hud_rect(250 + 4, y + 4, 360, 46, C_SHADOW, 255); hud_rect(250, y, 360, 46, C_HAZARD, 255); }
    hud_text(F_H3, 266, y + 9, PAUSE_ROWS[i], on ? C_TAPE : C_BONE, 255, AL_LEFT);
  }
  /* the controls of the seat that paused, in its own key names */
  int seat = M.pauser;
  int x = footer_hint(254, 404, seat_cap(seat, 0), "DO IT");
  footer_hint(x, 404, seat_cap(seat, 1), "DROP");
  footer_hint(254, 436, seat_cap(seat, 2), "PROPOSE (DOES NOTHING)");
  footer_hint(254, 468, seat_cap(seat, 3), "PAUSE");
  hud_text(F_MONO_S, 254, 508, "ANYONE CAN RESUME.", C_ASH, 255, AL_LEFT);
  /* the two loops: clout in amber, karma in teal */
  x = loop_chip(652, 236, "PLYWOOD", C_HAZARD, 255);
  x = loop_arrow(x, 236, C_HAZARD, 255);
  x = loop_chip(x, 236, "LASER", C_HAZARD, 255);
  x = loop_arrow(x, 236, C_HAZARD, 255);
  loop_chip(x, 236, "SHOW WINDOW", C_HAZARD, 255);
  hud_text(F_MONO_S, 652, 268, "+900 CLOUT", C_HAZARD, 255, AL_LEFT);
  x = loop_chip(652, 298, "MESS", C_LIVE, 255);
  x = loop_arrow(x, 298, C_LIVE, 255);
  x = loop_chip(x, 298, "TRASH", C_LIVE, 255);
  x = loop_chip(x + 14, 298, "?", C_LIVE, 255);
  x = loop_arrow(x, 298, C_LIVE, 255);
  loop_chip(x, 298, "HI", C_LIVE, 255);
  hud_text(F_MONO_S, 652, 330, "+40 KARMA", C_LIVE, 255, AL_LEFT);
  hud_eyebrow(652, 372, "CLUTTER", C_ASH, AL_LEFT);
  char b[8]; snprintf(b, sizeof b, "%d", (int)G.clutter);
  hud_text(F_MONO_S, 952, 368, b, C_ASH, 255, AL_RIGHT);
  hud_bar(652, 392, 300, 16, G.clutter / 100.f, G.clutter > 80 ? C_FAULT : G.clutter > 55 ? C_RUST : C_LIVE, C_CHAR);
  hud_glyph(962, 392, GL_MARSHAL, 1, C_FAULT, 255);
  hud_set_alpha(255);
}

/* a soft vignette, baked once: the HUD starts every frame from it */
static uint8_t *vignette;
static void bake_vignette(void) {
  vignette = calloc(HUD_W * HUD_H, 4);
  for (int y = 0; y < HUD_H; y++) for (int x = 0; x < HUD_W; x++) {
    float dx = (x - HUD_W * 0.5f) / (HUD_W * 0.62f), dy = (y - HUD_H * 0.52f) / (HUD_H * 0.66f);
    float d = sqrtf(dx * dx + dy * dy);
    float a = (d - 0.62f) / 0.55f;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    uint8_t *p = vignette + ((size_t)(HUD_H - 1 - y) * HUD_W + x) * 4;
    p[0] = 0x0a; p[1] = 0x08; p[2] = 0x06; p[3] = (uint8_t)(a * a * 170);
  }
}

void draw_hud(float dt) {
  T += dt;
  if (!vignette) bake_vignette();
  memcpy(hud_px, vignette, HUD_W * HUD_H * 4);
  if (G.phase == PH_TITLE || G.attract) {
    nplaced = 0;
    reserve(0, 0, HUD_W, 104);
    reserve(0, HUD_H - 40, HUD_W, 40);
    if (M.screen == SCR_ATTRACT) attract_overlay();
    fx_draw(dt);
    front_end(dt);
    fx2_draw(dt);
    return;
  }
  if (G.phase == PH_RESULTS) { results_screen(dt); fx2_draw(dt); return; }
  nplaced = 0;
  reserve(0, 0, HUD_W, 104);                 /* the top rows */
  if (G.phase == PH_NIGHT) reserve(0, HUD_H - 86, HUD_W, 86);   /* the player cards */
  else reserve(0, 570, HUD_W, 150);          /* the lobby seat cards */
  reserve(960, 104, 304, 52);                /* the toasts */
  int tw = toasts_widest();
  if (tw > 286) reserve(1264 - tw, 104, tw, 52);
  coach_step();                              /* G.coach_st for the ash guides */
  goal_guides();
  station_labels();
  floor_tags();
  npcs_overlay();
  fx_draw(dt);
  top_bar();
  if (G.phase == PH_LOBBY) lobby_overlay(dt);
  else { mode_panel(); join_strip(); }
  if (!danger()) { ticker_band(); if (G.ticker_t <= 0) coach(); }
  if (G.phase == PH_NIGHT) player_cards();
  toasts(1264, 108);
  /* last, so nothing on screen can ever hide your own name, prompt or progress */
  players_overlay();
  if (G.phase == PH_NIGHT) countdown();
  start_wipe();
  if (G.paused && M.screen != SCR_FLASH) pause_overlay();
  if (M.screen == SCR_FLASH) rules_flash();
  fx2_draw(dt);
}
