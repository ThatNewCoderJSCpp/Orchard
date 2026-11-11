#include <orchard/ai/voxel.hpp>

#include <algorithm>
#include <cmath>
#include <queue>

namespace orchard {
namespace ai {

namespace {

struct Open {
    double f;
    double g;
    std::size_t index;
    bool operator<(const Open& o) const { return f > o.f || (f == o.f && g < o.g); }
};

double octile(const BlockPos& a, const BlockPos& b) {
    const double dx = std::abs(a.x - b.x), dz = std::abs(a.z - b.z), dy = std::abs(a.y - b.y);
    return dx + dz + (1.41421356 - 2.0) * std::min(dx, dz) + dy;
}

} // namespace

bool VoxelPathfinder::passable(int x, int y, int z, const VoxelAgent& agent) const {
    const Block b = block(x, y, z);
    if (b.solid && !(b.open && agent.open_doors)) return false;
    return b.danger <= agent.max_danger;
}

bool VoxelPathfinder::body_fits(int x, int y, int z, const VoxelAgent& agent) const {
    for (int h = 0; h < std::max(1, agent.height); ++h)
        if (!passable(x, y + h, z, agent)) return false;
    return true;
}

bool VoxelPathfinder::standable(const BlockPos& p, const VoxelAgent& agent) const {
    if (!body_fits(p.x, p.y, p.z, agent)) return false;
    const Block below = block(p.x, p.y - 1, p.z);
    if (below.solid && !below.open) return true;
    const Block here = block(p.x, p.y, p.z);
    if (agent.climb && here.climbable) return true;
    if (agent.swim && here.liquid) return true;
    return false;
}

int VoxelPathfinder::ground_below(BlockPos p, int max_depth) const {
    for (int d = 0; d <= max_depth; ++d) {
        const Block b = block(p.x, p.y - d - 1, p.z);
        if (b.solid) return p.y - d;
    }
    return std::numeric_limits<int>::min();
}

void VoxelPathfinder::neighbors(const BlockPos& p, const VoxelAgent& agent, const std::function<void(const BlockPos&, double, VoxelMove)>& emit) const {
    auto danger_at = [&](int x, int y, int z) {
        float d = block(x, y - 1, z).danger;
        for (int h = 0; h < std::max(1, agent.height); ++h) d = std::max(d, block(x, y + h, z).danger);
        float near = 0.0f;
        static const int ring[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& r : ring) near = std::max(near, std::max(block(x + r[0], y, z + r[1]).danger, block(x + r[0], y - 1, z + r[1]).danger));
        return d + near * 0.5f;
    };
    auto cost_at = [&](int x, int y, int z, double len, double extra) {
        const Block floor = block(x, y - 1, z);
        const Block here = block(x, y, z);
        double c = len * std::max(0.05, static_cast<double>(floor.solid ? floor.cost : 1.0f));
        if (here.liquid) c += len * agent.swim_cost;
        if (here.open) c += 0.5;
        c += static_cast<double>(danger_at(x, y, z)) * agent.danger_weight;
        return c + extra;
    };
    static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    const Block here = block(p.x, p.y, p.z);
    const bool in_liquid = here.liquid;
    for (int i = 0; i < (agent.diagonal ? 8 : 4); ++i) {
        const int dx = dirs[i][0], dz = dirs[i][1];
        const int nx = p.x + dx, nz = p.z + dz;
        const bool diag = i >= 4;
        const double len = diag ? 1.41421356 : 1.0;
        if (diag && (!body_fits(p.x + dx, p.y, p.z, agent) || !body_fits(p.x, p.y, p.z + dz, agent))) continue;
        if (standable(BlockPos{nx, p.y, nz}, agent)) {
            emit(BlockPos{nx, p.y, nz}, cost_at(nx, p.y, nz, len, 0.0), in_liquid ? VoxelMove::swim : VoxelMove::walk);
        } else if (body_fits(nx, p.y, nz, agent)) {
            const int limit = agent.max_drop + (in_liquid ? 0 : 0);
            for (int d = 1; d <= std::max(limit, 0) + 8; ++d) {
                if (standable(BlockPos{nx, p.y - d, nz}, agent)) {
                    const bool water = block(nx, p.y - d, nz).liquid;
                    if (d <= agent.max_drop || water) emit(BlockPos{nx, p.y - d, nz}, cost_at(nx, p.y - d, nz, len, agent.drop_cost * d), VoxelMove::drop);
                    break;
                }
                if (!body_fits(nx, p.y - d, nz, agent)) break;
                if (d > agent.max_drop && !block(nx, p.y - d - 1, nz).liquid && d > agent.max_drop + 8) break;
            }
            if (!diag && agent.jump_across > 0) {
                for (int gap = 2; gap <= agent.jump_across + 1; ++gap) {
                    bool clear = true;
                    for (int s = 1; s < gap && clear; ++s) clear = body_fits(p.x + dx * s, p.y, p.z + dz * s, agent) && passable(p.x + dx * s, p.y + agent.height, p.z + dz * s, agent);
                    if (!clear) break;
                    const int lx = p.x + dx * gap, lz = p.z + dz * gap;
                    if (standable(BlockPos{lx, p.y, lz}, agent)) {
                        emit(BlockPos{lx, p.y, lz}, cost_at(lx, p.y, lz, static_cast<double>(gap), agent.jump_cost * 2.0), VoxelMove::jump);
                        break;
                    }
                    if (standable(BlockPos{lx, p.y - 1, lz}, agent)) {
                        emit(BlockPos{lx, p.y - 1, lz}, cost_at(lx, p.y - 1, lz, static_cast<double>(gap), agent.jump_cost * 2.0), VoxelMove::jump);
                        break;
                    }
                }
            }
        }
        const int rise = std::max(agent.step_up, agent.jump_up);
        for (int k = 1; k <= rise; ++k) {
            if (!passable(p.x, p.y + agent.height - 1 + k, p.z, agent)) break;
            if (standable(BlockPos{nx, p.y + k, nz}, agent)) {
                const bool step = k <= agent.step_up;
                emit(BlockPos{nx, p.y + k, nz}, cost_at(nx, p.y + k, nz, len, step ? 0.2 * k : agent.jump_cost * k), step ? VoxelMove::step_up : VoxelMove::jump);
                break;
            }
            if (!body_fits(nx, p.y + k, nz, agent) && block(nx, p.y + k - 1, nz).solid) continue;
        }
    }
    if (agent.climb) {
        if ((here.climbable || block(p.x, p.y + 1, p.z).climbable) && body_fits(p.x, p.y + 1, p.z, agent)) emit(BlockPos{p.x, p.y + 1, p.z}, cost_at(p.x, p.y + 1, p.z, 1.0, agent.climb_cost), VoxelMove::climb);
        if (block(p.x, p.y - 1, p.z).climbable && body_fits(p.x, p.y - 1, p.z, agent)) emit(BlockPos{p.x, p.y - 1, p.z}, cost_at(p.x, p.y - 1, p.z, 1.0, agent.climb_cost), VoxelMove::climb);
    }
    if (agent.swim && in_liquid) {
        if (body_fits(p.x, p.y + 1, p.z, agent) && (block(p.x, p.y + 1, p.z).liquid || standable(BlockPos{p.x, p.y + 1, p.z}, agent))) emit(BlockPos{p.x, p.y + 1, p.z}, cost_at(p.x, p.y + 1, p.z, 1.0, agent.swim_cost), VoxelMove::swim);
        if (block(p.x, p.y - 1, p.z).liquid && body_fits(p.x, p.y - 1, p.z, agent)) emit(BlockPos{p.x, p.y - 1, p.z}, cost_at(p.x, p.y - 1, p.z, 1.0, agent.swim_cost), VoxelMove::swim);
    }
}

VoxelPath VoxelPathfinder::find_any(BlockPos from, const std::vector<BlockPos>& goals, const VoxelAgent& agent) const {
    VoxelPath out;
    if (goals.empty()) return out;
    std::vector<BlockPos> nodes;
    std::vector<double> best;
    std::vector<std::size_t> parent;
    std::vector<VoxelMove> moves;
    std::vector<char> closed;
    std::unordered_map<BlockPos, std::size_t, BlockPosHash> index_of;
    auto intern = [&](const BlockPos& p) {
        const auto it = index_of.find(p);
        if (it != index_of.end()) return it->second;
        nodes.push_back(p);
        best.push_back(std::numeric_limits<double>::infinity());
        parent.push_back(static_cast<std::size_t>(-1));
        moves.push_back(VoxelMove::walk);
        closed.push_back(0);
        index_of.emplace(p, nodes.size() - 1);
        return nodes.size() - 1;
    };
    auto h = [&](const BlockPos& p) {
        double m = std::numeric_limits<double>::infinity();
        for (const BlockPos& g : goals) m = std::min(m, octile(p, g));
        return m;
    };
    std::priority_queue<Open> open;
    const std::size_t s = intern(from);
    best[s] = 0.0;
    open.push({h(from), 0.0, s});
    std::size_t goal = static_cast<std::size_t>(-1);
    std::size_t closest = s;
    double closest_h = h(from);
    while (!open.empty()) {
        const Open e = open.top();
        open.pop();
        if (closed[e.index] || e.g > best[e.index]) continue;
        closed[e.index] = 1;
        const BlockPos cur = nodes[e.index];
        if (std::find(goals.begin(), goals.end(), cur) != goals.end()) {
            goal = e.index;
            break;
        }
        const double hh = h(cur);
        if (hh < closest_h) {
            closest_h = hh;
            closest = e.index;
        }
        if (++out.expanded > agent.max_expand) break;
        const double g = best[e.index];
        neighbors(cur, agent, [&](const BlockPos& n, double step, VoxelMove mv) {
            const std::size_t j = intern(n);
            if (closed[j]) return;
            const double ng = g + step;
            if (ng < best[j]) {
                best[j] = ng;
                parent[j] = e.index;
                moves[j] = mv;
                open.push({ng + h(n), ng, j});
            }
        });
    }
    std::size_t end = goal;
    if (end == static_cast<std::size_t>(-1)) {
        end = closest;
        out.partial = true;
    } else {
        out.found = true;
    }
    for (std::size_t at = end; at != static_cast<std::size_t>(-1); at = parent[at]) out.steps.push_back(VoxelStep{nodes[at], moves[at]});
    std::reverse(out.steps.begin(), out.steps.end());
    out.cost = static_cast<float>(best[end]);
    return out;
}

VoxelPath VoxelPathfinder::find(BlockPos from, BlockPos to, const VoxelAgent& agent, bool allow_partial) const {
    VoxelPath p = find_any(from, {to}, agent);
    if (!p.found && !allow_partial) {
        p.steps.clear();
        p.partial = false;
    }
    return p;
}

VoxelPath VoxelPathfinder::flee(BlockPos from, BlockPos threat, int distance, const VoxelAgent& agent) const {
    VoxelPath out;
    std::unordered_map<BlockPos, std::pair<BlockPos, VoxelMove>, BlockPosHash> parent;
    std::unordered_map<BlockPos, double, BlockPosHash> cost;
    using Item = std::pair<double, BlockPos>;
    auto cmp = [](const Item& a, const Item& b) { return a.first > b.first; };
    std::priority_queue<Item, std::vector<Item>, decltype(cmp)> open(cmp);
    open.push({0.0, from});
    cost[from] = 0.0;
    BlockPos best = from;
    double best_score = -1e18;
    while (!open.empty() && out.expanded < agent.max_expand) {
        const Item it = open.top();
        open.pop();
        if (it.first > cost[it.second]) continue;
        ++out.expanded;
        const double away = octile(it.second, threat);
        const double score = away * 3.0 - it.first;
        if (score > best_score) {
            best_score = score;
            best = it.second;
        }
        if (away >= distance) {
            best = it.second;
            out.found = true;
            break;
        }
        neighbors(it.second, agent, [&](const BlockPos& n, double step, VoxelMove mv) {
            const double toward = std::max(0.0, 3.0 - octile(n, threat)) * 4.0;
            const double nc = it.first + step + toward;
            const auto f = cost.find(n);
            if (f == cost.end() || nc < f->second) {
                cost[n] = nc;
                parent[n] = {it.second, mv};
                open.push({nc, n});
            }
        });
    }
    for (BlockPos at = best;;) {
        const auto pr = parent.find(at);
        out.steps.push_back(VoxelStep{at, pr == parent.end() ? VoxelMove::walk : pr->second.second});
        if (pr == parent.end() || at == from) break;
        at = pr->second.first;
    }
    std::reverse(out.steps.begin(), out.steps.end());
    out.partial = !out.found;
    out.cost = static_cast<float>(cost[best]);
    return out;
}

VoxelWorld::VoxelWorld() {
    Block air;
    define("air", air);
    Block solid;
    solid.solid = true;
    define("solid", solid);
    Block ladder;
    ladder.climbable = true;
    define("ladder", ladder);
    Block water;
    water.liquid = true;
    water.cost = 2.0f;
    define("water", water);
    Block lava;
    lava.liquid = true;
    lava.danger = 1000.0f;
    define("lava", lava);
    Block fire;
    fire.danger = 100.0f;
    define("fire", fire);
    Block door;
    door.solid = true;
    door.open = true;
    define("door", door);
    Block slow = solid;
    slow.cost = 3.0f;
    define("slow", slow);
    Block spikes;
    spikes.danger = 50.0f;
    define("spikes", spikes);
}

int VoxelWorld::define(const std::string& name, const Block& block) {
    for (std::size_t i = 0; i < m_types.size(); ++i)
        if (m_types[i].name == name) {
            m_types[i].block = block;
            return static_cast<int>(i);
        }
    m_types.push_back(Type{name, block});
    return static_cast<int>(m_types.size() - 1);
}

int VoxelWorld::type_id(const std::string& name) const {
    for (std::size_t i = 0; i < m_types.size(); ++i)
        if (m_types[i].name == name) return static_cast<int>(i);
    return -1;
}

const VoxelWorld::Type* VoxelWorld::type(int id) const {
    return id >= 0 && id < static_cast<int>(m_types.size()) ? &m_types[static_cast<std::size_t>(id)] : nullptr;
}

void VoxelWorld::set(int x, int y, int z, int t) {
    if (t <= 0) m_cells.erase(BlockPos{x, y, z});
    else m_cells[BlockPos{x, y, z}] = t;
}

int VoxelWorld::get(int x, int y, int z) const {
    const auto it = m_cells.find(BlockPos{x, y, z});
    if (it != m_cells.end()) return it->second;
    return y < solid_floor ? 1 : 0;
}

void VoxelWorld::fill(BlockPos low, BlockPos high, int t) {
    for (int y = std::min(low.y, high.y); y <= std::max(low.y, high.y); ++y)
        for (int z = std::min(low.z, high.z); z <= std::max(low.z, high.z); ++z)
            for (int x = std::min(low.x, high.x); x <= std::max(low.x, high.x); ++x) set(x, y, z, t);
}

void VoxelWorld::clear() {
    m_cells.clear();
}

Block VoxelWorld::block(int x, int y, int z) const {
    const Type* t = type(get(x, y, z));
    return t ? t->block : Block{};
}

VoxelPathfinder VoxelWorld::pathfinder() const {
    return VoxelPathfinder([this](int x, int y, int z) { return block(x, y, z); });
}

} // namespace ai
} // namespace orchard
