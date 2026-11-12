#include <orchard/ai/navgrid.hpp>

#include <algorithm>
#include <cmath>
#include <queue>

namespace orchard {
namespace ai {

namespace {

constexpr std::int64_t k_bias = 1 << 23;
constexpr std::uint64_t k_none = ~0ull;

std::uint64_t pack(int x, int z, int layer) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x + k_bias) & 0xffffffu) << 40) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z + k_bias) & 0xffffffu) << 16) | static_cast<std::uint64_t>(layer & 0xffff);
}

void unpack(std::uint64_t r, int& x, int& z, int& layer) {
    x = static_cast<int>((r >> 40) & 0xffffffu) - static_cast<int>(k_bias);
    z = static_cast<int>((r >> 16) & 0xffffffu) - static_cast<int>(k_bias);
    layer = static_cast<int>(r & 0xffffu);
}

int floor_div(int a, int b) {
    return a >= 0 ? a / b : -((-a - 1) / b) - 1;
}

std::uint64_t tile_key(int tx, int tz) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tx)) << 32) | static_cast<std::uint32_t>(tz);
}

float flat_distance(Vec3 a, Vec3 b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

struct OpenItem {
    double f;
    double g;
    std::size_t index;
    bool operator<(const OpenItem& o) const { return f > o.f || (f == o.f && g < o.g); }
};

} // namespace

struct NavGrid::Impl {
    struct Column {
        std::vector<NavSurface> surfaces;
        std::vector<std::int8_t> walk;
    };
    struct Tile {
        std::vector<Column> columns;
    };
    struct Flow {
        std::unordered_map<std::uint64_t, float> dist;
        NavPathOptions options;
    };

    NavSettings s;
    ColumnSampler sampler;
    mutable std::unordered_map<std::uint64_t, Tile> tiles;
    std::vector<NavDanger> dangers;
    int next_danger = 1;
    std::vector<NavArea> areas;
    std::unordered_map<int, Flow> flows;
    int next_flow = 1;

    int index(float v) const { return static_cast<int>(std::floor(v / s.cell)); }
    float center(int i) const { return (static_cast<float>(i) + 0.5f) * s.cell; }
    int tile_size() const { return std::max(4, s.tile); }

    Column* column(int x, int z) const {
        if (!sampler) return nullptr;
        if (s.flat_2d && z != 0) return nullptr;
        const int t = tile_size();
        const int tx = floor_div(x, t), tz = s.flat_2d ? 0 : floor_div(z, t);
        const std::uint64_t key = tile_key(tx, tz);
        auto it = tiles.find(key);
        if (it == tiles.end()) it = tiles.emplace(key, build(tx, tz)).first;
        const int lx = x - tx * t, lz = s.flat_2d ? 0 : z - tz * t;
        return &it->second.columns[static_cast<std::size_t>(lz * t + lx)];
    }

    Tile build(int tx, int tz) const {
        Tile tile;
        const int t = tile_size();
        const int depth = s.flat_2d ? 1 : t;
        tile.columns.resize(static_cast<std::size_t>(t * depth));
        for (int lz = 0; lz < depth; ++lz)
            for (int lx = 0; lx < t; ++lx) {
                Column& c = tile.columns[static_cast<std::size_t>(lz * t + lx)];
                const float wx = center(tx * t + lx);
                const float wz = s.flat_2d ? 0.0f : center(tz * t + lz);
                sampler(wx, wz, s.top, s.bottom, c.surfaces);
                std::sort(c.surfaces.begin(), c.surfaces.end(), [](const NavSurface& a, const NavSurface& b) { return a.y > b.y; });
                if (c.surfaces.size() > 0xfff0) c.surfaces.resize(0xfff0);
                c.walk.assign(c.surfaces.size(), -1);
            }
        return tile;
    }

    const NavSurface* surface(int x, int z, int layer) const {
        Column* c = column(x, z);
        if (!c || layer < 0 || layer >= static_cast<int>(c->surfaces.size())) return nullptr;
        return &c->surfaces[static_cast<std::size_t>(layer)];
    }

    float area_cost(Vec3 p) const {
        float c = 1.0f;
        for (const NavArea& a : areas)
            if (p.x >= a.low.x && p.x <= a.high.x && p.y >= a.low.y && p.y <= a.high.y && p.z >= a.low.z && p.z <= a.high.z) c = a.cost <= 0.0f ? 0.0f : c * a.cost;
        return c;
    }

