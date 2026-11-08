#ifndef XYLEM_XYLEM_HPP
#define XYLEM_XYLEM_HPP

#include <orchard/axes.hpp>
#include <orchard/project.hpp>
#include <orchard/scene.hpp>
#include <orchard/scripting.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace xylem {

struct FlagOption {
    std::uint64_t bit;
    const char* name;
    const char* label;
    const char* group;
    bool two_d;
};

const std::vector<FlagOption>& flag_options();
std::uint64_t base_flags(const orchard::Body& b);
std::uint64_t body_flags(const orchard::Body& b);
bool flag_on(const orchard::Body& b, std::uint64_t bit);
void set_flag(orchard::Body& b, std::uint64_t bit, bool on);

struct Contact {
    orchard::Entity a;
    orchard::Entity b;
    bool sensor = false;
};

struct FluidDraw {
    orchard::Entity entity;
    orchard::FluidLevel level = orchard::FluidLevel::still;
    orchard::Vec3 color{0.12f, 0.35f, 0.6f};
    float opacity = 0.7f;
    orchard::Vec3 origin;
    orchard::Vec3 axis_u{1.0f, 0.0f, 0.0f};
    orchard::Vec3 axis_up{0.0f, 1.0f, 0.0f};
    orchard::Vec3 axis_w{0.0f, 0.0f, 1.0f};
    orchard::Vec3 size{1.0f, 1.0f, 1.0f};
    float cell = 0.0f;
    int nx = 0;
    int ny = 0;
    int nz = 0;
    std::vector<float> values;
    std::vector<orchard::Vec3> particles;
    float radius = 0.0f;
    float level_height = 0.0f;
    bool flat = false;
    int image_width = 0;
    int image_height = 0;
    std::vector<std::uint8_t> image;
    orchard::Vec3 image_min;
    orchard::Vec3 image_max;
};

enum class FluidBrush { pour = 0, erase = 1, push = 2, explode = 3, sand = 4, wall = 5, heat = 6 };

orchard::FluidLevel fluid_level_for(const orchard::Fluid& f, orchard::FluidLevel project_level);
void preview_fluids(const orchard::Scene& scene, orchard::FluidLevel project_level, std::vector<FluidDraw>& out);

struct Settings {
    orchard::Vec3 gravity{0.0f, -9.80665f, 0.0f};
    float fixed_dt = 1.0f / 60.0f;
    int max_steps = 5;
    orchard::CombineMode friction_combine = orchard::CombineMode::geometric_mean;
    orchard::CombineMode bounce_combine = orchard::CombineMode::maximum;
    orchard::PhysicsSettings physics;

    static Settings for_project(const orchard::ProjectInfo& info) {
        Settings s = with_axes(info.dimension, info.axis_system(), info.y_axis);
        s.friction_combine = info.friction_combine;
        s.bounce_combine = info.bounce_combine;
        s.physics = info.physics;
        return s;
    }

    Settings& with_quality(orchard::PhysicsQuality q) {
        physics = orchard::PhysicsSettings::preset(q);
        return *this;
    }

    static Settings with_axes(orchard::Dimension dimension, const orchard::AxisSystem& axes, orchard::YAxis2D y_axis = orchard::YAxis2D::up) {
        Settings s;
        s.gravity = dimension == orchard::Dimension::two_d ? orchard::gravity_of(y_axis) : axes.gravity();
        return s;
    }

    static Settings with_axes(orchard::Dimension dimension, orchard::Axes axes, orchard::YAxis2D y_axis = orchard::YAxis2D::up) {
        Settings s;
        s.gravity = dimension == orchard::Dimension::two_d ? orchard::gravity_of(y_axis) : orchard::gravity_of(axes);
        return s;
    }
};

class World : public orchard::ScriptPhysics {
public:
    explicit World(orchard::Dimension dimension = orchard::Dimension::three_d, Settings settings = Settings());
    ~World() override;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void build(orchard::Scene& scene);
    void clear();
    int step(float dt);
    void step_once();
    void write_back();

    orchard::Dimension dimension() const;
    const Settings& settings() const;
    void set_gravity(orchard::Vec3 g);
    void set_physics(const orchard::PhysicsSettings& physics);
    std::size_t body_count() const;
    std::size_t collider_count() const;
    const std::vector<Contact>& contacts() const;
    bool awake(orchard::Entity e) const;

    bool has_body(orchard::Entity e) const override;
    orchard::Vec3 velocity(orchard::Entity e) const override;
    void set_velocity(orchard::Entity e, orchard::Vec3 v) override;
    orchard::Vec3 spin(orchard::Entity e) const override;
    void set_spin(orchard::Entity e, orchard::Vec3 v) override;
    void impulse(orchard::Entity e, orchard::Vec3 v) override;
    void force(orchard::Entity e, orchard::Vec3 v) override;
    void torque(orchard::Entity e, orchard::Vec3 v) override;
    void moved(orchard::Entity e) override;
    void added(orchard::Entity e) override;
    bool touching(orchard::Entity a, orchard::Entity b) const override;
    orchard::Entity raycast(orchard::Vec3 origin, orchard::Vec3 direction, float max_distance, orchard::Vec3* point, orchard::Vec3* normal, float* distance) const override;
    std::vector<orchard::Entity> overlap_sphere(orchard::Vec3 center, float radius) const override;
    void set_layers(orchard::Entity e, std::uint64_t layer, std::uint64_t mask) override;

    std::size_t fluid_count() const;
    void fluid_draws(std::vector<FluidDraw>& out) const;
    float fluid_surface(orchard::Vec3 point) const;
    float fluid_depth(orchard::Vec3 point) const;
    orchard::Vec3 fluid_velocity(orchard::Vec3 point) const;
    std::size_t fluid_brush(FluidBrush brush, orchard::Vec3 point, float radius, orchard::Vec3 velocity, int material = 0);
    std::size_t fluid_brush(int brush, orchard::Vec3 point, float radius, orchard::Vec3 velocity, int material) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

orchard::Quat world_rotation(const orchard::Scene& scene, orchard::Entity e);
orchard::Vec3 world_position(const orchard::Scene& scene, orchard::Entity e);
orchard::Vec3 world_scale(const orchard::Scene& scene, orchard::Entity e);

} // namespace xylem

#endif // XYLEM_XYLEM_HPP
