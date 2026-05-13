/**
 * @file FixtureSymbolLibrary.hpp
 * @brief Tesisat armatür sembol kütüphanesi (DIN/ISO plan sembolleri)
 *
 * Plan görünümünde kullanılan standart semboller:
 *   - Lavabo (Washbasin)   : DIN 1986 / ISO 6309
 *   - WC (Toilet)          : DIN 1986
 *   - Duş (Shower)         : DIN 1986
 *   - Banyo (Bathtub)      : DIN 1986
 *   - Bulaşık makinesi     : generic
 *   - Çamaşır makinesi     : generic
 *   - Pompa                : DIN EN ISO 10628
 *   - Su sayacı            : generic
 *   - Vana (Valve)         : DIN EN 806
 *
 * Her sembol, ölçeklenebilir Entity primitifleri (Line, Circle, Polyline)
 * koleksiyonu olarak üretilir. Semboller 1×1 birim kutusuna normalize edilmiştir;
 * `scale` parametresi ile gerçek boyuta getirilir.
 */

#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace vkt {
namespace cad {

enum class FixtureSymbolType {
    Washbasin,          ///< Lavabo
    WC,                 ///< Klozet
    Shower,             ///< Duş teknesi
    Bathtub,            ///< Küvet
    DishwasherConnection, ///< Bulaşık makinesi bağlantısı
    WashingMachineConnection, ///< Çamaşır makinesi bağlantısı
    Pump,               ///< Pompa
    WaterMeter,         ///< Su sayacı
    GateValve,          ///< Sürgülü vana
    CheckValve,         ///< Çek valf
    BallValve,          ///< Küresel vana
    Drain,              ///< Pis su tahliye
    Source,             ///< Su kaynağı / şebeke girişi
    Junction,           ///< Bağlantı noktası
};

/**
 * @struct SymbolGeometry
 * @brief Bir armatür sembolünün geometrik primitifleri
 */
struct SymbolGeometry {
    // Primitifler doğrudan SVG path komutları olarak saklanır (taşıma kolaylığı için)
    // DXF çıktısında Entity'lere, SVG çıktısında path'e dönüştürülür
    struct Circle  { double cx, cy, r; };
    struct Line    { double x1, y1, x2, y2; };
    struct Rect    { double x, y, w, h; };
    struct Arc     { double cx, cy, r, startDeg, endDeg; };

    std::vector<Circle> circles;
    std::vector<Line>   lines;
    std::vector<Rect>   rects;
    std::vector<Arc>    arcs;

    // Bağlantı noktaları (boru bağlanır)
    struct ConnectionPoint { double x, y; std::string label; };
    std::vector<ConnectionPoint> connections;

    std::string label; ///< Kısa etiket (ör. "Lv", "WC")
};

/**
 * @class FixtureSymbolLibrary
 * @brief Tüm armatür sembollerini üreten fabrika
 *
 * Semboller 100×100 mm referans boyutuna normalize edilmiştir.
 * GetSymbol(type, scale) ile istenen boyuta ölçeklenir.
 */
class FixtureSymbolLibrary {
public:
    static FixtureSymbolLibrary& Instance();

    /**
     * @brief Sembol geometrisi al (normalize edilmiş 100×100 mm kutu)
     * @param type    Armatür tipi
     * @param scale   Ölçek faktörü (1.0 = 100×100 mm)
     * @param centerX Yerleştirme merkezi X (mm)
     * @param centerY Yerleştirme merkezi Y (mm)
     */
    SymbolGeometry GetSymbol(FixtureSymbolType type,
                             double scale  = 1.0,
                             double centerX = 0.0,
                             double centerY = 0.0) const;

    /**
     * @brief Armatür tipini string'den bul (DXF/DWG etiketlerinden eşleştirme)
     * @param fixtureLabel ör. "Lavabo", "WC", "Duş", "Bathtub"
     */
    static FixtureSymbolType FromLabel(const std::string& fixtureLabel);

    /**
     * @brief Sembolü SVG group olarak yaz
     */
    static std::string ToSVG(const SymbolGeometry& sym,
                              const std::string& color = "#000080");

    /**
     * @brief Sembolü Entity listesine dönüştür (DXF/Vulkan render için)
     * @return Line + Circle entity'leri
     */
    static std::vector<std::unique_ptr<Entity>>
        ToEntities(const SymbolGeometry& sym,
                   const std::string& layerName = "VKT-ARMATUR");

private:
    FixtureSymbolLibrary();

    using SymbolFactory = std::function<SymbolGeometry(double, double, double)>;
    std::unordered_map<int, SymbolFactory> m_factories;

    void RegisterAll();

    // Fabrika metodları (her armatür tipi için)
    static SymbolGeometry MakeWashbasin(double scale, double cx, double cy);
    static SymbolGeometry MakeWC       (double scale, double cx, double cy);
    static SymbolGeometry MakeShower   (double scale, double cx, double cy);
    static SymbolGeometry MakeBathtub  (double scale, double cx, double cy);
    static SymbolGeometry MakePump     (double scale, double cx, double cy);
    static SymbolGeometry MakeGateValve(double scale, double cx, double cy);
    static SymbolGeometry MakeCheckValve(double scale, double cx, double cy);
    static SymbolGeometry MakeBallValve(double scale, double cx, double cy);
    static SymbolGeometry MakeWaterMeter(double scale, double cx, double cy);
    static SymbolGeometry MakeDrain    (double scale, double cx, double cy);
    static SymbolGeometry MakeSource   (double scale, double cx, double cy);
    static SymbolGeometry MakeJunction (double scale, double cx, double cy);
    static SymbolGeometry MakeAppliance(double scale, double cx, double cy,
                                        const std::string& label);
};

} // namespace cad
} // namespace vkt
