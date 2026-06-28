/**
 * @file FixtureSymbolLibrary.cpp
 * @brief Tesisat armatür sembol kütüphanesi — DIN/ISO plan sembolleri
 *
 * Tüm semboller 100×100 mm referans kutusuna normalize edilmiştir.
 * Koordinat sistemi: merkez (0,0), sağ (+X), yukarı (+Y).
 */

#include "cad/FixtureSymbolLibrary.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Arc.hpp"
#include "cad/Polyline.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include "../../third_party/nlohmann/json.hpp"

namespace vkt {
namespace cad {

static constexpr double PI = 3.14159265358979323846;

// ═══════════════════════════════════════════════════════════
//  SINGLETON
// ═══════════════════════════════════════════════════════════

FixtureSymbolLibrary& FixtureSymbolLibrary::Instance() {
    static FixtureSymbolLibrary inst;
    return inst;
}

FixtureSymbolLibrary::FixtureSymbolLibrary() {
    RegisterAll();
}

void FixtureSymbolLibrary::RegisterAll() {
    m_factories[static_cast<int>(FixtureSymbolType::Washbasin)] =
        [](double s, double cx, double cy) { return MakeWashbasin(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::WC)] =
        [](double s, double cx, double cy) { return MakeWC(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Shower)] =
        [](double s, double cx, double cy) { return MakeShower(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Bathtub)] =
        [](double s, double cx, double cy) { return MakeBathtub(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Pump)] =
        [](double s, double cx, double cy) { return MakePump(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::GateValve)] =
        [](double s, double cx, double cy) { return MakeGateValve(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::CheckValve)] =
        [](double s, double cx, double cy) { return MakeCheckValve(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::BallValve)] =
        [](double s, double cx, double cy) { return MakeBallValve(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::WaterMeter)] =
        [](double s, double cx, double cy) { return MakeWaterMeter(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Drain)] =
        [](double s, double cx, double cy) { return MakeDrain(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Source)] =
        [](double s, double cx, double cy) { return MakeSource(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Junction)] =
        [](double s, double cx, double cy) { return MakeJunction(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::DishwasherConnection)] =
        [](double s, double cx, double cy) { return MakeAppliance(s, cx, cy, "BM"); };
    m_factories[static_cast<int>(FixtureSymbolType::WashingMachineConnection)] =
        [](double s, double cx, double cy) { return MakeAppliance(s, cx, cy, "CM"); };
    // Gaz / Isitma / Yangin
    m_factories[static_cast<int>(FixtureSymbolType::GasAppliance)] =
        [](double s, double cx, double cy) { return MakeGasAppliance(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::GasValve)] =
        [](double s, double cx, double cy) { return MakeGasValve(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Boiler)] =
        [](double s, double cx, double cy) { return MakeBoiler(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Radiator)] =
        [](double s, double cx, double cy) { return MakeRadiator(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::HotSource)] =
        [](double s, double cx, double cy) { return MakeHotSource(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::Sprinkler)] =
        [](double s, double cx, double cy) { return MakeSprinkler(s, cx, cy); };
    m_factories[static_cast<int>(FixtureSymbolType::FirePump)] =
        [](double s, double cx, double cy) { return MakeFirePump(s, cx, cy); };
}

// ═══════════════════════════════════════════════════════════
//  DISPATCH
// ═══════════════════════════════════════════════════════════

SymbolGeometry FixtureSymbolLibrary::GetSymbol(FixtureSymbolType type,
                                                double scale,
                                                double centerX,
                                                double centerY) const {
    auto it = m_factories.find(static_cast<int>(type));
    if (it != m_factories.end()) {
        return it->second(scale, centerX, centerY);
    }
    return MakeJunction(scale, centerX, centerY);
}

FixtureSymbolType FixtureSymbolLibrary::FromLabel(const std::string& label) {
    std::string s = label;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s.find("lavabo")    != std::string::npos ||
        s.find("washbasin") != std::string::npos ||
        s.find("sink")      != std::string::npos)   return FixtureSymbolType::Washbasin;
    if (s.find("wc")        != std::string::npos ||
        s.find("klozet")    != std::string::npos ||
        s.find("toilet")    != std::string::npos)   return FixtureSymbolType::WC;
    if (s.find("dus")       != std::string::npos ||
        s.find("shower")    != std::string::npos)   return FixtureSymbolType::Shower;
    if (s.find("kuvet")     != std::string::npos ||
        s.find("bathtub")   != std::string::npos)   return FixtureSymbolType::Bathtub;
    if (s.find("pompa")     != std::string::npos ||
        s.find("pump")      != std::string::npos)   return FixtureSymbolType::Pump;
    if (s.find("sayac")     != std::string::npos ||
        s.find("meter")     != std::string::npos)   return FixtureSymbolType::WaterMeter;
    if (s.find("tahliye")   != std::string::npos ||
        s.find("drain")     != std::string::npos)   return FixtureSymbolType::Drain;
    if (s.find("kaynak")    != std::string::npos ||
        s.find("source")    != std::string::npos)   return FixtureSymbolType::Source;
    if (s.find("bulasik")   != std::string::npos)   return FixtureSymbolType::DishwasherConnection;
    if (s.find("camasir")   != std::string::npos)   return FixtureSymbolType::WashingMachineConnection;
    if (s.find("kazan")     != std::string::npos ||
        s.find("boiler")    != std::string::npos)   return FixtureSymbolType::Boiler;
    if (s.find("radyator")  != std::string::npos ||
        s.find("radiator")  != std::string::npos)   return FixtureSymbolType::Radiator;
    if (s.find("sprinkler") != std::string::npos)   return FixtureSymbolType::Sprinkler;
    if (s.find("yangin")    != std::string::npos ||
        s.find("fire")      != std::string::npos)   return FixtureSymbolType::FirePump;
    if (s.find("gaz")       != std::string::npos ||
        s.find("kombi")     != std::string::npos ||
        s.find("ocak")      != std::string::npos)   return FixtureSymbolType::GasAppliance;

