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

typedef struct { int slot; uint32_t btn; float x, y; } phone_t;
static phone_t phones[MAX_PLAYERS];
static float status_clock;

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

  for (int i = 0; i < MAX_PLAYERS; i++) phones[i].slot = -1;
  net_init(G.relay, G.room);
  audio_music(1);
  WC_LOG("do-ocracy: clocked in");
}

/* ── input ───────────────────────────────────────────────────────── */

static int pad_slot[4] = { -1, -1, -1, -1 };
static float pad_gone[4];
static uint32_t pad_prev[4];

#define KEY(k) ((keys[(k) >> 3] >> ((k) & 7)) & 1)
#define LETTER(c) (0x04 + ((c) - 'a'))
static int kb_slot[2] = { -1, -1 };
static uint32_t kb_prev[2];


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

static bool any_pressed;   /* someone pressed A or START this frame */
static bool start_pressed;

static void seat(int *slot, uint32_t b, uint32_t prev, float x, float y, int pad, const char *name) {
  bool edge_a = (b & IN_A) && !(prev & IN_A), edge_s = (b & IN_START) && !(prev & IN_START);
  if (edge_a || edge_s) any_pressed = true;
  if (edge_s) start_pressed = true;
  if (*slot >= 0 && !P[*slot].active) *slot = -1;
  if (*slot < 0) {
    if ((edge_a || edge_s) && G.phase != PH_RESULTS) {
      *slot = game_add_player(false, pad, NULL, name);
      if (*slot >= 0) P[*slot].prev = b;
    }
    return;
  }
  game_player_input(*slot, b, x, y);
}

static bool keys_held(void) {
  for (int i = 1; i < 32; i++) if (keys[i]) return true;   /* skip byte 0: no keys live there */
  return false;
}

static void read_input(float dt) {
  any_pressed = start_pressed = false;
  bool kb_down = keys_held();
  for (int i = 0; i < 4; i++) {
    wc_pad_t *p = &pads[i];
    float x = 0, y = 0;
    uint32_t b = p->connected ? pad_buttons(p, &x, &y) : 0;
    if (!p->connected && pad_slot[i] >= 0) {
      pad_gone[i] += dt;
      if (pad_gone[i] > 3) { game_remove_player(pad_slot[i]); pad_slot[i] = -1; }
      continue;
    }
    pad_gone[i] = 0;
    char name[16];
    snprintf(name, sizeof name, "PAD %d", i + 1);
    if (i == 0 && kb_down && pad_slot[0] < 0) { pad_prev[0] = b; continue; }   /* that is the keyboard, mirrored */
    seat(&pad_slot[i], b, pad_prev[i], x, y, i, name);
    pad_prev[i] = b;
  }
  /* two keyboard seats, read as raw keys (browser and Node hosts; under
   * RetroArch the keyboard arrives as RetroPad 1 instead) */
  for (int k = 0; k < 2; k++) {
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
    char name[16];
    snprintf(name, sizeof name, k == 0 ? "WASD" : "ARROWS");
    int before = kb_slot[k];
    seat(&kb_slot[k], b, kb_prev[k], x, y, -1, name);
    if (kb_slot[k] >= 0 && before < 0) P[kb_slot[k]].kb = k;
    kb_prev[k] = b;
  }
  /* phones */
  for (int i = 0; i < MAX_PLAYERS; i++) {
    phone_t *ph = &phones[i];
    if (ph->slot < 0) continue;
    player_t *p = &P[ph->slot];
    if (!p->active || !p->phone) { ph->slot = -1; continue; }
    p->seen += dt;
    if (p->seen > 12) {
      char msg[64];
      snprintf(msg, sizeof msg, "%s LOST SIGNAL.", p->name);
      game_toast(msg, C_ASH);
      game_remove_player(ph->slot);
      ph->slot = -1;
      continue;
    }
    uint32_t edge = ph->btn & ~p->prev;
    if (edge & (IN_A | IN_START)) any_pressed = true;
    if (edge & IN_START) start_pressed = true;
    game_player_input(ph->slot, ph->btn, ph->x, ph->y);
  }
}

static int phone_slot(const char *pid) {
  for (int i = 0; i < MAX_PLAYERS; i++) if (phones[i].slot >= 0 && !strcmp(P[phones[i].slot].pid, pid)) return i;
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
    if (k >= 0) { P[phones[k].slot].seen = 0; welcome(phones[k].slot); return; }
    if (G.phase == PH_RESULTS) return;
    int slot = game_add_player(true, -1, pid, n ? name : NULL);
    if (slot < 0) { net_send(pid, "F"); return; }
    for (int i = 0; i < MAX_PLAYERS; i++) if (phones[i].slot < 0) { phones[i] = (phone_t){ slot, 0, 0, 0 }; break; }
    welcome(slot);
  } else if (msg[0] == 'I') {
    if (k < 0) { net_send(pid, "R"); return; }   /* the cart restarted: please rejoin */
    int b = 0, x = 0, y = 0;
    sscanf(msg + 1, "%d,%d,%d", &b, &x, &y);
    phone_t *ph = &phones[k];
    ph->btn = (uint32_t)b & 15;
    ph->x = x / 100.f; ph->y = y / 100.f;
    if (ph->x > 1) ph->x = 1;
    if (ph->x < -1) ph->x = -1;
    if (ph->y > 1) ph->y = 1;
    if (ph->y < -1) ph->y = -1;
    P[ph->slot].seen = 0;
  } else if (msg[0] == 'L') {
    if (k >= 0) { game_remove_player(phones[k].slot); phones[k].slot = -1; }
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
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (phones[i].slot < 0) continue;
    player_t *p = &P[phones[i].slot];
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
  float x = 0, y = 21.5f, z = 15.0f, lx = 0, lz = -0.75f;
  if (G.phase == PH_TITLE || G.attract) { x = sinf(t * 0.12f) * 5; y = 16.5f; z = 12.5f + cosf(t * 0.12f) * 1.5f; lx = 2.5f; lz = -0.5f; }
  if (G.shake > 0) { x += sinf(t * 60) * G.shake * 0.3f; }
  t3_object_set_position(world_cam, x, y, z);
  t3_object_look_at(world_cam, lx, 0, lz);
}

__attribute__((export_name("wc_render")))
void wc_render(void) {
  float dt = (float)(tm.delta_ms / 1000.0);
  if (dt > 0.05f) dt = 0.05f;
  if (dt <= 0) dt = 1.f / 60;

  read_input(dt);
  net_update(dt);
  game_update(dt);
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
