/**
 * @file DXFReader.cpp
 * @brief DXF okuyucu implementasyonu
 */

#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "cad/DXFReader.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include "cad/Ellipse.hpp"
#include "cad/Spline.hpp"
#include "cad/Text.hpp"
#include "cad/Hatch.hpp"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cctype>

namespace vkt::cad {

// DXFHeader implementation
double DXFHeader::GetScaleToMM() const {
    switch (insUnits) {
        case 1:  return 25.4;    // inches
        case 2:  return 304.8;   // feet
        case 3:  return 1609344; // miles
        case 4:  return 1.0;     // millimeters
        case 5:  return 10.0;    // centimeters
        case 6:  return 1000.0;  // meters
        case 14: return 100.0;   // decimeters
        default: return 1.0;     // unitless — assume mm
    }
}

// DXFCode implementation
double DXFCode::AsDouble() const {
    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

int DXFCode::AsInt() const {
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

// DXFReader implementation
DXFReader::DXFReader(const std::string& filename)
    : m_filename(filename), m_stats{} {}

DXFReader::~DXFReader() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

bool DXFReader::Read() {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Dosya aç
    m_file.open(m_filename);
    if (!m_file.is_open()) {
        m_lastError = "Dosya açılamadı: " + m_filename;
        return false;
    }
    
    // Temizle
    m_entities.clear();
    m_layers.clear();
    m_blocks.clear();
    m_stats = Statistics{};
    
    try {
        DXFCode code;
        
        // DXF dosyası SECTION'lardan oluşur
        while (ReadCode(code)) {
            if (code.code == 0 && code.value == "SECTION") {
                // Section adını oku
                if (!ReadCode(code) || code.code != 2) {
                    m_lastError = "SECTION adı okunamadı";
                    return false;
                }
                
                std::string sectionName = code.value;
                
                if (sectionName == "HEADER") {
                    if (!ReadHeader()) return false;
                } else if (sectionName == "TABLES") {
                    if (!ReadTables()) return false;
                } else if (sectionName == "ENTITIES") {
                    if (!ReadEntities()) return false;
                } else if (sectionName == "BLOCKS") {
                    if (!ReadBlocks()) return false;
                } else {
                    // Diğer section'lar (OBJECTS vs.) skip
                    while (ReadCode(code)) {
                        if (code.code == 0 && code.value == "ENDSEC") {
                            break;
                        }
                    }
                }
            } else if (code.code == 0 && code.value == "EOF") {
                break;
            }
        }
        
    } catch (const std::exception& e) {
        m_lastError = std::string("DXF okuma hatası: ") + e.what();
        return false;
    }
    
    // W-Block baz noktası ofseti — tüm entity'leri referans noktasına göre hizala.
    // Katlar arası hizalama için her katta ortak bir referans noktası (örn. kolon
    // köşesi) seçilmeli ve bu noktanın koordinatları SetInsertionOffset'e verilmelidir.
    if (m_insertionOffsetX != 0.0 || m_insertionOffsetY != 0.0) {
        geom::Vec3 shift(-m_insertionOffsetX, -m_insertionOffsetY, 0.0);
        for (auto& ent : m_entities) {
            if (ent) ent->Move(shift);
        }
    }

    // İstatistikleri güncelle
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.totalEntities = m_entities.size();
    m_stats.totalLayers = m_layers.size();
    m_stats.readTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    m_file.close();
    return true;
}

bool DXFReader::ReadCode(DXFCode& code) {
    // Pushback mekanizması: Eğer önceki entity okuyucu bir code 0 okumuşsa
    // onu geri koymuştur, önce onu döndür
    if (m_hasPendingCode) {
        code = m_pendingCode;
        m_hasPendingCode = false;
        return true;
    }

    std::string line;

    // Group code satırını oku
    if (!std::getline(m_file, line)) {
        return false;
    }
    m_stats.totalLines++;

    line = Trim(line);
    try {
        code.code = std::stoi(line);
    } catch (...) {
        return false;
    }

    // Değer satırını oku
    if (!std::getline(m_file, line)) {
        return false;
    }
    m_stats.totalLines++;

    code.value = Trim(line);
    return true;
}

void DXFReader::PushBackCode(const DXFCode& code) {
    m_pendingCode = code;
    m_hasPendingCode = true;
}

bool DXFReader::ReadHeader() {
    DXFCode code;
    
    while (ReadCode(code)) {
        if (code.code == 0 && code.value == "ENDSEC") {
            break;
        }
        
        // Header variable'ları
        if (code.code == 9) {
            std::string varName = code.value;
            
            if (varName == "$ACADVER") {
                ReadCode(code);
                m_header.version = code.value;
            } else if (varName == "$INSUNITS") {
                if (ReadCode(code) && code.code == 70)
                    m_header.insUnits = code.AsInt();
            } else if (varName == "$LTSCALE") {
                if (ReadCode(code) && code.code == 40)
                    m_header.ltscale = code.AsDouble();
            } else if (varName == "$EXTMIN") {
                // Extent minimum
                if (ReadCode(code) && code.code == 10) m_header.extMin.x = code.AsDouble();
                if (ReadCode(code) && code.code == 20) m_header.extMin.y = code.AsDouble();
                if (ReadCode(code) && code.code == 30) m_header.extMin.z = code.AsDouble();
            } else if (varName == "$EXTMAX") {
                // Extent maximum
                if (ReadCode(code) && code.code == 10) m_header.extMax.x = code.AsDouble();
                if (ReadCode(code) && code.code == 20) m_header.extMax.y = code.AsDouble();
                if (ReadCode(code) && code.code == 30) m_header.extMax.z = code.AsDouble();
            }
        }
    }
    
    return true;
}

bool DXFReader::ReadTables() {
    DXFCode code;
    
    while (ReadCode(code)) {
        if (code.code == 0 && code.value == "ENDSEC") {
            break;
        }
        
        // TABLE başlangıcı
        if (code.code == 0 && code.value == "TABLE") {
            // Table adını oku
            if (!ReadCode(code) || code.code != 2) continue;
            
            std::string tableName = code.value;
            
            if (tableName == "LAYER") {
                // LAYER table - katman tanımları
                while (ReadCode(code)) {
                    if (code.code == 0 && code.value == "ENDTAB") break;
                    
                    if (code.code == 0 && code.value == "LAYER") {
                        // Bir layer tanımı
                        Layer layer;
                        std::string layerName;

                        while (ReadCode(code)) {
                            if (code.code == 0) {
                                PushBackCode(code);
                                break;
                            }
                            
                            if (code.code == 2) {
                                // Layer name
                                layerName = code.value;
                            } else if (code.code == 62) {
                                // Color number
                                layer.SetColor(Layer::GetColorFromACI(code.AsInt()));
                            } else if (code.code == 70) {
                                // Flags (frozen, locked)
                                int flags = code.AsInt();
                                if (flags & 1) layer.SetFrozen(true);
                                if (flags & 4) layer.SetLocked(true);
                            }
                        }
                        
                        if (!layerName.empty()) {
                            m_layers[layerName] = layer;
                        }
                    }
                }
            } else {
                // Diğer table'lar (LTYPE, STYLE) şimdilik skip
                while (ReadCode(code)) {
                    if (code.code == 0 && code.value == "ENDTAB") break;
                }
            }
        }
    }
    
    return true;
}

bool DXFReader::ReadEntities() {
    DXFCode code;

    while (ReadCode(code)) {
        if (code.code == 0 && code.value == "ENDSEC") {
            break;
        }

        if (code.code == 0) {
            std::string entityType = code.value;

            if (entityType == "EOF") {
                PushBackCode(code);
                break;
            }

            auto entity = ReadEntity(entityType);
            if (entity) {
                m_entities.push_back(std::move(entity));
            } else {
                m_stats.skippedEntities++;
            }
        }
    }

    return true;
}

std::unique_ptr<Entity> DXFReader::ReadEntity(const std::string& entityType) {
    if      (entityType == "LWPOLYLINE") return ReadLWPolyline();
    else if (entityType == "LINE")       return ReadLine();
    else if (entityType == "ARC")        return ReadArc();
    else if (entityType == "CIRCLE")     return ReadCircle();
    else if (entityType == "ELLIPSE")    return ReadEllipse();
    else if (entityType == "SPLINE")     return ReadSpline();
    else if (entityType == "INSERT")     return ReadInsert();
    else if (entityType == "TEXT")       return ReadText();
    else if (entityType == "MTEXT")      return ReadMText();
    else if (entityType == "HATCH")      return ReadHatch();
    else { SkipEntity(); return nullptr; }
}

std::unique_ptr<Entity> DXFReader::ReadEllipse() {
    DXFCode code;
    EntityProps props;
    geom::Vec3 center;
    geom::Vec3 majorAxis(1.0, 0.0, 0.0); // relative to center
    double axisRatio  = 1.0;
    double startParam = 0.0;
    double endParam   = 6.283185307179586; // 2π

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if      (code.code == 10) center.x    = code.AsDouble();
        else if (code.code == 20) center.y    = code.AsDouble();
        else if (code.code == 30) center.z    = code.AsDouble();
        else if (code.code == 11) majorAxis.x = code.AsDouble(); // major axis endpoint (relative)
        else if (code.code == 21) majorAxis.y = code.AsDouble();
        else if (code.code == 31) majorAxis.z = code.AsDouble();
        else if (code.code == 40) axisRatio   = code.AsDouble();
        else if (code.code == 41) startParam  = code.AsDouble();
        else if (code.code == 42) endParam    = code.AsDouble();
    }

    double semiMajor = std::sqrt(majorAxis.x * majorAxis.x + majorAxis.y * majorAxis.y);
    if (semiMajor < 1e-12) return nullptr;

    double rotAngle = std::atan2(majorAxis.y, majorAxis.x);

    constexpr double twoPi = 6.283185307179586;
    bool full = (std::abs(endParam - startParam) < 1e-10) ||
                (std::abs(endParam - startParam - twoPi) < 1e-10);
    if (full) { startParam = 0.0; endParam = twoPi; }

    auto ellipse = std::make_unique<Ellipse>(center, semiMajor, axisRatio,
                                              rotAngle, startParam, endParam);
    ApplyProps(ellipse.get(), props);
    return ellipse;
}

std::unique_ptr<Entity> DXFReader::ReadSpline() {
    DXFCode code;
    EntityProps props;
    auto spline = std::make_unique<Spline>();

    std::vector<geom::Vec3> ctrlPts, fitPts;
    std::vector<double> knots;
    int degree = 3;
    bool closed = false;
    int numKnots = 0, numCtrl = 0, numFit = 0;
    // state for repeated group codes
    bool readingCtrl = false, readingFit = false;
    geom::Vec3 curPt;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;

        if (code.code == 70) {
            int flags = std::stoi(code.value);
            closed = (flags & 1) != 0;
        }
        else if (code.code == 71) degree  = std::stoi(code.value);
        else if (code.code == 72) numKnots = std::stoi(code.value);
        else if (code.code == 73) numCtrl  = std::stoi(code.value);
        else if (code.code == 74) numFit   = std::stoi(code.value);
        else if (code.code == 40) knots.push_back(code.AsDouble());
        // Control points: 10/20/30 sequence
        else if (code.code == 10) { readingCtrl = true; readingFit = false; curPt = {}; curPt.x = code.AsDouble(); }
        else if (code.code == 20 && readingCtrl) curPt.y = code.AsDouble();
        else if (code.code == 30 && readingCtrl) { curPt.z = code.AsDouble(); ctrlPts.push_back(curPt); }
        // Fit points: 11/21/31 sequence
        else if (code.code == 11) { readingFit = true; readingCtrl = false; curPt = {}; curPt.x = code.AsDouble(); }
        else if (code.code == 21 && readingFit) curPt.y = code.AsDouble();
        else if (code.code == 31 && readingFit) { curPt.z = code.AsDouble(); fitPts.push_back(curPt); }
    }
    (void)numKnots; (void)numCtrl; (void)numFit;

    spline->SetDegree(degree);
    spline->SetClosed(closed);
    if (!fitPts.empty())  spline->SetFitPoints(std::move(fitPts));
    if (!ctrlPts.empty()) spline->SetCtrlPoints(std::move(ctrlPts));
    if (!knots.empty())   spline->SetKnots(std::move(knots));

    if (!spline->HasFitPoints() && !spline->HasCtrlPoints()) return nullptr;
    ApplyProps(spline.get(), props);
    return spline;
}

std::unique_ptr<Entity> DXFReader::ReadText() {
    DXFCode code;
    EntityProps props;
    geom::Vec3 insertPt, alignPt;
    double height = 2.5, rotation = 0.0;
    std::string content;
    int hAlign = 0, vAlign = 0;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if      (code.code == 1)  content    = code.value;
        else if (code.code == 10) insertPt.x = code.AsDouble();
        else if (code.code == 20) insertPt.y = code.AsDouble();
        else if (code.code == 30) insertPt.z = code.AsDouble();
        else if (code.code == 11) alignPt.x  = code.AsDouble();
        else if (code.code == 21) alignPt.y  = code.AsDouble();
        else if (code.code == 31) alignPt.z  = code.AsDouble();
        else if (code.code == 40) height     = code.AsDouble();
        else if (code.code == 50) rotation   = code.AsDouble();
        else if (code.code == 72) hAlign     = code.AsInt();
        else if (code.code == 73) vAlign     = code.AsInt();
    }

