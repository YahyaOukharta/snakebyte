#!/usr/bin/env python3
"""Time builds of the bot on x86 (Kaggle CPU kernel), compiled the way CodinGame does it.

  kaggle_bench.py push NAME SRC [SRC ...]     sources with an embedded net (tools/pack.py) or plain bot.cpp
  kaggle_bench.py status NAME
  kaggle_bench.py fetch NAME
Each source is compiled with -O3 and the instruction sets named in the #pragma lines at the
top of bot.cpp (avx2, bmi, bmi2, popcnt, lzcnt, sse4.1), not -march=native: what CodinGame's
compiler is allowed to use. (The pragmas themselves do not compile with -DLOCAL on GCC 13.) Every build plays the positions in tools/bench/ with SB_BENCH (fixed iterations,
fixed seed), five times, interleaved; the best time counts. Builds that play the same
(equal "sims") are comparable by time. SB_KP=2 selects the second account.
"""
import glob, json, os, re, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kaggle_match as km
ROOT, USER, SFX, kg = km.ROOT, km.USER, km.SFX, km.kg
SCRIPT = '''import glob, os, subprocess
SRCS, ITERS, REPS = %r, %d, 5
d = os.path.dirname(glob.glob("/kaggle/input/**/pos11.txt", recursive=True)[0])
print(subprocess.check_output("lscpu | grep -E 'Model name|Flags' | cut -c1-400", shell=True, text=True))
print(subprocess.check_output(["g++", "--version"], text=True).splitlines()[0])
pos = sorted(glob.glob(d + "/pos*.txt")) + sorted(glob.glob(d + "/edge*.txt"))
for s in SRCS:
    subprocess.check_call(["g++", "-O3", "-std=c++17", "-mavx2", "-mbmi", "-mbmi2", "-mpopcnt", "-mlzcnt", "-msse4.1", "-DLOCAL", "-DNO_PRAGMA",
                           "-o", "/tmp/" + s + ".bin", d + "/" + s])
best = {}
for rep in range(REPS):
    for s in SRCS:
        for p in pos:
            out = subprocess.run(["/tmp/" + s + ".bin"], stdin=open(p), capture_output=True, text=True, env={**os.environ, "SB_BENCH": str(ITERS), "SB_SEED": "7"}).stdout
            l = [x for x in out.splitlines() if x.startswith("bench:")][-1].split()
            sims, ms = int(l[5]), float(l[7])
            k = (s, os.path.basename(p))
            if k not in best or ms < best[k][1]: best[k] = (sims, ms)
for p in pos:
    b = os.path.basename(p)
    for s in SRCS:
        sims, ms = best[(s, b)]
        print("BENCH %%-12s %%-28s sims %%9d  %%7.0f ms  %%6.0f ns/sim" %% (b, s, sims, ms, ms * 1e6 / sims), flush=True)
s = SRCS[-1]                                       # phase profile of the last source (rdtsc at each phase boundary)
subprocess.check_call(["g++", "-O3", "-std=c++17", "-mavx2", "-mbmi", "-mbmi2", "-mpopcnt", "-mlzcnt", "-msse4.1", "-DLOCAL", "-DNO_PRAGMA", "-DPROF",
                       "-o", "/tmp/prof.bin", d + "/" + s])
for p in pos:
    out = subprocess.run(["/tmp/prof.bin"], stdin=open(p), capture_output=True, text=True, env={**os.environ, "SB_BENCH": str(ITERS), "SB_SEED": "7"}).stdout
    print("PROF", os.path.basename(p), " ".join(" ".join(x.split()[:2]) for x in out.splitlines() if x.startswith("  ")), flush=True)
tot = {s: sum(best[(s, os.path.basename(p))][1] for p in pos) for s in SRCS}
for s in SRCS: print("TOTAL %%-28s %%8.0f ms  x%%.3f" %% (s, tot[s], tot[SRCS[0]] / tot[s]), flush=True)
'''


def push(name, srcs, iters=3000):
    d = os.path.join(ROOT, "kaggle", "benchsrc" + SFX)
    os.makedirs(d, exist_ok=True)
    new = not os.path.exists(os.path.join(d, "dataset-metadata.json"))
    json.dump({"title": "sb-bench", "id": f"{USER}/sb-bench", "licenses": [{"name": "CC0-1.0"}]}, open(os.path.join(d, "dataset-metadata.json"), "w"))
    for f in glob.glob(os.path.join(ROOT, "tools", "bench", "*.txt")): shutil.copy(f, d)
    names = []
    for f in srcs:
        dst = name + "_" + os.path.basename(f)
        shutil.copy(f, os.path.join(d, dst)); names.append(dst)
    r = kg("datasets", "create", "-p", d) if new else kg("datasets", "version", "-p", d, "-m", name)
    print((r.stdout + r.stderr).strip().splitlines()[-1][:120])
    for _ in range(40):
        time.sleep(6)
        if "ready" in kg("datasets", "status", f"{USER}/sb-bench").stdout: break
    time.sleep(10)
    k = os.path.join(ROOT, "kaggle", f"b{SFX}_{name}")
    os.makedirs(k, exist_ok=True)
    open(os.path.join(k, "bench.py"), "w").write(SCRIPT % (names, iters))
    json.dump({"id": f"{USER}/sb-bench-{name}", "title": f"sb-bench-{name}", "code_file": "bench.py", "language": "python", "kernel_type": "script",
               "is_private": True, "enable_gpu": False, "enable_internet": False, "dataset_sources": [f"{USER}/sb-bench"]},
              open(os.path.join(k, "kernel-metadata.json"), "w"))
    r = kg("kernels", "push", "-p", k)
    print((r.stdout + r.stderr).strip().splitlines()[-1][:110])


def status(name):
    r = kg("kernels", "status", f"{USER}/sb-bench-{name}")
    print(re.search(r"KernelWorkerStatus\.(\w+)", r.stdout + r.stderr).group(1))


def fetch(name):
    d = os.path.join(ROOT, "kaggle", "out", f"b{SFX}_{name}")
    shutil.rmtree(d, ignore_errors=True); os.makedirs(d)
    kg("kernels", "output", f"{USER}/sb-bench-{name}", "-p", d)
    for f in glob.glob(os.path.join(d, "*.log")):
        txt = open(f).read()
        try: txt = "".join(e.get("data", "") for e in json.loads(txt))
        except Exception: pass
        for l in txt.splitlines():
            if l.startswith(("BENCH", "TOTAL", "PROF", "Model name", "g++")): print(l[:150])


if __name__ == "__main__":
    c, name = sys.argv[1], sys.argv[2]
    if c == "push": push(name, sys.argv[3:])
    elif c == "status": status(name)
    else: fetch(name)
