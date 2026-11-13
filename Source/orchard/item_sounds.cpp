#include <orchard/items.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace orchard {

namespace {

constexpr int rate = 44100;
constexpr float tau = 6.2831853f;

struct Synth {
    std::vector<float> out;
    std::mt19937 rng{1234u};
    std::uniform_real_distribution<float> uni{-1.0f, 1.0f};

    explicit Synth(float seconds) : out(static_cast<std::size_t>(seconds * rate), 0.0f) {}

    float t(std::size_t i) const { return static_cast<float>(i) / rate; }

    void noise(float start, float length, float volume, float attack, float decay, float smooth) {
        float low = 0.0f;
        const std::size_t a = static_cast<std::size_t>(start * rate);
        const std::size_t n = static_cast<std::size_t>(length * rate);
        for (std::size_t i = 0; i < n && a + i < out.size(); ++i) {
            const float x = static_cast<float>(i) / rate;
            const float env = (attack > 0.0f ? std::min(1.0f, x / attack) : 1.0f) * std::exp(-x / std::max(1e-4f, decay));
            low += (uni(rng) - low) * std::clamp(1.0f - smooth, 0.01f, 1.0f);
            out[a + i] += low * env * volume;
        }
    }

    void tone(float start, float length, float freq_from, float freq_to, float volume, float attack, float decay) {
        float phase = 0.0f;
        const std::size_t a = static_cast<std::size_t>(start * rate);
        const std::size_t n = static_cast<std::size_t>(length * rate);
        for (std::size_t i = 0; i < n && a + i < out.size(); ++i) {
            const float x = static_cast<float>(i) / rate;
            const float k = n > 1 ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f;
            const float f = freq_from + (freq_to - freq_from) * k;
            phase += tau * f / rate;
            const float env = (attack > 0.0f ? std::min(1.0f, x / attack) : 1.0f) * std::exp(-x / std::max(1e-4f, decay));
            out[a + i] += std::sin(phase) * env * volume;
        }
    }

    void swell(float start, float length, float volume, float smooth) {
        float low = 0.0f;
        const std::size_t a = static_cast<std::size_t>(start * rate);
        const std::size_t n = static_cast<std::size_t>(length * rate);
        for (std::size_t i = 0; i < n && a + i < out.size(); ++i) {
            const float k = static_cast<float>(i) / static_cast<float>(std::max<std::size_t>(1, n));
            const float env = std::sin(k * 3.14159265f);
            low += (uni(rng) - low) * std::clamp(1.0f - smooth, 0.01f, 1.0f);
            out[a + i] += low * env * env * volume;
        }
    }

    void click(float start, float volume, float ring_freq) {
        tone(start, 0.06f, ring_freq, ring_freq * 0.97f, volume * 0.6f, 0.0005f, 0.012f);
        noise(start, 0.012f, volume, 0.0f, 0.003f, 0.0f);
    }

    void echo(float delay, float feedback) {
        const std::size_t d = static_cast<std::size_t>(delay * rate);
        for (std::size_t i = d; i < out.size(); ++i) out[i] += out[i - d] * feedback;
    }
};

bool write_wav(const std::vector<float>& samples, const std::string& path) {
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    float peak = 1e-4f;
    for (float s : samples) peak = std::max(peak, std::fabs(s));
    const float gain = peak > 0.95f ? 0.95f / peak : 1.0f;
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    auto u32 = [&](std::uint32_t v) {
        const char b[4] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff), static_cast<char>((v >> 16) & 0xff), static_cast<char>((v >> 24) & 0xff)};
        f.write(b, 4);
    };
    auto u16 = [&](std::uint16_t v) {
        const char b[2] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff)};
        f.write(b, 2);
    };
    const std::uint32_t data = static_cast<std::uint32_t>(samples.size() * 2);
    f.write("RIFF", 4);
    u32(36 + data);
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    f.write("data", 4);
    u32(data);
    for (float s : samples) {
        const float v = std::clamp(s * gain, -1.0f, 1.0f);
        u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(v * 32000.0f)));
    }
    return static_cast<bool>(f);
}

} // namespace

