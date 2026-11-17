#!/usr/bin/env python3
"""Compare dump-build gauntlet runs: results, margin, and what our snakes do at 7-10 moves
from an apple (turns 10-50), from exact replays.

  gauntlet_stats.py SRC_NAME [SRC_NAME ...]     names as logged in league/gauntlet.jsonl
"""
import collections, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import trace
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = [json.loads(l) for l in open(os.path.join(ROOT, "league", "gauntlet.jsonl"))]
for name in sys.argv[1:]:
    rs = [r for r in rows if r["src"] == name and r["scores"]]
    c = collections.defaultdict(collections.Counter)
    g = 0
    for r in rs:
        try: res, bad, n = trace.run(os.path.join(ROOT, "arena", "logs", r["log"]))
        except Exception: continue
        if bad: continue
        g += 1
        pos = []
        for line in res:
            f = line.split("|"); v = f[4].split()
            pos.append(([tuple(int(x) for x in v[i * 7:i * 7 + 7]) for i in range(n)], set(int(e.split(":")[0]) for e in f[5].split())))
        for t in range(11, min(50, len(pos))):
            for i in range(n):
                a, b = pos[t - 1][0][i], pos[t][0][i]
                if not a[0] or not b[0]: continue
                sd = i >= n // 2
                if i in pos[t][1]: c[sd]["eaten"] += 1
                if 7 <= a[6] <= 10:
                    c[sd]["band"] += 1
                    c[sd]["closer"] += b[6] < a[6]
                    c[sd]["same"] += b[6] == a[6]
                    c[sd]["pass"] += b[6] == a[6] and a[2:4] == b[2:4] and a[1] == b[1]
    w = sum(r["result"] == 1 for r in rs); l = sum(r["result"] == 0 for r in rs)
    m = [r["scores"][0] - r["scores"][1] for r in rs]
    print(f"{name}: {len(rs)} games +{w} -{l} ={len(rs) - w - l}, mean margin {sum(m) / max(1, len(m)):+.2f}, replayed {g}")
    for sd in (False, True):
        k = c[sd]; b = k["band"] or 1
        print(f"    {'them' if sd else 'us  '} at 7-10: closer {100 * k['closer'] / b:.0f}%  same {100 * k['same'] / b:.0f}%  passes/game {k['pass'] / max(g, 1):.1f}   apples t10-50/game {k['eaten'] / max(g, 1):.2f}")
