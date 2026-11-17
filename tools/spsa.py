#!/usr/bin/env python3
"""SPSA tuning of the PARAMs of bot.cpp by self-play on the local referee.

  spsa.py [--batches 200] [--maps 16] [--iters 1000] [--jobs 7] [--net tools/nets/n0b.bin] [--log league/spsa.jsonl]
Every batch perturbs all parameters at once by +/- their step, plays theta+ against
theta- on MAPS maps with both seats, and moves every parameter towards the side that
scored better. The state after every batch is appended to the log; a run continues from
the last line of an existing log.
"""
import argparse, json, os, random, re, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# name: (start, step, min, max)
P = {
    "UCB_C": (0.7, 0.1, 0.1, 3), "EV_K": (6, 0.8, 1, 30), "EV_OWN": (0.25, 0.05, 0, 2), "EV_S1": (0.08, 0.015, 0, 0.5),
    "EV_N1": (8, 1.5, 1, 30), "EV_S2": (0.015, 0.005, 0, 0.2), "EV_N2": (20, 4, 1, 60), "EV_ELEV": (0.01, 0.01, -0.2, 0.2),
    "EV_SELF": (1.0, 0.2, 0, 5), "EV_MEAN": (0.5, 0.12, 0, 1), "EV_LOSS": (0.05, 0.05, -1, 1), "EV_WIN": (4, 1, 0, 20),
    "NET_A": (2.0, 0.4, 0, 10), "EV_ALIVE": (0.0, 0.3, -3, 3), "EV_UG": (0.0, 0.003, 0, 0.02),
}
ap = argparse.ArgumentParser()
ap.add_argument("--batches", type=int, default=200)
ap.add_argument("--maps", type=int, default=16)
ap.add_argument("--iters", type=int, default=1000)
ap.add_argument("--jobs", type=int, default=7)
ap.add_argument("--net", default="")
ap.add_argument("--lr", type=float, default=1.5, help="step per batch, in units of the parameter's step, for a 100%% score")
ap.add_argument("--log", default=os.path.join(ROOT, "league", "spsa.jsonl"))
a = ap.parse_args()

theta = {k: v[0] for k, v in P.items()}
k0 = 0
if os.path.exists(a.log):
    for line in open(a.log):
        r = json.loads(line)
        theta, k0 = r["theta"], r["batch"] + 1
rnd = random.Random(1234 + k0)
bot = os.path.join(ROOT, "build", "bot")
for k in range(k0, k0 + a.batches):
    d = {n: rnd.choice((-1, 1)) for n in P}
    def spec(sign):
        v = {n: min(max(theta[n] + sign * d[n] * P[n][1], P[n][2]), P[n][3]) for n in P}
        s = ",".join(f"SB_{n}={v[n]:.6g}" for n in P)
        return bot + "@" + s + (",SB_NET=" + a.net if a.net else "")
    out = subprocess.check_output([os.path.join(ROOT, "build", "referee"), spec(1), spec(-1), "-g", str(a.maps), "-j", str(a.jobs),
                                   "-n", str(a.iters), "-s", str(100000 + 1000 * k)], text=True).strip().splitlines()[-1]
    score = float(re.search(r"\(([\d.]+)%\)", out).group(1)) / 100
    lr = a.lr / (1 + k / 150)
    for n in P:
        theta[n] = min(max(theta[n] + lr * 2 * (score - 0.5) * d[n] * P[n][1], P[n][2]), P[n][3])
    with open(a.log, "a") as f:
        f.write(json.dumps({"batch": k, "score": score, "theta": {n: round(theta[n], 6) for n in P}}) + "\n")
    print(k, f"{score:.3f}", " ".join(f"{n}={theta[n]:.4g}" for n in P), flush=True)
