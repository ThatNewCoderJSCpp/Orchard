#include <orchard/scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace orchard {

namespace {

const std::string empty_name;

std::string number(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    return buf;
}

} // namespace

const char* to_string(Dimension d) {
    return d == Dimension::two_d ? "2d" : "3d";
}

const std::vector<BodyFlagInfo>& body_flags() {
    static const std::vector<BodyFlagInfo> all = {
        {body_gravity, "gravity", "Gravity", true, true},
        {body_collisions, "collisions", "Collides", true, true},
        {body_friction, "friction", "Friction", true, true},
        {body_bounce, "bounce", "Bounces", true, true},
        {body_sleep, "sleep", "Can sleep", true, true},
        {body_start_awake, "awake", "Starts awake", true, true},
        {body_continuous, "continuous", "Fast-moving (CCD)", true, true},
        {body_contact_events, "contacts", "Contact events", true, true},
        {body_lock_position_x, "lock-x", "Lock X", true, true},
        {body_lock_position_y, "lock-y", "Lock Y", true, true},
        {body_lock_position_z, "lock-z", "Lock Z", false, true},
        {body_lock_rotation_x, "lock-rot-x", "Lock rotation X", false, true},
        {body_lock_rotation_y, "lock-rot-y", "Lock rotation Y", false, true},
        {body_lock_rotation_z, "lock-rot-z", "Lock rotation", true, true},
    };
    return all;
}

const char* to_string(BodyType type) {
    switch (type) {
    case BodyType::fixed: return "static";
    case BodyType::dynamic: return "dynamic";
    case BodyType::kinematic: return "kinematic";
    }
    return "dynamic";
}

std::optional<BodyType> parse_body_type(const std::string& text) {
    if (text == "static" || text == "fixed") return BodyType::fixed;
    if (text == "dynamic") return BodyType::dynamic;
    if (text == "kinematic") return BodyType::kinematic;
    return std::nullopt;
}

const char* to_string(ColliderShape shape) {
    switch (shape) {
    case ColliderShape::box: return "box";
    case ColliderShape::sphere: return "sphere";
    case ColliderShape::capsule: return "capsule";
    case ColliderShape::cylinder: return "cylinder";
    case ColliderShape::cone: return "cone";
    case ColliderShape::rounded_box: return "rounded-box";
    }
    return "box";
}

std::optional<ColliderShape> parse_collider_shape(const std::string& text) {
    if (text == "box" || text == "rect") return ColliderShape::box;
    if (text == "sphere" || text == "circle") return ColliderShape::sphere;
    if (text == "capsule") return ColliderShape::capsule;
    if (text == "cylinder") return ColliderShape::cylinder;
    if (text == "cone") return ColliderShape::cone;
    if (text == "rounded-box" || text == "rounded_box" || text == "rounded") return ColliderShape::rounded_box;
    return std::nullopt;
}

const std::vector<ColliderShape>& collider_shapes() {
    static const std::vector<ColliderShape> all = {ColliderShape::box, ColliderShape::sphere, ColliderShape::capsule, ColliderShape::cylinder, ColliderShape::cone, ColliderShape::rounded_box};
    return all;
}

bool collider_shape_3d_only(ColliderShape shape) {
    return shape == ColliderShape::cylinder || shape == ColliderShape::cone;
}

const char* to_string(BodyQuality q) {
    switch (q) {
    case BodyQuality::project_default: return "default";
    case BodyQuality::arcade: return "arcade";
    case BodyQuality::normal: return "normal";
    case BodyQuality::high: return "high";
    case BodyQuality::ultra: return "ultra";
    }
    return "default";
}

std::optional<BodyQuality> parse_body_quality(const std::string& text) {
    if (text == "default") return BodyQuality::project_default;
    if (text == "arcade") return BodyQuality::arcade;
    if (text == "normal") return BodyQuality::normal;
    if (text == "high") return BodyQuality::high;
    if (text == "ultra") return BodyQuality::ultra;
    return std::nullopt;
}

const char* to_string(BodyContinuous c) {
    switch (c) {
    case BodyContinuous::project_default: return "default";
    case BodyContinuous::on: return "on";
    case BodyContinuous::off: return "off";
    }
    return "default";
}

std::optional<BodyContinuous> parse_body_continuous(const std::string& text) {
    if (text == "default") return BodyContinuous::project_default;
    if (text == "on") return BodyContinuous::on;
    if (text == "off") return BodyContinuous::off;
    return std::nullopt;
}

const char* to_string(CombineMode mode) {
    switch (mode) {
    case CombineMode::project_default: return "default";
    case CombineMode::average: return "average";
    case CombineMode::geometric_mean: return "geometric-mean";
    case CombineMode::harmonic_mean: return "harmonic-mean";
    case CombineMode::quadratic_mean: return "quadratic-mean";
    case CombineMode::minimum: return "minimum";
    case CombineMode::multiply: return "multiply";
    case CombineMode::maximum: return "maximum";
    case CombineMode::sum: return "sum";
    }
    return "default";
}

std::optional<CombineMode> parse_combine_mode(const std::string& text) {
    if (text == "default" || text == "project") return CombineMode::project_default;
    if (text == "average" || text == "mean") return CombineMode::average;
    if (text == "geometric-mean" || text == "geometric") return CombineMode::geometric_mean;
    if (text == "harmonic-mean" || text == "harmonic") return CombineMode::harmonic_mean;
    if (text == "quadratic-mean" || text == "quadratic" || text == "rms") return CombineMode::quadratic_mean;
    if (text == "minimum" || text == "min") return CombineMode::minimum;
    if (text == "multiply" || text == "product") return CombineMode::multiply;
    if (text == "maximum" || text == "max") return CombineMode::maximum;
    if (text == "sum" || text == "add") return CombineMode::sum;
    return std::nullopt;
}

const char* combine_label(CombineMode mode) {
    switch (mode) {
    case CombineMode::project_default: return "Project default";
    case CombineMode::average: return "Average";
    case CombineMode::geometric_mean: return "Geometric mean";
    case CombineMode::harmonic_mean: return "Harmonic mean";
    case CombineMode::quadratic_mean: return "Quadratic mean";
    case CombineMode::minimum: return "Minimum";
    case CombineMode::multiply: return "Multiply";
    case CombineMode::maximum: return "Maximum";
    case CombineMode::sum: return "Sum";
    }
    return "Project default";
}

