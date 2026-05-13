/**
 * @file DXFWriter.cpp
 * @brief AutoCAD uyumlu ASCII DXF R2000 dosya yazıcısı
 */

#include "cad/DXFWriter.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/Circle.hpp"
#include "cad/Layer.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <unordered_set>
#include <algorithm>

namespace vkt {
namespace cad {

// ═══════════════════════════════════════════════════════════
//  GRUP KODU YAZICILAR
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteGroup(std::ostream& out, int code, const std::string& value) {
    out << std::setw(3) << code << "\n" << value << "\n";
}

void DXFWriter::WriteGroup(std::ostream& out, int code, double value) {
    out << std::setw(3) << code << "\n"
        << std::fixed << std::setprecision(6) << value << "\n";
}

void DXFWriter::WriteGroup(std::ostream& out, int code, int value) {
    out << std::setw(3) << code << "\n" << value << "\n";
}

// ═══════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════

bool DXFWriter::Write(const std::string& filePath,
                      const std::vector<std::unique_ptr<Entity>>& entities,
                      const mep::NetworkGraph& network,
                      const std::string& projectName) const {
    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    out << std::fixed << std::setprecision(6);

    WriteHeader(out, projectName);
    WriteTables(out, entities);

    // Boş BLOCKS bölümü (R2000 gerektirir)
    WriteGroup(out, 0, "SECTION");
    WriteGroup(out, 2, "BLOCKS");
    WriteGroup(out, 0, "ENDSEC");

    WriteEntitiesSection(out, entities, network);

    WriteGroup(out, 0, "SECTION");
    WriteGroup(out, 2, "OBJECTS");
    WriteGroup(out, 0, "DICTIONARY");
    WriteGroup(out, 5, "C");
    WriteGroup(out, 100, "AcDbDictionary");
    WriteGroup(out, 281, 1);
    WriteGroup(out, 0, "ENDSEC");

    WriteGroup(out, 0, "EOF");
    return true;
}

bool DXFWriter::WriteNetwork(const std::string& filePath,
                             const mep::NetworkGraph& network) const {
    std::vector<std::unique_ptr<Entity>> empty;
    return Write(filePath, empty, network, "VKT Network");
}

// ═══════════════════════════════════════════════════════════
//  HEADER
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteHeader(std::ostream& out, const std::string& projectName) const {
    WriteGroup(out, 0, "SECTION");
    WriteGroup(out, 2, "HEADER");

    // DXF versiyonu: AutoCAD R2000
    WriteGroup(out, 9, "$ACADVER");
    WriteGroup(out, 1, "AC1015");

    // Çizim birimi: milimetre (4 = mm)
    WriteGroup(out, 9, "$INSUNITS");
    WriteGroup(out, 70, 4);

    // Proje adı (başlık bloğu için)
    WriteGroup(out, 9, "$PROJECTNAME");
    WriteGroup(out, 1, projectName);

    WriteGroup(out, 0, "ENDSEC");
}

// ═══════════════════════════════════════════════════════════
//  TABLES
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteTables(std::ostream& out,
                            const std::vector<std::unique_ptr<Entity>>& entities) const {
    WriteGroup(out, 0, "SECTION");
    WriteGroup(out, 2, "TABLES");

    // LTYPE tablosu (minimum: CONTINUOUS gerekli)
    WriteGroup(out, 0, "TABLE");
    WriteGroup(out, 2, "LTYPE");
    WriteGroup(out, 70, 1);
    WriteGroup(out, 0, "LTYPE");
    WriteGroup(out, 2, "Continuous");
    WriteGroup(out, 70, 0);
    WriteGroup(out, 3, "Solid line");
    WriteGroup(out, 72, 65);
    WriteGroup(out, 73, 0);
    WriteGroup(out, 40, 0.0);
    WriteGroup(out, 0, "ENDTAB");

    // LAYER tablosu — tüm entity katmanları + MEP katmanları
    std::unordered_set<std::string> layerNames;
    for (const auto& e : entities) {
        if (e) layerNames.insert(e->GetLayerName());
    }
    // MEP sabitleri
    layerNames.insert("VKT-TEMIZ-SU");
    layerNames.insert("VKT-ATIK-SU");
    layerNames.insert("VKT-HAVALANDIRMA");
    layerNames.insert("VKT-NODE");
    layerNames.insert("0");  // AutoCAD varsayılan katmanı

    WriteGroup(out, 0, "TABLE");
    WriteGroup(out, 2, "LAYER");
    WriteGroup(out, 70, static_cast<int>(layerNames.size()));

    for (const auto& name : layerNames) {
        WriteGroup(out, 0, "LAYER");
        WriteGroup(out, 2, name);
        WriteGroup(out, 70, 0);   // kilitli değil

        // Katmana göre renk ata
        int aci = 7; // beyaz (varsayılan)
        if (name == "VKT-TEMIZ-SU")       aci = 5;  // mavi
        else if (name == "VKT-ATIK-SU")   aci = 30; // kahverengi
        else if (name == "VKT-HAVALANDIRMA") aci = 3; // yeşil
        else if (name == "VKT-NODE")       aci = 1;  // kırmızı

        WriteGroup(out, 62, aci);
        WriteGroup(out, 6, "Continuous");
    }
    WriteGroup(out, 0, "ENDTAB");

    WriteGroup(out, 0, "ENDSEC");
}

// ═══════════════════════════════════════════════════════════
//  ENTITIES SECTION
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteEntitiesSection(std::ostream& out,
                                     const std::vector<std::unique_ptr<Entity>>& entities,
                                     const mep::NetworkGraph& network) const {
    WriteGroup(out, 0, "SECTION");
    WriteGroup(out, 2, "ENTITIES");

    // CAD arka plan entity'leri
    for (const auto& e : entities) {
        if (!e) continue;
        switch (e->GetType()) {
            case EntityType::Line:
                WriteEntityLine(out, *e);
                break;
            case EntityType::Polyline:
                WriteEntityPolyline(out, *e);
                break;
            case EntityType::Circle:
                WriteEntityCircle(out, *e);
                break;
            case EntityType::Text:
                WriteEntityText(out, *e);
                break;
            default:
                break;
        }
    }

    // MEP tesisat şebekesi
    WriteNetworkEdges(out, network);
    WriteNetworkNodes(out, network);

    WriteGroup(out, 0, "ENDSEC");
}

// ═══════════════════════════════════════════════════════════
//  ENTITY YAZICILAR
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteEntityLine(std::ostream& out, const Entity& e) const {
    const auto& line = static_cast<const Line&>(e);
    auto s = line.GetStart();
    auto en = line.GetEnd();
    auto c = e.GetColor();

    WriteGroup(out, 0, "LINE");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0)  // ByLayer değilse rengi yaz
        WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 10, s.x);
    WriteGroup(out, 20, s.y);
    WriteGroup(out, 30, s.z);
    WriteGroup(out, 11, en.x);
    WriteGroup(out, 21, en.y);
    WriteGroup(out, 31, en.z);
}

