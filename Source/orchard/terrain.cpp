#include <orchard/terrain.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>

namespace orchard {

namespace {

std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.7g", static_cast<double>(v));
    return buf;
}

std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '%' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
            out += buf;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string unescape(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out.push_back(static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16)));
            i += 2;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string vec(Vec3 v) {
    return num(v.x) + "," + num(v.y) + "," + num(v.z);
}

bool parse_vec(const std::string& s, Vec3& out) {
    float v[3] = {0.0f, 0.0f, 0.0f};
    std::size_t at = 0;
    for (int i = 0; i < 3; ++i) {
        const std::size_t comma = s.find(',', at);
        const std::string part = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        if (part.empty()) return false;
        char* end = nullptr;
        v[i] = std::strtof(part.c_str(), &end);
        if (end == part.c_str()) return false;
        if (comma == std::string::npos) {
            if (i != 2) return false;
            break;
        }
        at = comma + 1;
    }
    out = {v[0], v[1], v[2]};
    return true;
}

float to_float(const std::string& s, float fallback) {
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    return end == s.c_str() ? fallback : v;
}

std::uint32_t to_u32(const std::string& s) {
    return static_cast<std::uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
}

bool to_bool(const std::string& s) {
    return s == "1" || s == "true" || s == "yes" || s == "on";
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

float smooth01(float edge0, float edge1, float x) {
    if (edge1 <= edge0) return x >= edge1 ? 1.0f : 0.0f;
    const float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

std::uint32_t hash2(int x, int z, std::uint32_t seed) {
    return hash32(static_cast<std::uint32_t>(x) * 0x8da6b343u ^ hash32(static_cast<std::uint32_t>(z) * 0xd8163841u ^ hash32(seed + 0x9e3779b9u)));
}

float unit(std::uint32_t h) {
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

const float gradients[16][2] = {
    {1.0f, 0.0f}, {0.9238795f, 0.3826834f}, {0.7071068f, 0.7071068f}, {0.3826834f, 0.9238795f},
    {0.0f, 1.0f}, {-0.3826834f, 0.9238795f}, {-0.7071068f, 0.7071068f}, {-0.9238795f, 0.3826834f},
    {-1.0f, 0.0f}, {-0.9238795f, -0.3826834f}, {-0.7071068f, -0.7071068f}, {-0.3826834f, -0.9238795f},
    {0.0f, -1.0f}, {0.3826834f, -0.9238795f}, {0.7071068f, -0.7071068f}, {0.9238795f, -0.3826834f}};

float grad_dot(int ix, int iz, std::uint32_t seed, float dx, float dz) {
    const float* g = gradients[hash2(ix, iz, seed) & 15u];
    return g[0] * dx + g[1] * dz;
}

float signed_noise(float x, float z, std::uint32_t seed) {
    const float fx = std::floor(x), fz = std::floor(z);
    const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
    const float dx = x - fx, dz = z - fz;
    const float u = dx * dx * dx * (dx * (dx * 6.0f - 15.0f) + 10.0f);
    const float v = dz * dz * dz * (dz * (dz * 6.0f - 15.0f) + 10.0f);
    const float a = grad_dot(ix, iz, seed, dx, dz);
    const float b = grad_dot(ix + 1, iz, seed, dx - 1.0f, dz);
    const float c = grad_dot(ix, iz + 1, seed, dx, dz - 1.0f);
    const float d = grad_dot(ix + 1, iz + 1, seed, dx - 1.0f, dz - 1.0f);
    return clampf(lerpf(lerpf(a, b, u), lerpf(c, d, u), v) * 1.4142135f, -1.0f, 1.0f);
}

float signed_fbm(float x, float z, std::uint32_t seed, int octaves, float gain, float lacunarity) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, f = 1.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * signed_noise(x * f, z * f, seed + static_cast<std::uint32_t>(o) * 1013u);
        norm += amp;
        amp *= gain;
        f *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

float ridged(float x, float z, std::uint32_t seed, int octaves, float gain) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, f = 1.0f, weight = 1.0f;
    for (int o = 0; o < octaves; ++o) {
        float n = 1.0f - std::fabs(signed_noise(x * f, z * f, seed + static_cast<std::uint32_t>(o) * 7919u));
        n *= n;
        n *= weight;
        weight = clampf(n * 2.0f, 0.0f, 1.0f);
        sum += n * amp;
        norm += amp;
        amp *= gain;
        f *= 2.0f;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

float terrace(float h, float steps, float sharpness) {
    const float s = h * steps;
    const float base = std::floor(s);
    const float frac = s - base;
    const float shaped = smooth01(0.5f - 0.5f * sharpness, 0.5f + 0.5f * sharpness, frac);
    return (base + lerpf(frac, shaped, sharpness)) / steps;
}

void normalize01(std::vector<float>& v) {
    if (v.empty()) return;
    const auto mm = std::minmax_element(v.begin(), v.end());
    const float lo = *mm.first, hi = *mm.second;
    if (hi - lo < 1e-9f) {
        std::fill(v.begin(), v.end(), 0.0f);
        return;
    }
    const float k = 1.0f / (hi - lo);
    for (float& x : v) x = (x - lo) * k;
}

Quat axis_angle(Vec3 axis, float radians_) {
    const Vec3 a = normalize(axis);
    const float s = std::sin(radians_ * 0.5f);
    return normalize(Quat{a.x * s, a.y * s, a.z * s, std::cos(radians_ * 0.5f)});
}

Quat arc(Vec3 from, Vec3 to) {
    const Vec3 a = normalize(from), b = normalize(to);
    const float d = dot(a, b);
    if (d > 0.999999f) return Quat{};
    if (d < -0.999999f) {
        Vec3 axis = cross({1.0f, 0.0f, 0.0f}, a);
        if (length(axis) < 1e-4f) axis = cross({0.0f, 0.0f, 1.0f}, a);
        return axis_angle(axis, pi);
    }
    const Vec3 c = cross(a, b);
    return normalize(Quat{c.x, c.y, c.z, 1.0f + d});
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

void put_f32(std::vector<std::uint8_t>& out, float f) {
    std::uint32_t v = 0;
    std::memcpy(&v, &f, 4);
    put_u32(out, v);
}

std::uint32_t get_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

float get_f32(const std::uint8_t* p) {
    const std::uint32_t v = get_u32(p);
    float f = 0.0f;
    std::memcpy(&f, &v, 4);
    return f;
}

std::uint32_t be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string extension(const std::string& name) {
    const std::size_t dot = name.find_last_of('.');
    const std::size_t slash = name.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return std::string();
    return lower(name.substr(dot));
}

struct BitReader {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
    std::uint32_t buf = 0;
    int count = 0;
    bool bad = false;

    std::uint32_t bits(int k) {
        while (count < k) {
            if (pos >= size) {
                bad = true;
                return 0;
            }
            buf |= static_cast<std::uint32_t>(data[pos++]) << count;
            count += 8;
        }
        const std::uint32_t v = k == 0 ? 0u : (buf & ((1u << k) - 1u));
        buf = k >= 32 ? 0u : (buf >> k);
        count -= k;
        return v;
    }
};

struct Huffman {
    std::array<std::uint16_t, 16> counts{};
    std::array<std::uint16_t, 320> symbols{};
};

bool build_huffman(Huffman& h, const std::uint8_t* lengths, int n) {
    h.counts.fill(0);
    for (int i = 0; i < n; ++i) ++h.counts[lengths[i]];
    h.counts[0] = 0;
    int left = 1;
    for (int len = 1; len < 16; ++len) {
        left <<= 1;
        left -= h.counts[static_cast<std::size_t>(len)];
        if (left < 0) return false;
    }
    std::array<std::uint16_t, 16> offs{};
    for (int len = 1; len < 15; ++len) offs[static_cast<std::size_t>(len + 1)] = static_cast<std::uint16_t>(offs[static_cast<std::size_t>(len)] + h.counts[static_cast<std::size_t>(len)]);
    for (int i = 0; i < n; ++i)
        if (lengths[i]) h.symbols[offs[lengths[i]]++] = static_cast<std::uint16_t>(i);
    return true;
}

int huff_decode(BitReader& br, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= static_cast<int>(br.bits(1));
        if (br.bad) return -1;
        const int c = h.counts[static_cast<std::size_t>(len)];
        if (code - c < first) return h.symbols[static_cast<std::size_t>(index + (code - first))];
        index += c;
        first += c;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

bool inflate_codes(BitReader& br, std::vector<std::uint8_t>& out, const Huffman& lit, const Huffman& dist) {
    static const std::uint16_t lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const std::uint8_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const std::uint16_t dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const std::uint8_t dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = huff_decode(br, lit);
        if (sym < 0) return false;
        if (sym < 256) {
            out.push_back(static_cast<std::uint8_t>(sym));
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            const std::size_t len = lbase[sym] + br.bits(lext[sym]);
            const int ds = huff_decode(br, dist);
            if (ds < 0 || ds >= 30) return false;
            const std::size_t d = dbase[ds] + br.bits(dext[ds]);
            if (br.bad || d > out.size()) return false;
            const std::size_t from = out.size() - d;
            for (std::size_t i = 0; i < len; ++i) out.push_back(out[from + i]);
        }
    }
}

bool inflate_raw(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out) {
    BitReader br;
    br.data = data;
    br.size = size;
    int last = 0;
    do {
        last = static_cast<int>(br.bits(1));
        const int type = static_cast<int>(br.bits(2));
        if (br.bad) return false;
        if (type == 0) {
            br.buf = 0;
            br.count = 0;
            if (br.pos + 4 > br.size) return false;
            const std::size_t len = static_cast<std::size_t>(br.data[br.pos]) | (static_cast<std::size_t>(br.data[br.pos + 1]) << 8);
            const std::size_t nlen = static_cast<std::size_t>(br.data[br.pos + 2]) | (static_cast<std::size_t>(br.data[br.pos + 3]) << 8);
            br.pos += 4;
            if ((len ^ 0xffffu) != nlen || br.pos + len > br.size) return false;
            out.insert(out.end(), br.data + br.pos, br.data + br.pos + len);
            br.pos += len;
        } else if (type == 1) {
            static Huffman lit, dist;
            static std::once_flag once;
            std::call_once(once, [] {
                std::uint8_t l[288];
                for (int i = 0; i < 144; ++i) l[i] = 8;
                for (int i = 144; i < 256; ++i) l[i] = 9;
                for (int i = 256; i < 280; ++i) l[i] = 7;
                for (int i = 280; i < 288; ++i) l[i] = 8;
                build_huffman(lit, l, 288);
                std::uint8_t d[30];
                for (int i = 0; i < 30; ++i) d[i] = 5;
                build_huffman(dist, d, 30);
            });
            if (!inflate_codes(br, out, lit, dist)) return false;
        } else if (type == 2) {
            static const std::uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            const int nlen = static_cast<int>(br.bits(5)) + 257;
            const int ndist = static_cast<int>(br.bits(5)) + 1;
            const int ncode = static_cast<int>(br.bits(4)) + 4;
            if (br.bad || nlen > 286 || ndist > 30) return false;
            std::uint8_t lengths[320] = {};
            for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<std::uint8_t>(br.bits(3));
            Huffman lencode;
            if (!build_huffman(lencode, lengths, 19)) return false;
            int index = 0;
            std::memset(lengths, 0, sizeof(lengths));
            while (index < nlen + ndist) {
                const int sym = huff_decode(br, lencode);
                if (sym < 0) return false;
                if (sym < 16) {
                    lengths[index++] = static_cast<std::uint8_t>(sym);
                } else {
                    std::uint8_t value = 0;
                    int rep = 0;
                    if (sym == 16) {
                        if (index == 0) return false;
                        value = lengths[index - 1];
                        rep = 3 + static_cast<int>(br.bits(2));
                    } else if (sym == 17) {
                        rep = 3 + static_cast<int>(br.bits(3));
                    } else {
                        rep = 11 + static_cast<int>(br.bits(7));
                    }
                    if (index + rep > nlen + ndist) return false;
                    while (rep--) lengths[index++] = value;
                }
            }
            Huffman lit, dist;
            if (!build_huffman(lit, lengths, nlen)) return false;
            if (!build_huffman(dist, lengths + nlen, ndist)) return false;
            if (!inflate_codes(br, out, lit, dist)) return false;
        } else {
            return false;
        }
        if (br.bad) return false;
    } while (!last);
    return true;
}

bool inflate_zlib(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out) {
    if (size < 2 || (data[0] & 0x0f) != 8 || ((static_cast<unsigned>(data[0]) << 8) | data[1]) % 31 != 0) return false;
    if (data[1] & 0x20) return false;
    return inflate_raw(data + 2, size - 2, out);
}

bool fail(std::string* error, const std::string& what) {
    if (error) *error = what;
    return false;
}

std::uint8_t paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
    if (pb <= pc) return static_cast<std::uint8_t>(b);
    return static_cast<std::uint8_t>(c);
}

bool decode_png(const std::uint8_t* data, std::size_t size, HeightImage& out, std::string* error) {
    static const std::uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || std::memcmp(data, sig, 8) != 0) return fail(error, "not a PNG file");
    std::size_t at = 8;
    std::uint32_t w = 0, h = 0;
    int depth = 0, color = 0, interlace = 0;
    std::vector<std::uint8_t> idat, palette;
    while (at + 8 <= size) {
        const std::uint32_t len = be32(data + at);
        const std::uint8_t* type = data + at + 4;
        if (at + 12 + static_cast<std::size_t>(len) > size) return fail(error, "PNG is cut short");
        const std::uint8_t* body = data + at + 8;
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            w = be32(body);
            h = be32(body + 4);
            depth = body[8];
            color = body[9];
            interlace = body[12];
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            palette.assign(body, body + len);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), body, body + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        at += 12 + static_cast<std::size_t>(len);
    }
    if (w == 0 || h == 0 || w > 65536 || h > 65536) return fail(error, "PNG has no usable size");
    if (interlace) return fail(error, "interlaced PNG heightmaps aren't supported; save it without interlacing");
    int channels = 0;
    switch (color) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default: return fail(error, "unsupported PNG colour type");
    }
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return fail(error, "unsupported PNG bit depth");
    if ((color == 2 || color == 4 || color == 6) && depth < 8) return fail(error, "unsupported PNG bit depth");
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(h) * (static_cast<std::size_t>(w) * channels * depth / 8 + 2));
    if (!inflate_zlib(idat.data(), idat.size(), raw)) return fail(error, "PNG image data is corrupt");
    const std::size_t bits_pp = static_cast<std::size_t>(channels) * static_cast<std::size_t>(depth);
    const std::size_t stride = (static_cast<std::size_t>(w) * bits_pp + 7) / 8;
    const std::size_t bpp = std::max<std::size_t>(1, bits_pp / 8);
    if (raw.size() < static_cast<std::size_t>(h) * (stride + 1)) return fail(error, "PNG image data is too short");
    std::vector<std::uint8_t> prev(stride, 0), row(stride, 0);
    out.width = static_cast<int>(w);
    out.height = static_cast<int>(h);
    out.bits = depth;
    out.values.assign(static_cast<std::size_t>(w) * h, 0.0f);
    const float maxv = static_cast<float>((1u << depth) - 1u);
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint8_t* src = raw.data() + static_cast<std::size_t>(y) * (stride + 1);
        const int filter = src[0];
        for (std::size_t i = 0; i < stride; ++i) {
            const int a = i >= bpp ? row[i - bpp] : 0;
            const int b = prev[i];
            const int c = i >= bpp ? prev[i - bpp] : 0;
            const int x = src[1 + i];
            int v = x;
            switch (filter) {
            case 0: break;
            case 1: v = x + a; break;
            case 2: v = x + b; break;
            case 3: v = x + ((a + b) >> 1); break;
            case 4: v = x + paeth(a, b, c); break;
            default: return fail(error, "bad PNG filter");
            }
            row[i] = static_cast<std::uint8_t>(v & 0xff);
        }
        for (std::uint32_t x = 0; x < w; ++x) {
            auto sample = [&](int ch) -> float {
                const std::size_t idx = static_cast<std::size_t>(x) * channels + static_cast<std::size_t>(ch);
                if (depth == 16) return static_cast<float>((row[idx * 2] << 8) | row[idx * 2 + 1]);
                if (depth == 8) return static_cast<float>(row[idx]);
                const std::size_t bit = idx * static_cast<std::size_t>(depth);
                return static_cast<float>((row[bit / 8] >> (8 - depth - static_cast<int>(bit % 8))) & ((1 << depth) - 1));
            };
            float v = 0.0f;
            if (color == 3) {
                const std::size_t p = static_cast<std::size_t>(sample(0)) * 3;
                v = p + 2 < palette.size() ? (palette[p] + palette[p + 1] + palette[p + 2]) / (3.0f * 255.0f) : 0.0f;
            } else if (channels >= 3) {
                v = (sample(0) + sample(1) + sample(2)) / (3.0f * maxv);
            } else {
                v = sample(0) / maxv;
            }
            out.values[static_cast<std::size_t>(y) * w + x] = clampf(v, 0.0f, 1.0f);
        }
        std::swap(prev, row);
    }
    return true;
}