const char* combine_explanation(CombineMode mode) {
    switch (mode) {
    case CombineMode::project_default: return "use the project's setting";
    case CombineMode::average: return "(a + b) / 2, the middle; Unity's and Unreal's default";
    case CombineMode::geometric_mean: return "sqrt(a * b); a 0 on either side wins, like Box2D's friction";
    case CombineMode::harmonic_mean: return "2ab / (a + b); leans hard toward the smaller one";
    case CombineMode::quadratic_mean: return "sqrt((a² + b²) / 2); leans toward the bigger one";
    case CombineMode::minimum: return "the smaller of the two";
    case CombineMode::multiply: return "a * b; Bullet's default, ice stays slippery against anything";
    case CombineMode::maximum: return "the bigger of the two; one bouncy thing bounces off everything";
    case CombineMode::sum: return "a + b; both add up";
    }
    return "";
}

const std::vector<CombineMode>& combine_modes() {
    static const std::vector<CombineMode> modes = {CombineMode::project_default, CombineMode::average, CombineMode::geometric_mean, CombineMode::harmonic_mean, CombineMode::quadratic_mean, CombineMode::minimum, CombineMode::multiply, CombineMode::maximum, CombineMode::sum};
    return modes;
}

namespace {

std::string lower_dash(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '_' || c == ' ') c = '-';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        out.push_back(c);
    }
    return out;
}

std::string escape_token(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '%' || c == ' ' || c == '\t' || c == '\n') {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
            out += buf;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string unescape_token(const std::string& s) {
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

std::string vec_token(Vec3 v) {
    return number(v.x) + "," + number(v.y) + "," + number(v.z);
}

bool parse_vec_token(const std::string& s, Vec3& out) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (std::sscanf(s.c_str(), "%f,%f,%f", &x, &y, &z) != 3) return false;
    out = {x, y, z};
    return true;
}

bool same(Vec3 a, Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

} // namespace

const char* to_string(LightType v) {
    switch (v) {
    case LightType::point: return "point";
    case LightType::spot: return "spot";
    case LightType::directional: return "directional";
    case LightType::sphere: return "sphere";
    case LightType::disk: return "disk";
    case LightType::rectangle: return "rectangle";
    case LightType::tube: return "tube";
    }
    return "point";
}

const char* to_string(LightMobility v) {
    switch (v) {
    case LightMobility::dynamic: return "dynamic";
    case LightMobility::stationary: return "stationary";
    case LightMobility::baked: return "baked";
    }
    return "dynamic";
}

const char* to_string(LightUnit v) {
    switch (v) {
    case LightUnit::unitless: return "unitless";
    case LightUnit::candela: return "candela";
    case LightUnit::lumens: return "lumens";
    case LightUnit::lux: return "lux";
    case LightUnit::nits: return "nits";
    case LightUnit::watts: return "watts";
    }
    return "unitless";
}

const char* to_string(LightFalloff v) {
    switch (v) {
    case LightFalloff::physical: return "physical";
    case LightFalloff::linear: return "linear";
    case LightFalloff::smooth: return "smooth";
    case LightFalloff::constant: return "constant";
    case LightFalloff::custom: return "custom";
    }
    return "physical";
}

std::optional<LightType> parse_light_type(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "point" || s == "omni" || s == "bulb") return LightType::point;
    if (s == "spot" || s == "cone") return LightType::spot;
    if (s == "directional" || s == "sun" || s == "direction") return LightType::directional;
    if (s == "sphere" || s == "ball") return LightType::sphere;
    if (s == "disk" || s == "disc" || s == "circle") return LightType::disk;
    if (s == "rectangle" || s == "rect" || s == "area" || s == "panel" || s == "window") return LightType::rectangle;
    if (s == "tube" || s == "line" || s == "capsule" || s == "strip") return LightType::tube;
    return std::nullopt;
}

std::optional<LightMobility> parse_light_mobility(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "dynamic" || s == "movable" || s == "realtime" || s == "real-time") return LightMobility::dynamic;
    if (s == "stationary" || s == "mixed") return LightMobility::stationary;
    if (s == "baked" || s == "static") return LightMobility::baked;
    return std::nullopt;
}

std::optional<LightUnit> parse_light_unit(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "unitless" || s == "none" || s == "artist") return LightUnit::unitless;
    if (s == "candela" || s == "cd") return LightUnit::candela;
    if (s == "lumens" || s == "lumen" || s == "lm") return LightUnit::lumens;
    if (s == "lux" || s == "lx") return LightUnit::lux;
    if (s == "nits" || s == "nit") return LightUnit::nits;
    if (s == "watts" || s == "watt" || s == "w") return LightUnit::watts;
    return std::nullopt;
}

std::optional<LightFalloff> parse_light_falloff(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "physical" || s == "inverse-square" || s == "realistic") return LightFalloff::physical;
    if (s == "linear") return LightFalloff::linear;
    if (s == "smooth" || s == "quadratic") return LightFalloff::smooth;
    if (s == "constant" || s == "none") return LightFalloff::constant;
    if (s == "custom" || s == "exponent") return LightFalloff::custom;
    return std::nullopt;
}

const std::vector<LightType>& light_types() {
    static const std::vector<LightType> all = {LightType::point, LightType::spot, LightType::directional, LightType::sphere, LightType::disk, LightType::rectangle, LightType::tube};
    return all;
}

const char* light_type_label(LightType v) {
    switch (v) {
    case LightType::point: return "Point (bulb)";
    case LightType::spot: return "Spot (cone)";
    case LightType::directional: return "Directional (sun, moon)";
    case LightType::sphere: return "Sphere (round bulb, soft shadows)";
    case LightType::disk: return "Disk (downlight, round panel)";
    case LightType::rectangle: return "Rectangle (window, TV, panel)";
    case LightType::tube: return "Tube (fluorescent, neon, strip)";
    }
    return "Point";
}

Light Light::sun(float lux) {
    Light l;
    l.type = LightType::directional;
    l.intensity = lux;
    return l;
}

Light Light::spot(float strength, float outer, float inner) {
    Light l;
    l.type = LightType::spot;
    l.intensity = strength;
    l.outer_cone = outer;
    l.inner_cone = inner;
    return l;
}

const char* to_string(LightAnimationMode v) {
    switch (v) {
    case LightAnimationMode::none: return "none";
    case LightAnimationMode::strobe: return "strobe";
    case LightAnimationMode::pulse: return "pulse";
    case LightAnimationMode::flicker: return "flicker";
    case LightAnimationMode::fade: return "fade";
    }
    return "none";
}

