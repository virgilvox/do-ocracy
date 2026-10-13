# DO-OCRACY humor brief

Source pass over `src/content.h`, the `EVENTS` table in `src/game.c`, the results screen in `src/draw.c`, and the HeatSync lore (dataset, 24C3 design patterns, dynamics framework, history blog, field notes, Teardown script, voice rules). Everything below is written to the voice rules: no dashes, no emoji, no names, flat statements, caps for in-game text. Banners are under 48 characters, sub-lines under 64. Counts were checked by script.

Hard limits from the code that every line here respects:

| Surface | Buffer or width | Limit used here |
|---|---|---|
| Ticker banner (`ticker_sub` text) | F_H1 across 1280 px | 47 chars |
| Ticker sub, toasts (`game_toast`) | `char msg[96]`, F_MONO | 63 chars |
| Award one-liner | `char award[][64]` after `snprintf` | 63 chars after the `%d` is filled |
| Proposal bubble | panel grows to fit, min 190 px | 40 chars so it stays on screen at the right edge |
| Item and donation names | shown inside `"%s SHOWED OFF: %s"` and `"DONATED: %s"` | 40 chars |
| Results head | F_H1, drops to F_H2 over 1160 px | 47 chars |

## 1. What is funny about hackerspaces

The engine under every joke: people came for the laser and the Tesla coil, and the place survives on floors, dishes and the fridge. Comedy is the gap between the reason you joined and the reason it is still open, played completely straight. The funniest line in the whole corpus is a statistic read flat: 1,716 scheduled hours of cleaning and maintenance, deliberately kept off the wall posters. So the house joke format is a plain fact that is also an indictment. No winking, no exclamation marks unless the game is literally shouting an alarm.

Rules for the punch-up, in priority order:

1. Flat and declarative. The joke lives in the fact, not the delivery. "THE FREE PILE ONLY GROWS." beats "LOL the free pile is huge."
2. Short, punchline last. Cut the setup until the line barely stands up.
3. Punch up at the institution and at the veteran players. Never at the newbie. The newbie is the pipeline and decides in five minutes; the game is allowed to make the player feel bad for not saying hi, never to mock the newbie.
4. Specific beats generic. "GRUB SCREW" beats "FIXED IT." "FOUR POWER STRIPS DEEP" beats "UNSAFE."
5. Callbacks are allowed once per screen. The floor loop, "all clout is void" and "nobody does the dishes" are the three recurring bits. Do not stack them in one line.
6. Roles only. A founder, a board member, the landlord, the fire marshal. No real people, no governance fights, nothing from private channels.
7. Safety lines stay flat and unfunny when they are real rules. The daisy chain is a joke target because the fix is one action. Nothing about the laser hood or the table saw gets a punchline.

What the sources say is funny, topic by topic:

