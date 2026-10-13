#include "hud.h"
#include "wasmcart.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#include "qrcodegen.h"
#include "logo_mesh.h"
#include "glyphs.h"

uint8_t *hud_px;

typedef struct {
  stbtt_bakedchar ch[96];
  uint8_t *atlas;
  int aw, ah, px, ascent;
} font_t;
static font_t fonts[F_COUNT];

static uint8_t *load_asset(const char *name, int *len) {
  int n = wc_asset_size(name, (unsigned)strlen(name));
  if (n <= 0) return NULL;
  uint8_t *b = malloc((size_t)n);
  if (wc_load_asset(name, (unsigned)strlen(name), b, (unsigned)n) != n) { free(b); return NULL; }
  if (len) *len = n;
  return b;
}

static void bake(int id, const uint8_t *ttf, int px) {
  font_t *f = &fonts[id];
  f->px = px;
  f->aw = px >= 80 ? 2048 : 1024;
  f->ah = px >= 80 ? 1024 : px >= 40 ? 512 : 256;
  f->atlas = calloc((size_t)f->aw * f->ah, 1);
  stbtt_BakeFontBitmap(ttf, 0, (float)px, f->atlas, f->aw, f->ah, 32, 96, f->ch);
  stbtt_fontinfo fi;
  stbtt_InitFont(&fi, ttf, 0);
  int a, d, g;
  stbtt_GetFontVMetrics(&fi, &a, &d, &g);
  f->ascent = (int)(a * stbtt_ScaleForPixelHeight(&fi, (float)px) + 0.5f);
}

static uint16_t glyph_rows[GL_COUNT][16];

void hud_init(void) {
  hud_px = calloc(HUD_W * HUD_H, 4);
  for (int g = 0; g < GL_COUNT; g++) for (int r = 0; r < 16; r++) {
    uint16_t v = 0;
    for (int c = 0; c < 16 && GLYPH_SRC[g][r][c]; c++) if (GLYPH_SRC[g][r][c] == '#') v |= (uint16_t)(0x8000u >> c);
    glyph_rows[g][r] = v;
  }
  uint8_t *archivo = load_asset("ArchivoBlack-Regular.ttf", NULL);
  uint8_t *plex = load_asset("IBMPlexMono-SemiBold.ttf", NULL);
  uint8_t *plexb = load_asset("IBMPlexMono-Bold.ttf", NULL);
  uint8_t *charis = load_asset("CharisSIL-Bold.ttf", NULL);
  if (!archivo || !plex || !plexb || !charis) { WC_LOG("hud: missing font assets"); return; }
  bake(F_TITLE, archivo, 128);
  bake(F_H1, archivo, 64);
  bake(F_H2, archivo, 40);
  bake(F_H3, archivo, 26);
  bake(F_MONO_L, plexb, 30);
  bake(F_MONO, plex, 21);
  bake(F_MONO_S, plexb, 15);
  bake(F_LOGO, charis, 44);
  free(archivo); free(plex); free(plexb); free(charis);
}

void hud_clear(void) { memset(hud_px, 0, HUD_W * HUD_H * 4); }

/* the clip rect (x0, y0 inclusive, x1, y1 exclusive) and the global alpha multiplier */
static int clip_x0, clip_y0, clip_x1 = HUD_W, clip_y1 = HUD_H;
static int alpha_mul = 255;

void hud_set_alpha(int mul) { alpha_mul = mul < 0 ? 0 : mul > 255 ? 255 : mul; }

void hud_clip(int x, int y, int w, int h) {
  clip_x0 = x < 0 ? 0 : x; clip_y0 = y < 0 ? 0 : y;
  clip_x1 = x + w > HUD_W ? HUD_W : x + w; clip_y1 = y + h > HUD_H ? HUD_H : y + h;
  if (clip_x1 < clip_x0) clip_x1 = clip_x0;
  if (clip_y1 < clip_y0) clip_y1 = clip_y0;
}

