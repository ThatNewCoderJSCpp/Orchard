#include "particle_textures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace orchard {
namespace editor {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Rgba {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

float mixf(float a, float b, float t) {
    return a + (b - a) * t;
}

float smooth(float e0, float e1, float x) {
    const float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

std::uint32_t hash3(int x, int y, int z, std::uint32_t seed) {
    std::uint32_t h = seed * 0x9E3779B1u;
    h ^= static_cast<std::uint32_t>(x) * 0x85EBCA77u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<std::uint32_t>(y) * 0xC2B2AE3Du;
    h = (h << 17) | (h >> 15);
    h ^= static_cast<std::uint32_t>(z) * 0x27D4EB2Fu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

float grad(std::uint32_t h, float x, float y, float z) {
    switch (h & 15u) {
    case 0: return x + y;
    case 1: return -x + y;
    case 2: return x - y;
    case 3: return -x - y;
    case 4: return x + z;
    case 5: return -x + z;
    case 6: return x - z;
    case 7: return -x - z;
    case 8: return y + z;
    case 9: return -y + z;
    case 10: return y - z;
    case 11: return -y - z;
    case 12: return x + y;
    case 13: return -x + y;
    case 14: return -y + z;
    default: return -y - z;
    }
}

float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float noise(float x, float y, float z, std::uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy), iz = static_cast<int>(fz);
    const float dx = x - fx, dy = y - fy, dz = z - fz;
    const float u = fade(dx), v = fade(dy), w = fade(dz);
    auto g = [&](int ox, int oy, int oz) { return grad(hash3(ix + ox, iy + oy, iz + oz, seed), dx - static_cast<float>(ox), dy - static_cast<float>(oy), dz - static_cast<float>(oz)); };
    const float x00 = mixf(g(0, 0, 0), g(1, 0, 0), u);
    const float x10 = mixf(g(0, 1, 0), g(1, 1, 0), u);
    const float x01 = mixf(g(0, 0, 1), g(1, 0, 1), u);
    const float x11 = mixf(g(0, 1, 1), g(1, 1, 1), u);
    return mixf(mixf(x00, x10, v), mixf(x01, x11, v), w) * 0.94f;
}

float fbm(float x, float y, float z, int octaves, std::uint32_t seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise(x, y, z, seed + static_cast<std::uint32_t>(i) * 101u);
        norm += amp;
        amp *= 0.5f;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f + 3.7f;
        z = z * 2.03f + 9.3f;
    }
    return sum / norm;
}

float billow(float x, float y, float z, int octaves, std::uint32_t seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * (1.0f - std::fabs(noise(x, y, z, seed + static_cast<std::uint32_t>(i) * 131u)) * 1.6f);
        norm += amp;
        amp *= 0.5f;
        x = x * 2.07f + 5.3f;
        y = y * 2.07f + 11.9f;
        z = z * 2.07f + 2.1f;
    }
    return sum / norm;
}

struct Rng {
    std::uint32_t s;
    explicit Rng(std::uint32_t seed) : s(seed * 2654435761u + 0x6D2B79F5u) {}
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s & 0xFFFFFFu) / 16777216.0f;
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};

struct Sheet {
    int width = 0;
    int height = 0;
    std::vector<Rgba> px;

    Sheet(int w, int h) : width(w), height(h), px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h)) {}

    template <class F>
    void cell(int column, int row, int cw, int ch, int samples, F&& f) {
        const int n = std::max(1, samples);
        const float inv = 1.0f / static_cast<float>(n * n);
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                Rgba acc;
                float wr = 0.0f, wg = 0.0f, wb = 0.0f;
                for (int sy = 0; sy < n; ++sy)
                    for (int sx = 0; sx < n; ++sx) {
                        const float u = ((static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / static_cast<float>(n)) / static_cast<float>(cw)) * 2.0f - 1.0f;
                        const float v = 1.0f - ((static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / static_cast<float>(n)) / static_cast<float>(ch)) * 2.0f;
                        const Rgba c = f(u, v);
                        const float a = clamp01(c.a);
                        acc.a += a;
                        wr += c.r * a;
                        wg += c.g * a;
                        wb += c.b * a;
                        acc.r += c.r;
                        acc.g += c.g;
                        acc.b += c.b;
                    }
                Rgba out;
                out.a = acc.a * inv;
                if (acc.a > 1e-6f) {
                    out.r = wr / acc.a;
                    out.g = wg / acc.a;
                    out.b = wb / acc.a;
                } else {
                    out.r = acc.r * inv;
                    out.g = acc.g * inv;
                    out.b = acc.b * inv;
                }
                px[static_cast<std::size_t>(row * ch + y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column * cw + x)] = out;
            }
    }

    ParticlePixels bytes() const {
        ParticlePixels out;
        out.width = width;
        out.height = height;
        out.rgba.resize(px.size() * 4);
        auto b = [](float v) { return static_cast<std::uint8_t>(std::lround(clamp01(v) * 255.0f)); };
        for (std::size_t i = 0; i < px.size(); ++i) {
            out.rgba[i * 4 + 0] = b(px[i].r);
            out.rgba[i * 4 + 1] = b(px[i].g);
            out.rgba[i * 4 + 2] = b(px[i].b);
            out.rgba[i * 4 + 3] = b(px[i].a);
        }
        return out;
    }
};

Rgba gray(float v, float a) {
    return Rgba{v, v, v, a};
}

float edge_window(float u, float v, float inner = 0.78f) {
    return smooth(1.0f, inner, std::sqrt(u * u + v * v));
}

Rgba heat_ramp(float h, float a) {
    struct Stop {
        float t, r, g, b;
    };
    static const Stop stops[] = {
        {0.00f, 0.22f, 0.02f, 0.00f}, {0.25f, 0.70f, 0.12f, 0.01f}, {0.50f, 1.00f, 0.42f, 0.05f},
        {0.72f, 1.00f, 0.72f, 0.24f}, {0.88f, 1.00f, 0.90f, 0.58f}, {1.00f, 1.00f, 0.98f, 0.90f},
    };
    h = clamp01(h);
    for (std::size_t i = 1; i < std::size(stops); ++i) {
        if (h <= stops[i].t) {
            const Stop& p = stops[i - 1];
            const Stop& q = stops[i];
            const float t = (h - p.t) / (q.t - p.t);
            return Rgba{mixf(p.r, q.r, t), mixf(p.g, q.g, t), mixf(p.b, q.b, t), a};
        }
    }
    return Rgba{1.0f, 0.98f, 0.9f, a};
}

ParticlePixels make_glow() {
    Sheet s(64, 64);
    s.cell(0, 0, 64, 64, 2, [](float u, float v) {
        const float r2 = u * u + v * v;
        const float a = (std::exp(-5.5f * r2) * 0.8f + std::exp(-28.0f * r2) * 0.35f) * edge_window(u, v, 0.7f);
        return gray(1.0f, a);
    });
    return s.bytes();
}

struct Blob {
    float x, y, r;
};

struct PuffStyle {
    float warp;
    float detail;
    float threshold;
    float softness;
    float opacity;
    float shade_lo;
    float shade_hi;
    float stretch_y;
    int blobs;
    float spread;
    float rmin, rmax;
    float cmin, cmax;
    float thickness;
    float normal;
    float scale;
};

