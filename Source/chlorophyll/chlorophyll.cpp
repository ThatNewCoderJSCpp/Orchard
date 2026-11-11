#include <chlorophyll/chlorophyll.hpp>
#include <orchard/solid.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <unordered_map>

#ifdef CHLOROPHYLL_WITH_MODELS
#include <fizmo/../../Source/Util Hpp/util_functions.hpp>
#include <fizmo/../../Source/Graphics/model_3d.hpp>
#endif // CHLOROPHYLL_WITH_MODELS

namespace chlorophyll {

namespace {

std::string dashed(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '_' || c == ' ') c = '-';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        out.push_back(c);
    }
    return out;
}

bool to_bool(const std::string& v, bool& out) {
    const std::string s = dashed(v);
    if (s == "on" || s == "true" || s == "yes" || s == "1") {
        out = true;
        return true;
    }
    if (s == "off" || s == "false" || s == "no" || s == "0") {
        out = false;
        return true;
    }
    return false;
}

bool to_float(const std::string& v, float& out) {
    char* end = nullptr;
    const double d = std::strtod(v.c_str(), &end);
    if (end == v.c_str()) return false;
    out = static_cast<float>(d);
    return true;
}

bool to_int(const std::string& v, int& out) {
    char* end = nullptr;
    const long l = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str()) return false;
    out = static_cast<int>(l);
    return true;
}

bool to_color(const std::string& v, fl::Rgb& out) {
    if (!v.empty() && v[0] == '#' && (v.size() == 7 || v.size() == 4)) {
        unsigned x = 0;
        std::string hex = v.substr(1);
        if (hex.size() == 3) hex = std::string{ hex[0], hex[0], hex[1], hex[1], hex[2], hex[2] };
        if (std::sscanf(hex.c_str(), "%x", &x) != 1) return false;
        out = fl::Rgb::from_hex(x);
        return true;
    }
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (std::sscanf(v.c_str(), "%f,%f,%f", &r, &g, &b) == 3 || std::sscanf(v.c_str(), "%f %f %f", &r, &g, &b) == 3) {
        out = fl::Rgb(r, g, b);
        return true;
    }
    if (std::sscanf(v.c_str(), "%f", &r) == 1) {
        out = fl::Rgb(r);
        return true;
    }
    return false;
}

fl::Rgb to_rgb(orchard::Vec3 v) {
    return fl::Rgb(v.x, v.y, v.z);
}

orchard::Vec3 transform_point(const orchard::Mat4& m, orchard::Vec3 p) {
    const orchard::Vec4 r = m * orchard::Vec4{ p.x, p.y, p.z, 1.0f };
    return { r.x, r.y, r.z };
}

orchard::Vec3 transform_vector(const orchard::Mat4& m, orchard::Vec3 v) {
    const orchard::Vec4 r = m * orchard::Vec4{ v.x, v.y, v.z, 0.0f };
    return { r.x, r.y, r.z };
}

fl::Vec3 fv(orchard::Vec3 v) {
    return fl::Vec3{ v.x, v.y, v.z };
}

fl::Transform to_transform(const orchard::Mat4& m) {
    float rows[12];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) rows[r * 4 + c] = m.at(r, c);
    }
    return fl::Transform::from_rows(rows);
}

bool same_light(const fl::Light& a, const fl::Light& b) {
    return a.enabled == b.enabled && a.shape == b.shape && a.mobility == b.mobility && a.position == b.position && a.direction == b.direction && a.up == b.up
        && a.color == b.color && a.temperature == b.temperature && a.intensity == b.intensity && a.unit == b.unit && a.range == b.range && a.falloff == b.falloff
        && a.falloff_exponent == b.falloff_exponent && a.spread == b.spread && a.spill == b.spill && a.radius == b.radius && a.length == b.length && a.width == b.width && a.height == b.height && a.two_sided == b.two_sided
        && a.inner_cone == b.inner_cone && a.outer_cone == b.outer_cone && a.angular_diameter == b.angular_diameter && a.casts_shadows == b.casts_shadows
        && a.shadow_strength == b.shadow_strength && a.shadow_bias == b.shadow_bias && a.shadow_samples == b.shadow_samples && a.diffuse == b.diffuse && a.specular == b.specular
        && a.indirect == b.indirect && a.volumetric == b.volumetric && a.visible == b.visible && a.channels == b.channels && a.cookie_size == b.cookie_size;
}

bool is_camera(const orchard::Scene& scene, orchard::Entity e) {
    return scene.name(e).find("Camera") != std::string::npos;
}

orchard::Mat4 scale_matrix(orchard::Vec3 s) {
    orchard::Mat4 m;
    m.at(0, 0) = s.x;
    m.at(1, 1) = s.y;
    m.at(2, 2) = s.z;
    return m;
}

orchard::Mat4 translate_matrix(orchard::Vec3 t) {
    orchard::Mat4 m;
    m.at(0, 3) = t.x;
    m.at(1, 3) = t.y;
    m.at(2, 3) = t.z;
    return m;
}

const std::map<std::string, const char*>& setting_help() {
    static const std::map<std::string, const char*> all = {
        { "shadows", "none, hard or soft" },
        { "shadow-casters", "sun-only, flagged or everything" },
        { "shadow-samples", "rays per area light per pixel" },
        { "shadow-bias", "meters" },
        { "gi", "none, ambient, baked, probes or path-traced" },
        { "ambient", "sky light strength" },
        { "ambient-occlusion", "rays per pixel, 0 for none" },
        { "ao-distance", "meters" },
        { "gi-intensity", "bounced light strength" },
        { "probe-spacing", "meters between light probes" },
        { "probe-rays", "rays per probe per frame" },
        { "probe-bounces", "bounces when lights change" },
        { "probe-hysteresis", "0 to 1, how slowly probes change" },
        { "reflections", "none, environment or ray-traced" },
        { "reflection-bounces", "mirror-in-mirror depth" },
        { "refraction", "on or off" },
        { "dispersion", "on or off, rainbows from glass" },
        { "transmission-depth", "glass layers to see through" },
        { "transport", "rgb or spectral" },
        { "bounces", "path length" },
        { "samples", "samples per pixel per frame" },
        { "russian-roulette", "bounce where paths may stop early" },
        { "firefly-clamp", "0 for none" },
        { "light-samples", "lights sampled per point when there are many" },
        { "all-lights-threshold", "sample every light when there are this few" },
        { "caustics", "on or off, photon-mapped caustics" },
        { "photons", "photons per frame" },
        { "photon-bundle", "wavelengths per photon pack" },
        { "photon-radius", "meters" },
        { "photon-bounces", "bounces per photon" },
        { "denoise", "on or off" },
        { "denoise-passes", "1 to 5" },
        { "area-lights", "on or off, off treats shaped lights as points" },
        { "emissive-lights", "on or off, glowing materials light things" },
        { "ies", "on or off" },
        { "cookies", "on or off" },
        { "baked-static", "on or off" },
        { "fog", "on or off" },
        { "fog-density", "per meter" },
        { "fog-color", "#rrggbb or r,g,b" },
        { "fog-anisotropy", "-1 to 1, forward glow around lights" },
        { "fog-height", "meters" },
        { "fog-falloff", "per meter, 0 for even fog" },
        { "fog-distance", "meters" },
        { "fog-steps", "ray-march steps" },
        { "exposure", "brightness multiplier" },
        { "ev100", "camera exposure value, sets exposure" },
        { "tone-mapper", "none, reinhard, aces, filmic or agx" },
        { "white-balance", "kelvin, 0 for none" },
        { "saturation", "1 is unchanged" },
        { "contrast", "1 is unchanged" },
        { "threads", "0 for every core" },
        { "seed", "random seed" },
        { "sky", "none, color, gradient or physical" },
        { "sky-color", "#rrggbb or r,g,b" },
        { "sky-zenith", "#rrggbb or r,g,b" },
        { "sky-horizon", "#rrggbb or r,g,b" },
        { "sky-ground", "#rrggbb or r,g,b" },
        { "sky-intensity", "multiplier" },
        { "sky-turbidity", "haze for the physical sky" },
        { "sky-glow", "sun glow in the gradient sky" },
        { "sky-visible", "on or off" },
        { "sky-lights-scene", "on or off" },
    };
    return all;
}

} // namespace

fl::LightingQuality to_fizmo(orchard::LightingLevel level) {
    switch (level) {
    case orchard::LightingLevel::off: return fl::LightingQuality::off;
    case orchard::LightingLevel::simple: return fl::LightingQuality::simple;
    case orchard::LightingLevel::shadowed: return fl::LightingQuality::shadowed;
    case orchard::LightingLevel::realistic: return fl::LightingQuality::realistic;
    case orchard::LightingLevel::path_traced: return fl::LightingQuality::path_traced;
    case orchard::LightingLevel::photon: return fl::LightingQuality::photon;
    }
    return fl::LightingQuality::shadowed;
}

fl::Light to_light(const orchard::Light& l, const orchard::Mat4& world, const orchard::AxisSystem& axes, double seconds) {
    fl::Light out = to_light(l, world, axes);
    if (!l.animation.active()) return out;
    const orchard::LightAnimationSample a = l.sample(seconds);
    out.color = to_rgb(a.color);
    out.intensity = l.intensity * a.brightness;
    return out;
}

