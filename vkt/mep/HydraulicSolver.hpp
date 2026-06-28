/**
 * @file HydraulicSolver.hpp
 * @brief Hidrolik Hesaplama Motoru
 * 
 * Darcy-Weisbach, EN 12056 ve Kritik Devre analizi.
 */

#pragma once

#include "NetworkGraph.hpp"
#include <vector>
#include <array>
#include <unordered_set>

namespace vkt { namespace core { class FloorManager; } }

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
    Hospital,       ///< Hastane (K=0.7, DIN a/b/c farklı)
    School,         ///< Okul / Yurt (K=0.5)
    Office,         ///< Ofis / Ticari (K=0.5)
    Industrial      ///< Endüstriyel / Laboratuvar (K=1.0)
};

/**
 * @enum HydroNorm
 * @brief Hesap normu — besleme debisi hesaplama yöntemi
 */
enum class HydroNorm {
    EN806_3,    ///< TS EN 806-3: Q = 0.682 * LU^0.45  (l/s)  (default)
    DIN1988,    ///< DIN 1988-300: eşzamanlılık faktörü ile Q = φ * √(ΣLU) (l/s)
    SARFIYAT    ///< TS 825 Musluk Birimi: Q = K_s * √(ΣSB) (l/s) — Türkiye kamu ihalelerinde kullanılır
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

    /// Su sıcaklığını ayarla (viskozite/yoğunluk hesabı için, default 20°C)
    void SetWaterTemperature(double tempC) { m_waterTempC = tempC; }
    double GetWaterTemperature() const { return m_waterTempC; }

    /// Boru yaşı (yıl) — pürüzlülük artışı modeli (0 = yeni boru)
    void SetPipeAge(double years) { m_pipeAgeYears = years; }
    double GetPipeAge() const { return m_pipeAgeYears; }

    /// Global norm — tüm oturumda kullanılır (singleton gibi)
    static HydroNorm& GlobalNorm() { static HydroNorm n = HydroNorm::EN806_3; return n; }

    /// Son çözüm sırasında oluşan uyarı/hata mesajları
    const std::vector<std::string>& GetWarnings() const { return m_warnings; }
    const std::vector<std::string>& GetErrors()   const { return m_errors; }
    bool HasErrors() const { return !m_errors.empty(); }

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
     * @brief Doğal gaz şebekesi hesabı (TS EN 1775)
     * - GasAppliance → m³/h debi (güç / Hu)
     * - Darcy-Weisbach gaz için (ρ_gas=0.75 kg/m³)
     * - Max hız 2 m/s, max ΔP 200 Pa iç mekan
     * - DN seçimi GetGasDN() ile
     */
    void SolveGas();

    /**
     * @brief Isıtma sistemi hesabı (EN 12831)
     * - Radyatör → kW yük → m_flow (l/s) = Q/(cp×ΔT)
     * - Gidiş/dönüş fark ΔT=20K (70°C/50°C)
     * - Darcy-Weisbach su ile (ρ=1000 kg/m³)
     * - DN seçimi GetHeatingDN() ile
     */
    void SolveHeating();

    /**
     * @brief Yangın / Sprinkler hesabı (EN 12845 yoğunluk-alan metodu)
     * - Tehlike sınıfına göre yoğunluk ve tasarım alanı
     * - Q_sistem = yoğunluk × alan → tüm FireLine edge'lere dağıt
     * - Min basınç 5 bar yangın pompasında
     * - DN seçimi GetFireDN() ile
     */
    void SolveFire(const std::string& hazardClass = "OH1");

    /**
     * @brief Elektrik tesisat hesabı (IEC 60364)
     * - Kablo kesiti: I = P / (V × cosφ)
     * - Gerilim düşümü: ΔV = 2 × I × R × L
     * - Sigorta uyumu
     */
    void SolveElectric();

