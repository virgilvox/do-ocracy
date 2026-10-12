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

/* a GANTRY eyebrow: tracked mono caps */
static void eyebrow(int x, int y, const char *s, uint32_t c, int align) {
  char buf[160];
  int n = 0;
  for (const char *p = s; *p && n < 150; p++) { buf[n++] = *p; if (p[1]) buf[n++] = ' '; }
  buf[n] = 0;
  hud_text(F_MONO_S, x, y, buf, c, 255, align);
}

/* what a player presses, named for their own input */
static const char *glyph_a(const player_t *p) { return p->kb == 0 ? "SPACE" : p->kb == 1 ? "." : "A"; }

/* a key cap: the button name in a box, sized to fit */
static int keycap(int x, int y, const char *k, uint32_t fill, uint32_t ink) {
  int w = hud_text_w(F_MONO_S, k) + 14;
  if (w < 24) w = 24;
  hud_rect(x + 2, y + 2, w, 24, C_SHADOW, 255);
  hud_rect(x, y, w, 24, fill, 255);
  hud_text(F_MONO_S, x + w / 2, y + 3, k, ink, 255, AL_CENTER);
  return w;
}

static void clock_str(char *out, int n) {
  float frac = G.phase == PH_NIGHT ? G.t / G.night_len : 0;
  int mins = 19 * 60 + (int)(frac * 240);
  int h = mins / 60, m = mins % 60;
  snprintf(out, (size_t)n, "%d:%02d PM", h > 12 ? h - 12 : h, m);
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

static void station_labels(void) {
  for (int i = 0; i < ST_COUNT; i++) {
    station_t *s = &S[i];
    if (i == ST_COUCH || i == ST_TABLE) continue;
    int sx, sy;
    float top = i == ST_FRIDGE ? 2.4f : i == ST_LUMBER ? 2.1f : (i >= ST_PRINT1 && i <= ST_PRINT2) ? 2.0f : 1.3f;
    if (!project(s->x, top, s->z, &sx, &sy)) continue;
    char st[48] = "";
    uint32_t c = C_ASH;
    bool alarm = false;
    /* what it pays: amber stations make clout, teal ones take karma work */
    int pay = 0;   /* 1 clout, 2 karma */
    switch (i) {
      case ST_LASER:
        pay = 1;
        if (s->state == S_BROKEN) { snprintf(st, sizeof st, "TOO DUSTY. HOLD TO CLEAN"); c = C_INK_ERR; alarm = true; pay = 2; }
        else if (s->state == S_LOADED || s->state == S_BUSY) { snprintf(st, sizeof st, "LOADED. HOLD TO CUT"); c = C_HAZARD; }
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
    bool ready = (i == ST_LASER && (s->state == S_LOADED || s->state == S_BUSY)) || ((i == ST_PRINT1 || i == ST_PRINT2) && s->state == S_DONE);
    if (!(alarm || ready || near)) st[0] = 0;
    uint32_t edge = pay == 1 ? C_HAZARD : pay == 2 ? C_LIVE : C_STEEL_HI;
    int w = hud_text_w(F_MONO_S, s->label);
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
      label(sx, sy - 26, it->type == IT_DAISY ? "UNPLUG ME" : it->type == IT_PUDDLE ? "MOP ME" : "SWEEP ME", it->type == IT_DAISY ? C_INK_ERR : C_INK_OK, C_CHAR, F_MONO_S);
    } else if (it->type == IT_PROJECT) {
      label(sx, sy - 20, item_name(it), C_HAZARD, C_CHAR, F_MONO_S);
    } else if (it->type == IT_DONATION) {
      label(sx, sy - 20, item_name(it), C_BONE, C_CHAR, F_MONO_S);
      label(sx, sy + 2, "TO FREE PILE", C_INK_OK, C_TAPE, F_MONO_S);
    } else if (it->type != IT_PLYWOOD) {
      label(sx, sy - 18, "TRASH ME", C_INK_OK, C_CHAR, F_MONO_S);
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
      float f = n->patience / 20.f;
      hud_circle(sx, sy, 20, C_CHAR, 235);
      hud_ring(sx, sy, 20, 5, f, f < 0.35f ? C_FAULT : C_LIVE, 255);
      hud_text(F_H3, sx, sy - 16, "?", C_BONE, 255, AL_CENTER);
      label(sx, sy + 26, f < 0.35f ? "LEAVING! SAY HI" : "NEWBIE: SAY HI", f < 0.35f ? C_INK_ERR : C_INK_OK, C_CHAR, F_MONO_S);
    } else if (n->state == 2) {
      hud_text(F_MONO_S, sx, sy + 10, "MEMBER", C_INK_OK, 255, AL_CENTER);
    }
  }
}

/* the arrow from your hands to where the thing goes */
static void goal_guides(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active || !p->has_goal || p->bot) continue;
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
      hud_frame(bx, by, tot, 32, 2, c, 255);
      keycap(bx + 6, by + 4, g, c, C_TAPE);
      hud_text(F_MONO, bx + gw + 16, by + 4, p->prompt, karma ? C_INK_OK : C_HAZARD, 255, AL_LEFT);
      if (p->work > 0) hud_ring(sx, sy - 52, 17, 6, fminf(1, p->work / 2.4f), c, 255);
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

static void join_panel(int x, int y, bool big) {
  int w = big ? 300 : 232, q = big ? 172 : 104;
  int h = big ? 290 : 158;
  if (!net_online()) h = 108;
  hud_panel(x, y, w, h, C_SLAB, C_STEEL_HI);
  if (net_online()) {
    hud_qr(x + 12, y + 12, q, G.join_url);
    if (big) {
      eyebrow(x + 12, y + q + 22, "SCAN TO JOIN", C_ASH, AL_LEFT);
      hud_text(F_H1, x + 10, y + q + 38, G.room, C_HAZARD, 255, AL_LEFT);
      hud_text(F_MONO_S, x + w - 12, y + q + 64, "NO APP", C_ASH, 255, AL_RIGHT);
      hud_text(F_MONO_S, x + w - 12, y + q + 84, "NO ACCOUNT", C_ASH, 255, AL_RIGHT);
    } else {
      eyebrow(x + q + 20, y + 14, "JOIN", C_ASH, AL_LEFT);
      hud_text(F_H3, x + q + 18, y + 34, G.room, C_HAZARD, 255, AL_LEFT);
      hud_text(F_MONO_S, x + q + 20, y + 76, "SCAN IT", C_ASH, 255, AL_LEFT);
      hud_text(F_MONO_S, x + q + 20, y + 96, "ANY PHONE", C_ASH, 255, AL_LEFT);
    }
  } else {
    eyebrow(x + 14, y + 16, "PHONES OFFLINE", C_ASH, AL_LEFT);
    hud_text_wrap(F_MONO, x + 14, y + 44, w - 28, "GRAB A CONTROLLER.", C_BONE, 255, 26);
  }
}

static void join_strip(int x, int y) {
  hud_panel(x, y, 314, 76, C_SLAB, C_STEEL_HI);
  if (net_online()) {
    hud_qr(x + 6, y + 6, 64, G.join_url);
    eyebrow(x + 82, y + 10, "PHONES JOIN", C_ASH, AL_LEFT);
    hud_text(F_H2, x + 80, y + 26, G.room, C_HAZARD, 255, AL_LEFT);
    hud_text(F_MONO_S, x + 300, y + 48, "ANY TIME", C_SMOKE, 255, AL_RIGHT);
  } else {
    eyebrow(x + 14, y + 12, "PHONES OFFLINE", C_ASH, AL_LEFT);
    hud_text(F_MONO_S, x + 14, y + 40, "GRAB A CONTROLLER.", C_BONE, 255, AL_LEFT);
  }
}

/* the feed: three short lines, gone in a few seconds */
static void toasts(int x, int y) {
  for (int i = 0; i < 3; i++) {
    if (toast_t[i] <= 2) continue;
    float t = toast_t[i] - 2;
    int a = t < 1 ? (int)(t * 255) : 255;
    int w = hud_text_w(F_MONO_S, toast_text[i]);
    hud_rect(x, y - i * 24, w + 18, 21, C_CHAR, a * 210 / 255);
    hud_rect(x, y - i * 24, 4, 21, toast_col[i], a);
    hud_text(F_MONO_S, x + 10, y - i * 24 + 2, toast_text[i], C_BONE, a, AL_LEFT);
  }
}

/* one message slot, just under the top bar, over the back wall: nothing you
 * play with is up there, so a message never covers the floor */
#define SLOT_Y 94
#define SLOT_H 40
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
  hud_text(cf, x + cw / 2, y + (cf == F_H3 ? 6 : 4), chip, C_TAPE, a, AL_CENTER);
  if (mw) hud_text(mf, x + cw + 14, y + (mf == F_MONO ? 9 : 12), msg, msg_c, a, AL_LEFT);
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
  eyebrow(x, y, name, C_BONE, AL_LEFT);
  hud_text(F_MONO_S, x + hud_text_w(F_MONO_S, name) * 2 + 10, y, rule, C_SMOKE, 255, AL_LEFT);
  hud_text(F_MONO_S, x + w, y, right, C_ASH, 255, AL_RIGHT);
  hud_bar(x, y + 22, w, 18, v / 100.f, fill, C_CHAR);
}