    return FixtureSymbolType::Junction;
}

// ═══════════════════════════════════════════════════════════
//  SEMBOL FABRIKALARI
//  Tum koordinatlar: merkez (cx,cy), boyut scale*100mm
// ═══════════════════════════════════════════════════════════

static inline double S(double v, double scale) { return v * scale; }

// --- Lavabo (DIN 1986 / ISO 6309) ---
// Dikdortgen govde + eliptik ic havuz + musluk isaretleri + tahliye
SymbolGeometry FixtureSymbolLibrary::MakeWashbasin(double scale,
                                                    double cx, double cy) {
    SymbolGeometry g;
    g.label = "Lv";

    double hw = S(40, scale);
    double hh = S(30, scale);

    // Dis dikdortgen (lavabo govdesi)
    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});

    // Ic eliptik havuz -- iki yari daire ile elips yaklasimi
    double basinRx = S(28, scale);
    double basinRy = S(18, scale);
    double basinCy = cy + S(4, scale);
    g.arcs.push_back({cx, basinCy, basinRx, 0, 180});
    g.arcs.push_back({cx, basinCy + S(5, scale) * (basinRx - basinRy) / basinRx,
                      basinRy, 180, 360});

    // Tahliye deligi (merkez)
    g.circles.push_back({cx, basinCy, S(3.5, scale)});

    // On kenar kalinligi
    g.lines.push_back({cx - hw + S(5, scale), cy - hh,
                       cx + hw - S(5, scale), cy - hh});

    // Musluk isaretleri (iki kucuk daire, ust kenara yakin)
    g.circles.push_back({cx - S(12, scale), cy + S(20, scale), S(3, scale)});
    g.circles.push_back({cx + S(12, scale), cy + S(20, scale), S(3, scale)});

    g.connections.push_back({cx, cy - hh, "Su"});
    return g;
}

// --- WC / Klozet (DIN 1986) ---
SymbolGeometry FixtureSymbolLibrary::MakeWC(double scale,
                                             double cx, double cy) {
    SymbolGeometry g;
    g.label = "WC";

    double hw = S(30, scale);
    double bodyTop = cy + S(18, scale);
    double bodyBot = cy - S(35, scale);

    // Rezervuar (dikdortgen, ustte)
    g.rects.push_back({cx - hw, bodyTop, hw*2, S(27, scale)});

    // Govde: iki yan cizgi + alt yari daire (D sekli)
    g.lines.push_back({cx - hw, bodyTop, cx - hw, bodyBot});
    g.lines.push_back({cx + hw, bodyTop, cx + hw, bodyBot});
    g.arcs.push_back({cx, bodyBot, hw, 180, 360});

    // Oturma yeri (ic oval)
    double seatRx = S(22, scale);
    double seatMid = (bodyTop + bodyBot) / 2.0 - S(5, scale);
    g.arcs.push_back({cx, seatMid, seatRx, 0, 180});
    g.arcs.push_back({cx, seatMid + S(4, scale), S(16, scale), 180, 360});

    g.connections.push_back({cx, bodyTop + S(27, scale), "Su"});
    g.connections.push_back({cx, bodyBot, "Pis"});
    return g;
}

