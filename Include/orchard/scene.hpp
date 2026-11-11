#ifndef ORCHARD_SCENE_HPP
#define ORCHARD_SCENE_HPP

#include "math.hpp"
#include "terrain.hpp"

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

enum class ReflectionMode { highlights, surroundings, realistic };
const char* to_string(ReflectionMode v);
std::optional<ReflectionMode> parse_reflection_mode(const std::string& text);
const char* reflection_label(ReflectionMode v);

enum class ColliderShape {
    box,
    sphere,
    capsule,
    cylinder,
    cone,
    rounded_box,
    sphere_section,
    convex_hull,
    mesh,
    triangle
};

enum class TriangleRule {
    sss,
    sas,
    asa,
    aas,
    ssa,
    right
};

const char* to_string(TriangleRule rule);
std::optional<TriangleRule> parse_triangle_rule(const std::string& text);
const char* triangle_rule_label(TriangleRule rule);
const std::vector<TriangleRule>& triangle_rules();

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
    Vec3 start_velocity;
    Vec3 start_spin;
    Vec3 acceleration;
    bool motion_local = false;

    bool has(BodyFlag f) const { return (flags & f) != 0; }
    void set(BodyFlag f, bool on) { flags = on ? (flags | f) : (flags & ~static_cast<std::uint32_t>(f)); }
};

enum ColliderLock : std::uint32_t {
    collider_lock_mass = 1u << 0,
    collider_lock_density = 1u << 1,
    collider_lock_size_x = 1u << 2,
    collider_lock_size_y = 1u << 3,
    collider_lock_size_z = 1u << 4,
    collider_lock_size = collider_lock_size_x | collider_lock_size_y | collider_lock_size_z
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
    float rolling_friction = 0.005f;
    float spinning_friction = 0.005f;
    Vec3 grip{1.0f, 1.0f, 1.0f};
    float polar_min = 0.0f;
    float polar_max = 90.0f;
    float azimuth_min = 0.0f;
    float azimuth_max = 360.0f;
    float shell = 0.0f;
    std::string source;
    std::string name;
    float mass = 0.0f;
    std::uint32_t locks = 0;
    TriangleRule triangle = TriangleRule::sss;
    Vec3 triangle_sides{1.0f, 1.0f, 1.0f};
    Vec3 triangle_angles{60.0f, 60.0f, 60.0f};
    bool triangle_wide = false;

    bool locked(ColliderLock l) const { return (locks & l) != 0; }
    void lock(ColliderLock l, bool on) { locks = on ? (locks | l) : (locks & ~static_cast<std::uint32_t>(l)); }
};

bool solve_triangle(const Collider& c, Vec3& sides, Vec3& angles, std::string* why = nullptr);
std::vector<Vec3> triangle_corners(const Collider& c);
std::vector<Vec3> triangle_prism_points(const Collider& c, Vec3 scale = Vec3{1.0f, 1.0f, 1.0f});
bool fit_triangle_size(Collider& c, std::string* why = nullptr);
float collider_volume(const Collider& c, Vec3 scale = Vec3{1.0f, 1.0f, 1.0f}, Dimension dimension = Dimension::three_d);
float collider_density_unit(Dimension dimension);
float collider_mass(const Collider& c, Vec3 scale = Vec3{1.0f, 1.0f, 1.0f}, Dimension dimension = Dimension::three_d);

enum class MassEdit { mass, density, size_x, size_y, size_z };

bool edit_collider_mass(Collider& c, MassEdit what, float value, Vec3 scale, Dimension dimension, std::string* why = nullptr);

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

