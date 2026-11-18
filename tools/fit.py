#!/usr/bin/env python3
"""Fit the evaluation to a stronger player's moves: coordinate ascent on tools/agree.py.

  fit.py [--opp _H3R0_] [--net FILE] [--rounds 2] [START=VALUE ...]
The harvested games against OPP are split 70 / 30. Each parameter in turn is tried at a
few values around its current one and kept where the agreement on the 70% rises by more
than 0.12 points; at the end the 30% and the other top player are scored with the start
and with the result. The log goes to league/fit.jsonl.
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import agree
a = sys.argv[1:]
def opt(name, default):
    if name in a:
        i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return default
opp, net, rounds = opt("--opp", "_H3R0_"), opt("--net", None), int(opt("--rounds", 2))
cur = {k: float(v) for k, v in (x.split("=") for x in a)}
# name: (default in bot.cpp, candidate factors or absolute values)
GRID = {"EV_TEMPO": [0.06, 0.15, 0.22, 0.3], "EV_K": [11, 20, 26], "NET_A": [0.6, 0.8, 1.3], "EV_MEAN": [0.2, 0.5], "EV_SELF": [0.45, 0.85, 1.1], "EV_OWN": [0.05, 0.12, 0.3],
        "EV_VOR": [0.15, 0.35, 0.5], "EV_SPACE": [0.5, 1.5, 2.5], "UCB_C": [0.6, 0.9, 1.1], "EV_ELEV": [0.0, 0.02, 0.07], "EV_ALIVE": [0.5, 2.2, 3.5],
        "EV_S2": [0.035, 0.055], "EV_S1": [0.0, 0.035], "EV_WIN": [5, 13, 20], "EV_LOSS": [0.0, 0.12, 0.25], "SP_MIN": [6, 10], "EV_TRAP": [0.5, 1.5], "D_STEP2": [700, 1500],
        "EV_PASS": [-0.05, 0.05], "EV_STUCK": [0.5]}
tr = lambda p: agree.score(p, opp, net=net, part=(0.0, 0.7))[0]
start = dict(cur)
best = tr(cur); print(f"start {100 * best:.2f}% on the fitting games  {cur}", flush=True)
log = os.path.join(agree.ROOT, "league", "fit.jsonl")
for r in range(rounds):
    for name, vals in GRID.items():
        for v in vals:
            if cur.get(name) == v: continue
            p = {**cur, name: v}
            s = tr(p)
            open(log, "a").write(json.dumps({"opp": opp, "params": p, "agree": s}) + "\n")
            if s > best + 0.0012:
                best, cur = s, p
                print(f"  {name}={v}: {100 * s:.2f}%  kept", flush=True)
print("result:", cur, flush=True)
for label, o, part in (("held-out games of " + opp, opp, (0.7, 1.0)), ("sZoom" if opp != "sZoom" else "_H3R0_", "sZoom" if opp != "sZoom" else "_H3R0_", (0.0, 1.0))):
    s0 = agree.score(start, o, net=net, part=part)[0]; s1 = agree.score(cur, o, net=net, part=part)[0]
    d0 = agree.score({}, o, net=net, part=part)[0]
    print(f"{label}: defaults {100 * d0:.2f}%  start {100 * s0:.2f}%  fitted {100 * s1:.2f}%", flush=True)
