#!/usr/bin/env python3
"""Train the value net on exported records (build/referee --export).

  train.py -o tools/nets/n0.bin data/g0.f32 [more.f32 ...] [--epochs 20] [--lam 1.0]
A record is NF float features, the game result for the side to view (1 / 0.5 / 0) and the
final score difference / 16, and the search score. Target: the result, softened by the final
margin (--mw, --margin), blended with the search score (--lam < 1).
The net is NF -> 96 -> 32 -> 1 (ReLU); its output is the logit of a win. The .bin written is
raw float32 in the order the engine reads: W1 b1 W2 b2 W3 b3.
"""
import argparse, glob, numpy as np, torch, torch.nn as nn, time

NF, H1, H2 = 83, 96, 32
ap = argparse.ArgumentParser()
ap.add_argument("files", nargs="+")
ap.add_argument("-o", "--out", required=True)
ap.add_argument("--epochs", type=int, default=20)
ap.add_argument("--lam", type=float, default=1.0, help="weight of the result against the search score")
ap.add_argument("--k", type=float, default=6.0, help="search score scale")
ap.add_argument("--mw", type=float, default=0.5, help="share of the margin-softened result")
ap.add_argument("--margin", type=float, default=0.4, help="sigmoid slope per body part of final margin")
ap.add_argument("--lr", type=float, default=2e-3)
ap.add_argument("--bs", type=int, default=4096)
ap.add_argument("--h1", type=int, default=H1)
ap.add_argument("--h2", type=int, default=H2)
a = ap.parse_args()

files = sorted(sum((glob.glob(f) for f in a.files), []))
def load(f):                                              # a file cut short loses its last partial record
    x = np.fromfile(f, dtype=np.float32)
    return x[:len(x) // (NF + 3) * (NF + 3)].reshape(-1, NF + 3)
data = np.concatenate([load(f) for f in files])
rng = np.random.default_rng(1)
rng.shuffle(data)
nval = min(len(data) // 20, 200000)
dev = "mps" if torch.backends.mps.is_available() else "cuda" if torch.cuda.is_available() else "cpu"
X = torch.from_numpy(data[:, :NF]).to(dev)
res, diff, sc = (torch.from_numpy(data[:, NF + k]).to(dev) for k in range(3))
soft = torch.sigmoid(a.margin * 16 * diff)                 # result softened by the final margin (in body parts)
tgt = (1 - a.mw) * res + a.mw * soft
if a.lam < 1:                                             # blend with the search score of the position
    tgt = a.lam * tgt + (1 - a.lam) * torch.sigmoid(a.k * torch.atanh(sc.clamp(-0.999, 0.999)))
Y = tgt
Xv, Yv, Rv = X[:nval], Y[:nval], res[:nval]
Xt, Yt = X[nval:], Y[nval:]
print(f"{len(data)} records from {len(files)} files, {nval} held out, device {dev}; mean result {res.mean():.3f}")

net = nn.Sequential(nn.Linear(NF, a.h1), nn.ReLU(), nn.Linear(a.h1, a.h2), nn.ReLU(), nn.Linear(a.h2, 1)).to(dev)
opt = torch.optim.Adam(net.parameters(), lr=a.lr)
sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs)
bce = nn.BCEWithLogitsLoss()
n = len(Xt)
for ep in range(a.epochs):
    t0 = time.time()
    perm = torch.randperm(n, device=dev)
    tot = 0.0
    net.train()
    for i in range(0, n, a.bs):
        idx = perm[i:i + a.bs]
        loss = bce(net(Xt[idx]).squeeze(1), Yt[idx])
        opt.zero_grad(); loss.backward(); opt.step()
        tot += loss.item() * len(idx)
    sched.step()
    net.eval()
    with torch.no_grad():
        out = net(Xv).squeeze(1)
        vl = bce(out, Yv).item()
        dec = Rv != 0.5
        acc = ((out[dec] > 0) == (Rv[dec] > 0.5)).float().mean().item()
    print(f"epoch {ep + 1:2d}  train {tot / n:.4f}  val {vl:.4f}  val accuracy {acc:.3f}  {time.time() - t0:.0f}s", flush=True)

sd = net.state_dict()
with open(a.out, "wb") as f:
    for k in ("0.weight", "0.bias", "2.weight", "2.bias", "4.weight", "4.bias"):
        f.write(sd[k].detach().cpu().numpy().astype(np.float32).tobytes())
print("wrote", a.out)
# reference outputs for `referee --evalcheck`: the first 1000 records of the first file, unshuffled
ref = torch.from_numpy(np.fromfile(files[0], dtype=np.float32, count=1000 * (NF + 3)).reshape(-1, NF + 3)[:, :NF]).to(dev)
with torch.no_grad():
    np.savetxt(a.out + ".check", net(ref).squeeze(1).cpu().numpy(), fmt="%.6f")
