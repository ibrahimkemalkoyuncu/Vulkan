/**
 * @file DXFWriter.cpp
 * @brief AutoCAD uyumlu ASCII DXF R2000 dosya yazıcısı
 */

#include "cad/DXFWriter.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include "cad/Ellipse.hpp"
#include "cad/Block.hpp"
#include "cad/Layer.hpp"
#include "cad/Dimension.hpp"
#include "cad/Leader.hpp"
#include "cad/Hatch.hpp"
#include "cad/Spline.hpp"
#include "cad/Text.hpp"

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
                      const std::string& projectName,
                      const BlockRegistry* registry,
                      const std::unordered_map<std::string, Layer>* layerMap) const {
    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    out << std::fixed << std::setprecision(6);

    WriteHeader(out, projectName);
    WriteTables(out, entities, registry, layerMap);
    WriteBlocksSection(out, registry);
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
                            const std::vector<std::unique_ptr<Entity>>& entities,
                            const BlockRegistry* registry,
                            const std::unordered_map<std::string, Layer>* layerMap) const {
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
        if (e && !e->GetLayerName().empty()) layerNames.insert(e->GetLayerName());
    }
    // MEP sabit katmanlar (renk tablosu aşağıda)
    static const std::pair<const char*, int> kMepLayers[] = {
        {"VKT-TEMIZ-SU",    4 },  // cyan
        {"VKT-SICAK-SU",    1 },  // red
        {"VKT-ATIK-SU",     30},  // brown
        {"VKT-HAVALANDIRMA",3 },  // green
        {"VKT-GAZ",         2 },  // yellow
        {"VKT-ISITMA",      30},  // orange-brown
        {"VKT-ISITMA-DONUS",40},  // orange
        {"VKT-YANGIN",      1 },  // red
        {"VKT-NODE",        7 },
        {"VKT-DIM",         3 },  // green for dims
        {"0",               7 },
    };
    for (const auto& lp : kMepLayers)
        layerNames.insert(lp.first);

    WriteGroup(out, 0, "TABLE");
    WriteGroup(out, 2, "LAYER");
    WriteGroup(out, 70, static_cast<int>(layerNames.size()));

    for (const auto& name : layerNames) {
        WriteGroup(out, 0, "LAYER");
        WriteGroup(out, 2, name);

        // DXF code 70 layer flags:
        // bit 0 = frozen, bit 2 = frozen in new viewports, bit 4 = locked
        int flag70 = 0;
        if (layerMap) {
            auto it = layerMap->find(name);
            if (it != layerMap->end()) {
                if (it->second.IsFrozen()) flag70 |= 1;
                if (it->second.IsLocked()) flag70 |= 4;
            }
        }
        WriteGroup(out, 70, flag70);

        // Default ACI from MEP table; override from layerMap if present
        int aci = 7;
        for (const auto& lp : kMepLayers)
            if (name == lp.first) { aci = lp.second; break; }
        if (layerMap) {
            auto it = layerMap->find(name);
            if (it != layerMap->end()) {
                auto col = it->second.GetColor();
                if (col.a != 0) aci = ColorToACI(col.r, col.g, col.b);
            }
        }

        WriteGroup(out, 62, aci);
        WriteGroup(out, 6, "Continuous");
    }
    WriteGroup(out, 0, "ENDTAB");

    // BLOCK table (blok isimlerini kaydeder — R2000 için gerekli)
    std::vector<std::string> blockNames;
    if (registry) {
        blockNames = registry->GetBlockNames();
    }
    // *MODEL_SPACE ve *PAPER_SPACE zorunlu sentinel blokları
    WriteGroup(out, 0, "TABLE");
    WriteGroup(out, 2, "BLOCK_RECORD");
    WriteGroup(out, 70, static_cast<int>(blockNames.size()) + 2);
    WriteGroup(out, 0, "BLOCK_RECORD");
    WriteGroup(out, 2, "*Model_Space");
    WriteGroup(out, 0, "BLOCK_RECORD");
    WriteGroup(out, 2, "*Paper_Space");
    for (const auto& bn : blockNames) {
        WriteGroup(out, 0, "BLOCK_RECORD");
        WriteGroup(out, 2, bn);
    }
    WriteGroup(out, 0, "ENDTAB");

    WriteGroup(out, 0, "ENDSEC");
}

// ═══════════════════════════════════════════════════════════
//  BLOCKS SECTION
// ═══════════════════════════════════════════════════════════

