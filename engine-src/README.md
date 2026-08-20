# Engine source (build-time only)

`pack.html` still runs with **zero build step for players** — download it,
open it, play. Nothing here needs to touch a user's machine.

This folder exists purely because the AI search engine is authored once, in
C, and compiled to a small embedded WASM binary for native-class search
speed, then baked directly into `pack.html` as inert `<script type="text/plain">`
text (base64 WASM + glue JS, no separate file, no network fetch). A pure-JS
transliteration of the exact same algorithm also lives in `pack.html` itself
(`#engine-core`) as the automatic fallback if WASM ever fails to load — the
game never depends on the WASM path working.

## Files

- `engine.c` — the engine: board representation, move generation, make/unmake
  search (minimax + alpha-beta + quiescence), MVV-LVA move ordering,
  time-boxed iterative deepening. Also contains a `perft()` correctness
  checker and a `#ifdef PERFT_MAIN` native test harness.
- `test2.c` — extra correctness + timing tests (Kiwipete depth-4 perft,
  Position 3 depth-5 perft, and a search-timing sanity check that fails if
  any difficulty ever exceeds its wall-clock budget by more than 250ms).
- `ref_perft.py` — an **independently written** (not translated from the C)
  Python move generator, used only to cross-validate `engine.c`'s move
  generation against a second implementation, since a chess engine with a
  rules bug is worse than a slow one.
- `divide.c` — perft "divide" (per-root-move breakdown), used to localize a
  discrepancy to a specific line of play when debugging.
- `build.sh` / `embed.py` — rebuilds `engine.wasm.js` from `engine.c` via
  emscripten and re-embeds it into `../pack.html`'s `#wasm-engine-core`
  script block.

## Rebuilding

```
sudo apt-get install emscripten   # provides emcc
./build.sh
```

## Testing (run before committing any change to engine.c)

```
gcc -O2 -DPERFT_MAIN -o /tmp/perft_test engine.c && /tmp/perft_test
gcc -O2 -o /tmp/test2 test2.c -lm && /tmp/test2
python3 ref_perft.py 4 divide | sort > /tmp/py4.txt
gcc -O2 -o /tmp/divide divide.c -lm && /tmp/divide 4 | sort | grep -v ^total > /tmp/c4.txt
diff <(grep -v ^total /tmp/py4.txt) /tmp/c4.txt && echo "cross-validation OK"
```

All of the above must pass — perft against known-correct reference values
(startpos through depth 5, Kiwipete through depth 4, the en-passant-heavy
"Position 3" through depth 5) **and** an exact cross-check against the
independent Python implementation — before `build.sh` output is embedded.

## Why C/WASM instead of just faster JS

The JS engine (`#engine-core` in `pack.html`) already fixes the original
"AI thinks for 8 minutes" bug on its own: it replaced full-board-clone move
generation with in-place make/unmake (zero per-node allocation), added
MVV-LVA move ordering for real alpha-beta pruning, and wrapped the whole
search in a hard wall-clock deadline via iterative deepening. That alone
bounds every difficulty to well under its time budget.

The WASM build sits on top of that as a genuine speed multiplier — the same
algorithm, compiled and run at native speed — so the same time budget
reaches a deeper, stronger search instead of just "not hanging." It's
loaded only inside the AI Worker (never blocks the UI thread either way),
and the JS engine remains the automatic, transparent fallback.