// --- Dus (DIN 1986) ---
SymbolGeometry FixtureSymbolLibrary::MakeShower(double scale,
                                                 double cx, double cy) {
    SymbolGeometry g;
    g.label = "Ds";

    double hw = S(40, scale);

    g.rects.push_back({cx - hw, cy - hw, hw*2, hw*2});
    g.arcs.push_back({cx - hw, cy + hw, S(28, scale), 270, 360});

    // Yagmurbasi sembolü (sol ust bolgede: daire + 8 yon isini)
    double shx = cx - S(18, scale);
    double shy = cy + S(18, scale);
    g.circles.push_back({shx, shy, S(8, scale)});
    for (int i = 0; i < 8; ++i) {
        double ang = i * PI / 4.0;
        double r1 = S(10, scale), r2 = S(14, scale);
        g.lines.push_back({shx + r1 * std::cos(ang), shy + r1 * std::sin(ang),
                           shx + r2 * std::cos(ang), shy + r2 * std::sin(ang)});
    }

    // Tahliye: capraz cizgiler sag alt
    double dx = cx + S(18, scale), dy = cy - S(18, scale);
    double dd = S(8, scale);
    g.lines.push_back({dx - dd, dy, dx + dd, dy});
    g.lines.push_back({dx, dy - dd, dx, dy + dd});
    g.circles.push_back({dx, dy, S(4, scale)});

    g.connections.push_back({cx, cy - hw, "Su"});
    g.connections.push_back({dx, cy - hw, "Pis"});
    return g;
}

// --- Kuvet (DIN 1986) ---
SymbolGeometry FixtureSymbolLibrary::MakeBathtub(double scale,
                                                   double cx, double cy) {
    SymbolGeometry g;
    g.label = "Kv";

    double hw = S(50, scale);
    double hh = S(25, scale);

    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});
    g.arcs.push_back({cx - S(8, scale), cy, S(36, scale), 0, 360});
    g.circles.push_back({cx + S(36, scale), cy, S(4, scale)});

    // Musluk (sol ucta)
    g.circles.push_back({cx - hw + S(8, scale), cy - S(8, scale), S(3, scale)});
    g.circles.push_back({cx - hw + S(8, scale), cy + S(8, scale), S(3, scale)});

    g.connections.push_back({cx - hw, cy, "Su"});
    g.connections.push_back({cx + S(36, scale), cy - hh, "Pis"});
    return g;
}

// --- Pompa (DIN EN ISO 10628-2) ---
SymbolGeometry FixtureSymbolLibrary::MakePump(double scale,
                                               double cx, double cy) {
    SymbolGeometry g;
    g.label = "P";

    double r = S(28, scale);
    g.circles.push_back({cx, cy, r});

    double tx = S(15, scale);
    g.lines.push_back({cx,      cy,      cx + tx, cy - tx});
    g.lines.push_back({cx + tx, cy - tx, cx + tx, cy + tx});
    g.lines.push_back({cx + tx, cy + tx, cx,      cy});

    g.connections.push_back({cx - r, cy, "Giris"});
    g.connections.push_back({cx + r, cy, "Cikis"});
    return g;
}