    /**
     * @brief Havalandırma / HVAC kanal hesabı (EN 15665)
     * - Kişi başı hava debisi: 10 L/s (ofis), 8 L/s (konut)
     * - Kanal boyutlandırma: Q = v × A, v_max = 5 m/s
     * - Basınç kaybı: ΔP = f × (L/D_h) × (ρv²/2)
     */
    void SolveVentilation();

    /**
     * @brief Kritik devre analizi (Pompa yüksekliği)
     */
    CriticalPathResult CalculateCriticalPath();

    // ═══════════════════════════════════════════════════════════
    //  HVAC ENGINE — Soğutma Yükü / Psikrometri / ERV
    // ═══════════════════════════════════════════════════════════

    /**
     * @struct CoolingLoadResult
     * @brief ASHRAE CLTD yöntemiyle soğutma yükü bileşenleri
     */
    struct CoolingLoadResult {
        double Q_wall_W = 0;       ///< Duvar/çatı iletim yükü (W)
        double Q_glass_W = 0;      ///< Cam güneş yükü — SHGC (W)
        double Q_internal_W = 0;   ///< İç yükler: kişi + ekipman + aydınlatma (W)
        double Q_ventilation_W = 0;///< Taze hava yükü (W)
        double Q_total_W = 0;      ///< Toplam soğutma yükü (W)
    };

    /**
     * @brief ASHRAE CLTD yöntemiyle soğutma yükü hesabı
     */
    CoolingLoadResult CalculateCoolingLoad(double area_m2, double wallU, double roofU,
        double glassArea_m2, double SHGC, int numPeople, double equipW,
        double lightWm2, double outdoorTempC, double indoorTempC) const;

    /**
     * @struct PsychrometricState
     * @brief Psikrometrik durum noktası
     */
    struct PsychrometricState {
        double T_db = 20.0;     ///< Kuru termometre sıcaklığı (°C)
        double T_wb = 14.0;     ///< Yaş termometre sıcaklığı (°C)
        double RH = 0.50;       ///< Bağıl nem (0-1)
        double W = 0.0073;      ///< Nem oranı (kg/kg)
        double h = 38.5;        ///< Entalpi (kJ/kg)
        double v = 0.84;        ///< Özgül hacim (m³/kg)
        double T_dp = 9.3;      ///< Çiğ noktası sıcaklığı (°C)
    };

    /**
     * @brief Psikrometrik hesaplama (Antoine denklemi tabanlı)
     * @param T_db Kuru termometre sıcaklığı (°C)
     * @param RH Bağıl nem (0-1)
     * @param P_atm Atmosfer basıncı (Pa), default 101325
     */
    static PsychrometricState CalcPsychrometric(double T_db, double RH, double P_atm = 101325.0);

    /**
     * @struct ERVResult
     * @brief Enerji Geri Kazanım Ventilasyon sonucu
     */
    struct ERVResult {
        double sensibleEff = 0.75;      ///< Duyulabilir etkinlik
        double latentEff = 0.65;        ///< Gizli ısı etkinliği
        double heatRecovered_W = 0;     ///< Geri kazanılan ısı (W)
        double moistureRecovered_gs = 0;///< Geri kazanılan nem (g/s)
    };

    /**
     * @brief ERV (Enerji Geri Kazanım) hesabı
     */
    ERVResult CalculateERV(double airflow_Ls, double outdoorT, double outdoorRH,
        double indoorT, double indoorRH, double sensEff = 0.75, double latEff = 0.65) const;

    // ═══════════════════════════════════════════════════════════
    //  HVAC Gürültü Analizi (ASHRAE Handbook Ch.48)
    // ═══════════════════════════════════════════════════════════

    struct NoiseResult {
        double Lw_dB = 0;            ///< Ses güç seviyesi (dB)
        double Lp_room_dB = 0;       ///< Oda içi ses basınç seviyesi (dB)
        double NC_rating = 0;        ///< Noise Criteria derecesi
        std::string assessment;       ///< "Kabul edilebilir" / "Yüksek" / "Çok yüksek"
    };

    NoiseResult CalculateDuctNoise(double airflow_Ls, double velocity_ms,
        double ductArea_m2, double roomVolume_m3, double roomAbsorption = 0.2) const;

