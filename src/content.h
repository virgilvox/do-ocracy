/* All the words in the game. Plain maker voice: no dashes, no emoji, flat
 * statements, punchline last (docs/design/humor-brief.md). Edit freely;
 * nothing here is load-bearing except array lengths and the one %s or %d a
 * call site fills. Toasts stay under 64 characters, banners under 48. */
#ifndef CONTENT_H
#define CONTENT_H

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* the title, one at a time, every 6 s (humor brief 2.1) */
static const char *const TAGLINES[] = {
  "WHOEVER DOES IT, DECIDES.",
  "NOBODY DOES THE DISHES.",
  "COMMUNICATION IS NOT PERMISSION.",
  "A JANITORIAL CO-OP WITH CLASSROOMS.",
  "THE LASER IS BOOKED. THE MOP IS FREE.",
  "MEET ON TUESDAY. END OF DISCUSSION.",
  "RENT IS DUE. VIBES ARE NOT A CURRENCY.",
  "THE FREE PILE ONLY GROWS.",
  "STEP 5: RETURN TO STEP 1.",
  "IF THE LAB CLOSES, YOUR CLOUT IS VOID.",
  "SOMEBODY HAS TO. IT IS YOU.",
  "LINUX NIGHT IS CANCELED. PLAY ANYWAY.",
  "BE EXCELLENT. ALSO SWEEP.",
  "THE WIKI SAYS TO UPDATE THE WIKI.",
};

/* The attract loop. The floor-cleaning loop is real; it has never exited.
 * Indexed by time, so the five numbered lines stay first and in order. */
static const char *const ATTRACT_TIPS[] = {
  "1. WE SHOULD CLEAN THE FLOORS.",
  "2. TESTING ENSUES.",
  "3. GET A QUOTE. IT IS $4,000.",
  "4. WHY DON'T WE CLEAN THEM OURSELVES?",
  "5. RETURN TO STEP 1.",
  "THE FLOOR LOOP HAS NEVER EXITED.",
  "CLOUT IS FOR YOU. KARMA IS FOR THE SPACE.",
  "THE LASER IS BOOKED. SIR CUTSALOT IS FREE.",
  "A NEWBIE DECIDES IN FIVE MINUTES. SAY HI.",
  "THE FIRE MARSHAL COUNTS POWER STRIPS.",
  "NO STAFF. NO FRONT DESK. ONLY YOU.",
  "1,716 HOURS OF CLEANING. NOT ON THE POSTER.",
  "COORDINATE BEFORE. DO NOT APOLOGIZE AFTER.",
};

/* the title's call to action (6.1) and the attract tag (6.2): the keyboard
 * player presses Space, so the block says so, and the demo says it is one */
static const char *const TITLE_CTA = "PRESS A OR SPACE";
static const char *const TITLE_CTA_SUB = "A ON A PAD. SPACE ON THE KEYBOARD.";
static const char *const ATTRACT_TAG = "DEMO";
static const char *const ATTRACT_SUB = "THE BOTS ARE PLAYING. NOBODY IS AT THE CONTROLS.";
static const char *const ATTRACT_CTA = "PRESS A OR SPACE TO PLAY";
/* the first five seconds of a real night, over every human's head */
static const char *const YOU_ARE = "YOU ARE %s";

/* the main menu (FINAL-frontend.md 6.3) */
static const char *const MENU_ROWS[] = { "PLAY", "MODES", "SETTINGS", "HOW TO PLAY", "CREDITS" };
static const char *const DENY_LINES[] = {
  "ONE AT A TIME. THIS IS A DO-OCRACY.",
  "JOIN IN THE LOBBY. P1 IS PICKING.",
  "PINNED BY THE MODE. PICK CUSTOM.",
};
static const char *const NEEDS_LINE = "NEEDS 2 PLAYERS OR BOTS";
/* CREDITS (6.7): roles and places only, never a person's name. The last line is hazard */
static const char *const CREDITS[] = {
  "HEATSYNC LABS. 108 W MAIN ST, MESA. SINCE 2009.",
  "ENGINE: THREE.C ON WASMCART.  NOISES: BELLOWS.",
  "FONTS: ARCHIVO BLACK, IBM PLEX MONO, CHARIS SIL.",
  "THE FLOOR LOOP HAS NEVER EXITED.",
};
static const char *const CREDITS_HEAD = "A GAME ABOUT A HACKERSPACE, MADE AT ONE.";