    float danger_at(Vec3 p) const {
        float d = 0.0f;
        for (const NavDanger& n : dangers) {
            const float dist = length(p - n.at);
            if (dist < n.radius) d += n.amount * (1.0f - dist / std::max(1e-3f, n.radius) * 0.6f);
        }
        return d;
    }

    bool walkable(int x, int z, int layer) const {
        Column* c = column(x, z);
        if (!c || layer < 0 || layer >= static_cast<int>(c->surfaces.size())) return false;
        std::int8_t& cached = c->walk[static_cast<std::size_t>(layer)];
        if (cached >= 0) return cached == 1;
        const NavSurface su = c->surfaces[static_cast<std::size_t>(layer)];
        bool ok = su.clearance >= s.height;
        const float keep = s.radius * 0.6f;
        const int r = static_cast<int>(std::ceil(keep / s.cell + 0.5f));
        for (int dz = s.flat_2d ? 0 : -r; ok && dz <= (s.flat_2d ? 0 : r); ++dz)
            for (int dx = -r; ok && dx <= r; ++dx) {
                if (dx == 0 && dz == 0) continue;
                const float edge = (std::sqrt(static_cast<float>(dx * dx + dz * dz)) - 0.5f) * s.cell;
                if (edge >= keep) continue;
                Column* n = column(x + dx, z + dz);
                if (!n) continue;
                for (const NavSurface& t : n->surfaces)
                    if (t.y > su.y + s.step && t.bottom < su.y + s.height - 0.05f) {
                        ok = false;
                        break;
                    }
            }
        if (ok && area_cost(Vec3{center(x), su.y, s.flat_2d ? 0.0f : center(z)}) <= 0.0f) ok = false;
        cached = ok ? 1 : 0;
        return ok;
    }

    Vec3 pos(std::uint64_t ref) const {
        int x = 0, z = 0, l = 0;
        unpack(ref, x, z, l);
        const NavSurface* su = surface(x, z, l);
        return Vec3{center(x), su ? su->y : 0.0f, s.flat_2d ? 0.0f : center(z)};
    }

    bool gap(int x, int z, float y) const {
        Column* c = column(x, z);
        if (!c) return true;
        for (const NavSurface& t : c->surfaces)
            if (t.y > y - s.step && t.bottom < y + s.height) return false;
        return true;
    }

