/* The night itself: players, stations, messes, newbies and events. */
#include "game.h"
#include "hud.h"
#include "content.h"
#include "audio.h"
#include "net.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "wasmcart.h"

game_t G;
settings_t SET;   /* the saved settings; main.c loads and writes them */
menu_t M;         /* the front end state; main.c drives it, draw.c draws it */
player_t P[MAX_PLAYERS];
item_t I[MAX_ITEMS];
npc_t N[MAX_NPCS];
station_t S[ST_COUNT];

char toast_text[TOASTS][96];
float toast_t[TOASTS];
uint32_t toast_col[TOASTS];

const uint32_t PCOL[MAX_PLAYERS] = { 0xf2ab1e, 0x5fb892, 0x9b7fd0, 0x5e93c4, 0xc2542c, 0xece3d3, 0xe87fa8, 0x8fbde6 };

static uint32_t rng = 2463534242u;
void rnd_seed(uint32_t s) { rng = s ? s : 2463534242u; }
uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
float frnd(void) { return (rnd() & 0xffffff) / 16777216.f; }
#define PICK(arr) (arr[rnd() % COUNT(arr)])

/* The floor plan lives in src/levels.c: stations, furniture, spawn anchors and
 * the night script all come from the active level L. The room shell (24.6 x
 * 14.3, the camera, the wall clamps in collide) does not change per level. */
#define REACH 0.85f

enum { W_NONE, W_PICK, W_DEPOSIT, W_LOAD, W_CUT, W_TAKE, W_CLEAN_LASER, W_PRINT, W_FIX, W_STICKER, W_BENCH,
       W_LUMBER, W_DISH, W_MATE, W_FRIDGE, W_FLOOR, W_GREET,
       /* the new stations' work, wired up one station at a time */
       W_ROCK, W_WIPE, W_SEW, W_LIGHT, W_BATH, W_NOTICE, W_FORGE, W_SHELVE, W_SHUT, W_SWITCH, W_BIKE, W_DRAIN, W_LOITER, W_POKE };

static float hold_time(int kind, int ref) {
  switch (kind) {
    case W_CUT: return 2.4f;
    case W_CLEAN_LASER: return 2.0f;
    case W_FIX: return 2.4f;
    case W_STICKER: return 1.4f;
    case W_BENCH: return 3.2f;
    case W_DISH: return 0.7f;
    case W_FRIDGE: return 2.2f;
    case W_ROCK: return 1.6f;
    case W_WIPE: return 1.8f;
    case W_SEW: return 1.6f;
    case W_BATH: return 3.0f;   /* the longest hold in the game */
    case W_FORGE: return G.heat > 0 ? 1.4f : 2.8f;   /* it is already 118 in the alley */
    case W_SHUT: return 1.2f;
    case W_BIKE: return 1.6f;
    case W_DRAIN: return 1.5f;
    case W_FLOOR: return I[ref].type == IT_DAISY ? 2.0f : I[ref].type == IT_PUDDLE ? 1.5f : 1.2f;
    default: return 0;
  }
}

int player_count(void) { int n = 0; for (int i = 0; i < MAX_PLAYERS; i++) n += P[i].active; return n; }

void game_toast(const char *msg, uint32_t color) {
  for (int i = TOASTS - 1; i > 0; i--) {
    memcpy(toast_text[i], toast_text[i - 1], sizeof toast_text[0]);
    toast_t[i] = toast_t[i - 1]; toast_col[i] = toast_col[i - 1];
  }
  snprintf(toast_text[0], sizeof toast_text[0], "%s", msg);
  toast_t[0] = 6; toast_col[0] = color;
}

static void pop(player_t *p, uint32_t color, const char *fmt, int v) {
  snprintf(p->pop, sizeof p->pop, fmt, v);
  p->pop_color = color;
  p->flash = 1.4f;
}

static void ticker_sub(const char *msg, const char *sub, uint32_t color) {
  snprintf(G.ticker, sizeof G.ticker, "%s", msg);
  snprintf(G.ticker_sub, sizeof G.ticker_sub, "%s", sub ? sub : "");
  G.ticker_t = 5.5f;
  G.ticker_color = color;
}

static void ticker(const char *msg, uint32_t color) {
  G.ticker_sub[0] = 0;
  snprintf(G.ticker, sizeof G.ticker, "%s", msg);
  G.ticker_t = 5.0f;
  G.ticker_color = color;
}

const char *item_name(const item_t *it) {
  switch (it->type) {
    case IT_PLYWOOD: return "PLYWOOD";
    case IT_PROJECT:
      return it->kind == 0 ? LASER_PROJECTS[it->name] : it->kind == 1 ? PRINT_PROJECTS[it->name]
           : it->kind == 2 ? BENCH_PROJECTS[it->name] : it->kind == 4 ? SEWING_PROJECTS[it->name]
           : it->kind == 5 ? FORGE_PROJECTS[it->name] : STICKERS[it->name];
    case IT_SCRAP: return "PLYWOOD SCRAP";
    case IT_BOTTLE: return "EMPTY CLUB-MATE";
    case IT_SPAGHETTI: return "PRINT SPAGHETTI";
    case IT_DONATION: return DONATIONS[it->name].name;
    case IT_MOVEBOX: return "MOVE BOX. LABELED MISC.";
    case IT_SAWDUST: return "SAWDUST";
    case IT_PUDDLE: return "AC LEAK";
    case IT_DAISY: return "DAISY CHAIN";
  }
  return "";
}

static bool is_mess(int t) { return t == IT_SAWDUST || t == IT_PUDDLE || t == IT_DAISY; }
static bool is_junk(int t) { return t == IT_SCRAP || t == IT_BOTTLE || t == IT_SPAGHETTI || t == IT_DONATION || t == IT_MOVEBOX; }

static float weight(const item_t *it) {
  float w;
  switch (it->type) {
    case IT_SCRAP: w = 4; break;
    case IT_BOTTLE: w = 3; break;
    case IT_SPAGHETTI: w = 4; break;
    case IT_DONATION: w = it->heavy ? 14 : 9; break;
    case IT_SAWDUST: w = 5; break;
    case IT_PUDDLE: w = 7; break;
    case IT_DAISY: w = 24; break;
    case IT_PLYWOOD: w = 2; break;
    case IT_MOVEBOX: w = 6; break;
    default: w = 0;
  }
  if (it->age > 25) w *= 1.5f;   /* red tagged */
  if (G.labels_t > 0) w *= 0.5f;  /* somebody labeled everything */
  return w;
}

static int spawn_item(int type, float x, float z, int kind, int name) {
  for (int i = 0; i < MAX_ITEMS; i++) {
    if (I[i].alive) continue;
    item_t *it = &I[i];
    memset(it, 0, sizeof *it);
    it->alive = true; it->type = type; it->x = x; it->z = z; it->holder = -1;
    it->kind = kind; it->name = name;
    if (type == IT_DONATION) it->heavy = DONATIONS[name].heavy;
    it->obj = world_item_obj(type, type == IT_DONATION ? it->heavy : kind);
    return i;
  }
  return -1;
}

static void kill_item(int i) {
  item_t *it = &I[i];
  if (!it->alive) return;
  if (it->holder >= 0 && it->holder < MAX_PLAYERS && P[it->holder].hold == i) P[it->holder].hold = -1;
  for (int k = 0; k < MAX_NPCS; k++) { if (N[k].target == i) N[k].target = -1; if (N[k].hold == i) N[k].hold = -1; }
  world_free(it->obj);
  it->obj = NULL;
  it->alive = false;
}

static void give(player_t *p, int item) {
  if (item < 0) return;
  I[item].holder = (int)(p - P);
  p->hold = item;
}

static bool adding_bot;
static bool sewing_seen;   /* the first sewing toast of the night */
static float tour_t;       /* E_TOUR: the next of the group walks in */
static int light_donation(void);
static void spawn_newbie(bool tour);
static void clear_world(void);
static void calm_meters(void);

/* ── geometry ────────────────────────────────────────────────────── */

static float box_dist(float x, float z, const station_t *s) {
  float dx = fabsf(x - s->x) - s->hw, dz = fabsf(z - s->z) - s->hd;
  if (dx < 0) dx = 0;
  if (dz < 0) dz = 0;
  return sqrtf(dx * dx + dz * dz);
}

/* a station the simulation treats as a solid: present, and not one of the two
 * that live inside the wall planes behind the clamps */
static bool blocks(int i) { return S[i].present && i != ST_SWITCH && i != ST_BACKDOOR; }

/* nowhere near a station or a piece of furniture */
static bool clear_spot(float x, float z, float margin) {
  for (int i = 0; i < ST_COUNT; i++) if (blocks(i) && box_dist(x, z, &S[i]) < margin) return false;
  for (int i = 0; i < L->ob_n; i++) {
    const obst_t *o = &L->ob[i];
    float dx = fabsf(x - o->x) - o->hw, dz = fabsf(z - o->z) - o->hd;
    if (dx < margin && dz < margin) return false;
  }
  return true;
}

/* a random clear point inside a box; false when twenty tries found none */
static bool spot_in(const box_t *b, float margin, float *x, float *z) {
  for (int tries = 0; tries < 20; tries++) {
    *x = b->x0 + frnd() * (b->x1 - b->x0);
    *z = b->z0 + frnd() * (b->z1 - b->z0);
    if (clear_spot(*x, *z, margin)) return true;
  }
  return false;
}

/* where a station is used from: its face pushed out by m */
static void approach_point(int i, float m, float *x, float *z) {
  const station_t *s = &S[i];
  switch (s->face) {
    case FACE_NZ: *x = s->x; *z = s->z - s->hd - m; break;
    case FACE_PX: *x = s->x + s->hw + m; *z = s->z; break;
    case FACE_NX: *x = s->x - s->hw - m; *z = s->z; break;
    default: *x = s->x; *z = s->z + s->hd + m; break;
  }
}

static void collide(float *x, float *z, float r) {
  if (*x < -11.6f) *x = -11.6f;
  if (*x > 11.6f) *x = 11.6f;
  if (*z < -6.6f) *z = -6.6f;
  if (*z > 7.0f && fabsf(*x - L->door_x) > L->door_hw) *z = 7.0f;
  if (*z > 8.5f) *z = 8.5f;
  for (int i = 0; i < ST_COUNT + L->ob_n; i++) {
    station_t ob;
    const station_t *s = &ob;
    if (i < ST_COUNT) { if (!blocks(i)) continue; s = &S[i]; }
    else { const obst_t *o = &L->ob[i - ST_COUNT]; ob.x = o->x; ob.z = o->z; ob.hw = o->hw; ob.hd = o->hd; }
    float cx = fmaxf(s->x - s->hw, fminf(*x, s->x + s->hw));
    float cz = fmaxf(s->z - s->hd, fminf(*z, s->z + s->hd));
    float dx = *x - cx, dz = *z - cz, d2 = dx * dx + dz * dz;
    if (d2 >= r * r) continue;
    if (d2 > 1e-6f) {
      float d = sqrtf(d2);
      *x = cx + dx / d * r; *z = cz + dz / d * r;
    } else {
      /* centre inside the box: leave by the nearest face */
      float l = *x - (s->x - s->hw), rr = (s->x + s->hw) - *x, b = *z - (s->z - s->hd), f = (s->z + s->hd) - *z;
      float m = fminf(fminf(l, rr), fminf(b, f));
      if (m == l) *x = s->x - s->hw - r; else if (m == rr) *x = s->x + s->hw + r;
      else if (m == b) *z = s->z - s->hd - r; else *z = s->z + s->hd + r;
    }
  }
}

/* ── finding the way: a walkable grid and a breadth first search ─────
 * Straight line steering wedges on the shop fence, the counters and the
 * vestibule, so bots and newbies plan on a 0.2 unit grid of cells a body can
 * stand in and steer toward the farthest cell of the path they can see. The
 * grid is rebuilt when the level or the set of present stations changes. */
#define NAV_P 0.2f
#define NAV_X0 (-12.3f)
#define NAV_Z0 (-7.15f)
#define NAV_NX 123
#define NAV_NZ 79
#define NAV_R 0.30f    /* planning clearance; collide keeps the last 0.08 of the body's radius */
static uint8_t nav_ok[NAV_NZ][NAV_NX];
static int16_t nav_from[NAV_NZ * NAV_NX];
static uint16_t nav_q[NAV_NZ * NAV_NX];
static const level_t *nav_level;
static uint32_t nav_sig;

static int nav_cx(float x) { int i = (int)floorf((x - NAV_X0) / NAV_P); return i < 0 ? 0 : i >= NAV_NX ? NAV_NX - 1 : i; }
static int nav_cz(float z) { int j = (int)floorf((z - NAV_Z0) / NAV_P); return j < 0 ? 0 : j >= NAV_NZ ? NAV_NZ - 1 : j; }
static float nav_x(int i) { return NAV_X0 + (i + 0.5f) * NAV_P; }
static float nav_z(int j) { return NAV_Z0 + (j + 0.5f) * NAV_P; }

