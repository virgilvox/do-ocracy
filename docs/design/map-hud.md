# DO-OCRACY HUD map

Technical map of the 2D layer. Source of truth is `src/draw.c` (778 lines), `src/hud.c` (347), `src/hud.h` (64), `src/content.h` (110). Line numbers below refer to those files as read on 2026-10-12. Cross references to `game.c`, `game.h` and `main.c` are given where draw.c reads state it does not own.

## 1. Rendering model

- The HUD is a 1280x720 RGBA buffer (`HUD_W`, `HUD_H`, hud.h:9-10) drawn over the 3D scene every frame. Rows are stored bottom up because the buffer is the texel array of a GL data texture (hud.h:38, hud.c:68, main.c:121-124: after init `hud_px` points straight at the texture's pixels).
- `draw_hud(float dt)` (draw.c:752-778) is the single entry point. Order of operations:
  1. `T += dt` (the file-local animation clock, draw.c:16) and `memcpy` of the baked vignette into the whole buffer (draw.c:755). So the frame always starts from a soft dark edge, never from a clear buffer. The vignette itself is baked once in `bake_vignette` (draw.c:739-750): colour 0x0a0806, alpha `a*a*170`, elliptical falloff centred slightly below middle.
  2. Title or attract: `fx_draw`, `title_screen`, return (draw.c:756-760). Nothing else is drawn, so no station labels, no toasts, no player cards during attract even though three bots are playing a real night underneath (main.c:364-370).
  3. Results: `fx_draw`, `results_screen`, return (draw.c:761).
  4. Lobby and night: reset the label placement list, reserve the top rows (0,0,1280,104) and the player card strip (0,634,1280,86) (draw.c:762-764), then `goal_guides`, `station_labels`, `floor_tags`, `npcs_overlay`, `fx_draw`, `top_bar`, then either `lobby_overlay` or `join_strip(944,6)`, then `danger()` and if not in danger `ticker_band()` and if no ticker `coach()`, then `player_cards`, `toasts(16, 620)`, and last `players_overlay` so the player's own name, prompt and progress are never covered (draw.c:765-777).
- Projection: `project()` (draw.c:18-25, integer out, exported) and `projf()` (draw.c:27-34, float out, static). Both reject points with camera-space z > -0.1.
- Phase enum: `PH_TITLE, PH_LOBBY, PH_NIGHT, PH_RESULTS` (game.h:18). End enum `END_NONE, END_SURVIVED, END_FIRE, END_BURNOUT` (game.h:20). `G.attract` (game.h:140) is true while bots demo a night on the title screen.
- Flow (main.c:342-375): title waits 1.5 s then adds three bots and starts an attract night. Any human A or START ends attract and goes to lobby (game.c:234-240, 264). Lobby auto-starts at 25 s or on START (main.c:350). Results returns on any press after 4 s or at 40 s (main.c:360). Night length defaults to 180 s (game.c:939).

## 2. Fonts (hud.c:46-62)

| id | enum | face | px | atlas |
|---|---|---|---|---|
| 0 | `F_TITLE` | Archivo Black | 128 | 2048x1024 |
| 1 | `F_H1` | Archivo Black | 64 | 1024x512 |
| 2 | `F_H2` | Archivo Black | 40 | 1024x512 |
| 3 | `F_H3` | Archivo Black | 26 | 1024x256 |
| 4 | `F_MONO_L` | IBM Plex Mono Bold | 30 | 1024x256 |
| 5 | `F_MONO` | IBM Plex Mono SemiBold | 21 | 1024x256 |
| 6 | `F_MONO_S` | IBM Plex Mono Bold | 15 | 1024x256 |
| 7 | `F_LOGO` | Charis SIL Bold | 44 | 1024x512 (baked, never used in draw.c) |

Assets are loaded by name through `wc_asset_size` / `wc_load_asset` (hud.c:23-30). Only ASCII 32..127 is baked (`stbtt_BakeFontBitmap(..., 32, 96, ...)`, hud.c:38); any other byte renders as `?` (hud.c:137, 152). There is no kerning, no outline, no bold synthesis; one weight per slot. `hud_font_h` returns the requested pixel size, not the line box (hud.c:129). `hud_text` places y as the top of the line box and adds `ascent` (hud.c:143-149), so cap height begins roughly a third of the way down.

Atlas sizes are fixed per pixel size band (hud.c:35-36). A new font size above 80 px costs 2 MB of atlas; 40 to 79 px costs 512 KB.

## 3. Colour tokens (hud.h:12-33)

GANTRY tokens copied from `new-hsl/src/styles/tokens.css`: `C_CHAR 0x15120f`, `C_GRIME`, `C_SLAB 0x26211b`, `C_SLAB_HI`, `C_STEEL`, `C_STEEL_HI 0x4a4338`, `C_BONE 0xece3d3`, `C_ASH 0xa79d8c`, `C_SMOKE 0x756c5e`, `C_HAZARD 0xf2ab1e`, `C_HAZARD_DP`, `C_RUST 0xc2542c`, `C_LIVE 0x5fb892`, `C_FAULT 0xd24f3a`, `C_VIOLET`, `C_INFO`, `C_TAPE 0x14110d`, `C_SHADOW 0x0a0806`, `C_INK_OK 0x7fd0aa`, `C_INK_ERR 0xef7b68`, `C_LOGO_SUN 0xf99a1c` (the mark's own orange, never used for UI, hud.h:33).

Player colours `PCOL[8]` (game.c:21): hazard, live, violet, info, rust, bone, pink 0xe87fa8, sky 0x8fbde6. Note slot 0 is the same amber as `C_HAZARD` and slot 5 is bone, so player colour is not a safe way to distinguish a player chip from a UI accent.

## 4. HUD primitives (hud.h:40-62, bodies in hud.c)

| call | hud.c | what it does |
|---|---|---|
| `hud_init()` | 46-62 | allocs buffer, loads four TTFs, bakes 8 fonts |
| `hud_clear()` | 64 | memset 0 (not used by draw.c; the vignette memcpy replaces it) |
| `blend(x,y,rgb,a)` static | 66-77 | per pixel source-over with bounds check; the only pixel writer apart from the opaque rect fast path |
| `hud_rect(x,y,w,h,rgb,a)` | 79-93 | clipped fill; `a>=255` uses a 32 bit row fill fast path, otherwise per-pixel blend |
| `hud_frame(x,y,w,h,t,rgb,a)` | 95-100 | four rects, a square-cornered border of thickness t |
| `hud_panel(x,y,w,h,bg,border)` | 103-107 | GANTRY card: 6 px solid `C_SHADOW` offset, opaque fill, 2 px frame. Always opaque, always 6 px shadow, always 2 px rule |
| `hud_tape(x,y,w,h,phase)` | 110-120 | 45 degree caution stripe, 24 px hazard then 24 px tape, scrolling by `phase` pixels, framed 2 px tape. Per-pixel blend so it is slow across 1280 px wide bands (used at 24 px tall only) |
| `hud_bar(x,y,w,h,frac,fill,bg)` | 122-127 | bg box with 2 px inset fill |
| `hud_font_h(font)` | 129 | pixel size |
| `hud_text_w(font,s)` | 131-141 | advance width sum, rounded |
| `hud_text(font,x,y,s,rgb,a,align)` | 144-167 | draws one line; align `AL_LEFT/AL_CENTER/AL_RIGHT` (hud.h:36); returns width |
| `hud_text_wrap(font,x,y,w,s,rgb,a,lh)` | 169-192 | greedy word wrap, left aligned only, returns height used |
| `hud_circle(cx,cy,r,rgb,a)` | 194-201 | anti-aliased disc |
| `hud_line(x0,y0,x1,y1,t,rgb,a)` | 204-224 | anti-aliased thick segment, per pixel over its bounding box |
| `hud_chevron(x,y,dx,dy,size,t,rgb,a)` | 226-235 | two lines forming a > along (dx,dy), centred |
| `hud_ring(cx,cy,r,t,frac,rgb,a)` | 237-250 | arc from 12 o'clock clockwise through `frac` of a turn; frac 1 is a full ring |
| `hud_logo(x,y,size,flame_rgb)` | 301-308 | the HeatSync mark: sun disc in `C_LOGO_SUN`, flame and arrow in `flame_rgb`. Even-odd scanline filled from `logo_mesh.h` point rings at 4x supersampling (`fill_rings`, 255-282), cached in 6 slots by size and flame colour (284-299) |
| `hud_logo_image(size,flame,bg,bg_a)` | 310-325 | RGBA copy of the mark for the 3D sign texture, caller frees |
| `hud_qr(x,y,size,text)` | 333-347 | qrcodegen ECC medium, version 1..15, cached by text; draws bone background with a 2 module quiet zone, char modules. Module size is `size / (n+4)` floored so the drawn square is usually a little smaller than `size` |

What does not exist: no sprite or image blit (only the logo and QR), no rotated text, no outlined or shadowed text helper (draw.c fakes a shadow by drawing twice, 367-368), no clipping rectangle or scissor, no gradient, no dashed line, no rounded anything (by design), no triangle fill, no arbitrary polygon fill exposed (fill_rings is static and logo-specific), no 9-slice, no icon set, no cursor or focus-ring helper, no list or menu row helper, no tween or easing helper, no per-frame layout stack.

## 5. draw.c helpers

| helper | lines | notes |
|---|---|---|
| `label(x,y,s,fg,bg,font)` | 36-40 | centred text on a 235 alpha pill, 6 px side padding, 3 px top, no collision avoidance |
| `reserve/hits/settle` | 44-63 | the placement system, see section 6 |
| `label_av(...)` | 64-69 | same as `label` but `settle`d first |
| `eyebrow(x,y,s,c,align)` | 72-78 | GANTRY eyebrow: inserts a space between every character of `s` (so it is twice as wide as `hud_text_w(F_MONO_S, s)`), F_MONO_S. Max 75 source chars |
| `glyph_a(p)` | 81 | "SPACE" for kb seat 0, "." for kb seat 1, else "A" |
| `keycap(x,y,k,fill,ink)` | 84-91 | 24 px tall cap with 2 px shadow, min 24 wide, F_MONO_S, returns width |
| `clock_str` | 93-98 | maps night progress to 7:00 PM .. 11:00 PM |
| `slot_bar(chip,chip_c,msg,msg_c,a,dy)` | 438-454 | the single message slot at `SLOT_Y 68`, `SLOT_H 32` (436-437), centred, chip in F_H3 (steps down to F_MONO_L) and message in F_MONO (steps down to F_MONO_S) until it fits in 1248 px. 5 px shadow, 2 px frame in chip colour |
| `meter(x,y,w,name,rule,v,fill,right)` | 464-469 | eyebrow name, grey rule text beside it, right value, 16 px bar at y+20 |
| `flow_chip(x,y,s,c)` | 563-570 | 26 px tall chip with 3 px shadow and coloured frame, returns width |
| `flow_arrow(x,y,c)` | 572-575 | one chevron, returns 26 |

## 6. Label placement system (draw.c:42-69, 230, 762-764)

- `placed[192]` rects and `nplaced` (draw.c:45-46). `reserve` appends (47). `hits` is a brute force overlap test over all placed rects (48-54).
- `settle(x,y,w,h)` (55-63) tries vertical offsets `0, +1, -1, +2, -2, +3, -3` times `(h/2 + 3)` and takes the first free slot, reserving it. If all seven collide it reserves the original anyway and overlaps. Horizontal position never moves.
- Each frame `draw_hud` zeroes `nplaced` and reserves the top 104 px and the bottom 86 px (762-764) so world labels never slide under the top bar or player cards.
- Users of `settle`: `label_av` (floor tags, newbie tags) and `station_labels` directly (230, which reserves the name tag plus status line as one 24 or 48 px block). Not users: `label` (the status line under a station name is drawn with `label` at a fixed +24, 234), player name tags, goal lines, prompt boxes, pops and proposal bubbles in `players_overlay` (these are drawn last, on top of everything, 776-777), and the alarm badge above a station (235-240).
- Order matters: `goal_guides` draws first but reserves nothing; `station_labels` reserves before `floor_tags` before `npcs_overlay`, so station names always win the vertical fight.
- Known limits: the search is vertical only and in steps of about half a label height, so two labels at the same x with the same height end up stacked tightly with 3 px gap. There is no priority or age, so a label can jump between slots frame to frame as others appear or vanish (flicker risk when a player walks near a station and its status line appears, growing the reserved block from 24 to 48 px).

## 7. Particles (draw.c:100-170)

- Ring buffer `FX[360]` of `fx_t { x,y,z, vx,vy,vz, life, max, size, c, kind }` with `fx_next` wrapping (102-111). Oldest is overwritten silently.
- `fx_burst(kind, x,y,z, color)` exported (113-138). Kinds and counts: `FX_SPARK` 7 (gravity, 3 px squares, half in the given colour half hazard), `FX_KARMA` 10 rising plus signs 6 to 10 px, `FX_CONFETTI` 46 (gravity, drag, capped fall speed, 6 colour palette `CONF`), `FX_DUST` 6 soft discs, `FX_STEAM` 3 soft discs rising, `FX_JOIN` 24 in a ring at radius 4 units/s with gravity.
- `fx_draw(dt)` (140-170) integrates, applies gravity 14 to spark/confetti/join, floors at y 0.02, projects to screen, fades alpha over the last 30 percent of life. Karma draws a cross (154-159), dust and steam draw a growing third-alpha circle (160-162), confetti a 4 px tall strip with alternating width (163-165), everything else a square (166-167).
- Particles are world-space only. There is no screen-space particle (for a menu or results confetti you would have to pick a world point in view).

## 8. Screens

### 8.1 Title and attract (`title_screen`, draw.c:625-645)

Under it the camera orbits slowly (main.c:381) over a live attract night played by Builder, Talker and Founder bots. Only `fx_draw` and this screen are drawn.

| element | lines | position, size | font, colour | purpose |
|---|---|---|---|---|
| dark scrim | 626 | full screen, char at alpha 150 | | dims the 3D |
| caution tape | 627 | (0,0) 1280x24, scrolls at 30 px/s | | GANTRY header stripe |
| HeatSync mark | 628 | (64,62) 200 px, flame bone | | brand |
| "HEATSYNC LABS ARCADE" | 629 | (290,70) left | eyebrow F_MONO_S ash | eyebrow |
| "DO-OCRACY" | 630 | (282,96) left, about 128 px tall | F_TITLE bone | title |
| tagline | 631-632 | (290,242) left, changes every 3.5 s, 8 of them (content.h:8-17) | F_H3 hazard | flavour |
| how_to board | 634 | (64,292) 600x296, shown 9 of every 14 s | see 8.5 | teaches the two loops |
| controls_card | 635 | (64,300) 460x150, shown 5 of every 14 s | see 8.6 | teaches buttons |
| join_panel big | 636 | (916,300) 300x290 | see 8.7 | QR, room code |
| CTA "PRESS A OR SPACE TO PLAY" | 637-641 | (64,604) 600x56 hazard block with 6 px shadow, visible 0.85 of every 1.2 s | F_H2 tape ink | call to action |
| attract tip bar | 642-644 | (0,680) 1280x40 tape black, text centred at y 686, changes every 3 s, 12 tips (content.h:20-33) | F_MONO ash | jokes and lore |

There is no cursor, no option, no player mode, no settings. The only inputs the title reacts to are A or START on any pad, SPACE/ENTER or `.` on the keyboard seats, or a phone join; each of those creates a player and jumps to the lobby (game.c:234-240, 264; main.c:170-180).

Text density at any moment: 5 header strings + 14 (how_to) or 20 (controls) + 4 (join) + 1 CTA + 1 tip, so 25 to 31 separate strings, in 6 fonts, with three of them animating on different clocks (tagline 3.5 s, tip 3 s, board swap 14 s, CTA blink 1.2 s). That is the "busy on start" problem.

### 8.2 Lobby (`lobby_overlay`, draw.c:647-656, plus the whole night stack)

The lobby runs the full night draw path (section 1 step 4) so station names, floor tags, meters, cards and toasts are all live, with these additions:

| element | lines | position | purpose |
|---|---|---|---|
| how_to board | 648 | (16,146) 470x296 | over the left of the room |
| controls_card | 649 | (16,454) 400x150 | under it; bottom edge 604, overlapping the player card eyebrows at y 638 region only when cards are present? Cards start at y 658, eyebrows at 638, so no overlap but tight |
| join_panel small | 650 | (1032,146) 232x158 | QR 104 px, "JOIN", room code F_H3, "SCAN IT", "ANY PHONE" |
| slot bar | 651-655 | SLOT_Y 68 | chip "PRESS A OR SPACE TO JOIN" when nobody is in, else "START OR ENTER: BEGIN (AUTO N)" counting down from 25, with message "WALK AROUND. TRY THINGS. NOTHING COUNTS YET." |

Note the how_to board here is 470 wide while its content (three chips and two arrows) was laid out for 600; "SHOW & TELL" chip ends about x 16+34+~430 which fits, but the red rule block at `w-32` wraps nothing, so two F_MONO_S lines at 470 wide are fine. `join_strip` is not drawn in the lobby (771-772); `join_panel` small replaces it, so the room code moves from the top-right corner at (944,6) in the night to (1032,146) in the lobby.

### 8.3 Night (the in-game HUD)

Top row, `top_bar` (draw.c:473-505):

| element | lines | position | font, colour | purpose |
|---|---|---|---|---|
| clock panel | 476 | (16,6) 300x56 slab | | |
| mark | 477 | (20,9) 50 px bone flame | | brand |
| clock | 478 | (74,14) | F_H2 bone | 7:00 PM to 11:00 PM mapped from `G.t / night_len` |
| "M:SS LEFT" | 481-486 | right edge x 306, y 10 | F_MONO_S ash, blinks hazard in last 30 s | countdown |
| night bar | 487-491 | (76,54) 232x5 with 3 hour ticks | hazard, rust when closing | progress |
| "CLOCKING IN" | 492 | right edge x 306 | F_MONO_S ash | lobby state only |
| meters panel | 494 | (330,6) 600x56 slab | | |
| CLUTTER meter | 495-500 | (346,12) w 270; eyebrow "CLUTTER", rule "100 = FIRE MARSHAL", value or "FIRE MARSHAL IN N" | F_MONO_S; bar hazard, rust above 55, fault above 80 | |
| VIBE meter | 501-502 | (640,12) w 274; eyebrow "VIBE", rule "0 = BURNOUT", value | bar live, fault under 25 | |
| fire flash | 503 | 12 px fault frame round the whole screen, 2 Hz | | fire warning |
| heat wash | 504 | full-screen 0xff7a1a at alpha 28 while `G.heat > 0` | | event tint |

Right of the meters, `join_strip(944,6)` (draw.c:408-419): 320x56 slab, QR 48 px at (949,10), eyebrow "PHONES JOIN", room code F_H3 hazard at (1004,30), "ANY TIME" F_MONO_S smoke right aligned. Offline variant: "PHONES OFFLINE" and "GRAB A CONTROLLER."

Message slot at y 68 (one of three, mutually exclusive, draw.c:773):

- `danger()` (532-540): when clutter > 80 or vibe < 20 in the night, flashing chip "TOO MUCH CLUTTER!" with "CLEAN UP BEFORE THE FIRE MARSHAL COMES", or "VIBES ARE DYING!" with "SAY HI TO NEWBIES. CLEAN UP." Fault chip, ink-err message, alpha flickers 255/150 at 0.6 s.
- `ticker_band()` (456-462): `G.ticker` chip and `G.ticker_sub` message for 5.0 or 5.5 s (game.c:99-111), fades in over 0.4 s and slides 20 px, colour fault, live or hazard.
- `coach()` (543-559): first 40 s of a night, five 8 s steps: "1 CLOUT" "GRAB PLYWOOD FROM THE LUMBER RACK (LEFT WALL)", "2 CLOUT" "LOAD IT INTO PEWPEW 9000, THEN HOLD TO CUT", "3 CLOUT" "CARRY YOUR PROJECT TO SHOW & TELL (FRONT)", "4 KARMA" "TEAL RINGS ARE MESSES: HOLD TO CLEAN. JUNK GOES IN THE TRASH", "5 KARMA" "NEWBIES WITH A ? NEED A HI, OR THEY LEAVE". Chip hazard for 1 to 3, live for 4 and 5. Suppressed while a ticker is live. Runs for every night including the second and tenth one; there is no "seen it" flag.

World overlays (drawn in this order, draw.c:765-769):

- `goal_guides` (290-319): for each human with `has_goal`, a pulsing 5 px outline around the target station footprint in the player colour, and chevrons marching along the floor every 0.9 units from the player to the station edge, fading in over the first 0.8 units. Pure graphics, no text. This is the best piece of guidance in the game.
- `station_labels` (174-242): for 11 of the 13 stations (couch and table skipped, 177) a name tag at projected (x, top, z) where top is 2.4 for fridge, 2.1 lumber, 2.0 printers, else 1.3 (179). Tag: char 240 alpha, 24 px tall, 4 px left edge coloured hazard (pays clout), live (pays karma) or steel (neither) (226, 231-233), name in F_MONO_S bone. Status line (F_MONO_S on tape pill at +24, 234) built in the switch at 186-215, shown only when `alarm || ready || near` (216-225, near means a human within 2.2 units of the footprint). Alarm adds a pulsing fault disc with a white "!" in F_H3 above the tag (235-240). Status strings: "TOO DUSTY. HOLD TO CLEAN", "CUTTING", "LOADED. HOLD TO CUT", "BRING PLYWOOD  DUST N%", "JAMMED. HOLD TO FIX", "PRINTING Ns", "READY. GRAB IT", "START A PRINT", "PLYWOOD FOR THE LASER", "HOLD TO SOLDER", "ALWAYS FREE. STICKERS", "BRING PROJECTS HERE", "N DISHES. HOLD TO WASH", "CLEAN. FOR NOW", "SMELLS. HOLD TO CLEAN", "CLUB-MATE: SPEED BOOST", "JUNK GOES HERE", "DONATIONS GO HERE". Station names from `LAYOUT` (game.c:33-47): LUMBER, PEWPEW 9000, LOAFER, DR. STRANGE, SIR CUTSALOT, ELECTRONICS, SINK, FRIDGE, TRASH, FREE PILE, SHOW & TELL (LURKER COUCH and WORK ISLAND are never labelled).
- `floor_tags` (244-269): per loose item at y 0.45. Messes (sawdust, puddle, daisy chain) get a pulsing ring (live, fault for the daisy chain) and a `label_av` "SWEEP ME" / "MOP ME" / "UNPLUG ME". Projects get their name in hazard. Donations get their name plus a second tag "TO FREE PILE" on tape. Other junk gets "TRASH ME". Plywood gets nothing. Items older than 25 s that are not project or plywood get a fault "RED TAG" block under them (264-267).
- `npcs_overlay` (271-287): newbies (state 0 or 1) get a char disc with a patience ring (live, fault under 35 percent) and a "?" in F_H3, plus "NEWBIE: SAY HI" or "LEAVING! SAY HI". Converted members (state 2) get a bare "MEMBER" in ink-ok.
- `fx_draw` (769).

Bottom, `player_cards` (507-530): centred row of 210 px cards (150 when more than 4) at y 658, 50 tall, slab with 8 px colour edge; name F_MONO_S top left, source tag "BOT"/"PHONE"/"KEYS"/"PAD" top right in smoke, clout F_MONO_L hazard bottom left, karma F_MONO_L ink-ok bottom right. Eyebrows "CLOUT" and "KARMA" at y 638 at the row's outer edges (528-529).

`toasts(16, 620)` (422-432): three slots stacked upward 24 px apart, each a char pill with a 4 px colour edge and F_MONO_S bone text. `game_toast` (game.c:84-90) shifts five slots and sets `toast_t = 6`; `toasts` only draws while `toast_t > 2` and fades in over the first second after that, so a toast is visible for 4 s and the fourth and fifth slots are never drawn. The toast column at x 16 overlaps the left end of the player card row when there are 6 or more players (cards start at 640 minus half the total width).

`players_overlay` (321-380), drawn last for every active player including bots:

- name tag at head height (y 2.25): player colour block, F_MONO_S name in tape ink (328-330). Bots get this too.
- goal line above the name when carrying something with a destination: char 235 box with a 3 px colour bar, F_MONO_S bone, strings "TO SHOW & TELL", "TO THE FREE PILE", "TO THE TRASH", "TO PEWPEW 9000", "LASER BUSY: FREE PILE" (game.c:689-694). Else, if holding something without a goal, the item name in bare F_MONO_S (hazard for projects).
- prompt box under the feet when `p->prompt[0]` (342-363): keycap with the player's own A glyph plus the action in F_MONO. Hazard for clout actions, live for karma (`p->px > 0` flags karma, 344). The box fills with the action colour as `p->work / p->work_need` grows (355-359), so the hold progress lives in the prompt. Prompt strings come from `find_target` (game.c:305-352): THROW OUT, FREE PILE IT, SHOW IT OFF, LOAD THE LASER, UNPLUG IT, CLEAN UP, PICK UP, GRAB PLYWOOD, CUT, CLEAN THE LASER, START A PRINT, TAKE THE PRINT, UNCLOG IT, CUT A STICKER, SOLDER SOMETHING, WASH A DISH, DEAL WITH THE FRIDGE, DRINK A CLUB-MATE, SAY HI.
- pop (364-369): `p->pop` in F_H2 rising 50 px over 1.4 s with a 3 px fake shadow. Strings: "+N KARMA", "+N CLOUT", "WASTED PLYWOOD", "SPEED", "+0 CLOUT" (game.c:376, 396, 406, 463, 680).
- proposal bubble (370-378): bone panel 50 tall to the right of the head, "I THINK WE SHOULD" in smoke and one of 15 `PROPOSALS` (content.h:36-52).

### 8.4 Results (`results_screen`, draw.c:658-735)

| element | lines | position | font |
|---|---|---|---|
| scrim char 215, tape header scrolling at 20 px/s | 659-660 | | |
| eyebrow "11:00 PM. LIGHTS OUT." or "HYH IS CANCELED" | 663 | (64,48) | F_MONO_S ink-ok or ink-err |
| headline "THE SPACE SURVIVES ANOTHER TUESDAY." / "THE FIRE MARSHAL SHUT IT DOWN." / "EVERYONE BURNED OUT." | 662-664 | (60,70), drops to F_H2 if wider than 1160 | F_H1 bone |
| sub line "CLOUT COUNTS. BUT YOU KNOW WHO DID THE DISHES." or "ALL CLOUT IS VOID. NOBODY CARES ABOUT YOUR LASER MUSTACHE." | 665-666 | (64,150) | F_MONO ash or ink-err |
| column eyebrows MEMBER (130) CLOUT (right at 560) KARMA (right at 700) AWARD (780) | 697-700 | y 200 | F_MONO_S |
| up to 6 rows, 1152x58 alternating slab, sliding in from +80 px with a 0.25 s stagger | 702-724 | from y 226, 64 apart | rank F_H2 smoke, name F_H3, clout and karma F_MONO_L, award name F_MONO_S hazard and award line F_MONO_S bone |
| "THE REAL WINNER: name. LEAVE IT BETTER." | 725-732 | centred at y 600, live block 44 tall | F_H3 tape ink |
| "PRESS A OR SPACE FOR ANOTHER TUESDAY" | 733-734 | centred y 670, blinking after 4 s | F_MONO bone |

Awards (689-694, strings content.h:103-108): JANITORIAL CO-OP (most karma), GLAMOUR BIAS (most clout, not already awarded), TALKER (3+ proposals), GREETER (2+ greets), CLUB-MATE SOMMELIER (3+ mates), LURKER (lowest tasks plus clout/500 with 2+ players). One award per player. Sorting is by clout on survival, by karma on failure (670-674). Players beyond 6 are not shown.

### 8.5 how_to board (`how_to`, draw.c:578-602)

Panel at (x,y) w by 296. Eyebrow "HOW TUESDAY WORKS". Two 64 px tall colour rails (hazard then live) each with an F_H3 heading ("CLOUT", "KARMA"), a grey sub ("FOR YOU", "FOR THE SPACE") and a row of `flow_chip`s joined by `flow_arrow`s: LUMBER > PEWPEW 9000 > SHOW & TELL, and MESS > TRASH, DISHES, SAY HI (the karma row mixes an arrow and plain gaps, 594-597, so it reads as a sequence when it is really four separate things). Below both, a solid fault block with two F_MONO_S lines: "CLUTTER 100 OR VIBE 0 CLOSES THE SPACE." and "THEN EVERYONE'S CLOUT IS VOID." The row widths are hardcoded; the lobby passes w 470 and the title w 600 and neither clips.

### 8.6 controls_card (`controls_card`, draw.c:604-623)

Panel 150 tall. Eyebrow "BUTTONS", column heads "DO IT" (hazard), "DROP" (bone), "PROPOSE" (ash) at x+120, x+210, x+290. Four rows PAD, WASD, ARROWS, PHONE with keycaps A/B/X, SPACE/Q/R, ./,// and A/B/X. The WASD row omits E, which main.c also maps to A (main.c:208), and ENTER (start) is not shown anywhere on this card. Keycap colours hazard, bone, violet.

### 8.7 join_panel and join_strip (draw.c:384-419)

`join_panel(x,y,big)`: big is 300x290 with a 172 px QR, eyebrow "SCAN TO JOIN", room code in F_H1 hazard, right-aligned "NO APP" and "NO ACCOUNT". Small is 232x158 with a 104 px QR, eyebrow "JOIN", room F_H3, "SCAN IT", "ANY PHONE". Offline either size collapses to 108 tall with "PHONES OFFLINE" and wrapped "GRAB A CONTROLLER.". `join_strip` is the 320x56 top-right night variant described in 8.3.

## 9. Clutter audit

Rating key: must (a first-time arcade player cannot play without it), help (useful once, in the first minute), nice (flavour or for the second night), noise (costs attention, pays nothing to a first timer).

### 9.1 Title screen

| text | lines | rating | proposal |
|---|---|---|---|
| HeatSync mark | 628 | must | keep, do not recolour |
| "HEATSYNC LABS ARCADE" eyebrow | 629 | nice | keep, it is tiny and tells you where you are |
| "DO-OCRACY" | 630 | must | keep |
| rotating tagline (8) | 631-632 | nice | keep one line, slow it to 6 s or tie it to the menu idle; it fights the tip bar for the same job |
| how_to board, 14 strings | 634, 578-602 | help | remove from the title. Replace with the rules flash (section 11) that plays on demand from a menu row and once automatically after START. The two-loop idea survives as two icons, not fourteen strings |
| controls_card, 20 strings | 635, 604-623 | help | remove from the title. Show a single keycap hint on the menu ("A SELECT  B BACK") and fold the full table into a Controls row in the menu, drawn one input type at a time for the seats actually connected |
| join_panel big: QR, "SCAN TO JOIN", room code, "NO APP", "NO ACCOUNT" | 636, 389-395 | help | keep QR and room code, make it a quiet corner module; drop "NO APP" and "NO ACCOUNT" (two of four strings) or merge to one line under the code. Hide entirely when offline rather than showing "PHONES OFFLINE / GRAB A CONTROLLER." |
| "PRESS A OR SPACE TO PLAY" | 637-641 | must | becomes the highlighted first menu row; the blink becomes the cursor pulse |
| attract tips bar (12) | 642-644 | noise on the title, nice in attract | show only while the attract night is running and only in the gap between menu idle and the demo; cut the five-step floor joke to a single line or move it to results |

Net: from 25 to 31 strings down to about 8 (mark, eyebrow, title, one tagline, 3 to 5 menu rows, room code). Everything removed reappears on demand or as pictures.

### 9.2 Night screen

| text | lines | rating | proposal |
|---|---|---|---|
| clock "7:00 PM .. 11:00 PM" | 478 | nice | shrink to F_H3 or drop; the night bar already shows progress and "M:SS LEFT" is the number people read |
| "M:SS LEFT" | 481-486 | must | promote to F_H2 and make it the main number in the clock panel |
| night bar with hour ticks | 487-491 | help | keep |
| "CLUTTER" eyebrow + "100 = FIRE MARSHAL" + value | 495-500 | must (the bar), noise (the rule text) | keep the bar and the value, drop the rule text, put a small fire marshal pictogram at the right end of the bar instead |
| "VIBE" eyebrow + "0 = BURNOUT" + value | 501-502 | must (bar), noise (rule) | same: drop "0 = BURNOUT", put a face or heart glyph at the left end |
| join_strip: "PHONES JOIN", room code, "ANY TIME" | 408-419 | nice | shrink to QR plus code only; drop "ANY TIME"; or hide after the first 60 s of a night and show again in the lobby |
| coach, 5 steps of two strings | 543-559 | help, first night only | keep on the first night, skip when every human has already played a night (needs a flag), shorten each line to under 32 chars, and make steps 1 to 3 point at the station (reuse `goal_guides` chevrons toward LUMBER) instead of saying "(LEFT WALL)" |
| ticker and ticker_sub | 456-462 | help | keep, it is the event feed; but a chip plus a sub line is two fonts; consider chip only with the sub as the toast |
| danger banners | 532-540 | must | keep, shorten to "CLUTTER!" / "VIBES!" with the meter itself flashing |
| station name tags, 11 always on | 227-233 | help | keep but demote: draw at alpha 160 unless a human is within 4 units or the station has a status; a first timer needs to read the three stations the coach names, not eleven |
| station status lines (18 strings) | 186-215, 234 | must when alarm or ready, noise when just near | keep alarm and ready; drop the "near" idle strings ("PLYWOOD FOR THE LASER", "ALWAYS FREE. STICKERS", "CLEAN. FOR NOW", "JUNK GOES HERE", "DONATIONS GO HERE", "CLUB-MATE: SPEED BOOST", "BRING PROJECTS HERE", "START A PRINT", "HOLD TO SOLDER") because the prompt box under the player's feet already says the exact action with the exact button |
| "!" alarm badge | 235-240 | must | keep, this is a picture |
| floor tags "SWEEP ME" / "MOP ME" / "UNPLUG ME" | 251-255 | help | keep the ring, drop the text once the player has cleaned one mess (or draw a broom glyph) |
| project names on the floor | 256-257 | nice | keep in hazard; it is the reward loop and the jokes live here |
| donation name + "TO FREE PILE" | 258-260 | noise (two tags per item) | merge: the name only; the goal line and chevrons already say where it goes once picked up |
| "TRASH ME" | 261-262 | noise | drop; `goal_guides` takes over the moment it is picked up, and the station edge colour already says trash is karma |
| "RED TAG" | 264-267 | nice | keep, it is a HeatSync lore joke, but it is a second label on an item that may already have one |
| newbie "?" + "NEWBIE: SAY HI" / "LEAVING! SAY HI" | 277-282 | must (ring and ?), help (text) | keep the ring and "?"; shorten text to "SAY HI" and only under 35 percent patience |
| "MEMBER" | 283-284 | noise | drop; the colour change on the NPC body should carry this |
| player name tags (also on bots) | 328-330 | must for humans, noise for bots | keep humans; for bots draw a smaller tag or none |
| goal line "TO THE TRASH" etc | 332-336 | help | keep, it names the chevron target |
| held item name | 337-339 | nice | keep |
| prompt box with keycap | 342-363 | must | keep, this is the core teaching device; nothing else on screen should repeat it |
| pops "+N CLOUT" | 364-369 | must | keep |
| proposal bubbles | 370-378 | nice | keep, it is the Talker joke, but cap to one bubble on screen at once |
| player cards: name, source tag, clout, karma | 507-527 | help | keep; drop the source tag ("KEYS"/"PAD") after the lobby, it is a debug string |
| "CLOUT" / "KARMA" eyebrows over the cards | 528-529 | help | merge into the cards as a tiny glyph per number instead of two floating words |
| toasts (3 lines) | 422-432 | nice | keep two slots, not three, and move them to the right so they never cross the card row |

The structural fix: there are five independent text channels competing (slot bar, station status, floor tags, toasts, pops) and three of them say the same thing about the same object. Rule for the redesign: one object, one label, and the prompt box is the only place a button name appears during play.

## 10. Toasts and ticker data flow

- `game_toast(msg, color)` (game.c:84-90): shift `toast_text[5]`, `toast_t[5]`, `toast_col[5]` down, set slot 0 to msg with `toast_t = 6`. Decays in `game_update` (game.c:993). Drawn by `toasts` for only three slots while `toast_t > 2`.
- `ticker_sub(msg, sub, color)` and `ticker(msg, color)` (game.c:99-111) set `G.ticker`, `G.ticker_sub`, `G.ticker_t` 5.5 or 5.0, `G.ticker_color`. Drawn by `ticker_band` through `slot_bar`. The colour is snapped to fault, live or hazard (draw.c:460).
- Join toasts "NAME JOINED FROM A PHONE" fire on every join (game.c:258-262) but are invisible on the title because the title path returns before `toasts`.

## 11. What a menu system and a rules flash need from hud.c

Present and reusable: `hud_panel`, `hud_rect`, `hud_frame`, `hud_text` with three alignments, `hud_text_w`, `hud_tape`, `hud_chevron`, `hud_ring`, `hud_bar`, `hud_logo`, `hud_qr`, `keycap` (draw.c static, should move to hud.c), `eyebrow` (same), `slot_bar` (same).

Missing for a menu (cursor, option rows, settings, player-mode picker):

1. A focus or cursor primitive: a thick frame that pulses, or a hazard block that slides between rows with an eased position. Nothing eases today; every animation is `sinf(T*k)` or `fmodf(T, period)`. Add `hud_ease(float t)` style helpers or a tiny tween struct in draw.c (target y, current y, speed).
2. A row helper `menu_row(x, y, w, label, value, focused, enabled)` that draws label left, value right, a chevron pair "< >" around the value when it is a choice, and the focus treatment. Keep square corners and the 6 px shadow only on the focused row so the stack stays calm.
3. A value stepper for settings: night length (90, 180, 300), bots on or off, phones on or off, coach on or off, sound. These need a place to live in `game_t` (game.h:126 has `night_len` already; nothing else) and a persistence hook if wasmcart offers one, else they reset on cart reload.
4. A player-mode picker: 1P with bots, 2P shared keyboard, up to 8 with pads and phones. Draw as a row of seat cards reusing the `player_cards` layout (draw.c:507-527) with an empty state "PRESS A" per seat, and a colour swatch from `PCOL`. Needs an input-to-seat model in main.c where A on an empty seat claims it instead of jumping to the lobby (main.c:170-180 currently joins on any A or START anywhere).
5. Menu input: up and down edges, A and B edges, with repeat. `game_player_input` (game.c:288-291) only stores raw state; the title has no edge detection of its own. Add a tiny menu input reader in main.c that looks at all seats' `sy` and buttons and emits one navigation event per frame.
6. Text with a drawn shadow or outline (one call), so the title and menu labels stay readable over the orbiting 3D without the 150 alpha scrim covering the whole screen. Today the scrim is the only thing making text legible (draw.c:626), and it kills the room the player is about to play in.
7. A clipping rectangle or a `hud_push_clip` so a long settings value or a scrolling controls table cannot bleed out of its panel. Today nothing clips except the screen edge.
8. A glyph set: tiny pictograms for clout (coin or star), karma (plus sign, already a particle at draw.c:154-159), pad, keyboard, phone, bot, fire marshal, broom, speech. Could be 12x12 or 16x16 bitmaps in a header drawn through a `hud_glyph(x, y, id, rgb, a)` blit. Fonts cannot do this because only ASCII is baked.
9. A screen-space particle or at least an `fx_burst` that accepts a screen point, so a selection or a results reveal can pop without picking a world coordinate.

Missing for a rules flash (3 to 5 quick animated picture cards, almost no text):

1. Pictogram drawing for the five beats: plywood sheet to laser to Show and Tell (clout), mess ring to broom to trash (karma), newbie "?" to a wave to "MEMBER", the clutter bar filling to a fire marshal silhouette, the vibe bar emptying to a slumped figure. All of these are rects, circles, rings, chevrons and lines, so they can be built from existing primitives, but they want a card frame helper `card(x, y, w, h, step, total)` with a step dot row and a hazard tape header, and a 0.6 s wipe between cards.
2. A timeline: each card is about 2.5 s, with the picture animating along `local_t` (chevron marches reuse `hud_chevron`, rings fill with `hud_ring frac`, bars with `hud_bar`). Needs a state struct in draw.c (`flash_step`, `flash_t`, `flash_active`) and a skip input (A advances, B exits).
3. A single caption slot per card, F_H3, at most three words ("MAKE A THING", "CLEAN A MESS", "SAY HI", "TOO MUCH JUNK", "KEEP VIBES UP"), using the two colour rails hazard and live so the clout and karma split is taught by colour not by paragraph.
4. A keycap glyph that can show the seat's own button inside the picture (reuse `keycap` and `glyph_a`), because the one thing a first timer must learn is "hold A on the thing".
5. Suppression flags: when the flash has been seen this session, the coach (draw.c:543-559) should skip steps 1 to 3 and the how_to board should not be drawn at all.

## 12. Key facts an implementer must not get wrong

- `hud_px` is bottom-up RGBA and after init is the live texel array of the HUD texture (main.c:121-124). Never realloc or free it, and keep every writer inside `blend` or the opaque `hud_rect` fast path.
- `draw_hud` starts every frame with a memcpy of the vignette (draw.c:755), not a clear; a new full-screen menu must either accept the vignette or draw its own background over it.
- `hud_text` y is the top of the line box, not the baseline; `hud_font_h` is the pixel size, not the line height.
- Only ASCII 32..127 renders; any other byte becomes `?`. In-game text must be plain ASCII caps; the house rule of no dashes and no emoji is also a rendering constraint.
- `eyebrow` doubles the string width by inserting spaces; measure with the spaced string, not `hud_text_w` of the raw label.
- `hud_panel` is always opaque with a 6 px shadow and a 2 px rule; pass alpha work through `hud_rect` and `hud_frame` directly when you need translucency.
- `hud_tape` blends per pixel and is costly on wide bands; keep it to 24 px tall strips.
- `hud_logo` never takes a sun colour; the mark's orange `C_LOGO_SUN` is fixed in hud.c:305 and only the flame colour is a parameter. Do not add a tint or redraw it from other shapes.
- Label placement: `settle` only moves vertically in half-height steps and only for `label_av` and station tags; players_overlay ignores it on purpose and is drawn last. Reserve the top 104 px and bottom 86 px before any world label (draw.c:763-764) if you change the bar heights.
- The slot bar at y 68 is a single shared channel: danger beats ticker beats coach (draw.c:773). Anything new that wants the slot must join that priority chain, not draw beside it.
- Toasts: five stored, three drawn, visible only while `toast_t > 2` (4 s window).
- Title and attract draw path returns before every world overlay and before toasts (draw.c:756-760). A menu must live inside `title_screen` or replace it; do not expect station labels or cards to be available there.
- Input: any A or START on any seat creates a player and leaves the title (main.c:170-180, game.c:234-240, 264). A menu needs this gated so that A means select while the menu is open.
- `glyph_a`: SPACE for keyboard seat 0, "." for seat 1, "A" for pads and phones; E also maps to A on seat 0 but is not shown anywhere.
- Lobby draws the full night HUD plus how_to, controls_card and small join_panel; it starts the night at 25 s or on START, and falls back to the title after 6 s empty.
- Coach runs for 40 s on every night, and is suppressed only by a live ticker; nothing remembers that a player has already seen it.
- Player colours include the UI amber and bone (`PCOL[0]`, `PCOL[5]`), so do not rely on colour alone to tell a player chip from a UI accent.
- Results shows at most 6 rows and one award per player; sort key flips between clout and karma depending on survival.
- `station_labels` skips LURKER COUCH and WORK ISLAND; their names exist only in `LAYOUT` and are never drawn.
- House style on anything drawn: square corners, solid offset shadows, char and bone with amber as a fill, caution tape between things; the HeatSync mark is never redrawn or recoloured.
