/**
 * @file Leader.hpp
 * @brief Leader (ok/işaret çizgisi) entity'si — DXF LEADER / DWG LEADER karşılığı
 *
 * Köşelerden oluşan kırık çizgi + opsiyonel metin notu.
 * VulkanRenderer'da polyline segmentleri olarak render edilir.
 */

#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <string>
#include <vector>

namespace vkt {
namespace cad {

class Leader : public Entity {
public:
    Leader() = default;
    explicit Leader(std::vector<geom::Vec3> vertices, std::string text = "");

    EntityType GetType() const override { return EntityType::Leader; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // Vertex access
    const std::vector<geom::Vec3>& GetVertices() const { return m_vertices; }
    void SetVertices(std::vector<geom::Vec3> v) { m_vertices = std::move(v); }
    void AddVertex(const geom::Vec3& v) { m_vertices.push_back(v); }

    // Optional annotation text
    const std::string& GetText() const { return m_text; }
    void SetText(const std::string& t) { m_text = t; }

    // Arrow head: draw filled triangle at first vertex
    bool HasArrowhead() const { return m_arrowhead; }
    void SetArrowhead(bool b) { m_arrowhead = b; }

    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;

private:
    std::vector<geom::Vec3> m_vertices;
    std::string m_text;
    bool m_arrowhead = true;
};

} // namespace cad
} // namespace vkt
