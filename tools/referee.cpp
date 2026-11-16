// Local referee for SnakeByte: plays two bot binaries against each other on random maps.
// Rules come from bot.cpp (the same engine the bot searches with; it is checked against
// CodinGame games by the bot's own resync test). The map generator follows GridMaker.java
// with its own random numbers: same distribution of maps, not the same map for a seed.
//
//   referee A B [-g MAPS] [-j JOBS] [-n ITERS] [-s SEED] [-l LEAGUE] [-v]
//   referee --map SEED          print one map
// Every map is played twice with the seats swapped. -n sets SB_ITERS for both bots
// (fixed iterations per move); without it the bots use their own clock.
#define SB_NO_MAIN
#ifndef LOCAL
#define LOCAL
#endif
#include "../bot.cpp"

#include <array>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

// ───────────────────────────── random ─────────────────────────────
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL) { next(), next(); }
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double dbl() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    int below(int n) { return (int)(next() % (uint64_t)n); }
    template <class T> void shuffle(vector<T> &v) {              // as Collections.shuffle
        for (int i = (int)v.size(); i > 1; i--) swap(v[i - 1], v[below(i)]);
    }
};

// ───────────────────────────── map generator (GridMaker.java) ─────────────────────────────
struct Map {
    int w, h;
    vector<uint8_t> wall;                    // y * w + x
    vector<int> apples;                      // cells
    vector<array<int, 3>> spawns;            // player 0 bodies, head first
    bool isWall(int x, int y) const { return x >= 0 && x < w && y >= 0 && y < h && wall[y * w + x]; }
    bool in(int x, int y) const { return x >= 0 && x < w && y >= 0 && y < h; }
    int opp(int c) const { return (c / w) * w + (w - 1 - c % w); }
};

static const int NX[4] = {0, 1, 0, -1}, NY[4] = {-1, 0, 1, 0};     // N E S W as in Grid.ADJACENCY

