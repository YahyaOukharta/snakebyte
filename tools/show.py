#!/usr/bin/env python3
"""Print the board of a harvested game at a turn.

  show.py GAME_ID TURN [SNAKE]      arena/traces/GAME_ID.trace; our snakes a-d, theirs w-z,
                                    heads in capitals; SNAKE (engine index) is drawn as @ and *
"""
import os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def board(game, turn, mark=-1):
    txt = subprocess.check_output([os.path.join(ROOT, "build", "referee"), "--trace", os.path.join(ROOT, "arena", "traces", f"{game}.trace"), str(turn)]).decode().split("\n")
    W, H = int(txt[1]), int(txt[2])
    g = [list(r) for r in txt[3:3 + H]]
    k = 3 + H; per = int(txt[k]); k += 1 + 2 * per
    na = int(txt[k])
    for j in range(na):
        x, y = map(int, txt[k + 1 + j].split()); g[y][x] = "o"
    k += 1 + na; ns = int(txt[k]); info = []
    for j in range(ns):
        sid, body = txt[k + 1 + j].split(); sid = int(sid)
        cells = [tuple(map(int, c.split(","))) for c in body.split(":")]
        info.append((sid, len(cells), cells[0]))
        for q, (x, y) in enumerate(cells):
            if 0 <= x < W and 0 <= y < H:
                ch = ("abcd" if sid < per else "wxyz")[sid % per]
                g[y][x] = ("@" if q == 0 else "*") if sid == mark else (ch.upper() if q == 0 else ch)
    return g, info


if __name__ == "__main__":
    game, turn = sys.argv[1], int(sys.argv[2])
    g, info = board(game, turn, int(sys.argv[3]) if len(sys.argv) > 3 else -1)
    print(f"game {game} turn {turn}: " + "  ".join(f"{s}:len{l}@{h[0]},{h[1]}" for s, l, h in info))
    for row in g: print("   " + "".join(row))
