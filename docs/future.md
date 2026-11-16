# Roadmap

State: v1.1 (rank 19 at 55% of its run). The handcrafted evaluation is on a plateau after
the per-snake credit; compute is worth about +60 Elo per tripling. Next in line: a learned
evaluation (phase 4) and engine speed.

## Phase 3: the known big wins

- [x] **Gravity-aware distances** (v1.1, a static reach model; see search.md). Open: other
  snakes and eaten apples as support; _H3R0_'s per-path model.
- [ ] **Voronoi territory** at the leaves on bitboards (64-bit rows): apples and cells each
  side reaches first. The board already has a stride of 64 for this.
- [x] Evaluation weights and UCB constant swept (v1.1): plateau.
- [ ] **Selection priors** as a bias inside UCB (frictionless, jacek). Ordering the first
  visits by apple distance alone did nothing.
- [ ] **State merging**: nodes keyed by (head cell, facing, turn parity) instead of move
  history (frictionless).
- [x] **Per-snake credit** (v1.1): +288 Elo.
- [ ] **Pairs**: snakes close together searched as one unit so they can climb on each other.

## Speed

- [ ] Selection is now the largest phase (42% on CodinGame): store the facing instead of
  deriving it, branch-free argmax, smaller nodes.
- [ ] Falls (30%): one pass instead of four over an airborne snake; a special case for the
  vertical climb.
- [ ] Copy only the rows of the board that are in use.
- [x] Tree reuse between turns: tried, no gain (history.md).

## Phase 4: learned evaluation

- [ ] Value net on per-snake and global features (sZoom, jacek), trained on self-play
  (Kaggle, two accounts), packed into the source.
- [ ] Self-play data generator under `#ifdef LOCAL`.

## Phase 5: the top two

- [ ] Port Java's `SHA1PRNG` so arena battles can be rebuilt from seed + moves.
- [ ] Our arena games against the top 10, searched again, as training data.
- [ ] Loss analysis against _H3R0_ and sZoom.

## Testing

- [x] Per-bot parameters in the referee (`A@SB_UCB_C=0.5`) and matches on Kaggle (`tools/kaggle_match.py`).
- [ ] A gauntlet tool: N test games against the players above us, results per opponent.