static void top_bar(void) {
  char clk[16];
  clock_str(clk, sizeof clk);
  hud_panel(16, 14, 286, 76, C_SLAB, C_STEEL_HI);
  hud_logo(24, 20, 64, C_BONE);
  eyebrow(98, 24, G.phase == PH_NIGHT ? "HYH TUESDAY" : "CLOCKING IN", C_ASH, AL_LEFT);
  hud_text(F_H2, 96, 40, G.phase == PH_NIGHT ? clk : "7:00 PM", C_BONE, 255, AL_LEFT);
  hud_text(F_MONO_S, 290, 66, "AZ", C_SMOKE, 255, AL_RIGHT);
  if (G.phase == PH_NIGHT) hud_bar(24, 84, 270, 4, G.t / G.night_len, C_HAZARD, C_CHAR);

  hud_panel(330, 14, 600, 76, C_SLAB, C_STEEL_HI);
  float c = G.clutter > 100 ? 100 : G.clutter;
  uint32_t cf = c > 80 ? C_FAULT : c > 55 ? C_RUST : C_HAZARD;
  char r[48];
  if (G.fire_warn > 0) snprintf(r, sizeof r, "FIRE MARSHAL IN %d", (int)ceilf(5 - G.fire_warn));
  else snprintf(r, sizeof r, "%d", (int)c);
  meter(346, 22, 270, "CLUTTER", "100 = FIRE MARSHAL", c, cf, r);
  snprintf(r, sizeof r, "%d", (int)G.vibe);
  meter(640, 22, 274, "VIBE", "0 = BURNOUT", G.vibe, G.vibe < 25 ? C_FAULT : C_LIVE, r);
  /* danger: say it in the middle of the screen, once it matters */
  /* danger is drawn later, in the message slot (see danger()) */
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
  eyebrow(HUD_W / 2 - total / 2, y - 20, "CLOUT", C_HAZARD, AL_LEFT);
  eyebrow(HUD_W / 2 + total / 2, y - 20, "KARMA", C_INK_OK, AL_RIGHT);
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
  static const char *const STEP[][2] = {
    { "1 CLOUT", "GRAB PLYWOOD FROM THE LUMBER RACK (LEFT WALL)" },
    { "2 CLOUT", "LOAD IT INTO PEWPEW 9000, THEN HOLD TO CUT" },
    { "3 CLOUT", "CARRY YOUR PROJECT TO SHOW & TELL (FRONT)" },
    { "4 KARMA", "TEAL RINGS ARE MESSES: HOLD TO CLEAN. JUNK GOES IN THE TRASH" },
    { "5 KARMA", "NEWBIES WITH A ? NEED A HI, OR THEY LEAVE" },
  };
  int k = (int)(G.t / 8);
  if (k > 4) k = 4;
  float local = G.t - k * 8;
  int a = local < 0.3f ? (int)(local / 0.3f * 255) : 255;
  bool karma = k >= 3;
  if (G.ticker_t > 0) return;   /* one thing at a time up top */
  slot_bar(STEP[k][0], karma ? C_LIVE : C_HAZARD, STEP[k][1], C_BONE, a, 0);
}

