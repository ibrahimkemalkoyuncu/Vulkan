/**
 * @file HydraulicSolver.hpp
 * @brief Hidrolik Hesaplama Motoru
 * 
 * Darcy-Weisbach, EN 12056 ve Kritik Devre analizi.
 */

#pragma once

#include "NetworkGraph.hpp"
#include <vector>
#include <unordered_set>

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
    double requiredFlow_m3h = 0.0;      ///< Kaynak çıkış debisi (m³/h)
    std::vector<uint32_t> criticalPath; ///< Kritik devre düğüm listesi
    std::string suggestedPumpModel;     ///< Önerilen pompa modeli
    double suggestedPumpHead_m = 0.0;   ///< Önerilen pompa max yüksekliği (m)
    double suggestedPumpFlow_m3h = 0.0; ///< Önerilen pompa max debisi (m³/h)
    double suggestedPumpPower_kW = 0.0; ///< Önerilen pompa gücü (kW)
};

/**
 * @class HydraulicSolver
 * @brief Hidrolik hesaplama motoru
 * 
 * İki mod destekler:
 * 1. Besleme (Supply) - Darcy-Weisbach / Haaland
 * 2. Drenaj (Drainage) - EN 12056
 */
/**
 * @enum BuildingType
 * @brief Bina tipi — EN 12056-2 K faktörünü belirler
 */
enum class BuildingType {
    Residential,    ///< Konut (K=0.5)
    Hotel,          ///< Otel / Hastane (K=0.7)
    Industrial      ///< Endüstriyel / Laboratuvar (K=1.0)
};

/**
 * @enum HydroNorm
 * @brief Hesap normu — besleme debisi hesaplama yöntemi
 */
enum class HydroNorm {
    EN806_3,    ///< TS EN 806-3: Q = 0.682 * LU^0.45  (l/s)  (default)
    DIN1988     ///< DIN 1988-300: eşzamanlılık faktörü ile Q = φ * √(ΣLU) (l/s)
};

class HydraulicSolver {
public:
    explicit HydraulicSolver(NetworkGraph& network);

    /// Bina tipini ayarla (EN 12056-2 K faktörü için)
    void SetBuildingType(BuildingType type);
    BuildingType GetBuildingType() const { return m_buildingType; }

    /// Hesap normunu ayarla (EN 806-3 veya DIN 1988-300)
    void SetNorm(HydroNorm norm) { m_norm = norm; }
    HydroNorm GetNorm() const { return m_norm; }

    /// Global norm — tüm oturumda kullanılır (singleton gibi)
    static HydroNorm& GlobalNorm() { static HydroNorm n = HydroNorm::EN806_3; return n; }

    /**
     * @brief Besleme şebekesi hesabı (TS EN 806-3)
     *
     * - Topolojik debi dağılımı (leaf→source kümülatif toplama)
     * - TS EN 806-3 LU→debi eğrisi
     * - Darcy-Weisbach / Haaland ile sürtünme kaybı
     * - Zeta ile lokal kayıplar
     */
    void Solve();

    /**
     * @brief Drenaj şebekesi hesabı (EN 12056-2)
     *
     * - DU toplamı (leaf→drain kümülatif)
     * - Manning denklemi ile boru boyutlandırma
     * - K faktörlü debi hesabı
     */
    void SolveDrainage();

    /**
     * @brief Kritik devre analizi (Pompa yüksekliği)
     */
    CriticalPathResult CalculateCriticalPath();

private:
    // Topolojik debi dağılımı
    void DistributeSupplyFlows();
    void DistributeDrainageFlows();

    // TS EN 806-3: hız kısıtına göre minimum çap seçimi (Database'den)
    void AutoSizeSupplyPipes();
    double SelectSupplyPipeDiameter(double flowRate_m3s, const std::string& material) const;

    // Besleme yardımcı fonksiyonlar — TS EN 806-3
    double CalculateFlowFromLU(double loadUnit) const;
    double CalculateHeadLoss(const Edge& edge);
    double HaalandFriction(double Re, double relativeRoughness) const;
    double CalculateLocalLoss(const Edge& edge);

    // Drenaj yardımcı fonksiyonlar — EN 12056-2 + Manning
    double CalculateFlowFromDU(double dischargeUnit) const;
    double GetKFactor() const;
    double SelectDrainPipeDiameter_Manning(double flowRate_Ls, double slope, const std::string& material) const;
    double ManningCapacity(double diameter_m, double slope, double n, double fillRatio) const;
    double GetManningN(const std::string& material) const;

    // Kritik devre — visited set ile DFS
    void DFS(uint32_t nodeId, std::vector<uint32_t>& path,
             std::vector<uint32_t>& bestPath, double& maxLoss, double currentLoss,
             std::unordered_set<uint32_t>& visited);

    // Hardy-Cross — kapalı döngü ağ çözümü
    struct NetworkLoop {
        std::vector<std::pair<uint32_t, int>> edges; // (edgeId, yön: +1 veya -1)
    };
    void SolveHardyCross(const std::vector<NetworkLoop>& loops);
    std::vector<NetworkLoop> DetectLoops() const;
    double ComputeResistance(const Edge& edge) const;

    NetworkGraph& m_network;
    BuildingType m_buildingType = BuildingType::Residential;
    HydroNorm    m_norm         = HydroNorm::EN806_3;
};

} // namespace mep
} // namespace vkt