bool decode_pgm(const std::uint8_t* data, std::size_t size, HeightImage& out, std::string* error) {
    if (size < 3 || data[0] != 'P' || (data[1] != '5' && data[1] != '2')) return fail(error, "not a PGM file");
    const bool ascii = data[1] == '2';
    std::size_t at = 2;
    auto next = [&](long& v) {
        for (;;) {
            while (at < size && (data[at] == ' ' || data[at] == '\t' || data[at] == '\r' || data[at] == '\n')) ++at;
            if (at < size && data[at] == '#') {
                while (at < size && data[at] != '\n') ++at;
                continue;
            }
            break;
        }
        if (at >= size || data[at] < '0' || data[at] > '9') return false;
        v = 0;
        while (at < size && data[at] >= '0' && data[at] <= '9') v = v * 10 + (data[at++] - '0');
        return true;
    };
    long w = 0, h = 0, maxv = 0;
    if (!next(w) || !next(h) || !next(maxv) || w <= 0 || h <= 0 || maxv <= 0 || maxv > 65535 || w > 65536 || h > 65536) return fail(error, "bad PGM header");
    const std::size_t count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    out.width = static_cast<int>(w);
    out.height = static_cast<int>(h);
    out.bits = maxv > 255 ? 16 : 8;
    out.values.assign(count, 0.0f);
    if (ascii) {
        for (std::size_t i = 0; i < count; ++i) {
            long v = 0;
            if (!next(v)) return fail(error, "PGM is cut short");
            out.values[i] = clampf(static_cast<float>(v) / static_cast<float>(maxv), 0.0f, 1.0f);
        }
        return true;
    }
    ++at;
    const std::size_t bytes = maxv > 255 ? 2 : 1;
    if (at + count * bytes > size) return fail(error, "PGM is cut short");
    for (std::size_t i = 0; i < count; ++i) {
        const float v = bytes == 2 ? static_cast<float>((data[at + i * 2] << 8) | data[at + i * 2 + 1]) : static_cast<float>(data[at + i]);
        out.values[i] = clampf(v / static_cast<float>(maxv), 0.0f, 1.0f);
    }
    return true;
}