const char* to_string(LightColorMode v) {
    switch (v) {
    case LightColorMode::none: return "none";
    case LightColorMode::alternate: return "alternate";
    case LightColorMode::blend: return "blend";
    case LightColorMode::rainbow: return "rainbow";
    }
    return "none";
}

std::optional<LightAnimationMode> parse_light_animation_mode(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "none" || s == "off" || s == "steady" || s.empty()) return LightAnimationMode::none;
    if (s == "strobe" || s == "flash" || s == "blink") return LightAnimationMode::strobe;
    if (s == "pulse" || s == "breathe" || s == "sine") return LightAnimationMode::pulse;
    if (s == "flicker" || s == "candle" || s == "fire") return LightAnimationMode::flicker;
    if (s == "fade" || s == "saw" || s == "ramp") return LightAnimationMode::fade;
    return std::nullopt;
}

std::optional<LightColorMode> parse_light_color_mode(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "none" || s == "off" || s.empty()) return LightColorMode::none;
    if (s == "alternate" || s == "swap" || s == "toggle") return LightColorMode::alternate;
    if (s == "blend" || s == "mix" || s == "fade") return LightColorMode::blend;
    if (s == "rainbow" || s == "hue" || s == "cycle") return LightColorMode::rainbow;
    return std::nullopt;
}

const std::vector<LightAnimationMode>& light_animation_modes() {
    static const std::vector<LightAnimationMode> all = {LightAnimationMode::none, LightAnimationMode::strobe, LightAnimationMode::pulse, LightAnimationMode::flicker, LightAnimationMode::fade};
    return all;
}

const std::vector<LightColorMode>& light_color_modes() {
    static const std::vector<LightColorMode> all = {LightColorMode::none, LightColorMode::alternate, LightColorMode::blend, LightColorMode::rainbow};
    return all;
}

const char* light_animation_label(LightAnimationMode v) {
    switch (v) {
    case LightAnimationMode::none: return "Steady";
    case LightAnimationMode::strobe: return "Strobe (on and off)";
    case LightAnimationMode::pulse: return "Pulse (smooth breathing)";
    case LightAnimationMode::flicker: return "Flicker (candle, broken bulb)";
    case LightAnimationMode::fade: return "Fade (bright, then dims)";
    }
    return "Steady";
}

const char* light_color_mode_label(LightColorMode v) {
    switch (v) {
    case LightColorMode::none: return "Keep its color";
    case LightColorMode::alternate: return "Alternate with the second color";
    case LightColorMode::blend: return "Blend to the second color";
    case LightColorMode::rainbow: return "Rainbow";
    }
    return "Keep its color";
}