/* X button. The Talker move. It feels productive. It is not. */
static const char *const PROPOSALS[] = {
  "...GET A BIGGER LASER",
  "...PAINT IT PURPLE",
  "...FORM A COMMITTEE",
  "...MAKE A CHANNEL FOR THAT",
  "...VOTE ON IT AT HYH",
  "...BUY A PALLET OF CLUB-MATE",
  "...LABEL EVERYTHING",
  "...GET A GLOWFORGE (NO)",
  "...REORGANIZE THE FREE PILE",
  "...WRITE A WIKI PAGE",
  "...MOVE THE WOOD SHOP AGAIN",
  "...HAVE A PIZZA BUDGET",
  "...CLEAN THE FLOORS",
  "...RUN A CLASS ON THAT",
  "...BUY A BRIDGEPORT",
};

/* What each station makes. Project kind: 0 laser, 1 print, 2 bench, 3 sticker, 4 sewing, 5 forge. */
static const char *const LASER_PROJECTS[] = {
  "LASER MUSTACHE", "TARDIS", "POKEJEANS", "COOKIE DOUGH", "NIXIE CLOCK FACE",
  "LIGHTNING OCTOPUS", "BUSINESS CARDS", "BOX WITH FINGER JOINTS", "LEGO WINDOW SIGN",
  "TOO-LARGE WOOD ETCH",
};
static const char *const PRINT_PROJECTS[] = {
  "BENCHY", "SPARE PART FOR A PRINTER", "FIDGET THING", "CTHULHU IN A TOP HAT",
  "3D PRINTED MUSTACHE", "ARCADE BUTTON BEZEL", "CALIBRATION CUBE", "TINY LASER",
};
static const char *const BENCH_PROJECTS[] = {
  "BLINKY BADGE", "LOLDIALER", "TESLA COIL (SMALL)", "THEREMIN", "NIXIE CLOCK",
  "BOATDUINODE", "TWITTER SMOKE SIGNALS",
};
static const char *const STICKERS[] = {
  "I VOIDED THE WARRANTY", "HACK THE PLANET", "MADE IN MESA", "OVER 9000",
};
/* the most used tool nobody mentions */
static const char *const SEWING_PROJECTS[] = {
  "CON BADGE LANYARD", "PATCH FOR THE PATCH", "LED HOODIE", "BAG FOR A PROJECT BAG",
  "CAPE (FOR A CAT)", "HEMMED ONE LEG", "A PLUSH TARDIGRADE", "POKEJEANS",
};
/* everything you make at the forge is a hook */
static const char *const FORGE_PROJECTS[] = {
  "A HOOK", "ANOTHER HOOK", "A KNIFE (IT IS A HOOK)", "BOTTLE OPENER",
  "RAILROAD SPIKE SOMETHING", "A LEAF", "TONGS TO MAKE MORE TONGS", "A VERY HOT NAIL",
};

typedef struct { const char *name; int heavy; } donation_t;
static const donation_t DONATIONS[] = {
  { "A VERY HEAVY CRT", 1 },
  { "THREE BROKEN PRINTERS", 1 },
  { "BOX OF WALL WARTS", 0 },
  { "BIN OF MYSTERY FASTENERS", 0 },
  { "CHEST FREEZER (RUNS FINE)", 1 },
  { "HALF A TON OF FREE STEEL", 1 },
  { "SOMEONE'S OLD RAID ARRAY", 0 },
  { "A BAG OF VGA CABLES", 0 },
};

static const char *const NEWBIE_LEAVES[] = {
  "A NEWBIE LEFT. NOBODY SAID HI.",
  "A NEWBIE FOUND A PLACE WITH A FRONT DESK.",
  "A NEWBIE WENT HOME TO A CLEANER GARAGE.",
  "A NEWBIE SAW THE FRIDGE AND MADE A CHOICE.",
  "A NEWBIE ASKED WHERE TO START. SILENCE.",
};

