#include <orchard/solid.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace orchard {

namespace {

struct P3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

P3 operator+(P3 a, P3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
P3 operator-(P3 a, P3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
P3 operator*(P3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot3(P3 a, P3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P3 cross3(P3 a, P3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
P3 to_p3(Vec3 v) { return {v.x, v.y, v.z}; }
Vec3 to_v3(P3 p) { return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)}; }

constexpr double split_epsilon = 1e-5;

struct Plane {
    P3 n;
    double w = 0.0;
};

struct Poly {
    std::vector<P3> v;
    Plane plane;
    int color = 0;
};

bool make_plane(const std::vector<P3>& v, Plane& out) {
    P3 n;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const P3& a = v[i];
        const P3& b = v[(i + 1) % v.size()];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    const double len = std::sqrt(dot3(n, n));
    if (!(len > 1e-12)) return false;
    out.n = n * (1.0 / len);
    P3 c;
    for (const P3& p : v) c = c + p;
    out.w = dot3(out.n, c * (1.0 / static_cast<double>(v.size())));
    return true;
}

void flip(Poly& p) {
    std::reverse(p.v.begin(), p.v.end());
    p.plane.n = p.plane.n * -1.0;
    p.plane.w = -p.plane.w;
}

enum : int { coplanar = 0, in_front = 1, behind = 2, spanning = 3 };

void split(const Plane& pl, const Poly& poly, std::vector<Poly>& coplanar_front, std::vector<Poly>& coplanar_back, std::vector<Poly>& front, std::vector<Poly>& back) {
    int kind = 0;
    std::vector<int> types(poly.v.size());
    for (std::size_t i = 0; i < poly.v.size(); ++i) {
        const double t = dot3(pl.n, poly.v[i]) - pl.w;
        types[i] = t < -split_epsilon ? behind : t > split_epsilon ? in_front : coplanar;
        kind |= types[i];
    }
    if (kind == coplanar) {
        (dot3(pl.n, poly.plane.n) > 0.0 ? coplanar_front : coplanar_back).push_back(poly);
        return;
    }
    if (kind == in_front) {
        front.push_back(poly);
        return;
    }
    if (kind == behind) {
        back.push_back(poly);
        return;
    }
    std::vector<P3> f, b;
    for (std::size_t i = 0; i < poly.v.size(); ++i) {
        const std::size_t j = (i + 1) % poly.v.size();
        const int ti = types[i], tj = types[j];
        const P3 vi = poly.v[i], vj = poly.v[j];
        if (ti != behind) f.push_back(vi);
        if (ti != in_front) b.push_back(vi);
        if ((ti | tj) == spanning) {
            const double den = dot3(pl.n, vj - vi);
            const double t = den != 0.0 ? (pl.w - dot3(pl.n, vi)) / den : 0.0;
            const P3 at = vi + (vj - vi) * t;
            f.push_back(at);
            b.push_back(at);
        }
    }
    if (f.size() >= 3) front.push_back(Poly{std::move(f), poly.plane, poly.color});
    if (b.size() >= 3) back.push_back(Poly{std::move(b), poly.plane, poly.color});
}

class Bsp {
public:
    Bsp() { m_nodes.emplace_back(); }

    void build(std::vector<Poly> polys) {
        std::vector<std::pair<std::size_t, std::vector<Poly>>> work;
        work.emplace_back(0, std::move(polys));
        while (!work.empty()) {
            const std::size_t i = work.back().first;
            std::vector<Poly> list = std::move(work.back().second);
            work.pop_back();
            if (list.empty()) continue;
            if (!m_nodes[i].has) {
                m_nodes[i].plane = list.front().plane;
                m_nodes[i].has = true;
            }
            const Plane pl = m_nodes[i].plane;
            std::vector<Poly> f, b;
            for (const Poly& p : list) split(pl, p, m_nodes[i].polys, m_nodes[i].polys, f, b);
            if (!f.empty()) {
                if (m_nodes[i].front < 0) {
                    m_nodes[i].front = static_cast<long>(m_nodes.size());
                    m_nodes.emplace_back();
                }
                work.emplace_back(static_cast<std::size_t>(m_nodes[i].front), std::move(f));
            }
            if (!b.empty()) {
                if (m_nodes[i].back < 0) {
                    m_nodes[i].back = static_cast<long>(m_nodes.size());
                    m_nodes.emplace_back();
                }
                work.emplace_back(static_cast<std::size_t>(m_nodes[i].back), std::move(b));
            }
        }
    }

    void invert() {
        for (Node& n : m_nodes) {
            for (Poly& p : n.polys) flip(p);
            n.plane.n = n.plane.n * -1.0;
            n.plane.w = -n.plane.w;
            std::swap(n.front, n.back);
        }
    }

    std::vector<Poly> clip_polygons(std::vector<Poly> in) const {
        std::vector<Poly> out;
        std::vector<std::pair<long, std::vector<Poly>>> work;
        work.emplace_back(0, std::move(in));
        while (!work.empty()) {
            const long i = work.back().first;
            std::vector<Poly> list = std::move(work.back().second);
            work.pop_back();
            const Node& n = m_nodes[static_cast<std::size_t>(i)];
            if (!n.has) {
                for (Poly& p : list) out.push_back(std::move(p));
                continue;
            }
            std::vector<Poly> f, b;
            for (const Poly& p : list) split(n.plane, p, f, b, f, b);
            if (n.front >= 0) work.emplace_back(n.front, std::move(f));
            else for (Poly& p : f) out.push_back(std::move(p));
            if (n.back >= 0) work.emplace_back(n.back, std::move(b));
        }
        return out;
    }

    void clip_to(const Bsp& other) {
        for (Node& n : m_nodes) n.polys = other.clip_polygons(std::move(n.polys));
    }

    std::vector<Poly> all() const {
        std::vector<Poly> out;
        for (const Node& n : m_nodes) out.insert(out.end(), n.polys.begin(), n.polys.end());
        return out;
    }

private:
    struct Node {
        Plane plane;
        bool has = false;
        long front = -1;
        long back = -1;
        std::vector<Poly> polys;
    };

    std::vector<Node> m_nodes;
};

std::vector<Poly> to_polys(const SolidMesh& m, const std::vector<int>& remap) {
    std::vector<Poly> out;
    out.reserve(m.triangles());
    for (std::size_t t = 0; t < m.triangles(); ++t) {
        Poly p;
        p.v = {to_p3(m.positions[t * 3]), to_p3(m.positions[t * 3 + 1]), to_p3(m.positions[t * 3 + 2])};
        if (!make_plane(p.v, p.plane)) continue;
        const int c = m.colors[t];
        p.color = c >= 0 && static_cast<std::size_t>(c) < remap.size() ? remap[static_cast<std::size_t>(c)] : 0;
        out.push_back(std::move(p));
    }
    return out;
}

void add_polys(SolidMesh& out, const std::vector<Poly>& polys) {
    for (const Poly& p : polys) {
        for (std::size_t k = 2; k < p.v.size(); ++k) {
            const P3 a = p.v[0], b = p.v[k - 1], c = p.v[k];
            const P3 n = cross3(b - a, c - a);
            if (dot3(n, n) <= 1e-20) continue;
            out.add_triangle(to_v3(a), to_v3(b), to_v3(c), p.color);
        }
    }
}

bool overlap(const SolidMesh& a, const SolidMesh& b) {
    Vec3 alo, ahi, blo, bhi;
    if (!a.bounds(alo, ahi) || !b.bounds(blo, bhi)) return false;
    const float pad = 1e-4f;
    return alo.x <= bhi.x + pad && blo.x <= ahi.x + pad && alo.y <= bhi.y + pad && blo.y <= ahi.y + pad && alo.z <= bhi.z + pad && blo.z <= ahi.z + pad;
}

enum class Op { join, cut, keep };

SolidMesh run(const SolidMesh& a, const SolidMesh& b, Op op) {
    SolidMesh out;
    out.palette = a.palette;
    std::vector<int> ra(a.palette.size()), rb(b.palette.size());
    for (std::size_t i = 0; i < ra.size(); ++i) ra[i] = static_cast<int>(i);
    for (std::size_t i = 0; i < rb.size(); ++i) rb[i] = out.add_color(b.palette[i]);
    if (a.empty() || b.empty() || !overlap(a, b)) {
        if (op == Op::keep) return out;
        add_polys(out, to_polys(a, ra));
        if (op == Op::join) add_polys(out, to_polys(b, rb));
        return out;
    }
    Bsp A, B;
    A.build(to_polys(a, ra));
    B.build(to_polys(b, rb));
    if (op == Op::join) {
        A.clip_to(B);
        B.clip_to(A);
        B.invert();
        B.clip_to(A);
        B.invert();
        A.build(B.all());
    } else if (op == Op::cut) {
        A.invert();
        A.clip_to(B);
        B.clip_to(A);
        B.invert();
        B.clip_to(A);
        B.invert();
        A.build(B.all());
        A.invert();
    } else {
        A.invert();
        B.clip_to(A);
        B.invert();
        A.clip_to(B);
        B.clip_to(A);
        A.build(B.all());
        A.invert();
    }
    add_polys(out, A.all());
    return out;
}

void add_outward(SolidMesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 center) {
    const Vec3 n = cross(b - a, c - a);
    if (dot(n, n) <= 1e-14f) return;
    const Vec3 mid = (a + b + c) * (1.0f / 3.0f);
    if (dot(n, mid - center) < 0.0f) m.add_triangle(a, c, b, 0);
    else m.add_triangle(a, b, c, 0);
}

void add_rows(SolidMesh& m, const std::vector<std::vector<Vec3>>& rows, Vec3 center) {
    for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
        const std::vector<Vec3>& r0 = rows[i];
        const std::vector<Vec3>& r1 = rows[i + 1];
        const std::size_t n = std::min(r0.size(), r1.size());
        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t k = (j + 1) % n;
            add_outward(m, r0[j], r0[k], r1[j], center);
            add_outward(m, r0[k], r1[k], r1[j], center);
        }
    }
}

Vec3 column(const Mat4& m, int c) { return {m.at(0, c), m.at(1, c), m.at(2, c)}; }

Quat quat_from_basis(Vec3 x, Vec3 y, Vec3 z) {
    const float m00 = x.x, m01 = y.x, m02 = z.x;
    const float m10 = x.y, m11 = y.y, m12 = z.y;
    const float m20 = x.z, m21 = y.z, m22 = z.z;
    Quat q;
    const float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return normalize(q);
}

Mat4 inverse_or_identity(const Mat4& m) {
    Mat4 out;
    if (!invert(m, out)) return Mat4{};
    return out;
}

Mat4 parent_world(const Scene& scene, Entity e) {
    const Entity p = scene.parent(e);
    return scene.alive(p) ? scene.world_matrix(p) : Mat4{};
}

std::string unique_stem(const std::filesystem::path& dir, const std::string& base) {
    std::error_code ec;
    for (int i = 1;; ++i) {
        const std::string stem = i == 1 ? base : base + std::to_string(i);
        if (!std::filesystem::exists(dir / (stem + ".obj"), ec) && !std::filesystem::exists(dir / (stem + ".parts"), ec)) return stem;
    }
}

std::string join_path(const std::string& root, const std::string& rel) {
    if (root.empty()) return rel;
    return (std::filesystem::path(root) / std::filesystem::path(rel)).string();
}

std::vector<Entity> part_roots(const Scene& scene, const std::vector<Entity>& picked) {
    std::vector<Entity> out;
    std::set<std::uint64_t> chosen;
    for (Entity e : picked)
        if (scene.alive(e)) chosen.insert(e.id());
    for (Entity e : picked) {
        if (!scene.alive(e)) continue;
        bool inside = false;
        for (Entity at = scene.parent(e); scene.alive(at); at = scene.parent(at))
            if (chosen.count(at.id())) inside = true;
        if (inside) continue;
        if (std::find(out.begin(), out.end(), e) == out.end()) out.push_back(e);
    }
    return out;
}

Vec3 part_color(const Scene& scene, Entity e) {
    for (Entity at = e; scene.alive(at); at = scene.parent(at))
        if (const Material* m = scene.material(at)) return m->color;
    return Material{}.color;
}

bool same_material(const Material& a, const Material& b) {
    return a.color.x == b.color.x && a.color.y == b.color.y && a.color.z == b.color.z && a.metallic == b.metallic && a.roughness == b.roughness && a.preset == b.preset && a.base_map == b.base_map;
}

struct Built {
    SolidMesh mesh;
    std::vector<std::string> notes;
    std::string error;
    std::optional<Material> material;
    std::optional<Body> body;
    std::optional<Collider> collider;
};

Built build_mesh(const Scene& scene, const std::vector<Entity>& roots, SolidOperation operation, const Mat4& into, const SolidOptions& options) {
    Built out;
    std::vector<SolidMesh> positive, negative;
    std::vector<const Material*> materials;
    bool any_without_material = false;
    for (Entity root : roots) {
        const bool root_negative = is_negative_part(scene, root);
        for (Entity e : scene.subtree(root)) {
            const std::vector<Collider>* cs = scene.colliders(e);
            if (!cs || cs->empty()) continue;
            std::string why;
            std::optional<SolidMesh> m = entity_solid(scene, e, into, options.loader, &why);
            if (!why.empty()) out.notes.push_back(scene.name(e) + ": " + why);
            if (!m || m->empty()) continue;
            const bool neg = root_negative || is_negative_part(scene, e);
            if (neg) {
                negative.push_back(std::move(*m));
                continue;
            }
            if (!out.collider) {
                for (const Collider& c : *cs)
                    if (c.enabled && !c.sensor) {
                        out.collider = c;
                        break;
                    }
            }
            if (!out.body) {
                const Entity owner = scene.body_owner(e);
                if (const Body* b = scene.body(owner)) out.body = *b;
            }
            const Material* mat = nullptr;
            for (Entity at = e; scene.alive(at) && !mat; at = scene.parent(at)) mat = scene.material(at);
            if (mat) materials.push_back(mat);
            else any_without_material = true;
            positive.push_back(std::move(*m));
        }
    }
    if (positive.empty()) {
        out.error = negative.empty() ? "pick parts that have colliders (boxes, spheres, cylinders, meshes...)" : "every part picked is negative: pick at least one part to cut them out of";
        return out;
    }
    SolidMesh result = std::move(positive.front());
    for (std::size_t i = 1; i < positive.size(); ++i) result = operation == SolidOperation::intersect_parts ? solid_intersect(result, positive[i]) : solid_union(result, positive[i]);
    for (const SolidMesh& n : negative) result = solid_subtract(result, n);
    if (result.empty()) {
        out.error = operation == SolidOperation::intersect_parts ? "those parts don't overlap, so nothing is left where they all meet" : "the negative parts cut everything away";
        return out;
    }
    if (!materials.empty()) {
        bool same = !any_without_material;
        for (const Material* m : materials) same = same && same_material(*m, *materials.front());
        if (same || options.one_color) out.material = *materials.front();
    } else if (options.one_color) {
        out.material = Material{};
    }
    if (out.material) result.set_color(out.material->color);
    out.mesh = std::move(result);
    return out;
}

bool write_parts(const Scene& scene, const std::vector<Entity>& roots, const Mat4& combined_world, const std::string& path, std::string* error) {
    Scene parts(scene.dimension());
    const Mat4 inv = inverse_or_identity(combined_world);
    for (Entity r : roots) {
        const Entity copy = parts.copy_from(scene, r);
        if (Transform* t = parts.transform(copy)) *t = transform_from_matrix(inv * scene.world_matrix(r));
    }
    if (!parts.save_file(path)) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

std::string clean_stem(const std::string& name) {
    std::string out;
    for (char c : name) out += std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ? c : '_';
    return out.empty() ? std::string("Solid") : out;
}

bool write_mesh_file(const Built& built, const Mat4& combined_world, const std::string& name, const SolidOptions& options, std::string& mesh_rel, std::string& stem, std::string* error) {
    SolidMesh local = built.mesh;
    local.transform(inverse_or_identity(combined_world));
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::path(join_path(options.project_root, options.folder));
    std::filesystem::create_directories(dir, ec);
    stem = unique_stem(dir, clean_stem(name));
    mesh_rel = options.folder + "/" + stem + ".obj";
    return write_solid_obj(local, join_path(options.project_root, mesh_rel), options.z_up, !built.material, error);
}

void use_mesh(Scene& scene, Entity e, const Built& built, const std::string& mesh_rel) {
    std::vector<Collider>* cs = scene.colliders(e);
    bool kept = false;
    if (cs)
        for (Collider& c : *cs)
            if ((c.shape == ColliderShape::mesh || c.shape == ColliderShape::convex_hull) && !c.source.empty()) {
                c.source = mesh_rel;
                kept = true;
            }
    if (kept) {
        if (built.material) scene.add_material(e) = *built.material;
        else scene.remove_material(e);
        return;
    }
    while (cs && !cs->empty()) scene.remove_collider(e, cs->size() - 1);
    const Body* b = scene.body(e);
    const bool fixed = !b || b->type == BodyType::fixed;
    Collider& c = *scene.add_collider(e, fixed ? ColliderShape::mesh : ColliderShape::convex_hull);
    if (built.collider) {
        c = *built.collider;
        c.shape = fixed ? ColliderShape::mesh : ColliderShape::convex_hull;
        c.offset = Vec3{};
        c.name.clear();
        c.locks = 0;
        c.mass = 0.0f;
    }
    c.size = Vec3{1.0f, 1.0f, 1.0f};
    c.source = mesh_rel;
    if (built.material) scene.add_material(e) = *built.material;
    else scene.remove_material(e);
}

SolidResult place_combined(Scene& scene, const std::vector<Entity>& roots, SolidOperation operation, const SolidOptions& options) {
    SolidResult res;
    if (scene.dimension() == Dimension::two_d) {
        res.error = "union and intersect work in 3D scenes";
        return res;
    }
    if (roots.empty()) {
        res.error = "pick the parts to combine first";
        return res;
    }
    const Entity parent = scene.parent(roots.front());
    const Mat4 pw = scene.alive(parent) ? scene.world_matrix(parent) : Mat4{};
    Built built = build_mesh(scene, roots, operation, Mat4{}, options);
    res.notes = built.notes;
    if (!built.error.empty()) {
        res.error = built.error;
        return res;
    }
    Vec3 lo, hi;
    built.mesh.bounds(lo, hi);
    Transform place;
    place.position = mul_point(inverse_or_identity(pw), (lo + hi) * 0.5f);
    const Mat4 combined_world = pw * trs(place.position, place.rotation, place.scale);
    const std::string name = operation == SolidOperation::intersect_parts ? std::string("Intersection") : std::string("Union");
    std::string mesh_rel, stem;
    if (!write_mesh_file(built, combined_world, name, options, mesh_rel, stem, &res.error)) return res;
    const std::string parts_rel = options.folder + "/" + stem + ".parts";
    if (!write_parts(scene, roots, combined_world, join_path(options.project_root, parts_rel), &res.error)) return res;
    const Entity e = scene.create(name, parent);
    *scene.transform(e) = place;
    if (built.body) scene.add_body(e) = *built.body;
    use_mesh(scene, e, built, mesh_rel);
    Solid& s = scene.add_solid(e);
    s.operation = operation;
    s.one_color = options.one_color;
    s.parts = parts_rel;
    for (Entity r : roots) scene.destroy(r);
    const Body* b = scene.body(e);
    if (b && b->type != BodyType::fixed) res.notes.push_back("it moves, so it collides as its convex hull (its outer shell); make it a container to collide with its exact shape");
    res.entity = e;
    res.entities.push_back(e);
    res.mesh_file = mesh_rel;
    return res;
}

} // namespace

void SolidMesh::add_triangle(Vec3 a, Vec3 b, Vec3 c, int color) {
    positions.push_back(a);
    positions.push_back(b);
    positions.push_back(c);
    colors.push_back(color);
}

int SolidMesh::add_color(Vec3 color) {
    for (std::size_t i = 0; i < palette.size(); ++i)
        if (palette[i].x == color.x && palette[i].y == color.y && palette[i].z == color.z) return static_cast<int>(i);
    palette.push_back(color);
    return static_cast<int>(palette.size() - 1);
}

void SolidMesh::append(const SolidMesh& other) {
    std::vector<int> remap(other.palette.size());
    for (std::size_t i = 0; i < remap.size(); ++i) remap[i] = add_color(other.palette[i]);
    for (std::size_t t = 0; t < other.triangles(); ++t) {
        const int c = other.colors[t];
        add_triangle(other.positions[t * 3], other.positions[t * 3 + 1], other.positions[t * 3 + 2], c >= 0 && static_cast<std::size_t>(c) < remap.size() ? remap[static_cast<std::size_t>(c)] : 0);
    }
}

void SolidMesh::transform(const Mat4& m) {
    for (Vec3& p : positions) p = mul_point(m, p);
    const float det = m.at(0, 0) * (m.at(1, 1) * m.at(2, 2) - m.at(1, 2) * m.at(2, 1)) - m.at(0, 1) * (m.at(1, 0) * m.at(2, 2) - m.at(1, 2) * m.at(2, 0)) + m.at(0, 2) * (m.at(1, 0) * m.at(2, 1) - m.at(1, 1) * m.at(2, 0));
    if (det < 0.0f)
        for (std::size_t t = 0; t < triangles(); ++t) std::swap(positions[t * 3 + 1], positions[t * 3 + 2]);
}

void SolidMesh::set_color(Vec3 color) {
    palette.assign(1, color);
    std::fill(colors.begin(), colors.end(), 0);
}

bool SolidMesh::bounds(Vec3& lo, Vec3& hi) const {
    if (positions.empty()) return false;
    lo = hi = positions.front();
    for (const Vec3& p : positions) {
        lo = Vec3{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = Vec3{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    return true;
}

float SolidMesh::volume() const {
    double v = 0.0;
    for (std::size_t t = 0; t < triangles(); ++t) v += dot3(to_p3(positions[t * 3]), cross3(to_p3(positions[t * 3 + 1]), to_p3(positions[t * 3 + 2])));
    return static_cast<float>(v / 6.0);
}

SolidMesh solid_box(Vec3 size) {
    SolidMesh m;
    const Vec3 h = size * 0.5f;
    Vec3 p[8];
    for (int i = 0; i < 8; ++i) p[i] = Vec3{(i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z};
    const int faces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    for (const auto& f : faces) {
        add_outward(m, p[f[0]], p[f[1]], p[f[2]], Vec3{});
        add_outward(m, p[f[0]], p[f[2]], p[f[3]], Vec3{});
    }
    m.palette.push_back(Material{}.color);
    return m;
}

SolidMesh solid_sphere(float diameter, int rings, int segments) {
    return solid_capsule(diameter, diameter, rings, segments);
}

SolidMesh solid_capsule(float diameter, float height, int rings, int segments) {
    SolidMesh m;
    rings = std::max(4, rings + (rings & 1));
    segments = std::max(6, segments);
    const float r = diameter * 0.5f;
    const float body = std::max(0.0f, height - diameter) * 0.5f;
    std::vector<std::vector<Vec3>> rows;
    auto ring = [&](int i, float shift) {
        const float phi = static_cast<float>(i) / static_cast<float>(rings) * pi;
        std::vector<Vec3> row;
        for (int j = 0; j < segments; ++j) {
            const float theta = static_cast<float>(j) / static_cast<float>(segments) * 2.0f * pi;
            row.push_back(Vec3{r * std::sin(phi) * std::cos(theta), r * std::cos(phi) + shift, r * std::sin(phi) * std::sin(theta)});
        }
        rows.push_back(std::move(row));
    };
    for (int i = 0; i <= rings / 2; ++i) ring(i, body);
    for (int i = rings / 2 + (body > 0.0f ? 0 : 1); i <= rings; ++i) ring(i, -body);
    add_rows(m, rows, Vec3{});
    m.palette.push_back(Material{}.color);
    return m;
}

SolidMesh solid_cylinder(float diameter, float height, float top, int segments) {
    SolidMesh m;
    segments = std::max(3, segments);
    const float r = diameter * 0.5f;
    const float h = height * 0.5f;
    std::vector<Vec3> bottom, upper;
    for (int j = 0; j < segments; ++j) {
        const float theta = static_cast<float>(j) / static_cast<float>(segments) * 2.0f * pi;
        bottom.push_back(Vec3{r * std::cos(theta), -h, r * std::sin(theta)});
        upper.push_back(Vec3{r * top * std::cos(theta), h, r * top * std::sin(theta)});
    }
    const Vec3 center{0.0f, top > 0.0f ? 0.0f : -h * 0.5f, 0.0f};
    for (int j = 0; j < segments; ++j) {
        const int k = (j + 1) % segments;
        add_outward(m, bottom[static_cast<std::size_t>(j)], bottom[static_cast<std::size_t>(k)], upper[static_cast<std::size_t>(j)], center);
        if (top > 0.0f) add_outward(m, bottom[static_cast<std::size_t>(k)], upper[static_cast<std::size_t>(k)], upper[static_cast<std::size_t>(j)], center);
        add_outward(m, Vec3{0.0f, -h, 0.0f}, bottom[static_cast<std::size_t>(j)], bottom[static_cast<std::size_t>(k)], center);
        if (top > 0.0f) add_outward(m, Vec3{0.0f, h, 0.0f}, upper[static_cast<std::size_t>(k)], upper[static_cast<std::size_t>(j)], center);
    }
    m.palette.push_back(Material{}.color);
    return m;
}

SolidMesh solid_union(const SolidMesh& a, const SolidMesh& b) { return run(a, b, Op::join); }
SolidMesh solid_subtract(const SolidMesh& a, const SolidMesh& b) { return run(a, b, Op::cut); }
SolidMesh solid_intersect(const SolidMesh& a, const SolidMesh& b) { return run(a, b, Op::keep); }

std::optional<SolidMesh> collider_solid(const Collider& c, const SolidMeshLoader& loader, std::string* why) {
    SolidMesh m;
    const float wide = std::max(c.size.x, c.size.z);
    switch (c.shape) {
    case ColliderShape::box:
    case ColliderShape::rounded_box:
        m = solid_box(c.size);
        break;
    case ColliderShape::sphere:
        m = solid_sphere(std::max({c.size.x, c.size.y, c.size.z}));
        break;
    case ColliderShape::capsule:
        m = solid_capsule(wide, std::max(c.size.y, wide));
        break;
    case ColliderShape::cylinder:
        m = solid_cylinder(wide, c.size.y, 1.0f);
        break;
    case ColliderShape::cone:
        m = solid_cylinder(wide, c.size.y, 0.0f);
        break;
    case ColliderShape::triangle: {
        const std::vector<Vec3> p = triangle_prism_points(c);
        if (p.size() < 6) {
            if (why) *why = "its triangle can't be built";
            return std::nullopt;
        }
        Vec3 center;
        for (const Vec3& v : p) center += v * (1.0f / 6.0f);
        add_outward(m, p[0], p[2], p[4], center);
        add_outward(m, p[1], p[3], p[5], center);
        for (int k = 0; k < 3; ++k) {
            const int n = (k + 1) % 3;
            add_outward(m, p[static_cast<std::size_t>(2 * k)], p[static_cast<std::size_t>(2 * n)], p[static_cast<std::size_t>(2 * n + 1)], center);
            add_outward(m, p[static_cast<std::size_t>(2 * k)], p[static_cast<std::size_t>(2 * n + 1)], p[static_cast<std::size_t>(2 * k + 1)], center);
        }
        m.palette.push_back(Material{}.color);
        break;
    }
    case ColliderShape::mesh:
    case ColliderShape::convex_hull: {
        std::vector<Vec3> v;
        std::vector<std::uint32_t> idx;
        if (c.source.empty() || !loader || !loader(c.source, v, idx) || idx.size() < 3) {
            if (why) *why = c.source.empty() ? "its mesh collider has no model" : "can't load " + c.source;
            return std::nullopt;
        }
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
            if (idx[i] >= v.size() || idx[i + 1] >= v.size() || idx[i + 2] >= v.size()) continue;
            m.add_triangle(v[idx[i]] * c.size, v[idx[i + 1]] * c.size, v[idx[i + 2]] * c.size, 0);
        }
        m.palette.push_back(Material{}.color);
        break;
    }
    case ColliderShape::sphere_section:
        if (why) *why = "sphere sections can't be combined yet, so it was left out";
        return std::nullopt;
    }
    for (Vec3& p : m.positions) p = p + c.offset;
    return m;
}

std::optional<SolidMesh> entity_solid(const Scene& scene, Entity e, const Mat4& into, const SolidMeshLoader& loader, std::string* why) {
    const std::vector<Collider>* cs = scene.colliders(e);
    if (!cs) return std::nullopt;
    std::optional<SolidMesh> out;
    for (const Collider& c : *cs) {
        if (!c.enabled || c.sensor) continue;
        std::string w;
        std::optional<SolidMesh> m = collider_solid(c, loader, &w);
        if (!w.empty() && why) *why = w;
        if (!m || m->empty()) continue;
        out = out ? solid_union(*out, *m) : std::move(*m);
    }
    if (!out) return std::nullopt;
    out->transform(into * scene.world_matrix(e));
    out->set_color(part_color(scene, e));
    return out;
}

bool write_solid_obj(const SolidMesh& mesh, const std::string& obj_path, bool z_up, bool with_colors, std::string* error) {
    const std::filesystem::path path(obj_path);
    auto out_of = [z_up](Vec3 p) { return z_up ? Vec3{p.x, p.z, -p.y} : p; };
    std::map<std::tuple<long long, long long, long long>, std::size_t> weld;
    std::vector<Vec3> verts;
    std::vector<std::size_t> corner(mesh.positions.size());
    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
        const Vec3 p = mesh.positions[i];
        const auto key = std::make_tuple(std::llround(p.x * 1e5), std::llround(p.y * 1e5), std::llround(p.z * 1e5));
        auto it = weld.find(key);
        if (it == weld.end()) {
            it = weld.emplace(key, verts.size()).first;
            verts.push_back(p);
        }
        corner[i] = it->second;
    }
    std::vector<Vec3> face_n(mesh.triangles());
    std::vector<std::vector<std::size_t>> touching(verts.size());
    for (std::size_t t = 0; t < mesh.triangles(); ++t) {
        const Vec3 n = cross(mesh.positions[t * 3 + 1] - mesh.positions[t * 3], mesh.positions[t * 3 + 2] - mesh.positions[t * 3]);
        face_n[t] = n;
        for (int k = 0; k < 3; ++k) touching[corner[t * 3 + static_cast<std::size_t>(k)]].push_back(t);
    }
    const float crease = std::cos(radians(35.0f));
    std::vector<Vec3> normals;
    std::map<std::tuple<long long, long long, long long>, std::size_t> normal_index;
    std::vector<std::size_t> corner_normal(mesh.positions.size());
    for (std::size_t t = 0; t < mesh.triangles(); ++t) {
        const Vec3 own = normalize(face_n[t]);
        for (int k = 0; k < 3; ++k) {
            const std::size_t ci = t * 3 + static_cast<std::size_t>(k);
            Vec3 sum;
            for (std::size_t other : touching[corner[ci]]) {
                const Vec3 on = normalize(face_n[other]);
                if (dot(on, own) >= crease) sum += face_n[other];
            }
            Vec3 n = dot(sum, sum) > 1e-20f ? normalize(sum) : own;
            n = out_of(n);
            const auto key = std::make_tuple(std::llround(n.x * 1e4), std::llround(n.y * 1e4), std::llround(n.z * 1e4));
            auto it = normal_index.find(key);
            if (it == normal_index.end()) {
                it = normal_index.emplace(key, normals.size()).first;
                normals.push_back(n);
            }
            corner_normal[ci] = it->second;
        }
    }
    std::ofstream obj(path, std::ios::binary | std::ios::trunc);
    if (!obj) {
        if (error) *error = "can't write " + obj_path;
        return false;
    }
    obj.precision(7);
    const std::string stem = path.stem().string();
    obj << "# Orchard solid\n";
    if (with_colors) {
        std::ofstream mtl(path.parent_path() / (stem + ".mtl"), std::ios::binary | std::ios::trunc);
        mtl.precision(5);
        for (std::size_t i = 0; i < mesh.palette.size(); ++i) {
            const Vec3 c = mesh.palette[i];
            mtl << "newmtl part" << i << "\nKd " << c.x << " " << c.y << " " << c.z << "\n";
        }
        obj << "mtllib " << stem << ".mtl\n";
    }
    obj << "o " << stem << "\n";
    for (const Vec3& v : verts) {
        const Vec3 p = out_of(v);
        obj << "v " << p.x << " " << p.y << " " << p.z << "\n";
    }
    for (const Vec3& n : normals) obj << "vn " << n.x << " " << n.y << " " << n.z << "\n";
    std::vector<std::size_t> order(mesh.triangles());
    for (std::size_t t = 0; t < order.size(); ++t) order[t] = t;
    if (with_colors) std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return mesh.colors[a] < mesh.colors[b]; });
    int current = -1;
    for (std::size_t t : order) {
        if (with_colors && mesh.colors[t] != current) {
            current = mesh.colors[t];
            obj << "usemtl part" << current << "\n";
        }
        obj << "f";
        for (int k = 0; k < 3; ++k) {
            const std::size_t ci = t * 3 + static_cast<std::size_t>(k);
            obj << " " << corner[ci] + 1 << "//" << corner_normal[ci] + 1;
        }
        obj << "\n";
    }
    if (!obj) {
        if (error) *error = "can't write " + obj_path;
        return false;
    }
    return true;
}

Transform transform_from_matrix(const Mat4& m) {
    Transform t;
    t.position = Vec3{m.at(0, 3), m.at(1, 3), m.at(2, 3)};
    Vec3 x = column(m, 0), y = column(m, 1), z = column(m, 2);
    float sx = length(x), sy = length(y), sz = length(z);
    if (dot(cross(x, y), z) < 0.0f) sx = -sx;
    t.scale = Vec3{sx, sy, sz};
    if (std::fabs(sx) < 1e-12f || sy < 1e-12f || sz < 1e-12f) return t;
    t.rotation = quat_from_basis(x * (1.0f / sx), y * (1.0f / sy), z * (1.0f / sz));
    return t;
}

bool is_negative_part(const Scene& scene, Entity e) {
    for (Entity at = e; scene.alive(at); at = scene.parent(at))
        if (const Solid* s = scene.solid(at))
            if (s->negative) return true;
    return false;
}

SolidResult combine_solids(Scene& scene, const std::vector<Entity>& parts, SolidOperation operation, const SolidOptions& options) {
    if (operation == SolidOperation::none) operation = SolidOperation::union_parts;
    return place_combined(scene, part_roots(scene, parts), operation, options);
}

SolidResult rebuild_solid(Scene& scene, Entity combined, const SolidOptions& options) {
    SolidResult res;
    const Solid* s = scene.solid(combined);
    if (!s || s->parts.empty()) {
        res.error = "it isn't a union or intersection";
        return res;
    }
    std::string why;
    std::optional<Scene> parts = Scene::load_file(join_path(options.project_root, s->parts), &why);
    if (!parts) {
        res.error = "can't read its parts (" + s->parts + "): " + why;
        return res;
    }
    const SolidOperation operation = s->operation;
    const Mat4 cw = scene.world_matrix(combined);
    Scene work(scene.dimension());
    const Entity holder = work.create("holder");
    *work.transform(holder) = transform_from_matrix(cw);
    std::vector<Entity> roots;
    for (Entity r : parts->roots()) roots.push_back(work.copy_from(*parts, r, holder));
    Built built = build_mesh(work, roots, operation, Mat4{}, options);
    res.notes = built.notes;
    if (!built.error.empty()) {
        res.error = built.error;
        return res;
    }
    std::string mesh_rel, stem;
    if (!write_mesh_file(built, cw, scene.name(combined), options, mesh_rel, stem, &res.error)) return res;
    use_mesh(scene, combined, built, mesh_rel);
    scene.add_solid(combined).one_color = options.one_color;
    res.entity = combined;
    res.entities.push_back(combined);
    res.mesh_file = mesh_rel;
    return res;
}

SolidResult separate_solid(Scene& scene, Entity combined, const SolidOptions& options) {
    SolidResult res;
    const Solid* s = scene.solid(combined);
    if (!s || s->parts.empty()) {
        res.error = "only a union or intersection can be separated";
        return res;
    }
    std::string why;
    std::optional<Scene> parts = Scene::load_file(join_path(options.project_root, s->parts), &why);
    if (!parts) {
        res.error = "can't read its parts (" + s->parts + "): " + why;
        return res;
    }
    const Entity parent = scene.parent(combined);
    const Mat4 into = inverse_or_identity(parent_world(scene, combined)) * scene.world_matrix(combined);
    for (Entity r : parts->roots()) {
        const Mat4 rel = trs(parts->transform(r)->position, parts->transform(r)->rotation, parts->transform(r)->scale);
        const Entity e = scene.copy_from(*parts, r, parent);
        if (Transform* t = scene.transform(e)) *t = transform_from_matrix(into * rel);
        res.entities.push_back(e);
    }
    if (res.entities.empty()) {
        res.error = "its parts file is empty";
        return res;
    }
    scene.destroy(combined);
    res.entity = res.entities.front();
    return res;
}

} // namespace orchard
