#include "builtin_models.hpp"

#include <orchard/items.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace orchard {
namespace editor {

namespace {

struct V {
    float x, y, z;
};

struct MeshData {
    std::vector<V> positions;
    std::vector<V> normals;
    std::vector<std::array<float, 4>> colors;
    std::vector<std::uint32_t> indices;
};

constexpr float two_pi = 6.28318530718f;

V sub(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V unit(V a) {
    const float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return l > 0.0f ? V{a.x / l, a.y / l, a.z / l} : V{0.0f, 1.0f, 0.0f};
}

float hash(std::uint32_t n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<float>(n & 0x7fffffffu) / 2147483647.0f;
}

void triangle(MeshData& m, V a, V b, V c, std::array<float, 4> col) {
    const V n = unit(cross(sub(b, a), sub(c, a)));
    const auto base = static_cast<std::uint32_t>(m.positions.size());
    for (V p : {a, b, c}) {
        m.positions.push_back(p);
        m.normals.push_back(n);
        m.colors.push_back(col);
    }
    m.indices.push_back(base);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);
}

void cone(MeshData& m, float y0, float y1, float r0, float r1, int sides, std::array<float, 4> col, std::uint32_t seed, float jitter) {
    std::vector<V> lo, hi;
    for (int i = 0; i < sides; ++i) {
        const float a = two_pi * static_cast<float>(i) / static_cast<float>(sides);
        const float j0 = 1.0f + jitter * (hash(seed + static_cast<std::uint32_t>(i)) - 0.5f);
        const float j1 = 1.0f + jitter * (hash(seed + 97u + static_cast<std::uint32_t>(i)) - 0.5f);
        lo.push_back({std::cos(a) * r0 * j0, y0, std::sin(a) * r0 * j0});
        hi.push_back({std::cos(a) * r1 * j1, y1, std::sin(a) * r1 * j1});
    }
    for (int i = 0; i < sides; ++i) {
        const int k = (i + 1) % sides;
        const float shade = 0.9f + 0.2f * hash(seed + 301u + static_cast<std::uint32_t>(i));
        const std::array<float, 4> c{std::min(1.0f, col[0] * shade), std::min(1.0f, col[1] * shade), std::min(1.0f, col[2] * shade), 1.0f};
        if (r1 <= 0.0f) {
            triangle(m, lo[static_cast<std::size_t>(i)], V{0.0f, y1, 0.0f}, lo[static_cast<std::size_t>(k)], c);
        } else {
            triangle(m, lo[static_cast<std::size_t>(i)], hi[static_cast<std::size_t>(i)], hi[static_cast<std::size_t>(k)], c);
            triangle(m, lo[static_cast<std::size_t>(i)], hi[static_cast<std::size_t>(k)], lo[static_cast<std::size_t>(k)], c);
        }
        triangle(m, lo[static_cast<std::size_t>(k)], V{0.0f, y0, 0.0f}, lo[static_cast<std::size_t>(i)], c);
    }
}

void blob(MeshData& m, V center, V radius, std::array<float, 4> col, std::uint32_t seed, float jitter, int rings, int sides) {
    std::vector<V> pts;
    for (int r = 0; r <= rings; ++r) {
        const float t = static_cast<float>(r) / static_cast<float>(rings) * 3.14159265f;
        for (int s = 0; s < sides; ++s) {
            const float a = two_pi * static_cast<float>(s) / static_cast<float>(sides);
            const bool pole = r == 0 || r == rings;
            const float j = pole ? 1.0f : 1.0f + jitter * (hash(seed + static_cast<std::uint32_t>(r * 131 + s)) - 0.5f);
            pts.push_back({center.x + std::sin(t) * std::cos(a) * radius.x * j, center.y + std::cos(t) * radius.y * j, center.z + std::sin(t) * std::sin(a) * radius.z * j});
        }
    }
    auto at = [&](int r, int s) { return pts[static_cast<std::size_t>(r * sides + ((s % sides) + sides) % sides)]; };
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sides; ++s) {
            const float shade = 0.85f + 0.3f * hash(seed + 777u + static_cast<std::uint32_t>(r * 37 + s));
            const std::array<float, 4> c{std::min(1.0f, col[0] * shade), std::min(1.0f, col[1] * shade), std::min(1.0f, col[2] * shade), 1.0f};
            if (r > 0) triangle(m, at(r, s), at(r, s + 1), at(r + 1, s), c);
            if (r + 1 < rings) triangle(m, at(r, s + 1), at(r + 1, s + 1), at(r + 1, s), c);
        }
    }
}

using C4 = std::array<float, 4>;

void box(MeshData& m, V c, V h, C4 col) {
    const V p[8] = {{c.x - h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y + h.y, c.z - h.z}, {c.x - h.x, c.y + h.y, c.z - h.z},
                    {c.x - h.x, c.y - h.y, c.z + h.z}, {c.x + h.x, c.y - h.y, c.z + h.z}, {c.x + h.x, c.y + h.y, c.z + h.z}, {c.x - h.x, c.y + h.y, c.z + h.z}};
    const int f[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6}, {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}};
    for (const auto& q : f) {
        triangle(m, p[q[0]], p[q[1]], p[q[2]], col);
        triangle(m, p[q[0]], p[q[2]], p[q[3]], col);
    }
}

void ellipsoid(MeshData& m, V c, V r, C4 col, int rings = 8, int sides = 14) {
    blob(m, c, r, col, 0u, 0.0f, rings, sides);
}