enum class LightSpread {
    natural,
    blob
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
const char* to_string(LightSpread v);
std::optional<LightType> parse_light_type(const std::string& text);
std::optional<LightMobility> parse_light_mobility(const std::string& text);
std::optional<LightUnit> parse_light_unit(const std::string& text);
std::optional<LightFalloff> parse_light_falloff(const std::string& text);
std::optional<LightSpread> parse_light_spread(const std::string& text);
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
    LightSpread spread = LightSpread::natural;
    float spill = 0.35f;
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
    float reflectivity = 0.0f;
    ReflectionMode reflections = ReflectionMode::surroundings;
    std::string base_map;
    std::string normal_map;
    std::string orm_map;
    std::string roughness_map;
    std::string metallic_map;
    std::string emission_map;
    std::string opacity_map;
    float normal_strength = 1.0f;
    float alpha_cutoff = 0.0f;
    float tiling_u = 1.0f;
    float tiling_v = 1.0f;
    float offset_u = 0.0f;
    float offset_v = 0.0f;
    float uv_rotation = 0.0f;
    bool pixelated = false;
    bool triplanar = false;
    float triplanar_scale = 1.0f;
    bool double_sided = true;
    bool casts_shadows = true;
    bool is_static = false;
    bool visible = true;
    std::uint32_t light_channels = 1u;

    bool has_textures() const;
    std::vector<std::string> texture_paths() const;

    static std::optional<Material> named(const std::string& preset);
    static const std::vector<std::string>& presets();
};

bool is_image_file(const std::string& path);

enum class FluidLevel {
    project_default,
    blocky,
    still,
    flowing,
    waves,
    realistic
};

const char* to_string(FluidLevel v);
const char* fluid_level_label(FluidLevel v);
std::optional<FluidLevel> parse_fluid_level(const std::string& text);
const std::vector<FluidLevel>& fluid_levels();

struct Fluid {
    FluidLevel level = FluidLevel::project_default;
    std::string preset = "water";
    Vec3 size{4.0f, 2.0f, 4.0f};
    float fill = 0.5f;
    float density = 1000.0f;
    float viscosity = 0.001f;
    float surface_tension = 0.072f;
    float drag = 1.0f;
    float quadratic_drag = 1.0f;
    float angular_drag = 1.0f;
    float friction = 0.1f;
    float restitution = 0.1f;
    Vec3 color{0.12f, 0.35f, 0.6f};
    float opacity = 0.7f;
    Vec3 current{0.0f, 0.0f, 0.0f};
    float wave_height = 0.0f;
    float reflectivity = 0.3f;
    ReflectionMode reflections = ReflectionMode::surroundings;
    float resolution = 0.0f;
    bool infinite = false;
    bool enabled = true;

    static std::optional<Fluid> named(const std::string& preset);
    static const std::vector<std::string>& presets();
};

enum class EmitterShape { point, sphere, hemisphere, cone, box, circle, line, fluid_surface, parts };
const char* to_string(EmitterShape v);
std::optional<EmitterShape> parse_emitter_shape(const std::string& text);
const std::vector<EmitterShape>& emitter_shapes();

enum class ParticleBlend { additive, alpha };
const char* to_string(ParticleBlend v);
std::optional<ParticleBlend> parse_particle_blend(const std::string& text);

struct Emitter {
    std::string preset = "sparks";
    bool enabled = true;
    bool autoplay = true;
    bool loop = true;
    float duration = 0.0f;
    float rate = 40.0f;
    int burst = 0;
    float burst_every = 0.0f;
    float lifetime = 0.8f;
    float lifetime_spread = 0.3f;
    float speed = 4.0f;
    float speed_spread = 2.0f;
    Vec3 direction{0.0f, 1.0f, 0.0f};
    float spread = 35.0f;
    EmitterShape shape = EmitterShape::point;
    Vec3 shape_size{0.1f, 0.1f, 0.1f};
    float gravity = 1.0f;
    float drag = 0.4f;
    Vec3 wind{0.0f, 0.0f, 0.0f};
    float turbulence = 0.0f;
    float size = 0.06f;
    float size_end = 0.02f;
    Vec3 color{1.0f, 0.85f, 0.4f};
    Vec3 color_end{1.0f, 0.3f, 0.05f};
    float alpha = 1.0f;
    float alpha_end = 0.0f;
    float glow = 2.0f;
    float stretch = 0.4f;
    ParticleBlend blend = ParticleBlend::additive;
    bool lit = false;
    bool collide = false;
    float bounce = 0.3f;
    float friction = 0.2f;
    bool die_on_hit = false;
    std::string texture;
    int frames = 1;
    int max_particles = 2000;
    int priority = 1;
    bool local_space = false;
    float inherit_velocity = 0.0f;
    std::string on_death;
    std::string part;
    std::string soft = "auto";
    float soft_distance = 0.0f;