static bool nav_clear(float x, float z) {
  if (x < -11.6f || x > 11.6f || z < -6.6f || z > 8.5f) return false;
  if (z > 7.0f && fabsf(x - L->door_x) > L->door_hw - NAV_R) return false;
  for (int i = 0; i < ST_COUNT + L->ob_n; i++) {
    float bx, bz, hw, hd;
    if (i < ST_COUNT) { if (!blocks(i)) continue; bx = S[i].x; bz = S[i].z; hw = S[i].hw; hd = S[i].hd; }
    else { const obst_t *o = &L->ob[i - ST_COUNT]; bx = o->x; bz = o->z; hw = o->hw; hd = o->hd; }
    float dx = fmaxf(0, fabsf(x - bx) - hw), dz = fmaxf(0, fabsf(z - bz) - hd);
    if (dx * dx + dz * dz < NAV_R * NAV_R) return false;
  }
  return true;
}

static void nav_build(void) {
  for (int j = 0; j < NAV_NZ; j++) for (int i = 0; i < NAV_NX; i++) nav_ok[j][i] = nav_clear(nav_x(i), nav_z(j));
  nav_level = L;
  nav_sig = 0;
  for (int i = 0; i < ST_COUNT; i++) nav_sig |= (uint32_t)S[i].present << i;
}

/* the nearest walkable cell to (i, j) within three rings, or -1 */
static int nav_snap(int i, int j) {
  for (int ring = 0; ring <= 3; ring++)
    for (int dj = -ring; dj <= ring; dj++) for (int di = -ring; di <= ring; di++) {
      if (abs(di) != ring && abs(dj) != ring) continue;
      int ni = i + di, nj = j + dj;
      if (ni >= 0 && nj >= 0 && ni < NAV_NX && nj < NAV_NZ && nav_ok[nj][ni]) return nj * NAV_NX + ni;
    }
  return -1;
}

static bool nav_los(float x0, float z0, float x1, float z1) {
  float dx = x1 - x0, dz = z1 - z0;
  int n = (int)(sqrtf(dx * dx + dz * dz) / 0.1f) + 1;
  for (int k = 1; k <= n; k++) {
    float t = (float)k / n;
    if (!nav_ok[nav_cz(z0 + dz * t)][nav_cx(x0 + dx * t)]) return false;
  }
  return true;
}

/* a path from (x, z) to (tx, tz): true and the next waypoint, or false when no path exists */
static bool nav_plan(float x, float z, float tx, float tz, float *wx, float *wz) {
  uint32_t sig = 0;
  for (int i = 0; i < ST_COUNT; i++) sig |= (uint32_t)S[i].present << i;
  if (nav_level != L || sig != nav_sig) nav_build();
  int s = nav_snap(nav_cx(x), nav_cz(z)), g = nav_snap(nav_cx(tx), nav_cz(tz));
  if (s < 0 || g < 0) return false;
  memset(nav_from, -1, sizeof nav_from);
  int head = 0, tail = 0;
  nav_q[tail++] = (uint16_t)g; nav_from[g] = (int16_t)g;
  while (head < tail && nav_from[s] < 0) {
    int c = nav_q[head++], ci = c % NAV_NX, cj = c / NAV_NX;
    for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) {
      if (!di && !dj) continue;
      int ni = ci + di, nj = cj + dj;
      if (ni < 0 || nj < 0 || ni >= NAV_NX || nj >= NAV_NZ || !nav_ok[nj][ni]) continue;
      if (di && dj && (!nav_ok[cj][ni] || !nav_ok[nj][ci])) continue;   /* no cutting corners */
      int n = nj * NAV_NX + ni;
      if (nav_from[n] >= 0) continue;
      nav_from[n] = (int16_t)c; nav_q[tail++] = (uint16_t)n;
    }
  }
  if (nav_from[s] < 0) return false;
  /* walk the chain toward the goal and keep the farthest cell still in sight */
  int c = s, best = nav_from[s];
  for (int k = 0; k < 60 && c != g; k++) {
    c = nav_from[c];
    if (nav_los(x, z, nav_x(c % NAV_NX), nav_z(c / NAV_NX))) best = c; else break;
  }
  if (best == g) { *wx = tx; *wz = tz; }
  else { *wx = nav_x(best % NAV_NX); *wz = nav_z(best / NAV_NX); }
  return true;
}

/* ── players ─────────────────────────────────────────────────────── */

/* a human walked up to the cabinet (or PLAY was chosen): the demo is over */
void game_begin_lobby(void) {
  if (G.attract) {
    game_remove_bots();
    clear_world();
    G.attract = false;
    G.phase = PH_LOBBY; G.t = 0;
    calm_meters();
    for (int i = 0; i < TOASTS; i++) toast_t[i] = 0;
    audio_music(1);
  } else if (G.phase == PH_TITLE) { G.phase = PH_LOBBY; G.t = 0; }
  for (int i = 0; i < MAX_PLAYERS; i++) P[i].ready = false;
  G.mode = SET.mode;
  game_apply_settings();
}

/* settings into the sim; SET itself is never written here */
void game_apply_settings(void) {
  int chaos = SET.chaos <= CHAOS_CHAOS ? SET.chaos : CHAOS_NORMAL;
  int len = SET.night_len < COUNT(NIGHT_LENS) ? SET.night_len : 1;
  /* NORMAL is the level's own length (config.json night_seconds scales the classic night) */
  G.night_len = len == 1 ? L->night.len : NIGHT_LENS[len].secs;
  G.pace = PACE[chaos]; G.badmul = BADMUL[chaos]; G.jam = 0.3f; G.want_bots = SET.bots != 0;
  switch (G.mode) {
    case MODE_SOLO: G.want_bots = true; break;
    case MODE_CHAOS: G.night_len = 120; G.pace = 0.65f; G.badmul = 2; G.jam = 0.6f; G.want_bots = true; break;
    default: break;   /* CO-OP, VERSUS and CUSTOM take SET as it is */
  }
}

/* the attract night ignores the settings: it is the level as written */
static void default_settings(void) {
  G.night_len = L->night.len; G.pace = 1; G.badmul = 1; G.jam = 0.3f; G.want_bots = true;
}

int game_mode_bots(int *a, int *b) {
  if (G.mode == MODE_VERSUS) { *a = BOT_BUILDER; *b = BOT_FOUNDER; }
  else { *a = L->night.solo_bots[0]; *b = L->night.solo_bots[1]; }
  return 2;
}

/* drop everyone and everything and go back to the title */
void game_quit_to_title(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active) game_remove_player(i);
  clear_world();
  calm_meters();
  G.attract = false; G.paused = false;
  G.phase = PH_TITLE; G.t = 0; G.end = END_NONE;
  for (int i = 0; i < TOASTS; i++) toast_t[i] = 0;
  audio_music(1);
}

int game_add_player(bool phone, int pad, const char *pid, const char *name) {
  if (G.attract && !adding_bot) game_begin_lobby();
  int slot = -1;
  for (int i = 0; i < MAX_PLAYERS; i++) if (!P[i].active) { slot = i; break; }
  if (slot < 0) return -1;
  player_t *p = &P[slot];
  memset(p, 0, sizeof *p);
  p->active = true; p->phone = phone; p->pad = pad; p->hold = -1; p->kb = -1; p->seat = -1;
  snprintf(p->pid, sizeof p->pid, "%s", pid ? pid : "");
  if (name && *name) snprintf(p->name, sizeof p->name, "%s", name);
  else snprintf(p->name, sizeof p->name, "PLAYER %d", slot + 1);
  p->color = PCOL[slot];
  p->x = L->lobby_x + (slot % 4) * 1.1f; p->z = L->lobby_z + (slot / 4) * 0.9f;   /* rows at z 4.1 and 5.0: the first four stand above the seat cards */
  p->face = 3.14159f;
  p->obj = world_person(p->color, false, &p->body);
  char msg[64];
  if (!adding_bot) {
    /* a pad or keyboard join is announced by its lobby card; phones keep the toast */
    if (phone) { snprintf(msg, sizeof msg, "%s JOINED FROM A PHONE", p->name); game_toast(msg, p->color); }
    audio_play(SFX_JOIN);
    fx_burst(FX_JOIN, p->x, 1.0f, p->z, p->color);
  }
  if (G.phase == PH_TITLE) { G.phase = PH_LOBBY; G.t = 0; G.mode = SET.mode; }
  return slot;
}

static void drop(player_t *p) {
  if (p->hold < 0) return;
  item_t *it = &I[p->hold];
  it->holder = -1;
  it->x = p->x + sinf(p->face) * 0.7f;
  it->z = p->z + cosf(p->face) * 0.7f;
  collide(&it->x, &it->z, 0.2f);
  it->y = 0; it->age = 0;
  p->hold = -1;
}

void game_remove_player(int slot) {
  player_t *p = &P[slot];
  if (!p->active) return;
  drop(p);
  world_free(p->obj);
  p->obj = NULL;
  p->active = false;
}

void game_player_input(int slot, uint32_t btn, float sx, float sy) {
  player_t *p = &P[slot];
  p->btn = btn; p->sx = sx; p->sy = sy;
}

/* find the best thing for a player to do right now */
typedef struct { int kind, ref; float d; const char *label; bool karma; } cand_t;

static const player_t *ft_p;   /* whose targets find_target is weighing */
static void consider(cand_t *best, int kind, int ref, float d, const char *label, bool karma) {
  /* a bot's own goal wins a tie with a neighbour (the bike station stands beside the scrap bays) */
  if (ft_p && ft_p->bot && kind == ft_p->goal && ref == ft_p->goal_ref) d -= 0.4f;
  if (d < best->d) { best->kind = kind; best->ref = ref; best->d = d; best->label = label; best->karma = karma; }
}

