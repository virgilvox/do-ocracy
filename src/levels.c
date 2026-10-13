/* The room as data. Every level is a complete level_t: where each station and
 * piece of furniture stands, the props, the signs, the floor accents on the
 * 0.6 unit tile grid, the overlays and stencils, the spawn anchors, the
 * lighting preset and the night script. LEVELS[0] is CLASSIC TUESDAY, the
 * geometry of docs/design/FINAL-space.md section 3; levels 1 to 4 start as
 * copies of it and take their section 6 deltas in levels_init.
 *
 * Fixed per build: ST_COUNT, the ST_ order and OBK_KIND_COUNT. draw.c
 * station_labels, world.c world_sync and the three destination tables in
 * game.c switch on station ids, so a level can move, hide and relabel a
 * station but never invent a kind. Nothing goes between ST_PRINT1 and
 * ST_PRINT2. ST_SWITCH and ST_BACKDOOR lie inside the wall planes and are
 * skipped by collide. A station with present false is still built by world.c
 * and parked at y -20, so world_sync keeps its pointers; the simulation skips
 * it everywhere else.
 *
 * Axes: -z is the back wall, +z the street and the camera, -x screen left.
 * Tile c covers x in [-12.3 + 0.6 c, +0.6), row r covers z in [-7.15 + 0.6 r, +0.6).
 * Every gap between solids is at least 0.76 on one axis or closed; the
 * checker in the design doc (section 11.4) passed every box below. */
#include "game.h"
#include "hud.h"
#include <string.h>

#define PI_2 1.5707963f
#define PI_1 3.1415927f

