/**
 * @file Hatch.hpp
 * @brief Tarama/dolgu entity'si (AutoCAD HATCH karşılığı)
 *
 * SOLID pattern için kapalı sınır polygon'u fan-triangulation ile doldurur.
 * Non-solid pattern'lar sınır çizgisi olarak render edilir.
 */

#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <string>
#include <vector>

namespace vkt {
namespace cad {

class Hatch : public Entity {
public:
    struct BoundaryVertex {
        geom::Vec3 pos;
        double bulge = 0.0;
    };

    Hatch();

    EntityType GetType() const override { return EntityType::Hatch; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // Pattern
    const std::string& GetPatternName() const { return m_patternName; }
    void SetPatternName(const std::string& n)  { m_patternName = n; }
    bool IsSolid() const { return m_patternName == "SOLID" || m_patternName.empty(); }

    double GetPatternAngle() const             { return m_patternAngle; }
    void   SetPatternAngle(double deg)         { m_patternAngle = deg; }
    double GetPatternScale() const             { return m_patternScale; }
    void   SetPatternScale(double s)           { m_patternScale = (s > 1e-9) ? s : 1.0; }

    // Boundary
    const std::vector<BoundaryVertex>& GetBoundary() const { return m_boundary; }
    void SetBoundary(std::vector<BoundaryVertex> b) { m_boundary = std::move(b); }
    void AddBoundaryVertex(const BoundaryVertex& v)  { m_boundary.push_back(v); }

    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& s) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;

private:
    std::string              m_patternName = "SOLID";
    double                   m_patternAngle = 0.0;   ///< degrees, from DXF code 52
    double                   m_patternScale = 1.0;   ///< from DXF code 41
    std::vector<BoundaryVertex> m_boundary;
};

} // namespace cad
} // namespace vkt