static Map makeMap(uint64_t seed, int league) {
    Rng r(seed);
    Map m;
    double skew = league == 1 ? 2 : league == 2 ? 1 : league == 3 ? 0.8 : 0.3;
    m.h = 10 + (int)floor(pow(r.dbl(), skew) * 14 + 0.5);
    m.w = (int)floor(m.h * 1.8f + 0.5f);
    if (m.w & 1) m.w++;
    int w = m.w, h = m.h;
    m.wall.assign(w * h, 0);
    double b = 5 + r.dbl() * 10;
    for (int x = 0; x < w; x++) m.wall[(h - 1) * w + x] = 1;
    for (int y = h - 2; y >= 0; y--) {
        double yNorm = (double)(h - 1 - y) / (h - 1), chance = 1 / (yNorm + 0.1) / b;
        for (int x = 0; x < w; x++)
            if (r.dbl() < chance) m.wall[y * w + x] = 1;
    }
    for (int c = 0; c < w * h; c++) m.wall[m.opp(c)] = m.wall[c];
    // air pockets smaller than 10 cells become wall
    {
        vector<char> seen(w * h, 0);
        for (int c = 0; c < w * h; c++) {
            if (m.wall[c] || seen[c]) continue;
            vector<int> comp{c};
            seen[c] = 1;
            for (size_t i = 0; i < comp.size(); i++)
                for (int k = 0; k < 4; k++) {
                    int x = comp[i] % w + NX[k], y = comp[i] / w + NY[k];
                    if (!m.in(x, y) || m.wall[y * w + x] || seen[y * w + x]) continue;
                    seen[y * w + x] = 1;
                    comp.push_back(y * w + x);
                }
            if (comp.size() < 10)
                for (int q : comp) m.wall[q] = 1;
        }
    }
    // a free cell with three or more wall neighbours loses one of them (not the one below)
    for (bool again = true; again;) {
        again = false;
        for (int c = 0; c < w * h; c++) {
            if (m.wall[c]) continue;
            vector<int> walls, destroyable;
            for (int k = 0; k < 4; k++) {
                int x = c % w + NX[k], y = c / w + NY[k];
                if (m.in(x, y) && m.wall[y * w + x]) {
                    walls.push_back(y * w + x);
                    if (y <= c / w) destroyable.push_back(y * w + x);
                }
            }
            if (walls.size() >= 3) {
                r.shuffle(destroyable);
                m.wall[destroyable[0]] = 0;
                m.wall[m.opp(destroyable[0])] = 0;
                again = true;
            }
        }
    }
    // sink the wall island that holds the bottom-left corner
    {
        vector<int> island;
        vector<char> seen(w * h, 0);
        int start = (h - 1) * w;
        if (m.wall[start]) {
            island.push_back(start);
            seen[start] = 1;
            for (size_t i = 0; i < island.size(); i++)
                for (int k = 0; k < 4; k++) {
                    int x = island[i] % w + NX[k], y = island[i] / w + NY[k];
                    if (!m.in(x, y) || !m.wall[y * w + x] || seen[y * w + x]) continue;
                    seen[y * w + x] = 1;
                    island.push_back(y * w + x);
                }
        }
        int lowerBy = 0;
        for (bool can = true; can;) {
            int y = h - 1 - (lowerBy + 1);
            for (int x = 0; x < w; x++)
                if (y < 0 || !seen[y * w + x]) { can = false; break; }
            if (can) lowerBy++;
        }
        if (lowerBy >= 2) lowerBy = 2 + r.below(lowerBy - 1);
        for (int c : island) m.wall[c] = 0, m.wall[m.opp(c)] = 0;
        for (int c : island) {
            int x = c % w, y = c / w + lowerBy;
            if (m.in(x, y)) m.wall[y * w + x] = 1, m.wall[m.opp(y * w + x)] = 1;
        }
    }
    auto hasApple = [&](int c) { return find(m.apples.begin(), m.apples.end(), c) != m.apples.end(); };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w / 2; x++)
            if (!m.wall[y * w + x] && r.dbl() < 0.025) m.apples.push_back(y * w + x), m.apples.push_back(m.opp(y * w + x));
    if (m.apples.size() < 8) {
        m.apples.clear();
        vector<int> freeT;
        for (int c = 0; c < w * h; c++)
            if (!m.wall[c]) freeT.push_back(c);
        r.shuffle(freeT);
        size_t want = max(4, (int)(0.025 * freeT.size()));
        size_t i = 0;
        vector<char> gone(w * h, 0);
        while (m.apples.size() < want * 2 && i < freeT.size()) {
            int c = freeT[i++];
            if (gone[c]) continue;
            m.apples.push_back(c), m.apples.push_back(m.opp(c));
            gone[m.opp(c)] = 1;
        }
    }
    // lone walls become apples
    for (int c = 0; c < w * h; c++) {
        if (!m.wall[c]) continue;
        int cnt = 0;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if ((dx || dy) && m.isWall(c % w + dx, c / w + dy)) cnt++;
        if (cnt == 0) {
            m.wall[c] = 0, m.wall[m.opp(c)] = 0;
            if (!hasApple(c)) m.apples.push_back(c);
            if (!hasApple(m.opp(c))) m.apples.push_back(m.opp(c));
        }
    }
    // spawns: three free cells above a wall, not next to another spawn or the centre line
    auto freeAbove = [&](int c) {
        vector<int> v;
        for (int i = 1; i <= 3; i++) {
            int x = c % w, y = c / w - i;
            if (m.in(x, y) && !m.wall[y * w + x]) v.push_back(y * w + x);
            else break;
        }
        return v;
    };
    vector<int> pot;
    for (int c = 0; c < w * h; c++)
        if (m.wall[c] && freeAbove(c).size() == 3) pot.push_back(c);
    r.shuffle(pot);
    int desired = 4 - (h <= 15) - (h <= 10);
    vector<char> isSpawn(w * h, 0);
    for (size_t i = 0; i < pot.size() && desired > 0; i++) {
        vector<int> loc = freeAbove(pot[i]);
        bool close = false;
        for (int c : loc) {
            int x = c % w, y = c / w;
            if (x == w / 2 - 1 || x == w / 2) close = true;
            for (int dy = -1; dy <= 1 && !close; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!(dx || dy) || !m.in(x + dx, y + dy)) continue;
                    int n = (y + dy) * w + x + dx;
                    if (isSpawn[n] || isSpawn[m.opp(n)]) close = true;
                }
            if (close) break;
        }
        if (close) continue;
        for (int c : loc) {
            isSpawn[c] = 1;
            for (int a : {c, m.opp(c)}) {
                auto it = find(m.apples.begin(), m.apples.end(), a);
                if (it != m.apples.end()) m.apples.erase(it);
            }
        }
        m.spawns.push_back({loc[2], loc[1], loc[0]});     // sorted by y: the top cell is the head
        desired--;
    }
    // a head between two walls: the left one is removed (on both sides)
    for (int p = 0; p < 2; p++)
        for (auto &sp : m.spawns) {
            int c = p ? m.opp(sp[0]) : sp[0];
            int x = c % w, y = c / w;
            if (m.isWall(x - 1, y) && m.isWall(x + 1, y)) m.wall[c - 1] = 0, m.wall[m.opp(c - 1)] = 0;
        }
    return m;
}