/* ── full screens ────────────────────────────────────────────────── */

static int flow_chip(int x, int y, const char *s, uint32_t c) {
  int w = hud_text_w(F_MONO_S, s) + 20;
  hud_rect(x + 3, y + 3, w, 26, C_SHADOW, 255);
  hud_rect(x, y, w, 26, C_CHAR, 255);
  hud_frame(x, y, w, 26, 2, c, 255);
  hud_text(F_MONO_S, x + w / 2, y + 4, s, C_BONE, 255, AL_CENTER);
  return w;
}

static int flow_arrow(int x, int y, uint32_t c) {
  hud_chevron((float)x + 12, (float)y + 13, 1, 0, 14, 4, c, 255);
  return 26;
}

/* the how-to board: two paths, one rule */
static void how_to(int x, int y, int w) {
  hud_panel(x, y, w, 296, C_SLAB, C_STEEL_HI);
  eyebrow(x + 16, y + 14, "HOW TUESDAY WORKS", C_ASH, AL_LEFT);
  int cy = y + 40;
  hud_rect(x + 16, cy, 8, 64, C_HAZARD, 255);
  hud_text(F_H3, x + 34, cy - 2, "CLOUT", C_HAZARD, 255, AL_LEFT);
  hud_text(F_MONO_S, x + 140, cy + 6, "FOR YOU", C_ASH, 255, AL_LEFT);
  int fx = x + 34, fy = cy + 32;
  fx += flow_chip(fx, fy, "LUMBER", C_HAZARD); fx += flow_arrow(fx, fy, C_HAZARD);
  fx += flow_chip(fx, fy, "PEWPEW 9000", C_HAZARD); fx += flow_arrow(fx, fy, C_HAZARD);
  flow_chip(fx, fy, "SHOW & TELL", C_HAZARD);
  cy += 80;
  hud_rect(x + 16, cy, 8, 64, C_LIVE, 255);
  hud_text(F_H3, x + 34, cy - 2, "KARMA", C_LIVE, 255, AL_LEFT);
  hud_text(F_MONO_S, x + 146, cy + 6, "FOR THE SPACE", C_ASH, 255, AL_LEFT);
  fx = x + 34; fy = cy + 32;
  fx += flow_chip(fx, fy, "MESS", C_LIVE); fx += flow_arrow(fx, fy, C_LIVE);
  fx += flow_chip(fx, fy, "TRASH", C_LIVE); fx += 10;
  fx += flow_chip(fx, fy, "DISHES", C_LIVE); fx += 10;
  flow_chip(fx, fy, "SAY HI", C_LIVE);
  cy += 80;
  hud_rect(x + 16, cy, w - 32, 50, C_FAULT, 255);
  hud_text(F_MONO_S, x + 28, cy + 7, "CLUTTER 100 OR VIBE 0 CLOSES THE SPACE.", C_BONE, 255, AL_LEFT);
  hud_text(F_MONO_S, x + 28, cy + 27, "THEN EVERYONE'S CLOUT IS VOID.", C_BONE, 255, AL_LEFT);
}

