# SnakeByte bot

A bot for the [CodinGame SnakeByte arena](https://www.codingame.com/multiplayer/bot-programming/winter-challenge-2026-snakebyte)
(Winter Challenge 2026), in one C++ file: an exact port of the referee's turn on a padded
byte board, a Smitsimax search (one tree per snake) and a handcrafted evaluation with
gravity-aware distances and per-snake credit.

Started at rank 328 of 394 in Legend (17.91) with the March 2026 contest bot
(`league/src/legacy.cpp`). Current: v1.5, in its arena run (2026-11-17); v1.4b finished at **33.68, rank 3**. Per-version results and every measurement: [docs/history.md](docs/history.md).

| Stage | Arena score | Rank (Legend) |
|---|---|---|
| legacy: per-snake MCTS, depth 6 | 17.91 | 328 |
| v1.0: exact engine, Smitsimax, plain evaluation | 19.0 (at 54%) | ~254 |
| v1.1: per-snake credit, gravity-aware distances, faster engine | 30.49 | 11 |
| v1.2: path-based reach model, taller board model | 32.14 | 3 |
| v1.3: walls-only reach tables, Voronoi apple territory, value net, SPSA parameters | 33.30 | 3 |
| v1.4b: net on new inputs, distance cost flat near and steep far, 38 ms per turn | 33.68 | 3 |
| v1.5: state merging in the search, speed pass | running | |

The arena's first two (_H3R0_ 36.9, sZoom 36.8) are five points clear of the third (31.9).

## The game

Side view of a grid of platforms, 18–44 wide and 10–24 high, mirrored left to right. Each
player has 2–4 snakes of length 3. All snakes move at once, one cell per turn, and never
stop. Gravity: a snake with no part resting on a platform, an apple or another snake falls.
A head that hits a platform or a body is lost (the whole snake if it has 3 parts or
fewer). Eating an apple adds a part. The game ends when the apples are gone, a player has
no snake left, or after 200 turns; the most body parts wins. 50 ms per turn, 1000 ms for
the first. [Statement](docs/statement.md),
[official referee](https://github.com/CodinGame/WinterChallenge2026-Exotec).

## How it works

- **Engine.** One game turn exactly as `Game.java` plays it (moves, eats, beheadings,
  falls), on a 64 × 48 byte board with room around the grid, because snakes may leave it.
  Falls are computed without stepping row by row: support counters skip them when nothing
  can fall, and airborne snakes drop straight to their landing row. Each turn the bot
  replays every possible opponent move on the previous position and requires one to match
  the input: 0 mismatches in every checked game. → [docs/engine.md](docs/engine.md)
- **Search.** Smitsimax: every snake has its own UCT tree over its own moves, all trees
  are descended together in one shared simulation, 6k–8k iterations and 46k–73k simulated
  turns per 44 ms on CodinGame, depth 11–14. → [docs/search.md](docs/search.md)
- **Evaluation.** Length difference, apples each side reaches first, distance to the
  nearest apple the snake can actually reach given its length (most apples hang in the
  air), and a per-snake share of the value so that each tree learns from its own snake's
  moves. → [docs/search.md](docs/search.md#evaluation-evaluate)
- **Workflow.** Own native referee with the official map generator, a local league,
  100-game gate matches on the laptop and on Kaggle, test games and submits through the
  CodinGame API. Nothing ships without beating the previous version.
  → [docs/workflow.md](docs/workflow.md)

## What decided the result so far

1. **Per-snake credit.** With eight snakes sharing one value, a snake's tree mostly sees
   noise from the other seven. Backing up the shared value plus the snake's own growth and
   distance: +288 Elo in one change.
2. **Looking at boards.** Dumps of games against rank 40 showed our snakes sitting still
   under apples they could not reach, while the opponent ate 19 apples in 24 turns. That
   led to the reach model and to the rule that a distance cost must stay below one apple.
3. **Speed was not the bottleneck.** 27% more simulations: nothing measurable. Tripling the
   iterations: about +60 Elo.

## Quick start

Needs a C++17 compiler and Python 3.

```sh
make                                              # build/bot and build/referee
./build/referee --map 7                           # a generated map
./build/referee build/bot build/bot -g 8 -n 500   # 16 games of self-play at 500 iterations per move
python3 tools/league.py match build/bot v1.0 -g 50 -n 1000     # gate against a frozen version
./build/referee build/bot@SB_UCB_C=0.4 build/bot -g 50 -n 1000 # one parameter changed on one side
SB_BENCH=3000 ./build/bot < tools/bench/pos11.txt              # speed
python3 tools/cg.py test bot.cpp default          # a test game on CodinGame (token in ~/.config/codingame/token)
python3 tools/cg.py submit bot.cpp                # arena submit + IDE mirror
```

`bot.cpp` is the submission as it is: the local tools compile only with `-DLOCAL`.

## Layout

| Path | What |
|---|---|
| `bot.cpp` | Engine, search, evaluation, protocol; under `#ifdef LOCAL`: parameters from the environment, fixed iterations, benchmark. `-DPROF` adds the phase profiler, `-DDUMP=N` a board dump every N turns, `-DSLOWFALL` the reference fall code |
| `tools/referee.cpp` | Native referee (includes `bot.cpp` for the rules) with the official map generator; matches with both seats, Elo ± 95% |
| `tools/league.py` | Frozen versions, head-to-head matches, results log |
| `tools/kaggle_match.py` | The same matches on Kaggle CPU kernels, two accounts |
| `tools/cg.py`, `tools/arena.py` | CodinGame API: test games, submit + IDE mirror, progress, leaderboard; our arena battles |
| `tools/bench/` | First-turn positions for the speed benchmark |
| `versions/` | Every submitted source |
| `league/src/` | Frozen versions for the local league (`legacy.cpp` is the contest bot) |
| `docs/` | How each part works, what was measured, what to try next |

## Docs

1. [Engine](docs/engine.md): board, snakes, one turn, falls, exactness, speed
2. [Search](docs/search.md): Smitsimax, selection, evaluation, gravity distances, credit
3. [Workflow](docs/workflow.md): tools, gates, CodinGame specifics
4. [History](docs/history.md): versions, every measurement, what was learned
5. [Research](docs/research.md): what the top bots do, rules found in the referee source
6. [Roadmap](docs/future.md): open ideas
7. [Statement](docs/statement.md): the rules