static cand_t find_target(player_t *p) {
  cand_t c = { W_NONE, -1, 1e9f, NULL, false };
  ft_p = p;
  if (p->hold >= 0) {
    int t = I[p->hold].type;
    float d;
    if (S[ST_TRASH].present && (d = box_dist(p->x, p->z, &S[ST_TRASH])) < REACH && (is_junk(t) || t == IT_PLYWOOD))
      consider(&c, W_DEPOSIT, ST_TRASH, d, "THROW OUT", true);
    if (S[ST_FREEPILE].present && (d = box_dist(p->x, p->z, &S[ST_FREEPILE])) < REACH && (t == IT_DONATION || t == IT_SCRAP || t == IT_PLYWOOD))
      consider(&c, W_DEPOSIT, ST_FREEPILE, d, "FREE PILE IT", true);
    if (S[ST_SHOW].present && (d = box_dist(p->x, p->z, &S[ST_SHOW])) < REACH && t == IT_PROJECT)
      consider(&c, W_DEPOSIT, ST_SHOW, d, "SHOW IT OFF", false);
    if (S[ST_LASER].present && (d = box_dist(p->x, p->z, &S[ST_LASER])) < REACH && t == IT_PLYWOOD && S[ST_LASER].state == S_IDLE)
      consider(&c, W_LOAD, ST_LASER, d, "LOAD THE LASER", false);
    if (S[ST_STORAGE].present && (d = box_dist(p->x, p->z, &S[ST_STORAGE])) < REACH && t == IT_MOVEBOX)
      consider(&c, W_DEPOSIT, ST_STORAGE, d - 0.01f, "SHELVE THE BOX", true);
    return c;
  }
  for (int i = 0; i < MAX_ITEMS; i++) {
    item_t *it = &I[i];
    if (!it->alive || it->holder >= 0) continue;
    float dx = it->x - p->x, dz = it->z - p->z, d = sqrtf(dx * dx + dz * dz);
    if (d > 1.15f) continue;
    if (is_mess(it->type)) consider(&c, W_FLOOR, i, d, it->type == IT_DAISY ? "UNPLUG IT" : "CLEAN UP", true);
    else consider(&c, W_PICK, i, d, "PICK UP", is_junk(it->type));
  }
  for (int i = 0; i < ST_COUNT; i++) {
    station_t *s = &S[i];
    if (!s->present) continue;
    float d = box_dist(p->x, p->z, s);
    if (d > REACH) continue;
    switch (i) {
      case ST_LUMBER: consider(&c, W_LUMBER, i, d, "GRAB PLYWOOD", false); break;
      case ST_LASER:
        if (s->state == S_LOADED) consider(&c, W_CUT, i, d, "CUT", false);
        else if (s->state == S_BROKEN) consider(&c, W_CLEAN_LASER, i, d, "CLEAN THE LASER", true);
        break;
      case ST_PRINT1: case ST_PRINT2:
        if (s->state == S_IDLE) consider(&c, W_PRINT, i, d, "START A PRINT", false);
        else if (s->state == S_DONE) consider(&c, W_TAKE, i, d, "TAKE THE PRINT", false);
        else if (s->state == S_BROKEN) consider(&c, W_FIX, i, d, "UNCLOG IT", true);
        break;
      case ST_CUTSALOT: consider(&c, W_STICKER, i, d, "CUT A STICKER", false); break;
      case ST_BENCH: consider(&c, W_BENCH, i, d, "SOLDER SOMETHING", false); break;
      case ST_SINK:
        if (s->state == S_BROKEN) consider(&c, W_DRAIN, i, d, "POUR SIX CUPS DOWN THE DRAIN", true);
        else if (s->count > 0) consider(&c, W_DISH, i, d, "WASH A DISH", true);
        break;
      case ST_FRIDGE:
        if (s->smell) consider(&c, W_FRIDGE, i, d, "DEAL WITH THE FRIDGE", true);
        else if (p->boost <= 0 && !L->night.fridge_locked) consider(&c, W_MATE, i, d, "GRAB A DRINK", false);
        break;
      case ST_VEND:
        if (s->state == S_BROKEN) consider(&c, W_ROCK, i, d, "ROCK IT", true);
        else if (p->boost <= 0) consider(&c, W_MATE, i, d, "BUY A DRINK", false);
        break;
      case ST_BOARD: if (s->count > 0) consider(&c, W_WIPE, i, d, "WIPE THE BOARD", true); break;
      case ST_SEWING: consider(&c, W_SEW, i, d, "SEW SOMETHING", false); break;
      case ST_BATH:
        if (s->timer >= 50) consider(&c, W_BATH, i, d, "CLEAN THE BATHROOM", true);
        else if (s->count) consider(&c, W_LIGHT, i, d, "LIGHT OFF", true);
        break;
      case ST_STORAGE: if (s->count >= 4) consider(&c, W_NOTICE, i, d, "POST A NOTICE", true); break;
      case ST_FORGE: if (s->state != S_BROKEN) consider(&c, W_FORGE, i, d, "WELD SOMETHING", false); break;
      case ST_LIBRARY: if (s->count > 0) consider(&c, W_SHELVE, i, d, "SHELVE A BOOK", true); break;
      case ST_BACKDOOR: if (s->state == S_BROKEN) consider(&c, W_SHUT, i, d, "SHUT THE DOOR", true); break;
      case ST_SWITCH: consider(&c, W_SWITCH, i, d, "THE LIGHTS", false); break;
      case ST_BIKE: if (s->state == S_BROKEN) consider(&c, W_BIKE, i, d, "FIX THE FLAT", true); break;
      default: break;
    }
  }
  /* the sleeper on the couch: the lowest priority there is */
  for (int i = 0; i < L->prop_n; i++) {
    const prop_t *pr = &L->props[i];
    if (pr->kind != PR_SLEEPER) continue;
    float dx = pr->x - p->x, dz = pr->z - p->z, d = sqrtf(dx * dx + dz * dz);
    if (d < 1.2f) consider(&c, W_POKE, -1, d + 5, "POKE THE SLEEPER", false);
  }
  for (int i = 0; i < MAX_NPCS; i++) {
    npc_t *n = &N[i];
    if (!n->alive || n->state != 1) continue;
    float dx = n->x - p->x, dz = n->z - p->z, d = sqrtf(dx * dx + dz * dz);
    if (d < 1.6f) consider(&c, W_GREET, i, d - 0.5f, "SAY HI", true);
  }
  return c;
}

static int project_value(int kind) {
  switch (kind) {
    case 0: return 900 + (int)(rnd() % 300);
    case 1: return 600 + (int)(rnd() % 200);
    case 2: return 450 + (int)(rnd() % 150);
    case 4: return 300 + (int)(rnd() % 100);
    case 5: return 700 + (int)(rnd() % 200);
    default: return 150;
  }
}

static int make_project(player_t *p, int kind) {
  int n = kind == 0 ? COUNT(LASER_PROJECTS) : kind == 1 ? COUNT(PRINT_PROJECTS) : kind == 2 ? COUNT(BENCH_PROJECTS)
        : kind == 4 ? COUNT(SEWING_PROJECTS) : kind == 5 ? COUNT(FORGE_PROJECTS) : COUNT(STICKERS);
  int it = spawn_item(IT_PROJECT, p->x, p->z, kind, (int)(rnd() % n));
  if (it >= 0) { I[it].value = project_value(kind); give(p, it); }
  return it;
}

static void karma(player_t *p, int v) {
  p->karma += v; p->tasks++;
  G.team_karma += v;
  fx_burst(FX_KARMA, p->x, 1.6f, p->z, C_LIVE);
  pop(p, C_INK_OK, "+%d KARMA", v);
  audio_play(SFX_KARMA);
}

static void act(player_t *p, int kind, int ref) {
  char msg[96];
  station_t *s = ref >= 0 && ref < ST_COUNT ? &S[ref] : NULL;
  switch (kind) {
    case W_PICK:
      give(p, ref);
      audio_play(SFX_PICKUP);
      break;
    case W_DEPOSIT: {
      item_t *it = &I[p->hold];
      if (ref == ST_SHOW) {
        int before = p->clout;
        p->clout += it->value;
        G.vibe = fminf(100, G.vibe + 1);
        S[ST_SHOW].timer = 4;
        fx_burst(FX_CONFETTI, S[ST_SHOW].x, 1.0f, S[ST_SHOW].z, p->color);
        pop(p, C_HAZARD, "+%d CLOUT", it->value);
        snprintf(msg, sizeof msg, "%s: %s", p->name, item_name(it));
        game_toast(msg, C_HAZARD);
        audio_play(SFX_GLAMOUR);
        if (before <= 9000 && p->clout > 9000) {
          snprintf(msg, sizeof msg, "%s'S CLOUT IS OVER 9000", p->name);
          ticker(msg, C_HAZARD);
        }
      } else if (ref == ST_TRASH) {
        int v = it->type == IT_SCRAP ? 40 : it->type == IT_BOTTLE ? 30 : it->type == IT_SPAGHETTI ? 50 : it->type == IT_DONATION ? 50
              : it->type == IT_MOVEBOX ? 20 : 0;
        if (v) karma(p, v); else { pop(p, C_INK_ERR, "WASTED PLYWOOD", 0); audio_play(SFX_DROP); }
        if (it->type == IT_SPAGHETTI && !p->bot) game_toast(TOAST_SPAGHETTI, C_INK_OK);
      } else if (ref == ST_FREEPILE) {
        int v = it->type == IT_DONATION ? 90 : it->type == IT_SCRAP ? 40 : 20;
        karma(p, v);
        if (it->type == IT_DONATION) { p->shelved++; game_toast(TOAST_SHELVED, C_ASH); }
      } else if (ref == ST_STORAGE) karma(p, 35);
      kill_item(p->hold);
      break;
    }
    case W_LOAD:
      kill_item(p->hold);
      s->state = S_LOADED;
      audio_play(SFX_DROP);
      break;
    case W_CUT:
      make_project(p, 0);
      if (!p->bot) game_toast(TOAST_CUT, C_HAZARD);
      spawn_item(IT_SCRAP, s->x + (frnd() - 0.5f) * 2.0f, s->z + s->hd + 0.6f, 0, 0);
      s->dust += L->night.cut_dust + frnd() * L->night.cut_dust_rnd;
      s->state = S_IDLE;
      if (s->dust >= 100) {
        s->state = S_BROKEN;
        snprintf(msg, sizeof msg, rnd() & 1 ? "%s: EVERY FREAKING INCH IS COVERED IN DUST" : "%s: HOOD NOT FULLY CLOSED", s->label);
        ticker(msg, C_RUST);
        audio_play(SFX_BAD);
      }
      break;
    case W_CLEAN_LASER:
      s->dust = 0; s->state = S_IDLE;
      karma(p, 80);
      break;
    case W_PRINT:
      s->state = S_BUSY; s->timer = 14;
      s->count = frnd() < G.jam ? 1 : 0;      /* this one is going to fail */
      audio_play(SFX_TICK);
      break;
    case W_TAKE:
      make_project(p, 1);
      s->state = S_IDLE;
      audio_play(SFX_PICKUP);
      break;
    case W_FIX:
      s->state = S_IDLE;
      karma(p, 80);
      p->fixes++;
      snprintf(msg, sizeof msg, PICK(FIXED_PRINTER), s->label);
      game_toast(msg, C_INK_OK);
      break;
    case W_STICKER: make_project(p, 3); audio_play(SFX_PICKUP); break;
    case W_BENCH: make_project(p, 2); audio_play(SFX_PICKUP); break;
    case W_LUMBER:
      give(p, spawn_item(IT_PLYWOOD, p->x, p->z, 0, 0));
      if (frnd() < 0.45f) spawn_item(IT_SAWDUST, s->x + (frnd() - 0.5f) * 1.6f, s->z + s->hd + 0.6f + frnd(), 0, 0);   /* on the bays' +z face */
      audio_play(SFX_PICKUP);
      break;
    case W_DISH:
      if (s->count > 0) {
        s->count--; p->dishes++; karma(p, 15);
        if (s->count == 0 && !p->bot) game_toast(TOAST_DISHES, C_INK_OK);
      }
      break;
    case W_MATE:
      p->boost = 7; p->mates++; p->owes_bottle = true;
      if (p->mates == 1 && !p->bot) game_toast(TOAST_MATE, C_HAZARD);
      pop(p, C_HAZARD, "SPEED", 0);
      audio_play(SFX_SLURP);
      if (ref == ST_VEND && rnd() % 4 == 0) {   /* the drink came out; the next dollar will not */
        s->state = S_BROKEN;
        game_toast(TOAST_VEND_ATE, C_RUST);
        audio_play(SFX_BAD);
      }
      break;
    case W_ROCK:
      s->state = S_IDLE;
      karma(p, 35);
      p->boost = 7; p->mates++; p->owes_bottle = true;   /* and the drink comes out */
      game_toast(TOAST_VEND_ROCK, C_INK_OK);
      fx_burst(FX_SPARK, s->x + s->hw, 1.2f, s->z, C_BONE);
      break;
    case W_WIPE:
      karma(p, 20 + 10 * s->count);
      p->wiped++;
      if (s->dust > 0 && rnd() % 5 == 0) {
        s->dust = 0; G.vibe -= 4;
        game_toast(TOAST_CORNER, C_INK_ERR);
      } else {
        snprintf(msg, sizeof msg, TOAST_WIPED, s->count);
        game_toast(msg, C_INK_OK);
      }
      s->count = 0;
      break;
    case W_SEW:
      make_project(p, 4);
      p->sewn++;
      audio_play(SFX_PICKUP);
      break;
    case W_LIGHT:
      s->count = 0;
      karma(p, 10);
      break;
    case W_BATH:
      s->timer = 0; s->count = 0;
      karma(p, 150);
      p->baths++;
      game_toast(TOAST_BATH, C_INK_OK);
      break;
    case W_NOTICE:
      p->notices++;
      if (++s->dust >= 3) {
        /* the third notice: the contents become a donation and go round through the free shelf */
        s->count = 0; s->dust = 0;
        karma(p, 60);
        float ax, az;
        approach_point(ref, 0.5f, &ax, &az);
        spawn_item(IT_DONATION, ax, az, 0, light_donation());
        game_toast(TOAST_NOTICE, C_INK_OK);
      } else karma(p, 10);
      break;
    case W_FORGE: {
      make_project(p, 5);
      p->welds++;
      s->dust += 35;
      float ax, az;
      approach_point(ref, 0.4f, &ax, &az);
      if (frnd() < 0.4f) spawn_item(IT_SCRAP, ax + (frnd() - 0.5f) * 0.6f, az + (frnd() - 0.5f) * 0.8f, 0, 0);   /* slag */
      if (G.clutter > 70) { G.fire_warn += 1.0f; game_toast(TOAST_SPARKS, C_INK_ERR); }
      if (!G.forge_toast && G.phase == PH_NIGHT) { G.forge_toast = true; game_toast(TOAST_FORGE_LIT, C_HAZARD); }
      audio_play(SFX_PICKUP);
      break;
    }
    case W_SHELVE:
      if (s->count > 0) s->count--;
      karma(p, 12);
      break;
    case W_SHUT:
      s->state = S_IDLE;
      karma(p, 60);
      game_toast(TOAST_DOOR_SHUT, C_INK_OK);
      break;
    case W_SWITCH:
      if (game_lights_dark()) { G.lights_on = !G.lights_on; G.lights_t = G.lights_on ? 25 : 0; }
      else { G.lights_on = true; G.lights_t = 6; game_toast(TOAST_LIGHTS_DAY, C_ASH); }
      audio_play(SFX_TICK);
      break;
    case W_BIKE:
      s->state = S_IDLE;
      karma(p, 60);
      p->flats++;
      game_toast(TOAST_FLAT, C_INK_OK);
      break;
    case W_DRAIN:
      s->state = S_IDLE;
      karma(p, 60);
      game_toast(TOAST_DRAIN, C_INK_OK);
      break;
    case W_POKE:
      if (G.sleeper_cool <= 0) {
        G.sleeper_cool = 30;
        G.vibe = fminf(100, G.vibe + 1);
        game_toast(TOAST_SLEEPER, C_ASH);
      }
      break;
    case W_FRIDGE:
      s->smell = false;
      karma(p, 90);
      game_toast(TOAST_FRIDGE, C_INK_OK);
      break;
    case W_FLOOR: {
      int t = I[ref].type;
      kill_item(ref);
      karma(p, t == IT_DAISY ? 120 : t == IT_PUDDLE ? 50 : 40);
      if (t == IT_DAISY) { p->unplugged++; game_toast(TOAST_UNPLUGGED, C_INK_OK); }
      else if (!p->bot) game_toast(t == IT_PUDDLE ? TOAST_MOPPED : TOAST_SWEPT, C_INK_OK);
      break;
    }
    case W_GREET: {
      npc_t *n = &N[ref];
      n->state = 2; n->life = L->night.member_life; n->target = -1;
      p->greeted++; G.newbies_greeted++;
      karma(p, L->night.greet_karma);
      G.vibe = fminf(100, G.vibe + L->night.greet_vibe);
      game_toast(PICK(NEWBIE_JOINS), C_INK_OK);
      if (p->greeted == 1 && !p->bot) game_toast(TOAST_FIRST_HI, C_INK_OK);
      break;
    }
  }
}