bool decode_raw(const std::uint8_t* data, std::size_t size, const std::string& ext, HeightImage& out, std::string* error) {
    auto side_of = [](std::size_t n) -> std::size_t {
        const std::size_t s = static_cast<std::size_t>(std::llround(std::sqrt(static_cast<double>(n))));
        return s * s == n ? s : 0;
    };
    if (ext == ".r32" || ext == ".f32") {
        const std::size_t side = size % 4 == 0 ? side_of(size / 4) : 0;
        if (!side) return fail(error, "raw float heightmaps must be square");
        out.width = out.height = static_cast<int>(side);
        out.bits = 32;
        out.values.resize(side * side);
        for (std::size_t i = 0; i < side * side; ++i) out.values[i] = get_f32(data + i * 4);
        return true;
    }
    std::size_t side = size % 2 == 0 ? side_of(size / 2) : 0;
    if (side) {
        out.width = out.height = static_cast<int>(side);
        out.bits = 16;
        out.values.resize(side * side);
        for (std::size_t i = 0; i < side * side; ++i) out.values[i] = static_cast<float>(data[i * 2] | (data[i * 2 + 1] << 8)) / 65535.0f;
        return true;
    }
    side = side_of(size);
    if (!side || ext == ".r16") return fail(error, "raw heightmaps must be square (16-bit little-endian)");
    out.width = out.height = static_cast<int>(side);
    out.bits = 8;
    out.values.resize(side * side);
    for (std::size_t i = 0; i < side * side; ++i) out.values[i] = data[i] / 255.0f;
    return true;
}

std::mutex& decoder_mutex() {
    static std::mutex m;
    return m;
}

HeightImageDecoder& decoder_slot() {
    static HeightImageDecoder d;
    return d;
}

void terrain_axes_box(const TerrainHeights& h, int x0, int z0, int x1, int z1, float& lo, float& hi) {
    lo = std::numeric_limits<float>::max();
    hi = -std::numeric_limits<float>::max();
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
            const float v = h.at(x, z);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    if (lo > hi) lo = hi = 0.0f;
}

std::vector<int> chunk_samples(int from, int to, int step) {
    std::vector<int> out;
    for (int i = from; i < to; i += step) out.push_back(i);
    out.push_back(to);
    return out;
}

} // namespace

std::string terrain_lines(std::size_t n, const Terrain& t) {
    const std::string id = std::to_string(n);
    std::string out = "terrain " + id + " size:" + vec(t.size) + " resolution:" + std::to_string(t.resolution) + " generator:" + escape(t.generator) + " seed:" + std::to_string(t.seed) + " roughness:" + num(t.roughness) + " friction:" + num(t.friction) + " restitution:" + num(t.restitution);
    if (!t.heightmap.empty()) out += " heightmap:" + escape(t.heightmap);
    if (!t.data.empty()) out += " data:" + escape(t.data);
    out += "\n";
    for (const TerrainLayer& l : t.layers) {
        out += "terrain-layer " + id;
        if (!l.texture.empty()) out += " texture:" + escape(l.texture);
        if (!l.normal_map.empty()) out += " normal-map:" + escape(l.normal_map);
        out += " color:" + vec(l.color) + " tiling:" + num(l.tiling) + " min-height:" + num(l.min_height) + " max-height:" + num(l.max_height) + " min-slope:" + num(l.min_slope) + " max-slope:" + num(l.max_slope) + " blend:" + num(l.blend) + "\n";
    }
    for (const TerrainScatter& s : t.scatter) {
        out += "terrain-scatter " + id;
        if (!s.model.empty()) out += " model:" + escape(s.model);
        out += " density:" + num(s.density) + " min-scale:" + num(s.min_scale) + " max-scale:" + num(s.max_scale) + " min-height:" + num(s.min_height) + " max-height:" + num(s.max_height) + " max-slope:" + num(s.max_slope) + " align:" + (s.align_to_slope ? "1" : "0") + " collide:" + (s.collide ? "1" : "0") + " draw-distance:" + num(s.draw_distance) + " seed:" + std::to_string(s.seed) + "\n";
    }
    return out;
}

bool is_terrain_line(const std::string& kind) {
    return kind == "terrain" || kind == "terrain-layer" || kind == "terrain-scatter";
}

bool read_terrain_line(const std::string& kind, std::istringstream& row, Terrain& t, std::string* error) {
    if (!is_terrain_line(kind)) return true;
    TerrainLayer layer;
    TerrainScatter scatter;
    for (std::string token; row >> token;) {
        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = token.substr(0, colon), v = token.substr(colon + 1);
        if (kind == "terrain") {
            if (key == "size") {
                if (!parse_vec(v, t.size)) {
                    if (error) *error = "bad terrain size '" + v + "'";
                    return false;
                }
            } else if (key == "resolution") t.resolution = clamp_terrain_resolution(std::atoi(v.c_str()));
            else if (key == "generator") t.generator = unescape(v);
            else if (key == "heightmap") t.heightmap = unescape(v);
            else if (key == "data") t.data = unescape(v);
            else if (key == "seed") t.seed = to_u32(v);
            else if (key == "roughness") t.roughness = to_float(v, t.roughness);
            else if (key == "friction") t.friction = to_float(v, t.friction);
            else if (key == "restitution" || key == "bounce") t.restitution = to_float(v, t.restitution);
        } else if (kind == "terrain-layer") {
            if (key == "texture") layer.texture = unescape(v);
            else if (key == "normal-map") layer.normal_map = unescape(v);
            else if (key == "color") {
                if (!parse_vec(v, layer.color)) {
                    if (error) *error = "bad terrain layer color '" + v + "'";
                    return false;
                }
            } else if (key == "tiling") layer.tiling = to_float(v, layer.tiling);
            else if (key == "min-height") layer.min_height = to_float(v, layer.min_height);
            else if (key == "max-height") layer.max_height = to_float(v, layer.max_height);
            else if (key == "min-slope") layer.min_slope = to_float(v, layer.min_slope);
            else if (key == "max-slope") layer.max_slope = to_float(v, layer.max_slope);
            else if (key == "blend") layer.blend = to_float(v, layer.blend);
        } else {
            if (key == "model") scatter.model = unescape(v);
            else if (key == "density") scatter.density = to_float(v, scatter.density);
            else if (key == "min-scale") scatter.min_scale = to_float(v, scatter.min_scale);
            else if (key == "max-scale") scatter.max_scale = to_float(v, scatter.max_scale);
            else if (key == "min-height") scatter.min_height = to_float(v, scatter.min_height);
            else if (key == "max-height") scatter.max_height = to_float(v, scatter.max_height);
            else if (key == "max-slope") scatter.max_slope = to_float(v, scatter.max_slope);
            else if (key == "align") scatter.align_to_slope = to_bool(v);
            else if (key == "collide") scatter.collide = to_bool(v);
            else if (key == "draw-distance") scatter.draw_distance = to_float(v, scatter.draw_distance);
            else if (key == "seed") scatter.seed = to_u32(v);
        }
    }
    if (kind == "terrain-layer") t.layers.push_back(layer);
    else if (kind == "terrain-scatter") t.scatter.push_back(scatter);
    return true;
}

int clamp_terrain_resolution(int resolution) {
    return std::clamp(resolution, terrain_min_resolution, terrain_max_resolution);
}

const std::vector<std::string>& terrain_generators() {
    static const std::vector<std::string> all = {"flat", "hills", "rolling", "mountains", "islands", "canyons", "plateaus", "dunes", "valleys"};
    return all;
}

const std::vector<std::string>& terrain_presets() {
    static const std::vector<std::string> all = {"flat", "hills", "mountains", "islands", "canyons", "dunes", "forest"};
    return all;
}

