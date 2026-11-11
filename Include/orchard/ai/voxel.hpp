#ifndef ORCHARD_AI_VOXEL_HPP
#define ORCHARD_AI_VOXEL_HPP

#include "search.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchard {
namespace ai {

struct Block {
    bool solid = false;
    bool climbable = false;
    bool liquid = false;
    bool open = false;
    float cost = 1.0f;
    float danger = 0.0f;
};

struct BlockPos {
    int x = 0;
    int y = 0;
    int z = 0;
    bool operator==(const BlockPos& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const BlockPos& o) const { return !(*this == o); }
};

struct BlockPosHash {
    std::size_t operator()(const BlockPos& p) const {
        std::uint64_t h = static_cast<std::uint32_t>(p.x) * 73856093ull ^ static_cast<std::uint32_t>(p.y) * 19349663ull ^ static_cast<std::uint32_t>(p.z) * 83492791ull;
        return static_cast<std::size_t>(h ^ (h >> 29));
    }
};

enum class VoxelMove : std::uint8_t { walk, step_up, drop, jump, climb, swim, door };

struct VoxelStep {
    BlockPos at;
    VoxelMove move = VoxelMove::walk;
};

struct VoxelAgent {
    int height = 2;
    int step_up = 1;
    int jump_up = 1;
    int jump_across = 0;
    int max_drop = 3;
    bool swim = true;
    bool climb = true;
    bool diagonal = true;
    bool open_doors = true;
    float danger_weight = 10.0f;
    float max_danger = 999.0f;
    float drop_cost = 0.5f;
    float jump_cost = 1.0f;
    float climb_cost = 1.5f;
    float swim_cost = 2.0f;
    std::size_t max_expand = 200000;
};

struct VoxelPath {
    std::vector<VoxelStep> steps;
    float cost = 0.0f;
    bool found = false;
    bool partial = false;
    std::size_t expanded = 0;
};

class VoxelPathfinder {
public:
    using Query = std::function<Block(int x, int y, int z)>;

    explicit VoxelPathfinder(Query query = {}) : m_query(std::move(query)) {}
    void set_query(Query query) { m_query = std::move(query); }

    bool standable(const BlockPos& p, const VoxelAgent& agent) const;
    VoxelPath find(BlockPos from, BlockPos to, const VoxelAgent& agent, bool allow_partial = true) const;
    VoxelPath find_any(BlockPos from, const std::vector<BlockPos>& goals, const VoxelAgent& agent) const;
    VoxelPath flee(BlockPos from, BlockPos threat, int distance, const VoxelAgent& agent) const;
    void neighbors(const BlockPos& p, const VoxelAgent& agent, const std::function<void(const BlockPos&, double, VoxelMove)>& emit) const;
    int ground_below(BlockPos p, int max_depth) const;

private:
    Block block(int x, int y, int z) const { return m_query ? m_query(x, y, z) : Block{}; }
    bool passable(int x, int y, int z, const VoxelAgent& agent) const;
    bool body_fits(int x, int y, int z, const VoxelAgent& agent) const;

    Query m_query;
};

class VoxelWorld {
public:
    struct Type {
        std::string name;
        Block block;
    };

    VoxelWorld();
    int define(const std::string& name, const Block& block);
    int type_id(const std::string& name) const;
    const Type* type(int id) const;
    void set(int x, int y, int z, int type);
    int get(int x, int y, int z) const;
    void fill(BlockPos low, BlockPos high, int type);
    void clear();
    std::size_t count() const { return m_cells.size(); }
    Block block(int x, int y, int z) const;
    VoxelPathfinder pathfinder() const;
    int solid_floor = std::numeric_limits<int>::min();

private:
    std::vector<Type> m_types;
    std::unordered_map<BlockPos, int, BlockPosHash> m_cells;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_VOXEL_HPP