static void controls_card(int x, int y, int w) {
  hud_panel(x, y, w, 150, C_SLAB, C_STEEL_HI);
  eyebrow(x + 16, y + 14, "BUTTONS", C_ASH, AL_LEFT);
  static const char *const ROWS[4][4] = {
    { "PAD", "A", "B", "X" },
    { "WASD", "SPACE", "Q", "R" },
    { "ARROWS", ".", ",", "/" },
    { "PHONE", "A", "B", "X" },
  };
  hud_text(F_MONO_S, x + 120, y + 14, "DO IT", C_HAZARD, 255, AL_LEFT);
  hud_text(F_MONO_S, x + 210, y + 14, "DROP", C_BONE, 255, AL_LEFT);
  hud_text(F_MONO_S, x + 290, y + 14, "PROPOSE", C_ASH, 255, AL_LEFT);
  for (int r = 0; r < 4; r++) {
    int ry = y + 38 + r * 27;
    hud_text(F_MONO_S, x + 16, ry + 4, ROWS[r][0], C_BONE, 255, AL_LEFT);
    keycap(x + 120, ry, ROWS[r][1], C_HAZARD, C_TAPE);
    keycap(x + 210, ry, ROWS[r][2], C_BONE, C_TAPE);
    keycap(x + 290, ry, ROWS[r][3], C_VIOLET, C_TAPE);
  }
}

static void title_screen(void) {
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, 150);
  hud_tape(0, 0, HUD_W, 24, T * 30);
  hud_logo(64, 62, 200, C_BONE);
  eyebrow(290, 70, "HEATSYNC LABS ARCADE", C_ASH, AL_LEFT);
  hud_text(F_TITLE, 282, 96, "DO-OCRACY", C_BONE, 255, AL_LEFT);
  const char *tag = TAGLINES[(int)(T / 3.5f) % COUNT(TAGLINES)];
  hud_text(F_H3, 290, 242, tag, C_HAZARD, 255, AL_LEFT);
  /* alternate the how-to and the buttons, so a passerby learns both */
  if (fmodf(T, 14.f) < 9.f) how_to(64, 292, 600);
  else controls_card(64, 300, 460);
  join_panel(916, 300, true);
  if (fmodf(T, 1.2f) < 0.85f) {
    hud_rect(70, 610, 600, 56, C_SHADOW, 255);
    hud_rect(64, 604, 600, 56, C_HAZARD, 255);
    hud_text(F_H2, 364, 610, "PRESS A OR SPACE TO PLAY", C_TAPE, 255, AL_CENTER);
  }
  const char *tip = ATTRACT_TIPS[(int)(T / 3.0f) % COUNT(ATTRACT_TIPS)];
  hud_rect(0, HUD_H - 40, HUD_W, 40, C_TAPE, 255);
  hud_text(F_MONO, HUD_W / 2, HUD_H - 34, tip, C_ASH, 255, AL_CENTER);
}