// --- Surgulu Vana (DIN EN 806) ---
SymbolGeometry FixtureSymbolLibrary::MakeGateValve(double scale,
                                                    double cx, double cy) {
    SymbolGeometry g;
    g.label = "SV";

    double hw = S(20, scale);
    double hh = S(15, scale);

    g.lines.push_back({cx - S(40, scale), cy, cx - hw, cy});
    g.lines.push_back({cx + hw, cy, cx + S(40, scale), cy});

    g.lines.push_back({cx - hw, cy - hh, cx - hw, cy + hh});
    g.lines.push_back({cx - hw, cy - hh, cx,       cy});
    g.lines.push_back({cx - hw, cy + hh, cx,       cy});

    g.lines.push_back({cx + hw, cy - hh, cx + hw, cy + hh});
    g.lines.push_back({cx + hw, cy - hh, cx,       cy});
    g.lines.push_back({cx + hw, cy + hh, cx,       cy});

    g.lines.push_back({cx, cy, cx, cy + S(25, scale)});
    g.lines.push_back({cx - S(10, scale), cy + S(25, scale),
                        cx + S(10, scale), cy + S(25, scale)});

    g.connections.push_back({cx - S(40, scale), cy, "A"});
    g.connections.push_back({cx + S(40, scale), cy, "B"});
    return g;
}

// --- Cek Valf ---
SymbolGeometry FixtureSymbolLibrary::MakeCheckValve(double scale,
                                                     double cx, double cy) {
    SymbolGeometry g;
    g.label = "CV";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx + r, cy, cx + S(40, scale), cy});

    g.lines.push_back({cx, cy - r, cx, cy + r});
    g.lines.push_back({cx - S(8, scale), cy - S(6, scale), cx, cy});
    g.lines.push_back({cx - S(8, scale), cy + S(6, scale), cx, cy});

    g.connections.push_back({cx - S(40, scale), cy, "Giris"});
    g.connections.push_back({cx + S(40, scale), cy, "Cikis"});
    return g;
}

// --- Kuresel Vana ---
SymbolGeometry FixtureSymbolLibrary::MakeBallValve(double scale,
                                                    double cx, double cy) {
    SymbolGeometry g;
    g.label = "KV";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx + r, cy, cx + S(40, scale), cy});

    g.lines.push_back({cx, cy + r, cx, cy + r + S(12, scale)});

    g.connections.push_back({cx - S(40, scale), cy, "A"});
    g.connections.push_back({cx + S(40, scale), cy, "B"});
    return g;
}

// --- Su Sayaci ---
SymbolGeometry FixtureSymbolLibrary::MakeWaterMeter(double scale,
                                                     double cx, double cy) {
    SymbolGeometry g;
    g.label = "M";

    double hw = S(25, scale);
    double hh = S(15, scale);

    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});

    g.lines.push_back({cx - S(40, scale), cy, cx - hw, cy});
    g.lines.push_back({cx + hw, cy, cx + S(40, scale), cy});

    for (int i = -2; i <= 2; ++i) {
        double tx = cx + i * S(8, scale);
        g.lines.push_back({tx, cy - hh + S(3, scale), tx, cy - S(3, scale)});
    }

    g.connections.push_back({cx - S(40, scale), cy, "Giris"});
    g.connections.push_back({cx + S(40, scale), cy, "Cikis"});
    return g;
}

// --- Pis Su Tahliye ---
SymbolGeometry FixtureSymbolLibrary::MakeDrain(double scale,
                                                double cx, double cy) {
    SymbolGeometry g;
    g.label = "T";

    double r = S(20, scale);
    g.circles.push_back({cx, cy, r});

    double d = S(14, scale);
    g.lines.push_back({cx - d, cy - d, cx + d, cy + d});
    g.lines.push_back({cx + d, cy - d, cx - d, cy + d});

    g.connections.push_back({cx, cy, "Pis"});
    return g;
}

// --- Sebeke Girisi / Kaynak ---
SymbolGeometry FixtureSymbolLibrary::MakeSource(double scale,
                                                 double cx, double cy) {
    SymbolGeometry g;
    g.label = "S";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx - S(40, scale), cy - S(8, scale),
                        cx - S(22, scale), cy});
    g.lines.push_back({cx - S(40, scale), cy + S(8, scale),
                        cx - S(22, scale), cy});

    g.connections.push_back({cx + r, cy, "Cikis"});
    return g;
}

// --- Baglanti Noktasi ---
SymbolGeometry FixtureSymbolLibrary::MakeJunction(double scale,
                                                   double cx, double cy) {
    SymbolGeometry g;
    g.label = "";
    g.circles.push_back({cx, cy, S(5, scale)});
    g.connections.push_back({cx, cy, ""});
    return g;
}

