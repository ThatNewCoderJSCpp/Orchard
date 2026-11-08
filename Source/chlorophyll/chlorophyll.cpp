#include <chlorophyll/chlorophyll.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

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

void World::build(const orchard::Scene& scene, const orchard::ProjectInfo& info, bool editor) {
    m_info = info;
    m_axes = info.dimension == orchard::Dimension::two_d ? orchard::AxisSystem() : info.axis_system();
    m_level = editor ? info.lighting.editor_quality : info.lighting.quality;
    m_custom_settings = false;
    m_scene.clear();
    m_light_of.clear();
    m_instances_of.clear();
    m_parts_of.clear();
    apply();
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
        const orchard::Material mat = om ? *om : orchard::Material{};
        const std::size_t material = m_scene.add_material(to_material(mat, m_textures));
        std::vector<std::pair<std::size_t, orchard::Mat4>> parts;
        const std::vector<orchard::Collider>* cs = scene.colliders(e);
        if (cs && !cs->empty()) {
            for (const orchard::Collider& c : *cs) {
                if (!c.enabled || c.sensor) continue;
                const orchard::Mat4 offset = translate_matrix(c.offset);
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
        ++seen_objects;
        const auto ids = m_instances_of.find(e.id());
        if (ids == m_instances_of.end()) {
            rebuild = true;
            break;
        }
        const orchard::Mat4 world = to_r * scene.world_matrix(e);
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
    return s;
}

} // namespace chlorophyll