- The floor-cleaning loop. Five steps that never exit: we should clean the floors, testing ensues, the quote is $4,000, why not do it ourselves, return to step one. Comedy of recursion. The loop is the attract screen and should stay numbered and in order.
- The committee. In a do-ocracy, forming a committee is how a room says no without anyone saying no. Pair with the Bikeshed anti-pattern: propose anything simple and everyone argues about its colour while the hard task sits there.
- Glamour work versus dishes. Fun visible work is claimed in seconds; cleaning, admin and documentation pile up on the same three people. Both counters climb, so the spreadsheet looks healthy right up to the resignation. This is the clout versus karma split and the whole scoring joke.
- Free Pile physics. Mass enters at the front door and leaves only by evaporation. The 2007 pattern says announce disposal three times with escalation and then throw the rest away; nobody ever gets to the third notice. "Someone will want that. Someday."
- The four year sine wave. Enthusiasm follows a sine curve with a four year period. Members drift away for no reason at all; it is just math. Keep the space running through the trough.
- The landlord. The ideal landlord is benevolent and uninterested. A walkthrough is ten seconds of looking normal.
- The fire marshal. Has opinions about power strips, daisy chains and whether you can reach the door. Clutter 100 is the fire marshal; the game already frames it that way.
- Club-Mate. Buy a pallet, sell it, stay awake. An acquired taste nobody has acquired. Vibrating slightly.
- Printers. There are always two, one is always down, and the question is always "any particular reason why it is down." It is always the grub screw. Spaghetti is the natural end state of an unattended print.
- The laser queue. The laser is always booked. The vinyl cutter is always free and nobody wants it.
- Communication is not permission. You never need permission, you always say what you are about to do. Telling the channel is not the same as asking the room. The patch is one habit: coordinate before, do not apologise after.
- The vending machine. The Club-Mate pattern's cousin. It eats a dollar, you rock it, the honour jar contains a note and a washer.
- Haboobs. A wall of desert arrives and is now inside, including inside the laser.
- 118 degrees. The air conditioner weeps on the floor. Everyone slows down. Mop it.
- Linux night canceled. A standing joke in every space: the most reliably scheduled event is the one that does not happen.
- The wiki nobody updates. 939 pages, and the most edited one is the rules page. There is always a page that says to update the page.
- Discord versus the mailing list. Text escalates, presence heals. A 200 message thread resolves in one calm evening in a room; the thread is never the space. 87 of 117 channels were sewing, cats, lockpicking and lasers.
- The person who labels everything. The label maker is governance by other means. Eventually they label the label maker.
- The mystery fasteners. One bin, metric and imperial mixed, never to be sorted, and someday somebody will need exactly one of them.
- Bonus bits worth using: Meet on Tuesday, end of discussion. Can I sleep at the lab (no, the roommate anti-pattern). The plant died (every space's plant dies). The dishwasher is the single most important machine. The first project night was potato cannons. Coffee and coworking is older than the laser.

## 2. Punch-up pass

Everything below is drop-in for `content.h` unless marked NEW, which means it needs a counter, an effect enum or a station that does not exist yet.

### 2.1 Taglines (title screen, rotates every 3.5 s, F_H3, under 48)

```
WHOEVER DOES IT, DECIDES.
NOBODY DOES THE DISHES.
COMMUNICATION IS NOT PERMISSION.
A JANITORIAL CO-OP WITH CLASSROOMS.
THE LASER IS BOOKED. THE MOP IS FREE.
MEET ON TUESDAY. END OF DISCUSSION.
RENT IS DUE. VIBES ARE NOT A CURRENCY.
THE FREE PILE ONLY GROWS.
STEP 5: RETURN TO STEP 1.
IF THE LAB CLOSES, YOUR CLOUT IS VOID.
SOMEBODY HAS TO. IT IS YOU.
LINUX NIGHT IS CANCELED. PLAY ANYWAY.
BE EXCELLENT. ALSO SWEEP.
THE WIKI SAYS TO UPDATE THE WIKI.
```

Drop "HACKING THE PLANET SINCE 2009." and "OPEN SOURCE. OPEN DOORS." from the rotation; they are brand lines, not jokes, and the voice rules keep them for real surfaces.

### 2.2 Proposals (X button bubble, shown under "I THINK WE SHOULD", under 40)

```
...FORM A COMMITTEE ON COMMITTEES
...GET A BIGGER LASER
...PAINT THE BATHROOM PURPLE
...MAKE A CHANNEL FOR THAT
...GET A QUOTE ON THE FLOORS
...BUY A PALLET OF CLUB-MATE
...LABEL THE LABEL MAKER
...SORT THE MYSTERY FASTENERS
...MOVE THE WOOD SHOP BACK
...WRITE A WIKI PAGE NOBODY READS
...VOTE ON IT AT HYH. NEXT HYH.
...HAVE A PIZZA BUDGET
...GET A GLOWFORGE (NO)
...RUN A CLASS ON RUNNING CLASSES
...BUY A BRIDGEPORT. CARRY IT IN.
...THROW OUT THE FREE PILE (RIOT)
...CLEAN THE FLOORS OURSELVES
...HOLD LINUX NIGHT FOR REAL
...NAME THE NEW PRINTER
...MOVE THE MEETING TO WEDNESDAY (NO)
```

Twenty lines, pick fifteen or keep all. The bubble is the Talker's whole move: it must never grant karma or clout, only `p->proposals++`.

### 2.3 Events (ticker banner under 48, sub under 64)

Format: `BANNER | SUB | effect | weight`. Existing effects reuse the enum in `game.c`. NEW effects are described after the table.

```
IT IS 118 OUTSIDE. | THE AC IS CRYING ON THE FLOOR. MOP IT. | E_HEAT | 3
HABOOB. | THE DESERT IS NOW INSIDE. SWEEP IT BACK OUT. | E_HABOOB | 3
SOMEBODY BROUGHT TACOS. | VIBE UP. DISHES UP. SAME NUMBER, OPPOSITE SIGN. | E_TACOS | 3
LANDLORD WALKTHROUGH. TEN SECONDS. | LOOK NORMAL. CLUTTER UNDER 50. | E_LANDLORD | 3
DAISY CHAIN DETECTED. | FOUR POWER STRIPS DEEP. UNPLUG IT. | E_DAISY | 3
DONATION ON THE DOORMAT. | NOBODY ASKED FOR IT. CARRY IT TO THE FREE PILE. | E_DONATION | 3
ANY PARTICULAR REASON DR. STRANGE IS DOWN? | SAME REASON AS LAST WEEK. HOLD ON IT TO FIX. | E_STRANGE | 2
SOMETHING IN THE FRIDGE HAS A PULSE. | SOMEBODY HAS TO. HOLD ON THE FRIDGE. | E_FRIDGE | 3
PRINT LEFT RUNNING OVERNIGHT. | IT IS SPAGHETTI NOW. TRASH IT. | E_SPAGHETTI | 2
COFFEE AND COWORKING RAN LONG. | OLDER THAN THE LASER. GOOD VIBES. | E_COFFEE | 2
LINUX NIGHT: CANCELED. | AS FORETOLD. NOTHING HAPPENS. | E_NONE | 1
A FIFTH THURSDAY?! | BLASPHEMY. ALSO IT IS TUESDAY. NOTHING HAPPENS. | E_NONE | 1
QUIET NIGHT. | SUSPICIOUS. ENJOY IT. | E_NONE | 1
THE MAILING LIST IS ON FIRE. | 200 MESSAGES. NOBODY HERE IS IN IT. VIBE DIPS. | E_THREAD (NEW) | 2
SOMEONE LABELED EVERYTHING. | INCLUDING THE LABEL MAKER. CLUTTER EASES. | E_LABELS (NEW) | 1
FREE PILE PURGE: THIRD AND FINAL NOTICE. | THE FIRST TWO WERE IMPLIED. PILE EMPTIES IN 20. | E_PURGE (NEW) | 1
THE WOOD SHOP USED TO BE RIGHT HERE. | IT DID. THREE TIMES. VIBE DIPS. | E_NOSTALGIA (NEW) | 1
CAN I SLEEP AT THE LAB? | NO. NOTHING HAPPENS. VIBE HOLDS. | E_NONE | 1
```

NEW effects, one line each:

- E_THREAD: `G.vibe -= 8`. Optional: if two or more players are within 2 units of each other when it fires, halve the hit (presence heals).
- E_LABELS: for 20 seconds, floor items count 0.5 weight toward clutter. Visual: a bone-coloured label sprite on each item.
- E_PURGE: after 20 seconds every item sitting in the Free Pile is destroyed, clutter drops, `G.vibe -= 3`, toast "SOMEONE WANTED THAT. TOO LATE."
- E_NOSTALGIA: `G.vibe -= 4`, nothing else. It is a joke about the Founder archetype and the three wood shop moves in the history.

The `if (e == G.event) e = (e + 1) % COUNT(EVENTS)` guard means consecutive events always differ; keep weights so E_NONE lines stay rare.

### 2.4 Awards (name on the first line, one-liner on the second, both F_MONO_S, one-liner under 64 after formatting)

Existing awards and their counters are in `results_screen()`. The first six are drop-in. The last four need a new `int` on `player_t` and an increment at the call site named.

```
JANITORIAL CO-OP | MOST KARMA. THE LAB OPENS TOMORROW BECAUSE OF YOU. | p->karma (exists)
GLAMOUR BIAS | MOST CLOUT, LEAST KARMA. THE SPREADSHEET LOOKED FINE. | p->clout (exists)
TALKER | %d PROPOSALS. ZERO FOLLOW-UPS. A RECORD. | p->proposals (exists)
GREETER | SAID HI TO %d NEWBIES. YOU ARE THE FRONT DESK NOW. | p->greeted (exists)
CLUB-MATE SOMMELIER | %d CLUB-MATES. CAN HEAR COLOURS. STILL SWEEPING. | p->mates (exists)
LURKER | PAID DUES. TOUCHED NOTHING. THE LIGHTS STAYED ON. | lowest tasks (exists)
FREE PILE CURATOR | SHELVED %d DONATIONS. NONE WILL EVER LEAVE. | NEW p->shelved, ++ on W_DEPOSIT to ST_FREEPILE
PRINTER WHISPERER | FIXED %d PRINTERS. NOBODY SAID THANKS. WE JUST DID. | NEW p->fixes, ++ where FIXED_PRINTER fires
MARSHAL'S FAVORITE | UNPLUGGED %d DAISY CHAINS. THE LAB STILL EXISTS. | NEW p->unplugged, ++ where IT_DAISY is trashed
DISH PIT | %d DISHES. NOBODY DOES THE DISHES. YOU DID. | NEW p->dishes, ++ per sink load
BIKESHED | ARGUED ABOUT THE COLOUR. THE SHED IS UNBUILT. | proposals >= 5 and tasks == 0, no counter needed
```

Keep the award priority order as it is in the code (karma first, then clout, then the rest) so one player never gets two. The "THE REAL WINNER: %s. LEAVE IT BETTER." banner stays; it is the one sincere line on the screen and the joke needs it.

### 2.5 Newbie lines (toasts, under 64)

Leaves, red:

```
A NEWBIE LEFT. FIVE MINUTES. NOBODY SAID HI.
A NEWBIE FOUND A MAKERSPACE WITH A FRONT DESK.
A NEWBIE WENT HOME TO A CLEANER GARAGE.
A NEWBIE SAW THE FRIDGE AND MADE A CHOICE.
A NEWBIE ASKED WHERE TO START. SILENCE. GONE.
```

Joins, green:

```
NEW MEMBER. THEY ASKED HOW TO HELP. OH NO.
NEW MEMBER. HANDED A BROOM WITHIN SECONDS.
NEW MEMBER. ALREADY EYEING THE FREE PILE.
NEW MEMBER. FUTURE BUILDER. CURRENT SWEEPER.
NEW MEMBER. THEY THINK THE LASER IS AVAILABLE.
```

The arrival toast "A NEWBIE WALKED IN. SOMEBODY SAY HI." stays as is. It is an instruction, not a joke, and it is the one line that teaches the greeting mechanic.

### 2.6 Toasts for scoring moments (under 64, fire at the call site named)

```
DISHES DONE. THE MOST IMPORTANT MACHINE WAS YOU. | sink load finished
SWEPT. THE DESERT WILL BE BACK. | sawdust or dust trashed
MOPPED. THE AC WILL CRY AGAIN. IT ALWAYS DOES. | puddle trashed
SPAGHETTI TRASHED. IT WAS GOING TO BE A BENCHY. | spaghetti trashed
SHELVED. SOMEONE WILL WANT THAT. SOMEDAY. NOT YOU. | donation to Free Pile
UNPLUGGED. SOMEWHERE A FIRE MARSHAL RELAXES. | daisy chain trashed
%s LIVES. GRUB SCREW. IT IS ALWAYS THE GRUB SCREW. | printer fixed (FIXED_PRINTER, %s = label)
FRIDGE CLEARED. DO NOT ASK. DO NOT LOOK BACK. | fridge hold finished
OVER 9000 CLOUT. THE FLOOR IS STILL DIRTY. | clout crosses 9000
YOU SAID HI. YOU ARE THE FRONT DESK NOW. | newbie greeted
CLUB-MATE. ACQUIRED TASTE. NOBODY HAS ACQUIRED IT. | mate picked up
CUT DONE. THE QUEUE BEHIND YOU WOULD LIKE A WORD. | laser finishes
```

FIXED_PRINTER variants to replace the three in `content.h` (each takes one `%s`):

```
%s LIVES. GRUB SCREW. IT IS ALWAYS THE GRUB SCREW.
%s IS BACK. NOBODY WILL SAY THANKS. WE JUST DID.
%s FIXED. IT WILL JAM AGAIN BY THURSDAY.
```

### 2.7 Donations (name under 40, heavy flag as in `donation_t`)

```
A CRT. IT WEIGHS A CRT. | 1
THREE PRINTERS. ALL BROKEN. ALL FIXABLE. | 1
BOX OF WALL WARTS. NONE MATCH ANYTHING. | 0
BIN OF MYSTERY FASTENERS. MIXED UNITS. | 0
CHEST FREEZER. RUNS FINE. SMELLS FINE. | 1
HALF A TON OF STEEL. FREE. SO FREE. | 1
A RAID ARRAY FROM 2009. STILL SPINNING. | 0
A BAG OF VGA CABLES. FOR THE FUTURE. | 0
A LASER PRINTER. NOT THAT KIND OF LASER. | 1
A PLANT. IT WILL DIE HERE. | 0
```

Heavy items halve walk speed (`speed *= 0.55f`), so the heavy flag is itself the joke: the CRT line is funnier because the player is slow while reading it.

### 2.8 Project names per station (under 40, eight each)

Laser (kind 0, `LASER_PROJECTS`):

```
LASER MUSTACHE
TARDIS, SLIGHTLY TOO BIG
FINGER JOINT BOX, NO LID
NIXIE CLOCK FACE
LIGHTNING OCTOPUS
400 BUSINESS CARDS
COOKIE DOUGH (DO NOT)
WOOD ETCH. SMELLS LIKE CAMPFIRE.
```

Printers (kind 1, `PRINT_PROJECTS`):

```
BENCHY NUMBER 300
SPARE PART FOR THE OTHER PRINTER
FIDGET THING
CTHULHU IN A TOP HAT
CALIBRATION CUBE, OFF BY 0.2
ARCADE BUTTON BEZEL
A TINY LASER
A PRINTER PART FOR A PRINTER
```

Electronics bench (kind 2, `BENCH_PROJECTS`):

```
BLINKY BADGE
THEREMIN THAT HOWLS
TESLA COIL (SMALL) (LOUD)
NIXIE CLOCK, WRONG TIME
BOATDUINODE
SMOKE SIGNAL TWEETER
555 TIMER DOING SOMETHING
A BADGE WITH TOO MANY LEDS
```

Sir Cutsalot (kind 3, `STICKERS`):

```
I VOIDED THE WARRANTY
HACK THE PLANET
MADE IN MESA
OVER 9000
COMMUNICATION IS NOT PERMISSION
ASK ME ABOUT THE FREE PILE
THIS MACHINE KILLS CLUTTER
DO NOT DAISY CHAIN
```

Forge (NEW station, see section 3):

```
A HOOK
ANOTHER HOOK
A KNIFE (IT IS A HOOK)
BOTTLE OPENER
RAILROAD SPIKE SOMETHING
A LEAF
TONGS TO MAKE MORE TONGS
A VERY HOT NAIL
```

Sewing (NEW station):

```
CON BADGE LANYARD
PATCH FOR THE PATCH
LED HOODIE
BAG FOR A PROJECT BAG
CAPE (FOR A CAT)
HEMMED ONE LEG
A PLUSH TARDIGRADE
POKEJEANS
```

Wood shop (if the lumber rack becomes a project station):

```
A BOX
A SLIGHTLY BIGGER BOX
CUTTING BOARD. GIFT.
SHELF FOR THE FREE PILE
A WOBBLY STOOL
A SIGN THAT SAYS SIGN
BIRDHOUSE. NO BIRDS.
SPEAKER CABINET, ONE SPEAKER
```

### 2.9 Attract-mode one-liners (bottom tape, F_MONO, under 48)

Keep the five numbered floor-loop lines first and in order; `ATTRACT_TIPS` is indexed by `T / 3.0` so the sequence reads as a loop. Then these:

```
THE FLOOR LOOP HAS NEVER EXITED.
CLOUT IS FOR YOU. KARMA IS FOR THE SPACE.
THE LASER IS BOOKED. SIR CUTSALOT IS FREE.
A NEWBIE DECIDES IN FIVE MINUTES. SAY HI.
THE FIRE MARSHAL COUNTS POWER STRIPS.
NO STAFF. NO FRONT DESK. ONLY YOU.
1,716 HOURS OF CLEANING. NOT ON THE POSTER.
COORDINATE BEFORE. DO NOT APOLOGIZE AFTER.
```

The 1,716 figure is verified in the dataset (scheduled cleaning and maintenance hours, 2010 to 2025). It is the only number allowed on the attract screen besides the $4,000 quote.

### 2.10 End screens (eyebrow, head under 48, sub under 64)

Fire marshal (END_FIRE):

```
HYH IS CANCELED | THE FIRE MARSHAL HAD OPINIONS. | ALL CLOUT IS VOID. THEY COUNTED THE POWER STRIPS.
CLUTTER: 100 | THE FREE PILE WON. | IT WAS ALWAYS GOING TO. ALL CLOUT IS VOID.
EGRESS: BLOCKED | NOBODY COULD REACH THE DOOR. | ALL CLOUT IS VOID. THE DOOR IS BEHIND A CRT.
```

Burnout (END_BURNOUT):

```
VIBE: 0 | THE SAME THREE PEOPLE DID EVERYTHING. | THEN THEY LEFT. THE KNOWLEDGE LEFT WITH THEM.
VIBE: 0 | EVERYONE WENT HOME TO THEIR GARAGE. | ALL CLOUT IS VOID. THE SINE WAVE WAITS TWO YEARS.
VIBE: 0 | THE MAILING LIST WON. | 200 MESSAGES. NOBODY CAME IN. ALL CLOUT IS VOID.
```

Survived (END_SURVIVED), for variety:

```
11:00 PM. LIGHTS OUT. | THE SPACE SURVIVES ANOTHER TUESDAY. | CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES.
RENT IS PAID. BARELY. | THE LAB OPENS TOMORROW. | BECAUSE SOMEBODY SWEPT. SEE YOU THURSDAY.
```

Pick one per end by `rnd()` at `end_night()` and store the index in `G`, so the screen does not flicker between variants each frame.

## 3. New station ideas

Six primary, four spare. Each has a mechanic in one line and the joke it carries. All fit the existing `station_t` shape (state, timer, count, dust, smell) except where noted.

1. Vending machine (lounge wall, by the couch). Mechanic: press A for a Club-Mate (small speed boost, `p->mates++`); one in four purchases jams, the machine shows ATE A DOLLAR, and holding A rocks it free for karma. Joke: the honour jar next to it contains a note and a washer. Toast on a jam: "IT ATE A DOLLAR. AGAIN. ROCK IT."
2. The forge (alley door, outside line). Mechanic: hold to heat, then a short press makes a project; the heat event halves the forge timer because it is already 118 out there. Joke: everything you make is a hook. Project list in 2.8. Toast on first use: "THE FORGE IS LIT. IT IS NOW 140 IN THE ALLEY."
3. Sewing machines (front window, beside Show and Tell). Mechanic: fast, quiet projects worth low clout, but a newbie who sees a sewing machine in use gets a longer patience timer before leaving, because the room looks welcoming. Joke: the most used tool nobody mentions. 87 of 117 channels were sewing, cats, lockpicking and lasers. Toast: "SOMEBODY IS SEWING. THE ROOM LOOKS FRIENDLIER."
4. The whiteboard (by the work island). Mechanic: every proposal a player makes near it adds a line to the board; a full board counts as clutter; wiping it is a hold action for karma, but the corner says DO NOT ERASE and wiping that corner costs vibe. Joke: "DO NOT ERASE. DATED 2019." Toast on wipe: "WHITEBOARD WIPED. SIX COMMITTEES DISSOLVED."
5. The bathroom (back right corner, small door). Mechanic: the single highest karma task in the game, a long hold, nobody is ever drawn toward it, and every newbie checks it first; a dirty bathroom shortens newbie patience. Joke: a newbie judges the whole lab by this room, and that is fair. Toast: "BATHROOM DONE. +KARMA. NO WITNESSES."
6. Member storage shelves (left wall, replacing or beside OB_SHELF). Mechanic: bins fill with items over the night; a bin over capacity spills onto the floor; a player can post a notice (press A) and after three notices the bin converts to Free Pile donations. Joke: "THIRD AND FINAL NOTICE. FIRST AND SECOND WERE IMPLIED." Toast on spill: "A BIN GAVE UP. IT WAS LABELED MISC."

Spares:

7. Haboob cleanup, as a station rather than an event: the alley door blows open and dust spawns until someone holds it shut. Toast: "THE DESERT WOULD LIKE TO COME IN."
8. The library (shelf by the couch). Books in, never out; standing near it slowly restores vibe. Toast: "40 BINDERS OF DATASHEETS. ONE HAS BEEN READ."
9. The bike rack (front, outside). Newbies arrive on bikes in the heat; a shaded rack lengthens their patience. Toast: "A NEWBIE LOCKED UP. IT IS 118. BE FAST."
10. The fridge science experiment, extending the fridge: the smell grows in three stages and at stage three it gets a name. Toast: "IT HAS A NAME NOW. HOLD ON THE FRIDGE."

## 4. What an implementer must not get wrong

- Every `%d` and `%s` in an award or FIXED_PRINTER line is consumed by an `snprintf` at a fixed call site. Keep exactly one specifier where the code passes one argument, and none where it passes none (AWARD_LURKER, AWARD_GLAMOUR, AWARD_JANITOR are passed as plain `%s`).
- `award[MAX_PLAYERS][64]` truncates silently. Every award one-liner here is 63 or fewer after a two digit number is filled.
- Toasts and ticker subs share `char msg[96]`; the HUD, not the buffer, is the real limit. 63 characters keeps F_MONO inside the 1152 px rows.
- Ticker banners render at F_H1; over 47 characters they clip or wrap. Results heads over 1160 px drop to F_H2 automatically, but keep them under 48 anyway.
- `ATTRACT_TIPS` is indexed by time, so the five floor-loop steps must stay first and in order.
- Station labels PEWPEW 9000, LOAFER, DR. STRANGE and SIR CUTSALOT are the lab's tool nicknames in `LAYOUT` and are referenced by text in events and draw.c. Do not rename them in one place only.
- `E_NONE` events are the pacing jokes; keep their combined weight at or below a quarter of the table or the night feels empty.
- Heavy donations set `it->heavy` from `DONATIONS[].heavy` and slow the carrier; keep roughly half heavy.
- The roommate line, the sine wave line, the committee line and the labels line are all 24C3 pattern jokes and general hackerspace culture. Nothing in this brief references private governance, named people or the 2025 to 2026 conflict, and nothing added later should.
- No dashes of any kind in any string, including the hyphen used as a dash with spaces around it. Hyphens inside compound words (CLUB-MATE, DO-OCRACY, CO-OP, FOLLOW-UPS) are fine.
- Safety lines stay flat. If a line about the laser hood, the table saw or certification is ever added, it gets no punchline.
