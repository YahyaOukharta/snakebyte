// SnakeByte (CodinGame Winter Challenge 2026) bot.
// Engine: exact port of the referee's turn (move, eat, behead, fall) on a padded 64x48 board.
// Search: Smitsimax (one UCT tree per snake, all snakes simulated together).
// Local tools (fixed iterations, parameters from the environment) are under #ifdef LOCAL.
#if defined(__x86_64__) && !defined(LOCAL)
#pragma GCC optimize("O3,inline,omit-frame-pointer,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt,sse4.1")
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#if defined(__x86_64__)
#include <immintrin.h>
#endif
using namespace std;

// ───────────────────────────── board geometry ─────────────────────────────
// The real grid is at most 44x24. Snakes may leave it (the outside is empty space, not wall),
// so the model adds a margin: 10 columns on each side, 8 rows above, 15+ below.
// Cell index = (y + OY) * 64 + (x + OX). The outer ring of the model is VOID.
enum { STRIDE = 64, ROWS = 48, CELLS = STRIDE * ROWS, OX = 10, OY = 8 };
enum : uint8_t { EMPTY = 0, APPLE = 1, WALL = 2, VOID = 3, SNAKE = 4 };   // SNAKE + index
enum { MAXS = 8, CAP = 128, MAXA = 128 };
static const int DELTA[4] = {-STRIDE, 1, STRIDE, -1};                      // UP RIGHT DOWN LEFT
static const char *DNAME[4] = {"UP", "RIGHT", "DOWN", "LEFT"};

static int W, H;                 // real grid size
static int NS;                   // snakes in the game (both players)
static int OWNER[MAXS];          // 0 = me, 1 = opponent
static int CGID[MAXS];           // id used by the referee
static int LIMY;                 // first model row where a snake is entirely out: (H + 1) + OY

static inline int cellOf(int x, int y) { return (y + OY) * STRIDE + x + OX; }
static inline int cx(int c) { return (c & 63) - OX; }
static inline int cy(int c) { return (c >> 6) - OY; }
static inline bool inModel(int x, int y) { return x > -OX && x < STRIDE - OX - 1 && y > -OY && y < ROWS - OY - 1; }

// ───────────────────────────── profiler (-DPROF) ─────────────────────────────
// Time per phase of the search loop, in CPU ticks (x86) or nanoseconds.
#ifdef PROF
#if defined(__x86_64__)
#include <x86intrin.h>
static inline uint64_t ticks() { return __rdtsc(); }
#else
static inline uint64_t ticks() { return chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()).count(); }
#endif
enum { P_COPY, P_SELECT, P_MOVE, P_FALL, P_EVAL, P_BACKUP, P_N };
static uint64_t prof[P_N], profT;
static const char *PNAME[P_N] = {"copy", "select", "move", "fall", "eval", "backup"};
#define PROF_START() (profT = ticks())
#define PROF_END(k) do { uint64_t t_ = ticks(); prof[k] += t_ - profT; profT = t_; } while (0)
#else
#define PROF_START()
#define PROF_END(k)
#endif

// ───────────────────────────── state ─────────────────────────────
// A body is a ring buffer of cells; b[h] is the head, b[(h - len + 1) & 127] the tail.
struct Snake {
    uint16_t b[CAP];
    uint8_t h, len, alive, sup;      // sup: parts resting on a wall or an apple
    uint8_t f;                       // facing: direction index of head - neck
    inline int head() const { return b[h]; }
    inline int part(int i) const { return b[(h - i) & (CAP - 1)]; }   // 0 = head
    inline int tail() const { return b[(h - len + 1) & (CAP - 1)]; }
    inline int facing() const { return f; }
    inline void setFacing() {                                            // from the body: head - neck
        int d = b[h] - b[(h - 1) & (CAP - 1)];
        f = d == -STRIDE ? 0 : d == 1 ? 1 : d == STRIDE ? 2 : 3;
    }
};

struct State {
    uint8_t occ[CELLS];
    Snake s[MAXS];
    int napples, turn;
    int losses[2];
    int len[2];                  // total body parts per player (alive snakes)
    int alive[2];                // alive snakes per player

    void recount() {
        len[0] = len[1] = alive[0] = alive[1] = 0;
        for (int i = 0; i < NS; i++)
            if (s[i].alive) len[OWNER[i]] += s[i].len, alive[OWNER[i]]++, resup(i), s[i].setFacing();
    }
    inline void resup(int i) {
        Snake &k = s[i];
        int n = 0;
        for (int j = 0; j < k.len; j++) {
            uint8_t v = occ[k.part(j) + STRIDE];
            n += v == APPLE || v == WALL;
        }
        k.sup = n;
    }
    // The apple at cell c is gone: the part above it, if any, loses its support.
    inline void unsupport(int c) {
        uint8_t v = occ[c - STRIDE];
        if (v >= SNAKE) s[v - SNAKE].sup--;
    }
    inline bool terminal() const { return napples == 0 || alive[0] == 0 || alive[1] == 0 || turn >= 200; }

    inline void kill(int i) {
        Snake &k = s[i];
        for (int j = 0; j < k.len; j++) occ[k.part(j)] = EMPTY;
        k.alive = 0;
        len[OWNER[i]] -= k.len;
        alive[OWNER[i]]--;
    }

