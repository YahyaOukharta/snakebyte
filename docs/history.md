# History

"Local" is the gate match against the previous version on the local referee (random Legend
maps, both seats). Self-play overstates arena gains.

## Versions

| Version | Change | Local | Arena | Rank (Legend) |
|---|---|---|---|---|
| legacy | March 2026 submission: per-snake MCTS, depth 6, 32 ms | | 17.91 | 328 / 394 |
| v1.0 | from scratch: exact engine (0 desyncs), Smitsimax, handcrafted evaluation, 42 ms | vs legacy, real time: +14 −7 in the games legacy finished; legacy crashed in 59 of 80 | 19.0 at 54% (replaced) | ~254 |
| v1.1 | per-snake credit in the backup; gravity-aware apple distances with a capped two-slope cost; UCB constant 0.35 → 0.7; support counters + direct falls + branch-free selection (37k → 46k–73k simulated turns per turn) | vs v1.0: +88 −8 =4 real time (+382 ± 119), +88 −10 =2 at 1000 iterations | **30.49** (26.6 at 18%, 27.5 at 33%, 29.3 at 53%); 161 battles: +91 −60 =10 | **11** |
| v1.2 | path-based reach model (cell + unsupported run); an apple one cell beyond the head's reach counts as reachable; model 64 rows high (20 above the grid); 44 ms from the first input line | vs v1.1: real time +38 −35 =27 (+10 ± 59); 3000 iterations +67, −14 (200 games, +26 ± 44); vs the static model at 1000 iterations +51 ± 45 (200 games) | **32.14**; 159 battles: +87 −49 =23 | **3** |

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

| Apple reach off by one (the apple cell itself may be one further than the head can stand) vs v1.1-era build | 200, 1000 it | +70, +3 (pooled +36 ± 45) |
| Path-based reach (`gd[apple][length][run][cell]`) vs the static model with the off-by-one fix | 200, 1000 it | +42, +60 (pooled +51 ± 45) |
| Model 64 rows instead of 48 vs v1.1 | 100, 1000 it | +56 ± 64 (no regression; it removes desyncs when a snake climbs above the grid) |
| First value net n0a (72 features → 96 → 32 → 1, 2.7M records of 24k games at 600 iterations; held-out accuracy 78.5%) replacing the shared value, scale 1 / 2 / 4 / 8 | 100 each, 1000 it | −74 / −31 / +28 / −108 |
| n0a added to the handcrafted value, weight 1 / 2 / 4; half handcrafted + 3 | 100 each | +21 / +42 / −35; +35 (all ± 65) |

| Net n0b: the same net retrained on the path-based reach features. Replacing the shared value (scale 4) / added to it (weight 2) | 100 / 200, 1000 it | +42 / **+81, +89** (pooled +85 ± 45) |
| The n0b hybrid packed into the source, **real time on Kaggle** vs v1.2 | 200 | +28, +14 (pooled +21 ± 43): the net costs iterations |
| Nets on generation 1 (5.0M records of 70k games at 2000 iterations; held-out accuracy 80.4%): n1a (60% result + 40% search score), n1b (result only), vs n0b | 100 each | +14 / +17 |
| n1a with other mixes: pure net ×4; half handcrafted + 4; handcrafted + 3; handcrafted + 6, vs n0b's handcrafted + 2 | 100 each | −60 / −28 / −49 / −45 |
| Gradient for a snake with nothing in reach (`EV_UG` 0.004); live-snake term (`EV_ALIVE` 1) | 100 each | +21 / 0 |
| Values measured from the root's evaluation (`CENTER`): without net; with net; with `EV_K` 3 / 10; `EV_K` 4 + UCB 1.0; net weight 1 | 100 each | +14; −42; −3 / −31; −35; −38 |

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

- **The static reach model was wrong in two ways**, both found on one arena loss (12–31 on
  a 28 × 14 map, replayed with board dumps). It called an apple unreachable when it was
  one cell beyond the head's reach, although the head eats it before the fall and the
  growth keeps the tail down. And it called a cell reachable when it was near *any*
  supported cell, although the body has to lie on that cell: a snake spent the whole game
  hopping under an apple three rows above its head. The path-based model fixes both.
- **The first "never fall" attempt failed because of its budget, not its idea**: it
  applied the current stretch of the snake to every later run of the path, so a standing
  snake (stretched to its full length) saw nothing as reachable.
- **Arena games are close to coin flips one by one.** Twelve of v1.1's lost battles,
  replayed with the same code, seat, map and opponent: 9 wins. Replays of single games
  cannot judge a version; only the margin over many games means anything (v1.1 +31 over
  those twelve, the v1.2 code +71).
- **Desyncs in the arena** (one game with 11, one with 1, of 60 losses looked at) came
  from a snake above the 8 rows the model had over the grid.
- **A value net on the same information as the handcrafted evaluation adds little.** The
  first net equals it at best. In the earlier projects the first net saw the raw board;
  here its inputs are the handcrafted terms.

- **Where the points are (v1.2, 159 battles):** against ranks 3–12 we are level
  (+43 −24 =14 without the top two); against _H3R0_ and sZoom 1 win in 18. Replays show
  parity through the opening and middle game, then the opponent keeps eating while we
  stop. In the games looked at, the apples left were ones only a long snake reaches, or
  sat in dead-end pits where a snake of length 4 loses its head getting out (the search
  is right to refuse those).
- **The net saturates at these inputs.** Three times the data from a deeper teacher and a
  result / search-score blend: +15, inside the noise. Its inputs are the same quantities
  the handcrafted evaluation uses, as in sZoom's net, so the gain can only come from how
  they are combined. Every other mix of net and handcrafted value is worse than
  "handcrafted + 2 × logit".
- **Merging the net-input scan with the evaluation's scan** (one pass over apples ×
  snakes instead of two) brought the net build back from 5.1k to 6.2k iterations per
  turn on CodinGame.
- **Export is slow because of the reach tables**: 0.1–0.3 s per game, 45 minutes for 70k
  games on 8 cores. Records hold seed + moves, so a feature change costs one export, not
  a new generation.
- A wait loop written as `while pgrep -f "referee --gen"` matches itself and never ends:
  use `pgrep -x referee`.

## Arena notes

- v1.0 run: 14.1 at 6%, 17.9 at 19%, 19.0 at 22%, 19.5 at 42%, 19.0 at 54%, then replaced
  by v1.1. A run takes over two hours here.
- Test games against Renatys (rank 40), first seat: v1.0 23–29, 35–24, 25–26, 29–35,
  27–33, 22–55, 16–35; new defaults without credit 29–40, 20–29; v1.1 33–26.
