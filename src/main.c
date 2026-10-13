/* DO-OCRACY: the wasmcart cart. The host calls wc_get_info, wc_init, then
 * wc_render once a frame; everything else is ours. */
#include "wasmcart.h"
#include "three.h"
#include "game.h"
#include "hud.h"
#include "net.h"
#include "audio.h"
#include "content.h"
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
  float seen;          /* phones on the front end: seconds since the relay last heard it */
} seat_t;
static seat_t seats[SEAT_COUNT];
static void front_end_reset(void);
static float status_clock;

/* MUSIC and SFX into the band; the music sits at half while a night is paused */
static void apply_levels(void) { audio_set_levels(SET.music / 10.f * (G.paused ? 0.5f : 1.0f), SET.sfx / 10.f); }

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
  front_end_reset();
  net_init(G.relay, G.room);
  audio_music(1);
  apply_levels();
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
      if (KEY(0xe5)) b |= IN_START;                     /* right shift */
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
static float empty_t;
static int last_n;

static bool front_end_up(void) { return G.phase == PH_TITLE || G.attract; }
static void welcome(int slot);
static void open_modes(void);
static void open_settings(int then);

/* ── the front end: owner, pending seats, the menus (FINAL-frontend.md 3, 6) ── */

static void menu_goto(int screen) {
  M.prev_screen = M.screen; M.screen = screen; M.t = 0; M.idle = 0;
  if (screen == SCR_MAIN && (M.prev_screen == SCR_TITLE || M.prev_screen == SCR_ATTRACT)) { M.row = 0; M.cur_y = 200; }
}

static void front_end_reset(void) {
  bool kb = M.kb_seen;
  memset(&M, 0, sizeof M);
  M.kb_seen = kb; M.owner = -1; M.screen = M.prev_screen = SCR_TITLE; M.cur_y = 200; M.lobby_t = 25;
}

/* a new group walked up: everything about the last one is forgotten */
static void new_group(void) {
  G.phase = PH_TITLE; G.t = 0; G.attract = false; G.paused = false;
  G.flash_seen = false; G.group_nights = 0; G.wipe_t = 0;
  for (int i = 0; i < SEAT_COUNT; i++) {
    seat_t *s = &seats[i];
    s->pending = s->denied = false; s->hold_b = 0;
    if (i >= SEAT_PHONE(0) && s->slot < 0) s->present = false;
  }
  front_end_reset();
  audio_music(1);
}

static void seat_owner(int id) { M.owner = id; seats[id].pending = false; M.idle = 0; }

static int lowest_pending(void) {
  for (int i = 0; i < SEAT_COUNT; i++) if (seats[i].present && seats[i].pending && seats[i].slot < 0) return i;
  return -1;
}

/* a second seat pressed A on a menu: one joke per seat, then it waits at the door */
static void deny_seat(int id) {
  seat_t *s = &seats[id];
  if (s->pending || s->slot >= 0) return;
  if (!s->denied) {
    M.deny_t = 2;
    snprintf(M.deny, sizeof M.deny, "%s", DENY_LINES[M.screen == SCR_MAIN ? 0 : 1]);
    audio_play(SFX_MENU_DENY);
    s->denied = true;
  }
  s->pending = true;
}

/* seat one seat as a player in the lobby */
static int seat_in(int id) {
  seat_t *s = &seats[id];
  bool phone = id >= SEAT_PHONE(0);
  int slot = game_add_player(phone, id < SEAT_PADS ? id : -1, phone ? s->pid : NULL, s->name);
  if (slot < 0) return -1;
  s->slot = slot; s->pending = false; s->hold_b = 0;
  P[slot].seat = id; P[slot].prev = s->btn;
  if (id >= SEAT_KB(0) && !phone) P[slot].kb = id - SEAT_KB(0);
  if (phone) welcome(slot);
  return slot;
}

/* ── the rules flash (6.6) ── */

static bool flash_due(void) { return SET.help == HELP_ALWAYS || (SET.help == HELP_FIRST && !G.flash_seen); }

static void flash_open(int ret) {
  M.flash_step = 0; M.flash_t = 0; M.flash_wipe = 0; M.flash_fired = false; M.flash_end = 0;
  M.flash_return = ret;
  if (ret == SCR_MAIN) menu_goto(SCR_FLASH);
  else { M.prev_screen = M.screen; M.screen = SCR_FLASH; M.t = 0; }
}

