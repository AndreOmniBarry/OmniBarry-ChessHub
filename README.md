# ♟ OmniBarry ChessHub

> A fully playable chess engine — vanilla HTML, CSS, and JavaScript.  
> No frameworks. No libraries. No server. One file.

[![Live Demo](https://img.shields.io/badge/Live%20Demo-Visit%20Site-4a7c59?style=for-the-badge)](https://omnibarry-chesshub-now.vercel.app/)


## Preview

![OBChessHub Preview](assets/preview.png)

---

## What This Is

OmniBarry ChessHub is a complete, production-quality chess application built without a single dependency. No Chess.js. No Stockfish. No jQuery. No React. Every rule, every AI decision, every pixel — written from scratch in one HTML file.

It is built to be played by anyone from a first-time beginner to a competitive club player, with a difficulty system that scales from forgiving to formidable.

---

## Features

### Full FIDE Rule Enforcement
- Legal move validation — king cannot move into or remain in check
- Castling — kingside and queenside, with full path-safety enforcement (king cannot pass through or land on an attacked square)
- En passant capture
- Pawn promotion with interactive piece-selection UI
- Check, checkmate, and stalemate detection

### Game Modes
- **Player vs Player** — local two-player on one device
- **vs Computer** — built-in AI opponent with four difficulty levels
- **Play as Black** — flip the board, AI opens as White

### Difficulty Levels
| Level | Behaviour | Audience |
|---|---|---|
| **Easy** | Depth 1, deliberate mistakes, high blunder rate | Beginners, children |
| **Medium** | Depth 2, occasional oversights, noise injection | Casual players |
| **Hard** | Depth 3 + quiescence, no gifts, tactical awareness | Club players |
| **GoPro** | Depth 4 + full quiescence, maximum engine strength | Serious challengers |

### AI Engine — How It Thinks

The engine runs a **minimax tree search** with **alpha-beta pruning**, extended by **quiescence search** on Hard and GoPro to prevent scoring positions mid-exchange.

**Evaluation:**
- Material value (Pawn 100 → Queen 900 → King 20,000)
- Piece-square table positional bonuses (centralised knights, active bishops, advanced pawns, king safety)
- Personality bias — the AI draws one of three styles each game: aggressive, positional, or solid. Same strength, different character.

**Five layers of genuine variation** (why the AI never plays the same game twice):
1. **Opening book** — 20+ named openings (Sicilian, French, Caro-Kann, King's Indian, Dutch, Benoni, Alekhine, English, and more), chosen randomly per game
2. **Blunder injection** — Easy and Medium have a difficulty-scaled probability of ignoring the engine entirely, simulating human oversight
3. **Move order shuffle** — candidate list randomised before search; alpha-beta is order-sensitive, so equal lines surface differently each call
4. **Gaussian noise** — Easy and Medium add noise to move scores before pooling, so equivalent moves don't score identically
5. **Dynamic candidate pool** — all moves within a phase-scaled centipawn window of the best score are collected; one is chosen randomly. Opening phase: wider window. Endgame: tighter. The AI never deterministically locks onto a single move.

### UI & Experience
- Legal move highlighting — dots for empty squares, rings for captures
- Last-move highlight on both origin and destination squares
- King square pulses red when in check
- Scrollable algebraic move history (SAN notation)
- Captured pieces display with live material advantage indicator
- "Computer thinking" animation during AI calculation
- Coordinate labels (a–h / 1–8), correct orientation on flip
- Pawn promotion modal with piece selection

### Flip Board
Switch sides at any time in vs Computer mode. When playing as Black, the board renders from Black's perspective, coordinates reverse, and the AI opens as White automatically.

### How to Castle
Click your **king**. If castling is legal, a move dot will appear two squares toward the rook. Click it. The rook moves automatically. You never click the rook to castle.

Castling is unavailable if: the king or that rook has previously moved, any square between them is occupied, or the king is in check, passes through check, or would land in check.

### Responsive Design
- Playable on every screen from 280px (Galaxy Fold closed) to 4K
- Board-first layout — `--sq` CSS variable drives all sizing
- Landscape orientation handled with a dedicated breakpoint
- Minimum 44px touch targets on all interactive elements (WCAG AA)
- Sidebar reflows below board on mobile, beside it on tablet and desktop

---

## Architecture

Single `pack.html` file. Six internal sections:

| Section | Responsibility |
|---|---|
| HTML Markup | Board grid, coordinate labels, sidebar, promotion modal |
| CSS Styles | Board theme, highlights, difficulty/flip UI, responsive layout |
| Game State | Board array, turn, castling rights, en passant, flip state |
| Move Generation | Pseudo-legal moves per piece + check filtering = fully legal |
| AI Engine | Minimax + alpha-beta + quiescence + difficulty + personality |
| UI & Events | DOM rendering, click handling, history, captured pieces |

---

## Running Locally

Download `pack.html`. Open it in any modern browser. That is the entire setup.

No build step. No `npm install`. No config. No server.

---

## Browser Support

| Browser | Support |
|---|---|
| Chrome / Edge 90+ | ✅ Full |
| Firefox 88+ | ✅ Full |
| Safari 14+ | ✅ Full |
| Samsung Internet 14+ | ✅ Full |
| Opera 76+ | ✅ Full |

---

## Roadmap

- [ ] Board theme switcher (Classic, Green, Midnight, Blue)
- [ ] Move timer / clock mode (Blitz 5+0, Rapid 10+0)
- [ ] PGN export of completed games
- [ ] Puzzle mode — tactical positions for beginner skill-building
- [ ] Transposition table — faster depth-5 search with position memory
- [ ] Adaptive difficulty — AI adjusts to how you play mid-session
- [ ] Live spectator / share link for remote challenge mode

---

## License

MIT — free to use, modify, and distribute. See [LICENSE](LICENSE) for full terms.

---

## Author

**Andre Courage Aganmwonyi-Barry Osas**  
Built with precision. No shortcuts. No libraries.

> *"There are 1000 ways to win. The engine knows all of them."*