void rod(MeshData& m, V a, V b, float radius, C4 col, int sides = 10) {
    const V axis = unit(sub(b, a));
    V side = std::fabs(axis.y) < 0.9f ? unit(cross(axis, V{0.0f, 1.0f, 0.0f})) : unit(cross(axis, V{1.0f, 0.0f, 0.0f}));
    const V other = cross(axis, side);
    std::vector<V> lo, hi;
    for (int i = 0; i < sides; ++i) {
        const float t = two_pi * static_cast<float>(i) / static_cast<float>(sides);
        const V o{(side.x * std::cos(t) + other.x * std::sin(t)) * radius, (side.y * std::cos(t) + other.y * std::sin(t)) * radius, (side.z * std::cos(t) + other.z * std::sin(t)) * radius};
        lo.push_back({a.x + o.x, a.y + o.y, a.z + o.z});
        hi.push_back({b.x + o.x, b.y + o.y, b.z + o.z});
    }
    for (int i = 0; i < sides; ++i) {
        const auto k = static_cast<std::size_t>((i + 1) % sides), j = static_cast<std::size_t>(i);
        triangle(m, lo[j], lo[k], hi[k], col);
        triangle(m, lo[j], hi[k], hi[j], col);
        triangle(m, a, lo[k], lo[j], col);
        triangle(m, b, hi[j], hi[k], col);
    }
}

void pixel_person(MeshData& m) {
    const C4 skin{0.93f, 0.74f, 0.58f, 1.0f}, hair{0.33f, 0.2f, 0.11f, 1.0f}, shirt{0.2f, 0.52f, 0.72f, 1.0f}, sleeve{0.16f, 0.44f, 0.62f, 1.0f};
    const C4 pants{0.2f, 0.24f, 0.42f, 1.0f}, shoes{0.18f, 0.12f, 0.08f, 1.0f}, eye{0.08f, 0.08f, 0.1f, 1.0f}, white{0.97f, 0.97f, 0.97f, 1.0f}, mouth{0.62f, 0.32f, 0.3f, 1.0f}, belt{0.25f, 0.17f, 0.1f, 1.0f};
    for (float x : {-0.1f, 0.1f}) {
        box(m, {x, 0.47f, 0.0f}, {0.085f, 0.39f, 0.09f}, pants);
        box(m, {x, 0.045f, 0.03f}, {0.09f, 0.045f, 0.13f}, shoes);
    }
    box(m, {0.0f, 1.15f, 0.0f}, {0.2f, 0.27f, 0.11f}, shirt);
    box(m, {0.0f, 0.9f, 0.0f}, {0.205f, 0.03f, 0.115f}, belt);
    for (float x : {-0.27f, 0.27f}) {
        box(m, {x, 1.12f, 0.0f}, {0.065f, 0.3f, 0.07f}, sleeve);
        box(m, {x, 0.77f, 0.0f}, {0.06f, 0.06f, 0.065f}, skin);
    }
    box(m, {0.0f, 1.45f, 0.0f}, {0.07f, 0.04f, 0.07f}, skin);
    box(m, {0.0f, 1.63f, 0.0f}, {0.15f, 0.15f, 0.15f}, skin);
    box(m, {0.0f, 1.79f, -0.01f}, {0.16f, 0.025f, 0.16f}, hair);
    box(m, {0.0f, 1.66f, -0.15f}, {0.155f, 0.13f, 0.02f}, hair);
    for (float x : {-0.155f, 0.155f}) box(m, {x, 1.7f, -0.03f}, {0.01f, 0.08f, 0.12f}, hair);
    for (float x : {-0.06f, 0.06f}) {
        box(m, {x, 1.66f, 0.152f}, {0.035f, 0.025f, 0.004f}, white);
        box(m, {x + (x < 0.0f ? 0.012f : -0.012f), 1.66f, 0.156f}, {0.018f, 0.025f, 0.004f}, eye);
    }
    box(m, {0.0f, 1.56f, 0.152f}, {0.05f, 0.012f, 0.004f}, mouth);
}

void mannequin(MeshData& m) {
    const C4 body{0.78f, 0.78f, 0.8f, 1.0f}, joint{0.42f, 0.44f, 0.48f, 1.0f}, face{0.7f, 0.7f, 0.73f, 1.0f};
    ellipsoid(m, {0.0f, 1.65f, 0.0f}, {0.1f, 0.125f, 0.115f}, body, 10, 16);
    ellipsoid(m, {0.0f, 1.62f, 0.07f}, {0.06f, 0.05f, 0.05f}, face, 6, 10);
    rod(m, {0.0f, 1.44f, 0.0f}, {0.0f, 1.54f, 0.0f}, 0.045f, joint);
    ellipsoid(m, {0.0f, 1.28f, 0.0f}, {0.19f, 0.17f, 0.12f}, body, 10, 16);
    ellipsoid(m, {0.0f, 1.07f, 0.0f}, {0.14f, 0.1f, 0.09f}, joint, 8, 14);
    ellipsoid(m, {0.0f, 0.93f, 0.0f}, {0.17f, 0.1f, 0.11f}, body, 8, 14);
    for (float x : {-1.0f, 1.0f}) {
        ellipsoid(m, {0.22f * x, 1.4f, 0.0f}, {0.06f, 0.06f, 0.06f}, joint, 6, 10);
        ellipsoid(m, {0.25f * x, 1.22f, 0.0f}, {0.055f, 0.15f, 0.055f}, body, 8, 12);
        ellipsoid(m, {0.25f * x, 1.06f, 0.0f}, {0.042f, 0.042f, 0.042f}, joint, 6, 10);
        ellipsoid(m, {0.25f * x, 0.92f, 0.0f}, {0.045f, 0.13f, 0.045f}, body, 8, 12);
        ellipsoid(m, {0.25f * x, 0.75f, 0.01f}, {0.035f, 0.06f, 0.05f}, joint, 6, 10);
        ellipsoid(m, {0.1f * x, 0.67f, 0.0f}, {0.075f, 0.21f, 0.075f}, body, 8, 12);
        ellipsoid(m, {0.1f * x, 0.47f, 0.01f}, {0.055f, 0.05f, 0.055f}, joint, 6, 10);
        ellipsoid(m, {0.1f * x, 0.27f, 0.0f}, {0.058f, 0.2f, 0.058f}, body, 8, 12);
        ellipsoid(m, {0.1f * x, 0.045f, 0.05f}, {0.055f, 0.045f, 0.12f}, joint, 6, 12);
    }
}

