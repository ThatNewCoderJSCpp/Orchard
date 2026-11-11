#ifndef ORCHARD_AI_PARALLEL_SEARCH_HPP
#define ORCHARD_AI_PARALLEL_SEARCH_HPP

#include "search_result.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace orchard {
namespace ai {

struct ParallelSearchOptions {
    unsigned threads = 0;
    std::size_t max_expand = 1000000;
    std::size_t max_depth = std::numeric_limits<std::uint32_t>::max() - 1;
    bool first_found = true;
    std::size_t share_below = 2;
};

inline unsigned search_runner_count(unsigned threads) {
    if (threads == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        threads = hw == 0 ? 4 : hw;
    }
    return std::max(1u, std::min(threads, 64u));
}

struct ClaimInfo {
    std::uint32_t depth = 0;
    int runner = -1;
};

template <class Node, class Hash = std::hash<Node>>
class ClaimTable {
public:
    static constexpr std::size_t shard_count = 64;

    ClaimTable() : m_shards(new Shard[shard_count]) {}

    bool claim(const Node& node, const Node& parent, std::uint32_t depth, int runner, ClaimInfo& was) {
        Shard& s = shard(node);
        std::lock_guard<std::mutex> lock(s.mutex);
        const auto it = s.map.find(node);
        if (it == s.map.end()) {
            s.map.emplace(node, Entry{parent, depth, runner});
            was = ClaimInfo{};
            return true;
        }
        was.depth = it->second.depth;
        was.runner = it->second.runner;
        if (it->second.depth <= depth) return false;
        it->second = Entry{parent, depth, runner};
        return true;
    }

    bool depth_of(const Node& node, std::uint32_t& depth) const {
        Shard& s = shard(node);
        std::lock_guard<std::mutex> lock(s.mutex);
        const auto it = s.map.find(node);
        if (it == s.map.end()) return false;
        depth = it->second.depth;
        return true;
    }

    bool parent_of(const Node& node, Node& parent, ClaimInfo& info) const {
        Shard& s = shard(node);
        std::lock_guard<std::mutex> lock(s.mutex);
        const auto it = s.map.find(node);
        if (it == s.map.end()) return false;
        parent = it->second.parent;
        info.depth = it->second.depth;
        info.runner = it->second.runner;
        return true;
    }

    std::size_t size() const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < shard_count; ++i) {
            std::lock_guard<std::mutex> lock(m_shards[i].mutex);
            n += m_shards[i].map.size();
        }
        return n;
    }

private:
    struct Entry {
        Node parent;
        std::uint32_t depth;
        int runner;
    };

    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<Node, Entry, Hash> map;
    };

    Shard& shard(const Node& node) const {
        const std::uint64_t h = static_cast<std::uint64_t>(Hash()(node)) * 0x9E3779B97F4A7C15ull;
        return m_shards[static_cast<std::size_t>(h >> 58) % shard_count];
    }

    std::unique_ptr<Shard[]> m_shards;
};

class IndexClaimTable {
public:
    explicit IndexClaimTable(std::size_t count) : m_slots(new std::atomic<std::uint64_t>[count]), m_count(count) {
        for (std::size_t i = 0; i < count; ++i) m_slots[i].store(empty, std::memory_order_relaxed);
    }

    bool claim(std::size_t node, std::size_t parent, std::uint32_t depth, int runner, ClaimInfo& was) {
        if (node >= m_count) {
            was = ClaimInfo{};
            return false;
        }
        const std::uint64_t want = pack(depth, runner, parent);
        std::uint64_t cur = m_slots[node].load(std::memory_order_acquire);
        while (true) {
            if (cur != empty && depth_bits(cur) <= depth) {
                was.depth = depth_bits(cur);
                was.runner = runner_bits(cur);
                return false;
            }
            if (m_slots[node].compare_exchange_weak(cur, want, std::memory_order_acq_rel, std::memory_order_acquire)) {
                if (cur == empty) {
                    was = ClaimInfo{};
                } else {
                    was.depth = depth_bits(cur);
                    was.runner = runner_bits(cur);
                }
                return true;
            }
        }
    }

