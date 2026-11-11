#include <orchard/random.hpp>

#if defined(ORCHARD_WITH_FIZMO_RANDOM)
#include <fizmo/../basics.hpp>
#endif

#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <fstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <cerrno>
#include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <unistd.h>
#endif

namespace orchard {

namespace {

void wipe(void* data, std::size_t size) {
    volatile std::uint8_t* p = static_cast<volatile std::uint8_t*>(data);
    while (size--) *p++ = 0;
}

} // namespace

bool os_entropy(void* data, std::size_t size) {
    std::uint8_t* out = static_cast<std::uint8_t*>(data);
#if defined(_WIN32)
    while (size > 0) {
        const ULONG part = static_cast<ULONG>(size > 0x10000000u ? 0x10000000u : size);
        if (BCryptGenRandom(nullptr, out, part, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
        out += part;
        size -= part;
    }
    return true;
#else
#if defined(__linux__) || defined(__ANDROID__)
    while (size > 0) {
        const ssize_t got = getrandom(out, size, 0);
        if (got < 0) {
            if (errno == EINTR) continue;
            break;
        }
        out += got;
        size -= static_cast<std::size_t>(got);
    }
    if (size == 0) return true;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    while (size > 0) {
        const std::size_t part = size > 256 ? 256 : size;
        if (getentropy(out, part) != 0) break;
        out += part;
        size -= part;
    }
    if (size == 0) return true;
#endif
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom) return false;
    urandom.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(urandom.gcount()) == size;
#endif
}

SecureRandom::SecureRandom() {
    reseed();
}

void SecureRandom::reseed() {
    std::uint8_t probe[8];
    m_ok = true;
    fill(probe, sizeof(probe));
    wipe(probe, sizeof(probe));
}

void SecureRandom::fill(void* data, std::size_t size) {
    if (size == 0) return;
#if defined(ORCHARD_WITH_FIZMO_RANDOM)
    try {
        fizmo::random_bytes(data, size);
        return;
    } catch (...) {
    }
#endif
    m_ok = os_entropy(data, size);
    if (!m_ok) throw std::runtime_error("SecureRandom: the system has no secure random source");
}

std::vector<std::uint8_t> SecureRandom::bytes(std::size_t size) {
    std::vector<std::uint8_t> out(size);
    if (size) fill(out.data(), size);
    return out;
}

std::uint32_t SecureRandom::next32() {
    std::uint8_t b[4];
    fill(b, 4);
    return static_cast<std::uint32_t>(b[0]) | static_cast<std::uint32_t>(b[1]) << 8 | static_cast<std::uint32_t>(b[2]) << 16 | static_cast<std::uint32_t>(b[3]) << 24;
}

std::uint64_t SecureRandom::next() {
    return static_cast<std::uint64_t>(next32()) << 32 | next32();
}

std::uint64_t SecureRandom::below(std::uint64_t bound) {
    if (bound <= 1) return 0;
    const std::uint64_t limit = (0ull - bound) % bound;
    while (true) {
        const std::uint64_t v = next();
        if (v >= limit) return v % bound;
    }
}

long long SecureRandom::integer(long long low, long long high) {
    if (high < low) std::swap(low, high);
    const std::uint64_t span = static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low);
    if (span == ~0ull) return static_cast<long long>(next());
    return static_cast<long long>(static_cast<std::uint64_t>(low) + below(span + 1));
}

double SecureRandom::unit() {
    return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
}

double SecureRandom::range(double low, double high) {
    return low + (high - low) * unit();
}

bool SecureRandom::chance(double probability) {
    if (probability <= 0.0) return false;
    if (probability >= 1.0) return true;
    return unit() < probability;
}

std::string SecureRandom::text(std::size_t length, const std::string& alphabet) {
    if (alphabet.empty()) return std::string();
    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) out += alphabet[index(alphabet.size())];
    return out;
}

std::string SecureRandom::hex(std::size_t byte_count) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(byte_count * 2);
    for (std::uint8_t b : bytes(byte_count)) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}

std::string SecureRandom::token(std::size_t byte_count) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const std::vector<std::uint8_t> b = bytes(byte_count);
    std::string out;
    std::uint32_t acc = 0;
    int bits = 0;
    for (std::uint8_t v : b) {
        acc = (acc << 8) | v;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += table[(acc >> bits) & 63];
        }
    }
    if (bits > 0) out += table[(acc << (6 - bits)) & 63];
    return out;
}

std::string SecureRandom::uuid() {
    std::vector<std::uint8_t> b = bytes(16);
    b[6] = static_cast<std::uint8_t>((b[6] & 0x0F) | 0x40);
    b[8] = static_cast<std::uint8_t>((b[8] & 0x3F) | 0x80);
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out += '-';
        out += digits[b[i] >> 4];
        out += digits[b[i] & 15];
    }
    return out;
}

bool secure_random_uses_fizmo() {
#if defined(ORCHARD_WITH_FIZMO_RANDOM)
    return true;
#else
    return false;
#endif
}

SecureRandom& secure_random() {
    thread_local SecureRandom r;
    return r;
}

} // namespace orchard
