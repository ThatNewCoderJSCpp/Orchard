#ifndef ORCHARD_EDITOR_PARTICLE_TEXTURES_HPP
#define ORCHARD_EDITOR_PARTICLE_TEXTURES_HPP

#include <orchard/scene.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace orchard {
namespace editor {

enum class ParticleFlipbook { single, over_life, random, fps };

struct BuiltinParticleTexture {
    const char* name;
    const char* label;
    int columns;
    int rows;
    ParticleFlipbook mode;
    float fps;
    float rotation;
    float spin;
};

inline constexpr std::string_view builtin_particle_prefix = "builtin:";

inline const std::vector<BuiltinParticleTexture>& builtin_particle_textures() {
    static const std::vector<BuiltinParticleTexture> all = {
        {"builtin:glow", "Soft glow", 1, 1, ParticleFlipbook::single, 0.0f, 0.0f, 0.0f},
        {"builtin:smoke", "Smoke puffs (16 variants)", 4, 4, ParticleFlipbook::random, 0.0f, 360.0f, 25.0f},
        {"builtin:steam", "Steam wisps (16 variants)", 4, 4, ParticleFlipbook::random, 0.0f, 360.0f, 40.0f},
        {"builtin:fire", "Fire (animated)", 4, 4, ParticleFlipbook::over_life, 0.0f, 14.0f, 0.0f},
        {"builtin:explosion", "Explosion fireball (animated)", 4, 4, ParticleFlipbook::over_life, 0.0f, 360.0f, 30.0f},
        {"builtin:spark", "Spark streak", 1, 1, ParticleFlipbook::single, 0.0f, 0.0f, 0.0f},
        {"builtin:ember", "Embers (4 variants)", 2, 2, ParticleFlipbook::random, 0.0f, 360.0f, 90.0f},
        {"builtin:rain", "Raindrop streak", 1, 1, ParticleFlipbook::single, 0.0f, 0.0f, 0.0f},
        {"builtin:snow", "Snowflakes (4 variants)", 2, 2, ParticleFlipbook::random, 0.0f, 360.0f, 70.0f},
        {"builtin:magic", "Magic stars (4 variants)", 2, 2, ParticleFlipbook::random, 0.0f, 360.0f, 120.0f},
        {"builtin:bubble", "Bubble", 1, 1, ParticleFlipbook::single, 0.0f, 0.0f, 0.0f},
        {"builtin:debris", "Debris chunks (16 variants)", 4, 4, ParticleFlipbook::random, 0.0f, 360.0f, 420.0f},
        {"builtin:dust", "Dust motes (4 variants)", 2, 2, ParticleFlipbook::random, 0.0f, 360.0f, 30.0f},
        {"builtin:shockwave", "Shockwave ring", 1, 1, ParticleFlipbook::single, 0.0f, 360.0f, 0.0f},
        {"builtin:smoke-soft", "Soft smoke (16 variants, no lumps)", 4, 4, ParticleFlipbook::random, 0.0f, 360.0f, 18.0f},
        {"builtin:steam-wisp", "Steam wisps, thin (16 variants)", 4, 4, ParticleFlipbook::random, 0.0f, 25.0f, 15.0f},
        {"builtin:flame", "Flame tongues (16 variants)", 4, 4, ParticleFlipbook::random, 0.0f, 8.0f, 0.0f},
        {"builtin:flash", "Bright flash", 1, 1, ParticleFlipbook::single, 0.0f, 360.0f, 0.0f},
        {"builtin:scorch", "Scorch mark (4 variants)", 2, 2, ParticleFlipbook::random, 0.0f, 360.0f, 0.0f},
    };
    return all;
}

inline const BuiltinParticleTexture* find_builtin_particle_texture(std::string_view name) {
    for (const BuiltinParticleTexture& t : builtin_particle_textures())
        if (name == t.name) return &t;
    return nullptr;
}

inline bool is_builtin_particle_texture(std::string_view name) {
    return find_builtin_particle_texture(name) != nullptr;
}

inline const char* builtin_particle_texture_for_preset(std::string_view preset) {
    struct Pair {
        const char* preset;
        const char* texture;
    };
    static const Pair pairs[] = {
        {"sparks", "builtin:spark"}, {"smoke", "builtin:smoke-soft"}, {"fire", "builtin:fire"}, {"explosion", "builtin:explosion"},
        {"dust", "builtin:dust"}, {"rain", "builtin:rain"}, {"snow", "builtin:snow"}, {"magic", "builtin:magic"},
        {"debris", "builtin:debris"}, {"bubbles", "builtin:bubble"}, {"steam", "builtin:steam-wisp"}, {"embers", "builtin:ember"},
        {"flames", "builtin:flame"}, {"smoke-dark", "builtin:smoke-soft"}, {"smoke-puffs", "builtin:smoke"}, {"steam-puffs", "builtin:steam"},
        {"steam-jet", "builtin:steam-wisp"}, {"spark-shower", "builtin:spark"}, {"zap", "builtin:spark"}, {"flash", "builtin:flash"},
        {"fireball", "builtin:explosion"}, {"shockwave", "builtin:shockwave"}, {"splash", "builtin:rain"}, {"dust-burst", "builtin:smoke-soft"}, {"scorch", "builtin:scorch"},
    };
    for (const Pair& p : pairs)
        if (preset == p.preset) return p.texture;
    return nullptr;
}

struct ParticleLook {
    std::string simple;
    std::string better;
    bool better_covers = false;
};

inline ParticleLook particle_look_for(std::string_view texture, std::string_view preset) {
    std::string t(texture);
    if ((t.empty() || t == "builtin:glow") && !preset.empty())
        if (const char* p = builtin_particle_texture_for_preset(preset)) t = p;
    if (t == "builtin:smoke" || t == "builtin:smoke-soft") return {"builtin:smoke", "builtin:smoke-soft", false};
    if (t == "builtin:steam" || t == "builtin:steam-wisp") return {"builtin:steam", "builtin:steam-wisp", false};
    if (t == "builtin:fire" || t == "builtin:flame") return {"builtin:fire", "builtin:flame", false};
    if (t == "builtin:explosion") return {"builtin:explosion", "builtin:explosion", true};
    if (t.empty() || !is_builtin_particle_texture(t)) return {};
    return {t, t, false};
}

inline std::vector<std::pair<std::string, std::string>> particle_look_options(const ParticleLook& look) {
    std::vector<std::pair<std::string, std::string>> out = {{"plain", "Plain (soft dots, fastest)"}};
    if (look.simple.empty()) return out;
    if (look.simple == look.better && !look.better_covers) {
        out.emplace_back("better", "Textured");
        return out;
    }
    out.emplace_back("simple", "Simple (the older look)");
    out.emplace_back("better", "Better (softer, more detailed)");
    return out;
}

inline std::string particle_look_of(const Emitter& e) {
    const ParticleLook look = particle_look_for(e.texture, e.preset);
    if (e.texture == "builtin:glow") return "plain";
    if (look.simple.empty()) return "custom";
    const bool covers = e.blend == ParticleBlend::alpha;
    if (look.better_covers) return covers ? "better" : "simple";
    const std::string t = e.texture.empty() ? std::string(builtin_particle_texture_for_preset(e.preset) ? builtin_particle_texture_for_preset(e.preset) : "") : e.texture;
    if (t == look.better) return "better";
    if (t == look.simple) return "simple";
    return "custom";
}

inline bool set_particle_look(Emitter& e, std::string_view which) {
    const ParticleLook look = particle_look_for(e.texture, e.preset);
    if (which == "plain") {
        e.texture = "builtin:glow";
        return true;
    }
    if (look.simple.empty()) return false;
    const bool better = which == "better";
    e.texture = better ? look.better : look.simple;
    if (look.better_covers) e.blend = better ? ParticleBlend::alpha : ParticleBlend::additive;
    return true;
}

struct ParticlePixels {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

ParticlePixels generate_builtin_particle_texture(std::string_view name);

struct BuiltinTerrainTexture {
    const char* name;
    const char* label;
    float metres;
};

inline const std::vector<BuiltinTerrainTexture>& builtin_terrain_textures() {
    static const std::vector<BuiltinTerrainTexture> all = {
        {"builtin:grass", "Grass", 4.0f},
        {"builtin:dirt", "Dirt and pebbles", 3.0f},
        {"builtin:rock", "Rock", 6.0f},
        {"builtin:sand", "Sand", 4.0f},
        {"builtin:snow", "Snow", 5.0f},
    };
    return all;
}

inline const BuiltinTerrainTexture* find_builtin_terrain_texture(std::string_view name) {
    for (const BuiltinTerrainTexture& t : builtin_terrain_textures())
        if (name == t.name) return &t;
    return nullptr;
}

inline bool is_builtin_terrain_texture(std::string_view name) {
    return find_builtin_terrain_texture(name) != nullptr;
}

inline bool is_builtin_texture(std::string_view name) {
    return is_builtin_particle_texture(name) || is_builtin_terrain_texture(name);
}

struct TerrainPixels {
    ParticlePixels albedo;
    ParticlePixels normal;
};

TerrainPixels generate_builtin_terrain(std::string_view name, int size = 512);
ParticlePixels generate_builtin_terrain_texture(std::string_view name, int size = 512);
ParticlePixels generate_builtin_terrain_normal(std::string_view name, int size = 512);

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_PARTICLE_TEXTURES_HPP