    void neighbors(std::uint64_t ref, const NavPathOptions& o, const std::function<void(std::uint64_t, double, NavLink)>& emit) const {
        int x = 0, z = 0, l = 0;
        unpack(ref, x, z, l);
        const NavSurface* sp = surface(x, z, l);
        if (!sp) return;
        const NavSurface su = *sp;
        static const int dirs3[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        static const int dirs2[2][2] = {{1, 0}, {-1, 0}};
        const int count = s.flat_2d ? 2 : 8;
        auto level_walkable = [&](int cx, int cz, float y, int* out_layer) {
            Column* c = column(cx, cz);
            if (!c) return false;
            for (std::size_t j = 0; j < c->surfaces.size(); ++j)
                if (std::fabs(c->surfaces[j].y - y) <= s.step && walkable(cx, cz, static_cast<int>(j))) {
                    if (out_layer) *out_layer = static_cast<int>(j);
                    return true;
                }
            return false;
        };
        auto edge = [&](int nx, int nz, int nl, double len, double extra, NavLink link) {
            const NavSurface* t = surface(nx, nz, nl);
            if (!t) return;
            const Vec3 p{center(nx), t->y, s.flat_2d ? 0.0f : center(nz)};
            const float area = area_cost(p);
            if (area <= 0.0f) return;
            const float dz = danger_at(p);
            if (dz > o.max_danger) return;
            emit(pack(nx, nz, nl), len * area * (1.0 + static_cast<double>(dz) * o.danger_weight) + extra, link);
        };
        for (int i = 0; i < count; ++i) {
            const int dx = s.flat_2d ? dirs2[i][0] : dirs3[i][0];
            const int dz = s.flat_2d ? dirs2[i][1] : dirs3[i][1];
            const int nx = x + dx, nz = z + dz;
            const bool diag = dx != 0 && dz != 0;
            if (diag && (!level_walkable(x + dx, z, su.y, nullptr) || !level_walkable(x, z + dz, su.y, nullptr))) continue;
            Column* c = column(nx, nz);
            if (!c) continue;
            const double len = static_cast<double>(s.cell) * (diag ? 1.41421356 : 1.0);
            bool level = false;
            int drop_layer = -1;
            for (std::size_t j = 0; j < c->surfaces.size(); ++j) {
                const NavSurface& t = c->surfaces[j];
                const float dy = t.y - su.y;
                const int jl = static_cast<int>(j);
                if (std::fabs(dy) <= s.step) {
                    if (!walkable(nx, nz, jl)) continue;
                    if (std::min(t.clearance, su.clearance) < s.height) continue;
                    level = true;
                    edge(nx, nz, jl, std::sqrt(len * len + static_cast<double>(dy * dy)), 0.0, NavLink::walk);
                } else if (dy < -s.step && dy >= -s.max_drop && o.drops && drop_layer < 0) {
                    if (!walkable(nx, nz, jl)) continue;
                    bool clear = true;
                    for (std::size_t k = 0; k < c->surfaces.size() && clear; ++k)
                        if (k != j && c->surfaces[k].y > t.y + 0.05f && c->surfaces[k].bottom < su.y + s.height) clear = false;
                    if (clear) drop_layer = jl;
                } else if (dy > s.step && dy <= s.jump_up && o.jumps && s.jumps) {
                    if (!walkable(nx, nz, jl) || su.clearance < s.height + dy) continue;
                    edge(nx, nz, jl, len + dy, 1.0 + dy, NavLink::jump_up);
                }
            }
            if (!level && drop_layer >= 0) {
                const NavSurface& t = c->surfaces[static_cast<std::size_t>(drop_layer)];
                const float fall = su.y - t.y;
                edge(nx, nz, drop_layer, len + fall * 0.5, 0.5 + 0.3 * fall, NavLink::drop);
            }
            if (!level && !diag && o.jumps && s.jumps && s.jump_across > s.cell && gap(nx, nz, su.y)) {
                const int max_gap = static_cast<int>(std::ceil(s.jump_across / s.cell)) + 1;
                for (int g = 2; g <= max_gap; ++g) {
                    const int lx = x + dx * g, lz = z + dz * g;
                    int landing = -1;
                    Column* lc = column(lx, lz);
                    if (!lc) break;
                    for (std::size_t j = 0; j < lc->surfaces.size(); ++j) {
                        const float dy = lc->surfaces[j].y - su.y;
                        if (dy <= s.step && dy >= -s.step * 2.0f && walkable(lx, lz, static_cast<int>(j))) {
                            landing = static_cast<int>(j);
                            break;
                        }
                    }
                    if (landing >= 0) {
                        edge(lx, lz, landing, len * g, 1.5, NavLink::jump_across);
                        break;
                    }
                    if (!gap(lx, lz, su.y)) break;
                }
            }
        }
    }

    bool locate(Vec3 p, std::uint64_t& out, float search, float above = 0.0f) const {
        const int ix = index(p.x), iz = s.flat_2d ? 0 : index(p.z);
        const int r = std::max(0, static_cast<int>(std::ceil(search / s.cell)));
        double best = std::numeric_limits<double>::infinity();
        bool found = false;
        for (int ring = 0; ring <= r; ++ring) {
            for (int dz = s.flat_2d ? 0 : -ring; dz <= (s.flat_2d ? 0 : ring); ++dz)
                for (int dx = -ring; dx <= ring; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                    Column* c = column(ix + dx, iz + dz);
                    if (!c) continue;
                    for (std::size_t j = 0; j < c->surfaces.size(); ++j) {
                        const float dy = p.y - c->surfaces[j].y;
                        if (dy < -0.75f || dy > 2.5f + search + above) continue;
                        if (!walkable(ix + dx, iz + dz, static_cast<int>(j))) continue;
                        const float hx = center(ix + dx) - p.x, hz = s.flat_2d ? 0.0f : center(iz + dz) - p.z;
                        const double score = std::sqrt(static_cast<double>(hx * hx + hz * hz)) + std::fabs(dy) * 1.5;
                        if (score < best) {
                            best = score;
                            out = pack(ix + dx, iz + dz, static_cast<int>(j));
                            found = true;
                        }
                    }
                }
            if (found && ring >= 1) break;
        }
        return found;
    }

    NavPath search(std::uint64_t start, const std::vector<std::uint64_t>& goals, const std::vector<Vec3>& goal_points, const NavPathOptions& o) const {
        NavPath out;
        std::vector<std::uint64_t> nodes;
        std::vector<double> best;
        std::vector<std::size_t> parent;
        std::vector<NavLink> links;
        std::vector<char> closed;
        std::unordered_map<std::uint64_t, std::size_t> idx;
        auto intern = [&](std::uint64_t r) {
            const auto it = idx.find(r);
            if (it != idx.end()) return it->second;
            nodes.push_back(r);
            best.push_back(std::numeric_limits<double>::infinity());
            parent.push_back(static_cast<std::size_t>(-1));
            links.push_back(NavLink::walk);
            closed.push_back(0);
            idx.emplace(r, nodes.size() - 1);
            return nodes.size() - 1;
        };
        auto h = [&](std::uint64_t r) {
            const Vec3 p = pos(r);
            double m = std::numeric_limits<double>::infinity();
            for (const Vec3& g : goal_points) m = std::min(m, static_cast<double>(length(p - g)));
            return m;
        };
        std::priority_queue<OpenItem> open;
        const std::size_t s0 = intern(start);
        best[s0] = 0.0;
        open.push({h(start), 0.0, s0});
        std::size_t goal = static_cast<std::size_t>(-1), closest = s0;
        double closest_h = h(start);
        while (!open.empty()) {
            const OpenItem e = open.top();
            open.pop();
            if (closed[e.index] || e.g > best[e.index]) continue;
            closed[e.index] = 1;
            const std::uint64_t cur = nodes[e.index];
            if (std::find(goals.begin(), goals.end(), cur) != goals.end()) {
                goal = e.index;
                break;
            }
            const double hh = h(cur);
            if (hh < closest_h) {
                closest_h = hh;
                closest = e.index;
            }
            if (++out.expanded > o.max_expand) break;
            const double g = best[e.index];
            neighbors(cur, o, [&](std::uint64_t n, double step, NavLink link) {
                const std::size_t j = intern(n);
                if (closed[j]) return;
                const double ng = g + step;
                if (ng < best[j]) {
                    best[j] = ng;
                    parent[j] = e.index;
                    links[j] = link;
                    open.push({ng + h(n), ng, j});
                }
            });
        }
        std::size_t end = goal;
        if (end == static_cast<std::size_t>(-1)) {
            if (!o.partial) return out;
            end = closest;
            out.partial = true;
        } else {
            out.found = true;
        }
        for (std::size_t at = end; at != static_cast<std::size_t>(-1); at = parent[at]) out.points.push_back(NavPoint{pos(nodes[at]), links[at]});
        std::reverse(out.points.begin(), out.points.end());
        out.cost = static_cast<float>(best[end]);
        return out;
    }

    bool line(Vec3 a, Vec3 b, float max_danger) const {
        const float d = s.flat_2d ? std::fabs(b.x - a.x) : flat_distance(a, b);
        const int steps = std::max(1, static_cast<int>(std::ceil(d / (s.cell * 0.5f))));
        float y = a.y;
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float px = a.x + (b.x - a.x) * t, pz = s.flat_2d ? 0.0f : a.z + (b.z - a.z) * t;
            const int ix = index(px), iz = s.flat_2d ? 0 : index(pz);
            Column* c = column(ix, iz);
            if (!c) return false;
            bool ok = false;
            for (std::size_t j = 0; j < c->surfaces.size(); ++j)
                if (std::fabs(c->surfaces[j].y - y) <= s.step && walkable(ix, iz, static_cast<int>(j))) {
                    const Vec3 p{px, c->surfaces[j].y, pz};
                    if (danger_at(p) > max_danger) continue;
                    y = c->surfaces[j].y;
                    ok = true;
                    break;
                }
            if (!ok) return false;
        }
        return std::fabs(y - b.y) <= s.step * 2.0f;
    }

