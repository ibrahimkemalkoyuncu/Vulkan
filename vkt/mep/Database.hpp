/**
 * @file Database.hpp
 * @brief Tesisat Veritabanı
 * 
 * Armatür, boru ve yerel kayıp katsayıları tabloları.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace vkt {
namespace mep {

/**
 * @struct FixtureData
 * @brief Armatür özellikleri
 */
struct FixtureData {
    std::string name;               ///< İsim (Lavabo, WC, vs.)
    double loadUnit = 0.5;          ///< Yük birimi (LU) — EN 806-3 / DIN 1988-300
    double dischargeUnit = 0.5;     ///< Boşalım birimi (DU) — EN 12056-2
    double sarfiyat_unit = 0.5;     ///< Sarfiyat Birimi (SB) — TS 825 Musluk Birimi yöntemi
    double minPressure_bar = 1.0;   ///< Minimum basınç (bar)
    double flowRate_Ls = 0.1;       ///< Nominal debi (L/s)
};

/**
 * @struct PipeData
 * @brief Boru malzeme özellikleri
 */
struct PipeData {
    std::string material;       ///< Malzeme
    double roughness_mm = 0.0015; ///< Pürüzlülük (mm)
    std::vector<double> availableDiameters_mm; ///< Standart çaplar
    double unitPrice_TL = 0.0; ///< Birim fiyat (TL/m) — DN20 referans
};

/**
 * @struct ZetaData
 * @brief Lokal kayıp katsayıları
 */
struct ZetaData {
    std::string type;           ///< Tip (90° Dirsek, T-Parça, vs.)
    double zeta = 0.0;          ///< Kayıp katsayısı
};

/**
 * @struct PumpData
 * @brief Pompa katalog verisi
 */
struct PumpData {
    std::string model;          ///< Model adı
    std::string brand;          ///< Marka (Grundfos, Wilo, vb.)
    std::string category;       ///< Kategori (Sirkülasyon, Hidrofor, Dikey Milli, Paket Hidrofor, Yangın)
    double maxHead_m  = 0.0;    ///< Maksimum yükseklik (m)
    double maxFlow_m3h = 0.0;   ///< Maksimum debi (m³/h)
    double ratedPower_kW = 0.0; ///< Anma gücü (kW)
};

/**
 * @struct GasApplianceData
 * @brief Gaz cihazı kataloğu (TS EN 1775)
 */
struct GasApplianceData {
    std::string name;           ///< Cihaz adı (Kombi, Ocak, Şofben, Soba)
    double power_kW = 0.0;      ///< Toplam gaz gücü (kW)
    double gasFlow_m3h = 0.0;   ///< Saat debisi (m³/h) — power / Hu
    double minPressure_Pa = 17.4; ///< Min bağlantı basıncı (Pa) — TS EN 1775 L.P.
};

/**
 * @struct RadiatorData
 * @brief Radyatör kataloğu (EN 442)
 */
struct RadiatorData {
    std::string model;          ///< Model (Panel 11, 22, Kolon, Döküm)
    int panelCount = 1;         ///< Panel sayısı
    double outputPerMeter_W = 0.0; ///< 1 m boy için çıktı (W) — 70/50/20°C
    double massPerMeter_kg = 0.0;  ///< Birim ağırlık (kg/m) — sistem hacmi için
};

/**
 * @struct FanData
 * @brief Fan katalog verisi (Systemair, Rosenberg, ebm-papst)
 */
struct FanData {
    std::string model;
    std::string brand;
    double maxAirflow_Ls = 0;   ///< Maksimum hava debisi (L/s)
    double maxPressure_Pa = 0;  ///< Maksimum basınç (Pa)
    double power_kW = 0;        ///< Motor gücü (kW)
    double SFP = 0;             ///< Specific Fan Power (W/(L/s))
};

/**
 * @class Database
 * @brief Tesisat veritabanı yönetimi
 * 
 * Standart armatür, boru ve fitting verilerini sağlar.
 */
class Database {
public:
    static Database& Instance();
    
    // Armatür verileri
    FixtureData GetFixture(const std::string& name) const;
    std::vector<std::string> GetFixtureNames() const;
    
    // Boru verileri
    PipeData GetPipe(const std::string& material) const;
    std::vector<std::string> GetPipeMaterials() const;
    
    // Lokal kayıp katsayıları
    double GetZeta(const std::string& fittingType) const;
    std::vector<std::string> GetFittingTypes() const;

    // Pompa kataloğu — gerekli yükseklik ve debiye göre en küçük uygun pompa
    PumpData SuggestPump(double requiredHead_m, double requiredFlow_m3h) const;
    const std::vector<PumpData>& GetPumpCatalog() const { return m_pumps; }

    // Gaz cihazları kataloğu (TS EN 1775)
    GasApplianceData GetGasAppliance(const std::string& name) const;
    std::vector<std::string> GetGasApplianceNames() const;
    double GetGasDN(double totalFlow_m3h) const; ///< Debi → standart gaz borusu DN

    // Radyatör kataloğu (EN 442)
    RadiatorData GetRadiator(const std::string& model) const;
    std::vector<std::string> GetRadiatorModels() const;
    double GetHeatingDN(double flow_Ls) const; ///< Isıtma debisi → DN

    // Yangın borusu DN (EN 12845)
    double GetFireDN(double flow_Ls) const;

    // Fan kataloğu (Systemair, Rosenberg, ebm-papst)
    const std::vector<FanData>& GetFanCatalog() const { return m_fans; }
    FanData SuggestFan(double airflow_Ls, double pressure_Pa) const;

private:
    Database();
    void InitializeFixtures();
    void InitializePipes();
    void InitializeZetas();
    void InitializePumps();
    void InitializeGasAppliances();
    void InitializeRadiators();
    void InitializeFans();

    std::map<std::string, FixtureData>       m_fixtures;
    std::map<std::string, PipeData>          m_pipes;
    std::map<std::string, double>            m_zetas;
    std::vector<PumpData>                    m_pumps;
    std::map<std::string, GasApplianceData>  m_gasAppliances;
    std::map<std::string, RadiatorData>      m_radiators;
    std::vector<FanData>                     m_fans;
};

} // namespace mep
} // namespace vkt