    static std::optional<Emitter> named(const std::string& preset);
    static const std::vector<std::string>& presets();
};

enum class CameraControl { none, fly, walk };
const char* to_string(CameraControl v);
std::optional<CameraControl> parse_camera_control(const std::string& text);

struct Camera {
    CameraControl control = CameraControl::none;
    float move_speed = 5.0f;
    float look_speed = 0.25f;
    float eye_height = 1.7f;
    float jump_speed = 5.0f;
    bool mouse_look = true;
    bool active = true;
    int priority = 0;
    float fov = 60.0f;
    float near_clip = 0.05f;
    float far_clip = 5000.0f;
    float view_height = 12.0f;
};

struct Model {
    std::string source;
    bool visible = true;
    bool cast_shadows = true;
};

struct AnimationEvent {
    std::string clip;
    float time = 0.0f;
    std::string name;
};

struct Animator {
    std::string clip;
    bool playing = true;
    bool loop = true;
    float speed = 1.0f;
    float blend = 0.25f;
    float start = 0.0f;
    std::vector<AnimationEvent> events;
    std::uint32_t restart = 0;
};

bool is_model_file(const std::string& path);

enum class HumanoidKind { basic, head_and_body, full_body, detailed };
const char* to_string(HumanoidKind v);
std::optional<HumanoidKind> parse_humanoid_kind(const std::string& text);
const std::vector<HumanoidKind>& humanoid_kinds();
const char* humanoid_label(HumanoidKind v);

class Scene;
Entity add_humanoid(Scene& scene, HumanoidKind kind, Entity parent = Entity{}, const std::string& name = "Character", float height = 1.8f);

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
    Fluid* fluid(Entity e);
    const Fluid* fluid(Entity e) const;
    Fluid& add_fluid(Entity e);
    bool remove_fluid(Entity e);
    Camera* camera(Entity e);
    const Camera* camera(Entity e) const;
    Camera& add_camera(Entity e);
    bool remove_camera(Entity e);
    Entity active_camera() const;
    Emitter* emitter(Entity e);
    const Emitter* emitter(Entity e) const;
    Emitter& add_emitter(Entity e);
    bool remove_emitter(Entity e);
    Model* model(Entity e);
    const Model* model(Entity e) const;
    Model& add_model(Entity e);
    bool remove_model(Entity e);
    Animator* animator(Entity e);
    const Animator* animator(Entity e) const;
    Animator& add_animator(Entity e);
    bool remove_animator(Entity e);
    Terrain* terrain(Entity e);
    const Terrain* terrain(Entity e) const;
    Terrain& add_terrain(Entity e);
    bool remove_terrain(Entity e);

    const std::string& instance_of(Entity e) const;
    const std::string& bone(Entity e) const;
    bool set_bone(Entity e, const std::string& bone_name);
    Entity bone_owner(Entity e) const;
    bool set_instance_of(Entity e, const std::string& template_path);
    std::vector<Entity> instances_of(const std::string& template_path) const;
    std::vector<Entity> subtree(Entity e) const;
    Entity copy_from(const Scene& other, Entity source, Entity parent = Entity{});
    Entity duplicate(Entity e);
    Scene extract(Entity root, bool keep_position = false) const;
    Entity instantiate(const Scene& entity_template, const std::string& template_path, Entity parent, const Transform& at);
    std::size_t refresh_instances(const std::string& template_path, const Scene& entity_template);

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
        std::optional<Fluid> fluid;
        std::optional<Emitter> emitter;
        std::optional<Camera> camera;
        std::optional<Model> model;
        std::optional<Animator> animator;
        std::optional<Terrain> terrain;
        std::string bone;
        std::string instance_of;
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
