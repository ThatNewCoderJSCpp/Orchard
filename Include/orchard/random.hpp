#ifndef ORCHARD_RANDOM_HPP
#define ORCHARD_RANDOM_HPP

#include "math.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace orchard {

class Random {
public:
    Random();
    explicit Random(std::uint64_t seed);

    void seed(std::uint64_t seed);
    std::uint64_t seed_value() const { return m_seed; }
    std::uint64_t next();
    std::uint32_t next32() { return static_cast<std::uint32_t>(next() >> 32); }

    double unit();
    double range(double low, double high);
    float rangef(float low, float high) { return static_cast<float>(range(low, high)); }
    long long integer(long long low, long long high);
    std::size_t index(std::size_t size);
    bool chance(double probability);
    double gaussian(double mean = 0.0, double deviation = 1.0);
    double exponential(double rate = 1.0);
    double sign();
    double angle();
    Vec3 in_sphere(float radius = 1.0f);
    Vec3 on_sphere(float radius = 1.0f);
    Vec3 in_circle(float radius = 1.0f);
    Vec3 on_circle(float radius = 1.0f);
    Vec3 in_box(Vec3 low, Vec3 high);
    Vec3 direction_in_cone(Vec3 axis, float degrees);
    std::size_t weighted(const std::vector<double>& weights);
    std::string text(std::size_t length, const std::string& alphabet = "abcdefghijklmnopqrstuvwxyz");

    template <class T>
    void shuffle(std::vector<T>& items) {
        for (std::size_t i = items.size(); i > 1; --i) {
            const std::size_t j = index(i);
            using std::swap;
            swap(items[i - 1], items[j]);
        }
    }

    template <class T>
    const T& pick(const std::vector<T>& items) {
        if (items.empty()) throw std::out_of_range("Random::pick on an empty list");
        return items[index(items.size())];
    }

private:
    std::uint64_t m_state[4] = {};
    std::uint64_t m_seed = 0;
    bool m_has_spare = false;
    double m_spare = 0.0;
};

class SecureRandom {
public:
    SecureRandom();
    SecureRandom(const SecureRandom&) = delete;
    SecureRandom& operator=(const SecureRandom&) = delete;

    bool ok() const { return m_ok; }
    void reseed();
    void fill(void* data, std::size_t size);
    std::vector<std::uint8_t> bytes(std::size_t size);
    std::uint32_t next32();
    std::uint64_t next();
    std::uint64_t below(std::uint64_t bound);
    long long integer(long long low, long long high);
    double unit();
    double range(double low, double high);
    bool chance(double probability);
    std::size_t index(std::size_t size) { return static_cast<std::size_t>(below(size)); }
    std::string text(std::size_t length, const std::string& alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    std::string hex(std::size_t byte_count = 16);
    std::string token(std::size_t byte_count = 32);
    std::string uuid();

    template <class T>
    void shuffle(std::vector<T>& items) {
        for (std::size_t i = items.size(); i > 1; --i) {
            const std::size_t j = index(i);
            using std::swap;
            swap(items[i - 1], items[j]);
        }
    }

private:
    bool m_ok = false;
};

bool os_entropy(void* data, std::size_t size);
SecureRandom& secure_random();
bool secure_random_uses_fizmo();

Random& global_random();
std::uint64_t random_seed_from_time();

float noise(float x, float y = 0.0f, float z = 0.0f, std::uint32_t seed = 0);
float fractal_noise(float x, float y, float z, int octaves, std::uint32_t seed = 0, float lacunarity = 2.0f, float gain = 0.5f);
float ridged_noise(float x, float y, float z, int octaves, std::uint32_t seed = 0);
float cellular_noise(float x, float y, float z, std::uint32_t seed = 0);
float value_noise(float x, float y = 0.0f, float z = 0.0f, std::uint32_t seed = 0);
std::uint32_t hash32(std::uint32_t x);
std::uint64_t hash64(std::uint64_t x);
std::uint64_t hash_text(const std::string& text);

template <class T>
class ShuffleBag {
public:
    ShuffleBag() : m_random(&global_random()) {}
    explicit ShuffleBag(Random& random) : m_random(&random) {}
    ShuffleBag(std::initializer_list<T> items) : m_random(&global_random()), m_items(items) { refill(); }
    explicit ShuffleBag(std::vector<T> items) : m_random(&global_random()), m_items(std::move(items)) { refill(); }

    void use(Random& random) { m_random = &random; }

    void add(const T& value, std::size_t copies = 1) {
        for (std::size_t c = 0; c < copies; ++c) {
            m_items.push_back(value);
            const std::size_t span = m_items.size() - m_cursor;
            const std::size_t j = m_cursor + m_random->index(span);
            if (j != m_items.size() - 1) swap_at(j, m_items.size() - 1);
        }
    }

    bool remove(const T& value) {
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            if (!(m_items[i] == value)) continue;
            m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(i));
            if (i < m_cursor) --m_cursor;
            return true;
        }
        return false;
    }

    T next() {
        if (m_items.empty()) throw std::out_of_range("ShuffleBag::next on an empty bag");
        if (m_cursor >= m_items.size()) {
            const T last = m_items[m_cursor - 1];
            refill();
            if (m_avoid_repeat && m_items.size() > 1 && m_items[0] == last) {
                const std::size_t j = 1 + m_random->index(m_items.size() - 1);
                swap_at(0, j);
            }
        }
        return m_items[m_cursor++];
    }

    T operator()() { return next(); }

    const T* peek() const { return m_cursor < m_items.size() ? &m_items[m_cursor] : nullptr; }

    void refill() {
        m_cursor = 0;
        for (std::size_t i = m_items.size(); i > 1; --i) swap_at(i - 1, m_random->index(i));
    }

    void clear() {
        m_items.clear();
        m_cursor = 0;
    }

    void avoid_boundary_repeat(bool on) { m_avoid_repeat = on; }
    bool avoids_boundary_repeat() const { return m_avoid_repeat; }
    std::size_t size() const { return m_items.size(); }
    std::size_t remaining() const { return m_items.size() - m_cursor; }
    bool empty() const { return m_items.empty(); }
    const std::vector<T>& contents() const { return m_items; }

private:
    void swap_at(std::size_t a, std::size_t b) {
        using std::swap;
        swap(m_items[a], m_items[b]);
    }

    Random* m_random = nullptr;
    std::vector<T> m_items;
    std::size_t m_cursor = 0;
    bool m_avoid_repeat = true;
};

template <class T>
class WeightedTable {
public:
    void add(const T& value, double weight) {
        if (weight <= 0.0) return;
        m_items.push_back(value);
        m_weights.push_back(weight);
        m_total += weight;
    }

    const T& pick(Random& random) const {
        if (m_items.empty()) throw std::out_of_range("WeightedTable::pick on an empty table");
        double r = random.unit() * m_total;
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            r -= m_weights[i];
            if (r <= 0.0) return m_items[i];
        }
        return m_items.back();
    }

    const T& pick() const { return pick(global_random()); }
    std::size_t size() const { return m_items.size(); }
    bool empty() const { return m_items.empty(); }
    double total() const { return m_total; }
    void clear() {
        m_items.clear();
        m_weights.clear();
        m_total = 0.0;
    }

private:
    std::vector<T> m_items;
    std::vector<double> m_weights;
    double m_total = 0.0;
};

} // namespace orchard

#endif // ORCHARD_RANDOM_HPP