static void printMap(const Map &m) {
    vector<string> g(m.h, string(m.w, '.'));
    for (int c = 0; c < m.w * m.h; c++)
        if (m.wall[c]) g[c / m.w][c % m.w] = '#';
    for (int a : m.apples) g[a / m.w][a % m.w] = 'o';
    for (auto &sp : m.spawns)
        for (int j = 0; j < 3; j++) {
            g[sp[j] / m.w][sp[j] % m.w] = j ? 'a' : 'A';
            int o = m.opp(sp[j]);
            g[o / m.w][o % m.w] = j ? 'b' : 'B';
        }
    printf("%d x %d, %zu apples, %zu snakes per player\n", m.w, m.h, m.apples.size(), m.spawns.size());
    for (auto &row : g) puts(row.c_str());
}

// ───────────────────────────── bot processes ─────────────────────────────
struct Proc {
    pid_t pid = -1;
    int in = -1, out = -1;
    string buf;
    bool dead = false;
    double maxMs = 0;
    int slow = 0;

    void start(const char *path, long iters, uint64_t seed, bool showErr) {
        int a[2], b[2];
        if (pipe(a) || pipe(b)) exit(1);
        pid = fork();
        if (pid == 0) {
            dup2(a[0], 0), dup2(b[1], 1);
            close(a[0]), close(a[1]), close(b[0]), close(b[1]);
            if (!showErr) freopen("/dev/null", "w", stderr);
            if (iters) setenv("SB_ITERS", to_string(iters).c_str(), 1);
            setenv("SB_SEED", to_string(seed).c_str(), 1);
            execl(path, path, (char *)0);
            _exit(127);
        }
        close(a[0]), close(b[1]);
        in = a[1], out = b[0];
    }
    void send(const string &s) {
        if (dead) return;
        size_t off = 0;
        while (off < s.size()) {
            ssize_t n = write(in, s.data() + off, s.size() - off);
            if (n <= 0) { dead = true; return; }
            off += n;
        }
    }
    // One line, or "" when the bot is gone or silent for timeoutMs.
    string line(int timeoutMs) {
        for (;;) {
            size_t p = buf.find('\n');
            if (p != string::npos) {
                string l = buf.substr(0, p);
                buf.erase(0, p + 1);
                return l;
            }
            if (dead) return "";
            pollfd pf{out, POLLIN, 0};
            if (poll(&pf, 1, timeoutMs) <= 0) { dead = true; return ""; }
            char tmp[4096];
            ssize_t n = read(out, tmp, sizeof tmp);
            if (n <= 0) { dead = true; return ""; }
            buf.append(tmp, n);
        }
    }
    void stop() {
        if (in >= 0) close(in);
        if (out >= 0) close(out);
        if (pid > 0) kill(pid, SIGKILL), waitpid(pid, 0, 0);
    }
};

// ───────────────────────────── one game ─────────────────────────────
struct Result {
    int score[2], losses[2], turns;
    bool fault[2];
    double maxMs[2];
    int slow[2];
    int winner;          // 0, 1 or -1 (draw)
};

