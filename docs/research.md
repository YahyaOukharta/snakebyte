# Research notes

What the strong SnakeByte bots do, collected on 2026-11-16 from the contest post-mortems.
Public material is thin: six of the contest's top 30 wrote something; nothing from contest
ranks 2–4, 6 or 8–11.

## Sources

- Post-mortem thread (21 posts): https://forum.codingame.com/t/winter-challenge-2026-feedbacks-and-strategies/208044
- Contest thread (rules questions): https://forum.codingame.com/t/winter-challenge-2026/207974
- Puzzle discussion: https://forum.codingame.com/t/snakebyte-winter-challenge-2026-puzzle-discussion/208050
- sZoom (contest #5, arena #2): https://github.com/s-Zoom/winter-challenge-2026-snakebyte-PM
- _H3R0_ (contest #12, arena #1): https://github.com/yassine-nid/Winter-Challenge-2026-Post-Mortem
- Sheeesh (contest #7): https://gist.github.com/Sheee-sh/ae2dd0474412d604025fbedf883e89e0
- Official referee: https://github.com/CodinGame/WinterChallenge2026-Exotec
- Brutaltester-compatible referee: https://github.com/aexg/WinterChallenge2026-Exotec

Contest top 10 (2,382 players): frictionless, blasterpoard, zlakhdar, bowwowforeach, sZoom,
delineate, Sheeesh---, gaha, marwar22, eulerscheZahl. Arena on 2026-11-16: _H3R0_ 36.93,
sZoom 36.81, then ram_trains_2026 31.85: the first two are five points clear.

## What the top bots do

| Player | Search | Evaluation | Speed |
|---|---|---|---|
| frictionless (contest #1) | Smitsimax, depth 12; nodes merged by (head cell, facing), two per state by turn parity, so a tree has at most 8 × grid nodes | handcrafted: win/loss, score, distance to nearest apple, elevation, apples closest to each player | ~50k turns / 50 ms, a third of the time in the simulator |
| sZoom (#5) | Smitsimax, progressive depth without cap, low exploration constant, 4k–6k iterations / 45 ms | small value net: per snake alive, length, distance to 2 nearest apples; global apples left, turn, apples eaten, length lost, total length, apples in each player's Voronoi territory | ~250k simulations / 45 ms worst case |
| _H3R0_ (#12) | Smitsimax, 45 ms, depth = 3 + n/250 below 1000 iterations, then 7 + (n−1000)/1000 | Voronoi territory per leaf (uint64 rows, AVX2) + 10 heuristics tuned with Optuna | ~150k simulations / 45 ms |
| jacek (#17) | Smitsimax depth 10; UCB + wall penalty −1, apple +1, next to apple +0.25, 2% random moves | value net (tanh): map stats, body totals, min Manhattan distance to apples, per-snake head x/y, apple distance, length; output buckets by map height; labels ±0.999^roundsLeft | ~1,500 iterations, 4v4 medium map |
| Sheeesh (#7) | none: PPO policy | 160k-parameter U-Net, 17 input planes on 40×60, policy read at each head | ~30 ms inference, timeouts |
| Regulus136 (#28) | beam search per snake in turn (width 60, depth 20), others follow a greedy plan | | |

Details that mattered to their authors:

- **Gravity-aware distances.** _H3R0_: BFS that tracks the longest unsupported run, precomputed
  on turn 1 for every body length; ~#55 → top 20. sZoom: distance from every apple to every
  cell for each length up to 13, with the first gap the head must cross cached. frictionless
  did without: plain BFS from each apple plus an elevation term.
- **Fast falls.** sZoom: each snake counts its parts resting on a wall or apple; a snake on a
  grounded snake inherits support, so the fall code is skipped for most snakes. frictionless:
  each snake tracks how far from its tail it is supported.
- **State merging** (frictionless): called by Csipcsirip "the thing most people missed".
- **Selection policy** (frictionless): bias toward apples, a tail about to move, supported
  cells, moves toward the nearest apple; away from the side edges. Moves into a wall or a
  body (not the tail) pruned.
- **Per-snake credit** (jacek): each snake's backed-up score is blended with its own size
  change (weight 0.1).
- **Closest apples with AVX2** (frictionless): Manhattan distance to up to 64 apples, 32 per register.

## Lower-ranked write-ups

- Beam search with a frozen or greedy opponent tops out around ranks 50–150: "The opponent
  model was the ceiling… Needed MCTS or DUCT" (oidrissi).
- Gravity-aware BFS was "always too pessimistic" for oidrissi (bodies act as moving support).
- A tiny altitude bonus stops snakes stalling under apples they cannot reach (luxcem).
- Searching nearby snakes as pairs lets them climb on each other (HummusHustler: biggest gain).
- Smitsimax with too few simulations is corrupted by random wall collisions (wlesavo): prune them.

## Rules found in the referee source, not in the statement

- Turn order: all moves → eats → beheadings → falls. The tail leaves its cell unless the new
  head is on an apple, so a cell a tail just left is free.
- Eating is decided before beheading: a head that enters an apple and also collides keeps the
  growth, and the apple is gone. Two heads on one apple both grow and are both beheaded.
- A snake of 3 parts or fewer (counted with the new head) that collides is removed; a longer
  one loses its head only.
- The facing is not stored: each turn it is head minus neck. A backward command is ignored.
- Outside the grid is empty space, not wall. A snake dies only when every part is at
  y ≥ height + 1, and only after a fall step. Those deaths are not counted as losses.
- Falls: a snake is grounded if a part is above a wall, an apple or a grounded snake;
  ungrounded snakes drop one row together and the test repeats. `doIntercoiledFalls` is dead code.
- Tie-break: equal scores are decided by losses (parts lost to beheadings and collision deaths).
- Maps: height 10–24 (not 30), width = round(1.8 × height) made even (18–44), mirrored
  left–right; the bottom row is always wall; Legend favours large maps (skew 0.3); 2–4 snakes
  per player, each 3 long, standing on a wall; isolated wall cells become apples.

## Platform

- 50 ms per turn, 1000 ms for the first. Top players use 45 ms. Random timeouts were reported
  at league openings (jacek, Sheeesh).
- Local referees give false timeouts at 50 ms under load.
- A local pool of similar bots overfits (0x6E0FF).
