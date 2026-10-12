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
void hud_ring(int cx, int cy, int r, int t, float frac, uint32_t rgb, int a);
/* the HeatSync mark: two colours, untouched (sun disc, then flame and arrow) */
void hud_logo(int x, int y, int size, uint32_t flame_rgb);
/* a QR code with its quiet zone, light modules bone, dark char */
void hud_qr(int x, int y, int size, const char *text);
/* RGBA image of the mark for a 3D sign (caller frees) */
uint8_t *hud_logo_image(int size, uint32_t flame_rgb, uint32_t bg_rgb, int bg_a);

#endif