// --- Beyaz Esya Baglantisi ---
SymbolGeometry FixtureSymbolLibrary::MakeAppliance(double scale,
                                                    double cx, double cy,
                                                    const std::string& label) {
    SymbolGeometry g;
    g.label = label;

    double hw = S(35, scale);
    g.rects.push_back({cx - hw, cy - hw, hw*2, hw*2});

    g.lines.push_back({cx - hw + S(5, scale), cy,
                        cx + hw - S(5, scale), cy});
    g.lines.push_back({cx, cy - hw + S(5, scale),
                        cx, cy + hw - S(5, scale)});

    g.connections.push_back({cx, cy - hw, "Su"});
    g.connections.push_back({cx, cy + hw, "Pis"});
    return g;
}

// ═══════════════════════════════════════════════════════════
//  GAZ / ISITMA / YANGIN SEMBOLLERI (Session 29)
// ═══════════════════════════════════════════════════════════

// --- Gaz Cihazi (TS EN 12067 diamond) ---
SymbolGeometry FixtureSymbolLibrary::MakeGasAppliance(double scale,
                                                       double cx, double cy) {
    SymbolGeometry g;
    g.label = "G";

    double d = S(32, scale);

    // Baklava (diamond) -- 4 kose
    g.lines.push_back({cx,     cy + d, cx + d, cy});
    g.lines.push_back({cx + d, cy,     cx,     cy - d});
    g.lines.push_back({cx,     cy - d, cx - d, cy});
    g.lines.push_back({cx - d, cy,     cx,     cy + d});

    // Ic kucuk daire (gaz alevi)
    g.circles.push_back({cx, cy, S(8, scale)});

    // Alev ucu (ust ucgen)
    double fd = S(5, scale);
    g.lines.push_back({cx - fd, cy + S(10, scale), cx,     cy + S(18, scale)});
    g.lines.push_back({cx + fd, cy + S(10, scale), cx,     cy + S(18, scale)});
    g.lines.push_back({cx - fd, cy + S(10, scale), cx + fd, cy + S(10, scale)});

    g.connections.push_back({cx - d, cy, "Gaz"});
    return g;
}

// --- Gaz Vanasi (TS EN 331) ---
SymbolGeometry FixtureSymbolLibrary::MakeGasValve(double scale,
                                                    double cx, double cy) {
    SymbolGeometry g;
    g.label = "GV";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx + r, cy, cx + S(40, scale), cy});

    // Kol dik = kapali (guvenlik)
    g.lines.push_back({cx, cy - r, cx, cy + r + S(12, scale)});
    g.lines.push_back({cx - S(8, scale), cy + r + S(12, scale),
                        cx + S(8, scale), cy + r + S(12, scale)});

    // Butterfly disk (X isareti ic)
    g.lines.push_back({cx - S(10, scale), cy - S(10, scale),
                        cx + S(10, scale), cy + S(10, scale)});
    g.lines.push_back({cx + S(10, scale), cy - S(10, scale),
                        cx - S(10, scale), cy + S(10, scale)});

    g.connections.push_back({cx - S(40, scale), cy, "A"});
    g.connections.push_back({cx + S(40, scale), cy, "B"});
    return g;
}

// --- Kazan / Isi Merkezi (EN 12828) ---
SymbolGeometry FixtureSymbolLibrary::MakeBoiler(double scale,
                                                 double cx, double cy) {
    SymbolGeometry g;
    g.label = "K";

    double hw = S(35, scale);
    double hh = S(28, scale);

    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});

    // Ic yatay cizgiler (isi esanjor tuplerini temsil eder)
    for (int i = -1; i <= 1; ++i) {
        double y = cy + i * S(9, scale);
        g.lines.push_back({cx - hw + S(8, scale), y,
                           cx + hw - S(8, scale), y});
    }

    // Alev sembolu (altta)
    double fy = cy - hh + S(6, scale);
    g.lines.push_back({cx - S(8, scale), fy, cx, fy + S(10, scale)});
    g.lines.push_back({cx + S(8, scale), fy, cx, fy + S(10, scale)});

    g.connections.push_back({cx - hw, cy, "Donus"});
    g.connections.push_back({cx + hw, cy, "Gidis"});
    g.connections.push_back({cx, cy - hh, "Yakit"});
    return g;
}