    void smooth(NavPath& path, float max_danger) const {
        const std::vector<NavPoint>& p = path.points;
        const std::size_t n = p.size();
        if (n < 3) return;
        std::vector<NavPoint> out{p.front()};
        std::size_t anchor = 0;
        std::size_t i = 1;
        while (i < n) {
            if (p[i].link != NavLink::walk) {
                if (i - 1 != anchor) out.push_back(p[i - 1]);
                out.push_back(p[i]);
                anchor = i;
                ++i;
                continue;
            }
            std::size_t j = i;
            while (j + 1 < n && p[j + 1].link == NavLink::walk && line(p[anchor].position, p[j + 1].position, max_danger)) ++j;
            out.push_back(p[j]);
            anchor = j;
            i = j + 1;
        }
        path.points = std::move(out);
    }

    void finish(NavPath& path, Vec3 to, const NavPathOptions& o) const {
        if (path.points.empty()) return;
        if (path.found && path.points.back().link == NavLink::walk) {
            Vec3 end = to;
            end.y = path.points.back().position.y;
            if (s.flat_2d) end.z = 0.0f;
            path.points.back().position = end;
        }
        if (o.smooth) smooth(path, o.max_danger);
        path.length = 0.0f;
        for (std::size_t i = 1; i < path.points.size(); ++i) path.length += length(path.points[i].position - path.points[i - 1].position);
    }

