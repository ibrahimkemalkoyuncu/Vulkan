/**
 * @file RiserDiagram.cpp
 * @brief Kolon Şeması üretici implementasyonu
 */

#include "mep/RiserDiagram.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <cmath>

namespace vkt {
namespace mep {

// Boru tipine göre renk (RGB 0–1)
static void PipeColor(EdgeType type, float& r, float& g, float& b) {
    switch (type) {
        case EdgeType::Supply:   r=0.10f; g=0.47f; b=0.71f; break; // mavi
        case EdgeType::HotWater: r=0.80f; g=0.15f; b=0.10f; break; // kırmızı
        case EdgeType::Drainage: r=0.55f; g=0.35f; b=0.10f; break; // kahverengi
        case EdgeType::Vent:     r=0.30f; g=0.70f; b=0.30f; break; // yeşil
        default:                 r=0.50f; g=0.50f; b=0.50f; break;
    }
}

RiserDiagram::RiserDiagram(const NetworkGraph& network,
                            const core::FloorManager& floors)
    : m_network(network), m_floors(floors) {}

// ═══════════════════════════════════════════════════════════
//  ANA ÜRETİCİ
// ═══════════════════════════════════════════════════════════

RiserDiagramData RiserDiagram::Generate() const {
    RiserDiagramData data;
    data.columns = ExtractColumns();

    // Kat sayısını belirle
    int maxFloor = 0, minFloor = 0;
    for (const auto& col : data.columns) {
        for (const auto& seg : col.segments) {
            maxFloor = std::max(maxFloor, seg.floorIndex);
            minFloor = std::min(minFloor, seg.floorIndex);
        }
    }
    int numFloors = maxFloor - minFloor + 1;

    data.canvasWidth  = MARGIN_LEFT + data.columns.size() * COL_SPACING + 60.0f;
    data.canvasHeight = MARGIN_TOP  + numFloors * FLOOR_HEIGHT + 60.0f;

    // Kat çizgisi etiketleri (sol kenar)
    for (int fi = minFloor; fi <= maxFloor; ++fi) {
        float y = MARGIN_TOP + (maxFloor - fi) * FLOOR_HEIGHT;

        RiserLabel lbl;
        lbl.x = 5.0f;
        lbl.y = y + FLOOR_HEIGHT / 2.0f;
        lbl.fontSize = 9.0f;

        const core::Floor* fl = m_floors.GetFloor(fi);
        lbl.text = fl ? fl->label : (fi == 0 ? "Zemin" : std::to_string(fi) + ".Kat");
        data.labels.push_back(lbl);

        // Yatay kat çizgisi
        RiserLine hLine;
        hLine.a = {MARGIN_LEFT - 10.0f, y, 0.8f, 0.8f, 0.8f};
        hLine.b = {data.canvasWidth,     y, 0.8f, 0.8f, 0.8f};
        hLine.thickness = 0.5f;
        data.lines.push_back(hLine);
    }

    // Kolon dikey çizgileri + segment etiketleri
    for (size_t ci = 0; ci < data.columns.size(); ++ci) {
        const auto& col = data.columns[ci];
        float cx = MARGIN_LEFT + ci * COL_SPACING + COL_SPACING / 2.0f;

        // Kolon başlığı
        RiserLabel colLbl;
        colLbl.x = cx;
        colLbl.y = MARGIN_TOP - 15.0f;
        colLbl.text = col.label;
        colLbl.fontSize = 10.0f;
        data.labels.push_back(colLbl);

        float r, g, b;
        PipeColor(col.pipeType, r, g, b);

        // Her segment için dikey boru çizgisi
        for (const auto& seg : col.segments) {
            float yTop    = MARGIN_TOP + (maxFloor - seg.floorIndex)     * FLOOR_HEIGHT;
            float yBottom = MARGIN_TOP + (maxFloor - seg.floorIndex + 1) * FLOOR_HEIGHT;

            float thick = std::max(1.5f, seg.diameter_mm / 20.0f); // çap → kalınlık

            RiserLine vLine;
            vLine.a = {cx, yTop,    r, g, b};
            vLine.b = {cx, yBottom, r, g, b};
            vLine.thickness = thick;
            data.lines.push_back(vLine);

            // Çap etiketi
            RiserLabel dLbl;
            dLbl.x = cx + 8.0f;
            dLbl.y = (yTop + yBottom) / 2.0f;
            dLbl.fontSize = 8.0f;
            std::ostringstream ss;
            ss << "Ø" << static_cast<int>(seg.diameter_mm);
            if (seg.flowRate_Ls > 0.0f) {
                ss << " (" << std::fixed << std::setprecision(1) << seg.flowRate_Ls << "L/s)";
            }
            dLbl.text = ss.str();
            data.labels.push_back(dLbl);

            // Armatür bağlantı çizgileri (yatay kısa çizgi)
            float fy = (yTop + yBottom) / 2.0f;
            for (size_t ai = 0; ai < seg.fixtureLabels.size(); ++ai) {
                float ax = cx + 30.0f + ai * 15.0f;
                RiserLine hFix;
                hFix.a = {cx,  fy, r, g, b};
                hFix.b = {ax,  fy, r, g, b};
                hFix.thickness = 1.0f;
                data.lines.push_back(hFix);

                RiserLabel aLbl;
                aLbl.x = ax + 2.0f;
                aLbl.y = fy - 4.0f;
                aLbl.text = seg.fixtureLabels[ai];
                aLbl.fontSize = 7.0f;
                data.labels.push_back(aLbl);
            }
        }
    }

    return data;
}

// ═══════════════════════════════════════════════════════════
//  KOLON TOPOLOJI ÇIKARIMI
// ═══════════════════════════════════════════════════════════

std::vector<RiserColumn> RiserDiagram::ExtractColumns() const {
    std::vector<RiserColumn> columns;

    // Her Source ve Drain node'undan başlayan kolon zincirini bul
    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type == NodeType::Source) {
            RiserColumn col;
            col.sourceNodeId = id;
            col.pipeType     = EdgeType::Supply;
            col.label        = node.label.empty()
                               ? "Supply-" + std::to_string(id)
                               : node.label;
            TraceColumn(id, EdgeType::Supply, col);

            if (!col.segments.empty()) {
                columns.push_back(std::move(col));
            }
        }
        if (node.type == NodeType::Drain) {
            RiserColumn col;
            col.sourceNodeId = id;
            col.pipeType     = EdgeType::Drainage;
            col.label        = node.label.empty()
                               ? "Drenaj-" + std::to_string(id)
                               : node.label;
            // Drenaj kolonunda Drain → yukari doğru izle
            TraceColumn(id, EdgeType::Drainage, col);

            if (!col.segments.empty()) {
                columns.push_back(std::move(col));
            }
        }
    }

