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

    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<svg xmlns=\"http://www.w3.org/2000/svg\""
       << " width=\"" << static_cast<int>(data.canvasWidth) << "\""
       << " height=\"" << static_cast<int>(data.canvasHeight) << "\""
       << " style=\"background:#fff;font-family:Arial,sans-serif;\">\n";

    // Başlık
    ss << "<text x=\"" << data.canvasWidth / 2.0f
       << "\" y=\"20\" text-anchor=\"middle\""
       << " style=\"font-size:14px;font-weight:bold;\">KOLON ŞEMASI</text>\n";

    // Çizgiler
    for (const auto& line : data.lines) {
        ss << "<line"
           << " x1=\"" << line.a.x << "\" y1=\"" << line.a.y << "\""
           << " x2=\"" << line.b.x << "\" y2=\"" << line.b.y << "\""
           << " stroke=\"rgb(" << static_cast<int>(line.a.r * 255) << ","
                               << static_cast<int>(line.a.g * 255) << ","
                               << static_cast<int>(line.a.b * 255) << ")\""
           << " stroke-width=\"" << line.thickness << "\"/>\n";
    }

    // Etiketler
    for (const auto& lbl : data.labels) {
        ss << "<text"
           << " x=\"" << lbl.x << "\" y=\"" << lbl.y << "\""
           << " style=\"font-size:" << lbl.fontSize << "px;\">"
           << lbl.text
           << "</text>\n";
    }

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
           << " [" << (col.pipeType == EdgeType::Supply ? "Temiz Su" : "Drenaj") << "]\n";

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

} // namespace mep
} // namespace vkt