void robot(MeshData& m) {
    const C4 shell{0.58f, 0.62f, 0.68f, 1.0f}, dark{0.18f, 0.19f, 0.22f, 1.0f}, visor{0.04f, 0.05f, 0.07f, 1.0f}, glow{0.35f, 0.95f, 1.0f, 1.0f}, accent{0.95f, 0.55f, 0.15f, 1.0f};
    ellipsoid(m, {0.0f, 1.6f, 0.0f}, {0.19f, 0.17f, 0.17f}, shell, 10, 18);
    ellipsoid(m, {0.0f, 1.6f, 0.06f}, {0.155f, 0.1f, 0.12f}, visor, 8, 16);
    for (float x : {-0.06f, 0.06f}) ellipsoid(m, {x, 1.62f, 0.172f}, {0.03f, 0.04f, 0.012f}, glow, 6, 10);
    rod(m, {0.0f, 1.76f, 0.0f}, {0.0f, 1.86f, 0.0f}, 0.012f, dark, 6);
    ellipsoid(m, {0.0f, 1.87f, 0.0f}, {0.025f, 0.025f, 0.025f}, accent, 5, 8);
    for (float x : {-0.19f, 0.19f}) ellipsoid(m, {x, 1.6f, 0.0f}, {0.025f, 0.06f, 0.06f}, dark, 6, 10);
    rod(m, {0.0f, 1.38f, 0.0f}, {0.0f, 1.45f, 0.0f}, 0.04f, dark);
    ellipsoid(m, {0.0f, 1.2f, 0.0f}, {0.17f, 0.2f, 0.12f}, shell, 10, 16);
    ellipsoid(m, {0.0f, 1.24f, 0.1f}, {0.06f, 0.06f, 0.03f}, glow, 6, 10);
    ellipsoid(m, {0.0f, 0.95f, 0.0f}, {0.13f, 0.07f, 0.09f}, dark, 6, 14);
    for (float x : {-1.0f, 1.0f}) {
        ellipsoid(m, {0.21f * x, 1.34f, 0.0f}, {0.055f, 0.055f, 0.055f}, dark, 6, 10);
        rod(m, {0.23f * x, 1.32f, 0.0f}, {0.25f * x, 1.06f, 0.0f}, 0.035f, shell);
        ellipsoid(m, {0.25f * x, 1.05f, 0.0f}, {0.04f, 0.04f, 0.04f}, dark, 6, 10);
        rod(m, {0.25f * x, 1.04f, 0.0f}, {0.25f * x, 0.8f, 0.0f}, 0.032f, shell);
        box(m, {0.25f * x, 0.75f, 0.0f}, {0.035f, 0.05f, 0.045f}, dark);
        ellipsoid(m, {0.1f * x, 0.89f, 0.0f}, {0.055f, 0.055f, 0.055f}, dark, 6, 10);
        rod(m, {0.1f * x, 0.88f, 0.0f}, {0.1f * x, 0.5f, 0.0f}, 0.05f, shell);
        ellipsoid(m, {0.1f * x, 0.48f, 0.0f}, {0.05f, 0.05f, 0.05f}, dark, 6, 10);
        rod(m, {0.1f * x, 0.47f, 0.0f}, {0.1f * x, 0.1f, 0.0f}, 0.045f, shell);
        box(m, {0.1f * x, 0.05f, 0.04f}, {0.065f, 0.05f, 0.12f}, dark);
    }
}

