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
// so the model adds a margin: 10 columns on each side, 20 rows above, 19+ below.
// Cell index = (y + OY) * 64 + (x + OX). The outer ring of the model is VOID.
enum { STRIDE = 64, ROWS = 64, CELLS = STRIDE * ROWS, OX = 10, OY = 20 };
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
PARAM(UCB_C, 0.735);       // exploration constant
PARAM(EV_K, 8.55);        // value = tanh(raw / EV_K)
PARAM(EV_WIN, 8.69);      // bonus for a decided game
PARAM(EV_LOSS, 0.057);    // losses difference (the tie-break)
PARAM(EV_OWN, 0.19);     // apple closer to me than to the opponent
PARAM(EV_ELEV, 0.04);    // height of the head
PARAM(EV_S1, 0.02);      // nearest reachable apple: cost per cell for the first EV_N1 cells ...
PARAM(EV_N1, 6); 
PARAM(EV_S2, 0.045);     // ... and per cell for the next EV_N2. The total stays below one apple,
PARAM(EV_N2, 20);        // so eating the last apple in reach never lowers the evaluation
PARAM(EV_SELF, 0.642);     // per-snake credit: weight of the snake's own growth and apple distance in its own tree
PARAM(POL, 0);           // 1: an unvisited move is tried in order of distance to the nearest apple, 0: at random
PARAM(POL_EPS, 8);       // one first visit in POL_EPS ignores that order
PARAM(REUSE, 0);         // tree reuse: share of last turn's statistics kept under the moves played (0 = off)
PARAM(EV_GLOB, 1.0);     // weight of the shared value in a snake's own tree
PARAM(EV_OWNS, 0);       // credit to the snake that is nearest to an apple its side owns
PARAM(NET_A, 2.18);       // with a net: raw = NET_MIX * handcrafted + NET_A * logit of the net
PARAM(NET_MIX, 1.0);
PARAM(EV_ALIVE, 1.39);      // per live snake more than the opponent
PARAM(EV_UG, 0.0065);         // a snake with no apple in reach: cost per cell to the nearest apple a longer snake could reach
PARAM(CENTER, 0);        // 1: values are measured from the root's evaluation, so the squash stays in its steep part
PARAM(THREAT, 0);        // root: 1 = my snakes do not enter a cell an enemy head can enter this turn (apple cells excepted), 2 = apple cells too
PARAM(EV_PASS, 0);        // own credit: cost per pass in the line (the head ends where it was: up and fall back)
PARAM(EV_TEMPO, 0);       // own credit: cost per cell of the snake's mean table distance to its nearest apple over the line (get there sooner)
PARAM(DECIDED, 1);        // 1: a lead larger than the apples left gets the win bonus before the end, so the loser gains nothing by not finishing
PARAM(WIN_SMOOTH, 0);     // share of the win bonus given before the end, as tanh(lead / WIN_T) x (apples eaten / apples at the start)^2: no cliff at the last apple
PARAM(WIN_T, 2);
PARAM(TBL_DROP, 0);      // reach tables: 1 = a snake that steps off at full stretch falls until its head is on a supported cell, 0 = it drops one row
PARAM(EV_SHORT, 0);      // bonus per part for a live snake longer than SHORT_L: short snakes reach little
PARAM(SHORT_L, 3);
PARAM(SHORT_N, 1);       // ... for each of the first SHORT_N parts above SHORT_L
PARAM(EV_PROX, 0);       // apple density: sum over the apples a side is nearest to of 1 / (1 + distance)
PARAM(EV_BANK, 0);       // per apple I reach at least BANK_M moves before the opponent (or alone): it is mine later anyway
PARAM(BANK_M, 6);
PARAM(EV_SOFT, 0);       // soft apple ownership: per apple, tanh((opponent's distance - mine) / SOFT_T)
PARAM(SOFT_T, 3);        // ... so that getting nearer to a contested apple pays more than to a safe one
PARAM(SOFT_CAP, 40);     // distance used for "cannot reach"
PARAM(EV_STUCK, 0);      // extra cost for a snake with no apple in reach: it will not grow again
PARAM(EV_VOR, 0.25);        // per apple more in my Voronoi territory than in the opponent's (flood from the heads, bodies block)
PARAM(EV_VORC, 0);       // per cell of territory more
PARAM(VOR_STEPS, 32);    // the flood stops after this many steps
PARAM(VOR_TAIL, 1);      // parts at the tail end that do not block (they will have moved)
PARAM(EV_MEAN, 0.5);     // share of the mean length difference along the line (eat sooner)
PARAM(D_BASE, 3);        // depth = D_BASE + iterations / D_STEP1 until D_SW iterations ...
PARAM(D_STEP1, 250);
PARAM(D_SW, 1000);
PARAM(D_STEP2, 1000);    // ... then one more ply per D_STEP2 iterations
PARAM(D_MAX, 40);
PARAM(TIME_MS, 38);      // per turn, counted from the first input line of the turn
PARAM(TIME_FIRST, 850);

