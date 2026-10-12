/* Every screen's 2D layer, in GANTRY: char and bone, amber as a fill,
 * caution tape between things, solid offset shadows, no rounded corners. */
#include "game.h"
#include "hud.h"
#include "content.h"
#include "net.h"
#include <math.h>
#include <stdio.h>
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

static void label(int x, int y, const char *s, uint32_t fg, uint32_t bg, int font) {
  int w = hud_text_w(font, s), h = hud_font_h(font);
  hud_rect(x - w / 2 - 6, y - 3, w + 12, h + 8, bg, 235);
  hud_text(font, x, y, s, fg, 255, AL_CENTER);
}

/* a GANTRY eyebrow: tracked mono caps */
static void eyebrow(int x, int y, const char *s, uint32_t c, int align) {
  char buf[128];
  int n = 0;
  for (const char *p = s; *p && n < 120; p++) { buf[n++] = *p; if (p[1]) buf[n++] = ' '; }
  buf[n] = 0;
  hud_text(F_MONO_S, x, y, buf, c, 255, align);
}

static void clock_str(char *out, int n) {
  float frac = G.phase == PH_NIGHT ? G.t / G.night_len : 0;
  int mins = 19 * 60 + (int)(frac * 240);
  int h = mins / 60, m = mins % 60;
  snprintf(out, (size_t)n, "%d:%02d PM", h > 12 ? h - 12 : h, m);
}

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

static void toasts(int x, int y) {
  for (int i = 0; i < TOASTS; i++) {
    if (toast_t[i] <= 0) continue;
    int a = toast_t[i] < 1 ? (int)(toast_t[i] * 255) : 255;
    int w = hud_text_w(F_MONO, toast_text[i]);
    hud_rect(x, y - i * 30, w + 22, 26, C_CHAR, a * 220 / 255);
    hud_rect(x, y - i * 30, 4, 26, toast_col[i], a);
    hud_text(F_MONO, x + 12, y - i * 30 + 1, toast_text[i], C_BONE, a, AL_LEFT);
  }
}

static void ticker_band(void) {
  if (G.ticker_t <= 0) return;
  float k = G.ticker_t > 4.6f ? (5.0f - G.ticker_t) / 0.4f : G.ticker_t < 0.4f ? G.ticker_t / 0.4f : 1;
  int y = 104 - (int)((1 - k) * 60);
  hud_tape(0, y, HUD_W, 56, T * 40);
  int w = hud_text_w(F_H3, G.ticker) + 48;
  hud_rect(HUD_W / 2 - w / 2, y + 6, w, 44, C_CHAR, 255);
  hud_frame(HUD_W / 2 - w / 2, y + 6, w, 44, 2, G.ticker_color, 255);
  hud_text(F_H3, HUD_W / 2, y + 12, G.ticker, C_BONE, 255, AL_CENTER);
}

static void meter(int x, int y, int w, const char *name, float v, uint32_t fill, const char *right) {
  eyebrow(x, y, name, C_ASH, AL_LEFT);
  hud_text(F_MONO_S, x + w, y, right, C_ASH, 255, AL_RIGHT);
  hud_bar(x, y + 22, w, 16, v / 100.f, fill, C_CHAR);
}

