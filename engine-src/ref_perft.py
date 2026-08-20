#!/usr/bin/env python3
"""
Independent reference chess move generator, written from scratch (not derived
from the C or JS engines) purely to cross-validate perft results via a
completely separate implementation path. Board: dict (r,c)->(color,type) or
absent for empty. color: 'w'/'b'. type: 'P','N','B','R','Q','K'.
"""
import sys, copy

FILES = "abcdefgh"
RANKS = "87654321"

def in_b(r, c):
    return 0 <= r < 8 and 0 <= c < 8

def opp(color):
    return 'b' if color == 'w' else 'w'

def parse_fen_rows(rows):
    board = {}
    for r, row in enumerate(rows):
        c = 0
        for ch in row:
            if ch.isdigit():
                c += int(ch)
                continue
            color = 'b' if ch.islower() else 'w'
            board[(r, c)] = (color, ch.upper())
            c += 1
    return board

def gen_pseudo(board, r, c, ep, cr):
    color, ptype = board[(r, c)]
    moves = []
    if ptype == 'P':
        d = -1 if color == 'w' else 1
        sr = 6 if color == 'w' else 1
        if in_b(r+d, c) and (r+d, c) not in board:
            moves.append((r, c, r+d, c, None))
            if r == sr and (r+2*d, c) not in board:
                moves.append((r, c, r+2*d, c, None))
        for dc in (-1, 1):
            if not in_b(r+d, c+dc):
                continue
            tgt = board.get((r+d, c+dc))
            if tgt and tgt[0] != color:
                moves.append((r, c, r+d, c+dc, None))
            elif ep == (r+d, c+dc):
                moves.append((r, c, r+d, c+dc, None))
    elif ptype == 'N':
        for dr, dc in [(-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)]:
            nr, nc = r+dr, c+dc
            if in_b(nr, nc):
                tgt = board.get((nr, nc))
                if not tgt or tgt[0] != color:
                    moves.append((r, c, nr, nc, None))
    elif ptype in ('B', 'R', 'Q'):
        dirs = []
        if ptype in ('B', 'Q'): dirs += [(-1,-1),(-1,1),(1,-1),(1,1)]
        if ptype in ('R', 'Q'): dirs += [(-1,0),(1,0),(0,-1),(0,1)]
        for dr, dc in dirs:
            nr, nc = r+dr, c+dc
            while in_b(nr, nc):
                tgt = board.get((nr, nc))
                if tgt:
                    if tgt[0] != color:
                        moves.append((r, c, nr, nc, None))
                    break
                moves.append((r, c, nr, nc, None))
                nr += dr; nc += dc
    elif ptype == 'K':
        for dr in (-1,0,1):
            for dc in (-1,0,1):
                if dr==0 and dc==0: continue
                nr, nc = r+dr, c+dc
                if in_b(nr, nc):
                    tgt = board.get((nr, nc))
                    if not tgt or tgt[0] != color:
                        moves.append((r, c, nr, nc, None))
        backR = 7 if color == 'w' else 0
        if r == backR and c == 4:
            k_right = cr[color]['K']
            k_left = cr[color]['Q']
            kr = board.get((backR, 7))
            qr = board.get((backR, 0))
            if k_right and (backR,5) not in board and (backR,6) not in board and kr == (color,'R'):
                moves.append((r, c, backR, 6, 'K'))
            if k_left and (backR,3) not in board and (backR,2) not in board and (backR,1) not in board and qr == (color,'R'):
                moves.append((r, c, backR, 2, 'Q'))
    return moves

def is_attacked(board, r, c, by_color, ep, cr):
    for (pr, pc), (pcolor, ptype) in board.items():
        if pcolor != by_color:
            continue
        for mv in gen_pseudo(board, pr, pc, ep, cr):
            if mv[2] == r and mv[3] == c:
                return True
    return False

def find_king(board, color):
    for pos, (pcolor, ptype) in board.items():
        if pcolor == color and ptype == 'K':
            return pos
    return None

def is_in_check(board, color, ep, cr):
    king = find_king(board, color)
    if king is None:
        return True
    return is_attacked(board, king[0], king[1], opp(color), ep, cr)

