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

/* ── the front end: seats, settings, menus (FINAL-frontend.md 3, 4, 8.1) ── */
#define SEAT_PADS 4
#define SEAT_KBS 2
#define SEAT_PHONES MAX_PLAYERS
#define SEAT_COUNT (SEAT_PADS + SEAT_KBS + SEAT_PHONES)   /* 14 */
#define SEAT_PAD(i)   (i)          /* 0..3 */
#define SEAT_KB(k)    (4 + (k))    /* 4, 5 */
#define SEAT_PHONE(i) (6 + (i))    /* 6..13 */

enum { SCR_TITLE, SCR_ATTRACT, SCR_MAIN, SCR_MODES, SCR_SETTINGS, SCR_FLASH, SCR_CREDITS, SCR_NONE };
enum { TO_MAIN, TO_LOBBY };            /* M.settings_then: where DONE goes */
enum { MODE_SOLO, MODE_COOP, MODE_VERSUS, MODE_CHAOS, MODE_CUSTOM, MODE_COUNT };
enum { CHAOS_CALM, CHAOS_NORMAL, CHAOS_CHAOS };
enum { HELP_OFF, HELP_FIRST, HELP_ALWAYS };
enum { NAV_UP = 1, NAV_DOWN = 2, NAV_LEFT = 4, NAV_RIGHT = 8, NAV_A = 16, NAV_B = 32, NAV_START = 64 };

typedef struct {
  uint8_t mode;        /* MODE_* */
  uint8_t night_len;   /* index into NIGHT_LENS: 0 SHORT 120, 1 NORMAL 180, 2 LONG 300, 3 MARATHON 600 */
  uint8_t bots;        /* 1 on: fill when exactly one human is seated */
  uint8_t chaos;       /* CHAOS_* */
  uint8_t phones;      /* 1 on */
  uint8_t music, sfx;  /* 0..10 */
  uint8_t layout;      /* the ROOM row: index into LEVELS[] */
  uint8_t help;        /* HELP_* */
  uint8_t pad_[7];
} settings_t;          /* 16 bytes */
extern settings_t SET;  /* defined in game.c, written by main.c */

typedef struct {
  int screen, prev_screen;
  int owner;            /* seat id or -1 */
  int row, col;         /* cursor */
  float cur_y, snap_t;  /* eased cursor and the overshoot clock */
  float lift[MODE_COUNT];
  float val_slide; int val_dir;   /* settings and PLAY value nudge */
  float t;              /* seconds in this screen */
  float trans;          /* -1..0 outgoing, 0..1 incoming, 1 idle */
  float idle;           /* seconds since the owner's last nav */
  float title_idle;     /* seconds with no input at all on SCR_TITLE */
  int settings_then;    /* TO_MAIN or TO_LOBBY */
  int flash_step; float flash_t, flash_wipe; bool flash_fired; int flash_return;
  float deny_t; char deny[48];
  int pause_row; int pauser;      /* seat */
  float cam_blend;      /* 0 orbit, 1 play */
  bool kb_seen;
  int pending_count;    /* mirrored from the seats each frame for draw.c */
  int pending_seat[SEAT_COUNT];   /* seat ids in order, for the AT THE DOOR chips */
  /* mirrored by main.c each frame for draw.c */
  char seat_name[SEAT_COUNT][14];
  int cur_dir;          /* the last cursor move, -1 up, 1 down, for the overshoot */
  float sel_t;          /* the selected row flashes bone */
  float status_t; char status[48];   /* lobby status line (WAITING ON ...), not a deny */
  float lobby_t, ready_t;            /* lobby auto start (25 down) and the ready countdown (0 up to 3) */
  float hold_b[MAX_PLAYERS];         /* by slot: the hold-B leave ring, 0..1 */
  bool tape_held;       /* the lobby tape holds still */
} menu_t;
extern menu_t M;       /* defined in game.c beside G, driven by main.c, drawn by draw.c */

enum {
  IT_NONE,
  /* carried */
  IT_PLYWOOD, IT_PROJECT, IT_SCRAP, IT_BOTTLE, IT_SPAGHETTI, IT_DONATION,
  /* stuck to the floor, cleaned in place */
  IT_SAWDUST, IT_PUDDLE, IT_DAISY,
  IT_TYPES
};