static void top_bar(void) {
  char clk[16];
  clock_str(clk, sizeof clk);
  hud_panel(16, 14, 286, 76, C_SLAB, C_STEEL_HI);
  hud_logo(24, 20, 64, C_BONE);
  eyebrow(98, 24, G.phase == PH_NIGHT ? "HYH TUESDAY" : "CLOCKING IN", C_ASH, AL_LEFT);
  hud_text(F_H2, 96, 40, G.phase == PH_NIGHT ? clk : "7:00 PM", C_BONE, 255, AL_LEFT);
  hud_text(F_MONO_S, 290, 66, "AZ", C_SMOKE, 255, AL_RIGHT);

  hud_panel(330, 14, 600, 76, C_SLAB, C_STEEL_HI);
  float c = G.clutter > 100 ? 100 : G.clutter;
  uint32_t cf = c > 80 ? C_FAULT : c > 55 ? C_RUST : C_HAZARD;
  char r[48];
  if (G.fire_warn > 0) snprintf(r, sizeof r, "FIRE MARSHAL IN %d", (int)ceilf(5 - G.fire_warn));
  else snprintf(r, sizeof r, "%d / 100", (int)c);
  meter(346, 22, 270, "CLUTTER", c, cf, r);
  snprintf(r, sizeof r, "%d", (int)G.vibe);
  meter(640, 22, 274, "VIBE", G.vibe, G.vibe < 25 ? C_FAULT : C_LIVE, r);
  if (G.fire_warn > 0 && fmodf(T, 0.5f) < 0.25f) hud_frame(0, 0, HUD_W, HUD_H, 10, C_FAULT, 200);
  if (G.heat > 0) hud_rect(0, 0, HUD_W, HUD_H, 0xff7a1a, 26);
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
    if (p->phone || p->bot) hud_text(F_MONO_S, x + cw - 8, y + 6, p->bot ? "BOT" : "PHONE", C_SMOKE, 255, AL_RIGHT);
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

static void station_labels(void) {
  for (int i = 0; i < ST_COUNT; i++) {
    station_t *s = &S[i];
    if (i == ST_COUCH || i == ST_TABLE) continue;
    int sx, sy;
    float top = i == ST_FRIDGE ? 2.4f : i == ST_LUMBER ? 2.1f : (i >= ST_PRINT1 && i <= ST_PRINT3) ? 2.0f : 1.3f;
    if (!project(s->x, top, s->z, &sx, &sy)) continue;
    char st[40] = "";
    uint32_t c = C_ASH;
    if (i == ST_LASER) {
      if (s->state == S_BROKEN) { snprintf(st, sizeof st, "DUSTY. CLEAN ME"); c = C_INK_ERR; }
      else if (s->state == S_LOADED || s->state == S_BUSY) { snprintf(st, sizeof st, "LOADED"); c = C_HAZARD; }
      else snprintf(st, sizeof st, "NEEDS PLYWOOD  DUST %d%%", (int)s->dust);
    } else if (i >= ST_PRINT1 && i <= ST_PRINT3) {
      if (s->state == S_BROKEN) { snprintf(st, sizeof st, "CLOGGED"); c = C_INK_ERR; }
      else if (s->state == S_BUSY) snprintf(st, sizeof st, "PRINTING %ds", (int)ceilf(s->timer));
      else if (s->state == S_DONE) { snprintf(st, sizeof st, "READY"); c = C_HAZARD; }
      else snprintf(st, sizeof st, "IDLE");
    } else if (i == ST_SINK) {
      if (s->count) { snprintf(st, sizeof st, "DISHES %d", s->count); c = s->count > 6 ? C_INK_ERR : C_ASH; }
    } else if (i == ST_FRIDGE) {
      if (s->smell) { snprintf(st, sizeof st, "SMELLS"); c = C_INK_ERR; } else snprintf(st, sizeof st, "CLUB-MATE");
    } else if (i == ST_CUTSALOT) snprintf(st, sizeof st, "ALWAYS FREE");
    label(sx, sy, s->label, C_BONE, C_CHAR, F_MONO_S);
    if (st[0]) label(sx, sy + 22, st, c, C_TAPE, F_MONO_S);
  }
}

static void floor_tags(void) {
  for (int i = 0; i < MAX_ITEMS; i++) {
    item_t *it = &I[i];
    if (!it->alive || it->holder >= 0) continue;
    int sx, sy;
    if (!project(it->x, 0.4f, it->z, &sx, &sy)) continue;
    if (it->age > 25 && it->type != IT_PROJECT && it->type != IT_PLYWOOD) {
      hud_rect(sx - 26, sy - 8, 52, 16, C_FAULT, 255);
      hud_text(F_MONO_S, sx, sy - 9, "RED TAG", C_BONE, 255, AL_CENTER);
    } else if (it->type == IT_DONATION || it->type == IT_DAISY || it->type == IT_PROJECT) {
      label(sx, sy - 6, item_name(it), it->type == IT_PROJECT ? C_HAZARD : C_BONE, C_CHAR, F_MONO_S);
    }
  }
}

static void npcs_overlay(void) {
  for (int i = 0; i < MAX_NPCS; i++) {
    npc_t *n = &N[i];
    if (!n->alive) continue;
    int sx, sy;
    if (!project(n->x, 2.0f, n->z, &sx, &sy)) continue;
    if (n->state <= 1) {
      float f = n->patience / 20.f;
      hud_circle(sx, sy, 17, C_CHAR, 230);
      hud_ring(sx, sy, 17, 4, f, f < 0.35f ? C_FAULT : C_BONE, 255);
      hud_text(F_H3, sx, sy - 15, "?", C_BONE, 255, AL_CENTER);
      hud_text(F_MONO_S, sx, sy + 20, "NEWBIE", C_ASH, 255, AL_CENTER);
    } else if (n->state == 2) {
      hud_text(F_MONO_S, sx, sy + 6, "MEMBER", C_INK_OK, 255, AL_CENTER);
    }
  }
}

static void players_overlay(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    int sx, sy;
    if (!project(p->x, 2.05f, p->z, &sx, &sy)) continue;
    int w = hud_text_w(F_MONO_S, p->name);
    hud_rect(sx - w / 2 - 6, sy - 2, w + 12, 20, p->color, 255);
    hud_text(F_MONO_S, sx, sy - 1, p->name, C_TAPE, 255, AL_CENTER);
    if (p->hold >= 0) {
      int hx, hy;
      if (project(p->x, 2.4f, p->z, &hx, &hy)) {
        const char *nm = item_name(&I[p->hold]);
        hud_text(F_MONO_S, hx, hy - 20, nm, I[p->hold].type == IT_PROJECT ? C_HAZARD : C_BONE, 255, AL_CENTER);
      }
    }
    int fx, fy;
    if (p->prompt[0] && project(p->x, 0.0f, p->z, &fx, &fy)) {
      uint32_t c = p->px > 0 ? C_LIVE : C_HAZARD;
      int tw = hud_text_w(F_MONO_S, p->prompt);
      int bx = fx - (tw + 40) / 2, by = fy + 14;
      hud_rect(bx, by, tw + 40, 24, C_CHAR, 230);
      hud_circle(bx + 13, by + 12, 9, c, 255);
      hud_text(F_MONO_S, bx + 13, by + 2, "A", C_TAPE, 255, AL_CENTER);
      hud_text(F_MONO_S, bx + 28, by + 3, p->prompt, c == C_LIVE ? C_INK_OK : C_HAZARD, 255, AL_LEFT);
      if (p->work > 0) {
        float need = 1;
        (void)need;
        hud_ring(sx, sy - 28, 15, 5, fminf(1, p->work / 2.4f), c, 255);
      }
    }
    if (p->flash > 0) {
      int a = p->flash < 0.4f ? (int)(p->flash / 0.4f * 255) : 255;
      int rise = (int)((1.4f - p->flash) * 40);
      hud_text(F_H3, sx + 2, sy - 52 - rise + 2, p->pop, C_SHADOW, a, AL_CENTER);
      hud_text(F_H3, sx, sy - 52 - rise, p->pop, p->pop_color, a, AL_CENTER);
    }
    if (p->bubble > 0) {
      const char *t = PROPOSALS[p->bubble_text];
      int bw = hud_text_w(F_MONO_S, t);
      if (bw < 190) bw = 190;
      int bx = sx + 18, by = sy - 80;
      hud_panel(bx, by, bw + 20, 50, C_BONE, C_TAPE);
      hud_text(F_MONO_S, bx + 10, by + 6, "I THINK WE SHOULD", C_SMOKE, 255, AL_LEFT);
      hud_text(F_MONO_S, bx + 10, by + 26, t, C_TAPE, 255, AL_LEFT);
    }
  }
}