namespace {

float fract(double x) {
    return static_cast<float>(x - std::floor(x));
}

float hash01(std::uint32_t seed, std::int64_t i) {
    std::uint64_t x = static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL + seed * 0xBF58476D1CE4E5B9ULL + 0x632BE59BD9B4E019ULL;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return static_cast<float>(x >> 40) / 16777216.0f;
}

float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vec3 hue(float h) {
    const float r = std::clamp(std::fabs(h * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
    const float g = std::clamp(2.0f - std::fabs(h * 6.0f - 2.0f), 0.0f, 1.0f);
    const float b = std::clamp(2.0f - std::fabs(h * 6.0f - 4.0f), 0.0f, 1.0f);
    return {r, g, b};
}

} // namespace

LightAnimationSample LightAnimation::evaluate(double seconds, const Vec3& base) const {
    LightAnimationSample out;
    out.color = base;
    if (!active()) return out;
    double t = seconds - static_cast<double>(delay);
    if (t < 0.0) return out;
    if (duration > 0.0f) {
        const double d = static_cast<double>(duration);
        if (t >= d) {
            if (!loop) return out;
            t = std::fmod(t, d);
        }
    }
    out.running = true;
    const float p = duration > 0.0f ? static_cast<float>(t / static_cast<double>(duration)) : 0.0f;
    const double r0 = std::max(0.0f, rate);
    const double r1 = std::max(0.0f, rate_end);
    double cycles = r0 * t;
    if (duration > 0.0f) cycles += (r1 - r0) * t * t / (2.0 * static_cast<double>(duration));
    cycles += static_cast<double>(phase);
    const float c = fract(cycles);
    const std::int64_t index = static_cast<std::int64_t>(std::floor(cycles));
    const float peak = high + (high_end - high) * p;
    float wave = 1.0f;
    switch (mode) {
    case LightAnimationMode::none: wave = 1.0f; break;
    case LightAnimationMode::strobe: {
        const float on = std::clamp(on_fraction, 0.0f, 1.0f);
        const float edge = std::clamp(softness, 0.0f, 1.0f) * 0.5f * std::min(on, 1.0f - on);
        if (edge <= 1e-5f) wave = c < on ? 1.0f : 0.0f;
        else wave = smooth01(c / edge) * (1.0f - smooth01((c - on + edge) / edge));
        break;
    }
    case LightAnimationMode::pulse: wave = 0.5f - 0.5f * std::cos(6.28318530718f * c); break;
    case LightAnimationMode::flicker: {
        const float a = hash01(seed, index), b = hash01(seed, index + 1);
        const float k = softness > 0.0f ? smooth01(c) : (c < 0.5f ? 0.0f : 1.0f);
        wave = a + (b - a) * k;
        break;
    }
    case LightAnimationMode::fade: wave = 1.0f - c; break;
    }
    if (mode == LightAnimationMode::none) out.brightness = 1.0f;
    else out.brightness = std::max(0.0f, low + (peak - low) * wave);
    out.on = mode == LightAnimationMode::none || wave > 0.5f;
    switch (color_mode) {
    case LightColorMode::none: break;
    case LightColorMode::alternate: out.color = (index & 1) ? color_b : base; break;
    case LightColorMode::blend: {
        const float k = mode == LightAnimationMode::none ? 0.5f - 0.5f * std::cos(6.28318530718f * c) : wave;
        out.color = {base.x + (color_b.x - base.x) * k, base.y + (color_b.y - base.y) * k, base.z + (color_b.z - base.z) * k};
        break;
    }
    case LightColorMode::rainbow: {
        const Vec3 h = hue(c);
        const float y = std::max(0.2126f * base.x + 0.7152f * base.y + 0.0722f * base.z, 1e-3f);
        const float hy = std::max(0.2126f * h.x + 0.7152f * h.y + 0.0722f * h.z, 1e-3f);
        out.color = {h.x * y / hy, h.y * y / hy, h.z * y / hy};
        break;
    }
    }
    return out;
}

LightAnimation LightAnimation::strobe(float flashes, float on) {
    LightAnimation a;
    a.mode = LightAnimationMode::strobe;
    a.rate = flashes;
    a.rate_end = flashes;
    a.on_fraction = on;
    return a;
}

LightAnimation LightAnimation::pulse(float cycles, float low) {
    LightAnimation a;
    a.mode = LightAnimationMode::pulse;
    a.rate = cycles;
    a.rate_end = cycles;
    a.low = low;
    return a;
}

LightAnimation LightAnimation::flicker(float speed, float low) {
    LightAnimation a;
    a.mode = LightAnimationMode::flicker;
    a.rate = speed;
    a.rate_end = speed;
    a.low = low;
    a.softness = 1.0f;
    return a;
}

Light Light::area(float w, float h, float strength) {
    Light l;
    l.type = LightType::rectangle;
    l.width = w;
    l.height = h;
    l.intensity = strength;
    return l;
}

const std::vector<std::string>& Material::presets() {
    static const std::vector<std::string> all = {"default", "matte", "plastic", "rubber", "metal", "gold", "copper", "chrome", "mirror", "glass", "frosted-glass", "flint-glass", "crystal", "diamond", "water", "ice", "window", "glow"};
    return all;
}

std::optional<Material> Material::named(const std::string& name) {
    const std::string s = lower_dash(name);
    Material m;
    m.preset = s;
    if (s == "default") return m;
    if (s == "matte") {
        m.roughness = 1.0f;
        m.specular = 0.0f;
        return m;
    }
    if (s == "plastic") {
        m.roughness = 0.35f;
        return m;
    }
    if (s == "rubber") {
        m.color = {0.05f, 0.05f, 0.05f};
        m.roughness = 0.9f;
        m.specular = 0.3f;
        return m;
    }
    if (s == "metal" || s == "gold" || s == "copper" || s == "chrome" || s == "mirror") {
        m.metallic = 1.0f;
        m.roughness = s == "mirror" ? 0.0f : (s == "chrome" ? 0.05f : 0.2f);
        if (s == "gold") m.color = {1.0f, 0.766f, 0.336f};
        else if (s == "copper") m.color = {0.955f, 0.638f, 0.538f};
        else if (s == "metal") m.color = {0.75f, 0.75f, 0.75f};
        else m.color = {0.97f, 0.97f, 0.97f};
        return m;
    }
    if (s == "glass" || s == "frosted-glass" || s == "flint-glass" || s == "crystal" || s == "diamond" || s == "water" || s == "ice" || s == "window") {
        m.color = {1.0f, 1.0f, 1.0f};
        m.roughness = 0.0f;
        m.transmission = 1.0f;
        m.ior = 1.52f;
        m.abbe = 64.0f;
        if (s == "frosted-glass") m.roughness = 0.3f;
        if (s == "flint-glass") {
            m.ior = 1.72f;
            m.abbe = 29.0f;
        }
        if (s == "crystal") {
            m.ior = 1.65f;
            m.abbe = 33.0f;
        }
        if (s == "diamond") {
            m.ior = 2.417f;
            m.abbe = 55.3f;
        }
        if (s == "water") {
            m.ior = 1.333f;
            m.abbe = 55.7f;
            m.absorption_color = {0.55f, 0.85f, 0.92f};
            m.absorption_distance = 4.0f;
        }
        if (s == "ice") {
            m.ior = 1.309f;
            m.abbe = 56.0f;
            m.roughness = 0.08f;
            m.absorption_color = {0.85f, 0.95f, 1.0f};
            m.absorption_distance = 2.0f;
        }
        if (s == "window") {
            m.abbe = 0.0f;
            m.thin = true;
        }
        return m;
    }
    if (s == "glow" || s == "emissive" || s == "neon") {
        m.color = {0.0f, 0.0f, 0.0f};
        m.emission = {1.0f, 1.0f, 1.0f};
        m.emission_strength = 5.0f;
        return m;
    }
    return std::nullopt;
}

std::optional<Dimension> parse_dimension(const std::string& text) {
    if (text == "2d" || text == "2D" || text == "two_d") return Dimension::two_d;
    if (text == "3d" || text == "3D" || text == "three_d") return Dimension::three_d;
    return std::nullopt;
}

Scene::Scene(Dimension dimension) : m_dimension(dimension) {}

Scene::Slot* Scene::slot(Entity e) {
    if (!e.valid() || e.index >= m_slots.size()) return nullptr;
    Slot& s = m_slots[e.index];
    return s.used && s.generation == e.generation ? &s : nullptr;
}

const Scene::Slot* Scene::slot(Entity e) const {
    if (!e.valid() || e.index >= m_slots.size()) return nullptr;
    const Slot& s = m_slots[e.index];
    return s.used && s.generation == e.generation ? &s : nullptr;
}

Entity Scene::create(const std::string& name, Entity parent) {
    std::uint32_t index;
    if (!m_free.empty()) {
        index = m_free.back();
        m_free.pop_back();
    } else {
        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }
    Slot& s = m_slots[index];
    s.used = true;
    s.name = name;
    s.transform = Transform{};
    s.parent = alive(parent) ? parent : Entity{};
    s.body.reset();
    s.colliders.clear();
    s.script.clear();
    s.light.reset();
    s.material.reset();
    ++m_alive;
    return {index, s.generation};
}

bool Scene::destroy(Entity e) {
    if (!slot(e)) return false;
    for (Entity child : children(e)) destroy(child);
    Slot& s = m_slots[e.index];
    s.used = false;
    s.name.clear();
    s.parent = Entity{};
    s.body.reset();
    s.colliders.clear();
    s.script.clear();
    s.light.reset();
    s.material.reset();
    ++s.generation;
    if (s.generation == 0) s.generation = 1;
    m_free.push_back(e.index);
    --m_alive;
    return true;
}

bool Scene::alive(Entity e) const {
    return slot(e) != nullptr;
}

void Scene::clear() {
    m_slots.clear();
    m_free.clear();
    m_alive = 0;
}

const std::string& Scene::name(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->name : empty_name;
}

bool Scene::set_name(Entity e, const std::string& name) {
    Slot* s = slot(e);
    if (!s) return false;
    s->name = name;
    return true;
}

Transform* Scene::transform(Entity e) {
    Slot* s = slot(e);
    return s ? &s->transform : nullptr;
}

const Transform* Scene::transform(Entity e) const {
    const Slot* s = slot(e);
    return s ? &s->transform : nullptr;
}

Mat4 Scene::world_matrix(Entity e) const {
    Mat4 m;
    const Slot* s = slot(e);
    int depth = 0;
    while (s && depth < 1024) {
        m = trs(s->transform.position, s->transform.rotation, s->transform.scale) * m;
        s = slot(s->parent);
        ++depth;
    }
    return m;
}

Entity Scene::parent(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->parent : Entity{};
}

bool Scene::is_ancestor(Entity maybe_ancestor, Entity e) const {
    Entity p = parent(e);
    while (p.valid()) {
        if (p == maybe_ancestor) return true;
        p = parent(p);
    }
    return false;
}

bool Scene::set_parent(Entity e, Entity parent) {
    Slot* s = slot(e);
    if (!s) return false;
    if (parent.valid() && (!alive(parent) || parent == e || is_ancestor(e, parent))) return false;
    s->parent = parent;
    return true;
}

std::vector<Entity> Scene::children(Entity e) const {
    std::vector<Entity> out;
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && s.parent == e && e.valid()) out.push_back({i, s.generation});
    }
    return out;
}