void hud_clip_clear(void) { clip_x0 = 0; clip_y0 = 0; clip_x1 = HUD_W; clip_y1 = HUD_H; }

static inline void blend(int x, int y, uint32_t rgb, int a) {
  if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1 || a <= 0) return;
  if (alpha_mul < 255) { a = a * alpha_mul / 255; if (a <= 0) return; }
  uint8_t *p = hud_px + ((HUD_H - 1 - y) * HUD_W + x) * 4;
  int sr = (rgb >> 16) & 255, sg = (rgb >> 8) & 255, sb = rgb & 255;
  if (a >= 255 || p[3] == 0) { p[0] = sr; p[1] = sg; p[2] = sb; p[3] = a > 255 ? 255 : a; return; }
  int da = p[3] * (255 - a) / 255;
  int oa = a + da;
  p[0] = (sr * a + p[0] * da) / oa;
  p[1] = (sg * a + p[1] * da) / oa;
  p[2] = (sb * a + p[2] * da) / oa;
  p[3] = oa;
}

void hud_rect(int x, int y, int w, int h, uint32_t rgb, int a) {
  int x0 = x < clip_x0 ? clip_x0 : x, y0 = y < clip_y0 ? clip_y0 : y;
  int x1 = x + w > clip_x1 ? clip_x1 : x + w, y1 = y + h > clip_y1 ? clip_y1 : y + h;
  if (x0 >= x1 || y0 >= y1) return;
  if (a >= 255 && alpha_mul >= 255) {
    uint8_t c[4] = { (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255 };
    uint32_t v; memcpy(&v, c, 4);
    for (int yy = y0; yy < y1; yy++) {
      uint32_t *row = (uint32_t *)(hud_px + ((HUD_H - 1 - yy) * HUD_W) * 4);
      for (int xx = x0; xx < x1; xx++) row[xx] = v;
    }
    return;
  }
  for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) blend(xx, yy, rgb, a);
}

void hud_frame(int x, int y, int w, int h, int t, uint32_t rgb, int a) {
  hud_rect(x, y, w, t, rgb, a);
  hud_rect(x, y + h - t, w, t, rgb, a);
  hud_rect(x, y + t, t, h - 2 * t, rgb, a);
  hud_rect(x + w - t, y + t, t, h - 2 * t, rgb, a);
}

/* GANTRY card: 2px rule, solid 6px offset shadow, square corners */
void hud_panel(int x, int y, int w, int h, uint32_t bg, uint32_t border) {
  hud_rect(x + 6, y + 6, w, h, C_SHADOW, 255);
  hud_rect(x, y, w, h, bg, 255);
  hud_frame(x, y, w, h, 2, border, 255);
}

/* repeating-linear-gradient(-45deg, hazard 0 24px, tape-dark 24px 48px) */
void hud_tape(int x, int y, int w, int h, float phase) {
  int ph = (int)phase;
  for (int yy = y; yy < y + h; yy++) {
    if ((unsigned)yy >= HUD_H) continue;
    for (int xx = x; xx < x + w; xx++) {
      int k = ((xx + yy + ph) % 48 + 48) % 48;
      blend(xx, yy, k < 24 ? C_HAZARD : C_TAPE, 255);
    }
  }
  hud_frame(x, y, w, h, 2, C_TAPE, 255);
}

void hud_bar(int x, int y, int w, int h, float frac, uint32_t fill, uint32_t bg) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  hud_rect(x, y, w, h, bg, 255);
  hud_rect(x + 2, y + 2, (int)((w - 4) * frac), h - 4, fill, 255);
}

int hud_font_h(int font) { return fonts[font].px; }

int hud_text_w(int font, const char *s) {
  font_t *f = &fonts[font];
  if (!f->atlas) return 0;
  float w = 0;
  for (; *s; s++) {
    int c = (unsigned char)*s;
    if (c < 32 || c > 127) c = '?';
    w += f->ch[c - 32].xadvance;
  }
  return (int)(w + 0.5f);
}