/* ── bots: the archetypes, playing as members ────────────────────── */

static const char *const BOT_NAMES[] = { "", "BUILDER", "TALKER", "FOUNDER" };
const char *game_bot_name(int arch) { return arch > 0 && arch < COUNT(BOT_NAMES) ? BOT_NAMES[arch] : ""; }

int human_count(void) { int n = 0; for (int i = 0; i < MAX_PLAYERS; i++) n += P[i].active && !P[i].bot; return n; }

int game_add_bot(int arch) {
  int phase = G.phase;
  adding_bot = true;
  int slot = game_add_player(false, -1, NULL, BOT_NAMES[arch]);
  adding_bot = false;
  G.phase = phase;
  if (slot >= 0) { P[slot].bot = arch; P[slot].talk_in = 4 + frnd() * 6; }
  return slot;
}

void game_remove_bots(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && P[i].bot) game_remove_player(i);
}

static bool goal_valid(player_t *p) {
  int r = p->goal_ref;
  switch (p->goal) {
    case W_PICK: case W_FLOOR: return I[r].alive && I[r].holder < 0 && p->hold < 0;
    case W_GREET: return N[r].alive && N[r].state == 1 && p->hold < 0;
    case W_DEPOSIT: return p->hold >= 0;
    case W_LOAD: return p->hold >= 0 && S[ST_LASER].state == S_IDLE;
    case W_CUT: return p->hold < 0 && S[ST_LASER].state == S_LOADED;
    case W_LUMBER: return p->hold < 0 && S[ST_LASER].state == S_IDLE;
    case W_CLEAN_LASER: return p->hold < 0 && S[ST_LASER].state == S_BROKEN;
    case W_PRINT: return p->hold < 0 && S[r].state == S_IDLE;
    case W_TAKE: return p->hold < 0 && S[r].state == S_DONE;
    case W_FIX: return p->hold < 0 && S[r].state == S_BROKEN;
    case W_DISH: return p->hold < 0 && S[ST_SINK].count > 0 && S[ST_SINK].state != S_BROKEN;
    case W_DRAIN: return p->hold < 0 && S[ST_SINK].state == S_BROKEN;
    case W_NOTICE: return p->hold < 0 && S[ST_STORAGE].count >= 4;
    case W_FORGE: return p->hold < 0 && S[ST_FORGE].state != S_BROKEN;
    case W_SHELVE: return p->hold < 0 && S[ST_LIBRARY].count > 0;
    case W_SHUT: return p->hold < 0 && S[ST_BACKDOOR].state == S_BROKEN;
    case W_BIKE: return p->hold < 0 && S[ST_BIKE].state == S_BROKEN;
    case W_FRIDGE: return p->hold < 0 && S[ST_FRIDGE].smell;
    case W_BENCH: case W_STICKER: case W_SEW: return p->hold < 0;
    case W_ROCK: return p->hold < 0 && S[ST_VEND].state == S_BROKEN;
    case W_WIPE: return p->hold < 0 && S[ST_BOARD].count > 0;
    case W_LIGHT: return p->hold < 0 && S[ST_BATH].count && S[ST_BATH].timer < 50;
    case W_BATH: return p->hold < 0 && S[ST_BATH].timer >= 50;
    case W_LOITER: return p->hold < 0 && p->bot == BOT_TALKER && p->talk_in > 0 && S[ST_BOARD].present;
  }
  return false;
}

static bool goal_taken(player_t *p, int kind, int ref) {
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (&P[i] != p && P[i].active && P[i].bot && P[i].goal == kind && P[i].goal_ref == ref && kind != W_DISH) return true;
  return false;
}

static void choose_goal(player_t *p) {
  float g = p->bot == BOT_FOUNDER ? 0.85f : p->bot == BOT_TALKER ? 0.55f : 0.15f;
  p->goal = W_NONE; p->goal_ref = -1;
  if (p->hold >= 0) {
    int t = I[p->hold].type;
    if (t == IT_PROJECT) { if (S[ST_SHOW].present) { p->goal = W_DEPOSIT; p->goal_ref = ST_SHOW; } }
    else if (t == IT_DONATION) { if (S[ST_FREEPILE].present) { p->goal = W_DEPOSIT; p->goal_ref = ST_FREEPILE; } }
    else if (t == IT_PLYWOOD) { if (S[ST_LASER].present && S[ST_LASER].state == S_IDLE) { p->goal = W_LOAD; p->goal_ref = ST_LASER; } }
    else if (t == IT_MOVEBOX && S[ST_STORAGE].present) { p->goal = W_DEPOSIT; p->goal_ref = ST_STORAGE; }
    else if (S[ST_TRASH].present) { p->goal = W_DEPOSIT; p->goal_ref = ST_TRASH; }
    return;
  }
  float best = 0;
#define WANT(k, r, base, glam, WX, WZ) do { \
    float dx = (WX) - p->x, dz = (WZ) - p->z; \
    float sc = (base) * ((glam) ? g : 1 - g) / (1 + sqrtf(dx * dx + dz * dz) * 0.12f) + frnd() * 0.25f; \
    if (sc > best && !goal_taken(p, k, r)) { best = sc; p->goal = k; p->goal_ref = r; } } while (0)
  for (int i = 0; i < MAX_ITEMS; i++) {
    item_t *it = &I[i];
    if (!it->alive || it->holder >= 0) continue;
    if (is_mess(it->type)) WANT(W_FLOOR, i, it->type == IT_DAISY ? 2.0f : 1.2f, false, it->x, it->z);
    else if (is_junk(it->type)) WANT(W_PICK, i, 1.0f, false, it->x, it->z);
    else if (it->type == IT_PROJECT) WANT(W_PICK, i, 2.0f, true, it->x, it->z);
  }
  station_t *la = &S[ST_LASER];
  if (la->present) {
    if (la->state == S_LOADED) WANT(W_CUT, ST_LASER, 2.2f, true, la->x, la->z);
    else if (la->state == S_IDLE && S[ST_LUMBER].present) WANT(W_LUMBER, ST_LUMBER, 1.6f, true, S[ST_LUMBER].x, S[ST_LUMBER].z);
    else if (la->state == S_BROKEN) WANT(W_CLEAN_LASER, ST_LASER, 1.4f, false, la->x, la->z);
  }
  for (int k = ST_PRINT1; k <= ST_PRINT2; k++) {
    station_t *s = &S[k];
    if (!s->present) continue;
    if (s->state == S_IDLE) WANT(W_PRINT, k, 0.9f, true, s->x, s->z);
    else if (s->state == S_DONE) WANT(W_TAKE, k, 1.8f, true, s->x, s->z);
    else if (s->state == S_BROKEN) WANT(W_FIX, k, 1.3f, false, s->x, s->z);
  }
  if (S[ST_BENCH].present) WANT(W_BENCH, ST_BENCH, 0.8f, true, S[ST_BENCH].x, S[ST_BENCH].z);
  if (S[ST_CUTSALOT].present) WANT(W_STICKER, ST_CUTSALOT, 0.35f, true, S[ST_CUTSALOT].x, S[ST_CUTSALOT].z);
  if (S[ST_SINK].present && S[ST_SINK].state == S_BROKEN) WANT(W_DRAIN, ST_SINK, 1.2f, false, S[ST_SINK].x, S[ST_SINK].z);
  else if (S[ST_SINK].present && S[ST_SINK].count >= 2) WANT(W_DISH, ST_SINK, 0.5f + S[ST_SINK].count * 0.12f, false, S[ST_SINK].x, S[ST_SINK].z);
  if (S[ST_FRIDGE].present && S[ST_FRIDGE].smell) WANT(W_FRIDGE, ST_FRIDGE, 1.5f, false, S[ST_FRIDGE].x, S[ST_FRIDGE].z);
  station_t *vd = &S[ST_VEND], *bd = &S[ST_BOARD], *sw = &S[ST_SEWING], *ba = &S[ST_BATH];
  if (vd->present && vd->state == S_BROKEN) WANT(W_ROCK, ST_VEND, 1.0f, false, vd->x, vd->z);   /* bots never buy drinks */
  if (bd->present && bd->count > 0) WANT(W_WIPE, ST_BOARD, 0.4f + 0.15f * bd->count, false, bd->x, bd->z);
  if (bd->present && p->bot == BOT_TALKER) WANT(W_LOITER, ST_BOARD, 0.5f, false, bd->x, bd->z);   /* stands by the board and fills it */
  if (sw->present) WANT(W_SEW, ST_SEWING, 0.5f, true, sw->x, sw->z);
  if (ba->present && ba->timer >= 50) WANT(W_BATH, ST_BATH, 1.1f, false, ba->x, ba->z);
  else if (ba->present && ba->count) WANT(W_LIGHT, ST_BATH, 0.3f, false, ba->x, ba->z);
  station_t *so = &S[ST_STORAGE], *fo = &S[ST_FORGE], *li = &S[ST_LIBRARY], *bk = &S[ST_BACKDOOR], *bi = &S[ST_BIKE];
  if (so->present && so->count >= 4) WANT(W_NOTICE, ST_STORAGE, 0.6f, false, so->x, so->z);
  if (fo->present && fo->state != S_BROKEN) WANT(W_FORGE, ST_FORGE, 1.2f, true, fo->x, fo->z);
  if (li->present && li->count > 0) WANT(W_SHELVE, ST_LIBRARY, 0.3f, false, li->x, li->z);
  if (bk->present && bk->state == S_BROKEN) WANT(W_SHUT, ST_BACKDOOR, 1.4f, false, bk->x, bk->z);
  if (bi->present && bi->state == S_BROKEN) WANT(W_BIKE, ST_BIKE, 0.9f, false, bi->x, bi->z);
  for (int i = 0; i < MAX_NPCS; i++)
    if (N[i].alive && N[i].state == 1) WANT(W_GREET, i, 1.6f, false, N[i].x, N[i].z);
#undef WANT
}

static void goal_point(player_t *p, float *tx, float *tz) {
  int r = p->goal_ref;
  if (p->goal == W_PICK || p->goal == W_FLOOR) { *tx = I[r].x; *tz = I[r].z; return; }
  if (p->goal == W_GREET) { *tx = N[r].x; *tz = N[r].z; return; }
  station_t *s = &S[r];
  float m = 0.55f;
  *tx = fmaxf(s->x - s->hw - m, fminf(p->x, s->x + s->hw + m));
  *tz = fmaxf(s->z - s->hd - m, fminf(p->z, s->z + s->hd + m));
  /* then onto the station's face line, so nobody aims at a wall side */
  switch (s->face) {
    case FACE_PZ: *tz = s->z + s->hd + m; break;
    case FACE_NZ: *tz = s->z - s->hd - m; break;
    case FACE_PX: *tx = s->x + s->hw + m; break;
    case FACE_NX: *tx = s->x - s->hw - m; break;
    default: break;
  }
  if (p->goal == W_LOITER) approach_point(r, m + 1.2f, tx, tz);   /* the board's approach point, 1.2 further out */
  else if (!nav_clear(*tx, *tz)) approach_point(r, m, tx, tz);    /* the clamped point is in a neighbour (the back door beside the trash) */
}