    if (content.empty()) return nullptr;
    auto txt = std::make_unique<Text>(insertPt, content, height, rotation);
    txt->SetHAlign(hAlign);
    txt->SetVAlign(vAlign);
    txt->SetAlignPoint(alignPt);
    ApplyProps(txt.get(), props);
    return txt;
}

std::unique_ptr<Entity> DXFReader::ReadMText() {
    DXFCode code;
    EntityProps props;
    geom::Vec3 insertPt;
    double height = 2.5, rotation = 0.0, rectWidth = 0.0;
    std::string content;
    int attachPoint = 1; // MTEXT code 71: 1=TL,2=TC,3=TR,4=ML,5=MC,6=MR,7=BL,8=BC,9=BR

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if      (code.code == 1)  content   += code.value; // MTEXT çok satırlı olabilir
        else if (code.code == 3)  content   += code.value; // ek metin chunk'ları
        else if (code.code == 10) insertPt.x = code.AsDouble();
        else if (code.code == 20) insertPt.y = code.AsDouble();
        else if (code.code == 30) insertPt.z = code.AsDouble();
        else if (code.code == 40) height     = code.AsDouble();
        else if (code.code == 41) rectWidth  = code.AsDouble(); // column/wrap width
        else if (code.code == 50) rotation   = code.AsDouble();
        else if (code.code == 71) attachPoint = code.AsInt();
    }

    // MTEXT format kodlarını temizle: \P (paragraf), \~ (boşluk), {\...} blokları
    std::string clean;
    clean.reserve(content.size());
    bool inBrace = false;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '{') { inBrace = true; continue; }
        if (content[i] == '}') { inBrace = false; continue; }
        if (content[i] == '\\' && i + 1 < content.size()) {
            char next = content[i+1];
            if (next == 'P' || next == 'p') { clean += '\n'; i++; continue; }
            if (next == '~')               { clean += ' ';  i++; continue; }
            if (next == 'n' || next == 'N'){ clean += '\n'; i++; continue; }
            // \fFont;  \H height; \S frac; \T spacing — skip to semicolon
            if (next == 'f' || next == 'F' || next == 'H' || next == 'S' || next == 'T' || next == 'Q') {
                i += 2;
                while (i < content.size() && content[i] != ';') i++;
                continue;
            }
            i++; continue; // diğer escape'leri atla
        }
        if (!inBrace) clean += content[i];
    }

    if (clean.empty()) return nullptr;
    auto txt = std::make_unique<Text>(insertPt, clean, height, rotation);
    // MTEXT attachPoint → hAlign/vAlign: rows=top(1-3)/mid(4-6)/bot(7-9), cols=L/C/R
    static const int hTab[] = {0, 0,1,2, 0,1,2, 0,1,2}; // index 1-9
    static const int vTab[] = {0, 3,3,3, 2,2,2, 1,1,1}; // 3=Top,2=Mid,1=Bot
    if (attachPoint >= 1 && attachPoint <= 9) {
        txt->SetHAlign(hTab[attachPoint]);
        txt->SetVAlign(vTab[attachPoint]);
    }
    if (rectWidth > 0.0) txt->SetRectWidth(rectWidth);
    ApplyProps(txt.get(), props);
    return txt;
}

