/* The night itself: players, stations, messes, newbies and events. */
#include "game.h"
#include "content.h"
#include "audio.h"
#include "hud.h"
#include "net.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

game_t G;
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

/* the floor plan: zones the way a real shop is laid out. Wood shop and laser
 * at the back left, printing and electronics at the back right, the metal
 * shop on the right wall by the alley door, the kitchen and the lounge at the
 * front left, Show & Tell in the front window, the Free Pile by the door out */
static const struct { const char *label; float x, z, hw, hd; } LAYOUT[ST_COUNT] = {
  [ST_LUMBER]   = { "LUMBER", -11.0f, -4.6f, 0.55f, 1.5f },
  [ST_LASER]    = { "PEWPEW 9000", -4.6f, -5.95f, 1.4f, 0.8f },
  [ST_PRINT1]   = { "LOAFER", -0.6f, -6.35f, 0.55f, 0.45f },
  [ST_PRINT2]   = { "DR. STRANGE", 1.0f, -6.35f, 0.55f, 0.45f },
  [ST_CUTSALOT] = { "SIR CUTSALOT", 3.0f, -6.4f, 0.75f, 0.38f },
  [ST_BENCH]    = { "ELECTRONICS", 7.2f, -6.0f, 1.9f, 0.7f },
  [ST_SINK]     = { "SINK", -11.0f, 1.9f, 0.6f, 0.9f },
  [ST_FRIDGE]   = { "FRIDGE", -11.0f, 4.4f, 0.55f, 0.5f },
  [ST_TRASH]    = { "TRASH", 11.1f, 1.6f, 0.65f, 0.55f },
  [ST_FREEPILE] = { "FREE PILE", 10.4f, 5.5f, 1.2f, 0.8f },
  [ST_SHOW]     = { "SHOW & TELL", 1.8f, 6.3f, 2.2f, 0.5f },
  [ST_COUCH]    = { "LURKER COUCH", -4.6f, 3.6f, 1.6f, 0.45f },
  [ST_TABLE]    = { "WORK ISLAND", 3.4f, 0.6f, 1.6f, 0.8f },
};

const obst_t OBST[OB_COUNT] = {
  [OB_SAW]    = { -8.0f, -5.7f, 0.7f, 0.55f },    /* table saw, wood shop */
  [OB_SHELF]  = { -11.55f, -2.0f, 0.35f, 0.9f },  /* parts shelving, left wall */
  [OB_COFFEE] = { -11.15f, -0.2f, 0.5f, 0.6f },   /* coffee and coworking */
  [OB_KTABLE] = { -8.3f, 2.4f, 0.75f, 0.65f },    /* kitchen table */
  [OB_TV]     = { -4.6f, 6.25f, 0.9f, 0.2f },     /* the lounge TV, clear of the couch */
  [OB_CTABLE] = { -2.75f, 3.55f, 0.25f, 0.25f },  /* side table at the end of the couch */
  [OB_ISLAND] = { -2.0f, -1.8f, 1.3f, 0.6f },     /* assembly table */
  [OB_LATHE]  = { 11.15f, -5.2f, 0.55f, 1.1f },   /* metal shop */
  [OB_MILL]   = { 11.1f, -2.9f, 0.55f, 0.55f },
};

#define DOOR_X -8.0f
#define DOOR_Z 7.9f
#define REACH 0.85f

enum { W_NONE, W_PICK, W_DEPOSIT, W_LOAD, W_CUT, W_TAKE, W_CLEAN_LASER, W_PRINT, W_FIX, W_STICKER, W_BENCH,
       W_LUMBER, W_DISH, W_MATE, W_FRIDGE, W_FLOOR, W_GREET };

static float hold_time(int kind, int ref) {
  switch (kind) {
    case W_CUT: return 2.4f;
    case W_CLEAN_LASER: return 2.0f;
    case W_FIX: return 2.4f;
    case W_STICKER: return 1.4f;
    case W_BENCH: return 3.2f;
    case W_DISH: return 0.7f;
    case W_FRIDGE: return 2.2f;
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
           : it->kind == 2 ? BENCH_PROJECTS[it->name] : STICKERS[it->name];
    case IT_SCRAP: return "PLYWOOD SCRAP";
    case IT_BOTTLE: return "EMPTY CLUB-MATE";
    case IT_SPAGHETTI: return "PRINT SPAGHETTI";
    case IT_DONATION: return DONATIONS[it->name].name;
    case IT_SAWDUST: return "SAWDUST";
    case IT_PUDDLE: return "AC LEAK";
    case IT_DAISY: return "DAISY CHAIN";
  }
  return "";
}

