#include <orchard/ai/chess.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace orchard {
namespace ai {

namespace {

constexpr int k_mate = 100000;
constexpr int k_inf = 1000000;

const int k_values[7] = {0, 100, 320, 330, 500, 900, 0};

const int k_pst[7][64] = {
    {0},
    {0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, -20, -20, 10, 10, 5, 5, -5, -10, 0, 0, -10, -5, 5, 0, 0, 0, 20, 20, 0, 0, 0,
     5, 5, 10, 25, 25, 10, 5, 5, 10, 10, 20, 30, 30, 20, 10, 10, 50, 50, 50, 50, 50, 50, 50, 50, 0, 0, 0, 0, 0, 0, 0, 0},
    {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0, 5, 5, 0, -20, -40, -30, 5, 10, 15, 15, 10, 5, -30, -30, 0, 15, 20, 20, 15, 0, -30,
     -30, 5, 15, 20, 20, 15, 5, -30, -30, 0, 10, 15, 15, 10, 0, -30, -40, -20, 0, 0, 0, 0, -20, -40, -50, -40, -30, -30, -30, -30, -40, -50},
    {-20, -10, -10, -10, -10, -10, -10, -20, -10, 5, 0, 0, 0, 0, 5, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 0, 10, 10, 10, 10, 0, -10,
     -10, 5, 5, 10, 10, 5, 5, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 0, 0, 0, 0, 0, 0, -10, -20, -10, -10, -10, -10, -10, -10, -20},
    {0, 0, 0, 5, 5, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5,
     -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 5, 10, 10, 10, 10, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0},
    {-20, -10, -10, -5, -5, -10, -10, -20, -10, 0, 5, 0, 0, 0, 0, -10, -10, 5, 5, 5, 5, 5, 0, -10, 0, 0, 5, 5, 5, 5, 0, -5,
     -5, 0, 5, 5, 5, 5, 0, -5, -10, 0, 5, 5, 5, 5, 0, -10, -10, 0, 0, 0, 0, 0, 0, -10, -20, -10, -10, -5, -5, -10, -10, -20},
    {20, 30, 10, 0, 0, 10, 30, 20, 20, 20, 0, 0, 0, 0, 20, 20, -10, -20, -20, -20, -20, -20, -20, -10, -20, -30, -30, -40, -40, -30, -30, -20,
     -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30}};

const int k_king_end[64] = {-50, -30, -30, -30, -30, -30, -30, -50, -30, -30, 0, 0, 0, 0, -30, -30, -30, -10, 20, 30, 30, 20, -10, -30, -30, -10, 30, 40, 40, 30, -10, -30,
                            -30, -10, 30, 40, 40, 30, -10, -30, -30, -10, 20, 30, 30, 20, -10, -30, -30, -20, -10, 0, 0, -10, -20, -30, -50, -40, -30, -20, -20, -30, -40, -50};

struct Zobrist {
    std::uint64_t piece[13][64];
    std::uint64_t side;
    std::uint64_t castle[16];
    std::uint64_t ep[8];
    Zobrist() {
        Random r(0x0C4E55ull);
        for (auto& p : piece)
            for (auto& s : p) s = r.next();
        side = r.next();
        for (auto& c : castle) c = r.next();
        for (auto& e : ep) e = r.next();
    }
};

const Zobrist& zobrist() {
    static const Zobrist z;
    return z;
}

double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int file_of(int s) {
    return s & 7;
}

int rank_of(int s) {
    return s >> 3;
}

bool on_board(int f, int r) {
    return f >= 0 && f < 8 && r >= 0 && r < 8;
}

const int k_knight[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
const int k_king[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
const int k_diag[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
const int k_line[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

} // namespace

std::string Chess::Move::uci() const {
    std::string s = square_name(from) + square_name(to);
    if (promotion) s += static_cast<char>(std::tolower(piece_letter(promotion)));
    return s;
}

Chess::Chess() {
    reset();
}

void Chess::reset() {
    load_fen(start_fen);
}

int Chess::parse_square(const std::string& n) {
    if (n.size() != 2) return -1;
    const int f = std::tolower(static_cast<unsigned char>(n[0])) - 'a', r = n[1] - '1';
    return on_board(f, r) ? r * 8 + f : -1;
}

std::string Chess::square_name(int s) {
    if (s < 0 || s >= 64) return "-";
    return std::string(1, static_cast<char>('a' + file_of(s))) + static_cast<char>('1' + rank_of(s));
}

char Chess::piece_letter(int p) {
    static const char letters[7] = {'.', 'P', 'N', 'B', 'R', 'Q', 'K'};
    const int a = std::abs(p);
    if (a > 6) return '?';
    return p < 0 ? static_cast<char>(std::tolower(letters[a])) : letters[a];
}

bool Chess::load_fen(const std::string& fen, std::string* error) {
    std::istringstream in(fen);
    std::string placement, side, castle, ep;
    int half = 0, full = 1;
    in >> placement >> side >> castle >> ep;
    if (!(in >> half)) half = 0;
    if (!(in >> full)) full = 1;
    std::array<int, 64> board{};
    int r = 7, f = 0;
    for (char c : placement) {
        if (c == '/') {
            --r;
            f = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            f += c - '0';
            continue;
        }
        const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        int p = 0;
        switch (u) {
        case 'P': p = pawn; break;
        case 'N': p = knight; break;
        case 'B': p = bishop; break;
        case 'R': p = rook; break;
        case 'Q': p = queen; break;
        case 'K': p = king; break;
        default:
            if (error) *error = std::string("bad piece '") + c + "'";
            return false;
        }
        if (!on_board(f, r)) {
            if (error) *error = "too many squares in a rank";
            return false;
        }
        board[static_cast<std::size_t>(r * 8 + f)] = std::isupper(static_cast<unsigned char>(c)) ? p : -p;
        ++f;
    }
    if (r != 0 || (side != "w" && side != "b")) {
        if (error) *error = "not a full board and side to move";
        return false;
    }
    m_board = board;
    m_side = side == "w" ? 1 : -1;
    m_castling = 0;
    for (char c : castle) {
        if (c == 'K') m_castling |= 1;
        if (c == 'Q') m_castling |= 2;
        if (c == 'k') m_castling |= 4;
        if (c == 'q') m_castling |= 8;
    }
    m_ep = ep == "-" ? -1 : parse_square(ep);
    m_halfmove = half;
    m_fullmove = std::max(1, full);
    m_history.clear();
    m_table.clear();
    m_hash = compute_hash();
    m_positions.assign(1, m_hash);
    return true;
}

std::string Chess::fen() const {
    std::string out;
    for (int r = 7; r >= 0; --r) {
        int gap = 0;
        for (int f = 0; f < 8; ++f) {
            const int p = m_board[static_cast<std::size_t>(r * 8 + f)];
            if (!p) {
                ++gap;
                continue;
            }
            if (gap) out += std::to_string(gap);
            gap = 0;
            out += piece_letter(p);
        }
        if (gap) out += std::to_string(gap);
        if (r) out += '/';
    }
    out += m_side == 1 ? " w " : " b ";
    std::string c;
    if (m_castling & 1) c += 'K';
    if (m_castling & 2) c += 'Q';
    if (m_castling & 4) c += 'k';
    if (m_castling & 8) c += 'q';
    out += c.empty() ? "-" : c;
    out += " " + (m_ep >= 0 ? square_name(m_ep) : std::string("-"));
    out += " " + std::to_string(m_halfmove) + " " + std::to_string(m_fullmove);
    return out;
}

std::uint64_t Chess::compute_hash() const {
    const Zobrist& z = zobrist();
    std::uint64_t h = 0;
    for (int s = 0; s < 64; ++s)
        if (m_board[static_cast<std::size_t>(s)]) h ^= z.piece[m_board[static_cast<std::size_t>(s)] + 6][s];
    if (m_side < 0) h ^= z.side;
    h ^= z.castle[m_castling & 15];
    if (m_ep >= 0) h ^= z.ep[file_of(m_ep)];
    return h;
}

bool Chess::attacked(int sq, int by) const {
    const int f = file_of(sq), r = rank_of(sq);
    const int pr = r - by;
    for (int df : {-1, 1})
        if (on_board(f + df, pr) && m_board[static_cast<std::size_t>(pr * 8 + f + df)] == pawn * by) return true;
    for (const auto& k : k_knight)
        if (on_board(f + k[0], r + k[1]) && m_board[static_cast<std::size_t>((r + k[1]) * 8 + f + k[0])] == knight * by) return true;
    for (const auto& k : k_king)
        if (on_board(f + k[0], r + k[1]) && m_board[static_cast<std::size_t>((r + k[1]) * 8 + f + k[0])] == king * by) return true;
    for (const auto& d : k_diag)
        for (int x = f + d[0], y = r + d[1]; on_board(x, y); x += d[0], y += d[1]) {
            const int p = m_board[static_cast<std::size_t>(y * 8 + x)];
            if (!p) continue;
            if (p == bishop * by || p == queen * by) return true;
            break;
        }
    for (const auto& d : k_line)
        for (int x = f + d[0], y = r + d[1]; on_board(x, y); x += d[0], y += d[1]) {
            const int p = m_board[static_cast<std::size_t>(y * 8 + x)];
            if (!p) continue;
            if (p == rook * by || p == queen * by) return true;
            break;
        }
    return false;
}

int Chess::king_square(int side) const {
    for (int s = 0; s < 64; ++s)
        if (m_board[static_cast<std::size_t>(s)] == king * side) return s;
    return -1;
}

void Chess::generate(std::vector<Move>& out, bool captures_only) const {
    const int side = m_side;
    auto add = [&](int from, int to) {
        const int target = m_board[static_cast<std::size_t>(to)];
        if (captures_only && !target) return;
        out.push_back(Move{from, to, 0});
    };
    for (int s = 0; s < 64; ++s) {
        const int p = m_board[static_cast<std::size_t>(s)];
        if (!p || (p > 0) != (side > 0)) continue;
        const int a = std::abs(p);
        const int f = file_of(s), r = rank_of(s);
        if (a == pawn) {
            const int dir = side;
            const int nr = r + dir;
            const bool promo = nr == (side > 0 ? 7 : 0);
            auto push = [&](int to) {
                if (promo) {
                    for (int pp : {queen, knight, rook, bishop}) out.push_back(Move{s, to, pp});
                } else {
                    out.push_back(Move{s, to, 0});
                }
            };
            if (on_board(f, nr) && !m_board[static_cast<std::size_t>(nr * 8 + f)]) {
                if (!captures_only || promo) push(nr * 8 + f);
                const int start = side > 0 ? 1 : 6;
                if (!captures_only && r == start && !m_board[static_cast<std::size_t>((r + 2 * dir) * 8 + f)]) out.push_back(Move{s, (r + 2 * dir) * 8 + f, 0});
            }
            for (int df : {-1, 1}) {
                if (!on_board(f + df, nr)) continue;
                const int to = nr * 8 + f + df;
                const int t = m_board[static_cast<std::size_t>(to)];
                if ((t && (t > 0) != (side > 0)) || to == m_ep) push(to);
            }
            continue;
        }
        if (a == knight || a == king) {
            const auto& offs = a == knight ? k_knight : k_king;
            for (const auto& k : offs) {
                const int x = f + k[0], y = r + k[1];
                if (!on_board(x, y)) continue;
                const int t = m_board[static_cast<std::size_t>(y * 8 + x)];
                if (t && (t > 0) == (side > 0)) continue;
                add(s, y * 8 + x);
            }
            if (a == king && !captures_only) {
                const int base = side > 0 ? 0 : 56;
                const int ks = side > 0 ? 1 : 4, qs = side > 0 ? 2 : 8;
                if (s == base + 4 && !attacked(s, -side)) {
                    if ((m_castling & ks) && !m_board[static_cast<std::size_t>(base + 5)] && !m_board[static_cast<std::size_t>(base + 6)] && m_board[static_cast<std::size_t>(base + 7)] == rook * side && !attacked(base + 5, -side) && !attacked(base + 6, -side)) out.push_back(Move{s, base + 6, 0});
                    if ((m_castling & qs) && !m_board[static_cast<std::size_t>(base + 3)] && !m_board[static_cast<std::size_t>(base + 2)] && !m_board[static_cast<std::size_t>(base + 1)] && m_board[static_cast<std::size_t>(base)] == rook * side && !attacked(base + 3, -side) && !attacked(base + 2, -side)) out.push_back(Move{s, base + 2, 0});
                }
            }
            continue;
        }
        auto slide = [&](const int (*dirs)[2], int n) {
            for (int i = 0; i < n; ++i)
                for (int x = f + dirs[i][0], y = r + dirs[i][1]; on_board(x, y); x += dirs[i][0], y += dirs[i][1]) {
                    const int t = m_board[static_cast<std::size_t>(y * 8 + x)];
                    if (t && (t > 0) == (side > 0)) break;
                    add(s, y * 8 + x);
                    if (t) break;
                }
        };
        if (a == bishop || a == queen) slide(k_diag, 4);
        if (a == rook || a == queen) slide(k_line, 4);
    }
}

void Chess::make(const Move& m) {
    Undo u;
    u.move = m;
    u.moved = m_board[static_cast<std::size_t>(m.from)];
    u.captured = m_board[static_cast<std::size_t>(m.to)];
    u.capture_square = m.to;
    u.castling = m_castling;
    u.ep = m_ep;
    u.halfmove = m_halfmove;
    u.hash = m_hash;
    const int p = u.moved;
    const int a = std::abs(p);
    if (a == pawn && m.to == m_ep && !u.captured) {
        u.capture_square = m.to - 8 * m_side;
        u.captured = m_board[static_cast<std::size_t>(u.capture_square)];
        m_board[static_cast<std::size_t>(u.capture_square)] = 0;
    }
    m_board[static_cast<std::size_t>(m.to)] = m.promotion ? m.promotion * m_side : p;
    m_board[static_cast<std::size_t>(m.from)] = 0;
    if (a == king && std::abs(m.to - m.from) == 2) {
        const bool kingside = m.to > m.from;
        const int rf = kingside ? m.from + 3 : m.from - 4, rt = kingside ? m.from + 1 : m.from - 1;
        m_board[static_cast<std::size_t>(rt)] = m_board[static_cast<std::size_t>(rf)];
        m_board[static_cast<std::size_t>(rf)] = 0;
    }
    if (a == king) m_castling &= m_side > 0 ? ~3 : ~12;
    auto corner = [&](int sq) {
        if (sq == 0) m_castling &= ~2;
        if (sq == 7) m_castling &= ~1;
        if (sq == 56) m_castling &= ~8;
        if (sq == 63) m_castling &= ~4;
    };
    corner(m.from);
    corner(m.to);
    m_ep = a == pawn && std::abs(m.to - m.from) == 16 ? (m.from + m.to) / 2 : -1;
    m_halfmove = a == pawn || u.captured ? 0 : m_halfmove + 1;
    if (m_side < 0) ++m_fullmove;
    m_side = -m_side;
    m_hash = compute_hash();
    m_history.push_back(u);
    m_positions.push_back(m_hash);
}

void Chess::unmake() {
    if (m_history.empty()) return;
    const Undo u = m_history.back();
    m_history.pop_back();
    m_positions.pop_back();
    m_side = -m_side;
    if (m_side < 0) --m_fullmove;
    const Move& m = u.move;
    m_board[static_cast<std::size_t>(m.from)] = u.moved;
    m_board[static_cast<std::size_t>(m.to)] = 0;
    if (u.captured) m_board[static_cast<std::size_t>(u.capture_square)] = u.captured;
    if (std::abs(u.moved) == king && std::abs(m.to - m.from) == 2) {
        const bool kingside = m.to > m.from;
        const int rf = kingside ? m.from + 3 : m.from - 4, rt = kingside ? m.from + 1 : m.from - 1;
        m_board[static_cast<std::size_t>(rf)] = m_board[static_cast<std::size_t>(rt)];
        m_board[static_cast<std::size_t>(rt)] = 0;
    }
    m_castling = u.castling;
    m_ep = u.ep;
    m_halfmove = u.halfmove;
    m_hash = u.hash;
}

void Chess::legal(std::vector<Move>& out) {
    std::vector<Move> pseudo;
    generate(pseudo, false);
    out.clear();
    const int side = m_side;
    for (const Move& m : pseudo) {
        make(m);
        const int k = king_square(side);
        if (k >= 0 && !attacked(k, -side)) out.push_back(m);
        unmake();
    }
}

std::vector<Chess::Move> Chess::legal_moves() const {
    Chess copy = *this;
    std::vector<Move> out;
    copy.legal(out);
    return out;
}

std::vector<Chess::Move> Chess::legal_moves_from(int square) const {
    std::vector<Move> out;
    for (const Move& m : legal_moves())
        if (m.from == square) out.push_back(m);
    return out;
}

bool Chess::in_check() const {
    const int k = king_square(m_side);
    return k >= 0 && attacked(k, -m_side);
}

bool Chess::checkmate() const {
    return in_check() && legal_moves().empty();
}

bool Chess::stalemate() const {
    return !in_check() && legal_moves().empty();
}

int Chess::repetitions() const {
    int n = 0;
    for (std::uint64_t h : m_positions)
        if (h == m_hash) ++n;
    return n;
}

bool Chess::insufficient() const {
    int minors = 0;
    for (int p : m_board) {
        const int a = std::abs(p);
        if (a == pawn || a == rook || a == queen) return false;
        if (a == knight || a == bishop) ++minors;
    }
    return minors <= 1;
}

bool Chess::draw() const {
    return m_halfmove >= 100 || repetitions() >= 3 || insufficient() || stalemate();
}

bool Chess::over() const {
    return legal_moves().empty() || draw();
}

std::string Chess::status() const {
    const bool none = legal_moves().empty();
    if (none) return in_check() ? "checkmate" : "stalemate";
    if (m_halfmove >= 100) return "draw-fifty-moves";
    if (repetitions() >= 3) return "draw-repetition";
    if (insufficient()) return "draw-material";
    return in_check() ? "check" : "playing";
}

int Chess::winner() const {
    return checkmate() ? -m_side : 0;
}

std::string Chess::san(const Move& m) const {
    const int p = m_board[static_cast<std::size_t>(m.from)];
    const int a = std::abs(p);
    std::string s;
    if (a == king && std::abs(m.to - m.from) == 2) {
        s = m.to > m.from ? "O-O" : "O-O-O";
    } else {
        const bool capture = m_board[static_cast<std::size_t>(m.to)] != 0 || (a == pawn && m.to == m_ep);
        if (a == pawn) {
            if (capture) s += static_cast<char>('a' + file_of(m.from));
        } else {
            s += piece_letter(a);
            bool same_file = false, same_rank = false, other = false;
            for (const Move& o : legal_moves()) {
                if (o.to != m.to || o.from == m.from || std::abs(m_board[static_cast<std::size_t>(o.from)]) != a) continue;
                other = true;
                if (file_of(o.from) == file_of(m.from)) same_file = true;
                if (rank_of(o.from) == rank_of(m.from)) same_rank = true;
            }
            if (other) {
                if (!same_file) s += static_cast<char>('a' + file_of(m.from));
                else if (!same_rank) s += static_cast<char>('1' + rank_of(m.from));
                else s += square_name(m.from);
            }
        }
        if (capture) s += 'x';
        s += square_name(m.to);
        if (m.promotion) {
            s += '=';
            s += piece_letter(m.promotion);
        }
    }
    Chess after = *this;
    after.make(m);
    if (after.in_check()) s += after.legal_moves().empty() ? '#' : '+';
    return s;
}

std::optional<Chess::Move> Chess::parse_move(const std::string& raw) const {
    std::string t;
    for (char c : raw)
        if (!std::isspace(static_cast<unsigned char>(c)) && c != '+' && c != '#' && c != '!' && c != '?') t += c;
    for (char& c : t)
        if (c == '0') c = 'O';
    const std::vector<Move> legal = legal_moves();
    if (t.size() >= 4 && t.size() <= 5) {
        const int from = parse_square(t.substr(0, 2)), to = parse_square(t.substr(2, 2));
        if (from >= 0 && to >= 0) {
            int promo = 0;
            if (t.size() == 5) {
                switch (std::tolower(static_cast<unsigned char>(t[4]))) {
                case 'q': promo = queen; break;
                case 'r': promo = rook; break;
                case 'b': promo = bishop; break;
                case 'n': promo = knight; break;
                default: break;
                }
            }
            for (const Move& m : legal)
                if (m.from == from && m.to == to && (m.promotion == promo || (!promo && m.promotion == queen))) return m;
        }
    }
    for (const Move& m : legal) {
        std::string s = san(m);
        while (!s.empty() && (s.back() == '+' || s.back() == '#')) s.pop_back();
        if (s == t) return m;
        std::string no_eq = s;
        no_eq.erase(std::remove(no_eq.begin(), no_eq.end(), '='), no_eq.end());
        if (no_eq == t) return m;
    }
    return std::nullopt;
}

bool Chess::play(const Move& m) {
    const std::vector<Move> legal = legal_moves();
    if (std::find(legal.begin(), legal.end(), m) == legal.end()) return false;
    make(m);
    return true;
}

bool Chess::play(const std::string& text) {
    const std::optional<Move> m = parse_move(text);
    if (!m) return false;
    make(*m);
    return true;
}

bool Chess::undo() {
    if (m_history.empty()) return false;
    unmake();
    return true;
}

std::vector<std::string> Chess::history_san() const {
    Chess replay = *this;
    std::vector<Move> moves;
    while (!replay.m_history.empty()) {
        moves.push_back(replay.m_history.back().move);
        replay.unmake();
    }
    std::reverse(moves.begin(), moves.end());
    std::vector<std::string> out;
    for (const Move& m : moves) {
        out.push_back(replay.san(m));
        replay.make(m);
    }
    return out;
}

int Chess::evaluate() const {
    int score = 0;
    int material = 0;
    for (int p : m_board)
        if (std::abs(p) != king && std::abs(p) != pawn) material += k_values[std::abs(p)];
    const bool endgame = material <= 1300;
    for (int s = 0; s < 64; ++s) {
        const int p = m_board[static_cast<std::size_t>(s)];
        if (!p) continue;
        const int a = std::abs(p);
        const int idx = p > 0 ? s : (7 - rank_of(s)) * 8 + file_of(s);
        const int pst = a == king && endgame ? k_king_end[idx] : k_pst[a][idx];
        const int v = k_values[a] + pst;
        score += p > 0 ? v : -v;
    }
    int bishops_w = 0, bishops_b = 0;
    for (int p : m_board) {
        if (p == bishop) ++bishops_w;
        if (p == -bishop) ++bishops_b;
    }
    if (bishops_w >= 2) score += 30;
    if (bishops_b >= 2) score -= 30;
    return score;
}

void Chess::order(std::vector<Move>& moves, const Move* first, int ply) const {
    auto value = [&](const Move& m) {
        if (first && m == *first) return 1000000;
        const int victim = std::abs(m_board[static_cast<std::size_t>(m.to)]);
        const int attacker = std::abs(m_board[static_cast<std::size_t>(m.from)]);
        int v = 0;
        if (victim) v = 100000 + k_values[victim] * 10 - k_values[attacker];
        if (m.promotion) v += 90000 + k_values[m.promotion];
        if (ply < 128 && (m == m_killers[static_cast<std::size_t>(ply)][0] || m == m_killers[static_cast<std::size_t>(ply)][1])) v += 50000;
        return v;
    };
    std::stable_sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) { return value(a) > value(b); });
}

bool Chess::time_up() {
    if (m_limits.max_nodes > 0 && m_nodes > m_limits.max_nodes) return true;
    if (m_limits.seconds <= 0.0) return false;
    return now_seconds() - m_started > m_limits.seconds;
}

int Chess::quiesce(int alpha, int beta, int ply, bool& stop) {
    ++m_nodes;
    if ((m_nodes & 2047u) == 0 && time_up()) stop = true;
    if (stop) return 0;
    const int stand = evaluate() * m_side;
    if (stand >= beta) return stand;
    if (stand > alpha) alpha = stand;
    if (ply > 64) return stand;
    std::vector<Move> moves;
    generate(moves, true);
    order(moves, nullptr, 127);
    const int side = m_side;
    for (const Move& m : moves) {
        make(m);
        const int k = king_square(side);
        if (k >= 0 && attacked(k, -side)) {
            unmake();
            continue;
        }
        const int v = -quiesce(-beta, -alpha, ply + 1, stop);
        unmake();
        if (stop) return 0;
        if (v >= beta) return v;
        if (v > alpha) alpha = v;
    }
    return alpha;
}

int Chess::negamax(int depth, int alpha, int beta, int ply, bool& stop) {
    ++m_nodes;
    if ((m_nodes & 2047u) == 0 && time_up()) stop = true;
    if (stop) return 0;
    if (ply > 0) {
        if (m_halfmove >= 100) return 0;
        int reps = 0;
        for (std::size_t i = m_positions.size() - 1; i-- > 0;)
            if (m_positions[i] == m_hash && ++reps >= 1) return 0;
        if (insufficient()) return 0;
    }
    const bool check = in_check();
    if (check) ++depth;
    if (depth <= 0) return quiesce(alpha, beta, ply, stop);
    const int alpha0 = alpha;
    const Move* hint = nullptr;
    auto t = m_table.find(m_hash);
    Move hint_move;
    if (t != m_table.end() && t->second.key == m_hash) {
        hint_move = t->second.best;
        hint = &hint_move;
        if (ply > 0 && t->second.depth >= depth) {
            const int sc = t->second.score;
            if (t->second.flag == 0) return sc;
            if (t->second.flag == 1 && sc >= beta) return sc;
            if (t->second.flag == 2 && sc <= alpha) return sc;
        }
    }
    std::vector<Move> moves;
    generate(moves, false);
    order(moves, hint, ply);
    int best = -k_inf;
    Move best_move;
    int legal_count = 0;
    const int side = m_side;
    for (const Move& m : moves) {
        make(m);
        const int k = king_square(side);
        if (k >= 0 && attacked(k, -side)) {
            unmake();
            continue;
        }
        ++legal_count;
        const bool quiet = !m_history.back().captured && !m.promotion;
        const int v = -negamax(depth - 1, -beta, -alpha, ply + 1, stop);
        unmake();
        if (stop) return 0;
        if (v > best) {
            best = v;
            best_move = m;
        }
        if (v > alpha) alpha = v;
        if (alpha >= beta) {
            if (quiet && ply < 128) {
                m_killers[static_cast<std::size_t>(ply)][1] = m_killers[static_cast<std::size_t>(ply)][0];
                m_killers[static_cast<std::size_t>(ply)][0] = m;
            }
            break;
        }
    }
    if (legal_count == 0) return check ? -(k_mate - ply) : 0;
    TableEntry e;
    e.key = m_hash;
    e.depth = depth;
    e.score = best;
    e.best = best_move;
    e.flag = best <= alpha0 ? 2 : best >= beta ? 1 : 0;
    if (m_table.size() > 400000) m_table.clear();
    m_table[m_hash] = e;
    return best;
}

Chess::SearchSettings Chess::level_settings(int level) {
    SearchSettings s;
    level = std::clamp(level, 1, 10);
    static const int depths[11] = {0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 8};
    static const double noise[11] = {0, 350, 180, 110, 60, 35, 15, 0, 0, 0, 0};
    static const double secs[11] = {0, 0.5, 0.5, 0.5, 0.5, 1, 1, 1.5, 2, 3, 5};
    s.depth = depths[level];
    s.randomness = noise[level];
    s.seconds = secs[level];
    return s;
}

Chess::SearchInfo Chess::search_level(int level, Random* random) const {
    return search(level_settings(level), random);
}

Chess::SearchInfo Chess::search(const SearchSettings& settings, Random* random) const {
    Chess work = *this;
    return work.run_search(settings, random);
}

Chess::SearchInfo Chess::run_search(const SearchSettings& settings, Random* random) {
    SearchInfo info;
    m_limits = settings;
    m_started = now_seconds();
    m_nodes = 0;
    m_killers = {};
    std::vector<Move> root;
    legal(root);
    if (root.empty()) return info;
    std::vector<int> scores(root.size(), 0);
    bool stop = false;
    for (int depth = 1; depth <= std::max(1, settings.depth); ++depth) {
        std::vector<int> next(root.size(), -k_inf);
        int alpha = -k_inf;
        bool partial = false;
        for (std::size_t i = 0; i < root.size(); ++i) {
            make(root[i]);
            const int lo = settings.randomness > 0.0 ? -k_inf : alpha;
            const int v = -negamax(depth - 1, -k_inf, -lo, 1, stop);
            unmake();
            if (stop) {
                partial = true;
                break;
            }
            next[i] = v;
            alpha = std::max(alpha, v);
        }
        if (partial && depth > 1) break;
        if (partial) {
            for (std::size_t i = 0; i < next.size(); ++i)
                if (next[i] == -k_inf) next[i] = scores[i];
        }
        scores = next;
        info.depth = depth;
        std::vector<std::size_t> idx(root.size());
        for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return scores[a] > scores[b]; });
        std::vector<Move> r2;
        std::vector<int> s2;
        for (std::size_t i : idx) {
            r2.push_back(root[i]);
            s2.push_back(scores[i]);
        }
        root = r2;
        scores = s2;
        if (stop || std::abs(scores[0]) > k_mate - 200) break;
    }
    std::size_t pick = 0;
    if (random && settings.randomness > 0.0) {
        std::vector<double> w;
        for (int sc : scores) w.push_back(std::exp((static_cast<double>(sc) - scores[0]) / settings.randomness));
        pick = random->weighted(w);
    }
    info.move = root[pick];
    info.found = true;
    info.score = scores[pick];
    info.nodes = m_nodes;
    info.line.push_back(root[pick]);
    return info;
}

std::string Chess::board_text() const {
    std::string out;
    for (int r = 7; r >= 0; --r) {
        out += static_cast<char>('1' + r);
        out += ' ';
        for (int f = 0; f < 8; ++f) {
            out += piece_letter(m_board[static_cast<std::size_t>(r * 8 + f)]);
            if (f < 7) out += ' ';
        }
        out += '\n';
    }
    out += "  a b c d e f g h\n";
    return out;
}

} // namespace ai
} // namespace orchard