    // Etiketleri alfabetik sırala
    std::sort(columns.begin(), columns.end(),
              [](const RiserColumn& a, const RiserColumn& b) {
                  return a.label < b.label;
              });

    return columns;
}

void RiserDiagram::TraceColumn(uint32_t startNodeId, EdgeType type,
                                RiserColumn& col) const {
    std::unordered_set<uint32_t> visited;
    std::vector<uint32_t> stack = {startNodeId};

    // Kat → segment haritası
    std::unordered_map<int, RiserColumn::FloorSegment> segMap;

    while (!stack.empty()) {
        uint32_t nodeId = stack.back();
        stack.pop_back();
        if (visited.count(nodeId)) continue;
        visited.insert(nodeId);

        const Node* node = m_network.GetNode(nodeId);
        if (!node) continue;

        // Bu node'un kat indeksini bul
        int fi = InferFloor(nodeId);

        // Fixture ise armatür etiketini ilgili kata ekle
        if (node->type == NodeType::Fixture) {
            auto& seg = segMap[fi];
            seg.floorIndex = fi;
            std::string label = node->fixtureType.empty()
                                ? "Arm." : node->fixtureType;
            seg.fixtureLabels.push_back(label);
        }

        // Bağlı edge'leri gez
        for (uint32_t eid : m_network.GetConnectedEdges(nodeId)) {
            const Edge* edge = m_network.GetEdge(eid);
            if (!edge || edge->type != type) continue;

            // Edge'in diğer ucunu bul
            uint32_t nextNode = (edge->nodeA == nodeId) ? edge->nodeB : edge->nodeA;
            if (visited.count(nextNode)) continue;

            // Bu edge için bir segment kaydı yap
            int edgeFi = InferFloor(nodeId);
            auto& seg = segMap[edgeFi];
            seg.floorIndex   = edgeFi;
            seg.diameter_mm  = static_cast<float>(edge->diameter_mm);
            seg.flowRate_Ls  = static_cast<float>(edge->flowRate_m3s * 1000.0);
            seg.cumulativeDU = static_cast<float>(edge->cumulativeDU);

            stack.push_back(nextNode);
        }
    }

    // Segment haritasını vektöre çevir (kat sırasına göre)
    for (auto& [fi, seg] : segMap) {
        col.segments.push_back(seg);
    }
    std::sort(col.segments.begin(), col.segments.end(),
              [](const RiserColumn::FloorSegment& a, const RiserColumn::FloorSegment& b) {
                  return a.floorIndex < b.floorIndex;
              });
}