    bool predecessor_cost(std::uint64_t from, std::uint64_t to, const NavPathOptions& o, double& cost) const {
        bool found = false;
        neighbors(from, o, [&](std::uint64_t n, double step, NavLink) {
            if (n == to && (!found || step < cost)) {
                cost = step;
                found = true;
            }
        });
        return found;
    }
};

NavGrid::NavGrid() : m(std::make_unique<Impl>()) {}
NavGrid::~NavGrid() = default;
NavGrid::NavGrid(NavGrid&&) noexcept = default;
NavGrid& NavGrid::operator=(NavGrid&&) noexcept = default;

void NavGrid::configure(const NavSettings& settings, ColumnSampler sampler) {
    m->s = settings;
    m->s.cell = std::max(0.05f, m->s.cell);
    m->sampler = std::move(sampler);
    m->tiles.clear();
    m->flows.clear();
}

const NavSettings& NavGrid::settings() const {
    return m->s;
}

bool NavGrid::ready() const {
    return static_cast<bool>(m->sampler);
}

void NavGrid::clear() {
    m->tiles.clear();
    m->flows.clear();
}

void NavGrid::invalidate(Vec3 low, Vec3 high) {
    const int t = m->tile_size();
    const int x0 = floor_div(m->index(std::min(low.x, high.x)) - 2, t), x1 = floor_div(m->index(std::max(low.x, high.x)) + 2, t);
    const int z0 = m->s.flat_2d ? 0 : floor_div(m->index(std::min(low.z, high.z)) - 2, t), z1 = m->s.flat_2d ? 0 : floor_div(m->index(std::max(low.z, high.z)) + 2, t);
    for (int tz = z0; tz <= z1; ++tz)
        for (int tx = x0; tx <= x1; ++tx) m->tiles.erase(tile_key(tx, tz));
    m->flows.clear();
}

std::size_t NavGrid::bake(Vec3 low, Vec3 high) {
    if (!ready()) return 0;
    const int t = m->tile_size();
    const int x0 = floor_div(m->index(std::min(low.x, high.x)), t), x1 = floor_div(m->index(std::max(low.x, high.x)), t);
    const int z0 = m->s.flat_2d ? 0 : floor_div(m->index(std::min(low.z, high.z)), t), z1 = m->s.flat_2d ? 0 : floor_div(m->index(std::max(low.z, high.z)), t);
    std::size_t n = 0;
    for (int tz = z0; tz <= z1; ++tz)
        for (int tx = x0; tx <= x1; ++tx) {
            m->column(tx * t, tz * t);
            ++n;
        }
    return n;
}

std::size_t NavGrid::tiles() const {
    return m->tiles.size();
}

std::size_t NavGrid::surfaces() const {
    std::size_t n = 0;
    for (const auto& [k, tile] : m->tiles)
        for (const auto& c : tile.columns) n += c.surfaces.size();
    return n;
}

bool NavGrid::locate(Vec3 p, std::uint64_t& out, float search) const {
    return m->locate(p, out, search);
}

Vec3 NavGrid::position(std::uint64_t ref) const {
    return m->pos(ref);
}

