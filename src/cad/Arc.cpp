#include "cad/Arc.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Layer.hpp"
#include "nlohmann/json.hpp"
#include <cmath>

namespace vkt::cad {

constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;

Arc::Arc() : Entity(), m_center(), m_radius(1.0), m_startAngle(0.0), m_endAngle(90.0) {}

Arc::Arc(const geom::Vec3& center, double radius, double startAngle, double endAngle)
    : Entity(), m_center(center), m_radius(radius), m_startAngle(startAngle), m_endAngle(endAngle) {}

std::unique_ptr<Entity> Arc::Clone() const {
    auto clone = std::make_unique<Arc>(m_center, m_radius, m_startAngle, m_endAngle);
    clone->SetLayer(const_cast<Layer*>(GetLayer()));
    clone->SetColor(GetColor());
    clone->SetVisible(IsVisible());
    clone->SetLocked(IsLocked());
    return clone;
}

BoundingBox Arc::GetBounds() const {
    // Calculate bounding box for arc
    BoundingBox bounds;
    
    // Start and end points
    geom::Vec3 start = GetStartPoint();
    geom::Vec3 end = GetEndPoint();
    
    bounds.min = geom::Vec3(
        std::min(start.x, end.x),
        std::min(start.y, end.y),
        std::min(start.z, end.z)
    );
    bounds.max = geom::Vec3(
        std::max(start.x, end.x),
        std::max(start.y, end.y),
        std::max(start.z, end.z)
    );
    
    // Check if arc crosses 0, 90, 180, 270 degrees
    double sweep = GetSweepAngle();
    double normalized_start = NormalizeAngle(m_startAngle);
    
    // Check each cardinal direction
    double cardinals[] = {0.0, 90.0, 180.0, 270.0};
    for (double angle : cardinals) {
        double diff = NormalizeAngle(angle - normalized_start);
        if (diff >= 0 && diff <= sweep) {
            geom::Vec3 point = GetPointAtAngle(angle);
            bounds.Expand(point);
        }
    }
    
    return bounds;
}

void Arc::Serialize(nlohmann::json& j) const {
    j["type"] = "Arc";
    j["center"] = {m_center.x, m_center.y, m_center.z};
    j["radius"] = m_radius;
    j["startAngle"] = m_startAngle;
    j["endAngle"] = m_endAngle;
}

void Arc::Deserialize(const nlohmann::json& j) {
    if (j.contains("center") && j["center"].is_array() && j["center"].size() >= 2) {
        m_center.x = j["center"][0];
        m_center.y = j["center"][1];
        m_center.z = j["center"].size() > 2 ? j["center"][2].get<double>() : 0.0;
    }
    m_radius = j.value("radius", 1.0);
    m_startAngle = j.value("startAngle", 0.0);
    m_endAngle = j.value("endAngle", 90.0);
}

geom::Vec3 Arc::GetStartPoint() const {
    return GetPointAtAngle(m_startAngle);
}

geom::Vec3 Arc::GetEndPoint() const {
    return GetPointAtAngle(m_endAngle);
}

geom::Vec3 Arc::GetMidPoint() const {
    double midAngle = m_startAngle + GetSweepAngle() * 0.5;
    return GetPointAtAngle(midAngle);
}

double Arc::GetLength() const {
    return m_radius * GetSweepAngleRadians();
}

double Arc::GetSweepAngle() const {
    double sweep = m_endAngle - m_startAngle;
    if (sweep < 0) sweep += 360.0;
    return sweep;
}

double Arc::GetSweepAngleRadians() const {
    return GetSweepAngle() * DEG_TO_RAD;
}

geom::Vec3 Arc::GetPointAt(double t) const {
    double angle = m_startAngle + t * GetSweepAngle();
    return GetPointAtAngle(angle);
}

geom::Vec3 Arc::GetPointAtAngle(double angle) const {
    double rad = angle * DEG_TO_RAD;
    return geom::Vec3(
        m_center.x + m_radius * std::cos(rad),
        m_center.y + m_radius * std::sin(rad),
        m_center.z
    );
}

bool Arc::ContainsPoint(const geom::Vec3& point, double tolerance) const {
    double dist = m_center.DistanceTo(point);
    if (std::abs(dist - m_radius) > tolerance) return false;
    
    // Check angle
    double dx = point.x - m_center.x;
    double dy = point.y - m_center.y;
    double angle = std::atan2(dy, dx) * RAD_TO_DEG;
    angle = NormalizeAngle(angle);
    
    double start = NormalizeAngle(m_startAngle);
    double sweep = GetSweepAngle();
    double pointAngle = NormalizeAngle(angle - start);
    
    return pointAngle >= 0 && pointAngle <= sweep;
}

geom::Vec3 Arc::GetClosestPoint(const geom::Vec3& point) const {
    // Direction from center to point
    geom::Vec3 dir = geom::Vec3(
        point.x - m_center.x,
        point.y - m_center.y,
        point.z - m_center.z
    );
    
    double angle = std::atan2(dir.y, dir.x) * RAD_TO_DEG;
    angle = NormalizeAngle(angle);
    
    double start = NormalizeAngle(m_startAngle);
    double sweep = GetSweepAngle();
    double pointAngle = NormalizeAngle(angle - start);
    
    // Clamp to arc range
    if (pointAngle < 0) return GetStartPoint();
    if (pointAngle > sweep) return GetEndPoint();
    
    return GetPointAtAngle(angle);
}

geom::Vec3 Arc::GetCentroid() const {
    // Arc centroid formula
    double sweep = GetSweepAngleRadians();
    double avgAngle = (m_startAngle + m_endAngle) * 0.5 * DEG_TO_RAD;
    double r = (2.0 * m_radius * std::sin(sweep / 2.0)) / sweep;
    
    return geom::Vec3(
        m_center.x + r * std::cos(avgAngle),
        m_center.y + r * std::sin(avgAngle),
        m_center.z
    );
}

bool Arc::IsCounterClockwise() const {
    return GetSweepAngle() > 0;
}

void Arc::Reverse() {
    std::swap(m_startAngle, m_endAngle);
}

void Arc::Move(const geom::Vec3& delta) {
    m_center.x += delta.x;
    m_center.y += delta.y;
    m_center.z += delta.z;
}

void Arc::Scale(const geom::Vec3& scale) {
    m_center.x *= scale.x;
    m_center.y *= scale.y;
    m_center.z *= scale.z;
    m_radius *= (scale.x + scale.y) * 0.5;  // Average scale
}

void Arc::Rotate(double angle) {
    m_startAngle += angle * RAD_TO_DEG;
    m_endAngle += angle * RAD_TO_DEG;
    
    // Rotate center around origin
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    double x = m_center.x * cosA - m_center.y * sinA;
    double y = m_center.x * sinA + m_center.y * cosA;
    m_center.x = x;
    m_center.y = y;
}

void Arc::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    // Mirror center point
    Line mirrorLine(p1, p2);
    geom::Vec3 closest = mirrorLine.GetClosestPoint(m_center);
    geom::Vec3 delta = closest - m_center;
    m_center = closest + delta;
    