void DXFWriter::WriteEntityPolyline(std::ostream& out, const Entity& e) const {
    const auto& poly = static_cast<const Polyline&>(e);
    const auto& verts = poly.GetVertices();
    if (verts.empty()) return;

    auto c = e.GetColor();

    WriteGroup(out, 0, "LWPOLYLINE");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0)
        WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 100, "AcDbEntity");
    WriteGroup(out, 100, "AcDbPolyline");
    WriteGroup(out, 90, static_cast<int>(verts.size()));
    WriteGroup(out, 70, poly.IsClosed() ? 1 : 0);

    for (const auto& v : verts) {
        WriteGroup(out, 10, v.pos.x);
        WriteGroup(out, 20, v.pos.y);
        if (v.bulge != 0.0)
            WriteGroup(out, 42, v.bulge);
    }
}

void DXFWriter::WriteEntityCircle(std::ostream& out, const Entity& e) const {
    const auto& circ = static_cast<const Circle&>(e);
    auto cen = circ.GetCenter();
    auto c = e.GetColor();

    WriteGroup(out, 0, "CIRCLE");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0)
        WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 10, cen.x);
    WriteGroup(out, 20, cen.y);
    WriteGroup(out, 30, cen.z);
    WriteGroup(out, 40, circ.GetRadius());
}

void DXFWriter::WriteEntityText(std::ostream& out, const Entity& e) const {
    auto pos = e.GetPosition();
    auto c = e.GetColor();

    WriteGroup(out, 0, "TEXT");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0)
        WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 10, pos.x);
    WriteGroup(out, 20, pos.y);
    WriteGroup(out, 30, pos.z);
    WriteGroup(out, 40, 2.5);    // varsayılan metin yüksekliği (mm)
    WriteGroup(out, 1, e.GetLayerName()); // Text içeriği olarak katman adı (fallback)
    WriteGroup(out, 50, e.GetRotation());
}