bool NavGrid::on_grid(Vec3 p, float search) const {
    std::uint64_t r = 0;
    return m->locate(p, r, search);
}

Vec3 NavGrid::snap(Vec3 p, float search) const {
    std::uint64_t r = 0;
    if (!m->locate(p, r, search)) return p;
    Vec3 q = m->pos(r);
    if (m->s.flat_2d) q.z = p.z;
    return q;
}

float NavGrid::ground_height(Vec3 p, float fallback) const {
    std::uint64_t r = 0;
    return m->locate(p, r, 0.5f) ? m->pos(r).y : fallback;
}

NavPath NavGrid::find_path(Vec3 from, Vec3 to, const NavPathOptions& options) const {
    return find_path_to_any(from, {to}, options);
}

NavPath NavGrid::find_path_to_any(Vec3 from, const std::vector<Vec3>& goals, const NavPathOptions& options) const {
    NavPath out;
    std::uint64_t start = 0;
    if (!ready() || goals.empty() || !m->locate(from, start, 2.0f)) return out;
    std::vector<std::uint64_t> refs;
    std::vector<Vec3> points;
    for (const Vec3& g : goals) {
        std::uint64_t r = 0;
        if (m->locate(g, r, 3.0f)) {
            refs.push_back(r);
            points.push_back(g);
        } else if (m->locate(g, r, 3.0f, 60.0f)) {
            refs.push_back(r);
            points.push_back(m->pos(r));
        }
    }
    if (refs.empty() && !options.partial) return out;
    NavPathOptions o = options;
    if (refs.empty()) o.max_expand = std::min<std::size_t>(o.max_expand, 2000);
    out = m->search(start, refs, refs.empty() ? goals : points, o);
    m->finish(out, goals.size() == 1 ? goals.front() : (out.points.empty() ? from : out.points.back().position), options);
    return out;
}

NavPath NavGrid::flee_path(Vec3 from, Vec3 threat, float distance, const NavPathOptions& o) const {
    NavPath out;
    std::uint64_t start = 0;
    if (!ready() || !m->locate(from, start, 2.0f)) return out;
    std::unordered_map<std::uint64_t, double> cost;
    std::unordered_map<std::uint64_t, std::pair<std::uint64_t, NavLink>> parent;
    using Item = std::pair<double, std::uint64_t>;
    auto cmp = [](const Item& a, const Item& b) { return a.first > b.first; };
    std::priority_queue<Item, std::vector<Item>, decltype(cmp)> open(cmp);
    open.push({0.0, start});
    cost[start] = 0.0;
    std::uint64_t best = start;
    double best_score = -1e18;
    while (!open.empty() && out.expanded < o.max_expand) {
        const Item it = open.top();
        open.pop();
        if (it.first > cost[it.second]) continue;
        ++out.expanded;
        const float away = length(m->pos(it.second) - threat);
        const double score = static_cast<double>(away) * 3.0 - it.first;
        if (score > best_score) {
            best_score = score;
            best = it.second;
        }
        if (away >= distance) {
            best = it.second;
            out.found = true;
            break;
        }
        m->neighbors(it.second, o, [&](std::uint64_t n, double step, NavLink link) {
            const float near = length(m->pos(n) - threat);
            const double nc = it.first + step + std::max(0.0, 4.0 - static_cast<double>(near)) * 3.0;
            const auto f = cost.find(n);
            if (f == cost.end() || nc < f->second) {
                cost[n] = nc;
                parent[n] = {it.second, link};
                open.push({nc, n});
            }
        });
    }
    for (std::uint64_t at = best;;) {
        const auto pr = parent.find(at);
        out.points.push_back(NavPoint{m->pos(at), pr == parent.end() ? NavLink::walk : pr->second.second});
        if (pr == parent.end() || at == start) break;
        at = pr->second.first;
    }
    std::reverse(out.points.begin(), out.points.end());
    out.partial = !out.found;
    out.cost = static_cast<float>(cost[best]);
    NavPathOptions so = o;
    so.partial = true;
    NavPath copy = out;
    copy.found = false;
    m->finish(copy, out.points.back().position, so);
    copy.found = out.found;
    return copy;
}

