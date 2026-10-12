/* Pipeline smoke test: three.c scene + a CPU HUD texture drawn over it. */
#define WC_USE_GL
#include "wasmcart.h"
#include "three.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 1280
#define H 720
#define AUDIO_CAP 2048
static uint32_t fb[4];
static float audio[AUDIO_CAP * 2];
static uint32_t audio_w;
static wc_pad_t pads[4];
static wc_time_t tm;
static wc_info_t info;
static wc_host_info_t hi;

static t3_renderer *R;
static t3_scene *scene, *hud;
static t3_camera *cam, *hudcam;
static t3_mesh *cube;
static t3_texture *hudtex;
#define HW 480
#define HH 270

__attribute__((export_name("wc_get_info")))
wc_info_t *wc_get_info(void) {
  info.version = WC_ABI_VERSION; info.width = W; info.height = H;
  info.fb_ptr = (uint32_t)(uintptr_t)fb; info.audio_ptr = (uint32_t)(uintptr_t)audio;
  info.audio_cap = AUDIO_CAP; info.audio_write_ptr = (uint32_t)(uintptr_t)&audio_w;
  info.input_ptr = (uint32_t)(uintptr_t)pads; info.time_ptr = (uint32_t)(uintptr_t)&tm;
  info.host_info_ptr = (uint32_t)(uintptr_t)&hi; info.flags = WC_FLAG_AUDIO_F32; info.gpu_api = 1;
  return &info;
}

__attribute__((export_name("wc_init")))
void wc_init(void) {
  extern const struct t3_gen_table hsx_gl;
  t3_register_program_table(&hsx_gl, NULL);
  R = t3_renderer_new(W, H);
  t3_renderer_set_shadow_map(R, true, T3_PCF_SHADOW_MAP);
  t3_renderer_set_exact_output(R, true);
  t3_renderer_set_clear_color(R, 0x15120f, 1);
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(40, (float)W / H, 0.1f, 100);
  t3_object_set_position(cam, 0, 6, 9);
  t3_object_look_at(cam, 0, 0, 0);
  t3_mesh *floor_ = t3_mesh_new(t3_plane_geometry_new(20, 20, 1, 1), t3_mesh_standard_material_new(0x26211b));
  t3_object_set_rotation(floor_, -1.5708f, 0, 0);
  floor_->base.receive_shadow = true;
  t3_object_add(&scene->base, floor_);
  cube = t3_mesh_new(t3_box_geometry_new(1.5f, 1.5f, 1.5f, 1, 1, 1), t3_mesh_standard_material_new(0xf2ab1e));
  t3_object_set_position(cube, 0, 1, 0);
  cube->base.cast_shadow = true;
  t3_object_add(&scene->base, cube);
  t3_object_add(&scene->base, t3_hemisphere_light_new(0xece3d3, 0x15120f, 1.0f));
  t3_light *sun = t3_directional_light_new(0xffffff, 2.5f);
  t3_object_set_position(sun, 4, 10, 6);
  sun->base.cast_shadow = true;
  t3_object_add(&scene->base, sun);

  hud = t3_scene_new();
  hudcam = t3_orthographic_camera_new(-1, 1, 1, -1, 0, 10);
  t3_object_set_position(hudcam, 0, 0, 5);
  uint8_t *px = calloc(HW * HH, 4);
  hudtex = t3_data_texture_new(HW, HH, px);
  free(px);
  t3_material *hm = t3_mesh_basic_material_new(0xffffff);
  t3_material_set_map(hm, hudtex);
  hm->transparent = true; hm->depth_test = false; hm->depth_write = false;
  t3_mesh *quad = t3_mesh_new(t3_plane_geometry_new(2, 2, 1, 1), hm);
  t3_object_add(&hud->base, quad);
  WC_LOG("smoke: init ok");
}

__attribute__((export_name("wc_render")))
void wc_render(void) {
  float t = (float)tm.time_ms / 1000.0f;
  t3_object_set_rotation(cube, t * 0.7f, t, 0);
  uint8_t *p = hudtex->pixels;
  memset(p, 0, HW * HH * 4);
  for (int y = 10; y < 40; y++) for (int x = 10; x < 10 + (int)(fmodf(t, 4) * 100); x++) {
    uint8_t *q = p + (y * HW + x) * 4; q[0] = 0xec; q[1] = 0xe3; q[2] = 0xd3; q[3] = 255;
  }
  hudtex->version++;
  t3_renderer_set_auto_clear(R, true);
  t3_renderer_render(R, scene, cam);
  t3_renderer_set_auto_clear(R, false);
  if (pads[0].buttons & WC_BTN_A) t3_renderer_render(R, hud, hudcam);
  { const char *m = t3_renderer_generated_missing(R); if (m && *m && tm.frame % 60 == 0) wc_log(m, strlen(m)); }
}