static void flash_close(void) {
  if (M.flash_end > 0) return;
  M.flash_end = 0.25f;
  audio_play(SFX_MENU_BACK);
}

static void flash_next(void) {
  if (M.flash_wipe > 0 || M.flash_end > 0) return;
  if (M.flash_step >= 4) { flash_close(); return; }
  M.flash_wipe = 0.3f;
  audio_play(SFX_CARD);
}

/* the clock, the card beats and the owner's A and B */
static void flash_update(float dt) {
  int drv = G.paused ? M.pauser : M.owner;   /* over a paused night the pauser drives */
  int nav = drv >= 0 && seats[drv].present ? seats[drv].nav : 0;
  if (M.flash_end > 0) {
    M.flash_end -= dt;
    if (M.flash_end <= 0) {
      M.flash_end = 0;
      G.flash_seen = true;
      if (M.flash_return == SCR_MAIN) menu_goto(SCR_MAIN);
      else { M.prev_screen = SCR_FLASH; M.screen = SCR_NONE; M.t = 0; }
    }
    return;
  }
  if (drv < 0 || !seats[drv].present) { flash_close(); return; }
  if (nav & NAV_B) { flash_close(); return; }
  if (nav & (NAV_A | NAV_START)) { flash_next(); return; }
  if (M.flash_wipe > 0) {
    float before = M.flash_wipe;
    M.flash_wipe -= dt;
    if (before > 0.15f && M.flash_wipe <= 0.15f) { M.flash_step++; M.flash_fired = false; M.flash_t = 0; }
    if (M.flash_wipe <= 0) M.flash_wipe = 0;
    return;
  }
  M.flash_t += dt;
  /* each card's one beat */
  static const float beat[5] = { 2.0f, 1.85f, 99, 1.9f, 1.7f };   /* card 3 has no beat */
  static const int sfx[5] = { SFX_GLAMOUR, SFX_GLAMOUR, 0, SFX_ALARM, SFX_KARMA };
  int k = M.flash_step > 4 ? 4 : M.flash_step;
  if (!M.flash_fired && M.flash_t >= beat[k]) {
    M.flash_fired = true;
    audio_play(sfx[k]);
    if (M.flash_step == 0) fx2_burst(FX2_SPARK, 640, 220, C_HAZARD);
    else if (M.flash_step == 1) fx2_burst(FX2_CONFETTI, 860, 210, C_HAZARD);
    else if (M.flash_step == 4) fx2_burst(FX2_RING, 804, 270, C_LIVE);
  }
  if (M.flash_t >= 2.6f) flash_next();
}

/* PLAY: the owner is P1, then everyone at the door in seat order */
static void front_end_play(void) {
  game_begin_lobby();
  /* the ROOM setting: build the chosen level before anyone is seated in it */
  if (SET.layout != G.level && SET.layout < LEVEL_COUNT) {
    game_set_level(SET.layout);
    game_reset_world();
    world_set_level(L);
    game_apply_settings();
  }
  M.lobby_t = 25; M.ready_t = 0;
  if (M.owner >= 0) seat_in(M.owner);
  for (int i = 0; i < SEAT_COUNT; i++) if (seats[i].present && seats[i].pending && seats[i].slot < 0) seat_in(i);
  M.prev_screen = SCR_MAIN; M.screen = SCR_NONE; M.t = 0;
  if (flash_due()) flash_open(SCR_NONE);
}

/* the owner, alone in the lobby, held B: back to the menu with the seat kept */
static void front_end_back_to_menu(void) {
  int o = M.owner;
  game_quit_to_title();
  for (int i = 0; i < SEAT_COUNT; i++) { seats[i].slot = -1; seats[i].hold_b = 0; }
  M.owner = o; M.prev_screen = SCR_NONE; M.screen = SCR_MAIN; M.t = 0; M.row = 0; M.cur_y = 200; M.idle = 0;
  M.lobby_t = 25; M.ready_t = 0;
  audio_play(SFX_MENU_BACK);
}

static void menu_select(void) {
  audio_play(SFX_MENU_OK);
  M.sel_t = 0.08f;
  fx2_burst(FX2_SPARK, 616, 200 + M.row * 64 + 28, C_HAZARD);
  switch (M.row) {
    case 0: front_end_play(); break;
    case 1: open_modes(); break;
    case 2: open_settings(TO_MAIN); break;
    case 3: flash_open(SCR_MAIN); break;
    case 4: menu_goto(SCR_CREDITS); break;
  }
}

