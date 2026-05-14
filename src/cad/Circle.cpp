#include "cad/Circle.hpp"
#include "cad/Line.hpp"
#include "cad/Arc.hpp"
#include "cad/Layer.hpp"
#include "nlohmann/json.hpp"
#include <cmath>

namespace vkt::cad {

constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;

Circle::Circle() : Entity(), m_center(), m_radius(1.0) {}

Circle::Circle(const geom::Vec3& center, double radius)
    : Entity(), m_center(center), m_radius(radius) {}

std::unique_ptr<Entity> Circle::Clone() const {
    auto clone = std::make_unique<Circle>(m_center, m_radius);
    clone->SetLayer(const_cast<Layer*>(GetLayer()));
    clone->SetColor(GetColor());
    clone->SetVisible(IsVisible());
    clone->SetLocked(IsLocked());
    return clone;
}

BoundingBox Circle::GetBounds() const {
    return BoundingBox{
        geom::Vec3(m_center.x - m_radius, m_center.y - m_radius, m_center.z),
        geom::Vec3(m_center.x + m_radius, m_center.y + m_radius, m_center.z)
    };
}

void Circle::Serialize(nlohmann::json& j) const {
    j["type"] = "Circle";
    j["center"] = {m_center.x, m_center.y, m_center.z};
    j["radius"] = m_radius;
}

void Circle::Deserialize(const nlohmann::json& j) {
    if (j.contains("center") && j["center"].is_array() && j["center"].size() >= 2) {
        m_center.x = j["center"][0];
        m_center.y = j["center"][1];
        m_center.z = j["center"].size() > 2 ? j["center"][2].get<double>() : 0.0;
    }
    m_radius = j.value("radius", 1.0);
}

double Circle::GetCircumference() const {
    return 2.0 * PI * m_radius;
}

double Circle::GetArea() const {
    return PI * m_radius * m_radius;
}

double Circle::GetDiameter() const {
    return 2.0 * m_radius;
}

geom::Vec3 Circle::GetPointAtAngle(double angle) const {
    double rad = angle * DEG_TO_RAD;
    return geom::Vec3(
        m_center.x + m_radius * std::cos(rad),
        m_center.y + m_radius * std::sin(rad),
        m_center.z
    );
}

geom::Vec3 Circle::GetPointAt(double t) const {
    return GetPointAtAngle(t * 360.0);
}

bool Circle::ContainsPoint(const geom::Vec3& point, double tolerance) const {
    double dist = m_center.DistanceTo(point);
    return dist <= m_radius + tolerance;
}

bool Circle::ContainsPointInside(const geom::Vec3& point) const {
    return m_center.DistanceTo(point) <= m_radius;
}

geom::Vec3 Circle::GetClosestPoint(const geom::Vec3& point) const {
    geom::Vec3 dir = geom::Vec3(
        point.x - m_center.x,
        point.y - m_center.y,
        point.z - m_center.z
    );
    
    double len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-10) {
        // Point at center, return arbitrary point on circle
        return geom::Vec3(m_center.x + m_radius, m_center.y, m_center.z);
    }
    
    dir.x /= len;
    dir.y /= len;
    dir.z /= len;
    
    return geom::Vec3(
        m_center.x + dir.x * m_radius,
        m_center.y + dir.y * m_radius,
        m_center.z + dir.z * m_radius
    );
}

geom::Vec3 Circle::GetTangentAt(const geom::Vec3& point) const {
    geom::Vec3 radial = geom::Vec3(
        point.x - m_center.x,
        point.y - m_center.y,
        0.0
    );
    
    // Tangent is perpendicular to radial
    return geom::Vec3(-radial.y, radial.x, 0.0);
}

geom::Vec3 Circle::GetNormalAt(const geom::Vec3& point) const {
    geom::Vec3 normal = geom::Vec3(
        point.x - m_center.x,
        point.y - m_center.y,
        point.z - m_center.z
    );
    
    double len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > 1e-10) {
        normal.x /= len;
        normal.y /= len;
        normal.z /= len;
    }
    
    return normal;
}