static void bot_think(player_t *p, float dt) {
  uint32_t b = 0;
  float sx = 0, sy = 0;
  p->think -= dt;
  if (p->goal == W_NONE || !goal_valid(p) || p->think <= 0) {
    if (p->hold >= 0 && I[p->hold].type == IT_PLYWOOD && (!S[ST_LASER].present || S[ST_LASER].state != S_IDLE)) b |= IN_B;
    choose_goal(p);
    p->think = 3 + frnd() * 2;
  }
  if (p->bot == BOT_TALKER && (p->talk_in -= dt) <= 0) { b |= IN_X; p->talk_in = 6 + frnd() * 8; }
  if (p->escape > 0) {
    p->escape -= dt;
    float dx = p->esc_x - p->x, dz = p->esc_z - p->z, d = sqrtf(dx * dx + dz * dz);
    if (d > 0.2f) { sx = dx / d; sy = dz / d; } else p->escape = 0;
    p->lastx = p->x; p->lastz = p->z;
    p->btn = b; p->sx = sx; p->sy = sy;
    return;
  }
  if (p->goal != W_NONE) {
    cand_t c = find_target(p);
    float tx, tz;
    goal_point(p, &tx, &tz);
    float gx = tx - p->x, gz = tz - p->z, dg = sqrtf(gx * gx + gz * gz);
    bool at_goal = c.kind == p->goal && (c.ref == p->goal_ref || p->goal == W_DEPOSIT || p->goal == W_LOAD);
    /* arrived, but something else is at hand (a mess by the laser, junk on the
     * approach): deal with that first, or the goal never comes within reach */
    bool at_hand = !at_goal && dg < 0.15f && c.kind != W_NONE && c.kind != W_MATE && c.kind != W_SWITCH && c.kind != W_POKE
                   && c.kind != W_LUMBER;   /* plywood in hand is not dealing with what is at hand */
    if (p->goal == W_LOITER && dg < 0.3f && !at_hand) {
      p->stuck = 0; p->stuck_total = 0;   /* standing at the board on purpose: talking */
    } else if (at_goal || at_hand) {
      if (hold_time(c.kind, c.ref) > 0) b |= IN_A;
      else if (!(p->prev & IN_A)) b |= IN_A;
    } else {
      /* plan every quarter second, or whenever the goal changed or the waypoint was reached */
      p->path_t -= dt;
      float wx = tx, wz = tz;
      if (p->has_wp) { wx = p->wp_x; wz = p->wp_z; }
      float wdx = wx - p->x, wdz = wz - p->z;
      if (p->path_t <= 0 || p->wp_goal != p->goal || p->wp_ref != p->goal_ref || (p->has_wp && wdx * wdx + wdz * wdz < 0.01f)) {
        p->path_t = 0.25f; p->wp_goal = p->goal; p->wp_ref = p->goal_ref;
        p->has_wp = nav_plan(p->x, p->z, tx, tz, &p->wp_x, &p->wp_z);
        if (p->has_wp) { wx = p->wp_x; wz = p->wp_z; } else { wx = tx; wz = tz; }
      }
      float dx = wx - p->x, dz = wz - p->z, d = sqrtf(dx * dx + dz * dz);
      if (d > 0.05f) { sx = dx / d; sy = dz / d; }
      if (dg < 0.6f) { sx *= 0.5f; sy *= 0.5f; }
      float moved = fabsf(p->x - p->lastx) + fabsf(p->z - p->lastz);
      if (moved < 0.02f) p->stuck += dt; else { p->stuck = fmaxf(0, p->stuck - dt); p->stuck_total = fmaxf(0, p->stuck_total - dt * 0.5f); }
      if (p->stuck > 0.5f) { p->side = frnd() < 0.5f ? -1 : 1; p->stuck = -0.6f; p->stuck_total += 0.5f; }
      if (p->stuck_total > 3) {
        /* wedged: walk somewhere open, then think again */
        p->stuck_total = 0; p->goal = W_NONE; p->think = 1.5f;
        for (int t = 0; t < 20; t++) { p->esc_x = p->x + (frnd() - 0.5f) * 6; p->esc_z = p->z + (frnd() - 0.5f) * 6; if (clear_spot(p->esc_x, p->esc_z, 0.8f)) break; }
        p->escape = 1.5f;
      }
      if (p->stuck < 0) { float ox = sx; sx = sx * 0.3f - sy * p->side; sy = sy * 0.3f + ox * p->side; }
    }
  }
  p->lastx = p->x; p->lastz = p->z;
  p->btn = b; p->sx = sx; p->sy = sy;
}

static void player_update(player_t *p, float dt) {
  int slot = (int)(p - P);
  if (p->bot) bot_think(p, dt);
  bool pressed_a = (p->btn & IN_A) && !(p->prev & IN_A);
  bool pressed_b = (p->btn & IN_B) && !(p->prev & IN_B);
  bool pressed_x = (p->btn & IN_X) && !(p->prev & IN_X);
  p->prev = p->btn;
  if (p->flash > 0) p->flash -= dt;
  if (p->bubble > 0) p->bubble -= dt;

  cand_t c = find_target(p);
  float need = c.kind != W_NONE ? hold_time(c.kind, c.ref) : 0;
  p->work_need = need;
  bool working = need > 0 && (p->btn & IN_A) && p->work_kind == c.kind && p->work_ref == c.ref;

  if (need > 0 && (p->btn & IN_A)) {
    if (p->work_kind != c.kind || p->work_ref != c.ref) { p->work = 0; p->work_kind = c.kind; p->work_ref = c.ref; }
    working = true;
    float before = p->work;
    p->work += dt;
    if ((int)(before * 4) != (int)(p->work * 4)) {
      audio_play(c.kind == W_CUT ? SFX_LASER : SFX_TICK);
      if (c.kind == W_CUT) fx_burst(FX_SPARK, S[ST_LASER].x + (frnd() - 0.5f) * 1.6f, 1.2f, S[ST_LASER].z + (frnd() - 0.5f) * 0.8f, 0xff5a2a);
      else if (c.kind == W_DISH) fx_burst(FX_STEAM, S[ST_SINK].x, 1.2f, S[ST_SINK].z, 0xffffff);
      else if (c.kind == W_FLOOR && c.ref >= 0) fx_burst(FX_DUST, I[c.ref].x, 0.3f, I[c.ref].z, 0xd8b47a);
      else if (c.kind == W_BENCH) fx_burst(FX_SPARK, S[ST_BENCH].x + 0.8f, 1.0f, S[ST_BENCH].z, 0x8fbde6);
      else if (c.kind == W_FORGE) fx_burst(FX_SPARK, S[ST_FORGE].x - 0.1f, 0.95f, S[ST_FORGE].z + 0.2f, 0xffb36b);
    }
    if (p->work >= need) { act(p, c.kind, c.ref); p->work = 0; p->work_kind = W_NONE; }
  } else {
    p->work = 0; p->work_kind = W_NONE;
    if (pressed_a && c.kind != W_NONE) act(p, c.kind, c.ref);
  }
  if (c.kind == W_CUT && working && S[ST_LASER].state == S_LOADED) G.laser_cutting = 0.1f;
  if (c.kind == W_FORGE && working) G.welding = 0.1f;
  if (c.kind == W_SEW && working) {
    G.sewing = 0.3f;
    if (!sewing_seen && G.phase == PH_NIGHT) { sewing_seen = true; game_toast(TOAST_SEWING, C_INK_OK); }
  }

  if (c.kind != W_NONE) {
    snprintf(p->prompt, sizeof p->prompt, "%s", c.label);
    p->px = c.karma ? 1 : 0;   /* the prompt is a karma action */
  } else p->prompt[0] = 0;

  if (pressed_b && p->hold >= 0) { drop(p); audio_play(SFX_DROP); }
  if (pressed_x && p->bubble <= 0) {
    p->bubble = 2.4f; p->bubble_text = (int)(rnd() % COUNT(PROPOSALS)); p->proposals++;
    pop(p, C_SMOKE, "+0 CLOUT", 0);
    audio_play(SFX_TALK);
    /* the idea goes on the whiteboard, whoever is listening */
    if (S[ST_BOARD].present && box_dist(p->x, p->z, &S[ST_BOARD]) < 3.0f && S[ST_BOARD].count < 8) S[ST_BOARD].count++;
  }

  /* where the thing in your hands goes: drawn as an arrow, so nobody has to guess */
  p->has_goal = false;
  if (p->hold >= 0) {
    int t = I[p->hold].type, st = -1;
    const char *lab = NULL;
    if (t == IT_PROJECT) { st = ST_SHOW; lab = "TO THE SHOW WINDOW"; }
    else if (t == IT_MOVEBOX && S[ST_STORAGE].present) { st = ST_STORAGE; lab = "TO STORAGE"; }
    else if (t == IT_DONATION) { st = ST_FREEPILE; lab = "TO THE FREE SHELF"; }
    else if (is_junk(t)) { st = ST_TRASH; lab = "TO THE TRASH"; }
    else if (t == IT_PLYWOOD) {
      if (S[ST_LASER].present && S[ST_LASER].state == S_IDLE) { st = ST_LASER; lab = "TO THE LASER"; }
      else { st = ST_FREEPILE; lab = "LASER BUSY: FREE SHELF"; }
    }
    if (st >= 0 && S[st].present) { p->has_goal = true; p->goal_st = st; p->goal_x = S[st].x; p->goal_z = S[st].z; p->goal_label = lab; }
  }

  /* movement (not while hands are busy on a hold task) */
  float sx = p->sx, sy = p->sy;
  float mag = sqrtf(sx * sx + sy * sy);
  if (mag > 1) { sx /= mag; sy /= mag; mag = 1; }
  if (working) mag = 0;
  if (mag > 0.15f) {
    float speed = 5.0f * L->night.speed_mul;
    if (p->boost > 0) speed *= 1.5f;
    if (G.heat > 0) speed *= 0.7f;
    if (p->hold >= 0 && I[p->hold].heavy) speed *= 0.55f;
    p->x += sx * speed * dt;
    p->z += sy * speed * dt;
    float want = atan2f(sx, sy), d = want - p->face;
    while (d > 3.14159f) d -= 6.28318f;
    while (d < -3.14159f) d += 6.28318f;
    p->face += d * fminf(1, dt * 14);
    p->bob += dt * 13;
    p->idle = 0;
  } else {
    p->bob = 0;
    if (p->btn) p->idle = 0; else p->idle += dt;   /* holding a button is playing too */
  }
  /* the library: standing still by the books for three seconds is the one free vibe */
  if (p->read_cool > 0) p->read_cool -= dt;
  const station_t *li = &S[ST_LIBRARY];
  if (!p->bot && li->present && G.phase == PH_NIGHT && mag <= 0.15f && box_dist(p->x, p->z, li) < 1.6f) {
    p->read_t += dt;
    if (p->read_t >= 3 && p->read_cool <= 0) {
      p->read_cool = 30;
      G.vibe = fminf(100, G.vibe + 2);
      fx_burst(FX_KARMA, li->x, 1.6f, li->z, C_LIVE);
      if (!G.read_toast) { G.read_toast = true; game_toast(TOAST_READ, C_INK_OK); }
    }
  } else p->read_t = 0;
  collide(&p->x, &p->z, 0.38f);
  for (int k = 0; k < MAX_PLAYERS; k++) {
    if (k == slot || !P[k].active) continue;
    float dx = p->x - P[k].x, dz = p->z - P[k].z, d2 = dx * dx + dz * dz;
    if (d2 < 0.55f && d2 > 1e-5f) { float d = sqrtf(d2), push = (0.74f - d) * 0.5f; p->x += dx / d * push; p->z += dz / d * push; }
  }

  if (p->boost > 0) {
    p->boost -= dt;
    if (p->boost <= 0 && p->owes_bottle) {
      p->owes_bottle = false;
      spawn_item(IT_BOTTLE, p->x - sinf(p->face) * 0.5f, p->z - cosf(p->face) * 0.5f, 0, 0);
    }
  }
  if (p->hold >= 0) { I[p->hold].x = p->x; I[p->hold].z = p->z; }
}

/* ── newbies ─────────────────────────────────────────────────────── */

