/**
 * @file SnapOverlay.hpp
 * @brief Snap görsel göstergesi — Vulkan viewport üzerinde şeffaf Qt overlay
 *
 * VulkanWindow container widget'ının üzerine yerleştirilen, WA_TransparentForMouseEvents
 * ile mouse olaylarını geçiren şeffaf bir QWidget. paintEvent'de QPainter ile:
 *   - Crosshair (çapraz saç teli)
 *   - Snap noktasına renkli sembol (tip başına farklı şekil/renk)
 *   - Snap tipi tooltip etiketi
 * çizer.
 */

#pragma once

#include <QWidget>
#include <QPoint>
#include <QString>
#include <QColor>
#include <vector>
#include "cad/SnapManager.hpp"

namespace vkt {
namespace ui {

class SnapOverlay : public QWidget {
    Q_OBJECT

public:
    explicit SnapOverlay(QWidget* parent);

    /** @brief Mevcut mouse dünya koordinatı ve snap sonucunu güncelle */
    void Update(const QPoint& screenPos, const cad::SnapResult& snap);

    /** @brief Snap göstergesini gizle (çizim modu dışında) */
    void Hide();

    /** @brief Crosshair rengini ayarla */
    void SetCrosshairColor(const QColor& color) { m_crosshairColor = color; }

    /** @brief Crosshair boyutunu (piksel) ayarla */
    void SetCrosshairSize(int size) { m_crosshairSize = size; }

    /** @brief CAD Text entity'leri için ekran-uzayı etiketi */
    struct TextLabel {
        QPointF pos;            ///< Ekran koordinatı (pixel) — effective alignment anchor
        QString text;
        int     pixelH;         ///< Piksel yüksekliği
        QColor  color;
        double  rotDeg;         ///< Döndürme açısı (derece)
        int     hAlign = 0;     ///< 0=Left,1=Center,2=Right
        int     vAlign = 0;     ///< 0=Baseline,1=Bottom,2=Middle,3=Top
        double  maxWidthPx = 0; ///< Word-wrap genişliği (pixel). 0 = sarma yok.
    };

    /** @brief Render edilecek metin listesini güncelle (import/view değişiminde çağrılır) */
    void SetTextLabels(std::vector<TextLabel> labels) {
        m_textLabels = std::move(labels);
        update();
    }

    /** @brief Boru çizimi sırasında birinci noktadan imlece uzanan önizleme çizgisi */
    void SetRubberBand(QPoint from, QPoint to, bool active) {
        m_rubberFrom   = from;
        m_rubberTo     = to;
        m_rubberActive = active;
        update();
    }
    void ClearRubberBand() { m_rubberActive = false; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static QColor SnapColor(cad::SnapType type);
    static QString SnapLabel(cad::SnapType type);

    QPoint         m_screenPos;
    cad::SnapResult m_snapResult;
    bool           m_visible     = false;
    QColor         m_crosshairColor{80, 160, 255, 180};
    int            m_crosshairSize = 20;

    // Rubber-band önizleme çizgisi
    QPoint m_rubberFrom;
    QPoint m_rubberTo;
    bool   m_rubberActive = false;

    std::vector<TextLabel> m_textLabels;
};

} // namespace ui
} // namespace vkt