/* Station ids. The order is fixed per build: draw.c station_labels, world.c
 * world_sync and the destination tables in game.c switch on these, and the
 * loops ST_PRINT1 <= k <= ST_PRINT2 (game.c, draw.c, world.c) mean nothing
 * may ever go between the two printers. New ids go after ST_TABLE. */
enum {
  ST_LUMBER, ST_LASER, ST_PRINT1, ST_PRINT2, ST_CUTSALOT, ST_BENCH,
  ST_SINK, ST_FRIDGE, ST_TRASH, ST_FREEPILE, ST_SHOW, ST_COUCH, ST_TABLE,
  ST_VEND, ST_BOARD, ST_SEWING, ST_BATH, ST_STORAGE, ST_FORGE, ST_LIBRARY, ST_BACKDOOR, ST_SWITCH, ST_BIKE,
  ST_COUNT
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
  float ry;            /* the model group's rotation; collision boxes are given already rotated */
  uint8_t face;        /* FACE_*: the side bots and newbies approach from */
  bool present;        /* false: parked at y -20 by world.c, skipped by collide and every action */
  bool always_label;   /* the HUD name draws even when nothing is happening */
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
  float stuck_total, escape, esc_x, esc_z;
  float wp_x, wp_z, path_t;   /* the next waypoint on the planned path and when to replan */
  int wp_goal, wp_ref;
  bool has_wp;
  int pad;             /* local pad index, or -1 */
  int kb;              /* keyboard seat (0, 1), or -1 */
  bool has_goal;       /* carrying something: where it goes */
  float goal_x, goal_z;
  int goal_st;
  const char *goal_label;
  char pid[25];        /* phone id */
  char name[14];
  uint32_t color;
  float x, z, face, speed_mul;
  float bob;
  int hold;            /* item index or -1 */
  float work;          /* progress on a hold-A task */
  float work_need;     /* how long that task takes */
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
  /* per night counters for the new stations and their awards */
  int sewn, notices, wiped, baths, welds, flats, shelved, fixes, unplugged, dishes;
  float read_t, read_cool;   /* the library passive: standing still by the books */
  float idle;
  char prompt[48];
  float px, pz;        /* where the prompt points */
  t3_object *obj;
  t3_mesh *body, *carry_marker;
  float status_t;
  bool ready;          /* lobby */
  int seat;            /* seat id, for glyphs on cards and results; -1 for bots */
} player_t;

typedef struct {
  bool alive;
  int state;           /* 0 entering, 1 waiting, 2 helping, 3 leaving, 4 sulking out */
  float x, z, tx, tz;
  float patience, life, work;
  int target;          /* item index */
  int hold;
  int bike;            /* rack slot 0..3, or -1 when they walked */
  float path_t, wp_x, wp_z, wp_tx, wp_tz;   /* planned path: next waypoint, the target it was planned for */
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
  float laser_cutting;  /* seconds left of the cutting glow: someone held A on the laser this frame */
  char ticker[96];
  char ticker_sub[96];
  float ticker_t;
  uint32_t ticker_color;
  int toast_n;
  char room[8];
  char join_url[160];
  char relay[128];
  bool attract;
  float shake;
  int nights;
  /* the level and the timers the new stations and events add */
  int level;
  float sewing, door_dust, labels_t, purge_t, lights_t, sleeper_cool, next_storage, next_book, next_box, gust;
  int tour_pending;
  bool lights_on, bike_toast, sew_toast, forge_toast, read_toast;
  /* the front end: resolved settings and group state */
  int mode;            /* resolved from SET at lobby open */
  bool want_bots;      /* resolved: fill when one human */
  float pace;          /* 1.4, 1.0, 0.65: multiplies the four spawn refills */
  float badmul;        /* 0.6, 1, 2: multiplies bad event weights */
  float jam;           /* printer jam chance, 0.3 or 0.6 */
  bool paused;
  bool flash_seen;
  int group_nights;
  int end_variant;
  int coach_st;        /* station the coach points at, -1 none */
  int team_karma;      /* co-op, kept current in karma() */
  int newbies_seen, newbies_greeted;
  float wipe_t;        /* night start tape wipe, 0.6 down to 0 */
} game_t;

