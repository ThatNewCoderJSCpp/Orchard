#ifndef ORCHARD_AI_NAVGRID_HPP
#define ORCHARD_AI_NAVGRID_HPP

#include "../math.hpp"
#include "../random.hpp"
#include "../scene.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace orchard {
namespace ai {

struct NavSettings {
    float cell = 0.5f;
    float radius = 0.4f;
    float height = 1.8f;
    float step = 0.45f;
    float max_slope = 50.0f;
    float max_drop = 4.0f;
    float jump_up = 1.1f;
    float jump_across = 2.0f;
    bool jumps = true;
    float top = 120.0f;
    float bottom = -60.0f;
    int tile = 16;
    bool flat_2d = false;
};

struct NavSurface {
    float y = 0.0f;
    float bottom = 0.0f;
    float clearance = 1.0e6f;
};

using ColumnSampler = std::function<void(float x, float z, float top, float bottom, std::vector<NavSurface>& out)>;

enum class NavLink : std::uint8_t { walk, drop, jump_up, jump_across };

struct NavPoint {
    Vec3 position;
    NavLink link = NavLink::walk;
};

struct NavPathOptions {
    float danger_weight = 4.0f;
    float max_danger = std::numeric_limits<float>::infinity();
    bool jumps = true;
    bool drops = true;
    bool smooth = true;
    bool partial = true;
    std::size_t max_expand = 80000;
};

struct NavPath {
    std::vector<NavPoint> points;
    bool found = false;
    bool partial = false;
    float length = 0.0f;
    float cost = 0.0f;
    std::size_t expanded = 0;
};

struct NavDanger {
    Vec3 at;
    float radius = 1.0f;
    float amount = 1.0f;
    float fade = 0.0f;
    int id = 0;
};

struct NavArea {
    Vec3 low;
    Vec3 high;
    float cost = 1.0f;
};

class NavGrid {
public:
    NavGrid();
    ~NavGrid();
    NavGrid(NavGrid&&) noexcept;
    NavGrid& operator=(NavGrid&&) noexcept;

    void configure(const NavSettings& settings, ColumnSampler sampler);
    const NavSettings& settings() const;
    bool ready() const;
    void clear();
    void invalidate(Vec3 low, Vec3 high);
    std::size_t bake(Vec3 low, Vec3 high);
    std::size_t tiles() const;
    std::size_t surfaces() const;

    bool locate(Vec3 p, std::uint64_t& out, float search = 2.0f) const;
    Vec3 position(std::uint64_t ref) const;
    bool on_grid(Vec3 p, float search = 1.0f) const;
    Vec3 snap(Vec3 p, float search = 3.0f) const;
    float ground_height(Vec3 p, float fallback) const;

    NavPath find_path(Vec3 from, Vec3 to, const NavPathOptions& options = NavPathOptions()) const;
    NavPath find_path_to_any(Vec3 from, const std::vector<Vec3>& goals, const NavPathOptions& options = NavPathOptions()) const;
    NavPath flee_path(Vec3 from, Vec3 threat, float distance, const NavPathOptions& options = NavPathOptions()) const;
    bool walkable_line(Vec3 a, Vec3 b, float max_danger = std::numeric_limits<float>::infinity()) const;
    float path_distance(Vec3 a, Vec3 b, std::size_t max_expand = 20000) const;
    bool reachable(Vec3 a, Vec3 b, std::size_t max_expand = 20000) const;

    void neighbors(std::uint64_t ref, const NavPathOptions& options, const std::function<void(std::uint64_t, double, NavLink)>& emit) const;

    int add_danger(Vec3 at, float radius, float amount, float fade_per_second = 0.0f);
    void remove_danger(int id);
    void clear_danger();
    float danger_at(Vec3 p) const;
    const std::vector<NavDanger>& dangers() const;
    void set_area_cost(Vec3 low, Vec3 high, float cost);
    void clear_areas();
    float area_cost(Vec3 p) const;
    void tick(float dt);

    int make_flow(const std::vector<Vec3>& targets, float max_cost = 200.0f, const NavPathOptions& options = NavPathOptions());
    Vec3 flow_direction(int flow, Vec3 p) const;
    float flow_distance(int flow, Vec3 p) const;
    void drop_flow(int flow);

    Vec3 random_point(Random& random, Vec3 near, float radius, int tries = 24) const;
    std::vector<Vec3> sample(Vec3 center, float radius, float spacing, int max_points = 256) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace ai
} // namespace orchard

#endif // ORCHARD_AI_NAVGRID_HPP