    // Mirror angles
    geom::Vec3 dir = p2 - p1;
    double mirrorAngle = std::atan2(dir.y, dir.x);
    
    m_startAngle = 2.0 * mirrorAngle * RAD_TO_DEG - m_startAngle;
    m_endAngle = 2.0 * mirrorAngle * RAD_TO_DEG - m_endAngle;
    std::swap(m_startAngle, m_endAngle);
}

std::vector<std::unique_ptr<Entity>> Arc::Tessellate(int segments) const {
    std::vector<std::unique_ptr<Entity>> lines;
    
    double angleStep = GetSweepAngle() / segments;
    
    for (int i = 0; i < segments; ++i) {
        double angle1 = m_startAngle + i * angleStep;
        double angle2 = m_startAngle + (i + 1) * angleStep;
        
        geom::Vec3 p1 = GetPointAtAngle(angle1);
        geom::Vec3 p2 = GetPointAtAngle(angle2);
        
        auto line = std::make_unique<Line>(p1, p2);
        line->SetLayer(const_cast<Layer*>(GetLayer()));
        line->SetColor(GetColor());
        lines.push_back(std::move(line));
    }
    
    return lines;
}

std::unique_ptr<Entity> Arc::ToCircle() const {
    auto circle = std::make_unique<Circle>(m_center, m_radius);
    circle->SetLayer(const_cast<Layer*>(GetLayer()));
    circle->SetColor(GetColor());
    return circle;
}

