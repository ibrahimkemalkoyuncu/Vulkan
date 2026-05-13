/**
 * @file PrintLayout.cpp
 * @brief A3/A4 baskı düzeni implementasyonu
 */

#include "ui/PrintLayout.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/Circle.hpp"

#include <QPrinter>
#include <QPainter>
#include <QFont>
#include <QRectF>
#include <QString>
#include <QColor>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace vkt {
namespace ui {

// ═══════════════════════════════════════════════════════════
//  YARDIMCILAR
// ═══════════════════════════════════════════════════════════

void PrintLayout::GetPaperDimensions(PaperSize size,
                                     double& widthMM, double& heightMM) {
    switch (size) {
        case PaperSize::A4_Portrait:   widthMM = 210; heightMM = 297; break;
        case PaperSize::A4_Landscape:  widthMM = 297; heightMM = 210; break;
        case PaperSize::A3_Portrait:   widthMM = 297; heightMM = 420; break;
        case PaperSize::A3_Landscape:  widthMM = 420; heightMM = 297; break;
    }
}

PrintLayout::Viewport PrintLayout::ComputeViewport(double paperW,
                                                    double paperH) const {
    Viewport vp;
    vp.x      = TB_MARGIN;
    vp.y      = TB_MARGIN;
    vp.width  = paperW - 2.0 * TB_MARGIN;
    vp.height = paperH - 2.0 * TB_MARGIN - TB_HEIGHT;
    vp.scale  = m_scale;

    if (m_autoScale && m_entities) {
        // Çizim sınır kutusunu hesapla
        double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        for (const auto& e : *m_entities) {
            if (!e) continue;
            auto bb = e->GetBounds();
            if (!bb.IsValid()) continue;
            minX = std::min(minX, bb.min.x); minY = std::min(minY, bb.min.y);
            maxX = std::max(maxX, bb.max.x); maxY = std::max(maxY, bb.max.y);
        }
        if (maxX > minX && maxY > minY) {
            double drawW = maxX - minX;
            double drawH = maxY - minY;
            // Viewport'a sığacak ölçek (1mm = 1 birim = 1mm model)
            double sx = vp.width  / drawW;
            double sy = vp.height / drawH;
            vp.scale = std::min(sx, sy) * 0.95; // %5 kenar payı
        }
    }
    return vp;
}

// ═══════════════════════════════════════════════════════════
//  PDF EXPORT (QPrinter)
// ═══════════════════════════════════════════════════════════

bool PrintLayout::ExportToPDF(const std::string& filePath) const {
    double paperW, paperH;
    GetPaperDimensions(m_paperSize, paperW, paperH);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(QString::fromStdString(filePath));

    // Kağıt boyutu
    QPageSize pageSize;
    switch (m_paperSize) {
        case PaperSize::A4_Portrait:
        case PaperSize::A4_Landscape:
            pageSize = QPageSize(QPageSize::A4); break;
        case PaperSize::A3_Portrait:
        case PaperSize::A3_Landscape:
        default:
            pageSize = QPageSize(QPageSize::A3); break;
    }
    bool landscape = (m_paperSize == PaperSize::A4_Landscape ||
                      m_paperSize == PaperSize::A3_Landscape);
    printer.setPageSize(pageSize);
    printer.setPageOrientation(landscape ? QPageLayout::Landscape
                                         : QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(0,0,0,0), QPageLayout::Millimeter);

    QPainter painter;
    if (!painter.begin(&printer)) return false;

    // Printer koordinat sistemi: 1 unit = 1 mm * DPI/25.4
    double dpi    = printer.resolution();
    double mmToPx = dpi / 25.4;

    auto toPixels = [&](double mm) { return mm * mmToPx; };

    Viewport vp = ComputeViewport(paperW, paperH);

    // ── Çerçeve ──
    painter.setPen(QPen(Qt::black, toPixels(0.5)));
    painter.drawRect(QRectF(toPixels(TB_MARGIN), toPixels(TB_MARGIN),
                            toPixels(vp.width),  toPixels(vp.height)));

    // ── Başlık Bloğu (sağ alt) ──
    double tbX = TB_MARGIN;
    double tbY = paperH - TB_MARGIN - TB_HEIGHT;
    double tbW = paperW - 2.0 * TB_MARGIN;

    painter.setPen(QPen(Qt::black, toPixels(0.4)));
    painter.drawRect(QRectF(toPixels(tbX), toPixels(tbY),
                            toPixels(tbW), toPixels(TB_HEIGHT)));

    // Dikey ayraçlar
    double col1X = tbX + tbW - TB_COL1 - TB_COL2;
    double col2X = tbX + tbW - TB_COL1;
    painter.drawLine(QPointF(toPixels(col1X), toPixels(tbY)),
                     QPointF(toPixels(col1X), toPixels(tbY + TB_HEIGHT)));
    painter.drawLine(QPointF(toPixels(col2X), toPixels(tbY)),
                     QPointF(toPixels(col2X), toPixels(tbY + TB_HEIGHT)));

    // Yatay ayraç (ortada)
    double midY = tbY + TB_HEIGHT / 2.0;
    painter.drawLine(QPointF(toPixels(tbX),    toPixels(midY)),
                     QPointF(toPixels(tbX+tbW), toPixels(midY)));

    // Metin: küçük font
    QFont smallFont("Arial", static_cast<int>(toPixels(2.5)));
    QFont boldFont ("Arial", static_cast<int>(toPixels(3.5)));
    boldFont.setBold(true);

    auto drawField = [&](double x, double y, double w, double h,
                         const std::string& label, const std::string& value,
                         bool bold = false) {
        double px = toPixels(x), py = toPixels(y);
        double pw = toPixels(w), ph = toPixels(h);
        painter.setFont(smallFont);
        painter.setPen(Qt::darkGray);
        painter.drawText(QRectF(px+2, py+1, pw-4, ph*0.4),
                         Qt::AlignLeft | Qt::AlignTop,
                         QString::fromStdString(label));
        painter.setFont(bold ? boldFont : smallFont);
        painter.setPen(Qt::black);
        painter.drawText(QRectF(px+2, py + ph*0.4, pw-4, ph*0.55),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromStdString(value));
    };

    double hw = TB_HEIGHT / 2.0;
    // Sol blok — proje adı (büyük alan)
    double leftW = tbW - TB_COL1 - TB_COL2;
    drawField(tbX, tbY,    leftW, hw,  "PROJE",      m_titleBlock.projectName, true);
    drawField(tbX, midY,   leftW, hw,  "PAFTA ADI",  m_titleBlock.drawingTitle);

    // Orta blok
    drawField(col1X, tbY,  TB_COL2, hw, "FİRMA",     m_titleBlock.company);
    drawField(col1X, midY, TB_COL2, hw, "STANDART",  m_titleBlock.standard);

    // Sağ blok
    drawField(col2X, tbY,      TB_COL1, hw*0.5, "PAFTA NO",   m_titleBlock.drawingNumber);
    drawField(col2X, tbY+hw*0.5, TB_COL1, hw*0.5, "REV",      m_titleBlock.revision);
    drawField(col2X, midY,     TB_COL1, hw*0.5, "ÖLÇEK",      m_titleBlock.scale);
    drawField(col2X, midY+hw*0.5, TB_COL1, hw*0.5, "TARİH",   m_titleBlock.date);

    // ── Viewport: çizim entity'leri ──
    painter.setClipRect(QRectF(toPixels(vp.x), toPixels(vp.y),
                               toPixels(vp.width), toPixels(vp.height)));
    painter.translate(toPixels(vp.x), toPixels(vp.y));

    // Entity'leri çiz
    if (m_entities) {
        for (const auto& e : *m_entities) {
            if (!e) continue;
            auto c = e->GetColor();
            QColor qc(c.r, c.g, c.b);
            painter.setPen(QPen(qc, toPixels(0.2)));

            switch (e->GetType()) {
                case cad::EntityType::Line: {
                    const auto& ln = static_cast<const cad::Line&>(*e);
                    painter.drawLine(
                        QPointF(toPixels(ln.GetStart().x * vp.scale),
                                toPixels(ln.GetStart().y * vp.scale)),
                        QPointF(toPixels(ln.GetEnd().x   * vp.scale),
                                toPixels(ln.GetEnd().y   * vp.scale)));
                    break;
                }
                case cad::EntityType::Circle: {
                    const auto& cr = static_cast<const cad::Circle&>(*e);
                    double r = cr.GetRadius() * vp.scale;
                    painter.drawEllipse(
                        QPointF(toPixels(cr.GetCenter().x * vp.scale),
                                toPixels(cr.GetCenter().y * vp.scale)),
                        toPixels(r), toPixels(r));
                    break;
                }
                case cad::EntityType::Polyline: {
                    const auto& pl = static_cast<const cad::Polyline&>(*e);
                    const auto& verts = pl.GetVertices();
                    if (verts.size() < 2) break;
                    for (size_t i = 0; i + 1 < verts.size(); ++i) {
                        painter.drawLine(
                            QPointF(toPixels(verts[i].pos.x   * vp.scale),
                                    toPixels(verts[i].pos.y   * vp.scale)),
                            QPointF(toPixels(verts[i+1].pos.x * vp.scale),
                                    toPixels(verts[i+1].pos.y * vp.scale)));
                    }
                    if (pl.IsClosed() && verts.size() >= 3) {
                        painter.drawLine(
                            QPointF(toPixels(verts.back().pos.x  * vp.scale),
                                    toPixels(verts.back().pos.y  * vp.scale)),
                            QPointF(toPixels(verts.front().pos.x * vp.scale),
                                    toPixels(verts.front().pos.y * vp.scale)));
                    }
                    break;
                }
                default: break;
            }
        }
    }

    // Network borularını çiz
    if (m_network) {
        for (const auto& [eid, edge] : m_network->GetEdgeMap()) {
            const auto* nA = m_network->GetNode(edge.nodeA);
            const auto* nB = m_network->GetNode(edge.nodeB);
            if (!nA || !nB) continue;

            QColor qc;
            switch (edge.type) {
                case mep::EdgeType::Supply:   qc = QColor(0,  80, 180); break;
                case mep::EdgeType::Drainage: qc = QColor(140, 90, 25); break;
                case mep::EdgeType::Vent:     qc = QColor(50, 160, 50); break;
            }
            painter.setPen(QPen(qc, toPixels(0.4)));
            painter.drawLine(
                QPointF(toPixels(nA->position.x * vp.scale),
                        toPixels(nA->position.y * vp.scale)),
                QPointF(toPixels(nB->position.x * vp.scale),
                        toPixels(nB->position.y * vp.scale)));
        }
    }

    painter.end();
    return true;
}

// ═══════════════════════════════════════════════════════════
//  SVG EXPORT
// ═══════════════════════════════════════════════════════════

bool PrintLayout::ExportToSVG(const std::string& filePath) const {
    double paperW, paperH;
    GetPaperDimensions(m_paperSize, paperW, paperH);

    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\""
        << " width=\"" << paperW << "mm\""
        << " height=\"" << paperH << "mm\""
        << " viewBox=\"0 0 " << paperW << " " << paperH << "\""
        << " style=\"background:#fff;font-family:Arial,sans-serif;\">\n";

    Viewport vp = ComputeViewport(paperW, paperH);

    // Çerçeve
    out << "<rect x=\"" << TB_MARGIN << "\" y=\"" << TB_MARGIN << "\""
        << " width=\"" << vp.width << "\" height=\"" << vp.height << "\""
        << " fill=\"none\" stroke=\"#000\" stroke-width=\"0.5\"/>\n";

    // Entity'ler
    WriteSVGViewport(out, vp);

    // Başlık bloğu
    WriteSVGTitleBlock(out, paperW, paperH);

    out << "</svg>\n";
    return true;
}

void PrintLayout::WriteSVGTitleBlock(std::ostream& out,
                                     double paperW, double paperH) const {
    double tbX = TB_MARGIN;
    double tbY = paperH - TB_MARGIN - TB_HEIGHT;
    double tbW = paperW - 2.0 * TB_MARGIN;
    double hw  = TB_HEIGHT / 2.0;
    double col1X = tbX + tbW - TB_COL1 - TB_COL2;
    double col2X = tbX + tbW - TB_COL1;

    // Dış kutu
    out << "<rect x=\"" << tbX << "\" y=\"" << tbY << "\""
        << " width=\"" << tbW << "\" height=\"" << TB_HEIGHT << "\""
        << " fill=\"#f8f8f8\" stroke=\"#000\" stroke-width=\"0.4\"/>\n";

    // Yatay ayraç
    out << "<line x1=\"" << tbX    << "\" y1=\"" << tbY+hw << "\""
        << " x2=\"" << tbX+tbW << "\" y2=\"" << tbY+hw << "\""
        << " stroke=\"#000\" stroke-width=\"0.3\"/>\n";

    // Dikey ayraçlar
    for (double cx : {col1X, col2X}) {
        out << "<line x1=\"" << cx << "\" y1=\"" << tbY << "\""
            << " x2=\"" << cx << "\" y2=\"" << tbY+TB_HEIGHT << "\""
            << " stroke=\"#000\" stroke-width=\"0.3\"/>\n";
    }

    auto field = [&](double x, double y, const std::string& lbl,
                     const std::string& val, bool bold = false) {
        out << "<text x=\"" << x+1 << "\" y=\"" << y+3.5 << "\""
            << " style=\"font-size:2.2px;fill:#666;\">" << lbl << "</text>\n";
        out << "<text x=\"" << x+1 << "\" y=\"" << y+hw*0.85 << "\""
            << " style=\"font-size:3px;fill:#000;"
            << (bold ? "font-weight:bold;" : "") << "\">"
            << val << "</text>\n";
    };

    double leftW = tbW - TB_COL1 - TB_COL2;
    field(tbX,    tbY,    "PROJE",     m_titleBlock.projectName,  true);
    field(tbX,    tbY+hw, "PAFTA ADI", m_titleBlock.drawingTitle);
    field(col1X,  tbY,    "FİRMA",     m_titleBlock.company);
    field(col1X,  tbY+hw, "STANDART",  m_titleBlock.standard);
    field(col2X,  tbY,    "PAFTA NO",  m_titleBlock.drawingNumber + " / Rev:" + m_titleBlock.revision);
    field(col2X,  tbY+hw, "ÖLÇEK / TARİH", m_titleBlock.scale + "  " + m_titleBlock.date);
}

void PrintLayout::WriteSVGViewport(std::ostream& out,
                                   const Viewport& vp) const {
    // Clip rect
    out << "<clipPath id=\"vpClip\">"
        << "<rect x=\"" << vp.x << "\" y=\"" << vp.y
        << "\" width=\"" << vp.width << "\" height=\"" << vp.height << "\"/>"
        << "</clipPath>\n";
    out << "<g clip-path=\"url(#vpClip)\" transform=\"translate("
        << vp.x << "," << vp.y << ")\">\n";

    if (m_entities) {
        for (const auto& e : *m_entities) {
            if (!e) continue;
            WriteSVGEntity(out, *e, 0.0, 0.0, vp.scale);
        }
    }
    if (m_network) {
        WriteSVGNetwork(out, 0.0, 0.0, vp.scale);
    }

    out << "</g>\n";
}

void PrintLayout::WriteSVGEntity(std::ostream& out, const cad::Entity& e,
                                  double ox, double oy, double sc) const {
    auto c = e.GetColor();
    std::string stroke = "rgb(" + std::to_string(c.r) + "," +
                                   std::to_string(c.g) + "," +
                                   std::to_string(c.b) + ")";

    switch (e.GetType()) {
        case cad::EntityType::Line: {
            const auto& ln = static_cast<const cad::Line&>(e);
            out << "<line x1=\"" << ox + ln.GetStart().x * sc
                << "\" y1=\""   << oy + ln.GetStart().y * sc
                << "\" x2=\""   << ox + ln.GetEnd().x   * sc
                << "\" y2=\""   << oy + ln.GetEnd().y   * sc
                << "\" stroke=\"" << stroke << "\" stroke-width=\"0.2\"/>\n";
            break;
        }
        case cad::EntityType::Circle: {
            const auto& cr = static_cast<const cad::Circle&>(e);
            out << "<circle cx=\"" << ox + cr.GetCenter().x * sc
                << "\" cy=\""      << oy + cr.GetCenter().y * sc
                << "\" r=\""       << cr.GetRadius() * sc
                << "\" fill=\"none\" stroke=\"" << stroke << "\" stroke-width=\"0.2\"/>\n";
            break;
        }
        case cad::EntityType::Polyline: {
            const auto& pl = static_cast<const cad::Polyline&>(e);
            const auto& verts = pl.GetVertices();
            if (verts.size() < 2) break;
            out << "<polyline points=\"";
            for (const auto& v : verts)
                out << (ox + v.pos.x * sc) << "," << (oy + v.pos.y * sc) << " ";
            if (pl.IsClosed())
                out << (ox + verts[0].pos.x * sc) << "," << (oy + verts[0].pos.y * sc);
            out << "\" fill=\"none\" stroke=\"" << stroke << "\" stroke-width=\"0.2\"/>\n";
            break;
        }
        default: break;
    }
}

void PrintLayout::WriteSVGNetwork(std::ostream& out,
                                   double ox, double oy, double sc) const {
    if (!m_network) return;
    for (const auto& [eid, edge] : m_network->GetEdgeMap()) {
        const auto* nA = m_network->GetNode(edge.nodeA);
        const auto* nB = m_network->GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        std::string col;
        switch (edge.type) {
            case mep::EdgeType::Supply:   col = "#0050b4"; break;
            case mep::EdgeType::Drainage: col = "#8c5a19"; break;
            case mep::EdgeType::Vent:     col = "#329632"; break;
        }
        out << "<line x1=\"" << ox + nA->position.x * sc
            << "\" y1=\""   << oy + nA->position.y * sc
            << "\" x2=\""   << ox + nB->position.x * sc
            << "\" y2=\""   << oy + nB->position.y * sc
            << "\" stroke=\"" << col << "\" stroke-width=\"0.4\"/>\n";
    }
}

} // namespace ui
} // namespace vkt
