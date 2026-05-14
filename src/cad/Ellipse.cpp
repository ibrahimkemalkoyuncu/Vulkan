#include "cad/Ellipse.hpp"
#include <cmath>
#include <algorithm>

namespace vkt::cad {

static constexpr double kTwoPi = 6.283185307179586;

Ellipse::Ellipse()
    : Entity(EntityType::Ellipse)
    , m_center(0, 0, 0)
    , m_semiMajor(1.0)
    , m_axisRatio(1.0)
    , m_rotAngle(0.0)
    , m_startParam(0.0)
    , m_endParam(kTwoPi)
{}

Ellipse::Ellipse(const geom::Vec3& center,
                 double semiMajor, double axisRatio,
                 double rotAngle,
                 double startParam, double endParam)
    : Entity(EntityType::Ellipse)
    , m_center(center)
    , m_semiMajor(semiMajor)
    , m_axisRatio(axisRatio)
    , m_rotAngle(rotAngle)
    , m_startParam(startParam)
    , m_endParam(endParam)
{}

bool Ellipse::IsFullEllipse() const {
    double sweep = m_endParam - m_startParam;
    if (sweep < 0) sweep += kTwoPi;
    return (std::abs(sweep) < 1e-10) || (std::abs(sweep - kTwoPi) < 1e-10);
}

geom::Vec3 Ellipse::GetPointAt(double t) const {
    double semiMinor = m_semiMajor * m_axisRatio;
    double cosR = std::cos(m_rotAngle);
    double sinR = std::sin(m_rotAngle);
    double lx = m_semiMajor * std::cos(t);
    double ly = semiMinor   * std::sin(t);
    return geom::Vec3(
        m_center.x + lx * cosR - ly * sinR,
        m_center.y + lx * sinR + ly * cosR,
        m_center.z
    );
}

std::vector<geom::Vec3> Ellipse::Tessellate(int segments) const {
    if (segments <= 0) segments = 64;

    double start  = m_startParam;
    double end    = m_endParam;
    bool full     = IsFullEllipse();
    double sweep  = full ? kTwoPi : (end - start);
    if (sweep < 0) sweep += kTwoPi;

    std::vector<geom::Vec3> pts;
    pts.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        double t = start + sweep * i / segments;
        pts.push_back(GetPointAt(t));
    }
    return pts;
}

std::unique_ptr<Entity> Ellipse::Clone() const {
    auto e = std::make_unique<Ellipse>(m_center, m_semiMajor, m_axisRatio,
                                       m_rotAngle, m_startParam, m_endParam);
    e->m_color = m_color;
    e->m_layer = m_layer;
    return e;
}

BoundingBox Ellipse::GetBounds() const {
    // Conservative: axis-aligned bounding box of tessellated points
    auto pts = Tessellate(64);
    BoundingBox bb;
    for (const auto& p : pts) bb.Expand(p);
    return bb;
}

void Ellipse::Serialize(nlohmann::json& j) const {
    j["type"]       = "Ellipse";
    j["cx"]         = m_center.x;
    j["cy"]         = m_center.y;
    j["cz"]         = m_center.z;
    j["semiMajor"]  = m_semiMajor;
    j["axisRatio"]  = m_axisRatio;
    j["rotAngle"]   = m_rotAngle;
    j["startParam"] = m_startParam;
    j["endParam"]   = m_endParam;
}

void Ellipse::Deserialize(const nlohmann::json& j) {
    m_center    = geom::Vec3(j.value("cx", 0.0), j.value("cy", 0.0), j.value("cz", 0.0));
    m_semiMajor = j.value("semiMajor",  1.0);
    m_axisRatio = j.value("axisRatio",  1.0);
    m_rotAngle  = j.value("rotAngle",   0.0);
    m_startParam= j.value("startParam", 0.0);
    m_endParam  = j.value("endParam",   kTwoPi);
}

void Ellipse::Move(const geom::Vec3& delta) {
    m_center.x += delta.x;
    m_center.y += delta.y;
    m_center.z += delta.z;
}

void Ellipse::Scale(const geom::Vec3& scale) {
    // Uniform scale only (take average of x/y)
    double s = (std::abs(scale.x) + std::abs(scale.y)) * 0.5;
    m_center.x *= scale.x;
    m_center.y *= scale.y;
    m_center.z *= scale.z;
    m_semiMajor *= s;
}

void Ellipse::Rotate(double angleDeg) {
    double rad = angleDeg * kTwoPi / 360.0;
    double cosA = std::cos(rad), sinA = std::sin(rad);
    double nx = m_center.x * cosA - m_center.y * sinA;
    double ny = m_center.x * sinA + m_center.y * cosA;
    m_center.x = nx;
    m_center.y = ny;
    m_rotAngle += rad;
}

void Ellipse::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    // Mirror center
    double dx = p2.x - p1.x, dy = p2.y - p1.y;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-24) return;
    double t = ((m_center.x - p1.x) * dx + (m_center.y - p1.y) * dy) / len2;
    m_center.x = 2.0 * (p1.x + t * dx) - m_center.x;
    m_center.y = 2.0 * (p1.y + t * dy) - m_center.y;
    // Mirror rotation angle about mirror line angle
    double lineAngle = std::atan2(dy, dx);
    m_rotAngle = 2.0 * lineAngle - m_rotAngle;
}

} // namespace vkt::cad