static void title_screen(void) {
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, 150);
  hud_tape(0, 0, HUD_W, 24, T * 30);
  hud_logo(64, 70, 200, C_BONE);
  eyebrow(290, 76, "HEATSYNC LABS ARCADE", C_ASH, AL_LEFT);
  hud_text(F_TITLE, 282, 104, "DO-OCRACY", C_BONE, 255, AL_LEFT);
  const char *tag = TAGLINES[(int)(T / 3.5f) % COUNT(TAGLINES)];
  hud_text(F_H3, 290, 250, tag, C_HAZARD, 255, AL_LEFT);

  hud_panel(64, 330, 640, 214, C_SLAB, C_STEEL_HI);
  eyebrow(84, 346, "TONIGHT", C_ASH, AL_LEFT);
  hud_text_wrap(F_MONO, 84, 374, 600, "IT IS TUESDAY. IT IS HACK YOUR HACKERSPACE. THE LASER IS CALLING. SO ARE THE DISHES.", C_BONE, 255, 28);
  hud_text(F_MONO, 84, 440, "CLOUT  laser, printers, the bench", C_HAZARD, 255, AL_LEFT);
  hud_text(F_MONO, 84, 470, "KARMA  everything else", C_INK_OK, 255, AL_LEFT);
  hud_text(F_MONO_S, 84, 508, "IF THE SPACE CLOSES, ALL CLOUT IS VOID.", C_ASH, 255, AL_LEFT);

  join_panel(916, 300, true);
  if (fmodf(T, 1.2f) < 0.8f) {
    hud_rect(64, 580, 640, 60, C_HAZARD, 255);
    hud_text(F_H2, 384, 588, "PRESS A TO CLOCK IN", C_TAPE, 255, AL_CENTER);
  }
  const char *tip = ATTRACT_TIPS[(int)(T / 3.0f) % COUNT(ATTRACT_TIPS)];
  hud_rect(0, HUD_H - 40, HUD_W, 40, C_TAPE, 255);
  hud_text(F_MONO, HUD_W / 2, HUD_H - 34, tip, C_ASH, 255, AL_CENTER);
}

