# Engine

The simulator in `bot.cpp` (`struct State`) plays one game turn exactly as the official
referee does (`Game.java`: `doMoves`, `doEats`, `doBeheadings`, `doFalls`). The search, the
local referee and the opponent-move detection all run on it.

## Board

The real grid is at most 44 × 24, but snakes may leave it: outside is empty space, not wall.
The model is a 64 × 64 byte array with the grid placed 10 columns from the left and 20 rows
from the top (v1.0 and v1.1: 48 rows, 8 above; top bots climb higher than that above the
grid, which showed as desyncs in two arena games):

```
cell = (y + 20) * 64 + (x + 10)       x = (cell & 63) - 10      y = (cell >> 6) - 20
neighbours: up −64, right +1, down +64, left −1
```

| Value | Meaning |
|---|---|
| 0 `EMPTY` | free |
| 1 `APPLE` | power source: solid for falls, eaten by a head |
| 2 `WALL` | platform |
| 3 `VOID` | outer ring of the model: a head that enters it is beheaded; nothing rests on it |
| 4 + i | body part of snake i |

A stride of 64 makes row = `cell >> 6` and keeps a state small (4,096 bytes of board). The
`VOID` ring removes every bounds check. It is the one place the model differs from the
referee: a snake more than 9 columns outside the grid or 19 rows above it would behead
itself in the model and not in the game. The search never wants to go there, and the
opponent has not in any checked game.

## Snakes

A body is a ring buffer of 128 cells (`uint16_t b[128]`) with the head at `b[h]`:
advancing is one store, dropping the tail is `len--`. Part i from the head is
`b[(h - i) & 127]`. Next to it: `len`, `alive`, and `sup`, the number of parts that have a
wall or an apple directly below.

The facing is not stored, as in the referee: it is head − neck, so a beheaded snake faces
the way its new head points.

`State` also carries `napples`, `turn`, `losses[2]` (the tie-break), and per player the
total length `len[2]` and number of live snakes `alive[2]`, all updated in place so the end
test and the score are free.

## One turn: `State::step`

1. **Moves.** For each snake: new head = head + direction; `eat` = that cell is an apple.
   Without an apple the tail cell is cleared and the body shortened now, before any
   collision test, so a cell a tail just left is free for everybody.
2. **Collisions**, decided for all snakes on the position after all moves: the new head is
   in a wall, in `VOID`, in any body (its own included), or on the same cell as another new
   head.
3. **Apply.** A snake that did not collide gets its new head written. One that collided
   keeps the growth if it ate (the apple is removed whoever ate it), then: with 3 parts or
   fewer, counting the new head, it is removed and its size added to `losses`; otherwise
   only the new head is lost (it was never written) and `losses` grows by one.
4. **Falls.**

Checked cases: two heads entering the same apple both grow and are both beheaded; a snake
of exactly 3 that hits anything dies; one of 4 survives as 3.

## Falls

Referee rule: a snake is grounded if any part is directly above a wall, an apple or a
grounded snake. All other snakes drop one row together, and the test repeats until every
snake is grounded or gone. A snake is gone when, after a drop, all its parts are at
y ≥ height + 1.

`falls()` is that loop as written (kept as the reference, build with `-DSLOWFALL`).
The search uses two shortcuts that give the same result:

- **Support counters.** `sup` is kept up to date in `step`: −1 when the tail leaves a cell
  above a wall or apple, +1 when the head arrives above one, −1 for the part above an apple
  that is eaten. If every snake has `sup > 0`, nothing can fall and the fall code is skipped.
- **`fastFalls`.** Only snakes with `sup == 0` can be in the air. Snakes in the air drop in
  lockstep, so they never block each other: the drop of one is the distance to the first
  cell under any of its parts that is neither empty nor part of an airborne snake (a part
  with its own body below is skipped, the lower part decides). A drop of 0 means the snake
  rests on a grounded snake: it becomes ground and the others are measured again.
  Otherwise all airborne snakes drop by the smallest distance at once, those that landed
  get their `sup` recounted and become ground, and the loop repeats. A snake whose distance
  to "all parts below the grid" is reached first is removed.

A climbing snake falls every turn (the tail leaves the ground, the body drops back one
row), so falls are the common case, not the exception: they were 47% of the search time
before these shortcuts and about 30% after.

## Exactness

`fastFalls` against `falls()`: two builds play the same seeded games at fixed iterations
(80 games, 8 snakes) and must produce identical results. They do.

Against CodinGame: each turn the bot tries every combination of opponent moves (three per
snake, at most 81) on the previous position and requires one of them to reproduce the
position it received (`resync`). That also recovers the opponent's moves and the losses,
which are not in the input. A failure is counted in stderr (`desync N`) and tagged `DS` on
a move, which is visible in arena battle logs. Six test games, about 600 turns, against the
default bot and ranks 40, 150 and 300: 0 desyncs.

## Speed

Measured on CodinGame with the built-in profiler (`#define PROF` at the top of the source;
`ticks/sim` are TSC ticks per simulated turn of all 8 snakes):

| Build | Simulated turns per 42 ms | Iterations | Falls | Selection | Move | Eval | Copy | Backup |
|---|---|---|---|---|---|---|---|---|
| v1.0 | 37k (30k–52k) | 5.2k | 47% | 31% | 13% | 5% | 2% | 2% |
| + support counters, `fastFalls`, SSE selection | 47k | 6.0k | 30% | 42% | 21% | 4% | 1% | 2% |

The same build varies by about ±20% between CodinGame games. Locally:
`SB_BENCH=3000 ./build/bot < tools/bench/pos11.txt` plays 100 turns of self-play at 3,000
iterations and prints simulated turns per second (about 1.0–1.4M on an M1, depending on
load); a build with `-DPROF` adds the split by phase.

For scale, from the post-mortems: frictionless (contest #1) reported about 50k turns per
50 ms; sZoom 4k–6k iterations per 45 ms. The "150k" and "250k simulations" quoted by
_H3R0_ and sZoom are probably counted per snake move, which would put this engine at about
380k; that reading is not confirmed.
