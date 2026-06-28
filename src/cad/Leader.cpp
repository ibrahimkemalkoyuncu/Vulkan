/**
 * @file Leader.cpp
 * @brief Leader entity implementasyonu
 */

#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "cad/Leader.hpp"
#include <algorithm>

namespace vkt {
namespace cad {

Leader::Leader(std::vector<geom::Vec3> vertices, std::string text)
    : m_vertices(std::move(vertices)), m_text(std::move(text))
{}

std::unique_ptr<Entity> Leader::Clone() const {
    auto clone = std::make_unique<Leader>(m_vertices, m_text);
    clone->SetArrowhead(m_arrowhead);
    clone->SetColor(GetColor());
    clone->SetLayerName(GetLayerName());
    return clone;
}

BoundingBox Leader::GetBounds() const {
    BoundingBox bb;
    for (const auto& v : m_vertices) {
        bb.min.x = std::min(bb.min.x, v.x);
        bb.min.y = std::min(bb.min.y, v.y);
        bb.min.z = std::min(bb.min.z, v.z);
        bb.max.x = std::max(bb.max.x, v.x);
        bb.max.y = std::max(bb.max.y, v.y);
        bb.max.z = std::max(bb.max.z, v.z);
    }
    return bb;
}

void Leader::Serialize(nlohmann::json& j) const {
    j["type"] = "Leader";
    j["layer"] = GetLayerName();
    j["text"] = m_text;
    j["arrowhead"] = m_arrowhead;
    nlohmann::json verts = nlohmann::json::array();
    for (const auto& v : m_vertices)
        verts.push_back({{"x", v.x}, {"y", v.y}, {"z", v.z}});
    j["vertices"] = verts;
}

void Leader::Deserialize(const nlohmann::json& j) {
    SetLayerName(j.value("layer", "0"));
    m_text = j.value("text", "");
    m_arrowhead = j.value("arrowhead", true);
    m_vertices.clear();
    if (j.contains("vertices")) {
        for (const auto& v : j["vertices"])
            m_vertices.emplace_back(v.value("x", 0.0), v.value("y", 0.0), v.value("z", 0.0));
    }
}

void Leader::Move(const geom::Vec3& delta) {
    for (auto& v : m_vertices) {
        v.x += delta.x;
        v.y += delta.y;
        v.z += delta.z;
    }
}

void Leader::Scale(const geom::Vec3& scale) {
    for (auto& v : m_vertices) {
        v.x *= scale.x;
        v.y *= scale.y;
        v.z *= scale.z;
    }
}

void Leader::Rotate(double angle) {
    double rad = angle * M_PI / 180.0;
    double cosA = std::cos(rad), sinA = std::sin(rad);
    for (auto& v : m_vertices) {
        double nx = v.x * cosA - v.y * sinA;
        double ny = v.x * sinA + v.y * cosA;
        v.x = nx;
        v.y = ny;
    }
}

void Leader::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    double dx = p2.x - p1.x, dy = p2.y - p1.y;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-12) return;
    for (auto& v : m_vertices) {
        double t = ((v.x - p1.x)*dx + (v.y - p1.y)*dy) / len2;
        double fx = p1.x + t*dx;
        double fy = p1.y + t*dy;
        v.x = 2*fx - v.x;
        v.y = 2*fy - v.y;
    }
}

} // namespace cad
} // namespace vkt