level_t LEVELS[LEVEL_COUNT] = {
  [0] = {
    .name = "CLASSIC TUESDAY", .blurb = "AN ORDINARY OPEN NIGHT. DOORS LOCK AT TEN.",
    .door_x = 0.0f, .door_z = 7.9f, .door_hw = 1.1f,
    .newbie_box  = { -7.0f, -0.5f, 5.0f, 5.0f },
    .wander_box  = { -9.0f, -3.0f, 9.0f, 5.0f },
    .spot_box    = { -10.0f, -4.5f, 10.0f, 5.5f },
    .doormat_box = { -0.7f, 5.9f, 0.7f, 6.5f },
    .streak_box  = { 9.3f, 2.8f, 9.9f, 7.0f },
    /* puddle_box follows the sink: set in levels_init */
    .lobby_x = -1.2f, .lobby_z = 4.1f, .start_x = -1.5f, .start_z = 4.3f,

    /* 3.1 stations: label, x, z, hw, hd, ry, face, present, always_label */
    .st = {
      [ST_LUMBER]   = { "SCRAP BAYS",             -0.2f,  -6.5f,  0.9f,  0.5f,  0,    FACE_PZ,   true,  false },
      [ST_LASER]    = { "LARGE 80W LASER",        -3.4f,  -5.95f, 1.4f,  0.8f,  0,    FACE_PZ,   true,  true  },
      [ST_PRINT1]   = { "OUCH",                    7.0f,  -3.1f,  0.55f, 0.45f, 0,    FACE_PZ,   true,  false },
      [ST_PRINT2]   = { "DR. STRANGE",             8.6f,  -3.1f,  0.55f, 0.45f, 0,    FACE_PZ,   true,  false },
      [ST_CUTSALOT] = { "SIR CUTSALOT",           -8.0f,  -1.8f,  0.75f, 0.5f,  0,    FACE_PZ,   true,  false },
      [ST_BENCH]    = { "ELECTRONICS",            10.9f,   0.1f,  0.7f,  1.9f,  PI_2, FACE_NX,   true,  false },
      [ST_SINK]     = { "SINK",                  -11.0f,  -5.1f,  0.6f,  0.9f,  0,    FACE_PX,   true,  false },
      [ST_FRIDGE]   = { "SNACK FRIDGE",          -11.0f,  -6.5f,  0.55f, 0.5f,  0,    FACE_PX,   true,  false },
      [ST_TRASH]    = { "TRASH (ALLEY, 225 FT)",   9.85f, -6.6f,  0.6f,  0.5f,  0,    FACE_PZ,   true,  true  },
      [ST_FREEPILE] = { "FREE STUFF",            -11.1f,  -2.4f,  0.5f,  1.0f,  0,    FACE_PX,   true,  true  },
      [ST_SHOW]     = { "SHOW WINDOW",             5.75f,  6.75f, 3.75f, 0.3f,  0,    FACE_NZ,   true,  true  },
      [ST_COUCH]    = { "COUCH",                  -7.6f,  -4.3f,  0.45f, 1.2f,  PI_2, FACE_NONE, true,  false },
      [ST_TABLE]    = { "HYH TABLE",              -0.5f,   2.6f,  1.9f,  0.8f,  0,    FACE_NONE, true,  false },
      /* the new stations stand where section 3 puts them; the ones still parked
       * (present false) wait for their models and actions */
      [ST_VEND]     = { "VENDING",               -11.3f,   4.3f,  0.3f,  0.5f,  0,    FACE_PX,   true,  false },
      [ST_BOARD]    = { "WHITEBOARD",             -4.4f,   1.6f,  0.7f,  0.15f, 0,    FACE_PZ,   true,  false },
      [ST_SEWING]   = { "SEWING",                -11.2f,   2.2f,  0.4f,  0.8f,  0,    FACE_PX,   true,  false },
      [ST_BATH]     = { "BATHROOM",               -6.9f,  -6.85f, 0.5f,  0.3f,  0,    FACE_PZ,   true,  false },
      [ST_STORAGE]  = { "MEMBER STORAGE",        -11.3f,   0.0f,  0.3f,  0.6f,  0,    FACE_PX,   false, false },
      [ST_FORGE]    = { "WELDING BAY",            11.4f,  -4.2f,  0.5f,  0.8f,  0,    FACE_NX,   false, false },
      [ST_LIBRARY]  = { "COFFEE AND BOOKS",      -10.2f,   6.7f,  0.6f,  0.35f, 0,    FACE_NZ,   false, false },
      [ST_BACKDOOR] = { "BACK DOOR",              11.05f, -6.95f, 0.55f, 0.2f,  0,    FACE_PZ,   false, false },
      [ST_SWITCH]   = { "",                      -12.1f,  -1.0f,  0.15f, 0.25f, 0,    FACE_PX,   false, false },   /* never labelled */
      [ST_BIKE]     = { "BIKE STATION",            1.15f, -6.7f,  0.45f, 0.35f, 0,    FACE_PZ,   false, false },
    },

    /* 3.2 obstacles: kind, x, z, hw, hd, ry */
    .ob = {
      { OBK_CLOSET,  -8.75f, -6.75f, 0.55f, 0.4f,  0 },   /* supply closet and stairs, UPSTAIRS: LIMITED ACCESS */
      { OBK_WOODBAY,  3.6f,  -6.5f,  1.2f,  0.55f, 0 },   /* bandsaw, drill press, sander; touches the fence */
      { OBK_FENCE,    4.88f, -5.8f,  0.08f, 1.35f, 0 },   /* shop west half wall */
      { OBK_FENCE,    7.1f,  -4.5f,  2.3f,  0.08f, 0 },   /* shop front half wall; overlaps the west one at the corner */
      { OBK_LATHE,    6.15f, -6.3f,  1.15f, 0.55f, 0 },
      { OBK_MILL,     8.7f,  -6.4f,  0.55f, 0.55f, 0 },   /* the Blue Mill; touches the trash */
      { OBK_COUNTER,  7.8f,  -3.1f,  1.6f,  0.5f,  0 },   /* printing shelf; both printers nest in it */
      { OBK_COUNTER, -8.0f,  -1.8f,  1.4f,  0.5f,  0 },   /* craft counter; the cutter nests in it */
      { OBK_DESKS,   11.0f,   4.9f,  0.6f,  2.1f,  0 },   /* coworking row, closes the front right corner */
      { OBK_WINDOW,  -5.8f,   6.75f, 3.8f,  0.3f,  0 },   /* west display shelf: CRT wall and yellow crates */
      { OBK_COFFEE, -11.2f,   6.3f,  0.4f,  0.7f,  0 },   /* coffee cabinet, closes the front left corner */
      { OBK_LEGO,    -7.0f,   4.6f,  1.1f,  0.6f,  0 },
      { OBK_HOST,     3.1f,   5.25f, 0.5f,  0.35f, 0 },   /* host desk and the waiver computer */
      { OBK_ISLAND,   4.6f,   1.0f,  1.6f,  0.7f,  0 },   /* second table, class seating */
      { OBK_DONBIN,   4.4f,  -3.0f,  0.3f,  0.3f,  0 },   /* the tall donation bin */
    },
    .ob_n = 15,

    /* 3.4 props: kind, arg, x, y, z, ry, text */
    .props = {
      { PR_PENDANT, 0, 10.6f, 2.9f, 3.4f, 0, NULL }, { PR_PENDANT, 0, 10.6f, 2.9f, 5.4f, 0, NULL },     /* over the coworking row */
      { PR_PENDANT, 0, -1.2f, 2.9f, 2.6f, 0, NULL }, { PR_PENDANT, 0, 0.4f, 2.9f, 2.6f, 0, NULL },      /* over the HYH table */
      { PR_PENDANT, 0, 4.6f, 2.9f, 1.0f, 0, NULL },                                                    /* over the second table */
      { PR_PENDANT, 0, -10.2f, 2.9f, -5.0f, 0, NULL },                                                 /* over the kitchen */
      { PR_TUBE, 0, -9.0f, 3.05f, -6.88f, 0, NULL }, { PR_TUBE, 0, -3.0f, 3.05f, -6.88f, 0, NULL },
      { PR_TUBE, 0, 3.0f, 3.05f, -6.88f, 0, NULL }, { PR_TUBE, 0, 9.0f, 3.05f, -6.88f, 0, NULL },
      { PR_TUBE, 0, 6.0f, 3.05f, -5.6f, 0, NULL }, { PR_TUBE, 0, 8.6f, 3.05f, -5.6f, 0, NULL },         /* inside the shop */
      { PR_FAN, 0, 6.0f, 3.3f, 2.0f, 0, NULL }, { PR_FAN, 0, -6.0f, 3.3f, 0.0f, 0, NULL },
      { PR_EXTINGUISHER, 0, -1.45f, 1.1f, 6.95f, 0, NULL },                                            /* by the front door */
      { PR_EXTINGUISHER, 0, -1.3f, 1.1f, -6.95f, 0, NULL },                                            /* by the laser */
      { PR_EXTINGUISHER, 0, 10.0f, 1.1f, -4.48f, 0, NULL },                                            /* on the fence post */
      { PR_FIRSTAID, 0, -6.0f, 1.6f, -7.0f, 0, NULL },                                                 /* beside the bathroom door */
      { PR_SAFETYGLASSES, 0, 9.4f, 1.5f, -4.6f, PI_2, NULL },                                          /* fence east post, facing +x */
      { PR_SHOPVAC, 0, 2.0f, 0, -5.6f, 0, NULL },                                                      /* the pocket between bikes and wood bay */
      { PR_BROOM, 0, -10.3f, 0, -6.5f, 0, NULL },                                                      /* leaning on the fridge */
      { PR_TOOLCART, 0, 7.8f, 0, -5.2f, 0, NULL },                                                     /* inside the fence */
      { PR_BUCKET, 0, 6.0f, 0, -4.9f, 0, NULL }, { PR_BUCKET, 0, 9.0f, 0, -5.4f, 0, NULL },
      { PR_BUCKET, 0, 1.4f, 0, 2.2f, 0, NULL },                                                        /* under the HYH table */
      { PR_WATER, 0, 10.9f, 0, -6.0f, 0, NULL },                                                       /* two cases by the door */
      { PR_AFRAME, 0, 1.8f, 0, 7.6f, 0, NULL },                                                        /* on the sidewalk */
      { PR_BIKERACK, 0, -3.0f, 0, 7.7f, 0, NULL },                                                     /* slots at x -3.0, -2.4, -1.8, -1.2 */
      { PR_LEGO_CRATES, 0, -8.9f, 0, 6.2f, 0, NULL },
      { PR_CHAIR, 0, 10.0f, 0, 3.2f, PI_2, NULL }, { PR_CHAIR, 0, 10.0f, 0, 4.0f, PI_2, NULL },
      { PR_CHAIR, 0, 10.0f, 0, 4.8f, PI_2, NULL }, { PR_CHAIR, 0, 10.0f, 0, 5.6f, PI_2, NULL },
      { PR_CHAIR, 0, 10.0f, 0, 6.4f, PI_2, NULL },
      { PR_PLANT, 0, -9.9f, 0, -3.9f, 0, NULL }, { PR_PLANT, 0, 9.8f, 0, 6.6f, 0, NULL },
      { PR_PLANT, 1, 3.6f, 0.8f, 5.25f, 0, "A PLANT. IT WILL DIE HERE." },                            /* dead, on the host desk */
      { PR_HAT, 0, 2.8f, 0.82f, 5.25f, 0, NULL },                                                      /* the host's captain's hat */
      { PR_PROJECTOR, 0, -0.5f, 3.2f, 1.8f, 0, NULL },
    },
    .prop_n = 39,

    /* 3.5 signs: kind, text, x, y, z, ry, w, h. All basic+map, unlit. */
    .signs = {
      { SG_LOCKUP,   NULL,               4.4f,   2.95f, -6.98f, 0,     6.4f, 1.6f },   /* the mark via hud_logo, above the shop */
      { SG_FACADE,   NULL,               0.0f,   3.0f,   7.46f, 0,     4.0f, 1.0f },   /* the one new mark placement, on the header beam's street face */
      { SG_TILE,     "LASER CUTTER",    -3.4f,   2.05f, -6.98f, 0,     0.9f, 0.45f },
      { SG_TILE,     "WOOD TOOLS",       3.6f,   2.05f, -6.98f, 0,     0.9f, 0.45f },
      { SG_TILE,     "3D PRINTER",       7.8f,   1.45f, -3.55f, 0,     0.9f, 0.45f },  /* on the counter back board */
      { SG_TILE,     "VINYL STATION",   -8.0f,   1.45f, -2.25f, 0,     0.9f, 0.45f },  /* on the craft counter back board */
      { SG_TILE,     "ELECTRONICS",     11.97f,  2.05f,  0.1f,  -PI_2, 0.9f, 0.45f },
      { SG_TILE,     "KITCHEN",        -11.97f,  2.05f, -5.1f,   PI_2, 0.9f, 0.45f },
      { SG_TILE,     "FREE STUFF",     -11.97f,  2.05f, -2.4f,   PI_2, 0.9f, 0.45f },
      { SG_TILE,     "MEMBER STORAGE", -11.97f,  2.05f,  0.0f,   PI_2, 0.9f, 0.45f },
      { SG_TILE,     "SEWING",         -11.97f,  2.05f,  2.2f,   PI_2, 0.9f, 0.45f },
      { SG_TILE,     "COFFEE AND BOOKS", -11.97f, 2.05f, 6.3f,   PI_2, 0.9f, 0.45f },
      { SG_TILE,     "MACHINE SHOP",     9.0f,   1.3f,  -4.45f, 0,     0.9f, 0.45f },  /* on the fence by its east end */
      { SG_TRIPTYCH, "BE NICE.",        -4.2f,   2.95f, -6.98f, 0,     0.5f, 0.5f },
      { SG_TRIPTYCH, "BE MAKING.",      -3.4f,   2.95f, -6.98f, 0,     0.5f, 0.5f },
      { SG_TRIPTYCH, "CLEAN UP.",       -2.6f,   2.95f, -6.98f, 0,     0.5f, 0.5f },
      { SG_WIFI,     "WIFI: ASK THE HOST", 0.0f, 2.75f,  7.08f, PI_1,  2.0f, 0.25f },  /* door header, facing into the room */
      { SG_EXIT,     "EXIT",             0.0f,   2.95f,  7.08f, PI_1,  0.5f, 0.25f },
      { SG_EXIT,     "EXIT",             9.85f,  2.55f, -6.98f, 0,     0.5f, 0.25f },  /* above the back hallway recess */
      { SG_108,      "108",              0.5f,   1.6f,   7.12f, 0,     0.4f, 0.2f },   /* on the east door leaf */
      { SG_GAS,      "TURN OFF GAS",     0.0f,   1.9f,   0.06f, 0,     1.0f, 0.25f },  /* panel local: a child of the back door */
      { SG_CAUTION,  "CAUTION",          6.8f,   2.1f,  -6.98f, 0,     0.5f, 0.25f },  /* above the shop electrical panel */
      { SG_DARKROOM, "DARKROOM IN USE", -11.1f,  1.95f, -6.98f, 0,     1.0f, 0.25f },
      { SG_PASSES,   "PARKING PASSES 7 DAYS\nRED TAGS 14", -11.97f, 1.5f, 0.0f, PI_2, 0.6f, 0.3f },
      { SG_UPSTAIRS, "UPSTAIRS: LIMITED ACCESS", -8.75f, 2.0f, -6.33f, 0, 0.8f, 0.2f },
      { SG_OPEN,     "OPEN",            -3.2f,   2.45f,  7.13f, 0,     1.4f, 0.7f },   /* west window header */
      { SG_PEGBOARD, NULL,              11.97f,  2.15f,  0.1f,  -PI_2, 3.6f, 1.8f },   /* behind the bench */
      { SG_WC,       "WC",              -6.9f,   1.9f,  -6.93f,  0,     0.25f, 0.25f },
      { SG_PAY,      "PAY FOR SNACKS HERE", -10.44f, 1.7f, -6.5f, PI_2, 0.4f, 0.2f }, /* the fridge's +x face */
    },
    .sign_n = 29,

    /* 2.6 and 11.1 floor accents: kind, c, r, cols, rows, ring. Colour is always on the approach side. */
    .accents = {
      { T_RED,      13, 4,  4, 2, 0 },   /* laser: stand here */
      { T_RED,      19, 2,  3, 1, 0 },   /* scrap bays and bike station */
      { T_ORANGE,   23, 0,  6, 3, 1 },   /* wood tools: one tile ring around the bay */
      { T_GREY,     29, 0,  7, 4, 0 },   /* machine shop concrete */
      { T_ORANGE,   36, 1,  3, 2, 0 },   /* in front of the back hallway recess and the door */
      { T_MAT,      30, 6,  7, 2, 0 },   /* grey mat under the printing counter */
      { T_RED,      32, 8,  3, 2, 0 },   /* printers: stand here */
      { T_MAT,      37, 8,  3, 8, 0 },   /* grey mat under the electronics bench */
      { T_ORANGE,   36, 9,  1, 6, 0 },   /* electronics aisle band */
      { T_RED,      36, 17, 1, 7, 0 },   /* coworking aisle runway */
      { T_ORANGE,   15, 13, 1, 6, 0 },   /* HYH table: the L's west leg */
      { T_ORANGE,   15, 18, 9, 1, 0 },   /* HYH table: the L's south leg */
      { T_MAT,       4, 8,  6, 2, 0 },   /* grey mat under the craft counter */
      { T_ORANGE,    4, 10, 5, 1, 0 },   /* craft corner stripe */
      { T_CHECK,     2, 14, 2, 2, 0 },   /* sewing: the red and orange checker */
      { T_RED,       3, 2,  2, 3, 0 },   /* kitchen: in front of the sink */
      { T_ORANGE,    2, 6,  2, 3, 0 },   /* free stuff shelf */
      { T_RED,      24, 21, 12, 1, 0 },  /* show window: stand here row */
      { T_TERRAZZO, 19, 22, 3, 2, 0 },   /* the vestibule */
    },
    .accent_n = 19,

    /* 2.4 overlays in world units; the laser mat is appended in levels_init from the laser box */
    .overlays = {
      { OV_TAPE_LINE,  4.8f,  -4.42f, 9.4f,  -4.12f },   /* caution tape along the shop fence front */
      { OV_TAPE_LINE,  6.3f,  -6.75f, 7.3f,  -6.45f },   /* under the electrical panel */
      { OV_TAPE_RING, 10.5f,  -5.3f, 12.2f,  -3.1f },    /* around the welding bay */
      { OV_FRAME,      2.0f,   6.3f,  9.5f,   7.15f },   /* the show window stage frame */
    },
    .overlay_n = 4,

    /* 2.5 stencils: text, x, z, font, warn, on_grey. Safety lines carry no punchline. */
    .stencils = {
      { "LASER. EYES ON IT.", -3.4f, -3.3f, F_MONO, true,  false },
      { "HOT WORK OUTSIDE",   10.0f, -3.0f, F_MONO, true,  false },
      { "MACHINE SHOP",        7.1f, -5.1f, F_H3,   false, true  },
      { "COWORKING",           9.0f,  3.6f, F_H3,   false, false },
      { "KITCHEN",            -9.2f, -3.0f, F_H3,   false, false },
      { "CRAFT CORNER",       -8.0f, -0.3f, F_H3,   false, false },
    },
    .stencil_n = 6,

    /* 6.6 TUESDAY lighting: 1 hemisphere, 1 directional with the only shadow, 4 point */
    .light = {
      .sky = 0xfff2dc, .ground = 0x5a4a3a, .hemi = 1.4f,
      .sun_col = 0xffe2b8, .sun_i = 2.3f, .sun_x = -6, .sun_y = 16, .sun_z = 9, .bias = -0.0003f,
      .p = { { 0xffc07a, 11, 8 }, { 0x8fbde6, 10, 8 }, { 0xff3b2a, 0, 6 }, { 0xffd9a8, 10, 9 } },
      .pendant_e = 2.5f, .tube_e = 2.2f, .laser_idle = 0,
      .grout = 0x9c968a, .wash_col = 0, .wash_a = 0,
      .hud_tint = 0, .tint_col = 0,
    },

    /* 6.7 and 6.8: the CLASSIC night. Event rows follow section 5.15; rows the
     * game does not have yet (13 to 24) are ignored by fire_event until they land. */
    .night = {
      .len = 180, .clock_h0 = 19, .clock_span = 3,
      .weight = { 1, 1, 1, 3, 3, 3, 3, 3, 3, 2, 3, 2, 2, 2, 1, 1, 1, 2, 2, 1, 0, 2, 2, 2, 1 },
      .first_event_forced = -1,
      .first_event = 14, .event_gap = 17, .event_jit = 8,
      .first_newbie = 6, .newbie_gap = 13, .newbie_jit = 7, .newbie_until = 0,
      .max_waiting = 3, .bike_one_in = 3, .patience = 20, .member_life = 40,
      .greet_karma = 100, .greet_vibe = 8, .leave_vibe = -12,
      .dish_gap = 6, .first_donation = 24, .donation_gap = 24, .donation_jit = 10,
      .start_dishes = 3, .start_sawdust = 2, .start_scrap = 1, .start_donations = 1, .start_boxes = 0, .start_bath = 20,
      .rush_time = 0.8f, .rush_players = 0.12f,
      .cut_dust = 26, .cut_dust_rnd = 12, .haboob_dust = 45,
      .vibe_start = 70, .vibe_entropy = 0.12f, .vibe_heal = 0.15f, .landlord_limit = 50,
      .storage_gap = 40, .book_gap = 50, .box_gap = 0,
      .speed_mul = 1.0f, .door_open_at_start = false, .gust_gap = 0, .gust_jit = 0,
      .fridge_locked = false, .solo_bots = { BOT_BUILDER, BOT_TALKER },
    },
  },
};

