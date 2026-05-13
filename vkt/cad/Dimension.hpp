/**
 * @file Dimension.hpp
 * @brief Ölçü çizgisi entity'si (AutoCAD DIMLINEAR / DIMALIGNED karşılığı)
 *
 * Desteklenen tipler:
 *   - Linear   : Yatay veya dikey ölçü (DIMLINEAR)
 *   - Aligned  : İki nokta arasındaki eğik ölçü (DIMALIGNED)
 *   - Radius   : Daire yarıçap ölçüsü (DIMRADIUS)
 *   - Diameter : Daire çap ölçüsü (DIMDIAMETER)
 */

#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <string>

namespace vkt {
namespace cad {

enum class DimensionType {
    Linear,    ///< Yatay/dikey ölçü
    Aligned,   ///< İki nokta boyunca eğik ölçü
    Radius,    ///< Yarıçap
    Diameter   ///< Çap
};

class Dimension : public Entity {
public:
    Dimension();
    Dimension(const geom::Vec3& p1, const geom::Vec3& p2,
              const geom::Vec3& dimLinePos,
              DimensionType type = DimensionType::Aligned);

    // Entity interface
    EntityType GetType() const override { return EntityType::Dimension; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // Tanım noktaları
    const geom::Vec3& GetPoint1()      const { return m_p1; }
    const geom::Vec3& GetPoint2()      const { return m_p2; }
    const geom::Vec3& GetDimLinePos()  const { return m_dimLinePos; }
    void SetPoint1(const geom::Vec3& p)     { m_p1 = p; }
    void SetPoint2(const geom::Vec3& p)     { m_p2 = p; }
    void SetDimLinePos(const geom::Vec3& p) { m_dimLinePos = p; }

    // Ölçü tipi
    DimensionType GetDimType() const           { return m_dimType; }
    void          SetDimType(DimensionType t)  { m_dimType = t; }

    // Görünüm parametreleri
    double GetTextHeight()  const { return m_textHeight; }
    double GetArrowSize()   const { return m_arrowSize; }
    double GetExtOffset()   const { return m_extOffset; }   ///< Uzatma çizgisi kaydırma
    void SetTextHeight(double h)  { m_textHeight = h; }
    void SetArrowSize(double s)   { m_arrowSize  = s; }
    void SetExtOffset(double d)   { m_extOffset  = d; }

    // Metin
    const std::string& GetOverrideText() const { return m_overrideText; }
    void SetOverrideText(const std::string& t) { m_overrideText = t; }

    // Ölçülen mesafeyi hesapla (otomatik)
    double GetMeasuredValue() const;

    // Gösterilecek metin — override yoksa ölçülen değer
    std::string GetDisplayText() const;

    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;

private:
    geom::Vec3    m_p1;             ///< 1. tanım noktası
    geom::Vec3    m_p2;             ///< 2. tanım noktası
    geom::Vec3    m_dimLinePos;     ///< Ölçü çizgisinin geçtiği nokta
    DimensionType m_dimType = DimensionType::Aligned;

    double      m_textHeight   = 2.5;   ///< mm
    double      m_arrowSize    = 2.5;   ///< mm
    double      m_extOffset    = 1.0;   ///< mm — uzatma çizgisi başlama boşluğu
    std::string m_overrideText;         ///< Boşsa otomatik hesaplanır

    static geom::Vec3 RotatePoint2D(const geom::Vec3& pt,
                                    const geom::Vec3& center,
                                    double angleDeg);
};

} // namespace cad
} // namespace vkt
