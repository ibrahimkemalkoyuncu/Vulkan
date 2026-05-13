/**
 * @file Hatch.cpp
 * @brief Hatch entity implementasyonu
 */

#include "cad/Hatch.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace vkt {
namespace cad {

Hatch::Hatch() : Entity(EntityType::Hatch) {}

std::unique_ptr<Entity> Hatch::Clone() const {
    auto h = std::make_unique<Hatch>();
    h->m_patternName = m_patternName;
    h->m_boundary    = m_boundary;
    return h;
}

BoundingBox Hatch::GetBounds() const {
    BoundingBox bb;
    for (const auto& v : m_boundary) {
        bb.min.x = std::min(bb.min.x, v.pos.x);
        bb.min.y = std::min(bb.min.y, v.pos.y);
        bb.min.z = std::min(bb.min.z, v.pos.z);
        bb.max.x = std::max(bb.max.x, v.pos.x);
        bb.max.y = std::max(bb.max.y, v.pos.y);
        bb.max.z = std::max(bb.max.z, v.pos.z);
    }
    return bb;
}

void Hatch::Serialize(nlohmann::json& j) const {
    j["patternName"] = m_patternName;
    auto& bArr = j["boundary"] = nlohmann::json::array();
    for (const auto& v : m_boundary)
        bArr.push_back({{"x", v.pos.x}, {"y", v.pos.y}, {"z", v.pos.z}, {"bulge", v.bulge}});
}

void Hatch::Deserialize(const nlohmann::json& j) {
    m_patternName = j.value("patternName", "SOLID");
    if (j.contains("boundary")) {
        for (const auto& bv : j["boundary"]) {
            BoundaryVertex v;
            v.pos   = {bv.value("x", 0.0), bv.value("y", 0.0), bv.value("z", 0.0)};
            v.bulge = bv.value("bulge", 0.0);
            m_boundary.push_back(v);
        }
    }
}

void Hatch::Move(const geom::Vec3& delta) {
    for (auto& v : m_boundary) { v.pos.x += delta.x; v.pos.y += delta.y; v.pos.z += delta.z; }
}

void Hatch::Scale(const geom::Vec3& s) {
    for (auto& v : m_boundary) { v.pos.x *= s.x; v.pos.y *= s.y; v.pos.z *= s.z; }
}

void Hatch::Rotate(double angle) {
    double c = std::cos(angle), s = std::sin(angle);
    for (auto& v : m_boundary) {
        double x = v.pos.x * c - v.pos.y * s;
        double y = v.pos.x * s + v.pos.y * c;
        v.pos.x = x; v.pos.y = y;
    }
}

void Hatch::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    double dx = p2.x - p1.x, dy = p2.y - p1.y;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-18) return;
    for (auto& v : m_boundary) {
        double t = ((v.pos.x - p1.x)*dx + (v.pos.y - p1.y)*dy) / len2;
        double fx = p1.x + t*dx, fy = p1.y + t*dy;
        v.pos.x = 2*fx - v.pos.x;
        v.pos.y = 2*fy - v.pos.y;
    }
}

} // namespace cad
} // namespace vkt