int RiserDiagram::InferFloor(uint32_t nodeId) const {
    // FloorManager'da kayıtlı mı?
    int fi = m_floors.GetFloorOfNode(nodeId);
    if (fi != -999) return fi;

    // Z koordinatından tahmin: her 3m = 1 kat
    const Node* node = m_network.GetNode(nodeId);
    if (!node) return 0;

    double z = node->position.z;
    if (m_floors.GetFloorCount() > 0) {
        fi = m_floors.GetFloorIndexAtElevation(z);
        if (fi != -999) return fi;
    }

    // Fallback: 3m kat yüksekliği varsayımı
    return static_cast<int>(std::floor(z / 3.0));
}

// ═══════════════════════════════════════════════════════════
//  SVG ÇIKTI
// ═══════════════════════════════════════════════════════════

std::string RiserDiagram::ToSVG(const RiserDiagramData& data) const {
    std::ostringstream ss;

    // Viewbox ile ölçeklendirilebilir
    int W = static_cast<int>(data.canvasWidth);
    int H = static_cast<int>(data.canvasHeight);

    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<svg xmlns=\"http://www.w3.org/2000/svg\""
       << " width=\"" << W << "\" height=\"" << H << "\""
       << " viewBox=\"0 0 " << W << " " << H << "\""
       << " style=\"background:#fff;font-family:Arial,Helvetica,sans-serif;\">\n";

    // Arka plan dikdörtgeni
    ss << "<rect width=\"" << W << "\" height=\"" << H
       << "\" fill=\"#ffffff\" stroke=\"#cccccc\" stroke-width=\"1\"/>\n";

    // Başlık
    ss << "<text x=\"" << W / 2
       << "\" y=\"22\" text-anchor=\"middle\""
       << " font-size=\"15\" font-weight=\"bold\" fill=\"#222\">KOLON ŞEMASI</text>\n";
    // Alt çizgi başlık
    ss << "<line x1=\"" << MARGIN_LEFT - 10 << "\" y1=\"30\""
       << " x2=\"" << W - 10 << "\" y2=\"30\""
       << " stroke=\"#888\" stroke-width=\"1\"/>\n";

    // Kat alanı arka plan alternating
    if (!data.columns.empty()) {
        int maxFloor = 0, minFloor = 0;
        for (const auto& col : data.columns)
            for (const auto& seg : col.segments) {
                maxFloor = std::max(maxFloor, seg.floorIndex);
                minFloor = std::min(minFloor, seg.floorIndex);
            }
        for (int fi = minFloor; fi <= maxFloor; ++fi) {
            float y = MARGIN_TOP + (maxFloor - fi) * FLOOR_HEIGHT;
            if ((fi % 2) == 0) {
                ss << "<rect x=\"" << MARGIN_LEFT - 10 << "\" y=\"" << y
                   << "\" width=\"" << (W - MARGIN_LEFT) << "\" height=\"" << FLOOR_HEIGHT
                   << "\" fill=\"#f5f8ff\" opacity=\"0.5\"/>\n";
            }
        }
    }

    // Çizgiler
    for (const auto& line : data.lines) {
        int r = static_cast<int>(line.a.r * 255);
        int g = static_cast<int>(line.a.g * 255);
        int b = static_cast<int>(line.a.b * 255);
        ss << "<line"
           << " x1=\"" << std::fixed << std::setprecision(1) << line.a.x
           << "\" y1=\"" << line.a.y << "\""
           << " x2=\"" << line.b.x
           << "\" y2=\"" << line.b.y << "\""
           << " stroke=\"rgb(" << r << "," << g << "," << b << ")\""
           << " stroke-width=\"" << line.thickness
           << "\" stroke-linecap=\"round\"/>\n";
    }

    // Etiketler — kat etiketi sol kenarda farklı stil
    for (const auto& lbl : data.labels) {
        bool isFloorLabel = (lbl.x < MARGIN_LEFT - 5.0f);
        if (isFloorLabel) {
            ss << "<text"
               << " x=\"" << lbl.x << "\" y=\"" << lbl.y << "\""
               << " font-size=\"" << lbl.fontSize << "\" fill=\"#444\""
               << " font-weight=\"bold\">"
               << lbl.text << "</text>\n";
        } else {
            // DN / debi etiketi — kolon üstü siyah, segment etiketi gri
            bool isColumnHeader = (lbl.fontSize >= 10.0f && lbl.y < MARGIN_TOP);
            ss << "<text"
               << " x=\"" << lbl.x << "\" y=\"" << lbl.y << "\""
               << " font-size=\"" << lbl.fontSize << "\""
               << " fill=\"" << (isColumnHeader ? "#111" : "#2255aa") << "\""
               << (isColumnHeader ? " font-weight=\"bold\"" : "")
               << ">"
               << lbl.text << "</text>\n";
        }
    }

    // Lejant (sağ alt)
    float lx = W - 130.0f, ly = H - 60.0f;
    ss << "<rect x=\"" << lx - 5 << "\" y=\"" << ly - 12
       << "\" width=\"125\" height=\"68\" rx=\"4\""
       << " fill=\"#f9f9f9\" stroke=\"#ccc\" stroke-width=\"1\"/>\n";
    ss << "<line x1=\"" << lx << "\" y1=\"" << ly + 5 << "\""
       << " x2=\"" << lx + 30 << "\" y2=\"" << ly + 5 << "\""
       << " stroke=\"rgb(26,120,181)\" stroke-width=\"3\"/>\n";
    ss << "<text x=\"" << lx + 35 << "\" y=\"" << ly + 9
       << "\" font-size=\"8\" fill=\"#333\">Soguk Su</text>\n";
    ss << "<line x1=\"" << lx << "\" y1=\"" << ly + 22 << "\""
       << " x2=\"" << lx + 30 << "\" y2=\"" << ly + 22 << "\""
       << " stroke=\"rgb(204,38,26)\" stroke-width=\"3\"/>\n";
    ss << "<text x=\"" << lx + 35 << "\" y=\"" << ly + 26
       << "\" font-size=\"8\" fill=\"#333\">Sicak Su</text>\n";
    ss << "<line x1=\"" << lx << "\" y1=\"" << ly + 39 << "\""
       << " x2=\"" << lx + 30 << "\" y2=\"" << ly + 39 << "\""
       << " stroke=\"rgb(140,89,26)\" stroke-width=\"3\"/>\n";
    ss << "<text x=\"" << lx + 35 << "\" y=\"" << ly + 43
       << "\" font-size=\"8\" fill=\"#333\">Pis Su</text>\n";

    ss << "</svg>\n";
    return ss.str();
}

