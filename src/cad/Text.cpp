/**
 * @file Text.cpp
 * @brief Metin entity'si implementasyonu
 */

#include "cad/Text.hpp"
#include <cmath>

namespace vkt {
namespace cad {

static constexpr double PI = 3.14159265358979323846;

Text::Text() : Entity(EntityType::Text) {}

Text::Text(const geom::Vec3& insertPoint, const std::string& content,
           double height, double rotation)
    : Entity(EntityType::Text)
    , m_insertPoint(insertPoint), m_content(content)
    , m_height(height), m_rotationDeg(rotation) {}

std::unique_ptr<Entity> Text::Clone() const {
    return std::make_unique<Text>(*this);
}

BoundingBox Text::GetBounds() const {
    BoundingBox bb;
    bb.Extend(m_insertPoint);
    // Yaklaşık genişlik tahmini (karakter başı ~0.6 × yükseklik)
    double approxWidth = m_content.size() * m_height * 0.6;
    bb.Extend({m_insertPoint.x + approxWidth, m_insertPoint.y + m_height, m_insertPoint.z});
    return bb;
}

void Text::Serialize(nlohmann::json& j) const {
    j["type"]     = "Text";
    j["insert"]   = {m_insertPoint.x, m_insertPoint.y, m_insertPoint.z};
    j["content"]  = m_content;
    j["height"]   = m_height;
    j["rotation"] = m_rotationDeg;
    j["layer"]    = GetLayerName();
}

void Text::Deserialize(const nlohmann::json& j) {
    auto ins     = j["insert"];
    m_insertPoint = {ins[0], ins[1], ins[2]};
    m_content     = j.value("content",  "");
    m_height      = j.value("height",   2.5);
    m_rotationDeg = j.value("rotation", 0.0);
    SetLayerName(j.value("layer", ""));
}

void Text::Move(const geom::Vec3& delta) {
    m_insertPoint.x += delta.x;
    m_insertPoint.y += delta.y;
    m_insertPoint.z += delta.z;
    m_alignPoint.x  += delta.x;
    m_alignPoint.y  += delta.y;
    m_alignPoint.z  += delta.z;
}

void Text::Scale(const geom::Vec3& scale) {
    m_insertPoint.x *= scale.x;
    m_insertPoint.y *= scale.y;
    m_height        *= (scale.x + scale.y) / 2.0;
}

void Text::Rotate(double angleDeg) {
    m_rotationDeg += angleDeg;
    double rad  = angleDeg * PI / 180.0;
    double cosA = std::cos(rad), sinA = std::sin(rad);
    double x = m_insertPoint.x * cosA - m_insertPoint.y * sinA;
    double y = m_insertPoint.x * sinA + m_insertPoint.y * cosA;
    m_insertPoint.x = x;
    m_insertPoint.y = y;
}

void Text::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    double dx = p2.x - p1.x, dy = p2.y - p1.y;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-12) return;
    double t = ((m_insertPoint.x - p1.x)*dx + (m_insertPoint.y - p1.y)*dy) / len2;
    double fx = p1.x + t*dx, fy = p1.y + t*dy;
    m_insertPoint.x = 2.0*fx - m_insertPoint.x;
    m_insertPoint.y = 2.0*fy - m_insertPoint.y;
    m_rotationDeg   = -m_rotationDeg; // Yansımada metin yönü ters döner
}

} // namespace cad
} // namespace vkt