fl::Light to_light(const orchard::Light& l, const orchard::Mat4& world, const orchard::AxisSystem& axes) {
    const orchard::Mat4 m = axes.to_render_matrix() * world;
    fl::Light out;
    out.enabled = l.enabled;
    out.position = fv(transform_point(m, { 0.0f, 0.0f, 0.0f }));
    out.direction = fv(transform_vector(m, axes.forward())).normalized();
    out.up = fv(transform_vector(m, axes.up())).normalized();
    const fl::Vec3 right = fv(transform_vector(m, axes.right())).normalized();
    switch (l.type) {
    case orchard::LightType::point: out.shape = fl::LightShape::point; break;
    case orchard::LightType::spot: out.shape = fl::LightShape::spot; break;
    case orchard::LightType::directional: out.shape = fl::LightShape::directional; break;
    case orchard::LightType::sphere: out.shape = fl::LightShape::sphere; break;
    case orchard::LightType::disk: out.shape = fl::LightShape::disk; break;
    case orchard::LightType::rectangle: out.shape = fl::LightShape::rectangle; break;
    case orchard::LightType::tube:
        out.shape = fl::LightShape::tube;
        out.direction = right;
        break;
    }
    switch (l.mobility) {
    case orchard::LightMobility::dynamic: out.mobility = fl::LightMobility::dynamic; break;
    case orchard::LightMobility::stationary: out.mobility = fl::LightMobility::stationary; break;
    case orchard::LightMobility::baked: out.mobility = fl::LightMobility::baked; break;
    }
    switch (l.unit) {
    case orchard::LightUnit::unitless: out.unit = fl::LightUnit::unitless; break;
    case orchard::LightUnit::candela: out.unit = fl::LightUnit::candela; break;
    case orchard::LightUnit::lumens: out.unit = fl::LightUnit::lumens; break;
    case orchard::LightUnit::lux: out.unit = fl::LightUnit::lux; break;
    case orchard::LightUnit::nits: out.unit = fl::LightUnit::nits; break;
    case orchard::LightUnit::watts: out.unit = fl::LightUnit::watts; break;
    }
    switch (l.falloff) {
    case orchard::LightFalloff::physical: out.falloff = fl::Falloff::physical; break;
    case orchard::LightFalloff::linear: out.falloff = fl::Falloff::linear; break;
    case orchard::LightFalloff::smooth: out.falloff = fl::Falloff::smooth; break;
    case orchard::LightFalloff::constant: out.falloff = fl::Falloff::constant; break;
    case orchard::LightFalloff::custom: out.falloff = fl::Falloff::custom; break;
    }
    out.color = to_rgb(l.color);
    out.temperature = l.temperature;
    out.intensity = l.intensity;
    out.range = l.range;
    out.falloff_exponent = l.falloff_exponent;
    out.spread = l.spread == orchard::LightSpread::blob ? fl::LightSpread::blob : fl::LightSpread::natural;
    out.spill = l.spill;
    if (l.spread == orchard::LightSpread::blob) {
        out.falloff = fl::Falloff::smooth;
        if (out.range <= 0.0f) out.range = std::max(1.0f, std::sqrt(std::max(l.intensity, 0.0f) * 16.0f));
    }
    out.radius = l.radius;
    out.length = l.length;
    out.width = l.width;
    out.height = l.height;
    out.two_sided = l.two_sided;
    if (l.type == orchard::LightType::spot) {
        out.outer_cone = l.outer_cone;
        out.inner_cone = std::min(l.inner_cone, l.outer_cone);
    }
    out.angular_diameter = l.angular_diameter;
    out.casts_shadows = l.shadows;
    out.shadow_strength = l.shadow_strength;
    out.shadow_bias = l.shadow_bias;
    out.shadow_samples = l.shadow_samples;
    out.diffuse = l.diffuse;
    out.specular = l.specular;
    out.indirect = l.indirect;
    out.volumetric = l.volumetric;
    out.visible = l.visible;
    out.channels = l.channels;
    out.cookie_size = l.cookie_size;
    return out;
}

fl::Material to_material(const orchard::Material& m) {
    fl::Material out;
    out.name = m.preset;
    out.base_color = to_rgb(m.color);
    out.metallic = m.metallic;
    out.roughness = m.roughness * (1.0f - std::clamp(m.reflectivity, 0.0f, 1.0f));
    out.specular = std::max(m.specular, 0.5f + 0.5f * std::clamp(m.reflectivity, 0.0f, 1.0f) * (1.0f - m.metallic));
    out.emission = to_rgb(m.emission);
    out.emission_strength = m.emission_strength;
    out.transmission = m.transmission;
    out.ior = m.ior;
    out.abbe = m.abbe;
    out.thin_walled = m.thin;
    out.absorption_color = to_rgb(m.absorption_color);
    out.absorption_distance = m.absorption_distance;
    out.opacity = m.opacity;
    out.double_sided = m.double_sided;
    return out;
}

fl::Material to_material(const orchard::Material& m, const TextureLoader& textures) {
    fl::Material out = to_material(m);
    if (!textures || !m.has_textures()) return out;
    auto map = [&](const std::string& path, bool color, int channel) {
        fl::TextureMap t;
        if (path.empty()) return t;
        t.texture = textures(path, color);
        t.channel = channel;
        t.filter = m.pixelated ? fl::TextureFilter::nearest : fl::TextureFilter::bilinear;
        if (m.triplanar) {
            t.triplanar = true;
            t.triplanar_scale = 1.0f / std::max(0.01f, m.triplanar_scale);
        }
        return t;
    };
    out.uv = fl::UvTransform::make({m.tiling_u, m.tiling_v}, {m.offset_u, m.offset_v}, m.uv_rotation * orchard::pi / 180.0f);
    out.base_color_map = map(m.base_map, true, 0);
    out.normal_map = map(m.normal_map, false, 0);
    out.normal_map.triplanar = false;
    out.normal_map.strength = m.normal_strength;
    if (!m.orm_map.empty()) {
        out.roughness_map = map(m.orm_map, false, 1);
        out.metallic_map = map(m.orm_map, false, 2);
    } else {
        out.roughness_map = map(m.roughness_map, false, 0);
        out.metallic_map = map(m.metallic_map, false, 0);
    }
    if (out.roughness_map.valid()) out.roughness = 1.0f - std::clamp(m.reflectivity, 0.0f, 1.0f);
    if (out.metallic_map.valid()) out.metallic = 1.0f;
    out.emission_map = map(m.emission_map, true, 0);
    if (out.emission_map.valid() && out.emission.r <= 0.0f && out.emission.g <= 0.0f && out.emission.b <= 0.0f) out.emission = fl::Rgb(1.0f);
    out.opacity_map = map(m.opacity_map, false, 0);
    out.alpha_cutoff = std::clamp(m.alpha_cutoff, 0.0f, 1.0f);
    return out;
}

const std::vector<std::string>& setting_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (const auto& kv : setting_help()) v.push_back(kv.first);
        return v;
    }();
    return names;
}

bool apply_setting(fl::LightingSettings& s, fl::Environment& env, const std::string& raw_key, const std::string& value, std::string* problem) {
    const std::string key = dashed(raw_key);
    auto bad = [&](const std::string& why) {
        if (problem) *problem = key + ": " + why;
        return false;
    };
    float f = 0.0f;
    int i = 0;
    bool b = false;
    fl::Rgb c;
    auto need_float = [&]() { return to_float(value, f) ? true : bad("expected a number"); };
    auto need_int = [&]() { return to_int(value, i) ? true : bad("expected a whole number"); };
    auto need_bool = [&]() { return to_bool(value, b) ? true : bad("expected on or off"); };
    auto need_color = [&]() { return to_color(value, c) ? true : bad("expected #rrggbb or r,g,b"); };
    if (key == "shadows") {
        const auto v = fl::parse_shadow_method(value);
        if (!v) return bad("expected none, hard or soft");
        s.shadows = *v;
    } else if (key == "shadow-casters") {
        const auto v = fl::parse_shadow_casters(value);
        if (!v) return bad("expected sun-only, flagged or everything");
        s.shadow_casters = *v;
    } else if (key == "shadow-samples") {
        if (!need_int()) return false;
        s.shadow_samples = std::max(i, 1);
    } else if (key == "shadow-bias") {
        if (!need_float()) return false;
        s.shadow_bias = f;
    } else if (key == "gi") {
        const auto v = fl::parse_global_illumination(value);
        if (!v) return bad("expected none, ambient, baked, probes or path-traced");
        s.gi = *v;
    } else if (key == "ambient") {
        if (!need_float()) return false;
        s.ambient = f;
    } else if (key == "ambient-occlusion") {
        if (!need_int()) return false;
        s.ambient_occlusion_samples = std::max(i, 0);
    } else if (key == "ao-distance") {
        if (!need_float()) return false;
        s.ambient_occlusion_distance = f;
    } else if (key == "gi-intensity") {
        if (!need_float()) return false;
        s.gi_intensity = f;
    } else if (key == "probe-spacing") {
        if (!need_float()) return false;
        if (f <= 0.0f) return bad("must be above 0");
        s.probe_spacing = f;
    } else if (key == "probe-rays") {
        if (!need_int()) return false;
        s.probe_rays = std::max(i, 8);
    } else if (key == "probe-bounces") {
        if (!need_int()) return false;
        s.probe_bounces = std::max(i, 1);
    } else if (key == "probe-hysteresis") {
        if (!need_float()) return false;
        s.probe_hysteresis = std::clamp(f, 0.0f, 0.99f);
    } else if (key == "reflections") {
        const auto v = fl::parse_reflections(value);
        if (!v) return bad("expected none, environment or ray-traced");
        s.reflections = *v;
    } else if (key == "reflection-bounces") {
        if (!need_int()) return false;
        s.reflection_bounces = std::max(i, 0);
    } else if (key == "refraction") {
        if (!need_bool()) return false;
        s.refraction = b;
    } else if (key == "dispersion") {
        if (!need_bool()) return false;
        s.dispersion = b;
    } else if (key == "transmission-depth") {
        if (!need_int()) return false;
        s.transmission_depth = std::max(i, 0);
    } else if (key == "transport") {
        const auto v = fl::parse_transport(value);
        if (!v) return bad("expected rgb or spectral");
        s.transport = *v;
    } else if (key == "bounces") {
        if (!need_int()) return false;
        s.max_bounces = std::max(i, 1);
    } else if (key == "samples") {
        if (!need_int()) return false;
        s.samples_per_pixel = std::max(i, 1);
    } else if (key == "russian-roulette") {
        if (!need_int()) return false;
        s.russian_roulette_depth = std::max(i, 0);
    } else if (key == "firefly-clamp") {
        if (!need_float()) return false;
        s.firefly_clamp = std::max(f, 0.0f);
    } else if (key == "light-samples") {
        if (!need_int()) return false;
        s.light_samples = std::max(i, 1);
    } else if (key == "all-lights-threshold") {
        if (!need_int()) return false;
        s.all_lights_threshold = std::max(i, 0);
    } else if (key == "caustics") {
        if (!need_bool()) return false;
        s.caustics = b;
    } else if (key == "photons") {
        if (!need_int()) return false;
        s.photons = static_cast<std::size_t>(std::max(i, 0));
    } else if (key == "photon-bundle") {
        if (!need_int()) return false;
        s.photon_bundle = std::max(i, 1);
    } else if (key == "photon-radius") {
        if (!need_float()) return false;
        if (f <= 0.0f) return bad("must be above 0");
        s.photon_radius = f;
    } else if (key == "photon-bounces") {
        if (!need_int()) return false;
        s.photon_bounces = std::max(i, 1);
    } else if (key == "denoise") {
        if (!need_bool()) return false;
        s.denoise = b;
    } else if (key == "denoise-passes") {
        if (!need_int()) return false;
        s.denoise_passes = std::clamp(i, 1, 5);
    } else if (key == "area-lights") {
        if (!need_bool()) return false;
        s.area_lights = b;
    } else if (key == "emissive-lights") {
        if (!need_bool()) return false;
        s.emissive_lights = b;
    } else if (key == "ies") {
        if (!need_bool()) return false;
        s.use_ies = b;
    } else if (key == "cookies") {
        if (!need_bool()) return false;
        s.use_cookies = b;
    } else if (key == "baked-static") {
        if (!need_bool()) return false;
        s.baked_static = b;
    } else if (key == "fog") {
        if (!need_bool()) return false;
        s.fog.enabled = b;
    } else if (key == "fog-density") {
        if (!need_float()) return false;
        s.fog.density = std::max(f, 0.0f);
    } else if (key == "fog-color") {
        if (!need_color()) return false;
        s.fog.color = c;
    } else if (key == "fog-anisotropy") {
        if (!need_float()) return false;
        s.fog.anisotropy = std::clamp(f, -0.99f, 0.99f);
    } else if (key == "fog-height") {
        if (!need_float()) return false;
        s.fog.height = f;
    } else if (key == "fog-falloff") {
        if (!need_float()) return false;
        s.fog.height_falloff = std::max(f, 0.0f);
    } else if (key == "fog-distance") {
        if (!need_float()) return false;
        s.fog.max_distance = std::max(f, 0.0f);
    } else if (key == "fog-steps") {
        if (!need_int()) return false;
        s.fog.steps = std::max(i, 1);
    } else if (key == "exposure") {
        if (!need_float()) return false;
        s.exposure = std::max(f, 0.0f);
    } else if (key == "ev100") {
        if (!need_float()) return false;
        s.exposure = fl::exposure_from_ev100(f);
    } else if (key == "tone-mapper") {
        const auto v = fl::parse_tone_mapper(value);
        if (!v) return bad("expected none, reinhard, aces, filmic or agx");
        s.tone_mapper = *v;
    } else if (key == "white-balance") {
        if (!need_float()) return false;
        s.white_balance = std::max(f, 0.0f);
    } else if (key == "saturation") {
        if (!need_float()) return false;
        s.saturation = std::max(f, 0.0f);
    } else if (key == "contrast") {
        if (!need_float()) return false;
        s.contrast = std::max(f, 0.0f);
    } else if (key == "threads") {
        if (!need_int()) return false;
        s.threads = static_cast<unsigned>(std::max(i, 0));
    } else if (key == "seed") {
        if (!need_int()) return false;
        s.seed = static_cast<std::uint64_t>(std::max(i, 0));
    } else if (key == "sky") {
        const auto v = fl::parse_sky_kind(value);
        if (!v || *v == fl::SkyKind::image) return bad("expected none, color, gradient or physical");
        env.kind = *v;
    } else if (key == "sky-color") {
        if (!need_color()) return false;
        env.color = c;
    } else if (key == "sky-zenith") {
        if (!need_color()) return false;
        env.zenith = c;
    } else if (key == "sky-horizon") {
        if (!need_color()) return false;
        env.horizon = c;
    } else if (key == "sky-ground") {
        if (!need_color()) return false;
        env.ground = c;
    } else if (key == "sky-intensity") {
        if (!need_float()) return false;
        env.intensity = std::max(f, 0.0f);
    } else if (key == "sky-turbidity") {
        if (!need_float()) return false;
        env.turbidity = std::max(f, 1.0f);
    } else if (key == "sky-glow") {
        if (!need_float()) return false;
        env.sun_glow = std::max(f, 0.0f);
    } else if (key == "sky-visible") {
        if (!need_bool()) return false;
        env.visible = b;
    } else if (key == "sky-lights-scene") {
        if (!need_bool()) return false;
        env.lights_scene = b;
    } else {
        return bad("unknown lighting setting");
    }
    return true;
}

