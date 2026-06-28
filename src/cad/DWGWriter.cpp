/**
 * @file DWGWriter.cpp
 * @brief DWG-compatible DXF R2000 writer implementation
 */

#include "cad/DWGWriter.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include "cad/Text.hpp"

#include <iomanip>
#include <cmath>
#include <climits>

namespace vkt {
namespace cad {

// ── Group code writers ──────────────────────────────────────────

void DWGWriter::WriteGroup(std::ofstream& f, int code, const std::string& value) {
    f << std::setw(3) << code << "\n" << value << "\n";
}

void DWGWriter::WriteGroup(std::ofstream& f, int code, double value) {
    f << std::setw(3) << code << "\n"
      << std::fixed << std::setprecision(6) << value << "\n";
}

void DWGWriter::WriteGroup(std::ofstream& f, int code, int value) {
    f << std::setw(3) << code << "\n" << value << "\n";
}

int DWGWriter::ColorToACI(int r, int g, int b) {
    struct ACIEntry { int r, g, b, aci; };
    static const ACIEntry table[] = {
        {255,   0,   0, 1},
        {255, 255,   0, 2},
        {  0, 255,   0, 3},
        {  0, 255, 255, 4},
        {  0,   0, 255, 5},
        {255,   0, 255, 6},
        {255, 255, 255, 7},
        {128, 128, 128, 8},
        {192, 192, 192, 9},
    };
    int bestACI = 7;
    int bestDist = INT_MAX;
    for (const auto& e : table) {
        int dr = r - e.r, dg = g - e.g, db = b - e.b;
        int dist = dr*dr + dg*dg + db*db;
        if (dist < bestDist) { bestDist = dist; bestACI = e.aci; }
    }
    return bestACI;
}

// ── Header ──────────────────────────────────────────────────────

void DWGWriter::WriteHeader(std::ofstream& f, const std::map<std::string, Layer>& /*layers*/) {
    WriteGroup(f, 0, "SECTION");
    WriteGroup(f, 2, "HEADER");

    // DXF version: AutoCAD R2000
    WriteGroup(f, 9, "$ACADVER");
    WriteGroup(f, 1, "AC1015");

    // Units: millimetres
    WriteGroup(f, 9, "$INSUNITS");
    WriteGroup(f, 70, 4);

    WriteGroup(f, 0, "ENDSEC");
}

// ── Tables ──────────────────────────────────────────────────────

void DWGWriter::WriteTables(std::ofstream& f, const std::map<std::string, Layer>& layers) {
    WriteGroup(f, 0, "SECTION");
    WriteGroup(f, 2, "TABLES");

    // LTYPE table (minimum: CONTINUOUS)
    WriteGroup(f, 0, "TABLE");
    WriteGroup(f, 2, "LTYPE");
    WriteGroup(f, 70, 1);
    WriteGroup(f, 0, "LTYPE");
    WriteGroup(f, 2, "Continuous");
    WriteGroup(f, 70, 0);
    WriteGroup(f, 3, "Solid line");
    WriteGroup(f, 72, 65);
    WriteGroup(f, 73, 0);
    WriteGroup(f, 40, 0.0);
    WriteGroup(f, 0, "ENDTAB");

    // LAYER table
    WriteGroup(f, 0, "TABLE");
    WriteGroup(f, 2, "LAYER");
    WriteGroup(f, 70, static_cast<int>(layers.size()) + 1);

    // Default layer 0
    WriteGroup(f, 0, "LAYER");
    WriteGroup(f, 2, "0");
    WriteGroup(f, 70, 0);
    WriteGroup(f, 62, 7);
    WriteGroup(f, 6, "Continuous");

    for (const auto& [name, layer] : layers) {
        if (name == "0") continue;
        WriteGroup(f, 0, "LAYER");
        WriteGroup(f, 2, name);

        int flag70 = 0;
        if (layer.IsFrozen()) flag70 |= 1;
        if (layer.IsLocked()) flag70 |= 4;
        WriteGroup(f, 70, flag70);

        auto col = layer.GetColor();
        int aci = (col.a != 0) ? ColorToACI(col.r, col.g, col.b) : 7;
        WriteGroup(f, 62, aci);
        WriteGroup(f, 6, "Continuous");
    }
    WriteGroup(f, 0, "ENDTAB");

    // BLOCK_RECORD table (minimal)
    WriteGroup(f, 0, "TABLE");
    WriteGroup(f, 2, "BLOCK_RECORD");
    WriteGroup(f, 70, 2);
    WriteGroup(f, 0, "BLOCK_RECORD");
    WriteGroup(f, 2, "*Model_Space");
    WriteGroup(f, 0, "BLOCK_RECORD");
    WriteGroup(f, 2, "*Paper_Space");
    WriteGroup(f, 0, "ENDTAB");

    WriteGroup(f, 0, "ENDSEC");
}

// ── Public API ──────────────────────────────────────────────────

bool DWGWriter::Write(const std::string& filepath,
                      const std::vector<std::unique_ptr<Entity>>& entities,
                      const std::map<std::string, Layer>& layers,
                      const BlockRegistry* /*registry*/) {
    // Always write DXF R2000 format (AutoCAD can open .dwg with DXF content)
    return WriteDxfCompat(filepath, entities, layers);
}

bool DWGWriter::WriteDxfCompat(const std::string& filepath,
                                const std::vector<std::unique_ptr<Entity>>& entities,
                                const std::map<std::string, Layer>& layers) {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << std::fixed << std::setprecision(6);

    WriteHeader(f, layers);
    WriteTables(f, layers);

    // BLOCKS section (minimal sentinels)
    WriteGroup(f, 0, "SECTION");
    WriteGroup(f, 2, "BLOCKS");
    for (const auto* sentinel : {"*Model_Space", "*Paper_Space"}) {
        WriteGroup(f, 0, "BLOCK");
        WriteGroup(f, 8, "0");
        WriteGroup(f, 2, sentinel);
        WriteGroup(f, 70, 0);
        WriteGroup(f, 10, 0.0); WriteGroup(f, 20, 0.0); WriteGroup(f, 30, 0.0);
        WriteGroup(f, 3, sentinel);
        WriteGroup(f, 1, "");
        WriteGroup(f, 0, "ENDBLK");
        WriteGroup(f, 8, "0");
    }
    WriteGroup(f, 0, "ENDSEC");

    // ENTITIES section
    WriteGroup(f, 0, "SECTION");
    WriteGroup(f, 2, "ENTITIES");

    for (const auto& e : entities) {
        if (!e) continue;
        std::string layerName = e->GetLayerName().empty() ? "0" : e->GetLayerName();
        auto c = e->GetColor();

        switch (e->GetType()) {
            case EntityType::Line: {
                const auto& line = static_cast<const Line&>(*e);
                auto s = line.GetStart();
                auto en = line.GetEnd();
                WriteGroup(f, 0, "LINE");
                WriteGroup(f, 8, layerName);
                if (c.a != 0) WriteGroup(f, 62, ColorToACI(c.r, c.g, c.b));
                WriteGroup(f, 10, s.x);
                WriteGroup(f, 20, s.y);
                WriteGroup(f, 30, s.z);
                WriteGroup(f, 11, en.x);
                WriteGroup(f, 21, en.y);
                WriteGroup(f, 31, en.z);
                break;
            }
            case EntityType::Circle: {
                const auto& circ = static_cast<const Circle&>(*e);
                auto cen = circ.GetCenter();
                WriteGroup(f, 0, "CIRCLE");
                WriteGroup(f, 8, layerName);
                if (c.a != 0) WriteGroup(f, 62, ColorToACI(c.r, c.g, c.b));
                WriteGroup(f, 10, cen.x);
                WriteGroup(f, 20, cen.y);
                WriteGroup(f, 30, cen.z);
                WriteGroup(f, 40, circ.GetRadius());
                break;
            }
            case EntityType::Arc: {
                const auto& arc = static_cast<const Arc&>(*e);
                auto cen = arc.GetCenter();
                WriteGroup(f, 0, "ARC");
                WriteGroup(f, 8, layerName);
                if (c.a != 0) WriteGroup(f, 62, ColorToACI(c.r, c.g, c.b));
                WriteGroup(f, 10, cen.x);
                WriteGroup(f, 20, cen.y);
                WriteGroup(f, 30, cen.z);
                WriteGroup(f, 40, arc.GetRadius());
                WriteGroup(f, 50, arc.GetStartAngle());
                WriteGroup(f, 51, arc.GetEndAngle());
                break;
            }
            case EntityType::Polyline: {
                const auto& poly = static_cast<const Polyline&>(*e);
                const auto& verts = poly.GetVertices();
                if (verts.empty()) break;
                WriteGroup(f, 0, "LWPOLYLINE");
                WriteGroup(f, 8, layerName);
                if (c.a != 0) WriteGroup(f, 62, ColorToACI(c.r, c.g, c.b));
                WriteGroup(f, 100, "AcDbEntity");
                WriteGroup(f, 100, "AcDbPolyline");
                WriteGroup(f, 90, static_cast<int>(verts.size()));
                WriteGroup(f, 70, poly.IsClosed() ? 1 : 0);
                for (const auto& v : verts) {
                    WriteGroup(f, 10, v.pos.x);
                    WriteGroup(f, 20, v.pos.y);
                    if (v.bulge != 0.0) WriteGroup(f, 42, v.bulge);
                }
                break;
            }
            case EntityType::Text: {
                auto pos = e->GetPosition();
                WriteGroup(f, 0, "TEXT");
                WriteGroup(f, 8, layerName);
                if (c.a != 0) WriteGroup(f, 62, ColorToACI(c.r, c.g, c.b));
                WriteGroup(f, 10, pos.x);
                WriteGroup(f, 20, pos.y);
                WriteGroup(f, 30, pos.z);
                WriteGroup(f, 40, 2.5);
                WriteGroup(f, 1, layerName);
                break;
            }
            default:
                break;
        }
    }

    WriteGroup(f, 0, "ENDSEC");

    // OBJECTS section (minimal)
    WriteGroup(f, 0, "SECTION");
    WriteGroup(f, 2, "OBJECTS");
    WriteGroup(f, 0, "DICTIONARY");
    WriteGroup(f, 5, "C");
    WriteGroup(f, 100, "AcDbDictionary");
    WriteGroup(f, 281, 1);
    WriteGroup(f, 0, "ENDSEC");

    WriteGroup(f, 0, "EOF");
    return true;
}

} // namespace cad
} // namespace vkt
