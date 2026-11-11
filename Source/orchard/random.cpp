#include <orchard/random.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace orchard {

namespace {

std::uint64_t splitmix(std::uint64_t& x) {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

std::uint64_t rotl(std::uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
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

std::uint32_t lattice(int x, int y, int z, std::uint32_t seed) {
    std::uint32_t h = seed * 0x27d4eb2du;
    h ^= static_cast<std::uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
    h ^= static_cast<std::uint32_t>(z) * 0xcb1ab31fu;
    return hash32(h);
}

} // namespace

std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

std::uint64_t hash64(std::uint64_t x) {
    return splitmix(x);
}

std::uint64_t hash_text(const std::string& text) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::uint64_t random_seed_from_time() {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return hash64(static_cast<std::uint64_t>(now));
}

Random::Random() {
    seed(random_seed_from_time());
}

Random::Random(std::uint64_t s) {
    seed(s);
}

void Random::seed(std::uint64_t s) {
    m_seed = s;
    std::uint64_t x = s;
    for (std::uint64_t& v : m_state) v = splitmix(x);
    m_has_spare = false;
}

std::uint64_t Random::next() {
    const std::uint64_t result = rotl(m_state[1] * 5, 7) * 9;
    const std::uint64_t t = m_state[1] << 17;
    m_state[2] ^= m_state[0];
    m_state[3] ^= m_state[1];
    m_state[1] ^= m_state[2];
    m_state[0] ^= m_state[3];
    m_state[2] ^= t;
    m_state[3] = rotl(m_state[3], 45);
    return result;
}

double Random::unit() {
    return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
}

double Random::range(double low, double high) {
    if (high < low) std::swap(low, high);
    return low + (high - low) * unit();
}

long long Random::integer(long long low, long long high) {
    if (high < low) std::swap(low, high);
    const unsigned long long span = static_cast<unsigned long long>(high - low) + 1ull;
    if (span == 0) return static_cast<long long>(next());
    const unsigned long long limit = ~0ull - (~0ull % span);
    unsigned long long r = next();
    while (r >= limit) r = next();
    return low + static_cast<long long>(r % span);
}

std::size_t Random::index(std::size_t size) {
    if (size <= 1) return 0;
    return static_cast<std::size_t>(integer(0, static_cast<long long>(size) - 1));
}

bool Random::chance(double probability) {
    return unit() < probability;
}

double Random::gaussian(double mean, double deviation) {
    if (m_has_spare) {
        m_has_spare = false;
        return mean + deviation * m_spare;
    }
    double u = 0.0, v = 0.0, s = 0.0;
    do {
        u = unit() * 2.0 - 1.0;
        v = unit() * 2.0 - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);
    const double m = std::sqrt(-2.0 * std::log(s) / s);
    m_spare = v * m;
    m_has_spare = true;
    return mean + deviation * u * m;
}

double Random::exponential(double rate) {
    if (rate <= 0.0) return 0.0;
    return -std::log(1.0 - unit()) / rate;
}

double Random::sign() {
    return (next() & 1u) ? 1.0 : -1.0;
}

double Random::angle() {
    return unit() * 360.0;
}

Vec3 Random::on_sphere(float radius) {
    const double z = range(-1.0, 1.0);
    const double a = unit() * 6.283185307179586;
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    return Vec3{static_cast<float>(r * std::cos(a)), static_cast<float>(z), static_cast<float>(r * std::sin(a))} * radius;
}

Vec3 Random::in_sphere(float radius) {
    return on_sphere(radius * static_cast<float>(std::cbrt(unit())));
}

Vec3 Random::on_circle(float radius) {
    const double a = unit() * 6.283185307179586;
    return Vec3{static_cast<float>(std::cos(a)) * radius, 0.0f, static_cast<float>(std::sin(a)) * radius};
}

Vec3 Random::in_circle(float radius) {
    return on_circle(radius * static_cast<float>(std::sqrt(unit())));
}

Vec3 Random::in_box(Vec3 low, Vec3 high) {
    return Vec3{rangef(low.x, high.x), rangef(low.y, high.y), rangef(low.z, high.z)};
}

Vec3 Random::direction_in_cone(Vec3 axis, float deg) {
    const Vec3 a = length(axis) > 1e-6f ? normalize(axis) : Vec3{0.0f, 0.0f, 1.0f};
    const double cmin = std::cos(static_cast<double>(radians(std::clamp(deg, 0.0f, 180.0f))));
    const double z = range(cmin, 1.0);
    const double phi = unit() * 6.283185307179586;
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    const Vec3 helper = std::fabs(a.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    const Vec3 u = normalize(cross(helper, a));
    const Vec3 v = cross(a, u);
    return normalize(u * static_cast<float>(r * std::cos(phi)) + v * static_cast<float>(r * std::sin(phi)) + a * static_cast<float>(z));
}

std::size_t Random::weighted(const std::vector<double>& weights) {
    double total = 0.0;
    for (double w : weights) total += std::max(0.0, w);
    if (total <= 0.0) return weights.empty() ? 0 : index(weights.size());
    double r = unit() * total;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        r -= std::max(0.0, weights[i]);
        if (r <= 0.0) return i;
    }
    return weights.size() - 1;
}

std::string Random::text(std::size_t length, const std::string& alphabet) {
    std::string out;
    if (alphabet.empty()) return out;
    for (std::size_t i = 0; i < length; ++i) out += alphabet[index(alphabet.size())];
    return out;
}

Random& global_random() {
    static Random r;
    return r;
}

float noise(float x, float y, float z, std::uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)), zi = static_cast<int>(std::floor(z));
    const float xf = x - static_cast<float>(xi), yf = y - static_cast<float>(yi), zf = z - static_cast<float>(zi);
    const float u = fade(xf), v = fade(yf), w = fade(zf);
    auto g = [&](int dx, int dy, int dz) { return grad(lattice(xi + dx, yi + dy, zi + dz, seed), xf - static_cast<float>(dx), yf - static_cast<float>(dy), zf - static_cast<float>(dz)); };
    const float x00 = lerpf(g(0, 0, 0), g(1, 0, 0), u);
    const float x10 = lerpf(g(0, 1, 0), g(1, 1, 0), u);
    const float x01 = lerpf(g(0, 0, 1), g(1, 0, 1), u);
    const float x11 = lerpf(g(0, 1, 1), g(1, 1, 1), u);
    const float r = lerpf(lerpf(x00, x10, v), lerpf(x01, x11, v), w);
    return std::clamp(r * 0.9f, -1.0f, 1.0f);
}

