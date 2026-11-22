#!/usr/bin/env python3
"""A submission as a compiled executable inside a Python script.

  binpack.py build SRC.cpp NAME [--pgo] [FLAGS...]   compile SRC on a Kaggle kernel (x86-64 Linux, g++) -> out/NAME.bin;
                                             --pgo: profile-guided, the profile from three real-time games against itself
  binpack.py stub out/NAME.bin OUT.py        OUT.py: the xz-compressed executable as 14-bit characters; it writes
                                             the file to /tmp and replaces itself with it
CodinGame counts characters (100,000), not bytes: a character of the CJK block carries 14 bits,
so 175 KB of compressed executable fit. Its machines: Haswell (AVX2, FMA, BMI2), glibc 2.36,
/tmp writable. Submit OUT.py with SB_LANG=Python3 (tools/cg.py).
"""
import base64, json, lzma, os, subprocess, sys, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
USER = open(os.path.expanduser("~/.kaggle/user")).read().strip()
ENV = {**os.environ, "KAGGLE_API_TOKEN": open(os.path.expanduser("~/.kaggle/access_token")).read().strip(),
       "PATH": os.path.expanduser("~/.local/bin") + ":" + os.environ["PATH"]}
DEF = ["-O3", "-std=c++17", "-march=haswell", "-mtune=haswell", "-static-libstdc++", "-static-libgcc", "-s"]


def build(src, name, flags):
    pgo = "--pgo" in flags
    flags = [f for f in flags if f != "--pgo"] or DEF
    k = os.path.join(ROOT, "kaggle", "build_" + name)
    os.makedirs(k, exist_ok=True)
    pre = b"#include <features.h>\n#undef __GLIBC_USE_C2X_STRTOL\n#define __GLIBC_USE_C2X_STRTOL 0\n"   # no __isoc23_scanf / strtol: they need glibc 2.38
    pre += (b'extern "C" unsigned long __isoc23_strtoul(const char *s, char **e, int b) { unsigned long v = 0; while (*s == 32) s++; if (!b) b = 10; '
            b'for (;; s++) { int c = *s | 32, d = *s >= 48 && *s <= 57 ? *s - 48 : c >= 97 && c <= 122 ? c - 87 : 99; if (d >= b) break; v = v * b + d; } '
            b'if (e) *e = (char *)s; return v; }\n')                # the static libstdc++ refers to it
    enc = lambda data: base64.b64encode(lzma.compress(data)).decode()
    rd = lambda f: open(os.path.join(ROOT, f), "rb").read()
    py = ("import base64, lzma, os, subprocess\n"
          "w = lambda f, c: (os.makedirs(os.path.dirname(f), exist_ok=True), open(f, 'wb').write(lzma.decompress(base64.b64decode(c))))\n"
          "sh = lambda *a: (lambda r: print(a[0], r.returncode, (r.stdout + r.stderr)[-1500:], flush=True))(subprocess.run(list(a), capture_output=True, text=True))\n"
          f"w('/tmp/b.cpp', '{enc(pre + open(src, 'rb').read())}')\n"
          f"F = {flags!r}\n"
          "sh('g++', '--version')\n")
    if pgo:                                   # profile: real-time games of the instrumented build against itself under the referee
        py += (f"w('/tmp/src/tools/referee.cpp', '{enc(rd('tools/referee.cpp'))}')\n"
               f"w('/tmp/src/bot.cpp', '{enc(rd('bot.cpp'))}')\n"
               "t = open('/tmp/src/tools/referee.cpp').read()\n"       # the bots must end by themselves to write their profile
               "assert 'kill(pid, SIGKILL)' in t\n"
               "open('/tmp/src/tools/referee.cpp', 'w').write(t.replace('kill(pid, SIGKILL)', '(close(in), usleep(400000), kill(pid, SIGKILL))'))\n"
               "sh('g++', '-O2', '-std=c++17', '-march=native', '-o', '/tmp/referee', '/tmp/src/tools/referee.cpp')\n"
               "sh('g++', *F, '-fprofile-generate', '-fprofile-dir=/tmp/prof', '-o', '/tmp/gen', '/tmp/b.cpp')\n"
               "sh('/tmp/referee', '/tmp/gen', '/tmp/gen', '-g', '3', '-s', '4242', '-j', '1')\n"
               "sh('sh', '-c', 'ls -la /tmp/prof | head')\n"
               "sh('g++', *F, '-fprofile-use', '-fprofile-dir=/tmp/prof', '-fprofile-correction', '-Wno-missing-profile', '-o', '/kaggle/working/bot.bin', '/tmp/b.cpp')\n")
    else:
        py += "sh('g++', *F, '-o', '/kaggle/working/bot.bin', '/tmp/b.cpp')\n"
    py += "sh('sh', '-c', 'objdump -T /kaggle/working/bot.bin | grep GLIBC_2.3[789]')\n"
    open(os.path.join(k, "b.py"), "w").write(py)
    kid = f"{USER}/sb-build-{name}"
    json.dump({"id": kid, "title": f"sb-build-{name}", "code_file": "b.py", "language": "python", "kernel_type": "script",
               "is_private": True, "enable_gpu": False, "enable_internet": False}, open(os.path.join(k, "kernel-metadata.json"), "w"))
    kg = lambda *a: subprocess.run(["kaggle", *a], env=ENV, capture_output=True, text=True)
    r = kg("kernels", "push", "-p", k); print((r.stdout + r.stderr).strip()[-150:])
    for _ in range(120):
        time.sleep(15)
        st = kg("kernels", "status", kid).stdout
        if "COMPLETE" in st or "ERROR" in st: break
    print(st.strip())
    for f in os.listdir(k):
        if f.endswith(".log") or f.endswith(".bin"): os.remove(os.path.join(k, f))
    kg("kernels", "output", kid, "-p", k)
    for f in os.listdir(k):
        if f.endswith(".log"):
            print("".join(r["data"] for r in json.load(open(os.path.join(k, f))) if r["stream_name"] == "stdout")[-2500:])
    os.makedirs(os.path.join(ROOT, "out"), exist_ok=True)
    os.replace(os.path.join(k, "bot.bin"), os.path.join(ROOT, "out", name + ".bin"))
    print("out/%s.bin" % name, os.path.getsize(os.path.join(ROOT, "out", name + ".bin")), "bytes")


