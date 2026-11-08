#include <orchard/scene.hpp>

#include <algorithm>
#include <cctype>
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
    case ColliderShape::sphere_section: return "sphere-section";
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
    if (text == "sphere-section" || text == "sphere_section" || text == "hemisphere" || text == "dome" || text == "bowl" || text == "wedge") return ColliderShape::sphere_section;
    return std::nullopt;
}

const std::vector<ColliderShape>& collider_shapes() {
    static const std::vector<ColliderShape> all = {ColliderShape::box, ColliderShape::sphere, ColliderShape::capsule, ColliderShape::cylinder, ColliderShape::cone, ColliderShape::rounded_box, ColliderShape::sphere_section};
    return all;
}

bool collider_shape_3d_only(ColliderShape shape) {
    return shape == ColliderShape::cylinder || shape == ColliderShape::cone || shape == ColliderShape::sphere_section;
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

const char* to_string(LightSpread v) {
    switch (v) {
    case LightSpread::natural: return "natural";
    case LightSpread::blob: return "blob";
    }
    return "natural";
}

std::optional<LightSpread> parse_light_spread(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "natural" || s == "spill" || s == "realistic" || s == "physical") return LightSpread::natural;
    if (s == "blob" || s == "pool" || s == "compact") return LightSpread::blob;
    return std::nullopt;
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

bool Material::has_textures() const {
    return !base_map.empty() || !normal_map.empty() || !orm_map.empty() || !roughness_map.empty() || !metallic_map.empty() || !emission_map.empty() || !opacity_map.empty();
}

std::vector<std::string> Material::texture_paths() const {
    std::vector<std::string> out;
    for (const std::string* p : {&base_map, &normal_map, &orm_map, &roughness_map, &metallic_map, &emission_map, &opacity_map})
        if (!p->empty()) out.push_back(*p);
    return out;
}

bool is_image_file(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const char* const known[] = {"png", "apng", "jpg", "jpeg", "bmp", "gif", "webp", "tga", "qoi", "pnm", "ppm", "pgm", "ico", "tif", "tiff", "svg"};
    for (const char* k : known)
        if (ext == k) return true;
    return false;
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
        m.reflectivity = s == "mirror" ? 1.0f : (s == "chrome" ? 0.85f : 0.35f);
        return m;
    }
    if (s == "glass" || s == "frosted-glass" || s == "flint-glass" || s == "crystal" || s == "diamond" || s == "water" || s == "ice" || s == "window") {
        m.color = {1.0f, 1.0f, 1.0f};
        m.roughness = 0.0f;
        m.transmission = 1.0f;
        m.reflectivity = 0.25f;
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
    s.fluid.reset();
    s.emitter.reset();
    s.instance_of.clear();
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

Fluid* Scene::fluid(Entity e) {
    Slot* s = slot(e);
    return s && s->fluid ? &*s->fluid : nullptr;
}

const Fluid* Scene::fluid(Entity e) const {
    const Slot* s = slot(e);
    return s && s->fluid ? &*s->fluid : nullptr;
}

Fluid& Scene::add_fluid(Entity e) {
    static Fluid none;
    Slot* s = slot(e);
    if (!s) {
        none = Fluid{};
        return none;
    }
    if (!s->fluid) s->fluid = Fluid{};
    return *s->fluid;
}

bool Scene::remove_fluid(Entity e) {
    Slot* s = slot(e);
    if (!s || !s->fluid) return false;
    s->fluid.reset();
    return true;
}

Emitter* Scene::emitter(Entity e) {
    Slot* s = slot(e);
    return s && s->emitter ? &*s->emitter : nullptr;
}

const Emitter* Scene::emitter(Entity e) const {
    const Slot* s = slot(e);
    return s && s->emitter ? &*s->emitter : nullptr;
}

Emitter& Scene::add_emitter(Entity e) {
    static Emitter none;
    Slot* s = slot(e);
    if (!s) {
        none = Emitter{};
        return none;
    }
    if (!s->emitter) s->emitter = Emitter{};
    return *s->emitter;
}

bool Scene::remove_emitter(Entity e) {
    Slot* s = slot(e);
    if (!s || !s->emitter) return false;
    s->emitter.reset();
    return true;
}

const std::string& Scene::instance_of(Entity e) const {
    const Slot* s = slot(e);
    return s ? s->instance_of : empty_name;
}

bool Scene::set_instance_of(Entity e, const std::string& template_path) {
    Slot* s = slot(e);
    if (!s) return false;
    s->instance_of = template_path;
    return true;
}

std::vector<Entity> Scene::instances_of(const std::string& template_path) const {
    std::vector<Entity> out;
    if (template_path.empty()) return out;
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        if (s.used && s.instance_of == template_path) out.push_back({i, s.generation});
    }
    return out;
}

std::vector<Entity> Scene::subtree(Entity e) const {
    std::vector<Entity> out;
    if (!alive(e)) return out;
    out.push_back(e);
    for (std::size_t i = 0; i < out.size() && out.size() < 1000000; ++i)
        for (Entity c : children(out[i])) out.push_back(c);
    return out;
}

Entity Scene::copy_from(const Scene& other, Entity source, Entity parent) {
    const Slot* src = other.slot(source);
    if (!src) return Entity{};
    const Slot copy = *src;
    const Entity e = create(copy.name, parent);
    Slot* dst = slot(e);
    dst->transform = copy.transform;
    dst->body = copy.body;
    dst->colliders = copy.colliders;
    dst->script = copy.script;
    dst->light = copy.light;
    dst->material = copy.material;
    dst->fluid = copy.fluid;
    dst->emitter = copy.emitter;
    dst->instance_of = copy.instance_of;
    if (&other == this) {
        const Scene part = other.extract(source, true);
        const std::vector<Entity> part_roots = part.roots();
        if (!part_roots.empty())
            for (Entity c : part.children(part_roots.front())) copy_from(part, c, e);
        return e;
    }
    for (Entity c : other.children(source)) copy_from(other, c, e);
    return e;
}

Entity Scene::duplicate(Entity e) {
    if (!alive(e)) return Entity{};
    const Scene part = extract(e, true);
    const std::vector<Entity> part_roots = part.roots();
    if (part_roots.empty()) return Entity{};
    const Entity copy = copy_from(part, part_roots.front(), parent(e));
    set_name(copy, name(e) + " copy");
    return copy;
}

Scene Scene::extract(Entity root, bool keep_position) const {
    Scene out(m_dimension);
    if (!alive(root)) return out;
    const Entity r = out.copy_from(*this, root);
    if (!keep_position) {
        out.transform(r)->position = Vec3{0.0f, 0.0f, 0.0f};
        out.set_instance_of(r, std::string());
    }
    return out;
}

Entity Scene::instantiate(const Scene& entity_template, const std::string& template_path, Entity parent, const Transform& at) {
    const std::vector<Entity> roots = entity_template.roots();
    if (roots.empty()) return Entity{};
    Entity root;
    if (roots.size() == 1) {
        root = copy_from(entity_template, roots.front(), parent);
        Transform* t = transform(root);
        const Transform base = *t;
        t->position = at.position + rotate(at.rotation, base.position * at.scale);
        t->rotation = at.rotation * base.rotation;
        t->scale = base.scale * at.scale;
    } else {
        std::string label = template_path;
        const std::size_t slash = label.find_last_of("/\\");
        if (slash != std::string::npos) label = label.substr(slash + 1);
        const std::size_t dot = label.find_last_of('.');
        if (dot != std::string::npos && dot > 0) label = label.substr(0, dot);
        root = create(label.empty() ? std::string("Entity") : label, parent);
        *transform(root) = at;
        for (Entity r : roots) copy_from(entity_template, r, root);
    }
    set_instance_of(root, template_path);
    return root;
}

std::size_t Scene::refresh_instances(const std::string& template_path, const Scene& entity_template) {
    std::size_t n = 0;
    const std::vector<Entity> roots = entity_template.roots();
    for (Entity inst : instances_of(template_path)) {
        if (!alive(inst)) continue;
        bool nested = false;
        for (Entity up = parent(inst); alive(up); up = parent(up)) nested = nested || instance_of(up) == template_path;
        if (nested) continue;
        const Transform keep = *transform(inst);
        const std::string keep_name = name(inst);
        for (Entity c : children(inst)) destroy(c);
        Slot* dst = slot(inst);
        if (roots.size() == 1) {
            const Slot src = *entity_template.slot(roots.front());
            dst->body = src.body;
            dst->colliders = src.colliders;
            dst->script = src.script;
            dst->light = src.light;
            dst->material = src.material;
            dst->fluid = src.fluid;
            dst->emitter = src.emitter;
            for (Entity c : entity_template.children(roots.front())) copy_from(entity_template, c, inst);
        } else {
            dst->body.reset();
            dst->colliders.clear();
            dst->script.clear();
            dst->light.reset();
            dst->material.reset();
            dst->fluid.reset();
            dst->emitter.reset();
            for (Entity r : roots) copy_from(entity_template, r, inst);
        }
        *transform(inst) = keep;
        set_name(inst, keep_name);
        dst->instance_of = template_path;
        ++n;
    }
    return n;
}

const char* to_string(EmitterShape v) {
    switch (v) {
    case EmitterShape::point: return "point";
    case EmitterShape::sphere: return "sphere";
    case EmitterShape::hemisphere: return "hemisphere";
    case EmitterShape::cone: return "cone";
    case EmitterShape::box: return "box";
    case EmitterShape::circle: return "circle";
    case EmitterShape::line: return "line";
    }
    return "point";
}

std::optional<EmitterShape> parse_emitter_shape(const std::string& text) {
    const std::string s = lower_dash(text);
    for (EmitterShape v : emitter_shapes())
        if (s == to_string(v)) return v;
    if (s == "ball") return EmitterShape::sphere;
    if (s == "dome") return EmitterShape::hemisphere;
    if (s == "rect" || s == "cube") return EmitterShape::box;
    if (s == "ring" || s == "disc" || s == "disk") return EmitterShape::circle;
    return std::nullopt;
}

const std::vector<EmitterShape>& emitter_shapes() {
    static const std::vector<EmitterShape> all = {EmitterShape::point, EmitterShape::sphere, EmitterShape::hemisphere, EmitterShape::cone, EmitterShape::box, EmitterShape::circle, EmitterShape::line};
    return all;
}

const char* to_string(ParticleBlend v) {
    return v == ParticleBlend::alpha ? "alpha" : "additive";
}

std::optional<ParticleBlend> parse_particle_blend(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "additive" || s == "add" || s == "glow") return ParticleBlend::additive;
    if (s == "alpha" || s == "normal" || s == "blend") return ParticleBlend::alpha;
    return std::nullopt;
}