MeshData build(const std::string& name) {
    MeshData m;
    const std::array<float, 4> bark{0.36f, 0.24f, 0.14f, 1.0f};
    if (name == "character-pixel") {
        pixel_person(m);
    } else if (name == "character-mannequin") {
        mannequin(m);
    } else if (name == "character-robot") {
        robot(m);
    } else if (name == "pine") {
        cone(m, 0.0f, 1.2f, 0.18f, 0.14f, 7, bark, 11u, 0.1f);
        cone(m, 0.9f, 3.0f, 1.25f, 0.0f, 9, {0.12f, 0.32f, 0.16f, 1.0f}, 21u, 0.25f);
        cone(m, 2.0f, 4.1f, 1.0f, 0.0f, 9, {0.14f, 0.36f, 0.18f, 1.0f}, 31u, 0.25f);
        cone(m, 3.1f, 5.2f, 0.72f, 0.0f, 8, {0.16f, 0.4f, 0.2f, 1.0f}, 41u, 0.25f);
    } else if (name == "tree") {
        cone(m, 0.0f, 2.4f, 0.22f, 0.14f, 7, bark, 12u, 0.1f);
        blob(m, {0.0f, 3.2f, 0.0f}, {1.5f, 1.3f, 1.5f}, {0.24f, 0.48f, 0.18f, 1.0f}, 51u, 0.35f, 6, 10);
        blob(m, {0.6f, 2.7f, 0.3f}, {0.9f, 0.8f, 0.9f}, {0.28f, 0.52f, 0.2f, 1.0f}, 61u, 0.35f, 5, 8);
        blob(m, {-0.5f, 2.8f, -0.4f}, {0.9f, 0.8f, 0.9f}, {0.22f, 0.44f, 0.16f, 1.0f}, 71u, 0.35f, 5, 8);
    } else if (name == "bush") {
        blob(m, {0.0f, 0.45f, 0.0f}, {0.8f, 0.55f, 0.8f}, {0.26f, 0.46f, 0.2f, 1.0f}, 81u, 0.4f, 5, 9);
        blob(m, {0.45f, 0.35f, 0.2f}, {0.5f, 0.4f, 0.5f}, {0.3f, 0.5f, 0.22f, 1.0f}, 91u, 0.4f, 4, 7);
    } else {
        blob(m, {0.0f, 0.35f, 0.0f}, {0.8f, 0.55f, 0.65f}, {0.5f, 0.5f, 0.48f, 1.0f}, 101u, 0.45f, 5, 8);
    }
    return m;
}


C4 rgb(orchard::Vec3 c, float shade = 1.0f, float a = 1.0f) {
    return {std::clamp(c.x * shade, 0.0f, 1.0f), std::clamp(c.y * shade, 0.0f, 1.0f), std::clamp(c.z * shade, 0.0f, 1.0f), a};
}

// Every held item points along -Z (the way the holder faces) with the grip at the origin.
void item_gun(MeshData& m, const orchard::ItemDef& d, int q) {
    const float L = std::clamp(d.length, 0.12f, 2.0f);
    const C4 metal = rgb(d.color), dark = rgb(d.color, 0.6f), wood = rgb(d.accent);
    const bool rifle = L > 0.45f;
    const float body = rifle ? L * 0.45f : L * 0.7f;
    box(m, {0.0f, 0.05f, -body * 0.5f + 0.05f}, {0.025f, 0.035f, body * 0.5f}, metal);
    const float barrel_start = -body + 0.05f;
    rod(m, {0.0f, 0.06f, barrel_start}, {0.0f, 0.06f, -L + 0.08f}, rifle ? 0.012f : 0.01f, dark, q >= 2 ? 12 : 6);
    box(m, {0.0f, -0.04f, 0.0f}, {0.018f, 0.06f, 0.025f}, wood);
    if (d.magazine > 1 && d.magazine < 200) box(m, {0.0f, -0.03f, -0.07f}, {0.014f, 0.05f, 0.02f}, dark);
    if (rifle) box(m, {0.0f, 0.03f, 0.08f + L * 0.12f}, {0.02f, 0.045f, L * 0.13f}, wood);
    if (q >= 1) {
        box(m, {0.0f, 0.095f, -L + 0.11f}, {0.003f, 0.01f, 0.004f}, dark);
        box(m, {0.0f, 0.09f, 0.02f}, {0.008f, 0.008f, 0.006f}, dark);
        box(m, {0.0f, -0.005f, -0.03f}, {0.004f, 0.012f, 0.02f}, dark);
    }
    if (q >= 2) {
        if (rifle) rod(m, {0.0f, 0.04f, barrel_start + 0.05f}, {0.0f, 0.04f, barrel_start - L * 0.25f}, 0.02f, wood, 10);
        if (d.has("scoped") || d.category.find("Sniper") != std::string::npos) rod(m, {0.0f, 0.12f, -0.05f}, {0.0f, 0.12f, -0.3f}, 0.022f, dark, 14);
        rod(m, {0.0f, 0.06f, -L + 0.08f}, {0.0f, 0.06f, -L + 0.06f}, rifle ? 0.016f : 0.013f, dark, 12);
    }
}

