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
| Selection | an unvisited valid move at random first; then max of `±w/n + C·sqrt(ln N / n)`, four moves at once with SSE `rcp`/`rsqrt`, `sqrt(ln N)` from a table | C = 0.35 |
| Valid moves | not backwards; target cell empty or apple, or the tail of a snake (it will move away unless that snake eats). No valid move: go straight | |
| Depth | grows with the iteration count: `3 + n/250` up to 1,000 iterations, then one ply per 1,000 (after _H3R0_) | depth 11–14 in a 42 ms turn |
| Time | 42 ms per turn, 850 ms on the first; the clock is read every 16 iterations | |
| Move played | most visited move at the root of each of my snakes | |
| Opponent | searched like my own snakes with the sign flipped; its root choice is not used | |

## Evaluation (`evaluate`)

`value = tanh(raw / 6)`, with `raw` from my side:

| Term | Weight | |
|---|---|---|
| length difference | 1 | half from the final position, half from the mean along the line, so eating sooner scores more |
| losses difference | 0.05 | the tie-break |
| apple ownership | 0.25 per apple | +1 if my nearest head is closer than the opponent's, −1 if further |
| distance to the nearest apple | −0.03 per cell, per snake | capped at 60 |
| elevation of the head | 0.01 per row | higher is better |
| finished game | ±4 on top of the length difference | by score, then losses |

Distances are a plain BFS from each first-turn apple through non-wall cells (`bfsApple`,
once, on turn 1): one byte per cell per apple. They ignore gravity, so an apple high in the
air looks close; that is the first thing to replace ([future.md](future.md)).

None of these weights has been tuned yet.

## Parameters

Every constant is a `PARAM(name, default)`. A `-DLOCAL` build reads `SB_<name>` from the
environment, so a sweep needs no rebuild:

```sh
SB_UCB_C=0.5 ./build/bot          # one bot with another exploration constant
```

`SB_ITERS=N` replaces the clock by a fixed number of iterations per move (deterministic
with `SB_SEED`), which is what the referee's `-n` sets.