ParticlePixels make_puffs(std::uint32_t seed, const PuffStyle& st) {
    const int cs = 128;
    Sheet s(cs * 4, cs * 4);
    for (int k = 0; k < 16; ++k) {
        Rng rng(seed + static_cast<std::uint32_t>(k) * 7919u);
        std::vector<Blob> blobs;
        blobs.push_back({rng.range(-0.06f, 0.06f), rng.range(-0.06f, 0.06f), rng.range(st.cmin, st.cmax)});
        for (int i = 1; i < st.blobs; ++i) {
            const float ang = rng.range(0.0f, 2.0f * kPi);
            const float dist = rng.range(0.12f, st.spread);
            blobs.push_back({std::cos(ang) * dist, std::sin(ang) * dist * st.stretch_y, rng.range(st.rmin, st.rmax)});
        }
        const std::uint32_t ns = seed * 31u + static_cast<std::uint32_t>(k) * 977u;
        const float zoff = static_cast<float>(k) * 3.17f;
        auto density = [&](float u0, float v0) {
            const float u = u0 * st.scale, v = v0 * st.scale;
            const float wx = fbm(u * 1.7f, v * 1.7f, zoff, 3, ns + 1u);
            const float wy = fbm(u * 1.7f + 7.3f, v * 1.7f - 2.1f, zoff, 3, ns + 2u);
            const float qx = u + st.warp * wx;
            const float qy = v + st.warp * wy;
            float shape = 0.0f;
            for (const Blob& b : blobs) {
                const float dx = qx - b.x, dy = qy - b.y;
                const float t = 1.0f - (dx * dx + dy * dy) / (b.r * b.r);
                if (t > 0.0f) shape += t * t;
            }
            const float bil = billow(qx * 3.0f, qy * 3.0f, zoff, 5, ns + 3u);
            const float fine = fbm(qx * 5.0f, qy * 5.0f, zoff, 3, ns + 4u);
            const float r = std::sqrt(u0 * u0 + v0 * v0);
            return shape * 1.05f + st.detail * (bil - 0.45f) + 0.07f * fine - st.threshold - smooth(0.72f, 1.0f, r) * 1.5f;
        };
        s.cell(k % 4, k / 4, cs, cs, 1, [&](float u, float v) {
            const float d = density(u, v);
            if (d <= -0.05f) return gray(st.shade_hi, 0.0f);
            const float e = 2.0f / static_cast<float>(cs);
            const float gx = (density(u + e, v) - density(u - e, v)) / (2.0f * e);
            const float gy = (density(u, v + e) - density(u, v - e)) / (2.0f * e);
            float nx = -gx * st.normal, ny = -gy * st.normal, nz = 1.0f;
            const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= nl;
            ny /= nl;
            nz /= nl;
            const float lx = -0.48f, ly = 0.62f, lz = 0.62f;
            const float lam = clamp01(nx * lx + ny * ly + nz * lz);
            float shade = mixf(st.shade_lo, st.shade_hi, lam);
            shade *= 1.0f - 0.16f * smooth(0.35f, 1.3f, d);
            shade = mixf(shade, st.shade_hi, 0.25f * (1.0f - smooth(0.0f, 0.35f, d)));
            const float wisp = 0.55f + 0.45f * clamp01(billow(u * 4.0f + 9.0f, v * 4.0f, zoff + 1.5f, 4, ns + 9u) * 1.4f - 0.1f);
            const float a = (1.0f - std::exp(-st.thickness * std::max(0.0f, d))) * smooth(0.0f, st.softness, d) * st.opacity * wisp;
            return gray(clamp01(shade), a);
        });
    }
    return s.bytes();
}

ParticlePixels make_smoke() {
    PuffStyle st{0.34f, 0.95f, 0.18f, 0.45f, 0.95f, 0.58f, 1.0f, 1.0f, 8, 0.45f, 0.22f, 0.42f, 0.5f, 0.6f, 1.25f, 0.05f, 0.66f};
    return make_puffs(11u, st);
}

ParticlePixels make_steam() {
    PuffStyle st{0.45f, 0.75f, 0.18f, 0.5f, 0.9f, 0.8f, 1.0f, 1.4f, 7, 0.42f, 0.22f, 0.4f, 0.45f, 0.58f, 1.1f, 0.035f, 0.7f};
    return make_puffs(57u, st);
}

ParticlePixels make_fire() {
    const int cs = 128;
    Sheet s(cs * 4, cs * 4);
    const std::uint32_t seed = 303u;
    for (int f = 0; f < 16; ++f) {
        const float t = static_cast<float>(f) / 15.0f;
        const float time = t * 2.6f;
        const float grow = 0.3f + 0.7f * smooth(0.0f, 0.3f, t);
        const float lift = 0.5f * t * t;
        const float fadeout = smooth(0.5f, 1.0f, t);
        s.cell(f % 4, f / 4, cs, cs, 1, [&](float u, float v) {
            const float up = (v + 1.0f) * 0.5f;
            const float w1 = fbm(u * 2.0f, v * 2.0f - time * 1.7f, time * 0.35f, 4, seed);
            const float w2 = fbm(u * 2.0f + 5.2f, v * 2.0f - time * 1.7f, time * 0.35f, 4, seed + 9u);
            const float qx = u + 0.26f * w1 * (0.25f + up);
            const float qy = v + 0.14f * w2;
            const float base = -0.7f + lift;
            const float height = 0.75f + 0.95f * grow;
            const float width = 0.6f * (0.6f + 0.4f * grow) * (1.0f - 0.2f * fadeout);
            const float by = qy - base;
            float dist;
            if (by < 0.0f) {
                const float ex = qx / width, ey = by / (width * 0.8f);
                dist = std::sqrt(ex * ex + ey * ey);
            } else {
                const float yy = by / height;
                const float hw = width * std::max(0.02f, 1.0f - 0.92f * std::min(yy, 1.0f));
                const float ex = qx / hw;
                dist = std::sqrt(ex * ex + yy * yy);
            }
            const float yy = clamp01(by / height);
            const float tongues = fbm(u * 3.2f, v * 2.6f - time * 2.4f, time * 0.6f + 4.0f, 5, seed + 21u);
            const float r = std::sqrt(u * u + v * v);
            const float d = (1.0f - dist) + tongues * (0.45f + 0.75f * yy) - 0.1f - 0.55f * fadeout - smooth(0.75f, 1.0f, r) * 1.5f;
            if (d <= 0.0f) return heat_ramp(0.0f, 0.0f);
            const float intensity = 1.0f - 0.5f * fadeout;
            const float heat = clamp01((d * 0.7f + 0.16f * (1.0f - yy) + 0.04f) * intensity * (1.0f - 0.25f * fadeout));
            const float a = smooth(0.0f, 0.35f, d) * intensity;
            return heat_ramp(heat, a);
        });
    }
    return s.bytes();
}

