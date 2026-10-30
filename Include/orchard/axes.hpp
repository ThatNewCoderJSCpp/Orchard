#ifndef ORCHARD_AXES_HPP
#define ORCHARD_AXES_HPP

#include "math.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace orchard {

class Scene;

enum class Axes {
    y_up,
    y_up_left,
    z_up,
    z_up_left
};

enum class YAxis2D {
    up,
    down
};

const char* to_string(Axes axes);
std::optional<Axes> parse_axes(const std::string& text);
const char* to_string(YAxis2D y);
std::optional<YAxis2D> parse_y_axis(const std::string& text);

bool mirrored(Axes axes);
Vec3 to_render(Axes axes, Vec3 world);
Vec3 from_render(Axes axes, Vec3 render);
Mat4 to_render_matrix(Axes axes);
Vec3 up_of(Axes axes);
Vec3 forward_of(Axes axes);
Vec3 right_of(Axes axes);
float height_of(Axes axes, Vec3 p);
Vec3 gravity_of(Axes axes, float g = 9.80665f);
Vec3 gravity_of(YAxis2D y, float g = 9.80665f);

enum class AxisFlip : std::uint8_t {
    none = 0,
    x = 1,
    y = 2,
    z = 4
};

constexpr AxisFlip operator|(AxisFlip a, AxisFlip b) { return static_cast<AxisFlip>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)); }
constexpr AxisFlip operator&(AxisFlip a, AxisFlip b) { return static_cast<AxisFlip>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)); }
constexpr AxisFlip operator^(AxisFlip a, AxisFlip b) { return static_cast<AxisFlip>(static_cast<std::uint8_t>(a) ^ static_cast<std::uint8_t>(b)); }
constexpr bool has_flip(AxisFlip flips, AxisFlip axis) { return (flips & axis) != AxisFlip::none; }

std::string to_string(AxisFlip flips);
std::optional<AxisFlip> parse_axis_flips(const std::string& text);

class AxisSystem {
public:
    AxisSystem() = default;
    explicit AxisSystem(Axes base, AxisFlip flips = AxisFlip::none) : m_base(base), m_flips(flips) {}

    static AxisSystem standard() { return AxisSystem(); }
    static AxisSystem y_up() { return AxisSystem(Axes::y_up); }
    static AxisSystem y_up_left() { return AxisSystem(Axes::y_up_left); }
    static AxisSystem z_up() { return AxisSystem(Axes::z_up); }
    static AxisSystem z_up_left() { return AxisSystem(Axes::z_up_left); }
    static AxisSystem godot() { return y_up(); }
    static AxisSystem opengl() { return y_up(); }
    static AxisSystem unity() { return y_up_left(); }
    static AxisSystem blender() { return z_up(); }
    static AxisSystem unreal() { return z_up_left(); }
    static AxisSystem source() { return AxisSystem(Axes::z_up_left, AxisFlip::y); }

    Axes base() const { return m_base; }
    AxisFlip flips() const { return m_flips; }
    bool flipped(AxisFlip axis) const { return has_flip(m_flips, axis); }
    AxisSystem with_flips(AxisFlip extra) const { return AxisSystem(m_base, m_flips ^ extra); }
    AxisSystem& set_base(Axes base) { m_base = base; return *this; }
    AxisSystem& set_flips(AxisFlip flips) { m_flips = flips; return *this; }
    AxisSystem& flip(AxisFlip axis) { m_flips = m_flips ^ axis; return *this; }
    AxisSystem& flip_x() { return flip(AxisFlip::x); }
    AxisSystem& flip_y() { return flip(AxisFlip::y); }
    AxisSystem& flip_z() { return flip(AxisFlip::z); }

    bool mirrored() const;
    Vec3 to_render(Vec3 world) const;
    Vec3 from_render(Vec3 render) const;
    Mat4 to_render_matrix() const;
    Vec3 up() const;
    Vec3 forward() const;
    Vec3 right() const;
    float height(Vec3 p) const;
    Vec3 gravity(float g = 9.80665f) const;
    std::string name() const;
    static std::optional<AxisSystem> parse(const std::string& text);

    bool operator==(const AxisSystem& o) const;
    bool operator!=(const AxisSystem& o) const { return !(*this == o); }

private:
    Vec3 flip_world(Vec3 v) const;

    Axes m_base = Axes::y_up;
    AxisFlip m_flips = AxisFlip::none;
};

} // namespace orchard

namespace orchard {

struct AxisChange {
    float m[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    static AxisChange between(const AxisSystem& from, const AxisSystem& to);
    static AxisChange flips_2d(bool flip_x, bool flip_y);

    Vec3 apply(Vec3 v) const;
    bool identity() const;
    bool mirrors() const;
};

void remap_scene(Scene& scene, const AxisChange& change);
void remap_scene(Scene& scene, const AxisSystem& from, const AxisSystem& to);

} // namespace orchard

#endif // ORCHARD_AXES_HPP
