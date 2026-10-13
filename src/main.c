/* DO-OCRACY: the wasmcart cart. The host calls wc_get_info, wc_init, then
 * wc_render once a frame; everything else is ours. */
#include "wasmcart.h"
#include "three.h"
#include "game.h"
#include "hud.h"
#include "net.h"
#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>

#define W 1280
#define H 720
#define AUDIO_CAP 8192

static uint32_t framebuffer[4];          /* GL cart: the host presents the GL surface */
static float audio_ring[AUDIO_CAP * 2];
static uint32_t audio_cursor;
static wc_pad_t pads[4];
static wc_time_t tm;
static wc_info_t info;
static wc_host_info_t host_info;
static uint8_t keys[32];

static t3_renderer *R;
static t3_scene *scene, *hud_scene;
static t3_camera *hud_cam;
static t3_texture *hud_tex;

/* A seat is an input source that may or may not be a player yet: four pads,
 * two keyboard halves, one per phone (FINAL-frontend.md 3.1). */
typedef struct {
  bool present;        /* pad: always true; kb: a key was ever seen this boot; phone: registered */
  uint32_t btn, prev;  /* IN_A IN_B IN_X IN_START */
  float sx, sy;        /* -1..1, sy positive is down on screen (toward +z) */
  int dir, last_dir;   /* NAV_* direction currently held, for repeat */
  float repeat;        /* seconds until the next repeat event */
  int nav;             /* this frame's nav events (nav_read), for the menus */
  int slot;            /* player slot or -1 */
  char name[14];       /* "PAD 1".."PAD 4", "WASD", "ARROWS", or the phone's name */
  char pid[25];        /* phones only */
  bool pending;        /* pressed A on a menu, or a phone said J while the front end was up */
  bool denied;         /* the deny joke has fired for this seat */
  float hold_b;        /* lobby: seconds B has been held */
} seat_t;
static seat_t seats[SEAT_COUNT];
static float status_clock;

/* the save region: settings and records, persisted by the host after the frame */
#define SAVE_MAGIC 0x434f4f44u   /* 'DOOC' */
#define SAVE_VERSION 1
typedef struct {
  uint32_t magic, version, crc;   /* crc32 of everything after crc */
  settings_t set;
  uint32_t nights_played;
  int32_t best_karma;  char best_karma_name[14];
  int32_t best_clout;  char best_clout_name[14];
  int32_t best_team;               /* co-op team karma */
  uint8_t reserved[32];
} save_t;
static save_t SAVE;              /* static so the address is stable */

static uint32_t crc32(const void *p, size_t n) {
  const uint8_t *b = p;
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < n; i++) {
    c ^= b[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
  }
  return ~c;
}

#define SAVE_BODY(s) ((const uint8_t *)(s) + offsetof(save_t, set))
#define SAVE_BODY_N (sizeof(save_t) - offsetof(save_t, set))

/* recompute the crc and copy SET in; the host writes the region out after the frame */
static void save_commit(void) {
  SAVE.magic = SAVE_MAGIC; SAVE.version = SAVE_VERSION;
  SAVE.set = SET;
  SAVE.crc = crc32(SAVE_BODY(&SAVE), SAVE_BODY_N);
}

static void settings_defaults(settings_t *s) {
  memset(s, 0, sizeof *s);
  s->mode = MODE_SOLO; s->night_len = 1; s->bots = 1; s->chaos = CHAOS_NORMAL; s->phones = 1;
  s->music = 7; s->sfx = 8; s->layout = 0; s->help = HELP_FIRST;
}

/* magic, version and crc; anything else (a never saved cart is all zero) gets the defaults */
static void save_load(void) {
  bool ok = SAVE.magic == SAVE_MAGIC && SAVE.version == SAVE_VERSION && SAVE.crc == crc32(SAVE_BODY(&SAVE), SAVE_BODY_N);
  if (ok) {
    const settings_t *s = &SAVE.set;
    ok = s->mode < MODE_COUNT && s->night_len < 4 && s->bots <= 1 && s->chaos <= CHAOS_CHAOS && s->phones <= 1 &&
         s->music <= 10 && s->sfx <= 10 && s->layout < LEVEL_COUNT && s->help <= HELP_ALWAYS;
  }
  if (ok) { SET = SAVE.set; return; }
  memset(&SAVE, 0, sizeof SAVE);
  settings_defaults(&SET);
  save_commit();
}