static const char *const NEWBIE_JOINS[] = {
  "NEW MEMBER. THEY ASKED HOW TO HELP. OH NO.",
  "NEW MEMBER. HANDED A BROOM IN SECONDS.",
  "NEW MEMBER. ALREADY EYEING THE FREE PILE.",
  "NEW MEMBER. FUTURE BUILDER. CURRENT SWEEPER.",
  "NEW MEMBER. THINKS THE LASER IS FREE.",
};

/* one %s: the printer's label */
static const char *const FIXED_PRINTER[] = {
  "%s LIVES. GRUB SCREW. ALWAYS.",
  "%s IS BACK. NOBODY WILL SAY THANKS.",
  "%s FIXED. IT WILL JAM AGAIN BY THURSDAY.",
};

/* scoring moments, fired at the call site named */
static const char *const TOAST_SWEPT     = "SWEPT. THE DESERT WILL BE BACK.";            /* sawdust cleaned */
static const char *const TOAST_MOPPED    = "MOPPED. THE AC WILL CRY AGAIN.";             /* puddle cleaned */
static const char *const TOAST_UNPLUGGED = "UNPLUGGED. A FIRE MARSHAL RELAXES.";         /* daisy chain cleaned */
static const char *const TOAST_SPAGHETTI = "SPAGHETTI TRASHED. IT WAS A BENCHY ONCE.";   /* spaghetti to the trash */
static const char *const TOAST_SHELVED   = "SHELVED. SOMEONE WILL WANT THAT. SOMEDAY.";  /* donation to the free shelf */
static const char *const TOAST_FRIDGE    = "FRIDGE CLEARED. DO NOT ASK.";                /* fridge hold finished */
static const char *const TOAST_DISHES    = "DISHES DONE. THE MOST IMPORTANT MACHINE WAS YOU.";   /* the sink runs empty */
static const char *const TOAST_FIRST_HI  = "YOU SAID HI. YOU ARE THE FRONT DESK NOW.";   /* a player's first greeting */
static const char *const TOAST_MATE      = "CLUB-MATE. AN ACQUIRED TASTE. NOBODY HAS.";  /* a player's first bottle */
static const char *const TOAST_CUT       = "CUT DONE. THE QUEUE WOULD LIKE A WORD.";     /* the laser finishes */
static const char *const TOAST_VEND_ATE  = "IT ATE A DOLLAR. AGAIN. ROCK IT.";            /* the vending machine jams on a purchase */
static const char *const TOAST_VEND_ROCK = "THE HONOUR JAR HAS A NOTE AND A WASHER.";     /* rocked loose */
static const char *const TOAST_WIPED     = "WHITEBOARD WIPED. %d COMMITTEES DISSOLVED.";  /* one %d: the lines wiped */
static const char *const TOAST_CORNER    = "YOU WIPED THE CORNER. IT SAID DO NOT ERASE. DATED 2019.";
static const char *const TOAST_SEWING    = "SOMEBODY IS SEWING. THE ROOM LOOKS FRIENDLIER.";  /* first sewing of the night */
static const char *const TOAST_BATH      = "BATHROOM DONE. +KARMA. NO WITNESSES.";        /* the longest hold, finished */
static const char *const TOAST_BIN       = "A BIN GAVE UP. IT WAS LABELED MISC.";          /* member storage spills */
static const char *const TOAST_NOTICE    = "THIRD AND FINAL NOTICE. FIRST AND SECOND WERE IMPLIED.";   /* the third notice clears storage */
static const char *const TOAST_FORGE_LIT = "THE FORGE IS LIT. IT IS NOW 140 IN THE ALLEY.";   /* first weld of the night */
static const char *const TOAST_SPARKS    = "SPARKS NEAR THE FREE PILE. THE MARSHAL FELT THAT.";   /* a weld with clutter over 70 */
static const char *const TOAST_READ      = "40 BINDERS OF DATASHEETS. ONE HAS BEEN READ.";   /* first quiet read of the night */
static const char *const TOAST_FLAT      = "FIXED A FLAT. IT IS 118. THEY RODE HERE.";       /* the bike station */
static const char *const TOAST_BIKE_IN   = "A NEWBIE LOCKED UP. IT IS 118. BE FAST.";        /* first newbie by bike */
static const char *const TOAST_DOOR_SHUT = "DOOR SHUT. THE DESERT WAITS.";
static const char *const TOAST_DOOR_GUST = "THE DOOR AGAIN. THE DESERT IS PATIENT.";         /* HABOOB NIGHT gusts */
static const char *const TOAST_LIGHTS_DAY = "LIGHTS. SO PEOPLE KNOW WE ARE OPEN.";
static const char *const TOAST_LIGHTS_OFF = "SOMEONE TURNED THE LIGHTS OFF TO SAVE POWER.";   /* DARK: the switch times out */
static const char *const TOAST_SLEEPER   = "NO. TWO NIGHTS A WEEK MAX. THEY ROLLED OVER.";
static const char *const TOAST_PURGED    = "SOMEONE WANTED THAT. TOO LATE.";                 /* the free pile purge lands */
static const char *const TOAST_DRAIN     = "SIX CUPS. THE SEWER GAS RETREATS.";

