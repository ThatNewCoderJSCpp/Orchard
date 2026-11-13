#include "builtin_models.hpp"
#include "plant_models.hpp"

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

void item_gun(MeshData& m, const orchard::ItemDef& d, int q, bool open = false) {
    const float L = std::clamp(d.length, 0.12f, 2.0f);
    const C4 metal = rgb(d.color), dark = rgb(d.color, 0.6f), wood = rgb(d.accent), black{0.08f, 0.08f, 0.09f, 1.0f};
    const std::string act = orchard::gun_action_of(d);
    const std::string& sh = d.shape;
    const bool revolver = sh == "revolver" || act == "revolver_da" || act == "revolver_sa";
    const bool handgun = sh == "pistol" || revolver || sh == "flintlock";
    const bool bullpup = sh == "bullpup";
    const bool launcher = sh == "launcher";
    const bool rifle = L > 0.45f;
    if (launcher && d.ammo != "40mm" && d.ammo != "flare") {
        rod(m, {0.0f, 0.06f, 0.35f}, {0.0f, 0.06f, -L + 0.35f}, 0.045f, metal, q >= 1 ? 16 : 8);
        box(m, {0.0f, -0.02f, 0.0f}, {0.015f, 0.05f, 0.02f}, black);
        if (q >= 1) box(m, {0.03f, 0.11f, -0.1f}, {0.012f, 0.02f, 0.05f}, black);
        return;
    }
    if (handgun) {
        const float body = L * 0.75f;
        const float back = open ? 0.035f : 0.0f;
        if (revolver) {
            box(m, {0.0f, 0.05f, -0.01f}, {0.012f, 0.022f, 0.03f}, metal);
            rod(m, {0.0f, 0.052f, -0.035f}, {0.0f, 0.052f, -0.075f}, 0.024f, metal, q >= 1 ? 12 : 6);
            if (q >= 1)
                for (int i = 0; i < 6; ++i) {
                    const float a = two_pi * static_cast<float>(i) / 6.0f;
                    rod(m, {std::cos(a) * 0.014f, 0.052f + std::sin(a) * 0.014f, -0.0745f}, {std::cos(a) * 0.014f, 0.052f + std::sin(a) * 0.014f, -0.076f}, 0.005f, black, 6);
                }
            rod(m, {0.0f, 0.06f, -0.075f}, {0.0f, 0.06f, -L + 0.02f}, 0.008f, metal, q >= 1 ? 10 : 6);
            box(m, {0.0f, 0.075f, 0.022f}, {0.004f, 0.012f, 0.008f}, dark);
            box(m, {0.0f, -0.015f, 0.025f}, {0.013f, 0.045f, 0.018f}, wood);
            if (q >= 1) rod(m, {0.0f, 0.03f, -0.03f}, {0.0f, 0.005f, -0.01f}, 0.003f, dark, 6);
            return;
        }
        box(m, {0.0f, 0.06f, -body * 0.5f + 0.04f + back}, {0.013f, 0.018f, body * 0.5f}, metal);
        box(m, {0.0f, 0.032f, -body * 0.45f + 0.03f}, {0.012f, 0.012f, body * 0.42f}, d.color.x < 0.2f ? black : dark);
        rod(m, {0.0f, 0.06f, -body + 0.04f + back}, {0.0f, 0.06f, -body + 0.03f - (open ? 0.002f : 0.0f)}, 0.007f, dark, 8);
        if (open) rod(m, {0.0f, 0.06f, -body + 0.04f}, {0.0f, 0.06f, -body + 0.08f + back}, 0.0072f, rgb(orchard::Vec3{0.55f, 0.56f, 0.58f}), 8);
        box(m, {0.0f, -0.015f, 0.018f}, {0.012f, 0.05f, 0.018f}, d.color.x < 0.2f ? black : wood);
        if (d.magazine > 12 && d.gun_class == "machine_pistol") box(m, {0.0f, -0.08f, 0.018f}, {0.01f, 0.03f, 0.014f}, black);
        if (q >= 1) {
            box(m, {0.0f, 0.081f, -body + 0.05f + back}, {0.002f, 0.004f, 0.003f}, dark);
            box(m, {0.0f, 0.081f, 0.03f + back}, {0.006f, 0.004f, 0.003f}, dark);
            rod(m, {0.0f, 0.02f, -0.005f}, {0.0f, 0.005f, -0.02f}, 0.003f, dark, 6);
            for (int i = 0; i < 4; ++i) box(m, {0.0135f, 0.062f, 0.02f - i * 0.006f + back}, {0.0006f, 0.012f, 0.0015f}, dark);
        }
        if (d.has("suppressed")) rod(m, {0.0f, 0.06f, -body + 0.03f}, {0.0f, 0.06f, -body - 0.12f}, 0.016f, black, 12);
        return;
    }
    const float body = bullpup ? L * 0.55f : rifle ? L * 0.42f : L * 0.6f;
    const float rz0 = bullpup ? 0.18f : 0.05f;
    box(m, {0.0f, 0.05f, rz0 - body * 0.5f}, {0.024f, 0.034f, body * 0.5f}, metal);
    const float barrel_start = rz0 - body;
    const bool doubles = act == "break" && d.magazine == 2;
    if (doubles) {
        rod(m, {-0.012f, 0.06f, barrel_start}, {-0.012f, 0.06f, -L + 0.02f}, 0.011f, dark, q >= 1 ? 12 : 6);
        rod(m, {0.012f, 0.06f, barrel_start}, {0.012f, 0.06f, -L + 0.02f}, 0.011f, dark, q >= 1 ? 12 : 6);
    } else {
        rod(m, {0.0f, 0.06f, barrel_start}, {0.0f, 0.06f, -L + 0.06f}, rifle ? 0.011f : 0.009f, dark, q >= 2 ? 12 : 6);
    }
    box(m, {0.0f, -0.035f, bullpup ? 0.02f - body * 0.45f : 0.0f}, {0.016f, 0.05f, 0.022f}, act == "lever" || d.accent.x > 0.3f ? wood : black);
    const bool tube = act == "pump" || act == "lever" || (sh == "shotgun" && d.feed == "internal");
    if (tube && !doubles) rod(m, {0.0f, 0.035f, barrel_start}, {0.0f, 0.035f, -L * 0.85f}, 0.009f, dark, 8);
    if (act == "pump") box(m, {0.0f, 0.035f, barrel_start - L * 0.18f + (open ? 0.05f : 0.0f)}, {0.022f, 0.022f, L * 0.1f}, wood);
    else if (rifle && !bullpup && !doubles) box(m, {0.0f, 0.045f, barrel_start - L * 0.12f}, {0.02f, 0.022f, L * 0.13f}, wood);
    if (act == "lever") {
        rod(m, {0.0f, 0.0f, 0.03f}, {0.0f, -0.05f, 0.03f}, 0.004f, metal, 6);
        rod(m, {0.0f, -0.05f, 0.03f}, {0.0f, -0.06f, -0.04f}, 0.004f, metal, 6);
        rod(m, {0.0f, -0.06f, -0.04f}, {0.0f, 0.0f, -0.06f}, 0.004f, metal, 6);
    }
    if (act == "bolt" || act == "straight_pull") {
        const float bz = rz0 - 0.03f + (open ? 0.06f : 0.0f);
        rod(m, {0.02f, 0.065f, bz}, {0.06f, act == "bolt" ? 0.045f : 0.065f, bz}, 0.004f, metal, 6);
        ellipsoid(m, {0.062f, act == "bolt" ? 0.044f : 0.065f, bz}, {0.008f, 0.008f, 0.008f}, metal, 4, 8);
    } else if (!tube && !doubles && act != "muzzle") {
        box(m, {0.026f, 0.06f, rz0 - body * 0.35f + (open ? 0.03f : 0.0f)}, {0.004f, 0.006f, 0.012f}, dark);
    }
    const bool belt = d.feed == "belt";
    const bool detachable = d.feed == "magazine" && d.magazine > 1;
    const float mag_z = bullpup ? rz0 - 0.03f : rz0 - body * 0.55f;
    if (belt) {
        box(m, {0.06f, 0.0f, mag_z}, {0.035f, 0.05f, 0.05f}, rgb(orchard::Vec3{0.33f, 0.37f, 0.22f}));
        if (q >= 1) for (int i = 0; i < 4; ++i) rod(m, {0.025f, 0.05f - i * 0.008f, mag_z - 0.02f + i * 0.01f}, {0.025f, 0.05f - i * 0.008f, mag_z - 0.02f + i * 0.01f + 0.004f}, 0.004f, rgb(orchard::Vec3{0.85f, 0.68f, 0.3f}), 6);
    } else if (detachable) {
        if (d.magazine >= 50 && d.gun_class != "pdw") {
            rod(m, {-0.025f, -0.03f, mag_z}, {0.025f, -0.03f, mag_z}, 0.06f, black, q >= 1 ? 14 : 8);
        } else {
            const bool curved = d.caliber == "762x39" || d.caliber == "545x39" || d.ammo == "762x39" || d.ammo == "545x39";
            const float len = std::clamp(0.05f + static_cast<float>(d.magazine) * 0.0035f, 0.05f, 0.2f);
            const int segs = curved ? 4 : 1;
            for (int i = 0; i < segs; ++i) {
                const float k0 = static_cast<float>(i) / segs, k1 = static_cast<float>(i + 1) / segs;
                const float lean = curved ? (k0 + k1) * 0.5f * (k0 + k1) * 0.5f * 0.06f : 0.0f;
                box(m, {0.0f, 0.02f - len * (k0 + k1) * 0.5f, mag_z - lean}, {0.012f, len * 0.5f / segs, 0.022f}, black);
            }
        }
    }
    if (rifle && !bullpup) {
        if (d.gun_class == "smg" || d.gun_class == "pdw" || d.has("folding")) rod(m, {0.0f, 0.04f, rz0}, {0.0f, 0.03f, rz0 + L * 0.3f}, 0.005f, dark, 6);
        else box(m, {0.0f, 0.03f, rz0 + L * 0.13f}, {0.018f, 0.04f, L * 0.13f}, wood);
    }
    if (bullpup) box(m, {0.0f, 0.02f, rz0 + 0.04f}, {0.02f, 0.045f, 0.04f}, black);
    if (q >= 1) {
        box(m, {0.0f, 0.085f, -L + 0.09f}, {0.003f, 0.012f, 0.004f}, dark);
        box(m, {0.0f, 0.087f, rz0 - 0.02f}, {0.008f, 0.008f, 0.006f}, dark);
        box(m, {0.0f, -0.005f, (bullpup ? -body * 0.45f : 0.0f) - 0.03f}, {0.004f, 0.012f, 0.02f}, dark);
    }
    if (q >= 2) {
        if (d.has("scoped") || d.category.find("Sniper") != std::string::npos) {
            rod(m, {0.0f, 0.115f, rz0 - 0.02f}, {0.0f, 0.115f, rz0 - 0.3f}, 0.018f, black, 14);
            rod(m, {0.0f, 0.115f, rz0 - 0.3f}, {0.0f, 0.115f, rz0 - 0.34f}, 0.024f, black, 14);
            rod(m, {0.0f, 0.115f, rz0 + 0.01f}, {0.0f, 0.115f, rz0 - 0.02f}, 0.021f, black, 14);
        }
        if (!doubles) rod(m, {0.0f, 0.06f, -L + 0.08f}, {0.0f, 0.06f, -L + 0.05f}, rifle ? 0.015f : 0.012f, dark, 12);
    }
    if (d.has("suppressed")) rod(m, {0.0f, 0.06f, -L + 0.06f}, {0.0f, 0.06f, -L - 0.12f}, 0.022f, black, 12);
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
        if (d.kind == orchard::ItemKind::container) box(m, {0.0f, 0.15f, 0.0f}, {0.15f, 0.15f, 0.08f}, c);
        else if (slot == "head") ellipsoid(m, {0.0f, 0.06f, 0.0f}, {0.11f, 0.07f, 0.12f}, c, 6, 10);
        else if (slot == "feet") { box(m, {-0.06f, 0.04f, 0.0f}, {0.045f, 0.04f, 0.12f}, c); box(m, {0.06f, 0.04f, 0.0f}, {0.045f, 0.04f, 0.12f}, c); }
        else if (slot == "ring" || slot == "neck" || slot == "eyes" || slot == "face") ellipsoid(m, {0.0f, 0.02f, 0.0f}, {0.03f, 0.015f, 0.03f}, c, 4, 8);
        else box(m, {0.0f, 0.03f, 0.0f}, {0.2f, 0.03f, 0.15f}, c);
        if (q >= 1 && d.kind == orchard::ItemKind::container) box(m, {0.0f, 0.15f, 0.085f}, {0.1f, 0.08f, 0.02f}, a);
        return;
    }
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

