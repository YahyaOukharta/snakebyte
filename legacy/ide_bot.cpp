#pragma GCC optimize("O3,inline,omit-frame-pointer,unroll-loops",              \
                     "unsafe-math-optimizations", "no-trapping-math")
#pragma GCC option("arch=native", "tune=native", "no-zero-upper")
#pragma GCC target(                                                            \
    "sse,sse2,sse3,ssse3,sse4,mmx,avx,avx2,popcnt,rdrnd,abm,bmi2,fma")
#include <bitset>
#include <chrono>
#include <climits>
#include <cmath>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <vector>
using namespace std;
using namespace chrono;

#define MARGIN 15
#define MAX_WIDTH  (45 + MARGIN * 2)
#define MAX_HEIGHT (30 + MARGIN * 2)
#define BITSET_SIZE (MAX_WIDTH * MAX_HEIGHT)
#define N_CANDIDATES 4

enum DIR { UP, DOWN, LEFT, RIGHT };
string DIR_STR[4] = {"UP", "DOWN", "LEFT", "RIGHT"};

typedef pair<int, DIR> Move;

// ─────────────────────────────────────────────────────────────────────────────
//  Basic geometry
// ─────────────────────────────────────────────────────────────────────────────
struct Pos {
  int x, y;
  Pos apply_dir(DIR d) const {
    Pos p = *this;
    switch (d) {
      case UP:    p.y--; break;
      case DOWN:  p.y++; break;
      case LEFT:  p.x--; break;
      case RIGHT: p.x++; break;
    }
    return p;
  }
  bool operator==(const Pos &o) const { return x == o.x && y == o.y; }
  bool operator!=(const Pos &o) const { return !(*this == o); }
  string to_string() const {
    return "(" + std::to_string(x) + "," + std::to_string(y) + ")";
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Circular deque for snake body segments
// ─────────────────────────────────────────────────────────────────────────────
const int MAXN = 200;
struct Deque {
  Pos data[MAXN];
  int head = 0, tail = 0;
  void push_front(Pos v) { head = (head - 1 + MAXN) % MAXN; data[head] = v; }
  void push_back(Pos v)  { data[tail] = v; tail = (tail + 1) % MAXN; }
  void pop_front()       { head = (head + 1) % MAXN; }
  void pop_back()        { tail = (tail - 1 + MAXN) % MAXN; }
  Pos at(int i)  const   { return data[(head + i) % MAXN]; }
  Pos front()    const   { return data[head]; }
  Pos back()     const   { return data[(tail - 1 + MAXN) % MAXN]; }
  void clear()           { head = tail = 0; }
  bool empty()   const   { return head == tail; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Vec – small helper on top of vector<int>
// ─────────────────────────────────────────────────────────────────────────────
class Vec : public vector<int> {
public:
  Vec(int n)                  : vector<int>() { reserve(n); }
  Vec(const vector<int> &v)   : vector<int>(v) {}
  Vec()                       : vector<int>() {}
  bool has(int n) const { return std::find(begin(), end(), n) != end(); }
  void remove(int value) {
    for (size_t i = 0; i < size(); ++i)
      if (at(i) == value) { this->at(i) = back(); pop_back(); return; }
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Global bitset masks and distance table
// ─────────────────────────────────────────────────────────────────────────────
std::bitset<BITSET_SIZE> M_L, M_R, M_T, M_B;
std::bitset<BITSET_SIZE> target_bs, cur_bs, visits_bs;
std::bitset<BITSET_SIZE> obstacles;
std::bitset<BITSET_SIZE> walls;

static inline std::bitset<BITSET_SIZE> init_mask_l(int w, int h) {
  std::bitset<BITSET_SIZE> m;
  for (int i = 0; i < h; ++i) m.set(i * w);
  return m;
}
static inline std::bitset<BITSET_SIZE> init_mask_r(int w, int h) {
  std::bitset<BITSET_SIZE> m;
  for (int i = 0; i < h; ++i) m.set(i * w + w - 1);
  return m;
}
static inline std::bitset<BITSET_SIZE> init_mask_t(int w, int h) {
  std::bitset<BITSET_SIZE> m;
  for (int i = 0; i < w; ++i) m.set(i);
  return m;
}
static inline std::bitset<BITSET_SIZE> init_mask_b(int w, int h) {
  std::bitset<BITSET_SIZE> m;
  for (int i = 0; i < w; ++i) m.set(w * (h - 1 - MARGIN) + i);
  return m;
}

int dists[BITSET_SIZE][BITSET_SIZE];

void init_dists(const std::bitset<BITSET_SIZE> &obs, int w_oob, int h_oob) {
  for (int i = 0; i < BITSET_SIZE; i++)
    for (int j = 0; j < BITSET_SIZE; j++)
      dists[i][j] = -1;

  static int q[BITSET_SIZE];
  for (int i = 0; i < BITSET_SIZE; i++) {
    if (obs.test(i)) continue;
    int head = 0, tail = 0;
    q[tail++] = i;
    dists[i][i] = 0;
    while (head < tail) {
      int u = q[head++];
      int d = dists[i][u];
      int neighbors[4] = {u - w_oob, u + w_oob, u - 1, u + 1};
      bool at_edge[4] = {M_T.test(u), M_B.test(u), M_L.test(u), M_R.test(u)};
      for (int k = 0; k < 4; k++) {
        if (at_edge[k]) continue;
        int v = neighbors[k];
        if (v >= 0 && v < BITSET_SIZE && !obs.test(v) && dists[i][v] == -1) {
          dists[i][v] = d + 1;
          q[tail++] = v;
        }
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Snake
// ─────────────────────────────────────────────────────────────────────────────
struct Snake {
  Deque pieces;
  bitset<BITSET_SIZE> body;

  void apply_gravity(int w_oob) {
    body <<= w_oob;
    for (int i = pieces.head; i != pieces.tail; i = (i + 1) % MAXN)
      pieces.data[i].y += 1;
  }
  Pos head() const { return pieces.front(); }
  Pos tail() const { return pieces.back(); }
  bool dead() const { return pieces.empty(); }
  void kill()       { pieces.clear(); body = 0; }
};

vector<int> food_to_remove;

// ─────────────────────────────────────────────────────────────────────────────
//  State
// ─────────────────────────────────────────────────────────────────────────────
struct State {
  int w, h, w_oob, h_oob, n_snakes;
  Vec my_ids, opp_ids;
  Snake snakes[8];
  int max_food = -1;
  Vec food;

  State() {}
  State(int width, int height) {
    w = width; h = height;
    w_oob = w + MARGIN * 2;
    h_oob = h + MARGIN * 2;
  }

  // ── helpers ────────────────────────────────────────────────────────────────
  int cell_index(int x, int y) const { return (x + MARGIN) + (y + MARGIN) * w_oob; }
  int cell_index(Pos p)        const { return (p.x + MARGIN) + (p.y + MARGIN) * w_oob; }
  void set_bit_margin(bitset<BITSET_SIZE> &b, int x, int y) { b.set(cell_index(x, y)); }
  bool is_mine(int snake_id) const {
    return std::find(my_ids.begin(), my_ids.end(), snake_id) != my_ids.end();
  }

  bitset<BITSET_SIZE> solid() const {
    bitset<BITSET_SIZE> s = walls;
    for (int i = 0; i < n_snakes; i++) {
      if (!snakes[i].dead()) {
        s |= snakes[i].body;
        s.reset(cell_index(snakes[i].tail()));
      }
    }
    return s;
  }

  vector<DIR> get_moves(int snake_id) const {
    vector<DIR> moves;
    auto sol = solid();
    if (!snakes[snake_id].dead()) {
      for (int d = 0; d < 4; d++) {
        Pos nh = snakes[snake_id].head().apply_dir((DIR)d);
        if (!sol.test(cell_index(nh))) moves.push_back((DIR)d);
      }
    }
    return moves;
  }

  DIR deduce_dir(int source_cell, int target_cell) const {
    int delta = target_cell - source_cell;
    if (delta == 1)       return RIGHT;
    if (delta == -1)      return LEFT;
    if (delta == w_oob)   return DOWN;
    if (delta == -w_oob)  return UP;
    throw runtime_error("deduce_dir failed");
  }

  DIR deduce_dir(int snake_id) {
    int dx = snakes[snake_id].head().x - snakes[snake_id].pieces.at(1).x;
    int dy = snakes[snake_id].head().y - snakes[snake_id].pieces.at(1).y;
    if (dx ==  1) return RIGHT;
    if (dx == -1) return LEFT;
    if (dy ==  1) return DOWN;
    if (dy == -1) return UP;
    throw runtime_error("couldnt deduce dir");
  }

  void init_snake(int snake_id, vector<Pos> &parts) {
    snakes[snake_id].pieces = Deque();
    snakes[snake_id].body   = 0;
    for (const auto &p : parts) {
      snakes[snake_id].pieces.push_back(p);
      set_bit_margin(snakes[snake_id].body, p.x, p.y);
    }
  }

  // ── simulation step ────────────────────────────────────────────────────────
  void step(vector<Move> moves) {
    food_to_remove.clear();
    obstacles = walls;

    set<int> cells_with_new_heads;
    int meeting_heads[MAX_WIDTH * MAX_HEIGHT] = {0};

    for (auto &move : moves) {
      int sid = move.first;
      if (snakes[sid].dead()) continue;
      DIR dir = move.second;

      Pos new_head = snakes[sid].head().apply_dir(dir);
      if (new_head == snakes[sid].pieces.at(1)) {
        dir = deduce_dir(sid);
        new_head = snakes[sid].head().apply_dir(dir);
      }
      snakes[sid].pieces.push_front(new_head);
      int nhc = cell_index(new_head);
      snakes[sid].body.set(nhc);
      cells_with_new_heads.insert(nhc);
      meeting_heads[nhc] |= (1 << sid);

      if (!food.has(nhc)) {
        if (snakes[sid].tail() != new_head)
          snakes[sid].body.reset(cell_index(snakes[sid].tail()));
        snakes[sid].pieces.pop_back();
      } else {
        food_to_remove.push_back(nhc);
      }
    }

    // head collisions
    for (auto &cell : cells_with_new_heads) {
      int mask = meeting_heads[cell];
      if (mask & (mask - 1)) {
        while (mask) {
          int sid = __builtin_ctz(mask); mask &= (mask - 1);
          snakes[sid].body.reset(cell);
          if (snakes[sid].body.count() < 3) snakes[sid].kill();
          else snakes[sid].pieces.pop_front();
        }
      }
    }

    // wall/body collisions
    for (int i = 0; i < n_snakes; i++) {
      obstacles = walls;
      if (snakes[i].dead()) continue;
      for (int j = 0; j < n_snakes; j++)
        if (i != j && !snakes[j].dead()) obstacles |= snakes[j].body;
      if (obstacles.test(cell_index(snakes[i].head()))) {
        snakes[i].body &= ~obstacles;
        snakes[i].pieces.pop_front();
        if (snakes[i].body.count() < 3) snakes[i].kill();
      }
    }

    for (int v : food_to_remove) food.remove(v);

    // gravity
    obstacles = walls;
    for (int v : food) obstacles.set(v);

    Vec supported(n_snakes);
    for (int i = 0; i < n_snakes; i++) {
      if (!snakes[i].dead()) {
        if (((snakes[i].body << w_oob) & obstacles).any()) {
          supported.push_back(i);
          obstacles |= snakes[i].body;
        }
      } else {
        supported.push_back(i);
      }
    }
    while ((int)supported.size() < n_snakes) {
      for (int i = 0; i < n_snakes; i++) {
        if (!snakes[i].dead() && !supported.has(i)) {
          if (((snakes[i].body << w_oob) & obstacles).any()) {
            supported.push_back(i);
            obstacles |= snakes[i].body;
          } else if (snakes[i].body.none()) {
            snakes[i].kill(); supported.push_back(i);
          } else {
            snakes[i].apply_gravity(w_oob);
          }
        }
      }
    }
  }

  // ── evaluation ─────────────────────────────────────────────────────────────
  float food_control_score() const {
    struct FI { int my_d, op_d, delta; };
    vector<FI> infos;
    infos.reserve(food.size());
    for (int f : food) {
      int my_min = INT_MAX, op_min = INT_MAX;
      for (int i = 0; i < n_snakes; i++) {
        if (snakes[i].dead()) continue;
        int d = dists[cell_index(snakes[i].head())][f];
        if (d < 0) continue;
        if (is_mine(i)) my_min = min(my_min, d);
        else            op_min = min(op_min, d);
      }
      int me = (my_min == INT_MAX) ? 20 : my_min;
      int op = (op_min == INT_MAX) ? 20 : op_min;
      infos.push_back({me, op, op - me});
    }
    sort(infos.begin(), infos.end(),
         [](const FI &a, const FI &b){ return a.delta > b.delta; });
    float score = 0;
    int n = min((int)infos.size(), 4);
    for (int i = 0; i < n; i++) {
      const auto &fi = infos[i];
      if (fi.delta >= 0) { score += fi.delta; score += max(0, 10 - fi.my_d); }
      else               { score += (float)fi.delta / 2.0f; }
    }
    return score;
  }

  void voronoi(int voro[2]) const {
    bitset<BITSET_SIZE> cur[2] = {}, owned[2] = {}, vis = walls;
    voro[0] = voro[1] = 0;
    for (int i = 0; i < n_snakes; i++)
      if (!snakes[i].dead()) vis |= snakes[i].body;
    for (int i = 0; i < n_snakes; i++) {
      if (!snakes[i].dead()) {
        int hc = cell_index(snakes[i].head());
        int g = is_mine(i) ? 0 : 1;
        cur[g].set(hc); owned[g].set(hc); vis.set(hc);
      }
    }
    bool skip[2] = {!cur[0].any(), !cur[1].any()};
    while (true) {
      for (int i = 0; i < 2; i++) {
        if (skip[i]) continue;
        cur[i] = (cur[i] & ~M_T) >> w_oob | (cur[i] & ~M_B) << w_oob |
                 (cur[i] & ~M_L) >> 1       | (cur[i] & ~M_R) << 1;
        cur[i] &= ~vis;
      }
      if (!skip[0]) owned[0] |= cur[0] & ~cur[1];
      if (!skip[1]) owned[1] |= cur[1] & ~cur[0];
      skip[0] = cur[0].none(); skip[1] = cur[1].none();
      if (skip[0] && skip[1]) break;
      vis |= cur[0] | cur[1];
    }
    voro[0] = owned[0].count();
    voro[1] = owned[1].count();
  }

  float eval() const {
    int my_parts = 0, op_parts = 0, my_alive = 0, op_alive = 0;
    for (int i = 0; i < n_snakes; i++) {
      if (snakes[i].dead()) continue;
      if (is_mine(i)) { my_parts += snakes[i].body.count(); my_alive++; }
      else            { op_parts += snakes[i].body.count(); op_alive++; }
    }
    if (my_alive == 0 && op_alive == 0) return 0.5f;
    if (my_alive == 0) return 0.0f;
    if (op_alive == 0) return 1.0f;
    if (food.empty()) {
      if (my_parts > op_parts) return 1.0f;
      if (my_parts < op_parts) return 0.0f;
      return 0.5f;
    }
    float base   = (max_food > 0) ? (float)max_food * 2.0f
                                  : (float)(my_parts + op_parts + 1);
    float size_s = (float)(my_parts - op_parts) / base;
    int nf       = (int)food.size();
    float fc_raw = food_control_score();
    float fc_s   = nf > 0 ? fc_raw / (float)(nf * 12.0f) : 0.0f;
    fc_s         = max(-1.0f, min(1.0f, fc_s));
    int st       = my_alive + op_alive;
    float cnt_s  = st > 0 ? (float)(my_alive - op_alive) / (float)st : 0.0f;
    float raw    = size_s * 0.60f + fc_s * 0.30f + cnt_s * 0.10f;
    return max(0.01f, min(0.99f, 0.5f + 0.5f * raw));
  }

  float eval(int snake_id) const {
    return is_mine(snake_id) ? eval() : 1.0f - eval();
  }
vector<DIR> get_moves(int snake_id, const bitset<BITSET_SIZE> &sol) const {
  vector<DIR> moves;
  if (!snakes[snake_id].dead()) {
    for (int d = 0; d < 4; d++) {
      Pos nh = snakes[snake_id].head().apply_dir((DIR)d);
      if (!sol.test(cell_index(nh))) moves.push_back((DIR)d);
    }
  }
  return moves;
}
  // ── A* path finding ────────────────────────────────────────────────────────
  struct PathResult {
    vector<DIR> path;
    int fruits_eaten = 0;
  };
PathResult path_to_cell(int snake_id, int target_cell, bool stable = false,
                               int max_depth = 40) const
{
    struct Node
    {
        int f;
        int fruits;
        int g;
        int parent;   // index of parent in pool, -1 for root
        DIR move;     // move taken from parent to reach this node
        State state;
    };

    if (snakes[snake_id].dead())
        return {};

    const int start_head = cell_index(snakes[snake_id].head());
    if (start_head == target_cell)
        return {};

    const int h0 = dists[start_head][target_cell];
    if (h0 < 0)
        return {};

    if (h0 == 1 && !stable)
    {
        DIR d = deduce_dir(start_head, target_cell);
        return {{d}, 0};
    }

    // Exact same semantics, just tighter pruning:
    // - stable: must reach target cell
    // - non-stable: we accept reaching a cell adjacent to target
    const int target_limit = max_depth + (stable ? 0 : 1);
    if (h0 > target_limit)
        return {};

    struct Cmp
    {
        const std::vector<Node> *pool;
        bool operator()(int a, int b) const
        {
            const Node &A = (*pool)[a];
            const Node &B = (*pool)[b];
            if (A.f != B.f)
                return A.f > B.f;          // smaller f first
            return A.fruits < B.fruits;    // more fruits first
        }
    };

    std::vector<Node> pool;
    pool.reserve(256);

    std::priority_queue<int, std::vector<int>, Cmp> open((Cmp{&pool}));

    // Fast visited stamp array: avoids allocating/clearing vector<bool> every call.
    static int seen[BITSET_SIZE];
    static int seen_token = 1;
    ++seen_token;
    if (seen_token == INT_MAX)
    {
        std::fill(seen, seen + BITSET_SIZE, 0);
        seen_token = 1;
    }

    pool.push_back(Node{h0, 0, 0, -1, UP, *this});
    open.push(0);

    while (!open.empty())
    {
        const int cur_idx = open.top();
        open.pop();

        Node &cur = pool[cur_idx];

        if (cur.state.snakes[snake_id].dead())
            continue;
        if (cur.g >= max_depth)
            continue;

        const int head_cell = cur.state.cell_index(cur.state.snakes[snake_id].head());

        if (stable)
        {
            if (head_cell == target_cell)
            {
                std::vector<DIR> path;
                for (int idx = cur_idx; pool[idx].parent != -1; idx = pool[idx].parent)
                    path.push_back(pool[idx].move);
                std::reverse(path.begin(), path.end());
                return {std::move(path), cur.fruits};
            }
        }
        else
        {
            if (dists[head_cell][target_cell] == 1)
            {
                std::vector<DIR> path;
                for (int idx = cur_idx; pool[idx].parent != -1; idx = pool[idx].parent)
                    path.push_back(pool[idx].move);
                std::reverse(path.begin(), path.end());
                path.push_back(deduce_dir(head_cell, target_cell));
                return {std::move(path), cur.fruits};
            }
        }

        if (seen[head_cell] == seen_token)
            continue;
        seen[head_cell] = seen_token;

        const int body_before = static_cast<int>(cur.state.snakes[snake_id].body.count());
        const auto sol = cur.state.solid();

        const auto moves = cur.state.get_moves(snake_id, sol);
        for (DIR d : moves)
        {
            State next = cur.state;
            next.step({{snake_id, d}});

            if (next.snakes[snake_id].dead())
                continue;

            const int new_head = next.cell_index(next.snakes[snake_id].head());
            if (seen[new_head] == seen_token)
                continue;

            const int h = dists[new_head][target_cell];
            if (h < 0)
                continue;

            const int new_g = cur.g + 1;
            if (new_g + h > target_limit)
                continue;

            const int new_fruits =
                cur.fruits + std::max(0, static_cast<int>(next.snakes[snake_id].body.count()) - body_before);

            const int new_idx = (int)pool.size();
            pool.push_back(Node{new_g + h, new_fruits, new_g, cur_idx, d, std::move(next)});
            open.push(new_idx);
        }
    }

    return {};
}

  // ── debug print ────────────────────────────────────────────────────────────
  void print() const {
    for (int y = 0; y < h_oob; y++) {
      for (int x = 0; x < w_oob; x++) {
        if (walls.test(x + y * w_oob)) { cerr << "#"; continue; }
        if (food.has(x + y * w_oob))   { cerr << "@"; continue; }
        bool empty = true;
        for (int i = 0; i < n_snakes; i++) {
          if (snakes[i].body.test(x + y * w_oob)) {
            cerr << (snakes[i].head() == Pos{x-MARGIN, y-MARGIN}
                       ? (is_mine(i) ? "M" : "O") : to_string(i));
            empty = false;
          }
        }
        if (empty) cerr << ".";
      }
      cerr << "\n";
    }
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Snake-Zone assignment (doc 2)
// ─────────────────────────────────────────────────────────────────────────────
struct SnakeZone {
  int  snake_id;
  int  food_cell;
  int  distance;
  bool contested;
  int  border_dist;
  bool has_enemy_border;
  int  enemy_border_snake;
  bool is_closest_only;
};

struct SnakeZonesResult {
  map<int, vector<SnakeZone>> snake_zones;
};

int dist_to_wall_border(const State &s, int food_cell) {
  int x = food_cell % s.w_oob, y = food_cell / s.w_oob;
  return min(min(x, s.w_oob - 1 - x), min(y, s.h_oob - 1 - y));
}

vector<int> build_zone_map(const State &s) {
  vector<int> zone_map(s.w_oob * s.h_oob, -1);
  for (int cell = 0; cell < s.w_oob * s.h_oob; cell++) {
    if (walls.test(cell)) continue;
    int best_snake = -1, best_dist = INT_MAX;
    for (int i = 0; i < s.n_snakes; i++) {
      if (s.snakes[i].dead()) continue;
      int hc = s.cell_index(s.snakes[i].head());
      int d  = dists[hc][cell];
      if (d >= 0 && d < best_dist)       { best_dist = d; best_snake = i; }
      else if (d >= 0 && d == best_dist) { best_snake = -1; }
    }
    zone_map[cell] = best_snake;
  }
  return zone_map;
}

int get_enemy_border_snake(const State &s, int snake_id,
                           const vector<int> &zone_map) {
  for (int cell = 0; cell < s.w_oob * s.h_oob; cell++) {
    if (zone_map[cell] != snake_id) continue;
    int x = cell % s.w_oob, y = cell / s.w_oob;
    int neighbors[4] = {cell - s.w_oob, cell + s.w_oob, cell - 1, cell + 1};
    bool at_edge[4]  = {y == 0, y == s.h_oob-1, x == 0, x == s.w_oob-1};
    for (int k = 0; k < 4; k++) {
      if (at_edge[k]) continue;
      int nb = neighbors[k];
      if (nb >= 0 && nb < s.w_oob * s.h_oob) {
        int owner = zone_map[nb];
        if (owner >= 0 && owner != snake_id &&
            s.is_mine(snake_id) != s.is_mine(owner))
          return owner;
      }
    }
  }
  return -1;
}

int dist_to_enemy_border(const State &s, int food_cell, int snake_id,
                         const vector<int> &zone_map) {
  bool is_mine = s.is_mine(snake_id);
  vector<int> enemy_borders;
  for (int cell = 0; cell < s.w_oob * s.h_oob; cell++) {
    int owner = zone_map[cell];
    if (owner < 0 || s.is_mine(owner) == is_mine) continue;
    int x = cell % s.w_oob, y = cell / s.w_oob;
    int neighbors[4] = {cell - s.w_oob, cell + s.w_oob, cell - 1, cell + 1};
    bool at_edge[4]  = {y == 0, y == s.h_oob-1, x == 0, x == s.w_oob-1};
    for (int k = 0; k < 4; k++) {
      if (at_edge[k]) continue;
      int nb = neighbors[k];
      if (nb >= 0 && nb < s.w_oob * s.h_oob && zone_map[nb] == snake_id) {
        enemy_borders.push_back(cell); break;
      }
    }
  }
  if (enemy_borders.empty()) return INT_MAX;

  static vector<int> bfs_q, bfs_dist;
  bfs_q.assign(s.w_oob * s.h_oob, 0);
  bfs_dist.assign(s.w_oob * s.h_oob, -1);
  int head = 0, tail = 0;
  bfs_dist[food_cell] = 0; bfs_q[tail++] = food_cell;
  while (head < tail) {
    int cur = bfs_q[head++];
    for (int eb : enemy_borders) if (cur == eb) return bfs_dist[cur];
    int x = cur % s.w_oob, y = cur / s.w_oob;
    int neighbors[4] = {cur - s.w_oob, cur + s.w_oob, cur - 1, cur + 1};
    bool at_edge[4]  = {y == 0, y == s.h_oob-1, x == 0, x == s.w_oob-1};
    for (int k = 0; k < 4; k++) {
      if (at_edge[k]) continue;
      int next = neighbors[k];
      if (next >= 0 && next < s.w_oob * s.h_oob &&
          !walls.test(next) && bfs_dist[next] == -1) {
        bfs_dist[next] = bfs_dist[cur] + 1; bfs_q[tail++] = next;
      }
    }
  }
  return INT_MAX;
}

SnakeZonesResult snake_zones(const State &s) {
  SnakeZonesResult result;
  vector<int> zone_map = build_zone_map(s);

  for (int f : s.food) {
    int my_min = INT_MAX, op_min = INT_MAX;
    vector<int> my_at_min, op_at_min;
    for (int i = 0; i < s.n_snakes; i++) {
      if (s.snakes[i].dead()) continue;
      int d = dists[s.cell_index(s.snakes[i].head())][f];
      if (d < 0) continue;
      if (s.is_mine(i)) {
        if (d < my_min)       { my_min = d; my_at_min = {i}; }
        else if (d == my_min) { my_at_min.push_back(i); }
      } else {
        if (d < op_min)       { op_min = d; op_at_min = {i}; }
        else if (d == op_min) { op_at_min.push_back(i); }
      }
    }

    bool contested = (my_min != INT_MAX && op_min != INT_MAX && my_min == op_min);

    auto make_zone = [&](int sid, int dist, bool cont) {
      SnakeZone z;
      z.snake_id  = sid; z.food_cell = f; z.distance = dist;
      z.contested = cont; z.is_closest_only = false;
      z.enemy_border_snake = get_enemy_border_snake(s, sid, zone_map);
      z.has_enemy_border   = (z.enemy_border_snake >= 0);
      z.border_dist = z.has_enemy_border
                        ? dist_to_enemy_border(s, f, sid, zone_map)
                        : dist_to_wall_border(s, f);
      if (cont) { z.has_enemy_border = true; z.border_dist = dist; }
      return z;
    };

    if (my_min < op_min)
      for (int sid : my_at_min) result.snake_zones[sid].push_back(make_zone(sid, my_min, false));
    else if (op_min < my_min)
      for (int sid : op_at_min) result.snake_zones[sid].push_back(make_zone(sid, op_min, false));
    else if (contested) {
      for (int sid : my_at_min) result.snake_zones[sid].push_back(make_zone(sid, my_min, true));
      for (int sid : op_at_min) result.snake_zones[sid].push_back(make_zone(sid, op_min, true));
    }
  }

  // Fallback: snakes with no food assigned get their closest food
  for (int i = 0; i < s.n_snakes; i++) {
    if (s.snakes[i].dead()) continue;
    auto &zones = result.snake_zones[i];
    if (!zones.empty()) continue;
    int hc = s.cell_index(s.snakes[i].head());
    int best_f = -1, best_d = INT_MAX;
    for (int f : s.food) {
      int d = dists[hc][f];
      if (d >= 0 && d < best_d) { best_d = d; best_f = f; }
    }
    if (best_f >= 0) {
      SnakeZone z;
      z.snake_id = i; z.food_cell = best_f; z.distance = best_d;
      z.contested = false; z.is_closest_only = true;
      z.enemy_border_snake = get_enemy_border_snake(s, i, build_zone_map(s));
      z.has_enemy_border   = (z.enemy_border_snake >= 0);
      z.border_dist = z.has_enemy_border
                        ? dist_to_enemy_border(s, best_f, i, build_zone_map(s))
                        : dist_to_wall_border(s, best_f);
      zones.push_back(z);
    }
  }

  // Sort each snake's zones: contested first, then by border_dist
  for (auto &kv : result.snake_zones) {
    sort(kv.second.begin(), kv.second.end(),
         [](const SnakeZone &a, const SnakeZone &b) {
           if (a.contested != b.contested) return a.contested;
           return a.border_dist < b.border_dist;
         });
  }
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Processed candidates  (doc 2)
// ─────────────────────────────────────────────────────────────────────────────
struct SnakeCandidate {
  int snake_id, food_cell, border_dist, path_len, fruits_on_way;
  vector<DIR> path;
  bool contested, has_enemy_border, is_closest_only;
  int  enemy_border_snake;
};

struct ProcessedCandidates {
  map<int, vector<SnakeCandidate>> snake_candidates;
  double elapsed_ms = 0;
};

// Needed at file scope to avoid AVX target-option inlining conflict
struct FoodBorderEntry { int food_cell, border_dist; };

ProcessedCandidates process_candidates(const State &s,
                                        const SnakeZonesResult &result) {
  auto start = high_resolution_clock::now();
  ProcessedCandidates processed;
  int astar_calls = 0;

  auto run_astar = [&](int sid, const SnakeZone &zone) -> SnakeCandidate {
    SnakeCandidate c;
    c.snake_id          = zone.snake_id;
    c.food_cell         = zone.food_cell;
    c.border_dist       = zone.border_dist;
    c.contested         = zone.contested;
    c.has_enemy_border  = zone.has_enemy_border;
    c.enemy_border_snake= zone.enemy_border_snake;
    c.is_closest_only   = zone.is_closest_only;
    c.path_len          = zone.distance;
    c.fruits_on_way     = 0;
    int max_d = max(8, zone.distance * 2 + 8);
    astar_calls++;
    State::PathResult pr = s.path_to_cell(sid, zone.food_cell, false, max_d);
    c.path        = pr.path;
    c.fruits_on_way = pr.fruits_eaten;
    c.path_len    = (int)c.path.size();
    return c;
  };

  for (int i = 0; i < s.n_snakes; i++) {
    if (s.snakes[i].dead() || !s.is_mine(i)) continue;
    auto it = result.snake_zones.find(i);
    if (it == result.snake_zones.end() || it->second.empty()) continue;

    const auto &zones = it->second;
    vector<SnakeZone> sorted_zones(zones.begin(), zones.end());
    sort(sorted_zones.begin(), sorted_zones.end(),
         [](const SnakeZone &a, const SnakeZone &b){
           return a.border_dist < b.border_dist; });

    vector<SnakeCandidate> cands;
    int valid_count = 0;
    set<int> tried;

    // Phase 1: top N_CANDIDATES
    for (int j = 0; j < (int)sorted_zones.size() && j < N_CANDIDATES; j++) {
      tried.insert(sorted_zones[j].food_cell);
      auto c = run_astar(i, sorted_zones[j]);
      if (c.path_len > 0) valid_count++;
      cands.push_back(std::move(c));
    }

    // Phase 2: no valid path yet – try more assigned zones
    if (valid_count == 0) {
      int lim = min(N_CANDIDATES + 2, (int)sorted_zones.size());
      for (int j = N_CANDIDATES; j < N_CANDIDATES + lim; j++) {
        tried.insert(sorted_zones[j].food_cell);
        auto c = run_astar(i, sorted_zones[j]);
        cands.push_back(c);
        if (c.path_len > 0) { valid_count++; break; }
      }
    }

    // Phase 3: still nothing – try any unassigned food by border closeness
    if (valid_count == 0) {
      vector<FoodBorderEntry> unassigned;
      for (int f : s.food) {
        if (tried.count(f)) continue;
        unassigned.push_back({f, dist_to_wall_border(s, f)});
      }
      sort(unassigned.begin(), unassigned.end(),
           [](const FoodBorderEntry &a, const FoodBorderEntry &b){ return a.border_dist < b.border_dist; });
      int lim = min(N_CANDIDATES, (int)unassigned.size());
      for (int j = 0; j < lim; j++) {
        int hc = s.cell_index(s.snakes[i].head());
        int d  = dists[hc][unassigned[j].food_cell];
        if (d < 0) continue;
        SnakeZone fake{};
        fake.snake_id   = i; fake.food_cell = unassigned[j].food_cell;
        fake.distance   = d; fake.border_dist = unassigned[j].border_dist;
        fake.is_closest_only = true;
        auto c = run_astar(i, fake);
        cands.push_back(c);
        if (c.path_len > 0) { valid_count++; break; }
      }
    }

    // Sort: valid first, then by score = len - food + border_dist
    sort(cands.begin(), cands.end(),
         [](const SnakeCandidate &a, const SnakeCandidate &b) {
           bool av = a.path_len > 0, bv = b.path_len > 0;
           if (av != bv) return av;
           if (!av) return a.border_dist < b.border_dist;
           int sa = a.path_len - a.fruits_on_way + a.border_dist;
           int sb = b.path_len - b.fruits_on_way + b.border_dist;
           if (sa != sb) return sa < sb;
           return a.border_dist < b.border_dist;
         });

    processed.snake_candidates[i] = std::move(cands);
  }

  auto end = high_resolution_clock::now();
  processed.elapsed_ms = duration<double, milli>(end - start).count();
  cerr << "A* calls: " << astar_calls << endl;
  return processed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MCTS
// ─────────────────────────────────────────────────────────────────────────────
struct SnakeTarget {
  int         food_cell = -1;
  vector<DIR> path;           // full planned path (step 0 = move to make now)
  bool        valid = false;
};

struct MCTSNode {
  MCTSNode *parent;
  vector<MCTSNode *> children;
  int   visits = 0;
  float score  = 0;
  DIR   move;
  float prior;
  int   depth;

  MCTSNode(MCTSNode *parent, DIR move, float prior = 1.0f, int depth = 0)
      : parent(parent), move(move), prior(prior), depth(depth) {}

  string to_str(bool rec = true) const {
    stringstream ss;
    ss << "Node(" << this << "): visits=" << visits << " score=" << score
       << " avg=" << (visits > 0 ? score / visits : 0)
       << " move=" << DIR_STR[move];
    if (rec) {
      ss << " children=[\n";
      for (auto *c : children) ss << "  " << c->to_str(false) << ",\n";
      ss << "]";
    }
    return ss.str();
  }
};

class MCTS {
  State       state;
  MCTSNode   *root[8] = {};
  SnakeTarget targets[8] = {};
  chrono::high_resolution_clock::time_point start_time;

public:
  MCTS(const State &s,
       chrono::high_resolution_clock::time_point start,
       const ProcessedCandidates &pc)
      : state(s), start_time(start) {

    // Wire best candidate per snake as the assigned target
    for (const auto &[sid, cands] : pc.snake_candidates) {
      if (state.snakes[sid].dead()) continue;
      for (const auto &c : cands) {
        if (c.path_len > 0 && !c.path.empty()) {
          targets[sid] = {c.food_cell, c.path, true};
          break;
        }
      }
    }
    init_roots();
  }

  void init_roots() {
    for (int i = 0; i < state.n_snakes; i++)
      if (!state.snakes[i].dead())
        root[i] = new MCTSNode(nullptr, UP, 1.0f, 0);
  }

  // ── main search loop ───────────────────────────────────────────────────────
  void search() {
    const int TIME_LIMIT_MS = 32;
    const int MAX_DEPTH     = 6;
    int iters = 0;

    while (true) {
      if (iters % 20 == 0) {
        double ms = duration<double, milli>(
                      high_resolution_clock::now() - start_time).count();
        if (ms > TIME_LIMIT_MS) break;
      }

      State sim = state;
      MCTSNode *current[8] = {};
      for (int i = 0; i < sim.n_snakes; i++)
        if (!sim.snakes[i].dead()) current[i] = root[i];

      for (int depth = 0; depth < MAX_DEPTH; depth++) {
        vector<Move> depth_moves;

        for (int i = 0; i < sim.n_snakes; i++) {
          if (sim.snakes[i].dead()) continue;
          MCTSNode *cn = current[i];
          if (!cn) continue;

          // ── expand if leaf ────────────────────────────────────────────────
          if (cn->children.empty()) {
            auto legal = sim.get_moves(i);
            if (legal.empty()) { current[i] = nullptr; continue; }

            int next_depth = cn->depth + 1;
            for (DIR move : legal) {
              // Boost prior for moves that match the assigned path at this depth
              float prior = 1.0f;
              if (targets[i].valid &&
                  cn->depth < (int)targets[i].path.size() &&
                  move == targets[i].path[cn->depth]) {
                prior = 10.0f;
              }
              cn->children.push_back(new MCTSNode(cn, move, prior, next_depth));
            }
          }

          MCTSNode *sel = select(cn, i);
          if (!sel) { current[i] = nullptr; continue; }
          current[i] = sel;
          depth_moves.push_back({i, sel->move});
        }
        sim.step(depth_moves);
      }

      // Evaluate and backpropagate
      float scores[8] = {};
      for (int i = 0; i < 8; i++) scores[i] = sim.eval(i);
      for (int i = 0; i < 8; i++)
        if (current[i]) backpropagate(current[i], scores[i]);

      iters++;
    }

    double ms = duration<double, milli>(
                  high_resolution_clock::now() - start_time).count();
    cerr << iters << " iters in " << ms << " ms\n";
  }

  // ── UCB selection ──────────────────────────────────────────────────────────
  MCTSNode *select(MCTSNode *node, int /*snake_id*/) {
    if (node->children.empty()) return nullptr;
    if (node->visits < 12)
      return node->children[rand() % node->children.size()];

    const float C = 1.6275f;
    float best_ucb = INT_MIN / 2;
    MCTSNode *best = nullptr;
    for (MCTSNode *child : node->children) {
      if (child->visits == 0) return child;
      float ucb = child->score / (float)child->visits
                  + C * child->prior * sqrtf(logf(node->visits) / child->visits);
      if (ucb > best_ucb) { best_ucb = ucb; best = child; }
    }
    return best;
  }

  void backpropagate(MCTSNode *node, float score) {
    while (node) { node->visits++; node->score += score; node = node->parent; }
  }

  // ── pick final moves ───────────────────────────────────────────────────────
  // Strategy: follow the assigned path unless MCTS found something
  // significantly better (avg advantage > threshold).
  vector<Move> get_best_moves() {
    const float OVERRIDE_THRESHOLD = 0.159f;
    vector<Move> moves;

    for (int i = 0; i < 8; i++) {
      if (!root[i]) continue;

      MCTSNode *best_child    = nullptr;
      MCTSNode *assigned_child= nullptr;
      float best_visits       = -1;

      DIR  assigned_move  = UP;
      bool has_assignment = targets[i].valid && !targets[i].path.empty();
      if (has_assignment) assigned_move = targets[i].path[0];

      for (MCTSNode *child : root[i]->children) {
        if (child->visits > best_visits) {
          best_visits = child->visits; best_child = child;
        }
        if (has_assignment && child->move == assigned_move)
          assigned_child = child;
      }

      if (!best_child) {
        // No MCTS data at all
        if (has_assignment) {
          moves.push_back({i, assigned_move});
          cerr << "Snake " << i << " MCTS empty, using assigned move\n";
        } else {
          auto legal = state.get_moves(i);
          moves.push_back({i, legal.empty() ? UP : legal[0]});
        }
        continue;
      }

      DIR chosen = best_child->move;


      moves.push_back({i, chosen});
    }
    return moves;
  }

  const SnakeTarget &get_target(int sid) const { return targets[sid]; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────────────────────────────────────
vector<Pos> parseSnakeBody(const string &body) {
  vector<Pos> parts;
  stringstream ss(body);
  string seg;
  while (getline(ss, seg, ':')) {
    int x, y; sscanf(seg.c_str(), "%d,%d", &x, &y);
    parts.push_back({x, y});
  }
  return parts;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
  int seed = (int)time(0);
  cerr << "seed = " << seed << "\n";
  srand(1773731569);

  int my_id; cin >> my_id; cin.ignore();
  int width;  cin >> width;  cin.ignore();
  int height; cin >> height; cin.ignore();
  State s(width, height);

  for (int i = 0; i < height; i++) {
    string row; getline(cin, row);
    for (int j = 0; j < width; j++)
      if (row[j] == '#') s.set_bit_margin(walls, j, i);
  }

  M_L = init_mask_l(s.w_oob, s.h_oob);
  M_R = init_mask_r(s.w_oob, s.h_oob);
  M_T = init_mask_t(s.w_oob, s.h_oob);
  M_B = init_mask_b(s.w_oob, s.h_oob);
  init_dists(walls, s.w_oob, s.h_oob);

  // Add border walls
  for (int x = 0; x < s.w_oob; x++) walls.set(x);
  for (int y = 0; y < s.h_oob; y++) {
    walls.set(y * s.w_oob);
    walls.set(y * s.w_oob + s.w_oob - 1);
  }

  int snakebots_per_player; cin >> snakebots_per_player; cin.ignore();
  s.n_snakes = snakebots_per_player * 2;
  for (int i = 0; i < snakebots_per_player; i++) {
    int id; cin >> id; cin.ignore(); s.my_ids.push_back(id);
  }
  for (int i = 0; i < snakebots_per_player; i++) {
    int id; cin >> id; cin.ignore(); s.opp_ids.push_back(id);
  }

  // ── game loop ──────────────────────────────────────────────────────────────
  int turn = 0;
  int power_source_count;
  while (turn++ < 200 && (cin >> power_source_count)) {
    s.food.clear();
    cin.ignore();
    for (int i = 0; i < power_source_count; i++) {
      int x, y; cin >> x >> y; cin.ignore();
      s.food.push_back(s.cell_index(x, y));
    }
    if (s.max_food == -1) s.max_food = power_source_count;

    int snakebot_count; cin >> snakebot_count; cin.ignore();
    Vec remaining;
    for (int i = 0; i < snakebot_count; i++) {
      int sid; string body; cin >> sid >> body; cin.ignore();
      vector<Pos> parts = parseSnakeBody(body);
      s.init_snake(sid, parts);
      remaining.push_back(sid);
    }
    for (int i = 0; i < s.n_snakes; i++)
      if (!s.snakes[i].dead() && !remaining.has(i)) s.snakes[i].kill();

    // ── planning ─────────────────────────────────────────────────────────────
    auto t0 = high_resolution_clock::now();

    // 1. Zone-based food assignment
    SnakeZonesResult zones = snake_zones(s);

    auto t1 = high_resolution_clock::now();
    cerr << "snake_zones: "
         << duration<double, milli>(t1 - t0).count() << " ms\n";

    // 2. A*-backed candidate paths
    ProcessedCandidates pc = process_candidates(s, zones);
    auto t2 = high_resolution_clock::now();
    cerr << "process_candidates: "
         << duration<double, milli>(t2 - t1).count() << " ms\n";

    // 3. MCTS guided by zone-assignment priors
    MCTS mcts(s, t0, pc);
    mcts.search();
    auto best_moves = mcts.get_best_moves();

    // ── output ────────────────────────────────────────────────────────────────
    bool first = true;
    for (const auto &mv : best_moves) {
      if (!s.is_mine(mv.first)) continue;
      cout << (first ? "" : ";") << mv.first << " " << DIR_STR[mv.second];
      cerr << (first ? "" : ";") << mv.first << " " << DIR_STR[mv.second];
      first = false;
    }
    for (int i = 0; i < s.n_snakes; i++) {
      if (!s.is_mine(i)) continue;
      const auto &t = mcts.get_target(i);
      if (!t.valid) continue;
      int fx = (t.food_cell % s.w_oob) - MARGIN;
      int fy = (t.food_cell / s.w_oob) - MARGIN;
      cout << ";MARK " << fx << " " << fy;
      cerr << ";MARK " << fx << " " << fy;
    }
    cout << "\n"; cerr << "\n";

    auto t3 = high_resolution_clock::now();
    cerr << "TOTAL TURN: "
         << duration<double, milli>(t3 - t0).count() << " ms\n";
  }
}