def stub(binf, out):
    z = lzma.compress(open(binf, "rb").read(), preset=9 | lzma.PRESET_EXTREME)
    zp = z + bytes(-len(z) % 7)                               # 7 bytes = 56 bits = 4 characters of 14 bits
    s = "".join(chr(0x4E00 + (int.from_bytes(zp[i:i + 7], "big") >> sh & 0x3FFF)) for i in range(0, len(zp), 7) for sh in (42, 28, 14, 0))
    py = ("import os,lzma\n"
          f"s='{s}'\n"
          "o=ord;b=bytearray()\n"
          "for i in range(0,len(s),4):b+=((o(s[i])-19968<<42)|(o(s[i+1])-19968<<28)|(o(s[i+2])-19968<<14)|(o(s[i+3])-19968)).to_bytes(7,'big')\n"
          "p='/tmp/b%d'%os.getpid()\n"
          f"open(p,'wb').write(lzma.decompress(bytes(b[:{len(z)}])))\n"
          "os.chmod(p,493)\n"
          "os.execv(p,[p])\n")
    open(out, "w", encoding="utf-8").write(py)
    print(out, len(py), "characters;", len(z), "bytes compressed", "OVER 100000" if len(py) > 100000 else "")


if __name__ == "__main__":
    a = sys.argv[1:]
    build(a[1], a[2], a[3:]) if a[0] == "build" else stub(a[1], a[2])