static bool is_mess(int t) { return t == IT_SAWDUST || t == IT_PUDDLE || t == IT_DAISY; }
static bool is_junk(int t) { return t == IT_SCRAP || t == IT_BOTTLE || t == IT_SPAGHETTI || t == IT_DONATION; }

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
    default: w = 0;
  }
  if (it->age > 25) w *= 1.5f;   /* red tagged */
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
static void clear_world(void);

/* ── geometry ────────────────────────────────────────────────────── */

static float box_dist(float x, float z, const station_t *s) {
  float dx = fabsf(x - s->x) - s->hw, dz = fabsf(z - s->z) - s->hd;
  if (dx < 0) dx = 0;
  if (dz < 0) dz = 0;
  return sqrtf(dx * dx + dz * dz);
}

/* nowhere near a station or a piece of furniture */
static bool clear_spot(float x, float z, float margin) {
  for (int i = 0; i < ST_COUNT; i++) if (box_dist(x, z, &S[i]) < margin) return false;
  for (int i = 0; i < OB_COUNT; i++) {
    float dx = fabsf(x - OBST[i].x) - OBST[i].hw, dz = fabsf(z - OBST[i].z) - OBST[i].hd;
    if (dx < margin && dz < margin) return false;
  }
  return true;
}

static void collide(float *x, float *z, float r) {
  if (*x < -11.6f) *x = -11.6f;
  if (*x > 11.6f) *x = 11.6f;
  if (*z < -6.6f) *z = -6.6f;
  if (*z > 7.0f && fabsf(*x - DOOR_X) > 0.9f) *z = 7.0f;
  if (*z > 8.5f) *z = 8.5f;
  for (int i = 0; i < ST_COUNT + OB_COUNT; i++) {
    station_t ob;
    const station_t *s = &ob;
    if (i < ST_COUNT) s = &S[i];
    else { ob.x = OBST[i - ST_COUNT].x; ob.z = OBST[i - ST_COUNT].z; ob.hw = OBST[i - ST_COUNT].hw; ob.hd = OBST[i - ST_COUNT].hd; }
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

/* ── players ─────────────────────────────────────────────────────── */

int game_add_player(bool phone, int pad, const char *pid, const char *name) {
  if (G.attract && !adding_bot) {
    /* a human walked up to the cabinet: the demo is over */
    game_remove_bots();
    clear_world();
    G.attract = false;
    G.phase = PH_LOBBY; G.t = 0;
    G.ticker_t = 0;
    for (int i = 0; i < TOASTS; i++) toast_t[i] = 0;
    audio_music(1);
  }
  int slot = -1;
  for (int i = 0; i < MAX_PLAYERS; i++) if (!P[i].active) { slot = i; break; }
  if (slot < 0) return -1;
  player_t *p = &P[slot];
  memset(p, 0, sizeof *p);
  p->active = true; p->phone = phone; p->pad = pad; p->hold = -1; p->kb = -1;
  snprintf(p->pid, sizeof p->pid, "%s", pid ? pid : "");
  if (name && *name) snprintf(p->name, sizeof p->name, "%s", name);
  else snprintf(p->name, sizeof p->name, "PLAYER %d", slot + 1);
  p->color = PCOL[slot];
  p->x = DOOR_X + (slot % 4) * 0.8f - 1.2f; p->z = 5.6f - (slot / 4) * 0.9f;
  p->face = 3.14159f;
  p->obj = world_person(p->color, false, &p->body);
  char msg[64];
  if (!adding_bot) {
    snprintf(msg, sizeof msg, "%s JOINED%s", p->name, phone ? " FROM A PHONE" : "");
    game_toast(msg, p->color);
    audio_play(SFX_JOIN);
    fx_burst(FX_JOIN, p->x, 1.0f, p->z, p->color);
  }
  if (G.phase == PH_TITLE) { G.phase = PH_LOBBY; G.t = 0; }
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

static void consider(cand_t *best, int kind, int ref, float d, const char *label, bool karma) {
  if (d < best->d) { best->kind = kind; best->ref = ref; best->d = d; best->label = label; best->karma = karma; }
}

static cand_t find_target(player_t *p) {
  cand_t c = { W_NONE, -1, 1e9f, NULL, false };
  if (p->hold >= 0) {
    int t = I[p->hold].type;
    float d;
    if ((d = box_dist(p->x, p->z, &S[ST_TRASH])) < REACH && (is_junk(t) || t == IT_PLYWOOD))
      consider(&c, W_DEPOSIT, ST_TRASH, d, "THROW OUT", true);
    if ((d = box_dist(p->x, p->z, &S[ST_FREEPILE])) < REACH && (t == IT_DONATION || t == IT_SCRAP || t == IT_PLYWOOD))
      consider(&c, W_DEPOSIT, ST_FREEPILE, d, "FREE PILE IT", true);
    if ((d = box_dist(p->x, p->z, &S[ST_SHOW])) < REACH && t == IT_PROJECT)
      consider(&c, W_DEPOSIT, ST_SHOW, d, "SHOW IT OFF", false);
    if ((d = box_dist(p->x, p->z, &S[ST_LASER])) < REACH && t == IT_PLYWOOD && S[ST_LASER].state == S_IDLE)
      consider(&c, W_LOAD, ST_LASER, d, "LOAD THE LASER", false);
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
      case ST_SINK: if (s->count > 0) consider(&c, W_DISH, i, d, "WASH A DISH", true); break;
      case ST_FRIDGE:
        if (s->smell) consider(&c, W_FRIDGE, i, d, "DEAL WITH THE FRIDGE", true);
        else if (p->boost <= 0) consider(&c, W_MATE, i, d, "DRINK A CLUB-MATE", false);
        break;
      default: break;
    }
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
    default: return 150;
  }
}

static int make_project(player_t *p, int kind) {
  int n = kind == 0 ? COUNT(LASER_PROJECTS) : kind == 1 ? COUNT(PRINT_PROJECTS) : kind == 2 ? COUNT(BENCH_PROJECTS) : COUNT(STICKERS);
  int it = spawn_item(IT_PROJECT, p->x, p->z, kind, (int)(rnd() % n));
  if (it >= 0) { I[it].value = project_value(kind); give(p, it); }
  return it;
}

static void karma(player_t *p, int v) {
  p->karma += v; p->tasks++;
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
        snprintf(msg, sizeof msg, "%s SHOWED OFF: %s", p->name, item_name(it));
        game_toast(msg, C_HAZARD);
        audio_play(SFX_GLAMOUR);
        if (before <= 9000 && p->clout > 9000) {
          snprintf(msg, sizeof msg, "%s'S CLOUT IS OVER 9000", p->name);
          ticker(msg, C_HAZARD);
        }
      } else if (ref == ST_TRASH) {
        int v = it->type == IT_SCRAP ? 40 : it->type == IT_BOTTLE ? 30 : it->type == IT_SPAGHETTI ? 50 : it->type == IT_DONATION ? 50 : 0;
        if (v) karma(p, v); else { pop(p, C_INK_ERR, "WASTED PLYWOOD", 0); audio_play(SFX_DROP); }
      } else if (ref == ST_FREEPILE) {
        int v = it->type == IT_DONATION ? 90 : it->type == IT_SCRAP ? 40 : 20;
        karma(p, v);
        if (it->type == IT_DONATION) game_toast("SOMEONE WILL WANT THAT. SOMEDAY.", C_ASH);
      }
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
      spawn_item(IT_SCRAP, s->x + (frnd() - 0.5f) * 2.0f, s->z + s->hd + 0.6f, 0, 0);
      s->dust += 26 + frnd() * 12;
      s->state = S_IDLE;
      if (s->dust >= 100) {
        s->state = S_BROKEN;
        ticker(rnd() & 1 ? "PEWPEW 9000: EVERY FREAKING INCH IS COVERED IN DUST" : "PEWPEW 9000: HOOD NOT FULLY CLOSED", C_RUST);
        audio_play(SFX_BAD);
      }
      break;
    case W_CLEAN_LASER:
      s->dust = 0; s->state = S_IDLE;
      karma(p, 80);
      break;
    case W_PRINT:
      s->state = S_BUSY; s->timer = 14;
      s->count = frnd() < 0.3f ? 1 : 0;      /* this one is going to fail */
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
      snprintf(msg, sizeof msg, PICK(FIXED_PRINTER), s->label);
      game_toast(msg, C_INK_OK);
      break;
    case W_STICKER: make_project(p, 3); audio_play(SFX_PICKUP); break;
    case W_BENCH: make_project(p, 2); audio_play(SFX_PICKUP); break;
    case W_LUMBER:
      give(p, spawn_item(IT_PLYWOOD, p->x, p->z, 0, 0));
      if (frnd() < 0.45f) spawn_item(IT_SAWDUST, s->x + s->hw + 0.6f + frnd(), s->z + (frnd() - 0.5f) * 2.4f, 0, 0);
      audio_play(SFX_PICKUP);
      break;
    case W_DISH:
      if (s->count > 0) { s->count--; karma(p, 15); }
      break;
    case W_MATE:
      p->boost = 7; p->mates++; p->owes_bottle = true;
      pop(p, C_HAZARD, "SPEED", 0);
      audio_play(SFX_SLURP);
      break;
    case W_FRIDGE:
      s->smell = false;
      karma(p, 90);
      game_toast("THE FRIDGE IS SAFE AGAIN. NOBODY ASK WHAT IT WAS.", C_INK_OK);
      break;
    case W_FLOOR: {
      int t = I[ref].type;
      kill_item(ref);
      karma(p, t == IT_DAISY ? 120 : t == IT_PUDDLE ? 50 : 40);
      if (t == IT_DAISY) game_toast("UNPLUGGED. THE FIRE MARSHAL NODS SOMEWHERE.", C_INK_OK);
      break;
    }
    case W_GREET: {
      npc_t *n = &N[ref];
      n->state = 2; n->life = 40; n->target = -1;
      p->greeted++;
      karma(p, 100);
      G.vibe = fminf(100, G.vibe + 8);
      game_toast(PICK(NEWBIE_JOINS), C_INK_OK);
      break;
    }
  }
}