ParticlePixels make_explosion() {
    const int cs = 128;
    Sheet s(cs * 4, cs * 4);
    const std::uint32_t seed = 777u;
    for (int f = 0; f < 16; ++f) {
        const float t = static_cast<float>(f) / 15.0f;
        const float radius = 0.4f + 0.44f * std::sqrt(t);
        s.cell(f % 4, f / 4, cs, cs, 1, [&](float u, float v) {
            const float wx = fbm(u * 1.6f, v * 1.6f, t * 0.8f, 3, seed);
            const float wy = fbm(u * 1.6f + 3.3f, v * 1.6f + 1.7f, t * 0.8f, 3, seed + 3u);
            const float qx = u + 0.2f * wx, qy = v + 0.2f * wy - 0.06f * t;
            const float rr = std::sqrt(qx * qx + qy * qy) / radius;
            const float b = billow(qx * 2.6f / radius, qy * 2.6f / radius, t * 1.3f, 5, seed + 7u);
            const float r = std::sqrt(u * u + v * v);
            const float d = (1.0f - rr * rr) + 0.6f * (b - 0.42f) - 0.25f * t - smooth(0.8f, 1.0f, r) * 1.5f;
            if (d <= 0.0f) return heat_ramp(0.0f, 0.0f);
            const float cool = std::pow(1.0f - t, 1.8f);
            const float heat = clamp01((d * 0.95f + 0.15f) * (0.3f + 0.75f * cool) + 0.3f * (b - 0.42f));
            const float a = smooth(0.0f, 0.3f, d) * (1.0f - 0.55f * t);
            return heat_ramp(heat, a);
        });
    }
    return s.bytes();
}

ParticlePixels make_spark() {
    Sheet s(32, 128);
    s.cell(0, 0, 32, 128, 2, [](float u, float v) {
        const float core = std::exp(-(u / 0.16f) * (u / 0.16f));
        const float halo = std::exp(-(u / 0.5f) * (u / 0.5f)) * 0.35f;
        const float along = smooth(-1.0f, 0.35f, v) * smooth(1.0f, 0.72f, v);
        const float head = std::exp(-((v - 0.62f) / 0.22f) * ((v - 0.62f) / 0.22f)) * 0.35f;
        const float a = clamp01((core + halo) * (along + head * core)) * smooth(1.0f, 0.85f, std::fabs(u));
        const float hot = clamp01(core * 1.2f);
        return Rgba{1.0f, mixf(0.88f, 1.0f, hot), mixf(0.7f, 1.0f, hot), a};
    });
    return s.bytes();
}

ParticlePixels make_ember() {
    const int cs = 64;
    Sheet s(cs * 2, cs * 2);
    for (int k = 0; k < 4; ++k) {
        const std::uint32_t seed = 41u + static_cast<std::uint32_t>(k) * 13u;
        const float z = static_cast<float>(k) * 2.3f;
        s.cell(k % 2, k / 2, cs, cs, 2, [&](float u, float v) {
            const float n = fbm(u * 2.6f, v * 2.6f, z, 3, seed);
            const float qx = u * (1.0f + 0.25f * std::sin(static_cast<float>(k))), qy = v;
            const float r = std::sqrt(qx * qx + qy * qy) * (1.0f + 0.55f * n) / 0.42f;
            const float body = smooth(1.0f, 0.55f, r);
            const float core = std::exp(-r * r * 2.5f);
            const float halo = std::exp(-(u * u + v * v) * 4.5f) * 0.45f * edge_window(u, v, 0.7f);
            const float flick = 0.75f + 0.25f * fbm(u * 9.0f, v * 9.0f, z, 2, seed + 5u);
            const float a = clamp01(body * flick + halo);
            const float hot = clamp01(core * body);
            return Rgba{1.0f, mixf(0.62f, 0.98f, hot), mixf(0.32f, 0.85f, hot), a};
        });
    }
    return s.bytes();
}

ParticlePixels make_rain() {
    Sheet s(16, 128);
    s.cell(0, 0, 16, 128, 2, [](float u, float v) {
        const float across = std::exp(-(u / 0.4f) * (u / 0.4f)) * smooth(1.0f, 0.8f, std::fabs(u));
        const float along = smooth(-1.0f, -0.3f, v) * smooth(1.0f, 0.86f, v);
        const float head = 0.55f + 0.45f * smooth(-0.4f, 0.8f, v);
        const float glint = std::exp(-(u + 0.12f) * (u + 0.12f) / 0.02f) * 0.35f;
        const float a = clamp01(across * along * head * 0.9f + glint * along);
        return Rgba{0.86f + 0.14f * glint, 0.92f + 0.08f * glint, 1.0f, a};
    });
    return s.bytes();
}

float segment_distance(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    const float t = clamp01(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy));
    const float ex = px - ax - dx * t, ey = py - ay - dy * t;
    return std::sqrt(ex * ex + ey * ey);
}

ParticlePixels make_snow() {
    const int cs = 64;
    Sheet s(cs * 2, cs * 2);
    for (int k = 0; k < 4; ++k) {
        Rng rng(900u + static_cast<std::uint32_t>(k));
        const float arm = rng.range(0.62f, 0.8f);
        const float b1 = rng.range(0.25f, 0.4f), b2 = rng.range(0.48f, 0.6f);
        const float l1 = rng.range(0.16f, 0.26f), l2 = rng.range(0.1f, 0.18f);
        const float thick = rng.range(0.035f, 0.055f);
        s.cell(k % 2, k / 2, cs, cs, 3, [&](float u, float v) {
            const float r = std::sqrt(u * u + v * v);
            const float halo = std::exp(-r * r * 5.0f) * 0.35f * edge_window(u, v, 0.75f);
            if (k == 0) {
                const float n = fbm(u * 3.0f, v * 3.0f, 1.0f, 3, 77u);
                const float body = smooth(0.62f, 0.2f, r * (1.0f + 0.4f * n));
                return gray(1.0f, clamp01(body * 0.95f + halo));
            }
            float ang = std::atan2(v, u);
            const float sector = kPi / 3.0f;
            ang = std::fmod(ang + 2.0f * kPi, sector);
            if (ang > sector * 0.5f) ang = sector - ang;
            const float px = r * std::cos(ang), py = r * std::sin(ang);
            float d = segment_distance(px, py, 0.0f, 0.0f, arm, 0.0f);
            const float c60 = 0.5f, s60 = 0.8660254f;
            d = std::min(d, segment_distance(px, py, b1, 0.0f, b1 + l1 * c60, l1 * s60));
            d = std::min(d, segment_distance(px, py, b2, 0.0f, b2 + l2 * c60, l2 * s60));
            const float hex = r - 0.12f;
            const float shape = std::max(smooth(thick, thick * 0.35f, d), smooth(0.02f, -0.02f, hex));
            return gray(1.0f, clamp01(shape + halo));
        });
    }
    return s.bytes();
}

