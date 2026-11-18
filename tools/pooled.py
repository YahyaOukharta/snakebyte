#!/usr/bin/env python3
"""Pooled arena results per version, standardised for the mix of opponents.

  pooled.py NAME=SUB[,SUB...] [NAME=SUB...]
One arena run is 159 games against whoever the matchmaker picks; the share of games against
the top two changes the score by itself. Here every run of a version is pooled, the score
rate is taken apart for the top two, the ten most frequent other opponents and the rest,
and put together again with fixed weights (21% / 62% / 17%).
"""
import collections, json, math, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = [json.loads(l) for l in open(os.path.join(ROOT, "arena", "battles.jsonl"))]
V = {a.split("=")[0]: [int(x) for x in a.split("=")[1].split(",")] for a in sys.argv[1:]}
first = min(min(s) for s in V.values())
top = {"sZoom", "_H3R0_"}
cnt = collections.Counter(b["opponent"] for b in B if b["submission"] >= first)
mid = {o for o, _ in cnt.most_common(12)} - top
for v, subs in V.items():
    g = {"top": [0, 0], "mid": [0, 0], "rest": [0, 0]}
    for b in B:
        if b["submission"] not in subs: continue
        k = "top" if b["opponent"] in top else "mid" if b["opponent"] in mid else "rest"
        r = b["result"]
        g[k][0] += 1 if r.startswith("w") else 0.5 if r.startswith("d") else 0
        g[k][1] += 1
    p = {k: g[k][0] / max(1, g[k][1]) for k in g}
    n = sum(x[1] for x in g.values())
    std = 0.21 * p["top"] + 0.62 * p["mid"] + 0.17 * p["rest"]
    print(f"{v:8s} games {n:4d}  top two {100 * p['top']:3.0f}% ({g['top'][1]})  next ten {100 * p['mid']:3.0f}% ({g['mid'][1]})  "
          f"rest {100 * p['rest']:3.0f}% ({g['rest'][1]})  standardised {100 * std:.1f}% +/- {45 / math.sqrt(max(1, n)):.1f}")
