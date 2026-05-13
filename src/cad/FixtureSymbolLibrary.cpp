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
#include "cad/Polyline.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

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

    if (s.find("lavabo") != std::string::npos ||
        s.find("washbasin") != std::string::npos ||
        s.find("sink") != std::string::npos)        return FixtureSymbolType::Washbasin;
    if (s.find("wc") != std::string::npos ||
        s.find("klozet") != std::string::npos ||
        s.find("toilet") != std::string::npos)      return FixtureSymbolType::WC;
    if (s.find("duş") != std::string::npos ||
        s.find("dus") != std::string::npos ||
        s.find("shower") != std::string::npos)      return FixtureSymbolType::Shower;
    if (s.find("küvet") != std::string::npos ||
        s.find("kuvet") != std::string::npos ||
        s.find("bathtub") != std::string::npos)     return FixtureSymbolType::Bathtub;
    if (s.find("pompa") != std::string::npos ||
        s.find("pump") != std::string::npos)        return FixtureSymbolType::Pump;
    if (s.find("sayaç") != std::string::npos ||
        s.find("sayac") != std::string::npos ||
        s.find("meter") != std::string::npos)       return FixtureSymbolType::WaterMeter;
    if (s.find("tahliye") != std::string::npos ||
        s.find("drain") != std::string::npos)       return FixtureSymbolType::Drain;
    if (s.find("kaynak") != std::string::npos ||
        s.find("source") != std::string::npos)      return FixtureSymbolType::Source;
    if (s.find("bulaşık") != std::string::npos ||
        s.find("bulasik") != std::string::npos)     return FixtureSymbolType::DishwasherConnection;
    if (s.find("çamaşır") != std::string::npos ||
        s.find("camasir") != std::string::npos)     return FixtureSymbolType::WashingMachineConnection;

    return FixtureSymbolType::Junction;
}

// ═══════════════════════════════════════════════════════════
//  SEMBOL FABRİKALARI
//  Tüm koordinatlar: merkez (cx,cy), boyut scale×100mm
// ═══════════════════════════════════════════════════════════

// Yardımcı: scale uygulayarak koordinat dönüştür
static inline double S(double v, double scale) { return v * scale; }