fl::LightingSettings settings_for(const orchard::ProjectInfo& info, orchard::LightingLevel level, std::vector<std::string>* problems) {
    fl::LightingSettings s = fl::LightingSettings::preset(to_fizmo(level));
    fl::Environment ignored;
    const std::string prefix = std::string(orchard::to_string(level)) + ".";
    for (const auto& kv : info.lighting.settings) {
        std::string key = kv.first;
        const std::size_t dot = key.find('.');
        if (dot != std::string::npos) {
            const auto scope = orchard::parse_lighting_level(key.substr(0, dot));
            if (!scope) {
                if (problems) problems->push_back(kv.first + ": unknown quality '" + key.substr(0, dot) + "'");
                continue;
            }
            if (*scope != level) continue;
            key = key.substr(dot + 1);
        }
        if (key.rfind("sky", 0) == 0) continue;
        std::string why;
        if (!apply_setting(s, ignored, key, kv.second, &why) && problems) problems->push_back(why);
    }
    return s;
}

fl::Environment environment_for(const orchard::ProjectInfo& info, std::vector<std::string>* problems) {
    fl::Environment env;
    fl::LightingSettings ignored;
    for (const auto& kv : info.lighting.settings) {
        if (kv.first.rfind("sky", 0) != 0) continue;
        std::string why;
        if (!apply_setting(ignored, env, kv.first, kv.second, &why) && problems) problems->push_back(why);
    }
    env.up = fl::Vec3{ 0.0, 1.0, 0.0 };
    return env;
}

#ifdef CHLOROPHYLL_WITH_MODELS
namespace fg = fizmo::graphics;
namespace mm = fizmo::graphics::model_math;

namespace {

struct TextureKey {
    const void* image = nullptr;
    bool srgb = true;
    bool alpha = true;
    bool operator==(const TextureKey& o) const noexcept { return image == o.image && srgb == o.srgb && alpha == o.alpha; }
};

struct TextureKeyHash {
    std::size_t operator()(const TextureKey& k) const noexcept {
        return std::hash<const void*>()(k.image) ^ (k.srgb ? 0x9E37u : 0u) ^ (k.alpha ? 0x79B9u : 0u);
    }
};

struct TextureSlot {
    std::shared_ptr<const fizmo::images::BitmapImage> pin;
    std::shared_ptr<const fl::Texture> texture;
};

std::mutex& model_mutex() {
    static std::mutex m;
    return m;
}

std::unordered_map<TextureKey, TextureSlot, TextureKeyHash>& texture_slots() {
    static std::unordered_map<TextureKey, TextureSlot, TextureKeyHash> slots;
    return slots;
}

struct FileSlot {
    std::filesystem::file_time_type stamp{};
    std::shared_ptr<const ModelGeometry> geometry;
    std::string error;
};

std::unordered_map<std::string, FileSlot>& file_slots() {
    static std::unordered_map<std::string, FileSlot> slots;
    return slots;
}

std::uint64_t next_revision() {
    static std::atomic<std::uint64_t> counter{ 0 };
    return ++counter;
}

std::shared_ptr<const fl::Texture> lighting_texture(const fg::Texture& tex, bool srgb, bool alpha) {
    const std::shared_ptr<const fizmo::images::BitmapImage>& img = tex.shared_image();
    if (!img || !img->is_valid_image()) return nullptr;
    const TextureKey key{ img.get(), srgb, alpha };
    std::lock_guard<std::mutex> lock(model_mutex());
    auto& slots = texture_slots();
    const auto it = slots.find(key);
    if (it != slots.end()) return it->second.texture;
    std::vector<std::uint8_t> rgba = img->to_rgba();
    if (!alpha)
        for (std::size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    auto out = std::make_shared<fl::Texture>(fl::Texture::from_rgba8(img->width(), img->height(), std::move(rgba), srgb ? fl::ColorSpace::srgb : fl::ColorSpace::linear));
    slots[key] = TextureSlot{ img, out };
    return out;
}

fl::TextureMap texture_map(const fg::Model3D& model, int index, bool srgb, bool alpha, int channel) {
    fl::TextureMap map;
    if (index < 0 || static_cast<std::size_t>(index) >= model.textures.size()) return map;
    const fg::Texture& tex = model.textures[static_cast<std::size_t>(index)];
    map.texture = lighting_texture(tex, srgb, alpha);
    map.channel = channel;
    map.filter = tex.filter() == fg::SampleFilter::Nearest ? fl::TextureFilter::nearest : fl::TextureFilter::bilinear;
    map.wrap = tex.wrap() == fg::WrapMode::Clamp ? fl::TextureWrap::clamp : tex.wrap() == fg::WrapMode::MirrorRepeat ? fl::TextureWrap::mirror : fl::TextureWrap::repeat;
    return map;
}

float byte01(std::uint8_t v) {
    return static_cast<float>(v) / 255.0f;
}

void normal_matrix(const mm::Mat4& m, double out[9]) {
    const mm::Mat4 inv = mm::inverse(m);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) out[r * 3 + c] = inv[static_cast<std::size_t>(c * 4 + r)];
}

struct MaterialSlots {
    const fg::Model3D& model;
    ModelGeometry& out;
    std::vector<std::size_t> plain;
    std::vector<std::size_t> colored;

    MaterialSlots(const fg::Model3D& m, ModelGeometry& g) : model(m), out(g), plain(m.materials.size() + 1, SIZE_MAX), colored(m.materials.size() + 1, SIZE_MAX) {}

