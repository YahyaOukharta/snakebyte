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
| v1.4b | v1.4 with 38 ms per turn instead of 44 | | **33.68**; no timeout of ours in the battles checked | **3** |
| v1.5 | **state merging** in the search (a snake's node is its head cell, facing and depth parity; second depth step 2000 iterations); Voronoi flood capped at 14 steps; speed pass (about 5% on x86); packed source without end-of-line comments | vs v1.4b, real time: −21, +38, +21 (+13 ± 36); the same source with plain trees +28, +42, +24 (+31 ± 37) | **33.61** | **3** |
| v1.6 | plain trees again (shared states were level over 1,600 real-time games and in the arena); net n3a trained on 120k games played by the v1.4b logic (generation 3; held-out accuracy 85.9%) | vs v1.5, real time, four batches of 50: +28, +78, −21, −92 (**0 ± 43**). Submitted at the user's wish | **33.11**: half a point under v1.4b / v1.5, replaced | 3 |
| v1.7 | v1.5's source with plain trees and the net n2a again (the net that scored 33.6–33.7); root safety and survival checks in the source, off | | replaced by v1.8 during its run (30.4 at 15% was the last reading) | |
| v1.8 | **room of each snake** (cells its head reaches in 4 moves, a cost under 8: `EV_SPACE` 1) and **standing on another snake counts as support** when a snake's reach is read (`SNK_SUP`); packed without comment lines | real time vs the same build without them: room +70, +35, −56; support +14, +56, −24, +85 (+33 ± 31); both +45; the packed file vs v1.7 +21 (50 games). Submitted at the user's wish | **33.06**; against the top two the wall hits did not fall (0.45 a game, theirs 0.03); sZoom +0 −13 =3, _H3R0_ +3 −11 =1 | **3** |
| v1.9 | **tempo credit** (`EV_TEMPO` 0.1): a snake's own credit is charged the mean table distance to its nearest apple over the line, so getting there sooner counts. The switch that moved the agreement with _H3R0_'s moves most (+1.4 points) | Kaggle, real time, vs v1.8: +16, −2 (+7 ± 22) | **33.75**: the best so far | **3** |
| v1.10 | **root prior** (`PRIOR_W` 0.05, `movePrefs`): every snake's root, ours and theirs, gets a bias toward the kind of move the top players choose (sixteen facts about a move, weights fitted on 30,000 of their decisions), fading with the visits | agreement with _H3R0_'s moves 61.9% → 63.9%; real-time self-play −17, +42; Kaggle −10, −13 | **34.25**: the best so far, with a change that is slightly negative against ourselves | **3** |
| v1.11 | **the prior inside the whole tree** (`PRIOR_T` 0.15): at every node a fading bias from the facts that need no scan (closer to an apple by the table, onto a supported cell, straight on, not up or down); packed with `--release` (no local-tool code) | Kaggle, real time, vs v1.10 settings: weight 0.03 +20, 0.05 +45, 0.08 +36, **0.15 +63**, 0.25 +24, 0.4 −21, 0.7 −115 (400 games each); local +60 | **36.04**: 1.8 above v1.10; _H3R0_ 38.33, sZoom 38.25, fourth 32.40 | **3** |
| v1.12 | **the root prior charges a pass** (`PRIOR_PASS` 1.5): a move after which the head is where it was (up and fall back) loses 1.5 on the preference score; the share of pass moves on 48 arena positions falls from 31% to 13% | Kaggle, real time, vs v1.11 settings, 400 games each: weight 1 +23; 1.5 +33, +10, +13; 2 +12; 2.5 +3; 4 +30 | **34.95**: 1.1 under v1.11, reverted | 3 |
| v1.11 again | the same file resubmitted after v1.12 | | **35.54**: with the first run, v1.11 is about 35.8 and v1.12's 34.95 a real step down | 3 |
| v1.13 | v1.11 with the **value net n4c**: trained on self-play (generation 3) plus the 1,848 replayed arena games three times and the 331 games against the top two five more times | Kaggle, real time, vs n2a: +30, +27, +45, +10 (**+28 ± 15**, 1600 games) | **34.97** (+94 −48 =17, 64.5%; v1.11's rerun 68.2%): the Kaggle gain did not carry over. Reverted | 3 |
| v1.14 | v1.11 (net n2a) with the root prior weight 0.05 → 0.08 | agreement with _H3R0_'s moves 63.3% → 64.3% (25.7k decisions, 210 games); self-play on n4c −18 ± 30. Same signature as v1.10, which self-play disliked and the arena liked | running | |

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
| Graded short-snake bonus (0.4 per part for parts 4–6) **against sZoom**, dump build, replay stats; baseline v1.4b the same way | 20 / 18 gauntlet games | +3 −13 =4, margin −3.95 (baseline +5 −11 =2, margin +0.11). Our share of snake-turns at length 3–4 fell from 41% to 31% as intended (his: 21–27%) and it did not help: the even lengths are a result of his play, not the cause |
| Tempo credit (`EV_TEMPO`): cost per cell of a snake's mean table distance over the line, 0.03 / 0.08 / 0.2 | 100 each, 3000 it | −24 / −3 / +3 |
| Pass cost (`EV_PASS`): cost per move in the line that leaves the head where it was, 0.05 / 0.15 / 0.4 | 100 each, 3000 it | −14 / +3 / −7 |
| Tempo credit 0.2 **against sZoom**, dump build, replay stats (`tools/gauntlet_stats.py`) | 20 gauntlet games | +3 −14 =3, margin −2.9. At 7–10 moves our snakes now get closer on 61% of the turns (46% before; his 60–62%) and pass 1.3 times a game (5.0 before; his 0.9–1.4), and we still eat 1.2 apples fewer than he does in turns 10–50: the wasted turns were a symptom |
| Assignment-aware distance (`EV_TEAM`): a snake's cost uses the nearest apple it is its side's closest snake to | 200, 3000 it | +45, −45 |
| **State merging** (`MERGE`): a snake's node is (depth mod k, head cell, facing) instead of its move sequence. k = 2 / 16 / 4 vs plain trees | 100–200 each, 3000 it | +45, +45 / 0 / +49 |
| Merging k = 2 with a deeper schedule (second step 400); UCB 1.1; start depth 6 | 100 each, 3000 it | +14; +35; −28 |
| Merging k = 2 at **7000 iterations**: usual schedule; second depth step 2000 (depth 10 instead of 13); plain trees with that schedule | 100–200 each | +3, −28; **+42, +31**; −14 |
| Merging k = 2 in **real time** vs v1.4b: usual schedule; k = 4; slow schedule (three batches) | 100 each | −10; −74; +14, 0, +17 (pooled +10 ± 35) |
| Merging k = 2 with the slow schedule, on the source without the decided-game rule, real time vs v1.4b (the earlier real-time batches had the rule on) | 300 | −21, +38, +21 (+13 ± 36). Submitted as v1.5 at the user's wish: the arena is to judge it |
| **Ablation** of what v1.3 and v1.4 shipped as bundles: the current build (plain trees) with one part changed, real time against the full build. Net off; elevation 0.04 → 0.01; win bonus 8.7 → 4; live-snake term off; Voronoi off; value scale, own-credit weight, UCB and ownership back to their values before SPSA; distance cost back to the old shape | 100 each | **−92**; +28; −14; +17; −24; **−70**; +3 |
| Elevation 0.01 and no live-snake term together, on the v1.5 configuration | 200 | +3, +7 |
| Merging k = 2 with value-to-go (`TOGO`: a node is backed up without the gains made before it) and the slow schedule, vs plain trees, on Kaggle | 400, 7000 it | +14 ± 30 |
| Plain trees vs merged (v1.5), **real time on Kaggle**, both accounts; merged with value-to-go vs merged | 1600; 800 | −20, −9, +15, −10 (**−6 ± 15**: level); −15, −21 |
| Net n3a (generation 3: 120k games by the v1.4b logic at 2000 iterations, one position in five, 12 epochs; held-out accuracy 85.9%) on plain trees vs v1.5, real time | 4 × 50 | +28, +78, −21, −92: 0 ± 43 |
| n3b: the same with 40% search score in the target | 50 | +7 |
| Board windows as net inputs (9×9 around each head, a learned vector per cell and content, summed per snake) next to the 83 inputs, same 965k records, held-out | — | accuracy 85.39% → 85.53%, loss 0.4018 → 0.4010: nothing. Not built into the engine |
| **Blunder set** (`tools/blunders.py`): 305 positions from 105 incidents in arena games against sZoom and _H3R0_, one to three turns before one of our snakes hit a wall or died. Share of positions where the build repeats the move of the game: current build at 6000 iterations; with a cost for a snake without a legal move at the leaf (`EV_TRAP`); **at 60,000 iterations** | 305 × 4 seeds | 89%; 90%; **82%** |
| On the same set: two-ply worst case against the enemy snakes near (`SAFE` 2); survival of 6 moves with the other bodies fixed (`SAFE_D` 6); both; three and four plies against the nearest enemy | 305 × 4 | 86%; 89%; 86%; 85%; 84% |
| `SAFE` 3 against the nearest enemy + `SAFE_D` 6 in real-time self-play: counting every loss the enemy can force; counting only what we lose beyond what he pays | 100 each | **−147**; −38 |
| `SAFE_D` 6 alone, real-time self-play | 200 | −60, +14 |
| What surrounds our head when it is shut in (32 incidents against the top two / 41 against the rest): two walls; an enemy lying on our body; an enemy directly above the head | — | 97% / 90%; 66% / 54%; 47% / 29% |
| Cost for a snake without a legal move at the leaf (`EV_TRAP` 1.5 and 3, with and without a share for one move left), real-time self-play | 100 each | +3, −14, −3 |
| **Standing on another snake counts as support** in `reachIdx` (`SNK_SUP`), real-time self-play | 400 | +14, +56, −24, +85: **+33 ± 31** |
| **Room of each snake** (`EV_SPACE`): cells its head reaches in 4 moves by a flood of its own; cost × (8 − cells) / 8. Weight 1; 2.5; 1.5 with threshold 12 | 300; 100; 100 | +70, +35, −56; −14; 0 |
| Both together; the packed v1.8 against v1.7 | 100; 50 | +45; +21 |
| **Trap incidents replayed on rails** (`tools/rail.py`): the 38 wall incidents against the top two restarted 8 turns earlier, our build playing our snakes for 14 turns against his recorded moves. Shut in again / wall hits / our length change: v1.7 settings; v1.4b; v1.8; v1.8 + survival check; v1.8 + worst-case check + survival | 38 | 20 / 43 / −19; 18 / 43 / 0; 17 / 34 / −8; 13 / 30 / −10; **13 / 26 / +2** |
| Those two checks on top of v1.8 in real-time self-play (`SAFE` 2 with two enemies; `SAFE` 3 with one within 3 cells) | 100 each | −45; −28 |
| Kaggle, real time: `SNK_SUP` alone; `EV_SPACE` alone | 400 each | +13 ± 31; +21 ± 32 |
| **Opponent on rails as a strength test** (`tools/railset.py`): 702 stretches of 15 turns from the games against _H3R0_, margin gained per stretch. v1.4b; v1.2; v1.3; v1.6; v1.8 | 702 | +0.04; **+0.74**; +0.29; +0.34; +0.37 (± 0.12): the weakest version scores best. Not a strength test |
| **Agreement with the top players' moves** (`tools/agree.py`): share of their real decisions (a snake with at least two legal moves) for which our search at 2000 iterations picks the move they played. _H3R0_ (14,220 decisions); sZoom (16,578) | — | 60.6%; 59.1% |
| **Every evaluation switch scored on agreement with _H3R0_'s moves** (baseline 60.59%, 17 s a run). Gains: tempo credit 0.1 **+1.36** (0.05 +1.04, 0.2 +0.30, 0.35 −0.41); value scale 12 +0.32; net weight 1.2 +0.29; ownership 0 +0.23. Losses: territory cells 0.01 / 0.03 −2.0 / −5.6; pass cost −1.9; value scale 6 −1.45; room cost 3 −1.04; soft ownership −1.04; net weight 3.2 −0.91; the rest within ±0.9 | 14,220 decisions | the settings already in use are near the top of almost every sweep |
| Where the disagreements are: by turn (under 15 / 15–40 / over 40); by distance to his nearest apple (≤ 3 / 4–10 / 11+ / none in reach). Kind of disagreement: his move goes closer and ours further; both closer; his further and ours closer | 14,220 | agreement 71% / 66% / 57%; 68% / 60% / 55% / 55%. 25% / 19% / 18% of the disagreements |
| **Fit** (`tools/fit.py`, coordinate ascent on 70% of his games): tempo 0.1, value scale 15, net weight 1.0, eat-sooner share 0.35. Agreement on his held-out games: defaults / tempo only / fitted; on sZoom's games | 4,300 / 16,578 decisions | 58.7% / 59.6% / **61.6%**; 59.1% / 61.3% / **61.8%** |
| The same at 7000 iterations (last 40 games against him): without tempo / v1.9 / fitted | 5,600 | 58.0% / 58.6% / 61.5% |
| Fitted set against the v1.9 settings, real-time self-play | 200 | **−24, −49** |
| Tempo 0.1 with net weight 1.2, Kaggle real time, vs v1.8 | 800 | −36, −4 |
| Head threat (`EV_HEAD`): cost or bonus for a head within 2 cells of an enemy head, with and without regard to the lengths, on the fitted set | 7 runs | −0.4 to −0.9 points of agreement, both signs |
| The fitted set (value scale 15, net weight 1.0, eat-sooner 0.35) in ordinary play, Kaggle real time; and **against _H3R0_'s live bot**, paired replays against v1.9 | 800; 32 pairs | **−72, −106**; margin −2.2 ± 1.6. Rejected by both |
| Value scale 15 alone; with net weight 1.5; scale 12 + net 1.5 + eat-sooner 0.35; net weight 1.5 alone (Kaggle, real time) | 400 each | −37; −54; −30; +13 |
| Second fitting round from the fitted point, 20 weights on a finer grid | — | nothing kept: the weights are at their optimum for this measure |
| Search-structure switches on agreement (first-visit ordering, merged states, value-to-go) | — | −0.5 to −1.0 |
| **What a move is like when they pick it** (`tools/prefs.py`: softmax over the legal moves of a decision, linear in sixteen facts). Weight for _H3R0_ / sZoom / us: into a cell with no apple in reach −1.10 / −1.20 / **+0.21**; regains reach +0.94 / +0.76 / +0.20; second apple far behind the first −0.55 / −0.71 / −0.15; per cell further from the nearest apple −0.38 / −0.53 / −0.24; enemy head next to the target **+0.01** / −0.53 / **−0.86**; straight on +0.44 / +0.53 / +0.20; target rests on a snake +0.67 / +1.29 / +0.61 | 14k–17k decisions each | the linear model picks sZoom's move 67.5% of the time, our search 61.7% |
| Those preferences as leaf terms: dead-zone cost (`EV_DEAD`) 0.5 / 1.5 / 3; cluster term (`EV_D2`); weight of a snake's own lost parts in its credit (`OWN_LOSS`) 0.5 / 0 / 1.5 | — | agreement +0.3 / −1.4 / −1.6; −0.5 to −1.5; −0.3 / −0.6 / −0.3 |
| The preferences as a prior at the root: as virtual visits (100 per move; 5; 2); as a bias that fades with the visits (`PRIOR_W` 0.02 / 0.05 / 0.2) | — | agreement −0.5 to −1.6; +0.6; +0.7; **+0.7 / +1.8 / +2.1** |
| Root prior 0.05 / 0.15 against the v1.9 settings, real-time self-play | 200 / 100 | −17, +42; +10 |
| The static part of the preferences inside the whole tree (`PRIOR_T` 0.06) | 200 | agreement +0.4; self-play +38, −17 |
| **Preference net** (28 facts of a move → 16 → score, `moveFeatures` / `prefNet`, 480 weights): held-out decisions of _H3R0_; of sZoom; both | 8.9k / 9.8k / 18.7k | 63.7%; 72.9%; **68.6%** (our search without a prior 61.2%) |
| The net as the root prior, weight 0.03 / 0.06 / 0.12 / 0.25: agreement with _H3R0_; weight 0.06 against the linear prior in real-time self-play | — ; 200 | 65.0% / 66.0% / **66.3%** / 66.1% (no prior 61.9%, linear 63.9%); +17, +35 |
| Net root prior against the linear one in ordinary play (Kaggle, real time): weight 0.06 twice; 0.03; 0.1; linear at 0.025; in-tree 0.15 with the net root prior | 400 each | −10, +19; +20; −12; +29; +18: level |
| **Pass moves and frozen snakes per version** (arena games, turns 12+): share of our snake moves that leave the head where it was / share of snake-turns inside a stand-still of 10 turns or more / games with a frozen snake of ours. v1.3–v1.8; v1.9; v1.10; v1.11 (first 32 games). Opponents | — | 15–17% / 3–5% / 23–36%; 18% / 5% / 46%; **27% / 15% / 64%**; **40% / 27% / 66%**. Opponents 17–22% / 6–10% |
| The same measured locally (`tools/passrate.py`: 48 arena positions played on by self-play): v1.8 settings; v1.9; v1.10; v1.11; with the root prior charging a pass 1 / 1.5 / 2 / 3 (`PRIOR_PASS`); "straight on" for sideways moves only in the tree (`PRIOR_SH`) | 48 positions | 13.8%; 18.2%; 29.4%; 31.3%; 18.2% / 13.1% / 11.2% / 7.6%; 28.6% |
| Pass charge 1 and 2 with `PRIOR_SH`, real-time self-play against v1.11 settings | 100 each | −7; −49 |
| **Freezes replayed on rails** (`tools/freeze.py`: 60 stand-stills of v1.10 / v1.11 games, 40 turns against the opponent's recorded moves), our length gained in total: without priors; v1.11 settings; pass charge 1 / 1.5 / 2.5 with `PRIOR_SH` | 60 | +244; +311; +333 / +319 / +321 |
| Pass charge with "straight on only sideways" in the tree (`PRIOR_SH`), Kaggle: charge 1 / 1.5 / 0.6 / 2.5; `PRIOR_SH` alone | 400 each | +3, −3 / +4, −7 / −11 / −10; −23: the tree tweak cancels the gain, dropped |
| Around the pass charge 1.5: in-tree weight 0.12 / 0.2; root weight 0.08; tempo 0.15 | 400 each | −4 / −10; +11; −23 |
| v1.11 in the arena against the top two: record; wall hits a game (ours / theirs) | 29 games | _H3R0_ +5 −8 =1, sZoom +2 −11 =2; 0.14 / 0.07 (v1.8: 0.45 / 0.03) |
| Preferences fitted on the facts the tree has (distance change, target on a wall or apple, straight, up, down, apple): held-out accuracy on both players' decisions | 18.7k | 61.6% with six facts: distance −0.42 a cell, support +1.05, straight +0.42, down −0.57, apple +1.00 |
| Kaggle on the v1.12 settings: apple at the target in the tree 1.0 / 0.5; target on a snake or apple 1.0 / 0.5; both; fading constant of the tree 40 / 250; net root prior; survival check; no room term | 400 each | −2 / −25; −3 / −8; −33; −10 / −1; −5; **−51**; −15 |
| **Value nets with real games in the training set** (`referee --feat`: 1,848 replayed arena games, 406,660 records; 331 games against the top two, 77,800 records) against n2a, Kaggle real time: n4a self-play + real ×3; n4b real only; n4c self-play + real ×3 + top-two ×5 | 800 each | +20, −30; −39, −9; **+30, +27** |
| More weight on the real games: n4d (top-two ×12), n4e (real ×6, top-two ×8), n4f (20 epochs, 20% search score in the target), against n2a | 800 each | −3, −15; −24, −27; −1, +7 |
| Retuning on net n4c, Kaggle real time, 400 games each: NET_A 1.6 / 2.8; EV_TEMPO 0.05; PRIOR_T 0.1 / 0.2; PRIOR_W 0.03 / 0.08; EV_K 7 / 9.5 / 10.5 (three runs) / 12 (two) / 14; UCB_C 0.6 / 0.85; EV_SELF 0.8; EV_K 10.5 + NET_A 2.8; EV_K 12 + NET_A 3 | 400 each | −5 / 0; −12; −16 / −1; −2 / −18; −28 / +17 / +25, +23, −10 / +1, +2 / +9; −27 / +3; −12; +28; +17. Nothing outside the noise |
| Late-game agreement (from turn 40) with reach tables up to length 16 / 20; unreachable gradient; stuck cost 0.5: sZoom; _H3R0_ | 14.9k / 12.9k decisions | +0.4 / +0.4; +0.3; +0.9. −0.2 / −0.3; −0.1; −0.5 |
| Reach tables where a snake that steps off at full stretch falls until its head is on a supported cell (`TBL_DROP`) instead of dropping one row | 100, 3000 it | −38 ± 61 |
| Win bonus given smoothly before the end (`WIN_SMOOTH` 1 / 0.5 / scale 4) | 100 each, 3000 it | −56 / −60 / −56: a large shared term that swings with the lead drowns each snake's own signal |
| **Decided games** (`DECIDED`): a lead larger than the apples left gets the win bonus at once | 200, 3000 it | +17, −21: no change in results, and the bot no longer stalls (below) |
| The same in **real time against v1.4b** (found when the whole source was gated): with the rule, three batches; without it | 100 each | **−53, −56, −67**; −10, +28 |
| Source without the rule (speed pass, flood cap 14) vs v1.4b, real time | 300 | +28, +42, +24 (pooled about +30 ± 37) |

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

- **Where our snakes lose their turns (18 replayed games against sZoom, turns 10–50).**
  Split by the distance to the nearest reachable apple, both sides approach equally
  often except in one band: at 7–10 moves we get closer on 46% of the turns and stay at
  the same distance on 35%; he gets closer on 62% and stays on 16%. Of our "same" turns
  there, 42% leave the head exactly where it was (up and fall back, a pass): 89 passes
  against his 16. Some are real waits next to an enemy head; many are a snake stretched
  to its full length, where every real move makes it fall and the table distance does
  not improve, so the pass ties with everything else.

- **Our snakes travel in packs.** In the baseline games against sZoom a snake of ours
  has the same nearest apple as a team-mate on 43% of its turns (apples within 12 moves,
  turns 10–50); his on 26%. Making each snake aim at an apple it is its side's closest
  to is zero in self-play (+45, −45).
- **Twenty gauntlet games cannot separate candidates.** v1.3, v1.4, v1.4b and three
  variants all score 15–30% against sZoom with margins between +0.1 and −4; the spread
  between runs of the same build is as large. The replay statistics show what a change
  does to the behaviour, not whether it wins.

- **State merging helps a shallow search and fades at the real budget.** Merged nodes
  are sampled far more often, which is worth +45 at 3,000 iterations. At 7,000 it needs
  a slower depth schedule to keep +35, and in real time against v1.4b it is +10 ± 35
  over 300 games. Speed is unchanged (556 against 594 ns per simulated turn). Same
  pattern as the start depth and the tuned net: what a change is worth at 3,000
  iterations says little about 7,000.

- **What a leaf costs (v1.4b, laptop, per leaf):** reach-table terms 0.9 µs, the Voronoi
  flood 1.0 µs, the net 0.6 µs; the evaluation is 40% of the search time against 14%
  before the flood and the net. The flood runs 15.7 steps on average over 21 rows.
  Sweeping only the rows the frontier has reached and stopping once no apple is left
  unclaimed changes nothing in mid-game (apples behind bodies keep it going) and
  helps when few apples are left. Counting inside the sweep made it 15% slower: the loop
  must stay pure bit operations to be vectorised. The cap is now 14 steps (400 games on
  Kaggle: +20 ± 30 against 32).

- **Speed pass on x86** (`tools/kaggle_bench.py`: same positions, fixed iterations and seed,
  compiled with the instruction sets CodinGame allows, best of five; all steps play
  identically, checked on 40 games and between the x86 and ARM builds):

  | Step | Total time |
  |---|---|
  | Voronoi sweep in AVX2, four rows per instruction | −1.9% |
  | Apple scan over the apples alive at the root and the live snakes only | ±0 (−3% on the laptop) |
  | Net layers in explicit AVX | ±0 |
  | Support recount folded into the pass that writes a fallen snake back | −1.1% |
  | Tree nodes of exactly one cache line (64 bytes, were 52) and the chosen child prefetched | −1.7% |

  About 5% together. GCC already vectorised the flood, the scan and the net; writing them
  by hand adds nothing. Profile on the Kaggle Xeon: evaluation 30–39%, falls 23–40%,
  selection 22–26%, move 12–16%, at 1,150–1,500 ns per simulated turn.
- **The packed source reached 99,691 of CodinGame's 100,000 characters.** `pack.py` now
  drops the comments at the end of code lines when the result would pass 96,000 (91.5k
  after); comments on their own lines stay. The stripped file plays the same games.
- GCC 13 refuses `#pragma GCC target` in front of the standard headers in a -DLOCAL build
  ("target specific option mismatch" in std::string); the bench passes the same sets as
  flags with -DNO_PRAGMA.

- **The decided-game rule cost 55 Elo and the first test did not see it.** At 3,000 fixed
  iterations in self-play it was +17 and −21; in real time against the live version
  −53, −56, −67. A lead larger than the apples left is not decided (snakes still lose
  parts and die), and adding the bonus of 8.7 early moves every value into the flat part
  of the squash, where the search cannot tell moves apart. It is off; the stalling when
  behind near the end is back and waits for a fix that does not touch the scale of the
  values. Rule for the gates: a change that goes into the default build is gated in real
  time against the live version, each change on its own, before the next one is added.

- **What carries the weight (ablation, real time).** The net (−92 without it, more than
  the +20 it showed when it was added: it has since been retrained on truthful inputs)
  and the four search and credit values from the SPSA run (−70). The handcrafted
  Voronoi term is worth little on top of the net, which has the Voronoi counts as
  inputs (−24). Passengers: the elevation bonus at 0.04, the live-snake term and the
  win bonus at 8.7 change nothing either way. And the distance-cost shape of v1.4 is
  level with the old one here (+3), although it was +51 at 3,000 iterations: one more
  gain that a fixed-iteration test saw and the real budget does not.

- **The feature export filled the disk.** 120k games at one position in three are 4 GB
  of float records next to the earlier exports; with no free byte left neither the shell
  nor the file tools could start, and the user had to free space by hand. The export of
  generation 3 was redone at one position in eight with four processes (1.4 GB). Old
  exports are deleted once a net is trained from them: the game records (a few hundred
  bytes per game) are what is kept.

- **Fresh self-play data did not move the net.** Generation 3 was played by the current
  logic with truthful tables; the net trained on it predicts its own games better
  (85.9% against 81.1%) and plays exactly as well as the old one. Higher accuracy here
  means the games became more predictable, not that the net knows more.
- **Batches of 50 swing by ± 88 Elo**: the four batches of the same pair read +28, +78,
  −21, −92. They are good for early news and bad for decisions; the sum is what counts.
- A sanity check before a submission must look at *our* timeouts ("$0 has not provided"
  when we are first), not at any timeout in the log: the opponent's stopped a submit.

- **Every logged game can be replayed exactly.** The official map generator, seeded the way
  the CodinGame SDK seeds it (SecureRandom SHA1PRNG, `setSeed`), gives the map of a game
  from the seed in its log (`tools/mapdump`). With both players' moves from the log,
  `tools/harvest.py` rebuilt 1,115 of 1,154 arena battles with no turn disagreeing with
  the score line our bot printed (the rest: one early submission with simulator
  mismatches, and logs that could not be fetched). Every move the top players made
  against us is now a position on disk.
- **Where the body parts go (v1.3–v1.5, per game, `tools/losses.py`).** Head-on collisions
  are the bulk and are symmetric: 2.0 a game for both sides against the top two, the same
  number of deaths in them. The difference is the walls: we run into one 0.40 times a
  game against the top two (they 0.12; we 0.11 against everybody else) and lose a whole
  snake that way in 12% of those games (they in 1%). A snake only hits a wall when it has
  no legal move left.
- **These are not oversights of a shallow search.** On the positions one to three turns
  before such an incident the current build repeats the move of the game 89% of the
  time, and with ten times the iterations 82%. The search means it: its model of an enemy
  snake is our own evaluation (go and eat), and against sZoom and _H3R0_ the enemy does
  something else.
- **What the 38 wall incidents were.** Six were dead ends we walked into with nobody's
  help (a move that kept six legal moves ahead existed and the other was played): a
  length-11 snake turning into a one-cell pocket its own body then closed. For the other
  32 the last free choice was fine on the board of that moment and the door was shut
  afterwards: an enemy snake lying down on top of ours (it rests on us and follows), or
  parked in a 2×2 loop on the only way out of a pit while ours bled a part a turn.
- **A worst-case check at the root is the obvious answer and a costly one.** Counting
  every loss the nearest enemy can force makes our snakes run from every enemy head
  (−147 Elo in self-play: a head-on costs him a part too). Counting only what we lose
  beyond what he pays is −38, and moves the blunder set from 89% to 86%. Self-play
  cannot price this: the opponent there never shuts anyone in. It needs the gate against
  the real players.
- **A view of the raw board adds nothing to the net** (85.39% → 85.53% held out). The 83
  inputs already carry what a 9×9 window around each head shows.
- **A net that predicts its own games better is not a better net**: n3a, 85.9% on
  generation 3, played level locally and scored 33.11 in the arena against 33.6–33.7 for
  n2a.

- **The trap has a shape.** When one of our snakes is shut in against the top two there are
  two walls at its head in 97% of the cases (the floor and a wall ahead), an enemy snake
  is lying on its body in 66% and sits directly above the head in 47%. Their snakes climb
  onto ours (both treat another snake as something to stand on), ride along by gravity,
  and ours runs out of floor. We did neither: our reach lookup counted a snake standing on
  another snake as hanging in the air, and nothing in the evaluation knew how much room a
  head has.
- **The per-player Voronoi cannot see a single snake being boxed in** (the user's point):
  three free team-mates keep the total. The room is per snake: a four-step flood from each
  head on the rows the Voronoi already builds.
- The packed source reached 99,719 characters again; `pack.py` now also drops the comment
  lines when dropping the end-of-line comments is not enough (91.1k).

- **v1.8 did not stop the traps in real games.** On the old incidents replayed on rails it
  is shut in less often than v1.7; in its own arena games against the top two the wall
  hits are where they were (0.45 a game). Its four incidents show what _H3R0_ does: one
  of his snakes comes down onto the cell above or beside our head while ours is on the
  floor next to a wall. He moves in on purpose; a fixed recording of his old moves cannot
  show that.
- **An opponent who does not answer is not a test of strength.** With his snakes on their
  recorded moves the plain greedy v1.2 gains most and v1.4b least: the order of the arena
  upside down. The wall hits it counts are still worth reading; the margin is not.
- **What can be measured fast against the top players is agreement**: whether our search,
  for his snakes in his positions, picks the move he played. It needs no match, runs in
  under a minute on 14,000 decisions, and every switch of the evaluation can be scored on
  it. The assumption is that the first of the arena chooses better than we do.

- **Agreement with the first of the arena is a usable measure, with a limit.** It prefers
  on its own the settings that matches had already picked, it holds on games it was not
  fitted on and on the other top player, and it holds at the real search budget. But the
  set that agrees most (flatter values, half the net weight) loses about 35 Elo to the
  v1.9 settings in self-play. The net was trained on our own games and helps most against
  ourselves; whether trusting it less pays against the players who beat us is for the
  arena to say. Resolution: about ±0.4 points; the head-threat term, either sign, sits
  inside that.
- **He is more direct than we are.** A quarter of the disagreements are his snake going
  toward its nearest apple where our search picks a move that goes away from it, and the
  agreement falls with the distance to the apple and with the turn. The tempo credit is
  the term that answers it; in self-play it had been worth nothing, which is why it was
  off.

- **Fitting all the weights to their moves was a mistake the gates caught.** The set that
  agrees most with _H3R0_ lost 90 Elo in self-play and lost to v1.9 against his own bot.
  Agreement is a microscope for one idea at a time (the tempo credit it found is the best
  arena score so far); as a quantity to maximise it leads away from strength.
- **How the top two choose, in numbers.** They do not step onto cells from which their
  snake can reach no apple (we are indifferent), they go where a second apple is close
  behind the first, they keep going straight, and _H3R0_ does not avoid a cell because one
  of our heads is next to it while we avoid his: a head-on costs both a part, but our
  per-snake credit charges the snake for its own.
- **A preference about a move belongs at the root, not at the leaves.** The same facts as
  leaf terms changed nothing or cost agreement; as a bias on the root moves they add two
  points with the linear model and four with a small net. Virtual visits are the wrong
  way to give a prior (they take the exploration away: −0.8 with no prior at all); a bias
  inside the selection formula that fades with the visits is the right one.
- sZoom is far more predictable than _H3R0_ (73% against 64% for the same net).

- **The priors made the snakes stand still, and the standing still is mostly not the
  loss.** "Straight on" for a vertical snake is up, and up is falling back: with the root
  prior the share of pass moves went from 18% to 27% and with the in-tree prior to 40%,
  and two games in three had a snake of ours frozen for ten turns or more. A game the
  user pointed at (19–35 against putibuzu) had three of our four snakes motionless from
  turn 30. But in that game those three (lengths 3, 5 and 4) had no apple in reach by the
  tables: the apples left were high and the opponent's snakes were all 6 or longer. And
  on 60 replayed freezes the builds with the priors gain more length than the one without
  (+311 against +244); charging the pass in the prior adds a little (+333). The fault
  behind the freeze is the old one: he grows every snake early, we leave some short.
- **The arena confirmed the first change that self-play could not have found.** The root
  prior is slightly negative against a copy of ourselves (−10, −13 over 800 games) and
  finished half a point above v1.9 in the arena.
- Twenty to forty paired replays against one player do not resolve half an arena point
  (± 2.2 on the margin after 22 pairs) and spend the daily quota of 600 test games in a
  few hours; the IDE mirror of a submit needs a test game too and waits when the quota is
  gone. The quota is for sanity games and dumps.

- **The prior inside the tree was the largest arena gain since v1.3**: 34.25 → 36.04, 73%
  of the battles won, and against _H3R0_ 5 wins in 14 where it had been 1 in 5. The wall
  hits against the top two fell to a third. It costs a few table lookups per node.
  Six facts about a move, with weights read off the top players' games, are worth more
  than every evaluation term added since the net.

- **Seven positive gates and a lost arena run.** The pass charge was +3 to +33 in every
  one of seven Kaggle gates (2,800 games) and v1.12 finished 1.1 below v1.11. Either one
  of the two arena scores is off by more than the ±0.5 assumed so far, or standing still
  is right more often against real opponents than against ourselves (the top players
  pass on 17–20% of their moves, and the prior was fitted on what they do). v1.11 was
  resubmitted to measure the first.
- Against sZoom, v1.11 is level until a quarter of the apples are left (−0.4 at turn 30,
  −1.4 at 25%) and loses three more in the last quarter (−4.3 at the end); against
  _H3R0_ that late drop is gone (−1.5). The last apples are the clusters on the top edge;
  his long snakes are under them before the last quarter starts, one of them standing on
  our longest snake.

## Arena notes

- v1.0 run: 14.1 at 6%, 17.9 at 19%, 19.0 at 22%, 19.5 at 42%, 19.0 at 54%, then replaced
  by v1.1. A run takes over two hours here.
- Test games against Renatys (rank 40), first seat: v1.0 23–29, 35–24, 25–26, 29–35,
  27–33, 22–55, 16–35; new defaults without credit 29–40, 20–29; v1.1 33–26.