    bool depth_of(std::size_t node, std::uint32_t& depth) const {
        if (node >= m_count) return false;
        const std::uint64_t v = m_slots[node].load(std::memory_order_acquire);
        if (v == empty) return false;
        depth = depth_bits(v);
        return true;
    }

    bool parent_of(std::size_t node, std::size_t& parent, ClaimInfo& info) const {
        if (node >= m_count) return false;
        const std::uint64_t v = m_slots[node].load(std::memory_order_acquire);
        if (v == empty) return false;
        parent = static_cast<std::size_t>(v & 0xFFFFFFFFull);
        info.depth = depth_bits(v);
        info.runner = runner_bits(v);
        return true;
    }

    std::size_t count() const { return m_count; }

private:
    static constexpr std::uint64_t empty = ~0ull;

    static std::uint64_t pack(std::uint32_t depth, int runner, std::size_t parent) {
        const std::uint64_t d = std::min<std::uint64_t>(depth, 0xFFFFFEull);
        return (d << 40) | (static_cast<std::uint64_t>(runner & 0xFF) << 32) | (static_cast<std::uint64_t>(parent) & 0xFFFFFFFFull);
    }
    static std::uint32_t depth_bits(std::uint64_t v) { return static_cast<std::uint32_t>(v >> 40); }
    static int runner_bits(std::uint64_t v) { return static_cast<int>((v >> 32) & 0xFF); }

    std::unique_ptr<std::atomic<std::uint64_t>[]> m_slots;
    std::size_t m_count;
};

namespace detail {

template <class Node, class Table>
std::vector<Node> claimed_path(const Table& table, const Node& start, const Node& goal) {
    std::vector<Node> path;
    Node at = goal;
    ClaimInfo info;
    Node parent = goal;
    if (!table.parent_of(at, parent, info)) return path;
    std::size_t guard = static_cast<std::size_t>(info.depth) + 2;
    path.push_back(at);
    while (!(at == start) && guard-- > 0) {
        if (!table.parent_of(at, parent, info)) return {};
        at = parent;
        path.push_back(at);
    }
    if (!(at == start)) return {};
    std::reverse(path.begin(), path.end());
    return path;
}

class SearchBarrier {
public:
    explicit SearchBarrier(unsigned count) : m_count(count) {}