// --- Panel Radyator (EN 442) ---
SymbolGeometry FixtureSymbolLibrary::MakeRadiator(double scale,
                                                    double cx, double cy) {
    SymbolGeometry g;
    g.label = "R";

    double hw = S(45, scale);
    double hh = S(18, scale);
    double bh = S(5, scale);

    // Ust bant
    g.rects.push_back({cx - hw, cy + hh - bh, hw*2, bh});
    // Alt bant
    g.rects.push_back({cx - hw, cy - hh, hw*2, bh});

    // Fin cizgileri (7 adet -- EN 442 panel)
    for (int i = 0; i <= 7; ++i) {
        double x = cx - hw + i * (hw*2) / 7.0;
        g.lines.push_back({x, cy - hh, x, cy + hh - bh});
    }

    g.connections.push_back({cx - hw, cy - hh, "Gidis"});
    g.connections.push_back({cx + hw, cy - hh, "Donus"});
    return g;
}

// --- Sicak Su Kaynagi / Sofben ---
SymbolGeometry FixtureSymbolLibrary::MakeHotSource(double scale,
                                                     double cx, double cy) {
    SymbolGeometry g;
    g.label = "SS";

    double r = S(16, scale);
    g.circles.push_back({cx, cy, r});

    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx - S(40, scale), cy - S(8, scale),
                        cx - S(22, scale), cy});
    g.lines.push_back({cx - S(40, scale), cy + S(8, scale),
                        cx - S(22, scale), cy});

    // Dalgali isi cizgisi (3 kucuk yay ustte)
    for (int i = -1; i <= 1; ++i) {
        double wx = cx + i * S(7, scale);
        double wy = cy + r + S(4, scale);
        g.arcs.push_back({wx, wy, S(4, scale), 0, 180});
    }

    g.connections.push_back({cx + r, cy, "Sicak"});
    return g;
}

// --- Sprinkler Basligi (EN 12845) ---
SymbolGeometry FixtureSymbolLibrary::MakeSprinkler(double scale,
                                                     double cx, double cy) {
    SymbolGeometry g;
    g.label = "Sp";

    double r = S(14, scale);
    g.circles.push_back({cx, cy, r});

    // Capraz kol (sprinkler cercevesi)
    g.lines.push_back({cx - S(22, scale), cy, cx + S(22, scale), cy});
    g.lines.push_back({cx, cy - S(22, scale), cx, cy + S(22, scale)});

    // Su dagilim isaretleri -- 8 yonde
    for (int i = 0; i < 8; ++i) {
        double ang = i * PI / 4.0;
        double r1 = S(24, scale), r2 = S(30, scale);
        g.lines.push_back({cx + r1 * std::cos(ang), cy + r1 * std::sin(ang),
                           cx + r2 * std::cos(ang), cy + r2 * std::sin(ang)});
    }

    // Merkez nokta (deflekter)
    g.circles.push_back({cx, cy, S(3, scale)});

    g.connections.push_back({cx, cy + S(22, scale), "YH"});
    return g;
}

// --- Yangin Pompasi (EN 12845 / NFPA 20) ---
SymbolGeometry FixtureSymbolLibrary::MakeFirePump(double scale,
                                                   double cx, double cy) {
    SymbolGeometry g;
    g.label = "YP";

    double outerR = S(34, scale);
    g.circles.push_back({cx, cy, outerR});

    double r = S(22, scale);
    g.circles.push_back({cx, cy, r});

    double tx = S(12, scale);
    g.lines.push_back({cx,      cy,      cx + tx, cy - tx});
    g.lines.push_back({cx + tx, cy - tx, cx + tx, cy + tx});
    g.lines.push_back({cx + tx, cy + tx, cx,      cy});

    // Yangin gostergesi: dis halkada 4 isaret
    for (int i = 0; i < 4; ++i) {
        double ang = i * PI / 2.0 + PI / 4.0;
        double r1 = outerR - S(4, scale), r2 = outerR + S(4, scale);
        g.lines.push_back({cx + r1 * std::cos(ang), cy + r1 * std::sin(ang),
                           cx + r2 * std::cos(ang), cy + r2 * std::sin(ang)});
    }

    g.connections.push_back({cx - outerR, cy, "Giris"});
    g.connections.push_back({cx + outerR, cy, "Cikis"});
    return g;
}

// ═══════════════════════════════════════════════════════════
//  CIKTI FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

