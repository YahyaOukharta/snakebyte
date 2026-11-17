#!/usr/bin/env python3
"""An opponent on rails as a test: stretches of real arena games against one player.

  railset.py BOT[@PARAMS] [BOT2 ...] [--opp _H3R0_] [--every 12] [--turns 15] [--net FILE] [--iters 6000] [--max N]
Every harvested game against OPP is cut at turns 4, 4 + EVERY, ...; from each cut the build
plays our snakes for TURNS turns while his snakes repeat the moves he really played
(referee --rail). Score of a stretch: change of (our length - his length). Reported per
build: mean per stretch with its standard error, wall hits and snakes lost per 100
stretches, and for the builds after the first the paired difference to the first.
He would have answered other moves differently; this measures collecting, colliding and
getting shut in against his real movement, quickly (about a minute per build).
"""
import json, math, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
a = sys.argv[1:]
def opt(name, default):
    if name in a:
        i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return default
opp, every, turns, net, iters, mx = opt("--opp", "_H3R0_"), int(opt("--every", 12)), int(opt("--turns", 15)), opt("--net", None), opt("--iters", "6000"), int(opt("--max", 100000))
rows = [json.loads(l) for l in open(os.path.join(ROOT, "arena", "traces", "index.jsonl"))]
segs = [(r["game_id"], t) for r in rows if r["opponent"] in opp.split(",") and not r["mismatch"] for t in range(4, r["turns"] - turns, every)][:mx]
env = {**os.environ}
if net: env["SB_NET"] = net

def one(bot, g, t):
    o = subprocess.run([os.path.join(ROOT, "build", "referee"), "--rail", os.path.join(ROOT, "arena", "traces", f"{g}.trace"), str(t), str(turns), bot, iters],
                       capture_output=True, env=env).stdout.decode()
    m = re.search(r"RAIL walls (\d+) deaths (\d+) lost (\d+) len (\d+)->(\d+) opp (\d+)->(\d+)", o)
    if not m: return None
    w, d, l, a0, a1, b0, b1 = map(int, m.groups())
    return ((a1 - a0) - (b1 - b0), w, d)

print(f"{len(segs)} stretches of {turns} turns against {opp}")
base = None
for bot in a:
    with ThreadPoolExecutor(7) as ex:
        res = list(ex.map(lambda x: one(bot, *x), segs))
    v = [r[0] for r in res if r]; n = len(v)
    m = sum(v) / n; se = (sum((x - m) ** 2 for x in v) / (n - 1)) ** 0.5 / math.sqrt(n)
    line = f"{bot[-70:]:70} margin {m:+.3f} +/- {se:.3f}  walls/100 {100 * sum(r[1] for r in res if r) / n:5.1f}  snakes lost/100 {100 * sum(r[2] for r in res if r) / n:5.1f}"
    if base is None: base = res
    else:
        d = [r[0] - b[0] for r, b in zip(res, base) if r and b]; k = len(d)
        dm = sum(d) / k; dse = (sum((x - dm) ** 2 for x in d) / (k - 1)) ** 0.5 / math.sqrt(k)
        line += f"   vs first {dm:+.3f} +/- {dse:.3f}"
    print(line, flush=True)
