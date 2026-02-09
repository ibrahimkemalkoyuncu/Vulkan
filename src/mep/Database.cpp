/**
 * @file Database.cpp
 * @brief Tesisat Veritabanı İmplementasyonu
 */

#include "mep/Database.hpp"
#include <stdexcept>

namespace vkt {
namespace mep {

Database& Database::Instance() {
    static Database instance;
    return instance;
}

Database::Database() {
    InitializeFixtures();
    InitializePipes();
    InitializeZetas();
}

void Database::InitializeFixtures() {
    // TS EN 806-3 Standart armatürler
    m_fixtures["Lavabo"] = {"Lavabo", 0.5, 0.5, 1.0, 0.1};
    m_fixtures["Duş"] = {"Duş", 1.0, 1.0, 1.5, 0.15};
    m_fixtures["WC"] = {"WC", 2.0, 2.0, 1.0, 0.12};
    m_fixtures["Evye"] = {"Evye", 1.5, 1.5, 1.0, 0.2};
    m_fixtures["Bulaşık Makinesi"] = {"Bulaşık Makinesi", 1.5, 1.5, 1.5, 0.15};
    m_fixtures["Çamaşır Makinesi"] = {"Çamaşır Makinesi", 2.0, 2.0, 1.5, 0.2};
    m_fixtures["Pisuar"] = {"Pisuar", 0.5, 0.5, 1.0, 0.08};
    m_fixtures["Bidе"] = {"Bidе", 0.5, 0.5, 1.0, 0.1};
}

void Database::InitializePipes() {
    // Boru malzemeleri ve pürüzlülük değerleri
    PipeData pvc;
    pvc.material = "PVC";
    pvc.roughness_mm = 0.0015;
    pvc.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200};
    m_pipes["PVC"] = pvc;

    PipeData pp;
    pp.material = "PP";
    pp.roughness_mm = 0.0015;
    pp.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200};
    m_pipes["PP"] = pp;

    PipeData pe;
    pe.material = "PE";
    pe.roughness_mm = 0.0015;
    pe.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200};
    m_pipes["PE"] = pe;

    PipeData copper;
    copper.material = "Bakır";
    copper.roughness_mm = 0.0015;
    copper.availableDiameters_mm = {12, 15, 18, 22, 28, 35, 42, 54, 76, 108};
    m_pipes["Bakır"] = copper;

    PipeData steel;
    steel.material = "Çelik";
    steel.roughness_mm = 0.045;
    steel.availableDiameters_mm = {15, 20, 25, 32, 40, 50, 65, 80, 100, 125, 150, 200};
    m_pipes["Çelik"] = steel;
}

void Database::InitializeZetas() {
    // Lokal kayıp katsayıları (Standart değerler)
    m_zetas["90° Dirsek"] = 0.9;
    m_zetas["45° Dirsek"] = 0.4;
    m_zetas["T-Parça (Ana Hat)"] = 0.2;
    m_zetas["T-Parça (Yan Kollama)"] = 1.3;
    m_zetas["Vana (Küresel)"] = 0.1;
    m_zetas["Vana (Kelebek)"] = 0.3;
    m_zetas["Vana (Sürgülü)"] = 0.2;
    m_zetas["İndirgeme"] = 0.5;
    m_zetas["Genişleme"] = 1.0;
    m_zetas["Giriş (Keskin)"] = 0.5;
    m_zetas["Giriş (Yuvarlatılmış)"] = 0.05;
    m_zetas["Çıkış"] = 1.0;
}

FixtureData Database::GetFixture(const std::string& name) const {
    auto it = m_fixtures.find(name);
    if (it != m_fixtures.end()) {
        return it->second;
    }
    throw std::runtime_error("Armatür bulunamadı: " + name);
}

std::vector<std::string> Database::GetFixtureNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_fixtures) {
        names.push_back(pair.first);
    }
    return names;
}

PipeData Database::GetPipe(const std::string& material) const {
    auto it = m_pipes.find(material);
    if (it != m_pipes.end()) {
        return it->second;
    }
    throw std::runtime_error("Boru malzemesi bulunamadı: " + material);
}

std::vector<std::string> Database::GetPipeMaterials() const {
    std::vector<std::string> materials;
    for (const auto& pair : m_pipes) {
        materials.push_back(pair.first);
    }
    return materials;
}

double Database::GetZeta(const std::string& fittingType) const {
    auto it = m_zetas.find(fittingType);
    if (it != m_zetas.end()) {
        return it->second;
    }
    return 0.0; // Varsayılan
}

std::vector<std::string> Database::GetFittingTypes() const {
    std::vector<std::string> types;
    for (const auto& pair : m_zetas) {
        types.push_back(pair.first);
    }
    return types;
}

} // namespace mep
} // namespace vkt