ParticlePixels make_magic() {
    const int cs = 128;
    Sheet s(cs * 2, cs * 2);
    for (int k = 0; k < 4; ++k) {
        s.cell(k % 2, k / 2, cs, cs, 2, [&](float u, float v) {
            const float r2 = u * u + v * v;
            const float r = std::sqrt(r2);
            const float win = edge_window(u, v, 0.8f);
            const float core = std::exp(-r2 * 40.0f);
            const float halo = std::exp(-r2 * 7.0f) * 0.35f;
            auto ray = [&](float angle, float width, float len) {
                const float c = std::cos(angle), sn = std::sin(angle);
                const float along = std::fabs(u * c + v * sn), across = std::fabs(-u * sn + v * c);
                const float fall = std::pow(clamp01(1.0f - along / len), 2.2f);
                return std::exp(-(across / (width * (0.25f + fall))) * (across / (width * (0.25f + fall)))) * fall;
            };
            float a = 0.0f;
            if (k == 0) {
                a = core + halo + ray(0.0f, 0.05f, 0.98f) + ray(kPi * 0.5f, 0.05f, 0.98f) + 0.45f * ray(kPi * 0.25f, 0.035f, 0.55f) + 0.45f * ray(kPi * 0.75f, 0.035f, 0.55f);
            } else if (k == 1) {
                for (int i = 0; i < 3; ++i) a += ray(kPi * static_cast<float>(i) / 3.0f, 0.045f, 0.9f);
                a += core + halo;
            } else if (k == 2) {
                const float ring = std::exp(-((r - 0.55f) / 0.06f) * ((r - 0.55f) / 0.06f)) * 0.55f;
                a = core * 1.2f + halo * 1.6f + ring;
            } else {
                a = core * 1.1f + halo * 0.8f + 0.8f * ray(0.0f, 0.03f, 0.95f) + 0.8f * ray(kPi * 0.5f, 0.03f, 0.95f);
            }
            const float hot = clamp01(core * 1.5f);
            return Rgba{1.0f, 1.0f, mixf(0.95f, 1.0f, hot), clamp01(a) * win};
        });
    }
    return s.bytes();
}

ParticlePixels make_bubble() {
    Sheet s(128, 128);
    s.cell(0, 0, 128, 128, 2, [](float u, float v) {
        const float r = std::sqrt(u * u + v * v) / 0.94f;
        if (r >= 1.0f) return gray(1.0f, 0.0f);
        const float rim = std::exp(-((r - 0.93f) / 0.05f) * ((r - 0.93f) / 0.05f));
        const float fres = std::pow(r, 5.0f) * 0.55f;
        const float hx = u + 0.36f, hy = v - 0.38f;
        const float highlight = std::exp(-(hx * hx * 30.0f + hy * hy * 60.0f)) * smooth(0.0f, 0.2f, 0.5f - std::fabs(hx - hy) * 0.5f + 0.2f);
        const float hx2 = u - 0.42f, hy2 = v + 0.42f;
        const float highlight2 = std::exp(-(hx2 * hx2 + hy2 * hy2) * 90.0f) * 0.45f;
        const float edge = smooth(1.0f, 0.97f, r);
        const float a = clamp01(0.06f + fres + rim * 0.65f + highlight + highlight2) * edge;
        const float ang = std::atan2(v, u);
        const float irid = rim + fres;
        const float rr = 1.0f - 0.18f * irid * (0.5f + 0.5f * std::cos(ang * 2.0f + 0.5f));
        const float gg = 1.0f - 0.14f * irid * (0.5f + 0.5f * std::cos(ang * 2.0f + 2.6f));
        const float bb = 1.0f - 0.12f * irid * (0.5f + 0.5f * std::cos(ang * 2.0f + 4.7f));
        const float spec = clamp01(highlight + highlight2);
        return Rgba{mixf(rr, 1.0f, spec), mixf(gg, 1.0f, spec), mixf(bb, 1.0f, spec), a};
    });
    return s.bytes();
}

ParticlePixels make_debris() {
    const int cs = 64;
    Sheet s(cs * 4, cs * 4);
    for (int k = 0; k < 16; ++k) {
        Rng rng(5000u + static_cast<std::uint32_t>(k) * 31u);
        const int n = 5 + static_cast<int>(rng.next() * 4.0f);
        std::array<float, 10> angle{}, radius{}, tilt{}, facet{};
        float acc = 0.0f;
        for (int i = 0; i < n; ++i) {
            acc += rng.range(0.6f, 1.4f);
            angle[static_cast<std::size_t>(i)] = acc;
        }
        for (int i = 0; i < n; ++i) {
            angle[static_cast<std::size_t>(i)] *= 2.0f * kPi / acc;
            radius[static_cast<std::size_t>(i)] = rng.range(0.5f, 0.85f);
            tilt[static_cast<std::size_t>(i)] = rng.range(0.35f, 1.1f);
            facet[static_cast<std::size_t>(i)] = rng.range(0.85f, 1.1f);
        }
        const float top = rng.range(0.3f, 0.5f);
        const float elong = rng.range(0.7f, 1.0f);
        const std::uint32_t ns = 6000u + static_cast<std::uint32_t>(k) * 17u;
        s.cell(k % 4, k / 4, cs, cs, 3, [&](float u, float v) {
            const float x = u, y = v / elong;
            const float r = std::sqrt(x * x + y * y);
            float ang = std::atan2(y, x);
            if (ang < 0.0f) ang += 2.0f * kPi;
            int i = 0;
            while (i < n && angle[static_cast<std::size_t>(i)] < ang) ++i;
            const int a0 = (i - 1 + n) % n, a1 = i % n;
            float t0 = angle[static_cast<std::size_t>(a0)], t1 = angle[static_cast<std::size_t>(a1)];
            if (i == 0) t0 -= 2.0f * kPi;
            if (i == n) t1 += 2.0f * kPi;
            const float ax = std::cos(t0) * radius[static_cast<std::size_t>(a0)], ay = std::sin(t0) * radius[static_cast<std::size_t>(a0)];
            const float bx = std::cos(t1) * radius[static_cast<std::size_t>(a1)], by = std::sin(t1) * radius[static_cast<std::size_t>(a1)];
            const float ex = bx - ax, ey = by - ay;
            const float cross = ex * (y - ay) - ey * (x - ax);
            const float elen = std::sqrt(ex * ex + ey * ey);
            const float inside = cross / elen;
            const float rough = 0.03f * fbm(u * 6.0f, v * 6.0f, 0.0f, 3, ns);
            if (inside + rough < 0.0f) return gray(0.5f, 0.0f);
            const float mid = (t0 + t1) * 0.5f;
            float nx, ny, nz;
            const float edge_r = r / std::max(0.05f, (radius[static_cast<std::size_t>(a0)] + radius[static_cast<std::size_t>(a1)]) * 0.5f);
            if (edge_r < top) {
                nx = 0.15f * std::cos(mid + 1.0f);
                ny = 0.15f * std::sin(mid + 1.0f);
                nz = 1.0f;
            } else {
                const float tl = tilt[static_cast<std::size_t>(a0)];
                nx = std::cos(mid) * tl;
                ny = std::sin(mid) * tl;
                nz = 1.0f;
            }
            const float g = fbm(u * 9.0f, v * 9.0f, 1.0f, 4, ns + 1u);
            nx += g * 0.35f;
            ny += fbm(u * 9.0f + 3.0f, v * 9.0f, 1.0f, 4, ns + 2u) * 0.35f;
            const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            const float lam = clamp01((nx * -0.5f + ny * 0.6f + nz * 0.62f) / nl);
            const float albedo = (0.72f + 0.28f * fbm(u * 4.0f, v * 4.0f, 2.0f, 3, ns + 3u)) * facet[static_cast<std::size_t>(a0)];
            const float rim = smooth(0.0f, 0.06f, inside + rough);
            const float shade = albedo * (0.22f + 0.9f * lam) * (0.6f + 0.4f * rim);
            return gray(clamp01(shade), smooth(-0.01f, 0.02f, inside + rough));
        });
    }
    return s.bytes();
}