/* ── bots: the archetypes, playing as members ────────────────────── */

static const char *const BOT_NAMES[] = { "", "BUILDER", "TALKER", "FOUNDER" };

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
    case W_CUT: return p->hold < 0 && (S[ST_LASER].state == S_LOADED || S[ST_LASER].state == S_BUSY);
    case W_LUMBER: return p->hold < 0 && S[ST_LASER].state == S_IDLE;
    case W_CLEAN_LASER: return p->hold < 0 && S[ST_LASER].state == S_BROKEN;
    case W_PRINT: return p->hold < 0 && S[r].state == S_IDLE;
    case W_TAKE: return p->hold < 0 && S[r].state == S_DONE;
    case W_FIX: return p->hold < 0 && S[r].state == S_BROKEN;
    case W_DISH: return p->hold < 0 && S[ST_SINK].count > 0;
    case W_FRIDGE: return p->hold < 0 && S[ST_FRIDGE].smell;
    case W_BENCH: case W_STICKER: return p->hold < 0;
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
    if (t == IT_PROJECT) { p->goal = W_DEPOSIT; p->goal_ref = ST_SHOW; }
    else if (t == IT_DONATION) { p->goal = W_DEPOSIT; p->goal_ref = ST_FREEPILE; }
    else if (t == IT_PLYWOOD) { if (S[ST_LASER].state == S_IDLE) { p->goal = W_LOAD; p->goal_ref = ST_LASER; } }
    else { p->goal = W_DEPOSIT; p->goal_ref = ST_TRASH; }
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
  station_t *L = &S[ST_LASER];
  if (L->state == S_LOADED) WANT(W_CUT, ST_LASER, 2.2f, true, L->x, L->z);
  else if (L->state == S_IDLE) WANT(W_LUMBER, ST_LUMBER, 1.6f, true, S[ST_LUMBER].x, S[ST_LUMBER].z);
  else if (L->state == S_BROKEN) WANT(W_CLEAN_LASER, ST_LASER, 1.4f, false, L->x, L->z);
  for (int k = ST_PRINT1; k <= ST_PRINT2; k++) {
    station_t *s = &S[k];
    if (s->state == S_IDLE) WANT(W_PRINT, k, 0.9f, true, s->x, s->z);
    else if (s->state == S_DONE) WANT(W_TAKE, k, 1.8f, true, s->x, s->z);
    else if (s->state == S_BROKEN) WANT(W_FIX, k, 1.3f, false, s->x, s->z);
  }
  WANT(W_BENCH, ST_BENCH, 0.8f, true, S[ST_BENCH].x, S[ST_BENCH].z);
  WANT(W_STICKER, ST_CUTSALOT, 0.35f, true, S[ST_CUTSALOT].x, S[ST_CUTSALOT].z);
  if (S[ST_SINK].count >= 2) WANT(W_DISH, ST_SINK, 0.5f + S[ST_SINK].count * 0.12f, false, S[ST_SINK].x, S[ST_SINK].z);
  if (S[ST_FRIDGE].smell) WANT(W_FRIDGE, ST_FRIDGE, 1.5f, false, S[ST_FRIDGE].x, S[ST_FRIDGE].z);
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
  /* never aim at a wall-side face: come at it from the room */
  if (*tz < s->z - s->hd && s->z < -4) *tz = s->z + s->hd + m;
  if (*tx < s->x - s->hw && s->x < -10) *tx = s->x + s->hw + m;
  if (*tx > s->x + s->hw && s->x > 10) *tx = s->x - s->hw - m;
}