/* The event table, 25 rows in the order of docs/design/FINAL-space.md 5.15;
 * the night script's weight column and game.c's effect list index into it.
 * Row 6's sub takes the night's landlord limit (%d); row 9's banner takes
 * S[ST_PRINT2].label (%s). */
typedef struct { const char *text, *sub; } event_text_t;
static const event_text_t EVENT_TEXT[] = {
  { "LINUX NIGHT: CANCELED.",                  "AS FORETOLD. NOTHING HAPPENS." },
  { "QUIET NIGHT.",                            "SUSPICIOUS. ENJOY IT." },
  { "A FIFTH THURSDAY?!",                      "BLASPHEMY. ALSO IT IS TUESDAY. NOTHING HAPPENS." },
  { "IT IS 118 OUTSIDE.",                      "THE AC IS CRYING ON THE FLOOR. MOP IT." },
  { "HABOOB.",                                 "THE DESERT IS NOW INSIDE. SWEEP IT BACK OUT." },
  { "SOMEBODY BROUGHT TACOS.",                 "VIBE UP. DISHES UP. SAME NUMBER, OPPOSITE SIGN." },
  { "LANDLORD WALKTHROUGH. TEN SECONDS.",      "LOOK NORMAL. CLUTTER UNDER %d." },
  { "DAISY CHAIN DETECTED.",                   "FOUR POWER STRIPS DEEP. UNPLUG IT." },
  { "DONATION ON THE DOORMAT.",                "NOBODY ASKED FOR IT. CARRY IT TO THE FREE PILE." },
  { "ANY PARTICULAR REASON %s IS DOWN?",       "SAME REASON AS LAST WEEK. HOLD ON IT TO FIX." },
  { "SOMETHING IN THE FRIDGE HAS A PULSE.",    "SOMEBODY HAS TO. HOLD ON THE FRIDGE." },
  { "PRINT LEFT RUNNING OVERNIGHT.",           "IT IS SPAGHETTI NOW. TRASH IT." },
  { "COFFEE AND COWORKING RAN LONG.",          "OLDER THAN THE LASER. GOOD VIBES." },
  { "THE MAILING LIST IS ON FIRE.",            "200 MESSAGES. NOBODY HERE IS IN IT. VIBE DIPS." },
  { "SOMEONE LABELED EVERYTHING.",             "INCLUDING THE LABEL MAKER. CLUTTER EASES." },
  { "FREE PILE PURGE: THIRD AND FINAL NOTICE.", "THE FIRST TWO WERE IMPLIED. PILE EMPTIES IN 20." },
  { "THE WOOD SHOP USED TO BE RIGHT HERE.",    "IT DID. THREE TIMES. VIBE DIPS." },
  { "SOMEONE LEFT THE BATHROOM LIGHT ON.",     "AND THE REST OF IT. HOLD ON THE DOOR." },
  { "A BIKE ROLLED IN WITH A FLAT.",           "THIRD FRIDAY ENERGY. FIX IT AT THE BIKE STATION." },
  { "A STORAGE BIN GAVE UP.",                  "IT WAS LABELED MISC. POST A NOTICE." },
  { "A TOUR GROUP.",                           "THREE AT ONCE. SOMEBODY SAY HI THREE TIMES." },
  { "THE CLOSET DRAIN DRIED OUT.",             "SIX CUPS OF WATER. HOLD ON THE SINK." },
  { "THE DESERT WOULD LIKE TO COME IN.",       "THE BACK DOOR IS OPEN. SHUT IT." },
  { "THE VENDING MACHINE ATE A DOLLAR.",       "ROCK IT. GENTLY. IT IS OLDER THAN YOU." },
  { "CAN I SLEEP AT THE LAB?",                 "NO. NOTHING HAPPENS. VIBE HOLDS." },
};

