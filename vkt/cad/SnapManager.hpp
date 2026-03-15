/**
 * @file SnapManager.hpp
 * @brief Snap (Yakalama) Sistemi — CAD çizim hassasiyeti
 *
 * Snap Tipleri:
 * - Endpoint: Çizgi/arc başlangıç ve bitiş noktaları
 * - Midpoint: Çizgi/arc orta noktası
 * - Center: Daire/arc merkezi
 * - Intersection: Entity kesişim noktaları
 * - Perpendicular: Dik iz düşüm
 * - Nearest: Entity üzerindeki en yakın nokta
 * - Grid: Belirli grid aralıklarına snap
 */

#pragma once

#include "cad/Entity.hpp"
#include "geom/Math.hpp"
#include <vector>
#include <cstdint>

namespace vkt {
namespace cad {

/// Snap türleri
enum class SnapType {
    None = 0,
    Endpoint,       ///< Uç noktalar
    Midpoint,       ///< Orta nokta
    Center,         ///< Merkez (Circle/Arc)
    Intersection,   ///< Entity kesişim noktaları
    Perpendicular,  ///< Dik iz düşüm
    Nearest,        ///< En yakın nokta
    Grid            ///< Grid snap
};

/// Snap sonucu
struct SnapResult {
    geom::Vec3 point{0, 0, 0};   ///< Yakalanan nokta
    SnapType type = SnapType::None; ///< Snap türü
    EntityId entityId = 0;        ///< İlgili entity (varsa)
    double distance = 1e9;        ///< Mouse'a uzaklık (piksel)

    bool IsValid() const { return type != SnapType::None; }
};

/**
 * @class SnapManager
 * @brief Snap noktası bulma yöneticisi
 */
class SnapManager {
public:
    SnapManager();

    /// Aktif snap türlerini ayarla
    void SetActiveSnaps(const std::vector<SnapType>& snaps);
    void EnableSnap(SnapType type);
    void DisableSnap(SnapType type);
    bool IsSnapEnabled(SnapType type) const;
    void EnableAll();
    void DisableAll();

    /// Grid snap aralığı (mm cinsinden, varsayılan 100mm)
    void SetGridSpacing(double spacing) { m_gridSpacing = spacing; }
    double GetGridSpacing() const { return m_gridSpacing; }

    /// Snap aperture (piksel cinsinden yakalama mesafesi)
    void SetAperture(double aperture) { m_aperture = aperture; }
    double GetAperture() const { return m_aperture; }

    /**
     * @brief Verilen dünya koordinatı etrafında snap noktası bul
     * @param worldPos Mouse'un dünya koordinatı
     * @param entities Snap yapılacak entity'ler
     * @param viewScale Viewport ölçeği (piksel/birim) — aperture dönüşümü için
     * @return En yakın snap noktası
     */
    SnapResult FindSnapPoint(const geom::Vec3& worldPos,
                             const std::vector<Entity*>& entities,
                             double viewScale = 1.0) const;

private:
    uint32_t m_activeSnaps = 0; ///< Bit flags
    double m_gridSpacing = 0.1; ///< 100mm = 0.1m
    double m_aperture = 10.0;   ///< Piksel cinsinden yakalama mesafesi

    // Snap hesaplama yardımcıları
    void FindEndpointSnaps(const Entity* entity, const geom::Vec3& worldPos,
                           double tolerance, std::vector<SnapResult>& results) const;
    void FindMidpointSnaps(const Entity* entity, const geom::Vec3& worldPos,
                           double tolerance, std::vector<SnapResult>& results) const;
    void FindCenterSnaps(const Entity* entity, const geom::Vec3& worldPos,
                         double tolerance, std::vector<SnapResult>& results) const;
    void FindNearestSnaps(const Entity* entity, const geom::Vec3& worldPos,
                          double tolerance, std::vector<SnapResult>& results) const;
    void FindGridSnap(const geom::Vec3& worldPos, double tolerance,
                      std::vector<SnapResult>& results) const;

    static uint32_t SnapTypeToBit(SnapType type);
};

} // namespace cad
} // namespace vkt