    std::size_t get(int material, bool vertex_colors) {
        const std::size_t k = material >= 0 && static_cast<std::size_t>(material) < model.materials.size() ? static_cast<std::size_t>(material) : model.materials.size();
        std::vector<std::size_t>& slots = vertex_colors ? colored : plain;
        if (slots[k] != SIZE_MAX) return slots[k];
        fl::Material mat = k < model.materials.size() ? to_material(model.materials[k], model) : fl::Material();
        if (k >= model.materials.size()) {
            mat.base_color = fl::Rgb(1.0f);
            mat.roughness = 0.8f;
        }
        if (vertex_colors) {
            mat.base_color = fl::Rgb(1.0f);
            mat.opacity = 1.0f;
        }
        slots[k] = out.materials.size();
        out.materials.push_back(std::move(mat));
        return slots[k];
    }
};

bool needs_tangents(const fg::Model3D& model, int material) {
    return material >= 0 && static_cast<std::size_t>(material) < model.materials.size() && model.materials[static_cast<std::size_t>(material)].normal_texture >= 0;
}

void add_primitive(const fg::Model3D& model, const fg::ModelPrimitive& prim, const fg::Mesh3D& source, const mm::Mat4* xf, MaterialSlots& slots, ModelGeometry& out) {
    const auto& verts = source.vertices();
    if (verts.empty()) return;
    ModelPiece piece;
    fl::Mesh& m = piece.mesh;
    m.positions.reserve(verts.size());
    m.normals.reserve(verts.size());
    m.uvs.reserve(verts.size());
    double nm[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    if (xf) normal_matrix(*xf, nm);
    bool any_normal = false;
    const fg::Color factor = prim.material >= 0 && static_cast<std::size_t>(prim.material) < model.materials.size() ? model.materials[static_cast<std::size_t>(prim.material)].base_color : fg::Color(255, 255, 255, 255);
    const std::uint32_t factor_rgba = fg::Vertex3D::pack(factor);
    bool uniform = true;
    for (const fg::Vertex3D& v : verts) {
        if (xf) {
            const auto p = mm::point(*xf, v.x, v.y, v.z);
            m.positions.push_back({ static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z) });
            const fl::Vec3f n{ static_cast<float>(nm[0] * v.nx + nm[1] * v.ny + nm[2] * v.nz), static_cast<float>(nm[3] * v.nx + nm[4] * v.ny + nm[5] * v.nz), static_cast<float>(nm[6] * v.nx + nm[7] * v.ny + nm[8] * v.nz) };
            m.normals.push_back(fl::normalize(n));
        } else {
            m.positions.push_back({ v.x, v.y, v.z });
            m.normals.push_back(fl::normalize(fl::Vec3f{ v.nx, v.ny, v.nz }));
        }
        any_normal = any_normal || v.nx != 0.0f || v.ny != 0.0f || v.nz != 0.0f;
        m.uvs.push_back({ v.u, v.v });
        uniform = uniform && v.rgba == factor_rgba;
    }
    const bool blend = prim.material >= 0 && static_cast<std::size_t>(prim.material) < model.materials.size() && model.materials[static_cast<std::size_t>(prim.material)].transparent;
    if (!uniform) {
        m.colors.reserve(verts.size());
        for (const fg::Vertex3D& v : verts) {
            const fg::Color c = v.color();
            m.colors.push_back({ byte01(c.red()), byte01(c.green()), byte01(c.blue()), blend ? byte01(c.alpha()) : 1.0f });
        }
    }
    const auto& idx = source.indices();
    if (idx.empty()) {
        for (std::uint32_t i = 0; i + 2 < static_cast<std::uint32_t>(verts.size()); i += 3) m.indices.insert(m.indices.end(), { i, i + 1, i + 2 });
    } else {
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
            if (idx[i] < verts.size() && idx[i + 1] < verts.size() && idx[i + 2] < verts.size()) m.indices.insert(m.indices.end(), { idx[i], idx[i + 1], idx[i + 2] });
    }
    if (m.indices.empty()) return;
    if (!any_normal) m.compute_normals();
    if (needs_tangents(model, prim.material)) m.compute_tangents();
    piece.material = slots.get(prim.material, !uniform);
    out.pieces.push_back(std::move(piece));
}

std::shared_ptr<const ModelGeometry> convert(const fg::Model3D& model, const std::vector<mm::Mat4>& world, const fg::ModelAnimator* pose) {
    auto out = std::make_shared<ModelGeometry>();
    MaterialSlots slots(model, *out);
    for (std::size_t ni = 0; ni < model.nodes.size() && ni < world.size(); ++ni) {
        const fg::ModelNode& node = model.nodes[ni];
        if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= model.meshes.size()) continue;
        const auto& prims = model.meshes[static_cast<std::size_t>(node.mesh)].primitives;
        for (std::size_t p = 0; p < prims.size(); ++p) {
            const fg::Mesh3D* skinned = pose && node.skin >= 0 && prims[p].skinned() ? pose->skinned_mesh(static_cast<std::size_t>(node.mesh), p) : nullptr;
            if (skinned && !skinned->vertices().empty()) add_primitive(model, prims[p], *skinned, nullptr, slots, *out);
            else add_primitive(model, prims[p], prims[p].mesh, &world[ni], slots, *out);
        }
    }
    out->revision = next_revision();
    return out;
}

int find_clip(const fg::Model3D& model, const std::string& clip) {
    if (model.animations.empty()) return -1;
    if (clip.empty()) return 0;
    int index = model.find_animation(clip);
    if (index >= 0) return index;
    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string want = lower(clip);
    for (std::size_t i = 0; i < model.animations.size(); ++i)
        if (lower(model.animations[i].name) == want) return static_cast<int>(i);
    return -1;
}

} // namespace

bool models_supported() noexcept {
    return true;
}

fl::Material to_material(const fg::ModelMaterial& mat, const fg::Model3D& model) {
    fl::Material out;
    out.name = mat.name;
    out.base_color = fl::Rgb(byte01(mat.base_color.red()), byte01(mat.base_color.green()), byte01(mat.base_color.blue()));
    out.metallic = std::clamp(mat.metallic, 0.0f, 1.0f);
    out.roughness = std::clamp(mat.roughness, 0.02f, 1.0f);
    out.double_sided = mat.double_sided;
    const bool mask = mat.alpha_cutoff > 0.0f;
    out.opacity = mat.transparent || mask ? byte01(mat.base_color.alpha()) : 1.0f;
    out.alpha_cutoff = mask ? std::clamp(mat.alpha_cutoff, 0.0f, 1.0f) : 0.0f;
    if (mat.emissive_strength > 0.0f) {
        out.emission = fl::Rgb(byte01(mat.emissive.red()), byte01(mat.emissive.green()), byte01(mat.emissive.blue()));
        out.emission_strength = mat.emissive_strength;
    }
    if (mat.unlit) {
        out.emission = out.base_color;
        out.emission_strength = 1.0f;
        out.base_color = fl::Rgb(0.0f);
    }
    out.base_color_map = texture_map(model, mat.texture, true, mat.transparent || mask, 0);
    if (out.base_color_map.valid() && mat.unlit) out.emission_map = out.base_color_map;
    out.normal_map = texture_map(model, mat.normal_texture, false, false, 0);
    out.normal_map.strength = mat.normal_scale;
    if (mat.metallic_roughness_texture >= 0) {
        out.roughness_map = texture_map(model, mat.metallic_roughness_texture, false, false, 1);
        out.metallic_map = texture_map(model, mat.metallic_roughness_texture, false, false, 2);
        if (out.roughness_map.valid()) out.roughness = std::clamp(mat.roughness_factor, 0.02f, 1.0f);
        if (out.metallic_map.valid()) out.metallic = std::clamp(mat.metallic_factor, 0.0f, 1.0f);
    }
    if (mat.emissive_texture >= 0 && !mat.unlit) {
        out.emission_map = texture_map(model, mat.emissive_texture, true, false, 0);
        if (out.emission_map.valid()) {
            out.emission = fl::Rgb(byte01(mat.emissive.red()), byte01(mat.emissive.green()), byte01(mat.emissive.blue()));
            out.emission_strength = mat.emissive_map_strength;
        }
    }
    return out;
}

std::shared_ptr<const ModelGeometry> model_geometry(const fg::Model3D& model) {
    std::vector<mm::Mat4> world;
    model.world_matrices(model.rest_pose(), world);
    return convert(model, world, nullptr);
}

std::shared_ptr<const ModelGeometry> model_geometry(const fg::Model3D& model, const fg::ModelAnimator& pose) {
    if (pose.model() != &model || pose.world().size() != model.nodes.size()) return model_geometry(model);
    return convert(model, pose.world(), &pose);
}

std::shared_ptr<const ModelGeometry> load_model(const std::string& path, bool z_up, std::string* error) {
    if (path.empty()) {
        if (error) *error = "no model file";
        return nullptr;
    }
    std::error_code ec;
    const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(path, ec);
    if (ec) {
        if (error) *error = "can't find " + path;
        return nullptr;
    }
    const std::string key = path + (z_up ? "|z" : "|y");
    {
        std::lock_guard<std::mutex> lock(model_mutex());
        const auto it = file_slots().find(key);
        if (it != file_slots().end() && it->second.stamp == stamp) {
            if (!it->second.geometry && error) *error = it->second.error;
            return it->second.geometry;
        }
    }
    FileSlot slot;
    slot.stamp = stamp;
    fg::Model3D model;
    fg::ModelImportOptions options;
    options.z_up = z_up;
    std::string why;
    if (fg::Model3D::load(std::filesystem::path(path), model, &why, options)) {
        auto g = model_geometry(model);
        if (g->empty()) slot.error = path + " has no triangles";
        else slot.geometry = std::move(g);
    } else {
        slot.error = why.empty() ? "can't read " + path : why;
    }
    if (!slot.geometry && error) *error = slot.error;
    std::lock_guard<std::mutex> lock(model_mutex());
    file_slots()[key] = slot;
    return slot.geometry;
}

void clear_model_cache() {
    std::lock_guard<std::mutex> lock(model_mutex());
    file_slots().clear();
    texture_slots().clear();
}

struct ModelPoser::Impl {
    struct Rest {
        std::string source;
        std::size_t vertices = 0;
        std::shared_ptr<const ModelGeometry> geometry;
    };
    struct Live {
        const fg::Model3D* model = nullptr;
        fg::ModelAnimator anim;
        int clip = -2;
        double time = -1.0;
        std::shared_ptr<const ModelGeometry> geometry;
    };
    std::unordered_map<const fg::Model3D*, Rest> rest;
    std::unordered_map<std::uint64_t, Live> live;
};

ModelPoser::ModelPoser() : m(std::make_unique<Impl>()) {}
ModelPoser::~ModelPoser() = default;

std::shared_ptr<const ModelGeometry> ModelPoser::rest(const fg::Model3D& model) {
    Impl::Rest& r = m->rest[&model];
    const std::size_t verts = model.vertex_count();
    if (!r.geometry || r.source != model.source || r.vertices != verts) {
        r.source = model.source;
        r.vertices = verts;
        r.geometry = model_geometry(model);
    }
    return r.geometry;
}

std::shared_ptr<const ModelGeometry> ModelPoser::posed(orchard::Entity entity, const fg::Model3D& model, const std::string& clip, double time) {
    const int index = find_clip(model, clip);
    if (index < 0 || !entity.valid()) return rest(model);
    Impl::Live& l = m->live[entity.id()];
    if (l.model != &model) {
        l = Impl::Live{};
        l.model = &model;
        l.anim.attach(model);
    }
    if (l.geometry && l.clip == index && std::fabs(l.time - time) < 1e-6) return l.geometry;
    l.anim.play(index, true, 0.0, 1.0);
    l.anim.set_time(time);
    l.anim.update(0.0);
    l.clip = index;
    l.time = time;
    l.geometry = model_geometry(model, l.anim);
    return l.geometry;
}

