#!/usr/bin/env python3
"""Freezes replayed: does a build still stand still where a snake of ours did in the arena?

  freeze.py BOT[@PARAMS] [BOT2 ...] [--subs ID,ID] [--net FILE] [--turns 40] [--max N]
An incident is a snake of ours whose head stays on one cell for 10 turns in a harvested
arena game (of the submissions given, default: all). The game is replayed to 6 turns before
that, then the build plays our snakes for TURNS turns against the opponent's recorded moves
(referee --rail). Per build: our length change summed over the incidents, and the parts lost.
"""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
a = sys.argv[1:]
def opt(name, default):
    if name in a:
        i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return default
subs, net, turns, mx = opt("--subs", ""), opt("--net", None), int(opt("--turns", 40)), int(opt("--max", 60))
subs = {int(x) for x in subs.split(",") if x}
env = {**os.environ}
if net: env["SB_NET"] = net
REF = os.path.join(ROOT, "build", "referee"); T = os.path.join(ROOT, "arena", "traces")
inc = []
for r in map(json.loads, open(os.path.join(T, "index.jsonl"))):
    if r["mismatch"] or (subs and r["submission"] not in subs): continue
    out = subprocess.check_output([REF, "--trace", os.path.join(T, f"{r['game_id']}.trace")]).decode().split("\n")
    n = r["snakes"]; pos = []
    for line in out:
        f = line.split("|")
        if len(f) < 6: continue
        v = f[4].split(); pos.append([tuple(int(x) for x in v[i * 7:i * 7 + 7]) for i in range(n)])
    done = set()
    for t in range(22, len(pos) - turns):
        for i in range(n // 2):
            if i in done or not pos[t][i][0]: continue
            if all(pos[t - j][i][0] and pos[t - j][i][2:4] == pos[t][i][2:4] for j in range(1, 10)):
                inc.append((r["game_id"], t - 15)); done.add(i)
inc = inc[:mx]
def one(bot, g, t):
    o = subprocess.run([REF, "--rail", os.path.join(T, f"{g}.trace"), str(t), str(turns), bot], capture_output=True, env=env).stdout.decode()
    m = re.search(r"RAIL walls (\d+) deaths (\d+) lost (\d+) len (\d+)->(\d+)", o)
    return tuple(map(int, m.groups())) if m else None
print(len(inc), "freeze incidents")
for bot in a:
    with ThreadPoolExecutor(7) as ex: res = [x for x in ex.map(lambda q: one(bot, *q), inc) if x]
    print(f"{bot[-64:]:64} length change {sum(x[4] - x[3] for x in res):+4d}  parts lost {sum(x[2] for x in res):3d}  wall hits {sum(x[0] for x in res):3d}  ({len(res)} replays)", flush=True)
