#!/usr/bin/env python3
"""Fit move preferences: which legal move does a player pick, from the facts of each move.

  prefs.py DUMP [DUMP ...] [--side 1] [--hidden 16] [--emit]
DUMP: lines from `referee --agree FILE 1 ITERS` with SB_MDUMP=1, a game index in front:
game side turn snake played ourPick direction, then the NPF numbers of moveFeatures() in
bot.cpp. --side 1 fits the opponent's decisions (0: ours). The model is one hidden layer
(--hidden 0: linear), a softmax over the legal moves of a decision; whole games are held
out. --emit writes the weights into bot.cpp between the PREFNET markers.
"""
import os, re, sys, numpy as np, torch, torch.nn as nn
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
a = sys.argv[1:]
def opt(name, default):
    if name in a:
        i = a.index(name); v = a[i + 1]; del a[i:i + 2]; return v
    return default
side, hidden = int(opt("--side", 1)), int(opt("--hidden", 16))
emit = "--emit" in a
if emit: a.remove("--emit")
D = []
for k, f in enumerate(a):
    x = np.loadtxt(f, dtype=np.float32); x[:, 0] += 100000 * k; D.append(x[x[:, 1] == side])
D = np.concatenate(D)
X = torch.tensor(D[:, 7:]); y = torch.tensor(D[:, 4]); so = torch.tensor(D[:, 5])
key = D[:, 0].astype(np.int64) * 100000 + D[:, 2].astype(np.int64) * 10 + D[:, 3].astype(np.int64)
grp = np.zeros(len(D), dtype=np.int64)
grp[1:] = np.cumsum(key[1:] != key[:-1])
G = torch.tensor(grp); ng = int(grp[-1]) + 1
ok = torch.zeros(ng).index_add_(0, G, y) == 1
te = torch.tensor(D[:, 0].astype(np.int64) % 5 == 0); tr = ~te
torch.manual_seed(1)
net = nn.Linear(X.shape[1], 1) if hidden == 0 else nn.Sequential(nn.Linear(X.shape[1], hidden), nn.ReLU(), nn.Linear(hidden, 1))
o = torch.optim.Adam(net.parameters(), lr=0.01, weight_decay=1e-5)
def lsm(s):
    m = torch.full((ng,), -1e9).scatter_reduce(0, G, s, "amax")
    return s - (torch.log(torch.zeros(ng).index_add_(0, G, torch.exp(s - m[G]))) + m)[G]
fit_mask = (tr | torch.tensor(emit)) & ok[G]            # with --emit the held-out games are used too (after the split was read)
for it in range(800):
    s = net(X).squeeze(1)
    loss = -(y * lsm(s))[tr & ok[G]].sum() / y[tr & ok[G]].sum()
    o.zero_grad(); loss.backward(); o.step()
s = net(X).squeeze(1).detach()
best = torch.full((ng,), -1e9).scatter_reduce(0, G, s, "amax")
pick = (s >= best[G] - 1e-9).float()
def acc(v, mask):
    mm = mask & ok[G]; return 100 * float((y * v)[mm].sum() / y[mm].sum()), int(y[mm].sum())
print(f"side {side}, hidden {hidden}: {acc(pick, te)[1]} held-out decisions: model {acc(pick, te)[0]:.1f}%  (fitted games {acc(pick, tr)[0]:.1f}%)  our search {acc(so, te)[0]:.1f}%")
if emit and hidden:
    sd = net.state_dict()
    w1, b1, w2, b2 = sd["0.weight"].numpy(), sd["0.bias"].numpy(), sd["2.weight"].numpy()[0], float(sd["2.bias"][0])
    fmt = lambda v: ", ".join("%.4g" % x for x in v)
    code = "/*PREFNET*/\n// Preference net fitted by tools/prefs.py on the top players' decisions: NPF facts -> NPH -> score.\n"
    code += "static const float PFW1[NPH][NPF] = {\n" + ",\n".join("    {" + fmt(r) + "}" for r in w1) + "};\n"
    code += "static const float PFB1[NPH] = {" + fmt(b1) + "};\nstatic const float PFW2[NPH] = {" + fmt(w2) + "};\n"
    code += "static inline float prefNet(const float *f) {\n    float s = 0;\n    for (int j = 0; j < NPH; j++) {\n        float h = PFB1[j];\n        for (int k = 0; k < NPF; k++) h += PFW1[j][k] * f[k];\n        if (h > 0) s += PFW2[j] * h;\n    }\n    return s;\n}\n/*PREFNET_END*/\n"
    p = os.path.join(ROOT, "bot.cpp"); src = open(p).read()
    if "/*PREFNET_END*/" in src: src = re.sub(r"/\*PREFNET\*/.*?/\*PREFNET_END\*/\n", lambda m: code, src, flags=re.S)
    else: src = src.replace("/*PREFNET*/\n", code)
    open(p, "w").write(src)
    print("weights written to bot.cpp:", w1.size + b1.size + w2.size, "numbers,", len(code), "characters")