Terrain terrain_preset(const std::string& name) {
    Terrain t;
    TerrainLayer grass;
    grass.texture = "builtin:grass";
    grass.color = {1.0f, 1.0f, 1.0f};
    grass.tiling = 4.0f;
    TerrainLayer dirt;
    dirt.texture = "builtin:dirt";
    dirt.color = {1.0f, 1.0f, 1.0f};
    dirt.tiling = 3.0f;
    dirt.min_slope = 22.0f;
    dirt.blend = 0.15f;
    TerrainLayer rock;
    rock.texture = "builtin:rock";
    rock.color = {1.0f, 1.0f, 1.0f};
    rock.tiling = 6.0f;
    rock.min_slope = 38.0f;
    rock.blend = 0.12f;
    TerrainLayer snow;
    snow.texture = "builtin:snow";
    snow.color = {1.0f, 1.0f, 1.0f};
    snow.tiling = 5.0f;
    snow.min_height = 0.78f;
    snow.max_slope = 50.0f;
    snow.blend = 0.06f;
    TerrainLayer sand;
    sand.texture = "builtin:sand";
    sand.color = {1.0f, 1.0f, 1.0f};
    sand.tiling = 4.0f;
    sand.max_height = 0.12f;
    sand.blend = 0.04f;
    if (name == "flat") {
        t.generator = "flat";
        t.size = {100.0f, 10.0f, 100.0f};
        t.layers = {grass};
    } else if (name == "mountains") {
        t.generator = "mountains";
        t.size = {600.0f, 160.0f, 600.0f};
        t.resolution = 513;
        t.roughness = 0.6f;
        t.layers = {grass, rock, snow};
    } else if (name == "islands") {
        t.generator = "islands";
        t.size = {400.0f, 40.0f, 400.0f};
        t.resolution = 257;
        t.layers = {sand, grass, rock};
        t.layers[0].max_height = 1.0f;
        t.layers[1].min_height = 0.12f;
    } else if (name == "canyons") {
        t.generator = "canyons";
        t.size = {500.0f, 80.0f, 500.0f};
        t.resolution = 513;
        TerrainLayer red = sand;
        red.color = {1.0f, 0.62f, 0.5f};
        red.max_height = 1.0f;
        t.layers = {red, rock};
    } else if (name == "dunes") {
        t.generator = "dunes";
        t.size = {400.0f, 25.0f, 400.0f};
        t.resolution = 257;
        t.roughness = 0.35f;
        TerrainLayer s = sand;
        s.max_height = 1.0f;
        t.layers = {s};
    } else if (name == "forest") {
        t.generator = "hills";
        t.size = {1000.0f, 60.0f, 1000.0f};
        t.resolution = 1025;
        t.roughness = 0.45f;
        t.layers = {grass, dirt, rock};
        TerrainScatter trees;
        trees.model = "Assets/Models/tree.glb";
        trees.density = 0.012f;
        trees.min_scale = 0.8f;
        trees.max_scale = 1.4f;
        trees.max_height = 0.85f;
        trees.max_slope = 28.0f;
        trees.collide = true;
        trees.draw_distance = 400.0f;
        trees.seed = 1;
        TerrainScatter rocks;
        rocks.model = "Assets/Models/rock.glb";
        rocks.density = 0.002f;
        rocks.min_scale = 0.5f;
        rocks.max_scale = 2.0f;
        rocks.max_slope = 45.0f;
        rocks.align_to_slope = true;
        rocks.collide = true;
        rocks.draw_distance = 250.0f;
        rocks.seed = 2;
        trees.density = 0.007f;
        TerrainScatter pines = trees;
        pines.model = "Assets/Models/pine.glb";
        pines.density = 0.006f;
        pines.min_height = 0.25f;
        pines.max_height = 0.95f;
        pines.seed = 3;
        TerrainScatter bushes;
        bushes.model = "Assets/Models/bush.glb";
        bushes.density = 0.01f;
        bushes.min_scale = 0.6f;
        bushes.max_scale = 1.3f;
        bushes.max_slope = 30.0f;
        bushes.max_height = 0.8f;
        bushes.collide = false;
        bushes.draw_distance = 120.0f;
        bushes.seed = 4;
        t.scatter = {trees, pines, rocks, bushes};
    } else {
        t.generator = "hills";
        t.size = {200.0f, 25.0f, 200.0f};
        t.resolution = 257;
        t.layers = {grass, dirt, rock};
    }
    return t;
}

bool TerrainHeights::valid() const {
    return width >= 2 && depth >= 2 && heights.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);
}

void TerrainHeights::resize(int w, int d, float fill) {
    width = std::max(0, w);
    depth = std::max(0, d);
    heights.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(depth), fill);
    touch_all();
}

void TerrainHeights::set_size(Vec3 s) {
    size = {std::max(1e-3f, s.x), s.y, std::max(1e-3f, s.z)};
    touch_all();
}

float TerrainHeights::at(int x, int z) const {
    if (heights.empty()) return 0.0f;
    x = std::clamp(x, 0, width - 1);
    z = std::clamp(z, 0, depth - 1);
    return heights[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
}

void TerrainHeights::set(int x, int z, float h) {
    if (x < 0 || z < 0 || x >= width || z >= depth) return;
    heights[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = h;
}

float TerrainHeights::spacing_x() const {
    return width > 1 ? size.x / static_cast<float>(width - 1) : size.x;
}

float TerrainHeights::spacing_z() const {
    return depth > 1 ? size.z / static_cast<float>(depth - 1) : size.z;
}

Vec2 TerrainHeights::to_grid(float x, float z) const {
    return {(x + size.x * 0.5f) / spacing_x(), (z + size.z * 0.5f) / spacing_z()};
}

Vec3 TerrainHeights::local_position(int x, int z) const {
    return {static_cast<float>(x) * spacing_x() - size.x * 0.5f, at(x, z), static_cast<float>(z) * spacing_z() - size.z * 0.5f};
}

float TerrainHeights::height_at(float x, float z) const {
    if (!valid()) return 0.0f;
    const Vec2 g = to_grid(x, z);
    const int c = std::clamp(static_cast<int>(std::floor(g.x)), 0, width - 2);
    const int r = std::clamp(static_cast<int>(std::floor(g.y)), 0, depth - 2);
    const float u = clampf(g.x - static_cast<float>(c), 0.0f, 1.0f);
    const float v = clampf(g.y - static_cast<float>(r), 0.0f, 1.0f);
    const float h00 = at(c, r), h10 = at(c + 1, r), h01 = at(c, r + 1), h11 = at(c + 1, r + 1);
    if (u + v <= 1.0f) return h00 + (h10 - h00) * u + (h01 - h00) * v;
    return h11 + (h01 - h11) * (1.0f - u) + (h10 - h11) * (1.0f - v);
}

Vec3 TerrainHeights::grid_normal(int x, int z) const {
    const float dx = (at(x + 1, z) - at(x - 1, z)) / (spacing_x() * static_cast<float>((std::min(x + 1, width - 1)) - std::max(x - 1, 0)));
    const float dz = (at(x, z + 1) - at(x, z - 1)) / (spacing_z() * static_cast<float>((std::min(z + 1, depth - 1)) - std::max(z - 1, 0)));
    return normalize(Vec3{-dx, 1.0f, -dz});
}

Vec3 TerrainHeights::normal_at(float x, float z) const {
    if (!valid()) return {0.0f, 1.0f, 0.0f};
    const Vec2 g = to_grid(x, z);
    const int c = std::clamp(static_cast<int>(std::floor(g.x)), 0, width - 2);
    const int r = std::clamp(static_cast<int>(std::floor(g.y)), 0, depth - 2);
    const float u = clampf(g.x - static_cast<float>(c), 0.0f, 1.0f);
    const float v = clampf(g.y - static_cast<float>(r), 0.0f, 1.0f);
    const Vec3 n00 = grid_normal(c, r), n10 = grid_normal(c + 1, r), n01 = grid_normal(c, r + 1), n11 = grid_normal(c + 1, r + 1);
    const Vec3 a = n00 * (1.0f - u) + n10 * u;
    const Vec3 b = n01 * (1.0f - u) + n11 * u;
    return normalize(a * (1.0f - v) + b * v);
}

float TerrainHeights::slope_at(float x, float z) const {
    const Vec3 n = normal_at(x, z);
    return degrees(std::acos(clampf(n.y, -1.0f, 1.0f)));
}

float TerrainHeights::min_height() const {
    return heights.empty() ? 0.0f : *std::min_element(heights.begin(), heights.end());
}

float TerrainHeights::max_height() const {
    return heights.empty() ? 0.0f : *std::max_element(heights.begin(), heights.end());
}

bool TerrainHeights::contains(float x, float z) const {
    return x >= -size.x * 0.5f && x <= size.x * 0.5f && z >= -size.z * 0.5f && z <= size.z * 0.5f;
}

void TerrainHeights::touch(int x0, int z0, int x1, int z1) {
    ++revision;
    dirty.push_back({std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), revision});
    if (dirty.size() > 256) {
        dirty.erase(dirty.begin(), dirty.begin() + 128);
        oldest_dirty = dirty.front().revision;
    }
}

void TerrainHeights::touch_all() {
    ++revision;
    dirty.clear();
    dirty.push_back({0, 0, std::max(0, width - 1), std::max(0, depth - 1), revision});
    oldest_dirty = revision;
}

std::vector<std::uint8_t> TerrainHeights::encode() const {
    std::vector<std::uint8_t> out;
    out.reserve(36 + heights.size() * 4);
    out.push_back('O');
    out.push_back('R');
    out.push_back('H');
    out.push_back('T');
    put_u32(out, 1);
    put_u32(out, static_cast<std::uint32_t>(width));
    put_u32(out, static_cast<std::uint32_t>(depth));
    put_f32(out, size.x);
    put_f32(out, size.y);
    put_f32(out, size.z);
    put_u32(out, 0);
    for (float h : heights) put_f32(out, h);
    return out;
}

bool TerrainHeights::decode(const std::uint8_t* data, std::size_t n, std::string* error) {
    if (n < 32 || std::memcmp(data, "ORHT", 4) != 0) return fail(error, "not an Orchard .heights file");
    const std::uint32_t version = get_u32(data + 4);
    if (version != 1) return fail(error, "unknown .heights version " + std::to_string(version));
    const std::uint32_t w = get_u32(data + 8), d = get_u32(data + 12);
    if (w < 2 || d < 2 || w > 65536 || d > 65536) return fail(error, "bad .heights size");
    const Vec3 s{get_f32(data + 16), get_f32(data + 20), get_f32(data + 24)};
    const std::uint32_t encoding = get_u32(data + 28);
    const std::size_t count = static_cast<std::size_t>(w) * d;
    std::vector<float> v(count);
    if (encoding == 0) {
        if (n < 32 + count * 4) return fail(error, ".heights file is cut short");
        for (std::size_t i = 0; i < count; ++i) v[i] = get_f32(data + 32 + i * 4);
    } else if (encoding == 1) {
        if (n < 40 + count * 2) return fail(error, ".heights file is cut short");
        const float lo = get_f32(data + 32), hi = get_f32(data + 36);
        for (std::size_t i = 0; i < count; ++i) v[i] = lo + (hi - lo) * static_cast<float>(data[40 + i * 2] | (data[41 + i * 2] << 8)) / 65535.0f;
    } else {
        return fail(error, "unknown .heights encoding");
    }
    width = static_cast<int>(w);
    depth = static_cast<int>(d);
    size = s;
    heights = std::move(v);
    touch_all();
    return true;
}

bool TerrainHeights::save(const std::string& path, std::string* error) const {
    const std::vector<std::uint8_t> bytes = encode();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return fail(error, "can't write " + path);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) return fail(error, "can't write " + path);
    return true;
}

