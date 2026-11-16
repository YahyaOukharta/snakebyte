# Workflow

Rule: nothing is submitted unless it beats the live version in a gate match, and the arena
has the last word. The procedure is the one written up for the Cultist Wars bot
(`pipeline.md` there): cheap filters first, expensive truth last, one change per
measurement, negative results recorded.

## Tools

| Tool | Use |
|---|---|
| `build/referee A B -g 100 -j 4 [-n ITERS]` | Match between two binaries on random Legend maps, every map played with both seats. Without `-n` the bots use their own clock (real time); with it both run a fixed number of iterations per move. `-v` prints one JSON line per game |
| `build/referee --map SEED` / `--dump SEED` | Print a generated map / the first-turn input for it (benchmark positions in `tools/bench/`) |
| `tools/league.py add / match / show` | Frozen versions (`league/src`), head-to-head with Elo ± 95%, log in `league/results.jsonl` |
| `tools/cg.py test SRC [OPPONENT] [--replay GAME_ID]` | A test game on CodinGame; the log goes to `arena/logs/` |
| `tools/cg.py submit SRC` | Submit to the arena, then mirror the code into the CodinGame IDE |
| `tools/cg.py progress / opponents / top N` | Arena state |
| `tools/arena.py fetch / stats` | Our arena battles per opponent |
| `SB_BENCH=3000 ./build/bot < tools/bench/pos11.txt` | Speed: simulated turns per second over 100 turns of self-play |

## The gates, in order

1. **Exactness** (after any change to `State`). Fixed-iteration games of the new build and
   of a `-DSLOWFALL` reference build must be identical:
   ```sh
   c++ -O3 -std=c++17 -march=native -DLOCAL -DSLOWFALL -o build/bot_slow bot.cpp
   c++ -O3 -std=c++17 -march=native -DSLOWFALL -o build/referee_slow tools/referee.cpp
   ./build/referee_slow build/bot_slow build/bot_slow -n 400 -g 40 -j 8 -v | grep '^{' | sed 's/,"maxms.*//' | sort > a.txt
   ./build/referee      build/bot      build/bot      -n 400 -g 40 -j 8 -v | grep '^{' | sed 's/,"maxms.*//' | sort > b.txt
   diff a.txt b.txt          # must be empty
   ```
   Then on CodinGame: `tools/cg.py test bot.cpp <agent>` and `grep -c ' DS' arena/logs/test-*.log`
   must give 0, with `desync 0` on the last stderr line.
2. **Fixed iterations** against the live version: equal work, immune to machine load.
3. **Real time** against the live version: shows the cost of slower code. At most 4 games
   at once on the 8-core laptop.
4. **Real opponents**: test games against arena players (first seat only: in a test game
   our bot is always player 0).
5. **Arena run**, then record the result in [history.md](history.md) and freeze the version
   (`versions/vX.Y.cpp`, `tools/league.py add vX.Y`).

Fixed match lengths, decided before the match; no early stopping. 100 maps (200 games) give
about ±45 Elo.

## Local referee

`tools/referee.cpp` includes `bot.cpp` for the rules, so it is exactly as right as the
engine, which is checked against CodinGame separately (gate 1). Its map generator follows
`GridMaker.java` step by step (height skew, mirrored walls, air pockets filled, wall
thinning, sunk bottom island, apples, lone walls turned into apples, spawns) but with its
own random numbers: the maps have the referee's distribution, not its seeds. Rebuilding a
CodinGame game from its seed needs Java's `SHA1PRNG`; not done yet.

A bot that does not answer within 10 s or exits loses the game (`faults`). Slow turns
(over 50 ms, or 1000 ms on the first) are counted but not punished: under load the laptop
produces them for no fault of the bot.

## CodinGame specifics

- 50 ms per turn, 1000 ms for the first; the bot uses 42 / 850. Measured maximum in test
  games: 42.3 ms.
- No compiler flags: optimisation pragmas at the top of the file (x86 only).
- Test games: 10 per minute, 20 / 5 min, 30 / 15 min, 80 / hour, 150 / 3 h, 200 / 6 h,
  600 / day. Submits: 5 / 5 min, 20 / 30 min, 40 / hour, 100 / 5 h.
- An arena run in this game is slow: about 1% per two minutes.
- Battle logs have no stderr; the `DS` tag on a move is how a desync would show there.
- The legacy bot crashes in about two thirds of local games (`deduce_dir failed`, an
  uncaught exception in its path code), so matches against it overstate the gain; count
  the games it finishes.