extern game_t G;
extern player_t P[MAX_PLAYERS];
extern item_t I[MAX_ITEMS];
extern npc_t N[MAX_NPCS];
extern station_t S[ST_COUNT];

/* ── levels: the room as data (docs/design/FINAL-space.md section 7) ─────
 * A level is a complete description of one night: where every station and
 * piece of furniture stands, the props, the signs, the floor, the lighting and
 * the night script. src/levels.c holds the tables; game_set_level copies the
 * station places into S[] and the simulation reads everything else through L. */
#define MAX_OBST 24
#define MAX_PROPS 48
#define MAX_SIGNS 32
#define MAX_ACCENTS 24
#define MAX_OVERLAYS 12
#define MAX_STENCILS 8
#define EVENT_ROWS 25
#define LEVEL_COUNT 5

/* the side a station is used from */
enum { FACE_NONE, FACE_PZ, FACE_NZ, FACE_PX, FACE_NX };
/* furniture that blocks movement and does nothing; world.c picks a model by kind */
enum { OBK_CLOSET, OBK_WOODBAY, OBK_FENCE, OBK_LATHE, OBK_MILL, OBK_COUNTER, OBK_DESKS, OBK_WINDOW, OBK_COFFEE,
       OBK_LEGO, OBK_HOST, OBK_ISLAND, OBK_DONBIN, OBK_FOLD, OBK_BOXES, OBK_KIND_COUNT };
/* props: set dressing with no collision */
enum { PR_PENDANT, PR_TUBE, PR_FAN, PR_EXTINGUISHER, PR_FIRSTAID, PR_SAFETYGLASSES, PR_SHOPVAC, PR_BROOM, PR_TOOLCART,
       PR_BUCKET, PR_WATER, PR_AFRAME, PR_BIKERACK, PR_LEGO_CRATES, PR_CHAIR, PR_PLANT, PR_HAT, PR_PROJECTOR,
       PR_SLEEPER, PR_VISITOR, PR_KIND_COUNT };
/* signs: painted textures hung on walls and furniture */
enum { SG_LOCKUP, SG_FACADE, SG_TILE, SG_TRIPTYCH, SG_WIFI, SG_EXIT, SG_108, SG_GAS, SG_CAUTION, SG_DARKROOM, SG_PASSES,
       SG_UPSTAIRS, SG_OPEN, SG_PEGBOARD, SG_WC, SG_PAY, SG_KIND_COUNT };
enum { LIGHT_TUESDAY, LIGHT_DUST, LIGHT_DARK };
/* floor tiles on the 0.6 unit grid (section 2.2) and the things laid on top of them (2.4) */
enum { T_WHITE, T_ORANGE, T_RED, T_CHECK, T_GREY, T_TERRAZZO, T_MAT, T_GHOST };
enum { OV_LASER_MAT, OV_TAPE_LINE, OV_TAPE_RING, OV_FRAME, OV_RUG };

typedef struct { const char *label; float x, z, hw, hd, ry; uint8_t face; bool present, always_label; } st_place_t;
typedef struct { uint8_t kind; float x, z, hw, hd, ry; } obst_t;
typedef struct { uint8_t kind, arg; float x, y, z, ry; const char *text; } prop_t;   /* arg: plant 0 alive 1 dead; text: aframe copy */
typedef struct { uint8_t kind; const char *text; float x, y, z, ry, w, h; } sign_t;
typedef struct { uint8_t kind, c, r, cw, rh, ring; } accent_t;                        /* tile rect; ring 1 paints the one tile border only */
typedef struct { uint8_t kind; float x0, z0, x1, z1; } overlay_t;
typedef struct { const char *text; float x, z; uint8_t font; bool warn, on_grey; } stencil_t;
typedef struct { float x0, z0, x1, z1; } box_t;

