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

void hud_init(void) {
  hud_px = calloc(HUD_W * HUD_H, 4);
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

static inline void blend(int x, int y, uint32_t rgb, int a) {
  if ((unsigned)x >= HUD_W || (unsigned)y >= HUD_H || a <= 0) return;
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
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = x + w > HUD_W ? HUD_W : x + w, y1 = y + h > HUD_H ? HUD_H : y + h;
  if (x0 >= x1 || y0 >= y1) return;
  if (a >= 255) {
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