void item_melee(MeshData& m, const orchard::ItemDef& d, int q) {
    const float L = std::clamp(d.length, 0.1f, 3.0f);
    const C4 blade = rgb(d.color), grip = rgb(d.accent), guard = rgb(d.accent, 0.7f);
    const bool blunt = d.damage_type == "blunt" || d.has("blunt");
    const bool polearm = L > 1.4f;
    const float handle = polearm ? L * 0.8f : std::min(0.25f, L * 0.3f);
    rod(m, {0.0f, 0.0f, handle * 0.25f}, {0.0f, 0.0f, -handle * 0.75f}, polearm ? 0.016f : 0.017f, grip, q >= 2 ? 10 : 6);
    const float head0 = -handle * 0.75f, head1 = -L + handle * 0.25f;
    if (blunt) {
        const float hl = std::max(0.08f, (head0 - head1));
        if (d.shape.find("hammer") != std::string::npos || d.shape.find("mace") != std::string::npos || d.shape.find("maul") != std::string::npos)
            box(m, {0.0f, 0.0f, head1 + 0.05f}, {0.07f, 0.045f, 0.05f}, blade);
        else
            rod(m, {0.0f, 0.0f, head0}, {0.0f, 0.0f, head1}, std::min(0.035f, 0.012f + hl * 0.03f), blade, q >= 2 ? 10 : 6);
        if (q >= 1 && d.shape.find("mace") != std::string::npos)
            for (int i = 0; i < 6; ++i) {
                const float a = two_pi * static_cast<float>(i) / 6.0f;
                box(m, {std::cos(a) * 0.055f, std::sin(a) * 0.055f, head1 + 0.05f}, {0.012f, 0.012f, 0.012f}, blade);
            }
        return;
    }
    const bool axe = d.shape.find("axe") != std::string::npos || d.shape.find("hatchet") != std::string::npos;
    if (axe) {
        rod(m, {0.0f, 0.0f, head0}, {0.0f, 0.0f, head1}, 0.017f, grip, 6);
        triangle(m, {0.004f, 0.0f, head1 + 0.02f}, {0.004f, 0.11f, head1 - 0.03f}, {0.004f, 0.11f, head1 + 0.09f}, blade);
        triangle(m, {-0.004f, 0.0f, head1 + 0.02f}, {-0.004f, 0.11f, head1 + 0.09f}, {-0.004f, 0.11f, head1 - 0.03f}, blade);
        return;
    }
    const float w = d.shape.find("rapier") != std::string::npos || d.shape.find("estoc") != std::string::npos ? 0.008f : d.shape.find("knife") != std::string::npos || d.shape.find("dagger") != std::string::npos ? 0.014f : 0.022f;
    if (polearm) rod(m, {0.0f, 0.0f, head0}, {0.0f, 0.0f, head1 + 0.15f}, 0.015f, grip, 6);
    const float b0 = polearm ? head1 + 0.15f : head0, b1 = head1;
    const float curve = d.shape.find("katana") != std::string::npos || d.shape.find("saber") != std::string::npos || d.shape.find("scimitar") != std::string::npos ? 0.04f : 0.0f;
    const int segs = q >= 2 ? 8 : q == 1 ? 4 : 1;
    for (int i = 0; i < segs; ++i) {
        const float t0 = static_cast<float>(i) / segs, t1 = static_cast<float>(i + 1) / segs;
        const float z0 = b0 + (b1 - b0) * t0, z1 = b0 + (b1 - b0) * t1;
        const float y0 = curve * t0 * t0, y1 = curve * t1 * t1;
        const float w0 = w, w1 = i + 1 == segs ? 0.0f : w;
        box(m, {0.0f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f}, {0.003f, (w0 + w1) * 0.5f, std::fabs(z1 - z0) * 0.5f}, blade);
    }
    if (!polearm) box(m, {0.0f, 0.0f, head0}, {0.012f, std::max(0.03f, w * 2.2f), 0.008f}, guard);
    if (q >= 1 && !polearm) ellipsoid(m, {0.0f, 0.0f, handle * 0.27f}, {0.022f, 0.022f, 0.022f}, guard, 5, 8);
}

void item_bow(MeshData& m, const orchard::ItemDef& d, int q) {
    const C4 wood = rgb(d.color), string{0.9f, 0.9f, 0.85f, 1.0f};
    if (d.shape.find("crossbow") != std::string::npos) {
        box(m, {0.0f, 0.0f, -0.25f}, {0.02f, 0.025f, 0.3f}, rgb(d.accent));
        box(m, {0.0f, 0.02f, -0.5f}, {0.3f, 0.012f, 0.012f}, wood);
        rod(m, {-0.3f, 0.02f, -0.5f}, {0.0f, 0.02f, -0.3f}, 0.002f, string, 4);
        rod(m, {0.3f, 0.02f, -0.5f}, {0.0f, 0.02f, -0.3f}, 0.002f, string, 4);
        return;
    }
    if (d.shape.find("sling") != std::string::npos || d.shape == "blowgun") {
        rod(m, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -std::max(0.2f, d.length)}, 0.008f, wood, 6);
        return;
    }
    const float half = std::clamp(d.length, 0.6f, 2.0f) * 0.5f;
    const int segs = q >= 2 ? 12 : q == 1 ? 6 : 3;
    V prev{0.0f, -half, 0.0f};
    for (int i = 1; i <= segs; ++i) {
        const float t = -1.0f + 2.0f * static_cast<float>(i) / segs;
        const V cur{0.0f, t * half, -0.12f * (1.0f - t * t) - (d.shape.find("compound") != std::string::npos ? 0.02f : 0.0f)};
        rod(m, prev, cur, 0.012f, wood, 6);
        prev = cur;
    }
    rod(m, {0.0f, -half, 0.0f}, {0.0f, half, 0.0f}, 0.0015f, string, 4);
}

void item_magic(MeshData& m, const orchard::ItemDef& d, int q) {
    const C4 wood = rgb(d.accent), glow = rgb(d.color, 1.2f);
    if (d.shape == "staff") {
        rod(m, {0.0f, 0.0f, 0.4f}, {0.0f, 0.0f, -1.3f}, 0.018f, wood, q >= 2 ? 10 : 6);
        ellipsoid(m, {0.0f, 0.0f, -1.36f}, {0.06f, 0.06f, 0.06f}, glow, q >= 1 ? 8 : 4, q >= 1 ? 12 : 6);
    } else if (d.shape == "wand") {
        rod(m, {0.0f, 0.0f, 0.05f}, {0.0f, 0.0f, -0.3f}, 0.008f, wood, 6);
        if (q >= 1) ellipsoid(m, {0.0f, 0.0f, -0.31f}, {0.012f, 0.012f, 0.012f}, glow, 4, 6);
    } else if (d.shape == "tome") {
        box(m, {0.0f, 0.0f, 0.0f}, {0.12f, 0.03f, 0.16f}, rgb(d.color, 0.7f));
        if (q >= 1) box(m, {0.0f, 0.0f, 0.0f}, {0.115f, 0.032f, 0.155f}, {0.92f, 0.88f, 0.75f, 1.0f});
    } else {
        ellipsoid(m, {0.0f, 0.0f, 0.0f}, {0.07f, 0.07f, 0.07f}, glow, q >= 1 ? 10 : 5, q >= 1 ? 14 : 7);
    }
}