ParticlePixels make_dust() {
    const int cs = 64;
    Sheet s(cs * 2, cs * 2);
    for (int k = 0; k < 4; ++k) {
        const std::uint32_t seed = 1200u + static_cast<std::uint32_t>(k) * 7u;
        const float z = static_cast<float>(k) * 1.9f;
        s.cell(k % 2, k / 2, cs, cs, 1, [&](float u, float v) {
            const float n = fbm(u * 2.4f, v * 2.4f, z, 4, seed);
            const float r = std::sqrt(u * u + v * v) * (1.0f + 0.5f * n);
            const float body = std::exp(-r * r * 3.4f);
            const float grain = 0.7f + 0.3f * fbm(u * 8.0f, v * 8.0f, z, 3, seed + 3u);
            return gray(1.0f, clamp01(body * grain) * edge_window(u, v, 0.72f));
        });
    }
    return s.bytes();
}

ParticlePixels make_shockwave() {
    Sheet s(128, 128);
    s.cell(0, 0, 128, 128, 2, [](float u, float v) {
        const float r = std::sqrt(u * u + v * v);
        const float ang = std::atan2(v, u);
        const float n = fbm(std::cos(ang) * 2.5f, std::sin(ang) * 2.5f, r * 3.0f, 4, 4242u);
        const float ring = std::exp(-((r - 0.78f) / 0.07f) * ((r - 0.78f) / 0.07f));
        const float inner = smooth(0.25f, 0.78f, r) * smooth(0.86f, 0.76f, r) * 0.28f;
        const float a = clamp01((ring * 0.95f + inner) * (0.7f + 0.45f * n)) * edge_window(u, v, 0.9f);
        return gray(1.0f, a);
    });
    return s.bytes();
}

} // namespace

ParticlePixels make_soft_smoke() {
    const int cs = 128;
    Sheet s(cs * 4, cs * 4);
    for (int k = 0; k < 16; ++k) {
        const std::uint32_t ns = 4211u + static_cast<std::uint32_t>(k) * 613u;
        const float z = static_cast<float>(k) * 2.71f;
        s.cell(k % 4, k / 4, cs, cs, 1, [&](float u, float v) {
            const float wx = fbm(u * 1.4f, v * 1.4f, z, 3, ns);
            const float wy = fbm(u * 1.4f + 4.1f, v * 1.4f - 2.6f, z, 3, ns + 1u);
            const float qx = u + 0.28f * wx, qy = v + 0.28f * wy;
            const float r = std::sqrt(qx * qx + qy * qy);
            const float body = std::exp(-2.6f * r * r);
            const float cloud = 0.5f + 0.5f * fbm(qx * 2.2f, qy * 2.2f, z + 1.0f, 5, ns + 2u);
            const float d = body * (0.45f + 0.75f * cloud) - 0.12f;
            const float a = clamp01(d * 1.35f) * edge_window(u, v, 0.62f);
            const float shade = 0.78f + 0.22f * clamp01(0.5f + 0.5f * v + 0.3f * (cloud - 0.5f));
            return gray(shade, a * 0.9f);
        });
    }
    return s.bytes();
}

ParticlePixels make_steam_wisp() {
    const int cs = 128;
    Sheet s(cs * 4, cs * 4);
    for (int k = 0; k < 16; ++k) {
        const std::uint32_t ns = 9001u + static_cast<std::uint32_t>(k) * 421u;
        const float z = static_cast<float>(k) * 1.93f;
        s.cell(k % 4, k / 4, cs, cs, 1, [&](float u, float v) {
            const float bend = 0.35f * fbm(v * 1.2f, z, 0.0f, 3, ns);
            const float qx = (u - bend) * 1.6f;
            const float qy = v * 0.75f;
            const float streak = 0.5f + 0.5f * fbm(qx * 3.5f, qy * 1.4f, z + 3.0f, 5, ns + 7u);
            const float body = std::exp(-1.8f * (qx * qx + qy * qy));
            const float d = body * (0.3f + 0.9f * streak * streak) - 0.1f;
            const float a = clamp01(d * 1.2f) * edge_window(u, v, 0.6f);
            return gray(0.97f, a * 0.75f);
        });
    }
    return s.bytes();
}

ParticlePixels make_flame() {
    const int cs = 128;
    Sheet s(cs * 4, cs * 4);
    for (int k = 0; k < 16; ++k) {
        const std::uint32_t ns = 1301u + static_cast<std::uint32_t>(k) * 97u;
        const float z = static_cast<float>(k) * 3.3f;
        const float lean = (static_cast<float>(k % 5) - 2.0f) * 0.05f;
        s.cell(k % 4, k / 4, cs, cs, 1, [&](float u, float v) {
            const float up = (v + 1.0f) * 0.5f;
            const float wob = fbm(u * 1.8f, v * 2.4f, z, 4, ns) * (0.12f + 0.3f * up);
            const float qx = u - wob - lean * up;
            const float base = -0.75f;
            const float h = 1.55f;
            const float t = clamp01((v - base) / h);
            const float width = 0.42f * std::pow(1.0f - t, 0.75f) * (0.75f + 0.5f * std::sqrt(t)) + 0.01f;
            float dist = std::fabs(qx) / width;
            if (v < base) dist = std::sqrt(qx * qx + (v - base) * (v - base) * 4.0f) / 0.42f;
            const float tongues = fbm(u * 3.0f, v * 2.2f, z + 5.0f, 5, ns + 3u);
            const float d = (1.0f - dist) + tongues * (0.25f + 0.55f * t) - 0.08f;
            if (d <= 0.0f) return heat_ramp(0.0f, 0.0f);
            const float heat = clamp01(d * 0.9f + 0.25f * (1.0f - t) - 0.05f);
            const float a = smooth(0.0f, 0.3f, d) * smooth(1.0f, 0.82f, std::sqrt(u * u + v * v));
            return heat_ramp(heat, a);
        });
    }
    return s.bytes();
}

ParticlePixels make_flash() {
    Sheet s(128, 128);
    s.cell(0, 0, 128, 128, 2, [](float u, float v) {
        const float r = std::sqrt(u * u + v * v);
        const float ang = std::atan2(v, u);
        const float rays = std::pow(std::fabs(std::cos(ang * 3.0f)), 24.0f) + 0.6f * std::pow(std::fabs(std::cos(ang * 3.0f + 0.52f)), 40.0f);
        const float core = std::exp(-14.0f * r * r);
        const float halo = std::exp(-3.5f * r * r) * 0.45f;
        const float streak = rays * std::exp(-2.2f * r) * 0.9f;
        const float a = clamp01(core + halo + streak) * edge_window(u, v, 0.75f);
        const float hot = clamp01(core * 1.3f);
        return Rgba{1.0f, mixf(0.86f, 1.0f, hot), mixf(0.6f, 0.95f, hot), a};
    });
    return s.bytes();
}