/* End of night awards: the one-liner under the name, one %d where the code passes one. */
static const char *const AWARD_JANITOR  = "MOST KARMA. THE LAB OPENS TOMORROW BECAUSE OF YOU.";
static const char *const AWARD_GLAMOUR  = "MOST CLOUT, LEAST KARMA. THE SPREADSHEET LOOKED FINE.";
static const char *const AWARD_TALKER   = "%d PROPOSALS. ZERO FOLLOW-UPS. A RECORD.";
static const char *const AWARD_GREETER  = "SAID HI TO %d NEWBIES. YOU ARE THE FRONT DESK NOW.";
static const char *const AWARD_MATE     = "%d CLUB-MATES. CAN HEAR COLOURS. STILL SWEEPING.";
static const char *const AWARD_LURKER   = "PAID DUES. TOUCHED NOTHING. THE LIGHTS STAYED ON.";
static const char *const AWARD_CURATOR  = "SHELVED %d DONATIONS. NONE WILL EVER LEAVE.";
static const char *const AWARD_WHISPER  = "FIXED %d PRINTERS. NOBODY SAID THANKS. WE JUST DID.";
static const char *const AWARD_MARSHAL  = "UNPLUGGED %d DAISY CHAINS. THE LAB STILL EXISTS.";
static const char *const AWARD_DISHPIT  = "%d DISHES. NOBODY DOES THE DISHES. YOU DID.";
static const char *const AWARD_BIKESHED = "ARGUED ABOUT THE COLOUR. THE SHED IS UNBUILT.";
/* the awards the new stations hand out (work items 9 and 11 count baths and welds) */
static const char *const AWARD_WITNESS  = "CLEANED THE BATHROOM %d TIMES. NO WITNESSES.";
static const char *const AWARD_GREASE   = "FIXED %d FLATS. THIRD FRIDAY ENERGY.";

/* Front end settings tables (FINAL-frontend.md 8.6). SET indexes these. */
static const struct { const char *name; float secs; } NIGHT_LENS[] = {
  { "SHORT 2:00", 120 }, { "NORMAL 3:00", 180 }, { "LONG 5:00", 300 }, { "MARATHON 10:00", 600 } };
static const char *const CHAOS_NAMES[] = { "CALM", "NORMAL", "CHAOS" };
static const float PACE[] = { 1.4f, 1.0f, 0.65f };      /* multiplies the four spawn refills */
static const float BADMUL[] = { 0.6f, 1.0f, 2.0f };    /* multiplies the bad event weights */
static const char *const HELP_NAMES[] = { "OFF", "FIRST NIGHT", "ALWAYS" };
static const char *const ONOFF[] = { "OFF", "ON" };
static const char *const ROOM_NAMES[] = { "108 W MAIN" };   /* until the ROOM row reads LEVELS[].name */
/* how many end lines each ending has (END_NONE, END_SURVIVED, END_FIRE, END_BURNOUT) */