__attribute__((export_name("wc_get_info")))
wc_info_t *wc_get_info(void) {
  info.version = WC_ABI_VERSION;
  info.width = W; info.height = H;
  info.fb_ptr = (uint32_t)(uintptr_t)framebuffer;
  info.audio_ptr = (uint32_t)(uintptr_t)audio_ring;
  info.audio_cap = AUDIO_CAP;
  info.audio_write_ptr = (uint32_t)(uintptr_t)&audio_cursor;
  info.input_ptr = (uint32_t)(uintptr_t)pads;
  info.time_ptr = (uint32_t)(uintptr_t)&tm;
  info.host_info_ptr = (uint32_t)(uintptr_t)&host_info;
  info.keys_ptr = (uint32_t)(uintptr_t)keys;
  info.save_ptr = (uint32_t)(uintptr_t)&SAVE;
  info.save_size = sizeof SAVE;
  info.flags = WC_FLAG_AUDIO_F32 | WC_FLAG_NET_PEER | WC_FLAG_KEYBOARD;
  info.gpu_api = 1;
  return &info;
}

__attribute__((export_name("wc_set_seed")))
void wc_set_seed(uint32_t s) { rnd_seed(s); }

/* ── config.json (packed as an asset; edit and repack to point phones elsewhere) ── */

static void json_str(const char *js, const char *key, char *out, int n) {
  char pat[64];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char *p = strstr(js, pat);
  if (!p) return;
  p = strchr(p + strlen(pat), ':');
  if (!p) return;
  while (*++p == ' ') {}
  if (*p == '"') {
    p++;
    int i = 0;
    while (*p && *p != '"' && i < n - 1) out[i++] = *p++;
    out[i] = 0;
  } else {
    int i = 0;
    while (*p && (isdigit((unsigned char)*p) || *p == '.') && i < n - 1) out[i++] = *p++;
    out[i] = 0;
  }
}

