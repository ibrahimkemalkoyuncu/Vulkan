#include "render/Gizmo.hpp"
#include <cmath>

namespace vkt {
namespace render {

Gizmo::Gizmo() {
    m_rotation = geom::Quaternion::Identity();
}

void Gizmo::GenerateGeometry(std::vector<GizmoVertex>& vertices, std::vector<uint32_t>& indices) const {
    vertices.clear();
    indices.clear();
    if (m_mode == GizmoMode::Translate)
        GenerateTranslateGizmo(vertices, indices);
    else if (m_mode == GizmoMode::Rotate)
        GenerateRotateGizmo(vertices, indices);
}

void Gizmo::GenerateTranslateGizmo(std::vector<GizmoVertex>& vertices, std::vector<uint32_t>& indices) const {
    geom::Vec3 cX = (m_hoveredAxis == GizmoAxis::X || m_activeAxis == GizmoAxis::X) ? geom::Vec3{1,1,0} : geom::Vec3{1,0,0};
    geom::Vec3 cY = (m_hoveredAxis == GizmoAxis::Y || m_activeAxis == GizmoAxis::Y) ? geom::Vec3{1,1,0} : geom::Vec3{0,1,0};
    geom::Vec3 cZ = (m_hoveredAxis == GizmoAxis::Z || m_activeAxis == GizmoAxis::Z) ? geom::Vec3{1,1,0} : geom::Vec3{0,0,1};

    float len = 2.0f * m_scale;

    auto addLine = [&](const geom::Vec3& dir, const geom::Vec3& color) {
        uint32_t i0 = static_cast<uint32_t>(vertices.size());
        vertices.push_back({m_position, color});
        vertices.push_back({m_position + m_rotation.Rotate(dir), color});
        indices.push_back(i0);
        indices.push_back(i0 + 1);
    };

    addLine({(double)len, 0, 0}, cX);
    addLine({0, (double)len, 0}, cY);
    addLine({0, 0, (double)len}, cZ);
}

void Gizmo::GenerateRotateGizmo(std::vector<GizmoVertex>& vertices, std::vector<uint32_t>& indices) const {
    geom::Vec3 cX = (m_hoveredAxis == GizmoAxis::X || m_activeAxis == GizmoAxis::X) ? geom::Vec3{1,1,0} : geom::Vec3{1,0,0};
    geom::Vec3 cY = (m_hoveredAxis == GizmoAxis::Y || m_activeAxis == GizmoAxis::Y) ? geom::Vec3{1,1,0} : geom::Vec3{0,1,0};
    geom::Vec3 cZ = (m_hoveredAxis == GizmoAxis::Z || m_activeAxis == GizmoAxis::Z) ? geom::Vec3{1,1,0} : geom::Vec3{0,0,1};

    float radius = 2.0f * m_scale;
    const int segments = 36;

    auto addCircle = [&](int plane, const geom::Vec3& color) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < segments; ++i) {
            float angle = static_cast<float>(i) / segments * 2.0f * 3.14159265f;
            float ca = std::cos(angle) * radius;
            float sa = std::sin(angle) * radius;
            geom::Vec3 local{};
            if      (plane == 0) { local.y = ca; local.z = sa; }
            else if (plane == 1) { local.x = ca; local.z = sa; }
            else                 { local.x = ca; local.y = sa; }
            vertices.push_back({m_position + m_rotation.Rotate(local), color});
            indices.push_back(base + i);
            indices.push_back(base + (i + 1) % segments);
        }
    };

    addCircle(0, cX);
    addCircle(1, cY);
    addCircle(2, cZ);
}

GizmoAxis Gizmo::HitTest(const geom::Ray& ray, float /*scaleFactor*/) const {
    float len    = 2.0f * m_scale;
    float radius = 0.1f * m_scale;
    double tMin  = 1e9;
    GizmoAxis hit = GizmoAxis::None;

    if (m_mode == GizmoMode::Translate) {
        auto checkAxis = [&](const geom::Vec3& dir, GizmoAxis axis) {
            geom::Vec3 end = m_position + m_rotation.Rotate(dir);
            double t;
            if (ray.IntersectsCylinder(m_position, end, radius, t) && t < tMin) {
                tMin = t; hit = axis;
            }
        };
        checkAxis({(double)len, 0, 0}, GizmoAxis::X);
        checkAxis({0, (double)len, 0}, GizmoAxis::Y);
        checkAxis({0, 0, (double)len}, GizmoAxis::Z);

    } else if (m_mode == GizmoMode::Rotate) {
        float ringR     = 2.0f * m_scale;
        float thickness = 0.2f * m_scale;

        auto checkRing = [&](const geom::Vec3& normal, GizmoAxis axis) {
            geom::Vec3 n = m_rotation.Rotate(normal);
            double t;
            if (ray.IntersectsPlane(n, m_position, t)) {
                geom::Vec3 hp = ray.PointAt(t);
                double d = hp.DistanceTo(m_position);
                if (std::abs(d - ringR) < thickness && t < tMin) {
                    tMin = t; hit = axis;
                }
            }
        };
        checkRing({1,0,0}, GizmoAxis::X);
        checkRing({0,1,0}, GizmoAxis::Y);
        checkRing({0,0,1}, GizmoAxis::Z);
    }

    return hit;
}

void Gizmo::UpdateManipulation(const geom::Ray& ray, geom::Vec3& outPos, geom::Quaternion& outRot) {
    if (m_activeAxis == GizmoAxis::None) return;
    outPos = m_position;
    outRot = m_rotation;

    if (m_mode == GizmoMode::Translate) {
        geom::Vec3 axisDir{};
        if      (m_activeAxis == GizmoAxis::X) axisDir = {1,0,0};
        else if (m_activeAxis == GizmoAxis::Y) axisDir = {0,1,0};
        else if (m_activeAxis == GizmoAxis::Z) axisDir = {0,0,1};

        geom::Vec3 a = m_rotation.Rotate(axisDir);
        geom::Vec3 w0 = ray.origin - m_position;
        double b  = geom::Vec3::Dot(ray.direction, a);
        double denom = geom::Vec3::Dot(ray.direction, ray.direction) * geom::Vec3::Dot(a, a) - b * b;
        if (std::abs(denom) > geom::EPSILON) {
            double tc = (geom::Vec3::Dot(ray.direction, ray.direction) * geom::Vec3::Dot(a, w0)
                       - b * geom::Vec3::Dot(ray.direction, w0)) / denom;
            outPos = m_position + (a * tc);
        }

    } else if (m_mode == GizmoMode::Rotate) {
        geom::Vec3 normal{};
        if      (m_activeAxis == GizmoAxis::X) normal = {1,0,0};
        else if (m_activeAxis == GizmoAxis::Y) normal = {0,1,0};
        else if (m_activeAxis == GizmoAxis::Z) normal = {0,0,1};

        geom::Vec3 n = m_rotation.Rotate(normal);
        double t;
        if (ray.IntersectsPlane(n, m_position, t)) {
            constexpr double kStep = 0.05;
            geom::Quaternion delta = geom::Quaternion::FromAxisAngle(n, kStep);
            outRot = delta * m_rotation;
        }
    }
}

} // namespace render
} // namespace vkt
