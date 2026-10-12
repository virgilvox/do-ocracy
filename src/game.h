/* DO-OCRACY: one Tuesday night of Hack Your Hackerspace at HeatSync Labs.
 *
 * Everyone scores CLOUT for glamour work (laser, printers, the bench) and
 * KARMA for the janitorial work that keeps the space open. The space itself
 * has two meters: CLUTTER (100 brings the fire marshal) and VIBE (0 means
 * everyone burned out). Survive until 11 PM. If the space closes, all clout
 * is void; karma stands. It is the commons dilemma with a laser cutter. */
#ifndef HSX_GAME_H
#define HSX_GAME_H
#include <stdint.h>
#include <stdbool.h>
#include "three.h"

#define MAX_PLAYERS 8
#define MAX_ITEMS 64
#define MAX_NPCS 8

enum { PH_TITLE, PH_LOBBY, PH_NIGHT, PH_RESULTS };
enum { BOT_NONE, BOT_BUILDER, BOT_TALKER, BOT_FOUNDER };
enum { END_NONE, END_SURVIVED, END_FIRE, END_BURNOUT };

/* buttons the game reads, merged from pads, keyboard and phones */
#define IN_A 1
#define IN_B 2
#define IN_X 4
#define IN_START 8

enum {
  IT_NONE,
  /* carried */
  IT_PLYWOOD, IT_PROJECT, IT_SCRAP, IT_BOTTLE, IT_SPAGHETTI, IT_DONATION,
  /* stuck to the floor, cleaned in place */
  IT_SAWDUST, IT_PUDDLE, IT_DAISY,
  IT_TYPES
};

enum {
  ST_LUMBER, ST_LASER, ST_PRINT1, ST_PRINT2, ST_CUTSALOT, ST_BENCH,
  ST_SINK, ST_FRIDGE, ST_TRASH, ST_FREEPILE, ST_SHOW, ST_COUCH, ST_TABLE, ST_COUNT
};

/* station states */
enum { S_IDLE, S_LOADED, S_BUSY, S_DONE, S_BROKEN };

typedef struct {
  bool alive;
  int type;
  float x, z, y;
  int holder;          /* player slot, or -1 on the floor */
  float age;           /* seconds on the floor */
  int value;           /* clout for a project */
  int name;            /* index into the project / donation tables */
  int kind;            /* project source: 0 laser 1 print 2 bench 3 sticker */
  bool heavy;
  t3_object *obj;
} item_t;

typedef struct {
  const char *label;
  float x, z, hw, hd;  /* centre and half extents on the floor */
  bool solid;
  int state;
  float timer, dust;
  int count;           /* dishes in the sink, prints done, ... */
  bool smell;          /* fridge */
  int proj_name;
  t3_object *obj;
  t3_mesh *light, *beam, *growth;
} station_t;

typedef struct {
  bool active, phone;
  int bot;             /* 0 human, else an archetype (BOT_*) */
  int goal, goal_ref;
  float think, stuck, side, lastx, lastz, talk_in;
  int pad;             /* local pad index, or -1 */
  char pid[25];        /* phone id */
  char name[14];
  uint32_t color;
  float x, z, face, speed_mul;
  float bob;
  int hold;            /* item index or -1 */
  float work;          /* progress on a hold-A task */
  int work_kind, work_ref;
  uint32_t btn, prev;
  float sx, sy;        /* stick, -1..1 */
  float seen;          /* seconds since a phone was last heard */
  float boost;         /* club-mate */
  bool owes_bottle;
  float bubble;        /* talker bubble time left */
  int bubble_text;
  float flash;         /* score popup */
  char pop[48];
  uint32_t pop_color;
  int clout, karma, proposals, greeted, mates, tasks;
  float idle;
  char prompt[48];
  float px, pz;        /* where the prompt points */
  t3_object *obj;
  t3_mesh *body, *carry_marker;
  float status_t;
} player_t;

typedef struct {
  bool alive;
  int state;           /* 0 entering, 1 waiting, 2 helping, 3 leaving, 4 sulking out */
  float x, z, tx, tz;
  float patience, life, work;
  int target;          /* item index */
  int hold;
  float bob;
  t3_object *obj;
  t3_mesh *body;
} npc_t;

typedef struct {
  int phase, end;
  float t;             /* seconds in this phase */
  float night_len;
  float clutter, vibe, fire_warn;
  float next_event, next_newbie, next_dish, next_donation;
  int event;           /* index into the event table, -1 none */
  float event_t, heat, landlord;
  char ticker[96];
  float ticker_t;
  uint32_t ticker_color;
  int toast_n;
  char room[8];
  char join_url[160];
  char relay[128];
  bool attract;
  float shake;
  int nights;
} game_t;

extern game_t G;
extern player_t P[MAX_PLAYERS];
extern item_t I[MAX_ITEMS];
extern npc_t N[MAX_NPCS];
extern station_t S[ST_COUNT];

uint32_t rnd(void);
float frnd(void);
void rnd_seed(uint32_t s);

#define TOASTS 5
extern char toast_text[TOASTS][96];
extern float toast_t[TOASTS];
extern uint32_t toast_col[TOASTS];
extern const uint32_t PCOL[MAX_PLAYERS];
int player_count(void);

/* game.c */
void game_init(void);
void game_update(float dt);
void game_player_input(int slot, uint32_t btn, float sx, float sy);
int  game_add_player(bool phone, int pad, const char *pid, const char *name);
void game_remove_player(int slot);
void game_start_night(void);
int  game_add_bot(int archetype);
void game_remove_bots(void);
int  human_count(void);
void game_reset_world(void);
void game_toast(const char *msg, uint32_t color);
const char *item_name(const item_t *it);

/* world.c: the 3D side */
void world_init(t3_scene *scene);
void world_sync(float dt);
t3_object *world_item_obj(int type, int kind);
t3_object *world_person(uint32_t color, bool newbie, t3_mesh **body);
void world_free(t3_object *o);
extern t3_camera *world_cam;

/* draw.c: the HUD side */
void draw_hud(float dt);
bool project(float x, float y, float z, int *sx, int *sy);

#endif
