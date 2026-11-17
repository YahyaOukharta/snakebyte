#!/usr/bin/env python3
"""Gauntlet: a source against real arena players through CodinGame test games.

  gauntlet.py SRC GAMES AGENT_ID [AGENT_ID ...]

Plays GAMES test games against each agent (ids from `cg.py opponents`), in turn, and appends
one line per game to league/gauntlet.jsonl. Test games are limited (10 per minute, 600 per
day) and a single game is close to a coin flip, so this is a coarse check on the opponents
that self-play cannot stand in for.
"""
import io, json, os, re, sys, time, contextlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cg

OUT = os.path.join(cg.ROOT, "league", "gauntlet.jsonl")


def one(src, opp):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        path = cg.test(src, str(opp))
    text = open(path).read()
    sc = re.findall(r"scores: 0=(-?\d+) 1=(-?\d+)", buf.getvalue())
    rk = re.findall(r"winner first\): ([0-9 ]+)", buf.getvalue())
    s = [int(x) for x in sc[0]] if sc else None
    res = None
    if s: res = 1 if s[0] > s[1] else 0 if s[0] < s[1] else 0.5
    if s and s[0] == s[1] and rk: res = 0.5 if len(set(rk[0].split())) < 2 else res
    return {"src": os.path.basename(src), "opp": opp, "scores": s, "ranking": rk[0].strip() if rk else None, "result": res,
            "log": os.path.basename(path), "desync": len(re.findall(r"desync [1-9]", text)) > 0}


if __name__ == "__main__":
    src, games, opps = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
    tally = {o: [0, 0, 0] for o in opps}
    for g in range(games):
        for o in opps:
            try:
                r = one(src, o)
            except SystemExit as e:
                print("failed:", str(e)[:200], flush=True)
                time.sleep(60)
                continue
            open(OUT, "a").write(json.dumps(r) + "\n")
            if r["result"] is not None:
                tally[o][0 if r["result"] == 1 else 1 if r["result"] == 0 else 2] += 1
            print(g, o, r["scores"], r["ranking"], " | ".join(f"{k}: +{v[0]} -{v[1]} ={v[2]}" for k, v in tally.items()), flush=True)
            time.sleep(7)