static void menu_main_input(int nav) {
  int n = COUNT(MENU_ROWS);
  if (nav & (NAV_UP | NAV_DOWN)) {
    int d = (nav & NAV_UP) ? -1 : 1;
    M.row = (M.row + d + n) % n; M.cur_dir = d; M.snap_t = 0;
    audio_play(SFX_MENU_MOVE);
  }
  if (M.row == 0 && (nav & (NAV_LEFT | NAV_RIGHT))) {
    int d = (nav & NAV_LEFT) ? -1 : 1;
    SET.mode = (uint8_t)((SET.mode + d + MODE_COUNT) % MODE_COUNT);
    save_commit();
    M.val_slide = 0.18f; M.val_dir = d;
    audio_play(SFX_MENU_MOVE);
  }
  if (nav & NAV_B) { M.owner = -1; menu_goto(SCR_TITLE); audio_play(SFX_MENU_BACK); return; }
  if (nav & NAV_START) { M.row = 0; menu_select(); return; }
  if (nav & NAV_A) menu_select();
}


/* ── MODES and SETTINGS (6.4, 6.5) ── */

static void open_modes(void) {
  menu_goto(SCR_MODES);
  M.col = SET.mode < MODE_COUNT ? SET.mode : 0;
  for (int i = 0; i < MODE_COUNT; i++) M.lift[i] = i == M.col ? 1 : 0;
}

static void open_settings(int then) {
  M.settings_then = then;
  menu_goto(SCR_SETTINGS);
  M.row = 0; M.cur_y = 174; M.cur_dir = 0; M.snap_t = 1; M.val_slide = 0;
}

/* back on the main menu with a row focused and the cursor already there */
static void main_at(int row) {
  menu_goto(SCR_MAIN);
  M.row = row; M.cur_y = 200 + row * 64; M.cur_dir = 0; M.snap_t = 1;
}

static void modes_input(int nav) {
  if (nav & (NAV_LEFT | NAV_RIGHT)) {
    int d = (nav & NAV_LEFT) ? -1 : 1;
    M.col = (M.col + d + MODE_COUNT) % MODE_COUNT;
    audio_play(SFX_MENU_MOVE);
  }
  if (nav & NAV_B) { main_at(1); audio_play(SFX_MENU_BACK); return; }
  if (!(nav & (NAV_A | NAV_START))) return;
  SET.mode = (uint8_t)M.col;
  save_commit();
  audio_play(SFX_MENU_OK);
  M.sel_t = 0.08f;
  fx2_burst(FX2_RING, 48 + M.col * 240 + 112, 380, MODES[M.col].color);
  if (M.col == MODE_CUSTOM) open_settings(TO_LOBBY);
  else if (nav & NAV_START) front_end_play();
  else main_at(0);
}

/* the rows the mode decides; LEFT and RIGHT on them only deny */
bool settings_pinned(int row) {
  switch (SET.mode) {
    case MODE_SOLO: return row == SET_ROW_BOTS;
    case MODE_CHAOS: return row == SET_ROW_NIGHT || row == SET_ROW_CHAOS;
    default: return false;
  }
}

static int step_clamp(int v, int d, int n) { v += d; return v < 0 ? 0 : v >= n ? n - 1 : v; }

/* one step of a value; returns false when nothing changed (the end of a list) */
static bool settings_step(int row, int dir) {
  settings_t was = SET;
  switch (row) {
    case SET_ROW_NIGHT: SET.night_len = (uint8_t)step_clamp(SET.night_len, dir, COUNT(NIGHT_LENS)); break;
    case SET_ROW_BOTS:  SET.bots = !SET.bots; break;
    case SET_ROW_CHAOS: SET.chaos = (uint8_t)step_clamp(SET.chaos, dir, COUNT(CHAOS_NAMES)); break;
    case SET_ROW_PHONES: SET.phones = !SET.phones; break;
    case SET_ROW_MUSIC: SET.music = (uint8_t)step_clamp(SET.music, dir, 11); break;
    case SET_ROW_SFX:   SET.sfx = (uint8_t)step_clamp(SET.sfx, dir, 11); break;
    case SET_ROW_ROOM:  if (LEVEL_COUNT > 1) SET.layout = (uint8_t)((SET.layout + dir + LEVEL_COUNT) % LEVEL_COUNT); break;
    case SET_ROW_HELP:  SET.help = (uint8_t)step_clamp(SET.help, dir, COUNT(HELP_NAMES)); break;
    default: return false;
  }
  if (!memcmp(&was, &SET, sizeof SET)) return false;
  save_commit();
  if (row == SET_ROW_MUSIC || row == SET_ROW_SFX) apply_levels();
  M.val_slide = 0.18f; M.val_dir = dir;
  audio_play(SFX_MENU_MOVE);
  return true;
}

