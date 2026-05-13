/**
 * @file RiserDiagram.hpp
 * @brief Kolon Şeması (Riser Diagram) Üretici
 *
 * FINE SANI'nin en kritik özelliği: tesisat boru kolonlarını
 * kat kat dikey şema olarak üretir.
 *
 * Çıktı formatları:
 *   1. SVG  — tarayıcıda/raporda gösterim
 *   2. Vertex listesi — Vulkan render için (RiserVertex)
 *   3. Metin özeti — konsol / log
 */

#pragma once

#include "NetworkGraph.hpp"
#include "core/FloorManager.hpp"
#include <string>
#include <vector>

namespace vkt {
namespace mep {

// ─────────────────────────────────────────────
//  Render primitifleri (Vulkan'a beslenecek)
// ─────────────────────────────────────────────

struct RiserVertex {
    float x, y;          ///< Şema koordinatı (piksel)
    float r, g, b;        ///< Renk
};

struct RiserLine {
    RiserVertex a, b;
    float thickness = 1.0f;
};

struct RiserLabel {
    float x, y;
    std::string text;
    float fontSize = 10.0f;
};

// ─────────────────────────────────────────────
//  Şema veri modeli
// ─────────────────────────────────────────────

/**
 * @struct RiserColumn
 * @brief Tek bir dikey boru kolonu
 *
 * Bir kolonda her kat için:
 *   - boru çapı
 *   - kümülatif DU / debi
 *   - kolona bağlı armatürler
 */
struct RiserColumn {
    uint32_t sourceNodeId = 0;          ///< Kolonun başlayacağı kaynak/pompa node'u
    std::string label;                  ///< Kolon etiketi ("Kolon 1", "K-1A" ...)
    EdgeType pipeType = EdgeType::Supply;

    struct FloorSegment {
        int   floorIndex  = 0;
        float diameter_mm = 0.0f;
        float flowRate_Ls = 0.0f;
        float cumulativeDU = 0.0f;
        std::vector<std::string> fixtureLabels; ///< Bu katta bağlı armatürler
    };

    std::vector<FloorSegment> segments; ///< Altan üste sıralı
};

/**
 * @struct RiserDiagramData
 * @brief Üretilen tüm şema verisi
 */
struct RiserDiagramData {
    std::vector<RiserColumn> columns;
    std::vector<RiserLine>   lines;
    std::vector<RiserLabel>  labels;
    float canvasWidth  = 800.0f;
    float canvasHeight = 600.0f;
};

// ─────────────────────────────────────────────
//  Üretici sınıf
// ─────────────────────────────────────────────

class RiserDiagram {
public:
    explicit RiserDiagram(const NetworkGraph& network,
                          const core::FloorManager& floors);

    /**
     * @brief Şema verilerini ağdan üret
     *
     * FloorManager bilgisi yoksa Z koordinatından kat tahmin edilir
     * (her 3m = 1 kat).
     */
    RiserDiagramData Generate() const;

    /**
     * @brief SVG formatında kolon şeması
     * @param data Generate() sonucu
     * @return SVG string (dosyaya yazılabilir veya raporun içine gömülebilir)
     */
    std::string ToSVG(const RiserDiagramData& data) const;

    /**
     * @brief Metin özeti — log / konsol
     */
    std::string ToText(const RiserDiagramData& data) const;

private:
    const NetworkGraph&       m_network;
    const core::FloorManager& m_floors;

    // Ağdan kolon topolojisini çıkar
    std::vector<RiserColumn> ExtractColumns() const;

    // Bir kolona ait edge zincirini DFS ile topla
    void TraceColumn(uint32_t startNodeId, EdgeType type,
                     RiserColumn& col) const;

    // Node'un kat indeksini bul (FloorManager yoksa Z'den tahmin)
    int InferFloor(uint32_t nodeId) const;

    // Şema koordinatları
    static constexpr float COL_SPACING  = 180.0f; ///< Kolonlar arası yatay mesafe (px)
    static constexpr float FLOOR_HEIGHT = 100.0f; ///< Kat yüksekliği (px)
    static constexpr float MARGIN_LEFT  =  80.0f;
    static constexpr float MARGIN_TOP   =  60.0f;
};

} // namespace mep
} // namespace vkt
