#!/usr/bin/env python3
"""How often does our search pick the move a player really made?

  agree.py [--opp _H3R0_] [--every 3] [--iters 2000] [--net FILE] [--max GAMES] [PARAM=VALUE ...]
For every EVERY-th turn of the harvested games against OPP, our search (fixed iterations) is
run on the position and its move for each snake with a real choice is compared with the
move of the game: for his snakes ("opp") and for ours ("us"). PARAM=VALUE sets SB_PARAM for
the run, so the same positions score any parameter set in a few seconds: the weights under
which our search plays most like the stronger player are a tuning target that needs no
match at all.
"""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def score(params, opp="_H3R0_", every=3, iters=2000, net=None, mx=10**9, subs=None):
    rows = [json.loads(l) for l in open(os.path.join(ROOT, "arena", "traces", "index.jsonl"))]
    games = [r["game_id"] for r in rows if r["opponent"] in opp.split(",") and not r["mismatch"] and (not subs or r["submission"] in subs)][-mx:]
    env = {**os.environ}
    if net: env["SB_NET"] = net
    for k, v in params.items(): env["SB_" + k] = str(v)
    def one(g):
        o = subprocess.run([os.path.join(ROOT, "build", "referee"), "--agree", os.path.join(ROOT, "arena", "traces", f"{g}.trace"), str(every), str(iters)],
                           capture_output=True, env=env).stdout.decode()
        m = re.search(r"AGREE us (\d+) (\d+) opp (\d+) (\d+)", o)
        return tuple(map(int, m.groups())) if m else (0, 0, 0, 0)
    with ThreadPoolExecutor(7) as ex: res = list(ex.map(one, games))
    a = [sum(r[i] for r in res) for i in range(4)]
    return a[2] / max(1, a[3]), a[0] / max(1, a[1]), a[3], len(games)


if __name__ == "__main__":
    a = sys.argv[1:]
    def opt(name, default):
        if name in a:
            i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
        return default
    opp, every, iters, net, mx = opt("--opp", "_H3R0_"), int(opt("--every", 3)), int(opt("--iters", 2000)), opt("--net", None), int(opt("--max", 10**9))
    params = dict(x.split("=") for x in a)
    o, u, n, g = score(params, opp, every, iters, net, mx)
    print(f"{opp} {g} games, {n} decisions: his moves {100 * o:.2f}%   ours {100 * u:.2f}%   {params}")