void ModelPoser::forget(orchard::Entity entity) {
    m->live.erase(entity.id());
}

void ModelPoser::clear() {
    m->rest.clear();
    m->live.clear();
}
#else
bool models_supported() noexcept {
    return false;
}

fl::Material to_material(const fizmo::graphics::ModelMaterial&, const fizmo::graphics::Model3D&) {
    return fl::Material();
}

std::shared_ptr<const ModelGeometry> model_geometry(const fizmo::graphics::Model3D&) {
    return nullptr;
}

std::shared_ptr<const ModelGeometry> model_geometry(const fizmo::graphics::Model3D&, const fizmo::graphics::ModelAnimator&) {
    return nullptr;
}

std::shared_ptr<const ModelGeometry> load_model(const std::string&, bool, std::string* error) {
    if (error) *error = "this build can't read model files";
    return nullptr;
}

void clear_model_cache() {}

struct ModelPoser::Impl {};

ModelPoser::ModelPoser() : m(std::make_unique<Impl>()) {}
ModelPoser::~ModelPoser() = default;

std::shared_ptr<const ModelGeometry> ModelPoser::rest(const fizmo::graphics::Model3D&) {
    return nullptr;
}

std::shared_ptr<const ModelGeometry> ModelPoser::posed(orchard::Entity, const fizmo::graphics::Model3D&, const std::string&, double) {
    return nullptr;
}

void ModelPoser::forget(orchard::Entity) {}

void ModelPoser::clear() {}
#endif // CHLOROPHYLL_WITH_MODELS

World::World() = default;
World::~World() = default;

fl::Vec3 World::to_render(const orchard::Vec3& world) const {
    return fv(m_axes.to_render(world));
}

void World::apply() {
    m_problems.clear();
    if (!m_custom_settings) m_settings = settings_for(m_info, m_level, &m_problems);
    m_scene.environment() = environment_for(m_info, &m_problems);
    m_renderer.set_settings(m_settings);
}

void World::set_level(orchard::LightingLevel level) {
    m_level = level;
    m_custom_settings = false;
    apply();
}

void World::set_settings(const fl::LightingSettings& settings) {
    m_settings = settings;
    m_custom_settings = true;
    m_renderer.set_settings(settings);
}

namespace {

fl::Mesh prism_mesh(const orchard::Collider& c) {
    std::vector<orchard::Vec3> t = orchard::triangle_corners(c);
    if ((t[1].x - t[0].x) * (t[2].y - t[0].y) - (t[1].y - t[0].y) * (t[2].x - t[0].x) < 0.0f) std::swap(t[1], t[2]);
    const float hz = 0.5f * c.size.z;
    fl::Mesh out;
    auto face = [&](orchard::Vec3 a, orchard::Vec3 b, orchard::Vec3 d) {
        const auto base = static_cast<std::uint32_t>(out.positions.size());
        for (const orchard::Vec3& q : { a, b, d }) out.positions.push_back(fl::Vec3f{ q.x, q.y, q.z });
        out.indices.insert(out.indices.end(), { base, base + 1, base + 2 });
    };
    const orchard::Vec3 front{ 0.0f, 0.0f, hz }, back{ 0.0f, 0.0f, -hz };
    face(t[0] + front, t[1] + front, t[2] + front);
    face(t[0] + back, t[2] + back, t[1] + back);
    for (int k = 0; k < 3; ++k) {
        const orchard::Vec3 a = t[static_cast<std::size_t>(k)], b = t[static_cast<std::size_t>((k + 1) % 3)];
        face(a + back, b + back, b + front);
        face(a + back, b + front, a + front);
    }
    out.compute_normals();
    return out;
}

fl::Mesh section_mesh(const orchard::Collider& c) {
    const float d = std::max({ c.size.x, c.size.y, c.size.z, 1e-4f });
    const float outer = 0.5f;
    const float inner = c.shell > 0.0f ? std::max(0.0f, outer - c.shell / d) : 0.0f;
    const float p0 = std::clamp(c.polar_min, 0.0f, 180.0f) * 0.017453293f;
    const float p1 = std::clamp(c.polar_max, c.polar_min, 180.0f) * 0.017453293f;
    const float a0 = c.azimuth_min * 0.017453293f;
    const float a1 = std::min(std::max(c.azimuth_max, c.azimuth_min), c.azimuth_min + 360.0f) * 0.017453293f;
    const bool full_turn = a1 - a0 >= 6.2831f;
    const int rings = std::max(2, static_cast<int>(std::ceil((p1 - p0) / 0.1f)));
    const int segs = std::max(3, static_cast<int>(std::ceil((a1 - a0) / 0.1f)));
    auto at = [](float r, float t, float a) { return fl::Vec3f{ r * std::sin(t) * std::cos(a), r * std::cos(t), r * std::sin(t) * std::sin(a) }; };
    fl::Mesh out;
    auto patch = [&](float r, bool flip) {
        const auto base = static_cast<std::uint32_t>(out.positions.size());
        for (int i = 0; i <= rings; ++i) {
            for (int j = 0; j <= segs; ++j) out.positions.push_back(at(r, p0 + (p1 - p0) * i / rings, a0 + (a1 - a0) * j / segs));
        }
        for (int i = 0; i < rings; ++i) {
            for (int j = 0; j < segs; ++j) {
                const std::uint32_t q = base + static_cast<std::uint32_t>(i * (segs + 1) + j);
                const std::uint32_t w = q + static_cast<std::uint32_t>(segs + 1);
                if (flip) out.indices.insert(out.indices.end(), { q, q + 1, w, q + 1, w + 1, w });
                else out.indices.insert(out.indices.end(), { q, w, q + 1, q + 1, w, w + 1 });
            }
        }
    };
    auto strip = [&](const std::vector<fl::Vec3f>& a, const std::vector<fl::Vec3f>& b) {
        const auto base = static_cast<std::uint32_t>(out.positions.size());
        for (std::size_t k = 0; k < a.size(); ++k) {
            out.positions.push_back(a[k]);
            out.positions.push_back(b[k]);
        }
        for (std::uint32_t k = 0; k + 1 < a.size(); ++k) {
            const std::uint32_t q = base + k * 2;
            out.indices.insert(out.indices.end(), { q, q + 1, q + 2, q + 2, q + 1, q + 3 });
        }
    };
    patch(outer, false);
    if (inner > 0.0f) patch(inner, true);
    auto ring = [&](float r, float t) {
        std::vector<fl::Vec3f> v;
        for (int j = 0; j <= segs; ++j) v.push_back(at(r, t, a0 + (a1 - a0) * j / segs));
        return v;
    };
    auto arc = [&](float r, float a) {
        std::vector<fl::Vec3f> v;
        for (int i = 0; i <= rings; ++i) v.push_back(at(r, p0 + (p1 - p0) * i / rings, a));
        return v;
    };
    if (p0 > 1e-4f) strip(ring(outer, p0), ring(inner, p0));
    if (p1 < 3.14159f) strip(ring(inner, p1), ring(outer, p1));
    if (!full_turn) {
        strip(arc(inner, a0), arc(outer, a0));
        strip(arc(outer, a1), arc(inner, a1));
    }
    out.compute_normals();
    return out;
}

} // namespace

struct World::Piece {
    bool terrain = false;
    std::string signature;
    std::shared_ptr<const ModelGeometry> geometry;
    std::vector<std::size_t> meshes;
    std::vector<std::size_t> materials;
    bool material_override = false;
    std::shared_ptr<const orchard::TerrainHeights> heights;
    std::uint64_t revision = 0;
    std::vector<orchard::ScatterInstance> scatter;
    std::vector<std::size_t> scatter_pieces;
    std::size_t first_scatter = 0;
    int lod = 0;
    std::vector<orchard::Vec3> tints;
};

