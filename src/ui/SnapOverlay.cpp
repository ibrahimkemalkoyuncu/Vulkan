/**
 * @file SnapOverlay.cpp
 * @brief Snap görsel göstergesi implementasyonu
 */

#include "ui/SnapOverlay.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QPen>

namespace vkt {
namespace ui {

SnapOverlay::SnapOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    raise();
}

void SnapOverlay::Update(const QPoint& screenPos, const cad::SnapResult& snap) {
    m_screenPos  = screenPos;
    m_snapResult = snap;
    m_visible    = true;
    update();
}

void SnapOverlay::Hide() {
    m_visible = false;
    update();
}

// ═══════════════════════════════════════════════════════════
//  PAINT
// ═══════════════════════════════════════════════════════════

void SnapOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // ── CAD Text entity'leri ─────────────────────────────────
    for (const auto& lbl : m_textLabels) {
        if (lbl.text.isEmpty()) continue;
        int fh = std::max(6, lbl.pixelH);
        QFont tf("Arial");
        tf.setPixelSize(fh);
        p.setFont(tf);

        QFontMetrics fm(tf);
        // Split on \n (MTEXT \P paragraph separator)
        QStringList lines = lbl.text.split('\n', Qt::SkipEmptyParts);
        if (lines.isEmpty()) continue;

        int lineH    = fm.height();
        int totalH   = lines.size() * lineH;

        // blockTop: y of top edge of text block in anchor-local space
        // Qt drawText(x, y) → y is the baseline
        int blockTop = 0;
        if      (lbl.vAlign == 0) blockTop = -fm.ascent();           // Baseline at anchor
        else if (lbl.vAlign == 1) blockTop = -totalH;                // Bottom of block at anchor
        else if (lbl.vAlign == 2) blockTop = -(totalH / 2);          // Middle of block at anchor
        else if (lbl.vAlign == 3) blockTop = 0;                      // Top of block at anchor

        p.save();
        p.translate(lbl.pos);
        // CAD CCW angle in Y-up space → CW in screen Y-down space → Qt positive rotate
        p.rotate(lbl.rotDeg);
        p.setPen(lbl.color);

        for (int li = 0; li < lines.size(); ++li) {
            int baseline = blockTop + fm.ascent() + li * lineH;
            int tw = fm.horizontalAdvance(lines[li]);
            int dx = 0;
            if      (lbl.hAlign == 1 || lbl.hAlign == 4) dx = -tw / 2;
            else if (lbl.hAlign == 2)                     dx = -tw;
            p.drawText(dx, baseline, lines[li]);
        }
        p.restore();
    }

    if (!m_visible) return;

    const int   cx    = m_screenPos.x();
    const int   cy    = m_screenPos.y();
    const int   hs    = m_crosshairSize;         // half-size
    const QColor snapCol = SnapColor(m_snapResult.type);

    // ── Crosshair ──────────────────────────────────────────
    {
        QPen pen(m_crosshairColor, 1.0f, Qt::SolidLine);
        p.setPen(pen);
        const int gap = (m_snapResult.IsValid()) ? 6 : 0; // snap varken ortayı boş bırak
        // Yatay kollar
        p.drawLine(cx - hs, cy, cx - gap, cy);
        p.drawLine(cx + gap, cy, cx + hs, cy);
        // Dikey kollar
        p.drawLine(cx, cy - hs, cx, cy - gap);
        p.drawLine(cx, cy + gap, cx, cy + hs);
    }

    if (!m_snapResult.IsValid()) return;

    // ── Snap sembolü ───────────────────────────────────────
    const int sym = 7; // sembol yarı boyutu

    QPen symPen(snapCol, 1.5f);
    p.setPen(symPen);
    p.setBrush(Qt::NoBrush);