/* y is the top of the line box (cap height starts about a third down) */
int hud_text(int font, int x, int y, const char *s, uint32_t rgb, int a, int align) {
  font_t *f = &fonts[font];
  if (!f->atlas) return 0;
  int w = hud_text_w(font, s);
  float cx = (float)(align == AL_CENTER ? x - w / 2 : align == AL_RIGHT ? x - w : x);
  float cy = (float)(y + f->ascent);
  for (; *s; s++) {
    int c = (unsigned char)*s;
    if (c < 32 || c > 127) c = '?';
    stbtt_bakedchar *b = &f->ch[c - 32];
    int gx = (int)floorf(cx + b->xoff + 0.5f), gy = (int)floorf(cy + b->yoff + 0.5f);
    for (int yy = b->y0; yy < b->y1; yy++) {
      const uint8_t *row = f->atlas + yy * f->aw;
      int py = gy + yy - b->y0;
      if ((unsigned)py >= HUD_H) continue;
      for (int xx = b->x0; xx < b->x1; xx++) {
        int cov = row[xx];
        if (cov) blend(gx + xx - b->x0, py, rgb, cov * a / 255);
      }
    }
    cx += b->xadvance;
  }
  return w;
}

int hud_text_wrap(int font, int x, int y, int w, const char *s, uint32_t rgb, int a, int lh) {
  char line[256], word[128];
  int ln = 0, used = 0;
  line[0] = 0;
  while (*s) {
    int wn = 0;
    while (*s == ' ') s++;
    while (*s && *s != ' ' && wn < 126) word[wn++] = *s++;
    word[wn] = 0;
    if (!wn) break;
    char trial[384];
    snprintf(trial, sizeof trial, ln ? "%s %s" : "%s%s", line, word);
    if (ln && hud_text_w(font, trial) > w) {
      hud_text(font, x, y + used, line, rgb, a, AL_LEFT);
      used += lh;
      snprintf(line, sizeof line, "%s", word);
    } else {
      snprintf(line, sizeof line, "%s", trial);
    }
    ln = (int)strlen(line);
  }
  if (ln) { hud_text(font, x, y + used, line, rgb, a, AL_LEFT); used += lh; }
  return used;
}

void hud_circle(int cx, int cy, int r, uint32_t rgb, int a) {
  for (int y = -r - 1; y <= r + 1; y++) for (int x = -r - 1; x <= r + 1; x++) {
    float d = sqrtf((float)(x * x + y * y)) - r;
    if (d > 0.5f) continue;
    float cov = d < -0.5f ? 1.f : 0.5f - d;
    blend(cx + x, cy + y, rgb, (int)(a * cov));
  }
}

/* anti-aliased thick line: distance to the segment, per pixel in its box */
void hud_line(float x0, float y0, float x1, float y1, float t, uint32_t rgb, int a) {
  float r = t * 0.5f;
  int bx0 = (int)floorf(fminf(x0, x1) - r - 1), bx1 = (int)ceilf(fmaxf(x0, x1) + r + 1);
  int by0 = (int)floorf(fminf(y0, y1) - r - 1), by1 = (int)ceilf(fmaxf(y0, y1) + r + 1);
  if (bx0 < 0) bx0 = 0;
  if (by0 < 0) by0 = 0;
  if (bx1 > HUD_W - 1) bx1 = HUD_W - 1;
  if (by1 > HUD_H - 1) by1 = HUD_H - 1;
  float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
  for (int y = by0; y <= by1; y++) for (int x = bx0; x <= bx1; x++) {
    float px = x + 0.5f - x0, py = y + 0.5f - y0;
    float u = l2 > 0 ? (px * dx + py * dy) / l2 : 0;
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    float ex = px - u * dx, ey = py - u * dy;
    float d = sqrtf(ex * ex + ey * ey) - r;
    if (d > 0.5f) continue;
    float cov = d < -0.5f ? 1.f : 0.5f - d;
    blend(x, y, rgb, (int)(a * cov));
  }
}

