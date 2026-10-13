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
    case ST_BOARD: case ST_LASER: return 1.9f;
    case ST_FORGE: return 1.6f;
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
        else snprintf(st, sizeof st, "START A PRINT");
        break;
      case ST_LUMBER: pay = 1; snprintf(st, sizeof st, "PLYWOOD FOR THE LASER"); break;
      case ST_BENCH: pay = 1; snprintf(st, sizeof st, "HOLD TO SOLDER"); break;
      case ST_CUTSALOT: pay = 1; snprintf(st, sizeof st, "ALWAYS FREE. STICKERS"); break;
      case ST_SHOW: pay = 1; snprintf(st, sizeof st, "BRING PROJECTS HERE"); c = C_HAZARD; break;
      case ST_SINK:
        pay = 2;
        if (s->count) { snprintf(st, sizeof st, "%d DISHES. HOLD TO WASH", s->count); c = s->count > 5 ? C_INK_ERR : C_ASH; alarm = s->count > 5; }
        else snprintf(st, sizeof st, "CLEAN. FOR NOW");
        break;
      case ST_FRIDGE:
        if (s->smell) { snprintf(st, sizeof st, "SMELLS. HOLD TO CLEAN"); c = C_INK_ERR; alarm = true; pay = 2; }
        else snprintf(st, sizeof st, "CLUB-MATE: SPEED BOOST");
        break;
      case ST_TRASH: pay = 2; snprintf(st, sizeof st, "JUNK GOES HERE"); break;
      case ST_FREEPILE: pay = 2; snprintf(st, sizeof st, "DONATIONS GO HERE"); break;
    }
    /* the status line only when it matters: trouble, something ready, or a person close by */
    bool near = false;
    for (int k = 0; k < MAX_PLAYERS; k++) {
      player_t *p = &P[k];
      if (!p->active || p->bot) continue;
      float dx = fabsf(p->x - s->x) - s->hw, dz = fabsf(p->z - s->z) - s->hd;
      if (dx < 2.2f && dz < 2.2f) near = true;
    }
    bool ready = (i == ST_LASER && s->state == S_LOADED) || ((i == ST_PRINT1 || i == ST_PRINT2) && s->state == S_DONE);
    if (names_only) { st[0] = 0; alarm = false; }
    if (!(alarm || ready || near)) st[0] = 0;
    if (!(alarm || ready || near || s->always_label)) continue;
    uint32_t edge = pay == 1 ? C_HAZARD : pay == 2 ? C_LIVE : C_STEEL_HI;
    int w = hud_text_w(F_MONO_S, s->label);
    int sw = st[0] ? hud_text_w(F_MONO_S, st) + 12 : 0;
    int bw = (w + 16 > sw ? w + 16 : sw);
    sy = settle(sx - bw / 2, sy - 4, bw, st[0] ? 48 : 24) + 4;
    hud_rect(sx - w / 2 - 8, sy - 4, w + 16, 24, C_CHAR, 240);
    hud_rect(sx - w / 2 - 8, sy - 4, 4, 24, edge, 255);
    hud_text(F_MONO_S, sx + 2, sy, s->label, C_BONE, 255, AL_CENTER);
    if (st[0]) label(sx, sy + 24, st, c, C_TAPE, F_MONO_S);
    if (alarm) {
      float pulse = 0.5f + 0.5f * sinf(T * 8);
      int r = 14 + (int)(pulse * 3);
      hud_circle(sx, sy - 26, r, C_FAULT, 255);
      hud_text(F_H3, sx, sy - 42, "!", C_BONE, 255, AL_CENTER);
    }
  }
}

