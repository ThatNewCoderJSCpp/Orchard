#ifndef ORCHARD_PROJECT_HPP
#define ORCHARD_PROJECT_HPP

#include "axes.hpp"
#include "scene.hpp"

#include <optional>
#include <string>
#include <vector>

namespace orchard {

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
