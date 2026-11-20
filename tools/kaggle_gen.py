#!/usr/bin/env python3
"""Self-play records on Kaggle CPU kernels (4 vCPUs each: four generator processes).

  kaggle_gen.py push TAG KERNELS GAMES SEED ITERS [NET] [EPS]
  kaggle_gen.py status TAG
  kaggle_gen.py fetch TAG                  -> data/TAG_<kernel>_<process>.bin
GAMES is per process. Kernel i, process j uses seed SEED + 100000 * (4 * i + j): give every
generation its own SEED block. NET (a file in tools/nets/) makes the generator play with that
net. SB_GENENV=SB_X=v,... sets engine parameters of the generator. SB_KP=2 selects the second
account.
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
GAMES, ITERS, SEED, NET, EPS, EXTRA = %d, %d, %d, %r, %r, %r
d = os.path.dirname(glob.glob("/kaggle/input/**/referee.cpp", recursive=True)[0])
os.makedirs("/tmp/src/tools", exist_ok=True)
shutil.copy(d + "/referee.cpp", "/tmp/src/tools/referee.cpp")
shutil.copy(d + "/bot.cpp", "/tmp/src/bot.cpp")
subprocess.check_call(["g++", "-O3", "-std=c++17", "-march=native", "-o", "/tmp/referee", "/tmp/src/tools/referee.cpp"])
env = dict(os.environ)
env.update(dict(kv.split("=") for kv in EXTRA.split(",") if kv))
if NET: env["SB_NET"] = d + "/" + NET
ps = [subprocess.Popen(["/tmp/referee", "--gen", str(GAMES), str(ITERS), str(SEED + 100000 * j), "/kaggle/working/g_%%d.bin" %% j, EPS], env=env) for j in range(4)]
print("DONE", [p.wait() for p in ps], [os.path.getsize("/kaggle/working/g_%%d.bin" %% j) for j in range(4)], flush=True)
'''


def kg(*a):
    return subprocess.run(["kaggle", *a], env=ENV, capture_output=True, text=True)


def push(tag, kernels, games, seed, iters, net, eps):
    d = os.path.join(ROOT, "kaggle", "gensrc" + SFX)
    os.makedirs(d, exist_ok=True)
    new = not os.path.exists(os.path.join(d, "dataset-metadata.json"))
    json.dump({"title": "sb-gen", "id": f"{USER}/sb-gen", "licenses": [{"name": "CC0-1.0"}]}, open(os.path.join(d, "dataset-metadata.json"), "w"))
    shutil.copy(os.path.join(ROOT, "tools", "referee.cpp"), d)
    shutil.copy(os.path.join(ROOT, "bot.cpp"), d)
    if net:
        shutil.copy(os.path.join(ROOT, "tools", "nets", net), d)
    r = kg("datasets", "create", "-p", d) if new else kg("datasets", "version", "-p", d, "-m", tag)
    print((r.stdout + r.stderr).strip().splitlines()[-1][:120])
    for _ in range(40):
        time.sleep(6)
        if "ready" in kg("datasets", "status", f"{USER}/sb-gen").stdout:
            break
    time.sleep(10)
    for i in range(kernels):
        k = os.path.join(ROOT, "kaggle", f"g{SFX}_{tag}_{i + 1}")
        os.makedirs(k, exist_ok=True)
        base = seed + 100000 * 4 * (i + (0 if PROF == "1" else 50))      # the second account gets its own range
        open(os.path.join(k, "gen.py"), "w").write(SCRIPT % (games, iters, base, net or "", str(eps), os.environ.get("SB_GENENV", "")))
        json.dump({"id": f"{USER}/sb-gen-{tag}-{i + 1}", "title": f"sb-gen-{tag}-{i + 1}", "code_file": "gen.py", "language": "python",
                   "kernel_type": "script", "is_private": True, "enable_gpu": False, "enable_internet": False,
                   "dataset_sources": [f"{USER}/sb-gen"]}, open(os.path.join(k, "kernel-metadata.json"), "w"))
        r = kg("kernels", "push", "-p", k)
        print(i + 1, (r.stdout + r.stderr).strip().splitlines()[-1][:110], flush=True)


def kernels(tag):
    return sorted(glob.glob(os.path.join(ROOT, "kaggle", f"g{SFX}_{tag}_*")), key=lambda k: int(k.rsplit("_", 1)[1]))


def status(tag):
    st = []
    for k in kernels(tag):
        r = kg("kernels", "status", f"{USER}/sb-gen-{tag}-{k.rsplit('_', 1)[1]}")
        m = re.search(r"KernelWorkerStatus\.(\w+)", r.stdout + r.stderr)
        st.append(m.group(1) if m else "?")
    print(" ".join(st))


def fetch(tag):
    os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
    for k in kernels(tag):
        i = k.rsplit("_", 1)[1]
        d = os.path.join(ROOT, "kaggle", "out", f"g{SFX}_{tag}_{i}")
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        kg("kernels", "output", f"{USER}/sb-gen-{tag}-{i}", "-p", d)
        got = 0
        for f in glob.glob(os.path.join(d, "g_*.bin")):
            j = re.search(r"g_(\d)\.bin", f).group(1)
            shutil.copy(f, os.path.join(ROOT, "data", f"{tag}{SFX}_{i}_{j}.bin"))
            got += os.path.getsize(f)
        print(i, f"{got / 1e6:.1f} MB")


if __name__ == "__main__":
    c, tag, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    if c == "push":
        push(tag, int(rest[0]), int(rest[1]), int(rest[2]), int(rest[3]), rest[4] if len(rest) > 4 and rest[4] != "-" else None, rest[5] if len(rest) > 5 else "0.05")
    elif c == "status": status(tag)
    elif c == "fetch": fetch(tag)
