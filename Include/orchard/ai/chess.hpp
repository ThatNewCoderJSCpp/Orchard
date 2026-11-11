#ifndef ORCHARD_AI_CHESS_HPP
#define ORCHARD_AI_CHESS_HPP

#include "../random.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchard {
namespace ai {

class Chess {
public:
    enum Piece : int { empty = 0, pawn = 1, knight = 2, bishop = 3, rook = 4, queen = 5, king = 6 };

    struct Move {
        int from = 0;
        int to = 0;
        int promotion = 0;
        bool operator==(const Move& o) const { return from == o.from && to == o.to && promotion == o.promotion; }
        std::string uci() const;
    };

    struct SearchSettings {
        int depth = 4;
        double seconds = 0.0;
        double randomness = 0.0;
        std::size_t max_nodes = 0;
    };

    struct SearchInfo {
        Move move;
        bool found = false;
        int score = 0;
        int depth = 0;
        std::size_t nodes = 0;
        std::vector<Move> line;
    };

    static constexpr const char* start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Chess();
    void reset();
    bool load_fen(const std::string& fen, std::string* error = nullptr);
    std::string fen() const;

    int turn() const { return m_side; }
    int piece_at(int square) const { return square >= 0 && square < 64 ? m_board[static_cast<std::size_t>(square)] : 0; }
    int piece_at(const std::string& square) const { return piece_at(parse_square(square)); }
    static int parse_square(const std::string& name);
    static std::string square_name(int square);
    static char piece_letter(int piece);

    std::vector<Move> legal_moves() const;
    std::vector<Move> legal_moves_from(int square) const;
    std::optional<Move> parse_move(const std::string& text) const;
    bool play(const Move& m);
    bool play(const std::string& text);
    bool undo();
    std::size_t ply() const { return m_history.size(); }
    std::string san(const Move& m) const;
    std::vector<std::string> history_san() const;

    bool in_check() const;
    bool checkmate() const;
    bool stalemate() const;
    bool draw() const;
    bool over() const;
    std::string status() const;
    int winner() const;

    int evaluate() const;
    SearchInfo search(const SearchSettings& settings, Random* random = nullptr) const;
    SearchInfo search_level(int level, Random* random = nullptr) const;
    static SearchSettings level_settings(int level);

    std::string board_text() const;
    std::uint64_t hash() const { return m_hash; }

private:
    struct Undo {
        Move move;
        int moved = 0;
        int captured = 0;
        int capture_square = -1;
        int castling = 0;
        int ep = -1;
        int halfmove = 0;
        std::uint64_t hash = 0;
    };
    struct TableEntry {
        std::uint64_t key = 0;
        int depth = -1;
        int score = 0;
        int flag = 0;
        Move best;
    };

    void generate(std::vector<Move>& out, bool captures_only) const;
    bool attacked(int square, int by) const;
    int king_square(int side) const;
    void make(const Move& m);
    void unmake();
    bool legal_after(const Move& m) const;
    std::uint64_t compute_hash() const;
    int repetitions() const;
    bool insufficient() const;

    void legal(std::vector<Move>& out);
    SearchInfo run_search(const SearchSettings& settings, Random* random);
    int negamax(int depth, int alpha, int beta, int ply, bool& stop);
    int quiesce(int alpha, int beta, int ply, bool& stop);
    void order(std::vector<Move>& moves, const Move* first, int ply) const;
    bool time_up();

    std::array<int, 64> m_board{};
    int m_side = 1;
    int m_castling = 0;
    int m_ep = -1;
    int m_halfmove = 0;
    int m_fullmove = 1;
    std::uint64_t m_hash = 0;
    std::vector<Undo> m_history;
    std::vector<std::uint64_t> m_positions;

    std::unordered_map<std::uint64_t, TableEntry> m_table;
    std::array<std::array<Move, 2>, 128> m_killers{};
    std::size_t m_nodes = 0;
    SearchSettings m_limits;
    double m_started = 0.0;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_CHESS_HPP
