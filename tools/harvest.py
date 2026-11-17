#!/usr/bin/env python3
"""Turn every logged arena battle into an exact replay.

  harvest.py            fetch the logs that are missing (arena/battle_logs/), rebuild each
                        game from its seed and moves (tools/trace.py), check it against the
                        score line our bot printed every turn, and keep the replay input in
                        arena/traces/<game_id>.trace (referee --trace reads it)
A line per game goes to arena/traces/index.jsonl: game id, opponent, our seat, result,
turns, mismatches.
"""
import json, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cg, trace
ROOT = cg.ROOT
LOGS, OUT = os.path.join(ROOT, "arena", "battle_logs"), os.path.join(ROOT, "arena", "traces")
os.makedirs(OUT, exist_ok=True)
rows = [json.loads(l) for l in open(os.path.join(ROOT, "arena", "battles.jsonl"))]
done = set()
idx = os.path.join(OUT, "index.jsonl")
if os.path.exists(idx): done = {json.loads(l)["game_id"] for l in open(idx)}
ok = bad = fail = 0
for r in rows:
    g = r["game_id"]
    if g in done: continue
    log = os.path.join(LOGS, f"{g}.log")
    try:
        if not os.path.exists(log):
            open(log, "w").write(cg.call(f"/games/{g}", raw=True)); time.sleep(0.3)
        text = open(log).read()
        res, mism, n = trace.run(log, os.path.join(OUT, f"{g}.trace"))
        me = int(re.search(r"\n  (\d) \S+ \(me", text).group(1))
        open(idx, "a").write(json.dumps({"game_id": g, "opponent": r["opponent"], "submission": r["submission"], "seat": me, "result": r["result"],
                                         "turns": len(res) - 1, "mismatch": mism, "snakes": n}) + "\n")
        ok += mism == 0; bad += mism != 0
    except (Exception, SystemExit) as e:
        fail += 1
        print("fail", g, str(e)[:80], flush=True)
    if (ok + bad + fail) % 100 == 0: print(ok, "exact,", bad, "with mismatches,", fail, "failed", flush=True)
print("done:", ok, "exact,", bad, "with mismatches,", fail, "failed")
