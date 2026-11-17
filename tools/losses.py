#!/usr/bin/env python3
"""Every collision in the harvested arena games (arena/traces/), by cause and by opponent group.

  losses.py
Causes: W wall, S own body, B another snake's body, H two heads on one cell, F fell out of
the game. "died" = the whole snake was removed (length 3 or less). Rates are per game.
"""
import collections, json, os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(ROOT, "arena", "traces")
TOP = {"sZoom", "_H3R0_"}
rows = [json.loads(l) for l in open(os.path.join(T, "index.jsonl"))]
subs = set(int(x) for x in sys.argv[1:]) if len(sys.argv) > 1 else None
agg = collections.defaultdict(collections.Counter); games = collections.Counter(); when = collections.defaultdict(list)
for r in rows:
    if r["mismatch"] or (subs and r["submission"] not in subs): continue
    out = subprocess.check_output([os.path.join(ROOT, "build", "referee"), "--trace", os.path.join(T, f"{r['game_id']}.trace")]).decode().split("\n")
    grp = "top2" if r["opponent"] in TOP else "rest"
    games[grp] += 1
    n = r["snakes"]
    for line in out:
        f = line.split("|")
        if len(f) < 6: continue
        turn = int(f[0])
        for e in f[5].split():
            if not e.startswith("!"): continue
            i, cause, other, ln = e[1:].split(":")
            i, other, ln = int(i), int(other), int(ln)
            side = "us" if i < n // 2 else "them"
            kind = cause
            if cause in "BH":
                same = (other < n // 2) == (i < n // 2)
                kind = cause + ("-team" if same else "-enemy")
            died = ln <= 3 or cause == "F"
            agg[(grp, side)][kind] += 1
            agg[(grp, side)]["parts"] += ln if died else 1
            if died: agg[(grp, side)]["died"] += 1
            when[(grp, side)].append(turn)
KINDS = ["W", "S", "B-enemy", "B-team", "H-enemy", "H-team", "F", "died", "parts"]
print(f"{'':10}" + "".join(f"{k:>9}" for k in KINDS) + "   median turn")
for grp in ("top2", "rest"):
    for side in ("us", "them"):
        c = agg[(grp, side)]; g = games[grp] or 1
        w = sorted(when[(grp, side)])
        print(f"{grp:5}{side:5}" + "".join(f"{c[k] / g:9.2f}" for k in KINDS) + f"   {w[len(w) // 2] if w else 0}   ({games[grp]} games)")