std::unique_ptr<Entity> DXFReader::ReadHatch() {
    DXFCode code;
    EntityProps props;
    std::string patternName = "SOLID";
    double patternAngle = 0.0, patternScale = 1.0;
    std::vector<Hatch::BoundaryVertex> boundary;

    // DXF HATCH boundary reading state
    int boundaryType = 0;    // code 92: boundary path type flags
    int numEdges = 0;        // code 93: edge count
    int edgeType = 0;        // code 72 within edge: 1=LINE,2=ARC,3=ELLIPSE,4=SPLINE
    bool inBoundaryData = false;
    bool inPatternData = false;
    geom::Vec3 edgePt1, edgePt2; // for LINE edges
    double edgeCx = 0, edgeCy = 0, edgeR = 0, edgeA0 = 0, edgeA1 = 0;
    bool edgeCcw = true;
    int edgeIdx = 0;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;

        if (code.code == 2)  { patternName = code.value; continue; }
        if (code.code == 52) { patternAngle = code.AsDouble(); continue; }
        if (code.code == 41) { patternScale = code.AsDouble(); continue; }

        // Boundary path header
        if (code.code == 91) { // number of boundary paths
            inBoundaryData = true;
            inPatternData = false;
            continue;
        }
        if (code.code == 92) { // boundary path type flag
            boundaryType = code.AsInt();
            edgeIdx = 0;
            continue;
        }
        if (code.code == 93) { // edge count
            numEdges = code.AsInt();
            continue;
        }

        // Edge data within boundary
        if (inBoundaryData) {
            if (code.code == 72 && !inPatternData) {
                // flush previous edge if LINE
                if (edgeType == 1 && edgeIdx > 0) {
                    boundary.push_back({edgePt1});
                }
                edgeType = code.AsInt();
                edgePt1 = edgePt2 = {};
                edgeCx = edgeCy = edgeR = edgeA0 = edgeA1 = 0;
                edgeCcw = true;
                edgeIdx++;
                continue;
            }
            if (edgeType == 1) { // LINE
                if      (code.code == 10) edgePt1.x = code.AsDouble();
                else if (code.code == 20) edgePt1.y = code.AsDouble();
                else if (code.code == 11) edgePt2.x = code.AsDouble();
                else if (code.code == 21) edgePt2.y = code.AsDouble();
            } else if (edgeType == 2) { // ARC
                if      (code.code == 10) edgeCx = code.AsDouble();
                else if (code.code == 20) edgeCy = code.AsDouble();
                else if (code.code == 40) edgeR  = code.AsDouble();
                else if (code.code == 50) edgeA0 = code.AsDouble();
                else if (code.code == 51) edgeA1 = code.AsDouble();
                else if (code.code == 73) { // flush arc as tessellated points
                    edgeCcw = code.AsBool();
                    double sweep = edgeA1 - edgeA0;
                    if (edgeCcw && sweep < 0) sweep += 360.0;
                    if (!edgeCcw && sweep > 0) sweep -= 360.0;
                    sweep = std::abs(sweep);
                    int steps = std::max(8, (int)(sweep / 5.0)); // 5° per step
                    for (int k = 0; k < steps; ++k) {
                        double t = (edgeA0 + (edgeCcw ? 1 : -1) * sweep * k / steps) * M_PI / 180.0;
                        Hatch::BoundaryVertex v;
                        v.pos = geom::Vec3(edgeCx + edgeR * std::cos(t),
                                          edgeCy + edgeR * std::sin(t), 0.0);
                        boundary.push_back(v);
                    }
                }
            }
        }

        // Pattern line data starts (code 78 = number of pattern lines)
        if (code.code == 78) { inPatternData = true; inBoundaryData = false; }
    }

    // Flush last LINE edge
    if (edgeType == 1 && !boundary.empty()) {
        boundary.push_back({edgePt2});
    }

    if (boundary.size() < 3) return nullptr;
    auto h = std::make_unique<Hatch>();
    h->SetPatternName(patternName);
    h->SetBoundary(std::move(boundary));
    h->SetPatternAngle(patternAngle);
    h->SetPatternScale(patternScale > 1e-9 ? patternScale : 1.0);
    ApplyProps(h.get(), props);
    return h;
}