bool TerrainHeights::load(const std::string& path, std::string* error) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes)) return fail(error, "can't read " + path);
    return decode(bytes.data(), bytes.size(), error);
}

void TerrainHeights::resample(int w, int d) {
    w = std::max(2, w);
    d = std::max(2, d);
    if (w == width && d == depth && valid()) return;
    if (!valid()) {
        resize(w, d, 0.0f);
        return;
    }
    std::vector<float> out(static_cast<std::size_t>(w) * static_cast<std::size_t>(d));
    for (int z = 0; z < d; ++z) {
        const float gz = static_cast<float>(z) * static_cast<float>(depth - 1) / static_cast<float>(d - 1);
        const int z0 = std::min(static_cast<int>(gz), depth - 2);
        const float fz = gz - static_cast<float>(z0);
        for (int x = 0; x < w; ++x) {
            const float gx = static_cast<float>(x) * static_cast<float>(width - 1) / static_cast<float>(w - 1);
            const int x0 = std::min(static_cast<int>(gx), width - 2);
            const float fx = gx - static_cast<float>(x0);
            const float a = lerpf(at(x0, z0), at(x0 + 1, z0), fx);
            const float b = lerpf(at(x0, z0 + 1), at(x0 + 1, z0 + 1), fx);
            out[static_cast<std::size_t>(z) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = lerpf(a, b, fz);
        }
    }
    width = w;
    depth = d;
    heights = std::move(out);
    touch_all();
}

bool raycast_terrain(const TerrainHeights& h, Vec3 origin, Vec3 direction, float max_distance, float& distance, Vec3* point, Vec3* normal) {
    if (!h.valid()) return false;
    const Vec3 dir = normalize(direction);
    if (length(dir) < 0.5f) return false;
    const float lo = h.min_height() - 1e-3f, hi = h.max_height() + 1e-3f;
    const Vec3 bmin{-h.size.x * 0.5f, lo, -h.size.z * 0.5f}, bmax{h.size.x * 0.5f, hi, h.size.z * 0.5f};
    float t0 = 0.0f, t1 = max_distance;
    const float o[3] = {origin.x, origin.y, origin.z}, dv[3] = {dir.x, dir.y, dir.z}, mn[3] = {bmin.x, bmin.y, bmin.z}, mx[3] = {bmax.x, bmax.y, bmax.z};
    for (int a = 0; a < 3; ++a) {
        if (std::fabs(dv[a]) < 1e-9f) {
            if (o[a] < mn[a] || o[a] > mx[a]) return false;
            continue;
        }
        float ta = (mn[a] - o[a]) / dv[a], tb = (mx[a] - o[a]) / dv[a];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    const float step = std::max(1e-3f, std::min(h.spacing_x(), h.spacing_z()) * 0.5f);
    auto above = [&](float t) {
        const Vec3 p = origin + dir * t;
        return p.y - h.height_at(p.x, p.z);
    };
    float prev_t = t0, prev = above(t0);
    if (prev <= 0.0f) {
        distance = t0;
        if (point) *point = origin + dir * t0;
        if (normal) *normal = h.normal_at((origin + dir * t0).x, (origin + dir * t0).z);
        return true;
    }
    for (float t = t0 + step;; t += step) {
        const float tc = std::min(t, t1);
        const float cur = above(tc);
        if (cur <= 0.0f) {
            float a = prev_t, b = tc;
            for (int i = 0; i < 24; ++i) {
                const float m = 0.5f * (a + b);
                if (above(m) > 0.0f) a = m;
                else b = m;
            }
            distance = b;
            const Vec3 p = origin + dir * b;
            if (point) *point = p;
            if (normal) *normal = h.normal_at(p.x, p.z);
            return true;
        }
        prev_t = tc;
        prev = cur;
        if (tc >= t1) break;
    }
    return false;
}

float terrain_noise(float x, float z, std::uint32_t seed) {
    return 0.5f + 0.5f * signed_noise(x, z, seed);
}

float terrain_fbm(float x, float z, std::uint32_t seed, int octaves, float gain, float lacunarity) {
    return 0.5f + 0.5f * signed_fbm(x, z, seed, std::max(1, octaves), gain, lacunarity);
}

void generate_heights(const Terrain& t, TerrainHeights& out) {
    const int res = clamp_terrain_resolution(t.resolution);
    out.width = res;
    out.depth = res;
    out.size = {std::max(1e-3f, t.size.x), t.size.y, std::max(1e-3f, t.size.z)};
    out.heights.assign(static_cast<std::size_t>(res) * static_cast<std::size_t>(res), 0.0f);
    const std::string gen = lower(t.generator);
    if (gen == "flat" || gen.empty()) {
        out.touch_all();
        return;
    }
    const float rough = clampf(t.roughness, 0.0f, 1.0f);
    const float base = clampf(std::max(t.size.x, t.size.z) / 150.0f, 1.5f, 64.0f);
    int octaves = 3 + static_cast<int>(std::lround(rough * 5.0f));
    while (octaves > 1 && base * std::pow(2.0f, static_cast<float>(octaves - 1)) > static_cast<float>(res) * 0.5f) --octaves;
    const float gain = 0.3f + 0.35f * rough;
    const std::uint32_t seed = hash32(t.seed * 0x9e3779b9u + 0x632be5abu);
    const float aspect = t.size.z / std::max(1e-3f, t.size.x);
    std::vector<float>& v = out.heights;
    for (int z = 0; z < res; ++z) {
        const float vz = static_cast<float>(z) / static_cast<float>(res - 1);
        for (int x = 0; x < res; ++x) {
            const float ux = static_cast<float>(x) / static_cast<float>(res - 1);
            const float px = ux * base, pz = vz * base * aspect;
            float h = 0.0f;
            if (gen == "rolling") {
                h = signed_fbm(px * 0.5f, pz * 0.5f, seed, std::max(2, octaves - 2), gain * 0.8f, 2.0f);
            } else if (gen == "mountains") {
                h = ridged(px, pz, seed, octaves, 0.45f + 0.2f * rough) * 0.8f + 0.2f * (0.5f + 0.5f * signed_fbm(px * 0.5f, pz * 0.5f, seed + 17u, 3, 0.5f, 2.0f));
            } else if (gen == "islands") {
                const float n = 0.5f + 0.5f * signed_fbm(px, pz, seed, octaves, gain, 2.0f);
                const float dx = ux * 2.0f - 1.0f, dz = vz * 2.0f - 1.0f;
                const float r = std::sqrt(dx * dx + dz * dz) + 0.25f * signed_noise(px * 0.7f, pz * 0.7f, seed + 3u);
                h = std::max(0.0f, n * 0.75f + 0.45f - smooth01(0.25f, 0.95f, r) * 1.1f);
            } else if (gen == "canyons") {
                const float wx = px + 0.6f * signed_noise(px * 0.5f, pz * 0.5f, seed + 5u);
                const float wz = pz + 0.6f * signed_noise(px * 0.5f + 9.1f, pz * 0.5f + 3.7f, seed + 6u);
                const float river = std::fabs(signed_fbm(wx * 0.35f, wz * 0.35f, seed + 7u, 3, 0.5f, 2.0f));
                const float cut = smooth01(0.02f, 0.16f + 0.08f * rough, river);
                const float top = 0.7f + 0.3f * (0.5f + 0.5f * signed_fbm(px, pz, seed, octaves, gain, 2.0f));
                h = terrace(lerpf(0.05f, top, cut), 5.0f, 0.55f);
            } else if (gen == "plateaus") {
                const float n = 0.5f + 0.5f * signed_fbm(px * 0.7f, pz * 0.7f, seed, octaves, gain, 2.0f);
                h = terrace(n, 4.0f, 0.8f) * 0.85f + 0.15f * n;
            } else if (gen == "dunes") {
                const float warp = signed_fbm(px * 0.6f, pz * 0.6f, seed + 11u, 3, 0.5f, 2.0f);
                const float ph = (px * 2.2f + pz * 0.9f + warp * 1.6f);
                const float wave = ph - std::floor(ph);
                const float crest = wave < 0.7f ? smooth01(0.0f, 0.7f, wave) : 1.0f - smooth01(0.7f, 1.0f, wave);
                h = crest * 0.75f + 0.25f * (0.5f + 0.5f * signed_fbm(px, pz, seed, std::max(2, octaves - 1), gain, 2.0f));
            } else if (gen == "valleys") {
                h = 1.0f - ridged(px * 0.8f, pz * 0.8f, seed, octaves, 0.5f);
                h = h * h;
            } else {
                h = signed_fbm(px, pz, seed, octaves, gain, 2.0f);
            }
            v[static_cast<std::size_t>(z) * static_cast<std::size_t>(res) + static_cast<std::size_t>(x)] = h;
        }
    }
    normalize01(v);
    if (gen == "mountains") for (float& h : v) h = std::pow(h, 1.6f);
    if (gen == "hills" || gen == "rolling") for (float& h : v) h = h * 0.9f + 0.05f;
    for (float& h : v) h *= t.size.y;
    out.touch_all();
}

void set_height_image_decoder(HeightImageDecoder decoder) {
    std::lock_guard<std::mutex> lock(decoder_mutex());
    decoder_slot() = std::move(decoder);
}

bool decode_height_image(const std::uint8_t* data, std::size_t size, const std::string& name, HeightImage& out, std::string* error) {
    if (!data || size == 0) return fail(error, "heightmap is empty");
    const std::string ext = extension(name);
    if (size >= 8 && data[0] == 137 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return decode_png(data, size, out, error);
    if (size >= 2 && data[0] == 'P' && (data[1] == '5' || data[1] == '2')) return decode_pgm(data, size, out, error);
    if (ext == ".raw" || ext == ".r16" || ext == ".r32" || ext == ".f32" || ext == ".r8") return decode_raw(data, size, ext, out, error);
    return fail(error, "unsupported heightmap format" + (ext.empty() ? std::string() : " " + ext) + " (use a 16-bit PNG, PGM or .r16)");
}

bool load_height_image(const std::string& path, HeightImage& out, std::string* error) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes)) return fail(error, "can't read " + path);
    std::string first;
    if (decode_height_image(bytes.data(), bytes.size(), path, out, &first)) return true;
    HeightImageDecoder d;
    {
        std::lock_guard<std::mutex> lock(decoder_mutex());
        d = decoder_slot();
    }
    std::string second;
    if (d && d(path, out, &second) && out.width > 0 && out.height > 0 && out.values.size() == static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height)) return true;
    return fail(error, second.empty() ? first : second);
}