static void settings_leave(void) {
  if (M.settings_then == TO_LOBBY) { audio_play(SFX_MENU_OK); front_end_play(); }
  else { main_at(2); audio_play(SFX_MENU_BACK); }
}

static void settings_input(int nav) {
  int n = SET_ROW_COUNT;
  if (nav & (NAV_UP | NAV_DOWN)) {
    int d = (nav & NAV_UP) ? -1 : 1;
    M.row = (M.row + d + n) % n; M.cur_dir = d; M.snap_t = 0;
    audio_play(SFX_MENU_MOVE);
  }
  if (nav & NAV_B) {
    if (M.settings_then == TO_LOBBY) { open_modes(); M.col = MODE_CUSTOM; for (int i = 0; i < MODE_COUNT; i++) M.lift[i] = i == M.col; }
    else main_at(2);
    audio_play(SFX_MENU_BACK);
    return;
  }
  if (nav & NAV_START) { settings_leave(); return; }
  int dir = (nav & NAV_LEFT) ? -1 : (nav & (NAV_RIGHT | NAV_A)) ? 1 : 0;
  if (!dir) return;
  if (M.row == SET_ROW_DONE) { if (nav & NAV_A) settings_leave(); return; }
  if (M.row == SET_ROW_RESET) {
    if (!(nav & NAV_A)) return;
    uint8_t mode = SET.mode;
    settings_defaults(&SET);
    SET.mode = mode;   /* the mode is picked on MODES, not here */
    save_commit(); apply_levels();
    M.val_slide = 0.18f; M.val_dir = 2;   /* every value slides in */
    audio_play(SFX_MENU_BACK);
    return;
  }
  if (settings_pinned(M.row)) {
    M.deny_t = 2;
    snprintf(M.deny, sizeof M.deny, "%s", DENY_LINES[2]);
    audio_play(SFX_MENU_DENY);
    return;
  }
  /* A wraps the two toggles and every list; LEFT and RIGHT stop at the ends */
  if ((nav & NAV_A) && !settings_step(M.row, 1)) settings_step(M.row, -99);
  else if (!(nav & NAV_A)) settings_step(M.row, dir);
}

/* ── pause (6.10) and the way back to the menu from a night or the results ── */

static bool resumed_now;   /* the START that resumed must not pause again in seats_join */

static void pause_night(int seat) {
  G.paused = true; M.pauser = seat; M.pause_row = 0; M.pause_t = 0;
  apply_levels();
  audio_play(SFX_MENU_MOVE);
}

static void resume_night(void) {
  G.paused = false; resumed_now = true;
  apply_levels();
  audio_play(SFX_MENU_BACK);
}

/* everyone off the floor and back to MAIN: this seat drives, every other human waits at the door */
static void menu_with_owner(int seat) {
  bool seated[SEAT_COUNT];
  for (int i = 0; i < SEAT_COUNT; i++) seated[i] = seats[i].slot >= 0;
  game_quit_to_title();
  apply_levels();
  for (int i = 0; i < SEAT_COUNT; i++) {
    seats[i].slot = -1; seats[i].hold_b = 0;
    seats[i].pending = seated[i] && i != seat && seats[i].present;
  }
  M.owner = seat; seats[seat].pending = false;
  M.prev_screen = SCR_NONE; M.screen = SCR_MAIN; M.t = 0; M.idle = 0;
  M.row = 0; M.cur_y = 200; M.cur_dir = 0; M.snap_t = 1;
  M.lobby_t = 25; M.ready_t = 0;
  audio_play(SFX_MENU_BACK);
}

static void pause_input(void) {
  int nav = M.pauser >= 0 && seats[M.pauser].present ? seats[M.pauser].nav : 0;
  for (int i = 0; i < SEAT_COUNT; i++) if (seats[i].nav & (NAV_START | NAV_B)) { resume_night(); return; }
  if (nav & (NAV_UP | NAV_DOWN)) {
    int d = (nav & NAV_UP) ? -1 : 1;
    M.pause_row = (M.pause_row + d + 3) % 3;
    audio_play(SFX_MENU_MOVE);
  }
  if (!(nav & NAV_A)) return;
  audio_play(SFX_MENU_OK);
  if (M.pause_row == 0) resume_night();
  else if (M.pause_row == 1) flash_open(SCR_NONE);
  else menu_with_owner(M.pauser);
}