std::vector<Entity> Scene::roots() const {
    std::vector<Entity> out;
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && !alive(s.parent)) out.push_back({i, s.generation});
    }
    return out;
}

std::vector<Entity> Scene::entities() const {
    std::vector<Entity> out;
    out.reserve(m_alive);
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].used) out.push_back({i, m_slots[i].generation});
    }
    return out;
}

Entity Scene::find(const std::string& name) const {
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && s.name == name) return {i, s.generation};
    }
    return Entity{};
}

Body* Scene::body(Entity e) {
    Slot* s = slot(e);
    return s && s->body ? &*s->body : nullptr;
}

const Body* Scene::body(Entity e) const {
    const Slot* s = slot(e);
    return s && s->body ? &*s->body : nullptr;
}

Body& Scene::add_body(Entity e) {
    static Body none;
    Slot* s = slot(e);
    if (!s) {
        none = Body{};
        return none;
    }
    if (!s->body) {
        s->body = Body{};
        if (m_dimension == Dimension::two_d) s->body->set(body_lock_position_z, true);
    }
    return *s->body;
}

bool Scene::remove_body(Entity e) {
    Slot* s = slot(e);
    if (!s || !s->body) return false;
    s->body.reset();
    return true;
}

std::vector<Collider>* Scene::colliders(Entity e) {
    Slot* s = slot(e);
    return s ? &s->colliders : nullptr;
}

const std::vector<Collider>* Scene::colliders(Entity e) const {
    const Slot* s = slot(e);
    return s ? &s->colliders : nullptr;
}

Collider* Scene::add_collider(Entity e, ColliderShape shape) {
    Slot* s = slot(e);
    if (!s) return nullptr;
    Collider c;
    c.shape = shape;
    if (shape == ColliderShape::capsule || shape == ColliderShape::cylinder || shape == ColliderShape::cone) c.size = {0.5f, 1.0f, 0.5f};
    if (m_dimension == Dimension::two_d && shape == ColliderShape::box) c.size.z = 1.0f;
    s->colliders.push_back(c);
    return &s->colliders.back();
}

