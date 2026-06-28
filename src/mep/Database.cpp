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
    InitializeFans();
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

    // PEX-a (çapraz bağlı polietilen — yerden ısıtma, modern konut)
    PipeData pexa;
    pexa.material = "PEX-a";
    pexa.roughness_mm = 0.007;
    pexa.unitPrice_TL = 70.0;
    pexa.availableDiameters_mm = {12, 16, 20, 25, 32, 40, 50, 63};
    m_pipes["PEX-a"] = pexa;

    // PPR (polipropilen random — sıcak/soğuk su, yaygın)
    PipeData ppr;
    ppr.material = "PPR";
    ppr.roughness_mm = 0.007;
    ppr.unitPrice_TL = 50.0;
    ppr.availableDiameters_mm = {20, 25, 32, 40, 50, 63, 75, 90, 110};
    m_pipes["PPR"] = ppr;

    // Galvaniz Çelik (eski bina renovasyon)
    PipeData galv;
    galv.material = "Galvaniz";
    galv.roughness_mm = 0.15;
    galv.unitPrice_TL = 110.0;
    galv.availableDiameters_mm = {15, 20, 25, 32, 40, 50, 65, 80, 100, 125, 150};
    m_pipes["Galvaniz"] = galv;

    // Paslanmaz Çelik 304 (hastane, gıda tesisi)
    PipeData ss304;
    ss304.material = "Paslanmaz 304";
    ss304.roughness_mm = 0.015;
    ss304.unitPrice_TL = 350.0;
    ss304.availableDiameters_mm = {15, 20, 25, 32, 40, 50, 65, 80, 100};
    m_pipes["Paslanmaz 304"] = ss304;

    // HDPE (yüksek yoğunluklu polietilen — dış hat, altyapı)
    PipeData hdpe;
    hdpe.material = "HDPE";
    hdpe.roughness_mm = 0.007;
    hdpe.unitPrice_TL = 55.0;
    hdpe.availableDiameters_mm = {20, 25, 32, 40, 50, 63, 75, 90, 110, 125, 160, 200, 250, 315};
    m_pipes["HDPE"] = hdpe;

    // PE-RT (yükseltilmiş sıcaklık polietilen — yerden ısıtma)
    PipeData pert;
    pert.material = "PE-RT";
    pert.roughness_mm = 0.007;
    pert.unitPrice_TL = 65.0;
    pert.availableDiameters_mm = {12, 16, 20, 25, 32};
    m_pipes["PE-RT"] = pert;

    // Kompozit (çok katmanlı — PEX/Al/PEX)
    PipeData composite;
    composite.material = "Kompozit";
    composite.roughness_mm = 0.007;
    composite.unitPrice_TL = 85.0;
    composite.availableDiameters_mm = {16, 20, 25, 32, 40, 50, 63};
    m_pipes["Kompozit"] = composite;

    // Pik Döküm (drenaj, yangın)
    PipeData castIron;
    castIron.material = "Pik Döküm";
    castIron.roughness_mm = 0.26;
    castIron.unitPrice_TL = 180.0;
    castIron.availableDiameters_mm = {50, 75, 100, 125, 150, 200, 250, 300};
    m_pipes["Pik Döküm"] = castIron;

    // Tıbbi Bakır (EN 13348 — hastane gaz, medikal)
    PipeData medCopper;
    medCopper.material = "Tıbbi Bakır";
    medCopper.roughness_mm = 0.0015;
    medCopper.unitPrice_TL = 280.0;
    medCopper.availableDiameters_mm = {12, 15, 18, 22, 28, 35, 42, 54};
    m_pipes["Tıbbi Bakır"] = medCopper;

    // Paslanmaz Çelik 316L (deniz suyu, kimya tesisi)
    PipeData ss316;
    ss316.material = "Paslanmaz 316L";
    ss316.roughness_mm = 0.015;
    ss316.unitPrice_TL = 450.0;
    ss316.availableDiameters_mm = {15, 20, 25, 32, 40, 50, 65, 80, 100};
    m_pipes["Paslanmaz 316L"] = ss316;

    // ── HVAC Kanal Malzemeleri ──
    PipeData galvDuct;
    galvDuct.material = "Galvaniz Kanal";
    galvDuct.roughness_mm = 0.15;
    galvDuct.unitPrice_TL = 130.0;
    galvDuct.availableDiameters_mm = {100, 125, 150, 200, 250, 300, 350, 400, 500, 600, 800};
    m_pipes["Galvaniz Kanal"] = galvDuct;

    PipeData flexDuct;
    flexDuct.material = "Flex Kanal";
    flexDuct.roughness_mm = 3.0;
    flexDuct.unitPrice_TL = 85.0;
    flexDuct.availableDiameters_mm = {100, 125, 150, 200, 250, 300};
    m_pipes["Flex Kanal"] = flexDuct;

    PipeData izoleDuct;
    izoleDuct.material = "İzoleli Kanal";
    izoleDuct.roughness_mm = 0.9;
    izoleDuct.unitPrice_TL = 180.0;
    izoleDuct.availableDiameters_mm = {100, 125, 150, 200, 250, 300, 350, 400, 500};
    m_pipes["İzoleli Kanal"] = izoleDuct;
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
    m_pumps = {
        // Sirkülasyon pompaları (ısıtma)
        {"Wilo Yonos PICO 25/1-4",      "Wilo",     "Sirkulasyon",     4.0,   2.0,  0.04},
        {"Grundfos UPS 25-40",           "Grundfos", "Sirkulasyon",     4.0,   3.5,  0.06},
        {"Wilo Stratos 25/1-8",          "Wilo",     "Sirkulasyon",     8.0,   4.5,  0.08},
        {"Grundfos ALPHA3 25-60",        "Grundfos", "Sirkulasyon",     6.0,   3.0,  0.05},
        {"Wilo Stratos MAXO 30/0.5-12",  "Wilo",     "Sirkulasyon",    12.0,   8.0,  0.25},
        // Hidrofor pompaları (temiz su basınçlandırma)
        {"Grundfos CM 3-4",              "Grundfos", "Hidrofor",       30.0,   3.0,  0.37},
        {"Grundfos CM 5-5",              "Grundfos", "Hidrofor",       45.0,   5.0,  0.55},
        {"Grundfos CM 10-3",             "Grundfos", "Hidrofor",       30.0,  10.0,  0.75},
        {"Grundfos CM 15-2",             "Grundfos", "Hidrofor",       20.0,  15.0,  1.10},
        {"Grundfos CM 25-3",             "Grundfos", "Hidrofor",       30.0,  25.0,  1.50},
        // Dikey milli pompa (yüksek bina)
        {"Wilo MVI 204",                 "Wilo",     "Dikey Milli",    40.0,  20.0,  1.10},
        {"Wilo MVI 406",                 "Wilo",     "Dikey Milli",    60.0,  40.0,  2.20},
        {"Wilo MVI 408",                 "Wilo",     "Dikey Milli",    80.0,  40.0,  3.00},
        {"Wilo MVI 810",                 "Wilo",     "Dikey Milli",    80.0,  80.0,  5.50},
        {"Wilo MVI 1606",                "Wilo",     "Dikey Milli",   100.0, 160.0, 11.00},
        {"Wilo MVI 1610",                "Wilo",     "Dikey Milli",   150.0, 160.0, 15.00},
        // CR serisi (yüksek basınç)
        {"Grundfos CR 3-10",             "Grundfos", "Dikey Milli",    60.0,   3.0,  1.50},
        {"Grundfos CR 5-12",             "Grundfos", "Dikey Milli",    90.0,   5.0,  2.20},
        {"Grundfos CR 10-12",            "Grundfos", "Dikey Milli",   120.0,  10.0,  5.50},
        {"Grundfos CR 15-8",             "Grundfos", "Dikey Milli",    80.0,  15.0,  4.00},
        {"Grundfos CR 20-10",            "Grundfos", "Dikey Milli",   100.0,  20.0,  5.50},
        {"Grundfos CR 32-6",             "Grundfos", "Dikey Milli",    60.0,  32.0,  5.50},
        {"Grundfos CR 45-3",             "Grundfos", "Dikey Milli",    30.0,  45.0,  5.50},
        // Paket hidrofor
        {"Wilo Helix V 2202",            "Wilo",     "Paket Hidrofor", 50.0,  22.0,  2.20},
        {"Wilo Helix V 3604",            "Wilo",     "Paket Hidrofor",100.0,  36.0,  4.00},
        {"Wilo Helix V 5202",            "Wilo",     "Paket Hidrofor", 50.0,  52.0,  5.50},
        {"Grundfos Hydro MPC-E 2",       "Grundfos", "Paket Hidrofor", 80.0,  40.0,  4.00},
        {"Grundfos Hydro MPC-E 4",       "Grundfos", "Paket Hidrofor",120.0,  80.0, 11.00},
        {"Grundfos Hydro MPC-E 6",       "Grundfos", "Paket Hidrofor",150.0, 120.0, 15.00},
        // Yangın pompası (EN 12845)
        {"Wilo-SCP 150-350",             "Wilo",     "Yangin",        100.0, 540.0, 55.00},
        {"Grundfos NK 80-250",           "Grundfos", "Yangin",         60.0, 120.0, 18.50},
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

void Database::InitializeFans() {
    // Systemair fans
    m_fans.push_back({"K 100 M",     "Systemair", 80,   200,  0.045, 0.56});
    m_fans.push_back({"K 160 M",     "Systemair", 170,  350,  0.09,  0.53});
    m_fans.push_back({"K 200 M",     "Systemair", 330,  450,  0.15,  0.45});
    m_fans.push_back({"K 250 M",     "Systemair", 550,  600,  0.25,  0.45});
    m_fans.push_back({"K 315 M",     "Systemair", 1100, 750,  0.55,  0.50});
    m_fans.push_back({"KVO 100",     "Systemair", 95,   180,  0.05,  0.53});
    m_fans.push_back({"AW 200E2",    "Systemair", 400,  250,  0.12,  0.30});
    // Rosenberg fans
    m_fans.push_back({"DRAD 200-4",  "Rosenberg", 220,  300,  0.10,  0.45});
    m_fans.push_back({"DRAD 250-4",  "Rosenberg", 500,  450,  0.22,  0.44});
    m_fans.push_back({"DRAD 315-4",  "Rosenberg", 900,  600,  0.45,  0.50});
    m_fans.push_back({"DRAD 400-4",  "Rosenberg", 2000, 800,  1.10,  0.55});
    m_fans.push_back({"ERAB 20-2",   "Rosenberg", 280,  200,  0.08,  0.29});
    // ebm-papst fans
    m_fans.push_back({"R2E133-BH72", "ebm-papst", 150,  350,  0.07,  0.47});
    m_fans.push_back({"R2E190-AO26", "ebm-papst", 350,  500,  0.18,  0.51});
    m_fans.push_back({"R2E225-BD92", "ebm-papst", 650,  700,  0.35,  0.54});
    m_fans.push_back({"R4E310-AP11", "ebm-papst", 1500, 900,  0.75,  0.50});
}

FanData Database::SuggestFan(double airflow_Ls, double pressure_Pa) const {
    // Find smallest fan that meets both airflow and pressure requirements
    const FanData* best = nullptr;
    double bestPower = 1e9;

    for (const auto& fan : m_fans) {
        if (fan.maxAirflow_Ls >= airflow_Ls && fan.maxPressure_Pa >= pressure_Pa) {
            if (fan.power_kW < bestPower) {
                bestPower = fan.power_kW;
                best = &fan;
            }
        }
    }

    if (best) return *best;

    // If no fan meets requirements, return the largest one
    if (!m_fans.empty()) {
        const FanData* largest = &m_fans[0];
        for (const auto& fan : m_fans) {
            if (fan.maxAirflow_Ls > largest->maxAirflow_Ls)
                largest = &fan;
        }
        return *largest;
    }

    return FanData{"N/A", "N/A", 0, 0, 0, 0};
}

} // namespace mep
} // namespace vkt
