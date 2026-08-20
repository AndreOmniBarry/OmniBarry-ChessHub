#!/usr/bin/env bash
# Rebuilds engine.wasm.js from engine.c and re-embeds it into ../pack.html.
#
# Requires emscripten (emcc). On Debian/Ubuntu: `sudo apt-get install emscripten`.
set -euo pipefail
cd "$(dirname "$0")"

emcc -O3 -DNDEBUG \
  -sTOTAL_STACK=2097152 \
  -sEXPORTED_FUNCTIONS='["_wasm_search","_get_board_ptr","_get_result_fr","_get_result_fc","_get_result_tr","_get_result_tc","_get_result_depth","_get_result_elapsed","_get_result_eval","_get_result_nodes","_get_result_blunder","_get_result_fallback","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["cwrap","ccall"]' \
  -sMODULARIZE=1 -sEXPORT_NAME=createChessWasm \
  -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=16MB \
  -sENVIRONMENT=worker \
  -sSINGLE_FILE=1 \
  -sFILESYSTEM=0 \
  -o engine.wasm.js engine.c

echo "Built engine.wasm.js ($(wc -c < engine.wasm.js) bytes)."
echo "Re-embedding into ../pack.html ..."
python3 embed.py
echo "Done. Run the correctness suite before committing:"
echo "  gcc -O2 -DPERFT_MAIN -o /tmp/perft_test engine.c && /tmp/perft_test"
echo "  gcc -O2 -o /tmp/test2 test2.c -lm && /tmp/test2"
