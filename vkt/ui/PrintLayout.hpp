/**
 * @file PrintLayout.hpp
 * @brief A3/A4 baskı düzeni — başlık bloğu, ölçek, PDF çıktı
 *
 * AutoCAD "paper space" eşdeğeri:
 *   - Kağıt boyutu seçimi (A4/A3)
 *   - ISO 7200 başlık bloğu (proje adı, çizen, tarih, ölçek, pafta no)
 *   - Viewport: çizimi ölçekle kağıda yerleştirme
 *   - Qt6 QPrinter ile PDF veya yazıcı çıktısı
 */

#pragma once

#include "cad/Entity.hpp"
#include "mep/NetworkGraph.hpp"
#include <vector>
#include <string>
#include <memory>

namespace vkt {
namespace ui {

enum class PaperSize {
    A4_Portrait,    ///< 210 × 297 mm
    A4_Landscape,   ///< 297 × 210 mm
    A3_Portrait,    ///< 297 × 420 mm
    A3_Landscape    ///< 420 × 297 mm
};

/**
 * @struct TitleBlock
 * @brief ISO 7200 başlık bloğu verileri
 */
struct TitleBlock {
    std::string projectName;    ///< Proje adı
    std::string drawingTitle;   ///< Pafta başlığı (ör. "Bodrum Kat Tesisat")
    std::string drawingNumber;  ///< Pafta numarası (ör. "P-001")
    std::string revision;       ///< Revizyon (ör. "A")
    std::string drawnBy;        ///< Çizen (initials)
    std::string checkedBy;      ///< Kontrol eden
    std::string date;           ///< Tarih (ör. "2026-04-29")
    std::string scale;          ///< Ölçek (ör. "1:50")
    std::string company;        ///< Firma adı
    std::string standard = "TS EN 806-3 / EN 12056-2";
};

/**
 * @class PrintLayout
 * @brief Çizimi kağıda yerleştirip PDF'e yazar
 */
class PrintLayout {
public:
    PrintLayout() = default;

    // Sayfa ve başlık
    void SetPaperSize(PaperSize size)         { m_paperSize = size; }
    void SetTitleBlock(const TitleBlock& tb)  { m_titleBlock = tb; }
    void SetScale(double scale)               { m_scale = scale; }   ///< ör. 0.02 = 1:50
    void SetAutoScale(bool enable)            { m_autoScale = enable; }

    // Çizim verisi
    void SetEntities(const std::vector<std::unique_ptr<cad::Entity>>* entities) {
        m_entities = entities;
    }
    void SetNetwork(const mep::NetworkGraph* network) { m_network = network; }

    /**
     * @brief PDF olarak kaydet (QPrinter + QTextDocument + QPainter)
     * @return Başarılı ise true
     */
    bool ExportToPDF(const std::string& filePath) const;

    /**
     * @brief SVG olarak kaydet (başlık bloğu dahil)
     */
    bool ExportToSVG(const std::string& filePath) const;

    // Kağıt boyutunu mm cinsinden döndür
    static void GetPaperDimensions(PaperSize size,
                                   double& widthMM, double& heightMM);

private:
    PaperSize   m_paperSize  = PaperSize::A3_Landscape;
    TitleBlock  m_titleBlock;
    double      m_scale      = 0.02;    // 1:50 varsayılan
    bool        m_autoScale  = true;

    const std::vector<std::unique_ptr<cad::Entity>>* m_entities = nullptr;
    const mep::NetworkGraph*                          m_network  = nullptr;

    // Başlık bloğu boyutları (mm)
    static constexpr double TB_HEIGHT  = 40.0;   ///< Başlık bloğu yüksekliği
    static constexpr double TB_MARGIN  = 10.0;   ///< Kenar boşluğu
    static constexpr double TB_COL1    = 80.0;   ///< 1. kolon genişliği
    static constexpr double TB_COL2    = 60.0;   ///< 2. kolon genişliği

    // Viewport: çizimin kağıttaki alanı
    struct Viewport {
        double x, y, width, height; ///< mm cinsinden
        double scale;
    };

    Viewport ComputeViewport(double paperW, double paperH) const;

    void WriteSVGTitleBlock(std::ostream& out,
                            double paperW, double paperH) const;
    void WriteSVGViewport(std::ostream& out,
                          const Viewport& vp) const;
    void WriteSVGEntity(std::ostream& out,
                        const cad::Entity& e,
                        double offsetX, double offsetY, double scale) const;
    void WriteSVGNetwork(std::ostream& out,
                         double offsetX, double offsetY, double scale) const;
};

} // namespace ui
} // namespace vkt