void heights_from_image(const HeightImage& image, const Terrain& t, TerrainHeights& out) {
    const int res = clamp_terrain_resolution(t.resolution);
    out.width = res;
    out.depth = res;
    out.size = {std::max(1e-3f, t.size.x), t.size.y, std::max(1e-3f, t.size.z)};
    out.heights.assign(static_cast<std::size_t>(res) * static_cast<std::size_t>(res), 0.0f);
    if (image.width <= 0 || image.height <= 0 || image.values.size() < static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height)) {
        out.touch_all();
        return;
    }
    auto px = [&](int x, int y) {
        x = std::clamp(x, 0, image.width - 1);
        y = std::clamp(y, 0, image.height - 1);
        return image.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)];
    };
    for (int z = 0; z < res; ++z) {
        const float gy = static_cast<float>(z) * static_cast<float>(image.height - 1) / static_cast<float>(res - 1);
        const int y0 = static_cast<int>(std::floor(gy));
        const float fy = gy - static_cast<float>(y0);
        for (int x = 0; x < res; ++x) {
            const float gx = static_cast<float>(x) * static_cast<float>(image.width - 1) / static_cast<float>(res - 1);
            const int x0 = static_cast<int>(std::floor(gx));
            const float fx = gx - static_cast<float>(x0);
            const float a = lerpf(px(x0, y0), px(x0 + 1, y0), fx);
            const float b = lerpf(px(x0, y0 + 1), px(x0 + 1, y0 + 1), fx);
            out.heights[static_cast<std::size_t>(z) * static_cast<std::size_t>(res) + static_cast<std::size_t>(x)] = lerpf(a, b, fy) * t.size.y;
        }
    }
    out.touch_all();
}

bool load_heightmap(const Terrain& t, const std::string& path, TerrainHeights& out, std::string* error) {
    HeightImage image;
    if (!load_height_image(path, image, error)) return false;
    heights_from_image(image, t, out);
    return true;
}

std::string terrain_file_path(const std::string& root, const std::string& file) {
    if (file.empty()) return file;
    const bool absolute = file[0] == '/' || file[0] == '\\' || (file.size() > 1 && file[1] == ':');
    if (absolute || root.empty()) return file;
    const char last = root.back();
    return root + (last == '/' || last == '\\' ? "" : "/") + file;
}

bool build_terrain_heights(const Terrain& t, const std::string& root, TerrainHeights& out, std::string* error) {
    const int res = clamp_terrain_resolution(t.resolution);
    if (!t.data.empty()) {
        TerrainHeights loaded;
        std::string why;
        if (loaded.load(terrain_file_path(root, t.data), &why)) {
            const float old_y = loaded.size.y;
            loaded.resample(res, res);
            if (std::fabs(old_y) > 1e-6f && std::fabs(old_y - t.size.y) > 1e-6f) {
                const float k = t.size.y / old_y;
                for (float& h : loaded.heights) h *= k;
            }
            loaded.size = {std::max(1e-3f, t.size.x), t.size.y, std::max(1e-3f, t.size.z)};
            loaded.touch_all();
            out = std::move(loaded);
            return true;
        }
        if (error) *error = why;
    }
    if (!t.heightmap.empty()) {
        std::string why;
        if (load_heightmap(t, terrain_file_path(root, t.heightmap), out, &why)) return true;
        if (error) *error = why;
    }
    generate_heights(t, out);
    return t.data.empty() && t.heightmap.empty();
}

std::string terrain_signature(const Terrain& t) {
    return vec(t.size) + "|" + std::to_string(t.resolution) + "|" + t.generator + "|" + std::to_string(t.seed) + "|" + num(t.roughness) + "|" + t.heightmap + "|" + t.data;
}

std::string default_terrain_data_path(const std::string& entity_name) {
    std::string clean;
    for (char c : entity_name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        clean.push_back(ok ? c : (c == ' ' ? '_' : '-'));
    }
    if (clean.empty()) clean = "terrain";
    return "Assets/Terrain/" + clean + ".heights";
}

const char* to_string(TerrainBrushKind kind) {
    switch (kind) {
    case TerrainBrushKind::raise: return "raise";
    case TerrainBrushKind::lower: return "lower";
    case TerrainBrushKind::smooth: return "smooth";
    case TerrainBrushKind::flatten: return "flatten";
    case TerrainBrushKind::noise: return "noise";
    case TerrainBrushKind::set_height: return "set-height";
    }
    return "raise";
}

std::optional<TerrainBrushKind> parse_terrain_brush(const std::string& text) {
    const std::string s = lower(text);
    for (TerrainBrushKind k : terrain_brushes())
        if (s == to_string(k)) return k;
    if (s == "set_height" || s == "set") return TerrainBrushKind::set_height;
    return std::nullopt;
}

const std::vector<TerrainBrushKind>& terrain_brushes() {
    static const std::vector<TerrainBrushKind> all = {TerrainBrushKind::raise, TerrainBrushKind::lower, TerrainBrushKind::smooth, TerrainBrushKind::flatten, TerrainBrushKind::noise, TerrainBrushKind::set_height};
    return all;
}

float brush_weight(const TerrainBrush& b, float distance) {
    const float r = std::max(1e-4f, b.radius);
    if (distance >= r) return 0.0f;
    const float inner = r * (1.0f - clampf(b.falloff, 0.0f, 1.0f));
    if (distance <= inner) return 1.0f;
    return 1.0f - smooth01(inner, r, distance);
}

bool apply_brush(TerrainHeights& h, const TerrainBrush& brush, float x, float z, float dt) {
    if (!h.valid() || brush.radius <= 0.0f) return false;
    const float sx = h.spacing_x(), sz = h.spacing_z();
    const Vec2 g = h.to_grid(x, z);
    const int x0 = std::max(0, static_cast<int>(std::floor(g.x - brush.radius / sx)));
    const int x1 = std::min(h.width - 1, static_cast<int>(std::ceil(g.x + brush.radius / sx)));
    const int z0 = std::max(0, static_cast<int>(std::floor(g.y - brush.radius / sz)));
    const int z1 = std::min(h.depth - 1, static_cast<int>(std::ceil(g.y + brush.radius / sz)));
    if (x0 > x1 || z0 > z1) return false;
    std::vector<float> before;
    if (brush.kind == TerrainBrushKind::smooth) {
        before.reserve(static_cast<std::size_t>(x1 - x0 + 3) * static_cast<std::size_t>(z1 - z0 + 3));
        for (int zz = z0 - 1; zz <= z1 + 1; ++zz)
            for (int xx = x0 - 1; xx <= x1 + 1; ++xx) before.push_back(h.at(xx, zz));
    }
    const int bw = x1 - x0 + 3;
    auto old = [&](int xx, int zz) { return before[static_cast<std::size_t>(zz - z0 + 1) * static_cast<std::size_t>(bw) + static_cast<std::size_t>(xx - x0 + 1)]; };
    const float noise_scale = 1.0f / std::max(brush.radius * 0.3f, std::max(sx, sz) * 2.0f);
    bool changed = false;
    for (int zz = z0; zz <= z1; ++zz) {
        for (int xx = x0; xx <= x1; ++xx) {
            const Vec3 p = h.local_position(xx, zz);
            const float dx = p.x - x, dz = p.z - z;
            const float w = brush_weight(brush, std::sqrt(dx * dx + dz * dz));
            if (w <= 0.0f) continue;
            const float cur = p.y;
            float next = cur;
            switch (brush.kind) {
            case TerrainBrushKind::raise: next = cur + brush.strength * w * dt; break;
            case TerrainBrushKind::lower: next = cur - brush.strength * w * dt; break;
            case TerrainBrushKind::smooth: {
                float sum = 0.0f;
                for (int oz = -1; oz <= 1; ++oz)
                    for (int ox = -1; ox <= 1; ++ox) sum += old(std::clamp(xx + ox, 0, h.width - 1), std::clamp(zz + oz, 0, h.depth - 1));
                next = lerpf(cur, sum / 9.0f, clampf(brush.strength * dt, 0.0f, 1.0f) * w);
                break;
            }
            case TerrainBrushKind::flatten: next = lerpf(cur, brush.target, clampf(brush.strength * dt, 0.0f, 1.0f) * w); break;
            case TerrainBrushKind::noise: next = cur + signed_noise(p.x * noise_scale, p.z * noise_scale, brush.seed) * brush.strength * w * dt; break;
            case TerrainBrushKind::set_height: next = lerpf(cur, brush.target, w); break;
            }
            if (next != cur) {
                h.set(xx, zz, next);
                changed = true;
            }
        }
    }
    if (changed) h.touch(x0, z0, x1, z1);
    return changed;
}