// ═══════════════════════════════════════════════════════════
//  METİN ÖZETİ
// ═══════════════════════════════════════════════════════════

std::string RiserDiagram::ToText(const RiserDiagramData& data) const {
    std::ostringstream ss;
    ss << "═══════════════════════════════════════\n";
    ss << "  KOLON ŞEMASI\n";
    ss << "═══════════════════════════════════════\n\n";

    for (const auto& col : data.columns) {
        ss << "▌ " << col.label
           << " ["
           << (col.pipeType == EdgeType::Supply   ? "Soguk Su"
             : col.pipeType == EdgeType::HotWater ? "Sicak Su"
             : col.pipeType == EdgeType::Drainage ? "Pis Su"
                                                  : "Diger")
           << "]\n";

        for (auto it = col.segments.rbegin(); it != col.segments.rend(); ++it) {
            const auto& seg = *it;
            ss << "  Kat " << seg.floorIndex << "  "
               << "Ø" << static_cast<int>(seg.diameter_mm) << " mm  "
               << "Q=" << std::fixed << std::setprecision(2) << seg.flowRate_Ls << " L/s";
            if (!seg.fixtureLabels.empty()) {
                ss << "  [";
                for (size_t i = 0; i < seg.fixtureLabels.size(); ++i) {
                    if (i) ss << ", ";
                    ss << seg.fixtureLabels[i];
                }
                ss << "]";
            }
            ss << "\n";
        }
        ss << "\n";
    }

    return ss.str();
}

