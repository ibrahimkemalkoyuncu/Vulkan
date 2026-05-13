/**
 * @file Dimension.cpp
 * @brief Ölçü çizgisi entity'si implementasyonu
 */

#include "cad/Dimension.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace vkt {
namespace cad {

static constexpr double PI = 3.14159265358979323846;

Dimension::Dimension()
    : Entity(EntityType::Dimension) {}

Dimension::Dimension(const geom::Vec3& p1, const geom::Vec3& p2,
                     const geom::Vec3& dimLinePos, DimensionType type)
    : Entity(EntityType::Dimension)
    , m_p1(p1), m_p2(p2), m_dimLinePos(dimLinePos), m_dimType(type) {}

// ═══════════════════════════════════════════════════════════
//  ÖLÇÜM HESABI
// ═══════════════════════════════════════════════════════════

double Dimension::GetMeasuredValue() const {
    switch (m_dimType) {
        case DimensionType::Linear: {
            // Yatay/dikey bileşen — hangi eksen daha büyükse onu ölç
            double dx = std::abs(m_p2.x - m_p1.x);
            double dy = std::abs(m_p2.y - m_p1.y);
            return (dx >= dy) ? dx : dy;
        }
        case DimensionType::Aligned: {
            double dx = m_p2.x - m_p1.x;
            double dy = m_p2.y - m_p1.y;
            double dz = m_p2.z - m_p1.z;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        }
        case DimensionType::Radius:
        case DimensionType::Diameter: {
            // m_p1 = merkez, m_p2 = çevre noktası
            double dx = m_p2.x - m_p1.x;
            double dy = m_p2.y - m_p1.y;
            double r  = std::sqrt(dx*dx + dy*dy);
            return (m_dimType == DimensionType::Diameter) ? r * 2.0 : r;
        }
    }
    return 0.0;
}

std::string Dimension::GetDisplayText() const {
    if (!m_overrideText.empty()) return m_overrideText;

    std::ostringstream ss;
    double val = GetMeasuredValue();

    if (m_dimType == DimensionType::Radius)
        ss << "R";
    else if (m_dimType == DimensionType::Diameter)
        ss << "Ø";

    // Tam sayıya yakınsa tamsayı, değilse 2 ondalık
    if (std::abs(val - std::round(val)) < 0.005)
        ss << static_cast<int>(std::round(val));
    else
        ss << std::fixed << std::setprecision(2) << val;

    return ss.str();
}

// ═══════════════════════════════════════════════════════════
//  ENTITY INTERFACE
// ═══════════════════════════════════════════════════════════

std::unique_ptr<Entity> Dimension::Clone() const {
    return std::make_unique<Dimension>(*this);
}

BoundingBox Dimension::GetBounds() const {
    BoundingBox bb;
    bb.Extend(m_p1);
    bb.Extend(m_p2);
    bb.Extend(m_dimLinePos);
    return bb;
}

void Dimension::Serialize(nlohmann::json& j) const {
    j["type"] = "Dimension";
    j["dimType"] = static_cast<int>(m_dimType);
    j["p1"]  = {m_p1.x, m_p1.y, m_p1.z};
    j["p2"]  = {m_p2.x, m_p2.y, m_p2.z};
    j["dimLinePos"] = {m_dimLinePos.x, m_dimLinePos.y, m_dimLinePos.z};
    j["textHeight"]    = m_textHeight;
    j["arrowSize"]     = m_arrowSize;
    j["extOffset"]     = m_extOffset;
    j["overrideText"]  = m_overrideText;
    j["layer"]         = GetLayerName();
}

void Dimension::Deserialize(const nlohmann::json& j) {
    m_dimType     = static_cast<DimensionType>(j.value("dimType", 1));
    auto p1       = j["p1"];  m_p1       = {p1[0], p1[1], p1[2]};
    auto p2       = j["p2"];  m_p2       = {p2[0], p2[1], p2[2]};
    auto dlp      = j["dimLinePos"]; m_dimLinePos = {dlp[0], dlp[1], dlp[2]};
    m_textHeight  = j.value("textHeight",  2.5);
    m_arrowSize   = j.value("arrowSize",   2.5);
    m_extOffset   = j.value("extOffset",   1.0);
    m_overrideText = j.value("overrideText", "");
    SetLayerName(j.value("layer", ""));
}

// ═══════════════════════════════════════════════════════════
//  TRANSFORMASYONLAR
// ═══════════════════════════════════════════════════════════

void Dimension::Move(const geom::Vec3& delta) {
    m_p1.x += delta.x;          m_p1.y += delta.y;          m_p1.z += delta.z;
    m_p2.x += delta.x;          m_p2.y += delta.y;          m_p2.z += delta.z;
    m_dimLinePos.x += delta.x;  m_dimLinePos.y += delta.y;  m_dimLinePos.z += delta.z;
}

void Dimension::Scale(const geom::Vec3& s) {
    m_p1.x *= s.x;          m_p1.y *= s.y;
    m_p2.x *= s.x;          m_p2.y *= s.y;
    m_dimLinePos.x *= s.x;  m_dimLinePos.y *= s.y;
    m_textHeight *= (s.x + s.y) / 2.0;
    m_arrowSize  *= (s.x + s.y) / 2.0;
}

geom::Vec3 Dimension::RotatePoint2D(const geom::Vec3& pt,
                                    const geom::Vec3& center,
                                    double angleDeg) {
    double rad = angleDeg * PI / 180.0;
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);
    double dx = pt.x - center.x;
    double dy = pt.y - center.y;
    return { center.x + dx*cosA - dy*sinA,
             center.y + dx*sinA + dy*cosA,
             pt.z };
}

void Dimension::Rotate(double angleDeg) {
    geom::Vec3 center = {
        (m_p1.x + m_p2.x) / 2.0,
        (m_p1.y + m_p2.y) / 2.0,
        (m_p1.z + m_p2.z) / 2.0
    };
    m_p1        = RotatePoint2D(m_p1,       center, angleDeg);
    m_p2        = RotatePoint2D(m_p2,       center, angleDeg);
    m_dimLinePos = RotatePoint2D(m_dimLinePos, center, angleDeg);
}

void Dimension::Mirror(const geom::Vec3& lineP1, const geom::Vec3& lineP2) {
    auto mirrorPt = [&](const geom::Vec3& pt) -> geom::Vec3 {
        double dx = lineP2.x - lineP1.x;
        double dy = lineP2.y - lineP1.y;
        double len2 = dx*dx + dy*dy;
        if (len2 < 1e-12) return pt;
        double t = ((pt.x - lineP1.x)*dx + (pt.y - lineP1.y)*dy) / len2;
        double fx = lineP1.x + t*dx;
        double fy = lineP1.y + t*dy;
        return { 2.0*fx - pt.x, 2.0*fy - pt.y, pt.z };
    };
    m_p1         = mirrorPt(m_p1);
    m_p2         = mirrorPt(m_p2);
    m_dimLinePos = mirrorPt(m_dimLinePos);
}

} // namespace cad
} // namespace vkt