namespace {

orchard::Entity model_owner(const orchard::Scene& scene, orchard::Entity e) {
    for (orchard::Entity at = e; scene.alive(at); at = scene.parent(at))
        if (const orchard::Model* md = scene.model(at))
            if (md->visible && !md->source.empty()) return at;
    return orchard::Entity{};
}

std::string model_signature(const orchard::Scene& scene, orchard::Entity e, const orchard::Model& model) {
    const orchard::Material* om = scene.material(e);
    std::string s = model.source + (model.cast_shadows ? "|s" : "|-") + (om ? "|m" : "|-");
    if (om) s += (om->casts_shadows ? "s" : "-") + std::string(om->is_static ? "t" : "-") + std::to_string(om->light_channels);
    return s;
}

std::string terrain_key(const orchard::Terrain& t) {
    std::string k = orchard::terrain_signature(t) + "#";
    for (const orchard::TerrainLayer& l : t.layers) {
        k += l.texture + "|" + std::to_string(l.color.x) + "," + std::to_string(l.color.y) + "," + std::to_string(l.color.z) + "|" + std::to_string(l.tiling) + "|" + std::to_string(l.min_height) + "|" + std::to_string(l.max_height) + "|" + std::to_string(l.min_slope) + "|" + std::to_string(l.max_slope) + "|" + std::to_string(l.blend) + ";";
    }
    k += "#";
    for (const orchard::TerrainScatter& s : t.scatter) {
        k += s.model + "|" + std::to_string(s.density) + "|" + std::to_string(s.min_scale) + "|" + std::to_string(s.max_scale) + "|" + std::to_string(s.min_height) + "|" + std::to_string(s.max_height) + "|" + std::to_string(s.max_slope) + "|" + (s.align_to_slope ? "a" : "-") + "|" + std::to_string(s.seed) + ";";
    }
    return k;
}

int terrain_lod(const orchard::TerrainHeights& h) {
    int lod = 0;
    while (lod < 6 && static_cast<std::size_t>(((h.width - 1) >> lod) + 1) * static_cast<std::size_t>(((h.depth - 1) >> lod) + 1) > 1100000u) ++lod;
    return lod;
}

fl::Mesh terrain_mesh(const orchard::Terrain& t, const orchard::TerrainHeights& h, int lod, const std::vector<orchard::Vec3>& tints) {
    orchard::TerrainMesh tm;
    orchard::TerrainMeshOptions o;
    o.skirts = false;
    o.layer_tints = tints.empty() ? nullptr : &tints;
    orchard::build_terrain_mesh(t, h, tm, lod, o);
    fl::Mesh m;
    m.positions.reserve(tm.positions.size());
    for (const orchard::Vec3& p : tm.positions) m.positions.push_back({ p.x, p.y, p.z });
    if (tm.normals.size() == tm.positions.size()) {
        m.normals.reserve(tm.normals.size());
        for (const orchard::Vec3& n : tm.normals) m.normals.push_back({ n.x, n.y, n.z });
    }
    if (tm.uvs.size() == tm.positions.size()) {
        m.uvs.reserve(tm.uvs.size());
        for (const orchard::Vec2& uv : tm.uvs) m.uvs.push_back({ uv.x, uv.y });
    }
    if (tm.colors.size() == tm.positions.size()) {
        m.colors.reserve(tm.colors.size());
        for (const orchard::Vec3& c : tm.colors) m.colors.push_back({ c.x, c.y, c.z, 1.0f });
    }
    m.indices = std::move(tm.indices);
    if (!m.has_normals()) m.compute_normals();
    return m;
}

orchard::Vec3 builtin_layer_color(const std::string& name) {
    if (name == "builtin:grass") return { 0.2f, 0.34f, 0.11f };
    if (name == "builtin:dirt") return { 0.34f, 0.26f, 0.18f };
    if (name == "builtin:rock") return { 0.42f, 0.4f, 0.37f };
    if (name == "builtin:sand") return { 0.74f, 0.65f, 0.47f };
    if (name == "builtin:snow") return { 0.9f, 0.92f, 0.95f };
    return { 1.0f, 1.0f, 1.0f };
}

orchard::Vec3 texture_average(const std::shared_ptr<const fl::Texture>& tex) {
    if (!tex || !tex->valid()) return { 1.0f, 1.0f, 1.0f };
    const fl::Rgba a = tex->average();
    return { a.r, a.g, a.b };
}

bool same_map(const fl::TextureMap& a, const fl::TextureMap& b) {
    return a.texture == b.texture && a.channel == b.channel && a.strength == b.strength && a.filter == b.filter && a.wrap == b.wrap;
}

bool same_material(const fl::Material& a, const fl::Material& b) {
    return a.base_color == b.base_color && a.metallic == b.metallic && a.roughness == b.roughness && a.emission == b.emission && a.emission_strength == b.emission_strength
        && a.opacity == b.opacity && a.alpha_cutoff == b.alpha_cutoff && a.double_sided == b.double_sided && same_map(a.base_color_map, b.base_color_map)
        && same_map(a.normal_map, b.normal_map) && same_map(a.roughness_map, b.roughness_map) && same_map(a.metallic_map, b.metallic_map) && same_map(a.emission_map, b.emission_map);
}

bool same_shape(const ModelGeometry& a, const ModelGeometry& b) {
    if (a.pieces.size() != b.pieces.size() || a.materials.size() != b.materials.size()) return false;
    for (std::size_t k = 0; k < a.pieces.size(); ++k)
        if (a.pieces[k].material != b.pieces[k].material) return false;
    return true;
}

orchard::Mat4 scatter_local(const orchard::Mat4& frame, const orchard::Mat4& unimport, const orchard::ScatterInstance& s) {
    return frame * orchard::trs(s.position, s.rotation, { s.scale, s.scale, s.scale }) * unimport;
}

} // namespace

std::shared_ptr<const ModelGeometry> World::model_for(orchard::Entity e, const std::string& source) {
    if (source.empty()) return nullptr;
    if (m_model_source) return m_model_source(e, source);
    std::filesystem::path p(source);
    if (!p.is_absolute() && !m_asset_root.empty()) p = std::filesystem::path(m_asset_root) / p;
    return load_model(p.string(), m_axes.up().z > 0.5f);
}

std::shared_ptr<const orchard::TerrainHeights> World::heights_for(orchard::Entity e, const orchard::Terrain& t) {
    if (m_terrain_source) {
        std::shared_ptr<const orchard::TerrainHeights> h = m_terrain_source(e, t);
        if (h && h->valid()) return h;
    }
    auto& slot = m_default_heights[e.id()];
    const std::string sig = orchard::terrain_signature(t);
    if (slot.second && slot.first == sig) return slot.second;
    auto made = std::make_shared<orchard::TerrainHeights>();
    std::string why;
    if (!orchard::build_terrain_heights(t, m_asset_root, *made, &why) && !why.empty()) m_problems.push_back("terrain: " + why);
    slot = { sig, made };
    return made;
}

std::vector<std::size_t> World::scatter_meshes(const std::string& source, std::vector<std::size_t>& materials) {
    auto it = m_scatter_meshes.find(source);
    if (it == m_scatter_meshes.end()) {
        ScatterMesh sm;
        sm.geometry = model_for(orchard::Entity{}, source);
        if (sm.geometry) {
            std::vector<std::size_t> mats;
            for (const fl::Material& m : sm.geometry->materials) mats.push_back(m_scene.add_material(m));
            for (const ModelPiece& p : sm.geometry->pieces) {
                sm.meshes.push_back(m_scene.add_mesh(p.mesh));
                sm.materials.push_back(p.material < mats.size() ? mats[p.material] : 0);
            }
        }
        it = m_scatter_meshes.emplace(source, std::move(sm)).first;
    }
    materials = it->second.materials;
    return it->second.meshes;
}

bool World::add_model(const orchard::Scene& scene, orchard::Entity e, const orchard::Model& model) {
    std::shared_ptr<const ModelGeometry> geo = model_for(e, model.source);
    if (!geo || geo->empty()) return false;
    auto piece = std::make_shared<Piece>();
    piece->signature = model_signature(scene, e, model);
    piece->geometry = geo;
    const orchard::Material* om = scene.material(e);
    piece->material_override = om != nullptr;
    if (om) {
        piece->materials.push_back(m_scene.add_material(to_material(*om, m_textures)));
    } else {
        for (const fl::Material& m : geo->materials) piece->materials.push_back(m_scene.add_material(m));
        if (piece->materials.empty()) piece->materials.push_back(m_scene.add_material(fl::Material()));
    }
    const orchard::Mat4 world = m_axes.to_render_matrix() * scene.world_matrix(e);
    std::vector<std::size_t>& ids = m_instances_of[e.id()];
    std::vector<orchard::Mat4>& locals = m_parts_of[e.id()];
    for (const ModelPiece& p : geo->pieces) {
        const std::size_t mesh = m_scene.add_mesh(p.mesh);
        piece->meshes.push_back(mesh);
        fl::Instance inst;
        inst.name = scene.name(e);
        inst.id = e.id();
        inst.mesh = mesh;
        inst.material = om ? piece->materials.front() : piece->materials[std::min(p.material, piece->materials.size() - 1)];
        inst.transform = to_transform(world);
        inst.casts_shadows = model.cast_shadows && (!om || om->casts_shadows);
        inst.is_static = om && om->is_static;
        inst.channels = om ? om->light_channels : 1u;
        inst.instanced = true;
        ids.push_back(m_scene.add_instance(inst));
        locals.push_back(orchard::Mat4{});
    }
    m_pieces[e.id()] = std::move(piece);
    return true;
}

bool World::add_terrain(const orchard::Scene& scene, orchard::Entity e, const orchard::Terrain& t) {
    std::shared_ptr<const orchard::TerrainHeights> h = heights_for(e, t);
    if (!h || !h->valid()) return false;
    auto piece = std::make_shared<Piece>();
    piece->terrain = true;
    piece->signature = terrain_key(t);
    piece->heights = h;
    piece->revision = h->revision;
    piece->lod = terrain_lod(*h);
    fl::Material mat;
    mat.name = "terrain";
    mat.base_color = fl::Rgb(1.0f);
    mat.roughness = 0.92f;
    mat.specular = 0.3f;
    std::vector<std::shared_ptr<const fl::Texture>> texes(t.layers.size());
    if (m_textures)
        for (std::size_t i = 0; i < t.layers.size(); ++i)
            if (!t.layers[i].texture.empty()) texes[i] = m_textures(t.layers[i].texture, true);
    piece->tints.assign(t.layers.size(), orchard::Vec3{ 1.0f, 1.0f, 1.0f });
    if (!texes.empty() && texes[0] && texes[0]->valid()) {
        mat.base_color_map = fl::TextureMap(texes[0]);
        const orchard::Vec3 a0 = texture_average(texes[0]);
        const orchard::Vec3 inv{ 1.0f / std::max(0.05f, a0.x), 1.0f / std::max(0.05f, a0.y), 1.0f / std::max(0.05f, a0.z) };
        for (std::size_t i = 1; i < t.layers.size(); ++i) piece->tints[i] = texture_average(texes[i]) * inv;
    } else {
        for (std::size_t i = 0; i < t.layers.size(); ++i) piece->tints[i] = texes[i] ? texture_average(texes[i]) : builtin_layer_color(t.layers[i].texture);
    }
    const std::size_t mesh = m_scene.add_mesh(terrain_mesh(t, *h, piece->lod, piece->tints));
    const std::size_t material = m_scene.add_material(mat);
    piece->meshes.push_back(mesh);
    piece->materials.push_back(material);
    const orchard::Material* om = scene.material(e);
    const orchard::Quat q = orchard::terrain_frame(m_axes.up());
    const orchard::Mat4 frame = orchard::rotation(q);
    const orchard::Mat4 unimport = orchard::rotation(orchard::Quat{ -q.x, -q.y, -q.z, q.w });
    const orchard::Mat4 world = m_axes.to_render_matrix() * scene.world_matrix(e);
    std::vector<std::size_t>& ids = m_instances_of[e.id()];
    std::vector<orchard::Mat4>& locals = m_parts_of[e.id()];
    fl::Instance ground;
    ground.name = scene.name(e);
    ground.id = e.id();
    ground.mesh = mesh;
    ground.material = material;
    ground.transform = to_transform(world * frame);
    ground.casts_shadows = !om || om->casts_shadows;
    ground.is_static = om && om->is_static;
    ground.channels = om ? om->light_channels : 1u;
    ground.instanced = true;
    ids.push_back(m_scene.add_instance(ground));
    locals.push_back(frame);
    piece->first_scatter = ids.size();
    piece->scatter = orchard::scatter_instances(t, *h);
    piece->scatter_pieces.reserve(piece->scatter.size());
    for (const orchard::ScatterInstance& s : piece->scatter) {
        std::vector<std::size_t> mats;
        const std::vector<std::size_t> meshes = scatter_meshes(s.model, mats);
        const orchard::Mat4 local = scatter_local(frame, unimport, s);
        const fl::Transform xf = to_transform(world * local);
        for (std::size_t k = 0; k < meshes.size(); ++k) {
            fl::Instance inst;
            inst.id = e.id();
            inst.mesh = meshes[k];
            inst.material = mats[k];
            inst.transform = xf;
            inst.instanced = true;
            ids.push_back(m_scene.add_instance(inst));
            locals.push_back(local);
        }
        piece->scatter_pieces.push_back(meshes.size());
    }
    m_pieces[e.id()] = std::move(piece);
    return true;
}