float value_noise(float x, float y, float z, std::uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)), zi = static_cast<int>(std::floor(z));
    const float u = fade(x - static_cast<float>(xi)), v = fade(y - static_cast<float>(yi)), w = fade(z - static_cast<float>(zi));
    auto c = [&](int dx, int dy, int dz) { return static_cast<float>(lattice(xi + dx, yi + dy, zi + dz, seed ^ 0x51ed270bu) & 0xffffffu) / 8388607.5f - 1.0f; };
    return lerpf(lerpf(lerpf(c(0, 0, 0), c(1, 0, 0), u), lerpf(c(0, 1, 0), c(1, 1, 0), u), v), lerpf(lerpf(c(0, 0, 1), c(1, 0, 1), u), lerpf(c(0, 1, 1), c(1, 1, 1), u), v), w);
}

float fractal_noise(float x, float y, float z, int octaves, std::uint32_t seed, float lacunarity, float gain) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, f = 1.0f;
    for (int i = 0; i < std::clamp(octaves, 1, 16); ++i) {
        sum += noise(x * f, y * f, z * f, seed + static_cast<std::uint32_t>(i) * 101u) * amp;
        norm += amp;
        amp *= gain;
        f *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

float ridged_noise(float x, float y, float z, int octaves, std::uint32_t seed) {
    float sum = 0.0f, amp = 0.5f, f = 1.0f, norm = 0.0f;
    for (int i = 0; i < std::clamp(octaves, 1, 16); ++i) {
        const float n = 1.0f - std::fabs(noise(x * f, y * f, z * f, seed + static_cast<std::uint32_t>(i) * 131u));
        sum += n * n * amp;
        norm += amp;
        amp *= 0.5f;
        f *= 2.0f;
    }
    return norm > 0.0f ? sum / norm * 2.0f - 1.0f : 0.0f;
}

float cellular_noise(float x, float y, float z, std::uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)), zi = static_cast<int>(std::floor(z));
    float best = 1e9f;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const std::uint32_t h = lattice(xi + dx, yi + dy, zi + dz, seed ^ 0x9e3779b9u);
                const float px = static_cast<float>(xi + dx) + static_cast<float>(h & 1023u) / 1023.0f;
                const float py = static_cast<float>(yi + dy) + static_cast<float>((h >> 10) & 1023u) / 1023.0f;
                const float pz = static_cast<float>(zi + dz) + static_cast<float>((h >> 20) & 1023u) / 1023.0f;
                const float d = (px - x) * (px - x) + (py - y) * (py - y) + (pz - z) * (pz - z);
                best = std::min(best, d);
            }
    return std::clamp(std::sqrt(best), 0.0f, 1.0f) * 2.0f - 1.0f;
}

} // namespace orchard
