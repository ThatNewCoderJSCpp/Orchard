#ifndef ORCHARD_SURFACES_HPP
#define ORCHARD_SURFACES_HPP

#include "math.hpp"

#include <map>
#include <string>
#include <vector>

namespace orchard {

struct SurfaceDef {
    std::string id;
    std::string name;
    float walk = 1.0f;
    float walk_minus = 0.0f;
    float wade = 1.0f;
    float jump = 1.0f;
    float stamina = 1.0f;
    float slip = 0.0f;
    float ricochet = 1.0f;
    float ricochet_angle = 15.0f;
    float resistance = 6.0f;
    float slows_bullets = 0.5f;
    float noise = 1.0f;
    bool volume = false;
    bool soft = false;
    std::string step_sound = "step_concrete";
    std::string impact = "impact";
    std::string impact_sound = "hit_concrete";
    Vec3 color{0.6f, 0.6f, 0.6f};
};

const std::vector<SurfaceDef>& surface_library();
const SurfaceDef* builtin_surface(const std::string& id);
std::vector<std::string> surface_ids();
std::string guess_surface(const std::string& text);
bool set_surface_field(SurfaceDef& s, const std::string& key, const std::string& value);
std::string surface_field(const SurfaceDef& s, const std::string& key);
const std::vector<std::string>& surface_field_keys();
bool parse_speed_change(const std::string& text, float& multiplier, float& minus);
std::string speed_change_text(float multiplier, float minus);
float ease_for(const std::string& ease_list, const std::string& surface);

struct SurfaceCatalog {
    std::map<std::string, SurfaceDef> custom;
    const SurfaceDef* find(const std::string& id) const;
    SurfaceDef& define(const std::string& id);
};

} // namespace orchard

#endif // ORCHARD_SURFACES_HPP
