#ifndef HSX_AUDIO_H
#define HSX_AUDIO_H
#include <stdint.h>

enum {
  SFX_PICKUP, SFX_DROP, SFX_TICK, SFX_GLAMOUR, SFX_KARMA, SFX_LASER, SFX_BAD,
  SFX_JOIN, SFX_ALARM, SFX_WIN, SFX_LOSE, SFX_TALK, SFX_SLURP, SFX_COUNT
};

void audio_play(int sfx);
/* music: 0 off, 1 lobby, 2 night, 3 closing time (faster) */
void audio_music(int mode);
/* fill the cart's stereo f32 ring with dt worth of samples */
void audio_render(float *ring, uint32_t cap, uint32_t *cursor, uint32_t rate, double dt_ms);

#endif