static void lobby_overlay(void) {
  how_to(16, 146, 470);
  controls_card(16, 454, 400);
  join_panel(1032, 146, false);
  char msg[80];
  int left = (int)ceilf(25 - G.t);
  if (player_count() == 0) snprintf(msg, sizeof msg, "PRESS A OR SPACE TO JOIN");
  else snprintf(msg, sizeof msg, "START OR ENTER: BEGIN (AUTO %d)", left < 0 ? 0 : left);
  slot_bar(msg, C_HAZARD, player_count() ? "WALK AROUND. TRY THINGS. NOTHING COUNTS YET." : NULL, C_BONE, 255, 0);
}

static void results_screen(void) {
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, 215);
  hud_tape(0, 0, HUD_W, 24, T * 20);
  bool ok = G.end == END_SURVIVED;
  const char *head = ok ? "THE SPACE SURVIVES ANOTHER TUESDAY." : G.end == END_FIRE ? "THE FIRE MARSHAL SHUT IT DOWN." : "EVERYONE BURNED OUT.";
  eyebrow(64, 48, ok ? "11:00 PM. LIGHTS OUT." : "HYH IS CANCELED", ok ? C_INK_OK : C_INK_ERR, AL_LEFT);
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
  for (int j = 0; j < n; j++) {
    int i = order[j];
    if (top_k < 0 || P[i].karma > P[top_k].karma) top_k = i;
    if (top_c < 0 || P[i].clout > P[top_c].clout) top_c = i;
    if (top_t < 0 || P[i].proposals > P[top_t].proposals) top_t = i;
    if (top_g < 0 || P[i].greeted > P[top_g].greeted) top_g = i;
    if (top_m < 0 || P[i].mates > P[top_m].mates) top_m = i;
    if (low < 0 || P[i].tasks + P[i].clout / 500 < P[low].tasks + P[low].clout / 500) low = i;
  }
  if (top_k >= 0 && P[top_k].karma > 0) { aname[top_k] = "JANITORIAL CO-OP"; snprintf(award[top_k], 64, "%s", AWARD_JANITOR); }
  if (top_c >= 0 && !aname[top_c] && P[top_c].clout > 0) { aname[top_c] = "GLAMOUR BIAS"; snprintf(award[top_c], 64, "%s", AWARD_GLAMOUR); }
  if (top_t >= 0 && !aname[top_t] && P[top_t].proposals >= 3) { aname[top_t] = "TALKER"; snprintf(award[top_t], 64, AWARD_TALKER, P[top_t].proposals); }
  if (top_g >= 0 && !aname[top_g] && P[top_g].greeted >= 2) { aname[top_g] = "GREETER"; snprintf(award[top_g], 64, AWARD_GREETER, P[top_g].greeted); }
  if (top_m >= 0 && !aname[top_m] && P[top_m].mates >= 3) { aname[top_m] = "CLUB-MATE SOMMELIER"; snprintf(award[top_m], 64, AWARD_MATE, P[top_m].mates); }
  if (n >= 2 && low >= 0 && !aname[low]) { aname[low] = "LURKER"; snprintf(award[low], 64, "%s", AWARD_LURKER); }

  int y = 200;
  eyebrow(130, y, "MEMBER", C_ASH, AL_LEFT);
  eyebrow(560, y, "CLOUT", C_HAZARD, AL_RIGHT);
  eyebrow(700, y, "KARMA", C_INK_OK, AL_RIGHT);
  eyebrow(780, y, "AWARD", C_ASH, AL_LEFT);
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
    fx_draw(dt);
    title_screen();
    return;
  }
  if (G.phase == PH_RESULTS) { fx_draw(dt); results_screen(); return; }
  goal_guides();
  floor_tags();
  station_labels();
  npcs_overlay();
  fx_draw(dt);
  players_overlay();
  top_bar();
  if (G.phase == PH_LOBBY) lobby_overlay();
  else join_strip(950, 14);
  if (!danger()) { ticker_band(); if (G.ticker_t <= 0) coach(); }
  player_cards();
  toasts(16, HUD_H - 100);
}