    // ═══════════════════════════════════════════════════════════
    //  Basit Enerji Simülasyonu (Aylık bin-method)
    // ═══════════════════════════════════════════════════════════

    struct MonthlyEnergy {
        double heating_kWh = 0;
        double cooling_kWh = 0;
        double hotWater_kWh = 0;
        double fanPump_kWh = 0;
        double total_kWh = 0;
    };
    struct AnnualEnergyResult {
        std::array<MonthlyEnergy, 12> months;
        double annualHeating_kWh = 0;
        double annualCooling_kWh = 0;
        double annualTotal_kWh = 0;
        double EUI_kWh_m2 = 0;       ///< Energy Use Intensity
    };

    AnnualEnergyResult CalculateAnnualEnergy(
        double floorArea_m2, double envelopeUA_WK, double ventFlow_Ls,
        int numOccupants, double hotWaterLpd = 50.0,
        double heatingSetpoint = 21.0, double coolingSetpoint = 26.0) const;

    // ═══════════════════════════════════════════════════════════
    //  Mevzuat Uyum Raporu
    // ═══════════════════════════════════════════════════════════

    struct ComplianceCheck {
        std::string standard;       ///< "TS EN 806-3", "EN 12056-2" vb.
        std::string requirement;    ///< "Min basınç 1.0 bar"
        bool passed = true;
        std::string detail;         ///< "Tüm fixture'larda basınç >= 1.0 bar"
    };

    std::vector<ComplianceCheck> CheckCompliance() const;

    // ═══════════════════════════════════════════════════════════
    //  Kat Bazlı Statik Basınç Raporu
    // ═══════════════════════════════════════════════════════════

    /**
     * @struct FloorPressureResult
     * @brief Kat bazlı basınç analiz sonucu
     */
    struct FloorPressureResult {
        int floorIndex = 0;
        std::string floorName;
        double elevation_m = 0;
        double staticPressure_mSS = 0;   ///< ρgh statik basınç (m su sütunu)
        double frictionLoss_mSS = 0;     ///< Sürtünme kaybı (m su sütunu)
        double requiredPressure_mSS = 0; ///< Gerekli toplam basınç (m su sütunu)
        int nodeCount = 0;
        int edgeCount = 0;
    };

    /**
     * @brief Kat bazlı statik basınç raporu
     */
    std::vector<FloorPressureResult> CalculateFloorPressures(const core::FloorManager& fm) const;

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

    // Boru yaşlanma — pürüzlülük artış faktörü
    double AgingRoughnessFactor() const;

public:
    // DN-bağımlı fitting K değeri (çap arttıkça K düşer) — test erişimi için public
    double FittingK_Elbow90(double dn_mm) const;
    double FittingK_Tee(double dn_mm) const;
    double WaterDensity() const;
    double WaterKinematicViscosity() const;

private:

    // Kritik devre — Dijkstra (priority queue) ile max direnç yolu
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

    // Newton-Raphson — çok dallı karmaşık ağ çözümü
    // Darcy-Weisbach head loss + nodal mass balance
    // Jacobian matrisi: ∂F/∂Q (debi düzeltme), ∂F/∂H (basınç düzeltme)
    void SolveNewtonRaphson(int maxIter = 50, double tolerance = 1e-6);
    // Bir edge'in Q→ΔH fonksiyonu ve türevi (Jacobian bileşeni)
    double EdgeHeadLossFunc(const Edge& edge, double Q_m3s) const;
    double EdgeHeadLossDerivative(const Edge& edge, double Q_m3s) const;

    NetworkGraph& m_network;
    BuildingType m_buildingType = BuildingType::Residential;
    HydroNorm    m_norm         = HydroNorm::EN806_3;
    double       m_waterTempC   = 20.0;
    double       m_pipeAgeYears = 0.0;
    std::vector<std::string> m_warnings;
    std::vector<std::string> m_errors;
};

} // namespace mep
} // namespace vkt