void hud_chevron(float x, float y, float dx, float dy, float size, float t, uint32_t rgb, int a) {
  float l = sqrtf(dx * dx + dy * dy);
  if (l < 1e-4f) return;
  dx /= l; dy /= l;
  float tipx = x + dx * size * 0.5f, tipy = y + dy * size * 0.5f;
  float bx = x - dx * size * 0.5f, by = y - dy * size * 0.5f;
  float nx = -dy * size * 0.6f, ny = dx * size * 0.6f;
  hud_line(bx + nx, by + ny, tipx, tipy, t, rgb, a);
  hud_line(bx - nx, by - ny, tipx, tipy, t, rgb, a);
}

void hud_ring(int cx, int cy, int r, int t, float frac, uint32_t rgb, int a) {
  float lim = frac * 6.2831853f;
  for (int y = -r - 1; y <= r + 1; y++) for (int x = -r - 1; x <= r + 1; x++) {
    float d = sqrtf((float)(x * x + y * y));
    if (d > r + 0.5f || d < r - t - 0.5f) continue;
    float ang = atan2f((float)x, (float)-y);
    if (ang < 0) ang += 6.2831853f;
    if (ang > lim) continue;
    float cov = 1.f;
    if (d > r - 0.5f) cov = r + 0.5f - d;
    else if (d < r - t + 0.5f) cov = d - (r - t - 0.5f);
    blend(cx + x, cy + y, rgb, (int)(a * cov));
  }
}

/* ── the HeatSync mark ───────────────────────────────────────────── */

/* even-odd scanline fill into a coverage grid (ss x ss supersampled) */
static void fill_rings(uint8_t *cov, int size, const float *pts, const int *ring, int nrings) {
  const int SS = 4;
  int gs = size * SS;
  float scale = (float)gs / 1000.f;
  float *xs = malloc(sizeof(float) * 4096);
  for (int sy = 0; sy < gs; sy++) {
    float y = (sy + 0.5f) / scale;
    int n = 0;
    for (int r = 0; r < nrings; r++) {
      int a0 = ring[r], a1 = ring[r + 1];
      for (int i = a0; i < a1; i++) {
        int j = i + 1 < a1 ? i + 1 : a0;
        float x0 = pts[i * 2], y0 = pts[i * 2 + 1], x1 = pts[j * 2], y1 = pts[j * 2 + 1];
        if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
          if (n < 4096) xs[n++] = x0 + (y - y0) / (y1 - y0) * (x1 - x0);
        }
      }
    }
    for (int i = 1; i < n; i++) { float v = xs[i]; int k = i - 1; while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; } xs[k + 1] = v; }
    for (int i = 0; i + 1 < n; i += 2) {
      int xa = (int)(xs[i] * scale + 0.5f), xb = (int)(xs[i + 1] * scale + 0.5f);
      if (xa < 0) xa = 0;
      if (xb > gs) xb = gs;
      for (int sx = xa; sx < xb; sx++) cov[(sy / SS) * size + sx / SS]++;
    }
  }
  free(xs);
}

typedef struct { int size; uint32_t flame; uint8_t *sun, *ink; } logo_cache_t;
static logo_cache_t lcache[6];

static logo_cache_t *logo_get(int size, uint32_t flame) {
  for (int i = 0; i < 6; i++) if (lcache[i].sun && lcache[i].size == size && lcache[i].flame == flame) return &lcache[i];
  int slot = 0;
  for (int i = 0; i < 6; i++) if (!lcache[i].sun) { slot = i; break; }
  logo_cache_t *c = &lcache[slot];
  free(c->sun); free(c->ink);
  c->size = size; c->flame = flame;
  c->sun = calloc((size_t)size * size, 1);
  c->ink = calloc((size_t)size * size, 1);
  fill_rings(c->sun, size, logo_sun_pts, logo_sun_ring, LOGO_SUN_RINGS);
  fill_rings(c->ink, size, logo_flame_pts, logo_flame_ring, LOGO_FLAME_RINGS);
  return c;
}