def apply_move(board, mv, cr):
    fr, fc, tr, tc, castle = mv
    color, ptype = board[(fr, fc)]
    new_board = dict(board)
    new_cr = {k: dict(v) for k, v in cr.items()}
    captured = new_board.get((tr, tc))
    is_ep = ptype == 'P' and fc != tc and (tr, tc) not in board
    if is_ep:
        captured = new_board.pop((fr, tc))
    del new_board[(fr, fc)]
    new_board[(tr, tc)] = (color, ptype)
    if castle == 'K':
        rook = new_board.pop((fr, 7))
        new_board[(fr, 5)] = rook
    elif castle == 'Q':
        rook = new_board.pop((fr, 0))
        new_board[(fr, 3)] = rook
    if ptype == 'P' and (tr == 0 or tr == 7):
        new_board[(tr, tc)] = (color, 'Q')
    if ptype == 'K':
        new_cr[color]['K'] = False
        new_cr[color]['Q'] = False
    if ptype == 'R':
        br = 7 if color == 'w' else 0
        if fr == br and fc == 7: new_cr[color]['K'] = False
        if fr == br and fc == 0: new_cr[color]['Q'] = False
    if captured and captured[1] == 'R':
        ccolor = captured[0]
        br2 = 7 if ccolor == 'w' else 0
        if tr == br2 and tc == 7: new_cr[ccolor]['K'] = False
        if tr == br2 and tc == 0: new_cr[ccolor]['Q'] = False
    new_ep = None
    if ptype == 'P' and abs(tr - fr) == 2:
        new_ep = ((fr+tr)//2, fc)
    return new_board, new_cr, new_ep

def legal_moves(board, color, ep, cr):
    out = []
    for (r, c), (pcolor, ptype) in list(board.items()):
        if pcolor != color:
            continue
        for mv in gen_pseudo(board, r, c, ep, cr):
            fr, fc, tr, tc, castle = mv
            if ptype == 'K' and abs(tc - c) == 2:
                step = 1 if tc > c else -1
                safe = True
                sq = c
                while safe and sq != tc + step:
                    tmp = dict(board)
                    del tmp[(r, c)]
                    tmp[(r, sq)] = (color, 'K')
                    if is_attacked(tmp, r, sq, opp(color), ep, cr):
                        safe = False
                    sq += step
                if not safe:
                    continue
            nb, ncr, nep = apply_move(board, mv, cr)
            if not is_in_check(nb, color, None, ncr):
                out.append(mv)
    return out

def perft(board, depth, color, ep, cr):
    if depth == 0:
        return 1
    moves = legal_moves(board, color, ep, cr)
    if depth == 1:
        return len(moves)
    total = 0
    for mv in moves:
        nb, ncr, nep = apply_move(board, mv, cr)
        total += perft(nb, depth-1, opp(color), nep, ncr)
    return total

def divide(board, depth, color, ep, cr):
    moves = legal_moves(board, color, ep, cr)
    results = {}
    for mv in moves:
        nb, ncr, nep = apply_move(board, mv, cr)
        cnt = perft(nb, depth-1, opp(color), nep, ncr)
        key = FILES[mv[1]]+RANKS[mv[0]]+FILES[mv[3]]+RANKS[mv[2]]
        results[key] = cnt
    return results

if __name__ == '__main__':
    kiwipete_rows = ["r3k2r","p1ppqpb1","bn2pnp1","3PN3","1p2P3","2N2Q1p","PPPBBPPP","R3K2R"]
    board = parse_fen_rows(kiwipete_rows)
    cr = {'w': {'K': True, 'Q': True}, 'b': {'K': True, 'Q': True}}
    depth = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    mode = sys.argv[2] if len(sys.argv) > 2 else 'divide'
    if mode == 'divide':
        results = divide(board, depth, 'w', None, cr)
        total = 0
        for k in sorted(results):
            print(f"{k}: {results[k]}")
            total += results[k]
        print(f"total: {total}")
    else:
        print(perft(board, depth, 'w', None, cr))
