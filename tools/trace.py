#!/usr/bin/env python3
"""Rebuild a CodinGame game from a dump-build log and replay it through the engine.

  trace.py LOG [-o OUT.trace]      prints the engine's line for every turn (referee --trace)
The first position comes from the seed in the log through the official map generator
(tools/mapdump/build.sh, needs referee-src and a JDK); without it, from the board a build with
`#define DUMP n` printed on turn 0. The replay is checked against the score our bot printed on
every turn: a mismatch means the reconstruction is wrong.
"""
import os, re, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIRS = {"UP": 0, "RIGHT": 1, "DOWN": 2, "LEFT": 3}


def from_seed(text):
    """First position from the seed in the log, through the official generator (tools/mapdump)."""
    cp = os.path.join(ROOT, "build", "mapdump")
    seed = re.search(r"seed=(-?\d+)", text).group(1)
    out = subprocess.check_output(["java", "-cp", cp + ":" + open(os.path.join(cp, "classpath")).read().strip(), "MapDump", seed]).decode().split("\n")
    W, H = map(int, out[1].split())
    grid = [list(r) for r in out[2:2 + H]]
    k = 2 + H
    na = int(out[k]); apples = [tuple(map(int, l.split())) for l in out[k + 1:k + 1 + na]]
    k += 1 + na
    nb = int(out[k]); birds = []
    for l in out[k + 1:k + 1 + nb]:
        v = list(map(int, l.split()))
        birds.append([(v[i], v[i + 1]) for i in range(0, len(v), 2)])
    return W, H, grid, apples, birds


def parse(path):
    text = open(path).read()
    frames = re.split(r"\nframe (\d+)(?: agent (\d))?", text)
    me = int(re.search(r"\n  (\d) \S+ \(me", text).group(1))      # our seat: our snakes come first in the engine
    if os.path.exists(os.path.join(ROOT, "build", "mapdump", "MapDump.class")):
        W, H, grid, apples, birds = from_seed(text)
        per = len(birds) // 2
        snakes = birds if me == 0 else birds[per:] + birds[:per]
    else:                                              # no generator: the board a dump build printed on turn 0
        m = re.search(r"tables \d+ ms, \d+ apples, (\d+)x(\d+)", text)
        W, H = int(m.group(1)), int(m.group(2))
        first = text[text.index("tables "):]
        lines = first[:first.index("\n  stdout:")].split("\n")
        board = lines[-(H + 2):]
        x0 = board[-1].index("#")                      # the floor starts at x = 0
        grid = [[(row + " " * 200)[x0 + x] for x in range(W)] for row in board[2:]]
        apples = [(x, y) for y in range(H) for x in range(W) if grid[y][x] == "o"]
        per = len(set(c.lower() for row in grid for c in row if c.isalpha() and c not in "o")) // 2
        snakes = []
        for owner, base in ((0, "a"), (1, "w")):
            for k in range(per):
                ch = chr(ord(base) + k)
                cells = [(x, y) for y in range(H) for x in range(W) if grid[y][x].lower() == ch]
                head = next(c for c in cells if grid[c[1]][c[0]].isupper())
                body, rest = [head], [c for c in cells if c != head]
                while rest:                            # follow the body from the head
                    nxt = next(c for c in rest if abs(c[0] - body[-1][0]) + abs(c[1] - body[-1][1]) == 1)
                    body.append(nxt), rest.remove(nxt)
                snakes.append(body)
    n = 2 * per
    # moves: one stdout per agent per turn
    outs = {0: [], 1: []}
    for i in range(1, len(frames), 3):
        ag, body = frames[i + 1], frames[i + 2]
        if ag is None: continue
        so = re.search(r"\n  stdout: (.*)", body)
        outs[int(ag)].append(so.group(1) if so else "")
    turns = min(len(outs[0]), len(outs[1]))
    moves = []
    for t in range(turns):
        mv = [-1] * n
        for part in (outs[0][t] + ";" + outs[1][t]).split(";"):
            w = part.split()
            if len(w) >= 2 and w[0].isdigit() and w[1] in DIRS and int(w[0]) < n:
                j = int(w[0])
                if me == 1: j = j - per if j >= per else j + per
                mv[j] = DIRS[w[1]]
        moves.append(mv)
    mine = re.findall(r"t(\d+) it \d+ d \d+ sims \d+ nodes \d+ [\d.]+ms desync \d+ L (\d+)/(\d+) score (\d+)-(\d+) apples (\d+)", text)
    return W, H, grid, apples, snakes, moves, mine


def write(path, W, H, grid, apples, snakes, moves):
    with open(path, "w") as f:
        f.write(f"{W} {H}\n")
        for row in grid: f.write("".join("#" if c == "#" else "." for c in row) + "\n")
        f.write(f"{len(apples)}\n" + "".join(f"{x} {y}\n" for x, y in apples))
        f.write(f"{len(snakes)}\n" + "".join(" ".join(f"{x} {y}" for x, y in b[:3]) + "\n" for b in snakes))
        f.write(f"{len(moves)}\n" + "".join(" ".join(map(str, m)) + "\n" for m in moves))


def run(log, out=None):
    W, H, grid, apples, snakes, moves, mine = parse(log)
    out = out or "/tmp/" + os.path.basename(log) + ".trace"
    write(out, W, H, grid, apples, snakes, moves)
    res = subprocess.check_output([os.path.join(ROOT, "build", "referee"), "--trace", out]).decode().strip().split("\n")
    bad = 0
    for t, l1, l2, s0, s1, ap in mine:
        t = int(t)
        if t >= len(res): break
        f = res[t].split("|")
        if f[1].split() != [s0, s1] or f[3].strip() != ap: bad += 1
    return res, bad, len(snakes)


if __name__ == "__main__":
    if len(sys.argv) > 3 and sys.argv[2] == "--pos":       # trace.py LOG --pos TURN: the bot's input at that turn
        run(sys.argv[1], "/tmp/pos.trace")
        sys.stdout.write(subprocess.check_output([os.path.join(ROOT, "build", "referee"), "--trace", "/tmp/pos.trace", sys.argv[3]]).decode())
        sys.exit(0)
    res, bad, n = run(sys.argv[1], sys.argv[3] if len(sys.argv) > 3 else None)
    print("\n".join(res))
    print(f"# {len(res)} positions, {bad} turns disagree with the bot's own score line", file=sys.stderr)