    // One game turn, in the referee's order: moves, eats, beheadings, falls.
    // mv[i] = direction index for snake i (ignored for dead snakes).
    void step(const int8_t *mv) {
        int nh[MAXS];
        bool eat[MAXS], col[MAXS];
        // moves: the tail leaves its cell unless the head enters an apple
        for (int i = 0; i < NS; i++) {
            Snake &k = s[i];
            if (!k.alive) continue;
            nh[i] = k.head() + DELTA[mv[i]];
            eat[i] = occ[nh[i]] == APPLE && k.len < CAP - 1;
            if (!eat[i]) {
                int t = k.tail();
                uint8_t u = occ[t + STRIDE];
                k.sup -= u == APPLE || u == WALL;
                occ[t] = EMPTY;
                k.len--;
            }
        }
        // beheadings are decided on the position after all moves
        for (int i = 0; i < NS; i++) {
            if (!s[i].alive) continue;
            col[i] = occ[nh[i]] >= WALL;
            for (int j = 0; j < NS; j++)
                if (j != i && s[j].alive && nh[j] == nh[i]) col[i] = true;
        }
        for (int i = 0; i < NS; i++) {
            Snake &k = s[i];
            if (!k.alive) continue;
            int o = OWNER[i];
            if (!col[i]) {
                k.h = (k.h + 1) & (CAP - 1);
                k.b[k.h] = nh[i];
                k.f = mv[i];                                            // a beheaded snake keeps its old facing
                k.len++;
                occ[nh[i]] = SNAKE + i;
                if (eat[i]) napples--, len[o]++, unsupport(nh[i]);
                uint8_t u = occ[nh[i] + STRIDE];
                k.sup += u == APPLE || u == WALL;
                continue;
            }
            if (occ[nh[i]] == APPLE) occ[nh[i]] = EMPTY, napples--, unsupport(nh[i]);   // eaten by every head on it
            int size = k.len + 1;                                       // size with the new head
            if (size <= 3) {
                losses[o] += size;
                len[o] -= size - (eat[i] ? 1 : 0);                      // len[] still counts the size before the move
                for (int j = 0; j < k.len; j++) occ[k.part(j)] = EMPTY;
                k.alive = 0;
                alive[o]--;
            } else {
                losses[o]++;
                if (!eat[i]) len[o]--;
            }
        }
        PROF_END(P_MOVE);
        // every snake rests on a wall or an apple: nothing can fall
        unsigned air = 0;
        for (int i = 0; i < NS; i++)
            if (s[i].alive && !s[i].sup) air |= 1u << i;
#ifdef SLOWFALL                                              // reference build: the referee's loop, as written
        if (air) falls();
#else
        if (air) fastFalls(air);
#endif
        PROF_END(P_FALL);
        turn++;
    }

    // Falls, exact but without stepping row by row.
    // air = snakes with no part on a wall or an apple. Snakes in the air drop together, so
    // they never block each other: each one's drop is the distance to the first cell under
    // any of its parts that is not empty and not an airborne snake. A drop of 0 means it
    // rests on a grounded snake: it is grounded too, and the others are measured again.
    // Otherwise all drop by the smallest distance, the ones that land become ground, repeat.
    void fastFalls(unsigned air) {
        int d[MAXS], lim[MAXS];
        for (;;) {
            bool again = false;
            int m = 99;
            for (int i = 0; i < NS && !again; i++) {
                if (!(air >> i & 1)) continue;
                const Snake &k = s[i];
                const uint8_t self = SNAKE + i;
                int di = 99, top = ROWS, bottom = 0;
                for (int j = 0; j < k.len; j++) {
                    int c = k.part(j), n = 0, row = c >> 6;
                    if (row < top) top = row;
                    if (row > bottom) bottom = row;
                    if (occ[c + STRIDE] == self) continue;       // the part below shields this one
                    for (int q = c + STRIDE; n < di; q += STRIDE, n++) {
                        uint8_t v = occ[q];
                        if (v != EMPTY && !(v >= SNAKE && (air >> (v - SNAKE) & 1))) break;
                    }
                    if (n < di) di = n;
                    if (!di) break;
                }
                if (!di) { air &= ~(1u << i); again = true; break; }
                d[i] = di;
                lim[i] = min(LIMY - top, ROWS - 1 - bottom);     // rows until it is out of the game
                m = min(m, min(di, lim[i]));
            }
            if (again) continue;
            if (!air) return;
            for (int i = 0; i < NS; i++)
                if (air >> i & 1) {
                    const Snake &k = s[i];
                    for (int j = 0; j < k.len; j++) occ[k.part(j)] = EMPTY;
                }
            unsigned was = air;
            for (int i = 0; i < NS; i++) {
                if (!(was >> i & 1)) continue;
                Snake &k = s[i];
                if (lim[i] == m) {                               // out before anything stops it
                    k.alive = 0;
                    len[OWNER[i]] -= k.len;
                    alive[OWNER[i]]--;
                    air &= ~(1u << i);
                    continue;
                }
                const uint8_t self = SNAKE + i;
                int off = m * STRIDE;
                for (int j = 0; j < k.len; j++) {
                    uint16_t &c = k.b[(k.h - j) & (CAP - 1)];
                    c += off;
                    occ[c] = self;
                }
            }
            for (int i = 0; i < NS; i++)
                if ((air >> i & 1) && d[i] == m) resup(i), air &= ~(1u << i);
            if (!air) return;
        }
    }

