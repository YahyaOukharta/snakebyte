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
| `tools/kaggle_match.py push / status / fetch` | The same matches on Kaggle CPU kernels (x86), one pairing per kernel, two accounts (`SB_KP=2`); bot specs take parameters: `bot.cpp@SB_UCB_C=0.5` |
| `build/referee build/bot@SB_UCB_C=0.5 build/bot ...` | Parameters for one side only: any `PARAM` of `bot.cpp` as `SB_<name>` |
| `tools/league.py add / match / show` | Frozen versions (`league/src`), head-to-head with Elo ± 95%, log in `league/results.jsonl` |
| `tools/cg.py test SRC [OPPONENT] [--replay GAME_ID]` | A test game on CodinGame; the log goes to `arena/logs/` |
| `tools/cg.py submit SRC` | Submit to the arena, then mirror the code into the CodinGame IDE |
| `tools/cg.py progress / opponents / top N` | Arena state |
| `tools/arena.py fetch / stats` | Our arena battles per opponent |
| `SB_BENCH=3000 ./build/bot < tools/bench/pos11.txt` | Speed: simulated turns per second over 100 turns of self-play |

## Value net pipeline

```sh
./build/referee --gen 3000 600 100000 data/g0_0.bin 0.05      # self-play records: games, iterations, seed, file, random-move rate
python3 tools/kaggle_gen.py push g1 4 2500 5000000 2000 - 0.05  # the same on Kaggle: tag, kernels, games per process, seed, iterations, net
python3 tools/kaggle_gen.py fetch g1                            # -> data/g1_<kernel>_<process>.bin
for f in data/g1*.bin; do SB_KEEP=3 ./build/referee --export ${f%.bin}.f32 $f & done   # features + labels, one position in 3
python3 tools/train.py -o tools/nets/n1a.bin 'data/g1*.f32' --epochs 20 --lam 0.6
./build/referee --evalcheck tools/nets/n1a.bin data/g1_1_0.f32 1000 | diff - tools/nets/n1a.bin.check   # engine = PyTorch
./build/referee build/bot@SB_NET=tools/nets/n1a.bin build/bot@SB_NET=tools/nets/n0b.bin -g 50 -n 1000  # gate
python3 tools/pack.py -n tools/nets/n1a.bin -o out/sub.cpp      # submission with the net embedded (3 characters per weight)
```

A record file holds seed + moves + search scores, not positions: the game is rebuilt by the
referee's map generator and the engine, so the features can change without a new
generation. `tools/spsa.py` tunes all the `PARAM`s at once by self-play (log in
`league/spsa.jsonl`).

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

Fixed match lengths, decided before the match; no early stopping. Batches are 100 games
(`-g 50`): about ±65 Elo, two minutes at 1000 iterations on the laptop. Enough for large
effects; a small one needs several batches pooled. Depth and time questions are tested in
real time, not at 1000 iterations (history.md).

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