std::unique_ptr<Entity> DXFReader::ReadLWPolyline() {
    DXFCode code;
    EntityProps props;
    std::vector<geom::Vec3> vertices;
    std::vector<double> bulges;
    bool closed = false;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if (code.code == 70) {
            closed = (code.AsInt() & 1) != 0;
        } else if (code.code == 90) {
            vertices.reserve(code.AsInt());
            bulges.reserve(code.AsInt());
        } else if (code.code == 10) {
            double x = code.AsDouble();
            if (ReadCode(code) && code.code == 20) {
                vertices.push_back(geom::Vec3(x, code.AsDouble(), 0.0));
                bulges.push_back(0.0);
            }
        } else if (code.code == 42) {
            if (!bulges.empty()) bulges.back() = code.AsDouble();
        }
    }

    if (vertices.size() < 2) return nullptr;

    std::vector<Polyline::Vertex> polyVerts;
    polyVerts.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        Polyline::Vertex v;
        v.pos = vertices[i];
        v.bulge = i < bulges.size() ? bulges[i] : 0.0;
        polyVerts.push_back(v);
    }

    auto polyline = std::make_unique<Polyline>(polyVerts, closed);
    ApplyProps(polyline.get(), props);
    return polyline;
}

std::unique_ptr<Entity> DXFReader::ReadLine() {
    DXFCode code;
    EntityProps props;
    geom::Vec3 start, end;
    bool hasStart = false, hasEnd = false;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if (code.code == 10) { start.x = code.AsDouble(); hasStart = true; }
        else if (code.code == 20) start.y = code.AsDouble();
        else if (code.code == 30) start.z = code.AsDouble();
        else if (code.code == 11) { end.x = code.AsDouble(); hasEnd = true; }
        else if (code.code == 21) end.y = code.AsDouble();
        else if (code.code == 31) end.z = code.AsDouble();
    }

    if (!hasStart || !hasEnd) return nullptr;
    auto line = std::make_unique<Line>(start, end);
    ApplyProps(line.get(), props);
    return line;
}