    inline bool supported(int i, unsigned grounded) const {
        const Snake &k = s[i];
        for (int j = 0; j < k.len; j++) {
            uint8_t v = occ[k.part(j) + STRIDE];
            if (v == APPLE || v == WALL) return true;
            if (v >= SNAKE && v != SNAKE + i && (grounded >> (v - SNAKE) & 1)) return true;
        }
        return false;
    }

    // Snakes with no part above a wall, an apple or a grounded snake fall one row at a time, together.
    void falls() {
        unsigned grounded = 0, live = 0;
        for (int i = 0; i < NS; i++)
            if (s[i].alive) live |= 1u << i;
        for (;;) {
            bool changed = true;
            while (changed) {
                changed = false;
                for (int i = 0; i < NS; i++)
                    if ((live & ~grounded) >> i & 1)
                        if (supported(i, grounded)) grounded |= 1u << i, changed = true;
            }
            unsigned air = live & ~grounded;
            if (!air) return;
            for (int i = 0; i < NS; i++)
                if (air >> i & 1) {
                    Snake &k = s[i];
                    for (int j = 0; j < k.len; j++) occ[k.part(j)] = EMPTY;
                }
            for (int i = 0; i < NS; i++)
                if (air >> i & 1) {
                    Snake &k = s[i];
                    bool out = true, lost = false;
                    for (int j = 0; j < k.len; j++) {
                        uint16_t &c = k.b[(k.h - j) & (CAP - 1)];
                        c += STRIDE;
                        if ((c >> 6) < LIMY) out = false;
                        if ((c >> 6) >= ROWS - 1) lost = true;          // bottom of the model
                    }
                    if (out || lost) {
                        k.alive = 0;
                        live &= ~(1u << i);
                        len[OWNER[i]] -= k.len;
                        alive[OWNER[i]]--;
                    }
                }
            for (int i = 0; i < NS; i++)
                if ((air & live) >> i & 1) {
                    Snake &k = s[i];
                    for (int j = 0; j < k.len; j++) occ[k.part(j)] = SNAKE + i;
                }
            for (int i = 0; i < NS; i++)
                if ((air & live) >> i & 1) resup(i);
        }
    }
};

// ───────────────────────────── parameters ─────────────────────────────
#ifdef LOCAL
static double prm(const char *name, double def) {
    const char *e = getenv(name);
    return e ? atof(e) : def;
}
#define PARAM(name, def) static const double name = prm("SB_" #name, def)
#else
#define PARAM(name, def) static const double name = def
#endif
PARAM(UCB_C, 0.7);       // exploration constant
PARAM(EV_K, 6.0);        // value = tanh(raw / EV_K)
PARAM(EV_WIN, 4.0);      // bonus for a decided game
PARAM(EV_LOSS, 0.05);    // losses difference (the tie-break)
PARAM(EV_OWN, 0.25);     // apple closer to me than to the opponent
PARAM(EV_DIST, 0.03);    // distance of each snake to its nearest apple
PARAM(EV_ELEV, 0.01);    // height of the head
PARAM(EV_GRAV, 1);       // 1: gravity-aware apple distances, 0: plain BFS
PARAM(EV_S1, 0.08);      // nearest reachable apple: cost per cell for the first EV_N1 cells ...
PARAM(EV_N1, 8); 
PARAM(EV_S2, 0.015);     // ... and per cell for the next EV_N2. The total stays below one apple,
PARAM(EV_N2, 20);        // so eating the last apple in reach never lowers the evaluation
PARAM(EV_SELF, 1.0);     // per-snake credit: weight of the snake's own growth and apple distance in its own tree
PARAM(POL, 0);           // 1: an unvisited move is tried in order of distance to the nearest apple, 0: at random
PARAM(POL_EPS, 8);       // one first visit in POL_EPS ignores that order
PARAM(REUSE, 0);         // tree reuse: share of last turn's statistics kept under the moves played (0 = off)
PARAM(EV_GLOB, 1.0);     // weight of the shared value in a snake's own tree
PARAM(EV_OWNS, 0);       // credit to the snake that is nearest to an apple its side owns
PARAM(EV_MEAN, 0.5);     // share of the mean length difference along the line (eat sooner)
PARAM(D_BASE, 3);        // depth = D_BASE + iterations / D_STEP1 until D_SW iterations ...
PARAM(D_STEP1, 250);
PARAM(D_SW, 1000);
PARAM(D_STEP2, 1000);    // ... then one more ply per D_STEP2 iterations
PARAM(D_MAX, 40);
PARAM(TIME_MS, 44);      // per turn, counted from the first input line of the turn
PARAM(TIME_FIRST, 850);

// ───────────────────────────── apple distances ─────────────────────────────
// Plain BFS (no gravity) from every apple of the first turn through non-wall cells.
static uint8_t adist[MAXA][CELLS];
static int appleCell[MAXA], nApple0;
static uint8_t baseOcc[CELLS];   // walls and VOID only

