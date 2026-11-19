#!/usr/bin/env python3
"""SPSA chains in real time on Kaggle CPU kernels (x86, like CodinGame).

  kaggle_spsa.py push NAME SRC [--chains 5] [--hours 9] [--maps 8] [--fixed SB_X=v,...] [--start SB_X=v,...]
  kaggle_spsa.py status NAME
  kaggle_spsa.py fetch NAME          every chain's last state and the mean over the chains
SRC is a packed source that still reads its parameters from the environment (pack.py --keep).
Every kernel runs its own chain from the same start with its own random signs and maps:
theta+ plays theta- on MAPS maps with both seats at the real time budget, two games at once.
The parameter table is the one of tools/spsa.py. SB_KP=2 selects the second account.
"""
import glob, json, os, re, shutil, subprocess, sys, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROF = os.environ.get("SB_KP", "1")
_SUF = "" if PROF == "1" else f"_{PROF}"
USER = os.environ.get("KAGGLE_USERNAME") or open(os.path.expanduser(f"~/.kaggle/user{_SUF}")).read().strip()
SFX = "" if PROF == "1" else f"_p{PROF}"
ENV = {**os.environ, "KAGGLE_API_TOKEN": open(os.path.expanduser(f"~/.kaggle/access_token{_SUF}")).read().strip(),
       "PATH": os.path.expanduser("~/.local/bin") + ":" + os.environ["PATH"]}
SCRIPT = '''import glob, json, os, random, re, shutil, subprocess, time
SRC, P, START, FIXED, MAPS, HOURS, CHAIN, LR = %r, %r, %r, %r, %d, %f, %d, %f
d = os.path.dirname(glob.glob("/kaggle/input/**/referee.cpp", recursive=True)[0])
os.makedirs("/tmp/src/tools", exist_ok=True)
shutil.copy(d + "/referee.cpp", "/tmp/src/tools/referee.cpp")
shutil.copy(d + "/refbot.cpp", "/tmp/src/bot.cpp")
cc = ["g++", "-O3", "-std=c++17", "-march=native"]
subprocess.check_call(cc + ["-o", "/tmp/referee", "/tmp/src/tools/referee.cpp"])
subprocess.check_call(cc + ["-DLOCAL", "-o", "/tmp/a", d + "/" + SRC])
theta = {n: START.get(n, v[0]) for n, v in P.items()}
rnd = random.Random(977 * CHAIN + 5)
t0, k = time.time(), 0
while time.time() - t0 < HOURS * 3600:
    s = {n: rnd.choice((-1, 1)) for n in P}
    def spec(sign):
        v = {n: min(max(theta[n] + sign * s[n] * P[n][1], P[n][2]), P[n][3]) for n in P}
        return "/tmp/a@" + ",".join("SB_%%s=%%.6g" %% (n, v[n]) for n in P) + ("," + FIXED if FIXED else "")
    out = subprocess.check_output(["/tmp/referee", spec(1), spec(-1), "-g", str(MAPS), "-j", "2",
                                   "-s", str(1000000 * CHAIN + 1000 * k)], text=True).strip().splitlines()[-1]
    score = float(re.search(r"\\(([\\d.]+)%%\\)", out).group(1)) / 100
    lr = LR / (1 + k / 150)
    for n in P:
        theta[n] = min(max(theta[n] + lr * 2 * (score - 0.5) * s[n] * P[n][1], P[n][2]), P[n][3])
    print("SPSA", json.dumps({"batch": k, "score": score, "theta": {n: round(theta[n], 6) for n in P}}), flush=True)
    k += 1
'''


def kg(*a):
    return subprocess.run(["kaggle", *a], env=ENV, capture_output=True, text=True)


def table():
    src = open(os.path.join(ROOT, "tools", "spsa.py")).read()
    return eval(re.search(r"^P = (\{.*?^\})", src, re.S | re.M).group(1))


def kv(s):
    return {a.split("=")[0].replace("SB_", ""): float(a.split("=")[1]) for a in s.split(",") if a}