/* tour: one of an E_TOUR group, which ignores max_waiting up to five waiting */
static void spawn_newbie(bool tour) {
  int waiting = 0;
  for (int i = 0; i < MAX_NPCS; i++) if (N[i].alive && N[i].state <= 1) waiting++;
  if (waiting >= (tour ? 5 : L->night.max_waiting)) return;
  for (int i = 0; i < MAX_NPCS; i++) {
    if (N[i].alive) continue;
    npc_t *n = &N[i];
    memset(n, 0, sizeof *n);
    G.newbies_seen++;
    n->alive = true; n->state = 0; n->x = L->door_x; n->z = L->door_z; n->hold = -1; n->target = -1; n->bike = -1;
    const box_t *b = &L->newbie_box;
    for (int t = 0; t < 30; t++) {
      n->tx = b->x0 + frnd() * (b->x1 - b->x0); n->tz = b->z0 + frnd() * (b->z1 - b->z0);
      if (clear_spot(n->tx, n->tz, 0.8f)) break;
    }
    n->patience = L->night.patience;
    /* by bike: less patient, and the bike goes in the next free rack slot */
    if (G.phase == PH_NIGHT && L->night.bike_one_in > 0 && rnd() % (uint32_t)L->night.bike_one_in == 0) {
      for (int slot = 0; slot < 4 && n->bike < 0; slot++) {
        bool used = false;
        for (int k = 0; k < MAX_NPCS; k++) if (k != i && N[k].alive && N[k].bike == slot) used = true;
        if (!used) n->bike = slot;
      }
      if (n->bike >= 0) {
        n->patience = L->night.patience * 0.7f;
        if (!G.bike_toast) { G.bike_toast = true; game_toast(TOAST_BIKE_IN, C_BONE); }
      }
    }
    /* and one in four rolled in with a flat */
    if (G.phase == PH_NIGHT && S[ST_BIKE].present && rnd() % 4 == 0) S[ST_BIKE].state = S_BROKEN;
    if (S[ST_BATH].present) { S[ST_BATH].timer = fminf(100, S[ST_BATH].timer + 8); S[ST_BATH].count = 1; }   /* every newbie checks it first */
    n->obj = world_person(0x6f685c, true, &n->body);
    game_toast("A NEWBIE WALKED IN. SOMEBODY SAY HI.", C_BONE);
    return;
  }
}

static bool walk(npc_t *n, float dt, float speed) {
  float dx = n->tx - n->x, dz = n->tz - n->z, d = sqrtf(dx * dx + dz * dz);
  if (d < 0.2f) return true;
  /* replan every third of a second, when the target moved, or when the waypoint is reached */
  float mx = n->wp_x - n->x, mz = n->wp_z - n->z;
  if ((n->path_t -= dt) <= 0 || n->wp_tx != n->tx || n->wp_tz != n->tz || mx * mx + mz * mz < 0.01f) {
    n->path_t = 0.3f; n->wp_tx = n->tx; n->wp_tz = n->tz;
    if (!nav_plan(n->x, n->z, n->tx, n->tz, &n->wp_x, &n->wp_z)) { n->wp_x = n->tx; n->wp_z = n->tz; }
    mx = n->wp_x - n->x; mz = n->wp_z - n->z;
  }
  float md = sqrtf(mx * mx + mz * mz);
  if (md > 1e-3f) { float st = fminf(md, speed * dt); n->x += mx / md * st; n->z += mz / md * st; }
  n->bob += dt * 11;
  collide(&n->x, &n->z, 0.36f);
  return false;
}

static void npc_update(npc_t *n, float dt) {
  int idx = (int)(n - N);
  switch (n->state) {
    case 0:
      n->work += dt;   /* walking in; if something is in the way, stop and wait where you are */
      if (walk(n, dt, 2.4f) || n->work > 6) { n->state = 1; n->work = 0; }
      break;
    case 1:
      if (G.phase == PH_NIGHT) {
        float drain = 1;
        if (G.sewing > 0) drain *= 0.5f;                                       /* somebody sewing: the room looks friendlier */
        if (S[ST_BATH].present && S[ST_BATH].timer >= 70) drain *= 1.4f;     /* they checked the bathroom */
        n->patience -= dt * drain;
      }
      if (n->patience <= 0) {
        n->state = 4; n->tx = L->door_x; n->tz = L->door_z + 0.5f;
        G.vibe += L->night.leave_vibe;
        game_toast(PICK(NEWBIE_LEAVES), C_INK_ERR);
        audio_play(SFX_BAD);
      }
      break;
    case 2: {
      n->life -= dt;
      if (n->hold >= 0) {
        item_t *it = &I[n->hold];
        int dest = it->type == IT_DONATION && S[ST_FREEPILE].present ? ST_FREEPILE
                 : it->type == IT_MOVEBOX && S[ST_STORAGE].present ? ST_STORAGE
                 : S[ST_TRASH].present ? ST_TRASH : S[ST_FREEPILE].present ? ST_FREEPILE : -1;
        if (dest < 0) { it->holder = -1; it->x = n->x; it->z = n->z; it->age = 0; n->hold = -1; break; }   /* nowhere to take it: set it down */
        approach_point(dest, 0.9f, &n->tx, &n->tz);
        it->x = n->x; it->z = n->z;
        if (walk(n, dt, 2.6f) || box_dist(n->x, n->z, &S[dest]) < REACH) { kill_item(n->hold); n->hold = -1; G.vibe = fminf(100, G.vibe + 1); }
        break;
      }
      if (n->life <= 0) {
        n->state = 3; n->tx = L->door_x; n->tz = L->door_z + 0.5f;
        if (rnd() % 3 == 0) game_toast("A MEMBER DRIFTED AWAY. THE FOUR YEAR SINE WAVE.", C_ASH);
        break;
      }
      if (n->target < 0 || !I[n->target].alive || I[n->target].holder >= 0) {
        n->target = -1;
        float best = 1e9f;
        for (int i = 0; i < MAX_ITEMS; i++) {
          item_t *it = &I[i];
          if (!it->alive || it->holder >= 0 || !(is_mess(it->type) || is_junk(it->type))) continue;
          bool taken = false;
          for (int k = 0; k < MAX_NPCS; k++) if (k != idx && N[k].alive && N[k].target == i) taken = true;
          if (taken) continue;
          float dx = it->x - n->x, dz = it->z - n->z, d = dx * dx + dz * dz;
          if (d < best) { best = d; n->target = i; }
        }
        n->work = 0;
      }
      if (n->target >= 0) {
        item_t *it = &I[n->target];
        n->tx = it->x; n->tz = it->z;
        float dx = it->x - n->x, dz = it->z - n->z;
        if (dx * dx + dz * dz < 1.0f || walk(n, dt, 2.6f)) {
          if (is_mess(it->type)) {
            n->work += dt;
            if (n->work > 3.5f) { kill_item(n->target); n->target = -1; G.vibe = fminf(100, G.vibe + 1); }
          } else {
            it->holder = 100 + idx;   /* held by a newbie */
            n->hold = n->target; n->target = -1;
          }
        }
      } else {
        if (walk(n, dt, 1.6f) || (n->work += dt) > 5) {
          n->work = 0;
          spot_in(&L->wander_box, 0.7f, &n->tx, &n->tz);   /* the last try stands even when it is not clear, as before */
        }
      }
      break;
    }
    case 3: case 4:
      if (walk(n, dt, n->state == 4 ? 3.2f : 2.4f)) { world_free(n->obj); n->obj = NULL; n->alive = false; }
      break;
  }
}

/* ── events ──────────────────────────────────────────────────────── */

/* Effects, in the row order of the design doc's 25 row table (FINAL-space.md
 * 5.15). Rows 13 onward arrive with their stations; until a row exists here
 * its weight in the night script is ignored. */
enum { E_NONE, E_HEAT, E_HABOOB, E_TACOS, E_LANDLORD, E_DAISY, E_DONATION, E_STRANGE, E_FRIDGE, E_SPAGHETTI, E_COFFEE,
       E_THREAD, E_LABELS, E_PURGE, E_NOSTALGIA, E_BATH, E_FLAT, E_BIN, E_TOUR, E_DRAIN, E_DOOR, E_VEND };
/* the effect of each row the game has; the banner and sub are EVENT_TEXT[row] in content.h */
static const int EVENTS[] = {
  E_NONE, E_NONE, E_NONE, E_HEAT, E_HABOOB, E_TACOS, E_LANDLORD, E_DAISY, E_DONATION, E_STRANGE,
  E_FRIDGE, E_SPAGHETTI, E_COFFEE, E_THREAD, E_LABELS, E_PURGE, E_NOSTALGIA, E_BATH, E_FLAT, E_BIN,
  E_TOUR, E_DRAIN, E_DOOR, E_VEND, E_NONE,
};
/* the effects the game can play so far; a row whose effect is not here yet,
 * or whose station the level lacks, weighs nothing */
static bool event_ready(int row) {
  switch (EVENTS[row]) {
    case E_PURGE: return S[ST_FREEPILE].present;
    case E_FLAT: return S[ST_BIKE].present;
    case E_BIN: return S[ST_STORAGE].present;
    case E_DRAIN: return S[ST_SINK].present;
    case E_DOOR: return S[ST_BACKDOOR].present;
    case E_BATH: return S[ST_BATH].present;
    case E_VEND: return S[ST_VEND].present;
    default: return true;
  }
}

static void spawn_donation(void) {
  int d = (int)(rnd() % COUNT(DONATIONS));
  const box_t *b = &L->doormat_box;   /* left on the door mat */
  spawn_item(IT_DONATION, b->x0 + frnd() * (b->x1 - b->x0), b->z0 + frnd() * (b->z1 - b->z0), 0, d);
  char msg[96];
  snprintf(msg, sizeof msg, "DONATED: %s", DONATIONS[d].name);
  game_toast(msg, C_ASH);
}

/* a clear spot on the open floor; false when there is none, and the caller skips */
static bool floor_spot(float *x, float *z) { return spot_in(&L->spot_box, 0.6f, x, z); }

/* a donation one person can carry */
static int light_donation(void) {
  for (int t = 0; t < 20; t++) { int d = (int)(rnd() % COUNT(DONATIONS)); if (!DONATIONS[d].heavy) return d; }
  return 0;
}

/* member storage gives up: a scrap and a donation fall out of the +x face */
static void storage_spill(void) {
  station_t *s = &S[ST_STORAGE];
  for (int k = 0; k < 2; k++) {
    float x = s->x + s->hw + 0.5f, z = s->z;
    for (int t = 0; t < 12; t++) {
      x = s->x + s->hw + 0.3f + frnd() * 1.2f; z = s->z + (frnd() - 0.5f) * 2.4f;
      if (clear_spot(x, z, 0.3f)) break;
    }
    if (k == 0) spawn_item(IT_SCRAP, x, z, 0, 0); else spawn_item(IT_DONATION, x, z, 0, light_donation());
  }
  s->count = 3; s->timer = 4;
  game_toast(TOAST_BIN, C_INK_ERR);
  audio_play(SFX_DROP);
}

/* the events that hurt; CALM and CHAOS scale their weights by G.badmul */
static bool bad_event(int eff) {
  return eff == E_HEAT || eff == E_HABOOB || eff == E_DAISY || eff == E_DONATION ||
         eff == E_STRANGE || eff == E_FRIDGE || eff == E_SPAGHETTI || eff == E_LANDLORD || eff == E_BATH || eff == E_VEND ||
         eff == E_THREAD || eff == E_PURGE || eff == E_NOSTALGIA || eff == E_FLAT || eff == E_BIN || eff == E_DRAIN || eff == E_DOOR;
}

static int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

/* a weighted pick over the rows the game has, from the night's weight column.
 * Weights are scaled to integers by tenths of badmul and the tenths divided
 * back out where they can be, so at badmul 1 the pick is the plain integer pick. */
static int pick_event(void) {
  int n = COUNT(EVENTS) < EVENT_ROWS ? COUNT(EVENTS) : EVENT_ROWS;
  int w[EVENT_ROWS], total = 0, g = 0;
  int bad10 = (int)(G.badmul * 10 + 0.5f);
  for (int i = 0; i < n; i++) {
    w[i] = event_ready(i) ? L->night.weight[i] * (bad_event(EVENTS[i]) ? bad10 : 10) : 0;
    if (w[i] < 0) w[i] = 0;
    g = gcd(g, w[i]);
  }
  if (g <= 0) return -1;
  g = gcd(g, 10);   /* undo only the tenths, so the integer pick at badmul 1 is unchanged */
  for (int i = 0; i < n; i++) { w[i] /= g; total += w[i]; }
  if (total <= 0) return -1;
  int r = (int)(rnd() % (uint32_t)total), e;
  for (e = 0; e < n; e++) { r -= w[e]; if (r < 0) break; }
  return e < n ? e : n - 1;
}