void hud_logo(int x, int y, int size, uint32_t flame_rgb) {
  logo_cache_t *c = logo_get(size, flame_rgb);
  for (int yy = 0; yy < size; yy++) for (int xx = 0; xx < size; xx++) {
    int i = yy * size + xx;
    if (c->sun[i]) blend(x + xx, y + yy, C_LOGO_SUN, c->sun[i] * 255 / 16);
    if (c->ink[i]) blend(x + xx, y + yy, flame_rgb, c->ink[i] * 255 / 16);
  }
}

uint8_t *hud_logo_image(int size, uint32_t flame_rgb, uint32_t bg_rgb, int bg_a) {
  logo_cache_t *c = logo_get(size, flame_rgb);
  uint8_t *img = malloc((size_t)size * size * 4);
  for (int i = 0; i < size * size; i++) {
    float r = ((bg_rgb >> 16) & 255) / 255.f, g = ((bg_rgb >> 8) & 255) / 255.f, b = (bg_rgb & 255) / 255.f;
    float a = bg_a / 255.f;
    float s = c->sun[i] / 16.f, k = c->ink[i] / 16.f;
    r = r * (1 - s) + (C_LOGO_SUN >> 16 & 255) / 255.f * s; g = g * (1 - s) + (C_LOGO_SUN >> 8 & 255) / 255.f * s;
    b = b * (1 - s) + (C_LOGO_SUN & 255) / 255.f * s; a = a + s * (1 - a);
    r = r * (1 - k) + (flame_rgb >> 16 & 255) / 255.f * k; g = g * (1 - k) + (flame_rgb >> 8 & 255) / 255.f * k;
    b = b * (1 - k) + (flame_rgb & 255) / 255.f * k; a = a + k * (1 - a);
    img[i * 4] = (uint8_t)(r * 255); img[i * 4 + 1] = (uint8_t)(g * 255);
    img[i * 4 + 2] = (uint8_t)(b * 255); img[i * 4 + 3] = (uint8_t)(a * 255);
  }
  return img;
}

/* ── QR ──────────────────────────────────────────────────────────── */

static char qr_text[256];
static uint8_t qr_buf[qrcodegen_BUFFER_LEN_MAX];
static bool qr_ok;

void hud_qr(int x, int y, int size, const char *text) {
  if (strcmp(text, qr_text) != 0) {
    snprintf(qr_text, sizeof qr_text, "%s", text);
    uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];
    qr_ok = qrcodegen_encodeText(text, tmp, qr_buf, qrcodegen_Ecc_MEDIUM, 1, 15, qrcodegen_Mask_AUTO, true);
  }
  if (!qr_ok) return;
  int n = qrcodegen_getSize(qr_buf);
  int m = size / (n + 4);
  if (m < 1) m = 1;
  int total = m * (n + 4);
  hud_rect(x, y, total, total, C_BONE, 255);
  for (int j = 0; j < n; j++) for (int i = 0; i < n; i++)
    if (qrcodegen_getModule(qr_buf, i, j)) hud_rect(x + (i + 2) * m, y + (j + 2) * m, m, m, C_CHAR, 255);
}

/* ── front end helpers (FINAL-frontend.md section 5) ─────────────── */

int hud_text_sh(int font, int x, int y, const char *s, uint32_t rgb, int a, int align) {
  hud_text(font, x + 3, y + 3, s, C_SHADOW, a * 4 / 5, align);
  return hud_text(font, x, y, s, rgb, a, align);
}

int hud_text_runs(int font, int x, int y, const run_t *runs, int a, int align) {
  int w = 0;
  for (const run_t *r = runs; r->s; r++) w += hud_text_w(font, r->s);
  int cx = align == AL_CENTER ? x - w / 2 : align == AL_RIGHT ? x - w : x;
  for (const run_t *r = runs; r->s; r++) {
    hud_text_sh(font, cx, y, r->s, r->c, a, AL_LEFT);
    cx += hud_text_w(font, r->s);
  }
  return w;
}