const std::vector<std::string>& item_sound_names() {
    static const std::vector<std::string> all = {"gunshot_pistol", "gunshot_smg", "gunshot_rifle", "gunshot_sniper", "gunshot_shotgun", "musket", "launcher", "laser", "spell", "bow_release", "bow_draw", "crossbow", "dry_fire", "mag_out", "mag_in", "bolt_open", "bolt_close", "cylinder_open", "cylinder_close", "shell_insert", "round_in", "aim", "swing", "swing_heavy", "drink", "eat", "bandage", "equip", "throw", "pickup",
        "gunshot_suppressed", "gunshot_rimfire", "gunshot_heavy", "gunshot_mg", "gunshot_magnum", "slide_rack", "slide_release", "slide_lock", "pump_back", "pump_forward", "lever_open", "lever_close", "hammer_cock", "break_open", "break_close", "shell_out", "ejector", "charging_handle", "feed_cover_open", "feed_cover_close", "belt_in", "powder_pour", "ramrod", "selector", "jam", "garand_ping", "casing_brass", "casing_shell", "casing_soft", "ricochet",
        "step_concrete", "step_metal", "step_wood", "step_tile", "step_dirt", "step_grass", "step_leaves", "step_gravel", "step_sand", "step_mud", "step_snow", "step_ice", "step_water", "step_carpet",
        "hit_concrete", "hit_metal", "hit_wood", "hit_dirt", "hit_mud", "hit_snow", "hit_water", "hit_glass", "hit_leaves", "hit_flesh"};
    return all;
}

