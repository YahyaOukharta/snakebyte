# Search

Smitsimax ("decoupled UCT"): one tree per snake, all trees descended together, one shared
simulation. It handles simultaneous moves and eight units without a joint move space
(3⁸ joint moves per turn), and it is what every top bot of this game uses
([research.md](research.md)).

## One iteration (`search` in `bot.cpp`)

1. Copy the root state (5.2 kB).
2. For each ply up to the current depth, for each live snake: read the moves that do not
   run into a wall or a body (`validMask`), pick one in the snake's own tree at its current
   node (`selectMove`), remember (node, move). Then play all moves at once with
   `State::step`.
3. Evaluate the final position once, from my side, in (−1, 1).
4. Add the value to every remembered (node, move). A snake of mine maximises it, an
   opponent's snake minimises it (the sign is applied in the selection).

Nothing in a tree node says where the other snakes are: a node is "this snake played these
moves", averaged over whatever the others did. That is the approximation Smitsimax makes.

## Details

| Piece | What | Value |
|---|---|---|
| Node | `w[4]`, `n[4]` (floats), `ch[4]`, `N`; 52 bytes, pool of 1.5M, cleared on allocation, rebuilt every turn | |
| Selection | branch-free on x86: `±w/n + C·sqrt(ln N / n)` for the four moves at once with SSE `rcp`/`rsqrt` (`sqrt(ln N)` from a table); an unvisited move gets 10⁶ plus random low bits, a move outside the mask −10⁹; the winner is the position of the maximum | C = 0.7 |
| Valid moves | not backwards; target cell empty or apple, or the tail of a snake (it will move away unless that snake eats). No valid move: go straight | |
| Depth | grows with the iteration count: `3 + n/250` up to 1,000 iterations, then one ply per 1,000 (after _H3R0_) | depth 11–14 in a turn |
| Time | 44 ms per turn counted from the first input line (v1.1: 42 ms of search), 850 ms on the first; the clock is read every 16 iterations | |
| Move played | most visited move at the root of each of my snakes | |
| Opponent | searched like my own snakes with the sign flipped; its root choice is not used | |

## Evaluation (`evaluate`)

`raw`, from my side:

| Term | Weight | |
|---|---|---|
| length difference | 1 | half from the final position, half from the mean along the line, so eating sooner scores more |
| losses difference | 0.05 | the tie-break |
| apple ownership | 0.25 per apple | +1 if my nearest head is closer than the opponent's, −1 if further; an apple nobody can reach counts for nobody |
| distance of each snake to its nearest reachable apple | 0.08 per cell for the first 8, 0.015 for the next 20 | at most 0.94: always less than one apple, so eating the last apple in reach never lowers the value |
| elevation of the head | 0.01 per row | |
| finished game | ±4 on top of the length difference | by score, then losses |

### Gravity-aware distances

Most apples hang in the air. A snake of length 3 cannot reach one four rows above the
ground, and a plain BFS distance sends it straight up at it.

After every turn some part of a snake rests on something, so its head is never more than
(length − 1) cells of free path away from a *supported cell* (a free cell with a wall or an
apple below). On turn 1:

1. `bfsSupport`: `hsup[c]` = BFS distance from cell c to the nearest supported cell.
2. `bfsGravity`: for each apple and each length b = 1..16, a BFS from the apple through
   cells with `hsup ≤ b − 1` only: `gdist[apple][b−1][cell]`, 255 if there is no such path.

A snake of length L reads `gdist[apple][min(L,16)−1][head]`. Supports are the walls and the
first-turn apples; other snakes as support are ignored, so the table is pessimistic about
climbing on a team-mate.

### Per-snake credit

The value backed up into snake i's tree is not the shared value but

```
value_i = squash((raw ± own_i) / 6)        own_i = growth of snake i since the root − its distance cost
```

(+ for my snakes, − for the opponent's, which minimise). `squash` is a rational
approximation of tanh. With one shared value, a snake's tree mostly measures what the
other seven snakes happened to do in that simulation; adding the snake's own contribution
makes its statistics depend on its own moves. This single change was worth +288 Elo
([history.md](history.md)).

### Tried and switched off

`POL` (first visits ordered by apple distance), `REUSE` (tree reuse between turns),
`EV_OWNS` (ownership credit per snake) and `EV_GLOB` ≠ 1 are still in the source as
parameters; none beat the default.

## Parameters

Every constant is a `PARAM(name, default)`. A `-DLOCAL` build reads `SB_<name>` from the
environment, so a sweep needs no rebuild:

```sh
SB_UCB_C=0.5 ./build/bot          # one bot with another exploration constant
```

`SB_ITERS=N` replaces the clock by a fixed number of iterations per move (deterministic
with `SB_SEED`), which is what the referee's `-n` sets.
