#ifndef ORCHARD_PROJECT_HPP
#define ORCHARD_PROJECT_HPP

#include "axes.hpp"
#include "scene.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace orchard {

enum class PhysicsQuality {
    arcade,
    standard,
    high,
    realistic,
    custom
};

const char* to_string(PhysicsQuality q);
std::optional<PhysicsQuality> parse_physics_quality(const std::string& text);
const std::vector<PhysicsQuality>& physics_qualities();
const char* physics_quality_label(PhysicsQuality q);
const char* physics_quality_explanation(PhysicsQuality q);

struct PhysicsSettings {
    PhysicsQuality quality = PhysicsQuality::standard;
    int substeps = 1;
    int velocity_iterations = 10;
    int position_iterations = 3;
    bool continuous = true;
    bool continuous_all = false;
    bool sleeping = true;
    bool warm_starting = true;
    bool gyroscopic = true;
    float speculative_distance = 0.02f;
    float slop = 0.002f;
    float correction = 0.2f;
    float world_size = 500.0f;
    bool soft_solver = false;
    bool octree = false;
    int threads = 1;
    bool wide_solver = false;
    bool deterministic = false;
    bool reuse_contacts = true;
    bool spin_aware_ccd = true;
    int soft_substeps = 4;
    float contact_hertz = 30.0f;
    float contact_damping = 10.0f;
    float joint_hertz = 60.0f;
    float push_speed = 3.0f;
    float settle = 2.0f;
    FluidLevel fluid_level = FluidLevel::waves;
    int fluid_quality = 1;
    int particle_budget = 60000;
    int particle_quality = 1;

    static PhysicsSettings preset(PhysicsQuality q);
    bool operator==(const PhysicsSettings& o) const;
    bool operator!=(const PhysicsSettings& o) const { return !(*this == o); }
};

enum class LightingLevel {
    off,
    simple,
    shadowed,
    realistic,
    path_traced,
    photon
};

const char* to_string(LightingLevel q);
std::optional<LightingLevel> parse_lighting_level(const std::string& text);
const std::vector<LightingLevel>& lighting_levels();
const char* lighting_level_label(LightingLevel q);
const char* lighting_level_explanation(LightingLevel q);

struct LightingProject {
    LightingLevel quality = LightingLevel::shadowed;
    LightingLevel editor_quality = LightingLevel::shadowed;
    bool players_choose = true;
    std::vector<std::pair<std::string, std::string>> settings;

    const std::string* get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    bool erase(const std::string& key);
};

struct ProjectInfo {
    std::string name = "Untitled";
    Dimension dimension = Dimension::three_d;
    std::string engine_version;
    std::string main_scene = "Scenes/main.scene";
    double max_fps = 0.0;
    double tick_rate = 60.0;
    Axes axes = Axes::y_up;
    YAxis2D y_axis = YAxis2D::up;
    AxisFlip axis_flips = AxisFlip::none;
    CombineMode friction_combine = CombineMode::geometric_mean;
    CombineMode bounce_combine = CombineMode::maximum;
    PhysicsSettings physics;
    LightingProject lighting;

    AxisSystem axis_system() const { return AxisSystem(axes, axis_flips); }
    void set_axis_system(const AxisSystem& s) {
        axes = s.base();
        axis_flips = s.flips();
    }
};

class Project {
public:
    static constexpr const char* file_name = "project.orchard";

    static std::optional<Project> create(const std::string& folder, const std::string& name, Dimension dimension, std::string* error = nullptr);
    static std::optional<Project> create(const std::string& folder, const std::string& name, Dimension dimension, Axes axes, std::string* error = nullptr);
    static std::optional<Project> create(const std::string& folder, const std::string& name, Dimension dimension, const AxisSystem& axes, std::string* error = nullptr);
    static std::optional<Project> open(const std::string& path, std::string* error = nullptr);

    const std::string& folder() const { return m_folder; }
    std::string file() const;
    std::string path_of(const std::string& relative) const;

    ProjectInfo& info() { return m_info; }
    const ProjectInfo& info() const { return m_info; }

    bool save(std::string* error = nullptr) const;

    std::vector<std::string> scripts() const;
    static const std::vector<std::string>& script_extensions();
    std::vector<std::string> graphs() const;
    std::vector<std::string> scenes() const;
    std::vector<std::string> images() const;
    std::vector<std::string> sounds() const;
    std::vector<std::string> behaviors() const;
    std::vector<std::string> entity_templates() const;
    static std::string entity_template_path(const std::string& name);
    static const std::vector<std::string>& image_extensions();
    std::vector<std::string> models() const;
    static const std::vector<std::string>& model_extensions();

    static std::string write_info(const ProjectInfo& info);
    static std::optional<ProjectInfo> read_info(const std::string& text, std::string* error = nullptr);

private:
    std::vector<std::string> files_with(const std::string& folder, const std::string& extension) const;
    std::vector<std::string> files_with(const std::string& folder, const std::vector<std::string>& extensions) const;

    std::string m_folder;
    ProjectInfo m_info;
};

} // namespace orchard

#endif // ORCHARD_PROJECT_HPP
