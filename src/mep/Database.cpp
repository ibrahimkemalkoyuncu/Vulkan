/**
 * @file Database.cpp
 * @brief Tesisat Veritabanı İmplementasyonu
 */

#include "mep/Database.hpp"
#include <stdexcept>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    InitializePumps();
    InitializeGasAppliances();
    InitializeRadiators();
}

void Database::InitializeFixtures() {
    // name, LU (EN 806-3), DU (EN 12056-2), SB (TS 825 Sarfiyat Birimi), minP_bar, Q_Ls
    m_fixtures["Lavabo"]           = {"Lavabo",           0.5, 0.5, 0.5, 1.0, 0.10};
    m_fixtures["Duş"]              = {"Duş",              1.0, 1.0, 1.0, 1.5, 0.15};
    m_fixtures["WC"]               = {"WC",               2.0, 2.0, 2.0, 1.0, 0.12};
    m_fixtures["Evye"]             = {"Evye",             1.5, 1.5, 1.0, 1.0, 0.20};
    m_fixtures["Bulaşık Makinesi"] = {"Bulaşık Makinesi", 1.5, 1.5, 2.0, 1.5, 0.15};
    m_fixtures["Çamaşır Makinesi"] = {"Çamaşır Makinesi", 2.0, 2.0, 2.0, 1.5, 0.20};
    m_fixtures["Pisuar"]           = {"Pisuar",           0.5, 0.5, 1.5, 1.0, 0.08};
    m_fixtures["Küvet"]            = {"Küvet",            3.0, 3.0, 2.0, 1.0, 0.30};
    m_fixtures["Bide"]             = {"Bide",             0.5, 0.5, 0.5, 1.0, 0.10};
    m_fixtures["Bidе"]             = {"Bidе",             0.5, 0.5, 0.5, 1.0, 0.10}; // compat alias
}

void Database::InitializePipes() {
    // Boru malzemeleri ve pürüzlülük değerleri
    // Birim fiyatlar TL/m — DN20 referans (2026 yaklaşık piyasa değerleri)
    PipeData pvc;
    pvc.material = "PVC";
    pvc.roughness_mm = 0.0015;
    pvc.unitPrice_TL = 45.0;  // ~45 TL/m DN20
    pvc.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200};
    m_pipes["PVC"] = pvc;

    PipeData pp;
    pp.material = "PP";
    pp.roughness_mm = 0.0015;
    pp.unitPrice_TL = 55.0;   // PPR biraz daha pahalı
    pp.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200};
    m_pipes["PP"] = pp;

    PipeData ppe;  // variable renamed from 'pe' to avoid shadowing
    ppe.material = "PE";
    ppe.roughness_mm = 0.0015;
    ppe.unitPrice_TL = 60.0;
    ppe.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200};
    m_pipes["PE"] = ppe;

    PipeData copper;
    copper.material = "Bakır";
    copper.roughness_mm = 0.0015;
    copper.unitPrice_TL = 220.0; // Bakır çok daha pahalı
    copper.availableDiameters_mm = {12, 15, 18, 22, 28, 35, 42, 54, 76, 108};
    m_pipes["Bakır"] = copper;

    PipeData steel;
    steel.material = "Çelik";
    steel.roughness_mm = 0.045;
    steel.unitPrice_TL = 95.0;
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

void Database::InitializePumps() {
    // Standart sirkülasyon / hidrofar pompaları (head m, flow m³/h, kW)
    m_pumps = {
        {"Wilo Yonos PICO 25/1-4",   4.0,   2.0,  0.04},
        {"Grundfos UPS 25-40",        4.0,   3.5,  0.06},
        {"Wilo Stratos 25/1-8",       8.0,   4.5,  0.08},
        {"Grundfos CM 3-4",          30.0,   3.0,  0.37},
        {"Grundfos CM 5-5",          45.0,   5.0,  0.55},
        {"Wilo MVI 204",             40.0,  20.0,  1.10},
        {"Grundfos CM 10-3",         30.0,  10.0,  0.75},
        {"Wilo MVI 406",             60.0,  40.0,  2.20},
        {"Grundfos CM 25-3",         30.0,  25.0,  1.50},
        {"Wilo MVI 810",             80.0,  80.0,  5.50},
        {"Grundfos CR 10-12",       120.0,  10.0,  5.50},
        {"Wilo MVI 1605",           150.0, 160.0, 15.00},
    };
}