void item_flask(MeshData& m, const orchard::ItemDef& d, int q, bool liquid_only) {
    const bool bottle = d.kind == orchard::ItemKind::drink;
    const float r = bottle ? 0.035f : 0.045f, h = bottle ? 0.2f : 0.09f;
    const C4 liquid = rgb(d.color, 1.0f, q >= 2 ? 0.85f : 1.0f);
    if (liquid_only) {
        // The sloshing part: a flattened blob that the game tilts with motion.
        ellipsoid(m, {0.0f, bottle ? h * 0.35f : r * 0.8f, 0.0f}, {r * 0.88f, bottle ? h * 0.33f : r * 0.62f, r * 0.88f}, liquid, 6, 12);
        return;
    }
    const C4 glass = q >= 2 ? C4{0.85f, 0.92f, 0.95f, 0.3f} : liquid;
    if (bottle) cone(m, 0.0f, h, r, r, q >= 1 ? 12 : 6, glass, 3u, 0.0f);
    else ellipsoid(m, {0.0f, r, 0.0f}, {r, r, r}, glass, q >= 1 ? 8 : 4, q >= 1 ? 12 : 6);
    const float top = bottle ? h : r * 1.9f;
    cone(m, top, top + 0.04f, r * 0.35f, r * 0.3f, q >= 1 ? 10 : 5, glass, 5u, 0.0f);
    cone(m, top + 0.04f, top + 0.06f, r * 0.33f, r * 0.33f, 6, rgb(d.accent), 7u, 0.0f);
}

void item_wear(MeshData& m, const orchard::ItemDef& d, int q, bool worn) {
    const C4 c = rgb(d.color), a = rgb(d.accent);
    const std::string& slot = d.wear;
    if (!worn) {
        // Folded or packed version for the ground.
        if (d.kind == orchard::ItemKind::container) box(m, {0.0f, 0.15f, 0.0f}, {0.15f, 0.15f, 0.08f}, c);
        else if (slot == "head") ellipsoid(m, {0.0f, 0.06f, 0.0f}, {0.11f, 0.07f, 0.12f}, c, 6, 10);
        else if (slot == "feet") { box(m, {-0.06f, 0.04f, 0.0f}, {0.045f, 0.04f, 0.12f}, c); box(m, {0.06f, 0.04f, 0.0f}, {0.045f, 0.04f, 0.12f}, c); }
        else if (slot == "ring" || slot == "neck" || slot == "eyes" || slot == "face") ellipsoid(m, {0.0f, 0.02f, 0.0f}, {0.03f, 0.015f, 0.03f}, c, 4, 8);
        else box(m, {0.0f, 0.03f, 0.0f}, {0.2f, 0.03f, 0.15f}, c);
        if (q >= 1 && d.kind == orchard::ItemKind::container) box(m, {0.0f, 0.15f, 0.085f}, {0.1f, 0.08f, 0.02f}, a);
        return;
    }
    // Worn shells, sized for a 1.8 m humanoid with its feet at the origin of the anchor slot.
    const float t = d.kind == orchard::ItemKind::armor ? 0.03f : 0.012f;
    const int rs = q >= 2 ? 10 : 6, ss = q >= 2 ? 16 : 8;
    if (slot == "head") ellipsoid(m, {0.0f, 0.0f, 0.0f}, {0.115f + t, 0.12f + t, 0.125f + t}, c, rs, ss);
    else if (slot == "eyes") box(m, {0.0f, 0.0f, -0.12f}, {0.08f, 0.02f, 0.01f}, c);
    else if (slot == "face") box(m, {0.0f, -0.04f, -0.12f}, {0.08f, 0.06f, 0.01f}, c);
    else if (slot == "neck") rod(m, {0.0f, -0.03f, 0.0f}, {0.0f, 0.03f, 0.0f}, 0.065f, c, ss);
    else if (slot == "torso" || slot == "vest" || slot == "over") {
        const float g = slot == "over" ? 0.03f : slot == "vest" ? 0.02f : 0.0f;
        box(m, {0.0f, 0.0f, 0.0f}, {0.19f + t + g, 0.27f + g, 0.11f + t + g}, c);
        if (slot == "torso") { rod(m, {-0.22f, 0.2f, 0.0f}, {-0.26f, -0.1f, 0.0f}, 0.05f + t, c, rs); rod(m, {0.22f, 0.2f, 0.0f}, {0.26f, -0.1f, 0.0f}, 0.05f + t, c, rs); }
        if (q >= 1 && d.kind == orchard::ItemKind::container) for (float x : {-0.1f, 0.0f, 0.1f}) box(m, {x, -0.05f, -0.13f - t}, {0.04f, 0.05f, 0.025f}, a);
    } else if (slot == "back") {
        box(m, {0.0f, 0.0f, 0.18f}, {0.16f, 0.24f, 0.09f}, c);
        if (q >= 1) { box(m, {0.0f, -0.08f, 0.28f}, {0.12f, 0.08f, 0.02f}, a); rod(m, {-0.1f, 0.24f, 0.1f}, {-0.1f, -0.2f, 0.1f}, 0.012f, a, 4); rod(m, {0.1f, 0.24f, 0.1f}, {0.1f, -0.2f, 0.1f}, 0.012f, a, 4); }
    } else if (slot == "hands") {
        ellipsoid(m, {-0.3f, -0.35f, 0.0f}, {0.045f + t, 0.06f, 0.035f + t}, c, 5, 8);
        ellipsoid(m, {0.3f, -0.35f, 0.0f}, {0.045f + t, 0.06f, 0.035f + t}, c, 5, 8);
    } else if (slot == "waist") {
        rod(m, {0.0f, -0.025f, 0.0f}, {0.0f, 0.025f, 0.0f}, 0.17f, c, ss);
    } else if (slot == "legs") {
        rod(m, {-0.09f, 0.0f, 0.0f}, {-0.09f, -0.8f, 0.0f}, 0.07f + t, c, rs);
        rod(m, {0.09f, 0.0f, 0.0f}, {0.09f, -0.8f, 0.0f}, 0.07f + t, c, rs);
    } else if (slot == "feet") {
        box(m, {-0.09f, 0.05f, -0.04f}, {0.05f + t, 0.05f, 0.12f + t}, c);
        box(m, {0.09f, 0.05f, -0.04f}, {0.05f + t, 0.05f, 0.12f + t}, c);
    } else if (slot == "shoulders") {
        ellipsoid(m, {-0.22f, 0.0f, 0.0f}, {0.08f, 0.05f, 0.09f}, c, 5, 8);
        ellipsoid(m, {0.22f, 0.0f, 0.0f}, {0.08f, 0.05f, 0.09f}, c, 5, 8);
    } else if (slot == "ring" || slot == "ring2") {
        rod(m, {0.0f, -0.003f, 0.0f}, {0.0f, 0.003f, 0.0f}, 0.011f, c, 8);
    } else {
        box(m, {0.0f, 0.0f, 0.0f}, {0.06f, 0.08f, 0.03f}, c);
    }
}

