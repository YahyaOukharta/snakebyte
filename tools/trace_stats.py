#!/usr/bin/env python3
"""Statistics over replayed dump-build games (tools/trace.py): what our snakes and the
opponent's do with their turns.

  trace_stats.py LOG [LOG ...]
For every live snake and turn, with d = table distance to its nearest reachable apple:
  eat      it ate this turn
  closer   d went down
  same     d unchanged
  further  d went up (and it did not eat)
  none     no apple in reach before or after
split by side and by game phase.
"""
import collections, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import trace

PH = [(0, 10), (10, 20), (20, 30), (30, 50), (50, 200)]
tot = collections.defaultdict(lambda: collections.Counter())
eaten = collections.defaultdict(lambda: [0, 0])
games = 0
for log in sys.argv[1:]:
    try:
        res, bad, n = trace.run(log)
    except Exception as e:
        print("skip", log, e, file=sys.stderr); continue
    if bad: print("mismatch", log, bad, file=sys.stderr); continue
    games += 1
    pos = []
    for line in res:
        f = line.split("|")
        v = f[4].split()
        sn = [tuple(int(x) for x in v[i * 7:i * 7 + 7]) for i in range(n)]
        ate = set(int(e.split(":")[0]) for e in f[5].split())
        pos.append((sn, ate))
    for t in range(1, len(pos)):
        ph = next(p for p in PH if p[0] <= t - 1 < p[1])
        for i in range(n):
            a, b = pos[t - 1][0][i], pos[t][0][i]
            if not a[0] or not b[0]: continue
            side = i >= n // 2
            if i in pos[t][1]: k = "eat"; eaten[ph][side] += 1
            elif a[6] == 255 and b[6] == 255: k = "none"
            elif b[6] < a[6]: k = "closer"
            elif b[6] == a[6]: k = "same"
            else: k = "further"
            tot[(ph, side)][k] += 1
print(games, "games")
print(f"{'turns':>9} {'side':>5} {'eat':>6} {'closer':>7} {'same':>6} {'further':>8} {'none':>6}   apples/game")
for ph in PH:
    for side in (0, 1):
        c = tot[(ph, side)]; s = sum(c.values()) or 1
        print(f"{ph[0]:>4}-{ph[1]:<4} {'us' if side == 0 else 'them':>5} " + " ".join(f"{100 * c[k] / s:>6.1f}%" for k in ("eat", "closer", "same", "further", "none")) + f"   {eaten[ph][side] / max(games, 1):.2f}")