static void menu_update(float dt) {
  M.t += dt; M.snap_t += dt;
  if (M.deny_t > 0) M.deny_t -= dt;
  if (M.status_t > 0) M.status_t -= dt;
  if (M.sel_t > 0) M.sel_t -= dt;
  if (M.val_slide > 0) M.val_slide -= dt;
  /* a phone waiting at the door that the relay stopped hearing has gone */
  for (int i = SEAT_PHONE(0); i < SEAT_COUNT; i++) {
    seat_t *s = &seats[i];
    if (!s->present || s->slot >= 0) continue;
    if (net_online()) s->seen += dt;
    if (s->seen > 12) { s->present = s->pending = false; if (M.owner == i) M.owner = -1; }
  }
  if (G.paused) M.pause_t += dt;
  if (G.phase == PH_RESULTS) {
    if (G.t > 4) for (int i = 0; i < SEAT_COUNT; i++) if (seats[i].present && (seats[i].nav & NAV_B)) { menu_with_owner(i); break; }
  } else if (G.paused && M.screen != SCR_FLASH) {
    pause_input();
  } else if (M.screen == SCR_FLASH) {
    flash_update(dt);
    /* over the front end the others still get their one deny line */
    if (front_end_up()) for (int i = 0; i < SEAT_COUNT; i++) if (i != M.owner && (seats[i].nav & (NAV_A | NAV_START))) deny_seat(i);
  } else if (front_end_up()) {
    int any = 0, claim = -1;
    for (int i = 0; i < SEAT_COUNT; i++) {
      any |= seats[i].nav;
      if (claim < 0 && (seats[i].nav & (NAV_A | NAV_START))) claim = i;
    }
    switch (M.screen) {
      case SCR_TITLE:
        M.title_idle += dt;
        if (any) M.title_idle = 0;
        if (claim >= 0) { seat_owner(claim); menu_goto(SCR_MAIN); audio_play(SFX_MENU_OK); }
        else if (M.title_idle > 20) menu_goto(SCR_ATTRACT);
        break;
      case SCR_ATTRACT:
        if (any) {
          M.title_idle = 0;
          menu_goto(SCR_TITLE);
          if (claim >= 0) { seat_owner(claim); menu_goto(SCR_MAIN); audio_play(SFX_MENU_OK); }
          else audio_play(SFX_MENU_MOVE);
        }
        break;
      default: {
        if (M.owner < 0 || !seats[M.owner].present) { M.owner = -1; menu_goto(SCR_TITLE); break; }
        int nav = seats[M.owner].nav;
        if (nav) M.idle = 0; else M.idle += dt;
        if (M.idle > 20) {
          int p = lowest_pending();
          if (p >= 0) { seat_owner(p); audio_play(SFX_MENU_OK); }
          else { M.owner = -1; menu_goto(SCR_TITLE); audio_play(SFX_MENU_BACK); break; }
          nav = 0;
        }
        for (int i = 0; i < SEAT_COUNT; i++) if (i != M.owner && (seats[i].nav & (NAV_A | NAV_START))) deny_seat(i);
        if (M.screen == SCR_MAIN) menu_main_input(nav);
        else if (M.screen == SCR_MODES) modes_input(nav);
        else if (M.screen == SCR_SETTINGS) settings_input(nav);
        else if (nav & (NAV_A | NAV_B | NAV_START)) { menu_goto(SCR_MAIN); audio_play(SFX_MENU_BACK); }   /* the placeholder screens */
        break;
      }
    }
  }
  /* mirrored for draw.c */
  M.pending_count = 0;
  for (int i = 0; i < SEAT_COUNT; i++) {
    if (seats[i].present && seats[i].pending && seats[i].slot < 0) M.pending_seat[M.pending_count++] = i;
    memcpy(M.seat_name[i], seats[i].name, sizeof M.seat_name[i]);
  }
}

/* ── joins, play input, ready and leave ─────────────────────────── */

static bool can_join(void) { return G.phase == PH_LOBBY || (G.phase == PH_NIGHT && !G.attract); }

static void toggle_ready(int slot) {
  player_t *p = &P[slot];
  p->ready = !p->ready;
  if (!p->ready) { audio_play(SFX_MENU_BACK); return; }
  audio_play(SFX_READY);
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && !P[i].bot && !P[i].ready) {
    snprintf(M.status, sizeof M.status, "WAITING ON %s", P[i].name);
    M.status_t = 2;
    break;
  }
}

