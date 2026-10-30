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
    capsule
};

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
