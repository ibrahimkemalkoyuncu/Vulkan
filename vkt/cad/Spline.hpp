#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <vector>
#include <memory>

namespace vkt::cad {

/**
 * @brief Spline (NURBS / B-spline) entity'si
 *
 * DXF/DWG SPLINE tipini parametrik olarak saklar.
 * İki temsil desteklenir:
 *   A) Fit noktaları — kullanıcı tarafından doğrudan belirlenen noktalar
 *   B) Kontrol noktaları + knot vektörü + derece — tam NURBS
 *
 * HasFitPoints() true ise fit noktaları esas alınır (doğrudan Tessellate).
 * Aksi hâlde De Boor algoritması ile B-spline evaluate edilir.
 */
class Spline : public Entity {
public:
    Spline();

    // Entity interface
    EntityType GetType() const override { return EntityType::Spline; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // ── Fit points ────────────────────────────────────────────
    bool HasFitPoints() const { return !m_fitPts.empty(); }
    const std::vector<geom::Vec3>& GetFitPoints()  const { return m_fitPts; }
    void SetFitPoints(std::vector<geom::Vec3> pts)       { m_fitPts = std::move(pts); }
    void AddFitPoint(const geom::Vec3& p)                { m_fitPts.push_back(p); }

    // ── Control points (NURBS) ────────────────────────────────
    bool HasCtrlPoints() const { return !m_ctrlPts.empty(); }
    const std::vector<geom::Vec3>& GetCtrlPoints() const { return m_ctrlPts; }
    void SetCtrlPoints(std::vector<geom::Vec3> pts)      { m_ctrlPts = std::move(pts); }

    const std::vector<double>& GetKnots()  const { return m_knots; }
    void SetKnots(std::vector<double> k)         { m_knots = std::move(k); }

    int  GetDegree()  const { return m_degree; }
    void SetDegree(int d)   { m_degree = d; }

    bool IsClosed()   const { return m_closed; }
    void SetClosed(bool c)  { m_closed = c; }

    // ── Tessellate ────────────────────────────────────────────
    /**
     * @brief Render için vertex listesi üret
     * @param segments Segment sayısı (0 = auto: max(32, ctrlCount*8))
     */
    std::vector<geom::Vec3> Tessellate(int segments = 0) const;

    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;

private:
    std::vector<geom::Vec3> m_fitPts;   ///< Fit noktaları
    std::vector<geom::Vec3> m_ctrlPts;  ///< Kontrol noktaları (NURBS)
    std::vector<double>     m_knots;    ///< Knot vektörü
    int  m_degree = 3;                  ///< B-spline derecesi
    bool m_closed = false;

    // De Boor B-spline evaluation at parameter t
    geom::Vec3 DeBoor(double t, double tStart, double tEnd) const;

    template<typename Fn>
    static void TransformPts(std::vector<geom::Vec3>& pts, Fn fn);
};

} // namespace vkt::cad
