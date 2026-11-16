#!/usr/bin/env python3
"""Play gate matches on Kaggle CPU kernels (x86, like CodinGame).

  kaggle_match.py push NAME [--games MAPS] [--iters N] A B [A2 B2 ...]
  kaggle_match.py status NAME
  kaggle_match.py fetch NAME
A and B are bot specs: a source file, optionally followed by '@' and parameters, e.g.
  bot.cpp@SB_UCB_C=0.5,SB_EV_OWN=0.3  league/src/v1.0.cpp
Each pairing is one private kernel sb-match-NAME-i. --iters 0 (default) = real time, two
games at once on the kernel's 4 vCPUs; otherwise fixed iterations per move, four at once.
Every map is played with both seats, so a kernel plays 2 x MAPS games.
SB_KP=2 selects the second account (~/.kaggle/user_2, ~/.kaggle/access_token_2).
"""
import glob, json, os, re, shutil, subprocess, sys, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROF = os.environ.get("SB_KP", "1")
_SUF = "" if PROF == "1" else f"_{PROF}"
USER = os.environ.get("KAGGLE_USERNAME") or open(os.path.expanduser(f"~/.kaggle/user{_SUF}")).read().strip()
SFX = "" if PROF == "1" else f"_p{PROF}"
ENV = {**os.environ, "KAGGLE_API_TOKEN": open(os.path.expanduser(f"~/.kaggle/access_token{_SUF}")).read().strip(),
       "PATH": os.path.expanduser("~/.local/bin") + ":" + os.environ["PATH"]}
SCRIPT = '''import glob, os, shutil, subprocess
A, PA, B, PB, GAMES, ITERS, SEED = %r, %r, %r, %r, %d, %d, %d
d = os.path.dirname(glob.glob("/kaggle/input/**/referee.cpp", recursive=True)[0])
os.makedirs("/tmp/src/tools", exist_ok=True)
shutil.copy(d + "/referee.cpp", "/tmp/src/tools/referee.cpp")
shutil.copy(d + "/refbot.cpp", "/tmp/src/bot.cpp")
cc = ["g++", "-O3", "-std=c++17", "-march=native"]
subprocess.check_call(cc + ["-o", "/tmp/referee", "/tmp/src/tools/referee.cpp"])
subprocess.check_call(cc + ["-DLOCAL", "-o", "/tmp/a", d + "/" + A])
subprocess.check_call(cc + ["-DLOCAL", "-o", "/tmp/b", d + "/" + B])
mode = ["-j", "4", "-n", str(ITERS)] if ITERS else ["-j", "2"]
out = subprocess.check_output(["/tmp/referee", "/tmp/a" + ("@" + PA if PA else ""), "/tmp/b" + ("@" + PB if PB else ""),
                               "-g", str(GAMES), "-s", str(SEED)] + mode, text=True)
print("RESULT", A + "@" + PA, "|", B + "@" + PB, "|", out.strip().splitlines()[-1].split(": ", 1)[1], flush=True)
'''


def kg(*a):
    return subprocess.run(["kaggle", *a], env=ENV, capture_output=True, text=True)


def push(name, specs, games, iters):
    d = os.path.join(ROOT, "kaggle", "matchsrc" + SFX)
    os.makedirs(d, exist_ok=True)
    new = not os.path.exists(os.path.join(d, "dataset-metadata.json"))
    json.dump({"title": "sb-match", "id": f"{USER}/sb-match", "licenses": [{"name": "CC0-1.0"}]},
              open(os.path.join(d, "dataset-metadata.json"), "w"))
    shutil.copy(os.path.join(ROOT, "tools", "referee.cpp"), d)
    shutil.copy(os.path.join(ROOT, "bot.cpp"), os.path.join(d, "refbot.cpp"))
    bots = []
    for sp in specs:
        f, _, params = sp.partition("@")
        dst = name + "_" + os.path.basename(f)
        shutil.copy(f, os.path.join(d, dst))
        bots.append((dst, params))
    r = kg("datasets", "create", "-p", d) if new else kg("datasets", "version", "-p", d, "-m", name)
    print((r.stdout + r.stderr).strip().splitlines()[-1][:120])
    for _ in range(40):
        time.sleep(6)
        if "ready" in (kg("datasets", "status", f"{USER}/sb-match").stdout):
            break
    time.sleep(10)
    for i in range(0, len(bots), 2):
        k = os.path.join(ROOT, "kaggle", f"m{SFX}_{name}_{i // 2 + 1}")
        os.makedirs(k, exist_ok=True)
        seed = 777 + 1000 * (i // 2) + sum(map(ord, name + SFX))      # different maps per kernel and per batch
        open(os.path.join(k, "match.py"), "w").write(SCRIPT % (bots[i][0], bots[i][1], bots[i + 1][0], bots[i + 1][1], games, iters, seed))
        json.dump({"id": f"{USER}/sb-match-{name}-{i // 2 + 1}", "title": f"sb-match-{name}-{i // 2 + 1}", "code_file": "match.py",
                   "language": "python", "kernel_type": "script", "is_private": True, "enable_gpu": False,
                   "enable_internet": False, "dataset_sources": [f"{USER}/sb-match"]}, open(os.path.join(k, "kernel-metadata.json"), "w"))
        r = kg("kernels", "push", "-p", k)
        print(i // 2 + 1, (r.stdout + r.stderr).strip().splitlines()[-1][:110], flush=True)


def kernels(name):
    return sorted(glob.glob(os.path.join(ROOT, "kaggle", f"m{SFX}_{name}_*")), key=lambda k: int(k.rsplit("_", 1)[1]))


def status(name):
    st = []
    for k in kernels(name):
        r = kg("kernels", "status", f"{USER}/sb-match-{name}-{k.rsplit('_', 1)[1]}")
        m = re.search(r"KernelWorkerStatus\.(\w+)", r.stdout + r.stderr)
        st.append(m.group(1) if m else "?")
    print(" ".join(st))
    return st


def fetch(name):
    for k in kernels(name):
        i = k.rsplit("_", 1)[1]
        d = os.path.join(ROOT, "kaggle", "out", f"m{SFX}_{name}_{i}")
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        kg("kernels", "output", f"{USER}/sb-match-{name}-{i}", "-p", d)
        found = False
        for f in glob.glob(os.path.join(d, "*.log")):
            txt = open(f).read().replace('\\\\"', '"')
            m = re.search(r'RESULT (.*?\]\s*)', txt, re.S)
            if m:
                print(i, m.group(1).strip()); found = True
        if not found:
            print(i, "no result yet")


if __name__ == "__main__":
    c, name, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    if c == "push":
        games, iters, files = 150, 0, []
        i = 0
        while i < len(rest):
            if rest[i] == "--games": games = int(rest[i + 1]); i += 2
            elif rest[i] == "--iters": iters = int(rest[i + 1]); i += 2
            else: files.append(rest[i]); i += 1
        push(name, files, games, iters)
    elif c == "status": status(name)
    elif c == "fetch": fetch(name)
