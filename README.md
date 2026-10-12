# DO-OCRACY

Whoever does it, decides. Nobody does the dishes.

DO-OCRACY is an arcade game about one Tuesday night of Hack Your Hackerspace at [HeatSync Labs](https://heatsynclabs.org). It ships as a single [wasmcart](https://github.com/wasmcart/wasmcart) cartridge, `do-ocracy.wasc`, built to live on the shared game station: Batocera, RetroArch, the wasmcart libretro core. Up to 8 members play at once on gamepads, keyboards and phones. Phones join by scanning a QR code on the screen, with no app and no account.

![the night](docs/night.png)

## The game

It is 7 PM on a Tuesday. The doors are open, the laser is calling, and the sink already has three dishes in it.

Everybody scores two things:

- **CLOUT** is for you. Cut something on PEWPEW 9000, run a print on Loafer or Dr. Strange, solder something at the bench, cut a sticker on Sir Cutsalot (always free). Carry it to Show & Tell.
- **KARMA** is for the space. Wash the dishes. Haul scrap, empty Club-Mate bottles and print spaghetti to the trash. Move donations (a very heavy CRT, three broken printers, a chest freezer that runs fine) to the Free Pile. Unclog the printers. Clean the laser when it is covered in dust. Unplug the daisy-chained power strips. Say hi to the newbies.

The space has two meters:

- **CLUTTER** goes up with every mess on the floor, every dirty dish, every broken machine. At 100 the fire marshal is on the way. Five seconds later the night is over.
- **VIBE** drains when clutter is high, when a newbie walks out because nobody said hi, and when the landlord sees the Free Pile. At 0 everybody burns out.

Survive until 11 PM and the space lives to see another Tuesday. Then the scoreboard ranks everyone by clout and hands out awards. Most clout with the least karma gets GLAMOUR BIAS. Most karma gets JANITORIAL CO-OP, and a banner that says they were the real winner. Whoever did nothing gets LURKER: "Paid dues. Said nothing. Kept the lights on."

If the space closes, every point of clout is void. Karma stands. That is the whole design: the commons dilemma with a laser cutter. Glamour work is fun and pays you. Janitorial work is boring and pays the room. The laser leaves scrap. The prints jam. Club-Mate makes you fast and then you litter. Every newbie you greet becomes a member who quietly cleans up after everyone, for a while, until they drift away (the four year sine wave).

The night throws events at you: IT IS 118 OUTSIDE, HABOOB, SOMEBODY BROUGHT TACOS, LANDLORD WALKTHROUGH, ANY PARTICULAR REASON WHY DR. STRANGE IS DOWN?, and the occasional LINUX NIGHT: CANCELED, which does nothing. X makes a proposal ("I think we should... form a committee"). It earns zero clout and says so.

When nobody is playing, the cabinet runs an attract mode. Three archetypes play a night on their own: BUILDER (mostly karma), TALKER (proposals and the laser) and FOUNDER (all glamour). If you play alone, BUILDER and TALKER clock in with you, so there is a commons to ruin.

## Controls

| | Move | Do it (hold for work) | Drop | Propose | Begin |
| --- | --- | --- | --- | --- | --- |
| Gamepad (any of 4) | stick or d-pad | A (south) | B (east) | X or Y | Start |
| Phone | drag the stick box | A | B | X | Start the night |
| Keyboard, browser player | arrows | X or Space | Z | C | Enter |
| Keyboard seat 2 | WASD | F | G | H | |
| Keyboard seat 3 | IJKL | U | O | P | |

Press A or Start on any pad to clock in. The night starts when someone presses Start, or by itself 25 seconds after the first person joins. Drop in any time.

## Phones

The cart talks to phones itself. It dials `wss://relay.clasp.to` through wasmcart's `wc_peer_open` and speaks [CLASP](https://clasp.to)'s binary protocol directly: HELLO, SUBSCRIBE and event PUBLISHes carrying short strings. Nothing wraps the cart, so this works in RetroArch on Batocera the same as in a browser. The manifest grants the cart exactly one network destination, `relay.clasp.to`.

The QR code opens `web/join.html#ROOM`, the phone controller. The room code is four letters, drawn from the relay's session id on first contact, so two cabinets never share one. The wire format is documented at the top of [src/net.h](src/net.h). If the relay cannot be reached, the screen says PHONES OFFLINE and the game carries on with controllers.

The QR points at the URL in [config.json](config.json), `https://virgilvox.github.io/do-ocracy/join.html` by default. The GitHub Pages workflow in `.github/workflows/pages.yml` publishes `web/` there. To use a phone page on the shop's own network instead:

```sh
JOIN_URL=http://192.168.1.20:8747/join.html ./build.sh
npm run serve        # serves web/ on port 8747 and prints the LAN address
```

## Put it on the game station (Batocera)

1. Install the wasmcart libretro core once. Follow [wasmcart-libretro/frontend/batocera](https://github.com/wasmcart/wasmcart-libretro/tree/main/frontend/batocera): copy `wasmcart_libretro.so`, the `.info`, `es_systems_wasmcart.cfg`, `wasmcart.service.sh` and `install.sh` to the box and run `sh install.sh`.
2. Copy `build/do-ocracy.wasc` (or `web/do-ocracy.wasc`, the same file) to `/userdata/roms/wasmcart/`.
3. Restart EmulationStation. DO-OCRACY shows up under wasmcart.

The core gives GL carts a GLES3 context and dials the relay with Node's own WebSocket inside the core, so phones work as long as the box is online. RetroArch maps the keyboard onto RetroPad 1. The raw-keyboard seats only exist in the browser and Node hosts.

## Play it in a browser

```sh
npm run serve
# open http://localhost:8747/ and press Clock in
```

`web/index.html` runs the same `.wasc` with wasmcart's web host (`CartHostWeb`, vendored in `web/vendor/`), plays the cart's audio through WebAudio, and reads gamepads and the keyboard.

## Build

You need [Emscripten](https://emscripten.org) (`emcc`/`em++`, the build looks in `~/emsdk`), Node 22 or newer, and the submodules:

```sh
git submodule update --init
npm install
./build.sh                 # build/do-ocracy.wasc and web/do-ocracy.wasc
```

The cart is about 950 KB: three.c, the game, the Bellows engine, five fonts and a config file.

Check it the way romdev does (`npx romdevtools`, then from any MCP client or plain HTTP):

```sh
curl -s -X POST localhost:7331/tool/loadMedia -H 'content-type: application/json' -H 'x-romdev-session: hsx' \
  -d "{\"platform\":\"wasmcart\",\"path\":\"$PWD/build/do-ocracy.wasc\"}"
curl -s -X POST localhost:7331/tool/wasm -H 'content-type: application/json' -H 'x-romdev-session: hsx' -d '{"op":"conformance"}'
```

Headless testing, on the same GL path as the native hosts:

```sh
node tools/shot.mjs build/do-ocracy.wasc build/shots/s 60,600            # screenshots of frames 60 and 600
node tools/shot.mjs build/do-ocracy.wasc build/shots/s 600 script.json   # with scripted pad input
WAV=build/night.wav node tools/shot.mjs ...                               # and the audio
PACE=1 node tools/shot.mjs ... & node tools/phone-sim.mjs ROOM "NAME"     # a fake phone over the real relay
```

## How it is made

| Part | What it is |
| --- | --- |
| [src/main.c](src/main.c) | The wasmcart entry points, input from pads, keyboard and phones, phase flow, camera |
| [src/game.c](src/game.c) | The night: stations, messes, newbies, events, meters, scoring, the archetype bots |
| [src/world.c](src/world.c) | The 3D space in [three.c](https://github.com/monteslu/three.c): the three.js API in C, drawing three.js r186's own programs |
| [src/draw.c](src/draw.c), [src/hud.c](src/hud.c) | The 2D layer in HeatSync's GANTRY theme: stb_truetype text, panels, caution tape, the QR, and the HeatSync mark rasterized from its official paths |
| [src/audio_bellows.cpp](src/audio_bellows.cpp) | Music and effects on [Bellows](https://bellows.live) (bellows-embedded, the C++ port of its DSP core): E minor punk over Em C G D at three intensities, FM bells for clout, plucked strings for karma |
| [src/net.c](src/net.c) | The CLASP client, about 150 lines of C |
| [src/content.h](src/content.h) | Every word in the game. Edit freely |
| [tools/states.txt](tools/states.txt) | The r186 material states the game draws; `build.sh` turns them into a program table with three.c's `gen-project-table.mjs` |
| [web/](web/) | The browser player and the phone controller |

The HUD is a 1280x720 texture redrawn on the CPU each frame and composited over the scene by a second three.c render. The renderer runs with `set_exact_output` so both passes share r186's output target.

## Credits and licences

The game code is MIT (see [LICENSE](LICENSE)).

- [three.c](https://github.com/monteslu/three.c) (MIT) and [three.js](https://threejs.org) r186 (MIT), whose generated programs three.c draws with
- [wasmcart](https://github.com/wasmcart/wasmcart) (MIT), the spec, the C headers and the web host in `web/vendor/wasmcart`
- [Bellows](https://github.com/virgilvox/bellowsjs) bellows-embedded (Apache-2.0, `third_party/bellows/LICENSE`)
- [QR Code generator](https://github.com/nayuki/QR-Code-generator) by Project Nayuki (MIT), [stb_truetype](https://github.com/nothings/stb) (public domain), [fflate](https://github.com/101arrowz/fflate) (MIT)
- Archivo Black, IBM Plex Mono and Charis SIL, under the SIL Open Font License (`assets/fonts/OFL.txt`)
- The HeatSync Labs mark and the GANTRY theme belong to HeatSync Labs. The mark is drawn from its official paths, in its two colours, unmodified
- Hackerspace patterns and vocabulary (do-ocracy, glamour work, the sine wave of enthusiasm, the Tuesday pattern) come from Ohlig and Weiler's Hackerspace Design Patterns (24C3, 2007) and the people who have kept HeatSync open since 2009

Be excellent to each other.
