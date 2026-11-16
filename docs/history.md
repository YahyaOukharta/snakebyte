# History

"Local" is the gate match against the previous version on the local referee (random Legend
maps, both seats). Self-play overstates arena gains.

## Versions

| Version | Change | Local | Arena | Rank (Legend) |
|---|---|---|---|---|
| legacy | March 2026 submission: per-snake MCTS, depth 6, 32 ms | | 17.91 | 328 / 394 |
| v1.0 | from scratch: exact engine (0 desyncs), Smitsimax, handcrafted evaluation, 42 ms | vs legacy, real time: +14 −7 in the games legacy finished; legacy crashed in 59 of 80 | 19.0 at 54% (replaced) | ~254 |
| v1.1 | per-snake credit in the backup; gravity-aware apple distances with a capped two-slope cost; UCB constant 0.35 → 0.7; support counters + direct falls + branch-free selection (37k → 46k–73k simulated turns per turn) | vs v1.0: +88 −8 =4 real time (+382 ± 119), +88 −10 =2 at 1000 iterations | 26.6 at 18%, 27.5 at 33%, 29.3 at 53%, **29.42 at 55%** (still running) | **19** |

## Measurements

All at fixed iterations per move against the stated baseline. ± is the 95% interval.

| Experiment | Games | Result (Elo) |
|---|---|---|
| **Per-snake credit** (`EV_SELF`): each snake's tree is backed up with the global value plus its own growth and apple-distance cost. Weight 0.5 / 1 / 2 / 4 vs 0 | 100 each, 1000 it | **+269 / +288 / +205 / +173** (± 75–95) |
| UCB constant 0.2 / 0.6 / 1.0 vs 0.35 (Kaggle) | 300 each, 1500 it | −55 / **+61** / +20 (± 39) |
| UCB 0.8 vs 0.35; 0.6 with `EV_K` 10 vs 0.35 | 100 each | +21, +49 (± 68) |
| Value scale `EV_K` 3 / 12 vs 6 | 300 each | −64 / +34 |
| Apple ownership 0.1 / 0.5 vs 0.25 | 300 / 100 | +16 / −60 |
| Plain distance weight 0.08 vs 0.03; 0.015 vs 0.03 | 300 / 100 | −54; −10 |
| Mean-length share 0 vs 0.5 | 300 | +12 |
| Elevation 0.03 vs 0.01 | 100 | +21 (noise) |
| Depth cap 10; first depth step 150 instead of 250 | 100 each | −38; +49 (noise) |
| Gravity distances, first model ("never fall": unsupported run along the path < length), unreachable = plain + 8 / + 30 | 120 / 100 | −73 / **−382** |
| Gravity distances, second model (cells within length − 1 of a supported cell), unreachable = plain + 8 / + 0 | 100 each | +10 / −38 |
| Second model, plain cost 0.05 / 0.1 / 0.2 per cell with unreachable + 20 / 20 / 30 | 100 each | +85 / +21 / −89 |
| Second model with the capped two-slope cost: 0.05×10 + 0.01×30 / 0.08×8 + 0.01×30 / 0.05×10 + 0.02×20 / 0.03×20 + 0.01×20 | 100 each | +3 / +70 / +42 / +3 (± 70) |
| Faster engine alone (support counters, direct falls, SSE selection: +27% simulated turns) vs v1.0, real time on a loaded laptop | 120 | −14 ± 61 |

| Apple-directed first visits (`POL`): an unvisited move is tried in order of distance to the nearest reachable apple, 1 in 8 / 1 in 3 at random, vs random order | 100 each, 1000 it | −7 / +24 (± 64): off |
| Start depth `D_BASE` 5 / 8 / 12 vs 3 | 100–200 each, 1000 it | +135, +85 / +78 / +10 |
| Start depth 5 / 7 vs 3, **real time** | 100 each | −3 / −21 (± 60): the gain at 1000 iterations is not there at 6000 |
| Gravity distances off (`EV_GRAV` 0) vs on, with credit | 200 | +53, −3: no difference in self-play |
| Iterations ×3: 3000 vs 1000; 12000 vs 4000 | 100 each | +104 ± 67; +63 ± 61 |
| Tree reuse (`REUSE`): statistics under the played moves kept, scaled by 0.25 / 0.5 / 0.75 / 1 | 100 each, 1000 it | −10 / 0 / −35 / −60: off |
| Shared value weight in a snake's tree (`EV_GLOB`) 0.25 / 0.5 / 1.5 vs 1 | 100 each | −89 / −31 / 0 |
| Credit for owned apples to the nearest snake (`EV_OWNS`) 0.25 / 0.5 | 100 each | −35 / −42 |
| With credit: UCB 0.4 / 1.1 vs 0.7; `EV_K` 4 / 9; mean share 0; ownership 0 / 0.5; win bonus 1; losses 0.3; elevation 0; second depth step 500 | 100 each | +10 / −14; −24 / −38; −38; −100 / −35; −42; +14; −10; +3 |
| Two-slope cost 0.12×6 / 0.05×12 vs 0.08×8 | 100 each | −21 / +45 (± 61) |

## What was learned

- **Credit assignment was the bottleneck, not speed.** With eight snakes sharing one value,
  a snake's tree mostly sees noise from the other seven. Adding the snake's own growth and
  distance to the value its tree receives was worth more than everything else together.
  27% more simulations were worth nothing measurable.
- **Why the snakes sat still** (board dumps of games against rank 40): most apples hang in
  the air, out of reach for a snake of length 3 or 4, and the plain BFS distance pointed
  straight up at them. The opponent took 19 apples in the first 24 turns, we took 12.
- **"Never fall" is the wrong model of reach.** A snake that turns sideways at the top of
  its column drops one row and keeps going: falls are part of normal movement. The model
  that works is static: a head can only be on cells within (length − 1) steps of a
  supported cell.
- **A distance cost must stay below one apple**, or eating the last apple in reach lowers
  the evaluation (the next one is far or out of reach) and the snake refuses to eat. That
  is why larger plain weights lost.
- **100 games resolve about ±70 Elo.** Fine for effects like the credit term; the weight
  tuning above is mostly inside the noise.
- **Fixed-iteration tests at 1000 iterations mislead on depth.** A deeper start looked like
  +100 Elo and was worth nothing in real time, where the depth is already 12. Depth and
  time questions are tested in real time or at 5000+ iterations.
- **Compute is worth about +60 Elo per tripling** at the arena's level of iterations, so a
  1.5× faster engine is worth about +20: real, but smaller than one good evaluation idea.
- **Tree reuse does not pay here.** Kept statistics come from a search one ply shallower with
  the values measured from the old root; the more is kept, the worse it plays.
- **After the credit term the handcrafted evaluation is on a plateau**: twenty parameter and
  structure changes in a row inside the noise.
- Selection was 42% of the search time on CodinGame before it was made branch-free; falls
  47% before the support counters.

## Arena notes

- v1.0 run: 14.1 at 6%, 17.9 at 19%, 19.0 at 22%, 19.5 at 42%, 19.0 at 54%, then replaced
  by v1.1. A run takes over two hours here.
- Test games against Renatys (rank 40), first seat: v1.0 23–29, 35–24, 25–26, 29–35,
  27–33, 22–55, 16–35; new defaults without credit 29–40, 20–29; v1.1 33–26.
