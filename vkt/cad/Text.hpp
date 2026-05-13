/**
 * @file Text.hpp
 * @brief Tek satır metin entity'si (AutoCAD TEXT karşılığı)
 */

#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <string>

namespace vkt {
namespace cad {

class Text : public Entity {
public:
    Text();
    Text(const geom::Vec3& insertPoint, const std::string& content,
         double height = 2.5, double rotation = 0.0);

    // Entity interface
    EntityType GetType() const override { return EntityType::Text; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // Metin içeriği
    const std::string& GetText() const          { return m_content; }
    void SetText(const std::string& text)        { m_content = text; }

    // Görünüm
    const geom::Vec3& GetInsertPoint() const     { return m_insertPoint; }
    void SetInsertPoint(const geom::Vec3& p)     { m_insertPoint = p; }
    double GetHeight() const                     { return m_height; }
    void SetHeight(double h)                     { m_height = h; }
    double GetRotationDeg() const                { return m_rotationDeg; }
    void SetRotationDeg(double deg)              { m_rotationDeg = deg; }

    // Justification (DXF code 72=hAlign, 73=vAlign)
    // hAlign: 0=Left,1=Center,2=Right,3=Aligned,4=Middle,5=Fit
    // vAlign: 0=Baseline,1=Bottom,2=Middle,3=Top
    int  GetHAlign() const                       { return m_hAlign; }
    void SetHAlign(int v)                        { m_hAlign = v; }
    int  GetVAlign() const                       { return m_vAlign; }
    void SetVAlign(int v)                        { m_vAlign = v; }

    // Second alignment point (DXF codes 11/21/31) — used when justification != (0,0)
    const geom::Vec3& GetAlignPoint() const      { return m_alignPoint; }
    void SetAlignPoint(const geom::Vec3& p)      { m_alignPoint = p; }

    // Returns the effective anchor point in world space
    // (alignPoint when justification is set, insertPoint otherwise)
    const geom::Vec3& GetEffectiveInsertPoint() const {
        return (m_hAlign != 0 || m_vAlign != 0) ? m_alignPoint : m_insertPoint;
    }

    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angleDeg) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;

private:
    geom::Vec3  m_insertPoint;
    geom::Vec3  m_alignPoint;          ///< Second point (codes 11/21/31), used when hAlign/vAlign != 0
    std::string m_content;
    double      m_height      = 2.5;   ///< mm
    double      m_rotationDeg = 0.0;
    int         m_hAlign      = 0;     ///< Horizontal justification (code 72)
    int         m_vAlign      = 0;     ///< Vertical justification (code 73)
};

} // namespace cad
} // namespace vkt