typedef struct {
  uint32_t sky, ground; float hemi;
  uint32_t sun_col; float sun_i, sun_x, sun_y, sun_z, bias;
  struct { uint32_t col; float i, dist; } p[4];      /* kitchen, bench, laser (idle), hall; positions derive from stations */
  float pendant_e, tube_e, laser_idle;
  uint32_t grout, wash_col; int wash_a;
  float hud_tint; uint32_t tint_col;
} lighting_t;

typedef struct {
  float len; int clock_h0, clock_span;                 /* 180, 19, 3: CLASSIC reads 7 PM to 10 PM */
  int weight[EVENT_ROWS]; int first_event_forced;      /* -1, or a row index fired first */
  float first_event, event_gap, event_jit;             /* 14, 17, 8 */
  float first_newbie, newbie_gap, newbie_jit, newbie_until;   /* 6, 13, 7, 0 (0: all night) */
  int max_waiting, bike_one_in; float patience, member_life;  /* 3, 3, 20, 40 */
  int greet_karma; float greet_vibe, leave_vibe;       /* 100, 8, -12 */
  float dish_gap, first_donation, donation_gap, donation_jit;  /* 6, 24, 24, 10 */
  int start_dishes, start_sawdust, start_scrap, start_donations, start_boxes; float start_bath;  /* 3, 2, 1, 1, 0, 20 (plus rnd 30 at night start) */
  float rush_time, rush_players;                       /* 0.8, 0.12 */
  float cut_dust, cut_dust_rnd, haboob_dust;           /* 26, 12, 45 */
  float vibe_start, vibe_entropy, vibe_heal; int landlord_limit;   /* 70, 0.12, 0.15, 50 */
  float storage_gap, book_gap, box_gap;                /* 40, 50, 40 */
  float speed_mul; bool door_open_at_start; float gust_gap, gust_jit;   /* 1.0, false, 0, 0 */
  bool fridge_locked; int solo_bots[2];                /* false, BUILDER, TALKER */
} night_t;

typedef struct {
  const char *name, *blurb;
  float door_x, door_z, door_hw;
  box_t newbie_box, wander_box, spot_box, doormat_box, puddle_box, streak_box;
  float lobby_x, lobby_z, start_x, start_z;
  st_place_t st[ST_COUNT];
  obst_t ob[MAX_OBST]; int ob_n;
  prop_t props[MAX_PROPS]; int prop_n;
  sign_t signs[MAX_SIGNS]; int sign_n;
  accent_t accents[MAX_ACCENTS]; int accent_n;
  overlay_t overlays[MAX_OVERLAYS]; int overlay_n;
  stencil_t stencils[MAX_STENCILS]; int stencil_n;
  lighting_t light;
  night_t night;
} level_t;

extern level_t LEVELS[LEVEL_COUNT];
extern const level_t *L;                 /* the active level */
void levels_init(void);                  /* levels.c: fills the tables and the derived fields */
void game_set_level(int idx);            /* levels.c: L, G.level, L->st into S[] */

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
void game_begin_lobby(void);            /* leave attract or the title for the lobby; resolves SET */
void game_apply_settings(void);         /* SET and the mode into G.night_len, pace, badmul, jam, want_bots */
void game_quit_to_title(void);
int  game_mode_bots(int *a, int *b);    /* which bots the current mode adds for one human; returns count */
const char *item_name(const item_t *it);
const char *game_bot_name(int arch);

/* world.c: the 3D side */
void world_init(t3_scene *scene);
void world_set_level(const level_t *nl);   /* rebuild the room for the active level; game_reset_world first */
void world_sync(float dt);
t3_object *world_item_obj(int type, int kind);
t3_object *world_person(uint32_t color, bool newbie, t3_mesh **body);
void world_free(t3_object *o);
extern t3_camera *world_cam;

/* draw.c: the HUD side */
enum { FX_SPARK, FX_KARMA, FX_CONFETTI, FX_DUST, FX_STEAM, FX_JOIN };
void fx_burst(int kind, float x, float y, float z, uint32_t color);
void draw_hud(float dt);
bool project(float x, float y, float z, int *sx, int *sy);

#endif