// ═══════════════════════════════════════════════════════════
//  GenerateAuto — IsColumnEdge() topolojisinden otomatik kolon şeması
// ═══════════════════════════════════════════════════════════
RiserDiagramData RiserDiagram::GenerateAuto() const {
    // İlk adım: dikey boru zincirlerini IsColumnEdge() ile tespit et
    auto columnEdgeIds = m_network.GetColumnEdges();

    if (columnEdgeIds.empty())
        return Generate(); // fallback: eski yöntem

    // Dikey boruları XY konumlarına göre grupla
    // Aynı XY (~100mm tolerans) → aynı kolon
    struct ColumnGroup {
        double x = 0, y = 0;
        std::vector<uint32_t> edgeIds;
        EdgeType type = EdgeType::Supply;
        std::string label;
    };
    std::vector<ColumnGroup> groups;
    constexpr double kGroupTol = 100.0; // mm

    for (uint32_t eid : columnEdgeIds) {
        const Edge* e = m_network.GetEdge(eid);
        if (!e) continue;
        const Node* nA = m_network.GetNode(e->nodeA);
        const Node* nB = m_network.GetNode(e->nodeB);
        if (!nA || !nB) continue;

        double mx = (nA->position.x + nB->position.x) * 0.5;
        double my = (nA->position.y + nB->position.y) * 0.5;

        // Mevcut grupta mı?
        bool found = false;
        for (auto& g : groups) {
            double dx = mx - g.x, dy = my - g.y;
            if (dx*dx + dy*dy < kGroupTol*kGroupTol) {
                g.edgeIds.push_back(eid);
                found = true;
                break;
            }
        }
        if (!found) {
            ColumnGroup g;
            g.x = mx; g.y = my;
            g.edgeIds.push_back(eid);
            g.type = e->type;
            groups.push_back(g);
        }
    }

    // Grup → RiserColumn dönüşümü
    std::vector<RiserColumn> columns;
    int colIdx = 0;
    for (auto& g : groups) {
        RiserColumn col;
        col.pipeType = g.type;
        col.label = (g.type == EdgeType::Drainage ? "PS-K" : "P-K") + std::to_string(++colIdx);

        // Her edge'in kat aralığını bul
        for (uint32_t eid : g.edgeIds) {
            const Edge* e = m_network.GetEdge(eid);
            if (!e) continue;
            int flA = InferFloor(e->nodeA);
            int flB = InferFloor(e->nodeB);
            int flLow = std::min(flA, flB);

            RiserColumn::FloorSegment seg;
            seg.floorIndex  = flLow;
            seg.diameter_mm = static_cast<float>(e->diameter_mm);
            seg.flowRate_Ls = static_cast<float>(e->flowRate_m3s * 1000.0);
            col.segments.push_back(seg);
        }

        if (!col.segments.empty()) {
            std::sort(col.segments.begin(), col.segments.end(),
                [](const auto& a, const auto& b) { return a.floorIndex < b.floorIndex; });
            columns.push_back(std::move(col));
        }
    }

    // RiserDiagramData oluştur (Generate() ile aynı render mantığı)
    RiserDiagramData data;
    data.columns = std::move(columns);

    int maxFloor = 0, minFloor = 0;
    for (const auto& col : data.columns) {
        for (const auto& seg : col.segments) {
            maxFloor = std::max(maxFloor, seg.floorIndex);
            minFloor = std::min(minFloor, seg.floorIndex);
        }
    }
    int numFloors = maxFloor - minFloor + 1;

    data.canvasWidth  = MARGIN_LEFT + data.columns.size() * COL_SPACING + 60.0f;
    data.canvasHeight = MARGIN_TOP  + numFloors * FLOOR_HEIGHT + 60.0f;

    // Kat çizgisi etiketleri
    for (int fi = minFloor; fi <= maxFloor; ++fi) {
        float y = MARGIN_TOP + (maxFloor - fi) * FLOOR_HEIGHT;
        RiserLabel lbl;
        lbl.x = 5.0f;
        lbl.y = y + FLOOR_HEIGHT / 2.0f;
        lbl.fontSize = 9.0f;
        const core::Floor* fl = m_floors.GetFloor(fi);
        lbl.text = fl ? fl->label : (fi == 0 ? "Zemin" : std::to_string(fi) + ".Kat");
        data.labels.push_back(lbl);

        RiserLine hLine;
        hLine.a = {MARGIN_LEFT - 10.0f, y, 0.8f, 0.8f, 0.8f};
        hLine.b = {data.canvasWidth,     y, 0.8f, 0.8f, 0.8f};
        hLine.thickness = 0.5f;
        data.lines.push_back(hLine);
    }

    // Kolon dikey çizgileri
    for (size_t ci = 0; ci < data.columns.size(); ++ci) {
        const auto& col = data.columns[ci];
        float x = MARGIN_LEFT + ci * COL_SPACING;

        float r = 0.2f, g = 0.5f, b = 1.0f;
        if (col.pipeType == EdgeType::Drainage) { r=0.6f; g=0.35f; b=0.1f; }
        else if (col.pipeType == EdgeType::HotWater) { r=1.0f; g=0.2f; b=0.1f; }
        else if (col.pipeType == EdgeType::Gas) { r=0.9f; g=0.85f; b=0.0f; }

        for (const auto& seg : col.segments) {
            float y1 = MARGIN_TOP + (maxFloor - seg.floorIndex) * FLOOR_HEIGHT;
            float y2 = y1 + FLOOR_HEIGHT;
            RiserLine vl;
            vl.a = {x, y1, r, g, b};
            vl.b = {x, y2, r, g, b};
            vl.thickness = 2.5f;
            data.lines.push_back(vl);

            // DN etiketi
            RiserLabel dl;
            dl.x = x + 5.0f;
            dl.y = y1 + FLOOR_HEIGHT * 0.5f;
            dl.fontSize = 7.0f;
            dl.text = "DN" + std::to_string((int)seg.diameter_mm);
            data.labels.push_back(dl);
        }

        // Kolon başlığı
        RiserLabel cl;
        cl.x = x - 10.0f;
        cl.y = MARGIN_TOP - 15.0f;
        cl.fontSize = 9.0f;
        cl.text = col.label;
        data.labels.push_back(cl);
    }

    return data;
}

} // namespace mep
} // namespace vkt