std::string FixtureSymbolLibrary::ToSVG(const SymbolGeometry& sym,
                                         const std::string& color) {
    std::ostringstream ss;
    std::string style = "fill:none;stroke:" + color + ";stroke-width:0.8";

    ss << "<g style=\"" << style << "\">\n";

    for (const auto& r : sym.rects) {
        ss << "  <rect x=\""      << r.x << "\" y=\"" << r.y
           << "\" width=\""       << r.w << "\" height=\"" << r.h
           << "\" rx=\"1\"/>\n";
    }
    for (const auto& c : sym.circles) {
        ss << "  <circle cx=\""   << c.cx << "\" cy=\"" << c.cy
           << "\" r=\""           << c.r  << "\"/>\n";
    }
    for (const auto& l : sym.lines) {
        ss << "  <line x1=\""     << l.x1 << "\" y1=\"" << l.y1
           << "\" x2=\""          << l.x2 << "\" y2=\"" << l.y2 << "\"/>\n";
    }
    for (const auto& a : sym.arcs) {
        double startRad = a.startDeg * PI / 180.0;
        double endRad   = a.endDeg   * PI / 180.0;
        bool   fullCircle = (std::abs(a.endDeg - a.startDeg) >= 360.0);

        if (fullCircle) {
            ss << "  <circle cx=\"" << a.cx << "\" cy=\"" << a.cy
               << "\" r=\"" << a.r << "\"/>\n";
        } else {
            double sx = a.cx + a.r * std::cos(startRad);
            double sy = a.cy + a.r * std::sin(startRad);
            double ex = a.cx + a.r * std::cos(endRad);
            double ey = a.cy + a.r * std::sin(endRad);
            int large = ((a.endDeg - a.startDeg) > 180) ? 1 : 0;

            ss << "  <path d=\"M " << sx << " " << sy
               << " A " << a.r << " " << a.r << " 0 " << large << " 1 "
               << ex << " " << ey << "\"/>\n";
        }
    }

    if (!sym.label.empty()) {
        ss << "  <text x=\"0\" y=\"-8\""
           << " style=\"font-size:8px;fill:" << color
           << ";stroke:none;text-anchor:middle;\">"
           << sym.label << "</text>\n";
    }

    ss << "</g>";
    return ss.str();
}

std::vector<std::unique_ptr<Entity>>
FixtureSymbolLibrary::ToEntities(const SymbolGeometry& sym,
                                  const std::string& layerName) {
    std::vector<std::unique_ptr<Entity>> result;

    for (const auto& ln : sym.lines) {
        auto e = std::make_unique<Line>(
            geom::Vec3{ln.x1, ln.y1, 0.0},
            geom::Vec3{ln.x2, ln.y2, 0.0});
        e->SetLayerName(layerName);
        result.push_back(std::move(e));
    }

    for (const auto& c : sym.circles) {
        auto e = std::make_unique<Circle>(
            geom::Vec3{c.cx, c.cy, 0.0}, c.r);
        e->SetLayerName(layerName);
        result.push_back(std::move(e));
    }

    for (const auto& r : sym.rects) {
        auto poly = std::make_unique<Polyline>();
        poly->AddVertex({r.x,       r.y,       0.0});
        poly->AddVertex({r.x + r.w, r.y,       0.0});
        poly->AddVertex({r.x + r.w, r.y + r.h, 0.0});
        poly->AddVertex({r.x,       r.y + r.h, 0.0});
        poly->SetClosed(true);
        poly->SetLayerName(layerName);
        result.push_back(std::move(poly));
    }

    for (const auto& a : sym.arcs) {
        auto e = std::make_unique<Arc>(
            geom::Vec3{a.cx, a.cy, 0.0}, a.r,
            a.startDeg, a.endDeg);
        e->SetLayerName(layerName);
        result.push_back(std::move(e));
    }

    return result;
}

