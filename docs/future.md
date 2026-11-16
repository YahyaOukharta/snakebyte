# Roadmap

State: v1.0 is a correct engine, a plain Smitsimax and an untuned handcrafted evaluation.
Everything below is ordered by what the post-mortems say paid most ([research.md](research.md)).

## Phase 3: the known big wins

- [ ] **Gravity-aware distances.** BFS that tracks the longest unsupported run, precomputed
  on turn 1 per body length (_H3R0_: ~#55 → top 20; sZoom the same idea up to length 13).
  The current plain BFS calls an apple four rows up in open air "4 away".
- [ ] **Voronoi territory** at the leaves on bitboards (64-bit rows): apples and cells each
  side reaches first. The board already has a stride of 64 for this.
- [ ] **Tune the evaluation weights and UCB constant** on the local league (none is tuned).
- [ ] **Selection priors**: toward apples, supported cells, a tail about to move
  (frictionless, jacek).
- [ ] **State merging**: nodes keyed by (head cell, facing, turn parity) instead of move
  history (frictionless).
- [ ] **Per-snake credit**: blend each snake's backed-up value with its own size change (jacek).
- [ ] **Pairs**: snakes close together searched as one unit so they can climb on each other.

## Speed

- [ ] Selection is now the largest phase (42% on CodinGame): store the facing instead of
  deriving it, branch-free argmax, smaller nodes.
- [ ] Falls (30%): one pass instead of four over an airborne snake; a special case for the
  vertical climb.
- [ ] Copy only the rows of the board that are in use.
- [ ] Tree reuse between turns.

## Phase 4: learned evaluation

- [ ] Value net on per-snake and global features (sZoom, jacek), trained on self-play
  (Kaggle, two accounts), packed into the source.
- [ ] Self-play data generator under `#ifdef LOCAL`.

## Phase 5: the top two

- [ ] Port Java's `SHA1PRNG` so arena battles can be rebuilt from seed + moves.
- [ ] Our arena games against the top 10, searched again, as training data.
- [ ] Loss analysis against _H3R0_ and sZoom.

## Testing

- [ ] Per-bot parameters in the referee (`A@SB_UCB_C=0.5`) for sweeps.
- [ ] Real-time gates on Kaggle (x86, like CodinGame) instead of the laptop.
- [ ] A gauntlet tool: N test games against the players above us, results per opponent.
