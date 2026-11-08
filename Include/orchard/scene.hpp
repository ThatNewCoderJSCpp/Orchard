#ifndef ORCHARD_SCENE_HPP
#define ORCHARD_SCENE_HPP

#include "math.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace orchard {

enum class Dimension {
    two_d,
    three_d
};

const char* to_string(Dimension d);
std::optional<Dimension> parse_dimension(const std::string& text);

struct Entity {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    bool valid() const { return generation != 0; }
    std::uint64_t id() const { return (static_cast<std::uint64_t>(generation) << 32) | index; }
    static Entity from_id(std::uint64_t id) { return {static_cast<std::uint32_t>(id & 0xffffffffu), static_cast<std::uint32_t>(id >> 32)}; }

    friend bool operator==(Entity a, Entity b) { return a.index == b.index && a.generation == b.generation; }
    friend bool operator!=(Entity a, Entity b) { return !(a == b); }
};

struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

enum class BodyType {
    fixed,
    dynamic,
    kinematic
};

enum class ColliderShape {
    box,
    sphere,
    capsule,
    cylinder,
    cone,
    rounded_box
};

enum class BodyQuality {
    project_default,
    arcade,
    normal,
    high,
    ultra
};

enum class BodyContinuous {
    project_default,
    on,
    off
};

const char* to_string(BodyQuality q);
std::optional<BodyQuality> parse_body_quality(const std::string& text);
const char* to_string(BodyContinuous c);
std::optional<BodyContinuous> parse_body_continuous(const std::string& text);
const std::vector<ColliderShape>& collider_shapes();
bool collider_shape_3d_only(ColliderShape shape);

enum BodyFlag : std::uint32_t {
    body_gravity = 1u << 0,
    body_collisions = 1u << 1,
    body_friction = 1u << 2,
    body_bounce = 1u << 3,
    body_sleep = 1u << 4,
    body_continuous = 1u << 5,
    body_contact_events = 1u << 6,
    body_lock_rotation_x = 1u << 7,
    body_lock_rotation_y = 1u << 8,
    body_lock_rotation_z = 1u << 9,
    body_lock_position_x = 1u << 10,
    body_lock_position_y = 1u << 11,
    body_lock_position_z = 1u << 12,
    body_start_awake = 1u << 13,
    body_default_flags = body_gravity | body_collisions | body_friction | body_bounce | body_sleep | body_start_awake | body_contact_events
};

struct BodyFlagInfo {
    BodyFlag flag;
    const char* name;
    const char* label;
    bool two_d;
    bool three_d;
};

const std::vector<BodyFlagInfo>& body_flags();
const char* to_string(BodyType type);
std::optional<BodyType> parse_body_type(const std::string& text);
const char* to_string(ColliderShape shape);
std::optional<ColliderShape> parse_collider_shape(const std::string& text);

enum class CombineMode {
    project_default,
    average,
    geometric_mean,
    harmonic_mean,
    quadratic_mean,
    minimum,
    multiply,
    maximum,
    sum
};

const char* to_string(CombineMode mode);
std::optional<CombineMode> parse_combine_mode(const std::string& text);
const char* combine_label(CombineMode mode);
const char* combine_explanation(CombineMode mode);
const std::vector<CombineMode>& combine_modes();

struct Body {
    bool enabled = true;
    BodyType type = BodyType::dynamic;
    std::uint32_t flags = body_default_flags;
    float gravity_scale = 1.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.05f;
    std::uint64_t collision_layer = 1ull;
    std::uint64_t collision_mask = ~0ull;
    BodyQuality quality = BodyQuality::project_default;
    BodyContinuous continuous = BodyContinuous::project_default;
    int velocity_iterations = 0;
    int position_iterations = 0;
    float max_speed = 0.0f;
    float max_spin = 0.0f;
    float sleep_speed = 0.01f;
    float sleep_spin = 0.035f;
    float sleep_delay = 0.5f;
    std::uint64_t flags_on = 0;
    std::uint64_t flags_off = 0;