def push(name, src, chains, hours, maps, fixed, start, lr):
    d = os.path.join(ROOT, "kaggle", "matchsrc" + SFX)
    os.makedirs(d, exist_ok=True)
    new = not os.path.exists(os.path.join(d, "dataset-metadata.json"))
    json.dump({"title": "sb-match", "id": f"{USER}/sb-match", "licenses": [{"name": "CC0-1.0"}]},
              open(os.path.join(d, "dataset-metadata.json"), "w"))
    shutil.copy(os.path.join(ROOT, "tools", "referee.cpp"), d)
    shutil.copy(os.path.join(ROOT, "bot.cpp"), os.path.join(d, "refbot.cpp"))
    dst = name + "_" + os.path.basename(src)
    shutil.copy(src, os.path.join(d, dst))
    r = kg("datasets", "create", "-p", d) if new else kg("datasets", "version", "-p", d, "-m", name)
    print((r.stdout + r.stderr).strip().splitlines()[-1][:120])
    for _ in range(40):
        time.sleep(6)
        if "ready" in (kg("datasets", "status", f"{USER}/sb-match").stdout):
            break
    time.sleep(10)
    for i in range(1, chains + 1):
        k = os.path.join(ROOT, "kaggle", f"s{SFX}_{name}_{i}")
        os.makedirs(k, exist_ok=True)
        chain = i + (100 if SFX else 0) + sum(map(ord, name))
        open(os.path.join(k, "spsa.py"), "w").write(SCRIPT % (dst, table(), kv(start), fixed, maps, hours, chain, lr))
        json.dump({"id": f"{USER}/sb-spsa-{name}-{i}", "title": f"sb-spsa-{name}-{i}", "code_file": "spsa.py",
                   "language": "python", "kernel_type": "script", "is_private": True, "enable_gpu": False,
                   "enable_internet": False, "dataset_sources": [f"{USER}/sb-match"]}, open(os.path.join(k, "kernel-metadata.json"), "w"))
        r = kg("kernels", "push", "-p", k)
        print(i, (r.stdout + r.stderr).strip().splitlines()[-1][:110], flush=True)


def kernels(name):
    return sorted(glob.glob(os.path.join(ROOT, "kaggle", f"s{SFX}_{name}_*")), key=lambda k: int(k.rsplit("_", 1)[1]))


def status(name):
    st = []
    for k in kernels(name):
        r = kg("kernels", "status", f"{USER}/sb-spsa-{name}-{k.rsplit('_', 1)[1]}")
        m = re.search(r"KernelWorkerStatus\.(\w+)", r.stdout + r.stderr)
        st.append(m.group(1) if m else "?")
    print(" ".join(st))


def fetch(name):
    ends = []
    for k in kernels(name):
        i = k.rsplit("_", 1)[1]
        d = os.path.join(ROOT, "kaggle", "out", f"s{SFX}_{name}_{i}")
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        kg("kernels", "output", f"{USER}/sb-spsa-{name}-{i}", "-p", d)
        rows = []
        for f in glob.glob(os.path.join(d, "*.log")):
            txt = open(f).read().replace('\\"', '"')
            rows += [json.loads(m) for m in re.findall(r'SPSA (\{.*?\}\})', txt)]
        if not rows:
            print(i, "nothing yet"); continue
        with open(os.path.join(ROOT, "league", f"kspsa_{name}{SFX}_{i}.jsonl"), "w") as f:
            for r in rows: f.write(json.dumps(r) + "\n")
        tail = rows[-max(1, len(rows) // 5):]                  # the last fifth of the chain, averaged
        ends.append({n: sum(r["theta"][n] for r in tail) / len(tail) for n in rows[-1]["theta"]})
        print(i, len(rows), "batches, mean score", round(sum(r["score"] for r in rows) / len(rows), 3))
    if ends:
        mean = {n: sum(e[n] for e in ends) / len(ends) for n in ends[0]}
        print(",".join(f"SB_{n}={v:.4g}" for n, v in mean.items()))


if __name__ == "__main__":
    c, name, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    if c == "push":
        opt = {"--chains": "5", "--hours": "9", "--maps": "8", "--fixed": "", "--start": "", "--lr": "1.5"}
        for o in list(opt):
            if o in rest:
                j = rest.index(o); opt[o] = rest[j + 1]; del rest[j:j + 2]
        push(name, rest[0], int(opt["--chains"]), float(opt["--hours"]), int(opt["--maps"]), opt["--fixed"], opt["--start"], float(opt["--lr"]))
    elif c == "status": status(name)
    elif c == "fetch": fetch(name)
