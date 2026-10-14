#!/usr/bin/env bash
# Builds DO-OCRACY as a wasmcart GL cart (build/do-ocracy.wasc).
#
#   ./build.sh              # full build
#   ./build.sh smoke        # the pipeline smoke test cart (tools/pipeline-smoke.c)
#
# Needs: emcc (emsdk), node >= 22, and the submodules
# (git submodule update --init).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
THREE="$ROOT/third_party/three.c"
WASMCART="$ROOT/third_party/wasmcart"
OUT="$ROOT/build"
TOBJ="$OUT/obj/three_core"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

if ! command -v emcc >/dev/null; then
  [ -f "$HOME/emsdk/emsdk_env.sh" ] && source "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
fi
command -v emcc >/dev/null || { echo "emcc not found: install emsdk (https://emscripten.org)" >&2; exit 1; }
[ -f "$THREE/include/three.h" ] || git -C "$ROOT" submodule update --init
[ -d "$WASMCART/node_modules" ] || (cd "$WASMCART" && npm install --silent)

mkdir -p "$TOBJ" "$OUT/obj/game"
OPT="-O2 -DNDEBUG -flto -msimd128 -msse2"

# three.c, built once and cached (the GL program tables only)
pids=()
for s in math core geometry curves animation raycaster loaders gltf renderer backend_gles gen_program gen/programs_core_gl gen/programs_dfg; do
  o="$TOBJ/${s//\//_}.o"
  if [ ! -f "$o" ] || [ "$THREE/src/$s.c" -nt "$o" ]; then
    emcc -c "$THREE/src/$s.c" -o "$o" $OPT -std=c99 -DT3_WASMCART -DT3_TABLE_CORE \
      -I "$WASMCART/include" -I "$THREE/include" -I "$THREE/src" &
    pids+=($!)
    while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do sleep 0.05; done
  fi
done
fail=0; for p in "${pids[@]+"${pids[@]}"}"; do wait "$p" || fail=1; done
[ $fail = 0 ] || { echo "three.c compile failed" >&2; exit 1; }

# the r186 programs this game draws (tools/states.txt), as a C table.
# The table is committed, so a normal build never needs three.js. Regenerate
# only when asked (REGEN_STATES=1) or when it is missing; that needs
#   (cd third_party/three.c && npm install --ignore-scripts)
if [ "${REGEN_STATES:-0}" = 1 ] || [ ! -f "$ROOT/src/gen/hsx_gl.c" ]; then
  (cd "$ROOT" && node "$THREE/tools/gen-project-table.mjs" --name hsx --out src/gen --states tools/states.txt --cache build/t3-captures)
fi

# generated sources: the HeatSync mark as polylines
node "$ROOT/tools/gen-logo.mjs" "$ROOT/assets/brand/hsl-mark-paths.json" "$OUT/obj/game/logo_mesh.h"

if [ "${1:-}" = smoke ]; then
  SRCS="$ROOT/tools/pipeline-smoke.c $ROOT/src/gen/hsx_gl.c"; NAME=smoke; GAMEF=""
else
  SRCS="$(ls "$ROOT"/src/*.c) $ROOT/src/gen/hsx_gl.c $ROOT/third_party/qrcodegen/qrcodegen.c"; NAME=do-ocracy; GAMEF="-DWC_USE_GL -DWC_USE_NET_PEER"
fi

# the Bellows audio engine (header-only C++17, no exceptions, no RTTI, no libc++)
GAMEOBJ=""
if [ "$NAME" = do-ocracy ]; then
  em++ -c "$ROOT/src/audio_bellows.cpp" -o "$OUT/obj/game/audio_bellows.o" $OPT -std=c++17 -fno-exceptions -fno-rtti \
    -I "$ROOT/third_party/bellows" -I "$ROOT/src"
  GAMEOBJ="$OUT/obj/game/audio_bellows.o"
fi

emcc $OPT -std=gnu11 -Wall -Wno-unused-function -DT3_WASMCART $GAMEF \
  -I "$WASMCART/include" -I "$THREE/include" -I "$ROOT/third_party" -I "$ROOT/third_party/qrcodegen" \
  -I "$ROOT/src" -I "$THREE/src" -I "$OUT/obj/game" \
  $SRCS $GAMEOBJ "$TOBJ"/*.o \
  -sSTANDALONE_WASM=1 -sEXPORTED_FUNCTIONS='["_wc_init","_wc_render","_wc_get_info"]' \
  -sERROR_ON_UNDEFINED_SYMBOLS=0 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 \
  --no-entry -o "$OUT/$NAME.wasm"

# a cart may import only gl, wc_* and WASI
WASMCART="$WASMCART" node "$THREE/wasm/check-imports.mjs" "$OUT/$NAME.wasm"

rm -rf "$OUT/assets" && mkdir -p "$OUT/assets"
cp "$ROOT"/assets/fonts/*.ttf "$OUT/assets/"
# config.json: where the QR sends phones. JOIN_URL overrides it for a LAN build
#   JOIN_URL=http://192.168.1.20:8080/join.html ./build.sh
if [ -n "${JOIN_URL:-}" ]; then
  node -e 'const c=JSON.parse(require("fs").readFileSync(process.argv[1]));c.controller_url=process.argv[2];console.log(JSON.stringify(c,null,2))' "$ROOT/config.json" "$JOIN_URL" > "$OUT/assets/config.json"
else
  cp "$ROOT/config.json" "$OUT/assets/config.json"
fi

# manifest: the relay is the one network destination this cart may dial
node "$WASMCART/bin/wasmcart-pack.js" --wasm "$OUT/$NAME.wasm" --assets "$OUT/assets" \
  --name "DO-OCRACY" --version 0.1.0 --width 1280 --height 720 --players 4 \
  --controls dpad,left_stick,a,b,x,start --ws relay.clasp.to \
  --output "$OUT/$NAME.wasc" >/dev/null
[ "$NAME" = do-ocracy ] && cp "$OUT/$NAME.wasc" "$ROOT/web/do-ocracy.wasc"
echo "built build/$NAME.wasc ($(wc -c < "$OUT/$NAME.wasc" | tr -d ' ') bytes)"