static void bfsApple(int a) {
    static uint16_t q[CELLS];
    uint8_t *d = adist[a];
    memset(d, 255, CELLS);
    int qh = 0, qt = 0;
    q[qt++] = appleCell[a];
    d[appleCell[a]] = 0;
    while (qh < qt) {
        int c = q[qh++];
        if (d[c] >= 250) continue;
        for (int k = 0; k < 4; k++) {
            int n = c + DELTA[k];
            if (baseOcc[n] >= WALL || d[n] != 255) continue;
            d[n] = d[c] + 1;
            q[qt++] = n;
        }
    }
}

// Gravity-aware distances. After every turn some part of a snake rests on something, so
// its head is never more than (length - 1) cells of free path away from a supported cell
// (a free cell with a wall or an apple below). hsup[c] = that path distance for cell c.
// A snake of length b can only have its head on cells with hsup <= b - 1, so
// gdist[a][b-1][c] = BFS distance from c to apple a through such cells only, 255 if none.
enum { BMAX = 16 };
static uint8_t gdist[MAXA][BMAX][CELLS];
static uint8_t hsup[CELLS];

static void bfsSupport(const uint8_t *occ0) {
    static uint16_t q[CELLS];
    memset(hsup, 255, CELLS);
    int qh = 0, qt = 0;
    for (int c = STRIDE; c < CELLS - STRIDE; c++)
        if (baseOcc[c] < WALL && (baseOcc[c + STRIDE] == WALL || occ0[c + STRIDE] == APPLE)) hsup[c] = 0, q[qt++] = c;
    while (qh < qt) {
        int c = q[qh++];
        for (int k = 0; k < 4; k++) {
            int n = c + DELTA[k];
            if (baseOcc[n] >= WALL || hsup[n] != 255) continue;
            hsup[n] = hsup[c] + 1;
            q[qt++] = n;
        }
    }
}

static void bfsGravity(int a) {
    static uint16_t q[CELLS];
    for (int b = 1; b <= BMAX; b++) {
        uint8_t *d = gdist[a][b - 1];
        memset(d, 255, CELLS);
        if (hsup[appleCell[a]] > b - 1) continue;
        int qh = 0, qt = 0;
        q[qt++] = appleCell[a];
        d[appleCell[a]] = 0;
        while (qh < qt) {
            int c = q[qh++];
            if (d[c] >= 250) continue;
            for (int k = 0; k < 4; k++) {
                int n = c + DELTA[k];
                if (baseOcc[n] >= WALL || d[n] != 255 || hsup[n] > b - 1) continue;
                d[n] = d[c] + 1;
                q[qt++] = n;
            }
        }
    }
}

static inline int budget(const State &S, int i) { return S.s[i].len > BMAX ? BMAX : S.s[i].len; }

// ───────────────────────────── evaluation ─────────────────────────────
// From player 0's (my) point of view, in (-1, 1).
static inline float squash(double x) {              // cheap tanh
    x = x < -3 ? -3 : x > 3 ? 3 : x;
    return (float)(x * (27 + x * x) / (27 + 9 * x * x));
}
// own[i]: what snake i itself contributed (growth since the root, minus its apple distance cost),
// used to give each snake's tree a value that depends more on its own moves.
static double evaluate(const State &S, double meanDiff, const State &root, double *own) {
    double diff = S.len[0] - S.len[1];
    double raw = diff * (1 - EV_MEAN) + meanDiff * EV_MEAN + EV_LOSS * (S.losses[1] - S.losses[0]);
    if (S.terminal()) {
        double d = diff != 0 ? diff : (double)(S.losses[1] - S.losses[0]);
        raw = diff + (d > 0 ? EV_WIN : d < 0 ? -EV_WIN : 0);
        for (int i = 0; i < NS; i++) own[i] = S.s[i].alive ? S.s[i].len - root.s[i].len : -(double)root.s[i].len;
        return raw;
    }
    int best[MAXS], bud[MAXS], ownA[MAXS] = {0};
    const bool grav = EV_GRAV != 0;
    for (int i = 0; i < NS; i++) {
        best[i] = 255;
        if (grav && S.s[i].alive) bud[i] = budget(S, i) - 1;
    }
    for (int a = 0; a < nApple0; a++) {
        if (S.occ[appleCell[a]] != APPLE) continue;
        const uint8_t *d = adist[a];
        int m0 = 255, m1 = 255, i0 = 0, i1 = 0;
        for (int i = 0; i < NS; i++) {
            if (!S.s[i].alive) continue;
            int hd = S.s[i].head(), v = d[hd];
            if (grav) {
                v = gdist[a][bud[i]][hd];                    // 255: out of reach for this length
            }
            if (v < best[i]) best[i] = v;
            if (OWNER[i] == 0) { if (v < m0) m0 = v, i0 = i; }
            else if (v < m1) m1 = v, i1 = i;
        }
        if (m0 < m1) raw += EV_OWN, ownA[i0]++;             // (an apple nobody can reach counts for nobody)
        else if (m1 < m0) raw -= EV_OWN, ownA[i1]++;
    }
    for (int i = 0; i < NS; i++) {
        if (!S.s[i].alive) continue;
        double dc;
        if (grav) {
            int far = best[i] - (int)EV_N1;
            dc = best[i] <= EV_N1 ? EV_S1 * best[i] : EV_S1 * EV_N1 + EV_S2 * (far < EV_N2 ? far : EV_N2);
        } else dc = EV_DIST * min(best[i], 60);
        double v = -dc + EV_ELEV * (H - cy(S.s[i].head()));
        raw += OWNER[i] == 0 ? v : -v;
        own[i] = S.s[i].len - root.s[i].len - dc + EV_OWNS * ownA[i];
    }
    for (int i = 0; i < NS; i++)
        if (!S.s[i].alive) own[i] = -(double)root.s[i].len;
    return raw;
}