static void bot_think(player_t *p, float dt) {
  uint32_t b = 0;
  float sx = 0, sy = 0;
  p->think -= dt;
  if (p->goal == W_NONE || !goal_valid(p) || p->think <= 0) {
    if (p->hold >= 0 && I[p->hold].type == IT_PLYWOOD && S[ST_LASER].state != S_IDLE) b |= IN_B;
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
    if (c.kind == p->goal && (c.ref == p->goal_ref || p->goal == W_DEPOSIT || p->goal == W_LOAD)) {
      if (hold_time(c.kind, c.ref) > 0) b |= IN_A;
      else if (!(p->prev & IN_A)) b |= IN_A;
    } else {
      float tx, tz;
      goal_point(p, &tx, &tz);
      float dx = tx - p->x, dz = tz - p->z, d = sqrtf(dx * dx + dz * dz);
      if (d > 0.05f) { sx = dx / d; sy = dz / d; }
      if (d < 0.6f) { sx *= 0.5f; sy *= 0.5f; }
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
    }
    if (p->work >= need) { act(p, c.kind, c.ref); p->work = 0; p->work_kind = W_NONE; }
  } else {
    p->work = 0; p->work_kind = W_NONE;
    if (pressed_a && c.kind != W_NONE) act(p, c.kind, c.ref);
  }
  if (c.kind == W_CUT && working) S[ST_LASER].state = S_BUSY;

  if (c.kind != W_NONE) {
    snprintf(p->prompt, sizeof p->prompt, "%s", c.label);
    p->px = c.karma ? 1 : 0;   /* the prompt is a karma action */
  } else p->prompt[0] = 0;

  if (pressed_b && p->hold >= 0) { drop(p); audio_play(SFX_DROP); }
  if (pressed_x && p->bubble <= 0) {
    p->bubble = 2.4f; p->bubble_text = (int)(rnd() % COUNT(PROPOSALS)); p->proposals++;
    pop(p, C_SMOKE, "+0 CLOUT", 0);
    audio_play(SFX_TALK);
  }

  /* where the thing in your hands goes: drawn as an arrow, so nobody has to guess */
  p->has_goal = false;
  if (p->hold >= 0) {
    int t = I[p->hold].type, st = -1;
    const char *lab = NULL;
    if (t == IT_PROJECT) { st = ST_SHOW; lab = "TO SHOW & TELL"; }
    else if (t == IT_DONATION) { st = ST_FREEPILE; lab = "TO THE FREE PILE"; }
    else if (is_junk(t)) { st = ST_TRASH; lab = "TO THE TRASH"; }
    else if (t == IT_PLYWOOD) {
      if (S[ST_LASER].state == S_IDLE) { st = ST_LASER; lab = "TO PEWPEW 9000"; }
      else { st = ST_FREEPILE; lab = "LASER BUSY: FREE PILE"; }
    }
    if (st >= 0) { p->has_goal = true; p->goal_st = st; p->goal_x = S[st].x; p->goal_z = S[st].z; p->goal_label = lab; }
  }

  /* movement (not while hands are busy on a hold task) */
  float sx = p->sx, sy = p->sy;
  float mag = sqrtf(sx * sx + sy * sy);
  if (mag > 1) { sx /= mag; sy /= mag; mag = 1; }
  if (working) mag = 0;
  if (mag > 0.15f) {
    float speed = 5.0f;
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
    if (!p->btn) p->idle += dt;
  }
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

static void spawn_newbie(void) {
  int waiting = 0;
  for (int i = 0; i < MAX_NPCS; i++) if (N[i].alive && N[i].state <= 1) waiting++;
  if (waiting >= 3) return;
  for (int i = 0; i < MAX_NPCS; i++) {
    if (N[i].alive) continue;
    npc_t *n = &N[i];
    memset(n, 0, sizeof *n);
    n->alive = true; n->state = 0; n->x = DOOR_X; n->z = DOOR_Z; n->hold = -1; n->target = -1;
    for (int t = 0; t < 30; t++) {
      n->tx = -7 + frnd() * 12; n->tz = -0.5f + frnd() * 5.5f;
      if (clear_spot(n->tx, n->tz, 0.8f)) break;
    }
    n->patience = 20;
    n->obj = world_person(0x8b8578, true, &n->body);
    game_toast("A NEWBIE WALKED IN. SOMEBODY SAY HI.", C_BONE);
    return;
  }
}

static bool walk(npc_t *n, float dt, float speed) {
  float dx = n->tx - n->x, dz = n->tz - n->z, d = sqrtf(dx * dx + dz * dz);
  if (d < 0.2f) return true;
  float st = fminf(d, speed * dt);
  n->x += dx / d * st; n->z += dz / d * st;
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
      if (G.phase == PH_NIGHT) n->patience -= dt;
      if (n->patience <= 0) {
        n->state = 4; n->tx = DOOR_X; n->tz = DOOR_Z + 0.5f;
        G.vibe -= 12;
        game_toast(PICK(NEWBIE_LEAVES), C_INK_ERR);
        audio_play(SFX_BAD);
      }
      break;
    case 2: {
      n->life -= dt;
      if (n->hold >= 0) {
        item_t *it = &I[n->hold];
        int dest = it->type == IT_DONATION ? ST_FREEPILE : ST_TRASH;
        n->tx = S[dest].x - (dest == ST_TRASH ? 1.1f : 1.3f); n->tz = S[dest].z;
        it->x = n->x; it->z = n->z;
        if (walk(n, dt, 2.6f) || box_dist(n->x, n->z, &S[dest]) < REACH) { kill_item(n->hold); n->hold = -1; G.vibe = fminf(100, G.vibe + 1); }
        break;
      }
      if (n->life <= 0) {
        n->state = 3; n->tx = DOOR_X; n->tz = DOOR_Z + 0.5f;
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
          for (int t = 0; t < 20; t++) { n->tx = -8 + frnd() * 16; n->tz = -3 + frnd() * 8; if (clear_spot(n->tx, n->tz, 0.7f)) break; }
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

enum { E_NONE, E_HEAT, E_HABOOB, E_TACOS, E_LANDLORD, E_DAISY, E_DONATION, E_STRANGE, E_FRIDGE, E_SPAGHETTI, E_COFFEE };
static const struct { const char *text, *sub; int effect; int weight; } EVENTS[] = {
  { "LINUX NIGHT: CANCELED.", "NOTHING HAPPENS. AS USUAL.", E_NONE, 1 },
  { "QUIET NIGHT. NOTHING HAPPENS.", "ENJOY IT.", E_NONE, 1 },
  { "5TH THURSDAY?! IT'S BLASPHEMY!", "IT IS TUESDAY. NOTHING HAPPENS.", E_NONE, 1 },
  { "IT IS 118 OUTSIDE.", "EVERYONE SLOWS DOWN. THE AC LEAKS: MOP IT UP.", E_HEAT, 3 },
  { "HABOOB!", "DUST ON THE FLOOR AND IN THE LASER. SWEEP IT.", E_HABOOB, 3 },
  { "SOMEBODY BROUGHT TACOS.", "VIBE UP. SO ARE THE DISHES.", E_TACOS, 3 },
  { "LANDLORD WALKTHROUGH IN 10 SECONDS.", "GET CLUTTER UNDER 50 OR LOSE VIBE.", E_LANDLORD, 3 },
  { "POWER STRIP DAISY CHAIN DETECTED.", "UNPLUG IT. THE FIRE MARSHAL HATES THESE.", E_DAISY, 3 },
  { "DONATION AT THE FRONT DOOR.", "CARRY IT TO THE FREE PILE.", E_DONATION, 3 },
  { "ANY PARTICULAR REASON WHY DR. STRANGE IS DOWN?", "THE PRINTER JAMMED. HOLD YOUR BUTTON ON IT TO FIX.", E_STRANGE, 2 },
  { "MYSTERY FOOD IN THE FRIDGE.", "SOMEONE HAS TO DEAL WITH IT. HOLD ON THE FRIDGE.", E_FRIDGE, 3 },
  { "PRINT LEFT RUNNING.", "SPAGHETTI ON THE FLOOR. TRASH IT.", E_SPAGHETTI, 2 },
  { "COFFEE AND COWORKING RAN LONG.", "GOOD VIBES.", E_COFFEE, 2 },
};

static void spawn_donation(void) {
  int d = (int)(rnd() % COUNT(DONATIONS));
  spawn_item(IT_DONATION, DOOR_X - 0.7f + frnd() * 1.4f, 5.7f + frnd() * 0.6f, 0, d);   /* left on the door mat */
  char msg[96];
  snprintf(msg, sizeof msg, "DONATED: %s", DONATIONS[d].name);
  game_toast(msg, C_ASH);
}

static void floor_spot(float *x, float *z) {
  for (int tries = 0; tries < 20; tries++) {
    *x = -10 + frnd() * 20; *z = -4.5f + frnd() * 10;
    bool ok = true;
    if (!clear_spot(*x, *z, 0.6f)) ok = false;
    if (ok) return;
  }
}

static void fire_event(void) {
  int total = 0;
  for (int i = 0; i < COUNT(EVENTS); i++) total += EVENTS[i].weight;
  int r = (int)(rnd() % total), e = 0;
  for (e = 0; e < COUNT(EVENTS); e++) { r -= EVENTS[e].weight; if (r < 0) break; }
  if (e == G.event) e = (e + 1) % COUNT(EVENTS);
  G.event = e;
  ticker_sub(EVENTS[e].text, EVENTS[e].sub, C_HAZARD);
  audio_play(SFX_ALARM);
  float x, z;
  switch (EVENTS[e].effect) {
    case E_HEAT:
      G.heat = 12;
      spawn_item(IT_PUDDLE, -9.5f + frnd() * 2, -3 + frnd() * 3, 0, 0);
      floor_spot(&x, &z); spawn_item(IT_PUDDLE, x, z, 0, 0);
      break;
    case E_HABOOB:
      S[ST_LASER].dust += 45;
      if (S[ST_LASER].dust >= 100 && S[ST_LASER].state == S_IDLE) S[ST_LASER].state = S_BROKEN;
      for (int i = 0; i < 3; i++) { floor_spot(&x, &z); spawn_item(IT_SAWDUST, x, z, 0, 0); fx_burst(FX_DUST, x, 0.5f, z, 0xd8b47a); }
      break;
    case E_TACOS:
      G.vibe = fminf(100, G.vibe + 10);
      S[ST_SINK].count = S[ST_SINK].count + 5 > 10 ? 10 : S[ST_SINK].count + 5;
      break;
    case E_LANDLORD: G.landlord = 10; break;
    case E_DAISY: floor_spot(&x, &z); spawn_item(IT_DAISY, x, z, 0, 0); break;
    case E_DONATION: spawn_donation(); spawn_donation(); break;
    case E_STRANGE:
      S[ST_PRINT2].state = S_BROKEN;
      spawn_item(IT_SPAGHETTI, S[ST_PRINT2].x, S[ST_PRINT2].z + 1.0f, 0, 0);
      break;
    case E_FRIDGE: S[ST_FRIDGE].smell = true; break;
    case E_SPAGHETTI: {
      int k = ST_PRINT1 + (int)(rnd() % 2);
      spawn_item(IT_SPAGHETTI, S[k].x + (frnd() - 0.5f), S[k].z + 1.0f, 0, 0);
      break;
    }
    case E_COFFEE: G.vibe = fminf(100, G.vibe + 6); break;
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
  for (int i = 0; i < ST_COUNT; i++) {
    S[i].label = LAYOUT[i].label;
    S[i].x = LAYOUT[i].x; S[i].z = LAYOUT[i].z; S[i].hw = LAYOUT[i].hw; S[i].hd = LAYOUT[i].hd;
    S[i].solid = true;
  }
  for (int i = 0; i < MAX_NPCS; i++) N[i].target = N[i].hold = -1;
  G.phase = PH_TITLE;
  G.vibe = 70;
  G.event = -1;
  if (G.night_len <= 0) G.night_len = 180;
}

void game_reset_world(void) { clear_world(); }

void game_start_night(void) {
  clear_world();
  /* playing alone is lonely, and the dilemma needs a commons: bring members */
  if (!G.attract && human_count() == 1 && player_count() == 1) { game_add_bot(BOT_BUILDER); game_add_bot(BOT_TALKER); }
  G.phase = PH_NIGHT; G.end = END_NONE; G.t = 0;
  G.clutter = 0; G.vibe = 70; G.fire_warn = 0; G.heat = 0; G.landlord = 0;
  G.next_event = 14; G.next_newbie = 6; G.next_dish = 8; G.next_donation = 24;
  G.nights++;
  S[ST_SINK].count = 3;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    player_t *p = &P[i];
    if (!p->active) continue;
    p->clout = p->karma = p->proposals = p->greeted = p->mates = p->tasks = 0;
    p->idle = 0; p->boost = 0; p->owes_bottle = false; p->hold = -1;
    p->x = -1.5f + (i % 4) * 1.2f; p->z = 4.3f + (i / 4) * 0.9f;
  }
  /* the space is never clean when you arrive */
  float x, z;
  for (int i = 0; i < 2; i++) { floor_spot(&x, &z); spawn_item(IT_SAWDUST, x, z, 0, 0); }
  spawn_item(IT_SCRAP, -6.5f, -4.0f, 0, 0);
  spawn_donation();
  ticker_sub("HACK YOUR HACKERSPACE. TUESDAY 7 PM.", "MAKE COOL STUFF FOR CLOUT. KEEP THE SPACE OPEN FOR KARMA.", C_HAZARD);
  audio_music(G.attract ? 1 : 2);
  if (!G.attract) audio_play(SFX_ALARM);
}

static void end_night(int why) {
  if (G.attract) {
    game_remove_bots(); clear_world();
    G.attract = false; G.phase = PH_TITLE; G.t = 0;
    return;
  }
  G.end = why; G.phase = PH_RESULTS; G.t = 0;
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
  if (S[ST_LASER].state == S_BUSY) S[ST_LASER].state = S_LOADED;   /* re-set by whoever is cutting */
  if (S[ST_SHOW].timer > 0) S[ST_SHOW].timer -= dt;

  for (int i = 0; i < MAX_ITEMS; i++) if (I[i].alive && I[i].holder < 0) I[i].age += dt;

  if (G.phase == PH_TITLE) {
    /* attract: the odd newbie wanders in and leaves */
    G.next_newbie -= dt;
    if (G.next_newbie <= 0) { spawn_newbie(); G.next_newbie = 9; }
    return;
  }
  if (!night) return;

  /* spawns: the night gets busier as it goes, and with more people in the room */
  float rush = (1 + 0.8f * G.t / G.night_len) * (1 + 0.12f * (player_count() - 1));
  dt *= rush;
  if (G.heat > 0) G.heat -= dt / rush;
  if ((G.next_event -= dt) <= 0) { fire_event(); G.next_event = 17 + frnd() * 8; }
  if ((G.next_newbie -= dt) <= 0) { spawn_newbie(); G.next_newbie = 13 + frnd() * 7; }
  if ((G.next_dish -= dt) <= 0) { if (S[ST_SINK].count < 10) S[ST_SINK].count++; G.next_dish = 6; }
  if ((G.next_donation -= dt) <= 0) { spawn_donation(); G.next_donation = 24 + frnd() * 10; }

  dt /= rush;
  if (G.landlord > 0) {
    G.landlord -= dt;
    if (G.landlord <= 0) {
      if (G.clutter > 50) { G.vibe -= 15; ticker_sub("THE LANDLORD SAW THE FREE PILE.", "VIBE -15.", C_RUST); audio_play(SFX_BAD); }
      else { G.vibe = fminf(100, G.vibe + 10); ticker_sub("THE LANDLORD WAS IMPRESSED.", "THIS HAS NEVER HAPPENED. VIBE +10.", C_LIVE); audio_play(SFX_WIN); }
    }
  }

  /* meters */
  float c = 0;
  for (int i = 0; i < MAX_ITEMS; i++) if (I[i].alive && I[i].holder < 0) c += weight(&I[i]);
  c += S[ST_SINK].count * 2.5f;
  if (S[ST_FRIDGE].smell) c += 12;
  if (S[ST_LASER].state == S_BROKEN) c += 6;
  for (int k = ST_PRINT1; k <= ST_PRINT2; k++) if (S[k].state == S_BROKEN) c += 4;
  G.clutter += (c - G.clutter) * fminf(1, dt * 3);
  if (G.clutter > 60) G.vibe -= dt * (G.clutter - 60) / 40.f * 2.0f;
  else if (G.clutter < 25) G.vibe = fminf(100, G.vibe + dt * 0.15f);
  G.vibe -= dt * 0.12f;   /* entropy: vibes decay unless someone tends them */
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