std::unique_ptr<Entity> DXFReader::ReadArc() {
    DXFCode code;
    EntityProps props;
    geom::Vec3 center;
    double radius = 0.0, startAngle = 0.0, endAngle = 360.0;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if      (code.code == 10) center.x = code.AsDouble();
        else if (code.code == 20) center.y = code.AsDouble();
        else if (code.code == 30) center.z = code.AsDouble();
        else if (code.code == 40) radius = code.AsDouble();
        else if (code.code == 50) startAngle = code.AsDouble();
        else if (code.code == 51) endAngle = code.AsDouble();
    }

    if (radius <= 0.0) return nullptr;
    auto arc = std::make_unique<Arc>(center, radius, startAngle, endAngle);
    ApplyProps(arc.get(), props);
    return arc;
}

std::unique_ptr<Entity> DXFReader::ReadCircle() {
    DXFCode code;
    EntityProps props;
    geom::Vec3 center;
    double radius = 0.0;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }
        if (ReadEntityProp(code, props)) continue;
        if      (code.code == 10) center.x = code.AsDouble();
        else if (code.code == 20) center.y = code.AsDouble();
        else if (code.code == 30) center.z = code.AsDouble();
        else if (code.code == 40) radius = code.AsDouble();
    }

    if (radius <= 0.0) return nullptr;
    auto circle = std::make_unique<Circle>(center, radius);
    ApplyProps(circle.get(), props);
    return circle;
}