/* one pad or keyboard seat: an A or START edge on an unseated seat clocks
 * in (lobby and night only); a seated seat drives its player, and in the
 * lobby START is READY and a held B is the way home */
static void seat_step(seat_t *s, int id, int pad, float dt) {
  uint32_t b = s->btn, prev = s->prev;
  bool edge_a = (b & IN_A) && !(prev & IN_A), edge_s = (b & IN_START) && !(prev & IN_START);
  if (edge_a || edge_s) any_pressed = true;
  if (s->slot >= 0 && !P[s->slot].active) s->slot = -1;
  if (s->slot < 0) {
    s->hold_b = 0;
    if ((edge_a || edge_s) && can_join() && !G.paused) {
      s->slot = game_add_player(false, pad, NULL, s->name);
      if (s->slot >= 0) { P[s->slot].prev = b; P[s->slot].seat = id; s->pending = false; }
    }
    return;
  }
  if (M.screen == SCR_FLASH && id == M.owner) { s->hold_b = 0; game_player_input(s->slot, 0, 0, 0); return; }   /* the owner's buttons drive the flash */
  if (G.paused) { s->hold_b = 0; return; }
  if (edge_s && G.phase == PH_NIGHT && !G.attract && G.wipe_t <= 0 && !resumed_now) { s->hold_b = 0; pause_night(id); return; }
  if (G.phase == PH_LOBBY) {
    if (edge_s) toggle_ready(s->slot);
    s->hold_b = (b & IN_B) ? s->hold_b + dt : 0;
  } else s->hold_b = 0;
  game_player_input(s->slot, b, s->sx, s->sy);
}

static void went_home(seat_t *s) {
  char m[48]; snprintf(m, sizeof m, "%s WENT HOME.", P[s->slot].name); game_toast(m, C_ASH);
  game_remove_player(s->slot); s->slot = -1;
}

/* the lowest seat that is seated, for the owner handover */
static int lowest_seated(void) {
  for (int i = 0; i < SEAT_COUNT; i++) if (seats[i].slot >= 0 && P[seats[i].slot].active) return i;
  return -1;
}

/* hold B in the lobby: 1.5 s and you go home; the owner alone gets 1.0 s and the menu */
static void lobby_leave(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) M.hold_b[i] = 0;
  M.tape_held = M.screen == SCR_FLASH;   /* the lobby clock waits for the flash */
  if (G.phase != PH_LOBBY) return;
  for (int i = 0; i < SEAT_COUNT; i++) {
    seat_t *s = &seats[i];
    if (s->slot < 0 || s->hold_b <= 0) continue;
    bool alone = i == M.owner && human_count() == 1;
    float need = alone ? 1.0f : 1.5f;
    M.hold_b[s->slot] = fminf(1, s->hold_b / need);
    M.tape_held = true;
    if (s->hold_b < need) continue;
    if (alone) { front_end_back_to_menu(); return; }
    went_home(s);
    s->hold_b = 0;
    audio_play(SFX_MENU_BACK);
    if (i == M.owner) M.owner = lowest_seated();
  }
}

static void seats_join(float dt) {
  any_pressed = false;
  for (int i = 0; i < SEAT_PADS; i++) seat_step(&seats[SEAT_PAD(i)], SEAT_PAD(i), i, dt);
  for (int k = 0; k < SEAT_KBS; k++) {
    seat_t *s = &seats[SEAT_KB(k)];
    int before = s->slot;
    seat_step(s, SEAT_KB(k), -1, dt);
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
    if (M.screen == SCR_FLASH && SEAT_PHONE(i) == M.owner) { ph->hold_b = 0; game_player_input(ph->slot, 0, 0, 0); continue; }
    if (G.paused) continue;
    if ((edge & IN_START) && G.phase == PH_NIGHT && !G.attract && G.wipe_t <= 0 && !resumed_now) { pause_night(SEAT_PHONE(i)); continue; }
    if (edge & (IN_A | IN_START)) any_pressed = true;
    if (G.phase == PH_LOBBY) {
      if (edge & IN_START) toggle_ready(ph->slot);
      ph->hold_b = (ph->btn & IN_B) ? ph->hold_b + dt : 0;
    } else ph->hold_b = 0;
    game_player_input(ph->slot, ph->btn, ph->sx, ph->sy);
  }
  lobby_leave();
  resumed_now = false;
  for (int i = 0; i < SEAT_COUNT; i++) seats[i].prev = seats[i].btn;
}