// ───────────────────────────── apple distances ─────────────────────────────
static int appleCell[MAXA], nApple0;     // the apples of the first turn
static uint8_t baseOcc[CELLS];           // walls and VOID only

// Gravity-aware distances.
// The body of a snake lies on the last cells of its head's path, so the snake holds only
// while one of those cells is supported (a wall or an apple below it). Along a path, "run"
// counts the cells entered since the last supported one; a snake of length L holds while
// run <= L - 1. The search state is (cell, run):
//   step into a supported cell        -> run 0
//   step down                         -> run unchanged (hanging or falling: both go down)
//   step sideways or up               -> run + 1, allowed while <= L - 1
//   sideways at run L - 1             -> the snake drops one row: it ends on the cell below
//   step into the apple               -> always allowed: it is eaten before the fall, and the
//                                        new part keeps the tail where it was
// gd[L][run][cell][apple] = moves from (cell, run) to the apple for a snake of length L
// (3..12, longer snakes use 12), 255 if it cannot get there. Backward BFS from the apple.
// The apple index is the innermost one: a leaf asks for every apple from one (L, run,
// cell), and that way the answers sit next to each other in memory.
#ifndef LMAX_
#define LMAX_ 12
#endif
enum { LMIN = 3, LMAX = LMAX_, NL = LMAX - LMIN + 1 };
static uint8_t *gd;
static uint8_t supc[CELLS];      // cell is free and has a wall or a first-turn apple below
static inline uint8_t *GDROW(int l, int r, int c) { return gd + (((size_t)l * LMAX + r) * CELLS + c) * nApple0; }

static void bfsSupport(const uint8_t *occ0) {
    for (int c = 0; c < CELLS; c++)
#ifndef SUP_APPLES                                           // only walls hold: an apple is gone once it is eaten, the table is not
        supc[c] = c + STRIDE < CELLS && baseOcc[c] < WALL && baseOcc[c + STRIDE] == WALL;
#else
        supc[c] = c + STRIDE < CELLS && baseOcc[c] < WALL && (baseOcc[c + STRIDE] == WALL || occ0[c + STRIDE] == APPLE);
#endif
}

// Cells the tables cover: the grid, 3 columns beside it and 14 rows above it. Further out
// a snake is falling out of the game; searching the whole 64x64 model tripled the table time.
static uint8_t tblBlock[CELLS];
static void initTblBlock() {
    for (int c = 0; c < CELLS; c++) {
        int x = cx(c), y = cy(c);
        tblBlock[c] = baseOcc[c] >= WALL || x < -3 || x > W + 2 || y < -14 || y > H;
    }
}
static void bfsGravity(int a) {
    static uint32_t q[CELLS * LMAX];
    const int A = appleCell[a];
    for (int l = 0; l < NL; l++) {
        const int L = l + LMIN;
        uint8_t *base = GDROW(l, 0, 0) + a;          // entry (r, c) is base[(r * CELLS + c) * nApple0]
        const size_t st = nApple0;
#define D(i) base[(size_t)(i) * st]
        int qh = 0, qt = 0;
        for (int r = 0; r < L; r++) D(r * CELLS + A) = 0;
        for (int k = 0; k < 4; k++) {
            int c = A - DELTA[k];
            if (tblBlock[c]) continue;
            for (int r = 0; r < L; r++)
                if (D(r * CELLS + c) == 255) D(r * CELLS + c) = 1, q[qt++] = c << 5 | r;
        }
        while (qh < qt) {
            int n = q[qh] >> 5, r = q[qh] & 31;
            qh++;
            int d = D(r * CELLS + n);
            if (d >= 250) continue;
            bool sn = supc[n];
            if (sn && r) continue;                   // a supported cell is only ever entered with run 0
            auto relax = [&](int c, int rr) {
                uint8_t &x = D(rr * CELLS + c);
                if (x == 255) x = d + 1, q[qt++] = c << 5 | rr;
            };
            for (int k = 0; k < 4; k++) {            // predecessors c with the move c -> n in direction k
                int c = n - DELTA[k];
                if (tblBlock[c] || c == A) continue;
                if (sn) { for (int rr = 0; rr < L; rr++) relax(c, rr); }
                else if (k == 2) relax(c, r);
                else if (r >= 1) relax(c, r - 1);
            }
            if (TBL_DROP != 0) {                     // arrived here by falling after a sideways step: a fall goes on
                if (sn)                              // until the head is on a supported cell, however far that is
                    for (int m = n - STRIDE; !tblBlock[m] && !supc[m] && m != A; m -= STRIDE) {
                        if (!tblBlock[m - 1] && m - 1 != A) relax(m - 1, L - 1);
                        if (!tblBlock[m + 1] && m + 1 != A) relax(m + 1, L - 1);
                    }
            } else if (sn || r == L - 1) {           // old model: every sideways step at full stretch drops one row
                int m = n - STRIDE;
                if (!tblBlock[m] && !supc[m] && m != A) {
                    if (!tblBlock[m - 1] && m - 1 != A) relax(m - 1, L - 1);
                    if (!tblBlock[m + 1] && m + 1 != A) relax(m + 1, L - 1);
                }
            }
        }
#undef D
    }
}