const std::vector<std::string>& Emitter::presets() {
    static const std::vector<std::string> all = {"sparks", "smoke", "fire", "explosion", "dust", "rain", "snow", "magic", "debris", "bubbles", "steam", "embers"};
    return all;
}

std::optional<Emitter> Emitter::named(const std::string& preset) {
    const std::string s = lower_dash(preset);
    Emitter e;
    e.preset = s;
    if (s == "sparks") {
        e.collide = true;
        return e;
    }
    if (s == "smoke") {
        e.rate = 14.0f; e.lifetime = 3.5f; e.lifetime_spread = 1.0f; e.speed = 0.8f; e.speed_spread = 0.3f; e.spread = 18.0f;
        e.gravity = -0.08f; e.drag = 0.6f; e.turbulence = 0.6f; e.size = 0.35f; e.size_end = 1.6f;
        e.color = {0.35f, 0.35f, 0.37f}; e.color_end = {0.6f, 0.6f, 0.62f}; e.alpha = 0.55f; e.alpha_end = 0.0f;
        e.glow = 0.0f; e.stretch = 0.0f; e.blend = ParticleBlend::alpha; e.lit = true; e.shape = EmitterShape::circle; e.shape_size = {0.25f, 0.25f, 0.25f};
        e.max_particles = 400;
        return e;
    }
    if (s == "fire") {
        e.rate = 60.0f; e.lifetime = 0.9f; e.lifetime_spread = 0.3f; e.speed = 1.4f; e.speed_spread = 0.5f; e.spread = 12.0f;
        e.gravity = -0.35f; e.drag = 0.8f; e.turbulence = 0.9f; e.size = 0.3f; e.size_end = 0.05f;
        e.color = {1.0f, 0.75f, 0.25f}; e.color_end = {0.9f, 0.15f, 0.02f}; e.alpha = 0.9f; e.glow = 3.0f; e.stretch = 0.1f;
        e.shape = EmitterShape::circle; e.shape_size = {0.3f, 0.3f, 0.3f}; e.on_death = "smoke"; e.max_particles = 600;
        return e;
    }
    if (s == "explosion") {
        e.loop = false; e.rate = 0.0f; e.burst = 160; e.duration = 0.1f; e.lifetime = 1.1f; e.lifetime_spread = 0.5f;
        e.speed = 9.0f; e.speed_spread = 5.0f; e.spread = 180.0f; e.shape = EmitterShape::sphere; e.shape_size = {0.3f, 0.3f, 0.3f};
        e.gravity = 0.6f; e.drag = 1.6f; e.size = 0.25f; e.size_end = 0.6f; e.color = {1.0f, 0.85f, 0.45f}; e.color_end = {0.6f, 0.15f, 0.05f};
        e.glow = 4.0f; e.stretch = 0.2f; e.on_death = "smoke"; e.collide = true; e.max_particles = 400; e.priority = 3;
        return e;
    }
    if (s == "dust") {
        e.rate = 10.0f; e.lifetime = 4.0f; e.lifetime_spread = 1.5f; e.speed = 0.2f; e.speed_spread = 0.15f; e.spread = 180.0f;
        e.gravity = 0.02f; e.drag = 1.0f; e.turbulence = 0.3f; e.size = 0.04f; e.size_end = 0.04f;
        e.color = {0.75f, 0.68f, 0.55f}; e.color_end = e.color; e.alpha = 0.5f; e.glow = 0.0f; e.stretch = 0.0f;
        e.blend = ParticleBlend::alpha; e.lit = true; e.shape = EmitterShape::box; e.shape_size = {3.0f, 1.0f, 3.0f}; e.max_particles = 300;
        return e;
    }
    if (s == "rain") {
        e.rate = 600.0f; e.lifetime = 1.4f; e.lifetime_spread = 0.2f; e.speed = 12.0f; e.speed_spread = 2.0f; e.direction = {0.0f, -1.0f, 0.0f}; e.spread = 3.0f;
        e.gravity = 1.0f; e.drag = 0.1f; e.size = 0.015f; e.size_end = 0.015f; e.color = {0.7f, 0.8f, 0.95f}; e.color_end = e.color;
        e.alpha = 0.5f; e.alpha_end = 0.5f; e.glow = 0.0f; e.stretch = 1.5f; e.blend = ParticleBlend::alpha; e.shape = EmitterShape::box;
        e.shape_size = {20.0f, 0.1f, 20.0f}; e.collide = true; e.die_on_hit = true; e.on_death = ""; e.max_particles = 4000; e.priority = 0;
        return e;
    }
    if (s == "snow") {
        e.rate = 120.0f; e.lifetime = 8.0f; e.lifetime_spread = 2.0f; e.speed = 0.6f; e.speed_spread = 0.2f; e.direction = {0.0f, -1.0f, 0.0f}; e.spread = 20.0f;
        e.gravity = 0.05f; e.drag = 1.2f; e.turbulence = 0.5f; e.size = 0.04f; e.size_end = 0.04f; e.color = {1.0f, 1.0f, 1.0f}; e.color_end = e.color;
        e.alpha = 0.9f; e.alpha_end = 0.9f; e.glow = 0.0f; e.stretch = 0.0f; e.blend = ParticleBlend::alpha; e.lit = true;
        e.shape = EmitterShape::box; e.shape_size = {20.0f, 0.1f, 20.0f}; e.collide = true; e.max_particles = 3000; e.priority = 0;
        return e;
    }
    if (s == "magic") {
        e.rate = 50.0f; e.lifetime = 1.4f; e.speed = 0.6f; e.speed_spread = 0.4f; e.spread = 180.0f; e.gravity = -0.1f; e.drag = 0.5f; e.turbulence = 1.2f;
        e.size = 0.05f; e.size_end = 0.0f; e.color = {0.6f, 0.4f, 1.0f}; e.color_end = {0.2f, 0.9f, 1.0f}; e.glow = 3.0f; e.stretch = 0.0f;
        e.shape = EmitterShape::sphere; e.shape_size = {0.5f, 0.5f, 0.5f}; e.max_particles = 500;
        return e;
    }
    if (s == "debris") {
        e.loop = false; e.rate = 0.0f; e.burst = 40; e.duration = 0.1f; e.lifetime = 3.0f; e.lifetime_spread = 1.0f; e.speed = 5.0f; e.speed_spread = 2.0f; e.spread = 60.0f;
        e.gravity = 1.0f; e.drag = 0.1f; e.size = 0.08f; e.size_end = 0.08f; e.color = {0.45f, 0.38f, 0.3f}; e.color_end = e.color; e.alpha_end = 1.0f;
        e.glow = 0.0f; e.stretch = 0.0f; e.blend = ParticleBlend::alpha; e.lit = true; e.collide = true; e.bounce = 0.35f; e.friction = 0.6f; e.max_particles = 200;
        return e;
    }
    if (s == "bubbles") {
        e.rate = 12.0f; e.lifetime = 2.5f; e.speed = 0.5f; e.speed_spread = 0.2f; e.spread = 15.0f; e.gravity = -0.25f; e.drag = 1.5f; e.turbulence = 0.4f;
        e.size = 0.04f; e.size_end = 0.07f; e.color = {0.8f, 0.92f, 1.0f}; e.color_end = e.color; e.alpha = 0.6f; e.alpha_end = 0.3f; e.glow = 0.5f; e.stretch = 0.0f;
        e.blend = ParticleBlend::alpha; e.max_particles = 200;
        return e;
    }
    if (s == "steam") {
        Emitter m = *named("smoke");
        m.preset = s; m.color = {0.92f, 0.94f, 0.96f}; m.color_end = {1.0f, 1.0f, 1.0f}; m.alpha = 0.35f; m.lifetime = 2.0f; m.speed = 1.5f; m.gravity = -0.25f;
        return m;
    }
    if (s == "embers") {
        e.rate = 8.0f; e.lifetime = 2.5f; e.lifetime_spread = 1.0f; e.speed = 1.2f; e.speed_spread = 0.6f; e.spread = 25.0f; e.gravity = -0.15f; e.drag = 0.4f; e.turbulence = 1.0f;
        e.size = 0.025f; e.size_end = 0.01f; e.color = {1.0f, 0.6f, 0.2f}; e.color_end = {0.8f, 0.15f, 0.0f}; e.glow = 3.0f; e.stretch = 0.2f; e.shape = EmitterShape::circle;
        e.shape_size = {0.3f, 0.3f, 0.3f}; e.max_particles = 200;
        return e;
    }
    return std::nullopt;
}