/* the phone seat (0..SEAT_PHONES-1) registered with this id, seated or waiting at the door, or -1 */
static int phone_seat(const char *pid) {
  for (int i = 0; i < SEAT_PHONES; i++) {
    const seat_t *s = &seats[SEAT_PHONE(i)];
    if (s->present && !strcmp(s->pid, pid)) return i;
  }
  return -1;
}

static void welcome(int slot) {
  char m[64];
  snprintf(m, sizeof m, "W%d|%06x|%s", slot + 1, P[slot].color, P[slot].name);
  net_send(P[slot].pid, m);
}

void game_on_phone(const char *pid, const char *msg) {
  int k = phone_seat(pid);
  if (msg[0] == 'J') {
    /* names come from strangers' phones: short, caps, printable */
    char name[14];
    int n = 0;
    for (const char *s = msg + 1; *s && n < 12; s++) {
      unsigned char c = (unsigned char)toupper((unsigned char)*s);
      if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == '\'' || c == '_') name[n++] = (char)c;
    }
    name[n] = 0;
    if (!SET.phones) { net_send(pid, "F"); return; }
    if (k >= 0) {
      seat_t *s = &seats[SEAT_PHONE(k)];
      s->seen = 0;
      if (s->slot >= 0) { P[s->slot].seen = 0; welcome(s->slot); }
      else net_send(pid, "H");
      return;
    }
    if (G.phase == PH_RESULTS) { net_send(pid, "H"); return; }   /* hold on: next round */
    int free_seat = -1;
    for (int i = 0; i < SEAT_PHONES; i++) if (!seats[SEAT_PHONE(i)].present) { free_seat = SEAT_PHONE(i); break; }
    if (free_seat < 0) { net_send(pid, "F"); return; }
    seat_t *s = &seats[free_seat];
    *s = (seat_t){ .present = true, .slot = -1 };
    snprintf(s->pid, sizeof s->pid, "%s", pid);
    if (n) snprintf(s->name, sizeof s->name, "%s", name);
    else snprintf(s->name, sizeof s->name, "PHONE %d", free_seat - SEAT_PHONE(0) + 1);
    /* on the front end a phone waits at the door until PLAY opens the lobby */
    if (front_end_up()) { s->pending = true; net_send(pid, "H"); return; }
    int slot = game_add_player(true, -1, pid, s->name);
    if (slot < 0) { s->present = false; net_send(pid, "F"); return; }
    s->slot = slot; P[slot].seat = free_seat;
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
    ph->seen = 0;
    if (ph->slot >= 0) P[ph->slot].seen = 0;   /* input reaches the sim only once the phone has a slot (seats_join) */
  } else if (msg[0] == 'L') {
    if (k >= 0) {
      seat_t *s = &seats[SEAT_PHONE(k)];
      if (s->slot >= 0) game_remove_player(s->slot);
      s->slot = -1; s->present = s->pending = false;
      if (M.owner == SEAT_PHONE(k)) M.owner = -1;
    }
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

static bool all_ready(void) {
  int h = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (P[i].active && !P[i].bot) { if (!P[i].ready) return false; h++; }
  return h > 0;
}

/* the night start tape wipe; game_start_night fires at the closed frame */
static void begin_night(void) {
  if (G.wipe_t > 0) return;
  G.wipe_t = 0.6f;
  G.group_nights++;
}

static int last_phase = -1;
static bool confetti_fired[3];   /* results t 0.3, 1.1, 1.9 on a survived night */

/* records on the results entry frame: the best karma and clout (clout only
 * when the lab survived) and the best co-op team; draw.c gets the beaten ones
 * through M.rec_* for the LAB RECORD chips */
static void save_records(void) {
  M.rec_karma = M.rec_clout = -1; M.rec_team = false;
  SAVE.nights_played++;
  bool ok = G.end == END_SURVIVED;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const player_t *p = &P[i];
    if (!p->active || p->bot) continue;
    if (p->karma > SAVE.best_karma) {
      SAVE.best_karma = p->karma; snprintf(SAVE.best_karma_name, sizeof SAVE.best_karma_name, "%s", p->name); M.rec_karma = i;
    }
    if (ok && p->clout > SAVE.best_clout) {
      SAVE.best_clout = p->clout; snprintf(SAVE.best_clout_name, sizeof SAVE.best_clout_name, "%s", p->name); M.rec_clout = i;
    }
  }
  if (G.mode == MODE_COOP && G.team_karma > SAVE.best_team) { SAVE.best_team = G.team_karma; M.rec_team = true; }
  save_commit();
}