static void load_config(void) {
  char url[160] = "https://virgilvox.github.io/do-ocracy/join.html";
  char secs[16] = "";
  snprintf(G.relay, sizeof G.relay, "wss://relay.clasp.to");
  int n = wc_asset_size("config.json", 11);
  if (n > 0 && n < 4096) {
    char *js = calloc((size_t)n + 1, 1);
    wc_load_asset("config.json", 11, js, (unsigned)n);
    json_str(js, "controller_url", url, sizeof url);
    json_str(js, "relay", G.relay, sizeof G.relay);
    json_str(js, "night_seconds", secs, sizeof secs);
    free(js);
  }
  if (secs[0]) G.night_len = (float)atof(secs);
  if (!(G.night_len >= 20 && G.night_len <= 3600)) G.night_len = 180;
  LEVELS[0].night.len = G.night_len;   /* night_seconds scales the classic night only; other levels keep their own length */
  static const char L[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
  for (int i = 0; i < 4; i++) G.room[i] = L[rnd() % (sizeof L - 1)];
  G.room[4] = 0;
  snprintf(G.join_url, sizeof G.join_url, "%s#%s", url, G.room);
}

__attribute__((export_name("wc_init")))
void wc_init(void) {
  extern const struct t3_gen_table hsx_gl;
  t3_register_program_table(&hsx_gl, NULL);

  R = t3_renderer_new(W, H);
  t3_renderer_set_shadow_map(R, true, T3_PCF_SOFT_SHADOW_MAP);
  t3_renderer_set_exact_output(R, true);   /* the HUD pass composites over the scene */
  t3_renderer_set_clear_color(R, C_CHAR, 1);

  hud_init();
  save_load();
  game_init();
  load_config();

  scene = t3_scene_new();
  world_init(scene);

  hud_scene = t3_scene_new();
  hud_cam = t3_orthographic_camera_new(-1, 1, 1, -1, 0, 10);
  t3_object_set_position(hud_cam, 0, 0, 5);
  hud_clear();
  hud_tex = t3_data_texture_new(HUD_W, HUD_H, hud_px);
  hud_tex->color_space = T3_SRGB_COLOR_SPACE;
  free(hud_px);                 /* the HUD now draws straight into the texture's texels */
  hud_px = hud_tex->pixels;
  t3_material *hm = t3_mesh_basic_material_new(0xffffff);
  t3_material_set_map(hm, hud_tex);
  hm->transparent = true; hm->depth_test = false; hm->depth_write = false;
  t3_mesh *quad = t3_mesh_new(t3_plane_geometry_new(2, 2, 1, 1), hm);
  t3_object_add(&hud_scene->base, quad);
  t3_release(quad); t3_release(hm);

  for (int i = 0; i < SEAT_COUNT; i++) seats[i].slot = -1;
  for (int i = 0; i < SEAT_PADS; i++) {
    seats[SEAT_PAD(i)].present = true;
    snprintf(seats[SEAT_PAD(i)].name, sizeof seats[0].name, "PAD %d", i + 1);
  }
  snprintf(seats[SEAT_KB(0)].name, sizeof seats[0].name, "WASD");
  snprintf(seats[SEAT_KB(1)].name, sizeof seats[0].name, "ARROWS");
  M.owner = -1; M.screen = SCR_TITLE; M.prev_screen = SCR_TITLE;
  net_init(G.relay, G.room);
  audio_music(1);
  WC_LOG("do-ocracy: clocked in");
}

/* ── input ───────────────────────────────────────────────────────── */

#define KEY(k) ((keys[(k) >> 3] >> ((k) & 7)) & 1)
#define LETTER(c) (0x04 + ((c) - 'a'))

static uint32_t pad_buttons(const wc_pad_t *p, float *sx, float *sy) {
  uint32_t b = 0;
  if (p->buttons & WC_BTN_A) b |= IN_A;
  if (p->buttons & WC_BTN_B) b |= IN_B;
  if (p->buttons & (WC_BTN_X | WC_BTN_Y)) b |= IN_X;
  if (p->buttons & WC_BTN_START) b |= IN_START;
  float x = p->left_x / 32767.f, y = p->left_y / 32767.f;
  if (p->buttons & WC_BTN_LEFT) x = -1;
  if (p->buttons & WC_BTN_RIGHT) x = 1;
  if (p->buttons & WC_BTN_UP) y = -1;
  if (p->buttons & WC_BTN_DOWN) y = 1;
  if (fabsf(x) < 0.2f) x = 0;
  if (fabsf(y) < 0.2f) y = 0;
  *sx = x; *sy = y;
  return b;
}

/* menu events for one seat this frame: one direction at a time with repeat,
 * and A, B, START as edges (FINAL-frontend.md 3.3) */
static int nav_read(seat_t *s, float dt) {
  int nav = 0, dir = 0;
  if (fabsf(s->sy) >= 0.5f) dir = s->sy < 0 ? NAV_UP : NAV_DOWN;
  else if (fabsf(s->sx) >= 0.5f) dir = s->sx < 0 ? NAV_LEFT : NAV_RIGHT;
  if (!dir) s->dir = 0;
  else if (dir != s->dir) { s->dir = s->last_dir = dir; s->repeat = 0.28f; nav |= dir; }
  else if ((s->repeat -= dt) <= 0) { s->repeat = 0.11f; nav |= dir; }
  uint32_t edge = s->btn & ~s->prev;
  if (edge & IN_A) nav |= NAV_A;
  if (edge & IN_B) nav |= NAV_B;
  if (edge & IN_START) nav |= NAV_START;
  return nav;
}

/* fill every seat's buttons and stick: all four pads with no connected gate
 * (under RetroArch only pad 0 reports connected; an unplugged pad reads zero),
 * the two keyboard halves (browser and Node hosts), and the phones, whose
 * btn and stick game_on_phone writes */
static void seats_poll(float dt) {
  for (int i = 0; i < SEAT_PADS; i++) {
    seat_t *s = &seats[SEAT_PAD(i)];
    s->btn = pad_buttons(&pads[i], &s->sx, &s->sy);
  }
  for (int k = 0; k < SEAT_KBS; k++) {
    seat_t *s = &seats[SEAT_KB(k)];
    uint32_t b = 0;
    float x = 0, y = 0;
    if (k == 0) {
      if (KEY(0x2c) || KEY(LETTER('e'))) b |= IN_A;     /* space, E */
      if (KEY(LETTER('q'))) b |= IN_B;
      if (KEY(LETTER('r'))) b |= IN_X;
      if (KEY(0x28)) b |= IN_START;                     /* enter */
      x = (float)(KEY(LETTER('d')) - KEY(LETTER('a')));
      y = (float)(KEY(LETTER('s')) - KEY(LETTER('w')));
    } else {
      if (KEY(0x37)) b |= IN_A;                         /* . */
      if (KEY(0x36)) b |= IN_B;                         /* , */
      if (KEY(0x38)) b |= IN_X;                         /* / */
      x = (float)(KEY(0x4f) - KEY(0x50));               /* right, left */
      y = (float)(KEY(0x51) - KEY(0x52));               /* down, up */
    }
    if (x && y) { x *= 0.7071f; y *= 0.7071f; }
    s->btn = b; s->sx = x; s->sy = y;
    if (b || x || y) { s->present = true; M.kb_seen = true; }
  }
  for (int i = 0; i < SEAT_COUNT; i++) seats[i].nav = seats[i].present ? nav_read(&seats[i], dt) : 0;
}

static bool any_pressed;   /* someone pressed A or START this frame */
static bool start_pressed;

/* one pad or keyboard seat: an A or START edge on an unseated seat clocks
 * in; a seated seat drives its player */
static void seat_step(seat_t *s, int id, int pad) {
  uint32_t b = s->btn, prev = s->prev;
  bool edge_a = (b & IN_A) && !(prev & IN_A), edge_s = (b & IN_START) && !(prev & IN_START);
  if (edge_a || edge_s) any_pressed = true;
  if (s->slot >= 0 && !P[s->slot].active) s->slot = -1;
  if (s->slot >= 0 && edge_s) start_pressed = true;   /* the press that clocks you in does not also start the night */
  if (s->slot < 0) {
    if ((edge_a || edge_s) && G.phase != PH_RESULTS) {
      s->slot = game_add_player(false, pad, NULL, s->name);
      if (s->slot >= 0) { P[s->slot].prev = b; P[s->slot].seat = id; }
    }
    return;
  }
  game_player_input(s->slot, b, s->sx, s->sy);
}

static void went_home(seat_t *s) {
  char m[48]; snprintf(m, sizeof m, "%s WENT HOME.", P[s->slot].name); game_toast(m, C_ASH);
  game_remove_player(s->slot); s->slot = -1;
}

/* joins, play input and the idle rule. The menus that gate joining to the
 * lobby arrive with the front end; until then a press joins in any phase but
 * the results, exactly as before */
static void seats_join(float dt) {
  any_pressed = start_pressed = false;
  for (int i = 0; i < SEAT_PADS; i++) seat_step(&seats[SEAT_PAD(i)], SEAT_PAD(i), i);
  for (int k = 0; k < SEAT_KBS; k++) {
    seat_t *s = &seats[SEAT_KB(k)];
    int before = s->slot;
    seat_step(s, SEAT_KB(k), -1);
    if (s->slot >= 0 && before < 0) P[s->slot].kb = k;
  }
  /* someone who clocked in on a pad or the keyboard and walked away goes home
   * after a while, so the cabinet can fall back to its demo; this also covers
   * a pad that was unplugged */
  if (G.phase == PH_LOBBY || G.phase == PH_NIGHT)
    for (int i = 0; i < SEAT_PADS + SEAT_KBS; i++) if (seats[i].slot >= 0 && P[seats[i].slot].idle > 75) went_home(&seats[i]);
  /* phones */
  for (int i = 0; i < SEAT_PHONES; i++) {
    seat_t *ph = &seats[SEAT_PHONE(i)];
    if (ph->slot < 0) continue;
    player_t *p = &P[ph->slot];
    if (!p->active || !p->phone) { ph->slot = -1; ph->present = false; continue; }
    if (net_online()) p->seen += dt;   /* a relay outage is not the phone's fault */
    if (p->seen > 12) {
      char msg[64];
      snprintf(msg, sizeof msg, "%s LOST SIGNAL.", p->name);
      game_toast(msg, C_ASH);
      game_remove_player(ph->slot);
      ph->slot = -1; ph->present = false;
      continue;
    }
    uint32_t edge = ph->btn & ~p->prev;
    if (edge & (IN_A | IN_START)) any_pressed = true;
    if (edge & IN_START) start_pressed = true;
    game_player_input(ph->slot, ph->btn, ph->sx, ph->sy);
  }
  for (int i = 0; i < SEAT_COUNT; i++) seats[i].prev = seats[i].btn;
}

/* the phone seat (0..SEAT_PHONES-1) seated with this id, or -1 */
static int phone_slot(const char *pid) {
  for (int i = 0; i < SEAT_PHONES; i++) {
    const seat_t *s = &seats[SEAT_PHONE(i)];
    if (s->slot >= 0 && !strcmp(P[s->slot].pid, pid)) return i;
  }
  return -1;
}

static void welcome(int slot) {
  char m[64];
  snprintf(m, sizeof m, "W%d|%06x|%s", slot + 1, P[slot].color, P[slot].name);
  net_send(P[slot].pid, m);
}

void game_on_phone(const char *pid, const char *msg) {
  int k = phone_slot(pid);
  if (msg[0] == 'J') {
    /* names come from strangers' phones: short, caps, printable */
    char name[14];
    int n = 0;
    for (const char *s = msg + 1; *s && n < 12; s++) {
      unsigned char c = (unsigned char)toupper((unsigned char)*s);
      if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == '\'' || c == '_') name[n++] = (char)c;
    }
    name[n] = 0;
    if (k >= 0) { P[seats[SEAT_PHONE(k)].slot].seen = 0; welcome(seats[SEAT_PHONE(k)].slot); return; }
    if (G.phase == PH_RESULTS) { net_send(pid, "H"); return; }   /* hold on: next round */
    int slot = game_add_player(true, -1, pid, n ? name : NULL);
    if (slot < 0) { net_send(pid, "F"); return; }
    for (int i = 0; i < SEAT_PHONES; i++) {
      seat_t *s = &seats[SEAT_PHONE(i)];
      if (s->slot >= 0) continue;
      *s = (seat_t){ .present = true, .slot = slot };
      snprintf(s->pid, sizeof s->pid, "%s", pid);
      snprintf(s->name, sizeof s->name, "%s", P[slot].name);
      P[slot].seat = SEAT_PHONE(i);
      break;
    }
    welcome(slot);
  } else if (msg[0] == 'I') {
    if (k < 0) { net_send(pid, "R"); return; }   /* the cart restarted: please rejoin */
    int b = 0, x = 0, y = 0;
    sscanf(msg + 1, "%d,%d,%d", &b, &x, &y);
    seat_t *ph = &seats[SEAT_PHONE(k)];
    ph->btn = (uint32_t)b & 15;
    ph->sx = x / 100.f; ph->sy = y / 100.f;
    if (ph->sx > 1) ph->sx = 1;
    if (ph->sx < -1) ph->sx = -1;
    if (ph->sy > 1) ph->sy = 1;
    if (ph->sy < -1) ph->sy = -1;
    P[ph->slot].seen = 0;
  } else if (msg[0] == 'L') {
    if (k >= 0) { seat_t *s = &seats[SEAT_PHONE(k)]; game_remove_player(s->slot); s->slot = -1; s->present = false; }
  }
}