bool World::update_piece(const orchard::Scene& scene, orchard::Entity e, Piece& piece, bool& rebuild) {
    if (!piece.terrain) {
        const orchard::Model* model = scene.model(e);
        if (!model || model_owner(scene, e) != e || model_signature(scene, e, *model) != piece.signature) {
            rebuild = true;
            return false;
        }
        std::shared_ptr<const ModelGeometry> geo = model_for(e, model->source);
        if (!geo || geo->empty() || !same_shape(*geo, *piece.geometry)) {
            rebuild = true;
            return false;
        }
        if (geo == piece.geometry) return false;
        for (std::size_t k = 0; k < geo->pieces.size(); ++k) m_scene.replace_mesh(piece.meshes[k], geo->pieces[k].mesh);
        if (!piece.material_override) {
            for (std::size_t k = 0; k < geo->materials.size() && k < piece.materials.size(); ++k)
                if (!same_material(geo->materials[k], piece.geometry->materials[k])) m_scene.replace_material(piece.materials[k], geo->materials[k]);
        }
        piece.geometry = std::move(geo);
        return true;
    }
    const orchard::Terrain* t = scene.terrain(e);
    if (!t || terrain_key(*t) != piece.signature) {
        rebuild = true;
        return false;
    }
    std::shared_ptr<const orchard::TerrainHeights> h = heights_for(e, *t);
    if (!h || !h->valid()) {
        rebuild = true;
        return false;
    }
    if (h == piece.heights && h->revision == piece.revision) return false;
    const int lod = terrain_lod(*h);
    std::vector<orchard::ScatterInstance> scatter = orchard::scatter_instances(*t, *h);
    if (scatter.size() != piece.scatter.size() || lod != piece.lod) {
        rebuild = true;
        return false;
    }
    for (std::size_t i = 0; i < scatter.size(); ++i) {
        if (scatter[i].model != piece.scatter[i].model) {
            rebuild = true;
            return false;
        }
    }
    m_scene.replace_mesh(piece.meshes.front(), terrain_mesh(*t, *h, lod, piece.tints));
    const orchard::Quat q = orchard::terrain_frame(m_axes.up());
    const orchard::Mat4 frame = orchard::rotation(q);
    const orchard::Mat4 unimport = orchard::rotation(orchard::Quat{ -q.x, -q.y, -q.z, q.w });
    std::vector<orchard::Mat4>& locals = m_parts_of[e.id()];
    std::size_t at = piece.first_scatter;
    for (std::size_t i = 0; i < scatter.size(); ++i) {
        const orchard::Mat4 local = scatter_local(frame, unimport, scatter[i]);
        for (std::size_t k = 0; k < piece.scatter_pieces[i] && at < locals.size(); ++k) locals[at++] = local;
    }
    m_world_of.erase(e.id());
    piece.scatter = std::move(scatter);
    piece.heights = std::move(h);
    piece.revision = piece.heights->revision;
    return true;
}

std::size_t World::refresh_geometry(const orchard::Scene& scene) {
    std::size_t changed = 0;
    bool rebuild = false;
    for (auto& kv : m_pieces) {
        const orchard::Entity e = orchard::Entity::from_id(kv.first);
        if (!scene.alive(e)) {
            rebuild = true;
            break;
        }
        if (update_piece(scene, e, *kv.second, rebuild)) ++changed;
        if (rebuild) break;
    }
    if (rebuild) {
        sync(scene);
        return changed + 1;
    }
    if (changed > 0) sync(scene);
    return changed;
}

void World::build(const orchard::Scene& scene, const orchard::ProjectInfo& info, bool editor) {
    m_info = info;
    m_axes = info.dimension == orchard::Dimension::two_d ? orchard::AxisSystem() : info.axis_system();
    m_level = editor ? info.lighting.editor_quality : info.lighting.quality;
    m_custom_settings = false;
    m_scene.clear();
    m_light_of.clear();
    m_instances_of.clear();
    m_parts_of.clear();
    m_pieces.clear();
    m_scatter_meshes.clear();
    m_world_of.clear();
    apply();
    const bool geometry = m_geometry && info.dimension == orchard::Dimension::three_d;
    const std::size_t box = m_scene.add_mesh(fl::Mesh::box({ 0.5f, 0.5f, 0.5f }));
    const std::size_t ball = m_scene.add_mesh(fl::Mesh::sphere(0.5f, 32, 16));
    std::map<std::pair<int, int>, std::size_t> capsules;
    const orchard::Mat4 to_r = m_axes.to_render_matrix();
    for (orchard::Entity e : scene.entities()) {
        const orchard::Mat4 world = to_r * scene.world_matrix(e);
        if (const orchard::Light* l = scene.light(e)) {
            fl::Light fl_light = to_light(*l, scene.world_matrix(e), m_axes, m_time);
            fl_light.name = scene.name(e);
            fl_light.id = e.id();
            if (!l->ies.empty()) {
                std::string why;
                if (auto profile = fl::IesProfile::load_file(l->ies, &why)) fl_light.ies = std::make_shared<fl::IesProfile>(std::move(*profile));
                else m_problems.push_back(scene.name(e) + ": " + why);
            }
            m_light_of[e.id()] = m_scene.add_light(fl_light);
            continue;
        }
        if (is_camera(scene, e)) continue;
        if (const orchard::Material* shown = scene.material(e); !shown || shown->visible) {
            m_instances_of[e.id()];
            m_parts_of[e.id()];
        }
        if (const orchard::Fluid* fluid = scene.fluid(e)) {
            if (fluid->enabled && fluid->fill > 0.0f && info.dimension == orchard::Dimension::three_d) {
                fl::Material water = fl::Material::water();
                water.absorption_color = to_rgb(fluid->color);
                water.absorption_distance = std::max(0.05f, 4.0f * (1.0f - std::clamp(fluid->opacity, 0.0f, 0.99f)));
                if (fluid->opacity >= 0.99f) {
                    water.transmission = 0.0f;
                    water.base_color = to_rgb(fluid->color);
                    water.roughness = 0.1f;
                }
                const std::size_t wm = m_scene.add_material(water);
                const orchard::Vec4 c4 = scene.world_matrix(e) * orchard::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
                const orchard::Vec3 up = m_axes.up();
                orchard::Vec3 size = fluid->size;
                const orchard::Vec3 lo{ c4.x - size.x * 0.5f, c4.y - size.y * 0.5f, c4.z - size.z * 0.5f };
                const float f = std::clamp(fluid->fill, 0.0f, 1.0f);
                orchard::Vec3 at = lo;
                if (std::fabs(up.x) > 0.5f) {
                    if (up.x < 0.0f) at.x += size.x * (1.0f - f);
                    size.x *= f;
                } else if (std::fabs(up.z) > 0.5f) {
                    if (up.z < 0.0f) at.z += size.z * (1.0f - f);
                    size.z *= f;
                } else {
                    if (up.y < 0.0f) at.y += size.y * (1.0f - f);
                    size.y *= f;
                }
                fl::Instance inst;
                inst.name = scene.name(e);
                inst.id = e.id();
                inst.mesh = box;
                inst.material = wm;
                inst.transform = to_transform(to_r * translate_matrix({ at.x + size.x * 0.5f, at.y + size.y * 0.5f, at.z + size.z * 0.5f }) * scale_matrix(size));
                inst.casts_shadows = false;
                m_scene.add_instance(inst);
            }
            if (!scene.body(e) && (!scene.colliders(e) || scene.colliders(e)->empty())) continue;
        }
        const orchard::Material* om = scene.material(e);
        if (om && !om->visible) continue;
        if (orchard::is_negative_part(scene, e)) continue;
        m_instances_of[e.id()];
        m_parts_of[e.id()];
        if (geometry) {
            const orchard::Entity owner = model_owner(scene, e);
            if (owner.valid() && owner != e && model_for(owner, scene.model(owner)->source)) continue;
            if (owner == e && add_model(scene, e, *scene.model(e))) continue;
            if (const orchard::Terrain* t = scene.terrain(e)) {
                add_terrain(scene, e, *t);
                continue;
            }
        }
        const orchard::Material mat = om ? *om : orchard::Material{};
        const std::size_t material = m_scene.add_material(to_material(mat, m_textures));
        std::vector<std::pair<std::size_t, orchard::Mat4>> parts;
        const std::vector<orchard::Collider>* cs = scene.colliders(e);
        if (cs && !cs->empty()) {
            for (const orchard::Collider& c : *cs) {
                if (!c.enabled || c.sensor) continue;
                const orchard::Mat4 offset = translate_matrix(c.offset);
                if (geometry && (c.shape == orchard::ColliderShape::mesh || c.shape == orchard::ColliderShape::convex_hull) && !c.source.empty()) {
                    std::vector<std::size_t> mats;
                    const std::vector<std::size_t> meshes = scatter_meshes(c.source, mats);
                    if (!meshes.empty()) {
                        const orchard::Mat4 local = offset * scale_matrix(c.size);
                        std::vector<std::size_t>& ids = m_instances_of[e.id()];
                        std::vector<orchard::Mat4>& locals = m_parts_of[e.id()];
                        for (std::size_t k = 0; k < meshes.size(); ++k) {
                            fl::Instance inst;
                            inst.name = scene.name(e);
                            inst.id = e.id();
                            inst.mesh = meshes[k];
                            inst.material = om ? material : mats[k];
                            inst.transform = to_transform(world * local);
                            inst.casts_shadows = mat.casts_shadows;
                            inst.is_static = mat.is_static;
                            inst.channels = mat.light_channels;
                            inst.instanced = true;
                            ids.push_back(m_scene.add_instance(inst));
                            locals.push_back(local);
                        }
                        continue;
                    }
                }
                if (c.shape == orchard::ColliderShape::box) {
                    parts.emplace_back(box, offset * scale_matrix(c.size));
                } else if (c.shape == orchard::ColliderShape::sphere) {
                    const float d = std::max({ c.size.x, c.size.y, c.size.z });
                    parts.emplace_back(ball, offset * scale_matrix({ d, d, d }));
                } else if (c.shape == orchard::ColliderShape::rounded_box) {
                    parts.emplace_back(box, offset * scale_matrix(c.size));
                } else if (c.shape == orchard::ColliderShape::sphere_section) {
                    const float d = std::max({ c.size.x, c.size.y, c.size.z });
                    parts.emplace_back(m_scene.add_mesh(section_mesh(c)), offset * scale_matrix({ d, d, d }));
                } else if (c.shape == orchard::ColliderShape::triangle) {
                    parts.emplace_back(m_scene.add_mesh(prism_mesh(c)), offset);
                } else if (c.shape == orchard::ColliderShape::cylinder || c.shape == orchard::ColliderShape::cone) {
                    const bool cone = c.shape == orchard::ColliderShape::cone;
                    const float r = std::max(c.size.x, c.size.z) * 0.5f;
                    const float half = c.size.y * 0.5f;
                    const std::pair<int, int> key{ static_cast<int>(std::lround(r * 1000.0f)) * (cone ? -1 : 1) - (cone ? 1 : 0), static_cast<int>(std::lround(half * 1000.0f)) + 1000000 };
                    auto it = capsules.find(key);
                    if (it == capsules.end()) {
                        fl::Mesh m = fl::Mesh::cylinder(std::max(r, 1e-3f), std::max(half, 1e-3f), 24, true);
                        if (cone) {
                            for (auto& p : m.positions) {
                                if (p.y > 0.0f) {
                                    p.x = 0.0f;
                                    p.z = 0.0f;
                                }
                            }
                            m.compute_normals();
                        }
                        it = capsules.emplace(key, m_scene.add_mesh(m)).first;
                    }
                    parts.emplace_back(it->second, offset);
                } else {
                    const float r = std::max(c.size.x, c.size.z) * 0.5f;
                    const float half = std::max(0.0f, c.size.y * 0.5f - r);
                    const std::pair<int, int> key{ static_cast<int>(std::lround(r * 1000.0f)), static_cast<int>(std::lround(half * 1000.0f)) };
                    auto it = capsules.find(key);
                    if (it == capsules.end()) it = capsules.emplace(key, m_scene.add_mesh(fl::Mesh::capsule(std::max(r, 1e-3f), half, 24, 8))).first;
                    parts.emplace_back(it->second, offset);
                }
            }
        } else {
            parts.emplace_back(box, orchard::Mat4{});
        }
        std::vector<std::size_t>& ids = m_instances_of[e.id()];
        std::vector<orchard::Mat4>& locals = m_parts_of[e.id()];
        for (const auto& [mesh, local] : parts) {
            fl::Instance inst;
            inst.name = scene.name(e);
            inst.id = e.id();
            inst.mesh = mesh;
            inst.material = material;
            inst.transform = to_transform(world * local);
            inst.casts_shadows = mat.casts_shadows;
            inst.is_static = mat.is_static;
            inst.channels = mat.light_channels;
            ids.push_back(m_scene.add_instance(inst));
            locals.push_back(local);
        }
    }
    m_scene.commit();
}

