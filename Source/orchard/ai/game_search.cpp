#include <orchard/ai/game_search.hpp>

#include <cstdlib>

namespace orchard {
namespace ai {

std::vector<TicTacToe::Move> TicTacToe::moves() const {
    std::vector<Move> out;
    if (winner() != 0) return out;
    for (int i = 0; i < 9; ++i)
        if (cells[static_cast<std::size_t>(i)] == 0) out.push_back(i);
    return out;
}

void TicTacToe::play(Move m) {
    cells[static_cast<std::size_t>(m)] = to_move;
    history.push_back(m);
    to_move = -to_move;
}

void TicTacToe::undo() {
    if (history.empty()) return;
    cells[static_cast<std::size_t>(history.back())] = 0;
    history.pop_back();
    to_move = -to_move;
}

int TicTacToe::winner() const {
    static const int lines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
    for (const auto& l : lines) {
        const int a = cells[static_cast<std::size_t>(l[0])];
        if (a != 0 && a == cells[static_cast<std::size_t>(l[1])] && a == cells[static_cast<std::size_t>(l[2])]) return a;
    }
    return 0;
}

bool TicTacToe::over() const {
    if (winner() != 0) return true;
    for (int c : cells)
        if (c == 0) return false;
    return true;
}

double TicTacToe::score(int ply) const {
    const int w = winner();
    if (w == 0) return 0.0;
    return (w == to_move ? 1.0 : -1.0) * (100.0 - ply);
}

double TicTacToe::result(int player) const {
    const int w = winner();
    if (w == 0) return 0.5;
    return w == player ? 1.0 : 0.0;
}

std::vector<ConnectFour::Move> ConnectFour::moves() const {
    std::vector<Move> out;
    if (winner() != 0) return out;
    static const int order[columns] = {3, 2, 4, 1, 5, 0, 6};
    for (int c : order)
        if (heights[static_cast<std::size_t>(c)] < rows) out.push_back(c);
    return out;
}

void ConnectFour::play(Move c) {
    const int r = heights[static_cast<std::size_t>(c)]++;
    cells[static_cast<std::size_t>(r * columns + c)] = to_move;
    history.push_back(c);
    to_move = -to_move;
}

void ConnectFour::undo() {
    if (history.empty()) return;
    const int c = history.back();
    history.pop_back();
    const int r = --heights[static_cast<std::size_t>(c)];
    cells[static_cast<std::size_t>(r * columns + c)] = 0;
    to_move = -to_move;
}

int ConnectFour::winner() const {
    static const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c) {
            const int p = at(c, r);
            if (!p) continue;
            for (const auto& d : dirs) {
                int k = 1;
                while (k < 4) {
                    const int cc = c + d[0] * k, rr = r + d[1] * k;
                    if (cc < 0 || cc >= columns || rr < 0 || rr >= rows || at(cc, rr) != p) break;
                    ++k;
                }
                if (k == 4) return p;
            }
        }
    return 0;
}

bool ConnectFour::over() const {
    if (winner() != 0) return true;
    for (int h : heights)
        if (h < rows) return false;
    return true;
}

double ConnectFour::score(int ply) const {
    const int w = winner();
    if (w != 0) return (w == to_move ? 1.0 : -1.0) * (10000.0 - ply);
    double s = 0.0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c) {
            const int p = at(c, r);
            if (p) s += p * (3 - std::abs(3 - c));
        }
    return s * to_move;
}

double ConnectFour::result(int player) const {
    const int w = winner();
    if (w == 0) return 0.5;
    return w == player ? 1.0 : 0.0;
}

} // namespace ai
} // namespace orchard
