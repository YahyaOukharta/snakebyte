#!/usr/bin/env python3
"""Paired gate against real players: two sources replay the same arena games.

  paired.py A.cpp B.cpp N [OPPONENT ...]      (default opponents: sZoom _H3R0_)
Takes the N most recent harvested battles against those opponents and replays each one
through a CodinGame test game with A and with B (same map, same seat, the opponent's
current bot). One line per game goes to league/paired.jsonl; the summary is the mean of
(A's margin - B's margin) per game, with its standard error, and both win counts.
One test game a minute, 600 a day: N = 50 is two hours.
"""
import io, json, math, os, re, sys, time, contextlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cg
OUT = os.path.join(cg.ROOT, "league", "paired.jsonl")


def play(src, game):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        path = cg.test(src, replay=game)
    text = open(path).read()
    me = int(re.search(r"\n  (\d) \S+ \(me", text).group(1))
    sc = re.search(r"scores: 0=(-?\d+) 1=(-?\d+)", buf.getvalue())
    if not sc: return None
    s = [int(sc.group(1)), int(sc.group(2))]
    return {"margin": s[me] - s[1 - me], "win": s[me] > s[1 - me], "timeout": f"${me} has not provided" in text, "log": os.path.basename(path)}


if __name__ == "__main__":
    A, B, N = sys.argv[1], sys.argv[2], int(sys.argv[3])
    opps = set(sys.argv[4:]) or {"sZoom", "_H3R0_"}
    rows = [json.loads(l) for l in open(os.path.join(cg.ROOT, "arena", "traces", "index.jsonl"))]
    games = [r for r in rows if r["opponent"] in opps and not r["mismatch"]][-N:]
    d = []; wa = wb = 0
    for k, r in enumerate(games):
        res = {}
        for name, src in ((("A", A), ("B", B)) if k % 2 == 0 else (("B", B), ("A", A))):
            for attempt in range(3):
                try:
                    res[name] = play(src, r["game_id"]); break
                except SystemExit as e:
                    print("retry:", str(e)[:120], flush=True); time.sleep(70)
            time.sleep(7)
        if not res.get("A") or not res.get("B"): continue
        open(OUT, "a").write(json.dumps({"game": r["game_id"], "opp": r["opponent"], "a": os.path.basename(A), "b": os.path.basename(B), "A": res["A"], "B": res["B"]}) + "\n")
        d.append(res["A"]["margin"] - res["B"]["margin"]); wa += res["A"]["win"]; wb += res["B"]["win"]
        m = sum(d) / len(d); se = (sum((x - m) ** 2 for x in d) / max(1, len(d) - 1)) ** 0.5 / math.sqrt(len(d))
        print(f"{len(d)} games: margin A-B {m:+.2f} +/- {se:.2f}   wins A {wa} B {wb}   ({r['opponent']}: A {res['A']['margin']:+d}, B {res['B']['margin']:+d})", flush=True)