void World::sync(const orchard::Scene& scene) {
    std::size_t seen_lights = 0, seen_objects = 0;
    bool rebuild = false;
    const orchard::Mat4 to_r = m_axes.to_render_matrix();
    for (orchard::Entity e : scene.entities()) {
        if (const orchard::Light* l = scene.light(e)) {
            ++seen_lights;
            const auto it = m_light_of.find(e.id());
            if (it == m_light_of.end()) {
                rebuild = true;
                break;
            }
            fl::Light next = to_light(*l, scene.world_matrix(e), m_axes, m_time);
            const fl::Light& now = static_cast<const fl::Scene&>(m_scene).lights()[it->second];
            if (same_light(now, next)) continue;
            next.name = now.name;
            next.id = now.id;
            next.ies = now.ies;
            next.cookie = now.cookie;
            m_scene.lights()[it->second] = next;
            continue;
        }
        if (is_camera(scene, e)) continue;
        const orchard::Material* om = scene.material(e);
        if (om && !om->visible) continue;
        if (orchard::is_negative_part(scene, e)) continue;
        ++seen_objects;
        const auto ids = m_instances_of.find(e.id());
        if (ids == m_instances_of.end()) {
            rebuild = true;
            break;
        }
        if (m_geometry && m_info.dimension == orchard::Dimension::three_d) {
            const auto piece = m_pieces.find(e.id());
            if (piece != m_pieces.end()) {
                update_piece(scene, e, *piece->second, rebuild);
                if (rebuild) break;
            } else if (scene.model(e) && model_owner(scene, e) == e && model_for(e, scene.model(e)->source)) {
                rebuild = true;
                break;
            } else if (const orchard::Terrain* t = scene.terrain(e); t && ids->second.empty()) {
                const std::shared_ptr<const orchard::TerrainHeights> h = heights_for(e, *t);
                if (h && h->valid()) {
                    rebuild = true;
                    break;
                }
            }
        }
        const orchard::Mat4 world = to_r * scene.world_matrix(e);
        auto seen = m_world_of.find(e.id());
        if (seen != m_world_of.end() && std::equal(std::begin(world.m), std::end(world.m), std::begin(seen->second.m))) continue;
        m_world_of[e.id()] = world;
        const std::vector<orchard::Mat4>& locals = m_parts_of[e.id()];
        for (std::size_t k = 0; k < ids->second.size() && k < locals.size(); ++k) {
            const fl::Transform t = to_transform(world * locals[k]);
            const fl::Instance& now = static_cast<const fl::Scene&>(m_scene).instances()[ids->second[k]];
            if (now.transform.m != t.m) m_scene.move_instance(ids->second[k], t);
        }
    }
    if (rebuild || seen_lights != m_light_of.size() || seen_objects != m_instances_of.size()) {
        const orchard::LightingLevel keep = m_level;
        const bool custom = m_custom_settings;
        const fl::LightingSettings settings = m_settings;
        build(scene, m_info);
        m_level = keep;
        if (custom) set_settings(settings);
        else set_level(keep);
        return;
    }
    m_scene.commit();
}

void World::animate(const orchard::Scene& scene, double seconds) {
    m_time = seconds;
    sync(scene);
}

const std::vector<std::uint8_t>& World::render(const orchard::Vec3& eye, const orchard::Vec3& target, float fov, unsigned width, unsigned height) {
    const fl::Camera cam = fl::Camera::look_at(to_render(eye), to_render(target), fl::Vec3{ 0.0, 1.0, 0.0 }, fov);
    m_renderer.render(m_scene, cam, width, height);
    m_rgba = m_renderer.rgba8();
    return m_rgba;
}

fl::Camera World::camera_for(const orchard::Viewport& view, unsigned width, unsigned height) const {
    (void)width;
    orchard::Viewport& v = const_cast<orchard::Viewport&>(view);
    if (view.dimension() == orchard::Dimension::two_d) {
        const orchard::PanCamera& pan = v.pan_camera();
        const float distance = 1000.0f;
        const float visible = static_cast<float>(height) / std::max(pan.pixels_per_unit, 1e-3f);
        const float fov = 2.0f * std::atan(visible * 0.5f / distance) / fl::kDegToRad;
        return fl::Camera::look_at(fl::Vec3{ pan.center.x, pan.center.y, distance }, fl::Vec3{ pan.center.x, pan.center.y, 0.0 }, fl::Vec3{ 0.0, view.y_axis() == orchard::YAxis2D::down ? -1.0 : 1.0, 0.0 }, fov);
    }
    return fl::Camera::look_at(to_render(view.eye()), to_render(v.orbit_camera().target), fl::Vec3{ 0.0, 1.0, 0.0 }, v.orbit_camera().fov);
}

const std::vector<std::uint8_t>& World::render(const orchard::Viewport& view, unsigned width, unsigned height) {
    if (view.dimension() == orchard::Dimension::two_d) {
        orchard::Viewport& v = const_cast<orchard::Viewport&>(view);
        const orchard::PanCamera& pan = v.pan_camera();
        const float distance = 1000.0f;
        const float visible = static_cast<float>(height) / std::max(pan.pixels_per_unit, 1e-3f);
        const float fov = 2.0f * std::atan(visible * 0.5f / distance) / fl::kDegToRad;
        fl::Camera cam = fl::Camera::look_at(fl::Vec3{ pan.center.x, pan.center.y, distance }, fl::Vec3{ pan.center.x, pan.center.y, 0.0 }, fl::Vec3{ 0.0, view.y_axis() == orchard::YAxis2D::down ? -1.0 : 1.0, 0.0 }, fov);
        m_renderer.render(m_scene, cam, width, height);
        m_rgba = m_renderer.rgba8();
        return m_rgba;
    }
    orchard::Viewport& v = const_cast<orchard::Viewport&>(view);
    return render(view.eye(), v.orbit_camera().target, v.orbit_camera().fov, width, height);
}

fl::BakedLighting World::bake(const fl::BakeOptions& options) {
    return fl::bake(m_scene, m_settings, options);
}

void World::use_bake(std::shared_ptr<const fl::BakedLighting> baked) {
    m_renderer.set_baked(std::move(baked));
}

float World::light_level(const orchard::Vec3& point, const orchard::Vec3& normal) {
    return m_renderer.light_level(m_scene, to_render(point), to_render(normal));
}

float World::light_level(const orchard::Scene& scene, orchard::Entity e) {
    const orchard::Mat4 w = scene.world_matrix(e);
    return light_level(transform_point(w, { 0.0f, 0.0f, 0.0f }), m_axes.up());
}

bool World::lit_by(const orchard::Vec3& point, orchard::Entity light) {
    const auto it = m_light_of.find(light.id());
    if (it == m_light_of.end()) return false;
    return m_renderer.lit_by(m_scene, to_render(point), it->second);
}

bool World::has_light(orchard::Entity e) const {
    return m_light_of.count(e.id()) > 0;
}

std::vector<fl::Light> World::lights() const {
    return m_scene.lights();
}

const fl::Environment& World::environment() const {
    return m_scene.environment();
}

Stats World::stats() const {
    Stats s;
    s.lights = m_scene.lights().size();
    s.objects = m_scene.instances().size();
    s.triangles = m_scene.triangle_count();
    s.instanced = m_scene.instanced_count();
    s.traced_triangles = m_scene.traced_triangle_count();
    for (const auto& kv : m_pieces) {
        if (kv.second->terrain) {
            ++s.terrains;
            s.scatter += kv.second->scatter.size();
        } else {
            ++s.models;
        }
    }
    return s;
}

} // namespace chlorophyll