// ───────────────────────────── search (Smitsimax) ─────────────────────────────
struct Node {
    float w[4];      // sum of values per move (player 0's view)
    float n[4];      // visits per move (float: read straight into SSE)
    int32_t ch[4];   // child node per move, 0 = none
    int32_t N;       // visits of the node
};
enum { POOL = 1500000 };
static Node *pool, *pool2;       // pool2: last turn's trees, read while the kept part is copied over
static int poolN;
static int keptRoots[MAXS];      // root of each snake's tree after the last search
static bool haveTrees = false;
static float sqrtLog[65536];     // sqrt(ln N)
static uint64_t rngS = 0x9E3779B97F4A7C15ULL;
static inline uint32_t rnd() {
    rngS ^= rngS << 13, rngS ^= rngS >> 7, rngS ^= rngS << 17;
    return (uint32_t)(rngS >> 32);
}
static inline int newNode() {
    if (poolN >= POOL) return 0;
    memset(&pool[poolN], 0, sizeof(Node));
    return poolN++;
}

// Directions that do not run into a wall or a body (a tail cell counts as free). Never backwards.
static inline int validMask(const State &S, int i) {
    const Snake &k = S.s[i];
    int f = k.facing(), back = (f + 2) & 3, m = 0, hd = k.head();
    for (int d = 0; d < 4; d++) {
        if (d == back) continue;
        int t = hd + DELTA[d];
        uint8_t v = S.occ[t];
        if (v < WALL) m |= 1 << d;
        else if (v >= SNAKE && S.s[v - SNAKE].tail() == t) m |= 1 << d;
    }
    return m ? m : 1 << f;
}

// One UCB choice among the moves in mask, without branches on x86:
// q = sign * w / n + C * sqrt(ln N / n); an unvisited move gets a huge value with random
// low bits (so unvisited moves are tried first, in random order); a move outside the mask
// gets -1e9. The winner is the position of the maximum.
#if defined(__x86_64__)
static __m128 maskTab[16];
#endif
// nearF[i][c]: distance from cell c to the nearest apple still on the board that snake i
// can reach at its root length. Filled once per search; the order of first visits follows it.
static uint8_t nearF[MAXS][CELLS];

static inline int selectMove(const Node &nd, int mask, float sign, const uint8_t *F, int hd) {
    float e = (float)UCB_C * (nd.N < 65536 ? sqrtLog[nd.N] : sqrtf(logf((float)nd.N)));
    uint32_t r = rnd();
    // priority of an unvisited move: closer to an apple first, random among equals
    int pol = (r >> 24) % (int)POL_EPS != 0 ? 256 : 0;
    int p0 = (255 - F[hd - STRIDE]) * pol + (r & 255), p1 = (255 - F[hd + 1]) * pol + (r >> 8 & 255),
        p2 = (255 - F[hd + STRIDE]) * pol + (r >> 16 & 255), p3 = (255 - F[hd - 1]) * pol + (r >> 5 & 255);
#if defined(__x86_64__)
    __m128 n = _mm_loadu_ps(nd.n);
    __m128 q = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(_mm_loadu_ps(nd.w), _mm_set1_ps(sign)), _mm_rcp_ps(n)),
                          _mm_mul_ps(_mm_set1_ps(e), _mm_rsqrt_ps(n)));
    __m128 jit = _mm_add_ps(_mm_set1_ps(1e6f), _mm_cvtepi32_ps(_mm_set_epi32(p3, p2, p1, p0)));
    q = _mm_blendv_ps(q, jit, _mm_cmpeq_ps(n, _mm_setzero_ps()));
    q = _mm_blendv_ps(_mm_set1_ps(-1e9f), q, maskTab[mask]);
    __m128 m = _mm_max_ps(q, _mm_shuffle_ps(q, q, _MM_SHUFFLE(1, 0, 3, 2)));
    m = _mm_max_ps(m, _mm_shuffle_ps(m, m, _MM_SHUFFLE(2, 3, 0, 1)));
    return __builtin_ctz(_mm_movemask_ps(_mm_cmpeq_ps(q, m)));
#else
    const int pr[4] = {p0, p1, p2, p3};
    float bestV = -2e9f;
    int best = 0;
    for (int d = 0; d < 4; d++) {
        float q = !(mask >> d & 1) ? -1e9f
                  : nd.n[d] == 0 ? 1e6f + (float)pr[d]
                  : sign * nd.w[d] / nd.n[d] + e / sqrtf(nd.n[d]);
        if (q > bestV) bestV = q, best = d;
    }
    return best;
#endif
}