static Result playGame(const Map &m, const char *bot0, const char *bot1, long iters, uint64_t seed, bool verbose) {
    W = m.w, H = m.h, LIMY = H + 1 + OY;
    int per = (int)m.spawns.size();
    NS = per * 2;
    State S;
    memset(S.occ, EMPTY, CELLS);
    for (int c = 0; c < CELLS; c++) {
        int x = c & 63, y = c >> 6;
        if (x == 0 || x == STRIDE - 1 || y == 0 || y == ROWS - 1) S.occ[c] = VOID;
    }
    for (int c = 0; c < m.w * m.h; c++)
        if (m.wall[c]) S.occ[cellOf(c % m.w, c / m.w)] = WALL;
    S.napples = (int)m.apples.size();
    for (int a : m.apples) S.occ[cellOf(a % m.w, a / m.w)] = APPLE;
    for (int p = 0; p < 2; p++)
        for (int k = 0; k < per; k++) {
            int i = p * per + k;
            OWNER[i] = p, CGID[i] = i;
            Snake &sn = S.s[i];
            sn.alive = 1, sn.len = 3, sn.h = 2;
            for (int j = 0; j < 3; j++) {
                int c = p ? m.opp(m.spawns[k][j]) : m.spawns[k][j];
                sn.b[2 - j] = cellOf(c % m.w, c / m.w);
                S.occ[sn.b[2 - j]] = SNAKE + i;
            }
        }
    S.turn = 0, S.losses[0] = S.losses[1] = 0;
    S.recount();

    Proc pr[2];
    pr[0].start(bot0, iters, seed * 2 + 1, verbose);
    pr[1].start(bot1, iters, seed * 2 + 2, verbose);
    for (int p = 0; p < 2; p++) {
        string s = to_string(p) + "\n" + to_string(m.w) + "\n" + to_string(m.h) + "\n";
        for (int y = 0; y < m.h; y++) {
            for (int x = 0; x < m.w; x++) s += m.wall[y * m.w + x] ? '#' : '.';
            s += '\n';
        }
        s += to_string(per) + "\n";
        for (int k = 0; k < per; k++) s += to_string(p * per + k) + "\n";
        for (int k = 0; k < per; k++) s += to_string((1 - p) * per + k) + "\n";
        pr[p].send(s);
    }
    Result res{};
    while (!S.terminal()) {
        string frame;
        {
            string ap;
            int na = 0;
            for (int y = 0; y < m.h; y++)
                for (int x = 0; x < m.w; x++)
                    if (S.occ[cellOf(x, y)] == APPLE) ap += to_string(x) + " " + to_string(y) + "\n", na++;
            frame = to_string(na) + "\n" + ap;
            int live = 0;
            string sn;
            for (int i = 0; i < NS; i++) {
                if (!S.s[i].alive) continue;
                live++;
                sn += to_string(i) + " ";
                for (int j = 0; j < S.s[i].len; j++) {
                    int c = S.s[i].part(j);
                    if (j) sn += ':';
                    sn += to_string(cx(c)) + "," + to_string(cy(c));
                }
                sn += "\n";
            }
            frame += to_string(live) + "\n" + sn;
        }
        int8_t mv[MAXS];
        for (int i = 0; i < NS; i++) mv[i] = S.s[i].alive ? S.s[i].facing() : 0;
        auto t0 = chrono::steady_clock::now();
        pr[0].send(frame), pr[1].send(frame);
        string out[2];
        // both bots think at the same time; read in order, time each from the common start
        for (int p = 0; p < 2; p++) {
            out[p] = pr[p].line(10000);
            double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
            if (ms > pr[p].maxMs) pr[p].maxMs = ms;
            if (ms > (S.turn == 0 ? 1000 : 50)) pr[p].slow++;
        }
        if (pr[0].dead || pr[1].dead) break;
        for (int p = 0; p < 2; p++) {
            size_t pos = 0;
            while (pos < out[p].size()) {
                size_t e = out[p].find(';', pos);
                if (e == string::npos) e = out[p].size();
                string cmd = out[p].substr(pos, e - pos);
                pos = e + 1;
                int id;
                char dir[32];
                if (sscanf(cmd.c_str(), "%d %31s", &id, dir) != 2) continue;
                if (id < 0 || id >= NS || OWNER[id] != p || !S.s[id].alive) continue;
                int d = !strcmp(dir, "UP") ? 0 : !strcmp(dir, "RIGHT") ? 1 : !strcmp(dir, "DOWN") ? 2 : !strcmp(dir, "LEFT") ? 3 : -1;
                if (d < 0 || d == ((S.s[id].facing() + 2) & 3)) continue;     // backwards: ignored
                mv[id] = d;
            }
        }
        S.step(mv);
    }
    for (int p = 0; p < 2; p++) {
        res.fault[p] = pr[p].dead;
        res.maxMs[p] = pr[p].maxMs, res.slow[p] = pr[p].slow;
        res.score[p] = S.len[p], res.losses[p] = S.losses[p];
        pr[p].stop();
    }
    res.turns = S.turn;
    int a = res.score[0], b = res.score[1];
    if (a == b) a -= res.losses[0], b -= res.losses[1];
    res.winner = a > b ? 0 : b > a ? 1 : -1;
    if (res.fault[0] != res.fault[1]) res.winner = res.fault[0] ? 1 : 0;
    else if (res.fault[0]) res.winner = -1;
    return res;
}