bool NavGrid::walkable_line(Vec3 a, Vec3 b, float max_danger) const {
    std::uint64_t r = 0;
    if (!ready() || !m->locate(a, r, 0.75f)) return false;
    const Vec3 start = m->pos(r);
    Vec3 aa = a;
    aa.y = start.y;
    return m->line(aa, Vec3{b.x, b.y, b.z}, max_danger);
}

float NavGrid::path_distance(Vec3 a, Vec3 b, std::size_t max_expand) const {
    NavPathOptions o;
    o.max_expand = max_expand;
    o.partial = false;
    const NavPath p = find_path(a, b, o);
    return p.found ? p.length : std::numeric_limits<float>::infinity();
}

bool NavGrid::reachable(Vec3 a, Vec3 b, std::size_t max_expand) const {
    return std::isfinite(path_distance(a, b, max_expand));
}

void NavGrid::neighbors(std::uint64_t ref, const NavPathOptions& options, const std::function<void(std::uint64_t, double, NavLink)>& emit) const {
    m->neighbors(ref, options, emit);
}

int NavGrid::add_danger(Vec3 at, float radius, float amount, float fade) {
    NavDanger d;
    d.at = at;
    d.radius = std::max(0.01f, radius);
    d.amount = amount;
    d.fade = std::max(0.0f, fade);
    d.id = m->next_danger++;
    m->dangers.push_back(d);
    return d.id;
}

void NavGrid::remove_danger(int id) {
    m->dangers.erase(std::remove_if(m->dangers.begin(), m->dangers.end(), [id](const NavDanger& d) { return d.id == id; }), m->dangers.end());
}

void NavGrid::clear_danger() {
    m->dangers.clear();
}

float NavGrid::danger_at(Vec3 p) const {
    return m->danger_at(p);
}

const std::vector<NavDanger>& NavGrid::dangers() const {
    return m->dangers;
}

void NavGrid::set_area_cost(Vec3 low, Vec3 high, float cost) {
    NavArea a;
    a.low = Vec3{std::min(low.x, high.x), std::min(low.y, high.y), std::min(low.z, high.z)};
    a.high = Vec3{std::max(low.x, high.x), std::max(low.y, high.y), std::max(low.z, high.z)};
    a.cost = cost;
    m->areas.push_back(a);
    for (auto& [k, tile] : m->tiles)
        for (auto& c : tile.columns) std::fill(c.walk.begin(), c.walk.end(), static_cast<std::int8_t>(-1));
}

void NavGrid::clear_areas() {
    m->areas.clear();
    for (auto& [k, tile] : m->tiles)
        for (auto& c : tile.columns) std::fill(c.walk.begin(), c.walk.end(), static_cast<std::int8_t>(-1));
}

float NavGrid::area_cost(Vec3 p) const {
    return m->area_cost(p);
}

void NavGrid::tick(float dt) {
    for (NavDanger& d : m->dangers)
        if (d.fade > 0.0f) d.amount -= d.fade * dt;
    m->dangers.erase(std::remove_if(m->dangers.begin(), m->dangers.end(), [](const NavDanger& d) { return d.fade > 0.0f && d.amount <= 0.0f; }), m->dangers.end());
}

int NavGrid::make_flow(const std::vector<Vec3>& targets, float max_cost, const NavPathOptions& options) {
    Impl::Flow flow;
    flow.options = options;
    using Item = std::pair<double, std::uint64_t>;
    auto cmp = [](const Item& a, const Item& b) { return a.first > b.first; };
    std::priority_queue<Item, std::vector<Item>, decltype(cmp)> open(cmp);
    for (const Vec3& t : targets) {
        std::uint64_t r = 0;
        if (!m->locate(t, r, 3.0f)) continue;
        flow.dist[r] = 0.0f;
        open.push({0.0, r});
    }
    const int reach = m->s.jumps ? static_cast<int>(std::ceil(m->s.jump_across / m->s.cell)) + 1 : 1;
    std::size_t expanded = 0;
    while (!open.empty() && expanded < options.max_expand * 4) {
        const Item it = open.top();
        open.pop();
        const auto found = flow.dist.find(it.second);
        if (found != flow.dist.end() && it.first > found->second) continue;
        ++expanded;
        int x = 0, z = 0, l = 0;
        unpack(it.second, x, z, l);
        for (int dz = m->s.flat_2d ? 0 : -reach; dz <= (m->s.flat_2d ? 0 : reach); ++dz)
            for (int dx = -reach; dx <= reach; ++dx) {
                if (dx == 0 && dz == 0) continue;
                if (std::max(std::abs(dx), std::abs(dz)) > 1 && dx != 0 && dz != 0) continue;
                Impl::Column* c = m->column(x + dx, z + dz);
                if (!c) continue;
                for (std::size_t j = 0; j < c->surfaces.size(); ++j) {
                    if (!m->walkable(x + dx, z + dz, static_cast<int>(j))) continue;
                    const std::uint64_t from = pack(x + dx, z + dz, static_cast<int>(j));
                    double step = 0.0;
                    if (!m->predecessor_cost(from, it.second, options, step)) continue;
                    const double nd = it.first + step;
                    if (nd > max_cost) continue;
                    const auto f = flow.dist.find(from);
                    if (f == flow.dist.end() || nd < f->second) {
                        flow.dist[from] = static_cast<float>(nd);
                        open.push({nd, from});
                    }
                }
            }
    }
    const int id = m->next_flow++;
    m->flows[id] = std::move(flow);
    return id;
}