const level_t *L = &LEVELS[0];

/* 6.1: the other four nights. They play as copies of CLASSIC TUESDAY until
 * their section 6 deltas (geometry, the DUST and DARK presets, the night
 * columns) are applied here. */
static const struct { const char *name, *blurb; } META[LEVEL_COUNT] = {
  { "CLASSIC TUESDAY", "AN ORDINARY OPEN NIGHT. DOORS LOCK AT TEN." },
  { "OPEN HOUSE",      "FIRST SATURDAY. EVERYONE CAME. SAY HI TO ALL OF THEM." },
  { "HABOOB NIGHT",    "A WALL OF DESERT. IT IS INSIDE NOW. SWEEP." },
  { "MOVE NIGHT",      "THE WOOD SHOP MOVED AGAIN. THIRD TIME. BOXES." },
  { "LATE NIGHT",      "AFTER TEN. CARD ACCESS ONLY. THE FRIDGE IS AWAKE." },
};

void levels_init(void) {
  static bool done;
  if (done) return;
  done = true;
  level_t *b = &LEVELS[0];
  /* derived fields: the rubber mat follows the laser, the AC puddle box follows the sink */
  const st_place_t *la = &b->st[ST_LASER], *sk = &b->st[ST_SINK];
  if (b->overlay_n < MAX_OVERLAYS)
    b->overlays[b->overlay_n++] = (overlay_t){ OV_LASER_MAT, la->x - la->hw - 0.2f, la->z - la->hd, la->x + la->hw + 0.2f, la->z + la->hd + 0.25f };
  b->puddle_box = (box_t){ sk->x + 1.2f, sk->z - 1.0f, sk->x + 3.2f, sk->z + 2.0f };
  for (int i = 1; i < LEVEL_COUNT; i++) {
    LEVELS[i] = LEVELS[0];
    LEVELS[i].name = META[i].name;
    LEVELS[i].blurb = META[i].blurb;
  }
}

/* Make a level the active one: L, G.level and the station places in S[].
 * The model pointers in S[] are left alone; world.c owns them and parks the
 * models of absent stations at y -20 when it rebuilds. */
void game_set_level(int idx) {
  if (idx < 0 || idx >= LEVEL_COUNT) idx = 0;
  L = &LEVELS[idx];
  G.level = idx;
  for (int i = 0; i < ST_COUNT; i++) {
    const st_place_t *p = &L->st[i];
    S[i].label = p->label ? p->label : "";
    S[i].x = p->x; S[i].z = p->z; S[i].hw = p->hw; S[i].hd = p->hd; S[i].ry = p->ry;
    S[i].face = p->face; S[i].present = p->present; S[i].always_label = p->always_label;
    S[i].solid = true;
  }
}