bool Scene::remove_collider(Entity e, std::size_t index) {
    Slot* s = slot(e);
    if (!s || index >= s->colliders.size()) return false;
    s->colliders.erase(s->colliders.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

Entity Scene::body_owner(Entity e) const {
    Entity at = e;
    int depth = 0;
    while (alive(at) && depth < 1024) {
        if (body(at)) return at;
        at = parent(at);
        ++depth;
    }
    return Entity{};
}

const std::string& Scene::script(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->script : empty_name;
}

bool Scene::set_script(Entity e, const std::string& path) {
    Slot* s = slot(e);
    if (!s) return false;
    s->script = path;
    return true;
}

Light* Scene::light(Entity e) {
    Slot* s = slot(e);
    return s && s->light ? &*s->light : nullptr;
}

const Light* Scene::light(Entity e) const {
    const Slot* s = slot(e);
    return s && s->light ? &*s->light : nullptr;
}

Light& Scene::add_light(Entity e, LightType type) {
    static Light none;
    Slot* s = slot(e);
    if (!s) {
        none = Light{};
        return none;
    }
    if (!s->light) {
        switch (type) {
        case LightType::directional: s->light = Light::sun(); break;
        case LightType::spot: s->light = Light::spot(); break;
        case LightType::rectangle: s->light = Light::area(); break;
        default: s->light = Light{}; break;
        }
        s->light->type = type;
        if (type == LightType::sphere) s->light->radius = 0.1f;
        if (type == LightType::disk) s->light->radius = 0.25f;
        if (type == LightType::tube) s->light->radius = 0.025f;
    }
    return *s->light;
}

bool Scene::remove_light(Entity e) {
    Slot* s = slot(e);
    if (!s || !s->light) return false;
    s->light.reset();
    return true;
}

Material* Scene::material(Entity e) {
    Slot* s = slot(e);
    return s && s->material ? &*s->material : nullptr;
}

const Material* Scene::material(Entity e) const {
    const Slot* s = slot(e);
    return s && s->material ? &*s->material : nullptr;
}

Material& Scene::add_material(Entity e) {
    static Material none;
    Slot* s = slot(e);
    if (!s) {
        none = Material{};
        return none;
    }
    if (!s->material) s->material = Material{};
    return *s->material;
}

bool Scene::remove_material(Entity e) {
    Slot* s = slot(e);
    if (!s || !s->material) return false;
    s->material.reset();
    return true;
}

namespace {

std::string light_line(std::size_t n, const Light& l) {
    const Light d;
    std::string out = "light " + std::to_string(n) + " " + to_string(l.type);
    auto f = [&out](const char* key, float v, float def) {
        if (v != def) out += std::string(" ") + key + ":" + number(v);
    };
    auto b = [&out](const char* key, bool v, bool def) {
        if (v != def) out += std::string(" ") + key + ":" + (v ? "1" : "0");
    };
    if (l.mobility != d.mobility) out += std::string(" mobility:") + to_string(l.mobility);
    b("enabled", l.enabled, d.enabled);
    if (!same(l.color, d.color)) out += " color:" + vec_token(l.color);
    f("temperature", l.temperature, d.temperature);
    out += " intensity:" + number(l.intensity);
    if (l.unit != d.unit) out += std::string(" unit:") + to_string(l.unit);
    f("range", l.range, d.range);
    if (l.falloff != d.falloff) out += std::string(" falloff:") + to_string(l.falloff);
    f("falloff-exponent", l.falloff_exponent, d.falloff_exponent);
    f("radius", l.radius, d.radius);
    f("length", l.length, d.length);
    f("width", l.width, d.width);
    f("height", l.height, d.height);
    b("two-sided", l.two_sided, d.two_sided);
    f("inner-cone", l.inner_cone, d.inner_cone);
    f("outer-cone", l.outer_cone, d.outer_cone);
    f("angular-diameter", l.angular_diameter, d.angular_diameter);
    b("shadows", l.shadows, d.shadows);
    f("shadow-strength", l.shadow_strength, d.shadow_strength);
    f("shadow-bias", l.shadow_bias, d.shadow_bias);
    if (l.shadow_samples != d.shadow_samples) out += " shadow-samples:" + std::to_string(l.shadow_samples);
    f("diffuse", l.diffuse, d.diffuse);
    f("specular", l.specular, d.specular);
    f("indirect", l.indirect, d.indirect);
    f("volumetric", l.volumetric, d.volumetric);
    b("visible", l.visible, d.visible);
    if (l.channels != d.channels) out += " channels:" + std::to_string(l.channels);
    if (!l.ies.empty()) out += " ies:" + escape_token(l.ies);
    if (!l.cookie.empty()) out += " cookie:" + escape_token(l.cookie);
    f("cookie-size", l.cookie_size, d.cookie_size);
    const LightAnimation& a = l.animation;
    const LightAnimation da;
    if (a.mode != da.mode) out += std::string(" anim:") + to_string(a.mode);
    f("anim-rate", a.rate, da.rate);
    f("anim-rate-end", a.rate_end, da.rate_end);
    f("anim-on", a.on_fraction, da.on_fraction);
    f("anim-soft", a.softness, da.softness);
    f("anim-duration", a.duration, da.duration);
    f("anim-delay", a.delay, da.delay);
    b("anim-loop", a.loop, da.loop);
    f("anim-low", a.low, da.low);
    f("anim-high", a.high, da.high);
    f("anim-high-end", a.high_end, da.high_end);
    if (a.color_mode != da.color_mode) out += std::string(" anim-color:") + to_string(a.color_mode);
    if (!same(a.color_b, da.color_b)) out += " anim-color-b:" + vec_token(a.color_b);
    f("anim-phase", a.phase, da.phase);
    if (a.seed != da.seed) out += " anim-seed:" + std::to_string(a.seed);
    return out + "\n";
}

std::string material_line(std::size_t n, const Material& m) {
    const Material d;
    std::string out = "material " + std::to_string(n);
    auto f = [&out](const char* key, float v, float def) {
        if (v != def) out += std::string(" ") + key + ":" + number(v);
    };
    auto b = [&out](const char* key, bool v, bool def) {
        if (v != def) out += std::string(" ") + key + ":" + (v ? "1" : "0");
    };
    if (!m.preset.empty()) out += " preset:" + escape_token(m.preset);
    if (!same(m.color, d.color)) out += " color:" + vec_token(m.color);
    f("metallic", m.metallic, d.metallic);
    f("roughness", m.roughness, d.roughness);
    f("specular", m.specular, d.specular);
    if (!same(m.emission, d.emission)) out += " emission:" + vec_token(m.emission);
    f("emission-strength", m.emission_strength, d.emission_strength);
    f("transmission", m.transmission, d.transmission);
    f("ior", m.ior, d.ior);
    f("abbe", m.abbe, d.abbe);
    b("thin", m.thin, d.thin);
    if (!same(m.absorption_color, d.absorption_color)) out += " absorption:" + vec_token(m.absorption_color);
    f("absorption-distance", m.absorption_distance, d.absorption_distance);
    f("opacity", m.opacity, d.opacity);
    b("double-sided", m.double_sided, d.double_sided);
    b("casts-shadows", m.casts_shadows, d.casts_shadows);
    b("static", m.is_static, d.is_static);
    b("visible", m.visible, d.visible);
    if (m.light_channels != d.light_channels) out += " light-channels:" + std::to_string(m.light_channels);
    return out + "\n";
}

bool read_light(std::istringstream& row, Light& l, std::string& bad) {
    std::string type;
    row >> type;
    const std::optional<LightType> t = parse_light_type(type);
    if (!t) {
        bad = "unknown light type '" + type + "'";
        return false;
    }
    l.type = *t;
    for (std::string token; row >> token;) {
        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = token.substr(0, colon), v = token.substr(colon + 1);
        const float x = static_cast<float>(std::atof(v.c_str()));
        if (key == "mobility") {
            if (const auto m = parse_light_mobility(v)) l.mobility = *m;
        } else if (key == "enabled") l.enabled = v != "0";
        else if (key == "color") parse_vec_token(v, l.color);
        else if (key == "temperature") l.temperature = x;
        else if (key == "intensity") l.intensity = x;
        else if (key == "unit") {
            if (const auto u = parse_light_unit(v)) l.unit = *u;
        } else if (key == "range") l.range = x;
        else if (key == "falloff") {
            if (const auto fo = parse_light_falloff(v)) l.falloff = *fo;
        } else if (key == "falloff-exponent") l.falloff_exponent = x;
        else if (key == "radius") l.radius = x;
        else if (key == "length") l.length = x;
        else if (key == "width") l.width = x;
        else if (key == "height") l.height = x;
        else if (key == "two-sided") l.two_sided = v != "0";
        else if (key == "inner-cone") l.inner_cone = x;
        else if (key == "outer-cone") l.outer_cone = x;
        else if (key == "angular-diameter") l.angular_diameter = x;
        else if (key == "shadows") l.shadows = v != "0";
        else if (key == "shadow-strength") l.shadow_strength = x;
        else if (key == "shadow-bias") l.shadow_bias = x;
        else if (key == "shadow-samples") l.shadow_samples = std::atoi(v.c_str());
        else if (key == "diffuse") l.diffuse = x;
        else if (key == "specular") l.specular = x;
        else if (key == "indirect") l.indirect = x;
        else if (key == "volumetric") l.volumetric = x;
        else if (key == "visible") l.visible = v != "0";
        else if (key == "channels") l.channels = static_cast<std::uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
        else if (key == "ies") l.ies = unescape_token(v);
        else if (key == "cookie") l.cookie = unescape_token(v);
        else if (key == "cookie-size") l.cookie_size = x;
        else if (key == "anim") {
            if (const auto m = parse_light_animation_mode(v)) l.animation.mode = *m;
        } else if (key == "anim-rate") l.animation.rate = x;
        else if (key == "anim-rate-end") l.animation.rate_end = x;
        else if (key == "anim-on") l.animation.on_fraction = x;
        else if (key == "anim-soft") l.animation.softness = x;
        else if (key == "anim-duration") l.animation.duration = x;
        else if (key == "anim-delay") l.animation.delay = x;
        else if (key == "anim-loop") l.animation.loop = v != "0";
        else if (key == "anim-low") l.animation.low = x;
        else if (key == "anim-high") l.animation.high = x;
        else if (key == "anim-high-end") l.animation.high_end = x;
        else if (key == "anim-color") {
            if (const auto m = parse_light_color_mode(v)) l.animation.color_mode = *m;
        } else if (key == "anim-color-b") parse_vec_token(v, l.animation.color_b);
        else if (key == "anim-phase") l.animation.phase = x;
        else if (key == "anim-seed") l.animation.seed = static_cast<std::uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
    }
    return true;
}

void read_material(std::istringstream& row, Material& m) {
    for (std::string token; row >> token;) {
        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = token.substr(0, colon), v = token.substr(colon + 1);
        const float x = static_cast<float>(std::atof(v.c_str()));
        if (key == "preset") m.preset = unescape_token(v);
        else if (key == "color") parse_vec_token(v, m.color);
        else if (key == "metallic") m.metallic = x;
        else if (key == "roughness") m.roughness = x;
        else if (key == "specular") m.specular = x;
        else if (key == "emission") parse_vec_token(v, m.emission);
        else if (key == "emission-strength") m.emission_strength = x;
        else if (key == "transmission") m.transmission = x;
        else if (key == "ior") m.ior = x;
        else if (key == "abbe") m.abbe = x;
        else if (key == "thin") m.thin = v != "0";
        else if (key == "absorption") parse_vec_token(v, m.absorption_color);
        else if (key == "absorption-distance") m.absorption_distance = x;
        else if (key == "opacity") m.opacity = x;
        else if (key == "double-sided") m.double_sided = v != "0";
        else if (key == "casts-shadows") m.casts_shadows = v != "0";
        else if (key == "static") m.is_static = v != "0";
        else if (key == "visible") m.visible = v != "0";
        else if (key == "light-channels") m.light_channels = static_cast<std::uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
    }
}

} // namespace

std::string Scene::save() const {
    std::ostringstream out;
    out << "orchard-scene 3 " << to_string(m_dimension) << "\n";
    std::unordered_map<std::uint64_t, std::size_t> numbers;
    const std::vector<Entity> all = entities();
    for (std::size_t i = 0; i < all.size(); ++i) numbers[all[i].id()] = i + 1;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const Slot& s = m_slots[all[i].index];
        const Vec3 r = to_euler(s.transform.rotation);
        const auto p = numbers.find(s.parent.id());
        out << "entity " << (i + 1) << " " << (alive(s.parent) && p != numbers.end() ? p->second : 0)
            << " " << number(s.transform.position.x) << " " << number(s.transform.position.y) << " " << number(s.transform.position.z)
            << " " << number(r.x) << " " << number(r.y) << " " << number(r.z)
            << " " << number(s.transform.scale.x) << " " << number(s.transform.scale.y) << " " << number(s.transform.scale.z)
            << " " << s.name << "\n";
        const std::size_t n = i + 1;
        if (!s.script.empty()) out << "script " << n << " " << s.script << "\n";
        if (s.body) {
            const Body& b = *s.body;
            const Body base;
            out << "body " << n << " " << to_string(b.type) << " " << (b.enabled ? 1 : 0) << " " << b.flags << " "
                << number(b.gravity_scale) << " " << number(b.linear_damping) << " " << number(b.angular_damping);
            if (b.collision_layer != base.collision_layer) out << " layer:" << b.collision_layer;
            if (b.collision_mask != base.collision_mask) out << " mask:" << b.collision_mask;
            if (b.quality != base.quality) out << " quality:" << to_string(b.quality);
            if (b.continuous != base.continuous) out << " ccd:" << to_string(b.continuous);
            if (b.velocity_iterations != base.velocity_iterations) out << " viters:" << b.velocity_iterations;
            if (b.position_iterations != base.position_iterations) out << " piters:" << b.position_iterations;
            if (b.max_speed != base.max_speed) out << " max-speed:" << number(b.max_speed);
            if (b.max_spin != base.max_spin) out << " max-spin:" << number(b.max_spin);
            if (b.sleep_speed != base.sleep_speed) out << " sleep-speed:" << number(b.sleep_speed);
            if (b.sleep_spin != base.sleep_spin) out << " sleep-spin:" << number(b.sleep_spin);
            if (b.sleep_delay != base.sleep_delay) out << " sleep-delay:" << number(b.sleep_delay);
            if (b.flags_on) out << " on:" << b.flags_on;
            if (b.flags_off) out << " off:" << b.flags_off;
            out << "\n";
        }
        for (const Collider& c : s.colliders) {
            out << "collider " << n << " " << to_string(c.shape)
                << " " << number(c.size.x) << " " << number(c.size.y) << " " << number(c.size.z)
                << " " << number(c.offset.x) << " " << number(c.offset.y) << " " << number(c.offset.z)
                << " " << (c.enabled ? 1 : 0) << " " << (c.sensor ? 1 : 0)
                << " " << number(c.density) << " " << number(c.friction) << " " << number(c.restitution);
            if (c.friction_combine != CombineMode::project_default) out << " friction-mix:" << to_string(c.friction_combine);
            if (c.bounce_combine != CombineMode::project_default) out << " bounce-mix:" << to_string(c.bounce_combine);
            const Collider base;
            if (c.rounding != base.rounding) out << " rounding:" << number(c.rounding);
            if (c.rolling_friction != base.rolling_friction) out << " roll:" << number(c.rolling_friction);
            if (c.spinning_friction != base.spinning_friction) out << " spin:" << number(c.spinning_friction);
            if (c.grip.x != 1.0f || c.grip.y != 1.0f || c.grip.z != 1.0f) out << " grip:" << number(c.grip.x) << "," << number(c.grip.y) << "," << number(c.grip.z);
            out << "\n";
        }
        if (s.light) out << light_line(n, *s.light);
        if (s.material) out << material_line(n, *s.material);
    }
    return out.str();
}

std::optional<Scene> Scene::load(const std::string& text, std::string* error) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) {
        if (error) *error = "empty scene file";
        return std::nullopt;
    }
    std::istringstream head(line);
    std::string magic, dim;
    int version = 0;
    head >> magic >> version >> dim;
    const std::optional<Dimension> d = parse_dimension(dim);
    if (magic != "orchard-scene" || version < 1 || version > 3 || !d) {
        if (error) *error = "not an Orchard scene (version 1 to 3)";
        return std::nullopt;
    }
    Scene scene(*d);
    std::unordered_map<std::size_t, Entity> by_number;
    std::vector<std::pair<Entity, std::size_t>> parents;
    int line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        std::size_t num = 0, parent = 0;
        row >> kind >> num;
        if (kind == "script" || kind == "body" || kind == "collider" || kind == "light" || kind == "material") {
            const auto owner = by_number.find(num);
            if (!row || owner == by_number.end()) {
                if (error) *error = "line " + std::to_string(line_no) + ": " + kind + " for an entity that isn't there";
                return std::nullopt;
            }
            if (kind == "light") {
                Light l;
                std::string bad;
                if (!read_light(row, l, bad)) {
                    if (error) *error = "line " + std::to_string(line_no) + ": " + bad;
                    return std::nullopt;
                }
                scene.add_light(owner->second, l.type) = l;
                continue;
            }
            if (kind == "material") {
                Material m;
                read_material(row, m);
                scene.add_material(owner->second) = m;
                continue;
            }
            if (kind == "script") {
                std::string path;
                std::getline(row >> std::ws, path);
                scene.set_script(owner->second, path);
            } else if (kind == "body") {
                std::string type;
                int enabled = 1;
                Body b;
                row >> type >> enabled >> b.flags >> b.gravity_scale >> b.linear_damping >> b.angular_damping;
                const std::optional<BodyType> bt = parse_body_type(type);
                if (!row || !bt) {
                    if (error) *error = "line " + std::to_string(line_no) + ": bad body line";
                    return std::nullopt;
                }
                b.type = *bt;
                b.enabled = enabled != 0;
                for (std::string extra; row >> extra;) {
                    const std::size_t colon = extra.find(':');
                    if (colon == std::string::npos) continue;
                    const std::string key = extra.substr(0, colon);
                    const std::string val = extra.substr(colon + 1);
                    const float fv = static_cast<float>(std::atof(val.c_str()));
                    if (key == "layer") b.collision_layer = std::strtoull(val.c_str(), nullptr, 10);
                    else if (key == "mask") b.collision_mask = std::strtoull(val.c_str(), nullptr, 10);
                    else if (key == "quality") b.quality = parse_body_quality(val).value_or(b.quality);
                    else if (key == "ccd") b.continuous = parse_body_continuous(val).value_or(b.continuous);
                    else if (key == "viters") b.velocity_iterations = std::clamp(std::atoi(val.c_str()), 0, 256);
                    else if (key == "piters") b.position_iterations = std::clamp(std::atoi(val.c_str()), 0, 256);
                    else if (key == "max-speed") b.max_speed = std::max(0.0f, fv);
                    else if (key == "max-spin") b.max_spin = std::max(0.0f, fv);
                    else if (key == "sleep-speed") b.sleep_speed = std::max(0.0f, fv);
                    else if (key == "sleep-spin") b.sleep_spin = std::max(0.0f, fv);
                    else if (key == "sleep-delay") b.sleep_delay = std::max(0.0f, fv);
                    else if (key == "on") b.flags_on = std::strtoull(val.c_str(), nullptr, 10);
                    else if (key == "off") b.flags_off = std::strtoull(val.c_str(), nullptr, 10);
                }
                scene.add_body(owner->second) = b;
            } else {
                std::string shape;
                int enabled = 1, sensor = 0;
                Collider c;
                row >> shape >> c.size.x >> c.size.y >> c.size.z >> c.offset.x >> c.offset.y >> c.offset.z >> enabled >> sensor >> c.density >> c.friction >> c.restitution;
                const std::optional<ColliderShape> cs = parse_collider_shape(shape);
                if (!row || !cs) {
                    if (error) *error = "line " + std::to_string(line_no) + ": bad collider line";
                    return std::nullopt;
                }
                c.shape = *cs;
                c.enabled = enabled != 0;
                c.sensor = sensor != 0;
                for (std::string extra; row >> extra;) {
                    const std::size_t colon = extra.find(':');
                    if (colon == std::string::npos) continue;
                    const std::string key = extra.substr(0, colon);
                    const std::string val = extra.substr(colon + 1);
                    if (key == "rounding") {
                        c.rounding = std::max(0.0f, static_cast<float>(std::atof(val.c_str())));
                        continue;
                    }
                    if (key == "roll") {
                        c.rolling_friction = std::max(0.0f, static_cast<float>(std::atof(val.c_str())));
                        continue;
                    }
                    if (key == "spin") {
                        c.spinning_friction = std::max(0.0f, static_cast<float>(std::atof(val.c_str())));
                        continue;
                    }
                    if (key == "grip") {
                        float g[3] = {1.0f, 1.0f, 1.0f};
                        std::sscanf(val.c_str(), "%f,%f,%f", &g[0], &g[1], &g[2]);
                        c.grip = {std::max(0.0f, g[0]), std::max(0.0f, g[1]), std::max(0.0f, g[2])};
                        continue;
                    }
                    const std::optional<CombineMode> mode = parse_combine_mode(val);
                    if (!mode) continue;
                    if (extra.compare(0, colon, "friction-mix") == 0) c.friction_combine = *mode;
                    else if (extra.compare(0, colon, "bounce-mix") == 0) c.bounce_combine = *mode;
                }
                scene.colliders(owner->second)->push_back(c);
            }
            continue;
        }
        Transform t;
        Vec3 r;
        row >> parent >> t.position.x >> t.position.y >> t.position.z >> r.x >> r.y >> r.z >> t.scale.x >> t.scale.y >> t.scale.z;
        if (kind != "entity" || !row) {
            if (error) *error = "line " + std::to_string(line_no) + ": bad entity line";
            return std::nullopt;
        }
        std::string name;
        std::getline(row >> std::ws, name);
        t.rotation = from_euler(r);
        const Entity e = scene.create(name);
        *scene.transform(e) = t;
        by_number[num] = e;
        if (parent != 0) parents.emplace_back(e, parent);
    }
    for (const auto& [e, p] : parents) {
        const auto it = by_number.find(p);
        if (it != by_number.end()) scene.set_parent(e, it->second);
    }
    return scene;
}

bool Scene::save_file(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << save();
    return static_cast<bool>(out);
}

std::optional<Scene> Scene::load_file(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "can't open " + path;
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return load(ss.str(), error);
}

} // namespace orchard
