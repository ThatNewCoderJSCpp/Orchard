#include "plant_models.hpp"

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace orchard {
namespace editor {

namespace {

constexpr float pi = 3.14159265f;

struct P {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

P operator+(P a, P b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
P operator-(P a, P b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
P operator*(P a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(P a, P b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P cross(P a, P b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float len(P a) { return std::sqrt(dot(a, a)); }
P unit(P a) {
    const float l = len(a);
    return l > 1e-8f ? a * (1.0f / l) : P{0.0f, 1.0f, 0.0f};
}
P perp(P a) { return unit(std::fabs(a.y) < 0.9f ? cross(a, P{0.0f, 1.0f, 0.0f}) : cross(a, P{1.0f, 0.0f, 0.0f})); }
P rotate(P v, P axis, float angle) {
    axis = unit(axis);
    const float c = std::cos(angle), s = std::sin(angle);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}

struct Rng {
    std::uint32_t s;
    explicit Rng(std::uint32_t seed) : s(seed * 2654435761u + 12345u) {}
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s & 0xffffffu) / 16777216.0f;
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};

using C4 = std::array<float, 4>;

struct Mesh {
    std::vector<P> pos;
    std::vector<P> nrm;
    std::vector<std::array<float, 2>> uv;
    std::vector<C4> col;
    std::vector<std::uint32_t> idx;
    int material = 0;

    std::uint32_t vertex(P p, P n, float u, float v, C4 c) {
        pos.push_back(p);
        nrm.push_back(unit(n));
        uv.push_back({u, v});
        col.push_back(c);
        return static_cast<std::uint32_t>(pos.size() - 1);
    }
    void tri(std::uint32_t a, std::uint32_t b, std::uint32_t c, P outward) {
        const P n = cross(pos[b] - pos[a], pos[c] - pos[a]);
        if (dot(n, outward) >= 0.0f) {
            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(c);
        } else {
            idx.push_back(a);
            idx.push_back(c);
            idx.push_back(b);
        }
    }
};

std::uint32_t hash3(int x, int y, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float lattice(int x, int y, int px, int py, std::uint32_t seed) {
    x = ((x % px) + px) % px;
    y = ((y % py) + py) % py;
    return static_cast<float>(hash3(x, y, seed) & 0xffff) / 65535.0f;
}

float vnoise(float x, float y, int px, int py, std::uint32_t seed) {
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    const float a = lattice(ix, iy, px, py, seed), b = lattice(ix + 1, iy, px, py, seed);
    const float c = lattice(ix, iy + 1, px, py, seed), d = lattice(ix + 1, iy + 1, px, py, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

float fbm(float u, float v, int fx, int fy, std::uint32_t seed, int octaves = 4) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        const int sx = fx << o, sy = fy << o;
        sum += vnoise(u * sx, v * sy, sx, sy, seed + static_cast<std::uint32_t>(o) * 31u) * amp;
        norm += amp;
        amp *= 0.5f;
    }
    return sum / norm;
}

struct Image {
    int w = 0, h = 0;
    std::vector<std::array<float, 4>> px;
    Image(int ww, int hh) : w(ww), h(hh), px(static_cast<std::size_t>(ww * hh), {0.0f, 0.0f, 0.0f, 1.0f}) {}
    std::array<float, 4>& at(int x, int y) { return px[static_cast<std::size_t>(((y % h + h) % h) * w + ((x % w + w) % w))]; }
};

std::vector<std::uint8_t> png_of(const Image& img) {
    fizmo::images::BitmapImage out(static_cast<unsigned>(img.w), static_cast<unsigned>(img.h));
    for (int y = 0; y < img.h; ++y)
        for (int x = 0; x < img.w; ++x) {
            const auto& c = img.px[static_cast<std::size_t>(y * img.w + x)];
            auto b = [](float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
            out.set_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y), fizmo::colors::Color(b(c[0]), b(c[1]), b(c[2]), b(c[3])));
        }
    std::vector<std::uint8_t> bytes;
    fizmo::images::encode_png(out, bytes, true, 6);
    return bytes;
}

Image normal_from_height(const std::vector<float>& h, int w, int hh, float strength) {
    Image n(w, hh);
    auto H = [&](int x, int y) { return h[static_cast<std::size_t>(((y % hh + hh) % hh) * w + ((x % w + w) % w))]; };
    for (int y = 0; y < hh; ++y)
        for (int x = 0; x < w; ++x) {
            const float dx = (H(x + 1, y) - H(x - 1, y)) * strength;
            const float dy = (H(x, y + 1) - H(x, y - 1)) * strength;
            const P v = unit(P{-dx, -dy, 1.0f});
            n.at(x, y) = {v.x * 0.5f + 0.5f, v.y * 0.5f + 0.5f, v.z * 0.5f + 0.5f, 1.0f};
        }
    return n;
}

void bark_images(const std::string& kind, std::vector<std::uint8_t>& color, std::vector<std::uint8_t>& normal) {
    const int W = 256, H = 256;
    Image img(W, H);
    std::vector<float> height(static_cast<std::size_t>(W * H));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float u = static_cast<float>(x) / W, v = static_cast<float>(y) / H;
            float h = 0.0f;
            P c;
            if (kind == "birch") {
                const float base = fbm(u, v, 4, 4, 11u);
                const float band = fbm(u, v, 3, 24, 12u, 2);
                const float dash = vnoise(u * 24.0f, v * 3.0f, 24, 3, 13u);
                const float patch = fbm(u, v, 2, 3, 14u, 3);
                c = P{0.86f, 0.84f, 0.78f} * (0.9f + base * 0.15f);
                h = 0.5f + base * 0.2f;
                if (band > 0.66f && dash > 0.45f) {
                    c = P{0.22f, 0.19f, 0.17f};
                    h = 0.25f;
                }
                if (patch > 0.7f) {
                    c = P{0.12f, 0.11f, 0.1f} + c * 0.15f;
                    h = 0.2f;
                }
            } else if (kind == "palm") {
                const float ring = std::fmod(v * 10.0f + fbm(u, v, 4, 4, 21u) * 0.4f, 1.0f);
                const float diamond = std::fabs(std::sin((u * 6.0f + v * 10.0f) * pi)) * std::fabs(std::sin((u * 6.0f - v * 10.0f) * pi));
                h = 0.4f + 0.4f * std::pow(ring, 0.5f) * (0.5f + diamond * 0.5f);
                c = P{0.38f, 0.31f, 0.22f} * (0.7f + h * 0.6f);
            } else if (kind == "pine" || kind == "spruce") {
                const float plates = vnoise(u * 7.0f, v * 4.0f, 7, 4, kind == "pine" ? 31u : 32u);
                const float crack = std::fabs(vnoise(u * 7.0f + 0.5f, v * 4.0f + 0.5f, 7, 4, 33u) - 0.5f) * 2.0f;
                const float grain = fbm(u, v, 8, 8, 34u);
                h = std::clamp(plates * 0.6f + (crack < 0.15f ? -0.4f : 0.0f) + grain * 0.3f, 0.0f, 1.0f);
                const P light = kind == "pine" ? P{0.55f, 0.32f, 0.2f} : P{0.38f, 0.32f, 0.28f};
                const P dark = kind == "pine" ? P{0.2f, 0.12f, 0.08f} : P{0.15f, 0.12f, 0.1f};
                c = dark + (light - dark) * h;
            } else {
                const float warp = fbm(u, v, 4, 3, 41u);
                const float ridge = std::fabs(std::sin((u * (kind == "maple" ? 14.0f : 10.0f) + warp * 1.6f) * pi));
                const float grain = fbm(u, v, 8, 6, 42u);
                h = std::pow(ridge, kind == "maple" ? 0.9f : 0.55f) * 0.7f + grain * 0.3f;
                const P light = kind == "maple" ? P{0.45f, 0.4f, 0.35f} : P{0.42f, 0.35f, 0.27f};
                const P dark = P{0.15f, 0.11f, 0.08f};
                c = dark + (light - dark) * h;
            }
            img.at(x, y) = {c.x, c.y, c.z, 1.0f};
            height[static_cast<std::size_t>(y * W + x)] = h;
        }
    color = png_of(img);
    normal = png_of(normal_from_height(height, W, H, 6.0f));
}

std::vector<std::uint8_t> leaf_image(const std::string& kind, P tint) {
    const int W = 256, H = 256;
    Image img(W, H);
    const bool frond = kind == "palm" || kind == "fern" || kind == "willow";
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / W, v = (static_cast<float>(y) + 0.5f) / H;
            const float dx = std::fabs(u - 0.5f);
            const float mott = fbm(u, v, 6, 6, 51u);
            float shade = 0.85f + mott * 0.25f - dx * 0.35f;
            const float midrib = std::exp(-dx * dx / (frond ? 0.0004f : 0.00012f));
            float vein = 0.0f;
            if (!frond) {
                const float t = std::fmod((v - dx * 0.9f) * 8.0f + 10.0f, 1.0f);
                vein = std::exp(-(t - 0.5f) * (t - 0.5f) / 0.002f) * (dx < 0.46f ? 1.0f : 0.0f) * 0.7f;
                const float t2 = std::fmod((v + dx * 1.7f) * 40.0f, 1.0f);
                vein += std::exp(-(t2 - 0.5f) * (t2 - 0.5f) / 0.01f) * 0.15f;
            } else {
                const float t = std::fmod(u * 18.0f, 1.0f);
                vein = std::exp(-(t - 0.5f) * (t - 0.5f) / 0.01f) * 0.2f;
            }
            const float light = std::clamp(midrib * 0.55f + vein, 0.0f, 1.0f);
            P c = tint * shade;
            c = c + (P{0.85f, 0.95f, 0.6f} - c) * (light * 0.45f);
            if (dx > 0.47f) c = c * 0.8f;
            img.at(x, y) = {c.x, c.y, c.z, 1.0f};
        }
    return png_of(img);
}

std::vector<std::uint8_t> needle_image(P tint) {
    const int W = 256, H = 256;
    Image img(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / W, v = (static_cast<float>(y) + 0.5f) / H;
            const float dx = u - 0.5f;
            const float slant = std::fmod(v * 22.0f + std::fabs(dx) * 34.0f, 1.0f);
            const float needle = std::exp(-(slant - 0.5f) * (slant - 0.5f) / 0.02f);
            const float twig = std::exp(-dx * dx / 0.0003f);
            const float mott = fbm(u, v, 8, 8, 61u);
            P c = tint * (0.55f + needle * 0.6f + mott * 0.2f);
            c = c + (P{0.35f, 0.25f, 0.15f} - c) * (twig * 0.7f);
            const float edge = std::clamp((0.5f - std::fabs(dx)) * 12.0f, 0.0f, 1.0f) * std::clamp(std::min(v, 1.0f - v) * 14.0f, 0.0f, 1.0f);
            const float bristle = std::exp(-(slant - 0.5f) * (slant - 0.5f) / 0.05f);
            const float cover = std::clamp((bristle * 2.4f + twig * 2.0f + mott * 0.4f - 0.05f) * edge, 0.0f, 1.0f);
            img.at(x, y) = {c.x, c.y, c.z, cover};
        }
    return png_of(img);
}

struct Species {
    std::string bark = "oak";
    std::string leaf = "oak";
    P leaf_color{0.24f, 0.4f, 0.12f};
    float trunk_h = 4.0f;
    float trunk_r = 0.3f;
    int depth = 3;
    std::array<int, 4> children{6, 4, 3, 2};
    std::array<float, 4> angle{55.0f, 45.0f, 40.0f, 35.0f};
    std::array<float, 4> ratio{0.62f, 0.55f, 0.5f, 0.45f};
    std::array<float, 4> gravity{0.4f, 0.2f, 0.0f, -0.1f};
    float crown_start = 0.45f;
    float radius_ratio = 0.45f;
    int leaves = 10;
    float leaf_size = 0.09f;
    float leaf_droop = 0.2f;
    float jitter = 0.25f;
    bool conifer = false;
    bool conical = false;
    bool autumn = false;
};

struct Builder {
    Mesh bark;
    Mesh leaves;
    Mesh needles;
    Rng rng;
    Species sp;
    bool realistic = true;
    float leaf_scale = 1.0f;
    float leaf_count = 1.0f;

    Builder(const Species& s, bool real, std::uint32_t seed) : rng(seed), sp(s), realistic(real) {
        bark.material = 0;
        leaves.material = 1;
        needles.material = 2;
        leaf_scale = real ? 1.4f : 2.3f;
        leaf_count = real ? 4.0f : 1.1f;
    }

    void tube(const std::vector<P>& pts, const std::vector<float>& radii, int sides) {
        if (pts.size() < 2) return;
        P axis0 = unit(pts[1] - pts[0]);
        P side = perp(axis0);
        float along = 0.0f;
        const float circumference = 2.0f * pi * std::max(0.01f, radii.front());
        const float urep = std::max(1.0f, std::round(circumference / 0.35f));
        std::vector<std::uint32_t> prev;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const P axis = unit(i + 1 < pts.size() ? pts[i + 1] - pts[i] : pts[i] - pts[i - 1]);
            side = unit(side - axis * dot(side, axis));
            const P other = cross(axis, side);
            if (i > 0) along += len(pts[i] - pts[i - 1]);
            std::vector<std::uint32_t> ring;
            for (int k = 0; k <= sides; ++k) {
                const float a = 2.0f * pi * static_cast<float>(k) / static_cast<float>(sides);
                const P dir = side * std::cos(a) + other * std::sin(a);
                const float shade = 0.9f + 0.1f * std::sin(a * 3.0f + along);
                ring.push_back(bark.vertex(pts[i] + dir * radii[i], dir, static_cast<float>(k) / sides * urep, along / 0.35f, {shade, shade, shade, 1.0f}));
            }
            if (!prev.empty())
                for (int k = 0; k < sides; ++k) {
                    const P out = bark.nrm[prev[static_cast<std::size_t>(k)]];
                    bark.tri(prev[static_cast<std::size_t>(k)], prev[static_cast<std::size_t>(k + 1)], ring[static_cast<std::size_t>(k + 1)], out);
                    bark.tri(prev[static_cast<std::size_t>(k)], ring[static_cast<std::size_t>(k + 1)], ring[static_cast<std::size_t>(k)], out);
                }
            prev = ring;
        }
    }

    C4 leaf_tint() {
        const float b = rng.range(0.8f, 1.15f);
        if (sp.autumn) {
            const float t = rng.next();
            const P c = t < 0.4f ? P{1.0f, 0.35f, 0.15f} : t < 0.75f ? P{1.0f, 0.7f, 0.2f} : P{0.85f, 0.2f, 0.12f};
            return {c.x * b, c.y * b, c.z * b, 1.0f};
        }
        const float hue = rng.range(-0.06f, 0.06f);
        return {b * (1.0f + hue), b, b * (1.0f - hue), 1.0f};
    }

    void broad_leaf(P at, P axis, P normal, float size) {
        const P side = unit(cross(normal, axis));
        const P nrm = unit(cross(axis, side));
        const C4 tint = leaf_tint();
        const std::string& shape = sp.leaf;
        if (shape == "maple") {
            const int n = realistic ? 12 : 6;
            const std::uint32_t center = leaves.vertex(at, nrm, 0.5f, 0.0f, tint);
            std::uint32_t last = 0;
            for (int i = 0; i <= n; ++i) {
                const float t = -0.9f * pi + 1.8f * pi * static_cast<float>(i) / n;
                const float r = size * (0.55f + 0.45f * std::fabs(std::cos(2.5f * t))) * (1.0f - 0.25f * std::fabs(t) / pi);
                const float px = std::sin(t) * r, py = std::cos(t) * r + size * 0.35f;
                const P p = at + axis * py + side * px + nrm * (std::fabs(px) * 0.2f);
                const std::uint32_t vtx = leaves.vertex(p, nrm, 0.5f + px / (size * 2.2f), py / (size * 1.4f), tint);
                if (i > 0) leaves.tri(center, last, vtx, nrm);
                last = vtx;
            }
            return;
        }
        const int rows = realistic ? 3 : 2;
        const float length = shape == "willow" ? size * 2.4f : size * 1.3f;
        std::array<std::uint32_t, 3> prev{};
        for (int i = 0; i <= rows; ++i) {
            const float y = static_cast<float>(i) / rows;
            float w = 0.0f;
            if (shape == "oak") w = 0.34f * std::pow(std::sin(pi * y), 0.8f) * (1.0f + 0.3f * std::sin(y * pi * 6.0f));
            else if (shape == "birch") w = 0.4f * std::sin(pi * std::pow(y, 0.75f)) * (1.0f - y * 0.2f);
            else if (shape == "willow") w = 0.12f * std::sin(pi * y);
            else w = 0.4f * std::sin(pi * y);
            if (i == rows) w = 0.0f;
            const float droop = y * y * sp.leaf_droop;
            const P mid = at + axis * (y * length) - nrm * (droop * length);
            const float fold = w * length * 0.12f;
            const P l = mid - side * (w * length) + nrm * fold;
            const P r = mid + side * (w * length) + nrm * fold;
            const std::array<std::uint32_t, 3> row{leaves.vertex(l, nrm, 0.5f - w * 1.15f, y, tint), leaves.vertex(mid, nrm, 0.5f, y, tint), leaves.vertex(r, nrm, 0.5f + w * 1.15f, y, tint)};
            if (i > 0) {
                leaves.tri(prev[0], prev[1], row[1], nrm);
                leaves.tri(prev[0], row[1], row[0], nrm);
                leaves.tri(prev[1], prev[2], row[2], nrm);
                leaves.tri(prev[1], row[2], row[1], nrm);
            }
            prev = row;
        }
    }

    void spray(P at, P axis, P normal, float length, float width, C4 c) {
        const P side = unit(cross(normal, axis));
        const P nrm = unit(cross(axis, side));
        const int rows = realistic ? 3 : 2;
        std::array<std::uint32_t, 3> prev{};
        for (int i = 0; i <= rows; ++i) {
            const float y = static_cast<float>(i) / rows;
            const float w = width * std::pow(std::max(0.0f, std::sin(pi * (0.15f + 0.85f * y))), 0.6f) * (i == rows ? 0.2f : 1.0f);
            const P mid = at + axis * (y * length) - nrm * (y * y * length * 0.15f);
            const P l = mid - side * w - nrm * (w * 0.25f), r = mid + side * w - nrm * (w * 0.25f);
            const std::array<std::uint32_t, 3> row{needles.vertex(l, nrm, 0.0f, y, c), needles.vertex(mid, nrm, 0.5f, y, c), needles.vertex(r, nrm, 1.0f, y, c)};
            if (i > 0) {
                needles.tri(prev[0], prev[1], row[1], nrm);
                needles.tri(prev[0], row[1], row[0], nrm);
                needles.tri(prev[1], prev[2], row[2], nrm);
                needles.tri(prev[1], row[2], row[1], nrm);
            }
            prev = row;
        }
    }

    void needle_sprig(P at, P dir, float size) {
        const C4 t = leaf_tint();
        if (sp.leaf == "pine") {
            const P base = perp(dir);
            const float spin = rng.range(0.0f, pi);
            for (int k = 0; k < 3; ++k) {
                const P n = rotate(base, dir, spin + static_cast<float>(k) * pi / 3.0f);
                spray(at - dir * (size * 0.2f), dir, n, size * 1.6f, size * 0.55f, t);
            }
            return;
        }
        P n = unit(P{rng.range(-0.25f, 0.25f), 1.0f, rng.range(-0.25f, 0.25f)});
        if (std::fabs(dot(n, dir)) > 0.9f) n = perp(dir);
        spray(at, dir, n, size * 2.0f, size * 0.75f, t);
    }

    void grow(int level, P start, P dir, float length, float radius) {
        const int segs = level == 0 ? 7 : level == 1 ? 4 : 3;
        std::vector<P> pts{start};
        std::vector<float> radii{radius};
        P d = dir;
        const float step = length / segs;
        const float tip_ratio = level >= sp.depth ? 0.25f : 0.6f;
        for (int i = 1; i <= segs; ++i) {
            const P jitter{rng.range(-1.0f, 1.0f), rng.range(-0.3f, 0.3f), rng.range(-1.0f, 1.0f)};
            d = unit(d + P{0.0f, 1.0f, 0.0f} * (sp.gravity[static_cast<std::size_t>(std::min(level, 3))] * step / std::max(0.2f, length)) + jitter * (sp.jitter * step / std::max(0.2f, length)));
            pts.push_back(pts.back() + d * step);
            radii.push_back(radius * (1.0f - (1.0f - tip_ratio) * static_cast<float>(i) / segs));
        }
        const int sides = level == 0 ? (realistic ? 12 : 7) : level == 1 ? (realistic ? 7 : 5) : (realistic ? 5 : 3);
        if (radius > 0.004f) tube(pts, radii, sides);
        auto point_at = [&](float t, P& p, P& tangent, float& r) {
            const float f = std::clamp(t, 0.0f, 1.0f) * segs;
            const int i = std::min(segs - 1, static_cast<int>(f));
            const float k = f - static_cast<float>(i);
            p = pts[static_cast<std::size_t>(i)] + (pts[static_cast<std::size_t>(i + 1)] - pts[static_cast<std::size_t>(i)]) * k;
            tangent = unit(pts[static_cast<std::size_t>(i + 1)] - pts[static_cast<std::size_t>(i)]);
            r = radii[static_cast<std::size_t>(i)] + (radii[static_cast<std::size_t>(i + 1)] - radii[static_cast<std::size_t>(i)]) * k;
        };
        if (level < sp.depth) {
            const int n = sp.children[static_cast<std::size_t>(std::min(level, 3))];
            const float start_t = level == 0 ? sp.crown_start : 0.3f;
            const float tilt = sp.angle[static_cast<std::size_t>(std::min(level, 3))] * pi / 180.0f;
            const float az0 = rng.range(0.0f, 2.0f * pi);
            for (int k = 0; k < n; ++k) {
                const float t = start_t + (1.0f - start_t) * (static_cast<float>(k) + rng.range(0.2f, 0.8f)) / n;
                P p, tangent;
                float r = 0.0f;
                point_at(t, p, tangent, r);
                const P side = rotate(perp(tangent), tangent, az0 + static_cast<float>(k) * 2.39996f);
                const P child = unit(tangent * std::cos(tilt + rng.range(-0.15f, 0.15f)) + side * std::sin(tilt + rng.range(-0.15f, 0.15f)));
                float child_len = length * sp.ratio[static_cast<std::size_t>(std::min(level, 3))] * rng.range(0.8f, 1.15f);
                if (level == 0 && sp.conical) child_len *= std::max(0.15f, 1.0f - (t - start_t) / (1.0f - start_t + 1e-3f));
                grow(level + 1, p, child, child_len, std::max(0.003f, r * sp.radius_ratio));
            }
        }
        if (level >= sp.depth - 1 && level > 0) {
            const int count = std::max(1, static_cast<int>(std::round(sp.leaves * leaf_count * (level >= sp.depth ? 1.0f : 0.5f))));
            for (int i = 0; i < count; ++i) {
                const float t = rng.range(level >= sp.depth ? 0.25f : 0.55f, 1.0f);
                P p, tangent;
                float r = 0.0f;
                point_at(t, p, tangent, r);
                if (sp.conifer) {
                    const P out = unit(tangent + perp(tangent) * rng.range(-0.3f, 0.3f));
                    needle_sprig(p, out, sp.leaf_size * leaf_scale * rng.range(0.8f, 1.2f));
                    continue;
                }
                const P side = rotate(perp(tangent), tangent, rng.range(0.0f, 2.0f * pi));
                const P axis = unit(side * 0.8f + tangent * 0.5f + P{0.0f, rng.range(-0.3f, 0.3f), 0.0f});
                P normal = unit(P{0.0f, 1.0f, 0.0f} + side * rng.range(0.1f, 0.6f) + P{rng.range(-0.3f, 0.3f), 0.0f, rng.range(-0.3f, 0.3f)});
                if (std::fabs(dot(normal, axis)) > 0.95f) normal = perp(axis);
                broad_leaf(p + side * r, axis, normal, sp.leaf_size * leaf_scale * rng.range(0.75f, 1.25f));
            }
        }
    }
};

void frond(Mesh& m, Rng& rng, P base, P out, float length, float droop, int leaflets, float leaflet_len, C4 tint, bool realistic) {
    const int segs = realistic ? 10 : 5;
    std::vector<P> pts;
    P d = out;
    P p = base;
    for (int i = 0; i <= segs; ++i) {
        pts.push_back(p);
        d = unit(d + P{0.0f, -droop / segs, 0.0f});
        p = p + d * (length / segs);
    }
    const P side0 = unit(cross(out, P{0.0f, 1.0f, 0.0f}));
    const P side = len(side0) > 0.1f ? side0 : P{1.0f, 0.0f, 0.0f};
    for (int i = 1; i <= leaflets; ++i) {
        const float t = static_cast<float>(i) / (leaflets + 1);
        const float f = t * segs;
        const int k = std::min(segs - 1, static_cast<int>(f));
        const P at = pts[static_cast<std::size_t>(k)] + (pts[static_cast<std::size_t>(k + 1)] - pts[static_cast<std::size_t>(k)]) * (f - static_cast<float>(k));
        const P dir = unit(pts[static_cast<std::size_t>(k + 1)] - pts[static_cast<std::size_t>(k)]);
        const float size = leaflet_len * std::sin(pi * std::pow(t, 0.7f)) * rng.range(0.85f, 1.1f);
        for (int s = -1; s <= 1; s += 2) {
            const P lat = unit(side * static_cast<float>(s) + dir * 0.45f + P{0.0f, -0.25f, 0.0f});
            const P nrm = unit(cross(lat, dir) * static_cast<float>(s));
            const P w = dir * (size * 0.12f);
            const P tip = at + lat * size;
            const std::uint32_t a = m.vertex(at - w, nrm, 0.3f, 0.0f, tint);
            const std::uint32_t b = m.vertex(at + w, nrm, 0.7f, 0.0f, tint);
            const std::uint32_t c = m.vertex(tip + w * 0.6f + nrm * (size * 0.05f), nrm, 0.7f, 0.9f, tint);
            const std::uint32_t e = m.vertex(tip - w * 0.4f, nrm, 0.3f, 0.9f, tint);
            const std::uint32_t f2 = m.vertex(tip + lat * (size * 0.12f), nrm, 0.5f, 1.0f, tint);
            m.tri(a, b, c, nrm);
            m.tri(a, c, e, nrm);
            m.tri(e, c, f2, nrm);
        }
    }
}

Species species(const std::string& kind) {
    Species s;
    if (kind == "oak") {
        s.trunk_h = 3.2f;
        s.trunk_r = 0.38f;
        s.children = {7, 5, 3, 2};
        s.angle = {62.0f, 48.0f, 42.0f, 35.0f};
        s.ratio = {0.75f, 0.55f, 0.5f, 0.45f};
        s.gravity = {0.25f, 0.1f, 0.0f, -0.1f};
        s.crown_start = 0.5f;
        s.leaves = 14;
        s.leaf_size = 0.085f;
        s.leaf_color = {0.22f, 0.38f, 0.11f};
    } else if (kind == "maple" || kind == "maple_autumn") {
        s.bark = "maple";
        s.leaf = "maple";
        s.trunk_h = 3.6f;
        s.trunk_r = 0.3f;
        s.children = {6, 4, 3, 2};
        s.angle = {42.0f, 40.0f, 38.0f, 35.0f};
        s.ratio = {0.7f, 0.55f, 0.5f, 0.45f};
        s.gravity = {0.5f, 0.25f, 0.1f, 0.0f};
        s.crown_start = 0.45f;
        s.leaves = 11;
        s.leaf_size = 0.1f;
        s.leaf_color = {0.28f, 0.48f, 0.14f};
        s.autumn = kind == "maple_autumn";
    } else if (kind == "birch") {
        s.bark = "birch";
        s.leaf = "birch";
        s.trunk_h = 7.0f;
        s.trunk_r = 0.15f;
        s.children = {9, 4, 3, 2};
        s.angle = {48.0f, 50.0f, 45.0f, 40.0f};
        s.ratio = {0.38f, 0.6f, 0.55f, 0.5f};
        s.gravity = {0.15f, -0.25f, -0.4f, -0.4f};
        s.crown_start = 0.35f;
        s.leaves = 12;
        s.leaf_size = 0.05f;
        s.leaf_color = {0.36f, 0.55f, 0.18f};
        s.leaf_droop = 0.35f;
    } else if (kind == "willow") {
        s.bark = "oak";
        s.leaf = "willow";
        s.trunk_h = 2.6f;
        s.trunk_r = 0.32f;
        s.children = {7, 5, 3, 1};
        s.angle = {55.0f, 40.0f, 30.0f, 20.0f};
        s.ratio = {0.8f, 0.75f, 0.7f, 0.6f};
        s.gravity = {0.5f, -0.6f, -1.6f, -2.0f};
        s.crown_start = 0.7f;
        s.leaves = 18;
        s.leaf_size = 0.05f;
        s.leaf_color = {0.4f, 0.55f, 0.2f};
        s.leaf_droop = 0.4f;
    } else if (kind == "pine") {
        s.bark = "pine";
        s.leaf = "pine";
        s.conifer = true;
        s.trunk_h = 11.0f;
        s.trunk_r = 0.28f;
        s.depth = 2;
        s.children = {16, 5, 3, 2};
        s.angle = {78.0f, 45.0f, 40.0f, 35.0f};
        s.ratio = {0.32f, 0.45f, 0.45f, 0.45f};
        s.gravity = {0.05f, 0.25f, 0.2f, 0.0f};
        s.crown_start = 0.55f;
        s.conical = true;
        s.leaves = 2;
        s.leaf_size = 0.18f;
        s.leaf_color = {0.17f, 0.33f, 0.14f};
        s.jitter = 0.15f;
    } else if (kind == "spruce" || kind == "fir") {
        s.bark = "spruce";
        s.leaf = "spruce";
        s.conifer = true;
        s.trunk_h = 12.0f;
        s.trunk_r = 0.26f;
        s.depth = 2;
        s.children = {34, 6, 2, 2};
        s.angle = {88.0f, 50.0f, 40.0f, 35.0f};
        s.ratio = {0.42f, 0.4f, 0.4f, 0.4f};
        s.gravity = {-0.15f, 0.05f, 0.0f, 0.0f};
        s.crown_start = 0.08f;
        s.conical = true;
        s.leaves = 3;
        s.leaf_size = 0.12f;
        s.leaf_color = kind == "fir" ? P{0.12f, 0.28f, 0.16f} : P{0.13f, 0.27f, 0.13f};
        s.jitter = 0.1f;
    } else if (kind == "bush") {
        s.trunk_h = 0.25f;
        s.trunk_r = 0.05f;
        s.depth = 2;
        s.children = {8, 4, 2, 2};
        s.angle = {55.0f, 45.0f, 40.0f, 35.0f};
        s.ratio = {3.6f, 0.5f, 0.45f, 0.4f};
        s.gravity = {0.5f, 0.2f, 0.0f, 0.0f};
        s.crown_start = 0.2f;
        s.leaves = 14;
        s.leaf_size = 0.05f;
        s.leaf = "birch";
        s.leaf_color = {0.25f, 0.42f, 0.15f};
    }
    return s;
}

struct Material {
    std::string name;
    int base = -1;
    int normal = -1;
    float roughness = 0.85f;
    bool double_sided = false;
    float translucency = 0.0f;
    float cutoff = 0.0f;
};

struct Sway {
    float sway = 0.0f;
    float height = 1.0f;
    float flutter = 0.0f;
};

std::string glb(const std::vector<Mesh>& meshes, const std::vector<Material>& mats, const std::vector<std::vector<std::uint8_t>>& images, const std::string& name, const Sway& wind = {}) {
    std::string bin;
    struct View {
        std::size_t off, len;
        int target;
    };
    std::vector<View> views;
    auto add = [&](const void* data, std::size_t bytes, int target) {
        while (bin.size() % 4) bin.push_back('\0');
        const std::size_t off = bin.size();
        bin.append(static_cast<const char*>(data), bytes);
        views.push_back({off, bytes, target});
        return static_cast<int>(views.size() - 1);
    };
    std::ostringstream acc, prims;
    int accessor = 0;
    bool first_prim = true;
    for (const Mesh& m : meshes) {
        if (m.idx.empty()) continue;
        std::vector<float> pos, nrm, uv, col;
        P lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
        for (std::size_t i = 0; i < m.pos.size(); ++i) {
            const P p = m.pos[i];
            pos.insert(pos.end(), {p.x, p.y, p.z});
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
            nrm.insert(nrm.end(), {m.nrm[i].x, m.nrm[i].y, m.nrm[i].z});
            uv.insert(uv.end(), {m.uv[i][0], m.uv[i][1]});
            col.insert(col.end(), {m.col[i][0], m.col[i][1], m.col[i][2], m.col[i][3]});
        }
        const int vp = add(pos.data(), pos.size() * 4, 34962), vn = add(nrm.data(), nrm.size() * 4, 34962), vu = add(uv.data(), uv.size() * 4, 34962), vc = add(col.data(), col.size() * 4, 34962), vi = add(m.idx.data(), m.idx.size() * 4, 34963);
        const std::size_t n = m.pos.size();
        acc << (accessor ? "," : "") << "{\"bufferView\":" << vp << ",\"componentType\":5126,\"count\":" << n << ",\"type\":\"VEC3\",\"min\":[" << lo.x << "," << lo.y << "," << lo.z << "],\"max\":[" << hi.x << "," << hi.y << "," << hi.z << "]}";
        acc << ",{\"bufferView\":" << vn << ",\"componentType\":5126,\"count\":" << n << ",\"type\":\"VEC3\"}";
        acc << ",{\"bufferView\":" << vu << ",\"componentType\":5126,\"count\":" << n << ",\"type\":\"VEC2\"}";
        acc << ",{\"bufferView\":" << vc << ",\"componentType\":5126,\"count\":" << n << ",\"type\":\"VEC4\"}";
        acc << ",{\"bufferView\":" << vi << ",\"componentType\":5125,\"count\":" << m.idx.size() << ",\"type\":\"SCALAR\"}";
        prims << (first_prim ? "" : ",") << "{\"attributes\":{\"POSITION\":" << accessor << ",\"NORMAL\":" << accessor + 1 << ",\"TEXCOORD_0\":" << accessor + 2 << ",\"COLOR_0\":" << accessor + 3 << "},\"indices\":" << accessor + 4 << ",\"material\":" << m.material << "}";
        first_prim = false;
        accessor += 5;
    }
    std::vector<int> image_views;
    for (const auto& img : images) image_views.push_back(add(img.data(), img.size(), 0));
    while (bin.size() % 4) bin.push_back('\0');
    std::ostringstream js;
    js << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Orchard plants 2\"},\"extensionsUsed\":[\"KHR_materials_diffuse_transmission\"],\"scene\":0,\"scenes\":[{\"nodes\":[0]}],";
    js << "\"nodes\":[{\"name\":\"" << name << "\",\"mesh\":0}],\"meshes\":[{\"name\":\"" << name << "\",\"primitives\":[" << prims.str() << "]}],";
    js << "\"materials\":[";
    for (std::size_t i = 0; i < mats.size(); ++i) {
        const Material& m = mats[i];
        js << (i ? "," : "") << "{\"name\":\"" << m.name << "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":" << m.roughness;
        if (m.base >= 0) js << ",\"baseColorTexture\":{\"index\":" << m.base << "}";
        js << "}";
        if (m.normal >= 0) js << ",\"normalTexture\":{\"index\":" << m.normal << ",\"scale\":1.2}";
        if (m.double_sided) js << ",\"doubleSided\":true";
        if (m.cutoff > 0.0f) js << ",\"alphaMode\":\"MASK\",\"alphaCutoff\":" << m.cutoff;
        if (wind.sway > 0.0f) js << ",\"extras\":{\"wind\":" << wind.sway << ",\"windHeight\":" << wind.height << ",\"windFlutter\":" << (m.double_sided ? wind.flutter : 0.0f) << "}";
        if (m.translucency > 0.0f) js << ",\"extensions\":{\"KHR_materials_diffuse_transmission\":{\"diffuseTransmissionFactor\":" << m.translucency << "}}";
        js << "}";
    }
    js << "]";
    if (!images.empty()) {
        js << ",\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],\"images\":[";
        for (std::size_t i = 0; i < images.size(); ++i) js << (i ? "," : "") << "{\"bufferView\":" << image_views[i] << ",\"mimeType\":\"image/png\"}";
        js << "],\"textures\":[";
        for (std::size_t i = 0; i < images.size(); ++i) js << (i ? "," : "") << "{\"sampler\":0,\"source\":" << i << "}";
        js << "]";
    }
    js << ",\"buffers\":[{\"byteLength\":" << bin.size() << "}],\"bufferViews\":[";
    for (std::size_t i = 0; i < views.size(); ++i) {
        js << (i ? "," : "") << "{\"buffer\":0,\"byteOffset\":" << views[i].off << ",\"byteLength\":" << views[i].len;
        if (views[i].target) js << ",\"target\":" << views[i].target;
        js << "}";
    }
    js << "],\"accessors\":[" << acc.str() << "]}";
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

Sway sway_for(const std::string& kind, float height) {
    Sway w;
    w.height = std::max(0.2f, height);
    if (kind == "grass") { w.sway = 0.18f; w.flutter = 0.03f; }
    else if (kind == "fern") { w.sway = 0.1f; w.flutter = 0.04f; }
    else if (kind == "bush") { w.sway = 0.08f; w.flutter = 0.03f; }
    else if (kind == "palm") { w.sway = 0.04f * height; w.flutter = 0.06f; }
    else if (kind == "pine" || kind == "spruce" || kind == "fir") { w.sway = 0.02f * height; w.flutter = 0.012f; }
    else { w.sway = 0.025f * height; w.flutter = 0.025f; }
    return w;
}

std::string build_plant(const std::string& kind, bool realistic, float height) {
    std::vector<std::vector<std::uint8_t>> images;
    std::vector<Material> mats;
    std::vector<Mesh> meshes;
    Rng rng(static_cast<std::uint32_t>(std::hash<std::string>{}(kind)) | 1u);
    if (kind == "grass" || kind == "fern" || kind == "palm") {
        Mesh stem, blades;
        stem.material = 0;
        blades.material = 1;
        const P green = kind == "grass" ? P{0.32f, 0.5f, 0.18f} : kind == "fern" ? P{0.2f, 0.42f, 0.14f} : P{0.25f, 0.45f, 0.16f};
        if (kind == "palm") {
            std::vector<std::uint8_t> bc, bn;
            bark_images("palm", bc, bn);
            images.push_back(bc);
            images.push_back(bn);
            images.push_back(leaf_image("palm", green));
            mats.push_back({"Bark", 0, 1, 0.9f, false, 0.0f});
            mats.push_back({"Fronds", 2, -1, 0.6f, true, 0.5f});
            Species sp;
            Builder b(sp, realistic, 7u);
            std::vector<P> pts;
            std::vector<float> radii;
            const float h = 7.0f;
            for (int i = 0; i <= 14; ++i) {
                const float t = static_cast<float>(i) / 14.0f;
                pts.push_back({std::sin(t * 1.4f) * 0.9f, t * h, 0.0f});
                radii.push_back(0.22f - t * 0.08f + (i == 0 ? 0.06f : 0.0f));
            }
            b.tube(pts, radii, realistic ? 12 : 7);
            stem = b.bark;
            stem.material = 0;
            const int n = realistic ? 16 : 9;
            for (int i = 0; i < n; ++i) {
                const float a = static_cast<float>(i) * 2.39996f;
                const P out = unit(P{std::cos(a), rng.range(0.1f, 0.8f), std::sin(a)});
                const float b2 = rng.range(0.85f, 1.15f);
                frond(blades, rng, pts.back(), out, rng.range(2.6f, 3.4f), 1.3f, realistic ? 34 : 14, 0.55f, {b2, b2, b2, 1.0f}, realistic);
            }
        } else if (kind == "fern") {
            images.push_back(leaf_image("fern", green));
            mats.push_back({"Stem", -1, -1, 0.8f, false, 0.0f});
            mats.push_back({"Fronds", 0, -1, 0.65f, true, 0.45f});
            const int n = realistic ? 14 : 8;
            for (int i = 0; i < n; ++i) {
                const float a = static_cast<float>(i) * 2.39996f;
                const P out = unit(P{std::cos(a) * 0.6f, 1.0f, std::sin(a) * 0.6f});
                const float b2 = rng.range(0.85f, 1.15f);
                frond(blades, rng, P{0.0f, 0.02f, 0.0f}, out, rng.range(0.7f, 1.0f), 1.4f, realistic ? 24 : 10, 0.13f, {b2, b2, b2, 1.0f}, realistic);
            }
        } else {
            mats.push_back({"Stem", -1, -1, 0.8f, false, 0.0f});
            mats.push_back({"Blades", -1, -1, 0.7f, true, 0.5f});
            const int n = realistic ? 70 : 24;
            for (int i = 0; i < n; ++i) {
                const float a = rng.range(0.0f, 2.0f * pi), r = rng.range(0.0f, 0.12f);
                const P base{std::cos(a) * r, 0.0f, std::sin(a) * r};
                const P lean = unit(P{std::cos(a) * rng.range(0.1f, 0.5f), 1.0f, std::sin(a) * rng.range(0.1f, 0.5f)});
                const float h = rng.range(0.35f, 0.75f);
                const int segs = realistic ? 4 : 2;
                const P side = unit(cross(lean, P{0.0f, 1.0f, 0.0f}) + P{0.001f, 0.0f, 0.0f});
                const P nrm = unit(cross(side, lean));
                std::uint32_t pl = 0, pr = 0;
                const float shade = rng.range(0.8f, 1.15f);
                for (int s = 0; s <= segs; ++s) {
                    const float t = static_cast<float>(s) / segs;
                    const P at = base + lean * (t * h) + P{std::cos(a), 0.0f, std::sin(a)} * (t * t * h * 0.25f);
                    const float w = 0.008f * (1.0f - t * 0.85f);
                    const float g = 0.55f + t * 0.5f;
                    const C4 c{green.x * g * shade, green.y * g * shade, green.z * g * shade, 1.0f};
                    const std::uint32_t l = blades.vertex(at - side * w, nrm, 0.0f, t, c), rr = blades.vertex(at + side * w, nrm, 1.0f, t, c);
                    if (s > 0) {
                        blades.tri(pl, pr, rr, nrm);
                        blades.tri(pl, rr, l, nrm);
                    }
                    pl = l;
                    pr = rr;
                }
            }
        }
        meshes.push_back(stem);
        meshes.push_back(blades);
        return glb(meshes, mats, images, kind, sway_for(kind, height));
    }
    const Species sp = species(kind);
    Builder b(sp, realistic, static_cast<std::uint32_t>(std::hash<std::string>{}(kind)) | 1u);
    std::vector<std::uint8_t> bc, bn;
    bark_images(sp.bark, bc, bn);
    images.push_back(bc);
    images.push_back(bn);
    mats.push_back({"Bark", 0, 1, 0.92f, false, 0.0f});
    if (!sp.conifer) {
        images.push_back(leaf_image(sp.leaf, sp.autumn ? P{1.0f, 0.95f, 0.85f} : sp.leaf_color));
        mats.push_back({"Leaves", 2, -1, 0.6f, true, 0.55f});
    } else {
        mats.push_back({"Leaves", -1, -1, 0.7f, true, 0.3f});
    }
    mats.push_back({"Needles", -1, -1, 0.75f, true, 0.35f, 0.4f});
    if (kind == "bush") {
        for (int i = 0; i < 7; ++i) {
            const float a = static_cast<float>(i) * 2.39996f;
            b.grow(1, P{std::cos(a) * 0.06f, 0.0f, std::sin(a) * 0.06f}, unit(P{std::cos(a) * 0.6f, 1.0f, std::sin(a) * 0.6f}), b.rng.range(0.7f, 1.1f), 0.022f);
        }
    } else {
        b.grow(0, P{0.0f, -0.15f, 0.0f}, P{0.0f, 1.0f, 0.0f}, sp.trunk_h + 0.15f, sp.trunk_r);
    }
    if (sp.conifer) {
        images.push_back(needle_image(sp.leaf_color));
        mats.back().base = static_cast<int>(images.size() - 1);
        b.needles.material = 2;
        meshes.push_back(b.bark);
        meshes.push_back(b.needles);
    } else {
        meshes.push_back(b.bark);
        meshes.push_back(b.leaves);
    }
    return glb(meshes, mats, images, kind, sway_for(kind, height));
}

void simple_tube(Mesh& m, P a, P b, float r0, float r1, int sides, C4 col) {
    const P axis = unit(b - a);
    const P u = perp(axis), w = cross(axis, u);
    const std::uint32_t base = static_cast<std::uint32_t>(m.pos.size());
    for (int i = 0; i <= sides; ++i) {
        const float t = 2.0f * pi * static_cast<float>(i) / static_cast<float>(sides);
        const P n = u * std::cos(t) + w * std::sin(t);
        m.vertex(a + n * r0, n, static_cast<float>(i) / static_cast<float>(sides), 0.0f, col);
        m.vertex(b + n * r1, n, static_cast<float>(i) / static_cast<float>(sides), 1.0f, col);
    }
    for (int i = 0; i < sides; ++i) {
        const std::uint32_t k = base + static_cast<std::uint32_t>(i) * 2u;
        const P out = m.nrm[k];
        m.tri(k, k + 2u, k + 1u, out);
        m.tri(k + 1u, k + 2u, k + 3u, out);
    }
}

void simple_cone(Mesh& m, float y0, float y1, float r, int sides, C4 col, Rng& rng, float jitter) {
    const std::uint32_t tip = m.vertex(P{0.0f, y1, 0.0f}, P{0.0f, 1.0f, 0.0f}, 0.5f, 1.0f, col);
    const std::uint32_t mid = m.vertex(P{0.0f, y0, 0.0f}, P{0.0f, -1.0f, 0.0f}, 0.5f, 0.0f, col);
    std::vector<std::uint32_t> ring;
    for (int i = 0; i < sides; ++i) {
        const float t = 2.0f * pi * static_cast<float>(i) / static_cast<float>(sides);
        const float rr = r * (1.0f + rng.range(-jitter, jitter));
        const P at{std::cos(t) * rr, y0 + rng.range(-jitter, jitter) * (y1 - y0) * 0.15f, std::sin(t) * rr};
        const C4 c{col[0] * rng.range(0.85f, 1.1f), col[1] * rng.range(0.85f, 1.1f), col[2] * rng.range(0.85f, 1.1f), 1.0f};
        ring.push_back(m.vertex(at, P{std::cos(t), r / std::max(0.01f, y1 - y0), std::sin(t)}, 0.0f, 0.0f, c));
    }
    for (int i = 0; i < sides; ++i) {
        const std::uint32_t a = ring[static_cast<std::size_t>(i)], b = ring[static_cast<std::size_t>((i + 1) % sides)];
        const P out = unit(m.pos[a] + m.pos[b] - P{0.0f, y0 * 2.0f, 0.0f});
        m.tri(a, b, tip, out + P{0.0f, 0.5f, 0.0f});
        m.tri(a, mid, b, P{0.0f, -1.0f, 0.0f});
    }
}

void simple_blob(Mesh& m, P c, P r, C4 col, Rng& rng, float jitter, int rings, int sides) {
    const std::uint32_t base = static_cast<std::uint32_t>(m.pos.size());
    for (int i = 0; i <= rings; ++i) {
        const float v = pi * static_cast<float>(i) / static_cast<float>(rings);
        for (int j = 0; j < sides; ++j) {
            const float u = 2.0f * pi * static_cast<float>(j) / static_cast<float>(sides);
            const P n{std::sin(v) * std::cos(u), std::cos(v), std::sin(v) * std::sin(u)};
            const float k = (i == 0 || i == rings) ? 1.0f : 1.0f + rng.range(-jitter, jitter);
            const float shade = 0.8f + 0.25f * (n.y * 0.5f + 0.5f);
            m.vertex(c + P{n.x * r.x * k, n.y * r.y * k, n.z * r.z * k}, n, 0.0f, 0.0f, C4{col[0] * shade, col[1] * shade, col[2] * shade, 1.0f});
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < sides; ++j) {
            const std::uint32_t a = base + static_cast<std::uint32_t>(i * sides + j), b = base + static_cast<std::uint32_t>(i * sides + (j + 1) % sides);
            const std::uint32_t c2 = a + static_cast<std::uint32_t>(sides), d = b + static_cast<std::uint32_t>(sides);
            const P out = m.pos[a] - c;
            m.tri(a, c2, b, out);
            m.tri(b, c2, d, out);
        }
}

void simple_strip(Mesh& m, P base, P dir, float length, float width, float droop, C4 col) {
    const P side = unit(cross(dir, P{0.0f, 1.0f, 0.0f}));
    const int segs = 4;
    std::uint32_t prev_l = 0, prev_r = 0;
    for (int i = 0; i <= segs; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segs);
        const P at = base + dir * (length * t) + P{0.0f, -droop * length * t * t, 0.0f};
        const float wd = width * (i == segs ? 0.05f : 1.0f - t * 0.6f);
        const P n = unit(cross(side, dir) + P{0.0f, 0.3f, 0.0f});
        const std::uint32_t l = m.vertex(at - side * wd, n, 0.0f, t, col), r = m.vertex(at + side * wd, n, 1.0f, t, col);
        if (i) {
            m.tri(prev_l, prev_r, l, n);
            m.tri(prev_r, r, l, n);
        }
        prev_l = l;
        prev_r = r;
    }
}

std::string build_simple_plant(const std::string& kind, float height) {
    Mesh solid, flat;
    solid.material = 0;
    flat.material = 1;
    Rng rng(static_cast<std::uint32_t>(std::hash<std::string>{}(kind)) | 1u);
    const Species sp = species(kind);
    const P lc = kind == "maple_autumn" ? P{0.78f, 0.36f, 0.1f} : sp.leaf_color;
    const C4 leaf{std::min(1.0f, lc.x * 1.3f), std::min(1.0f, lc.y * 1.3f), std::min(1.0f, lc.z * 1.3f), 1.0f};
    const C4 bark = kind == "birch" ? C4{0.82f, 0.8f, 0.76f, 1.0f} : C4{0.36f, 0.25f, 0.16f, 1.0f};
    if (kind == "pine" || kind == "spruce" || kind == "fir") {
        simple_tube(solid, P{0.0f, -0.1f, 0.0f}, P{0.0f, height * 0.3f, 0.0f}, sp.trunk_r, sp.trunk_r * 0.7f, 7, bark);
        const int tiers = kind == "pine" ? 3 : 4;
        for (int i = 0; i < tiers; ++i) {
            const float f = static_cast<float>(i) / static_cast<float>(tiers);
            const float y0 = height * (0.15f + f * 0.62f), y1 = y0 + height * (kind == "pine" ? 0.42f : 0.36f);
            const float r = height * (kind == "spruce" ? 0.2f : 0.17f) * (1.0f - f * 0.65f);
            simple_cone(solid, y0, std::min(y1, height), r, 9, C4{leaf[0] * (0.9f + f * 0.15f), leaf[1] * (0.9f + f * 0.15f), leaf[2] * (0.9f + f * 0.15f), 1.0f}, rng, 0.2f);
        }
    } else if (kind == "palm") {
        P at{0.0f, -0.1f, 0.0f};
        for (int i = 0; i < 6; ++i) {
            const float t = static_cast<float>(i + 1) / 6.0f;
            const P next{std::sin(t * 1.4f) * height * 0.12f, t * height, 0.0f};
            simple_tube(solid, at, next, 0.22f - t * 0.06f + 0.06f, 0.22f - t * 0.08f, 7, bark);
            at = next;
        }
        for (int i = 0; i < 8; ++i) {
            const float a = 2.0f * pi * static_cast<float>(i) / 8.0f + rng.range(-0.2f, 0.2f);
            simple_strip(flat, at, unit(P{std::cos(a), 0.35f, std::sin(a)}), height * 0.4f, 0.35f, 0.9f, leaf);
        }
        simple_blob(solid, at, P{0.3f, 0.25f, 0.3f}, C4{0.3f, 0.22f, 0.12f, 1.0f}, rng, 0.2f, 4, 6);
    } else if (kind == "fern" || kind == "grass") {
        const bool fern = kind == "fern";
        const int n = fern ? 9 : 14;
        for (int i = 0; i < n; ++i) {
            const float a = static_cast<float>(i) * 2.39996f;
            const P dir = unit(P{std::cos(a) * (fern ? 0.8f : 0.25f), 1.0f, std::sin(a) * (fern ? 0.8f : 0.25f)});
            const float shade = rng.range(0.85f, 1.1f);
            simple_strip(flat, P{std::cos(a) * 0.04f, 0.0f, std::sin(a) * 0.04f}, dir, height * rng.range(0.75f, 1.05f), fern ? 0.1f : 0.025f, fern ? 0.8f : 0.3f, C4{leaf[0] * shade, leaf[1] * shade, leaf[2] * shade, 1.0f});
        }
    } else if (kind == "bush") {
        simple_blob(solid, P{0.0f, height * 0.36f, 0.0f}, P{height * 0.5f, height * 0.38f, height * 0.5f}, leaf, rng, 0.3f, 5, 9);
        for (int i = 0; i < 3; ++i) {
            const float a = static_cast<float>(i) * 2.1f;
            simple_blob(solid, P{std::cos(a) * height * 0.3f, height * 0.28f, std::sin(a) * height * 0.3f}, P{height * 0.3f, height * 0.26f, height * 0.3f}, C4{leaf[0] * 1.08f, leaf[1] * 1.05f, leaf[2], 1.0f}, rng, 0.3f, 4, 7);
        }
    } else {
        const bool willow = kind == "willow";
        const float trunk_top = height * (willow ? 0.5f : 0.45f);
        simple_tube(solid, P{0.0f, -0.1f, 0.0f}, P{0.0f, trunk_top, 0.0f}, sp.trunk_r, sp.trunk_r * 0.6f, 8, bark);
        const float cr = height * (kind == "birch" ? 0.2f : 0.3f);
        const float cy = trunk_top + cr * 0.7f;
        simple_blob(solid, P{0.0f, cy, 0.0f}, P{cr, cr * (kind == "birch" ? 1.5f : 0.85f), cr}, leaf, rng, 0.3f, 6, 10);
        for (int i = 0; i < 4; ++i) {
            const float a = static_cast<float>(i) * 1.7f + 0.4f;
            const float r = cr * rng.range(0.5f, 0.7f);
            simple_blob(solid, P{std::cos(a) * cr * 0.65f, cy + rng.range(-0.3f, 0.35f) * cr, std::sin(a) * cr * 0.65f}, P{r, r * 0.85f, r}, C4{leaf[0] * rng.range(0.9f, 1.1f), leaf[1] * rng.range(0.9f, 1.1f), leaf[2], 1.0f}, rng, 0.3f, 5, 8);
        }
        if (willow)
            for (int i = 0; i < 12; ++i) {
                const float a = 2.0f * pi * static_cast<float>(i) / 12.0f;
                simple_strip(flat, P{std::cos(a) * cr * 0.9f, cy, std::sin(a) * cr * 0.9f}, unit(P{std::cos(a) * 0.3f, -1.0f, std::sin(a) * 0.3f}), height * 0.35f, 0.25f, 0.0f, leaf);
            }
    }
    const std::vector<Material> mats = {{"Simple", -1, -1, 0.85f, false, 0.0f}, {"Simple leaves", -1, -1, 0.8f, true, 0.0f}};
    return glb({solid, flat}, mats, {}, kind, sway_for(kind, height));
}

} // namespace

const std::vector<PlantInfo>& plant_models() {
    static const std::vector<PlantInfo> all = [] {
        std::vector<PlantInfo> v;
        struct K {
            const char* id;
            const char* label;
            const char* surface;
            bool solid;
            float radius;
            float height;
        };
        const K kinds[] = {
            {"oak", "Oak", "wood", true, 0.38f, 9.0f},
            {"maple", "Maple", "wood", true, 0.3f, 9.0f},
            {"maple_autumn", "Maple (autumn)", "wood", true, 0.3f, 9.0f},
            {"birch", "Birch", "wood", true, 0.15f, 11.0f},
            {"willow", "Willow", "wood", true, 0.32f, 7.0f},
            {"pine", "Pine", "wood", true, 0.28f, 14.0f},
            {"spruce", "Spruce", "wood", true, 0.26f, 14.0f},
            {"fir", "Fir", "wood", true, 0.26f, 14.0f},
            {"palm", "Palm", "wood", true, 0.22f, 9.0f},
            {"bush", "Bush", "shrubs", false, 0.0f, 1.4f},
            {"fern", "Fern", "shrubs", false, 0.0f, 0.9f},
            {"grass", "Tall grass tuft", "tall_grass", false, 0.0f, 0.7f},
        };
        for (const K& k : kinds)
            for (const char* q : {"simple", "detailed", "realistic"}) {
                PlantInfo p;
                p.name = std::string("Plants/") + k.id + "-" + q;
                p.label = std::string(k.label) + (std::string(q) == "realistic" ? " (realistic, single leaves)" : std::string(q) == "simple" ? " (simple, low-poly)" : " (detailed)");
                p.surface = k.surface;
                p.solid = k.solid;
                p.trunk_radius = k.radius;
                p.height = k.height;
                v.push_back(p);
            }
        return v;
    }();
    return all;
}

const PlantInfo* plant_info(const std::string& name) {
    for (const PlantInfo& p : plant_models())
        if (p.name == name) return &p;
    return nullptr;
}

bool is_plant_model(const std::string& name) {
    return plant_info(name) != nullptr;
}

std::string plant_model_glb(const std::string& name) {
    if (!is_plant_model(name)) return std::string();
    std::string stem = name.substr(7);
    const std::size_t dash = stem.rfind('-');
    const std::string quality = stem.substr(dash + 1);
    if (quality == "simple") return build_simple_plant(stem.substr(0, dash), plant_info(name)->height);
    return build_plant(stem.substr(0, dash), quality == "realistic", plant_info(name)->height);
}

bool plant_model_outdated(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string head(4096, '\0');
    in.read(head.data(), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<std::size_t>(in.gcount()));
    return head.find("\"generator\":\"Orchard\"") != std::string::npos;
}

bool write_plant_model(const std::string& name, const std::string& path, std::string* error) {
    const std::string data = plant_model_glb(name);
    if (data.empty()) {
        if (error) *error = "no such plant: " + name;
        return false;
    }
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

} // namespace editor
} // namespace orchard