    switch (m_snapResult.type) {

        case cad::SnapType::Endpoint:
            // Sarı kare
            p.drawRect(cx - sym, cy - sym, sym * 2, sym * 2);
            break;

        case cad::SnapType::Midpoint:
            // Yeşil üçgen
            {
                QPointF tri[3] = {
                    {(double)(cx),        (double)(cy - sym)},
                    {(double)(cx - sym),  (double)(cy + sym)},
                    {(double)(cx + sym),  (double)(cy + sym)},
                };
                p.drawPolygon(tri, 3);
            }
            break;

        case cad::SnapType::Center:
            // Cyan daire
            p.drawEllipse(QPoint(cx, cy), sym, sym);
            // Artı işareti
            p.drawLine(cx - sym, cy, cx + sym, cy);
            p.drawLine(cx, cy - sym, cx, cy + sym);
            break;

        case cad::SnapType::Intersection:
            // Magenta X
            p.drawLine(cx - sym, cy - sym, cx + sym, cy + sym);
            p.drawLine(cx - sym, cy + sym, cx + sym, cy - sym);
            break;

        case cad::SnapType::Perpendicular:
            // Mavi dik açı sembolü
            p.drawLine(cx - sym, cy + sym, cx - sym, cy - sym);
            p.drawLine(cx - sym, cy + sym, cx + sym, cy + sym);
            // Dik açı küçük karesi
            p.drawLine(cx - sym + 4, cy + sym, cx - sym + 4, cy + sym - 4);
            p.drawLine(cx - sym,     cy + sym - 4, cx - sym + 4, cy + sym - 4);
            break;

        case cad::SnapType::Nearest:
            // Açık mavi elmas
            {
                QPointF dia[4] = {
                    {(double)(cx),        (double)(cy - sym)},
                    {(double)(cx + sym),  (double)(cy)},
                    {(double)(cx),        (double)(cy + sym)},
                    {(double)(cx - sym),  (double)(cy)},
                };
                p.drawPolygon(dia, 4);
            }
            break;

        case cad::SnapType::Grid:
            // Beyaz artı nokta
            p.setPen(QPen(snapCol, 2.5f));
            p.drawPoint(cx, cy);
            p.setPen(symPen);
            p.drawLine(cx - sym, cy, cx + sym, cy);
            p.drawLine(cx, cy - sym, cx, cy + sym);
            break;

        default:
            break;
    }

    // ── Tooltip etiketi ────────────────────────────────────
    {
        const QString label = SnapLabel(m_snapResult.type);
        if (label.isEmpty()) return;

        QFont font("Consolas", 8);
        font.setBold(true);
        p.setFont(font);

        const QFontMetrics fm(font);
        const QRect textRect = fm.boundingRect(label);
        const int tw = textRect.width() + 6;
        const int th = textRect.height() + 4;
        const int tx = cx + sym + 4;
        int ty = cy - th / 2;

        // Arkaplan
        p.setBrush(QColor(0, 0, 0, 160));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(tx, ty, tw, th, 2, 2);

        // Metin
        p.setPen(snapCol);
        p.drawText(tx + 3, ty + th - 5, label);
    }
}

// ═══════════════════════════════════════════════════════════
//  STATIC HELPERS
// ═══════════════════════════════════════════════════════════

QColor SnapOverlay::SnapColor(cad::SnapType type) {
    switch (type) {
        case cad::SnapType::Endpoint:      return QColor(255, 220,  50);  // sarı
        case cad::SnapType::Midpoint:      return QColor( 80, 200,  80);  // yeşil
        case cad::SnapType::Center:        return QColor( 50, 220, 220);  // cyan
        case cad::SnapType::Intersection:  return QColor(220,  80, 220);  // magenta
        case cad::SnapType::Perpendicular: return QColor( 80, 140, 255);  // mavi
        case cad::SnapType::Nearest:       return QColor(180, 220, 255);  // açık mavi
        case cad::SnapType::Grid:          return QColor(220, 220, 220);  // beyaz
        default:                           return QColor(180, 180, 180);
    }
}

QString SnapOverlay::SnapLabel(cad::SnapType type) {
    switch (type) {
        case cad::SnapType::Endpoint:      return "UÇ";
        case cad::SnapType::Midpoint:      return "ORTA";
        case cad::SnapType::Center:        return "MERKEZ";
        case cad::SnapType::Intersection:  return "KESİŞİM";
        case cad::SnapType::Perpendicular: return "DİK";
        case cad::SnapType::Nearest:       return "YAKIN";
        case cad::SnapType::Grid:          return "GRİD";
        default:                           return {};
    }
}

} // namespace ui
} // namespace vkt