const char* to_string(FluidLevel v) {
    switch (v) {
    case FluidLevel::project_default: return "default";
    case FluidLevel::blocky: return "blocky";
    case FluidLevel::still: return "still";
    case FluidLevel::flowing: return "flowing";
    case FluidLevel::waves: return "waves";
    case FluidLevel::realistic: return "realistic";
    }
    return "default";
}

const char* fluid_level_label(FluidLevel v) {
    switch (v) {
    case FluidLevel::project_default: return "Project default";
    case FluidLevel::blocky: return "Blocky (Minecraft)";
    case FluidLevel::still: return "Still (Garry's Mod, no flow)";
    case FluidLevel::flowing: return "Flowing (modded Minecraft)";
    case FluidLevel::waves: return "Waves (flows and ripples, no splashing)";
    case FluidLevel::realistic: return "Realistic (pours over walls, splashes)";
    }
    return "Project default";
}

std::optional<FluidLevel> parse_fluid_level(const std::string& text) {
    const std::string s = lower_dash(text);
    if (s == "default" || s == "project" || s == "project-default") return FluidLevel::project_default;
    if (s == "blocky" || s == "minecraft" || s == "cells") return FluidLevel::blocky;
    if (s == "still" || s == "static" || s == "gmod") return FluidLevel::still;
    if (s == "flowing" || s == "modded" || s == "flow") return FluidLevel::flowing;
    if (s == "waves" || s == "wave" || s == "somewhat-realistic" || s == "heightfield") return FluidLevel::waves;
    if (s == "realistic" || s == "super-realistic" || s == "particles" || s == "sph") return FluidLevel::realistic;
    return std::nullopt;
}