Vec3 NavGrid::flow_direction(int id, Vec3 p) const {
    const auto it = m->flows.find(id);
    if (it == m->flows.end()) return Vec3{};
    std::uint64_t r = 0;
    if (!m->locate(p, r, 1.5f)) return Vec3{};
    const auto here = it->second.dist.find(r);
    if (here == it->second.dist.end() || here->second <= 0.0f) return Vec3{};
    float best = here->second;
    std::uint64_t to = r;
    m->neighbors(r, it->second.options, [&](std::uint64_t n, double, NavLink) {
        const auto d = it->second.dist.find(n);
        if (d != it->second.dist.end() && d->second < best) {
            best = d->second;
            to = n;
        }
    });
    if (to == r) return Vec3{};
    Vec3 d = m->pos(to) - m->pos(r);
    if (m->s.flat_2d) d.z = 0.0f;
    const float len = length(d);
    return len > 1e-5f ? d * (1.0f / len) : Vec3{};
}

float NavGrid::flow_distance(int id, Vec3 p) const {
    const auto it = m->flows.find(id);
    std::uint64_t r = 0;
    if (it == m->flows.end() || !m->locate(p, r, 1.5f)) return std::numeric_limits<float>::infinity();
    const auto d = it->second.dist.find(r);
    return d == it->second.dist.end() ? std::numeric_limits<float>::infinity() : d->second;
}

void NavGrid::drop_flow(int id) {
    m->flows.erase(id);
}

Vec3 NavGrid::random_point(Random& random, Vec3 near, float radius, int tries) const {
    for (int i = 0; i < tries; ++i) {
        Vec3 o = m->s.flat_2d ? Vec3{random.rangef(-radius, radius), 0.0f, 0.0f} : random.in_circle(radius);
        Vec3 p = near + o;
        std::uint64_t r = 0;
        if (m->locate(p, r, m->s.cell * 2.0f)) {
            Vec3 q = m->pos(r);
            if (m->s.flat_2d) q.z = near.z;
            return q;
        }
    }
    return near;
}

std::vector<Vec3> NavGrid::sample(Vec3 center, float radius, float spacing, int max_points) const {
    std::vector<Vec3> out;
    spacing = std::max(m->s.cell, spacing);
    const int n = static_cast<int>(std::ceil(radius / spacing));
    std::vector<std::uint64_t> seen;
    for (int dz = m->s.flat_2d ? 0 : -n; dz <= (m->s.flat_2d ? 0 : n); ++dz)
        for (int dx = -n; dx <= n; ++dx) {
            const Vec3 p{center.x + static_cast<float>(dx) * spacing, center.y, center.z + static_cast<float>(dz) * spacing};
            if (flat_distance(p, center) > radius + 1e-3f) continue;
            std::uint64_t r = 0;
            if (!m->locate(p, r, spacing * 0.5f)) continue;
            if (std::find(seen.begin(), seen.end(), r) != seen.end()) continue;
            seen.push_back(r);
            Vec3 q = m->pos(r);
            if (m->s.flat_2d) q.z = center.z;
            out.push_back(q);
            if (static_cast<int>(out.size()) >= max_points) return out;
        }
    return out;
}

} // namespace ai
} // namespace orchard
