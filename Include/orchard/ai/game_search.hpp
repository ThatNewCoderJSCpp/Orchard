#ifndef ORCHARD_AI_GAME_SEARCH_HPP
#define ORCHARD_AI_GAME_SEARCH_HPP

#include "../random.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace orchard {
namespace ai {

template <class Move>
struct SearchChoice {
    std::optional<Move> move;
    double score = 0.0;
    int depth = 0;
    std::size_t nodes = 0;
};

template <class Game>
class Negamax {
public:
    using Move = typename Game::Move;

    int max_depth = 6;
    double seconds = 0.0;
    double randomness = 0.0;
    std::size_t max_nodes = 0;

    SearchChoice<Move> search(Game& game, Random* random = nullptr) {
        SearchChoice<Move> result;
        m_nodes = 0;
        m_stop = false;
        m_start = std::chrono::steady_clock::now();
        std::vector<Move> root = game.moves();
        if (root.empty()) return result;
        std::vector<double> scores(root.size(), 0.0);
        for (int depth = 1; depth <= std::max(1, max_depth); ++depth) {
            double alpha = -std::numeric_limits<double>::infinity();
            const double beta = std::numeric_limits<double>::infinity();
            std::vector<double> next(root.size(), -std::numeric_limits<double>::infinity());
            for (std::size_t i = 0; i < root.size(); ++i) {
                game.play(root[i]);
                const double v = -run(game, depth - 1, -beta, -alpha, 1);
                game.undo();
                if (m_stop) break;
                next[i] = v;
                alpha = std::max(alpha, v);
            }
            if (m_stop && depth > 1) break;
            scores = next;
            result.depth = depth;
            std::vector<std::size_t> order(root.size());
            for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return scores[a] > scores[b]; });
            std::vector<Move> sorted;
            std::vector<double> sorted_scores;
            for (std::size_t i : order) {
                sorted.push_back(root[i]);
                sorted_scores.push_back(scores[i]);
            }
            root = sorted;
            scores = sorted_scores;
            if (m_stop) break;
        }
        std::size_t pick = 0;
        if (random && randomness > 0.0) {
            std::vector<double> w;
            for (double s : scores) w.push_back(std::exp((s - scores[0]) / std::max(1e-6, randomness)));
            pick = random->weighted(w);
        }
        result.move = root[pick];
        result.score = scores[pick];
        result.nodes = m_nodes;
        return result;
    }

private:
    double run(Game& g, int depth, double alpha, double beta, int ply) {
        ++m_nodes;
        if ((m_nodes & 1023u) == 0 && expired()) m_stop = true;
        if (m_stop) return 0.0;
        if (depth <= 0 || g.over()) return g.score(ply);
        std::vector<Move> ms = g.moves();
        if (ms.empty()) return g.score(ply);
        double best = -std::numeric_limits<double>::infinity();
        for (const Move& m : ms) {
            g.play(m);
            const double v = -run(g, depth - 1, -beta, -alpha, ply + 1);
            g.undo();
            if (v > best) best = v;
            if (v > alpha) alpha = v;
            if (alpha >= beta) break;
        }
        return best;
    }

    bool expired() const {
        if (max_nodes > 0 && m_nodes > max_nodes) return true;
        if (seconds <= 0.0) return false;
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count() > seconds;
    }

    std::size_t m_nodes = 0;
    bool m_stop = false;
    std::chrono::steady_clock::time_point m_start;
};

template <class Game>
class MonteCarlo {
public:
    using Move = typename Game::Move;

    int iterations = 2000;
    double seconds = 0.0;
    double exploration = 1.41421356;
    int playout_limit = 400;

    SearchChoice<Move> search(const Game& root_state, Random& random) {
        SearchChoice<Move> out;
        m_nodes.clear();
        m_nodes.push_back(Node{});
        m_nodes[0].untried = root_state.moves();
        m_nodes[0].player = root_state.player();
        if (m_nodes[0].untried.empty()) return out;
        const auto start = std::chrono::steady_clock::now();
        for (int it = 0; it < std::max(1, iterations); ++it) {
            if (seconds > 0.0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() > seconds) break;
            Game state = root_state;
            std::size_t n = 0;
            while (m_nodes[n].untried.empty() && !m_nodes[n].children.empty()) {
                n = select(n);
                state.play(*m_nodes[n].move);
            }
            if (!m_nodes[n].untried.empty()) {
                const std::size_t pick = random.index(m_nodes[n].untried.size());
                const Move mv = m_nodes[n].untried[pick];
                m_nodes[n].untried.erase(m_nodes[n].untried.begin() + static_cast<std::ptrdiff_t>(pick));
                const int mover = state.player();
                state.play(mv);
                Node child;
                child.move = mv;
                child.parent = n;
                child.player = state.player();
                child.mover = mover;
                child.untried = state.moves();
                m_nodes.push_back(child);
                m_nodes[n].children.push_back(m_nodes.size() - 1);
                n = m_nodes.size() - 1;
            }
            for (int p = 0; p < playout_limit && !state.over(); ++p) {
                const std::vector<Move> ms = state.moves();
                if (ms.empty()) break;
                state.play(ms[random.index(ms.size())]);
            }
            for (std::size_t at = n;; at = m_nodes[at].parent) {
                Node& node = m_nodes[at];
                ++node.visits;
                node.wins += state.result(node.mover);
                if (at == 0) break;
            }
            ++out.nodes;
        }
        std::size_t best = m_nodes[0].children.front();
        for (std::size_t c : m_nodes[0].children)
            if (m_nodes[c].visits > m_nodes[best].visits) best = c;
        out.move = m_nodes[best].move;
        out.score = m_nodes[best].visits > 0 ? m_nodes[best].wins / m_nodes[best].visits : 0.0;
        return out;
    }

private:
    struct Node {
        std::optional<Move> move;
        std::size_t parent = 0;
        std::vector<std::size_t> children;
        std::vector<Move> untried;
        double wins = 0.0;
        double visits = 0.0;
        int player = 0;
        int mover = 0;
    };

    std::size_t select(std::size_t n) const {
        double best = -1e300;
        std::size_t pick = m_nodes[n].children.front();
        const double lnN = std::log(std::max(1.0, m_nodes[n].visits));
        for (std::size_t c : m_nodes[n].children) {
            const Node& k = m_nodes[c];
            const double u = k.wins / std::max(1e-9, k.visits) + exploration * std::sqrt(lnN / std::max(1e-9, k.visits));
            if (u > best) {
                best = u;
                pick = c;
            }
        }
        return pick;
    }

    std::vector<Node> m_nodes;
};

class TicTacToe {
public:
    using Move = int;
    std::array<int, 9> cells{};
    int to_move = 1;
    std::vector<int> history;

    std::vector<Move> moves() const;
    void play(Move m);
    void undo();
    int winner() const;
    bool over() const;
    double score(int ply) const;
    int player() const { return to_move; }
    double result(int player) const;
};

class ConnectFour {
public:
    using Move = int;
    static constexpr int columns = 7;
    static constexpr int rows = 6;
    std::array<int, columns * rows> cells{};
    std::array<int, columns> heights{};
    int to_move = 1;
    std::vector<int> history;

    std::vector<Move> moves() const;
    void play(Move column);
    void undo();
    int winner() const;
    bool over() const;
    double score(int ply) const;
    int player() const { return to_move; }
    double result(int player) const;
    int at(int column, int row) const { return cells[static_cast<std::size_t>(row * columns + column)]; }
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_GAME_SEARCH_HPP