const std::vector<FluidLevel>& fluid_levels() {
    static const std::vector<FluidLevel> all = {FluidLevel::project_default, FluidLevel::blocky, FluidLevel::still, FluidLevel::flowing, FluidLevel::waves, FluidLevel::realistic};
    return all;
}

const std::vector<std::string>& Fluid::presets() {
    static const std::vector<std::string> all = {"water", "sea-water", "oil", "honey", "lava", "mud", "mercury", "sand", "acid"};
    return all;
}

std::optional<Fluid> Fluid::named(const std::string& preset) {
    const std::string s = lower_dash(preset);
    Fluid f;
    f.preset = s;
    if (s == "water") return f;
    if (s == "sea-water") {
        f.density = 1025.0f;
        f.color = {0.08f, 0.3f, 0.45f};
        return f;
    }
    if (s == "oil") {
        f.density = 900.0f;
        f.viscosity = 0.1f;
        f.surface_tension = 0.032f;
        f.color = {0.25f, 0.18f, 0.05f};
        f.opacity = 0.9f;
        return f;
    }
    if (s == "honey") {
        f.density = 1420.0f;
        f.viscosity = 10.0f;
        f.surface_tension = 0.05f;
        f.color = {0.85f, 0.55f, 0.1f};
        f.opacity = 0.85f;
        f.drag = 4.0f;
        f.quadratic_drag = 2.0f;
        return f;
    }
    if (s == "lava") {
        f.density = 2600.0f;
        f.viscosity = 100.0f;
        f.surface_tension = 0.4f;
        f.color = {1.0f, 0.3f, 0.02f};
        f.opacity = 1.0f;
        f.drag = 8.0f;
        f.quadratic_drag = 3.0f;
        return f;
    }
    if (s == "mud") {
        f.density = 1700.0f;
        f.viscosity = 5.0f;
        f.color = {0.3f, 0.2f, 0.1f};
        f.opacity = 1.0f;
        f.drag = 5.0f;
        f.quadratic_drag = 2.0f;
        f.friction = 0.6f;
        return f;
    }
    if (s == "sand") {
        f.density = 1600.0f;
        f.viscosity = 5.0f;
        f.surface_tension = 0.0f;
        f.color = {0.86f, 0.74f, 0.45f};
        f.opacity = 1.0f;
        f.friction = 0.6f;
        f.drag = 4.0f;
        return f;
    }
    if (s == "acid") {
        f.density = 1200.0f;
        f.viscosity = 0.002f;
        f.color = {0.45f, 0.95f, 0.2f};
        f.opacity = 0.8f;
        return f;
    }
    if (s == "mercury") {
        f.density = 13534.0f;
        f.viscosity = 0.0015f;
        f.surface_tension = 0.485f;
        f.color = {0.8f, 0.8f, 0.82f};
        f.opacity = 1.0f;
        f.restitution = 0.3f;
        return f;
    }
    return std::nullopt;
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
    if (l.spread != d.spread) out += std::string(" spread:") + to_string(l.spread);
    f("spill", l.spill, d.spill);
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

std::string fluid_line(std::size_t n, const Fluid& v) {
    const Fluid d;
    std::string out = "fluid " + std::to_string(n) + " " + to_string(v.level);
    auto f = [&out](const char* key, float x, float def) {
        if (x != def) out += std::string(" ") + key + ":" + number(x);
    };
    if (!v.preset.empty()) out += " preset:" + escape_token(v.preset);
    out += " size:" + vec_token(v.size);
    f("fill", v.fill, d.fill);
    f("density", v.density, d.density);
    f("viscosity", v.viscosity, d.viscosity);
    f("surface-tension", v.surface_tension, d.surface_tension);
    f("drag", v.drag, d.drag);
    f("quadratic-drag", v.quadratic_drag, d.quadratic_drag);
    f("angular-drag", v.angular_drag, d.angular_drag);
    f("friction", v.friction, d.friction);
    f("bounce", v.restitution, d.restitution);
    if (!same(v.color, d.color)) out += " color:" + vec_token(v.color);
    f("opacity", v.opacity, d.opacity);
    if (!same(v.current, d.current)) out += " current:" + vec_token(v.current);
    f("wave-height", v.wave_height, d.wave_height);
    f("resolution", v.resolution, d.resolution);
    if (v.infinite) out += " infinite:1";
    if (!v.enabled) out += " enabled:0";
    return out + "\n";
}

std::string emitter_line(std::size_t n, const Emitter& v) {
    const Emitter d;
    std::string out = "emitter " + std::to_string(n);
    auto f = [&out](const char* key, float x, float def) {
        if (x != def) out += std::string(" ") + key + ":" + number(x);
    };
    auto i = [&out](const char* key, int x, int def) {
        if (x != def) out += std::string(" ") + key + ":" + std::to_string(x);
    };
    auto b = [&out](const char* key, bool x, bool def) {
        if (x != def) out += std::string(" ") + key + ":" + (x ? "1" : "0");
    };
    auto v3 = [&out](const char* key, Vec3 x, Vec3 def) {
        if (!same(x, def)) out += std::string(" ") + key + ":" + vec_token(x);
    };
    auto t = [&out](const char* key, const std::string& x) {
        if (!x.empty()) out += std::string(" ") + key + ":" + escape_token(x);
    };
    t("preset", v.preset);
    b("enabled", v.enabled, d.enabled);
    b("autoplay", v.autoplay, d.autoplay);
    b("loop", v.loop, d.loop);
    f("duration", v.duration, d.duration);
    f("rate", v.rate, d.rate);
    i("burst", v.burst, d.burst);
    f("burst-every", v.burst_every, d.burst_every);
    f("life", v.lifetime, d.lifetime);
    f("life-spread", v.lifetime_spread, d.lifetime_spread);
    f("speed", v.speed, d.speed);
    f("speed-spread", v.speed_spread, d.speed_spread);
    v3("direction", v.direction, d.direction);
    f("spread", v.spread, d.spread);
    if (v.shape != d.shape) out += std::string(" shape:") + to_string(v.shape);
    v3("shape-size", v.shape_size, d.shape_size);
    f("gravity", v.gravity, d.gravity);
    f("drag", v.drag, d.drag);
    v3("wind", v.wind, d.wind);
    f("turbulence", v.turbulence, d.turbulence);
    f("size", v.size, d.size);
    f("size-end", v.size_end, d.size_end);
    v3("color", v.color, d.color);
    v3("color-end", v.color_end, d.color_end);
    f("alpha", v.alpha, d.alpha);
    f("alpha-end", v.alpha_end, d.alpha_end);
    f("glow", v.glow, d.glow);
    f("stretch", v.stretch, d.stretch);
    if (v.blend != d.blend) out += std::string(" blend:") + to_string(v.blend);
    b("lit", v.lit, d.lit);
    b("collide", v.collide, d.collide);
    f("bounce", v.bounce, d.bounce);
    f("friction", v.friction, d.friction);
    b("die-on-hit", v.die_on_hit, d.die_on_hit);
    t("texture", v.texture);
    i("frames", v.frames, d.frames);
    i("max", v.max_particles, d.max_particles);
    i("priority", v.priority, d.priority);
    b("local", v.local_space, d.local_space);
    f("inherit", v.inherit_velocity, d.inherit_velocity);
    if (!v.on_death.empty()) out += " on-death:" + escape_token(v.on_death);
    else if (v.on_death != d.on_death) out += " on-death:none";
    return out + "\n";
}

void read_emitter(std::istringstream& row, Emitter& v) {
    for (std::string token; row >> token;) {
        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = token.substr(0, colon), s = token.substr(colon + 1);
        const float x = static_cast<float>(std::atof(s.c_str()));
        const int n = std::atoi(s.c_str());
        if (key == "preset") v.preset = unescape_token(s);
        else if (key == "enabled") v.enabled = s != "0";
        else if (key == "autoplay") v.autoplay = s != "0";
        else if (key == "loop") v.loop = s != "0";
        else if (key == "duration") v.duration = std::max(0.0f, x);
        else if (key == "rate") v.rate = std::max(0.0f, x);
        else if (key == "burst") v.burst = std::max(0, n);
        else if (key == "burst-every") v.burst_every = std::max(0.0f, x);
        else if (key == "life") v.lifetime = std::max(0.01f, x);
        else if (key == "life-spread") v.lifetime_spread = std::max(0.0f, x);
        else if (key == "speed") v.speed = x;
        else if (key == "speed-spread") v.speed_spread = std::max(0.0f, x);
        else if (key == "direction") parse_vec_token(s, v.direction);
        else if (key == "spread") v.spread = std::clamp(x, 0.0f, 180.0f);
        else if (key == "shape") {
            if (const auto sh = parse_emitter_shape(s)) v.shape = *sh;
        } else if (key == "shape-size") parse_vec_token(s, v.shape_size);
        else if (key == "gravity") v.gravity = x;
        else if (key == "drag") v.drag = std::max(0.0f, x);
        else if (key == "wind") parse_vec_token(s, v.wind);
        else if (key == "turbulence") v.turbulence = std::max(0.0f, x);
        else if (key == "size") v.size = std::max(0.0f, x);
        else if (key == "size-end") v.size_end = std::max(0.0f, x);
        else if (key == "color") parse_vec_token(s, v.color);
        else if (key == "color-end") parse_vec_token(s, v.color_end);
        else if (key == "alpha") v.alpha = std::clamp(x, 0.0f, 1.0f);
        else if (key == "alpha-end") v.alpha_end = std::clamp(x, 0.0f, 1.0f);
        else if (key == "glow") v.glow = std::max(0.0f, x);
        else if (key == "stretch") v.stretch = std::max(0.0f, x);
        else if (key == "blend") {
            if (const auto bl = parse_particle_blend(s)) v.blend = *bl;
        } else if (key == "lit") v.lit = s != "0";
        else if (key == "collide") v.collide = s != "0";
        else if (key == "bounce") v.bounce = std::clamp(x, 0.0f, 1.0f);
        else if (key == "friction") v.friction = std::max(0.0f, x);
        else if (key == "die-on-hit") v.die_on_hit = s != "0";
        else if (key == "texture") v.texture = unescape_token(s);
        else if (key == "frames") v.frames = std::max(1, n);
        else if (key == "max") v.max_particles = std::clamp(n, 1, 1000000);
        else if (key == "priority") v.priority = std::clamp(n, 0, 3);
        else if (key == "local") v.local_space = s != "0";
        else if (key == "inherit") v.inherit_velocity = x;
        else if (key == "on-death") v.on_death = s == "none" ? std::string() : unescape_token(s);
    }
}

void read_fluid(std::istringstream& row, Fluid& v) {
    std::string level;
    row >> level;
    if (const auto lv = parse_fluid_level(level)) v.level = *lv;
    for (std::string token; row >> token;) {
        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = token.substr(0, colon), s = token.substr(colon + 1);
        const float x = static_cast<float>(std::atof(s.c_str()));
        if (key == "preset") v.preset = unescape_token(s);
        else if (key == "size") parse_vec_token(s, v.size);
        else if (key == "fill") v.fill = std::clamp(x, 0.0f, 1.0f);
        else if (key == "density") v.density = std::max(1.0f, x);
        else if (key == "viscosity") v.viscosity = std::max(0.0f, x);
        else if (key == "surface-tension") v.surface_tension = std::max(0.0f, x);
        else if (key == "drag") v.drag = std::max(0.0f, x);
        else if (key == "quadratic-drag") v.quadratic_drag = std::max(0.0f, x);
        else if (key == "angular-drag") v.angular_drag = std::max(0.0f, x);
        else if (key == "friction") v.friction = std::max(0.0f, x);
        else if (key == "bounce") v.restitution = std::clamp(x, 0.0f, 1.0f);
        else if (key == "color") parse_vec_token(s, v.color);
        else if (key == "opacity") v.opacity = std::clamp(x, 0.0f, 1.0f);
        else if (key == "current") parse_vec_token(s, v.current);
        else if (key == "wave-height") v.wave_height = std::max(0.0f, x);
        else if (key == "resolution") v.resolution = std::max(0.0f, x);
        else if (key == "infinite") v.infinite = s != "0";
        else if (key == "enabled") v.enabled = s != "0";
    }
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
    f("reflectivity", m.reflectivity, d.reflectivity);
    auto t = [&out](const char* key, const std::string& v) {
        if (!v.empty()) out += std::string(" ") + key + ":" + escape_token(v);
    };
    t("base-map", m.base_map);
    t("normal-map", m.normal_map);
    t("orm-map", m.orm_map);
    t("roughness-map", m.roughness_map);
    t("metallic-map", m.metallic_map);
    t("emission-map", m.emission_map);
    t("opacity-map", m.opacity_map);
    f("normal-strength", m.normal_strength, d.normal_strength);
    f("alpha-cutoff", m.alpha_cutoff, d.alpha_cutoff);
    f("tiling-u", m.tiling_u, d.tiling_u);
    f("tiling-v", m.tiling_v, d.tiling_v);
    f("offset-u", m.offset_u, d.offset_u);
    f("offset-v", m.offset_v, d.offset_v);
    f("uv-rotation", m.uv_rotation, d.uv_rotation);
    b("pixelated", m.pixelated, d.pixelated);
    b("triplanar", m.triplanar, d.triplanar);
    f("triplanar-scale", m.triplanar_scale, d.triplanar_scale);
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
        else if (key == "spread") {
            if (const auto sp = parse_light_spread(v)) l.spread = *sp;
        } else if (key == "spill") l.spill = x;
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
        else if (key == "reflectivity") m.reflectivity = x;
        else if (key == "base-map") m.base_map = unescape_token(v);
        else if (key == "normal-map") m.normal_map = unescape_token(v);
        else if (key == "orm-map") m.orm_map = unescape_token(v);
        else if (key == "roughness-map") m.roughness_map = unescape_token(v);
        else if (key == "metallic-map") m.metallic_map = unescape_token(v);
        else if (key == "emission-map") m.emission_map = unescape_token(v);
        else if (key == "opacity-map") m.opacity_map = unescape_token(v);
        else if (key == "normal-strength") m.normal_strength = x;
        else if (key == "alpha-cutoff") m.alpha_cutoff = x;
        else if (key == "tiling-u") m.tiling_u = x;
        else if (key == "tiling-v") m.tiling_v = x;
        else if (key == "offset-u") m.offset_u = x;
        else if (key == "offset-v") m.offset_v = x;
        else if (key == "uv-rotation") m.uv_rotation = x;
        else if (key == "pixelated") m.pixelated = v != "0";
        else if (key == "triplanar") m.triplanar = v != "0";
        else if (key == "triplanar-scale") m.triplanar_scale = x;
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
            if (c.shape == ColliderShape::sphere_section) out << " arc:" << number(c.polar_min) << "," << number(c.polar_max) << "," << number(c.azimuth_min) << "," << number(c.azimuth_max);
            if (c.shell != base.shell) out << " shell:" << number(c.shell);
            out << "\n";
        }
        if (s.light) out << light_line(n, *s.light);
        if (s.material) out << material_line(n, *s.material);
        if (s.fluid) out << fluid_line(n, *s.fluid);
        if (s.emitter) out << emitter_line(n, *s.emitter);
        if (!s.instance_of.empty()) out << "instance " << n << " " << s.instance_of << "\n";
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
        if (kind == "script" || kind == "body" || kind == "collider" || kind == "light" || kind == "material" || kind == "fluid" || kind == "emitter" || kind == "instance") {
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
            if (kind == "emitter") {
                Emitter v;
                read_emitter(row, v);
                scene.add_emitter(owner->second) = v;
                continue;
            }
            if (kind == "instance") {
                std::string path;
                std::getline(row >> std::ws, path);
                scene.set_instance_of(owner->second, path);
                continue;
            }
            if (kind == "fluid") {
                Fluid v;
                read_fluid(row, v);
                scene.add_fluid(owner->second) = v;
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
                    if (key == "arc") {
                        float a[4] = {c.polar_min, c.polar_max, c.azimuth_min, c.azimuth_max};
                        std::size_t at = 0;
                        for (int k = 0; k < 4 && at <= val.size(); ++k) {
                            const std::size_t comma = val.find(',', at);
                            a[k] = static_cast<float>(std::atof(val.substr(at, comma == std::string::npos ? std::string::npos : comma - at).c_str()));
                            if (comma == std::string::npos) break;
                            at = comma + 1;
                        }
                        c.polar_min = a[0];
                        c.polar_max = a[1];
                        c.azimuth_min = a[2];
                        c.azimuth_max = a[3];
                        continue;
                    }
                    if (key == "shell") {
                        c.shell = std::max(0.0f, static_cast<float>(std::atof(val.c_str())));
                        continue;
                    }
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
