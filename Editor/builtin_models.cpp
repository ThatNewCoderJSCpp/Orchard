#include "builtin_models.hpp"

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

MeshData build(const std::string& name) {
    MeshData m;
    const std::array<float, 4> bark{0.36f, 0.24f, 0.14f, 1.0f};
    if (name == "pine") {
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

template <class T>
void put(std::string& bin, const std::vector<T>& v) {
    while (bin.size() % 4) bin.push_back('\0');
    const std::size_t at = bin.size();
    bin.resize(at + v.size() * sizeof(T));
    if (!v.empty()) std::memcpy(&bin[at], v.data(), v.size() * sizeof(T));
}

} // namespace

const std::vector<std::string>& builtin_model_names() {
    static const std::vector<std::string> all = {"tree", "pine", "bush", "rock"};
    return all;
}

std::string builtin_model_glb(const std::string& name) {
    const MeshData m = build(name);
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
    js << "\"materials\":[{\"name\":\"" << name << "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":0.9}}],";
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
