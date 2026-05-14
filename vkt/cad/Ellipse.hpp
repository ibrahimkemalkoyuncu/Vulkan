#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <vector>
#include <memory>

namespace vkt::cad {

/**
 * @brief Ellipse (elips) entity'si
 *
 * DXF/DWG ELLIPSE tipini parametre olarak saklar; render için
 * istenilen segment sayısında tessellate edilebilir.
 *
 * Parametrik form:  P(t) = center + R(rotAngle) * (a*cos(t), b*sin(t))
 * Burada a = semiMajor, b = semiMinor = a * axisRatio
 */
class Ellipse : public Entity {
public:
    Ellipse();

    /**
     * @param center     Merkez nokta
     * @param semiMajor  Büyük eksen yarıçapı (metre)
     * @param axisRatio  Küçük/büyük eksen oranı [0,1]
     * @param rotAngle   Büyük eksenin X-eksenine göre açısı (radyan)
     * @param startParam Başlangıç parametresi (radyan, genelde 0)
     * @param endParam   Bitiş parametresi (radyan, genelde 2π)
     */
    Ellipse(const geom::Vec3& center,
            double semiMajor, double axisRatio,
            double rotAngle,
            double startParam, double endParam);

    // Entity interface
    EntityType GetType() const override { return EntityType::Ellipse; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // Parametreler
    const geom::Vec3& GetCenter()    const { return m_center; }
    double GetSemiMajor()            const { return m_semiMajor; }
    double GetSemiMinor()            const { return m_semiMajor * m_axisRatio; }
    double GetAxisRatio()            const { return m_axisRatio; }
    double GetRotAngle()             const { return m_rotAngle; }
    double GetStartParam()           const { return m_startParam; }
    double GetEndParam()             const { return m_endParam; }

    void SetCenter(const geom::Vec3& c) { m_center = c; }
    void SetSemiMajor(double v)         { m_semiMajor = v; }
    void SetAxisRatio(double v)         { m_axisRatio = v; }
    void SetRotAngle(double v)          { m_rotAngle  = v; }
    void SetStartParam(double v)        { m_startParam = v; }
    void SetEndParam(double v)          { m_endParam   = v; }

    bool IsFullEllipse() const;

    /** @brief Parametrik noktayı hesapla (t radyan) */
    geom::Vec3 GetPointAt(double t) const;

    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;

    /**
     * @brief Render için vertex listesi üret
     *
     * Adaptive segment count: büyük eksen ekranı kaç px kaplıyorsa ona göre
     * hedef chord ≤ 4px. Caller genelde ppu (pixels per unit) geçirir.
     *
     * @param segments  Segment sayısı (0 = 64 default)
     */
    std::vector<geom::Vec3> Tessellate(int segments = 64) const;

private:
    geom::Vec3 m_center;
    double m_semiMajor  = 1.0;
    double m_axisRatio  = 1.0;  ///< minor/major, [0,1]
    double m_rotAngle   = 0.0;  ///< radyan
    double m_startParam = 0.0;  ///< radyan
    double m_endParam   = 0.0;  ///< radyan (0 = tam ellips anlamı için IsFullEllipse() kullan)

    static constexpr double kTwoPi = 6.283185307179586;
};

} // namespace vkt::cad