SymbolGeometry FixtureSymbolLibrary::MakeWashbasin(double scale,
                                                    double cx, double cy) {
    // DIN 1986 lavabo: dikdörtgen gövde + oval iç havuz + iki musluk noktası
    SymbolGeometry g;
    g.label = "Lv";

    double hw = S(40, scale);  // yarı genişlik
    double hh = S(30, scale);  // yarı yükseklik
    double tw = S(5,  scale);  // duvar kalınlığı

    // Dış dikdörtgen
    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});
    // İç oval (havuz)
    g.circles.push_back({cx, cy + S(5, scale), S(22, scale)});
    // Tahliye deliği
    g.circles.push_back({cx, cy + S(5, scale), S(4, scale)});
    // Boru bağlantısı (alt orta)
    g.connections.push_back({cx, cy - hh, "Su"});

    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeWC(double scale,
                                             double cx, double cy) {
    // DIN 1986 klozet: D şekli gövde + iç oval + rezervuar
    SymbolGeometry g;
    g.label = "WC";

    double hw = S(30, scale);
    double hh = S(45, scale);

    // Rezervuar (üst)
    g.rects.push_back({cx - hw, cy + S(20, scale), hw*2, S(25, scale)});

    // Ana gövde (yuvarlak ön + düz arka) — arc + lines ile
    // Arka çizgi
    g.lines.push_back({cx - hw, cy + S(20, scale), cx - hw, cy - S(25, scale)});
    g.lines.push_back({cx + hw, cy + S(20, scale), cx + hw, cy - S(25, scale)});
    // Alt yay (ön)
    g.arcs.push_back({cx, cy - S(25, scale), hw, 180, 360});

    // İç oval (oturma yeri)
    g.circles.push_back({cx, cy - S(5, scale), S(20, scale)});

    g.connections.push_back({cx, cy + hh, "Su"});
    g.connections.push_back({cx, cy - hh, "Pis"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeShower(double scale,
                                                 double cx, double cy) {
    // DIN 1986 duş: kare tekne + köşe eğimi + yağmurbaşı sembolü
    SymbolGeometry g;
    g.label = "Dş";

    double hw = S(40, scale);

    // Dış kare
    g.rects.push_back({cx - hw, cy - hw, hw*2, hw*2});
    // Köşe eğimi (sol üst)
    g.arcs.push_back({cx - hw, cy + hw, S(25, scale), 270, 360});
    // Yağmurbaşı (merkez daire)
    g.circles.push_back({cx - S(15, scale), cy + S(15, scale), S(10, scale)});
    // Tahliye (merkez)
    g.circles.push_back({cx + S(10, scale), cy - S(10, scale), S(4, scale)});

    g.connections.push_back({cx, cy - hw, "Su"});
    g.connections.push_back({cx + S(10, scale), cy - S(10, scale), "Pis"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeBathtub(double scale,
                                                   double cx, double cy) {
    // DIN 1986 küvet: uzun dikdörtgen + iç oval
    SymbolGeometry g;
    g.label = "Kv";

    double hw = S(50, scale);
    double hh = S(25, scale);

    // Dış dikdörtgen
    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});
    // İç oval
    g.arcs.push_back({cx, cy, S(35, scale), 0, 360});
    // Tahliye deliği
    g.circles.push_back({cx + S(35, scale), cy, S(4, scale)});

    g.connections.push_back({cx - hw, cy, "Su"});
    g.connections.push_back({cx + S(35, scale), cy - hh, "Pis"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakePump(double scale,
                                               double cx, double cy) {
    // DIN EN ISO 10628 pompa: daire + üçgen (impeller)
    SymbolGeometry g;
    g.label = "P";

    double r = S(30, scale);
    g.circles.push_back({cx, cy, r});

    // İmpeller ok (saat 3 yönünde üçgen)
    double tx = S(15, scale);
    g.lines.push_back({cx,      cy,      cx + tx, cy - tx});
    g.lines.push_back({cx + tx, cy - tx, cx + tx, cy + tx});
    g.lines.push_back({cx + tx, cy + tx, cx,      cy});

    g.connections.push_back({cx - r, cy, "Giriş"});
    g.connections.push_back({cx + r, cy, "Çıkış"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeGateValve(double scale,
                                                    double cx, double cy) {
    // DIN EN 806 sürgülü vana: iki üçgen karşılıklı + yatay boru
    SymbolGeometry g;
    g.label = "SV";

    double hw = S(20, scale);
    double hh = S(15, scale);

    // Boru hattı
    g.lines.push_back({cx - S(40, scale), cy, cx - hw, cy});
    g.lines.push_back({cx + hw, cy, cx + S(40, scale), cy});

    // Sol üçgen
    g.lines.push_back({cx - hw, cy - hh, cx - hw, cy + hh});
    g.lines.push_back({cx - hw, cy - hh, cx,       cy});
    g.lines.push_back({cx - hw, cy + hh, cx,       cy});

    // Sağ üçgen
    g.lines.push_back({cx + hw, cy - hh, cx + hw, cy + hh});
    g.lines.push_back({cx + hw, cy - hh, cx,       cy});
    g.lines.push_back({cx + hw, cy + hh, cx,       cy});

    // Mil (vana kolu)
    g.lines.push_back({cx, cy, cx, cy + S(25, scale)});
    g.lines.push_back({cx - S(8, scale), cy + S(25, scale),
                        cx + S(8, scale), cy + S(25, scale)});

    g.connections.push_back({cx - S(40, scale), cy, "A"});
    g.connections.push_back({cx + S(40, scale), cy, "B"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeCheckValve(double scale,
                                                     double cx, double cy) {
    // Çek valf: daire + ok işareti (tek yön)
    SymbolGeometry g;
    g.label = "ÇV";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    // Boru hattı
    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx + r, cy, cx + S(40, scale), cy});

    // Ok (daire içinde dikey çizgi + ok ucu)
    g.lines.push_back({cx, cy - r, cx, cy + r});  // kapak çizgisi
    // Ok ucu (sağa doğru akış)
    g.lines.push_back({cx - S(8, scale), cy - S(6, scale), cx, cy});
    g.lines.push_back({cx - S(8, scale), cy + S(6, scale), cx, cy});

    g.connections.push_back({cx - S(40, scale), cy, "Giriş"});
    g.connections.push_back({cx + S(40, scale), cy, "Çıkış"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeBallValve(double scale,
                                                    double cx, double cy) {
    // Küresel vana: daire + çizgi (açık/kapalı pozisyon)
    SymbolGeometry g;
    g.label = "KV";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    // Boru hattı
    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx + r, cy, cx + S(40, scale), cy});

    // Kol (açık = yatay = boru hattıyla hizalı)
    g.lines.push_back({cx, cy - r, cx, cy + r + S(10, scale)});

    g.connections.push_back({cx - S(40, scale), cy, "A"});
    g.connections.push_back({cx + S(40, scale), cy, "B"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeWaterMeter(double scale,
                                                     double cx, double cy) {
    // Su sayacı: dikdörtgen + "m³" etiketi + boru hattı
    SymbolGeometry g;
    g.label = "M";

    double hw = S(25, scale);
    double hh = S(15, scale);

    g.rects.push_back({cx - hw, cy - hh, hw*2, hh*2});

    // Boru hattı
    g.lines.push_back({cx - S(40, scale), cy, cx - hw, cy});
    g.lines.push_back({cx + hw, cy, cx + S(40, scale), cy});

    // Merkez ölçek işaretleri
    for (int i = -2; i <= 2; ++i) {
        double tx = cx + i * S(8, scale);
        g.lines.push_back({tx, cy - hh + S(3, scale), tx, cy - S(3, scale)});
    }

    g.connections.push_back({cx - S(40, scale), cy, "Giriş"});
    g.connections.push_back({cx + S(40, scale), cy, "Çıkış"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeDrain(double scale,
                                                double cx, double cy) {
    // Pis su tahliye: daire içinde X
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

SymbolGeometry FixtureSymbolLibrary::MakeSource(double scale,
                                                 double cx, double cy) {
    // Şebeke girişi: üçgen + daire
    SymbolGeometry g;
    g.label = "Ş";

    double r = S(15, scale);
    g.circles.push_back({cx, cy, r});

    // Ok (sağa doğru besleme)
    double aw = S(20, scale);
    g.lines.push_back({cx - S(40, scale), cy, cx - r, cy});
    g.lines.push_back({cx - S(40, scale), cy - S(8, scale),
                        cx - S(22, scale), cy});
    g.lines.push_back({cx - S(40, scale), cy + S(8, scale),
                        cx - S(22, scale), cy});

    g.connections.push_back({cx + r, cy, "Çıkış"});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeJunction(double scale,
                                                   double cx, double cy) {
    // Bağlantı noktası: dolu küçük daire
    SymbolGeometry g;
    g.label = "";
    g.circles.push_back({cx, cy, S(5, scale)});
    g.connections.push_back({cx, cy, ""});
    return g;
}

SymbolGeometry FixtureSymbolLibrary::MakeAppliance(double scale,
                                                    double cx, double cy,
                                                    const std::string& label) {
    // Beyaz eşya bağlantısı: kare + etiket
    SymbolGeometry g;
    g.label = label;

    double hw = S(35, scale);
    g.rects.push_back({cx - hw, cy - hw, hw*2, hw*2});

    // İç çizgi deseni
    g.lines.push_back({cx - hw + S(5, scale), cy,
                        cx + hw - S(5, scale), cy});
    g.lines.push_back({cx, cy - hw + S(5, scale),
                        cx, cy + hw - S(5, scale)});

    g.connections.push_back({cx, cy - hw, "Su"});
    g.connections.push_back({cx, cy + hw, "Pis"});
    return g;
}

// ═══════════════════════════════════════════════════════════
//  ÇIKTI FONKSİYONLARI
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
        // SVG arc path
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
        // Etiket — dolu siyah metin, sembolün üstünde
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

    return result;
}

} // namespace cad
} // namespace vkt