// Copies the subtree at old node o (in pool2) into pool, scaling its statistics by f.
static int copyTree(int o, float f) {
    int x = newNode();
    if (!x) return 0;
    const Node &src = pool2[o];
    float N = 0;
    for (int d = 0; d < 4; d++) {
        float n = floorf(src.n[d] * f);
        pool[x].n[d] = n;
        pool[x].w[d] = src.n[d] > 0 ? src.w[d] * (n / src.n[d]) : 0;
        N += n;
        if (n > 0 && src.ch[d]) {
            int c = copyTree(src.ch[d], f);
            pool[x].ch[d] = c;
        }
    }
    pool[x].N = (int32_t)N;
    return x;
}

static int lastIters, lastDepth, lastSims;
static long fixedIters = 0;      // LOCAL: iterations per move instead of time

// Returns the chosen direction per snake in out[] (all snakes, opponents included).
static void search(const State &root, double budgetMs, int8_t *out, const int8_t *played = nullptr) {
    auto t0 = chrono::steady_clock::now();
    int roots[MAXS];
    if (REUSE > 0 && played && haveTrees) {                  // keep what was searched under the moves played
        swap(pool, pool2);
        poolN = 1;
        for (int i = 0; i < NS; i++) {
            roots[i] = 0;
            if (!root.s[i].alive) continue;
            int o = keptRoots[i] ? pool2[keptRoots[i]].ch[played[i]] : 0;
            roots[i] = o ? copyTree(o, (float)REUSE) : 0;
            if (!roots[i]) roots[i] = newNode();
        }
    } else {
        poolN = 1;
        for (int i = 0; i < NS; i++) roots[i] = root.s[i].alive ? newNode() : 0;
    }
    static int pathNode[64][MAXS];
    static int8_t pathMove[64][MAXS];
    int iters = 0, depth = (int)D_BASE;
    int rootDiff = root.len[0] - root.len[1];
    int sims = 0;
    for (int i = 0; i < NS; i++) {
        if (!root.s[i].alive) continue;
        uint8_t *F = nearF[i];
        memset(F, POL != 0 ? 255 : 0, CELLS);
        if (POL == 0) continue;
        int b = budget(root, i) - 1;
        for (int a = 0; a < nApple0; a++) {
            if (root.occ[appleCell[a]] != APPLE) continue;
            const uint8_t *g = gdist[a][b];
            for (int c = 0; c < CELLS; c++) F[c] = g[c] < F[c] ? g[c] : F[c];
        }
    }
    for (;;) {
        if (fixedIters) { if (iters >= fixedIters) break; }
        else if ((iters & 15) == 0 &&
                 chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count() > budgetMs) break;
        if (poolN > POOL - 64 * MAXS) break;
        depth = iters < D_SW ? (int)(D_BASE + iters / D_STEP1) : (int)(D_BASE + D_SW / D_STEP1 + (iters - D_SW) / D_STEP2);
        if (depth > D_MAX) depth = (int)D_MAX;
        if (depth > 60) depth = 60;
        PROF_START();
        State S = root;
        PROF_END(P_COPY);
        int cur[MAXS];
        memcpy(cur, roots, sizeof cur);
        int d = 0;
        double sum = 0;
        for (; d < depth && !S.terminal(); d++) {
            int8_t mv[MAXS];
            for (int i = 0; i < NS; i++) {
                pathNode[d][i] = 0;
                if (!S.s[i].alive) continue;
                int mask = validMask(S, i);
                int nd = cur[i], m;
                if (!nd) {                                   // pool exhausted below this point
                    do m = rnd() & 3; while (!(mask >> m & 1));
                    mv[i] = m;
                    continue;
                }
                m = selectMove(pool[nd], mask, OWNER[i] == 0 ? 1.f : -1.f, nearF[i], S.s[i].head());
                mv[i] = m;
                pathNode[d][i] = nd;
                pathMove[d][i] = m;
                if (!pool[nd].ch[m]) pool[nd].ch[m] = newNode();
                cur[i] = pool[nd].ch[m];
            }
            PROF_END(P_SELECT);
            S.step(mv);
            sum += S.len[0] - S.len[1] - rootDiff;
        }
        double own[MAXS];
        double raw = evaluate(S, d ? rootDiff + sum / d : rootDiff, root, own);
        float val[MAXS];                                     // value for each snake's tree, player 0's view
        for (int i = 0; i < NS; i++)
            val[i] = squash((EV_GLOB * raw + (OWNER[i] == 0 ? EV_SELF : -EV_SELF) * own[i]) / EV_K);
        PROF_END(P_EVAL);
        for (int e = 0; e < d; e++)
            for (int i = 0; i < NS; i++) {
                int nd = pathNode[e][i];
                if (!nd) continue;
                Node &x = pool[nd];
                int m = pathMove[e][i];
                x.w[m] += val[i], x.n[m]++, x.N++;
            }
        PROF_END(P_BACKUP);
        iters++, sims += d;
    }
    for (int i = 0; i < NS; i++) {
        if (!root.s[i].alive) { out[i] = 0; continue; }
        Node &x = pool[roots[i]];
        int best = root.s[i].facing();
        float bn = -1;
        for (int d = 0; d < 4; d++)
            if (x.n[d] > bn) bn = x.n[d], best = d;
        out[i] = best;
    }
    lastIters = iters, lastDepth = depth, lastSims = sims;
    memcpy(keptRoots, roots, sizeof roots), haveTrees = true;
}