static void flow(float dt) {
  int n = player_count();
  if (G.wipe_t > 0) {
    float before = G.wipe_t;
    G.wipe_t -= dt;
    if (before > 0.3f && G.wipe_t <= 0.3f && G.phase == PH_LOBBY) game_start_night();
    if (G.wipe_t < 0) { G.wipe_t = 0; M.ready_t = 0; }
  }
  switch (G.phase) {
    case PH_LOBBY:
      if (n > last_n) M.lobby_t = 25;   /* every join resets the clock */
      if (n == 0) { if ((empty_t += dt) > 6) new_group(); }
      else if (G.wipe_t <= 0) {
        empty_t = 0;
        if (!M.tape_held) M.lobby_t -= dt;
        if (M.lobby_t < 0) M.lobby_t = 0;
        if (M.screen == SCR_FLASH) M.ready_t = 0;
        else if (all_ready()) {
          float before = M.ready_t;
          M.ready_t += dt;
          if (M.ready_t >= 3) { audio_play(SFX_GO); begin_night(); }
          else if (before == 0 || (int)before != (int)M.ready_t) audio_play(SFX_COUNTDOWN);
        } else {
          M.ready_t = 0;
          if (M.lobby_t <= 0) begin_night();
        }
      }
      break;
    case PH_NIGHT:
      if (G.attract) break;
      if (human_count() == 0) { game_remove_bots(); n = 0; }
      if (n == 0) { if ((empty_t += dt) > 12) new_group(); }
      else empty_t = 0;
      break;
    case PH_RESULTS:
      if (last_phase != PH_RESULTS) { save_records(); for (int k = 0; k < 3; k++) confetti_fired[k] = false; }
      if (G.end == END_SURVIVED)
        for (int k = 0; k < 3; k++) if (!confetti_fired[k] && G.t >= 0.3f + 0.8f * k) {
          fx_burst(FX_CONFETTI, S[ST_SHOW].x, 2.0f, S[ST_SHOW].z - 0.6f, C_HAZARD);
          confetti_fired[k] = true;
        }
      if ((G.t > 4 && any_pressed) || G.t > 40) {
        game_remove_bots();
        if (human_count()) { G.phase = PH_LOBBY; G.t = 0; M.lobby_t = 25; M.ready_t = 0; audio_music(1); }
        else new_group();
      }
      break;
    case PH_TITLE:
      /* attract mode: three archetypes play a night behind the menus */
      if (G.t > 1.5f) {
        G.attract = true;
        game_add_bot(BOT_BUILDER); game_add_bot(BOT_TALKER); game_add_bot(BOT_FOUNDER);
        game_start_night();
      }
      break;
    default: break;
  }
  last_n = player_count();
  last_phase = G.phase;
}

/* the orbit over the storefront behind the menus, the play camera otherwise,
 * with a 0.8 s ease between them */
static void camera(float dt) {
  static float t;
  t += dt;
  float target = front_end_up() ? 0 : 1;
  if (M.cam_blend < target) M.cam_blend = fminf(target, M.cam_blend + dt / 0.8f);
  else M.cam_blend = fmaxf(target, M.cam_blend - dt / 0.8f);
  float k = hud_ease_inout(M.cam_blend);
  float ox = sinf(t * 0.12f) * 5, oy = 16.5f, oz = 12.5f + cosf(t * 0.12f) * 1.5f, olz = 4.5f;   /* the start screen opens on the storefront */
  float px = 0, py = 21.5f, pz = 15.0f, plz = -1.25f;
  float x = ox + (px - ox) * k, y = oy + (py - oy) * k, z = oz + (pz - oz) * k, lz = olz + (plz - olz) * k;
  if (G.shake > 0) { x += sinf(t * 60) * G.shake * 0.3f; }
  t3_object_set_position(world_cam, x, y, z);
  t3_object_look_at(world_cam, 0, 0, lz);
}

__attribute__((export_name("wc_render")))
void wc_render(void) {
  float dt = (float)(tm.delta_ms / 1000.0);
  if (dt > 0.05f) dt = 0.05f;
  if (dt <= 0) dt = 1.f / 60;

  seats_poll(dt);
  menu_update(dt);
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
