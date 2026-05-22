# Changelog

All notable changes to this project are documented here.  
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