// ───────────────────────────── match ─────────────────────────────
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    if (argc >= 3 && !strcmp(argv[1], "--map")) {
        printMap(makeMap(strtoull(argv[2], 0, 10), argc > 3 ? atoi(argv[3]) : 4));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: referee A B [-g MAPS] [-j JOBS] [-n ITERS] [-s SEED] [-l LEAGUE] [-v]\n");
        return 1;
    }
    const char *A = argv[1], *B = argv[2];
    int maps = 50, jobs = 4, league = 4;
    long iters = 0;
    uint64_t seed = 1;
    bool verbose = false;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-g")) maps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-j")) jobs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-n")) iters = atol(argv[++i]);
        else if (!strcmp(argv[i], "-s")) seed = strtoull(argv[++i], 0, 10);
        else if (!strcmp(argv[i], "-l")) league = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-v")) verbose = true;
    }
    int fd[2];
    if (pipe(fd)) return 1;
    for (int j = 0; j < jobs; j++) {
        if (fork() == 0) {
            close(fd[0]);
            for (int g = j; g < maps; g += jobs) {
                Map m = makeMap(seed + g, league);
                if (m.spawns.empty()) continue;
                for (int seat = 0; seat < 2; seat++) {
                    Result r = playGame(m, seat ? B : A, seat ? A : B, iters, (seed + g) * 2 + seat, false);
                    // reported from A's side
                    int a = seat, b = 1 - seat;
                    char line[256];
                    int n = snprintf(line, sizeof line, "%d %d %d %d %d %d %d %d %d %.0f %.0f %d %d %d %d %d\n", g, seat,
                                     r.winner < 0 ? -1 : (r.winner == a ? 0 : 1), r.score[a], r.score[b], r.turns,
                                     r.fault[a], r.fault[b], m.w, r.maxMs[a], r.maxMs[b], r.slow[a], r.slow[b], m.h,
                                     (int)m.spawns.size(), (int)m.apples.size());
                    write(fd[1], line, n);
                }
            }
            _exit(0);
        }
    }
    close(fd[1]);
    FILE *f = fdopen(fd[0], "r");
    int g, seat, win, sa, sb, turns, fa, fb, w, sla, slb, h, per, nap;
    double ma, mb;
    int n = 0, wins = 0, losses = 0, draws = 0, faultA = 0, faultB = 0, seat0wins = 0, decided = 0, slowA = 0, slowB = 0;
    double sum = 0, sum2 = 0, maxA = 0, maxB = 0;
    while (fscanf(f, "%d %d %d %d %d %d %d %d %d %lf %lf %d %d %d %d %d", &g, &seat, &win, &sa, &sb, &turns, &fa, &fb, &w,
                  &ma, &mb, &sla, &slb, &h, &per, &nap) == 16) {
        if (verbose)
            printf("{\"map\":%d,\"seat\":%d,\"result\":%d,\"score\":[%d,%d],\"turns\":%d,\"faults\":[%d,%d],\"size\":[%d,%d],\"snakes\":%d,\"apples\":%d,\"maxms\":[%.0f,%.0f]}\n",
                   g, seat, win, sa, sb, turns, fa, fb, w, h, per, nap, ma, mb);
        double sc = win == 0 ? 1 : win == 1 ? 0 : 0.5;
        n++, sum += sc, sum2 += sc * sc;
        wins += win == 0, losses += win == 1, draws += win < 0;
        faultA += fa, faultB += fb, slowA += sla, slowB += slb;
        maxA = max(maxA, ma), maxB = max(maxB, mb);
        if (win >= 0) decided++, seat0wins += (win == 0) == (seat == 0);
    }
    while (wait(0) > 0) {}
    if (!n) return 1;
    double p = sum / n, sd = sqrt(max(0.0, sum2 / n - p * p)), se = sd / sqrt((double)n);
    auto elo = [](double q) { q = min(max(q, 0.001), 0.999); return -400 * log10(1 / q - 1); };
    printf("%s vs %s: +%d -%d =%d (%.1f%%)  elo %+.0f +/- %.0f  first-seat wins %.0f%%  faults %d/%d  slow turns %d/%d  max ms %.0f/%.0f  [%d games%s]\n",
           A, B, wins, losses, draws, 100 * p, elo(p), (elo(p + 1.96 * se) - elo(p - 1.96 * se)) / 2,
           decided ? 100.0 * seat0wins / decided : 0, faultA, faultB, slowA, slowB, maxA, maxB, n,
           iters ? (", " + to_string(iters) + " iterations").c_str() : "");
    return 0;
}
