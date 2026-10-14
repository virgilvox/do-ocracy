/* Level smoke test: runs level_check (src/levelcheck.c) over all five levels
 * natively and fails on any finding. tools/accept.sh builds and runs it:
 *
 *   cc -std=gnu11 -O1 -I src -I third_party/three.c/include tools/smoke.c src/levels.c src/levelcheck.c -lm -o build/smoke
 *   build/smoke            # the checks; exit 1 on any error
 *   build/smoke --json     # every level's tables as JSON, for tools/gapcheck.py
 *
 * The pipeline smoke cart that used to live here is tools/pipeline-smoke.c. */
#include "game.h"
#include <stdio.h>
#include <string.h>

/* the globals levels.c writes in game_set_level */
game_t G;
station_t S[ST_COUNT];

static void out(const char *line) { puts(line); }

static void js(const char *s) {
  putchar('"');
  for (; s && *s; s++) { if (*s == '"' || *s == '\\') putchar('\\'); if (*s == '\n') { printf("\\n"); continue; } putchar(*s); }
  putchar('"');
}

static void box(const char *k, const box_t *b) { printf("\"%s\":[%g,%g,%g,%g],", k, b->x0, b->z0, b->x1, b->z1); }

static void dump(const level_t *lv) {
  printf("{\"name\":"); js(lv->name);
  printf(",\"door_x\":%g,\"door_hw\":%g,\"lobby\":[%g,%g],\"start\":[%g,%g],", lv->door_x, lv->door_hw, lv->lobby_x, lv->lobby_z, lv->start_x, lv->start_z);
  box("spot_box", &lv->spot_box); box("newbie_box", &lv->newbie_box); box("wander_box", &lv->wander_box); box("doormat_box", &lv->doormat_box);
  printf("\"st\":[");
  for (int i = 0; i < ST_COUNT; i++) {
    const st_place_t *s = &lv->st[i];
    printf("%s{\"id\":%d,\"label\":", i ? "," : "", i); js(s->label);
    printf(",\"x\":%g,\"z\":%g,\"hw\":%g,\"hd\":%g,\"face\":%d,\"present\":%d,\"wall\":%d}", s->x, s->z, s->hw, s->hd, s->face, s->present,
           i == ST_SWITCH || i == ST_BACKDOOR);
  }
  printf("],\"ob\":[");
  for (int i = 0; i < lv->ob_n; i++) {
    const obst_t *o = &lv->ob[i];
    printf("%s{\"kind\":%d,\"x\":%g,\"z\":%g,\"hw\":%g,\"hd\":%g}", i ? "," : "", o->kind, o->x, o->z, o->hw, o->hd);
  }
  printf("],\"props\":[");
  for (int i = 0; i < lv->prop_n; i++) {
    const prop_t *p = &lv->props[i];
    int disc = p->kind == PR_SHOPVAC || p->kind == PR_TOOLCART || p->kind == PR_WATER || p->kind == PR_BUCKET;
    printf("%s{\"kind\":%d,\"x\":%g,\"y\":%g,\"z\":%g,\"disc\":%d}", i ? "," : "", p->kind, p->x, p->y, p->z, disc);
  }
  printf("],\"signs\":[");
  for (int i = 0; i < lv->sign_n; i++) {
    const sign_t *s = &lv->signs[i];
    printf("%s{\"kind\":%d,\"text\":", i ? "," : "", s->kind); js(s->text);
    printf(",\"x\":%g,\"y\":%g,\"z\":%g,\"local\":%d}", s->x, s->y, s->z, s->kind == SG_GAS);
  }
  printf("],\"accents\":[");
  for (int i = 0; i < lv->accent_n; i++) {
    const accent_t *a = &lv->accents[i];
    printf("%s[%d,%d,%d,%d,%d,%d]", i ? "," : "", a->kind, a->c, a->r, a->cw, a->rh, a->ring);
  }
  printf("],\"stencils\":[");
  for (int i = 0; i < lv->stencil_n; i++) {
    const stencil_t *s = &lv->stencils[i];
    printf("%s{\"text\":", i ? "," : ""); js(s->text); printf(",\"x\":%g,\"z\":%g}", s->x, s->z);
  }
  printf("],\"enums\":{\"OBK_FENCE\":%d,\"OBK_COUNTER\":%d,\"T_WHITE\":%d,\"T_GREY\":%d,\"T_CHECK\":%d,\"FACE_NONE\":%d,\"FACE_NZ\":%d,\"FACE_PX\":%d,\"FACE_NX\":%d}}",
         OBK_FENCE, OBK_COUNTER, T_WHITE, T_GREY, T_CHECK, FACE_NONE, FACE_NZ, FACE_PX, FACE_NX);
}

int main(int argc, char **argv) {
  levels_init();
  if (argc > 1 && !strcmp(argv[1], "--json")) {
    printf("[");
    for (int k = 0; k < LEVEL_COUNT; k++) { if (k) printf(",\n"); dump(&LEVELS[k]); }
    printf("]\n");
    return 0;
  }
  int total = 0;
  for (int k = 0; k < LEVEL_COUNT; k++) total += level_check(&LEVELS[k], out);
  printf("smoke: %d finding%s over %d levels\n", total, total == 1 ? "" : "s", LEVEL_COUNT);
  return total ? 1 : 0;
}