void DXFWriter::WriteBlocksSection(std::ostream& out, const BlockRegistry* registry) const {
    WriteGroup(out, 0, "SECTION");
    WriteGroup(out, 2, "BLOCKS");

    // Zorunlu sentinel bloklar
    for (const auto* sentinel : {"*Model_Space", "*Paper_Space"}) {
        WriteGroup(out, 0, "BLOCK");
        WriteGroup(out, 8, "0");
        WriteGroup(out, 2, sentinel);
        WriteGroup(out, 70, 0);
        WriteGroup(out, 10, 0.0); WriteGroup(out, 20, 0.0); WriteGroup(out, 30, 0.0);
        WriteGroup(out, 3, sentinel);
        WriteGroup(out, 1, "");
        WriteGroup(out, 0, "ENDBLK");
        WriteGroup(out, 8, "0");
    }

    if (registry) {
        for (const auto& name : registry->GetBlockNames()) {
            const BlockDef* blk = registry->GetBlock(name);
            if (!blk) continue;

            WriteGroup(out, 0, "BLOCK");
            WriteGroup(out, 8, "0");
            WriteGroup(out, 2, blk->name);
            WriteGroup(out, 70, 0);
            WriteGroup(out, 10, blk->basePoint.x);
            WriteGroup(out, 20, blk->basePoint.y);
            WriteGroup(out, 30, blk->basePoint.z);
            WriteGroup(out, 3, blk->name);
            WriteGroup(out, 1, "");

            WriteBlockEntities(out, blk->entities);

            WriteGroup(out, 0, "ENDBLK");
            WriteGroup(out, 8, "0");
        }
    }

    WriteGroup(out, 0, "ENDSEC");
}

void DXFWriter::WriteBlockEntities(std::ostream& out,
                                    const std::vector<std::unique_ptr<Entity>>& blockEntities) const {
    for (const auto& e : blockEntities) {
        if (!e) continue;
        switch (e->GetType()) {
            case EntityType::Line:      WriteEntityLine(out, *e);      break;
            case EntityType::Polyline:  WriteEntityPolyline(out, *e);  break;
            case EntityType::Circle:    WriteEntityCircle(out, *e);    break;
            case EntityType::Arc:       WriteEntityArc(out, *e);       break;
            case EntityType::Ellipse:   WriteEntityEllipse(out, *e);   break;
            case EntityType::Spline:    WriteEntitySpline(out, *e);    break;
            case EntityType::Text:      WriteEntityText(out, *e);      break;
            case EntityType::Insert:    WriteEntityInsert(out, *e);    break;
            case EntityType::Dimension: WriteEntityDimension(out, *e); break;
            case EntityType::Leader:    WriteEntityLeader(out, *e);    break;
            case EntityType::Hatch:     WriteEntityHatch(out, *e);     break;
            default: break;
        }
    }
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
            case EntityType::Arc:
                WriteEntityArc(out, *e);
                break;
            case EntityType::Ellipse:
                WriteEntityEllipse(out, *e);
                break;
            case EntityType::Spline:
                WriteEntitySpline(out, *e);
                break;
            case EntityType::Text:
                WriteEntityText(out, *e);
                break;
            case EntityType::Insert:
                WriteEntityInsert(out, *e);
                break;
            case EntityType::Dimension:
                WriteEntityDimension(out, *e);
                break;
            case EntityType::Leader:
                WriteEntityLeader(out, *e);
                break;
            case EntityType::Hatch:
                WriteEntityHatch(out, *e);
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

void DXFWriter::WriteEntityArc(std::ostream& out, const Entity& e) const {
    const auto& arc = static_cast<const Arc&>(e);
    auto cen = arc.GetCenter();
    auto c = e.GetColor();

    WriteGroup(out, 0, "ARC");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0) WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 10, cen.x);
    WriteGroup(out, 20, cen.y);
    WriteGroup(out, 30, cen.z);
    WriteGroup(out, 40, arc.GetRadius());
    WriteGroup(out, 50, arc.GetStartAngle());
    WriteGroup(out, 51, arc.GetEndAngle());
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

void DXFWriter::WriteEntityEllipse(std::ostream& out, const Entity& e) const {
    const auto& ell = static_cast<const Ellipse&>(e);
    auto cen = ell.GetCenter();
    auto c = e.GetColor();

    // DXF ELLIPSE: group 11/21/31 = major axis endpoint relative to center
    double cosR = std::cos(ell.GetRotAngle());
    double sinR = std::sin(ell.GetRotAngle());
    double ax = ell.GetSemiMajor() * cosR;
    double ay = ell.GetSemiMajor() * sinR;

    WriteGroup(out, 0, "ELLIPSE");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0) WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 10, cen.x);
    WriteGroup(out, 20, cen.y);
    WriteGroup(out, 30, cen.z);
    WriteGroup(out, 11, ax);   // major axis endpoint (relative)
    WriteGroup(out, 21, ay);
    WriteGroup(out, 31, 0.0);
    WriteGroup(out, 40, ell.GetAxisRatio());
    WriteGroup(out, 41, ell.GetStartParam());
    WriteGroup(out, 42, ell.GetEndParam());
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
    WriteGroup(out, 40, 2.5);
    WriteGroup(out, 1, e.GetLayerName());
    WriteGroup(out, 50, e.GetRotation());
}