// Where snake i is in the tables: length index and current run (parts in front of its first
// supported part; a snake resting only on other snakes counts as fully stretched).
static inline void reachIdx(const State &S, int i, int &l, int &r) {
    const Snake &k = S.s[i];
    int L = k.len < LMIN ? LMIN : k.len > LMAX ? LMAX : k.len;
    l = L - LMIN;
    int j = 0;
    for (; j < k.len && j < L - 1; j++) {
        uint8_t v = S.occ[k.part(j) + STRIDE];
        if (v == WALL || v == APPLE) break;
    }
    r = j;
}

static void initGrid();
static chrono::steady_clock::time_point tblStart;   // set by main: the tables must not eat the first turn
static bool tblGuard = false;
// All distance tables for a new game, from its first position.
static void initDistances(const State &S) {
    nApple0 = 0;
    for (int c = 0; c < CELLS && nApple0 < MAXA; c++)
        if (S.occ[c] == APPLE) appleCell[nApple0++] = c;
    bfsSupport(S.occ);
    initTblBlock();
    free(gd);
    gd = (uint8_t *)malloc((size_t)nApple0 * NL * LMAX * CELLS);
    memset(gd, 255, (size_t)nApple0 * NL * LMAX * CELLS);
    for (int a = 0; a < nApple0; a++) {
        // out of time on a slow machine: the remaining apples stay "out of reach"
        if (tblGuard && chrono::duration<double, milli>(chrono::steady_clock::now() - tblStart).count() > 700) break;
        bfsGravity(a);
    }
    initGrid();
}