ParticlePixels make_scorch() {
    const int cs = 128;
    Sheet s(cs * 2, cs * 2);
    for (int k = 0; k < 4; ++k) {
        const std::uint32_t seed = 301u + static_cast<std::uint32_t>(k) * 17u;
        const float z = static_cast<float>(k) * 3.1f;
        s.cell(k % 2, k / 2, cs, cs, 2, [&](float u, float v) {
            const float r = std::sqrt(u * u + v * v);
            const float ang = std::atan2(v, u);
            const float ragged = fbm(std::cos(ang) * 2.2f, std::sin(ang) * 2.2f, z, 4, seed);
            const float rays = std::pow(clamp01(fbm(std::cos(ang) * 7.0f, std::sin(ang) * 7.0f, z + 9.0f, 2, seed + 3u) * 1.6f - 0.3f), 3.0f);
            const float reach = 0.6f + 0.2f * ragged + 0.16f * rays;
            const float body = smooth(reach, reach * 0.35f, r);
            const float soot = 0.55f + 0.45f * fbm(u * 6.0f, v * 6.0f, z + 4.0f, 4, seed + 7u);
            const float speck = clamp01(fbm(u * 22.0f, v * 22.0f, z, 2, seed + 11u) * 2.2f - 1.1f) * smooth(0.95f, 0.5f, r);
            const float core = std::exp(-r * r * 5.0f);
            const float a = clamp01(body * soot * (0.75f + 0.25f * core) + speck * 0.5f) * edge_window(u, v, 0.85f);
            const float shade = mixf(0.9f, 0.6f, core);
            return Rgba{shade, shade * 0.97f, shade * 0.94f, a};
        });
    }
    return s.bytes();
}

ParticlePixels generate_builtin_particle_texture(std::string_view name) {
    if (name == "builtin:glow") return make_glow();
    if (name == "builtin:smoke") return make_smoke();
    if (name == "builtin:steam") return make_steam();
    if (name == "builtin:fire") return make_fire();
    if (name == "builtin:explosion") return make_explosion();
    if (name == "builtin:spark") return make_spark();
    if (name == "builtin:ember") return make_ember();
    if (name == "builtin:rain") return make_rain();
    if (name == "builtin:snow") return make_snow();
    if (name == "builtin:magic") return make_magic();
    if (name == "builtin:bubble") return make_bubble();
    if (name == "builtin:debris") return make_debris();
    if (name == "builtin:dust") return make_dust();
    if (name == "builtin:shockwave") return make_shockwave();
    if (name == "builtin:smoke-soft") return make_soft_smoke();
    if (name == "builtin:steam-wisp") return make_steam_wisp();
    if (name == "builtin:flame") return make_flame();
    if (name == "builtin:flash") return make_flash();
    if (name == "builtin:scorch") return make_scorch();
    return ParticlePixels{};
}

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

int wrapi(int v, int n) {
    v %= n;
    return v < 0 ? v + n : v;
}

float unit_hash(std::uint32_t h) {
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

float tile_noise(float x, float y, int period, std::uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    const float dx = x - fx, dy = y - fy;
    auto g = [&](int ox, int oy) {
        const std::uint32_t h = hash3(wrapi(ix + ox, period), wrapi(iy + oy, period), 7, seed);
        const float a = unit_hash(h) * kTwoPi;
        return std::cos(a) * (dx - static_cast<float>(ox)) + std::sin(a) * (dy - static_cast<float>(oy));
    };
    const float u = fade(dx), v = fade(dy);
    return mixf(mixf(g(0, 0), g(1, 0), u), mixf(g(0, 1), g(1, 1), u), v) * 1.41f;
}

float tile_fbm(float u, float v, int period, int octaves, std::uint32_t seed, float gain = 0.5f) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * tile_noise(u * static_cast<float>(period), v * static_cast<float>(period), period, seed + static_cast<std::uint32_t>(i) * 977u);
        norm += amp;
        amp *= gain;
        period *= 2;
    }
    return sum / norm;
}

float tile_ridged(float u, float v, int period, int octaves, std::uint32_t seed) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        const float n = 1.0f - std::fabs(tile_noise(u * static_cast<float>(period), v * static_cast<float>(period), period, seed + static_cast<std::uint32_t>(i) * 433u));
        sum += amp * n * n;
        norm += amp;
        amp *= 0.5f;
        period *= 2;
    }
    return sum / norm;
}

struct Cell {
    float f1 = 9.0f;
    float f2 = 9.0f;
    std::uint32_t id = 0;
};

Cell tile_cells(float u, float v, int cells, std::uint32_t seed) {
    const float x = u * static_cast<float>(cells), y = v * static_cast<float>(cells);
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    Cell c;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            const int cx = ix + ox, cy = iy + oy;
            const std::uint32_t h = hash3(wrapi(cx, cells), wrapi(cy, cells), 3, seed);
            const float px = static_cast<float>(cx) + 0.1f + 0.8f * unit_hash(h);
            const float py = static_cast<float>(cy) + 0.1f + 0.8f * unit_hash(h * 0x9E3779B1u + 7u);
            const float d = std::sqrt((px - x) * (px - x) + (py - y) * (py - y));
            if (d < c.f1) {
                c.f2 = c.f1;
                c.f1 = d;
                c.id = h;
            } else if (d < c.f2) {
                c.f2 = d;
            }
        }
    }
    return c;
}

struct Field {
    int n = 0;
    std::vector<float> r, g, b, h;

    explicit Field(int size) : n(size), r(static_cast<std::size_t>(size) * size), g(r.size()), b(r.size()), h(r.size()) {}

    std::size_t at(int x, int y) const { return static_cast<std::size_t>(wrapi(y, n)) * static_cast<std::size_t>(n) + static_cast<std::size_t>(wrapi(x, n)); }

    void put(std::size_t i, float cr, float cg, float cb, float height) {
        r[i] = cr;
        g[i] = cg;
        b[i] = cb;
        h[i] = height;
    }
};

std::uint8_t byte_of(float v) {
    return static_cast<std::uint8_t>(std::lround(clamp01(v) * 255.0f));
}

ParticlePixels albedo_of(const Field& f) {
    ParticlePixels p;
    p.width = p.height = f.n;
    p.rgba.resize(static_cast<std::size_t>(f.n) * f.n * 4);
    for (std::size_t i = 0; i < f.r.size(); ++i) {
        p.rgba[i * 4 + 0] = byte_of(f.r[i]);
        p.rgba[i * 4 + 1] = byte_of(f.g[i]);
        p.rgba[i * 4 + 2] = byte_of(f.b[i]);
        p.rgba[i * 4 + 3] = byte_of(f.h[i]);
    }
    return p;
}

