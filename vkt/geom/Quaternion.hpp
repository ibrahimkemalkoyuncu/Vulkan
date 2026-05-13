#pragma once
#include "Math.hpp"
#include <cmath>

namespace vkt {
namespace geom {

struct Quaternion {
    double x = 0.0, y = 0.0, z = 0.0, w = 1.0;

    Quaternion() = default;
    Quaternion(double x_, double y_, double z_, double w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quaternion Identity() { return {0.0, 0.0, 0.0, 1.0}; }

    static Quaternion FromAxisAngle(const Vec3& axis, double angle) {
        Vec3 a = axis.Normalize();
        double s = std::sin(angle * 0.5);
        return {a.x * s, a.y * s, a.z * s, std::cos(angle * 0.5)};
    }

    Quaternion operator*(const Quaternion& o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    Vec3 Rotate(const Vec3& v) const {
        Vec3 qv{x, y, z};
        Vec3 t = Vec3::Cross(qv, v) * 2.0;
        return v + (t * w) + Vec3::Cross(qv, t);
    }
};

} // namespace geom
} // namespace vkt