const char *game_room_from_session(const char *session) {
  uint32_t h = 2166136261u ^ rnd();
  for (const char *c = session; *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
  static const char L[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
  for (int i = 0; i < 4; i++) { G.room[i] = L[h % (sizeof L - 1)]; h /= (sizeof L - 1); h ^= h >> 7; h *= 2654435761u; }
  G.room[4] = 0;
  char url[160];
  snprintf(url, sizeof url, "%s", G.join_url);
  char *hash = strchr(url, '#');
  if (hash) *hash = 0;
  snprintf(G.join_url, sizeof G.join_url, "%s#%s", url, G.room);
  return G.room;
}

static void phone_status(float dt) {
  status_clock -= dt;
  if (status_clock > 0) return;
  status_clock = 1.0f;
  char m[160];
  int left = G.phase == PH_NIGHT ? (int)(G.night_len - G.t) : 0;
  snprintf(m, sizeof m, "P%d|%d|%d|%d", G.phase, left, (int)G.clutter, (int)G.vibe);
  net_send(NULL, m);
  for (int i = 0; i < SEAT_PHONES; i++) {
    if (seats[SEAT_PHONE(i)].slot < 0) continue;
    player_t *p = &P[seats[SEAT_PHONE(i)].slot];
    snprintf(m, sizeof m, "S%d|%d|%s|%s", p->clout, p->karma, p->hold >= 0 ? item_name(&I[p->hold]) : "", p->prompt);
    net_send(p->pid, m);
  }
}

/* ── the frame ───────────────────────────────────────────────────── */

static float empty_t;

static void flow(float dt) {
  int n = player_count();
  switch (G.phase) {
    case PH_LOBBY:
      if (n == 0) { if ((empty_t += dt) > 6) { G.phase = PH_TITLE; G.t = 0; } }
      else {
        empty_t = 0;
        if (start_pressed || G.t >= 25) game_start_night();
      }
      break;
    case PH_NIGHT:
      if (G.attract) break;
      if (human_count() == 0) { game_remove_bots(); n = 0; }
      if (n == 0) { if ((empty_t += dt) > 12) { G.phase = PH_TITLE; G.t = 0; audio_music(1); } }
      else empty_t = 0;
      break;
    case PH_RESULTS:
      if ((G.t > 4 && any_pressed) || G.t > 40) {
        game_remove_bots();
        G.phase = human_count() ? PH_LOBBY : PH_TITLE; G.t = 0; audio_music(1);
      }
      break;
    case PH_TITLE:
      /* attract mode: three archetypes play a night while nobody is here */
      if (G.t > 1.5f) {
        G.attract = true;
        game_add_bot(BOT_BUILDER); game_add_bot(BOT_TALKER); game_add_bot(BOT_FOUNDER);
        game_start_night();
      }
      break;
    default: break;
  }
}

static void camera(float dt) {
  static float t;
  t += dt;
  float x = 0, y = 21.5f, z = 15.0f, lx = 0, lz = -1.25f;
  if (G.phase == PH_TITLE || G.attract) { x = sinf(t * 0.12f) * 5; y = 16.5f; z = 12.5f + cosf(t * 0.12f) * 1.5f; lx = 0; lz = 4.5f; }   /* the start screen opens on the storefront */
  if (G.shake > 0) { x += sinf(t * 60) * G.shake * 0.3f; }
  t3_object_set_position(world_cam, x, y, z);
  t3_object_look_at(world_cam, lx, 0, lz);
}

__attribute__((export_name("wc_render")))
void wc_render(void) {
  float dt = (float)(tm.delta_ms / 1000.0);
  if (dt > 0.05f) dt = 0.05f;
  if (dt <= 0) dt = 1.f / 60;

  seats_poll(dt);
  seats_join(dt);
  net_update(dt);
  if (!G.paused) game_update(dt);
  flow(dt);
  phone_status(dt);
  world_sync(dt);
  camera(dt);

  t3_renderer_set_auto_clear(R, true);
  t3_renderer_render(R, scene, world_cam);

  draw_hud(dt);
  hud_tex->version++;
  t3_renderer_set_auto_clear(R, false);
  t3_renderer_render(R, hud_scene, hud_cam);

  uint32_t rate = host_info.audio_sample_rate ? host_info.audio_sample_rate : 48000;
  audio_render(audio_ring, AUDIO_CAP, &audio_cursor, rate, tm.delta_ms > 0 ? tm.delta_ms : dt * 1000.0);

  static int logged;
  const char *miss = t3_renderer_generated_missing(R);
  if (miss && *miss && logged < 3 && tm.frame % 120 == 0) { wc_log(miss, (unsigned)strlen(miss)); logged++; }
}
