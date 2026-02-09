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
    std::string name;           ///< İsim (Lavabo, WC, vs.)
    double loadUnit = 0.5;      ///< Yük birimi (LU)
    double dischargeUnit = 0.5; ///< Boşalım birimi (DU)
    double minPressure_bar = 1.0; ///< Minimum basınç (bar)
    double flowRate_Ls = 0.1;   ///< Nominal debi (L/s)
};

/**
 * @struct PipeData
 * @brief Boru malzeme özellikleri
 */
struct PipeData {
    std::string material;       ///< Malzeme
    double roughness_mm = 0.0015; ///< Pürüzlülük (mm)
    std::vector<double> availableDiameters_mm; ///< Standart çaplar
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

private:
    Database();
    void InitializeFixtures();
    void InitializePipes();
    void InitializeZetas();

    std::map<std::string, FixtureData> m_fixtures;
    std::map<std::string, PipeData> m_pipes;
    std::map<std::string, double> m_zetas;
};

} // namespace mep
} // namespace vkt