ParticlePixels normal_of(const Field& f, float strength) {
    ParticlePixels p;
    p.width = p.height = f.n;
    p.rgba.resize(static_cast<std::size_t>(f.n) * f.n * 4);
    const float k = strength * static_cast<float>(f.n) / 512.0f;
    for (int y = 0; y < f.n; ++y) {
        for (int x = 0; x < f.n; ++x) {
            const float dx = (f.h[f.at(x + 1, y)] - f.h[f.at(x - 1, y)]) * 0.5f * k;
            const float dy = (f.h[f.at(x, y + 1)] - f.h[f.at(x, y - 1)]) * 0.5f * k;
            float nx = -dx, ny = dy, nz = 1.0f;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len;
            ny /= len;
            nz /= len;
            const std::size_t i = f.at(x, y) * 4;
            p.rgba[i + 0] = byte_of(nx * 0.5f + 0.5f);
            p.rgba[i + 1] = byte_of(ny * 0.5f + 0.5f);
            p.rgba[i + 2] = byte_of(nz * 0.5f + 0.5f);
            p.rgba[i + 3] = 255;
        }
    }
    return p;
}

float grain(int x, int y, std::uint32_t seed) {
    return unit_hash(hash3(x, y, 11, seed)) * 2.0f - 1.0f;
}

void make_grass(Field& f) {
    const int n = f.n;
    const float inv = 1.0f / static_cast<float>(n);
    std::vector<float> cover(f.r.size(), 0.0f);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float u = static_cast<float>(x) * inv, v = static_cast<float>(y) * inv;
            const float soil = tile_fbm(u, v, 6, 4, 41u);
            const float moss = tile_fbm(u, v, 3, 3, 43u) * 0.5f + 0.5f;
            const float g0 = grain(x, y, 47u) * 0.04f;
            const float r = mixf(0.20f, 0.13f, moss) + soil * 0.04f + g0;
            const float g = mixf(0.20f, 0.27f, moss) + soil * 0.03f + g0;
            const float b = mixf(0.09f, 0.06f, moss) + g0 * 0.5f;
            f.put(f.at(x, y), r, g, b, 0.15f + 0.1f * soil);
        }
    }
    const int grid = std::max(8, n / 10);
    const float cell = static_cast<float>(n) / static_cast<float>(grid);
    const float scale = static_cast<float>(n) / 512.0f;
    for (int pass = 0; pass < 3; ++pass) {
        for (int cy = 0; cy < grid; ++cy) {
            for (int cx = 0; cx < grid; ++cx) {
                for (int k = 0; k < 5; ++k) {
                    const std::uint32_t h = hash3(cx, cy, pass * 16 + k, 53u);
                    const float rx = (static_cast<float>(cx) + unit_hash(h)) * cell;
                    const float ry = (static_cast<float>(cy) + unit_hash(h * 0x85EBCA77u + 1u)) * cell;
                    const float u = rx * inv, v = ry * inv;
                    const float patch = tile_fbm(u, v, 4, 3, 59u) * 0.5f + 0.5f;
                    if (unit_hash(h * 0xC2B2AE3Du + 5u) > 0.35f + patch * 0.75f) continue;
                    const float ang = unit_hash(h * 0x27D4EB2Fu + 9u) * kTwoPi;
                    const float len = (7.0f + 9.0f * unit_hash(h * 0x165667B1u + 3u)) * scale * (pass == 2 ? 1.3f : 1.0f);
                    const float wid = (1.2f + 0.9f * unit_hash(h * 0x9E3779B9u + 13u)) * scale;
                    const float ex = rx + std::cos(ang) * len, ey = ry + std::sin(ang) * len;
                    const float dry = clamp01(tile_fbm(u, v, 2, 3, 61u) * 1.4f + 0.15f);
                    const float tone = unit_hash(h * 0x7FEB352Du + 17u);
                    const float br = mixf(mixf(0.16f, 0.30f, tone), mixf(0.44f, 0.58f, tone), dry * dry);
                    const float bg = mixf(mixf(0.30f, 0.48f, tone), mixf(0.46f, 0.52f, tone), dry * dry);
                    const float bb = mixf(mixf(0.07f, 0.12f, tone), mixf(0.14f, 0.20f, tone), dry * dry);
                    const int x0 = static_cast<int>(std::floor(std::min(rx, ex) - wid - 1.0f)), x1 = static_cast<int>(std::ceil(std::max(rx, ex) + wid + 1.0f));
                    const int y0 = static_cast<int>(std::floor(std::min(ry, ey) - wid - 1.0f)), y1 = static_cast<int>(std::ceil(std::max(ry, ey) + wid + 1.0f));
                    const float ax = ex - rx, ay = ey - ry, aa = ax * ax + ay * ay;
                    for (int py = y0; py <= y1; ++py) {
                        for (int px = x0; px <= x1; ++px) {
                            const float qx = static_cast<float>(px) + 0.5f - rx, qy = static_cast<float>(py) + 0.5f - ry;
                            const float t = clamp01((qx * ax + qy * ay) / std::max(aa, 1e-4f));
                            const float dx = qx - ax * t, dy = qy - ay * t;
                            const float w = mixf(wid, wid * 0.25f, t);
                            const float a = clamp01(w - std::sqrt(dx * dx + dy * dy) + 0.5f);
                            if (a <= 0.0f) continue;
                            const std::size_t i = f.at(px, py);
                            const float lift = 0.55f + 0.45f * t + 0.12f * static_cast<float>(pass);
                            const float height = std::min(1.0f, 0.35f + 0.65f * t * (0.7f + 0.15f * static_cast<float>(pass)));
                            if (height * a < f.h[i] * 0.98f && pass > 0 && cover[i] > 0.6f) continue;
                            f.r[i] = mixf(f.r[i], br * lift, a);
                            f.g[i] = mixf(f.g[i], bg * lift, a);
                            f.b[i] = mixf(f.b[i], bb * lift, a);
                            f.h[i] = std::max(f.h[i], height * a + f.h[i] * (1.0f - a));
                            cover[i] = std::max(cover[i], a);
                        }
                    }
                }
            }
        }
    }
}