void item_round(MeshData& m, const orchard::ItemDef& d, const orchard::BulletType* b, V at, float scale) {
    const float len = std::max(0.006f, d.length) * scale;
    const float r = std::clamp(len * (d.cartridge_class == "shotgun" ? 0.28f : d.cartridge_class == "airgun" ? 0.45f : 0.18f), 0.002f, 0.02f);
    const C4 casing = rgb(d.color);
    const C4 jacket = rgb(b && (b->tip == "soft" || b->tip == "flat") ? orchard::Vec3{0.55f, 0.55f, 0.58f} : orchard::Vec3{0.78f, 0.48f, 0.25f});
    const C4 tip = rgb(b ? b->tip_color : orchard::Vec3{0.78f, 0.48f, 0.25f});
    if (d.cartridge_class == "shotgun") {
        rod(m, at, {at.x, at.y + len * 0.2f, at.z}, r * 1.05f, rgb(orchard::Vec3{0.85f, 0.68f, 0.3f}), 10);
        rod(m, {at.x, at.y + len * 0.2f, at.z}, {at.x, at.y + len, at.z}, r, tip, 10);
        return;
    }
    const float case_len = len * (d.cartridge_class == "airgun" ? 0.0f : d.cartridge_class == "rifle" || d.cartridge_class == "specialty" ? 0.65f : 0.5f);
    if (case_len > 0.0f) rod(m, at, {at.x, at.y + case_len, at.z}, r, casing, 10);
    const float bullet_r = d.cartridge_class == "rifle" || d.cartridge_class == "specialty" ? r * 0.62f : r * 0.92f;
    const float nose_len = len - case_len;
    const std::string shape = b ? b->tip : std::string("round");
    const int segs = 5;
    for (int i = 0; i < segs; ++i) {
        const float k0 = static_cast<float>(i) / segs, k1 = static_cast<float>(i + 1) / segs;
        auto radius = [&](float k) {
            if (shape == "flat") return bullet_r;
            if (shape == "pointed" || shape == "boat") return bullet_r * std::sqrt(std::max(0.0f, 1.0f - k * k));
            return bullet_r * std::sqrt(std::max(0.0f, 1.0f - std::pow(k, 2.4f)));
        };
        const bool painted = (shape == "pointed" || shape == "open") ? k0 >= 0.7f : (shape == "soft" || shape == "hollow") ? k0 >= 0.78f : false;
        cone(m, at.y + case_len + nose_len * k0, at.y + case_len + nose_len * k1, radius(k0), std::max(0.0004f, radius(k1)), 10, painted ? tip : jacket, 0u, 0.0f);
    }
}