void hud_eyebrow(int x, int y, const char *s, uint32_t c, int align) {
  char buf[160];
  int n = 0;
  for (const char *p = s; *p && n < 150; p++) { buf[n++] = *p; if (p[1]) buf[n++] = ' '; }
  buf[n] = 0;
  hud_text(F_MONO_S, x, y, buf, c, 255, align);
}

int hud_keycap(int x, int y, const char *k, uint32_t fill, uint32_t ink) {
  int w = hud_text_w(F_MONO_S, k) + 14;
  if (w < 24) w = 24;
  hud_rect(x + 2, y + 2, w, 24, C_SHADOW, 255);
  hud_rect(x, y, w, 24, fill, 255);
  hud_text(F_MONO_S, x + w / 2, y + 3, k, ink, 255, AL_CENTER);
  return w;
}

static float clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }

float hud_ease_out(float t) { t = clamp01(t); float u = 1 - t; return 1 - u * u * u; }

float hud_ease_inout(float t) {
  t = clamp01(t);
  if (t < 0.5f) return 4 * t * t * t;
  float u = -2 * t + 2;
  return 1 - u * u * u / 2;
}

float hud_ease_back(float t) {
  t = clamp01(t);
  float u = t - 1;
  return 1 + 2.70158f * u * u * u + 1.70158f * u * u;
}

void hud_tween(float *cur, float target, float speed, float dt) {
  *cur += (target - *cur) * (1 - expf(-speed * dt));
  if (fabsf(target - *cur) < 0.5f) *cur = target;
}

void hud_glyph(int x, int y, int id, int s, uint32_t rgb, int a) {
  if (id < 0 || id >= GL_COUNT) return;
  if (s < 1) s = 1;
  for (int r = 0; r < 16; r++) {
    uint16_t v = glyph_rows[id][r];
    if (!v) continue;
    for (int c = 0; c < 16; c++) {
      if (!(v & (0x8000u >> c))) continue;
      if (s == 1) blend(x + c, y + r, rgb, a);
      else hud_rect(x + c * s, y + r * s, s, s, rgb, a);
    }
  }
}

void hud_steps(int cx, int y, int n, int step, float frac) {
  if (n <= 0) return;
  const int dot = 10, gap = 12, bar = 60;
  int cur = step >= 0 && step < n;
  int w = n * dot + (n - 1) * gap + (cur ? bar - dot : 0);
  int x = cx - w / 2;
  for (int k = 0; k < n; k++) {
    if (k < step) hud_rect(x, y, dot, dot, C_STEEL_HI, 255);
    else if (k == step) {
      hud_rect(x, y, bar, dot, C_STEEL, 255);
      hud_rect(x, y, (int)(bar * clamp01(frac)), dot, C_HAZARD, 255);
      x += bar + gap;
      continue;
    } else hud_frame(x, y, dot, dot, 2, C_STEEL, 255);
    x += dot + gap;
  }
}

/* a person seen from the front: x is the centre, y the top of the head */
void hud_pict_person(int x, int y, int h, uint32_t body, bool newbie) {
  if (newbie) body = 0x8b8578;
  int r = h / 6;
  if (r < 2) r = 2;
  hud_circle(x, y + r, r, body, 255);
  int bw = h / 3, by = y + 2 * r + 2;
  hud_rect(x - bw / 2, by, bw, y + h - by, body, 255);
}

void hud_pict_station(int x, int y, int w, int h, const char *label, uint32_t edge) {
  hud_rect(x + 4, y + 4, w, h, C_SHADOW, 255);
  hud_rect(x, y, w, h, C_CHAR, 255);
  hud_frame(x, y, w, h, 4, edge, 255);
  if (label && *label) hud_text(F_MONO_S, x + w / 2, y + h / 2 - hud_font_h(F_MONO_S) / 2 - 2, label, C_BONE, 255, AL_CENTER);
}