Quat terrain_frame(Vec3 up) {
    if (length(up) < 1e-6f) return Quat{};
    return arc({0.0f, 1.0f, 0.0f}, up);
}

Mat4 terrain_matrix(const Mat4& entity_world, Vec3 up) {
    return entity_world * rotation(terrain_frame(up));
}

Vec3 terrain_to_local(const Mat4& terrain_world, Vec3 world_point) {
    Mat4 inv;
    if (!invert(terrain_world, inv)) return world_point;
    return mul_point(inv, world_point);
}

Vec4 terrain_layer_weights(const Terrain& t, float height01, float slope_degrees) {
    float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const std::size_t n = t.layers.size();
    if (n == 0) return {1.0f, 0.0f, 0.0f, 0.0f};
    float remaining = 1.0f;
    for (std::size_t i = n; i-- > 1;) {
        const TerrainLayer& l = t.layers[i];
        const float b = std::max(1e-4f, l.blend);
        const float bd = std::max(1.0f, l.blend * 50.0f);
        const float lo = l.min_height <= 0.0f ? 1.0f : smooth01(l.min_height - b, l.min_height, height01);
        const float hi = l.max_height >= 1.0f ? 1.0f : 1.0f - smooth01(l.max_height, l.max_height + b, height01);
        const float slo = l.min_slope <= 0.0f ? 1.0f : smooth01(l.min_slope - bd, l.min_slope, slope_degrees);
        const float shi = l.max_slope >= 90.0f ? 1.0f : 1.0f - smooth01(l.max_slope, l.max_slope + bd, slope_degrees);
        const float a = clampf(lo * hi * slo * shi, 0.0f, 1.0f) * remaining;
        if (i < 4) w[i] += a;
        remaining -= a;
    }
    w[0] += remaining;
    return {w[0], w[1], w[2], w[3]};
}

void terrain_layer_weights(const Terrain& t, float height01, float slope_degrees, float* out, std::size_t count) {
    if (!out || count == 0) return;
    std::fill(out, out + count, 0.0f);
    const std::size_t n = t.layers.size();
    if (n == 0) {
        out[0] = 1.0f;
        return;
    }
    float remaining = 1.0f;
    for (std::size_t i = n; i-- > 1;) {
        const TerrainLayer& l = t.layers[i];
        const float b = std::max(1e-4f, l.blend);
        const float bd = std::max(1.0f, l.blend * 50.0f);
        const float lo = l.min_height <= 0.0f ? 1.0f : smooth01(l.min_height - b, l.min_height, height01);
        const float hi = l.max_height >= 1.0f ? 1.0f : 1.0f - smooth01(l.max_height, l.max_height + b, height01);
        const float slo = l.min_slope <= 0.0f ? 1.0f : smooth01(l.min_slope - bd, l.min_slope, slope_degrees);
        const float shi = l.max_slope >= 90.0f ? 1.0f : 1.0f - smooth01(l.max_slope, l.max_slope + bd, slope_degrees);
        const float a = clampf(lo * hi * slo * shi, 0.0f, 1.0f) * remaining;
        if (i < count) out[i] += a;
        remaining -= a;
    }
    out[0] += remaining;
}

int terrain_splat_size(int grid) {
    int n = 64;
    while (n < (grid - 1) * 2 && n < 1024) n *= 2;
    return n;
}

namespace {

void splat_pixels(const Terrain& t, const TerrainHeights& h, TerrainSplat& out, int x0, int y0, int x1, int y1) {
    const float height_scale = std::fabs(h.size.y) > 1e-6f ? 1.0f / h.size.y : 0.0f;
    const std::size_t count = static_cast<std::size_t>(std::clamp(out.layers, 1, terrain_max_splat_layers));
    float w[terrain_max_splat_layers];
    for (int y = y0; y <= y1; ++y) {
        const float z = (static_cast<float>(y) + 0.5f) / static_cast<float>(out.height) * h.size.z - h.size.z * 0.5f;
        for (int x = x0; x <= x1; ++x) {
            const float lx = (static_cast<float>(x) + 0.5f) / static_cast<float>(out.width) * h.size.x - h.size.x * 0.5f;
            terrain_layer_weights(t, h.height_at(lx, z) * height_scale, h.slope_at(lx, z), w, count);
            const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(out.width) + static_cast<std::size_t>(x)) * 4;
            for (std::size_t k = 0; k < 8; ++k) {
                const float v = k < count ? w[k] : 0.0f;
                std::vector<std::uint8_t>& dst = out.rgba[k / 4];
                if (dst.empty()) continue;
                dst[i + k % 4] = static_cast<std::uint8_t>(std::lround(clampf(v, 0.0f, 1.0f) * 255.0f));
            }
        }
    }
}

} // namespace

void build_terrain_splat(const Terrain& t, const TerrainHeights& h, TerrainSplat& out, int width, int height) {
    out.layers = std::clamp(static_cast<int>(t.layers.size()), 1, terrain_max_splat_layers);
    out.width = width > 0 ? width : terrain_splat_size(h.width);
    out.height = height > 0 ? height : terrain_splat_size(h.depth);
    const std::size_t bytes = static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height) * 4;
    out.rgba[0].assign(bytes, 0);
    if (out.layers > 4) out.rgba[1].assign(bytes, 0);
    else out.rgba[1].clear();
    if (!h.valid()) return;
    splat_pixels(t, h, out, 0, 0, out.width - 1, out.height - 1);
}

bool update_terrain_splat(const Terrain& t, const TerrainHeights& h, TerrainSplat& out, int gx0, int gz0, int gx1, int gz1, int* px0, int* py0, int* px1, int* py1) {
    if (!h.valid() || out.width <= 0 || out.height <= 0 || out.rgba[0].empty()) return false;
    const float sx = static_cast<float>(out.width) / static_cast<float>(std::max(1, h.width - 1));
    const float sz = static_cast<float>(out.height) / static_cast<float>(std::max(1, h.depth - 1));
    const int x0 = std::clamp(static_cast<int>(std::floor(static_cast<float>(std::min(gx0, gx1) - 2) * sx)) - 1, 0, out.width - 1);
    const int x1 = std::clamp(static_cast<int>(std::ceil(static_cast<float>(std::max(gx0, gx1) + 2) * sx)) + 1, 0, out.width - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(static_cast<float>(std::min(gz0, gz1) - 2) * sz)) - 1, 0, out.height - 1);
    const int y1 = std::clamp(static_cast<int>(std::ceil(static_cast<float>(std::max(gz0, gz1) + 2) * sz)) + 1, 0, out.height - 1);
    splat_pixels(t, h, out, x0, y0, x1, y1);
    if (px0) *px0 = x0;
    if (py0) *py0 = y0;
    if (px1) *px1 = x1;
    if (py1) *py1 = y1;
    return true;
}

Vec3 terrain_layer_color(const Terrain& t, float height01, float slope_degrees, const std::vector<Vec3>* tints) {
    if (t.layers.empty()) return {0.08f, 0.16f, 0.045f};
    Vec3 c;
    float remaining = 1.0f;
    auto tinted = [&](std::size_t i) {
        Vec3 col = t.layers[i].color;
        if (tints && i < tints->size()) col = col * (*tints)[i];
        return col;
    };
    for (std::size_t i = t.layers.size(); i-- > 1;) {
        const TerrainLayer& l = t.layers[i];
        const float b = std::max(1e-4f, l.blend);
        const float bd = std::max(1.0f, l.blend * 50.0f);
        const float lo = l.min_height <= 0.0f ? 1.0f : smooth01(l.min_height - b, l.min_height, height01);
        const float hi = l.max_height >= 1.0f ? 1.0f : 1.0f - smooth01(l.max_height, l.max_height + b, height01);
        const float slo = l.min_slope <= 0.0f ? 1.0f : smooth01(l.min_slope - bd, l.min_slope, slope_degrees);
        const float shi = l.max_slope >= 90.0f ? 1.0f : 1.0f - smooth01(l.max_slope, l.max_slope + bd, slope_degrees);
        const float a = clampf(lo * hi * slo * shi, 0.0f, 1.0f) * remaining;
        c += tinted(i) * a;
        remaining -= a;
    }
    c += tinted(0) * remaining;
    return c;
}

void TerrainMesh::clear() {
    positions.clear();
    normals.clear();
    uvs.clear();
    weights.clear();
    colors.clear();
    indices.clear();
    bounds_min = bounds_max = Vec3{};
}

TerrainChunking terrain_chunking(const TerrainHeights& h, int chunk_quads) {
    TerrainChunking c;
    c.chunk_quads = std::max(1, chunk_quads);
    if (!h.valid()) return c;
    c.chunks_x = (h.width - 2) / c.chunk_quads + 1;
    c.chunks_z = (h.depth - 2) / c.chunk_quads + 1;
    c.lods = 1;
    while ((c.chunk_quads >> c.lods) >= 1 && c.lods < 6) ++c.lods;
    return c;
}