static void floor_tags(void) {
  for (int i = 0; i < MAX_ITEMS; i++) {
    item_t *it = &I[i];
    if (!it->alive || it->holder >= 0) continue;
    int sx, sy;
    if (!project(it->x, 0.45f, it->z, &sx, &sy)) continue;
    bool red = it->age > 25 && it->type != IT_PROJECT && it->type != IT_PLYWOOD;
    if (it->type == IT_SAWDUST || it->type == IT_PUDDLE || it->type == IT_DAISY) {
      /* a mess: a ring that says clean me */
      float pulse = 0.5f + 0.5f * sinf(T * 4 + i);
      hud_ring(sx, sy + 8, 15 + (int)(pulse * 3), 3, 1, it->type == IT_DAISY ? C_FAULT : C_LIVE, 200);
      label_av(sx, sy - 26, it->type == IT_DAISY ? "UNPLUG ME" : it->type == IT_PUDDLE ? "MOP ME" : "SWEEP ME", it->type == IT_DAISY ? C_INK_ERR : C_INK_OK, C_CHAR, F_MONO_S);
    } else if (it->type == IT_PROJECT) {
      label_av(sx, sy - 20, item_name(it), C_HAZARD, C_CHAR, F_MONO_S);
    } else if (it->type == IT_DONATION) {
      label_av(sx, sy - 20, item_name(it), C_BONE, C_CHAR, F_MONO_S);
      label_av(sx, sy + 2, "TO FREE PILE", C_INK_OK, C_TAPE, F_MONO_S);
    } else if (it->type != IT_PLYWOOD) {
      label_av(sx, sy - 18, "TRASH ME", C_INK_OK, C_CHAR, F_MONO_S);
    }
    if (red) {
      hud_rect(sx - 30, sy + 14, 60, 16, C_FAULT, 255);
      hud_text(F_MONO_S, sx, sy + 13, "RED TAG", C_BONE, 255, AL_CENTER);
    }
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
      label_av(sx, sy + 26, f < 0.35f ? "LEAVING! SAY HI" : "NEWBIE: SAY HI", f < 0.35f ? C_INK_ERR : C_INK_OK, C_CHAR, F_MONO_S);
    } else if (n->state == 2) {
      hud_text(F_MONO_S, sx, sy + 10, "MEMBER", C_INK_OK, 255, AL_CENTER);
    }
  }
}

/* the arrow from your hands to where the thing goes */
static void goal_guides(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active || !p->has_goal || (p->bot && !bot_guides)) continue;
    station_t *s = &S[p->goal_st];
    /* target footprint, pulsing in the player's colour */
    float pulse = 0.5f + 0.5f * sinf(T * 6);
    float m = 0.25f + pulse * 0.12f;
    float cx[4] = { s->x - s->hw - m, s->x + s->hw + m, s->x + s->hw + m, s->x - s->hw - m };
    float cz[4] = { s->z - s->hd - m, s->z - s->hd - m, s->z + s->hd + m, s->z + s->hd + m };
    float px[4], py[4];
    bool ok = true;
    for (int k = 0; k < 4; k++) ok &= projf(cx[k], 0.05f, cz[k], &px[k], &py[k]);
    if (ok) for (int k = 0; k < 4; k++) hud_line(px[k], py[k], px[(k + 1) % 4], py[(k + 1) % 4], 5, p->color, 220);
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
        hud_chevron(ax, ay, bx - ax, by - ay, 22, 6, p->color, (int)(230 * fade));
      }
    }
  }
}