    void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        const std::size_t gen = m_generation;
        if (++m_waiting == m_count) {
            m_waiting = 0;
            ++m_generation;
            m_cv.notify_all();
            return;
        }
        m_cv.wait(lock, [&] { return gen != m_generation; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    unsigned m_count;
    unsigned m_waiting = 0;
    std::size_t m_generation = 0;
};

template <class Fn>
void run_runners(unsigned runners, Fn&& fn) {
    if (runners <= 1) {
        fn(0);
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve(runners - 1);
    for (unsigned i = 1; i < runners; ++i) pool.emplace_back([&fn, i] { fn(static_cast<int>(i)); });
    fn(0);
    for (std::thread& t : pool) t.join();
}

template <class Node, class Table, class Neighbors, class Goal>
SearchResult<Node> breadth_first_runners(Table& table, const Node& start, Goal is_goal, Neighbors neighbors, const ParallelSearchOptions& options) {
    SearchResult<Node> result;
    const unsigned runners = search_runner_count(options.threads);
    result.runners = runners;
    ClaimInfo unused;
    table.claim(start, start, 0, 0, unused);
    if (is_goal(start)) {
        result.found = true;
        result.path.push_back(start);
        result.winner = 0;
        return result;
    }
    std::vector<Node> frontier{start};
    std::vector<std::vector<Node>> next(runners);
    std::atomic<std::size_t> cursor{0};
    std::atomic<std::size_t> expanded{0};
    std::atomic<std::size_t> collisions{0};
    std::atomic<std::size_t> revisits{0};
    std::atomic<bool> stop{false};
    std::atomic<bool> finished{false};
    std::mutex goal_mutex;
    bool have_goal = false;
    Node goal = start;
    int winner = -1;
    std::uint32_t level = 0;
    SearchBarrier barrier(runners);
    auto worker = [&](int me) {
        while (true) {
            barrier.wait();
            if (finished.load()) return;
            const std::size_t chunk = std::max<std::size_t>(1, frontier.size() / (static_cast<std::size_t>(runners) * 8));
            while (!stop.load(std::memory_order_relaxed)) {
                const std::size_t begin = cursor.fetch_add(chunk);
                if (begin >= frontier.size()) break;
                const std::size_t end = std::min(frontier.size(), begin + chunk);
                for (std::size_t i = begin; i < end && !stop.load(std::memory_order_relaxed); ++i) {
                    if (expanded.fetch_add(1) >= options.max_expand) {
                        stop.store(true);
                        break;
                    }
                    const Node u = frontier[i];
                    neighbors(u, [&](const Node& v, double) {
                        if (stop.load(std::memory_order_relaxed)) return;
                        ClaimInfo was;
                        if (!table.claim(v, u, level + 1, me, was)) {
                            if (was.runner >= 0 && was.runner != me && was.depth == level + 1) collisions.fetch_add(1, std::memory_order_relaxed);
                            else revisits.fetch_add(1, std::memory_order_relaxed);
                            return;
                        }
                        if (is_goal(v)) {
                            std::lock_guard<std::mutex> lock(goal_mutex);
                            if (!have_goal) {
                                have_goal = true;
                                goal = v;
                                winner = me;
                            }
                            stop.store(true);
                            return;
                        }
                        next[static_cast<std::size_t>(me)].push_back(v);
                    });
                }
            }
            barrier.wait();
            if (me == 0) {
                frontier.clear();
                for (std::vector<Node>& part : next) {
                    frontier.insert(frontier.end(), part.begin(), part.end());
                    part.clear();
                }
                cursor.store(0);
                ++level;
                if (stop.load() || frontier.empty() || level >= options.max_depth) finished.store(true);
            }
        }
    };
    run_runners(runners, worker);
    result.expanded = std::min(expanded.load(), options.max_expand);
    result.collisions = collisions.load();
    result.revisits = revisits.load();
    if (have_goal) {
        result.path = claimed_path(table, start, goal);
        result.found = !result.path.empty();
        result.cost = result.found ? static_cast<double>(result.path.size() - 1) : 0.0;
        result.winner = winner;
    }
    return result;
}

template <class Node, class Hash, class Table, class Neighbors, class Goal>
SearchResult<Node> depth_first_runners(Table& table, const Node& start, Goal is_goal, Neighbors neighbors, const ParallelSearchOptions& options) {
    struct Seed {
        Node node;
        Node parent;
        std::uint32_t depth;
        std::vector<Node> above;
    };
    struct Frame {
        Node node;
        Node parent;
        std::uint32_t depth;
        std::vector<Node> next;
        std::size_t cursor = 0;
        bool expanded = false;
    };
    SearchResult<Node> result;
    const unsigned runners = search_runner_count(options.threads);
    result.runners = runners;
    ClaimInfo unused;
    table.claim(start, start, 0, 0, unused);
    std::mutex pool_mutex;
    std::condition_variable pool_cv;
    std::deque<Seed> pool;
    pool.push_back(Seed{start, start, 0, {}});
    unsigned idle = 0;
    bool done = false;
    std::atomic<bool> stop{false};
    std::atomic<unsigned> waiting{0};
    std::atomic<std::size_t> expanded{0};
    std::atomic<std::size_t> loops{0};
    std::atomic<std::size_t> collisions{0};
    std::atomic<std::size_t> revisits{0};
    std::atomic<std::uint32_t> best_goal{std::numeric_limits<std::uint32_t>::max()};
    std::mutex goal_mutex;
    bool have_goal = false;
    Node goal = start;
    int winner = -1;
    auto halt = [&] {
        stop.store(true);
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool_cv.notify_all();
    };
    auto take = [&](Seed& out) {
        std::unique_lock<std::mutex> lock(pool_mutex);
        ++idle;
        waiting.store(idle);
        while (pool.empty() && !done && !stop.load()) {
            if (idle == runners) {
                done = true;
                pool_cv.notify_all();
                break;
            }
            pool_cv.wait(lock);
        }
        if (pool.empty() || stop.load()) return false;
        --idle;
        waiting.store(idle);
        out = std::move(pool.front());
        pool.pop_front();
        return true;
    };
    auto give = [&](Seed&& s) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool.push_back(std::move(s));
        pool_cv.notify_one();
    };
    auto hungry = [&] {
        if (runners <= 1 || waiting.load(std::memory_order_relaxed) == 0) return false;
        std::lock_guard<std::mutex> lock(pool_mutex);
        return pool.size() < options.share_below + idle;
    };
    auto reach_goal = [&](const Node& n, std::uint32_t depth, int me) {
        {
            std::lock_guard<std::mutex> lock(goal_mutex);
            if (!have_goal || depth < best_goal.load()) {
                have_goal = true;
                goal = n;
                winner = me;
                best_goal.store(depth);
            }
        }
        if (options.first_found) halt();
    };
    auto worker = [&](int me) {
        std::vector<Frame> stack;
        std::unordered_set<Node, Hash> on_path;
        Seed seed{start, start, 0, {}};
        while (!stop.load() && take(seed)) {
            std::uint32_t now = 0;
            if (!options.first_found && table.depth_of(seed.node, now) && now < seed.depth) continue;
            stack.clear();
            on_path.clear();
            stack.push_back(Frame{seed.node, seed.parent, seed.depth, {}, 0, false});
            on_path.insert(seed.above.begin(), seed.above.end());
            on_path.insert(seed.node);
            while (!stack.empty() && !stop.load(std::memory_order_relaxed)) {
                Frame& f = stack.back();
                if (!f.expanded) {
                    f.expanded = true;
                    if (is_goal(f.node)) {
                        reach_goal(f.node, f.depth, me);
                        on_path.erase(f.node);
                        stack.pop_back();
                        continue;
                    }
                    if (expanded.fetch_add(1) >= options.max_expand) {
                        halt();
                        break;
                    }
                    neighbors(f.node, [&f](const Node& v, double) { f.next.push_back(v); });
                }
                std::uint32_t claimed = 0;
                if (!options.first_found && table.depth_of(f.node, claimed) && claimed < f.depth) {
                    on_path.erase(f.node);
                    stack.pop_back();
                    continue;
                }
                if (f.cursor >= f.next.size()) {
                    on_path.erase(f.node);
                    stack.pop_back();
                    continue;
                }
                const Node v = f.next[f.cursor++];
                const Node from = f.node;
                const std::uint32_t d = f.depth + 1;
                if (on_path.count(v)) {
                    if (!(v == f.parent)) loops.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (d > options.max_depth || d >= best_goal.load(std::memory_order_relaxed)) continue;
                ClaimInfo was;
                if (!table.claim(v, from, d, me, was)) {
                    if (was.runner >= 0 && was.runner != me) collisions.fetch_add(1, std::memory_order_relaxed);
                    else revisits.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (was.runner >= 0) {
                    if (was.runner != me) collisions.fetch_add(1, std::memory_order_relaxed);
                    else revisits.fetch_add(1, std::memory_order_relaxed);
                    if (options.first_found) continue;
                }
                if (stack.size() > 1 && hungry()) {
                    Seed handed{v, from, d, seed.above};
                    for (const Frame& up : stack) handed.above.push_back(up.node);
                    give(std::move(handed));
                    continue;
                }
                stack.push_back(Frame{v, from, d, {}, 0, false});
                on_path.insert(v);
            }
        }
    };
    run_runners(runners, worker);
    result.expanded = std::min(expanded.load(), options.max_expand);
    result.loops = loops.load();
    result.collisions = collisions.load();
    result.revisits = revisits.load();
    if (have_goal) {
        result.path = claimed_path(table, start, goal);
        result.found = !result.path.empty();
        result.cost = result.found ? static_cast<double>(result.path.size() - 1) : 0.0;
        result.winner = winner;
    }
    return result;
}

} // namespace detail

template <class Node, class Hash = std::hash<Node>, class Neighbors, class Goal>
SearchResult<Node> parallel_breadth_first(const Node& start, Goal is_goal, Neighbors neighbors, ParallelSearchOptions options = {}) {
    ClaimTable<Node, Hash> table;
    return detail::breadth_first_runners<Node>(table, start, is_goal, neighbors, options);
}

template <class Node, class Hash = std::hash<Node>, class Neighbors, class Goal>
SearchResult<Node> parallel_depth_first(const Node& start, Goal is_goal, Neighbors neighbors, ParallelSearchOptions options = {}) {
    ClaimTable<Node, Hash> table;
    return detail::depth_first_runners<Node, Hash>(table, start, is_goal, neighbors, options);
}

template <class Node, class Hash = std::hash<Node>, class Neighbors, class Goal>
SearchResult<Node> depth_first(const Node& start, Goal is_goal, Neighbors neighbors, std::size_t max_expand = 1000000, bool shortest = false) {
    ParallelSearchOptions o;
    o.threads = 1;
    o.max_expand = max_expand;
    o.first_found = !shortest;
    return parallel_depth_first<Node, Hash>(start, is_goal, neighbors, o);
}

template <class Neighbors, class Goal>
SearchResult<std::size_t> parallel_breadth_first_indexed(std::size_t count, std::size_t start, Goal is_goal, Neighbors neighbors, ParallelSearchOptions options = {}) {
    if (start >= count) return {};
    IndexClaimTable table(count);
    return detail::breadth_first_runners<std::size_t>(table, start, is_goal, neighbors, options);
}

template <class Neighbors, class Goal>
SearchResult<std::size_t> parallel_depth_first_indexed(std::size_t count, std::size_t start, Goal is_goal, Neighbors neighbors, ParallelSearchOptions options = {}) {
    if (start >= count) return {};
    IndexClaimTable table(count);
    return detail::depth_first_runners<std::size_t, std::hash<std::size_t>>(table, start, is_goal, neighbors, options);
}

template <class Node, class Hash = std::hash<Node>, class Neighbors>
std::vector<Node> find_cycle(const Node& start, Neighbors neighbors, bool undirected = false, std::size_t max_expand = 1000000) {
    struct Frame {
        Node node;
        std::size_t parent;
        std::vector<Node> next;
        std::size_t cursor = 0;
    };
    const std::size_t none = static_cast<std::size_t>(-1);
    std::vector<Frame> stack;
    std::unordered_map<Node, std::size_t, Hash> on_path;
    std::unordered_set<Node, Hash> done;
    std::size_t expanded = 0;
    auto open = [&](const Node& n, std::size_t parent) {
        Frame f{n, parent, {}, 0};
        neighbors(n, [&f](const Node& v, double) { f.next.push_back(v); });
        on_path[n] = stack.size();
        stack.push_back(std::move(f));
        ++expanded;
    };
    open(start, none);
    while (!stack.empty() && expanded <= max_expand) {
        Frame& f = stack.back();
        if (f.cursor >= f.next.size()) {
            on_path.erase(f.node);
            done.insert(f.node);
            stack.pop_back();
            continue;
        }
        const Node v = f.next[f.cursor++];
        const auto back = on_path.find(v);
        if (back != on_path.end()) {
            if (undirected && f.parent != none && stack[f.parent].node == v) continue;
            std::vector<Node> cycle;
            for (std::size_t i = back->second; i < stack.size(); ++i) cycle.push_back(stack[i].node);
            cycle.push_back(v);
            return cycle;
        }
        if (done.count(v)) continue;
        open(v, stack.size() - 1);
    }
    return {};
}

template <class Node, class Hash = std::hash<Node>, class Neighbors>
bool has_cycle(const Node& start, Neighbors neighbors, bool undirected = false, std::size_t max_expand = 1000000) {
    return !find_cycle<Node, Hash>(start, neighbors, undirected, max_expand).empty();
}

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_PARALLEL_SEARCH_HPP