void make_dirt(Field& f) {
    const int n = f.n;
    const float inv = 1.0f / static_cast<float>(n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float u = static_cast<float>(x) * inv, v = static_cast<float>(y) * inv;
            const float base = tile_fbm(u, v, 4, 6, 71u) * 0.5f + 0.5f;
            const float damp = tile_fbm(u, v, 2, 3, 73u) * 0.5f + 0.5f;
            const float g0 = grain(x, y, 79u) * 0.05f;
            float r = mixf(0.27f, 0.47f, base) * mixf(1.0f, 0.78f, damp) + g0;
            float g = mixf(0.19f, 0.35f, base) * mixf(1.0f, 0.78f, damp) + g0;
            float b = mixf(0.12f, 0.23f, base) * mixf(1.0f, 0.8f, damp) + g0 * 0.8f;
            float h = 0.3f + 0.25f * base;
            for (int layer = 0; layer < 2; ++layer) {
                const Cell c = tile_cells(u + 0.013f * tile_noise(u * 16.0f, v * 16.0f, 16, 83u), v, layer == 0 ? 14 : 34, 89u + static_cast<std::uint32_t>(layer));
                const float size = (layer == 0 ? 0.22f : 0.3f) + 0.22f * unit_hash(c.id * 0x85EBCA77u);
                if (unit_hash(c.id * 0x27D4EB2Fu) > (layer == 0 ? 0.3f : 0.32f) * (0.4f + damp)) continue;
                const float e = c.f1 / size;
                if (e >= 1.0f) {
                    const float rim = clamp01(1.0f - (e - 1.0f) * 2.5f);
                    r *= 1.0f - 0.25f * rim;
                    g *= 1.0f - 0.25f * rim;
                    b *= 1.0f - 0.25f * rim;
                    continue;
                }
                const float dome = std::sqrt(clamp01(1.0f - e * e));
                const float tone = unit_hash(c.id * 0x9E3779B1u);
                const float pr = mixf(0.36f, 0.62f, tone), pg = mixf(0.33f, 0.56f, tone), pb = mixf(0.29f, 0.5f, tone);
                const float edge = smooth(1.0f, 0.82f, e);
                const float shade = 0.72f + 0.28f * dome;
                r = mixf(r, pr * shade, edge);
                g = mixf(g, pg * shade, edge);
                b = mixf(b, pb * shade, edge);
                h = std::max(h, mixf(h, 0.55f + 0.45f * dome, edge));
            }
            f.put(f.at(x, y), r, g, b, h);
        }
    }
}

void make_rock(Field& f) {
    const int n = f.n;
    const float inv = 1.0f / static_cast<float>(n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float u = static_cast<float>(x) * inv, v = static_cast<float>(y) * inv;
            const float wu = u + 0.04f * tile_fbm(u, v, 3, 3, 101u), wv = v + 0.04f * tile_fbm(u, v, 3, 3, 103u);
            const float ridge = tile_ridged(wu, wv, 3, 6, 107u);
            const float strata = 0.5f + 0.5f * std::sin((wv * 7.0f + 0.6f * tile_fbm(wu, wv, 2, 4, 109u)) * kTwoPi);
            const Cell c = tile_cells(wu, wv, 4, 113u);
            const float reach = smooth(0.05f, 0.35f, tile_fbm(u, v, 4, 3, 119u));
            const float crack = 1.0f - (1.0f - smooth(0.0f, 0.03f, c.f2 - c.f1)) * reach;
            const Cell c2 = tile_cells(wu, wv, 11, 127u);
            const float fine = 1.0f - (1.0f - smooth(0.0f, 0.04f, c2.f2 - c2.f1)) * smooth(0.2f, 0.45f, tile_fbm(u, v, 6, 3, 121u));
            const float face = (unit_hash(c.id) * 0.12f - 0.06f) * reach;
            const float g0 = grain(x, y, 131u) * 0.06f;
            float h = clamp01(ridge * 0.8f + strata * 0.16f + face + tile_fbm(u, v, 8, 4, 141u) * 0.12f + 0.05f);
            h = h * (0.35f + 0.65f * crack) * (0.75f + 0.25f * fine);
            const float warm = tile_fbm(u, v, 2, 3, 137u) * 0.5f + 0.5f;
            const float lum = mixf(0.2f, 0.52f, h) + g0 + 0.07f * tile_fbm(u, v, 32, 2, 143u);
            float r = lum * mixf(0.97f, 1.07f, warm);
            float g = lum * mixf(0.98f, 1.0f, warm);
            float b = lum * mixf(1.04f, 0.92f, warm);
            const float lichen = smooth(0.42f, 0.62f, tile_fbm(u, v, 6, 4, 139u)) * crack * 0.55f;
            r = mixf(r, 0.52f, lichen);
            g = mixf(g, 0.5f, lichen);
            b = mixf(b, 0.28f, lichen);
            f.put(f.at(x, y), r, g, b, h);
        }
    }
}

void make_sand(Field& f) {
    const int n = f.n;
    const float inv = 1.0f / static_cast<float>(n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float u = static_cast<float>(x) * inv, v = static_cast<float>(y) * inv;
            const float warp = tile_fbm(u, v, 2, 4, 151u);
            const float phase = (v * 11.0f + u * 2.0f + warp * 1.1f) * kTwoPi;
            const float s = 0.5f + 0.5f * std::sin(phase + 0.6f * std::sin(phase));
            const float ripple = s * s;
            const float tone = tile_fbm(u, v, 3, 4, 157u) * 0.5f + 0.5f;
            const float g0 = grain(x, y, 163u);
            const float speck = unit_hash(hash3(x, y, 5, 167u));
            float lum = 0.86f + 0.1f * ripple + 0.07f * g0 + 0.05f * (tone - 0.5f);
            if (speck > 0.985f) lum *= 0.6f;
            else if (speck < 0.012f) lum *= 1.15f;
            const float r = 0.80f * lum, g = 0.70f * lum, b = 0.52f * lum * mixf(0.96f, 1.04f, tone);
            f.put(f.at(x, y), r, g, b, clamp01(0.25f + 0.55f * ripple + 0.1f * g0));
        }
    }
}

void make_snow(Field& f) {
    const int n = f.n;
    const float inv = 1.0f / static_cast<float>(n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float u = static_cast<float>(x) * inv, v = static_cast<float>(y) * inv;
            const float bump = tile_fbm(u, v, 3, 5, 181u) * 0.5f + 0.5f;
            const float drift = 0.5f + 0.5f * std::sin((v * 4.0f + u + tile_fbm(u, v, 2, 3, 191u)) * kTwoPi);
            const float h = clamp01(bump * 0.75f + drift * 0.2f + grain(x, y, 193u) * 0.03f);
            const float hollow = 1.0f - h;
            float r = mixf(0.95f, 0.78f, hollow * hollow);
            float g = mixf(0.96f, 0.84f, hollow * hollow);
            float b = mixf(0.98f, 0.95f, hollow * hollow);
            if (unit_hash(hash3(x, y, 9, 197u)) > 0.9965f) r = g = b = 1.0f;
            f.put(f.at(x, y), r, g, b, h);
        }
    }
}

float normal_strength(std::string_view name) {
    if (name == "builtin:rock") return 5.0f;
    if (name == "builtin:dirt") return 3.5f;
    if (name == "builtin:grass") return 2.5f;
    if (name == "builtin:sand") return 1.6f;
    return 1.4f;
}

} // namespace

TerrainPixels generate_builtin_terrain(std::string_view name, int size) {
    TerrainPixels out;
    if (!is_builtin_terrain_texture(name)) return out;
    int n = 16;
    while (n < size && n < 2048) n *= 2;
    Field f(n);
    if (name == "builtin:grass") make_grass(f);
    else if (name == "builtin:dirt") make_dirt(f);
    else if (name == "builtin:rock") make_rock(f);
    else if (name == "builtin:sand") make_sand(f);
    else make_snow(f);
    out.albedo = albedo_of(f);
    out.normal = normal_of(f, normal_strength(name));
    return out;
}

ParticlePixels generate_builtin_terrain_texture(std::string_view name, int size) {
    return generate_builtin_terrain(name, size).albedo;
}

ParticlePixels generate_builtin_terrain_normal(std::string_view name, int size) {
    return generate_builtin_terrain(name, size).normal;
}

} // namespace editor
} // namespace orchard
