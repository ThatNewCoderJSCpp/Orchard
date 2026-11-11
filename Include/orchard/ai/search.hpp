#ifndef ORCHARD_AI_SEARCH_HPP
#define ORCHARD_AI_SEARCH_HPP

#include "search_result.hpp"
#include "parallel_search.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orchard {
namespace ai {

template <class Node, class Hash = std::hash<Node>, class Neighbors, class Heuristic, class Goal>
SearchResult<Node> astar(const Node& start, Goal is_goal, Neighbors neighbors, Heuristic heuristic, std::size_t max_expand = 1000000, double heuristic_weight = 1.0) {
    struct Entry {
        double f;
        double g;
        std::size_t index;
        bool operator<(const Entry& o) const { return f > o.f || (f == o.f && g < o.g); }
    };
    std::vector<Node> nodes;
    std::vector<double> best;
    std::vector<std::size_t> parent;
    std::vector<char> closed;
    std::unordered_map<Node, std::size_t, Hash> index_of;
    std::priority_queue<Entry> open;
    auto intern = [&](const Node& n) {
        const auto it = index_of.find(n);
        if (it != index_of.end()) return it->second;
        const std::size_t i = nodes.size();
        nodes.push_back(n);
        best.push_back(std::numeric_limits<double>::infinity());
        parent.push_back(static_cast<std::size_t>(-1));
        closed.push_back(0);
        index_of.emplace(n, i);
        return i;
    };
    SearchResult<Node> result;
    const std::size_t s = intern(start);
    best[s] = 0.0;
    open.push({heuristic(start) * heuristic_weight, 0.0, s});
    std::size_t goal = static_cast<std::size_t>(-1);
    while (!open.empty()) {
        const Entry e = open.top();
        open.pop();
        if (closed[e.index] || e.g > best[e.index]) continue;
        closed[e.index] = 1;
        if (is_goal(nodes[e.index])) {
            goal = e.index;
            break;
        }
        if (++result.expanded > max_expand) break;
        const Node current = nodes[e.index];
        const double g = best[e.index];
        neighbors(current, [&](const Node& next, double step) {
            if (!(step >= 0.0) || !std::isfinite(step)) return;
            const std::size_t j = intern(next);
            if (closed[j]) return;
            const double ng = g + step;
            if (ng < best[j]) {
                best[j] = ng;
                parent[j] = e.index;
                open.push({ng + heuristic(next) * heuristic_weight, ng, j});
            }
        });
    }
    if (goal == static_cast<std::size_t>(-1)) return result;
    result.found = true;
    result.cost = best[goal];
    for (std::size_t at = goal; at != static_cast<std::size_t>(-1); at = parent[at]) result.path.push_back(nodes[at]);
    std::reverse(result.path.begin(), result.path.end());
    return result;
}

template <class Node, class Hash = std::hash<Node>, class Neighbors>
SearchResult<Node> astar_to(const Node& start, const Node& goal, Neighbors neighbors, std::function<double(const Node&, const Node&)> heuristic, std::size_t max_expand = 1000000) {
    return astar<Node, Hash>(start, [&goal](const Node& n) { return n == goal; }, neighbors, [&](const Node& n) { return heuristic ? heuristic(n, goal) : 0.0; }, max_expand);
}

template <class Node, class Hash = std::hash<Node>, class Neighbors>
std::unordered_map<Node, double, Hash> dijkstra_map(const std::vector<Node>& sources, Neighbors neighbors, double max_cost = std::numeric_limits<double>::infinity(), std::size_t max_expand = 1000000) {
    std::unordered_map<Node, double, Hash> dist;
    using Item = std::pair<double, Node>;
    auto cmp = [](const Item& a, const Item& b) { return a.first > b.first; };
    std::priority_queue<Item, std::vector<Item>, decltype(cmp)> open(cmp);
    for (const Node& s : sources) {
        dist[s] = 0.0;
        open.push({0.0, s});
    }
    std::size_t expanded = 0;
    while (!open.empty() && expanded < max_expand) {
        const Item it = open.top();
        open.pop();
        const auto d = dist.find(it.second);
        if (d != dist.end() && it.first > d->second) continue;
        ++expanded;
        neighbors(it.second, [&](const Node& next, double step) {
            const double nd = it.first + step;
            if (nd > max_cost) return;
            const auto found = dist.find(next);
            if (found == dist.end() || nd < found->second) {
                dist[next] = nd;
                open.push({nd, next});
            }
        });
    }
    return dist;
}

template <class Node, class Hash = std::hash<Node>, class Neighbors, class Goal>
SearchResult<Node> breadth_first(const Node& start, Goal is_goal, Neighbors neighbors, std::size_t max_expand = 1000000) {
    SearchResult<Node> result;
    std::unordered_map<Node, Node, Hash> parent;
    std::deque<Node> open{start};
    parent.emplace(start, start);
    while (!open.empty()) {
        const Node n = open.front();
        open.pop_front();
        if (is_goal(n)) {
            result.found = true;
            for (Node at = n;; at = parent.at(at)) {
                result.path.push_back(at);
                if (at == start) break;
            }
            std::reverse(result.path.begin(), result.path.end());
            result.cost = static_cast<double>(result.path.size() - 1);
            return result;
        }
        if (++result.expanded > max_expand) break;
        neighbors(n, [&](const Node& next, double) {
            if (parent.count(next)) return;
            parent.emplace(next, n);
            open.push_back(next);
        });
    }
    return result;
}

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

struct CellHash {
    std::size_t operator()(const Cell& c) const { return std::hash<long long>()((static_cast<long long>(c.x) << 32) ^ static_cast<unsigned int>(c.y)); }
};

class GridPathfinder {
public:
    GridPathfinder() = default;
    GridPathfinder(int width, int height, float cost = 1.0f) { resize(width, height, cost); }