std::vector<geom::Vec3> Circle::IntersectWith(const Circle& other, double tolerance) const {
    std::vector<geom::Vec3> intersections;
    
    double d = m_center.DistanceTo(other.m_center);
    
    // No intersection
    if (d > m_radius + other.m_radius + tolerance) return intersections;
    if (d < std::abs(m_radius - other.m_radius) - tolerance) return intersections;
    
    // One point (tangent)
    if (std::abs(d - (m_radius + other.m_radius)) < tolerance) {
        geom::Vec3 dir = geom::Vec3(
            other.m_center.x - m_center.x,
            other.m_center.y - m_center.y,
            0.0
        );
        dir.x /= d;
        dir.y /= d;
        
        intersections.push_back(geom::Vec3(
            m_center.x + dir.x * m_radius,
            m_center.y + dir.y * m_radius,
            m_center.z
        ));
        return intersections;
    }
    
    // Two points
    double a = (m_radius * m_radius - other.m_radius * other.m_radius + d * d) / (2.0 * d);
    double h = std::sqrt(m_radius * m_radius - a * a);
    
    geom::Vec3 p2 = geom::Vec3(
        m_center.x + a * (other.m_center.x - m_center.x) / d,
        m_center.y + a * (other.m_center.y - m_center.y) / d,
        m_center.z
    );
    
    double dx = other.m_center.x - m_center.x;
    double dy = other.m_center.y - m_center.y;
    
    intersections.push_back(geom::Vec3(
        p2.x + h * dy / d,
        p2.y - h * dx / d,
        m_center.z
    ));
    
    intersections.push_back(geom::Vec3(
        p2.x - h * dy / d,
        p2.y + h * dx / d,
        m_center.z
    ));
    
    return intersections;
}

std::vector<geom::Vec3> Circle::IntersectWithLine(
    const geom::Vec3& lineStart,
    const geom::Vec3& lineEnd,
    double tolerance
) const {
    std::vector<geom::Vec3> intersections;
    
    // Line direction
    geom::Vec3 d = geom::Vec3(
        lineEnd.x - lineStart.x,
        lineEnd.y - lineStart.y,
        lineEnd.z - lineStart.z
    );
    
    // Vector from line start to circle center
    geom::Vec3 f = geom::Vec3(
        lineStart.x - m_center.x,
        lineStart.y - m_center.y,
        lineStart.z - m_center.z
    );
    
    double a = geom::Vec3::Dot(d, d);
    double b = 2.0 * geom::Vec3::Dot(f, d);
    double c = geom::Vec3::Dot(f, f) - m_radius * m_radius;
    
    double discriminant = b * b - 4.0 * a * c;
    
    if (discriminant < 0) return intersections;
    
    discriminant = std::sqrt(discriminant);
    
    double t1 = (-b - discriminant) / (2.0 * a);
    double t2 = (-b + discriminant) / (2.0 * a);
    
    if (t1 >= 0 && t1 <= 1) {
        intersections.push_back(geom::Vec3(
            lineStart.x + t1 * d.x,
            lineStart.y + t1 * d.y,
            lineStart.z + t1 * d.z
        ));
    }
    
    if (t2 >= 0 && t2 <= 1 && std::abs(t1 - t2) > tolerance) {
        intersections.push_back(geom::Vec3(
            lineStart.x + t2 * d.x,
            lineStart.y + t2 * d.y,
            lineStart.z + t2 * d.z
        ));
    }
    
    return intersections;
}

bool Circle::IsTangentTo(const Circle& other, double tolerance) const {
    double d = m_center.DistanceTo(other.m_center);
    double sum = m_radius + other.m_radius;
    double diff = std::abs(m_radius - other.m_radius);
    
    return std::abs(d - sum) < tolerance || std::abs(d - diff) < tolerance;
}

void Circle::Move(const geom::Vec3& delta) {
    m_center.x += delta.x;
    m_center.y += delta.y;
    m_center.z += delta.z;
}