void DXFWriter::WriteEntityInsert(std::ostream& out, const Entity& e) const {
    const auto& ins = static_cast<const Insert&>(e);
    auto c = e.GetColor();

    WriteGroup(out, 0, "INSERT");
    WriteGroup(out, 8, e.GetLayerName().empty() ? "0" : e.GetLayerName());
    if (c.a != 0)
        WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 2, ins.GetBlockName());          // blok adı
    WriteGroup(out, 10, ins.GetInsertPt().x);
    WriteGroup(out, 20, ins.GetInsertPt().y);
    WriteGroup(out, 30, ins.GetInsertPt().z);
    WriteGroup(out, 41, ins.GetScaleX());            // X ölçeği
    WriteGroup(out, 42, ins.GetScaleY());            // Y ölçeği
    WriteGroup(out, 43, 1.0);                        // Z ölçeği
    WriteGroup(out, 50, ins.GetRotDeg());            // dönme açısı (derece)
}

void DXFWriter::WriteEntityDimension(std::ostream& out, const Entity& e) const {
    const auto& dim = static_cast<const Dimension&>(e);
    auto p1     = dim.GetPoint1();
    auto p2     = dim.GetPoint2();
    auto dimPos = dim.GetDimLinePos();

    // DXF type flag: 0=linear,1=aligned,3=diameter,4=radius
    int typeFlag = 1;
    switch (dim.GetDimType()) {
        case DimensionType::Aligned:  typeFlag = 1; break;
        case DimensionType::Linear:   typeFlag = 0; break;
        case DimensionType::Radius:   typeFlag = 4; break;
        case DimensionType::Diameter: typeFlag = 3; break;
        default: break;
    }

    std::string layer = e.GetLayerName().empty() ? "VKT-DIM" : e.GetLayerName();
    WriteGroup(out, 0, "DIMENSION");
    WriteGroup(out, 8, layer);
    // def point (code 10 = text mid position)
    WriteGroup(out, 10, dimPos.x);
    WriteGroup(out, 20, dimPos.y);
    WriteGroup(out, 30, dimPos.z);
    // text mid point
    WriteGroup(out, 11, dimPos.x);
    WriteGroup(out, 21, dimPos.y);
    WriteGroup(out, 31, dimPos.z);
    WriteGroup(out, 70, typeFlag);
    if (!dim.GetOverrideText().empty())
        WriteGroup(out, 1, dim.GetOverrideText());
    // extension line origin points
    WriteGroup(out, 13, p1.x);
    WriteGroup(out, 23, p1.y);
    WriteGroup(out, 33, p1.z);
    WriteGroup(out, 14, p2.x);
    WriteGroup(out, 24, p2.y);
    WriteGroup(out, 34, p2.z);
}

void DXFWriter::WriteEntityLeader(std::ostream& out, const Entity& e) const {
    const auto& ldr = static_cast<const Leader&>(e);
    const auto& verts = ldr.GetVertices();
    if (verts.size() < 2) return;

    std::string layer = e.GetLayerName().empty() ? "0" : e.GetLayerName();
    WriteGroup(out, 0, "LEADER");
    WriteGroup(out, 8, layer);
    WriteGroup(out, 76, static_cast<int>(verts.size())); // vertex count

    for (const auto& v : verts) {
        WriteGroup(out, 10, v.x);
        WriteGroup(out, 20, v.y);
        WriteGroup(out, 30, v.z);
    }

    WriteGroup(out, 73, ldr.HasArrowhead() ? 1 : 0);
    if (!ldr.GetText().empty())
        WriteGroup(out, 3, ldr.GetText());
}

