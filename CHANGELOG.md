# Changelog

All notable changes to this project are documented here.  
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [2.0] — 2026

### Fixed
- **AI thinking time was unbounded** — higher difficulties could take
  *minutes* per move (observed: 8+ minutes on GoPro). Root cause: legality
  checking cloned the entire board (fresh objects per piece) on every
  candidate move at every search node, there was no move ordering (so
  alpha-beta barely pruned), and there was no time limit of any kind.

### Added
- **Native C/WASM search engine** (`engine-src/engine.c`) — the same
  algorithm compiled to a ~29KB embedded WebAssembly binary for
  native-speed search, cross-validated against known-correct
  [perft](https://www.chessprogramming.org/Perft_Results) values (startpos
  through depth 5, Kiwipete through depth 4, an en-passant-heavy stress
  position through depth 5) and an independently written reference move
  generator (`engine-src/ref_perft.py`)
- In-place make/unmake move application — zero per-node allocation in the
  search, replacing full-board cloning
- MVV-LVA move ordering (captures/promotions searched first) for real
  alpha-beta pruning
- Hard time-boxed iterative deepening — every difficulty now has a
  wall-clock budget (0.35s–2.2s), enforced deep inside the search; a
  search that runs out of time discards its unfinished depth and returns
  the last *fully completed* depth's result
- AI search now runs inside a Web Worker (native WASM engine preferred,
  pure-JS engine as an automatic, transparent fallback) so the UI thread
  is never blocked regardless of search time
- Live engine HUD (search depth, elapsed time, evaluation, engine used)
- Complete visual rebuild — dark glass-and-glow "console" skin: animated
  ambient background, chrome/graphite board texture, gradient-metallic
  pieces, glowing move/check/selection highlights
- FLIP-animated piece movement and a fading capture "ghost" effect
- Drag-and-drop moving, in addition to click-to-move
- Synthesized sound effects (move, capture, check, game end) via the Web
  Audio API — no audio files, muteable
- `engine-src/` — the C engine source, build script, and correctness test
  suite (perft + timing-budget checks) used to produce the embedded WASM

### Changed
- Board/piece rendering, sidebar, difficulty selector, and promotion modal
  redesigned around the new console theme (existing gameplay, rules, and
  responsive breakpoints unchanged)

---

## [1.3] — 2026

### Added
- Opening book with real chess responses: Sicilian Defence, French Defence,
  Caro-Kann, Queen's Pawn, King's Indian, Dutch Defence, and more
- Candidate pool sampling — AI now picks from all moves within 30 centipawns
  of the best score, producing genuine variation in repeated positions
- Move order shuffling before minimax search for additional line diversity
- Full responsive layout system covering 280px (Galaxy Fold) through 4K
- Landscape orientation breakpoint for phones rotated sideways
- Minimum 44px touch targets on all interactive elements

### Changed
- AI `findBestMove()` rebuilt from deterministic single-pick to pooled random selection
- Responsive block expanded from 2 rules to a full 9-breakpoint system

### Fixed
- AI returning identical move on undo-replay of the same position

---

## [1.2] — 2026

### Added
- Complete chess engine in a single `index.html` file
- Full FIDE rule enforcement: legal move validation, castling (both sides),
  en passant, pawn promotion, check, checkmate, stalemate
- Player vs Player and Player vs Computer modes
- Minimax search with alpha-beta pruning at depth 3
- Piece-square table positional evaluation
- Legal move highlighting (dots and capture rings)
- Last-move highlight on origin and destination squares
- King square pulse animation when in check
- Scrollable algebraic move history
- Captured pieces display with material advantage indicator
- Undo move (reverses both sides in AI mode)
- Coordinate labels around the board
- Pawn promotion modal with piece selection
- "Computer thinking" animation
- Zero external dependencies

```
