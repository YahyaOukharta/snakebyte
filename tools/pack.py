#!/usr/bin/env python3
"""Build the submission: bot.cpp with a value net embedded.

  pack.py -n tools/nets/n0b.bin -o out/sub.cpp [-D NAME=VALUE ...]
The net (raw float32: W1 b1 W2 b2 W3 b3) is quantised to 16 bits per weight with one scale
per array and written as text, three characters per weight. If the result would pass 96,000
characters, the comments at the end of code lines are removed, then the comment lines
(CodinGame's limit is 100,000); before that, or always with --release, the blocks of the local
tools (#ifdef LOCAL, DUMP, PROF, SLOWFALL) are dropped, so such a file takes no SB_ parameters.
The commented source is bot.cpp in the repository. The output also compiles with
-DLOCAL, so the embedded net can be gated on the local referee.
"""
import argparse, numpy as np, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser()
ap.add_argument("-n", "--net", required=True)
ap.add_argument("-o", "--out", required=True)
ap.add_argument("-D", action="append", default=[], help="NAME=VALUE: replace the default of a PARAM")
ap.add_argument("src", nargs="?", default=os.path.join(ROOT, "bot.cpp"))
ap.add_argument("--release", action="store_true", help="drop the blocks of the local tools (LOCAL, DUMP, PROF, SLOWFALL)")
a = ap.parse_args()
src = open(a.src).read()
m = re.search(r"enum \{ NF_G = (\d+), NF_S = (\d+), NF = NF_G \+ 8 \* NF_S, NH1 = (\d+), NH2 = (\d+) \}", src)
nf, h1, h2 = int(m[1]) + 8 * int(m[2]), int(m[3]), int(m[4])
w = np.fromfile(a.net, dtype=np.float32)
cnt = [h1 * nf, h1, h2 * h1, h2, h2, 1]
assert len(w) == sum(cnt), f"net has {len(w)} weights, the source expects {sum(cnt)}"
text, scales, off, worst = [], [], 0, 0.0
for c in cnt:
    x = w[off:off + c]; off += c
    sc = max(float(np.abs(x).max()), 1e-9) / 32767.0
    q = np.clip(np.round(x / sc), -32767, 32767).astype(np.int64)
    worst = max(worst, float(np.abs(q * sc - x).max()))
    scales.append(sc)
    for v in q + 32768:
        text.append(chr(48 + (v >> 12)) + chr(48 + (v >> 6 & 63)) + chr(48 + (v & 63)))
data = "".join(text).replace("\\", "{").replace("?", "|")       # no escapes, no trigraphs
for d in a.D:
    name, val = d.split("=")
    src, k = re.subn(r"PARAM\(%s, [^)]*\)" % name, f"PARAM({name}, {val})", src)
    assert k == 1, name
head = f'#define NETW_DATA "{data}"\n#define NETW_SCALES {", ".join("%.9gf" % s for s in scales)}\n'
if "--release" in sys.argv or len(head) + len(src) > 96000:     # the code of the local tools is not needed on CodinGame
    keep, stack = [], []
    for l in src.split("\n"):
        t = l.strip()
        m = re.match(r"#\s*ifdef\s+(\w+)", t)
        if t.startswith("#if"):
            stack.append("off" if m and m.group(1) in ("LOCAL", "DUMP", "DUMPQ", "PROF", "SLOWFALL", "GDCHECK") else "plain")
            if stack[-1] == "off": continue
        elif t.startswith("#else") and stack and stack[-1] in ("off", "on"):
            stack[-1] = "on" if stack[-1] == "off" else "off"
            continue
        elif t.startswith("#endif") and stack:
            if stack.pop() in ("off", "on"): continue
        if "off" in stack: continue
        keep.append(l)
    src = "\n".join(keep)
if len(head) + len(src) > 96000:                              # CodinGame takes 100,000 characters: drop the comments at
    lines = []                                                # the end of code lines, keep the ones on their own lines
    for l in src.split("\n"):
        m = None if l.lstrip().startswith("//") else re.search(r'\s+//(?=(?:[^"]*"[^"]*")*[^"]*$)', l)
        lines.append(l[:m.start()] if m else l)
    src = "\n".join(lines)
if len(head) + len(src) > 96000:                              # still too long: the comment lines go too (the header stays)
    lines = src.split("\n")
    src = "\n".join(l for k, l in enumerate(lines) if k < 5 or not l.lstrip().startswith("//"))
out = head + src
open(a.out, "w").write(out)
print(f"{a.out}: {len(out)} characters ({len(data)} of weights), max quantisation error {worst:.2e}" + ("  OVER 100000" if len(out) > 100000 else ""))