bool write_item_sound(const std::string& name, const std::string& path) {
    auto gun = [](float len, float crack, float boom, float tail, float low_hz) {
        Synth s(len);
        s.noise(0.0f, 0.03f, crack, 0.0f, 0.008f, 0.0f);
        s.noise(0.0f, len, boom, 0.001f, tail, 0.82f);
        s.tone(0.0f, std::min(len, 0.4f), low_hz, low_hz * 0.5f, boom * 0.9f, 0.001f, 0.08f);
        return s;
    };
    Synth s(0.1f);
    if (name == "gunshot_pistol") s = gun(0.5f, 0.9f, 0.8f, 0.07f, 110.0f);
    else if (name == "gunshot_smg") s = gun(0.35f, 0.8f, 0.7f, 0.05f, 120.0f);
    else if (name == "gunshot_rifle") {
        s = gun(0.9f, 1.0f, 0.9f, 0.12f, 90.0f);
        s.echo(0.18f, 0.25f);
    } else if (name == "gunshot_sniper") {
        s = gun(1.6f, 1.0f, 1.0f, 0.2f, 70.0f);
        s.echo(0.32f, 0.35f);
    } else if (name == "gunshot_shotgun") {
        s = gun(1.0f, 0.8f, 1.0f, 0.16f, 60.0f);
        s.echo(0.22f, 0.25f);
    } else if (name == "musket") {
        s = gun(1.6f, 0.5f, 1.0f, 0.3f, 50.0f);
        s.noise(0.0f, 1.2f, 0.25f, 0.05f, 0.6f, 0.3f);
    } else if (name == "launcher") {
        s = Synth(1.2f);
        s.noise(0.0f, 0.08f, 0.9f, 0.0f, 0.04f, 0.5f);
        s.swell(0.02f, 1.1f, 0.7f, 0.6f);
    } else if (name == "laser") {
        s = Synth(0.35f);
        s.tone(0.0f, 0.3f, 1800.0f, 260.0f, 0.6f, 0.002f, 0.12f);
        s.tone(0.0f, 0.3f, 2700.0f, 390.0f, 0.25f, 0.002f, 0.1f);
    } else if (name == "spell") {
        s = Synth(0.9f);
        for (float f : {523.0f, 659.0f, 784.0f, 1046.0f}) s.tone(0.0f, 0.9f, f, f * 1.01f, 0.2f, 0.05f, 0.35f);
        s.swell(0.0f, 0.6f, 0.15f, 0.2f);
    } else if (name == "bow_release") {
        s = Synth(0.45f);
        s.tone(0.0f, 0.4f, 190.0f, 170.0f, 0.7f, 0.0005f, 0.09f);
        s.tone(0.0f, 0.4f, 380.0f, 340.0f, 0.3f, 0.0005f, 0.06f);
        s.noise(0.0f, 0.25f, 0.3f, 0.01f, 0.08f, 0.4f);
    } else if (name == "bow_draw") {
        s = Synth(0.7f);
        s.swell(0.0f, 0.7f, 0.25f, 0.92f);
        s.tone(0.0f, 0.7f, 90.0f, 140.0f, 0.12f, 0.2f, 1.0f);
    } else if (name == "crossbow") {
        s = Synth(0.4f);
        s.click(0.0f, 0.8f, 900.0f);
        s.tone(0.0f, 0.35f, 240.0f, 200.0f, 0.6f, 0.0005f, 0.07f);
    } else if (name == "dry_fire") {
        s = Synth(0.12f);
        s.click(0.0f, 0.6f, 2400.0f);
    } else if (name == "mag_out") {
        s = Synth(0.35f);
        s.click(0.0f, 0.7f, 1800.0f);
        s.swell(0.03f, 0.18f, 0.25f, 0.5f);
        s.click(0.22f, 0.3f, 1200.0f);
    } else if (name == "mag_in") {
        s = Synth(0.3f);
        s.swell(0.0f, 0.1f, 0.2f, 0.5f);
        s.click(0.1f, 0.9f, 1500.0f);
        s.tone(0.1f, 0.18f, 2200.0f, 2150.0f, 0.2f, 0.0005f, 0.04f);
    } else if (name == "bolt_open" || name == "bolt_close") {
        s = Synth(0.4f);
        s.click(0.0f, 0.7f, name == "bolt_open" ? 1300.0f : 1600.0f);
        s.swell(0.03f, 0.2f, 0.35f, 0.4f);
        s.click(0.24f, 0.9f, name == "bolt_open" ? 1100.0f : 1900.0f);
    } else if (name == "cylinder_open" || name == "cylinder_close") {
        s = Synth(0.45f);
        s.click(0.0f, 0.7f, 2000.0f);
        for (int i = 0; i < 6; ++i) s.click(0.08f + i * 0.04f, 0.18f, 2600.0f);
    } else if (name == "shell_insert" || name == "round_in") {
        s = Synth(0.18f);
        s.swell(0.0f, 0.07f, 0.2f, 0.5f);
        s.click(0.07f, 0.6f, name == "shell_insert" ? 900.0f : 1700.0f);
    } else if (name == "aim") {
        s = Synth(0.25f);
        s.swell(0.0f, 0.22f, 0.18f, 0.85f);
        s.click(0.18f, 0.12f, 900.0f);
    } else if (name == "swing" || name == "swing_heavy") {
        const bool heavy = name == "swing_heavy";
        s = Synth(heavy ? 0.5f : 0.35f);
        s.swell(0.0f, heavy ? 0.45f : 0.3f, 0.5f, heavy ? 0.88f : 0.7f);
    } else if (name == "drink") {
        s = Synth(0.9f);
        for (int i = 0; i < 3; ++i) {
            s.tone(0.1f + i * 0.25f, 0.12f, 180.0f, 120.0f, 0.5f, 0.01f, 0.05f);
            s.noise(0.1f + i * 0.25f, 0.1f, 0.08f, 0.01f, 0.04f, 0.8f);
        }
    } else if (name == "eat") {
        s = Synth(0.8f);
        for (int i = 0; i < 4; ++i) s.noise(0.05f + i * 0.18f, 0.1f, 0.45f, 0.003f, 0.03f, 0.3f);
    } else if (name == "bandage") {
        s = Synth(0.7f);
        s.noise(0.0f, 0.6f, 0.35f, 0.02f, 0.25f, 0.1f);
    } else if (name == "equip" || name == "pickup") {
        s = Synth(0.3f);
        s.swell(0.0f, 0.2f, 0.2f, 0.8f);
        s.click(0.18f, name == "pickup" ? 0.3f : 0.15f, 800.0f);
    } else if (name == "throw") {
        s = Synth(0.3f);
        s.swell(0.0f, 0.28f, 0.4f, 0.75f);
    } else if (name == "gunshot_suppressed") {
        s = Synth(0.3f);
        s.noise(0.0f, 0.12f, 0.6f, 0.001f, 0.03f, 0.55f);
        s.click(0.0f, 0.5f, 1500.0f);
        s.click(0.05f, 0.25f, 1900.0f);
    } else if (name == "gunshot_rimfire") {
        s = gun(0.3f, 0.6f, 0.35f, 0.035f, 180.0f);
    } else if (name == "gunshot_heavy") {
        s = gun(2.0f, 1.0f, 1.0f, 0.28f, 45.0f);
        s.echo(0.4f, 0.4f);
    } else if (name == "gunshot_mg") {
        s = gun(0.45f, 1.0f, 0.95f, 0.08f, 85.0f);
        s.echo(0.15f, 0.2f);
    } else if (name == "gunshot_magnum") {
        s = gun(0.9f, 1.0f, 1.0f, 0.11f, 80.0f);
        s.echo(0.2f, 0.25f);
    } else if (name == "slide_rack") {
        s = Synth(0.4f);
        s.click(0.0f, 0.6f, 1500.0f);
        s.swell(0.02f, 0.12f, 0.3f, 0.35f);
        s.click(0.15f, 0.5f, 1250.0f);
        s.swell(0.18f, 0.1f, 0.25f, 0.35f);
        s.click(0.3f, 1.0f, 1900.0f);
    } else if (name == "slide_release") {
        s = Synth(0.25f);
        s.click(0.0f, 0.4f, 2200.0f);
        s.swell(0.01f, 0.06f, 0.25f, 0.3f);
        s.click(0.08f, 1.0f, 1800.0f);
    } else if (name == "slide_lock") {
        s = Synth(0.15f);
        s.click(0.0f, 0.8f, 2000.0f);
        s.click(0.03f, 0.35f, 2600.0f);
    } else if (name == "pump_back" || name == "pump_forward") {
        const bool back = name == "pump_back";
        s = Synth(0.3f);
        s.swell(0.0f, 0.16f, 0.45f, 0.45f);
        s.click(0.15f, 1.0f, back ? 700.0f : 950.0f);
        s.tone(0.15f, 0.12f, back ? 420.0f : 520.0f, back ? 400.0f : 500.0f, 0.3f, 0.0005f, 0.03f);
    } else if (name == "lever_open" || name == "lever_close") {
        s = Synth(0.3f);
        s.click(0.0f, 0.5f, name == "lever_open" ? 1100.0f : 1400.0f);
        s.swell(0.02f, 0.15f, 0.3f, 0.4f);
        s.click(0.18f, 0.85f, name == "lever_open" ? 950.0f : 1700.0f);
    } else if (name == "hammer_cock") {
        s = Synth(0.2f);
        s.click(0.0f, 0.35f, 2400.0f);
        s.click(0.07f, 0.7f, 2800.0f);
    } else if (name == "break_open" || name == "break_close") {
        const bool open = name == "break_open";
        s = Synth(0.35f);
        s.click(0.0f, open ? 0.6f : 1.0f, open ? 900.0f : 1300.0f);
        s.tone(0.0f, 0.25f, open ? 380.0f : 520.0f, open ? 360.0f : 500.0f, 0.35f, 0.0005f, 0.05f);
        if (open) s.swell(0.05f, 0.2f, 0.2f, 0.5f);
    } else if (name == "shell_out") {
        s = Synth(0.6f);
        s.swell(0.0f, 0.15f, 0.25f, 0.5f);
        s.tone(0.25f, 0.2f, 350.0f, 300.0f, 0.25f, 0.001f, 0.04f);
        s.tone(0.4f, 0.15f, 320.0f, 280.0f, 0.2f, 0.001f, 0.03f);
    } else if (name == "ejector") {
        s = Synth(0.7f);
        s.click(0.0f, 0.5f, 1800.0f);
        for (int i = 0; i < 6; ++i) s.tone(0.12f + i * 0.07f, 0.15f, 4200.0f + i * 230.0f, 4000.0f + i * 200.0f, 0.18f, 0.0005f, 0.05f);
    } else if (name == "charging_handle") {
        s = Synth(0.35f);
        s.click(0.0f, 0.5f, 1100.0f);
        s.swell(0.02f, 0.18f, 0.35f, 0.3f);
        s.click(0.22f, 1.0f, 1500.0f);
    } else if (name == "feed_cover_open" || name == "feed_cover_close") {
        s = Synth(0.3f);
        s.click(0.0f, name == "feed_cover_close" ? 1.0f : 0.7f, 900.0f);
        s.tone(0.0f, 0.2f, 300.0f, 280.0f, 0.3f, 0.0005f, 0.05f);
    } else if (name == "belt_in") {
        s = Synth(0.6f);
        for (int i = 0; i < 8; ++i) s.click(0.02f + i * 0.06f, 0.25f, 2600.0f + (i % 3) * 300.0f);
    } else if (name == "powder_pour") {
        s = Synth(0.7f);
        s.noise(0.0f, 0.6f, 0.25f, 0.1f, 0.3f, 0.2f);
    } else if (name == "ramrod") {
        s = Synth(0.45f);
        s.swell(0.0f, 0.3f, 0.3f, 0.3f);
        s.click(0.3f, 0.6f, 700.0f);
        s.tone(0.3f, 0.12f, 1600.0f, 1550.0f, 0.15f, 0.0005f, 0.04f);
    } else if (name == "selector") {
        s = Synth(0.1f);
        s.click(0.0f, 0.6f, 2800.0f);
    } else if (name == "jam") {
        s = Synth(0.25f);
        s.click(0.0f, 0.9f, 600.0f);
        s.noise(0.0f, 0.06f, 0.4f, 0.0f, 0.02f, 0.3f);
    } else if (name == "garand_ping") {
        s = Synth(0.9f);
        s.click(0.0f, 0.4f, 1800.0f);
        s.tone(0.0f, 0.85f, 2350.0f, 2330.0f, 0.6f, 0.001f, 0.25f);
        s.tone(0.0f, 0.85f, 4700.0f, 4660.0f, 0.2f, 0.001f, 0.15f);
    } else if (name == "casing_brass") {
        s = Synth(0.6f);
        s.tone(0.0f, 0.12f, 5200.0f, 5100.0f, 0.35f, 0.0005f, 0.04f);
        s.tone(0.14f, 0.1f, 4900.0f, 4850.0f, 0.22f, 0.0005f, 0.03f);
        s.tone(0.24f, 0.08f, 5300.0f, 5250.0f, 0.12f, 0.0005f, 0.02f);
        s.tone(0.31f, 0.06f, 5000.0f, 4990.0f, 0.07f, 0.0005f, 0.015f);
    } else if (name == "casing_shell") {
        s = Synth(0.45f);
        s.tone(0.0f, 0.1f, 900.0f, 850.0f, 0.3f, 0.0005f, 0.03f);
        s.noise(0.0f, 0.05f, 0.2f, 0.0f, 0.015f, 0.4f);
        s.tone(0.16f, 0.08f, 850.0f, 820.0f, 0.15f, 0.0005f, 0.02f);
    } else if (name == "casing_soft") {
        s = Synth(0.15f);
        s.noise(0.0f, 0.08f, 0.25f, 0.002f, 0.02f, 0.7f);
    } else if (name == "ricochet") {
        s = Synth(0.7f);
        s.tone(0.0f, 0.6f, 3200.0f, 900.0f, 0.4f, 0.002f, 0.25f);
        s.noise(0.0f, 0.03f, 0.5f, 0.0f, 0.01f, 0.0f);
    } else if (name.rfind("step_", 0) == 0) {
        const std::string k = name.substr(5);
        s = Synth(0.3f);
        if (k == "concrete" || k == "tile") {
            s.noise(0.0f, 0.06f, 0.6f, 0.002f, 0.015f, 0.35f);
            s.tone(0.0f, 0.05f, 140.0f, 110.0f, 0.3f, 0.001f, 0.02f);
            if (k == "tile") s.click(0.005f, 0.2f, 2500.0f);
        } else if (k == "metal") {
            s.tone(0.0f, 0.25f, 380.0f, 370.0f, 0.35f, 0.001f, 0.08f);
            s.tone(0.0f, 0.25f, 760.0f, 740.0f, 0.18f, 0.001f, 0.06f);
            s.noise(0.0f, 0.04f, 0.4f, 0.0f, 0.012f, 0.3f);
        } else if (k == "wood") {
            s.tone(0.0f, 0.12f, 190.0f, 160.0f, 0.5f, 0.001f, 0.04f);
            s.noise(0.0f, 0.05f, 0.35f, 0.002f, 0.015f, 0.5f);
        } else if (k == "dirt" || k == "mud") {
            s.noise(0.0f, 0.1f, 0.5f, 0.005f, 0.03f, 0.8f);
            if (k == "mud") s.tone(0.02f, 0.15f, 120.0f, 80.0f, 0.3f, 0.01f, 0.06f);
        } else if (k == "grass" || k == "leaves") {
            s.noise(0.0f, 0.22f, k == "leaves" ? 0.5f : 0.35f, 0.01f, 0.07f, 0.15f);
            if (k == "leaves") s.noise(0.04f, 0.15f, 0.3f, 0.005f, 0.04f, 0.05f);
        } else if (k == "gravel") {
            for (int i = 0; i < 9; ++i) s.click(0.005f + i * 0.012f, 0.18f, 1800.0f + (i % 4) * 500.0f);
            s.noise(0.0f, 0.12f, 0.35f, 0.002f, 0.04f, 0.2f);
        } else if (k == "sand") {
            s.noise(0.0f, 0.18f, 0.35f, 0.02f, 0.06f, 0.35f);
        } else if (k == "snow") {
            s.noise(0.0f, 0.2f, 0.4f, 0.02f, 0.07f, 0.55f);
            for (int i = 0; i < 4; ++i) s.click(0.03f + i * 0.02f, 0.06f, 3000.0f);
        } else if (k == "ice") {
            s.click(0.0f, 0.4f, 3200.0f);
            s.noise(0.0f, 0.05f, 0.3f, 0.0f, 0.015f, 0.2f);
        } else if (k == "water") {
            s.noise(0.0f, 0.25f, 0.45f, 0.01f, 0.09f, 0.6f);
            s.tone(0.03f, 0.12f, 500.0f, 250.0f, 0.15f, 0.005f, 0.04f);
        } else {
            s.noise(0.0f, 0.08f, 0.25f, 0.01f, 0.03f, 0.85f);
        }
    } else if (name.rfind("hit_", 0) == 0) {
        const std::string k = name.substr(4);
        s = Synth(0.5f);
        if (k == "metal") {
            s.click(0.0f, 1.0f, 2600.0f);
            s.tone(0.0f, 0.45f, 1900.0f, 1850.0f, 0.4f, 0.0005f, 0.15f);
            s.tone(0.0f, 0.45f, 3100.0f, 3050.0f, 0.2f, 0.0005f, 0.1f);
        } else if (k == "concrete") {
            s.noise(0.0f, 0.08f, 0.9f, 0.0f, 0.02f, 0.25f);
            s.noise(0.02f, 0.3f, 0.2f, 0.01f, 0.1f, 0.5f);
        } else if (k == "wood") {
            s.tone(0.0f, 0.12f, 260.0f, 200.0f, 0.7f, 0.0005f, 0.04f);
            s.noise(0.0f, 0.06f, 0.5f, 0.0f, 0.02f, 0.45f);
        } else if (k == "dirt" || k == "mud" || k == "snow" || k == "leaves") {
            s.noise(0.0f, 0.15f, k == "leaves" ? 0.45f : 0.7f, 0.0f, k == "snow" ? 0.06f : 0.04f, k == "leaves" ? 0.1f : 0.8f);
        } else if (k == "water") {
            s.noise(0.0f, 0.35f, 0.5f, 0.002f, 0.12f, 0.55f);
            s.tone(0.0f, 0.15f, 900.0f, 300.0f, 0.25f, 0.002f, 0.05f);
        } else if (k == "glass") {
            for (int i = 0; i < 10; ++i) s.tone(i * 0.025f, 0.2f, 3500.0f + i * 410.0f, 3400.0f + i * 400.0f, 0.15f, 0.0005f, 0.06f);
            s.noise(0.0f, 0.05f, 0.6f, 0.0f, 0.02f, 0.0f);
        } else if (k == "flesh") {
            s.noise(0.0f, 0.1f, 0.6f, 0.0f, 0.03f, 0.85f);
            s.tone(0.0f, 0.1f, 140.0f, 90.0f, 0.4f, 0.001f, 0.03f);
        } else {
            s.noise(0.0f, 0.1f, 0.6f, 0.0f, 0.03f, 0.5f);
        }
    } else {
        s = Synth(0.15f);
        s.click(0.0f, 0.5f, 1200.0f);
    }
    return write_wav(s.out, path);
}

} // namespace orchard