    bool has(BodyFlag f) const { return (flags & f) != 0; }
    void set(BodyFlag f, bool on) { flags = on ? (flags | f) : (flags & ~static_cast<std::uint32_t>(f)); }
};

struct Collider {
    ColliderShape shape = ColliderShape::box;
    Vec3 size{1.0f, 1.0f, 1.0f};
    Vec3 offset;
    bool enabled = true;
    bool sensor = false;
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;
    CombineMode friction_combine = CombineMode::project_default;
    CombineMode bounce_combine = CombineMode::project_default;
    float rounding = 0.1f;
    float rolling_friction = 0.0f;
    float spinning_friction = 0.0f;
    Vec3 grip{1.0f, 1.0f, 1.0f};
};

enum class LightType {
    point,
    spot,
    directional,
    sphere,
    disk,
    rectangle,
    tube
};

enum class LightMobility {
    dynamic,
    stationary,
    baked
};

enum class LightUnit {
    unitless,
    candela,
    lumens,
    lux,
    nits,
    watts
};

enum class LightFalloff {
    physical,
    linear,
    smooth,
    constant,
    custom
};

const char* to_string(LightType v);
const char* to_string(LightMobility v);
const char* to_string(LightUnit v);
const char* to_string(LightFalloff v);
std::optional<LightType> parse_light_type(const std::string& text);
std::optional<LightMobility> parse_light_mobility(const std::string& text);
std::optional<LightUnit> parse_light_unit(const std::string& text);
std::optional<LightFalloff> parse_light_falloff(const std::string& text);
const std::vector<LightType>& light_types();
const char* light_type_label(LightType v);

enum class LightAnimationMode {
    none,
    strobe,
    pulse,
    flicker,
    fade
};

enum class LightColorMode {
    none,
    alternate,
    blend,
    rainbow
};

const char* to_string(LightAnimationMode v);
const char* to_string(LightColorMode v);
std::optional<LightAnimationMode> parse_light_animation_mode(const std::string& text);
std::optional<LightColorMode> parse_light_color_mode(const std::string& text);
const std::vector<LightAnimationMode>& light_animation_modes();
const std::vector<LightColorMode>& light_color_modes();
const char* light_animation_label(LightAnimationMode v);
const char* light_color_mode_label(LightColorMode v);

struct LightAnimationSample {
    float brightness = 1.0f;
    Vec3 color{1.0f, 1.0f, 1.0f};
    bool on = true;
    bool running = false;
};

struct LightAnimation {
    LightAnimationMode mode = LightAnimationMode::none;
    float rate = 4.0f;
    float rate_end = 4.0f;
    float on_fraction = 0.5f;
    float softness = 0.0f;
    float duration = 0.0f;
    float delay = 0.0f;
    bool loop = true;
    float low = 0.0f;
    float high = 1.0f;
    float high_end = 1.0f;
    LightColorMode color_mode = LightColorMode::none;
    Vec3 color_b{0.2f, 0.4f, 1.0f};
    float phase = 0.0f;
    std::uint32_t seed = 1u;

    bool active() const noexcept { return mode != LightAnimationMode::none || color_mode != LightColorMode::none; }
    LightAnimationSample evaluate(double seconds, const Vec3& base_color) const;

    static LightAnimation strobe(float flashes_per_second = 8.0f, float on_fraction = 0.25f);
    static LightAnimation pulse(float cycles_per_second = 1.0f, float low = 0.2f);
    static LightAnimation flicker(float speed = 12.0f, float low = 0.6f);
};

struct Light {
    bool enabled = true;
    LightType type = LightType::point;
    LightMobility mobility = LightMobility::dynamic;
    Vec3 color{1.0f, 1.0f, 1.0f};
    float temperature = 0.0f;
    float intensity = 10.0f;
    LightUnit unit = LightUnit::unitless;
    float range = 0.0f;
    LightFalloff falloff = LightFalloff::physical;
    float falloff_exponent = 2.0f;
    float radius = 0.0f;
    float length = 1.0f;
    float width = 1.0f;
    float height = 1.0f;
    bool two_sided = false;
    float inner_cone = 30.0f;
    float outer_cone = 40.0f;
    float angular_diameter = 0.53f;
    bool shadows = true;
    float shadow_strength = 1.0f;
    float shadow_bias = 0.0f;
    int shadow_samples = 0;
    float diffuse = 1.0f;
    float specular = 1.0f;
    float indirect = 1.0f;
    float volumetric = 1.0f;
    bool visible = true;
    std::uint32_t channels = 1u;
    std::string ies;
    std::string cookie;
    float cookie_size = 1.0f;
    LightAnimation animation;

