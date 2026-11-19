#!/usr/bin/env python3
"""Situations to solve: positions from real arena games a few turns before one of our snakes
was shut in, bled on a wall or died, and a score for a build on them.

  situations.py build [--since SUBMISSION]          -> arena/situations.jsonl
  situations.py run BOT[@PARAMS] [BOT2 ...] [--iters 3500] [--net FILE] [--kind K] [--max N]
A situation is (game, turn T, window K, kind, parts our side lost in the game during the
window). Kinds:
  bleed   a snake of ours hit a wall three times or more within ten turns; T is 8 turns
          before the first hit, K = 22
  death   a snake of ours was removed (length 3 or less in a collision, or fell out); T is 5
          turns before, K = 8
  wall    a single wall hit of a longer snake; T is 5 turns before, K = 8
`run` replays each one on rails (referee --rail: our snakes are played afresh from T, the
opponent repeats his real moves) and reports per build and kind: situations where the build
loses fewer parts than the game did (solved), the same, more; parts lost per situation
against the game's. He would have answered other moves differently, so a solved situation
is not a won game: it says the build does not walk into the same thing.
"""
import collections, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(ROOT, "arena", "traces")
SET = os.path.join(ROOT, "arena", "situations.jsonl")
REF = os.path.join(ROOT, "build", "referee")


def events(game):
    """[(turn, snake, cause, length before)] for every collision, and the number of snakes."""
    out = subprocess.check_output([REF, "--trace", os.path.join(T, f"{game}.trace")]).decode().split("\n")
    ev, n = [], 0
    for line in out:
        f = line.split("|")
        if len(f) < 6: continue
        n = len(f[4].split()) // 7
        for e in f[5].split():
            if e.startswith("!"):
                i, c, o, ln = e[1:].split(":")
                ev.append((int(f[0]), int(i), c, int(ln)))
    return ev, n


def build(since):
    rows = [json.loads(l) for l in open(os.path.join(T, "index.jsonl"))]
    out, cnt = [], collections.Counter()
    for r in rows:
        if r["mismatch"] or r["submission"] < since: continue
        ev, n = events(r["game_id"])
        mine = [e for e in ev if e[1] < n // 2]
        lost = lambda a, b: sum((e[3] if (e[3] <= 3 or e[2] == "F") else 1) for e in mine if a <= e[0] < b)
        used = set()
        walls = collections.defaultdict(list)
        for t, i, c, ln in mine:
            if c == "W": walls[i].append(t)
        for i, ts in walls.items():                              # bleeds first: they contain the single hits
            for k, t in enumerate(ts):
                if (i, t) in used: continue
                run = [u for u in ts if t <= u < t + 10]
                if len(run) >= 3:
                    t0 = max(1, t - 8)
                    out.append({"game": r["game_id"], "turn": t0, "k": 22, "kind": "bleed", "snake": i, "lost": lost(t0, t0 + 22), "opp": r["opponent"]})
                    used.update((i, u) for u in ts if t <= u < t + 22)
        for t, i, c, ln in mine:
            if (i, t) in used: continue
            died = ln <= 3 or c == "F"
            if not died and c != "W": continue
            t0 = max(1, t - 5)
            out.append({"game": r["game_id"], "turn": t0, "k": 8, "kind": "death" if died else "wall", "snake": i, "lost": lost(t0, t0 + 8), "opp": r["opponent"]})
    with open(SET, "w") as f:
        for s in out:
            cnt[s["kind"]] += 1
            f.write(json.dumps(s) + "\n")
    print(len(out), "situations", dict(cnt))


def rail(bot, s, iters):
    o = subprocess.run([REF, "--rail", os.path.join(T, f"{s['game']}.trace"), str(s["turn"]), str(s["k"]), bot, str(iters)],
                       capture_output=True, text=True).stdout
    m = re.search(r"RAIL walls (\d+) deaths (\d+) lost (\d+)", o)
    return int(m.group(3)) if m else None


def run(bots, iters, net, kind, mx):
    S = [json.loads(l) for l in open(SET)]
    if kind: S = [s for s in S if s["kind"] == kind]
    if mx: S = S[::max(1, len(S) // mx)][:mx]
    for b in bots:
        spec = b + ((("," if "@" in b else "@") + "SB_NET=" + net) if net else "")
        with ThreadPoolExecutor(7) as ex:
            res = list(ex.map(lambda s: rail(spec, s, iters), S))
        agg = collections.defaultdict(collections.Counter)
        for s, p in zip(S, res):
            if p is None: continue
            for k in (s["kind"], "all"):
                c = agg[k]
                c["n"] += 1; c["game"] += s["lost"]; c["bot"] += p
                c["solved" if p < s["lost"] else "same" if p == s["lost"] else "worse"] += 1
        print(b[-70:])
        for k in ("bleed", "death", "wall", "all"):
            c = agg[k]
            if c["n"]:
                print(f"  {k:6} {c['n']:4}  solved {c['solved']:4} ({100 * c['solved'] / c['n']:4.1f}%)  same {c['same']:4}  worse {c['worse']:4}   parts lost {c['bot'] / c['n']:.2f} (game {c['game'] / c['n']:.2f})")


if __name__ == "__main__":
    a = sys.argv[1:]
    opt = {"--since": "41741000", "--iters": "3500", "--net": "", "--kind": "", "--max": "0"}
    for o in list(opt):
        if o in a:
            j = a.index(o); opt[o] = a[j + 1]; del a[j:j + 2]
    if a[0] == "build": build(int(opt["--since"]))
    else: run(a[1:], int(opt["--iters"]), opt["--net"], opt["--kind"], int(opt["--max"]))
