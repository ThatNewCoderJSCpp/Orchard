#ifndef ORCHARD_AI_SEARCH_RESULT_HPP
#define ORCHARD_AI_SEARCH_RESULT_HPP

#include <cstddef>
#include <vector>

namespace orchard {
namespace ai {

template <class Node>
struct SearchResult {
    std::vector<Node> path;
    double cost = 0.0;
    bool found = false;
    std::size_t expanded = 0;
    unsigned runners = 1;
    std::size_t loops = 0;
    std::size_t collisions = 0;
    std::size_t revisits = 0;
    int winner = -1;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_SEARCH_RESULT_HPP