static void fire_event(void) {
  int n = COUNT(EVENTS) < EVENT_ROWS ? COUNT(EVENTS) : EVENT_ROWS;
  int e = -1;
  if (G.event < 0 && L->night.first_event_forced >= 0 && L->night.first_event_forced < n) e = L->night.first_event_forced;
  else {
    /* never the same event twice running: re-roll, so a zero weight row cannot fire by bump */
    int tries = 0;
    do { e = pick_event(); } while (e >= 0 && e == G.event && tries++ < 8);
  }
  if (e < 0) return;
  G.event = e;
  /* the banner names the printer and the sub names the landlord's limit through one %s or %d */
  char text[96], sub[96];
  if (EVENTS[e] == E_STRANGE) snprintf(text, sizeof text, EVENT_TEXT[e].text, S[ST_PRINT2].label);
  else snprintf(text, sizeof text, "%s", EVENT_TEXT[e].text);
  if (EVENTS[e] == E_LANDLORD) snprintf(sub, sizeof sub, EVENT_TEXT[e].sub, L->night.landlord_limit);
  else snprintf(sub, sizeof sub, "%s", EVENT_TEXT[e].sub);
  ticker_sub(text, sub, C_HAZARD);
  audio_play(SFX_ALARM);
  float x, z;
  switch (EVENTS[e]) {
    case E_HEAT:
      G.heat = 12;
      if (spot_in(&L->puddle_box, 0.6f, &x, &z)) spawn_item(IT_PUDDLE, x, z, 0, 0);   /* the AC drips by the sink */
      if (floor_spot(&x, &z)) spawn_item(IT_PUDDLE, x, z, 0, 0);
      break;
    case E_HABOOB:
      if (S[ST_LASER].present) {
        S[ST_LASER].dust += L->night.haboob_dust;
        if (S[ST_LASER].dust >= 100 && S[ST_LASER].state == S_IDLE) S[ST_LASER].state = S_BROKEN;
      }
      for (int i = 0; i < 3; i++) if (floor_spot(&x, &z)) { spawn_item(IT_SAWDUST, x, z, 0, 0); fx_burst(FX_DUST, x, 0.5f, z, 0xd8b47a); }
      if (S[ST_BACKDOOR].present) S[ST_BACKDOOR].state = S_BROKEN;   /* and the back door blows open */
      break;
    case E_TACOS:
      G.vibe = fminf(100, G.vibe + 10);
      if (S[ST_SINK].present) S[ST_SINK].count = S[ST_SINK].count + 5 > 10 ? 10 : S[ST_SINK].count + 5;
      break;
    case E_LANDLORD: G.landlord = 10; break;
    case E_DAISY: if (floor_spot(&x, &z)) spawn_item(IT_DAISY, x, z, 0, 0); break;
    case E_DONATION: spawn_donation(); spawn_donation(); break;
    case E_STRANGE:
      if (S[ST_PRINT2].present) {
        S[ST_PRINT2].state = S_BROKEN;
        spawn_item(IT_SPAGHETTI, S[ST_PRINT2].x, S[ST_PRINT2].z + 1.0f, 0, 0);
      }
      break;
    case E_FRIDGE: if (S[ST_FRIDGE].present) S[ST_FRIDGE].smell = true; break;
    case E_SPAGHETTI: {
      int k = ST_PRINT1 + (int)(rnd() % 2);
      if (!S[k].present) k = k == ST_PRINT1 ? ST_PRINT2 : ST_PRINT1;
      if (S[k].present) spawn_item(IT_SPAGHETTI, S[k].x + (frnd() - 0.5f), S[k].z + 1.0f, 0, 0);
      break;
    }
    case E_COFFEE: G.vibe = fminf(100, G.vibe + 6); break;
    case E_BATH:
      if (S[ST_BATH].present) { S[ST_BATH].timer = fmaxf(S[ST_BATH].timer, 70); S[ST_BATH].count = 1; }
      break;
    case E_VEND: if (S[ST_VEND].present) S[ST_VEND].state = S_BROKEN; break;
    case E_THREAD: {
      /* the mailing list stings less when two of you are standing together */
      bool together = false;
      for (int a = 0; a < MAX_PLAYERS; a++) for (int b = a + 1; b < MAX_PLAYERS; b++) {
        if (!P[a].active || !P[b].active || P[a].bot || P[b].bot) continue;
        float dx = P[a].x - P[b].x, dz = P[a].z - P[b].z;
        if (dx * dx + dz * dz < 4) together = true;
      }
      G.vibe -= together ? 4 : 8;
      break;
    }
    case E_LABELS: G.labels_t = 20; break;
    case E_PURGE: G.purge_t = 20; break;
    case E_NOSTALGIA: G.vibe -= 4; break;
    case E_FLAT: if (S[ST_BIKE].present) S[ST_BIKE].state = S_BROKEN; break;
    case E_BIN: if (S[ST_STORAGE].present) storage_spill(); break;
    case E_TOUR: G.tour_pending = 3; tour_t = 0; break;
    case E_DRAIN: if (S[ST_SINK].present) S[ST_SINK].state = S_BROKEN; break;
    case E_DOOR: if (S[ST_BACKDOOR].present) S[ST_BACKDOOR].state = S_BROKEN; break;
    default: break;
  }
}

/* ── the night ───────────────────────────────────────────────────── */

static void clear_world(void) {
  for (int i = 0; i < MAX_ITEMS; i++) if (I[i].alive) kill_item(i);
  for (int i = 0; i < MAX_NPCS; i++) if (N[i].alive) { world_free(N[i].obj); N[i].obj = NULL; N[i].alive = false; }
  for (int i = 0; i < ST_COUNT; i++) { S[i].state = S_IDLE; S[i].timer = 0; S[i].dust = 0; S[i].count = 0; S[i].smell = false; }
  for (int i = 0; i < MAX_PLAYERS; i++) P[i].hold = -1;
}

void game_init(void) {
  levels_init();
  game_set_level(0);
  for (int i = 0; i < MAX_NPCS; i++) { N[i].target = N[i].hold = -1; N[i].bike = -1; }
  G.phase = PH_TITLE;
  G.vibe = L->night.vibe_start;
  G.event = -1;
  if (G.night_len <= 0) G.night_len = L->night.len;
}

void game_reset_world(void) { clear_world(); }

bool game_lights_dark(void) { return L->light.sun_i < 1.0f; }   /* only the DARK preset runs the sun under 1 */

static void accept_reset(void);
static void accept_tick(float dt);

void game_start_night(void) {
  const night_t *nt = &L->night;
  clear_world();
  if (G.attract) default_settings(); else game_apply_settings();
  /* playing alone is lonely, and the dilemma needs a commons: bring members */
  if (!G.attract && human_count() == 1 && player_count() == 1 && G.want_bots) {
    int a, b;
    game_mode_bots(&a, &b);
    game_add_bot(a); game_add_bot(b);
  }
  G.phase = PH_NIGHT; G.end = END_NONE; G.t = 0;
  G.clutter = 0; G.vibe = nt->vibe_start; G.fire_warn = 0; G.heat = 0; G.landlord = 0;
  G.event = -1;
  G.next_event = nt->first_event * G.pace; G.next_newbie = nt->first_newbie * G.pace;
  G.next_dish = 8 * G.pace; G.next_donation = nt->first_donation * G.pace;
  G.coach_st = -1; G.team_karma = 0; G.newbies_seen = G.newbies_greeted = 0; G.end_variant = 0;
  G.nights++;
  S[ST_SINK].count = nt->start_dishes;
  S[ST_BOARD].count = 0; S[ST_BOARD].dust = 1;   /* the DO NOT ERASE corner, intact */
  /* the bathroom: CLASSIC and HABOOB start at 20 plus up to 30 more; the other scripts give a fixed value */
  S[ST_BATH].timer = nt->start_bath + (nt->start_bath == 20 ? frnd() * 30 : 0);
  S[ST_BATH].count = S[ST_BATH].timer > 0;
  G.sewing = 0; sewing_seen = false;
  G.door_dust = 0; G.labels_t = 0; G.purge_t = 0; G.lights_t = 0; G.lights_on = false; G.sleeper_cool = 0;
  G.next_storage = nt->storage_gap * G.pace; G.next_book = nt->book_gap * G.pace; G.next_box = nt->box_gap * G.pace;
  G.tour_pending = 0; tour_t = 0;
  G.bike_toast = G.forge_toast = G.read_toast = false;
  S[ST_STORAGE].count = 2;   /* the bins are never empty */
  if (nt->door_open_at_start) S[ST_BACKDOOR].state = S_BROKEN;
  G.gust = nt->gust_gap + frnd() * nt->gust_jit;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    p->clout = p->karma = p->proposals = p->greeted = p->mates = p->tasks = 0;
    p->sewn = p->notices = p->wiped = p->baths = p->welds = p->flats = p->shelved = p->fixes = p->unplugged = p->dishes = 0;
    p->read_t = p->read_cool = 0;
    p->ready = false;
    p->idle = 0; p->boost = 0; p->owes_bottle = false; p->hold = -1;
    p->x = L->start_x + (i % 4) * 1.2f; p->z = L->start_z + (i / 4) * 0.9f;
  }
  /* the space is never clean when you arrive */
  float x, z;
  for (int i = 0; i < nt->start_sawdust; i++) if (floor_spot(&x, &z)) spawn_item(IT_SAWDUST, x, z, 0, 0);
  for (int i = 0; i < nt->start_scrap; i++) {
    const station_t *la = &S[ST_LASER];
    if (la->present) spawn_item(IT_SCRAP, la->x + 1.0f + i * 0.7f, la->z + la->hd + 0.8f, 0, 0);   /* somebody's offcut, by the laser */
    else if (floor_spot(&x, &z)) spawn_item(IT_SCRAP, x, z, 0, 0);
  }
  for (int i = 0; i < nt->start_donations; i++) spawn_donation();
  for (int i = 0; i < nt->start_boxes; i++) {   /* MOVE NIGHT: the wood shop's boxes, in the middle of the room */
    box_t b = { -6, -3, 6, 4 };
    if (spot_in(&b, 0.6f, &x, &z)) spawn_item(IT_MOVEBOX, x, z, 0, 0);
  }
  /* CHAOS: the second printer is already down and has already made spaghetti */
  if (!G.attract && G.mode == MODE_CHAOS && S[ST_PRINT2].present) {
    S[ST_PRINT2].state = S_BROKEN;
    spawn_item(IT_SPAGHETTI, S[ST_PRINT2].x, S[ST_PRINT2].z + 1.0f, 0, 0);
  }
  ticker_sub("HACK YOUR HACKERSPACE. TUESDAY 7 PM.", "MAKE COOL STUFF FOR CLOUT. KEEP THE SPACE OPEN FOR KARMA.", C_HAZARD);
  audio_music(G.attract ? 1 : 2);
  if (!G.attract) audio_play(SFX_ALARM);
  accept_reset();
}

static void calm_meters(void) {
  G.fire_warn = 0; G.heat = 0; G.landlord = 0; G.clutter = 0; G.vibe = L->night.vibe_start; G.laser_cutting = 0;
  G.lights_on = false; G.lights_t = 0; G.labels_t = 0; G.purge_t = 0; G.tour_pending = 0;
  G.ticker_t = 0;
}

/* ── the acceptance log (FINAL-space.md 9.3 and 13.6) ────────────────
 * A bot that stays within 0.5 of one spot for a whole 10 s window, without
 * holding A on a task or loitering by the board, prints BOT_STILL slot x z; every half
 * second a bot within 0.9 of a station's footprint counts as a visit,
 * and end_night prints the visits and a NOVISIT line for each station with a
 * face that no bot reached. tools/accept.sh reads these lines. */
static float still_x[MAX_PLAYERS], still_z[MAX_PLAYERS], still_far[MAX_PLAYERS], still_t, visit_t;
static uint16_t visits[ST_COUNT];

static void accept_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void accept_log(const char *fmt, ...) {
  char b[256];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  wc_log(b, (unsigned)strlen(b));
}

static void accept_reset(void) {
  memset(visits, 0, sizeof visits); still_t = visit_t = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) { still_x[i] = P[i].x; still_z[i] = P[i].z; still_far[i] = 0; }
}

static void accept_tick(float dt) {
  if ((visit_t += dt) >= 0.5f) {
    visit_t = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (!P[i].active || !P[i].bot) continue;
      for (int k = 0; k < ST_COUNT; k++) {
        if (!S[k].present || S[k].face == FACE_NONE || k == ST_SWITCH) continue;
        if (box_dist(P[i].x, P[i].z, &S[k]) < 0.9f && visits[k] < 65535) visits[k]++;
      }
    }
  }
  /* the farthest each bot got from where the window began: a bot whose loop
   * brings it back to the same spot ten seconds later is not wedged */
  for (int i = 0; i < MAX_PLAYERS; i++) {
    float d = hypotf(P[i].x - still_x[i], P[i].z - still_z[i]);
    if (d > still_far[i]) still_far[i] = d;
    if (P[i].work_kind != W_NONE || P[i].goal == W_LOITER) still_far[i] = 99;   /* holding A or loitering on purpose */
  }
  if ((still_t += dt) < 10) return;
  still_t = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (p->active && p->bot && still_far[i] < 0.5f)
      accept_log("BOT_STILL %d %.2f %.2f goal %d ref %d t %.0f", i, p->x, p->z, p->goal, p->goal_ref, G.t);
    still_x[i] = p->x; still_z[i] = p->z; still_far[i] = 0;
  }
}