void DXFWriter::WriteEntitySpline(std::ostream& out, const Entity& e) const {
    const auto& sp = static_cast<const Spline&>(e);
    out << "  0\nSPLINE\n";
    out << "  5\n" << std::hex << e.GetId() << std::dec << "\n";
    out << "100\nAcDbEntity\n";
    out << "  8\n" << e.GetLayerName() << "\n";
    out << "100\nAcDbSpline\n";

    int degree = sp.GetDegree();
    out << " 71\n" << degree << "\n";

    if (sp.HasCtrlPoints()) {
        const auto& ctrl = sp.GetCtrlPoints();
        const auto& knots = sp.GetKnots();
        out << " 72\n" << knots.size() << "\n";
        out << " 73\n" << ctrl.size() << "\n";
        for (double k : knots)
            out << " 40\n" << k << "\n";
        for (const auto& p : ctrl) {
            out << " 10\n" << p.x << "\n";
            out << " 20\n" << p.y << "\n";
            out << " 30\n" << p.z << "\n";
        }
    } else if (sp.HasFitPoints()) {
        const auto& fit = sp.GetFitPoints();
        out << " 74\n" << fit.size() << "\n";
        for (const auto& p : fit) {
            out << " 11\n" << p.x << "\n";
            out << " 21\n" << p.y << "\n";
            out << " 31\n" << p.z << "\n";
        }
    }
}

void DXFWriter::WriteEntityHatch(std::ostream& out, const Entity& e) const {
    const auto& h = static_cast<const Hatch&>(e);
    const auto& bnd = h.GetBoundary();
    if (bnd.size() < 3) return;

    std::string layer = e.GetLayerName().empty() ? "0" : e.GetLayerName();
    auto c = e.GetColor();

    WriteGroup(out, 0, "HATCH");
    WriteGroup(out, 8, layer);
    if (c.a != 0)
        WriteGroup(out, 62, ColorToACI(c.r, c.g, c.b));
    WriteGroup(out, 2, h.GetPatternName().empty() ? "SOLID" : h.GetPatternName());
    WriteGroup(out, 70, h.IsSolid() ? 1 : 0);  // solid fill flag
    WriteGroup(out, 71, 0); // associativity (0=non-associative)
    WriteGroup(out, 91, 1); // 1 boundary path

    // Outer boundary as polyline path
    WriteGroup(out, 92, 7); // boundary path type = 7 (outer + polyline)
    WriteGroup(out, 72, 0); // hasBulge = false (simplified)
    WriteGroup(out, 73, 1); // isClosed
    WriteGroup(out, 93, static_cast<int>(bnd.size())); // vertex count
    for (const auto& v : bnd) {
        WriteGroup(out, 10, v.pos.x);
        WriteGroup(out, 20, v.pos.y);
        if (std::abs(v.bulge) > 1e-10)
            WriteGroup(out, 42, v.bulge);
    }
    WriteGroup(out, 97, 0); // source boundary count

    WriteGroup(out, 75, 0); // hatch style
    WriteGroup(out, 76, 1); // hatch pattern type (1=predefined)
    WriteGroup(out, 52, h.GetPatternAngle());
    WriteGroup(out, 41, h.GetPatternScale());
    WriteGroup(out, 77, 0); // pattern double flag
    WriteGroup(out, 78, 0); // number of pattern definition lines (0 for SOLID)
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
                layer = "VKT-TEMIZ-SU";    aci = 4;  break; // cyan
            case mep::EdgeType::HotWater:
                layer = "VKT-SICAK-SU";    aci = 1;  break; // red
            case mep::EdgeType::Drainage:
                layer = "VKT-ATIK-SU";     aci = 30; break; // brown
            case mep::EdgeType::Vent:
                layer = "VKT-HAVALANDIRMA";aci = 3;  break; // green
            case mep::EdgeType::Gas:
                layer = "VKT-GAZ";         aci = 2;  break; // yellow
            case mep::EdgeType::Heating:
                layer = "VKT-ISITMA";      aci = 30; break; // orange-brown
            case mep::EdgeType::HeatingReturn:
                layer = "VKT-ISITMA-DONUS";aci = 40; break; // orange
            case mep::EdgeType::FireLine:
                layer = "VKT-YANGIN";      aci = 1;  break; // red
            case mep::EdgeType::Electric:
                layer = "VKT-ELEKTRIK";    aci = 40; break; // orange
            case mep::EdgeType::Duct:
                layer = "VKT-HAVALANDIRMA";aci = 3;  break; // green
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

        // XDATA — MEP hidrolik verisi (round-trip interoperabilite)
        WriteGroup(out, 1001, "VKT_MEP");
        WriteGroup(out, 1000, "EdgeID=" + std::to_string(eid));
        WriteGroup(out, 1040, edge.diameter_mm);   // DN (mm)
        WriteGroup(out, 1040, edge.flowRate_m3s);   // Q (m³/s)
        WriteGroup(out, 1040, edge.velocity_ms);    // v (m/s)
        WriteGroup(out, 1040, edge.headLoss_m);     // ΔH (m)
        WriteGroup(out, 1000, "Material=" + edge.material);

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
