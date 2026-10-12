/* All the words in the game. Plain maker voice: no dashes, no emoji.
 * Edit freely; nothing here is load-bearing except array lengths. */
#ifndef CONTENT_H
#define CONTENT_H

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char *const TAGLINES[] = {
  "WHOEVER DOES IT, DECIDES.",
  "NOBODY DOES THE DISHES.",
  "COMMUNICATION IS NOT PERMISSION.",
  "LEAVE IT BETTER.",
  "BE EXCELLENT TO EACH OTHER.",
  "HACKING THE PLANET SINCE 2009.",
  "OPEN SOURCE. OPEN DOORS.",
  "A JANITORIAL CO-OP WITH CLASSROOMS ATTACHED.",
};

/* The attract loop. The floor-cleaning loop is real; it has never exited. */
static const char *const ATTRACT_TIPS[] = {
  "1. WE SHOULD CLEAN THE FLOORS.",
  "2. TESTING ENSUES.",
  "3. GET A QUOTE. IT IS $4,000.",
  "4. WHY DON'T WE CLEAN THEM OURSELVES?",
  "5. RETURN TO STEP 1.",
  "CLOUT IS FOR YOU. KARMA IS FOR THE SPACE.",
  "IF THE SPACE CLOSES, ALL CLOUT IS VOID.",
  "THE LASER IS ALWAYS BOOKED. SIR CUTSALOT IS ALWAYS FREE.",
  "A NEWBIE DECIDES IN FIVE MINUTES WHETHER TO COME BACK.",
  "THE FIRE MARSHAL HAS OPINIONS ABOUT POWER STRIPS.",
  "THERE ARE NO EMPLOYEES. ONLY YOU.",
  "MEET ON TUESDAY. END OF DISCUSSION.",
};

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

/* What the laser makes. All real HeatSync laser lore or close to it. */
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
  "A NEWBIE DECIDED NOT TO COME BACK.",
  "A NEWBIE FOUND A MAKERSPACE WITH A FRONT DESK.",
  "A NEWBIE WENT HOME TO THEIR GARAGE.",
  "NOBODY SAID HI. THE NEWBIE LEFT.",
};

static const char *const NEWBIE_JOINS[] = {
  "NEW MEMBER! THEY WILL BE CLEANING NOW.",
  "WELCOMED. THEY ASK HOW THEY CAN HELP.",
  "NEW MEMBER. THEY ALREADY LIKE THE FREE PILE.",
  "WELCOMED. FIRST CLASS IS ON THE LASER, PROBABLY.",
};

static const char *const FIXED_PRINTER[] = {
  "%s LIVES AGAIN.", "%s IS BACK. GRUB SCREWS TIGHTENED.", "%s FIXED. NOBODY WILL SAY THANKS.",
};

/* End of night awards. */
static const char *const AWARD_TALKER   = "%d PROPOSALS. 0 FOLLOW-UPS.";
static const char *const AWARD_LURKER   = "PAID DUES. SAID NOTHING. KEPT THE LIGHTS ON.";
static const char *const AWARD_GLAMOUR  = "MOST CLOUT, LEAST KARMA. A CLASSIC.";
static const char *const AWARD_JANITOR  = "THE REAL WINNER. LEAVE IT BETTER.";
static const char *const AWARD_GREETER  = "SAID HI TO %d NEWBIES.";
static const char *const AWARD_MATE     = "%d CLUB-MATES. VIBRATING SLIGHTLY.";

#endif