static void initSearch() {
    pool = (Node *)malloc(sizeof(Node) * POOL);
    pool2 = (Node *)malloc(sizeof(Node) * POOL);
    for (int i = 1; i < 65536; i++) sqrtLog[i] = sqrtf(logf((float)i));
#if defined(__x86_64__)
    for (int m = 0; m < 16; m++)
        maskTab[m] = _mm_castsi128_ps(_mm_set_epi32(m & 8 ? -1 : 0, m & 4 ? -1 : 0, m & 2 ? -1 : 0, m & 1 ? -1 : 0));
#endif
}

// ───────────────────────────── protocol ─────────────────────────────
#ifndef SB_NO_MAIN
static State cur, prevS;
static int8_t myMv[MAXS], playedMv[MAXS];   // playedMv: what every snake did last turn (from resync)
static bool havePrev = false;
static int desyncs = 0, turnNo = 0;

static bool sameState(const State &a, const State &b) {
    if (a.napples != b.napples) return false;
    for (int i = 0; i < NS; i++) {
        if (a.s[i].alive != b.s[i].alive) return false;
        if (!a.s[i].alive) continue;
        if (a.s[i].len != b.s[i].len) return false;
        for (int j = 0; j < a.s[i].len; j++)
            if (a.s[i].part(j) != b.s[i].part(j)) return false;
    }
    return true;
}

// Finds the opponent's moves of the last turn by trying every combination on the previous
// state; also carries the losses (not in the input). False = the simulator disagrees.
static bool resync() {
    int opp[MAXS], no = 0;
    for (int i = 0; i < NS; i++)
        if (OWNER[i] == 1 && prevS.s[i].alive) opp[no++] = i;
    int combos = 1;
    for (int i = 0; i < no; i++) combos *= 3;
    for (int c = 0; c < combos; c++) {
        State t = prevS;
        int8_t mv[MAXS];
        memcpy(mv, myMv, sizeof mv);
        int x = c;
        for (int i = 0; i < no; i++, x /= 3) {
            int f = prevS.s[opp[i]].facing();
            mv[opp[i]] = (f + 3 + x % 3) & 3;                // left, straight, right
        }
        t.step(mv);
        if (sameState(t, cur)) {
            cur.losses[0] = t.losses[0], cur.losses[1] = t.losses[1];
            memcpy(playedMv, mv, sizeof mv);
            return true;
        }
    }
    return false;
}

