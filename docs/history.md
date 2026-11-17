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
| v1.3 | reach tables hold on walls only (an eaten apple was support for ever); Voronoi apple territory at the leaves (flood from the heads on bit rows, bodies block), weight 0.25; value net n0b added to the handcrafted value (72 → 96 → 32 → 1, column-form forward pass); 15 parameters from SPSA; reach table with the apple index innermost | vs v1.2: real time +60 −21 =19 (**+143 ± 66**) | **33.30**; 159 battles: +81 −60 =18 (sZoom +3 −17 =3, _H3R0_ +4 −14 =2, ranks 4–9 +51 −28) | **3** |
| v1.4 | net n2a on 83 inputs (adds Voronoi apples per side, soft ownership, reachable apples per snake; trained on records re-exported with walls-only tables); distance cost flat near and steep far (0.02 × 6 cells + 0.045 × 20 instead of 0.07 × 11 + 0.015 × 16); reach tables limited to the grid plus a margin, with a 700 ms guard | vs v1.3: real time +54 −21 =25 (**+119 ± 62**), 3000 iterations +96 ± 66 | 31.0 at 33% with 5 timeouts in 35 battles: replaced | |
| v1.4b | v1.4 with 38 ms per turn instead of 44 | | running | |

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

| SPSA over 15 parameters with the n0b hybrid (250 batches of 16 games at 1000 iterations). End point: UCB 0.735, `EV_K` 8.55, `EV_WIN` 8.69, `EV_SELF` 0.64, `EV_OWN` 0.19, `EV_ELEV` 0.04, `EV_ALIVE` 1.39, `NET_A` 2.18, cost 0.07×11 + 0.015×16. Tuned vs defaults, both with the net | 100, 1000 it | +10 ± 60 |
| Tuned n0b hybrid vs v1.2 at equal iterations | 100, 3000 it | **+111 ± 64** |
| Tuned n0b hybrid vs v1.2, **real time** on the laptop (4 / 2 games at a time) | 100 each | +53 / +14 (± 61) |
| Net forward pass in column form (see below): the net's cost falls from 54% to 12% of a simulated turn (696 no net, 1071 before, 781 ns after). Tuned hybrid vs v1.2, real time | 100 | +14 ± 65 |
| The same with the budgets of CodinGame: 7000 iterations vs 7800 for v1.2 | 100 | +17 ± 58. Pooled with the three real-time batches: +24 ± 31 over 400 games |
| Reach tables for lengths up to 20 instead of 12 (tables 100 → 305 ms) | 100, 3000 it | −7 ± 63 |
| **Bit-row engine** (`tools/experiments/bitboard_engine.cpp`): apples, the union of snakes and every snake as one 64-bit word per row, the body stored minus an offset so a fall is one addition. 40 games identical to the byte board. Laptop: 708 vs 700 ns per simulated turn. CodinGame, same game replayed twice each: 398k / 762k simulated turns in the first 850 ms against 665k / 1013k | — | slower on x86: not kept |
| Stuck-snake case from a lost game against sZoom (below): reach tables on **walls only** (`SUP_APPLES` off) vs walls + first-turn apples | 100, 3000 it | −7 ± 65 alone |
| Extra cost for a snake with no apple in reach (`EV_STUCK`) 2 / 5 with walls-only tables; 2 with the old tables | 100 each, 3000 it | +24 / **−115**; +3 |
| **Voronoi apple territory** (`EV_VOR`): apples the flood from my heads reaches first minus the opponent's. Weight 0.12 / 0.25 / 0.6 vs 0; 0.25 replacing the static ownership term; territory cells 0.01 (`EV_VORC`) | 100 each, 3000 it | +21 / **+56** / −111; −28; 0. Cost: +6% per simulated turn |
| Walls-only tables **and** Voronoi 0.25 vs the tuned n0b hybrid; the same with `EV_STUCK` 2 | 100 each, 3000 it | **+111 ± 66**; +92 |
| That build (v1.3) vs v1.2, **real time** | 100 | **+143 ± 66** (+60 −21 =19) |
| Gauntlet (CodinGame test games, first seat) of the tuned n0b hybrid before these two changes: vs _H3R0_ / vs sZoom | 16 / 15 | +4 −11 =1 / **+0 −15** |
| After v1.3 — Voronoi: tail parts ignored 3 instead of 1; weight 0.35; flood capped at 14 steps; static ownership 0.1 | 100 each, 3000 it | −14; +67; +35; −28 |
| Voronoi weight 0.45; weight 0.35 with 14 steps | 100 each, 3000 it | −67; −70: the +67 of 0.35 was noise, 0.25 stays |
| Soft apple ownership (`EV_SOFT`): per apple tanh((opponent's distance − mine) / 3) instead of the 0/1 ownership, weight 0.2 / 0.4; 0.2 on top of it; 0.3 with scale 6 | 100 each, 3000 it | +49 / −42; −38; −7: off |
| Root safety (`THREAT`): my snakes do not enter a cell an enemy head can enter this turn, except apple cells / including them | 100 each, 3000 it | −56 / −78: off |
| Gauntlet of v1.3, first seat: vs sZoom / vs _H3R0_ | 15 each | +2 −11 =2 / +2 −10 =3 (before: +0 −15 / +4 −11 =1); most losses now by 1–3 points |
| Unreachable gradient again, now with walls-only tables (`EV_UG` 0.005 / 0.02), handcrafted only | 100 each, 2000 it | 0 / −78 |
| Net n2a on inputs v2 (83: + Voronoi apples per side, soft ownership, reachable apples per snake; 4.0M records of the generation 1–2 games re-exported with walls-only tables; held-out accuracy 81.1%) vs v1.3 | 200 at 3000 it + 100 real time | +42, +10; +3 (pooled +18 ± 37) |
| n2b: the same with 40% search score in the target; n2a with net weight 3.2 | 100 each, 3000 it | +31; 0 |
| n2a packed (out/v14.cpp) vs v1.3, **real time on Kaggle**, both accounts | 4 × 200 | +33, +28, +10, +14: pooled **+21 ± 22**; with the 300 local games about +20 ± 19 |
| Banked apples (`EV_BANK`): an apple I reach 6 moves before the opponent, or alone, counts 0.3 / 0.6; 0.3 with a margin of 3 | 100 each, 3000 it | 0 / **−200**; −63: the snakes postpone eating what is "theirs anyway" |
| Apple density (`EV_PROX`): sum of 1/(1+distance) over the apples a side is nearest to, weight 0.3 / 0.8; 0.5 with ownership 0.05 | 100 each, 3000 it | −10 / −100; +3 |
| Kaggle, 400 games each at 3000 iterations, on the n2a build: Voronoi 0.35; flood cap 14; soft ownership 0.2 without the 0/1 term; stuck cost 2; UCB 0.6; Voronoi tail 2 | 400 each | −31; +20; −23; −9; +1; +17 (all ± 31): nothing |
| **Distance-cost shape** (cost per cell × cells, near part + far part), vs 0.07×11 + 0.015×16: linear 0.03×33; **0.02×6 + 0.045×20**; linear 0.045×22; 0.07×11 + 0.03×16 | 100 each, 3000 it | +35; **+78**; +14; +17 |
| 0.02×6 + 0.045×20 again; 0×4 + 0.05×20; 0.02×6 + 0.06×20; 0.02×6 + 0.035×30 | 100 each, 3000 it | +24 (pooled +51 ± 43); −31; −14; +7 |
| n2a + the new shape (v1.4) vs v1.3 | 100 real time; 100 at 3000 it | **+119 ± 62; +96 ± 66** |
| Reach tables limited to the grid, 3 columns beside and 14 rows above (table time 70 → 44 ms, 56 → 37 ms) vs the whole model | 100, 3000 it | +24 ± 64: no loss |
| Gauntlet of v1.4, first seat: vs sZoom / vs _H3R0_ | 15 each | +2 −10 =1 (2 timeouts) / +1 −11 =1 (2 timeouts) |
| Kaggle, 400 games each at 3000 iterations, on v1.4: value scale 6.5; own-credit weight 1.0; eat-sooner share 0.25; elevation 0.01; ownership 0.3; second depth step 700 | 400 each | −8; +13; −1; −6; −17; −1 (all ± 31): a plateau |
| Bonus for a live snake above length 3 (`EV_SHORT`), as a cost on short snakes 1 / 2.5; as a bonus 1 / 2.5; for length above 4; graded over parts 4–6 at 0.4 / 0.8 | 100 each, 3000 it | −31 / −63; −10 / +14; −10; +38 / +10: nothing in self-play |
| Reach tables where a snake that steps off at full stretch falls until its head is on a supported cell (`TBL_DROP`) instead of dropping one row | 100, 3000 it | −38 ± 61 |
| Win bonus given smoothly before the end (`WIN_SMOOTH` 1 / 0.5 / scale 4) | 100 each, 3000 it | −56 / −60 / −56: a large shared term that swings with the lead drowns each snake's own signal |
| **Decided games** (`DECIDED`): a lead larger than the apples left gets the win bonus at once | 200, 3000 it | +17, −21: no change in results, and the bot no longer stalls (below) |

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

- **The simulation is spent on falling snakes.** Counted on two positions: 95–98% of
  simulated turns have at least one snake in the air, 2.5–4.3 snakes per turn, and almost
  all of them really drop (by one row on average): in the trees the snakes climb at
  random and come down again. So the fall code is the common path, not the exception.
- **"ns per sim" is per simulated turn**, all snakes together (`sims += depth`). At
  700 ns for about seven snakes a snake costs ~100 ns per turn: 30 for the selection, 60
  for a fall. On CodinGame the bot simulates 40–60k turns, about 350k snake moves, per
  44 ms.
- **Bit rows did not beat the byte board.** A fall became a few word operations per row
  the snake spans instead of four passes over its parts, yet the total did not move on
  the laptop and got worse on CodinGame: the byte board's per-part loops are short and
  the bit version pays on every move (two boards to update, an offset on every body
  read, a search for the owner of a cell).
- **A dot product per output unit is the slow way to run the net without -ffast-math.**
  Written as "for each input, add input × its column to the whole layer" every inner loop
  is a plain `y += a·x`, which the compiler vectorises as it is, and inputs that are zero
  (empty snake slots, units off after the ReLU) are skipped. 4.4× faster for the same
  outputs (checked against the training-side values).
- **The laptop in real time is about as fast as CodinGame** (7k iterations per turn on
  both), so a local real-time batch is a fair picture of the arena budget.
- **Gains of the evaluation shrink with depth**: the tuned hybrid is +111 at 3000
  iterations and about +20 at 7000. The same was seen for the start depth.
- zsh does not split `$VAR` into words: build option lists with `eval` or arrays.

- **Against the top two the margin goes steadily, then all at once.** Over 189 arena
  logs: level at three quarters of the apples left, −1.2 at half, −2.7 at a quarter,
  −4.9 at the end (against everybody else: +0.3, +0.1, +0.2, +1.0), and we lose one body
  part more per game than they do. In the gauntlet games the last quarter of the apples
  often goes 8 to 1.
- **One lost game against sZoom, replayed with board dumps, showed three faults:**
  1. *Phantom support.* The reach tables counted the apples of the first turn as support
     for the whole game. Once such an apple is eaten the table still walks over it: a
     length-3 snake in a pit was told for 80 turns that an apple was 9 moves away.
  2. *A body part can be worth a snake.* Both sides had a length-4 snake in a pit that a
     length-3 snake cannot leave. Ours stepped twice into a cell the opponent's head also
     wanted, lost a part, and sat in the pit for the remaining 190 turns; the mirrored
     snake of sZoom left its pit and ate nine apples. Two of our four snakes ended at
     length 3.
  3. *Bodies are not in the tables.* A length-9 snake circled for 24 turns ten moves
     from an apple behind an opponent's body.
  Walls-only tables fix the first; the Voronoi flood sees the third. Neither does much
  alone in self-play (−7, +56) but together +111: the flood needs truthful distances
  beside it. A flat cost for "nothing in reach" does not fix the second (−115 at 5): a
  snake in the air or between apples is often without reach for a turn.
- **Self-play between two builds that share a blind spot cannot see it.** Both sides had
  phantom paths, so the fix alone measured nothing; the games against real players
  (gauntlet, arena logs) showed it. A dump build replayed on a lost game
  (`cg.py test out/dump.cpp --replay GAME`) is the fastest way to such a fault.
- A column cut off by `cut -c1-60` reads "dist 1" for "dist 13": print the whole line
  before believing a bug.

- **A lost endgame is lost earlier.** In a v1.3 game against sZoom we led 39–37 with ten
  apples left and lost 41–44. At that point their four heads were already next to seven
  of the ten (three of them a cluster in the top corner, reached up a wall column by a
  length-7 snake standing on a live apple at the end); ours had been eating the nearest
  apple each. They take the contested and far apples first and leave the safe ones.
- **Single-parameter tests at 100 games have stopped telling anything**: after v1.3 the
  same change measures +67 and −70 in two batches. Changes of this size need 400+ games
  or a different kind of evidence (gauntlet, dumps).

- **Where the gap to the top two opens (v1.3, 43 arena games):** level through turn 8,
  then about 0.07 apples per turn behind from turn 10 to 30 (−0.6 at turn 15, −1.0 at
  20, −1.7 at 30), with equal body-part losses. Against everybody else the margin stays
  at zero over the same turns. It is the eating rate on the second wave of apples, the
  ones further away, not tactics and not the endgame.
- **The distance cost had its slope in the wrong place.** It was steep for the first 11
  cells and nearly flat beyond. But the search sees an apple a few moves away by itself;
  only the evaluation can pull a snake toward one beyond the horizon. Flat near and steep
  far (0.02 per cell for 6 cells, then 0.045 for 20) is the first shape change in the
  whole project that the SPSA neighbourhood had not found: it only moves each number a
  little. Total still about one apple, so eating never lowers the value.
- **Timeouts are machine stalls, about one game in 150.** Our own clock shows 30 of
  18,020 arena turns stretched past 46 ms and 14 past 50 ms with a 44 ms budget; v1.3
  lost one battle that way and its opponents two.
- **The reach tables took 605 ms on a 44 × 24 map with 72 apples** on a slow CodinGame
  machine. The search over (cell, run) covered the whole 64 × 64 model, mostly empty
  margin. Limited to the grid plus a margin, and stopped at 700 ms whatever happens.
- A chained shell line "test && submit" must check the test's result, not its exit
  code: the sanity game timed out and the submit ran anyway.

- **CodinGame's machines stall, and how often depends on the hour.** v1.4 lost 5 of its
  first 35 arena battles and 4 of 30 gauntlet games on time. In the logs the failing
  turn is complete: fewer iterations than usual and 61–74 ms on our clock, so the process
  was frozen for 20–30 ms in the middle of the search and stopped as soon as it ran
  again. A control with v1.3 at the same hour showed the same rate of stretched turns
  (13 of 1,178 past 46 ms; in the morning 3 of 3,528): the servers, not the version.
  The stalls are 2–30 ms, median 8. With a 44 ms budget 18 of 59 ended past 58 ms; with
  38 ms about 5 would. v1.4b runs at 38 ms.

- **Replaying real games exactly** (`tools/trace.py`): a dump build prints the board of
  turn 0; with both players' moves from the log the engine replays the whole game, and
  the result is checked against the score our bot printed every turn (0 mismatches).
  `trace.py LOG --pos T` writes the bot's input at turn T, so any position of a real game
  becomes a bench position. `trace_stats.py` counts, per side, what the snakes do with
  their turns.
- **What sZoom's snakes do differently (20 replayed games).** Between turns 10 and 50 his
  snakes get closer to an apple on 52–59% of their turns, ours on 45–47%; ours have
  nothing in reach 7–14% of the time, his 3–7%. A snake of length 3 has nothing in reach
  half the time, on either side, and we carry twice as many snake-turns at length 3–4
  as he does (1,190 against 598): he spreads the growth over his snakes, we feed one.
  In margin the 20 games are level (+0.1 apples per game) while the results are 5–11–2.
- **The bot stalled instead of eating when it was behind near the end.** Found on a game
  the user pointed at (34–40, three apples, nothing for 100 turns). Replayed and turned
  into a bench position: the snake walks to one step from the apple four times and
  turns away. Eating brings the end nearer, the end is a loss, and the loss penalty
  (8.7) exists only in finished games; not eating keeps the game "open" inside the
  search horizon. With the bonus applied as soon as the lead exceeds the apples left,
  the same position is played out in 23–64 turns and the apple is eaten.
- **The elevation bonus cancels the pull downwards.** At 0.04 per row against 0.045 per
  cell of distance, a step down toward a far apple is worth nothing. It was 0.01 before
  the SPSA run; at 400 games 0.01 and 0.04 are equal in self-play.
- Before blaming the reach tables, print them: the distance that looked "shorter than
  possible" was right; the snake's run had been left out of the reading.

## Arena notes

- v1.0 run: 14.1 at 6%, 17.9 at 19%, 19.0 at 22%, 19.5 at 42%, 19.0 at 54%, then replaced
  by v1.1. A run takes over two hours here.
- Test games against Renatys (rank 40), first seat: v1.0 23–29, 35–24, 25–26, 29–35,
  27–33, 22–55, 16–35; new defaults without credit 29–40, 20–29; v1.1 33–26.