// ───────────────────────────── Voronoi territory ─────────────────────────────
// Flood from all heads at once over the free cells of the grid, one step per turn, bodies
// and walls blocking; gravity is ignored. A cell belongs to the player who gets there
// first; a cell both reach in the same step belongs to nobody and stops both. Unlike the
// reach tables this sees the position of the bodies: an apple behind the opponent's body
// is not mine however near it is. The board is one 64-bit word per row.
static uint64_t GRID[ROWS];              // free cells of the real grid
static void initGrid() {
    memset(GRID, 0, sizeof GRID);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (baseOcc[cellOf(x, y)] < WALL) GRID[y + OY] |= 1ull << (x + OX);
}
struct Vor { int ap[2], cells[2]; };
static void voronoi(const State &S, Vor &v) {
    enum { R = 28 };
    const int n = H, tailSkip = (int)VOR_TAIL;
    uint64_t fr[R], bufA[2][R], bufB[2][R], oa[R], ob[R];
    for (int y = 0; y < n; y++) fr[y + 1] = GRID[y + OY];
    fr[0] = fr[n + 1] = 0;
    memset(bufA, 0, sizeof bufA), memset(bufB, 0, sizeof bufB);
    for (int i = 0; i < NS; i++) {
        const Snake &k = S.s[i];
        if (!k.alive) continue;
        for (int j = 0; j < k.len - tailSkip; j++) {
            int c = k.part(j), r = (c >> 6) - OY;
            if ((unsigned)r < (unsigned)n) fr[r + 1] &= ~(1ull << (c & 63));
        }
        int c = k.head(), r = (c >> 6) - OY;
        if ((unsigned)r < (unsigned)n) (OWNER[i] ? bufB[0] : bufA[0])[r + 1] |= 1ull << (c & 63);
    }
    memset(oa, 0, sizeof oa), memset(ob, 0, sizeof ob);
    int cur = 0;
    for (int step = 0, lim = (int)VOR_STEPS; step < lim; step++) {
        const uint64_t *A = bufA[cur], *B = bufB[cur];
        uint64_t *na = bufA[cur ^ 1], *nb = bufB[cur ^ 1];
        uint64_t any = 0;
        for (int y = 1; y <= n; y++) {
            uint64_t ea = (A[y] << 1 | A[y] >> 1 | A[y - 1] | A[y + 1]) & fr[y];
            uint64_t eb = (B[y] << 1 | B[y] >> 1 | B[y - 1] | B[y + 1]) & fr[y];
            uint64_t a = ea & ~eb, b = eb & ~ea;
            na[y] = a, nb[y] = b;
            oa[y] |= a, ob[y] |= b;
            fr[y] &= ~(ea | eb);
            any |= a | b;
        }
        if (!any) break;
        cur ^= 1;
    }
    v.ap[0] = v.ap[1] = v.cells[0] = v.cells[1] = 0;
    for (int a = 0; a < nApple0; a++) {
        int c = appleCell[a];
        if (S.occ[c] != APPLE) continue;
        int r = (c >> 6) - OY + 1;
        v.ap[0] += oa[r] >> (c & 63) & 1, v.ap[1] += ob[r] >> (c & 63) & 1;
    }
    if (EV_VORC != 0)
        for (int y = 1; y <= n; y++) v.cells[0] += __builtin_popcountll(oa[y]), v.cells[1] += __builtin_popcountll(ob[y]);
}

// ───────────────────────────── value net ─────────────────────────────
// Inputs: 16 numbers about the game and 7 per snake (4 slots per side, longest first), all
// from the point of view of player p. One hidden pair of layers, ReLU, one output: the
// logit of "p wins".
enum { NF_G = 19, NF_S = 8, NF = NF_G + 8 * NF_S, NH1 = 96, NH2 = 32 };

// One pass over apples x snakes: everything the evaluation and the net inputs need.
struct Scan {
    int d1[MAXS], d2[MAXS];      // distance of each snake to its nearest and second nearest reachable apple (255: none)
    int du[MAXS];                // distance to the nearest apple for the longest snake of the tables, run 0
    int own[MAXS];               // apples this snake is the nearest of all to
    int cnt[MAXS];               // apples this snake can reach
    int owned[2], contested, unreach;
    float prox[2];               // sum over owned apples of 1 / (1 + distance)
    int bank[2];                 // apples reached at least BANK_M moves before the opponent
    float soft;                  // sum over apples of softT[opponent's distance - mine] (player 0's view)
};
static float softT[129];             // tanh((margin - 64) / SOFT_T)
static struct SoftInit {
    SoftInit() { for (int i = 0; i < 129; i++) softT[i] = tanhf((i - 64) / (float)SOFT_T); }
} softInit;
static void scanApples(const State &S, Scan &c) {
    const uint8_t *row[MAXS], *rowU[MAXS];                   // each snake's distances to all apples
    for (int i = 0; i < NS; i++) {
        c.d1[i] = c.d2[i] = c.du[i] = 255, c.own[i] = c.cnt[i] = 0;
        if (!S.s[i].alive) continue;
        int l, r;
        reachIdx(S, i, l, r);
        row[i] = GDROW(l, r, S.s[i].head());
        rowU[i] = GDROW(NL - 1, 0, S.s[i].head());
    }
    c.owned[0] = c.owned[1] = c.contested = c.unreach = 0;
    c.prox[0] = c.prox[1] = 0;
    c.soft = 0;
    c.bank[0] = c.bank[1] = 0;
    const int bankM = (int)BANK_M;
    const int cap = (int)SOFT_CAP;
    for (int a = 0; a < nApple0; a++) {
        if (S.occ[appleCell[a]] != APPLE) continue;
        int m[2] = {255, 255}, im[2] = {0, 0};
        for (int i = 0; i < NS; i++) {
            if (!S.s[i].alive) continue;
            int v = row[i][a];
            c.cnt[i] += v != 255;
            if (v < c.d1[i]) c.d2[i] = c.d1[i], c.d1[i] = v;
            else if (v < c.d2[i]) c.d2[i] = v;
            int o = OWNER[i];
            if (v < m[o]) m[o] = v, im[o] = i;
        }
        if (EV_UG != 0)
            for (int i = 0; i < NS; i++)
                if (S.s[i].alive) {
                    int v = rowU[i][a];
                    if (v < c.du[i]) c.du[i] = v;
                }
        if (m[0] != 255 || m[1] != 255) c.soft += softT[64 + (m[1] < cap ? m[1] : cap) - (m[0] < cap ? m[0] : cap)];
        if (m[0] + bankM <= m[1]) c.bank[0]++;
        else if (m[1] + bankM <= m[0]) c.bank[1]++;
        if (m[0] == 255 && m[1] == 255) c.unreach++;
        else if (m[0] == m[1]) c.contested++;
        else {
            int o = m[0] < m[1] ? 0 : 1;
            c.owned[o]++, c.own[im[o]]++, c.prox[o] += 1.0f / (1 + m[o]);
        }
    }
}

