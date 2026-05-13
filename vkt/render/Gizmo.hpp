#pragma once

#include "geom/Math.hpp"
#include "geom/Quaternion.hpp"
#include <vector>
#include <cstdint>

namespace vkt {
namespace render {

enum class GizmoMode { None, Translate, Rotate, Scale };

enum class GizmoAxis { None, X, Y, Z, XY, XZ, YZ, XYZ };

struct GizmoVertex {
    geom::Vec3 position;
    geom::Vec3 color;
};

class Gizmo {
public:
    Gizmo();
    ~Gizmo() = default;

    void SetMode(GizmoMode mode) { m_mode = mode; }
    GizmoMode GetMode() const { return m_mode; }

    void SetPosition(const geom::Vec3& pos) { m_position = pos; }
    const geom::Vec3& GetPosition() const { return m_position; }

    void SetRotation(const geom::Quaternion& rot) { m_rotation = rot; }
    const geom::Quaternion& GetRotation() const { return m_rotation; }

    void SetActiveAxis(GizmoAxis axis) { m_activeAxis = axis; }
    GizmoAxis GetActiveAxis() const { return m_activeAxis; }

    void SetHoveredAxis(GizmoAxis axis) { m_hoveredAxis = axis; }
    GizmoAxis GetHoveredAxis() const { return m_hoveredAxis; }

    void SetScale(float s) { m_scale = s; }

    void GenerateGeometry(std::vector<GizmoVertex>& vertices, std::vector<uint32_t>& indices) const;
    GizmoAxis HitTest(const geom::Ray& ray, float scaleFactor = 1.0f) const;
    void UpdateManipulation(const geom::Ray& ray, geom::Vec3& outPos, geom::Quaternion& outRot);

private:
    void GenerateTranslateGizmo(std::vector<GizmoVertex>& vertices, std::vector<uint32_t>& indices) const;
    void GenerateRotateGizmo(std::vector<GizmoVertex>& vertices, std::vector<uint32_t>& indices) const;

    GizmoMode   m_mode        = GizmoMode::Translate;
    GizmoAxis   m_hoveredAxis = GizmoAxis::None;
    GizmoAxis   m_activeAxis  = GizmoAxis::None;
    geom::Vec3        m_position{0, 0, 0};
    geom::Quaternion  m_rotation;
    float             m_scale = 1.0f;
};

} // namespace render
} // namespace vkt