std::unique_ptr<Arc> Arc::CreateFrom3Points(
    const geom::Vec3& start,
    const geom::Vec3& mid,
    const geom::Vec3& end
) {
    // Calculate circle center from 3 points
    // Using perpendicular bisectors
    
    geom::Vec3 mid1 = geom::Vec3(
        (start.x + mid.x) * 0.5,
        (start.y + mid.y) * 0.5,
        (start.z + mid.z) * 0.5
    );
    
    geom::Vec3 mid2 = geom::Vec3(
        (mid.x + end.x) * 0.5,
        (mid.y + end.y) * 0.5,
        (mid.z + end.z) * 0.5
    );
    
    // Circumcircle via perpendicular bisector intersection
    // Midpoints of chords start-mid and mid-end
    double ax = (start.x + mid.x) * 0.5, ay = (start.y + mid.y) * 0.5;
    double bx = (mid.x + end.x)   * 0.5, by = (mid.y + end.y)   * 0.5;

    // Direction vectors of the two chords (perpendicular bisector is orthogonal)
    double dx1 = mid.x - start.x, dy1 = mid.y - start.y; // chord 1 direction
    double dx2 = end.x - mid.x,   dy2 = end.y - mid.y;   // chord 2 direction

    // Perpendicular bisectors: (ax,ay) + t*(-dy1,dx1)  and  (bx,by) + s*(-dy2,dx2)
    // Solve for t: ax - t*dy1 = bx - s*dy2  and  ay + t*dx1 = by + s*dx2
    // => -dy1*t + dy2*s = bx - ax
    //     dx1*t - dx2*s = by - ay
    double det = -dy1 * (-dx2) - (-dx2) * 0.0; // use Cramer's rule on 2×2 system
    // Rewrite: t*(-dy1) + s*(dy2) = bx-ax   =>  [ -dy1  dy2 ] [t]   [bx-ax]
    //          t*( dx1) + s*(-dx2)= by-ay       [  dx1 -dx2 ] [s] = [by-ay]
    det = (-dy1) * (-dx2) - (dy2) * (dx1);

    geom::Vec3 center;
    if (std::abs(det) < geom::EPSILON) {
        // Points are collinear — degenerate arc, fall back to midpoint
        center = mid;
    } else {
        double t = ((bx - ax) * (-dx2) - (by - ay) * (dy2)) / det;
        center.x = ax + t * (-dy1);
        center.y = ay + t * (dx1);
        center.z = (start.z + mid.z + end.z) / 3.0;
    }

    double radius = center.DistanceTo(start);
    double startAngle = std::atan2(start.y - center.y, start.x - center.x) * RAD_TO_DEG;
    double endAngle   = std::atan2(end.y   - center.y, end.x   - center.x) * RAD_TO_DEG;

    return std::make_unique<Arc>(center, radius, startAngle, endAngle);
}

std::unique_ptr<Arc> Arc::CreateFromBulge(
    const geom::Vec3& start,
    const geom::Vec3& end,
    double bulge
) {
    double chord = start.DistanceTo(end);
    double angle = 4.0 * std::atan(bulge);
    double radius = chord / (2.0 * std::sin(angle / 2.0));
    
    geom::Vec3 mid = geom::Vec3(
        (start.x + end.x) * 0.5,
        (start.y + end.y) * 0.5,
        (start.z + end.z) * 0.5
    );
    
    // Perpendicular direction
    geom::Vec3 dir = geom::Vec3(end.y - start.y, -(end.x - start.x), 0.0);
    double dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (dirLen > 1e-10) {
        dir.x /= dirLen;
        dir.y /= dirLen;
    }
    
    double sagitta = radius - std::sqrt(radius * radius - (chord * chord) / 4.0);
    geom::Vec3 center = geom::Vec3(
        mid.x + dir.x * sagitta,
        mid.y + dir.y * sagitta,
        mid.z
    );
    
    double startAngle = std::atan2(start.y - center.y, start.x - center.x) * RAD_TO_DEG;
    double endAngle = std::atan2(end.y - center.y, end.x - center.x) * RAD_TO_DEG;
    
    return std::make_unique<Arc>(center, std::abs(radius), startAngle, endAngle);
}

double Arc::NormalizeAngle(double angle) const {
    while (angle < 0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    return angle;
}

} // namespace vkt::cad