static void lobby_overlay(void) {
  hud_panel(16, 110, 380, 300, C_SLAB, C_STEEL_HI);
  eyebrow(32, 124, "HOW TUESDAY WORKS", C_ASH, AL_LEFT);
  int y = 150;
  y += hud_text_wrap(F_MONO_S, 32, y, 350, "CLOUT: CUT ON THE LASER, PRINT, SOLDER. CARRY IT TO SHOW & TELL.", C_HAZARD, 255, 20) + 8;
  y += hud_text_wrap(F_MONO_S, 32, y, 350, "KARMA: DISHES, TRASH, FREE PILE, FIX PRINTERS, SAY HI TO NEWBIES.", C_INK_OK, 255, 20) + 8;
  y += hud_text_wrap(F_MONO_S, 32, y, 350, "CLUTTER 100 BRINGS THE FIRE MARSHAL. VIBE 0 MEANS BURNOUT. EITHER WAY YOUR CLOUT IS VOID.", C_BONE, 255, 20) + 8;
  hud_text(F_MONO_S, 32, y, "A  DO IT. HOLD FOR WORK.", C_BONE, 255, AL_LEFT); y += 22;
  hud_text(F_MONO_S, 32, y, "B  DROP IT.", C_BONE, 255, AL_LEFT); y += 22;
  hud_text(F_MONO_S, 32, y, "X  MAKE A PROPOSAL.", C_BONE, 255, AL_LEFT);

  join_panel(1032, 110, false);
  char msg[64];
  int left = (int)ceilf(25 - G.t);
  if (player_count() == 0) snprintf(msg, sizeof msg, "PRESS A TO CLOCK IN");
  else snprintf(msg, sizeof msg, "START TO BEGIN  OR AUTO IN %d", left < 0 ? 0 : left);
  int w = hud_text_w(F_H3, msg) + 40;
  hud_rect(HUD_W / 2 - w / 2, 104, w, 44, C_HAZARD, 255);
  hud_text(F_H3, HUD_W / 2, 110, msg, C_TAPE, 255, AL_CENTER);
}

typedef struct { int slot; const char *award; char text[64]; } row_t;

static void results_screen(void) {
  hud_rect(0, 0, HUD_W, HUD_H, C_CHAR, 215);
  hud_tape(0, 0, HUD_W, 24, T * 20);
  bool ok = G.end == END_SURVIVED;
  const char *head = ok ? "THE SPACE SURVIVES ANOTHER TUESDAY." : G.end == END_FIRE ? "THE FIRE MARSHAL SHUT IT DOWN." : "EVERYONE BURNED OUT.";
  eyebrow(64, 48, ok ? "11:00 PM. LIGHTS OUT." : "HYH IS CANCELED", ok ? C_INK_OK : C_INK_ERR, AL_LEFT);
  hud_text(hud_text_w(F_H1, head) > 1160 ? F_H2 : F_H1, 60, 70, head, C_BONE, 255, AL_LEFT);
  hud_text(F_MONO, 64, 150, ok ? "CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES." : "ALL CLOUT IS VOID. NOBODY CARES ABOUT YOUR LASER MUSTACHE.",
           ok ? C_ASH : C_INK_ERR, 255, AL_LEFT);

  /* order: clout if the space survived, karma if it did not */
  int order[MAX_PLAYERS], n = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active) order[n++] = i;
  for (int a = 1; a < n; a++) {
    int v = order[a], k = a - 1;
    while (k >= 0 && (ok ? P[order[k]].clout < P[v].clout : P[order[k]].karma < P[v].karma)) { order[k + 1] = order[k]; k--; }
    order[k + 1] = v;
  }
  /* awards: one each, most deserved first */
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
    hud_text(F_MONO, HUD_W / 2, HUD_H - 50, "PRESS A FOR ANOTHER TUESDAY", C_BONE, 255, AL_CENTER);
}

void draw_hud(float dt) {
  T += dt;
  hud_clear();
  if (G.phase == PH_TITLE || G.attract) {
    title_screen();
    return;
  }
  if (G.phase == PH_RESULTS) { results_screen(); return; }
  station_labels();
  floor_tags();
  npcs_overlay();
  players_overlay();
  top_bar();
  if (G.phase == PH_LOBBY) lobby_overlay();
  else join_strip(950, 14);
  ticker_band();
  player_cards();
  toasts(16, HUD_H - 118);
}
