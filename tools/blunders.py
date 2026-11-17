#!/usr/bin/env python3
"""Blunder test set from real games against the top players, and a score for a build on it.

  blunders.py build [OPPONENT ...]     (default: sZoom _H3R0_) -> arena/blunders.jsonl
  blunders.py run BOT[@PARAMS] [--iters 6000] [--seeds 4] [--net FILE]
A case is a snake of ours that, in a harvested arena game, runs into a wall (it had no legal
move) or loses its whole body, with the position K = 1, 2, 3 turns before the first hit and
the move it played there. `run` plays each position with the given build and reports how
often the snake repeats the move of the game. Not every case had a way out; the number is
for comparing builds on the same set, lower is better.
"""
import collections, json, os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(ROOT, "arena", "traces")
SET = os.path.join(ROOT, "arena", "blunders.jsonl")
REF = os.path.join(ROOT, "build", "referee")
DN = ["UP", "RIGHT", "DOWN", "LEFT"]


def moves_of(game):
    l = open(os.path.join(T, f"{game}.trace")).read().split("\n")
    W, H = map(int, l[0].split()); k = 1 + H
    na = int(l[k]); k += 1 + na
    n = int(l[k]); k += 1 + n
    t = int(l[k])
    return [list(map(int, x.split())) for x in l[k + 1:k + 1 + t]]


def build(opps):
    rows = [json.loads(x) for x in open(os.path.join(T, "index.jsonl"))]
    out = []
    for r in rows:
        if r["mismatch"] or r["opponent"] not in opps: continue
        res = subprocess.check_output([REF, "--trace", os.path.join(T, f"{r['game_id']}.trace")]).decode().split("\n")
        n = r["snakes"]; mv = moves_of(r["game_id"]); seen = {}
        for line in res:
            f = line.split("|")
            if len(f) < 6: continue
            turn = int(f[0]) - 1                      # the events on line t happened in the move from t - 1
            for e in f[5].split():
                if not e.startswith("!"): continue
                i, cause, other, ln = e[1:].split(":"); i, ln = int(i), int(ln)
                if i >= n // 2: continue              # ours only
                died = ln <= 3 or cause == "F"
                if cause != "W" and not died: continue
                if i in seen and turn - seen[i] <= 3: seen[i] = turn; continue   # same incident
                seen[i] = turn
                for k in (1, 2, 3):
                    t = turn - k
                    if t < 0 or mv[t][i] < 0: continue
                    out.append({"game": r["game_id"], "opp": r["opponent"], "turn": t, "k": k, "snake": i, "move": mv[t][i], "cause": cause, "len": ln, "died": died})
    with open(SET, "w") as f:
        for o in out: f.write(json.dumps(o) + "\n")
    c = collections.Counter((o["cause"], o["k"]) for o in out)
    print(len(out), "positions from", len({(o["game"], o["snake"], o["turn"] + o["k"]) for o in out}), "incidents;", dict(c))


def run(bot, iters, seeds, net):
    path, _, params = bot.partition("@")
    env = {**os.environ, "SB_ITERS": str(iters)}
    for p in filter(None, params.split(",")):
        a, b = p.split("="); env[a] = b
    if net: env["SB_NET"] = net
    cases = [json.loads(x) for x in open(SET)]
    tot = collections.Counter(); rep = collections.Counter()
    for c in cases:
        pos = subprocess.check_output([REF, "--trace", os.path.join(T, f"{c['game']}.trace"), str(c["turn"])])
        for sd in range(seeds):
            o = subprocess.run([path], input=pos, capture_output=True, env={**env, "SB_SEED": str(sd + 1)}).stdout.decode().split("\n")[0]
            mine = {int(x.split()[0]): x.split()[1] for x in o.split(";") if len(x.split()) >= 2 and x.split()[0].isdigit()}
            same = mine.get(c["snake"]) == DN[c["move"]]
            for key in ("all", f"k={c['k']}", "wall" if c["cause"] == "W" else "death"):
                tot[key] += 1; rep[key] += same
    print(bot, " ".join(f"{k}: {100 * rep[k] / tot[k]:.0f}% ({tot[k] // seeds})" for k in ("all", "k=1", "k=2", "k=3", "wall", "death")))


if __name__ == "__main__":
    if sys.argv[1] == "build":
        build(set(sys.argv[2:]) or {"sZoom", "_H3R0_"})
    else:
        a = sys.argv[2:]
        it = int(a[a.index("--iters") + 1]) if "--iters" in a else 6000
        sd = int(a[a.index("--seeds") + 1]) if "--seeds" in a else 4
        net = a[a.index("--net") + 1] if "--net" in a else None
        run(a[0], it, sd, net)
