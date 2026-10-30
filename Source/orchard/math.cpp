#include <orchard/math.hpp>

#include <algorithm>

namespace orchard {

Quat operator*(Quat a, Quat b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

Quat normalize(Quat q) {
    const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l <= 0.0f) return Quat{};
    return {q.x / l, q.y / l, q.z / l, q.w / l};
}

Quat from_euler(Vec3 deg) {
    const float hx = radians(deg.x) * 0.5f;
    const float hy = radians(deg.y) * 0.5f;
    const float hz = radians(deg.z) * 0.5f;
    const Quat qx{std::sin(hx), 0.0f, 0.0f, std::cos(hx)};
    const Quat qy{0.0f, std::sin(hy), 0.0f, std::cos(hy)};
    const Quat qz{0.0f, 0.0f, std::sin(hz), std::cos(hz)};
    return normalize(qy * qx * qz);
}

Vec3 to_euler(Quat q) {
    const float sx = 2.0f * (q.w * q.x - q.y * q.z);
    const float x = std::asin(std::clamp(sx, -1.0f, 1.0f));
    const float y = std::atan2(2.0f * (q.w * q.y + q.x * q.z), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float z = std::atan2(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));
    return {degrees(x), degrees(y), degrees(z)};
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.at(row, k) * b.at(k, col);
            r.at(row, col) = s;
        }
    }
    return r;
}

Vec4 operator*(const Mat4& m, Vec4 v) {
    return {
        m.at(0, 0) * v.x + m.at(0, 1) * v.y + m.at(0, 2) * v.z + m.at(0, 3) * v.w,
        m.at(1, 0) * v.x + m.at(1, 1) * v.y + m.at(1, 2) * v.z + m.at(1, 3) * v.w,
        m.at(2, 0) * v.x + m.at(2, 1) * v.y + m.at(2, 2) * v.z + m.at(2, 3) * v.w,
        m.at(3, 0) * v.x + m.at(3, 1) * v.y + m.at(3, 2) * v.z + m.at(3, 3) * v.w};
}

Mat4 translation(Vec3 t) {
    Mat4 r;
    r.at(0, 3) = t.x;
    r.at(1, 3) = t.y;
    r.at(2, 3) = t.z;
    return r;
}

Mat4 scaling(Vec3 s) {
    Mat4 r;
    r.at(0, 0) = s.x;
    r.at(1, 1) = s.y;
    r.at(2, 2) = s.z;
    return r;
}

Mat4 rotation(Quat q) {
    Mat4 r;
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    r.at(0, 0) = 1.0f - 2.0f * (yy + zz);
    r.at(0, 1) = 2.0f * (xy - wz);
    r.at(0, 2) = 2.0f * (xz + wy);
    r.at(1, 0) = 2.0f * (xy + wz);
    r.at(1, 1) = 1.0f - 2.0f * (xx + zz);
    r.at(1, 2) = 2.0f * (yz - wx);
    r.at(2, 0) = 2.0f * (xz - wy);
    r.at(2, 1) = 2.0f * (yz + wx);
    r.at(2, 2) = 1.0f - 2.0f * (xx + yy);
    return r;
}

Mat4 trs(Vec3 t, Quat r, Vec3 s) {
    return translation(t) * rotation(r) * scaling(s);
}

Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = normalize(target - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r;
    r.at(0, 0) = s.x;
    r.at(0, 1) = s.y;
    r.at(0, 2) = s.z;
    r.at(1, 0) = u.x;
    r.at(1, 1) = u.y;
    r.at(1, 2) = u.z;
    r.at(2, 0) = -f.x;
    r.at(2, 1) = -f.y;
    r.at(2, 2) = -f.z;
    r.at(0, 3) = -dot(s, eye);
    r.at(1, 3) = -dot(u, eye);
    r.at(2, 3) = dot(f, eye);
    return r;
}

Mat4 perspective(float fov_y_degrees, float aspect, float near_plane, float far_plane) {
    const float f = 1.0f / std::tan(radians(fov_y_degrees) * 0.5f);
    Mat4 r;
    r.at(0, 0) = f / aspect;
    r.at(1, 1) = f;
    r.at(2, 2) = (far_plane + near_plane) / (near_plane - far_plane);
    r.at(2, 3) = 2.0f * far_plane * near_plane / (near_plane - far_plane);
    r.at(3, 2) = -1.0f;
    r.at(3, 3) = 0.0f;
    return r;
}

Mat4 orthographic(float left, float right, float bottom, float top, float near_plane, float far_plane) {
    Mat4 r;
    r.at(0, 0) = 2.0f / (right - left);
    r.at(1, 1) = 2.0f / (top - bottom);
    r.at(2, 2) = -2.0f / (far_plane - near_plane);
    r.at(0, 3) = -(right + left) / (right - left);
    r.at(1, 3) = -(top + bottom) / (top - bottom);
    r.at(2, 3) = -(far_plane + near_plane) / (far_plane - near_plane);
    return r;
}

} // namespace orchard