MeshData build_item(const orchard::ItemDef& d, const std::string& variant) {
    MeshData m;
    const int q = variant == "simple" ? 0 : variant == "realistic" ? 2 : 1;
    using K = orchard::ItemKind;
    if (variant == "liquid") {
        item_flask(m, d, 2, true);
        return m;
    }
    const bool worn = variant == "worn";
    if (worn && d.kind != K::clothing && d.kind != K::armor && d.kind != K::trinket && d.kind != K::container && d.kind != K::holster && !d.wearable()) {
        // Held in a hand: same mesh as on the ground, but with the grip at the origin.
        MeshData held;
        switch (d.kind) {
        case K::gun: item_gun(held, d, 1); break;
        case K::melee: case K::tool: item_melee(held, d, 1); break;
        case K::bow: item_bow(held, d, 1); break;
        case K::magic: item_magic(held, d, 1); break;
        default: held = build_item(d, "detailed"); break;
        }
        return held;
    }
    if (d.wearable() && (worn || d.kind == K::clothing || d.kind == K::armor || d.kind == K::trinket || d.kind == K::container || d.kind == K::holster)) {
        item_wear(m, d, worn ? 1 : q, worn);
        if (worn) return m;
    } else
    switch (d.kind) {
    case K::gun: item_gun(m, d, q); break;
    case K::melee: case K::tool: item_melee(m, d, q); break;
    case K::bow: item_bow(m, d, q); break;
    case K::magic: item_magic(m, d, q); break;
    case K::potion: case K::drink: item_flask(m, d, q, false); break;
    case K::ammo: {
        const int n = q >= 1 ? 5 : 1;
        for (int i = 0; i < n; ++i) rod(m, {(i - n / 2) * 0.015f, 0.0f, 0.0f}, {(i - n / 2) * 0.015f, 0.0f, -std::max(0.03f, d.length)}, 0.005f, rgb(d.color), q >= 2 ? 8 : 4);
        break;
    }
    case K::throwable:
        if (d.shape == "knife" || d.shape == "dagger" || d.shape == "hatchet" || d.shape == "spear" || d.shape == "shuriken") item_melee(m, d, q);
        else if (d.shape == "dynamite" || d.shape == "stick_grenade" || d.shape == "pipe") rod(m, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -std::max(0.12f, d.length)}, 0.02f, rgb(d.color), q >= 1 ? 10 : 5);
        else if (d.shape == "bottle" || d.shape == "can") item_flask(m, d, q, false);
        else ellipsoid(m, {0.0f, 0.04f, 0.0f}, {0.032f, 0.042f, 0.032f}, rgb(d.color), q >= 1 ? 6 : 3, q >= 1 ? 10 : 5);
        if (q >= 1 && d.shape == "grenade") rod(m, {0.0f, 0.08f, 0.0f}, {0.0f, 0.095f, 0.0f}, 0.01f, rgb(d.accent), 6);
        break;
    case K::food:
        if (d.shape == "can" || d.shape == "box" || d.shape == "bar") box(m, {0.0f, 0.04f, 0.0f}, {0.04f, 0.04f, d.shape == "bar" ? 0.07f : 0.04f}, rgb(d.color));
        else if (d.shape == "carrot" || d.shape == "fish") rod(m, {0.0f, 0.03f, 0.08f}, {0.0f, 0.03f, -0.08f}, 0.025f, rgb(d.color), q >= 1 ? 8 : 4);
        else ellipsoid(m, {0.0f, 0.04f, 0.0f}, {0.045f, 0.04f, 0.045f}, rgb(d.color), q >= 1 ? 6 : 3, q >= 1 ? 10 : 5);
        break;
    case K::medical:
        if (d.shape == "syringe" || d.shape == "tube") rod(m, {0.0f, 0.0f, 0.06f}, {0.0f, 0.0f, -0.06f}, 0.01f, rgb(d.color), 6);
        else box(m, {0.0f, 0.04f, 0.0f}, {0.08f, 0.04f, 0.06f}, rgb(d.color));
        if (q >= 1 && d.shape == "medkit") { box(m, {0.0f, 0.081f, 0.0f}, {0.03f, 0.002f, 0.008f}, {0.9f, 0.1f, 0.1f, 1.0f}); box(m, {0.0f, 0.081f, 0.0f}, {0.008f, 0.002f, 0.03f}, {0.9f, 0.1f, 0.1f, 1.0f}); }
        break;
    default: {
        const float L = std::clamp(d.length, 0.04f, 1.5f);
        box(m, {0.0f, L * 0.25f, 0.0f}, {L * 0.5f, L * 0.25f, L * 0.4f}, rgb(d.color));
        break;
    }
    }
    if (m.positions.empty()) box(m, {0.0f, 0.05f, 0.0f}, {0.05f, 0.05f, 0.05f}, rgb(d.color));
    // Lying on the ground the item's collider is centred on it, so centre the mesh too.
    V lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    for (V p : m.positions) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    const V c{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
    for (V& p : m.positions) p = {p.x - c.x, p.y - c.y, p.z - c.z};
    return m;
}