void Circle::Scale(const geom::Vec3& scale) {
    m_center.x *= scale.x;
    m_center.y *= scale.y;
    m_center.z *= scale.z;
    m_radius *= (scale.x + scale.y) * 0.5;  // Average scale
}

void Circle::Rotate(double angle) {
    // Rotate center around origin
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    double x = m_center.x * cosA - m_center.y * sinA;
    double y = m_center.x * sinA + m_center.y * cosA;
    m_center.x = x;
    m_center.y = y;
}

void Circle::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    Line mirrorLine(p1, p2);
    geom::Vec3 closest = mirrorLine.GetClosestPoint(m_center);
    geom::Vec3 delta = closest - m_center;
    m_center = closest + delta;
}

std::vector<std::unique_ptr<Entity>> Circle::Tessellate(int segments) const {
    std::vector<std::unique_ptr<Entity>> lines;
    
    double angleStep = 360.0 / segments;
    
    for (int i = 0; i < segments; ++i) {
        geom::Vec3 p1 = GetPointAtAngle(i * angleStep);
        geom::Vec3 p2 = GetPointAtAngle((i + 1) * angleStep);
        
        auto line = std::make_unique<Line>(p1, p2);
        line->SetLayer(const_cast<Layer*>(GetLayer()));
        line->SetColor(GetColor());
        lines.push_back(std::move(line));
    }
    
    return lines;
}

std::vector<std::unique_ptr<Entity>> Circle::SplitToArcs(int numArcs) const {
    std::vector<std::unique_ptr<Entity>> arcs;
    
    double angleStep = 360.0 / numArcs;
    
    for (int i = 0; i < numArcs; ++i) {
        double startAngle = i * angleStep;
        double endAngle = (i + 1) * angleStep;
        
        auto arc = std::make_unique<Arc>(m_center, m_radius, startAngle, endAngle);
        arc->SetLayer(const_cast<Layer*>(GetLayer()));
        arc->SetColor(GetColor());
        arcs.push_back(std::move(arc));
    }
    
    return arcs;
}

std::unique_ptr<Circle> Circle::CreateFrom3Points(
    const geom::Vec3& p1,
    const geom::Vec3& p2,
    const geom::Vec3& p3
) {
    // Calculate circle center using perpendicular bisectors
    // Midpoints
    geom::Vec3 mid12 = geom::Vec3(
        (p1.x + p2.x) * 0.5,
        (p1.y + p2.y) * 0.5,
        (p1.z + p2.z) * 0.5
    );
    
    geom::Vec3 mid23 = geom::Vec3(
        (p2.x + p3.x) * 0.5,
        (p2.y + p3.y) * 0.5,
        (p2.z + p3.z) * 0.5
    );
    
    // Simplified - proper calculation needed
    geom::Vec3 center = mid12;
    double radius = center.DistanceTo(p1);
    
    return std::make_unique<Circle>(center, radius);
}

std::unique_ptr<Circle> Circle::CreateFrom2PointsAndRadius(
    const geom::Vec3& p1,
    const geom::Vec3& p2,
    double radius,
    bool leftSide
) {
    geom::Vec3 mid = geom::Vec3(
        (p1.x + p2.x) * 0.5,
        (p1.y + p2.y) * 0.5,
        (p1.z + p2.z) * 0.5
    );
    
    double d = p1.DistanceTo(p2);
    if (d > 2.0 * radius) {
        // Invalid - chord too long
        return std::make_unique<Circle>(mid, radius);
    }
    
    double h = std::sqrt(radius * radius - (d * d) / 4.0);
    
    geom::Vec3 dir = geom::Vec3(p2.y - p1.y, -(p2.x - p1.x), 0.0);
    double dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (dirLen > 1e-10) {
        dir.x /= dirLen;
        dir.y /= dirLen;
    }
    
    if (!leftSide) h = -h;
    
    geom::Vec3 center = geom::Vec3(
        mid.x + dir.x * h,
        mid.y + dir.y * h,
        mid.z
    );
    
    return std::make_unique<Circle>(center, radius);
}

} // namespace vkt::cad
