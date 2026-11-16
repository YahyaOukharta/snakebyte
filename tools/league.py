#!/usr/bin/env python3
"""Local league for SnakeByte bot versions.

Every version is frozen (source in league/src, binary in league/bots) and plays through
the native referee (build/referee). Results are appended to league/results.jsonl.

  league.py add NAME [SRC]       freeze SRC (default bot.cpp) as version NAME
  league.py match A B [-g MAPS]  head-to-head; A / B are league names or binaries
  league.py show                 all recorded matches
Options: -n ITERS (fixed iterations per move; 0 = real time, the default), -j PROCS, -s SEED.
Every map is played twice with the seats swapped, so a match is 2 x MAPS games.
"""
import argparse, json, os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOTS = os.path.join(ROOT, "league", "bots")
SRCS = os.path.join(ROOT, "league", "src")
RESULTS = os.path.join(ROOT, "league", "results.jsonl")
REFEREE = os.path.join(ROOT, "build", "referee")
CXX = [os.environ.get("CXX", "c++"), "-O3", "-std=c++17", "-march=native"]


def build_referee():
    srcs = [os.path.join(ROOT, "tools", "referee.cpp"), os.path.join(ROOT, "bot.cpp")]
    if not os.path.exists(REFEREE) or os.path.getmtime(REFEREE) < max(map(os.path.getmtime, srcs)):
        os.makedirs(os.path.dirname(REFEREE), exist_ok=True)
        subprocess.check_call(CXX + ["-o", REFEREE, srcs[0]])


def resolve(name):
    p, src = os.path.join(BOTS, name), os.path.join(SRCS, name + ".cpp")
    if not os.path.exists(p) and os.path.exists(src):      # fresh clone: binaries are not tracked
        os.makedirs(BOTS, exist_ok=True)
        subprocess.check_call(CXX + ["-DLOCAL", "-o", p, src])
    if os.path.exists(p):
        return p
    if os.path.exists(name):
        return os.path.abspath(name)
    sys.exit(f"unknown bot: {name}")


def cmd_add(a):
    os.makedirs(BOTS, exist_ok=True), os.makedirs(SRCS, exist_ok=True)
    dst = os.path.join(SRCS, a.name + ".cpp")
    if os.path.exists(dst):
        sys.exit(f"{a.name} already exists (versions are immutable)")
    src = a.src or os.path.join(ROOT, "bot.cpp")
    subprocess.check_call(CXX + ["-DLOCAL", "-o", os.path.join(BOTS, a.name), src])
    shutil.copy(src, dst)
    print("added", a.name)


def cmd_match(a):
    build_referee()
    cmd = [REFEREE, resolve(a.a), resolve(a.b), "-g", str(a.games), "-j", str(a.jobs), "-s", str(a.seed)]
    if a.iters:
        cmd += ["-n", str(a.iters)]
    line = subprocess.check_output(cmd, text=True).strip().splitlines()[-1]
    m = re.search(r"\+(\d+) -(\d+) =(\d+) \(([\d.]+)%\)\s+elo ([+-]\d+) \+/- (\d+)\s+first-seat wins (\d+)%\s+faults (\d+)/(\d+)", line)
    rec = dict(a=a.a, b=a.b, wins=int(m[1]), losses=int(m[2]), draws=int(m[3]), score=float(m[4]), elo=int(m[5]), err=int(m[6]),
               first_seat=int(m[7]), faults=[int(m[8]), int(m[9])], iters=a.iters, seed=a.seed, time=int(time.time()))
    with open(RESULTS, "a") as f:
        f.write(json.dumps(rec) + "\n")
    print(f"{a.a} vs {a.b}: +{rec['wins']} -{rec['losses']} ={rec['draws']} ({rec['score']}%)  elo {rec['elo']:+d} +/- {rec['err']}"
          f"  first-seat wins {rec['first_seat']}%  faults {rec['faults'][0]}/{rec['faults'][1]}  [{'%d iterations' % a.iters if a.iters else 'real time'}]")


def cmd_show(a):
    if not os.path.exists(RESULTS):
        return
    for l in open(RESULTS):
        r = json.loads(l)
        print(f"{r['a']:<14} vs {r['b']:<14} +{r['wins']:<4} -{r['losses']:<4} ={r['draws']:<3} {r['score']:5.1f}%  elo {r['elo']:+5d} +/- {r['err']:<4}"
              f" faults {r['faults'][0]}/{r['faults'][1]}  {'%d it' % r['iters'] if r['iters'] else 'real time'}")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("add"); p.add_argument("name"); p.add_argument("src", nargs="?")
    p = sub.add_parser("match"); p.add_argument("a"); p.add_argument("b")
    p.add_argument("-g", "--games", type=int, default=100); p.add_argument("-n", "--iters", type=int, default=0)
    p.add_argument("-j", "--jobs", type=int, default=4); p.add_argument("-s", "--seed", type=int, default=1)
    sub.add_parser("show")
    a = ap.parse_args()
    {"add": cmd_add, "match": cmd_match, "show": cmd_show}[a.cmd](a)


if __name__ == "__main__":
    main()
