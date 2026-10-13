/* The 2D layer: a 1280x720 RGBA buffer drawn over the 3D space every frame,
 * styled after HeatSync's GANTRY theme (char and bone, one amber accent,
 * square corners, solid offset shadows, caution tape between things). */
#ifndef HSX_HUD_H
#define HSX_HUD_H
#include <stdint.h>
#include <stdbool.h>

#define HUD_W 1280
#define HUD_H 720

/* GANTRY tokens (new-hsl/src/styles/tokens.css) */
#define C_CHAR      0x15120f
#define C_GRIME     0x1d1915
#define C_SLAB      0x26211b
#define C_SLAB_HI   0x2f2920
#define C_STEEL     0x3a342b
#define C_STEEL_HI  0x4a4338
#define C_BONE      0xece3d3
#define C_ASH       0xa79d8c
#define C_SMOKE     0x756c5e
#define C_HAZARD    0xf2ab1e
#define C_HAZARD_DP 0xc98714
#define C_RUST      0xc2542c
#define C_LIVE      0x5fb892
#define C_FAULT     0xd24f3a
#define C_VIOLET    0x9b7fd0
#define C_INFO      0x5e93c4
#define C_TAPE      0x14110d
#define C_SHADOW    0x0a0806
#define C_INK_OK    0x7fd0aa
#define C_INK_ERR   0xef7b68
#define C_LOGO_SUN  0xf99a1c   /* the mark's own orange, never the UI amber */

enum { F_TITLE, F_H1, F_H2, F_H3, F_MONO_L, F_MONO, F_MONO_S, F_LOGO, F_COUNT };
enum { AL_LEFT, AL_CENTER, AL_RIGHT };

extern uint8_t *hud_px;     /* RGBA, rows stored bottom up (GL texture order) */

void hud_init(void);
void hud_clear(void);
void hud_rect(int x, int y, int w, int h, uint32_t rgb, int a);
void hud_frame(int x, int y, int w, int h, int t, uint32_t rgb, int a);
void hud_panel(int x, int y, int w, int h, uint32_t bg, uint32_t border);
void hud_tape(int x, int y, int w, int h, float phase);
void hud_bar(int x, int y, int w, int h, float frac, uint32_t fill, uint32_t bg);
int  hud_text(int font, int x, int y, const char *s, uint32_t rgb, int a, int align);
int  hud_text_w(int font, const char *s);
int  hud_font_h(int font);
/* text wrapped to a width; returns the height used */
int  hud_text_wrap(int font, int x, int y, int w, const char *s, uint32_t rgb, int a, int lh);
void hud_circle(int cx, int cy, int r, uint32_t rgb, int a);
void hud_line(float x0, float y0, float x1, float y1, float t, uint32_t rgb, int a);
/* a chevron pointing along (dx, dy), centred on (x, y) */
void hud_chevron(float x, float y, float dx, float dy, float size, float t, uint32_t rgb, int a);
void hud_ring(int cx, int cy, int r, int t, float frac, uint32_t rgb, int a);
/* the HeatSync mark: two colours, untouched (sun disc, then flame and arrow) */
void hud_logo(int x, int y, int size, uint32_t flame_rgb);
/* a QR code with its quiet zone, light modules bone, dark char */
void hud_qr(int x, int y, int size, const char *text);
/* RGBA image of the mark for a 3D sign (caller frees) */
uint8_t *hud_logo_image(int size, uint32_t flame_rgb, uint32_t bg_rgb, int bg_a);

/* a coloured run of text; a caption is an array of runs, terminated by s == NULL */
typedef struct { const char *s; uint32_t c; } run_t;

/* text with a 3 px solid shadow in C_SHADOW at 80 percent of a, then the text */
int  hud_text_sh(int font, int x, int y, const char *s, uint32_t rgb, int a, int align);
/* runs drawn left to right; total width measured with hud_text_w for align; shadowed */
int  hud_text_runs(int font, int x, int y, const run_t *runs, int a, int align);

/* global alpha multiplier applied inside blend(): a = a * mul / 255; default 255 */
void hud_set_alpha(int mul);

/* clip rect applied inside blend() and the hud_rect fast path; hud_clip_clear resets to the screen */
void hud_clip(int x, int y, int w, int h);
void hud_clip_clear(void);

/* a GANTRY eyebrow: tracked mono caps */
void hud_eyebrow(int x, int y, const char *s, uint32_t c, int align);
/* a key cap: the button name in a box, sized to fit; returns the width */
int  hud_keycap(int x, int y, const char *k, uint32_t fill, uint32_t ink);

/* easing: t in 0..1 */
float hud_ease_out(float t);      /* 1 - (1-t)^3 */
float hud_ease_inout(float t);    /* t<.5 ? 4t^3 : 1 - (-2t+2)^3 / 2 */
float hud_ease_back(float t);     /* overshoots to about 1.1 */
/* exponential tween: cur += (target - cur) * (1 - exp(-speed * dt)); snaps when within 0.5 */
void  hud_tween(float *cur, float target, float speed, float dt);

/* 16x16 one-bit glyphs drawn at integer scale s (16*s px square) */
enum { GL_CLOUT, GL_KARMA, GL_PAD, GL_KEYS, GL_PHONE, GL_BOT, GL_MARSHAL, GL_HEART,
       GL_BROOM, GL_HAND, GL_CHECK, GL_NOTE, GL_SPEAKER, GL_COUNT };
void hud_glyph(int x, int y, int id, int s, uint32_t rgb, int a);

/* a row of step dots: n dots, 10 px, pitch 22, centred on cx; dot k < step filled C_STEEL_HI,
   k == step is a 60 px bar filling by frac in C_HAZARD, k > step hollow 2 px C_STEEL */
void hud_steps(int cx, int y, int n, int step, float frac);

/* pictograms for the rules flash and the mode cards (screen space, px) */
void hud_pict_person(int x, int y, int h, uint32_t body, bool newbie);
void hud_pict_station(int x, int y, int w, int h, const char *label, uint32_t edge);
void hud_pict_item(int x, int y, int kind, uint32_t c);   /* 0 plywood, 1 project, 2 scrap, 3 daisy */

/* screen space particles, separate from the world FX in draw.c */
enum { FX2_SPARK, FX2_CONFETTI, FX2_RING };
void fx2_burst(int kind, float x, float y, uint32_t c);
void fx2_draw(float dt);

#endif
