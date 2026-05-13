#pragma once

#include "mep/NetworkGraph.hpp"
#include "cad/Entity.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace vkt {
namespace mep {

/**
 * @enum ClashSeverity
 * @brief Çakışma şiddeti
 */
enum class ClashSeverity {
    Hard,      ///< Fiziksel çakışma (iki nesne iç içe)
    Soft,      ///< Tolerans çakışması (yetersiz clearance mesafesi)
    Ignored    ///< Kullanıcı tarafından yoksayılmış
};

/**
 * @struct ClashResult
 * @brief Tespit edilen tek bir çakışma kaydı
 */
struct ClashResult {
    uint32_t id = 0;
    ClashSeverity severity = ClashSeverity::Hard;

    uint32_t edgeId           = 0;  ///< MEP boru kenar ID'si
    cad::EntityId architecturalId = 0; ///< Mimari CAD entity ID'si

    geom::Vec3 clashPoint;           ///< Dünya koordinatında çakışma noktası
    double overlapAmount_mm = 0.0;   ///< Çakışma derinliği (mm)

    std::string description;
};

/**
 * @class ClashEngine
 * @brief MEP boruları ile mimari CAD entity'leri arasındaki çakışmaları tespit eder.
 *
 * Algoritma:
 * 1. Her boru (Edge) için AABB hesapla (çap + tolerans genişletmesi dahil)
 * 2. Her mimari entity ile AABB hızlı kesişim testi
 * 3. Gerçek kesişim için segment–box testi
 * 4. Hard (fiziksel) ve Soft (tolerans ihlali) çakışmaları raporla
 */
class ClashEngine {
public:
    /**
     * @param network MEP şebeke grafiği (okunur/yazılır)
     * @param staticEntities Mimari CAD entityleri (duvarlar, döşemeler vb.)
     */
    ClashEngine(NetworkGraph& network,
                const std::vector<std::unique_ptr<cad::Entity>>& staticEntities);

    /**
     * @brief Tüm şebekeyi statik elemanlara karşı tarar
     * @return Tespit edilen çakışmaların listesi
     */
    std::vector<ClashResult> RunAnalysis();

    /**
     * @brief Soft-clash tolerans mesafesini ayarla
     * @param mm Tolerans (mm), varsayılan 50mm
     */
    void SetTolerance(double mm) { m_tolerance = mm; }

    double GetTolerance() const { return m_tolerance; }

private:
    bool CheckIntersection(const Edge& pipe, const cad::Entity* arch, ClashResult& result);

    NetworkGraph& m_network;
    const std::vector<std::unique_ptr<cad::Entity>>& m_archEntities;
    double m_tolerance = 50.0; // mm
};

} // namespace mep
} // namespace vkt