void DXFReader::SkipEntity() {
    DXFCode code;
    while (ReadCode(code)) {
        if (code.code == 0) {
            PushBackCode(code);
            break;
        }
    }
}

bool DXFReader::ReadBlocks() {
    DXFCode code;

    while (ReadCode(code)) {
        if (code.code == 0 && code.value == "ENDSEC") {
            break;
        }

        if (code.code == 0 && code.value == "BLOCK") {
            // Block tanımı başlangıcı — adını oku
            std::string blockName;
            geom::Vec3 basePoint{0, 0, 0};

            while (ReadCode(code)) {
                if (code.code == 0) {
                    PushBackCode(code);
                    break;
                }
                if (code.code == 2) blockName = code.value;
                else if (code.code == 10) basePoint.x = code.AsDouble();
                else if (code.code == 20) basePoint.y = code.AsDouble();
                else if (code.code == 30) basePoint.z = code.AsDouble();
            }

            // Block içindeki entity'leri oku (ENDBLK'a kadar)
            std::vector<std::unique_ptr<Entity>> blockEntities;
            while (ReadCode(code)) {
                if (code.code == 0 && code.value == "ENDBLK") {
                    // ENDBLK'ın kendi property'lerini skip et
                    SkipEntity();
                    break;
                }
                if (code.code == 0) {
                    std::string entityType = code.value;
                    auto entity = ReadEntity(entityType);
                    if (entity) {
                        // Base point offset uygula
                        entity->Move(geom::Vec3{0, 0, 0} - basePoint);
                        blockEntities.push_back(std::move(entity));
                    }
                }
            }

            if (!blockName.empty()) {
                m_blocks[blockName] = std::move(blockEntities);
            }
        }
    }

    return true;
}

