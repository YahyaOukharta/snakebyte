#!/usr/bin/env python3
"""Do we still walk into it? On-rails replays of the trap incidents.

  rail.py BOT[@PARAMS] [BOT2 ...] [--back 8] [--turns 14] [--net FILE] [--cause W]
For every incident in arena/blunders.jsonl (a snake of ours that hit a wall, i.e. had no
legal move, in an arena game against the top players), the game is replayed up to BACK turns
before the first hit; then the build plays our snakes afresh for TURNS turns while the
opponent's snakes follow the moves he really played (referee --rail). Reported per build:
incidents in which we hit a wall again, wall hits, snakes lost, parts lost, parts gained.
The opponent would not have played the same against other moves, so this measures whether a
build walks into the same situation, not how the game would have ended.
"""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
a = sys.argv[1:]
def opt(name, default):
    if name in a:
        i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return default
back, turns, net, cause = int(opt("--back", 8)), int(opt("--turns", 14)), opt("--net", None), opt("--cause", "W")
cases = [json.loads(l) for l in open(os.path.join(ROOT, "arena", "blunders.jsonl"))]
inc = sorted({(c["game"], c["turn"] + 1) for c in cases if c["cause"] == cause and c["k"] == 1})
env = {**os.environ}
if net: env["SB_NET"] = net

def one(bot, g, T):
    o = subprocess.run([os.path.join(ROOT, "build", "referee"), "--rail", os.path.join(ROOT, "arena", "traces", f"{g}.trace"), str(max(0, T - back)), str(turns), bot],
                       capture_output=True, env=env).stdout.decode()
    m = re.search(r"RAIL walls (\d+) deaths (\d+) lost (\d+) len (\d+)->(\d+)", o)
    return tuple(map(int, m.groups())) if m else None

print(f"{len(inc)} incidents, from {back} turns before the first hit, {turns} turns")
for bot in a:
    with ThreadPoolExecutor(6) as ex:
        res = [r for r in ex.map(lambda x: one(bot, *x), inc) if r]
    n = len(res)
    print(f"{bot[:60]:60} again {sum(r[0] > 0 for r in res):3d}/{n}  wall hits {sum(r[0] for r in res):3d}  snakes lost {sum(r[1] for r in res):3d}  parts lost {sum(r[2] for r in res):3d}  length change {sum(r[4] - r[3] for r in res):+d}")
