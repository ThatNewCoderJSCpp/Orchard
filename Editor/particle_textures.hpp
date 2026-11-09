#ifndef ORCHARD_EDITOR_PARTICLE_TEXTURES_HPP
#define ORCHARD_EDITOR_PARTICLE_TEXTURES_HPP

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
        {"sparks", "builtin:spark"}, {"smoke", "builtin:smoke"}, {"fire", "builtin:fire"}, {"explosion", "builtin:explosion"},
        {"dust", "builtin:dust"}, {"rain", "builtin:rain"}, {"snow", "builtin:snow"}, {"magic", "builtin:magic"},
        {"debris", "builtin:debris"}, {"bubbles", "builtin:bubble"}, {"steam", "builtin:steam"}, {"embers", "builtin:ember"},
    };
    for (const Pair& p : pairs)
        if (preset == p.preset) return p.texture;
    return nullptr;
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