/* the modes (FINAL-frontend.md 8.6); the colour and glyph tokens come from hud.h */
typedef struct { const char *short_name, *name1, *name2, *players, *blurb, *rule[3]; int rule_glyph[3]; uint32_t color; int glyph; } mode_def_t;
static const mode_def_t MODES[MODE_COUNT] = {
 { "SOLO",   "SOLO",   "WITH BOTS",    "1 PLAYER", "YOU PLUS TWO MEMBERS. ONE OF THEM MOSTLY TALKS.",
   { "BOTS: BUILDER, TALKER", "CLOCK: %s", "SCORE: CLOUT AND KARMA" }, { GL_BOT, GL_HEART, GL_CLOUT }, C_HAZARD, GL_BOT },
 { "CO-OP",  "CO-OP",  "VS THE CLOCK", "1 TO 8",   "ONE TEAM. ONE KARMA TOTAL. KEEP THE LAB OPEN.",
   { "SCORE: TEAM KARMA", "GRADE: S A B C", "LOSE: EVERYONE LOSES" }, { GL_KARMA, GL_CHECK, GL_MARSHAL }, C_LIVE, GL_KARMA },
 { "VERSUS", "VERSUS", "CLOUT RACE",   "2 TO 8",   "MOST CLOUT WINS. IF THE LAB CLOSES, NOBODY WINS.",
   { "SCORE: CLOUT", "LOST NIGHT: CLOUT VOID", "ALONE? BOTS ARE RIVALS" }, { GL_CLOUT, GL_MARSHAL, GL_BOT }, C_HAZARD, GL_CLOUT },
 { "CHAOS",  "CHAOS",  "NIGHT",        "1 TO 8",   "THINGS BREAK FASTER. A PRINTER IS ALREADY DOWN.",
   { "EVENTS: MORE AND WORSE", "PRINTERS: JAM MORE", "CLOCK: SHORT 2:00" }, { GL_MARSHAL, GL_BOT, GL_HEART }, C_RUST, GL_MARSHAL },
 { "CUSTOM", "CUSTOM", "YOUR RULES",   "1 TO 8",   "YOUR SETTINGS. YOUR PROBLEM.",
   { "OPENS SETTINGS", "THEN THE LOBBY", "SCORE: RANKED" }, { GL_KEYS, GL_CHECK, GL_CLOUT }, C_VIOLET, GL_KEYS },
};
static const char *const MODE_LOBBY_NAMES[MODE_COUNT] = { "SOLO WITH BOTS", "CO-OP VS THE CLOCK", "VERSUS CLOUT RACE", "CHAOS NIGHT", "CUSTOM YOUR RULES" };

/* rules flash captions as colour runs; NULL terminated (the first one also
 * captions the HOW TO PLAY preview on the main menu) */
static const run_t FLASH_CAPS[5][5] = {
  { { "HOLD A. ", C_HAZARD }, { "DO THE THING.", C_BONE }, { NULL, 0 } },
  { { "FOLLOW THE ", C_BONE }, { "ARROW.", C_HAZARD }, { NULL, 0 } },
  { { "AMBER", C_HAZARD }, { " IS YOURS. ", C_BONE }, { "TEAL", C_LIVE }, { " IS OURS.", C_BONE }, { NULL, 0 } },
  { { "CLUTTER", C_RUST }, { " 100: ", C_BONE }, { "FIRE MARSHAL.", C_FAULT }, { NULL, 0 } },
  { { "SAY HI", C_LIVE }, { " OR THEY LEAVE.", C_BONE }, { NULL, 0 } },
};

/* the coach: the first 40 s of a night, one step every 8 s (chip, line); steps
 * 1 to 3 point the ash chevrons at a station, 4 and 5 point nowhere. The %s
 * in the first two lines is the group's button (A, SPACE, PERIOD, A OR SPACE) */
static const char *const COACH_STEPS[5][2] = {
  { "1 CLOUT", "GRAB PLYWOOD AT THE RACK. PRESS %s" },
  { "2 CLOUT", "LOAD THE LASER. HOLD %s TO CUT" },
  { "3 CLOUT", "CARRY IT TO SHOW & TELL" },
  { "4 KARMA", "TEAL RINGS: HOLD TO CLEAN" },
  { "5 KARMA", "A ? NEEDS A HI OR THEY LEAVE" },
};
static const int COACH_ST[5] = { ST_LUMBER, ST_LASER, ST_SHOW, -1, -1 };