/* x, y is the item's centre */
void hud_pict_item(int x, int y, int kind, uint32_t c) {
  switch (kind) {
  case 0: hud_rect(x - 14 + 2, y - 9 + 2, 28, 18, C_SHADOW, 255); hud_rect(x - 14, y - 9, 28, 18, c, 255); break;
  case 1: hud_rect(x - 11 + 2, y - 11 + 2, 22, 22, C_SHADOW, 255); hud_rect(x - 11, y - 11, 22, 22, c, 255); break;
  case 2: hud_rect(x - 8 + 2, y - 8 + 2, 16, 16, C_SHADOW, 255); hud_rect(x - 8, y - 8, 16, 16, c, 255); break;
  default:
    for (int i = 0; i < 4; i++) {
      float x0 = x - 20 + i * 10.f, x1 = x0 + 10;
      hud_line(x0, (float)(i & 1 ? y - 6 : y + 6), x1, (float)(i & 1 ? y + 6 : y - 6), 3, c, 255);
    }
    break;
  }
}

/* ── screen space particles ──────────────────────────────────────── */

typedef struct { float x, y, vx, vy, life, max, size; uint32_t c; int kind; } fx2_t;
#define FX2_MAX 240
static fx2_t fx2[FX2_MAX];
static int fx2_next;
static uint32_t fx2_seed = 0x2f6b9e1u;   /* its own LCG: never touches the sim's random stream */

static float fx2_rnd(void) { fx2_seed = fx2_seed * 1664525u + 1013904223u; return (fx2_seed >> 8) / 16777216.f; }

static fx2_t *fx2_new(void) { fx2_t *f = &fx2[fx2_next]; fx2_next = (fx2_next + 1) % FX2_MAX; return f; }

void fx2_burst(int kind, float x, float y, uint32_t c) {
  static const uint32_t pal[] = { C_HAZARD, C_LIVE, C_VIOLET, C_INFO, C_RUST, C_BONE };
  if (kind == FX2_SPARK) {
    for (int i = 0; i < 12; i++) {
      fx2_t *f = fx2_new();
      float ang = fx2_rnd() * 6.2831853f, sp = 180 + fx2_rnd() * 240;
      *f = (fx2_t){ x, y, cosf(ang) * sp, sinf(ang) * sp, 0, 0.35f + fx2_rnd() * 0.15f, 3, c, kind };
      f->life = f->max;
    }
  } else if (kind == FX2_CONFETTI) {
    for (int i = 0; i < 60; i++) {
      fx2_t *f = fx2_new();
      *f = (fx2_t){ x + (fx2_rnd() - 0.5f) * 400, y, (fx2_rnd() - 0.5f) * 120, 80 + fx2_rnd() * 140,
                    0, 2.0f + fx2_rnd() * 0.6f, (float)(i & 1 ? 6 : 4), pal[i % 6], kind };
      f->life = f->max;
    }
  } else {
    for (int i = 0; i < 16; i++) {
      fx2_t *f = fx2_new();
      float ang = i * 6.2831853f / 16;
      *f = (fx2_t){ x, y, cosf(ang) * 260, sinf(ang) * 260, 0, 0.4f, 4, c, kind };
      f->life = f->max;
    }
  }
}

void fx2_draw(float dt) {
  for (int i = 0; i < FX2_MAX; i++) {
    fx2_t *f = &fx2[i];
    if (f->life <= 0) continue;
    f->life -= dt;
    if (f->life <= 0) continue;
    if (f->kind == FX2_SPARK) f->vy += 900 * dt;
    else if (f->kind == FX2_CONFETTI) {
      f->vy += 300 * dt;
      float d = expf(-1.5f * dt);
      f->vx *= d; f->vy *= d;
    }
    f->x += f->vx * dt; f->y += f->vy * dt;
    float k = f->life / f->max;
    int a = k < 0.3f ? (int)(255 * k / 0.3f) : 255;
    int sz = (int)f->size;
    if (f->kind == FX2_CONFETTI) hud_rect((int)f->x, (int)f->y, sz, 3, f->c, a);
    else hud_rect((int)f->x - sz / 2, (int)f->y - sz / 2, sz, sz, f->c, a);
  }
}
