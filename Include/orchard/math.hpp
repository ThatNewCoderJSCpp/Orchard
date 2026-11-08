#ifndef ORCHARD_MATH_HPP
#define ORCHARD_MATH_HPP

#include <cmath>

namespace orchard {

constexpr float pi = 3.14159265358979323846f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    const float l = length(a);
    return l > 0.0f ? a * (1.0f / l) : Vec3{};
}

inline float radians(float degrees) { return degrees * (pi / 180.0f); }
inline float degrees(float radians) { return radians * (180.0f / pi); }

Quat operator*(Quat a, Quat b);
Vec3 rotate(Quat q, Vec3 v);
Quat from_euler(Vec3 degrees);
Vec3 to_euler(Quat q);
Quat normalize(Quat q);

Mat4 operator*(const Mat4& a, const Mat4& b);
Vec4 operator*(const Mat4& m, Vec4 v);
Mat4 translation(Vec3 t);
Mat4 scaling(Vec3 s);
Mat4 rotation(Quat q);
Mat4 trs(Vec3 t, Quat r, Vec3 s);
Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up);
Mat4 perspective(float fov_y_degrees, float aspect, float near_plane, float far_plane);
Mat4 orthographic(float left, float right, float bottom, float top, float near_plane, float far_plane);
bool invert(const Mat4& m, Mat4& out);
Vec3 mul_point(const Mat4& m, Vec3 p);
Vec3 mul_direction(const Mat4& m, Vec3 d);

} // namespace orchard

#endif // ORCHARD_MATH_HPP