// ── Custom sembol JSON yükleme ────────────────────────────────────────────────
bool FixtureSymbolLibrary::RegisterCustom(const std::string& jsonPath,
                                          FixtureSymbolType  type) {
    std::ifstream f(jsonPath);
    if (!f.is_open()) return false;

    nlohmann::json j;
    try { f >> j; } catch (...) { return false; }

    SymbolGeometry geo;
    geo.label = j.value("label", "?");

    for (const auto& ln : j.value("lines", nlohmann::json::array())) {
        geo.lines.push_back({ln.value("x1",0.0), ln.value("y1",0.0),
                              ln.value("x2",0.0), ln.value("y2",0.0)});
    }
    for (const auto& ci : j.value("circles", nlohmann::json::array())) {
        geo.circles.push_back({ci.value("cx",0.0), ci.value("cy",0.0),
                                ci.value("r",10.0)});
    }
    for (const auto& rc : j.value("rects", nlohmann::json::array())) {
        geo.rects.push_back({rc.value("x",0.0), rc.value("y",0.0),
                              rc.value("w",0.0), rc.value("h",0.0)});
    }
    for (const auto& ar : j.value("arcs", nlohmann::json::array())) {
        geo.arcs.push_back({ar.value("cx",0.0), ar.value("cy",0.0),
                             ar.value("r",10.0),
                             ar.value("startDeg",0.0), ar.value("endDeg",360.0)});
    }

    int key = static_cast<int>(type);
    m_factories[key] = [geo](double scale, double cx, double cy) -> SymbolGeometry {
        SymbolGeometry out = geo;
        auto applyTransform = [&](double& x, double& y) {
            x = x * scale + cx;
            y = y * scale + cy;
        };
        for (auto& ln : out.lines) {
            applyTransform(ln.x1, ln.y1);
            applyTransform(ln.x2, ln.y2);
        }
        for (auto& ci : out.circles) {
            applyTransform(ci.cx, ci.cy);
            ci.r *= scale;
        }
        for (auto& rc : out.rects) {
            applyTransform(rc.x, rc.y);
            rc.w *= scale; rc.h *= scale;
        }
        for (auto& ar : out.arcs) {
            applyTransform(ar.cx, ar.cy);
            ar.r *= scale;
        }
        return out;
    };
    return true;
}

void FixtureSymbolLibrary::LoadCustomDirectory(const std::string& dir) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".json") {
            // Filename prefix determines type: washbasin.json, wc.json, etc.
            std::string stem = entry.path().stem().string();
            FixtureSymbolType type = FromLabel(stem);
            RegisterCustom(entry.path().string(), type);
        }
    }
}

std::vector<FixtureSymbolType> FixtureSymbolLibrary::GetRegisteredTypes() const {
    std::vector<FixtureSymbolType> types;
    for (const auto& kv : m_factories)
        types.push_back(static_cast<FixtureSymbolType>(kv.first));
    return types;
}

std::string FixtureSymbolLibrary::GetDisplayName(FixtureSymbolType type) {
    switch (type) {
        case FixtureSymbolType::Washbasin:               return "Lavabo";
        case FixtureSymbolType::WC:                      return "WC / Klozet";
        case FixtureSymbolType::Shower:                  return "Duş";
        case FixtureSymbolType::Bathtub:                 return "Küvet";
        case FixtureSymbolType::DishwasherConnection:    return "Bulaşık Makinesi";
        case FixtureSymbolType::WashingMachineConnection:return "Çamaşır Makinesi";
        case FixtureSymbolType::Pump:                    return "Pompa";
        case FixtureSymbolType::WaterMeter:              return "Su Sayacı";
        case FixtureSymbolType::GateValve:               return "Sürgülü Vana";
        case FixtureSymbolType::CheckValve:              return "Çek Valf";
        case FixtureSymbolType::BallValve:               return "Küresel Vana";
        case FixtureSymbolType::Drain:                   return "Tahliye";
        case FixtureSymbolType::Source:                  return "Su Kaynağı";
        case FixtureSymbolType::Junction:                return "Bağlantı";
        case FixtureSymbolType::GasAppliance:            return "Gaz Cihazı";
        case FixtureSymbolType::GasValve:                return "Gaz Vanası";
        case FixtureSymbolType::Boiler:                  return "Kazan";
        case FixtureSymbolType::Radiator:                return "Radyatör";
        case FixtureSymbolType::HotSource:               return "Sıcak Su Kaynağı";
        case FixtureSymbolType::Sprinkler:               return "Sprinkler";
        case FixtureSymbolType::FirePump:                return "Yangın Pompası";
        default:                                         return "Sembol";
    }
}

} // namespace cad
} // namespace vkt