void terrain_chunk_bounds(const TerrainHeights& h, int cx, int cz, int chunk_quads, Vec3& lo, Vec3& hi) {
    const int cq = std::max(1, chunk_quads);
    const int x0 = std::clamp(cx * cq, 0, std::max(0, h.width - 1)), x1 = std::min(x0 + cq, h.width - 1);
    const int z0 = std::clamp(cz * cq, 0, std::max(0, h.depth - 1)), z1 = std::min(z0 + cq, h.depth - 1);
    float a = 0.0f, b = 0.0f;
    terrain_axes_box(h, x0, z0, x1, z1, a, b);
    const Vec3 p0 = h.local_position(x0, z0), p1 = h.local_position(x1, z1);
    lo = {p0.x, a, p0.z};
    hi = {p1.x, b, p1.z};
}

void build_terrain_chunk(const Terrain& t, const TerrainHeights& h, int cx, int cz, int lod, TerrainMesh& out, const TerrainMeshOptions& options) {
    out.clear();
    if (!h.valid()) return;
    const int cq = std::max(1, options.chunk_quads);
    const int step = 1 << std::clamp(lod, 0, 10);
    const int x0 = cx * cq, z0 = cz * cq;
    if (x0 >= h.width - 1 || z0 >= h.depth - 1 || x0 < 0 || z0 < 0) return;
    const int x1 = std::min(x0 + cq, h.width - 1), z1 = std::min(z0 + cq, h.depth - 1);
    const std::vector<int> xs = chunk_samples(x0, x1, step), zs = chunk_samples(z0, z1, step);
    const std::size_t nx = xs.size(), nz = zs.size();
    const float tiling = options.uv_tiling > 0.0f ? options.uv_tiling : (t.layers.empty() ? 8.0f : std::max(1e-3f, t.layers[0].tiling));
    const float height_scale = std::fabs(h.size.y) > 1e-6f ? 1.0f / h.size.y : 0.0f;
    const std::size_t skirt_count = options.skirts ? 2 * (nx + nz) : 0;
    out.positions.reserve(nx * nz + skirt_count);
    out.normals.reserve(nx * nz + skirt_count);
    out.uvs.reserve(nx * nz + skirt_count);
    out.weights.reserve(nx * nz + skirt_count);
    out.colors.reserve(nx * nz + skirt_count);
    out.indices.reserve((nx - 1) * (nz - 1) * 6 + skirt_count * 12);
    Vec3 lo{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 hi = lo * -1.0f;
    auto push = [&](Vec3 p, Vec3 n, float slope) {
        out.positions.push_back(p);
        out.normals.push_back(n);
        out.uvs.push_back({(p.x + h.size.x * 0.5f) / tiling, (p.z + h.size.z * 0.5f) / tiling});
        const float h01 = p.y * height_scale;
        out.weights.push_back(terrain_layer_weights(t, h01, slope));
        out.colors.push_back(terrain_layer_color(t, h01, slope, options.layer_tints));
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    };
    for (std::size_t j = 0; j < nz; ++j) {
        for (std::size_t i = 0; i < nx; ++i) {
            const Vec3 p = h.local_position(xs[i], zs[j]);
            const Vec3 n = h.grid_normal(xs[i], zs[j]);
            push(p, n, degrees(std::acos(clampf(n.y, -1.0f, 1.0f))));
        }
    }
    for (std::size_t j = 0; j + 1 < nz; ++j) {
        for (std::size_t i = 0; i + 1 < nx; ++i) {
            const auto a = static_cast<std::uint32_t>(j * nx + i);
            const auto b = static_cast<std::uint32_t>(j * nx + i + 1);
            const auto d = static_cast<std::uint32_t>((j + 1) * nx + i);
            const auto e = static_cast<std::uint32_t>((j + 1) * nx + i + 1);
            out.indices.insert(out.indices.end(), {a, d, b, b, d, e});
        }
    }
    if (options.skirts) {
        const float depth = options.skirt_depth > 0.0f ? options.skirt_depth : std::max(0.5f, static_cast<float>(step) * std::max(h.spacing_x(), h.spacing_z()) * 3.0f);
        auto skirt = [&](const std::vector<std::uint32_t>& edge) {
            const auto base = static_cast<std::uint32_t>(out.positions.size());
            for (std::uint32_t idx : edge) {
                Vec3 p = out.positions[idx];
                p.y -= depth;
                out.positions.push_back(p);
                out.normals.push_back(out.normals[idx]);
                out.uvs.push_back(out.uvs[idx]);
                out.weights.push_back(out.weights[idx]);
                out.colors.push_back(out.colors[idx]);
                lo.y = std::min(lo.y, p.y);
            }
            for (std::size_t k = 0; k + 1 < edge.size(); ++k) {
                const std::uint32_t a = edge[k], b = edge[k + 1], c = base + static_cast<std::uint32_t>(k), d = base + static_cast<std::uint32_t>(k + 1);
                out.indices.insert(out.indices.end(), {a, c, b, b, c, d, a, b, c, b, d, c});
            }
        };
        std::vector<std::uint32_t> edge;
        for (std::size_t i = 0; i < nx; ++i) edge.push_back(static_cast<std::uint32_t>(i));
        skirt(edge);
        edge.clear();
        for (std::size_t i = 0; i < nx; ++i) edge.push_back(static_cast<std::uint32_t>((nz - 1) * nx + i));
        skirt(edge);
        edge.clear();
        for (std::size_t j = 0; j < nz; ++j) edge.push_back(static_cast<std::uint32_t>(j * nx));
        skirt(edge);
        edge.clear();
        for (std::size_t j = 0; j < nz; ++j) edge.push_back(static_cast<std::uint32_t>(j * nx + nx - 1));
        skirt(edge);
    }
    out.bounds_min = lo;
    out.bounds_max = hi;
}

void build_terrain_mesh(const Terrain& t, const TerrainHeights& h, TerrainMesh& out, int lod, const TerrainMeshOptions& options) {
    TerrainMeshOptions whole = options;
    whole.chunk_quads = std::max(h.width, h.depth);
    whole.skirts = false;
    build_terrain_chunk(t, h, 0, 0, lod, out, whole);
}

std::vector<ScatterInstance> scatter_instances(const Terrain& t, const TerrainHeights& h, std::size_t limit) {
    std::vector<ScatterInstance> out;
    if (!h.valid()) return out;
    const float area = h.size.x * h.size.z;
    const float height_scale = std::fabs(h.size.y) > 1e-6f ? 1.0f / h.size.y : 0.0f;
    for (std::size_t si = 0; si < t.scatter.size(); ++si) {
        const TerrainScatter& sc = t.scatter[si];
        if (sc.model.empty() || sc.density <= 0.0f) continue;
        float cell = 1.0f / std::sqrt(sc.density);
        const std::size_t budget = limit > out.size() ? limit - out.size() : 0;
        if (budget == 0) break;
        if (area / (cell * cell) > static_cast<float>(budget)) cell = std::sqrt(area / static_cast<float>(budget));
        const int nx = std::max(1, static_cast<int>(std::ceil(h.size.x / cell)));
        const int nz = std::max(1, static_cast<int>(std::ceil(h.size.z / cell)));
        const std::uint32_t seed = hash32(t.seed * 0x9e3779b9u ^ hash32(sc.seed * 0x85ebca6bu + static_cast<std::uint32_t>(si) * 0xc2b2ae35u));
        const float lo_scale = std::min(sc.min_scale, sc.max_scale), hi_scale = std::max(sc.min_scale, sc.max_scale);
        for (int j = 0; j < nz; ++j) {
            for (int i = 0; i < nx; ++i) {
                const std::uint32_t r0 = hash2(i, j, seed);
                const std::uint32_t r1 = hash32(r0 ^ 0x68e31da4u);
                const std::uint32_t r2 = hash32(r1 ^ 0xb5297a4du);
                const std::uint32_t r3 = hash32(r2 ^ 0x1b56c4e9u);
                const float x = (static_cast<float>(i) + unit(r0)) * cell - h.size.x * 0.5f;
                const float z = (static_cast<float>(j) + unit(r1)) * cell - h.size.z * 0.5f;
                if (!h.contains(x, z)) continue;
                const float y = h.height_at(x, z);
                const float h01 = y * height_scale;
                if (h01 < sc.min_height || h01 > sc.max_height) continue;
                const Vec3 n = h.normal_at(x, z);
                const float slope = degrees(std::acos(clampf(n.y, -1.0f, 1.0f)));
                if (slope > sc.max_slope) continue;
                ScatterInstance inst;
                inst.model = sc.model;
                inst.scatter = static_cast<std::uint32_t>(si);
                inst.position = {x, y, z};
                inst.yaw = unit(r2) * 360.0f;
                const Quat yaw = axis_angle({0.0f, 1.0f, 0.0f}, radians(inst.yaw));
                inst.rotation = sc.align_to_slope ? normalize(arc({0.0f, 1.0f, 0.0f}, n) * yaw) : yaw;
                inst.scale = lerpf(lo_scale, hi_scale, unit(r3));
                inst.collide = sc.collide;
                inst.draw_distance = sc.draw_distance;
                out.push_back(std::move(inst));
                if (out.size() >= limit) return out;
            }
        }
    }
    return out;
}

Mat4 scatter_matrix(const Mat4& terrain_world, const ScatterInstance& s) {
    return terrain_world * trs(s.position, s.rotation, {s.scale, s.scale, s.scale});
}

} // namespace orchard
