#ifndef ORCHARD_EDITOR_AXES_PREVIEW_HPP
#define ORCHARD_EDITOR_AXES_PREVIEW_HPP

#include <orchard/axes.hpp>
#include <orchard/scene.hpp>

#include <grove/canvas.hpp>

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>

#include <string>
#include <utility>
#include <vector>

namespace orchard {
namespace editor {

fizmo::Axes3D to_fizmo(const AxisSystem& system);

struct AxesChoice {
    Dimension dimension = Dimension::three_d;
    AxisSystem system;
    YAxis2D y_axis = YAxis2D::up;
    bool flip_x_2d = false;
};

std::vector<std::pair<std::string, std::string>> describe_axes(const AxesChoice& choice);

class AxesPreview {
public:
    void draw(grove::GpuCanvasFrame& frame, const AxesChoice& choice);
    void orbit(float dx, float dy);
    void reset();

private:
    void draw_3d(grove::GpuCanvasFrame& frame, const AxisSystem& system);
    void draw_2d(grove::GpuCanvasFrame& frame, const AxesChoice& choice);

    float m_yaw = 35.0f;
    float m_pitch = 22.0f;
};

} // namespace editor
} // namespace orchard

#endif // ORCHARD_EDITOR_AXES_PREVIEW_HPP
