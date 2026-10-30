#ifndef ORCHARD_VIEWPORT_HPP
#define ORCHARD_VIEWPORT_HPP

#include "axes.hpp"
#include "scene.hpp"

#include <cstdint>
#include <vector>

namespace orchard {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct OrbitCamera {
    Vec3 target;
    float yaw = 35.0f;
    float pitch = 25.0f;
    float distance = 10.0f;
    float fov = 60.0f;
};

struct PanCamera {
    Vec2 center;
    float pixels_per_unit = 48.0f;
};

class Viewport {
public:
    void resize(unsigned width, unsigned height);
    void set_view_size(unsigned width, unsigned height);
    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }

    void set_dimension(Dimension d) { m_dimension = d; }
    Dimension dimension() const { return m_dimension; }
    void set_axes(Axes axes) { m_system.set_base(axes); }
    Axes axes() const { return m_system.base(); }
    void set_axis_system(const AxisSystem& system) { m_system = system; }
    const AxisSystem& axis_system() const { return m_system; }
    void set_flip_x_2d(bool on) { m_flip_x_2d = on; }
    bool flip_x_2d() const { return m_flip_x_2d; }
    void set_y_axis(YAxis2D y) { m_y_axis = y; }
    YAxis2D y_axis() const { return m_y_axis; }
    Vec3 eye() const;
    Mat4 view_projection_matrix() const { return view_projection(); }

    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float steps);
    void reset_camera();

    OrbitCamera& orbit_camera() { return m_orbit; }
    PanCamera& pan_camera() { return m_pan; }

    const std::vector<std::uint8_t>& render(const Scene& scene, Entity selected);
    const std::vector<std::uint8_t>& pixels() const { return m_rgba; }

    Entity pick(const Scene& scene, float x, float y) const;
    bool project(Vec3 world, float& sx, float& sy) const;

private:
    Mat4 view_projection() const;
    void clear(Color top, Color bottom);
    void pixel(int x, int y, Color c);
    void line(float x0, float y0, float x1, float y1, Color c);
    void line3(Vec3 a, Vec3 b, Color c);
    void rect(float x, float y, float w, float h, Color c);
    void grid_2d();
    void grid_3d();
    void entity_2d(const Scene& scene, Entity e, bool selected);
    void entity_3d(const Scene& scene, Entity e, bool selected);

    unsigned m_width = 0;
    unsigned m_height = 0;
    Dimension m_dimension = Dimension::three_d;
    AxisSystem m_system;
    bool m_flip_x_2d = false;
    YAxis2D m_y_axis = YAxis2D::up;
    OrbitCamera m_orbit;
    PanCamera m_pan;
    std::vector<std::uint8_t> m_rgba;
};

} // namespace orchard

#endif // ORCHARD_VIEWPORT_HPP