static void fillFeatures(const State &S, int p, const Scan &c, const Vor &vr, float *f) {
    memset(f, 0, sizeof(float) * NF);
    const int *d1 = c.d1, *d2 = c.d2, *own = c.own, *owned = c.owned;
    const int contested = c.contested, unreach = c.unreach;
    const float *prox = c.prox;
    int q = 1 - p;
    f[0] = S.turn / 200.0f;
    f[1] = S.napples / 32.0f;
    f[2] = (S.len[p] - S.len[q]) / 16.0f;
    f[3] = S.len[p] / 32.0f, f[4] = S.len[q] / 32.0f;
    f[5] = S.losses[p] / 8.0f, f[6] = S.losses[q] / 8.0f;
    f[7] = S.alive[p] / 4.0f, f[8] = S.alive[q] / 4.0f;
    f[9] = owned[p] / 16.0f, f[10] = owned[q] / 16.0f;
    f[11] = contested / 16.0f, f[12] = unreach / 16.0f;
    f[13] = H / 24.0f;
    f[14] = prox[p] / 8.0f, f[15] = prox[q] / 8.0f;
    f[16] = vr.ap[p] / 16.0f, f[17] = vr.ap[q] / 16.0f;
    f[18] = (p ? -c.soft : c.soft) / 16.0f;
    int per = NS / 2;
    for (int side = 0; side < 2; side++) {
        int o = side == 0 ? p : q, idx[4], n = 0;
        for (int k = 0; k < per; k++)
            if (S.s[o * per + k].alive) idx[n++] = o * per + k;
        for (int a = 1; a < n; a++)                           // longest first
            for (int b = a; b > 0 && S.s[idx[b]].len > S.s[idx[b - 1]].len; b--) swap(idx[b], idx[b - 1]);
        for (int k = 0; k < n; k++) {
            float *g = f + NF_G + (side * 4 + k) * NF_S;
            int i = idx[k];
            g[0] = 1;
            g[1] = S.s[i].len / 16.0f;
            g[2] = (d1[i] > 32 ? 32 : d1[i]) / 32.0f;
            g[3] = (d2[i] > 32 ? 32 : d2[i]) / 32.0f;
            g[4] = d1[i] != 255;
            g[5] = (H - cy(S.s[i].head())) / 24.0f;
            g[6] = own[i] / 8.0f;
            g[7] = c.cnt[i] / 16.0f;
        }
    }
}

static void features(const State &S, int p, float *f) {
    Scan c;
    Vor v;
    scanApples(S, c);
    voronoi(S, v);
    fillFeatures(S, p, c, v, f);
}

static float NW1[NH1][NF], NB1[NH1], NW2[NH2][NH1], NB2[NH2], NW3[NH2], NB3;
static bool netLoaded = false;

