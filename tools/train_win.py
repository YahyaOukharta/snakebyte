#!/usr/bin/env python3
"""Experiment: does a view of the raw board add to the 83 hand-made inputs?

  train_win.py data/w_*.f32 [--epochs 10] [--dim 24] [--radius 4] [--no-win]
Each .f32 (referee --export with SB_WIN=1) has a .f32.win beside it: for every record, 8
snake slots x 81 bytes, the 9x9 window around that snake's head (0 empty, 1 wall, 2 apple,
3 itself, 4/5 team-mate body/head, 6/7 enemy body/head).

Model: every (cell of the window, type) has a vector of --dim numbers; a snake is the sum of
the vectors of what is in its window; my snakes' sums are added up, the opponent's too
(two sets of vectors); both sums go into the first layer next to the 83 inputs.
--radius r uses only the (2r+1)^2 centre of the window. --no-win trains the plain net on the
same records, for the comparison.
"""
import argparse, glob, numpy as np, torch, torch.nn as nn, time

NF, WN, WSIDE = 83, 81, 9
ap = argparse.ArgumentParser()
ap.add_argument("files", nargs="+")
ap.add_argument("--epochs", type=int, default=10)
ap.add_argument("--dim", type=int, default=24)
ap.add_argument("--radius", type=int, default=4)
ap.add_argument("--no-win", action="store_true")
ap.add_argument("--lr", type=float, default=2e-3)
ap.add_argument("--bs", type=int, default=4096)
ap.add_argument("--mw", type=float, default=0.5)
ap.add_argument("--margin", type=float, default=0.4)
a = ap.parse_args()

files = sorted(sum((glob.glob(f) for f in a.files), []))
X, Wn = [], []
for f in files:
    x = np.fromfile(f, dtype=np.float32)
    n = len(x) // (NF + 3)
    w = np.fromfile(f + ".win", dtype=np.uint8)
    n = min(n, len(w) // (8 * WN))
    X.append(x[:n * (NF + 3)].reshape(n, NF + 3)); Wn.append(w[:n * 8 * WN].reshape(n, 8, WN))
X = np.concatenate(X); Wn = np.concatenate(Wn)
rng = np.random.default_rng(1)
perm = rng.permutation(len(X))
X, Wn = X[perm], Wn[perm]
dev = "mps" if torch.backends.mps.is_available() else "cpu"
feat = torch.from_numpy(X[:, :NF]).to(dev)
res, diff = torch.from_numpy(X[:, NF]).to(dev), torch.from_numpy(X[:, NF + 1]).to(dev)
soft = torch.sigmoid(a.margin * 16 * diff)
Y = (1 - a.mw) * res + a.mw * soft
win = torch.from_numpy(Wn).to(dev)                                   # uint8 [N, 8, 81]
keep = torch.zeros(WSIDE, WSIDE, dtype=torch.bool)
c = WSIDE // 2
keep[c - a.radius:c + a.radius + 1, c - a.radius:c + a.radius + 1] = True
keep = keep.flatten().to(dev)
nval = min(100000, len(X) // 10)
print(f"{len(X)} records from {len(files)} files, {nval} held out, device {dev}; window {'off' if a.no_win else f'radius {a.radius}, dim {a.dim}'}")


class Net(nn.Module):
    def __init__(self):
        super().__init__()
        self.use = not a.no_win
        if self.use:
            self.emb = nn.Embedding(2 * 8 * WN, a.dim)                # [mine / theirs][type][cell]
            nn.init.normal_(self.emb.weight, std=0.05)
            self.register_buffer("cell", torch.arange(WN).view(1, 1, WN))
        self.body = nn.Sequential(nn.Linear(NF + (2 * a.dim if self.use else 0), 96), nn.ReLU(), nn.Linear(96, 32), nn.ReLU(), nn.Linear(32, 1))

    def forward(self, f, w):
        if not self.use: return self.body(f).squeeze(1)
        t = w.long()                                                  # [B, 8, 81]
        side = torch.cat([torch.zeros(4, dtype=torch.long), torch.ones(4, dtype=torch.long)]).view(1, 8, 1).to(t.device)
        idx = (side * 8 + t) * WN + self.cell
        e = self.emb(idx) * ((t > 0) & keep.view(1, 1, WN)).unsqueeze(-1)   # empty cells and cells outside the radius add nothing
        s = e.sum(2)                                                  # [B, 8, dim]
        mine, theirs = s[:, :4].sum(1), s[:, 4:].sum(1)
        return self.body(torch.cat([f, mine, theirs], 1)).squeeze(1)


net = Net().to(dev)
opt = torch.optim.Adam(net.parameters(), lr=a.lr)
sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs)
bce = nn.BCEWithLogitsLoss()
n = len(X) - nval
for ep in range(a.epochs):
    t0 = time.time()
    p = torch.randperm(n, device=dev) + nval
    tot = 0.0
    net.train()
    for i in range(0, n, a.bs):
        idx = p[i:i + a.bs]
        loss = bce(net(feat[idx], win[idx]), Y[idx])
        opt.zero_grad(); loss.backward(); opt.step()
        tot += loss.item() * len(idx)
    sched.step()
    net.eval()
    with torch.no_grad():
        out = torch.cat([net(feat[i:i + 8192], win[i:i + 8192]) for i in range(0, nval, 8192)])
        vl = bce(out, Y[:nval]).item()
        r = res[:nval]; dec = r != 0.5
        acc = ((out[dec] > 0) == (r[dec] > 0.5)).float().mean().item()
    print(f"epoch {ep + 1:2d}  train {tot / n:.4f}  val {vl:.4f}  val accuracy {acc:.4f}  {time.time() - t0:.0f}s", flush=True)
