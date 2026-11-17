# Roadmap

Written 2026-11-17, after v1.6 was submitted. Earlier roadmaps are in the git history.

## Where we are

- Rank 3 of 394 in Legend, 33.6–33.7 (v1.4b, v1.5), about 1.7 points clear of rank 4 and 4
  points behind sZoom and _H3R0_ (37.3 and 38.0).
- Against ranks 4–12 we score 60–65%. Against the top two 15–20%.
- What carries the bot (ablation, real time): the value net (−92 Elo without it), the tuned
  search and credit values (−70), per-snake credit, the reach tables.
- Since v1.4 every change has come out level: shared states in the tree, a net on fresh
  games, a 5% faster engine, twelve parameter gates, six behaviour fixes taken from replays
  of sZoom games. Self-play between two builds of ours has stopped telling anything, a
  20-game gauntlet against sZoom is noise, and an arena run costs three hours.

So the limits are two, and neither is in the code: **we cannot measure a change against the
players who beat us, and we do not know what they do differently.** The plan is built on
removing those two first.

## What we know about the gap

- Level through turn 8, then about 0.07 apples per turn behind from turn 10 to 30, with one
  body part more lost per game than they lose (189 + 43 arena games).
- In replayed games our snakes travelled in packs (same target as a team-mate 43% of the
  time against 26%), carried twice the snake-turns at length 3–4, and wasted a third of
  their turns at 7–10 moves from an apple. Each of these was fixed in a build; none of the
  builds did better against him. They are symptoms.
- Both top bots are the same kind of program as ours (Smitsimax, gravity-aware distances,
  Voronoi, a small evaluation). The difference is in what their evaluation and search
  prefer, which their write-ups do not say.

## Done on the first evening

- A1: exact replay from the seed works; 1,115 battles harvested (`harvest.py`).
- A4: the losses are classified (`losses.py`); the asymmetry against the top two is walls,
  i.e. snakes that were shut in.
- A3 for those positions: a ten times deeper search repeats 82% of the moves. The limit is
  the model of the opponent, not the depth.
- C3: board windows add nothing (85.39% → 85.53%). Dropped.
- A blunder set (`blunders.py`, 305 positions) and two root checks (`SAFE`, `SAFE_D`),
  which move it from 89% to 84–86% and cost Elo in self-play. Not shipped.

What this changes: **C1 moves to the front, in the form of an opponent model.** The search
has to expect what sZoom and _H3R0_ actually do with a snake next to ours. Their moves in
1,115 games are the training set for it; B1 (the paired gate against the real players) is
the only honest judge and has to exist before anything of this kind is shipped.

## Pillar A: every real game becomes data (day 1)

Found on 2026-11-17: the official map generator (`GridMaker`), seeded like the CodinGame SDK
(SecureRandom SHA1PRNG, `setSeed(seed)`), rebuilds the map of a logged game exactly from the
seed in its log. Java 17 and the SDK jars are on the laptop.

- [ ] **A1. Exact replay of any logged game.** `tools/mapdump/` (a 50-line Java program
  around the official generator) + `trace.py` taking the first position from the seed
  instead of a dump build. Check: the replay must agree with the score line our bot printed
  on every turn, for all 995 logged battles.
- [ ] **A2. Position set.** For every turn of every game: the position, the moves both
  players really made, the final result, who the opponent was. 995 battles now (68 against
  sZoom, 65 against _H3R0_, about 500 against ranks 3–9), 160 more with every arena run,
  up to 600 a day from test games against chosen players.
- [ ] **A3. Is it our search or our evaluation?** On the positions of the top two's snakes:
  the move our search picks at the real budget, the move it picks with twenty times the
  budget, and the move they played. If the deep search sides with them where the shallow
  one does not, the search is the limit (depth, priors). If the deep search disagrees with
  them too, the evaluation is, and their moves are the lesson.
- [ ] **A4. Where our body parts go.** Every part we lose in the 995 games, classified:
  wall, own body, enemy body, head-on, fall; and what the search thought the turn before.

## Pillar B: a gate against real opponents (day 2)

- [ ] **B1. Paired replay gate.** Take N arena games against the top five. The candidate and
  the live version each replay the same game (same map, seat and opponent) through
  CodinGame test games; compare the margins game by game. Pairing removes the map from the
  noise. About 250 games per version per day, unattended overnight.
- [ ] **B2. Opponent pool.** Searched on 2026-11-17: no top bot has public source. The
  strongest with code is a Legend #169 Rust bot; the rest are Gold and below (list in
  research.md). Too weak to gate against, useful only as a sanity check that a change does
  not lose to simple greedy play. The pool that matters is B1 (the real players) and B3.
- [ ] **B3. A stand-in for sZoom.** From A2, a net that predicts his move in a position.
  Played greedily or as the prior of a search, it is a local sparring partner with his
  habits.

## Pillar C: learn from the stronger players (days 2–4)

- [ ] **C1. Move prior.** A small net: a snake's surroundings and its reach distances per
  direction → which move a top player makes there. Trained on their real moves (tens of
  thousands of decisions against us), pre-trained on the moves of our own deep searches.
  Used inside the tree as the order of first visits and a bias that fades with visits, for
  our snakes and as the model of theirs. This is the only route by which strength we cannot
  design gets into the bot.
- [ ] **C2. Value on real games.** The value net fine-tuned on the results of games against
  strong opponents, mixed with self-play. Today it only knows how games end when we play
  ourselves.
- [ ] **C3. Board windows as net inputs.** A 9×9 window around each head, each (cell,
  content) with a learned vector, summed per snake. The go / no-go experiment (same records
  with and without the windows) is running; integrate only if the held-out loss drops
  clearly. Weights at 8 bits to fit the 100,000 characters.

## Pillar D: known faults (alongside, small)

- [ ] **D1. Stalling when behind near the end** (2% of arena games end at the turn limit
  unwon with enough apples left). The rule that fixed it cost 55 Elo by moving every value
  into the flat part of the squash. Needs a version that does not touch the scale: the turn
  limit as part of the terminal test, or the bonus measured from the root.
- [ ] **D2. Losses** follow from A4.
- [ ] **D3. Passengers.** Elevation 0.04, the live-snake term and the win bonus 8.7 change
  nothing either way (ablation); the last one is the cliff behind D1.

## Rules kept from this week

- One change, one real-time gate against the live version, before the next change goes
  into the default build. Batches of 50 for news, 200 or more for a decision.
- A fixed-iteration result at 3,000 says little about 7,000: four gains vanished that way.
- Self-play cannot see a fault both builds share. Replays of real games show what a change
  does to the behaviour; only games against real opponents show whether it wins.
- Exports are deleted once a net is trained; the game records are what is kept.

## Order and stop conditions

| Day | Work | Decision at the end |
|---|---|---|
| 1 | A1, A2, A3; read C3's experiment; v1.6's arena result; generation 4 | A3 says where the limit is: search → C1 first; evaluation → C2 / C3 first |
| 2 | B1 running overnight; C1 data and first net; A4 | move-prediction accuracy on held-out top-player moves: if it is no better than "toward the nearest apple", C1 stops |
| 3 | C1 in the tree; local gates, then B1 | submit if ahead in B1 |
| 4 | C2 or C3, D1 | |

Expected: unknown. Everything measurable in self-play has been taken; what is left is what
the stronger players know. If A3 shows that a deep search of ours already plays their
moves, the answer is search efficiency and C1 is the tool. If it does not, their moves and
results are the training signal and C1 / C2 carry it.