// Dot products are written as eight running sums so that the compiler turns them into
// 8-wide vector code without -ffast-math (CodinGame compiles without flags).
static inline float dot8(const float *w, const float *x, int n) {
    float acc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int k = 0; k < n; k += 8)
        for (int t = 0; t < 8; t++) acc[t] += w[k + t] * x[k + t];
    return ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7]));
}
// Column form: h += x[k] * W[:, k]. Each inner loop is a plain "y += a * x" over a whole
// layer, which vectorises without -ffast-math, and an input that is zero (an empty snake
// slot, a unit switched off by the ReLU) costs nothing.
alignas(32) static float NW1T[NF][NH1], NW2T[NH1][NH2];
static void netReady() {
    for (int j = 0; j < NH1; j++)
        for (int k = 0; k < NF; k++) NW1T[k][j] = NW1[j][k];
    for (int j = 0; j < NH2; j++)
        for (int k = 0; k < NH1; k++) NW2T[k][j] = NW2[j][k];
    netLoaded = true;
}
static float netForward(const float *f) {
    alignas(32) float h1[NH1], h2[NH2];
    memcpy(h1, NB1, sizeof h1);
    for (int k = 0; k < NF; k++) {
        float x = f[k];
        if (x == 0) continue;
        const float *w = NW1T[k];
        for (int j = 0; j < NH1; j++) h1[j] += x * w[j];
    }
    memcpy(h2, NB2, sizeof h2);
    for (int k = 0; k < NH1; k++) {
        float x = h1[k];
        if (x <= 0) continue;
        const float *w = NW2T[k];
        for (int j = 0; j < NH2; j++) h2[j] += x * w[j];
    }
    for (int j = 0; j < NH2; j++) h2[j] = h2[j] > 0 ? h2[j] : 0;
    return NB3 + dot8(NW3, h2, NH2);
}
#ifdef NETW_DATA
// Weights written by tools/pack.py: every value is 16 bits (three characters, 6 bits each,
// from '0'; '{' and '|' stand for the backslash and the question mark), times the scale of
// its array.
static inline int dig(char c) { return c == '{' ? 44 : c == '|' ? 15 : c - 48; }
static void loadEmbedded() {
    static const char *p = NETW_DATA;
    static const float sc[6] = {NETW_SCALES};
    float *arr[6] = {&NW1[0][0], NB1, &NW2[0][0], NB2, NW3, &NB3};
    const int cnt[6] = {NH1 * NF, NH1, NH2 * NH1, NH2, NH2, 1};
    for (int a = 0; a < 6; a++)
        for (int i = 0; i < cnt[a]; i++, p += 3)
            arr[a][i] = ((dig(p[0]) << 12 | dig(p[1]) << 6 | dig(p[2])) - 32768) * sc[a];
    netReady();
}
#endif
#ifdef LOCAL
static void loadNet(const char *path) {                      // raw float32: W1 b1 W2 b2 W3 b3
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "no net %s\n", path); exit(1); }
    size_t n = fread(NW1, 4, NH1 * NF, fp) + fread(NB1, 4, NH1, fp) + fread(NW2, 4, NH2 * NH1, fp) + fread(NB2, 4, NH2, fp) +
               fread(NW3, 4, NH2, fp) + fread(&NB3, 4, 1, fp);
    fclose(fp);
    if (n != NH1 * NF + NH1 + NH2 * NH1 + NH2 + NH2 + 1) { fprintf(stderr, "bad net %s\n", path); exit(1); }
    netReady();
}
#endif

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
    if (WIN_SMOOTH != 0 && nApple0 > 0) {
        double ph = 1.0 - (double)S.napples / nApple0, d = diff != 0 ? diff : 0.5 * (S.losses[1] - S.losses[0]);
        raw += EV_WIN * WIN_SMOOTH * ph * ph * tanh(d / WIN_T);
    }
    if (DECIDED != 0 && (diff > S.napples || -diff > S.napples)) raw += diff > 0 ? EV_WIN : -EV_WIN;
    Scan c;
    scanApples(S, c);
    raw += EV_PROX * (c.prox[0] - c.prox[1]) + EV_BANK * (c.bank[0] - c.bank[1]) + EV_SOFT * c.soft + EV_OWN * (c.owned[0] - c.owned[1]) + EV_ALIVE * (S.alive[0] - S.alive[1]);              // an apple nobody can reach, or a tie, counts for nobody
    Vor v;
    const bool useNet = netLoaded && NET_A != 0;
    if (EV_VOR != 0 || EV_VORC != 0 || useNet) {
        voronoi(S, v);
        raw += EV_VOR * (v.ap[0] - v.ap[1]) + EV_VORC * (v.cells[0] - v.cells[1]);
    }
    for (int i = 0; i < NS; i++) {
        if (!S.s[i].alive) continue;
        int far = c.d1[i] - (int)EV_N1;
        double dc = c.d1[i] <= EV_N1 ? EV_S1 * c.d1[i] : EV_S1 * EV_N1 + EV_S2 * (far < EV_N2 ? far : EV_N2);
        if (c.d1[i] == 255 && EV_UG != 0) dc = EV_S1 * EV_N1 + EV_S2 * EV_N2 - EV_UG * (40 - (c.du[i] < 40 ? c.du[i] : 40));
        if (c.d1[i] == 255) dc += EV_STUCK;
        if (EV_SHORT != 0) {                                     // a bonus, so that dying does not get cheaper
            int extra = S.s[i].len - (int)SHORT_L;
            dc -= EV_SHORT * (extra < 0 ? 0 : extra > SHORT_N ? SHORT_N : extra);
        }
        double v = -dc + EV_ELEV * (H - cy(S.s[i].head()));
        raw += OWNER[i] == 0 ? v : -v;
        own[i] = S.s[i].len - root.s[i].len - dc + EV_OWNS * c.own[i];
    }
    for (int i = 0; i < NS; i++)
        if (!S.s[i].alive) own[i] = -(double)root.s[i].len;
    if (useNet) {                                            // the net's logit is added to the handcrafted value; own[] stays
        float f[NF];
        fillFeatures(S, 0, c, v, f);
        return NET_MIX * raw + NET_A * netForward(f);
    }
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
static uint8_t nearT[MAXS][CELLS];   // the same for EV_TEMPO, capped at 40, filled whenever it is on

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
static double lastScore;         // mean shared value of the last search, player 0's view, in (-1, 1)
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
    double scoreSum = 0, rootRaw = 0;
    if (CENTER != 0 && !root.terminal()) {
        double tmp[MAXS];
        rootRaw = evaluate(root, rootDiff, root, tmp);
    }
    for (int i = 0; i < NS; i++) {
        if (!root.s[i].alive) continue;
        uint8_t *F = nearF[i];
        memset(F, POL != 0 ? 255 : 0, CELLS);
        if (POL == 0) continue;
        int l, r;
        reachIdx(root, i, l, r);
        for (int a = 0; a < nApple0; a++) {
            if (root.occ[appleCell[a]] != APPLE) continue;
            for (int c = 0; c < CELLS; c++) {
                uint8_t g = GDROW(l, 0, c)[a];
                F[c] = g < F[c] ? g : F[c];
            }
        }
    }
    if (EV_TEMPO != 0)
        for (int i = 0; i < NS; i++) {
            if (!root.s[i].alive) continue;
            uint8_t *T = nearT[i];
            memset(T, 40, CELLS);
            int l, r;
            reachIdx(root, i, l, r);
            for (int a = 0; a < nApple0; a++) {
                if (root.occ[appleCell[a]] != APPLE) continue;
                for (int c = 0; c < CELLS; c++) {
                    uint8_t g = GDROW(l, 0, c)[a];
                    T[c] = g < T[c] ? g : T[c];
                }
            }
        }
    const bool tempo = EV_TEMPO != 0, passOn = EV_PASS != 0;
    int rootMask[MAXS];                                      // my snakes' moves at the root
    for (int i = 0; i < NS; i++) {
        if (!root.s[i].alive || OWNER[i] != 0) continue;
        int valid = validMask(root, i), safe = valid;
        if (THREAT != 0)
            for (int d = 0; d < 4; d++) {
                if (!(valid >> d & 1)) continue;
                int c = root.s[i].head() + DELTA[d];
                if (THREAT < 2 && root.occ[c] == APPLE) continue;
                for (int j = 0; j < NS; j++) {
                    if (!root.s[j].alive || OWNER[j] == 0) continue;
                    int e = c - root.s[j].head();
                    if ((e == 1 || e == -1 || e == STRIDE || e == -STRIDE) && c != root.s[j].part(1)) safe &= ~(1 << d);
                }
            }
        rootMask[i] = safe ? safe : valid;
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
        int sumT[MAXS] = {0, 0, 0, 0, 0, 0, 0, 0}, passes[MAXS] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (; d < depth && !S.terminal(); d++) {
            int8_t mv[MAXS];
            for (int i = 0; i < NS; i++) {
                pathNode[d][i] = 0;
                if (!S.s[i].alive) continue;
                int mask = d == 0 && OWNER[i] == 0 ? rootMask[i] : validMask(S, i);
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
            int hd0[MAXS], ln0[MAXS];
            if (passOn)
                for (int i = 0; i < NS; i++) hd0[i] = S.s[i].head(), ln0[i] = S.s[i].len;
            S.step(mv);
            if (passOn)
                for (int i = 0; i < NS; i++)
                    if (S.s[i].alive && S.s[i].head() == hd0[i] && S.s[i].len == ln0[i]) passes[i]++;
            sum += S.len[0] - S.len[1] - rootDiff;
            if (tempo)
                for (int i = 0; i < NS; i++)
                    if (S.s[i].alive) sumT[i] += nearT[i][S.s[i].head()];
        }
        double own[MAXS];
        double raw = evaluate(S, d ? rootDiff + sum / d : rootDiff, root, own);
        scoreSum += squash(raw / EV_K);
        if (passOn)
            for (int i = 0; i < NS; i++) own[i] -= EV_PASS * passes[i];
        if (tempo && d)
            for (int i = 0; i < NS; i++)
                if (S.s[i].alive) own[i] -= EV_TEMPO * sumT[i] / d;
        raw -= rootRaw;
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
    lastScore = iters ? scoreSum / iters : 0;
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
#ifdef NETW_DATA
    loadEmbedded();
#endif
#ifdef LOCAL
    if (getenv("SB_ITERS")) fixedIters = atol(getenv("SB_ITERS"));
    if (getenv("SB_SEED")) rngS ^= strtoull(getenv("SB_SEED"), 0, 10) * 0x2545F4914F6CDD1DULL;
    if (getenv("SB_NET")) loadNet(getenv("SB_NET"));
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
        if (turnNo == 0) {
            tblStart = t0, tblGuard = true;
            initDistances(cur);
            fprintf(stderr, "tables %.0f ms, %d apples, %dx%d\n", chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count(), nApple0, W, H);
        }
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
#ifdef DUMPQ                                                 // root statistics of my snakes: visits and mean value per move
        for (int i = 0; i < NS; i++) {
            if (OWNER[i] != 0 || !cur.s[i].alive) continue;
            const Node &x = pool[keptRoots[i]];
            fprintf(stderr, " Q snake %d len %d head %d,%d:", CGID[i], cur.s[i].len, cx(cur.s[i].head()), cy(cur.s[i].head()));
            for (int d = 0; d < 4; d++)
                if (x.n[d] > 0) fprintf(stderr, " %s %.0f %+.3f", DNAME[d], x.n[d], x.w[d] / x.n[d]);
            fprintf(stderr, "\n");
        }
#endif
#ifdef DUMP                                                  // board every DUMP turns, and whenever a snake died or the simulator disagreed
        static int liveBefore = -1;
        int liveNow = cur.alive[0] + cur.alive[1];
        bool event = (liveBefore >= 0 && liveNow < liveBefore) || strstr(outp.c_str(), " DS");
        liveBefore = liveNow;
        if (event) {
            fprintf(stderr, "EVENT prev turn moves:");
            for (int i = 0; i < NS; i++) fprintf(stderr, " %d:%s%s", CGID[i], prevS.s[i].alive ? DNAME[playedMv[i]] : "-", OWNER[i] ? "'" : "");
            fprintf(stderr, "\n");
            for (int i = 0; i < NS; i++) {
                if (!prevS.s[i].alive) continue;
                fprintf(stderr, " prev %d%s len %d:", CGID[i], OWNER[i] ? "'" : "", prevS.s[i].len);
                for (int j = 0; j < prevS.s[i].len && j < 12; j++) fprintf(stderr, " %d,%d", cx(prevS.s[i].part(j)), cy(prevS.s[i].part(j)));
                fprintf(stderr, "\n");
            }
        }
        if (turnNo % DUMP == 0 || event)
            for (int i = 0; i < NS; i++) {                   // nearest apple each snake can reach, and how far
                if (!cur.s[i].alive) continue;
                int bd = 255, ba = -1, l, r;
                reachIdx(cur, i, l, r);
                for (int a = 0; a < nApple0; a++)
                    if (cur.occ[appleCell[a]] == APPLE && GDROW(l, r, cur.s[i].head())[a] < bd) bd = GDROW(l, r, cur.s[i].head())[a], ba = a;
                fprintf(stderr, " snake %d%s len %d head %d,%d run %d -> apple %d,%d dist %d\n", CGID[i], OWNER[i] ? "'" : "", cur.s[i].len,
                        cx(cur.s[i].head()), cy(cur.s[i].head()), r, ba < 0 ? -1 : cx(appleCell[ba]), ba < 0 ? -1 : cy(appleCell[ba]), bd);
            }
        if (turnNo % DUMP == 0 || event)
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