template <class T>
void put(std::string& bin, const std::vector<T>& v) {
    while (bin.size() % 4) bin.push_back('\0');
    const std::size_t at = bin.size();
    bin.resize(at + v.size() * sizeof(T));
    if (!v.empty()) std::memcpy(&bin[at], v.data(), v.size() * sizeof(T));
}

} // namespace

const std::vector<std::string>& builtin_model_names() {
    static const std::vector<std::string> all = {"tree", "pine", "bush", "rock", "character-pixel", "character-mannequin", "character-robot"};
    return all;
}

static std::string glb_of(const MeshData& m, const std::string& name) {
    std::string bin;
    std::vector<std::size_t> offsets, lengths;
    auto add = [&](auto&& v) {
        while (bin.size() % 4) bin.push_back('\0');
        offsets.push_back(bin.size());
        put(bin, v);
        lengths.push_back(bin.size() - offsets.back());
    };
    add(m.positions);
    add(m.normals);
    add(m.colors);
    add(m.indices);
    while (bin.size() % 4) bin.push_back('\0');
    V lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    for (V p : m.positions) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    std::ostringstream js;
    js << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Orchard\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],";
    js << "\"nodes\":[{\"name\":\"" << name << "\",\"mesh\":0}],";
    js << "\"meshes\":[{\"name\":\"" << name << "\",\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"COLOR_0\":2},\"indices\":3,\"material\":0}]}],";
    bool see_through = false;
    for (const auto& c : m.colors) see_through = see_through || c[3] < 0.99f;
    js << "\"materials\":[{\"name\":\"" << name << "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":0.9}" << (see_through ? ",\"alphaMode\":\"BLEND\",\"doubleSided\":true" : "") << "}],";
    js << "\"buffers\":[{\"byteLength\":" << bin.size() << "}],\"bufferViews\":[";
    for (std::size_t i = 0; i < 4; ++i) js << (i ? "," : "") << "{\"buffer\":0,\"byteOffset\":" << offsets[i] << ",\"byteLength\":" << lengths[i] << ",\"target\":" << (i == 3 ? 34963 : 34962) << "}";
    js << "],\"accessors\":[";
    js << "{\"bufferView\":0,\"componentType\":5126,\"count\":" << m.positions.size() << ",\"type\":\"VEC3\",\"min\":[" << lo.x << "," << lo.y << "," << lo.z << "],\"max\":[" << hi.x << "," << hi.y << "," << hi.z << "]},";
    js << "{\"bufferView\":1,\"componentType\":5126,\"count\":" << m.normals.size() << ",\"type\":\"VEC3\"},";
    js << "{\"bufferView\":2,\"componentType\":5126,\"count\":" << m.colors.size() << ",\"type\":\"VEC4\"},";
    js << "{\"bufferView\":3,\"componentType\":5125,\"count\":" << m.indices.size() << ",\"type\":\"SCALAR\"}]}";
    std::string json = js.str();
    while (json.size() % 4) json.push_back(' ');
    auto u32 = [](std::string& s, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xffu));
    };
    std::string out;
    u32(out, 0x46546C67u);
    u32(out, 2u);
    u32(out, static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
    u32(out, static_cast<std::uint32_t>(json.size()));
    u32(out, 0x4E4F534Au);
    out += json;
    u32(out, static_cast<std::uint32_t>(bin.size()));
    u32(out, 0x004E4942u);
    out += bin;
    return out;
}


std::string builtin_model_glb(const std::string& name) {
    return glb_of(build(name), name);
}

std::string item_model_glb(const ItemDef& def, const std::string& variant) {
    return glb_of(build_item(def, variant), def.id + "-" + variant);
}

bool write_item_model(const ItemDef& def, const std::string& variant, const std::string& path, std::string* error) {
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    const std::string data = item_model_glb(def, variant);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

bool write_builtin_model(const std::string& name, const std::string& path, std::string* error) {
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "can't write " + path;
        return false;
    }
    const std::string data = builtin_model_glb(name);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

} // namespace editor
} // namespace orchard
