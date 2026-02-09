/**
 * @file Line.cpp
 * @brief Line entity implementasyonu
 */

#include "cad/Line.hpp"
#include "cad/Layer.hpp"
#include <cmath>
#include <algorithm>

namespace vkt {
namespace cad {

Line::Line(const geom::Vec3& start, const geom::Vec3& end, Layer* layer)
    : Entity(EntityType::Line, layer)
    , m_start(start)
    , m_end(end)
{
}

std::unique_ptr<Entity> Line::Clone() const {
    // const_cast gerekli çünkü Layer* non-const ama GetLayer() const döner
    auto clone = std::make_unique<Line>(m_start, m_end, const_cast<Layer*>(GetLayer()));
    clone->SetColor(GetColor());
    clone->SetFlags(GetFlags());
    return clone;
}

BoundingBox Line::GetBounds() const {
    BoundingBox bbox;
    bbox.Extend(m_start);
    bbox.Extend(m_end);
    return bbox;
}

void Line::Serialize(nlohmann::json& j) const {
    j["type"] = "Line";
    j["start"] = {m_start.x, m_start.y, m_start.z};
    j["end"] = {m_end.x, m_end.y, m_end.z};
    j["layer"] = GetLayer() ? GetLayer()->GetName() : "0";
}

void Line::Deserialize(const nlohmann::json& j) {
    if (j.contains("start")) {
        auto s = j["start"];
        m_start = geom::Vec3{s[0], s[1], s[2]};
    }
    if (j.contains("end")) {
        auto e = j["end"];
        m_end = geom::Vec3{e[0], e[1], e[2]};
    }
}

double Line::GetLength() const {
    return m_start.DistanceTo(m_end);
}

geom::Vec3 Line::GetDirection() const {
    geom::Vec3 dir = m_end - m_start;
    return dir.Normalize();
}

geom::Vec3 Line::GetMidPoint() const {
    return geom::Vec3{
        (m_start.x + m_end.x) * 0.5,
        (m_start.y + m_end.y) * 0.5,
        (m_start.z + m_end.z) * 0.5
    };
}

geom::Vec3 Line::GetPointAt(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    return geom::Vec3{
        m_start.x + t * (m_end.x - m_start.x),
        m_start.y + t * (m_end.y - m_start.y),
        m_start.z + t * (m_end.z - m_start.z)
    };
}

geom::Vec3 Line::GetClosestPoint(const geom::Vec3& point, double* t) const {
    geom::Vec3 dir = m_end - m_start;
    double lenSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    
    if (lenSq < 1e-12) {
        // Degenerate line (start == end)
        if (t) *t = 0.0;
        return m_start;
    }
    
    // Projection: t = dot(point - start, dir) / |dir|²
    geom::Vec3 toPoint = point - m_start;
    double tValue = (toPoint.x * dir.x + toPoint.y * dir.y + toPoint.z * dir.z) / lenSq;
    tValue = std::clamp(tValue, 0.0, 1.0);
    
    if (t) *t = tValue;
    return GetPointAt(tValue);
}

double Line::DistanceToPoint(const geom::Vec3& point) const {
    geom::Vec3 closest = GetClosestPoint(point);
    return point.DistanceTo(closest);
}

bool Line::IntersectsWith(const Line& other, geom::Vec3* intersection, double tolerance) const {
    // 2D line-line intersection (Z yoksayılır)
    
    // Bu çizgi: P = A + t * (B - A)
    // Diğer çizgi: Q = C + s * (D - C)
    
    double x1 = m_start.x, y1 = m_start.y;
    double x2 = m_end.x, y2 = m_end.y;
    double x3 = other.m_start.x, y3 = other.m_start.y;
    double x4 = other.m_end.x, y4 = other.m_end.y;
    
    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    
    // Paralel mi?
    if (std::abs(denom) < tolerance) {
        return false;
    }
    
    double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    double s = ((x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2)) / denom;
    
    // Segment içinde mi?
    if (t >= 0.0 && t <= 1.0 && s >= 0.0 && s <= 1.0) {
        if (intersection) {
            intersection->x = x1 + t * (x2 - x1);
            intersection->y = y1 + t * (y2 - y1);
            intersection->z = 0.0; // 2D
        }
        return true;
    }
    
    return false;
}

bool Line::IsParallelTo(const Line& other, double tolerance) const {
    geom::Vec3 dir1 = GetDirection();
    geom::Vec3 dir2 = other.GetDirection();
    
    // Dot product yaklaşık ±1 ise paralel
    double dot = std::abs(geom::Vec3::Dot(dir1, dir2));
    return std::abs(dot - 1.0) < tolerance;
}

bool Line::IsPerpendicularTo(const Line& other, double tolerance) const {
    geom::Vec3 dir1 = GetDirection();
    geom::Vec3 dir2 = other.GetDirection();
    
    // Dot product yaklaşık 0 ise dik
    double dot = geom::Vec3::Dot(dir1, dir2);
    return std::abs(dot) < tolerance;
}

void Line::Move(const geom::Vec3& delta) {
    m_start = m_start + delta;
    m_end = m_end + delta;
    SetFlags(GetFlags() | EF_Modified);
}

void Line::Scale(const geom::Vec3& origin, double factor) {
    m_start = origin + (m_start - origin) * factor;
    m_end = origin + (m_end - origin) * factor;
    SetFlags(GetFlags() | EF_Modified);
}

void Line::Rotate(const geom::Vec3& center, double angleDegrees) {
    double angleRad = angleDegrees * 3.14159265358979323846 / 180.0;
    double cosA = std::cos(angleRad);
    double sinA = std::sin(angleRad);
    
    // Rotate start
    double dx = m_start.x - center.x;
    double dy = m_start.y - center.y;
    m_start.x = center.x + dx * cosA - dy * sinA;
    m_start.y = center.y + dx * sinA + dy * cosA;
    
    // Rotate end
    dx = m_end.x - center.x;
    dy = m_end.y - center.y;
    m_end.x = center.x + dx * cosA - dy * sinA;
    m_end.y = center.y + dx * sinA + dy * cosA;
    
    SetFlags(GetFlags() | EF_Modified);
}

void Line::Mirror(const Line& axis) {
    // Aynalama: noktayı eksene dik yansıt
    // P' = P + 2 * ((closest - P))
    
    geom::Vec3 closestStart = axis.GetClosestPoint(m_start);
    geom::Vec3 closestEnd = axis.GetClosestPoint(m_end);
    
    m_start = m_start + (closestStart - m_start) * 2.0;
    m_end = m_end + (closestEnd - m_end) * 2.0;
    
    SetFlags(GetFlags() | EF_Modified);
}

} // namespace cad
} // namespace vkt