    LightAnimationSample sample(double seconds) const { return animation.evaluate(seconds, color); }

    static Light sun(float lux = 3.0f);
    static Light spot(float intensity = 20.0f, float outer = 40.0f, float inner = 30.0f);
    static Light area(float width = 1.0f, float height = 1.0f, float intensity = 10.0f);
};

struct Material {
    std::string preset;
    Vec3 color{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float specular = 0.5f;
    Vec3 emission{0.0f, 0.0f, 0.0f};
    float emission_strength = 1.0f;
    float transmission = 0.0f;
    float ior = 1.5f;
    float abbe = 0.0f;
    bool thin = false;
    Vec3 absorption_color{1.0f, 1.0f, 1.0f};
    float absorption_distance = 0.0f;
    float opacity = 1.0f;
    bool double_sided = true;
    bool casts_shadows = true;
    bool is_static = false;
    bool visible = true;
    std::uint32_t light_channels = 1u;

    static std::optional<Material> named(const std::string& preset);
    static const std::vector<std::string>& presets();
};

class Scene {
public:
    explicit Scene(Dimension dimension = Dimension::three_d);

    Dimension dimension() const { return m_dimension; }
    void set_dimension(Dimension d) { m_dimension = d; }

    Entity create(const std::string& name, Entity parent = Entity{});
    bool destroy(Entity e);
    bool alive(Entity e) const;
    std::size_t size() const { return m_alive; }
    void clear();

    const std::string& name(Entity e) const;
    bool set_name(Entity e, const std::string& name);

    Transform* transform(Entity e);
    const Transform* transform(Entity e) const;
    Mat4 world_matrix(Entity e) const;

    Entity parent(Entity e) const;
    bool set_parent(Entity e, Entity parent);
    std::vector<Entity> children(Entity e) const;
    std::vector<Entity> roots() const;
    std::vector<Entity> entities() const;
    Entity find(const std::string& name) const;

    Body* body(Entity e);
    const Body* body(Entity e) const;
    Body& add_body(Entity e);
    bool remove_body(Entity e);

    std::vector<Collider>* colliders(Entity e);
    const std::vector<Collider>* colliders(Entity e) const;
    Collider* add_collider(Entity e, ColliderShape shape = ColliderShape::box);
    bool remove_collider(Entity e, std::size_t index);
    Entity body_owner(Entity e) const;

    const std::string& script(Entity e) const;
    bool set_script(Entity e, const std::string& path);

    Light* light(Entity e);
    const Light* light(Entity e) const;
    Light& add_light(Entity e, LightType type = LightType::point);
    bool remove_light(Entity e);

    Material* material(Entity e);
    const Material* material(Entity e) const;
    Material& add_material(Entity e);
    bool remove_material(Entity e);

    std::string save() const;
    static std::optional<Scene> load(const std::string& text, std::string* error = nullptr);

    bool save_file(const std::string& path) const;
    static std::optional<Scene> load_file(const std::string& path, std::string* error = nullptr);

private:
    struct Slot {
        std::uint32_t generation = 1;
        bool used = false;
        std::string name;
        Transform transform;
        Entity parent;
        std::optional<Body> body;
        std::vector<Collider> colliders;
        std::string script;
        std::optional<Light> light;
        std::optional<Material> material;
    };

    Slot* slot(Entity e);
    const Slot* slot(Entity e) const;
    bool is_ancestor(Entity maybe_ancestor, Entity e) const;

    Dimension m_dimension;
    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_free;
    std::size_t m_alive = 0;
};

} // namespace orchard

#endif // ORCHARD_SCENE_HPP