static void accept_visits(void) {
  bool bots = false;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && P[i].bot) bots = true;
  if (!bots) return;   /* a night of humans only says nothing about the bots */
  char b[200]; int n = snprintf(b, sizeof b, "VISITS %s", L->name);
  for (int k = 0; k < ST_COUNT; k++) {
    if (!S[k].present || S[k].face == FACE_NONE || k == ST_SWITCH) continue;
    if (!visits[k]) accept_log("NOVISIT %s", S[k].label);
    if (n < (int)sizeof b - 8) n += snprintf(b + n, sizeof b - n, " %d", visits[k]);
  }
  accept_log("%s", b);
}

static void end_night(int why) {
  accept_visits();
  if (G.attract) {
    calm_meters();
    game_remove_bots(); clear_world();
    G.attract = false; G.phase = PH_TITLE; G.t = 0;
    return;
  }
  G.end = why; G.phase = PH_RESULTS; G.t = 0;
  G.end_variant = END_VARIANT_COUNT[why & 3] ? (int)(rnd() % (uint32_t)END_VARIANT_COUNT[why & 3]) : 0;
  float c = G.clutter, v = G.vibe;
  calm_meters();
  G.clutter = c; G.vibe = v;   /* the results screen may want them; the warnings and heat stop now */
  audio_music(1);
  audio_play(why == END_SURVIVED ? SFX_WIN : SFX_LOSE);
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active) drop(&P[i]);
}

void game_update(float dt) {
  G.t += dt;
  for (int i = 0; i < TOASTS; i++) if (toast_t[i] > 0) toast_t[i] -= dt;
  if (G.ticker_t > 0) G.ticker_t -= dt;
  if (G.shake > 0) G.shake -= dt;

  if (G.phase == PH_RESULTS) {
    for (int i = 0; i < MAX_PLAYERS; i++) P[i].prev = P[i].btn;
    return;
  }

  bool night = G.phase == PH_NIGHT;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active) player_update(&P[i], dt);
  for (int i = 0; i < MAX_NPCS; i++) if (N[i].alive) npc_update(&N[i], dt);

  /* stations tick */
  for (int k = ST_PRINT1; k <= ST_PRINT2; k++) {
    station_t *s = &S[k];
    if (s->state != S_BUSY) continue;
    s->timer -= dt;
    if (s->count && s->timer < 7) {
      s->state = S_BROKEN; s->count = 0;
      spawn_item(IT_SPAGHETTI, s->x + (frnd() - 0.5f), s->z + 1.0f, 0, 0);
      char msg[96];
      snprintf(msg, sizeof msg, "%s JAMMED. SPAGHETTI EVERYWHERE.", s->label);
      game_toast(msg, C_INK_ERR);
      audio_play(SFX_BAD);
    } else if (s->timer <= 0) {
      s->state = S_DONE;
      audio_play(SFX_TICK);
    }
  }
  if (G.laser_cutting > 0) G.laser_cutting -= dt;
  if (G.sewing > 0) G.sewing -= dt;
  if (G.welding > 0) G.welding -= dt;
  if (S[ST_SHOW].timer > 0) S[ST_SHOW].timer -= dt;

  for (int i = 0; i < MAX_ITEMS; i++) if (I[i].alive && I[i].holder < 0) I[i].age += dt;

  if (G.phase == PH_TITLE) {
    /* attract: the odd newbie wanders in and leaves */
    G.next_newbie -= dt;
    if (G.next_newbie <= 0) { spawn_newbie(false); G.next_newbie = 9; }
    return;
  }
  if (!night) return;

  /* spawns: the night gets busier as it goes, and with more people in the room */
  const night_t *nt = &L->night;
  float rush = (1 + nt->rush_time * G.t / G.night_len) * (1 + nt->rush_players * (player_count() - 1));
  dt *= rush;
  if (G.heat > 0) G.heat -= dt / rush;
  if ((G.next_event -= dt) <= 0) { fire_event(); G.next_event = (nt->event_gap + frnd() * nt->event_jit) * G.pace; }
  if ((G.next_newbie -= dt) <= 0) {
    if (nt->newbie_until <= 0 || G.t < nt->newbie_until) spawn_newbie(false);
    G.next_newbie = (nt->newbie_gap + frnd() * nt->newbie_jit) * G.pace;
  }
  if ((G.next_dish -= dt) <= 0) { if (S[ST_SINK].present && S[ST_SINK].count < 10) S[ST_SINK].count++; G.next_dish = nt->dish_gap * G.pace; }
  if (nt->donation_gap > 0 && (G.next_donation -= dt) <= 0) { spawn_donation();   /* a gap of 0: no donations tonight */
    G.next_donation = (nt->donation_gap + frnd() * nt->donation_jit) * G.pace; }
  station_t *so = &S[ST_STORAGE], *li = &S[ST_LIBRARY];
  if (so->present && nt->storage_gap > 0 && (G.next_storage -= dt) <= 0) {   /* one more box; a full shelf gives up */
    if (so->count >= 6) storage_spill(); else so->count++;
    G.next_storage = nt->storage_gap * G.pace;
  }
  if (li->present && nt->book_gap > 0 && (G.next_book -= dt) <= 0) { if (li->count < 4) li->count++; G.next_book = nt->book_gap * G.pace; }
  if (nt->box_gap > 0 && (G.next_box -= dt) <= 0) {
    box_t b = { -6, -3, 6, 4 };
    float x, z;
    if (spot_in(&b, 0.6f, &x, &z)) spawn_item(IT_MOVEBOX, x, z, 0, 0);
    G.next_box = nt->box_gap * G.pace;
  }

  dt /= rush;
  accept_tick(dt);
  if (S[ST_BATH].present) S[ST_BATH].timer = fminf(100, S[ST_BATH].timer + 0.4f * dt);   /* dirtiness, on real time */
  if (S[ST_STORAGE].timer > 0) S[ST_STORAGE].timer -= dt;   /* the tilted box lies there a while */
  /* the forge cools on real time; too hot is a forced break */
  station_t *fo = &S[ST_FORGE];
  if (fo->present) {
    fo->dust = fmaxf(0, fo->dust - 4 * dt);
    if (fo->dust >= 100) fo->state = S_BROKEN;
    else if (fo->state == S_BROKEN && fo->dust < 60) fo->state = S_IDLE;
  }
  /* the back door: open, the desert comes in */
  station_t *bk = &S[ST_BACKDOOR];
  if (bk->present && bk->state == S_BROKEN) {
    if ((bk->dust += dt) >= 6) {
      bk->dust = 0;
      int near = 0;
      for (int i = 0; i < MAX_ITEMS; i++) {
        if (!I[i].alive || I[i].type != IT_SAWDUST || I[i].holder >= 0) continue;
        float dx = I[i].x - bk->x, dz = I[i].z - bk->z;
        if (dx * dx + dz * dz < 2.6f * 2.6f) near++;
      }
      box_t b = { bk->x - 2.6f, bk->z + 0.7f, bk->x - 0.6f, bk->z + 2.0f };
      float x, z;
      if (near < 4 && spot_in(&b, 0.3f, &x, &z)) { spawn_item(IT_SAWDUST, x, z, 0, 0); fx_burst(FX_DUST, x, 0.4f, z, 0xd8b47a); }
    }
    if ((G.door_dust += dt) >= 10) {
      G.door_dust -= 10;
      station_t *la = &S[ST_LASER];
      if (la->present) { la->dust = fminf(100, la->dust + 3); if (la->dust >= 100 && la->state == S_IDLE) la->state = S_BROKEN; }
    }
    fx_emit(bk->x - 0.1f, 1.0f, bk->z + 0.3f, 0.8f, 0xd8b47a, 2);
  } else if (bk->present && nt->gust_gap > 0 && (G.gust -= dt) <= 0) {   /* HABOOB NIGHT: it will not stay shut */
    bk->state = S_BROKEN;
    G.gust = nt->gust_gap + frnd() * nt->gust_jit;
    game_toast(TOAST_DOOR_GUST, C_INK_ERR);
  }
  if (G.labels_t > 0) G.labels_t -= dt;
  if (G.purge_t > 0 && (G.purge_t -= dt) <= 0) {   /* the purge lands: whatever is by the free shelf is gone */
    for (int i = 0; i < MAX_ITEMS; i++)
      if (I[i].alive && I[i].holder < 0 && box_dist(I[i].x, I[i].z, &S[ST_FREEPILE]) < 1.5f) kill_item(i);
    G.vibe -= 3;
    game_toast(TOAST_PURGED, C_INK_ERR);
  }
  if (G.lights_on && (G.lights_t -= dt) <= 0) {
    G.lights_on = false;
    if (game_lights_dark()) game_toast(TOAST_LIGHTS_OFF, C_ASH);
  }
  if (G.sleeper_cool > 0) G.sleeper_cool -= dt;
  if (G.tour_pending > 0 && (tour_t -= dt) <= 0) { spawn_newbie(true); G.tour_pending--; tour_t = 1.2f; }
  if (G.landlord > 0) {
    G.landlord -= dt;
    if (G.landlord <= 0) {
      if (G.clutter > nt->landlord_limit) { G.vibe -= 15; ticker_sub("THE LANDLORD SAW THE FREE PILE.", "VIBE -15.", C_RUST); audio_play(SFX_BAD); }
      else { G.vibe = fminf(100, G.vibe + 10); ticker_sub("THE LANDLORD WAS IMPRESSED.", "THIS HAS NEVER HAPPENED. VIBE +10.", C_LIVE); audio_play(SFX_WIN); }
    }
  }

  /* meters: only what is in the room counts, and only the stations the level has */
  float c = 0;
  for (int i = 0; i < MAX_ITEMS; i++) if (I[i].alive && I[i].holder < 0) c += weight(&I[i]);
  if (S[ST_SINK].present) c += S[ST_SINK].count * 2.5f;
  if (S[ST_FRIDGE].present && S[ST_FRIDGE].smell) c += 12;
  if (S[ST_LASER].present && S[ST_LASER].state == S_BROKEN) c += 6;
  for (int k = ST_PRINT1; k <= ST_PRINT2; k++) if (S[k].present && S[k].state == S_BROKEN) c += 4;
  if (S[ST_VEND].present && S[ST_VEND].state == S_BROKEN) c += 3;
  if (S[ST_BOARD].present) c += S[ST_BOARD].count * 1.5f;
  if (S[ST_BATH].present) c += S[ST_BATH].timer / 12;
  if (S[ST_SINK].present && S[ST_SINK].state == S_BROKEN) c += 8;
  if (S[ST_STORAGE].present) c += S[ST_STORAGE].count * 1.0f;
  if (S[ST_LIBRARY].present) c += S[ST_LIBRARY].count * 1.5f;
  if (S[ST_BIKE].present && S[ST_BIKE].state == S_BROKEN) c += 3;
  G.clutter += (c - G.clutter) * fminf(1, dt * 3);
  if (G.clutter > 60) G.vibe -= dt * (G.clutter - 60) / 40.f * 2.0f;
  else if (G.clutter < 25) G.vibe = fminf(100, G.vibe + dt * nt->vibe_heal);
  G.vibe -= dt * nt->vibe_entropy;   /* entropy: vibes decay unless someone tends them */
  if (G.clutter >= 100) {
    if (G.fire_warn == 0) { ticker_sub("FIRE MARSHAL ON THE WAY!", "CLEAN SOMETHING NOW OR THE NIGHT IS OVER.", C_FAULT); audio_play(SFX_ALARM); }
    G.fire_warn += dt;
  } else G.fire_warn = fmaxf(0, G.fire_warn - dt * 0.5f);

  if (!G.attract && G.t > G.night_len - 30 && G.t - dt <= G.night_len - 30) {
    audio_music(3);
    ticker_sub("10:30 PM. CLOSING TIME.", "30 SECONDS LEFT. WHO IS DOING THE DISHES?", C_HAZARD);
  }
  if (G.fire_warn >= 5) end_night(END_FIRE);
  else if (G.vibe <= 0) { G.vibe = 0; end_night(END_BURNOUT); }
  else if (G.t >= G.night_len) end_night(END_SURVIVED);
}