int main() {
    int myId;
    if (scanf("%d %d %d", &myId, &W, &H) != 3) return 0;
    auto t0 = chrono::steady_clock::now();
    LIMY = H + 1 + OY;
    memset(baseOcc, EMPTY, CELLS);
    for (int c = 0; c < CELLS; c++) {
        int x = c & 63, y = c >> 6;
        if (x == 0 || x == STRIDE - 1 || y == 0 || y == ROWS - 1) baseOcc[c] = VOID;
    }
    for (int y = 0; y < H; y++) {
        char row[128];
        scanf("%s", row);
        for (int x = 0; x < W; x++)
            if (row[x] == '#') baseOcc[cellOf(x, y)] = WALL;
    }
    int per;
    scanf("%d", &per);
    NS = per * 2;
    for (int i = 0; i < per; i++) scanf("%d", &CGID[i]), OWNER[i] = 0;
    for (int i = 0; i < per; i++) scanf("%d", &CGID[per + i]), OWNER[per + i] = 1;
    initSearch();
#ifdef LOCAL
    if (getenv("SB_ITERS")) fixedIters = atol(getenv("SB_ITERS"));
    if (getenv("SB_SEED")) rngS ^= strtoull(getenv("SB_SEED"), 0, 10) * 0x2545F4914F6CDD1DULL;
#endif
    cur.losses[0] = cur.losses[1] = 0;
    for (;;) {
        int na;
        if (scanf("%d", &na) != 1) break;
        if (turnNo) t0 = chrono::steady_clock::now();
        int l0 = cur.losses[0], l1 = cur.losses[1];
        memcpy(cur.occ, baseOcc, CELLS);
        cur.napples = 0;
        cur.turn = turnNo;
        cur.losses[0] = l0, cur.losses[1] = l1;
        static int ax[512], ay[512];
        for (int i = 0; i < na; i++) {
            scanf("%d %d", &ax[i], &ay[i]);
            if (!inModel(ax[i], ay[i])) continue;
            cur.occ[cellOf(ax[i], ay[i])] = APPLE;
            cur.napples++;
        }
        if (turnNo == 0) {
            nApple0 = 0;
            for (int i = 0; i < na && nApple0 < MAXA; i++)
                if (inModel(ax[i], ay[i])) appleCell[nApple0++] = cellOf(ax[i], ay[i]);
            bfsSupport(cur.occ);
            for (int a = 0; a < nApple0; a++) bfsApple(a), bfsGravity(a);
        }
        for (int i = 0; i < NS; i++) cur.s[i].alive = 0, cur.s[i].len = 0, cur.s[i].h = 0;
        int ns;
        scanf("%d", &ns);
        for (int q = 0; q < ns; q++) {
            int id;
            static char body[8192];
            scanf("%d %s", &id, body);
            int i = 0;
            while (i < NS && CGID[i] != id) i++;
            if (i == NS) continue;
            static int px[600], py[600];
            int n = 0;
            for (char *p = body; *p && n < 600;) {
                px[n] = strtol(p, &p, 10);
                p++;
                py[n] = strtol(p, &p, 10);
                n++;
                if (*p == ':') p++;
            }
            int keep = 0;                                    // parts inside the model, from the head
            while (keep < n && keep < CAP - 1 && inModel(px[keep], py[keep])) keep++;
            if (keep < 2) continue;                          // cannot be represented: left out
            Snake &k = cur.s[i];
            k.alive = 1, k.len = keep, k.h = keep - 1;
            for (int j = 0; j < keep; j++) {
                int c = cellOf(px[j], py[j]);
                k.b[keep - 1 - j] = c;
                cur.occ[c] = SNAKE + i;
            }
        }
        cur.recount();
        bool synced = true;
        if (havePrev) {
            synced = resync();
            if (!synced) desyncs++;
        }
#ifdef LOCAL
        // SB_BENCH=ITERS: self-play from this position at fixed iterations, report the speed
        if (getenv("SB_BENCH")) {
            fixedIters = atol(getenv("SB_BENCH"));
            long sims = 0, its = 0;
            double ms = 0;
            State b = cur;
#ifdef PROF
            memset(prof, 0, sizeof prof);
#endif
            while (!b.terminal() && b.turn < 100) {
                int8_t m[MAXS];
                auto t1 = chrono::steady_clock::now();
                search(b, 0, m);
                ms += chrono::duration<double, milli>(chrono::steady_clock::now() - t1).count();
                sims += lastSims, its += lastIters;
                b.step(m);
            }
            printf("bench: %d turns, %ld iterations, %ld sims, %.0f ms, %.0f sims/s, %.0f ns/sim, score %d-%d\n", b.turn, its, sims,
                   ms, sims / ms * 1000, ms * 1e6 / sims, b.len[0], b.len[1]);
#ifdef PROF
            uint64_t tot = 0;
            for (int k = 0; k < P_N; k++) tot += prof[k];
            for (int k = 0; k < P_N; k++) printf("  %-7s %5.1f%%  %6.1f ns/sim\n", PNAME[k], 100.0 * prof[k] / tot, (double)prof[k] / sims);
#endif
            return 0;
        }
#endif
        int8_t mv[MAXS];
#ifdef PROF
        memset(prof, 0, sizeof prof);            // drop what resync() added
#endif
        double used = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        search(cur, (turnNo == 0 ? TIME_FIRST : TIME_MS) - used, mv, havePrev && synced ? playedMv : nullptr);
        string outp;
        for (int i = 0; i < NS; i++) {
            if (OWNER[i] != 0 || !cur.s[i].alive) continue;
            if (!outp.empty()) outp += ';';
            outp += to_string(CGID[i]) + ' ' + DNAME[mv[i]];
            if (!synced) outp += " DS", synced = true;       // marker readable in battle logs
        }
        if (outp.empty()) outp = "WAIT";
        double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        fprintf(stderr, "t%d it %d d %d sims %d nodes %d %.1fms desync %d L %d/%d score %d-%d apples %d\n", turnNo, lastIters,
                lastDepth, lastSims, poolN, ms, desyncs, cur.losses[0], cur.losses[1], cur.len[0], cur.len[1], cur.napples);
#ifdef DUMP                                                  // board every DUMP turns: my snakes A-D (head) a-d, theirs W-Z w-z
        if (turnNo % DUMP == 0)
            for (int y = -2; y < H; y++) {
                char row[80];
                for (int x = -2; x < W + 2; x++) {
                    int c = cellOf(x, y), v = cur.occ[c];
                    char ch = v == EMPTY ? (x < 0 || x >= W || y < 0 ? ' ' : '.') : v == APPLE ? 'o' : v == WALL ? '#' : '?';
                    if (v >= SNAKE) {
                        int i = v - SNAKE, k = i % (NS / 2);
                        ch = (OWNER[i] ? 'w' : 'a') + k;
                        if (cur.s[i].head() == c) ch -= 32;
                    }
                    row[x + 2] = ch;
                }
                row[W + 4] = 0;
                fprintf(stderr, "%s\n", row);
            }
#endif
#ifdef PROF
        {
            uint64_t tot = 0;
            for (int k = 0; k < P_N; k++) tot += prof[k];
            fprintf(stderr, "prof");
            for (int k = 0; k < P_N; k++) fprintf(stderr, " %s %.1f%%", PNAME[k], 100.0 * prof[k] / (tot ? tot : 1));
            fprintf(stderr, " | ticks/sim %.0f state %zuB\n", lastSims ? (double)tot / lastSims : 0, sizeof(State));
            memset(prof, 0, sizeof prof);
        }
#endif
        puts(outp.c_str());
        fflush(stdout);
        // remember what was sent: a dead or missing snake keeps its facing in the referee
        prevS = cur;
        for (int i = 0; i < NS; i++) myMv[i] = cur.s[i].alive ? (OWNER[i] == 0 ? mv[i] : 0) : 0;
        havePrev = true;
        turnNo++;
    }
    return 0;
}
#endif