/* the slot bar when the room is in trouble: clutter over 80, vibe under 20 */
static const char *const DANGER_LINES[2][2] = {
  { "CLUTTER!", "CLEAN. THE MARSHAL IS COUNTING." },
  { "VIBES!",   "SAY HI. CLEAN UP." },
};

static const char *const SETTING_ROWS[] = { "NIGHT LENGTH", "BOTS", "CHAOS", "PHONES", "MUSIC", "SFX", "ROOM", "HELP", "RESET TO DEFAULTS", "DONE" };
static const char *const PAUSE_ROWS[] = { "RESUME", "HOW TO PLAY", "QUIT TO MENU" };

/* the settings screen's right panel: one line per row (FINAL-frontend.md 6.5) */
static const char *const SETTINGS_HELP[] = {
  "HOW LONG UNTIL CLOSING. THE DISHES DO NOT CARE.",
  "MEMBERS FILL EMPTY SEATS. ONE OF THEM TALKS.",
  "HOW OFTEN THINGS GO WRONG. CALM IS SUSPICIOUS.",
  "SCAN THE CODE TO JOIN. NO APP. NO ACCOUNT.",
  "THE BAND PLAYS LIVE. IT CANNOT BE FIRED.",
  "BEEPS. THE GOOD KIND.",
  "108 W MAIN. THE ONLY ROOM. FOR NOW.",
  "THE RULES FLASH AND THE COACH. FIRST NIGHT IS THE DEFAULT.",
  "BACK TO HOW IT WAS. LIKE THE WOOD SHOP.",
  "BACK TO THE MENU.",            /* DONE with TO_MAIN */
};
static const char *const SETTINGS_HELP_DONE_LOBBY = "OFF TO THE LOBBY.";

/* results: the head, eyebrow and sub per ending, picked once by end_night (6.11) */
typedef struct { const char *eyebrow, *head, *sub; } end_variant_t;
static const end_variant_t END_FIRE_V[3] = {
  { "HYH IS CANCELED", "THE FIRE MARSHAL HAD OPINIONS.", "ALL CLOUT IS VOID. THEY COUNTED THE POWER STRIPS." },
  { "CLUTTER: 100",    "THE FREE PILE WON.",             "IT WAS ALWAYS GOING TO. ALL CLOUT IS VOID." },
  { "EGRESS: BLOCKED", "NOBODY COULD REACH THE DOOR.",   "ALL CLOUT IS VOID. THE DOOR IS BEHIND A CRT." },
};
static const end_variant_t END_BURN_V[3] = {
  { "VIBE: 0", "THE SAME THREE PEOPLE DID EVERYTHING.", "THEN THEY LEFT. THE KNOWLEDGE LEFT WITH THEM." },
  { "VIBE: 0", "EVERYONE WENT HOME TO THEIR GARAGE.",   "ALL CLOUT IS VOID. THE SINE WAVE WAITS TWO YEARS." },
  { "VIBE: 0", "THE MAILING LIST WON.",                 "200 MESSAGES. NOBODY CAME IN. ALL CLOUT IS VOID." },
};
static const end_variant_t END_OK_V[2] = {
  { "%s. LIGHTS OUT.",      "THE SPACE SURVIVES ANOTHER TUESDAY.", "CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES." },
  { "RENT IS PAID. BARELY.", "THE LAB OPENS TOMORROW.",             "BECAUSE SOMEBODY SWEPT. SEE YOU THURSDAY." },
};

static const char *const COOP_TIER[4] = { "CLOSED", "BARELY", "OPEN TOMORROW", "SPOTLESS" };
static const char *const COOP_LETTER[4] = { "C", "B", "A", "S" };
static const uint32_t COOP_COLOR[4] = { C_FAULT, C_RUST, C_HAZARD, C_LIVE };
static const char *const COOP_BANNER = "THE REAL WINNER: EVERYONE WHO SWEPT.";
static const char *const VERSUS_LOST_BANNER = "NOBODY WINS. THE FLOOR IS STILL DIRTY.";

static const int END_VARIANT_COUNT[4] = { 0, 2, 3, 3 };

#endif
