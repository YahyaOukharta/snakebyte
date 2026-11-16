// SnakeByte (CodinGame Winter Challenge 2026) bot.
// Engine: exact port of the referee's turn (move, eat, behead, fall) on a padded 64x48 board.
// Search: Smitsimax (one UCT tree per snake, all snakes simulated together).
// Local tools (fixed iterations, parameters from the environment) are under #ifdef LOCAL.
#if defined(__x86_64__) && !defined(LOCAL)
#pragma GCC optimize("O3,inline,omit-frame-pointer,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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

// ───────────────────────────── state ─────────────────────────────
// A body is a ring buffer of cells; b[h] is the head, b[(h - len + 1) & 127] the tail.
struct Snake {
    uint16_t b[CAP];
    uint8_t h, len, alive, pad;
    inline int head() const { return b[h]; }
    inline int part(int i) const { return b[(h - i) & (CAP - 1)]; }   // 0 = head
    inline int tail() const { return b[(h - len + 1) & (CAP - 1)]; }
    inline int facing() const {                                          // direction index of head - neck
        int d = b[h] - b[(h - 1) & (CAP - 1)];
        return d == -STRIDE ? 0 : d == 1 ? 1 : d == STRIDE ? 2 : 3;
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
            if (s[i].alive) len[OWNER[i]] += s[i].len, alive[OWNER[i]]++;
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
                occ[k.tail()] = EMPTY;
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
                k.len++;
                occ[nh[i]] = SNAKE + i;
                if (eat[i]) napples--, len[o]++;
                continue;
            }
            if (occ[nh[i]] == APPLE) occ[nh[i]] = EMPTY, napples--;   // eaten by every head on it
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
        falls();
        turn++;
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
PARAM(UCB_C, 0.35);      // exploration constant
PARAM(EV_K, 6.0);        // value = tanh(raw / EV_K)
PARAM(EV_WIN, 4.0);      // bonus for a decided game
PARAM(EV_LOSS, 0.05);    // losses difference (the tie-break)
PARAM(EV_OWN, 0.25);     // apple closer to me than to the opponent
PARAM(EV_DIST, 0.03);    // distance of each snake to its nearest apple
PARAM(EV_ELEV, 0.01);    // height of the head
PARAM(EV_MEAN, 0.5);     // share of the mean length difference along the line (eat sooner)
PARAM(D_BASE, 3);        // depth = D_BASE + iterations / D_STEP1 until D_SW iterations ...
PARAM(D_STEP1, 250);
PARAM(D_SW, 1000);
PARAM(D_STEP2, 1000);    // ... then one more ply per D_STEP2 iterations
PARAM(D_MAX, 40);
PARAM(TIME_MS, 42);
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

// ───────────────────────────── evaluation ─────────────────────────────
// From player 0's (my) point of view, in (-1, 1).
static double evaluate(const State &S, double meanDiff) {
    double diff = S.len[0] - S.len[1];
    double raw = diff * (1 - EV_MEAN) + meanDiff * EV_MEAN + EV_LOSS * (S.losses[1] - S.losses[0]);
    if (S.terminal()) {
        double d = diff != 0 ? diff : (double)(S.losses[1] - S.losses[0]);
        raw = diff + (d > 0 ? EV_WIN : d < 0 ? -EV_WIN : 0);
        return tanh(raw / EV_K);
    }
    int best[MAXS];
    for (int i = 0; i < NS; i++) best[i] = 255;
    for (int a = 0; a < nApple0; a++) {
        if (S.occ[appleCell[a]] != APPLE) continue;
        const uint8_t *d = adist[a];
        int m0 = 255, m1 = 255;
        for (int i = 0; i < NS; i++) {
            if (!S.s[i].alive) continue;
            int v = d[S.s[i].head()];
            if (v < best[i]) best[i] = v;
            if (OWNER[i] == 0) { if (v < m0) m0 = v; }
            else if (v < m1) m1 = v;
        }
        if (m0 < m1) raw += EV_OWN;
        else if (m1 < m0) raw -= EV_OWN;
    }
    for (int i = 0; i < NS; i++) {
        if (!S.s[i].alive) continue;
        double v = -EV_DIST * min(best[i], 60) + EV_ELEV * (H - cy(S.s[i].head()));
        raw += OWNER[i] == 0 ? v : -v;
    }
    return tanh(raw / EV_K);
}

// ───────────────────────────── search (Smitsimax) ─────────────────────────────
struct Node {
    float w[4];
    int32_t n[4];
    int32_t ch[4];
    int32_t N;
};
enum { POOL = 1500000 };
static Node *pool;
static int poolN;
static float logTab[65536];
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

static inline int selectMove(Node &nd, int mask, float sign) {
    int unv = 0, cnt = 0;
    for (int d = 0; d < 4; d++)
        if ((mask >> d & 1) && nd.n[d] == 0) unv |= 1 << d, cnt++;
    if (cnt) {
        int r = rnd() % cnt;
        for (int d = 0; d < 4; d++)
            if (unv >> d & 1) { if (r-- == 0) return d; }
    }
    float lg = nd.N < 65536 ? logTab[nd.N] : logf((float)nd.N);
    float bestV = -1e9f;
    int best = 0;
    for (int d = 0; d < 4; d++) {
        if (!(mask >> d & 1)) continue;
        float q = sign * nd.w[d] / nd.n[d] + (float)UCB_C * sqrtf(lg / nd.n[d]);
        if (q > bestV) bestV = q, best = d;
    }
    return best;
}

static int lastIters, lastDepth;
static long fixedIters = 0;      // LOCAL: iterations per move instead of time

// Returns the chosen direction per snake in out[] (all snakes, opponents included).
static void search(const State &root, double budgetMs, int8_t *out) {
    auto t0 = chrono::steady_clock::now();
    poolN = 1;
    int roots[MAXS];
    for (int i = 0; i < NS; i++) roots[i] = root.s[i].alive ? newNode() : 0;
    static int pathNode[64][MAXS];
    static int8_t pathMove[64][MAXS];
    int iters = 0, depth = (int)D_BASE;
    int rootDiff = root.len[0] - root.len[1];
    for (;;) {
        if (fixedIters) { if (iters >= fixedIters) break; }
        else if ((iters & 15) == 0 &&
                 chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count() > budgetMs) break;
        if (poolN > POOL - 64 * MAXS) break;
        depth = iters < D_SW ? (int)(D_BASE + iters / D_STEP1) : (int)(D_BASE + D_SW / D_STEP1 + (iters - D_SW) / D_STEP2);
        if (depth > D_MAX) depth = (int)D_MAX;
        if (depth > 60) depth = 60;
        State S = root;
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
                m = selectMove(pool[nd], mask, OWNER[i] == 0 ? 1.f : -1.f);
                mv[i] = m;
                pathNode[d][i] = nd;
                pathMove[d][i] = m;
                if (!pool[nd].ch[m]) pool[nd].ch[m] = newNode();
                cur[i] = pool[nd].ch[m];
            }
            S.step(mv);
            sum += S.len[0] - S.len[1] - rootDiff;
        }
        float v = (float)evaluate(S, d ? rootDiff + sum / d : rootDiff);
        for (int e = 0; e < d; e++)
            for (int i = 0; i < NS; i++) {
                int nd = pathNode[e][i];
                if (!nd) continue;
                Node &x = pool[nd];
                int m = pathMove[e][i];
                x.w[m] += v, x.n[m]++, x.N++;
            }
        iters++;
    }
    for (int i = 0; i < NS; i++) {
        if (!root.s[i].alive) { out[i] = 0; continue; }
        Node &x = pool[roots[i]];
        int best = root.s[i].facing(), bn = -1;
        for (int d = 0; d < 4; d++)
            if (x.n[d] > bn) bn = x.n[d], best = d;
        out[i] = best;
    }
    lastIters = iters, lastDepth = depth;
}

static void initSearch() {
    pool = (Node *)malloc(sizeof(Node) * POOL);
    for (int i = 1; i < 65536; i++) logTab[i] = logf((float)i);
}

// ───────────────────────────── protocol ─────────────────────────────
#ifndef SB_NO_MAIN
static State cur, prevS;
static int8_t myMv[MAXS];
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
            for (int a = 0; a < nApple0; a++) bfsApple(a);
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
        int8_t mv[MAXS];
        search(cur, turnNo == 0 ? TIME_FIRST : TIME_MS, mv);
        string outp;
        for (int i = 0; i < NS; i++) {
            if (OWNER[i] != 0 || !cur.s[i].alive) continue;
            if (!outp.empty()) outp += ';';
            outp += to_string(CGID[i]) + ' ' + DNAME[mv[i]];
            if (!synced) outp += " DS", synced = true;       // marker readable in battle logs
        }
        if (outp.empty()) outp = "WAIT";
        double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        fprintf(stderr, "t%d it %d d %d nodes %d %.1fms desync %d L %d/%d\n", turnNo, lastIters, lastDepth, poolN, ms,
                desyncs, cur.losses[0], cur.losses[1]);
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