    void resize(int width, int height, float cost = 1.0f) {
        m_w = std::max(0, width);
        m_h = std::max(0, height);
        m_cost.assign(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), cost);
        m_danger.assign(m_cost.size(), 0.0f);
    }

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }
    void set_blocked(int x, int y, bool blocked = true) { if (inside(x, y)) m_cost[at(x, y)] = blocked ? 0.0f : 1.0f; }
    void set_cost(int x, int y, float cost) { if (inside(x, y)) m_cost[at(x, y)] = std::max(0.0f, cost); }
    float cost(int x, int y) const { return inside(x, y) ? m_cost[at(x, y)] : 0.0f; }
    bool walkable(int x, int y) const { return cost(x, y) > 0.0f; }
    void set_danger(int x, int y, float danger) { if (inside(x, y)) m_danger[at(x, y)] = danger; }
    void add_danger(int cx, int cy, float radius, float amount) {
        const int r = static_cast<int>(std::ceil(radius));
        for (int y = cy - r; y <= cy + r; ++y)
            for (int x = cx - r; x <= cx + r; ++x) {
                if (!inside(x, y)) continue;
                const float d = std::sqrt(static_cast<float>((x - cx) * (x - cx) + (y - cy) * (y - cy)));
                if (d <= radius) m_danger[at(x, y)] += amount * (1.0f - d / std::max(radius, 1e-3f) * 0.5f);
            }
    }
    float danger(int x, int y) const { return inside(x, y) ? m_danger[at(x, y)] : 0.0f; }
    void clear_danger() { std::fill(m_danger.begin(), m_danger.end(), 0.0f); }
    void decay_danger(float factor) { for (float& d : m_danger) d *= factor; }

    bool diagonal = true;
    bool cut_corners = false;
    float danger_weight = 1.0f;
    float max_danger = std::numeric_limits<float>::infinity();

    SearchResult<Cell> find(Cell from, Cell to, std::size_t max_expand = 1000000) const {
        if (!walkable(from.x, from.y) || !walkable(to.x, to.y)) return {};
        auto h = [&](const Cell& c) {
            const double dx = std::abs(c.x - to.x), dy = std::abs(c.y - to.y);
            return diagonal ? (dx + dy) + (1.41421356 - 2.0) * std::min(dx, dy) : dx + dy;
        };
        return astar<Cell, CellHash>(from, [&](const Cell& c) { return c == to; }, [&](const Cell& c, auto&& emit) { neighbors(c, emit); }, h, max_expand);
    }

    SearchResult<Cell> find_breadth_first(Cell from, Cell to, const ParallelSearchOptions& options = {}) const { return find_runners(from, to, options, false); }
    SearchResult<Cell> find_depth_first(Cell from, Cell to, const ParallelSearchOptions& options = {}) const { return find_runners(from, to, options, true); }

    template <class Emit>
    void neighbors(const Cell& c, Emit&& emit) const {
        static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (int i = 0; i < (diagonal ? 8 : 4); ++i) {
            const int nx = c.x + dirs[i][0], ny = c.y + dirs[i][1];
            if (!walkable(nx, ny)) continue;
            const float dz = danger(nx, ny);
            if (dz > max_danger) continue;
            const bool diag = i >= 4;
            if (diag && !cut_corners && (!walkable(c.x + dirs[i][0], c.y) || !walkable(c.x, c.y + dirs[i][1]))) continue;
            const double len = diag ? 1.41421356 : 1.0;
            emit(Cell{nx, ny}, len * (cost(nx, ny) + dz * danger_weight));
        }
    }

    bool line_clear(Cell a, Cell b) const {
        int x0 = a.x, y0 = a.y;
        const int dx = std::abs(b.x - a.x), dy = -std::abs(b.y - a.y);
        const int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
        int err = dx + dy;
        while (true) {
            if (!walkable(x0, y0)) return false;
            if (x0 == b.x && y0 == b.y) return true;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    std::vector<Cell> smooth(const std::vector<Cell>& path) const {
        if (path.size() < 3) return path;
        std::vector<Cell> out{path.front()};
        std::size_t anchor = 0;
        for (std::size_t i = 2; i < path.size(); ++i) {
            if (!line_clear(path[anchor], path[i])) {
                out.push_back(path[i - 1]);
                anchor = i - 1;
            }
        }
        out.push_back(path.back());
        return out;
    }

    std::vector<float> distance_field(const std::vector<Cell>& targets) const {
        std::vector<float> dist(m_cost.size(), std::numeric_limits<float>::infinity());
        auto map = dijkstra_map<Cell, CellHash>(targets, [&](const Cell& c, auto&& emit) { neighbors(c, emit); });
        for (const auto& [c, d] : map)
            if (inside(c.x, c.y)) dist[at(c.x, c.y)] = static_cast<float>(d);
        return dist;
    }

private:
    std::size_t at(int x, int y) const { return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x); }
    Cell cell_at(std::size_t i) const { return Cell{static_cast<int>(i % static_cast<std::size_t>(m_w)), static_cast<int>(i / static_cast<std::size_t>(m_w))}; }

    double step_cost(const Cell& a, const Cell& b) const {
        const bool diag = a.x != b.x && a.y != b.y;
        return (diag ? 1.41421356 : 1.0) * (cost(b.x, b.y) + danger(b.x, b.y) * danger_weight);
    }

    SearchResult<Cell> find_runners(Cell from, Cell to, const ParallelSearchOptions& options, bool depth) const {
        if (!walkable(from.x, from.y) || !walkable(to.x, to.y)) return {};
        const std::size_t goal = at(to.x, to.y);
        auto is_goal = [goal](std::size_t n) { return n == goal; };
        auto next = [this](std::size_t n, auto&& emit) {
            neighbors(cell_at(n), [&](const Cell& v, double c) { emit(at(v.x, v.y), c); });
        };
        const SearchResult<std::size_t> r = depth ? parallel_depth_first_indexed(m_cost.size(), at(from.x, from.y), is_goal, next, options)
                                                  : parallel_breadth_first_indexed(m_cost.size(), at(from.x, from.y), is_goal, next, options);
        SearchResult<Cell> out;
        out.found = r.found;
        out.expanded = r.expanded;
        out.runners = r.runners;
        out.loops = r.loops;
        out.collisions = r.collisions;
        out.revisits = r.revisits;
        out.winner = r.winner;
        out.path.reserve(r.path.size());
        for (std::size_t i : r.path) out.path.push_back(cell_at(i));
        for (std::size_t i = 1; i < out.path.size(); ++i) out.cost += step_cost(out.path[i - 1], out.path[i]);
        return out;
    }

    int m_w = 0;
    int m_h = 0;
    std::vector<float> m_cost;
    std::vector<float> m_danger;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_SEARCH_HPP
