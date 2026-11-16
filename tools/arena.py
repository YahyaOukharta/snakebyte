#!/usr/bin/env python3
"""Archive our CodinGame arena battles (the API only exposes games we played in).

  arena.py fetch           battles of every submission -> arena/battles.jsonl
  arena.py stats [SUB_ID]  score per opponent (default: latest submission)
  arena.py losses [SUB_ID] game ids of lost battles (replay one: cg.py test SRC --replay ID)
"""
import json, os, sys, time, urllib.request, urllib.error

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = "https://www.codingame.com/api/v1"
PUZ = "winter-challenge-2026-snakebyte"
BATTLES = os.path.join(ROOT, "arena", "battles.jsonl")
TOKEN = open(os.path.expanduser("~/.config/codingame/token")).read().strip()


def get(path):
    for _ in range(5):
        req = urllib.request.Request(API + path, headers={"Authorization": "Bearer " + TOKEN})
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                body = r.read().decode("utf-8")
            time.sleep(0.65)
            return json.loads(body)
        except urllib.error.HTTPError as e:
            if e.code == 429:
                time.sleep(int(e.headers.get("Retry-After", "30")) + 1)
                continue
            raise
    raise RuntimeError("rate limited: " + path)


def load():
    d = {}
    if os.path.exists(BATTLES):
        for l in open(BATTLES):
            if l.strip():
                r = json.loads(l)
                d[r["game_id"]] = r
    return d


def fetch():
    battles = load()
    subs = get(f"/puzzles/{PUZ}/submissions")
    subs = subs.get("submissions", subs)
    for s in subs:
        sid, before = s["submission_id"], None
        while True:
            page = get(f"/puzzles/{PUZ}/battles?submission={sid}&limit=100" + (f"&before={before}" if before else ""))
            for b in page.get("battles", []):
                if not b.get("done"):
                    continue
                opp = next(p for p in b["players"] if not p.get("me"))   # finished battles list players by position, not seat
                res = "draw" if opp.get("position") == b["my_position"] else ("win" if b["my_position"] == 0 else "loss")
                battles[b["game_id"]] = {"game_id": b["game_id"], "submission": sid, "result": res,
                                         "opponent": opp["nickname"], "ended_at": b.get("ended_at")}
            before = page.get("next_before")
            if not before:
                break
        print(f"submission {sid}: {sum(1 for b in battles.values() if b['submission'] == sid)} battles", flush=True)
    os.makedirs(os.path.dirname(BATTLES), exist_ok=True)
    with open(BATTLES, "w") as f:
        for k in sorted(battles):
            f.write(json.dumps(battles[k]) + "\n")


def pick(sub):
    battles = load()
    if not battles:
        sys.exit("no battles: run arena.py fetch")
    sub = int(sub) if sub else max(b["submission"] for b in battles.values())
    return sub, [b for b in battles.values() if b["submission"] == sub]


def stats(sub):
    sub, bs = pick(sub)
    rows = {}
    ix = {"win": 0, "loss": 1, "draw": 2}
    for b in bs:
        rows.setdefault(b["opponent"], [0, 0, 0])[ix[b["result"]]] += 1
    w, l, d = (sum(r[i] for r in rows.values()) for i in range(3))
    print(f"submission {sub}: +{w} -{l} ={d} ({100 * (w + d / 2) / max(w + l + d, 1):.1f}%)")
    for o, r in sorted(rows.items(), key=lambda x: ((x[1][0] + x[1][2] / 2) / sum(x[1]), -x[1][1])):
        print(f"  {o:<22}+{r[0]} -{r[1]} ={r[2]}")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "stats"
    arg = sys.argv[2] if len(sys.argv) > 2 else None
    if cmd == "fetch":
        fetch()
    elif cmd == "losses":
        for b in pick(arg)[1]:
            if b["result"] != "win":
                print(b["game_id"], b["result"], b["opponent"])
    else:
        stats(arg)