static void players_overlay(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    int sx, sy;
    if (!project(p->x, 2.25f, p->z, &sx, &sy)) continue;
    /* name tag in the player's colour */
    int w = hud_text_w(F_MONO_S, p->name);
    hud_rect(sx - w / 2 - 7, sy - 2, w + 14, 22, p->color, 255);
    hud_text(F_MONO_S, sx, sy, p->name, C_TAPE, 255, AL_CENTER);
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
    if (p->bubble > 0) {
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

static void join_strip(int x, int y) {
  hud_panel(x, y, 320, 56, C_SLAB, C_STEEL_HI);
  if (net_online()) {
    hud_qr(x + 5, y + 4, 48, G.join_url);
    hud_eyebrow(x + 62, y + 8, "PHONES JOIN", C_ASH, AL_LEFT);
    hud_text(F_H3, x + 60, y + 24, G.room, C_HAZARD, 255, AL_LEFT);
    hud_text(F_MONO_S, x + 308, y + 32, "ANY TIME", C_SMOKE, 255, AL_RIGHT);
  } else {
    hud_eyebrow(x + 14, y + 10, "PHONES OFFLINE", C_ASH, AL_LEFT);
    hud_text(F_MONO_S, x + 14, y + 32, "GRAB A CONTROLLER.", C_BONE, 255, AL_LEFT);
  }
}

/* the feed: the two newest lines, right aligned under the top bar, gone in a few seconds */
static void toasts(int xr, int y) {
  int shown = 0;
  for (int i = 0; i < TOASTS && shown < 2; i++) {
    if (toast_t[i] <= 2) continue;
    float t = toast_t[i] - 2;
    int a = t < 1 ? (int)(t * 255) : 255;
    int w = hud_text_w(F_MONO_S, toast_text[i]) + 18;
    int ty = y + shown * 24;
    hud_rect(xr - w, ty, w, 21, C_CHAR, a * 210 / 255);
    hud_rect(xr - 4, ty, 4, 21, toast_col[i], a);
    hud_text(F_MONO_S, xr - w + 8, ty + 2, toast_text[i], C_BONE, a, AL_LEFT);
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

static void meter(int x, int y, int w, const char *name, const char *rule, float v, uint32_t fill, const char *right) {
  hud_eyebrow(x, y, name, C_BONE, AL_LEFT);
  hud_text(F_MONO_S, x + hud_text_w(F_MONO_S, name) * 2 + 10, y, rule, C_SMOKE, 255, AL_LEFT);
  hud_text(F_MONO_S, x + w, y, right, C_ASH, 255, AL_RIGHT);
  hud_bar(x, y + 20, w, 16, v / 100.f, fill, C_CHAR);
}

/* two compact rows: clock, meters and join on top; messages in a strip below.
 * Both sit above the room, never over it */
static void top_bar(void) {
  char clk[16];
  clock_str(clk, sizeof clk);
  hud_panel(16, 6, 300, 56, C_SLAB, C_STEEL_HI);
  hud_logo(20, 9, 50, C_BONE);
  hud_text(F_H2, 74, 14, G.phase == PH_NIGHT ? clk : "7:00 PM", C_BONE, 255, AL_LEFT);
  if (G.phase == PH_NIGHT) {
    /* how much night is left: a countdown and a bar with the hours on it */
    int left = (int)ceilf(G.night_len - G.t);
    if (left < 0) left = 0;
    bool closing = left <= 30;
    char lt[24];
    snprintf(lt, sizeof lt, "%d:%02d LEFT", left / 60, left % 60);
    hud_text(F_MONO_S, 306, 10, lt, closing && fmodf(T, 0.8f) < 0.5f ? C_HAZARD : C_ASH, 255, AL_RIGHT);
    float f = G.t / G.night_len;
    int bx = 76, bw = 232, by = 54;
    hud_rect(bx, by, bw, 5, C_CHAR, 255);
    hud_rect(bx, by, (int)(bw * fminf(1, f)), 5, closing ? C_RUST : C_HAZARD, 255);
    for (int h = 1; h < 4; h++) hud_rect(bx + bw * h / 4, by, 2, 5, C_STEEL_HI, 255);
  } else { hud_text(F_MONO_S, 306, 10, "DOORS OPEN", C_ASH, 255, AL_RIGHT); return; }   /* the lobby panel takes the meters' place */

  hud_panel(330, 6, 600, 56, C_SLAB, C_STEEL_HI);
  float c = G.clutter > 100 ? 100 : G.clutter;
  uint32_t cf = c > 80 ? C_FAULT : c > 55 ? C_RUST : C_HAZARD;
  char r[48];
  if (G.fire_warn > 0) snprintf(r, sizeof r, "FIRE MARSHAL IN %d", (int)ceilf(5 - G.fire_warn));
  else snprintf(r, sizeof r, "%d", (int)c);
  meter(346, 12, 270, "CLUTTER", "100 = FIRE MARSHAL", c, cf, r);
  snprintf(r, sizeof r, "%d", (int)G.vibe);
  meter(640, 12, 274, "VIBE", "0 = BURNOUT", G.vibe, G.vibe < 25 ? C_FAULT : C_LIVE, r);
  if (G.fire_warn > 0 && fmodf(T, 0.5f) < 0.25f) hud_frame(0, 0, HUD_W, HUD_H, 12, C_FAULT, 220);
  if (G.heat > 0) hud_rect(0, 0, HUD_W, HUD_H, 0xff7a1a, 28);
}

static void player_cards(void) {
  int n = player_count();
  if (!n) return;
  int cw = n > 4 ? 150 : 210, gap = 10;
  int total = n * cw + (n - 1) * gap;
  int x = HUD_W / 2 - total / 2, y = HUD_H - 62;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    hud_panel(x, y, cw, 50, C_SLAB, C_STEEL_HI);
    hud_rect(x, y, 8, 50, p->color, 255);
    hud_text(F_MONO_S, x + 16, y + 6, p->name, C_BONE, 255, AL_LEFT);
    const char *tag = p->bot ? "BOT" : p->phone ? "PHONE" : p->kb >= 0 ? "KEYS" : "PAD";
    hud_text(F_MONO_S, x + cw - 8, y + 6, tag, C_SMOKE, 255, AL_RIGHT);
    char b[24];
    snprintf(b, sizeof b, "%d", p->clout);
    hud_text(F_MONO_L, x + 16, y + 16, b, C_HAZARD, 255, AL_LEFT);
    snprintf(b, sizeof b, "%d", p->karma);
    hud_text(F_MONO_L, x + cw - 10, y + 16, b, C_INK_OK, 255, AL_RIGHT);
    x += cw + gap;
  }
  hud_eyebrow(HUD_W / 2 - total / 2, y - 20, "CLOUT", C_HAZARD, AL_LEFT);
  hud_eyebrow(HUD_W / 2 + total / 2, y - 20, "KARMA", C_INK_OK, AL_RIGHT);
}

static bool danger(void) {
  if (G.phase != PH_NIGHT) return false;
  float c = G.clutter;
  if (!(c > 80 || G.vibe < 20)) return false;
  int a = fmodf(T, 0.6f) < 0.4f ? 255 : 150;
  if (c > 80) slot_bar("TOO MUCH CLUTTER!", C_FAULT, "CLEAN UP BEFORE THE FIRE MARSHAL COMES", C_INK_ERR, a, 0);
  else slot_bar("VIBES ARE DYING!", C_FAULT, "SAY HI TO NEWBIES. CLEAN UP.", C_INK_ERR, a, 0);
  return true;
}

/* the first forty seconds of every night: a coach walks you through it */
static void coach(void) {
  if (G.phase != PH_NIGHT || G.t > 40) return;
  /* the stations are named by the level, through one %s each */
  static const char *const STEP[][2] = {
    { "1 CLOUT", "GRAB PLYWOOD FROM THE %s (BACK WALL)" },
    { "2 CLOUT", "LOAD IT INTO THE %s, THEN HOLD TO CUT" },
    { "3 CLOUT", "CARRY YOUR PROJECT TO THE %s (FRONT)" },
    { "4 KARMA", "TEAL RINGS ARE MESSES: HOLD TO CLEAN. JUNK GOES IN THE TRASH" },
    { "5 KARMA", "NEWBIES WITH A ? NEED A HI, OR THEY LEAVE" },
  };
  static const int STEP_ST[5] = { ST_LUMBER, ST_LASER, ST_SHOW, -1, -1 };
  int k = (int)(G.t / 8);
  if (k > 4) k = 4;
  float local = G.t - k * 8;
  int a = local < 0.3f ? (int)(local / 0.3f * 255) : 255;
  bool karma = k >= 3;
  if (G.ticker_t > 0) return;   /* one thing at a time up top */
  char msg[96];
  if (STEP_ST[k] >= 0) snprintf(msg, sizeof msg, STEP[k][1], S[STEP_ST[k]].label);
  else snprintf(msg, sizeof msg, "%s", STEP[k][1]);
  slot_bar(STEP[k][0], karma ? C_LIVE : C_HAZARD, msg, C_BONE, a, 0);
}

/* ── full screens ────────────────────────────────────────────────── */

/* ── the front end: title, attract, main menu (FINAL-frontend.md 6.1 to 6.3) ── */

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static int lerpi(int a, int b, float k) { return a + (int)((b - a) * k); }
static bool phones_on(void) { return SET.phones && net_online(); }
static int seat_glyph(int seat) { return seat < 0 ? GL_BOT : seat < SEAT_KB(0) ? GL_PAD : seat < SEAT_PHONE(0) ? GL_KEYS : GL_PHONE; }
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
      snprintf(b, sizeof b, "ROOM  %s", ROOM_NAMES[0]);
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

static void deny_line(void) {
  if (M.deny_t <= 0) return;
  int a = M.deny_t < 0.3f ? (int)(255 * M.deny_t / 0.3f) : 255;
  hud_text_sh(F_MONO, 356, 630, M.deny, C_INK_ERR, a, AL_CENTER);
}

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

/* MODES, SETTINGS, HOW TO PLAY and CREDITS land in later stages; each is a
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
  if (phones_on()) join_strip(944, 6);
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

static void results_screen(void) {
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, 215);
  hud_tape(0, 0, HUD_W, 24, T * 20);
  bool ok = G.end == END_SURVIVED;
  const char *head = ok ? "THE SPACE SURVIVES ANOTHER TUESDAY." : G.end == END_FIRE ? "THE FIRE MARSHAL SHUT IT DOWN." : "EVERYONE BURNED OUT.";
  hud_eyebrow(64, 48, ok ? "11:00 PM. LIGHTS OUT." : "HYH IS CANCELED", ok ? C_INK_OK : C_INK_ERR, AL_LEFT);
  hud_text(hud_text_w(F_H1, head) > 1160 ? F_H2 : F_H1, 60, 70, head, C_BONE, 255, AL_LEFT);
  hud_text(F_MONO, 64, 150, ok ? "CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES." : "ALL CLOUT IS VOID. NOBODY CARES ABOUT YOUR LASER MUSTACHE.",
           ok ? C_ASH : C_INK_ERR, 255, AL_LEFT);

  int order[MAX_PLAYERS], n = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active) order[n++] = i;
  for (int a = 1; a < n; a++) {
    int v = order[a], k = a - 1;
    while (k >= 0 && (ok ? P[order[k]].clout < P[v].clout : P[order[k]].karma < P[v].karma)) { order[k + 1] = order[k]; k--; }
    order[k + 1] = v;
  }
  char award[MAX_PLAYERS][64];
  const char *aname[MAX_PLAYERS];
  memset(award, 0, sizeof award);
  for (int i = 0; i < MAX_PLAYERS; i++) aname[i] = NULL;
  int top_k = -1, top_c = -1, top_t = -1, top_g = -1, top_m = -1, low = -1;
  int top_s = -1, top_f = -1, top_u = -1, top_d = -1;
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
    if (low < 0 || P[i].tasks + P[i].clout / 500 < P[low].tasks + P[low].clout / 500) low = i;
  }
  if (top_k >= 0 && P[top_k].karma > 0) { aname[top_k] = "JANITORIAL CO-OP"; snprintf(award[top_k], 64, "%s", AWARD_JANITOR); }
  if (top_c >= 0 && !aname[top_c] && P[top_c].clout > 0) { aname[top_c] = "GLAMOUR BIAS"; snprintf(award[top_c], 64, "%s", AWARD_GLAMOUR); }
  if (top_t >= 0 && !aname[top_t] && P[top_t].proposals >= 3) { aname[top_t] = "TALKER"; snprintf(award[top_t], 64, AWARD_TALKER, P[top_t].proposals); }
  if (top_g >= 0 && !aname[top_g] && P[top_g].greeted >= 2) { aname[top_g] = "GREETER"; snprintf(award[top_g], 64, AWARD_GREETER, P[top_g].greeted); }
  if (top_m >= 0 && !aname[top_m] && P[top_m].mates >= 3) { aname[top_m] = "CLUB-MATE SOMMELIER"; snprintf(award[top_m], 64, AWARD_MATE, P[top_m].mates); }
  if (top_s >= 0 && !aname[top_s] && P[top_s].shelved >= 2) { aname[top_s] = "FREE PILE CURATOR"; snprintf(award[top_s], 64, AWARD_CURATOR, P[top_s].shelved); }
  if (top_f >= 0 && !aname[top_f] && P[top_f].fixes >= 1) { aname[top_f] = "PRINTER WHISPERER"; snprintf(award[top_f], 64, AWARD_WHISPER, P[top_f].fixes); }
  if (top_u >= 0 && !aname[top_u] && P[top_u].unplugged >= 1) { aname[top_u] = "MARSHAL'S FAVORITE"; snprintf(award[top_u], 64, AWARD_MARSHAL, P[top_u].unplugged); }
  if (top_d >= 0 && !aname[top_d] && P[top_d].dishes >= 4) { aname[top_d] = "DISH PIT"; snprintf(award[top_d], 64, AWARD_DISHPIT, P[top_d].dishes); }
  for (int j = 0; j < n; j++) { int i = order[j]; if (!aname[i] && P[i].proposals >= 5 && P[i].tasks == 0) { aname[i] = "BIKESHED"; snprintf(award[i], 64, "%s", AWARD_BIKESHED); } }
  if (n >= 2 && low >= 0 && !aname[low]) { aname[low] = "LURKER"; snprintf(award[low], 64, "%s", AWARD_LURKER); }

  int y = 200;
  hud_eyebrow(130, y, "MEMBER", C_ASH, AL_LEFT);
  hud_eyebrow(560, y, "CLOUT", C_HAZARD, AL_RIGHT);
  hud_eyebrow(700, y, "KARMA", C_INK_OK, AL_RIGHT);
  hud_eyebrow(780, y, "AWARD", C_ASH, AL_LEFT);
  y += 26;
  int rows = n > 6 ? 6 : n;
  for (int j = 0; j < rows; j++) {
    int i = order[j];
    player_t *p = &P[i];
    float in = fminf(1, fmaxf(0, (G.t - 0.6f - j * 0.25f) * 4));
    if (in <= 0) continue;
    int ox = (int)((1 - in) * 80);
    hud_rect(64 + ox, y, 1152, 58, j & 1 ? C_SLAB : C_SLAB_HI, (int)(255 * in));
    hud_rect(64 + ox, y, 8, 58, p->color, 255);
    char b[24];
    snprintf(b, sizeof b, "%d", j + 1);
    hud_text(F_H2, 86 + ox, y + 6, b, C_SMOKE, 255, AL_LEFT);
    hud_text(F_H3, 130 + ox, y + 14, p->name, C_BONE, 255, AL_LEFT);
    if (ok) snprintf(b, sizeof b, "%d", p->clout); else snprintf(b, sizeof b, "VOID");
    hud_text(F_MONO_L, 560 + ox, y + 12, b, ok ? C_HAZARD : C_INK_ERR, 255, AL_RIGHT);
    snprintf(b, sizeof b, "%d", p->karma);
    hud_text(F_MONO_L, 700 + ox, y + 12, b, C_INK_OK, 255, AL_RIGHT);
    if (aname[i]) {
      hud_text(F_MONO_S, 780 + ox, y + 8, aname[i], C_HAZARD, 255, AL_LEFT);
      hud_text(F_MONO_S, 780 + ox, y + 30, award[i], C_BONE, 255, AL_LEFT);
    }
    y += 64;
  }
  if (top_k >= 0 && P[top_k].karma > 0 && G.t > 2.5f) {
    char b[96];
    snprintf(b, sizeof b, "THE REAL WINNER: %s. LEAVE IT BETTER.", P[top_k].name);
    int w = hud_text_w(F_H3, b) + 40;
    hud_rect(HUD_W / 2 - w / 2 + 6, 606, w, 44, C_SHADOW, 255);
    hud_rect(HUD_W / 2 - w / 2, 600, w, 44, C_LIVE, 255);
    hud_text(F_H3, HUD_W / 2, 606, b, C_TAPE, 255, AL_CENTER);
  }
  if (G.t > 4 && fmodf(T, 1.2f) < 0.8f)
    hud_text(F_MONO, HUD_W / 2, HUD_H - 50, "PRESS A OR SPACE FOR ANOTHER TUESDAY", C_BONE, 255, AL_CENTER);
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
  if (G.phase == PH_RESULTS) { fx_draw(dt); results_screen(); fx2_draw(dt); return; }
  nplaced = 0;
  reserve(0, 0, HUD_W, 104);                 /* the top rows */
  if (G.phase == PH_NIGHT) reserve(0, HUD_H - 86, HUD_W, 86);   /* the player cards */
  else reserve(0, 570, HUD_W, 150);          /* the lobby seat cards */
  reserve(960, 104, 304, 52);                /* the toasts */
  goal_guides();
  station_labels();
  floor_tags();
  npcs_overlay();
  fx_draw(dt);
  top_bar();
  if (G.phase == PH_LOBBY) lobby_overlay(dt);
  else join_strip(944, 6);
  if (!danger()) { ticker_band(); if (G.ticker_t <= 0) coach(); }
  if (G.phase == PH_NIGHT) player_cards();
  toasts(1264, 108);
  /* last, so nothing on screen can ever hide your own name, prompt or progress */
  players_overlay();
  if (G.phase == PH_NIGHT) countdown();
  start_wipe();
  fx2_draw(dt);
}