std::unique_ptr<Entity> DXFReader::ReadInsert() {
    DXFCode code;

    std::string blockName;
    std::string layerName = "0";
    geom::Vec3 insertionPoint{0, 0, 0};
    geom::Vec3 scale{1, 1, 1};
    double rotation = 0.0;

    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }

        if (code.code == 2) blockName = code.value;
        else if (code.code == 8) layerName = code.value;
        else if (code.code == 10) insertionPoint.x = code.AsDouble();
        else if (code.code == 20) insertionPoint.y = code.AsDouble();
        else if (code.code == 30) insertionPoint.z = code.AsDouble();
        else if (code.code == 41) scale.x = code.AsDouble();
        else if (code.code == 42) scale.y = code.AsDouble();
        else if (code.code == 43) scale.z = code.AsDouble();
        else if (code.code == 50) rotation = code.AsDouble();
    }

    // Block tanımını bul — tüm entity'leri expand et
    auto blockIt = m_blocks.find(blockName);
    if (blockIt == m_blocks.end() || blockIt->second.empty()) return nullptr;

    // DXF kuralı: blok içinde layer "0" olan entity'ler INSERT'in layer'ını miras alır.
    // Tüm entity'leri klonla, transform (scale → rotate → translate) uygula, m_entities'e ekle.
    for (const auto& src : blockIt->second) {
        auto clone = src->Clone();
        if (!clone) continue;
        clone->Scale(scale);
        clone->Rotate(rotation);
        clone->Move(insertionPoint);
        if (clone->GetLayerName() == "0" || clone->GetLayerName().empty()) {
            clone->SetLayer(GetOrCreateLayer(layerName));
            clone->SetLayerName(layerName); // renderer lookup için isim de senkronize et
        }
        m_entities.push_back(std::move(clone));
    }
    return nullptr; // entity'ler zaten m_entities'e eklendi
}

bool DXFReader::ReadEntityProp(const DXFCode& code, EntityProps& props) {
    switch (code.code) {
        case 8:   props.layer = code.value; return true;
        case 62: { // ACI renk (0=ByBlock, 256=ByLayer)
            int aci = code.AsInt();
            if (aci == 0)   { props.color = Color::ByBlock(); props.hasExplicitColor = true; }
            else if (aci == 256) { props.color = Color::ByLayer(); props.hasExplicitColor = false; }
            else             { props.color = Color::FromACI(aci); props.hasExplicitColor = true; }
            return true;
        }
        case 420: { // Truecolor RGB (öncelik: code 420 > code 62)
            int rgb = code.AsInt();
            props.color = Color(
                static_cast<uint8_t>((rgb >> 16) & 0xFF),
                static_cast<uint8_t>((rgb >> 8)  & 0xFF),
                static_cast<uint8_t>( rgb        & 0xFF));
            props.hasExplicitColor = true;
            return true;
        }
        case 370: { // Lineweight (1/100 mm biriminde, -1=ByLayer, -2=ByBlock, -3=Default)
            int lw = code.AsInt();
            props.lineweight = (lw > 0) ? lw / 100.0 : -1.0;
            return true;
        }
        case 48:  props.ltScale = code.AsDouble(); return true; // Entity ltype scale
        default:  return false;
    }
}

