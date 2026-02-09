/**
 * @file HydraulicSolver.hpp
 * @brief Hidrolik Hesaplama Motoru
 * 
 * Darcy-Weisbach, EN 12056 ve Kritik Devre analizi.
 */

#pragma once

#include "NetworkGraph.hpp"
#include <vector>

namespace vkt {
namespace mep {

/**
 * @struct CriticalPathResult
 * @brief Kritik devre analiz sonucu
 */
struct CriticalPathResult {
    uint32_t disadvantagedNodeId = 0;   ///< En dezavantajlı düğüm
    double totalHeadLoss_m = 0.0;       ///< Toplam kayıp (m)
    double requiredPumpHead_m = 0.0;    ///< Gerekli pompa yüksekliği (m)
    std::vector<uint32_t> criticalPath; ///< Kritik devre düğüm listesi
};

/**
 * @class HydraulicSolver
 * @brief Hidrolik hesaplama motoru
 * 
 * İki mod destekler:
 * 1. Besleme (Supply) - Darcy-Weisbach / Haaland
 * 2. Drenaj (Drainage) - EN 12056
 */
class HydraulicSolver {
public:
    explicit HydraulicSolver(NetworkGraph& network);
    
    /**
     * @brief Besleme şebekesi hesabı (TS EN 806-3)
     * 
     * - LoadUnit'lerden debi hesabı
     * - Darcy-Weisbach / Haaland ile sürtünme kaybı
     * - Zeta ile lokal kayıplar
     */
    void Solve();
    
    /**
     * @brief Drenaj şebekesi hesabı (EN 12056)
     * 
     * - DU (Discharge Unit) toplamı
     * - Eğim ve çapa göre boru boyutlandırma
     */
    void SolveDrainage();
    
    /**
     * @brief Kritik devre analizi (Pompa yüksekliği)
     * 
     * En uzun / en kayıplı yolu bularak gerekli pompa başını hesaplar.
     */
    CriticalPathResult CalculateCriticalPath();
    
private:
    // Besleme yardımcı fonksiyonlar
    double CalculateFlowFromLU(double loadUnit) const;
    double CalculateHeadLoss(const Edge& edge);
    double HaalandFriction(double Re, double relativeRoughness) const;
    double CalculateLocalLoss(const Edge& edge);
    
    // Drenaj yardımcı fonksiyonlar
    double CalculateFlowFromDU(double dischargeUnit) const;
    double SelectDrainPipeDiameter(double flowRate_Ls, double slope) const;
    
    // Kritik devre yardımcı fonksiyonlar
    void DFS(uint32_t nodeId, std::vector<uint32_t>& path, 
             std::vector<uint32_t>& bestPath, double& maxLoss, double currentLoss);

    NetworkGraph& m_network;
};

} // namespace mep
} // namespace vkt