// ═══════════════════════════════════════════════════════════
//  MEP NETWORK YAZICILAR
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteNetworkEdges(std::ostream& out,
                                  const mep::NetworkGraph& net) const {
    for (const auto& [eid, edge] : net.GetEdgeMap()) {
        const mep::Node* nA = net.GetNode(edge.nodeA);
        const mep::Node* nB = net.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        std::string layer;
        int aci;
        switch (edge.type) {
            case mep::EdgeType::Supply:
                layer = "VKT-TEMIZ-SU"; aci = 5; break;
            case mep::EdgeType::Drainage:
                layer = "VKT-ATIK-SU";  aci = 30; break;
            case mep::EdgeType::Vent:
                layer = "VKT-HAVALANDIRMA"; aci = 3; break;
            default:
                layer = "0"; aci = 7; break;
        }

        WriteGroup(out, 0, "LINE");
        WriteGroup(out, 8, layer);
        WriteGroup(out, 62, aci);
        WriteGroup(out, 10, nA->position.x);
        WriteGroup(out, 20, nA->position.y);
        WriteGroup(out, 30, nA->position.z);
        WriteGroup(out, 11, nB->position.x);
        WriteGroup(out, 21, nB->position.y);
        WriteGroup(out, 31, nB->position.z);

        // Çap etiketi (TEXT entity, boru ortasına)
        double mx = (nA->position.x + nB->position.x) * 0.5;
        double my = (nA->position.y + nB->position.y) * 0.5;
        double mz = (nA->position.z + nB->position.z) * 0.5;

        std::ostringstream label;
        label << "Ø" << static_cast<int>(edge.diameter_mm) << " " << edge.material;

        WriteGroup(out, 0, "TEXT");
        WriteGroup(out, 8, layer);
        WriteGroup(out, 62, aci);
        WriteGroup(out, 10, mx);
        WriteGroup(out, 20, my);
        WriteGroup(out, 30, mz);
        WriteGroup(out, 40, 2.0);
        WriteGroup(out, 1, label.str());
    }
}

void DXFWriter::WriteNetworkNodes(std::ostream& out,
                                  const mep::NetworkGraph& net) const {
    for (const auto& [nid, node] : net.GetNodeMap()) {
        double radius;
        int aci;
        std::string typeLabel;

        switch (node.type) {
            case mep::NodeType::Source:
                radius = 5.0; aci = 5;  typeLabel = "Kaynak"; break;
            case mep::NodeType::Fixture:
                radius = 3.0; aci = 1;  typeLabel = node.fixtureType.empty() ? "Arm." : node.fixtureType; break;
            case mep::NodeType::Drain:
                radius = 4.0; aci = 30; typeLabel = "Tahliye"; break;
            case mep::NodeType::Pump:
                radius = 4.0; aci = 6;  typeLabel = "Pompa"; break;
            case mep::NodeType::Tank:
                radius = 5.0; aci = 4;  typeLabel = "Depo"; break;
            default:
                radius = 2.0; aci = 7;  typeLabel = ""; break;
        }

        // Daire sembolü
        WriteGroup(out, 0, "CIRCLE");
        WriteGroup(out, 8, "VKT-NODE");
        WriteGroup(out, 62, aci);
        WriteGroup(out, 10, node.position.x);
        WriteGroup(out, 20, node.position.y);
        WriteGroup(out, 30, node.position.z);
        WriteGroup(out, 40, radius);

        // Etiket metni
        std::string label = node.label.empty() ? typeLabel : node.label;
        if (!label.empty()) {
            WriteGroup(out, 0, "TEXT");
            WriteGroup(out, 8, "VKT-NODE");
            WriteGroup(out, 62, aci);
            WriteGroup(out, 10, node.position.x + radius + 1.0);
            WriteGroup(out, 20, node.position.y);
            WriteGroup(out, 30, node.position.z);
            WriteGroup(out, 40, 2.5);
            WriteGroup(out, 1, label);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  RENK DÖNÜŞTÜRÜCÜ (RGB → AutoCAD Color Index)
// ═══════════════════════════════════════════════════════════

int DXFWriter::ColorToACI(int r, int g, int b) {
    // En yakın ACI rengini bul (temel 9 renk için hızlı yol)
    struct ACIEntry { int r, g, b, aci; };
    static const ACIEntry table[] = {
        {255,   0,   0, 1},  // kırmızı
        {255, 255,   0, 2},  // sarı
        {  0, 255,   0, 3},  // yeşil
        {  0, 255, 255, 4},  // cyan
        {  0,   0, 255, 5},  // mavi
        {255,   0, 255, 6},  // magenta
        {255, 255, 255, 7},  // beyaz
        {128, 128, 128, 8},  // gri koyu
        {192, 192, 192, 9},  // gri açık
    };

    int bestACI = 7;
    int bestDist = INT_MAX;
    for (const auto& e : table) {
        int dr = r - e.r, dg = g - e.g, db = b - e.b;
        int dist = dr*dr + dg*dg + db*db;
        if (dist < bestDist) {
            bestDist = dist;
            bestACI = e.aci;
        }
    }
    return bestACI;
}

} // namespace cad
} // namespace vkt