MeshData build_item(const orchard::ItemDef& d, const std::string& variant) {
    MeshData m;
    if (d.kind == orchard::ItemKind::ammo && (variant.rfind("round_", 0) == 0 || variant.rfind("pile_", 0) == 0)) {
        const std::string bid = variant.substr(variant.find('_') + 1);
        const orchard::BulletType* bt = nullptr;
        for (const orchard::BulletType& x : orchard::bullet_types())
            if (x.id == bid) bt = &x;
        if (variant.rfind("round_", 0) == 0) {
            item_round(m, d, bt, {0.0f, 0.0f, 0.0f}, 1.0f);
        } else {
            const float step = std::max(0.006f, d.length) * 0.5f;
            for (int i = 0; i < 6; ++i) {
                const float a = static_cast<float>(i) * 2.399f;
                item_round(m, d, bt, {std::cos(a) * step * std::sqrt(static_cast<float>(i)), 0.0f, std::sin(a) * step * std::sqrt(static_cast<float>(i))}, 1.0f);
            }
        }
    } else if (d.kind == orchard::ItemKind::magazine && (variant == "full" || variant == "empty" || variant == "detailed" || variant == "simple" || variant == "realistic")) {
        const C4 dark = rgb(d.color);
        const C4 brass = rgb(orchard::Vec3{0.85f, 0.68f, 0.3f});
        const bool full = variant != "empty";
        if (d.shape == "drum") {
            m = MeshData{};
            rod(m, {0.0f, 0.0f, -0.035f}, {0.0f, 0.0f, 0.035f}, 0.09f, dark, 18);
            box(m, {0.0f, 0.1f, 0.0f}, {0.02f, 0.04f, 0.015f}, dark);
            if (full) rod(m, {0.0f, 0.14f, 0.0f}, {0.0f, 0.16f, 0.0f}, 0.004f, brass, 8);
        } else if (d.shape == "ammo_box") {
            box(m, {0.0f, 0.06f, 0.0f}, {0.12f, 0.06f, 0.05f}, rgb(orchard::Vec3{0.33f, 0.37f, 0.22f}));
            if (full)
                for (int i = 0; i < 6; ++i) rod(m, {-0.09f + i * 0.035f, 0.12f, 0.0f}, {-0.09f + i * 0.035f, 0.145f, 0.0f}, 0.005f, brass, 8);
        } else {
            const float h = d.shape == "mag_pistol" ? 0.11f : 0.18f;
            const float w = d.shape == "mag_pistol" ? 0.014f : 0.016f;
            const float depth = d.shape == "mag_pistol" ? 0.03f : 0.06f;
            const int segs = d.shape == "mag_pistol" ? 1 : 4;
            for (int i = 0; i < segs; ++i) {
                const float k0 = static_cast<float>(i) / segs, k1 = static_cast<float>(i + 1) / segs;
                const float lean0 = d.shape == "mag_pistol" ? 0.0f : k0 * k0 * 0.05f, lean1 = d.shape == "mag_pistol" ? 0.0f : k1 * k1 * 0.05f;
                box(m, {0.0f, h * (1.0f - (k0 + k1) * 0.5f), (lean0 + lean1) * 0.5f}, {w, h * 0.5f / segs, depth * 0.5f}, dark);
            }
            box(m, {0.0f, -0.005f, (d.shape == "mag_pistol" ? 0.0f : 0.05f)}, {w * 1.2f, 0.006f, depth * 0.55f}, dark);
            if (full) {
                rod(m, {0.0f, h, -depth * 0.2f}, {0.0f, h + 0.004f, -depth * 0.2f}, w * 0.7f, brass, 8);
                cone(m, h + 0.004f, h + 0.016f, w * 0.55f, 0.001f, 8, rgb(orchard::Vec3{0.78f, 0.48f, 0.25f}), 0u, 0.0f);
            }
        }
        V lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
        for (V p : m.positions) {
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
        const V c{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
        for (V& p : m.positions) p = {p.x - c.x, p.y - c.y, p.z - c.z};
        return m;
    }
    if (!m.positions.empty()) {
        V lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
        for (V p : m.positions) {
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
        const V c{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
        for (V& p : m.positions) p = {p.x - c.x, p.y - c.y, p.z - c.z};
        return m;
    }
    const int q = variant == "simple" ? 0 : variant == "realistic" ? 2 : 1;
    using K = orchard::ItemKind;
    if (variant == "liquid") {
        item_flask(m, d, 2, true);
        return m;
    }
    const bool worn = variant == "worn" || variant == "worn_open";
    if (worn && d.kind != K::clothing && d.kind != K::armor && d.kind != K::trinket && d.kind != K::container && d.kind != K::holster && !d.wearable()) {
        MeshData held;
        switch (d.kind) {
        case K::gun: item_gun(held, d, 1, variant == "worn_open"); break;
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
    static const std::vector<std::string> all = [] {
        std::vector<std::string> v = {"tree", "pine", "bush", "rock", "character-pixel", "character-mannequin", "character-robot"};
        for (const PlantInfo& p : plant_models()) v.push_back(p.name);
        return v;
    }();
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



namespace {

struct Canvas {
    int size = 64;
    std::vector<std::uint8_t> px;

    explicit Canvas(int n) : size(n), px(static_cast<std::size_t>(n * n * 4), 0) {}

    void plot(int x, int y, Vec3 c, float a = 1.0f) {
        if (x < 0 || y < 0 || x >= size || y >= size || a <= 0.0f) return;
        std::uint8_t* p = &px[static_cast<std::size_t>((y * size + x) * 4)];
        const float old = p[3] / 255.0f;
        const float out = a + old * (1.0f - a);
        auto mix = [&](std::uint8_t& ch, float v) {
            const float prev = ch / 255.0f;
            const float res = out > 0.0f ? (std::clamp(v, 0.0f, 1.0f) * a + prev * old * (1.0f - a)) / out : 0.0f;
            ch = static_cast<std::uint8_t>(std::clamp(res, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        mix(p[0], c.x);
        mix(p[1], c.y);
        mix(p[2], c.z);
        p[3] = static_cast<std::uint8_t>(std::clamp(out, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    void cylinder_rect(float x0, float y0, float x1, float y1, Vec3 c) {
        for (int y = static_cast<int>(std::floor(y0)); y < static_cast<int>(std::ceil(y1)); ++y)
            for (int x = static_cast<int>(std::floor(x0)); x < static_cast<int>(std::ceil(x1)); ++x) {
                const float u = (x + 0.5f - x0) / std::max(0.5f, x1 - x0);
                const float shade = 0.55f + 0.75f * std::sin(std::clamp(u, 0.0f, 1.0f) * 3.14159f) - 0.25f * u;
                plot(x, y, {c.x * shade, c.y * shade, c.z * shade});
            }
    }

    void nose(float x0, float x1, float base_y, float tip_y, const std::string& tip, Vec3 c, Vec3 tip_color) {
        const float h = base_y - tip_y;
        for (int y = static_cast<int>(std::floor(tip_y)); y < static_cast<int>(std::ceil(base_y)); ++y) {
            const float k = std::clamp((base_y - (y + 0.5f)) / std::max(0.5f, h), 0.0f, 1.0f);
            float half = (x1 - x0) * 0.5f;
            if (tip == "flat") half *= k > 0.85f ? 0.92f : 1.0f;
            else if (tip == "pointed" || tip == "boat") half *= std::sqrt(std::max(0.0f, 1.0f - k * k)) * (1.0f - k * 0.15f);
            else half *= std::sqrt(std::max(0.0f, 1.0f - std::pow(k, 2.4f)));
            const float cx = (x0 + x1) * 0.5f;
            for (int x = static_cast<int>(std::floor(cx - half)); x < static_cast<int>(std::ceil(cx + half)); ++x) {
                const float u = (x + 0.5f - (cx - half)) / std::max(0.5f, half * 2.0f);
                const float shade = 0.55f + 0.75f * std::sin(std::clamp(u, 0.0f, 1.0f) * 3.14159f) - 0.25f * u;
                const bool painted = (tip == "pointed" || tip == "open") ? k > 0.7f : (tip == "soft" || tip == "hollow") ? k > 0.78f : false;
                Vec3 col = painted ? tip_color : c;
                if (tip == "hollow" && k > 0.93f && std::fabs(x + 0.5f - cx) < half * 0.55f) col = {0.12f, 0.08f, 0.05f};
                plot(x, y, {col.x * shade, col.y * shade, col.z * shade});
            }
        }
    }

    void disc(float cx, float cy, float r, Vec3 c, float a = 1.0f) {
        for (int y = static_cast<int>(cy - r - 1); y <= static_cast<int>(cy + r + 1); ++y)
            for (int x = static_cast<int>(cx - r - 1); x <= static_cast<int>(cx + r + 1); ++x) {
                const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
                if (d <= r) {
                    const float shade = 1.15f - 0.5f * ((x + 0.5f - cx) + (y + 0.5f - cy)) / std::max(1.0f, r);
                    plot(x, y, {c.x * shade * 0.8f, c.y * shade * 0.8f, c.z * shade * 0.8f}, a * std::clamp(r - d + 0.5f, 0.0f, 1.0f));
                }
            }
    }
};

void draw_round(Canvas& cv, float cx, float bottom, float width, float height, const ItemDef& d, Vec3 bullet, Vec3 tip_color, const std::string& tip) {
    const std::string& klass = d.cartridge_class;
    if (klass == "airgun") {
        const float w = width * 1.2f;
        cv.cylinder_rect(cx - w * 0.5f, bottom - height * 0.35f, cx + w * 0.5f, bottom, bullet);
        cv.cylinder_rect(cx - w * 0.22f, bottom - height * 0.65f, cx + w * 0.22f, bottom - height * 0.35f, bullet);
        cv.nose(cx - w * 0.42f, cx + w * 0.42f, bottom - height * 0.65f, bottom - height, tip, bullet, tip_color);
        return;
    }
    if (klass == "shotgun") {
        const Vec3 brass{0.85f, 0.68f, 0.3f};
        cv.cylinder_rect(cx - width * 0.5f, bottom - height * 0.22f, cx + width * 0.5f, bottom, brass);
        cv.cylinder_rect(cx - width * 0.47f, bottom - height, cx + width * 0.47f, bottom - height * 0.22f, tip_color);
        for (int i = 0; i < 3; ++i) cv.cylinder_rect(cx - width * 0.47f, bottom - height + 2.0f + i * 2.0f, cx + width * 0.47f, bottom - height + 3.0f + i * 2.0f, Vec3{tip_color.x * 0.7f, tip_color.y * 0.7f, tip_color.z * 0.7f});
        return;
    }
    const float case_h = height * (klass == "rifle" || klass == "specialty" ? 0.62f : 0.5f);
    cv.cylinder_rect(cx - width * 0.5f, bottom - case_h, cx + width * 0.5f, bottom, d.color);
    cv.cylinder_rect(cx - width * 0.55f, bottom - height * 0.06f, cx + width * 0.55f, bottom, Vec3{d.color.x * 0.85f, d.color.y * 0.85f, d.color.z * 0.85f});
    const float bw = klass == "rifle" || klass == "specialty" ? width * 0.62f : width * 0.92f;
    if (klass == "rifle" || klass == "specialty") cv.cylinder_rect(cx - width * 0.5f, bottom - case_h - height * 0.06f, cx + width * 0.5f, bottom - case_h, d.color);
    const float neck = bottom - case_h - (klass == "rifle" || klass == "specialty" ? height * 0.06f : 0.0f);
    cv.nose(cx - bw * 0.5f, cx + bw * 0.5f, neck, bottom - height, tip, bullet, tip_color);
}

} // namespace

std::vector<std::uint8_t> item_icon_rgba(const ItemDef& d, const std::string& variant, const BulletType* bullet, float fill, int size) {
    Canvas cv(size);
    const float s = static_cast<float>(size);
    const Vec3 copper{0.78f, 0.48f, 0.25f};
    const Vec3 lead{0.55f, 0.55f, 0.58f};
    Vec3 tip_color = bullet ? bullet->tip_color : copper;
    std::string tip = bullet ? bullet->tip : std::string("round");
    Vec3 body = copper;
    if (bullet && (bullet->tip == "soft" || bullet->tip == "flat")) body = bullet->tip == "flat" ? lead : copper;
    if (bullet && (bullet->id == "frangible" || bullet->id == "rubber")) body = bullet->tip_color;
    if (d.cartridge_class == "airgun") body = lead;
    if (d.kind == ItemKind::ammo) {
        const bool pile = variant.rfind("pile", 0) == 0;
        const float w = d.cartridge_class == "shotgun" ? s * 0.26f : d.cartridge_class == "rimfire" ? s * 0.12f : d.cartridge_class == "airgun" ? s * 0.18f : s * 0.17f;
        const float h = d.cartridge_class == "airgun" ? s * 0.3f : std::clamp(s * (0.35f + std::log10(std::max(10.0f, d.cartridge_grain)) * 0.12f), s * 0.3f, s * 0.88f);
        if (pile) {
            draw_round(cv, s * 0.3f, s * 0.92f, w * 0.85f, h * 0.85f, d, body, tip_color, tip);
            draw_round(cv, s * 0.7f, s * 0.92f, w * 0.85f, h * 0.85f, d, body, tip_color, tip);
            draw_round(cv, s * 0.5f, s * 0.96f, w * 0.85f, h * 0.85f, d, body, tip_color, tip);
        } else {
            draw_round(cv, s * 0.5f, s * 0.94f, w, h, d, body, tip_color, tip);
        }
    } else if (d.kind == ItemKind::magazine) {
        const bool drum = d.shape == "drum";
        const bool box = d.shape == "ammo_box";
        const Vec3 dark{0.16f, 0.16f, 0.17f};
        if (drum) {
            cv.disc(s * 0.5f, s * 0.55f, s * 0.36f, dark);
            cv.disc(s * 0.5f, s * 0.55f, s * 0.12f, Vec3{0.3f, 0.3f, 0.32f});
            if (fill > 0.0f) cv.disc(s * 0.5f, s * 0.18f, s * 0.06f, copper);
        } else if (box) {
            cv.cylinder_rect(s * 0.15f, s * 0.3f, s * 0.85f, s * 0.92f, Vec3{0.33f, 0.37f, 0.22f});
            cv.cylinder_rect(s * 0.15f, s * 0.24f, s * 0.85f, s * 0.32f, Vec3{0.25f, 0.28f, 0.18f});
            if (fill > 0.0f)
                for (int i = 0; i < 5; ++i) cv.nose(s * (0.22f + i * 0.12f), s * (0.3f + i * 0.12f), s * 0.24f, s * 0.12f, "pointed", copper, copper);
        } else {
            const float w = d.shape == "mag_pistol" ? s * 0.26f : s * 0.32f;
            const float top = s * 0.18f;
            for (int y = static_cast<int>(top); y < static_cast<int>(s * 0.95f); ++y) {
                const float k = (y - top) / (s * 0.77f);
                const float lean = d.shape == "mag_pistol" ? 0.0f : k * k * s * 0.14f;
                for (int x = static_cast<int>(s * 0.5f - w * 0.5f + lean); x < static_cast<int>(s * 0.5f + w * 0.5f + lean); ++x) {
                    const float u = (x - (s * 0.5f - w * 0.5f + lean)) / w;
                    const float shade = 0.7f + 0.5f * std::sin(u * 3.14159f);
                    cv.plot(x, y, {dark.x * shade, dark.y * shade, dark.z * shade});
                }
            }
            const float win_top = s * 0.3f, win_bot = s * 0.85f;
            for (int i = 0; i < 6; ++i) {
                const float y = win_bot - (win_bot - win_top) * (i + 0.5f) / 6.0f;
                const bool on = fill * 6.0f > i;
                cv.disc(s * 0.5f + (d.shape == "mag_pistol" ? 0.0f : ((y - top) / (s * 0.77f)) * ((y - top) / (s * 0.77f)) * s * 0.14f), y, s * 0.03f, on ? Vec3{0.85f, 0.68f, 0.3f} : Vec3{0.08f, 0.08f, 0.08f});
            }
            if (fill > 0.0f) {
                cv.cylinder_rect(s * 0.5f - w * 0.32f, top - s * 0.04f, s * 0.5f + w * 0.32f, top + s * 0.01f, Vec3{0.85f, 0.68f, 0.3f});
                cv.nose(s * 0.5f - w * 0.28f, s * 0.5f + w * 0.28f, top - s * 0.04f, top - s * 0.16f, tip, body, tip_color);
            }
        }
    }
    return cv.px;
}

std::string builtin_model_glb(const std::string& name) {
    if (is_plant_model(name)) return plant_model_glb(name);
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