void DXFReader::ApplyProps(Entity* entity, const EntityProps& props) {
    entity->SetLayer(GetOrCreateLayer(props.layer));
    entity->SetLayerName(props.layer); // renderer GetLayerName() ile layer rengini lookup eder
    if (props.hasExplicitColor)
        entity->SetColor(props.color);
    if (props.lineweight > 0)
        entity->SetLineweight(props.lineweight);
    if (props.ltScale != 1.0)
        entity->SetLinetypeScale(props.ltScale);
}

Layer* DXFReader::GetOrCreateLayer(const std::string& layerName) {
    auto it = m_layers.find(layerName);
    if (it != m_layers.end()) {
        return &it->second;
    }
    
    // Yeni layer oluştur
    m_layers[layerName] = Layer(layerName);
    return &m_layers[layerName];
}

std::vector<std::string> DXFReader::GetLayerNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_layers) {
        names.push_back(pair.first);
    }
    return names;
}

std::map<EntityType, size_t> DXFReader::GetEntityCountByType() const {
    std::map<EntityType, size_t> counts;
    for (const auto& entity : m_entities) {
        counts[entity->GetType()]++;
    }
    return counts;
}

void DXFReader::FilterByLayer(const std::vector<std::string>& layerNames) {
    m_layerFilter = layerNames;
}

void DXFReader::FilterByType(const std::vector<EntityType>& types) {
    m_typeFilter = types;
}

void DXFReader::ClearFilters() {
    m_layerFilter.clear();
    m_typeFilter.clear();
}

std::vector<Entity*> DXFReader::GetFilteredEntities() const {
    std::vector<Entity*> filtered;
    
    for (const auto& entity : m_entities) {
        if (PassesFilter(entity.get())) {
            filtered.push_back(entity.get());
        }
    }
    
    return filtered;
}

bool DXFReader::PassesFilter(const Entity* entity) const {
    if (!entity) return false;
    
    // Layer filter
    if (!m_layerFilter.empty()) {
        const Layer* layer = entity->GetLayer();
        if (!layer) return false;
        
        bool layerMatch = false;
        std::string layerName = layer->GetName();
        for (const auto& filterName : m_layerFilter) {
            if (layerName == filterName) {
                layerMatch = true;
                break;
            }
        }
        if (!layerMatch) return false;
    }
    
    // Type filter
    if (!m_typeFilter.empty()) {
        bool typeMatch = false;
        for (EntityType filterType : m_typeFilter) {
            if (entity->GetType() == filterType) {
                typeMatch = true;
                break;
            }
        }
        if (!typeMatch) return false;
    }
    
    return true;
}

void DXFReader::ImportLayersTo(LayerManager& layerManager) {
    for (const auto& pair : m_layers) {
        const std::string& name = pair.first;
        const Layer& layer = pair.second;
        
        // Layer manager'da yoksa oluştur
        Layer* managedLayer = layerManager.CreateLayer(name);
        if (managedLayer) {
            // Özellikleri kopyala
            managedLayer->SetColor(layer.GetColor());
            managedLayer->SetVisible(layer.IsVisible());
            managedLayer->SetFrozen(layer.IsFrozen());
            managedLayer->SetLocked(layer.IsLocked());
        }
    }
}

std::string DXFReader::Statistics::ToString() const {
    std::stringstream ss;
    ss << "DXF Okuma İstatistikleri:\n";
    ss << "  Toplam Satır: " << totalLines << "\n";
    ss << "  Toplam Entity: " << totalEntities << "\n";
    ss << "  Toplam Layer: " << totalLayers << "\n";
    ss << "  Atlanan Entity: " << skippedEntities << "\n";
    ss << "  Okuma Süresi: " << readTimeMs << " ms\n";
    return ss.str();
}

std::string DXFReader::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

} // namespace vkt::cad
