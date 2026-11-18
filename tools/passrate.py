#!/usr/bin/env python3
"""Share of pass moves (the head ends where it was) in self-play from real positions.

  passrate.py [PARAM=VALUE ...] [--net FILE] [--iters 2000]
48 positions (turn 14 of harvested arena games, /tmp/posset, written by the snippet in
docs/workflow.md) are each played on for up to 100 turns by SB_BENCH. A snake that stands
still gains nothing; the top players pass on 17-20% of their moves.
"""
import glob, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
a = sys.argv[1:]
def opt(name, default):
    if name in a:
        i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return default
net, iters = opt("--net", None), opt("--iters", "2000")
env = {**os.environ, "SB_BENCH": iters, "SB_SEED": "5"}
if net: env["SB_NET"] = net
for x in a:
    k, v = x.split("="); env["SB_" + k] = v
def one(p):
    o = subprocess.run([os.path.join(ROOT, "build", "bot")], stdin=open(p), capture_output=True, env=env).stdout.decode()
    m = re.search(r"passes: (\d+) of (\d+)", o); s = re.search(r"score (\d+)-(\d+)", o)
    return (int(m.group(1)), int(m.group(2)), int(s.group(1)) + int(s.group(2))) if m else (0, 0, 0)
with ThreadPoolExecutor(7) as ex: r = list(ex.map(one, sorted(glob.glob("/tmp/posset/p*.txt"))))
print(f"{' '.join(a) or 'defaults':60} passes {100 * sum(x[0] for x in r) / max(1, sum(x[1] for x in r)):5.1f}%   total length at the end {sum(x[2] for x in r)}")