PumpData Database::SuggestPump(double requiredHead_m, double requiredFlow_m3h) const {
    // Katalogdan gerekli yükseklik ve debiyi karşılayan en küçük pompayı seç
    // Sıralama: maxHead_m ve maxFlow_m3h'ye göre ascending (InitializePumps'ta zaten küçükten büyüğe)
    for (const auto& pump : m_pumps) {
        if (pump.maxHead_m >= requiredHead_m && pump.maxFlow_m3h >= requiredFlow_m3h) {
            return pump;
        }
    }
    // Hiçbiri yetmiyorsa en büyüğünü döndür
    return m_pumps.empty() ? PumpData{} : m_pumps.back();
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

// ── Doğal Gaz Cihazları (TS EN 1775 / TS EN 437) ─────────────────────────────
// Hu (alt ısıl değer) = 9.5 kWh/m³ (doğal gaz G20)
// Debi (m³/h) = Güç (kW) / 9.5
void Database::InitializeGasAppliances() {
    // name, kW, m³/h, minP_Pa
    m_gasAppliances["Kombi 24kW"]         = {"Kombi 24kW",         24.0,  2.53, 17.4};
    m_gasAppliances["Kombi 28kW"]         = {"Kombi 28kW",         28.0,  2.95, 17.4};
    m_gasAppliances["Kombi 35kW"]         = {"Kombi 35kW",         35.0,  3.68, 17.4};
    m_gasAppliances["Şofben 11L"]         = {"Şofben 11L",         17.4,  1.83, 17.4};
    m_gasAppliances["Şofben 13L"]         = {"Şofben 13L",         20.9,  2.20, 17.4};
    m_gasAppliances["Ocak (4 gözlü)"]     = {"Ocak (4 gözlü)",     12.0,  1.26, 17.4};
    m_gasAppliances["Ocak + Fırın"]       = {"Ocak + Fırın",       16.0,  1.68, 17.4};
    m_gasAppliances["Doğalgaz Sobası"]    = {"Doğalgaz Sobası",     5.0,  0.53, 17.4};
    m_gasAppliances["Endüstriyel Ocak"]   = {"Endüstriyel Ocak",   60.0,  6.32, 17.4};
    m_gasAppliances["Merkezi Kazan 100kW"]= {"Merkezi Kazan 100kW",100.0,10.53, 17.4};
    m_gasAppliances["Merkezi Kazan 200kW"]= {"Merkezi Kazan 200kW",200.0,21.05, 17.4};
}

GasApplianceData Database::GetGasAppliance(const std::string& name) const {
    auto it = m_gasAppliances.find(name);
    if (it != m_gasAppliances.end()) return it->second;
    GasApplianceData d; d.name = name; d.power_kW = 24.0; d.gasFlow_m3h = 2.53; return d;
}

std::vector<std::string> Database::GetGasApplianceNames() const {
    std::vector<std::string> names;
    for (const auto& [n,_] : m_gasAppliances) names.push_back(n);
    return names;
}

double Database::GetGasDN(double totalFlow_m3h) const {
    // TS EN 1775: max hız 2 m/s, max ΔP=200 Pa/m iç mekan
    // Q (m³/s) = totalFlow_m3h / 3600
    // A = Q/v → D = √(4Q/(π·v))
    const double v_max = 2.0; // m/s
    double Q_m3s = totalFlow_m3h / 3600.0;
    double d_mm = std::sqrt(4.0 * Q_m3s / (M_PI * v_max)) * 1000.0;
    // Standart gaz borusu DN'leri
    static const double kGasDN[] = {15,20,25,32,40,50,65,80,100,0};
    for (int i = 0; kGasDN[i] > 0; ++i)
        if (kGasDN[i] >= d_mm) return kGasDN[i];
    return 100.0;
}

// ── Radyatörler (EN 442 / EN 12831) ──────────────────────────────────────────
// Referans koşul: 70°C gidiş / 50°C dönüş / 20°C oda (ΔT_lm=40K)
void Database::InitializeRadiators() {
    // model, panel, W/m, kg/m
    m_radiators["Panel 11"]  = {"Panel 11",  1, 580,  7.0};
    m_radiators["Panel 21"]  = {"Panel 21",  2, 900,  9.0};
    m_radiators["Panel 22"]  = {"Panel 22",  2,1200, 12.0};
    m_radiators["Panel 33"]  = {"Panel 33",  3,1700, 17.0};
    m_radiators["Döküm 5 Dilim"] = {"Döküm 5 Dilim", 1, 400, 30.0};
    m_radiators["Kolon 2 Bölüm"] = {"Kolon 2 Bölüm", 2, 650, 11.0};
}

RadiatorData Database::GetRadiator(const std::string& model) const {
    auto it = m_radiators.find(model);
    if (it != m_radiators.end()) return it->second;
    RadiatorData d; d.model = model; d.outputPerMeter_W = 1200; return d;
}

std::vector<std::string> Database::GetRadiatorModels() const {
    std::vector<std::string> names;
    for (const auto& [n,_] : m_radiators) names.push_back(n);
    return names;
}

double Database::GetHeatingDN(double flow_Ls) const {
    // Isıtma debi → DN (v_max = 1.5 m/s ısıtma için)
    const double v_max = 1.5;
    double d_mm = std::sqrt(4.0 * (flow_Ls / 1000.0) / (M_PI * v_max)) * 1000.0;
    static const double kDN[] = {15,20,25,32,40,50,65,80,100,0};
    for (int i = 0; kDN[i] > 0; ++i)
        if (kDN[i] >= d_mm) return kDN[i];
    return 100.0;
}

double Database::GetFireDN(double flow_Ls) const {
    // Yangın borusu: v_max = 5.0 m/s (EN 12845)
    const double v_max = 5.0;
    double d_mm = std::sqrt(4.0 * (flow_Ls / 1000.0) / (M_PI * v_max)) * 1000.0;
    static const double kDN[] = {25,32,40,50,65,80,100,150,200,0};
    for (int i = 0; kDN[i] > 0; ++i)
        if (kDN[i] >= d_mm) return kDN[i];
    return 200.0;
}

} // namespace mep
} // namespace vkt
