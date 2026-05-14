/**
 * @file DWGReader.cpp
 * @brief DWG Dosya Okuyucu - Basic LibreDWG Implementation
 */

#include "cad/DWGReader.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Arc.hpp"
#include "cad/Ellipse.hpp"
#include "cad/Polyline.hpp"
#include "cad/Hatch.hpp"
#include "cad/Text.hpp"
#include <sstream>
#include <chrono>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <filesystem>

#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// LibreDWG headers
extern "C" {
    #include <dwg.h>
    #include <dwg_api.h>
}

namespace vkt {
namespace cad {

std::string DWGStatistics::ToString() const {
    std::ostringstream oss;
    oss << "DWG Version: " << dwgVersion << "\n"
        << "Total Entities: " << entityCount << "\n"
        << "  Lines: " << lineCount << "\n"
        << "  Polylines: " << polylineCount << "\n"
        << "  Arcs: " << arcCount << "\n"
        << "  Circles: " << circleCount << "\n"
        << "  Ellipses: " << ellipseCount << "\n"
        << "  Splines: " << splineCount << "\n"
        << "  Texts: " << textCount << "\n"
        << "  MTexts: " << mtextCount << "\n"
        << "  Hatches: " << hatchCount << "\n"
        << "  INSERT expanded: " << insertExpanded << "\n"
        << "Layers: " << layerCount << "\n"
        << "Read Time: " << readTimeMs << " ms";
    return oss.str();
}

DWGReader::~DWGReader() {
    Clear();
}

void DWGReader::Clear() {
    m_entities.clear();
    m_layers.clear();
    m_stats = DWGStatistics();
    m_errorMessage.clear();
    
    if (m_dwgData) {
        dwg_free(static_cast<Dwg_Data*>(m_dwgData));
        m_dwgData = nullptr;
    }
}

bool DWGReader::Read(const std::string& filepath) {
    Clear();
    m_filePath = filepath;
    m_visitedXrefs.clear();
    m_missingXrefs.clear();

    auto start = std::chrono::high_resolution_clock::now();
    
    // Allocate and initialize DWG data structure
    Dwg_Data* dwg = static_cast<Dwg_Data*>(malloc(sizeof(Dwg_Data)));
    if (!dwg) {
        m_errorMessage = "Bellek ayırma hatası";
        return false;
    }
    memset(dwg, 0, sizeof(Dwg_Data));
    
    // Read DWG file
    int errorCode = dwg_read_file(filepath.c_str(), dwg);
    
    // Check for critical errors only (128 and above)
    // Non-critical errors (like unsupported classes) can be tolerated
    const int DWG_ERR_CRITICAL = 128;
    if (errorCode >= DWG_ERR_CRITICAL) {
        std::string errorMsg = "DWG okuma hatası (kod: " + std::to_string(errorCode) + ").";
        
        // Decode error flags
        if (errorCode & 128) errorMsg += " Class tanımları bulunamadı.";
        if (errorCode & 256) errorMsg += " Bölüm bulunamadı.";
        if (errorCode & 1024) errorMsg += " İç hata.";
        if (errorCode & 2048) errorMsg += " Geçersiz DWG formatı.";
        if (errorCode & 4096) errorMsg += " Dosya okuma hatası.";
        if (errorCode & 8192) errorMsg += " Bellek yetersiz.";
        
        m_errorMessage = errorMsg;
        dwg_free(dwg);
        return false;
    }
    
    // Log non-critical warnings
    if (errorCode != 0) {
        m_errorMessage = "Uyarı (kod: " + std::to_string(errorCode) + "): ";
        if (errorCode & 2) m_errorMessage += "Bazı desteklenmeyen özellikler atlandı. ";
        if (errorCode & 4) m_errorMessage += "Bazı bilinmeyen class'lar atlandı. ";
        if (errorCode & 64) m_errorMessage += "Bazı geçersiz değerler düzeltildi. ";
        // Continue despite warnings
    }
    
    m_dwgData = dwg;
    
    // Get version
    m_stats.dwgVersion = "R" + std::to_string(dwg->header.version);
    
    // Create default layer
    Layer defaultLayer("0", Color::White());
    m_layers["0"] = defaultLayer;
    m_stats.layerCount = 1;
    
    // Layer bilgilerini çıkar (entity parse'dan ÖNCE — renk çözümlemesi için)
    ExtractLayers(dwg);

    // Entity'leri parse et
    ParseEntities(dwg);

    auto end = std::chrono::high_resolution_clock::now();
    m_stats.readTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    m_stats.entityCount = m_entities.size();

    std::cout << "[DWGReader] Parse complete: " << m_entities.size() << " entities"
              << " (LINE=" << m_stats.lineCount
              << " ARC=" << m_stats.arcCount
              << " CIRCLE=" << m_stats.circleCount
              << " POLY=" << m_stats.polylineCount
              << " ELLIPSE=" << m_stats.ellipseCount
              << " SPLINE=" << m_stats.splineCount
              << " INSERT_EXP=" << m_stats.insertExpanded
              << " LAYERS=" << m_stats.layerCount << ")" << std::endl;
    
    if (m_stats.entityCount == 0) {
        m_errorMessage = "DWG dosyası okundu ancak desteklenen entity bulunamadı. Desteklenenler: LINE, ARC, CIRCLE, LWPOLYLINE.";
        return false;
    }
    
    return true;
}

// UTF-16LE char* → UTF-8 std::string dönüşümü (LibreDWG R2007+ DWG için)
static std::string Utf16LeToUtf8(const char* p) {
    if (!p) return "";
    // İlk iki byte'a bakarak UTF-16 mi yoksa ASCII/UTF-8 mi belirle
    bool isUtf16 = (p[0] != '\0' && p[1] == '\0');
    if (!isUtf16) return std::string(p);
    std::string out;
    const uint16_t* wp = reinterpret_cast<const uint16_t*>(p);
    while (*wp) {
        uint16_t ch = *wp++;
        if (ch < 0x80) {
            out += static_cast<char>(ch);
        } else if (ch < 0x800) {
            out += static_cast<char>(0xC0 | (ch >> 6));
            out += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (ch >> 12));
            out += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return out;
}

// Compound INSERT transform: base_pt subtraction, scale, rotate, translate
struct InsertTransform {
    geom::Vec3 insPoint;
    geom::Vec3 basePt;   // block base point to subtract before transform
    double xScale = 1.0;
    double yScale = 1.0;
    double rotation = 0.0; // radians
};

// Apply a single INSERT transform to a point
static geom::Vec3 ApplyTransform(const geom::Vec3& pt, const InsertTransform& t) {
    // Subtract block base point
    double x = (pt.x - t.basePt.x) * t.xScale;
    double y = (pt.y - t.basePt.y) * t.yScale;
    double cosR = std::cos(t.rotation);
    double sinR = std::sin(t.rotation);
    return geom::Vec3(
        t.insPoint.x + x * cosR - y * sinR,
        t.insPoint.y + x * sinR + y * cosR,
        pt.z
    );
}

// Apply a chain of transforms: innermost (back) first, then outer (front)
static geom::Vec3 ApplyTransformChain(const geom::Vec3& pt,
                                       const std::vector<InsertTransform>& chain) {
    geom::Vec3 result = pt;
    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i) {
        result = ApplyTransform(result, chain[i]);
    }
    return result;
}

// Compute effective uniform scale from transform chain
static double GetChainScale(const std::vector<InsertTransform>& chain) {
    double s = 1.0;
    for (const auto& t : chain) s *= std::abs(t.xScale);
    return s;
}

// Compute effective rotation from transform chain
static double GetChainRotation(const std::vector<InsertTransform>& chain) {
    double r = 0.0;
    for (const auto& t : chain) r += t.rotation;
    return r;
}

/**
 * @brief Entity'den renk ve layer bilgisini çıkar ve ata
 *
 * Renk çözümleme önceliği:
 * 1. Truecolor (method 0xc3): RGB doğrudan
 * 2. ACI indeksi (1-255): ACI→RGB dönüşümü
 * 3. BYLAYER (256): Entity'nin layer'ının rengini kullan
 * 4. BYBLOCK (0): Beyaz fallback
 *
 * Ayrıca entity'nin layer adını DWG handle'ından çözümler.
 */
void DWGReader::ExtractEntityColorAndLayer(void* obj_ptr, Entity* entity) {
    if (!entity) return;
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity) return;

    // Layer adını çıkar — UTF-16LE → UTF-8 dönüşümü ile
    std::string layerName = "0";
    if (obj->tio.entity->layer) {
        Dwg_Object* layerObj = obj->tio.entity->layer->obj;
        if (layerObj && layerObj->tio.object && layerObj->tio.object->tio.LAYER) {
            const char* p = layerObj->tio.object->tio.LAYER->name;
            if (p) {
                bool isUtf16 = (p[0] != '\0' && p[1] == '\0');
                if (isUtf16) {
                    layerName.clear();
                    const uint16_t* wp = reinterpret_cast<const uint16_t*>(p);
                    while (*wp) {
                        uint16_t ch = *wp++;
                        if (ch < 0x80) {
                            layerName += static_cast<char>(ch);
                        } else if (ch < 0x800) {
                            layerName += static_cast<char>(0xC0 | (ch >> 6));
                            layerName += static_cast<char>(0x80 | (ch & 0x3F));
                        } else {
                            layerName += static_cast<char>(0xE0 | (ch >> 12));
                            layerName += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                            layerName += static_cast<char>(0x80 | (ch & 0x3F));
                        }
                    }
                } else {
                    layerName = p;
                }
            }
        }
    }
    entity->SetLayerName(layerName);

    // Renk çözümleme
    // DWG method byte: 0xC2=truecolor RGB, 0xC3=ACAD color number (ACI idx in low byte)
    Dwg_Color& c = obj->tio.entity->color;

    if (c.method == 0xc2 && c.rgb != 0) {
        // True RGB (24-bit): 0x00RRGGBB
        float r = ((c.rgb >> 16) & 0xFF) / 255.0f;
        float g = ((c.rgb >> 8) & 0xFF) / 255.0f;
        float b = (c.rgb & 0xFF) / 255.0f;
        entity->SetColor(Color(r, g, b));
    } else if (c.method == 0xc3 && (c.rgb & 0xFF) >= 1) {
        // ACAD Color Number: ACI index in low byte of rgb
        entity->SetColor(Color::FromACI(static_cast<int>(c.rgb & 0xFF)));
    } else if (c.index == 0) {
        // ByBlock: INSERT rengini INSERT expand aşamasında atanacak
        entity->SetColor(Color::ByBlock());
    } else if (c.index >= 1 && c.index <= 255) {
        // ACI renk indeksi (1-255)
        entity->SetColor(Color::FromACI(c.index));
    } else {
        // BYLAYER (index==256 veya belirsiz): Color::ByLayer() varsayılanı kalır.
        // Renderer m_layerMap üzerinden entity->GetLayerName() ile çözümler.
    }

    // Linetype çıkar (ltype_flags: 0=ByLayer, 1=ByBlock, 2=Continuous, 3=entity-specific)
    {
        BITCODE_BB ltFlags = obj->tio.entity->ltype_flags;
        if (ltFlags == 2) {
            entity->SetLinetype("CONTINUOUS");
        } else if (ltFlags == 3 && obj->tio.entity->ltype) {
            Dwg_Object* ltObj = obj->tio.entity->ltype->obj;
            if (ltObj && ltObj->tio.object && ltObj->tio.object->tio.LTYPE) {
                const char* name = ltObj->tio.object->tio.LTYPE->name;
                if (name) {
                    std::string lt = name;
                    for (auto& ch : lt) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
                    entity->SetLinetype(lt);
                }
            }
        }
        // ltFlags 0/1 → ByLayer/ByBlock → default CONTINUOUS (rendered solid, layer may override)
    }

    // Lineweight çıkar (linewt: 0=ByLayer, 1=ByBlock, 2=Default, 3-24=specific widths via dxf_cvt_lweight)
    {
        // dxf_cvt_lweight converts encoded byte to mm*100: 0=0.05mm, 1=0.09mm, ...
        // index 0 special: entity inherits from layer
        int8_t linewt = static_cast<int8_t>(obj->tio.entity->linewt);
        if (linewt > 2) {
            // Convert encoded value to mm using standard AutoCAD lineweight table
            static const float kLwTable[] = {
                0.05f, 0.09f, 0.13f, 0.15f, 0.18f, 0.20f, 0.25f,
                0.30f, 0.35f, 0.40f, 0.50f, 0.53f, 0.60f, 0.70f,
                0.80f, 0.90f, 1.00f, 1.06f, 1.20f, 1.40f, 1.58f,
                2.00f, 2.11f
            };
            int idx = linewt - 3;
            if (idx >= 0 && idx < (int)(sizeof(kLwTable)/sizeof(kLwTable[0])))
                entity->SetLineweight(kLwTable[idx]);
        }
        // else ByLayer / ByBlock / Default → m_lineweight stays -1.0
    }
}

// Parse a single entity object into an Entity*, returns nullptr if unsupported
Entity* DWGReader::ParseEntityByType(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    Entity* entity = nullptr;

    switch (obj->type) {
        case DWG_TYPE_LINE:
            entity = ParseLine(obj);
            if (entity) m_stats.lineCount++;
            break;
        case DWG_TYPE_CIRCLE:
            entity = ParseCircle(obj);
            if (entity) m_stats.circleCount++;
            break;
        case DWG_TYPE_ARC:
            entity = ParseArc(obj);
            if (entity) m_stats.arcCount++;
            break;
        case DWG_TYPE_LWPOLYLINE:
            entity = ParseLWPolyline(obj);
            if (entity) m_stats.polylineCount++;
            break;
        case DWG_TYPE_POLYLINE_2D:
        case DWG_TYPE_POLYLINE_3D:
            entity = ParsePolyline(obj);
            if (entity) m_stats.polylineCount++;
            break;
        case DWG_TYPE_ELLIPSE:
            entity = ParseEllipse(obj);
            if (entity) m_stats.ellipseCount++;
            break;
        case DWG_TYPE_SPLINE:
            entity = ParseSpline(obj);
            if (entity) m_stats.splineCount++;
            break;
        case DWG_TYPE_POINT:
            entity = ParsePoint(obj);
            if (entity) m_stats.lineCount += 2;
            break;
        case DWG_TYPE_TEXT:
            entity = ParseText(obj);
            if (entity) m_stats.textCount++;
            break;
        case DWG_TYPE_MTEXT:
            entity = ParseMText(obj);
            if (entity) m_stats.mtextCount++;
            break;
        case DWG_TYPE_HATCH:
            entity = ParseHatch(obj);
            if (entity) m_stats.hatchCount++;
            break;
        default:
            break;
    }

    // Entity'ye renk ve layer bilgisi ata
    ExtractEntityColorAndLayer(obj_ptr, entity);

    return entity;
}

// Expand INSERT entity recursively with accumulated transform chain
void DWGReader::ExpandInsert(void* obj_ptr, void* dwg_ptr,
                              std::vector<InsertTransform>& transformChain, int depth) {
    if (depth > 8) return; // Max nesting depth

    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    Dwg_Data* dwg = static_cast<Dwg_Data*>(dwg_ptr);

    if (!obj->tio.entity || !obj->tio.entity->tio.INSERT) return;
    Dwg_Entity_INSERT* ins = obj->tio.entity->tio.INSERT;

    // INSERT'in kendi rengi — ByBlock entity'lere aktarılacak
    Color insertColor = Color::ByLayer(); // fallback: INSERT ByLayer ise child'lar da ByLayer olsun
    {
        Dwg_Color& ic = obj->tio.entity->color;
        if (ic.method == 0xc2 && ic.rgb != 0) {
            float ir = ((ic.rgb >> 16) & 0xFF) / 255.0f;
            float ig = ((ic.rgb >> 8) & 0xFF) / 255.0f;
            float ib = (ic.rgb & 0xFF) / 255.0f;
            insertColor = Color(ir, ig, ib);
        } else if (ic.method == 0xc3 && (ic.rgb & 0xFF) >= 1) {
            insertColor = Color::FromACI(static_cast<int>(ic.rgb & 0xFF));
        } else if (ic.index >= 1 && ic.index <= 255) {
            insertColor = Color::FromACI(ic.index);
        }
        // else: ByLayer INSERT → insertColor kalır ByLayer
    }

    // INSERT'in kendi layer ismi — layer "0" child entity'leri bunu miras alır
    std::string insertLayerName = "0";
    {
        if (obj->tio.entity->layer) {
            Dwg_Object* layerObj = obj->tio.entity->layer->obj;
            if (layerObj && layerObj->tio.object && layerObj->tio.object->tio.LAYER) {
                const char* p = layerObj->tio.object->tio.LAYER->name;
                if (p) insertLayerName = Utf16LeToUtf8(p);
            }
        }
    }

    // Build transform for this INSERT
    InsertTransform xform;
    xform.insPoint = geom::Vec3(ins->ins_pt.x, ins->ins_pt.y, 0.0);
    xform.xScale = ins->scale.x;
    xform.yScale = ins->scale.y;
    xform.rotation = ins->rotation; // radians

    if (std::abs(xform.xScale) < 1e-12) xform.xScale = 1.0;
    if (std::abs(xform.yScale) < 1e-12) xform.yScale = 1.0;

    // Resolve block header
    if (!ins->block_header || !ins->block_header->obj) return;
    Dwg_Object* blockObj = ins->block_header->obj;
    if (blockObj->type != DWG_TYPE_BLOCK_HEADER) return;
    if (!blockObj->tio.object || !blockObj->tio.object->tio.BLOCK_HEADER) return;

    Dwg_Object_BLOCK_HEADER* bh = blockObj->tio.object->tio.BLOCK_HEADER;
    xform.basePt = geom::Vec3(bh->base_pt.x, bh->base_pt.y, 0.0);

    // Xref detection: if this block is an external reference, load the referenced file
    if (bh->blkisxref && bh->xref_pname && bh->xref_pname[0]) {
        transformChain.push_back(xform);
        std::string xrefPath = bh->xref_pname;
        ExpandXref(xrefPath, transformChain, depth);
        transformChain.pop_back();
        return;
    }

    unsigned long blockHandle = blockObj->handle.value;

    // Push this transform onto the chain (innermost = back)
    transformChain.push_back(xform);

    // Find entities owned by this block
    for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
        Dwg_Object* child = &dwg->object[i];
        if (!child || child->supertype != DWG_SUPERTYPE_ENTITY) continue;
        if (!child->tio.entity) continue;

        unsigned long ownerHandle = child->tio.entity->ownerhandle ?
                                     child->tio.entity->ownerhandle->absolute_ref : 0;
        if (ownerHandle != blockHandle) continue;

        // İç içe INSERT/MINSERT'lere recursion
        if (child->type == DWG_TYPE_INSERT) {
            ExpandInsert(child, dwg, transformChain, depth + 1);
            continue;
        }
        if (child->type == DWG_TYPE_MINSERT) {
            // Nested MINSERT: pass the current transform chain as parent context
            ExpandMInsertNested(child, dwg, &transformChain, depth + 1);
            continue;
        }

        // Parse entity in block-local coords
        Entity* entity = ParseEntityByType(child);
        if (!entity) continue;

        // Transform entity through the full chain to world coordinates
        double chainScale = GetChainScale(transformChain);
        double chainRotation = GetChainRotation(transformChain);

        // Preserve color and layer before rebuilding the transformed entity
        Color  savedColor     = entity->GetColor();
        std::string savedLayer = entity->GetLayerName();

        switch (entity->GetType()) {
            case EntityType::Line: {
                Line* line = static_cast<Line*>(entity);
                geom::Vec3 s = ApplyTransformChain(line->GetStart(), transformChain);
                geom::Vec3 e = ApplyTransformChain(line->GetEnd(), transformChain);
                delete entity;
                entity = new Line(s, e);
                break;
            }
            case EntityType::Circle: {
                Circle* circle = static_cast<Circle*>(entity);
                geom::Vec3 c = ApplyTransformChain(circle->GetCenter(), transformChain);
                double r = circle->GetRadius() * chainScale;
                delete entity;
                entity = new Circle(c, r);
                break;
            }
            case EntityType::Arc: {
                Arc* arc = static_cast<Arc*>(entity);
                geom::Vec3 c = ApplyTransformChain(arc->GetCenter(), transformChain);
                double r = arc->GetRadius() * chainScale;
                double sa = arc->GetStartAngle() + chainRotation * (180.0 / M_PI);
                double ea = arc->GetEndAngle() + chainRotation * (180.0 / M_PI);
                delete entity;
                entity = new Arc(c, r, sa, ea);
                break;
            }
            case EntityType::Polyline: {
                Polyline* poly = static_cast<Polyline*>(entity);
                const auto& verts = poly->GetVertices();
                std::vector<Polyline::Vertex> newVerts;
                newVerts.reserve(verts.size());
                for (const auto& v : verts) {
                    Polyline::Vertex nv;
                    nv.pos = ApplyTransformChain(v.pos, transformChain);
                    nv.bulge = v.bulge;
                    nv.width = v.width * chainScale;
                    newVerts.push_back(nv);
                }
                bool closed = poly->IsClosed();
                delete entity;
                entity = new Polyline(newVerts, closed);
                break;
            }
            default:
                break;
        }

        // Restore color and layer on the rebuilt entity
        // ByBlock entity'ler INSERT rengini alır; layer "0" entity'ler INSERT layer'ını miras alır
        if (entity) {
            Color resolvedColor = savedColor.IsByBlock() ? insertColor : savedColor;
            entity->SetColor(resolvedColor);
            // DXF/DWG standardı: blok içi layer "0" entity'ler INSERT'in layer'ını miras alır
            std::string finalLayer = (savedLayer == "0" || savedLayer.empty()) ? insertLayerName : savedLayer;
            entity->SetLayerName(finalLayer);
        }

        if (entity) {
            m_entities.push_back(std::unique_ptr<Entity>(entity));
            m_stats.insertExpanded++;
        }
    }

    // Pop this transform when leaving this INSERT level
    transformChain.pop_back();
}

// Load an external xref DWG and apply the transform chain to all its model-space entities
void DWGReader::ExpandXref(const std::string& xrefPath,
                            std::vector<InsertTransform>& transformChain, int depth) {
    if (depth > 6) return;

    // Resolve path: try absolute, then relative to current file's directory
    namespace fs = std::filesystem;
    std::string resolved;
    if (fs::path(xrefPath).is_absolute() && fs::exists(xrefPath)) {
        resolved = xrefPath;
    } else {
        fs::path base = fs::path(m_filePath).parent_path();
        fs::path candidate = base / fs::path(xrefPath).filename();
        if (fs::exists(candidate)) {
            resolved = candidate.string();
        } else {
            // .dwg uzantısı yoksa dene
            candidate.replace_extension(".dwg");
            if (fs::exists(candidate))
                resolved = candidate.string();
        }
    }

    // Kullanıcı tanımlı ek arama dizinleri
    if (resolved.empty()) {
        for (const auto& searchDir : m_xrefSearchPaths) {
            fs::path candidate = fs::path(searchDir) / fs::path(xrefPath).filename();
            if (fs::exists(candidate)) { resolved = candidate.string(); break; }
            candidate.replace_extension(".dwg");
            if (fs::exists(candidate)) { resolved = candidate.string(); break; }
        }
    }

    if (resolved.empty()) {
        std::cout << "[DWGReader] xref not found: " << xrefPath << std::endl;
        m_missingXrefs.push_back(xrefPath);
        return;
    }

    // Canonical path ile döngüsel referans tespiti
    std::string canonical;
    try { canonical = fs::canonical(resolved).string(); }
    catch (...) { canonical = resolved; }

    if (m_visitedXrefs.count(canonical)) {
        std::cout << "[DWGReader] xref circular ref skipped: " << canonical << std::endl;
        return;
    }
    m_visitedXrefs.insert(canonical);

    // Load the external file with a separate reader
    DWGReader sub;
    sub.m_filePath = resolved;
    sub.m_layerFilter = m_layerFilter;
    sub.m_visitedXrefs = m_visitedXrefs; // döngüsel ref setini devret
    if (!sub.Read(resolved)) {
        std::cout << "[DWGReader] xref load failed: " << resolved << std::endl;
        return;
    }

    // Take entities from sub-reader and apply transform chain
    auto subEnts = sub.TakeEntities();
    double chainScale = GetChainScale(transformChain);
    double chainRotation = GetChainRotation(transformChain);

    for (auto& ent : subEnts) {
        if (!ent) continue;
        Entity* e = ent.release();
        Color savedColor = e->GetColor();
        std::string savedLayer = e->GetLayerName();

        switch (e->GetType()) {
            case EntityType::Line: {
                Line* l = static_cast<Line*>(e);
                geom::Vec3 s = ApplyTransformChain(l->GetStart(), transformChain);
                geom::Vec3 en = ApplyTransformChain(l->GetEnd(), transformChain);
                delete e; e = new Line(s, en);
                break;
            }
            case EntityType::Circle: {
                Circle* c = static_cast<Circle*>(e);
                geom::Vec3 ctr = ApplyTransformChain(c->GetCenter(), transformChain);
                double r = c->GetRadius() * chainScale;
                delete e; e = new Circle(ctr, r);
                break;
            }
            case EntityType::Arc: {
                Arc* a = static_cast<Arc*>(e);
                geom::Vec3 ctr = ApplyTransformChain(a->GetCenter(), transformChain);
                double r = a->GetRadius() * chainScale;
                double sa = a->GetStartAngle() + chainRotation * (180.0 / M_PI);
                double ea = a->GetEndAngle() + chainRotation * (180.0 / M_PI);
                delete e; e = new Arc(ctr, r, sa, ea);
                break;
            }
            case EntityType::Polyline: {
                Polyline* p = static_cast<Polyline*>(e);
                auto verts = p->GetVertices();
                for (auto& v : verts) {
                    v.pos = ApplyTransformChain(v.pos, transformChain);
                    v.width *= chainScale;
                }
                bool cl = p->IsClosed();
                delete e; e = new Polyline(verts, cl);
                break;
            }
            default: break;
        }

        if (e) {
            e->SetColor(savedColor);
            e->SetLayerName(savedLayer);
            m_entities.push_back(std::unique_ptr<Entity>(e));
            m_stats.insertExpanded++;
        }
    }

    // Merge layers from xref
    for (auto& [name, layer] : sub.m_layers) {
        if (m_layers.find(name) == m_layers.end())
            m_layers[name] = layer;
    }

    std::cout << "[DWGReader] xref expanded: " << resolved
              << " → " << subEnts.size() << " entities" << std::endl;
}

bool DWGReader::ParseEntities(void* dwg_ptr) {
    Dwg_Data* dwg = static_cast<Dwg_Data*>(dwg_ptr);

    // Phase 1: Find model space block header handle
    unsigned long modelSpaceHandle = 0;
    for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
        Dwg_Object* obj = &dwg->object[i];
        if (!obj || obj->type != DWG_TYPE_BLOCK_HEADER) continue;
        if (!obj->tio.object || !obj->tio.object->tio.BLOCK_HEADER) continue;

        Dwg_Object_BLOCK_HEADER* bh = obj->tio.object->tio.BLOCK_HEADER;
        if (bh->name && (strcmp(bh->name, "*Model_Space") == 0 ||
                          strcmp(bh->name, "*MODEL_SPACE") == 0)) {
            modelSpaceHandle = obj->handle.value;
            std::cout << "[DWGReader] Model space handle: " << modelSpaceHandle << std::endl;
            break;
        }
    }

    if (modelSpaceHandle == 0) {
        std::cout << "[DWGReader] WARNING: Model space not found, parsing all entities" << std::endl;
    }

    // Phase 2: Parse only model space entities
    int insertCount = 0, directCount = 0, skippedCount = 0;

    for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
        Dwg_Object* obj = &dwg->object[i];
        if (!obj || obj->supertype != DWG_SUPERTYPE_ENTITY) continue;
        if (!obj->tio.entity) continue;

        // Filter: only model space entities
        if (modelSpaceHandle != 0) {
            unsigned long ownerHandle = obj->tio.entity->ownerhandle ?
                                         obj->tio.entity->ownerhandle->absolute_ref : 0;
            if (ownerHandle != modelSpaceHandle) {
                skippedCount++;
                continue;
            }
        }

        if (obj->type == DWG_TYPE_INSERT) {
            // INSERT: blok referansını world koordinatlarına aç
            std::vector<InsertTransform> chain;
            ExpandInsert(obj, dwg, chain, 0);
            insertCount++;
        } else if (obj->type == DWG_TYPE_MINSERT) {
            // MINSERT: dizi INSERT — satır×sütun grid olarak aç
            ExpandMInsert(obj, dwg);
            insertCount++;
        } else {
            Entity* entity = ParseEntityByType(obj);
            if (entity) {
                m_entities.push_back(std::unique_ptr<Entity>(entity));
                directCount++;
            }
        }
    }

    std::cout << "[DWGReader] Model space: " << directCount << " direct, "
              << insertCount << " INSERTs expanded (" << m_stats.insertExpanded << " entities), "
              << skippedCount << " block-definition entities skipped" << std::endl;

    return true;
}

Entity* DWGReader::ParseLine(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.LINE) return nullptr;
    
    Dwg_Entity_LINE* line = obj->tio.entity->tio.LINE;
    
    geom::Vec3 start(line->start.x, line->start.y, 0.0);
    geom::Vec3 end(line->end.x, line->end.y, 0.0);
    
    return new Line(start, end);
}

/**
 * @brief Klasik POLYLINE_2D/3D entity parse
 *
 * LWPOLYLINE'dan farklı olarak vertex'ler ayrı sub-entity olarak saklanır.
 * vertex[] handle dizisi üzerinden iterate ederek Polyline oluşturur.
 */
Entity* DWGReader::ParsePolyline(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity) return nullptr;

    Dwg_Data* dwg = static_cast<Dwg_Data*>(m_dwgData);
    if (!dwg) return nullptr;

    bool is3D = (obj->type == DWG_TYPE_POLYLINE_3D);
    BITCODE_BL num_owned = 0;
    BITCODE_H* vertex_handles = nullptr;
    BITCODE_BS flag = 0;

    if (is3D) {
        if (!obj->tio.entity->tio.POLYLINE_3D) return nullptr;
        Dwg_Entity_POLYLINE_3D* poly3d = obj->tio.entity->tio.POLYLINE_3D;
        num_owned = poly3d->num_owned;
        vertex_handles = poly3d->vertex;
        flag = poly3d->flag;
    } else {
        if (!obj->tio.entity->tio.POLYLINE_2D) return nullptr;
        Dwg_Entity_POLYLINE_2D* poly2d = obj->tio.entity->tio.POLYLINE_2D;
        num_owned = poly2d->num_owned;
        vertex_handles = poly2d->vertex;
        flag = poly2d->flag;
    }

    if (num_owned < 2 || !vertex_handles) return nullptr;

    std::vector<Polyline::Vertex> vertices;
    vertices.reserve(num_owned);

    for (BITCODE_BL i = 0; i < num_owned; ++i) {
        if (!vertex_handles[i] || !vertex_handles[i]->obj) continue;
        Dwg_Object* vObj = vertex_handles[i]->obj;

        // VERTEX_2D ve VERTEX_3D aynı ilk alanları paylaşır
        if (vObj->type == DWG_TYPE_VERTEX_2D) {
            if (!vObj->tio.entity || !vObj->tio.entity->tio.VERTEX_2D) continue;
            Dwg_Entity_VERTEX_2D* v = vObj->tio.entity->tio.VERTEX_2D;
            Polyline::Vertex pv;
            pv.pos = geom::Vec3(v->point.x, v->point.y, 0.0);
            pv.bulge = v->bulge;
            pv.width = v->start_width;
            vertices.push_back(pv);
        } else if (vObj->type == DWG_TYPE_VERTEX_3D ||
                   vObj->type == 0x0c /* VERTEX_MESH */ ||
                   vObj->type == 0x0d /* VERTEX_PFACE */) {
            if (!vObj->tio.entity || !vObj->tio.entity->tio.VERTEX_3D) continue;
            Dwg_Entity_VERTEX_3D* v = vObj->tio.entity->tio.VERTEX_3D;
            Polyline::Vertex pv;
            pv.pos = geom::Vec3(v->point.x, v->point.y, 0.0);
            vertices.push_back(pv);
        }
    }

    if (vertices.size() < 2) return nullptr;

    bool closed = (flag & 0x01) != 0;
    return new Polyline(vertices, closed);
}

Entity* DWGReader::ParseLWPolyline(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.LWPOLYLINE) return nullptr;
    
    Dwg_Entity_LWPOLYLINE* lwpoly = obj->tio.entity->tio.LWPOLYLINE;
    
    // Skip if not enough points (2 is valid — a line segment)
    if (lwpoly->num_points < 2) return nullptr;
    
    // Create polyline with vertices
    std::vector<Polyline::Vertex> vertices;
    vertices.reserve(lwpoly->num_points);
    
    for (BITCODE_BL i = 0; i < lwpoly->num_points; ++i) {
        Polyline::Vertex vertex;
        vertex.pos = geom::Vec3(lwpoly->points[i].x, lwpoly->points[i].y, 0.0);
        
        // Check if bulges array exists
        if (lwpoly->bulges && i < lwpoly->num_bulges) {
            vertex.bulge = lwpoly->bulges[i];
        }
        
        // Check if widths array exists
        if (lwpoly->widths && i < lwpoly->num_widths) {
            vertex.width = lwpoly->widths[i].start;
        }
        
        vertices.push_back(vertex);
    }
    
    // Check if polyline is closed
    // Method 1: Check flag bit 1 (0x01 = closed flag)
    bool closed = (lwpoly->flag & 0x01) != 0;
    
    // Method 2: If flag not set, check if first == last point (tolerance 1e-6)
    if (!closed && lwpoly->num_points >= 3) {
        const auto& first = lwpoly->points[0];
        const auto& last = lwpoly->points[lwpoly->num_points - 1];
        double dx = first.x - last.x;
        double dy = first.y - last.y;
        double dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 1e-3) { // 1mm tolerance
            closed = true;
        }
    }
    
    Polyline* polyline = new Polyline(vertices, closed);
    return polyline;
}

Entity* DWGReader::ParseArc(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.ARC) return nullptr;

    Dwg_Entity_ARC* arc = obj->tio.entity->tio.ARC;

    geom::Vec3 center(arc->center.x, arc->center.y, 0.0);
    double radius = arc->radius;
    double startAngle = arc->start_angle * (180.0 / M_PI);
    double endAngle = arc->end_angle * (180.0 / M_PI);

    return new Arc(center, radius, startAngle, endAngle);
}

Entity* DWGReader::ParseCircle(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.CIRCLE) return nullptr;
    
    Dwg_Entity_CIRCLE* circle = obj->tio.entity->tio.CIRCLE;
    
    geom::Vec3 center(circle->center.x, circle->center.y, 0.0);
    double radius = circle->radius;
    
    return new Circle(center, radius);
}

Entity* DWGReader::ParseText(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.TEXT) return nullptr;

    Dwg_Entity_TEXT* txt = obj->tio.entity->tio.TEXT;
    if (!txt->text_value || txt->text_value[0] == '\0') return nullptr;

    geom::Vec3 insertPt(txt->ins_pt.x, txt->ins_pt.y, 0.0);
    double height   = (txt->height > 0.0) ? txt->height : 2.5;
    double rotation = txt->rotation * (180.0 / M_PI); // rad → deg

    return new Text(insertPt, std::string(txt->text_value), height, rotation);
}

// MTEXT format kodlarını temizle: {\fFont;}, \P, {\H2.5;} → düz metin
static std::string StripMTextFormatting(const char* raw) {
    if (!raw) return "";
    std::string out;
    out.reserve(std::strlen(raw));
    const char* p = raw;
    while (*p) {
        if (*p == '\\' && *(p + 1)) {
            char next = *(p + 1);
            if (next == 'P' || next == 'p') { out += '\n'; p += 2; continue; }
            if (next == '~')               { out += ' ';  p += 2; continue; }
            if (next == '\\')              { out += '\\'; p += 2; continue; }
            if (next == '{' || next == '}') { out += next; p += 2; continue; }
            // Kontrol kelimesi: \X... ; şeklinde — `;` ye kadar atla
            p += 2;
            while (*p && *p != ';' && *p != ' ') p++;
            if (*p == ';') p++;
            continue;
        }
        if (*p == '{' || *p == '}') { p++; continue; }
        out += *p++;
    }
    // Trim
    size_t f = out.find_first_not_of(" \t\r\n");
    if (f == std::string::npos) return "";
    size_t l = out.find_last_not_of(" \t\r\n");
    return out.substr(f, l - f + 1);
}

Entity* DWGReader::ParseMText(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.MTEXT) return nullptr;

    Dwg_Entity_MTEXT* mtext = obj->tio.entity->tio.MTEXT;
    if (!mtext->text || mtext->text[0] == '\0') return nullptr;

    geom::Vec3 insertPt(mtext->ins_pt.x, mtext->ins_pt.y, 0.0);
    double height = (mtext->text_height > 0.0) ? mtext->text_height : 2.5;

    // Rotation from x_axis_dir vector
    double rotDeg = 0.0;
    if (mtext->x_axis_dir.x != 0.0 || mtext->x_axis_dir.y != 0.0)
        rotDeg = std::atan2(mtext->x_axis_dir.y, mtext->x_axis_dir.x) * 180.0 / M_PI;

    std::string clean = StripMTextFormatting(mtext->text);
    if (clean.empty()) return nullptr;

    auto* txt = new Text(insertPt, clean, height, rotDeg);

    // attachment → hAlign/vAlign (same table as DXF code 71)
    static const int hTab[] = {0, 0,1,2, 0,1,2, 0,1,2};
    static const int vTab[] = {0, 3,3,3, 2,2,2, 1,1,1};
    int att = static_cast<int>(mtext->attachment);
    if (att >= 1 && att <= 9) {
        txt->SetHAlign(hTab[att]);
        txt->SetVAlign(vTab[att]);
    }

    if (mtext->rect_width > 0.0) txt->SetRectWidth(mtext->rect_width);

    return txt;
}

// HATCH → Hatch entity (SOLID pattern: fan-fill; diğerleri: sınır çizgisi)
Entity* DWGReader::ParseHatch(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.HATCH) return nullptr;

    Dwg_Entity_HATCH* hatch = obj->tio.entity->tio.HATCH;
    if (!hatch->num_paths || !hatch->paths) return nullptr;

    // Pattern ismi
    std::string patternName = (hatch->name && hatch->name[0]) ? hatch->name : "SOLID";

    // Dışa açık olmayan ilk path'i dış sınır kabul et
    Dwg_HATCH_Path* outerPath = nullptr;
    for (BITCODE_BL pi = 0; pi < hatch->num_paths; ++pi) {
        if (!(hatch->paths[pi].flag & 0x20)) {
            outerPath = &hatch->paths[pi];
            break;
        }
    }
    if (!outerPath) outerPath = &hatch->paths[0];

    std::vector<Hatch::BoundaryVertex> vertices;

    auto addPt = [&](double x, double y, double bulge = 0.0) {
        Hatch::BoundaryVertex v;
        v.pos = geom::Vec3(x, y, 0.0);
        v.bulge = bulge;
        vertices.push_back(v);
    };

    if (outerPath->flag & 0x02) {
        if (!outerPath->polyline_paths) return nullptr;
        for (BITCODE_BL si = 0; si < outerPath->num_segs_or_paths; ++si) {
            const auto& pp = outerPath->polyline_paths[si];
            addPt(pp.point.x, pp.point.y, pp.bulge);
        }
    } else {
        if (!outerPath->segs) return nullptr;
        for (BITCODE_BL si = 0; si < outerPath->num_segs_or_paths; ++si) {
            const Dwg_HATCH_PathSeg& seg = outerPath->segs[si];
            switch (seg.curve_type) {
                case 1: // LINE
                    addPt(seg.first_endpoint.x, seg.first_endpoint.y);
                    break;
                case 2: { // CIRCULAR ARC
                    double sweep = seg.end_angle - seg.start_angle;
                    if (!seg.is_ccw) sweep = -sweep;
                    if (sweep <= 0) sweep += 2.0 * M_PI;
                    int steps = std::max(8, (int)(sweep / (M_PI / 8)));
                    for (int k = 0; k < steps; ++k) {
                        double t = seg.start_angle + sweep * k / steps;
                        if (!seg.is_ccw) t = seg.start_angle - sweep * k / steps;
                        addPt(seg.center.x + seg.radius * std::cos(t),
                              seg.center.y + seg.radius * std::sin(t));
                    }
                    break;
                }
                case 3: { // ELLIPTIC ARC
                    double majorLen = std::sqrt(seg.endpoint.x*seg.endpoint.x +
                                                seg.endpoint.y*seg.endpoint.y);
                    if (majorLen < 1e-12) break;
                    double minorLen = majorLen * seg.minor_major_ratio;
                    double rotA = std::atan2(seg.endpoint.y, seg.endpoint.x);
                    double sweep = seg.end_angle - seg.start_angle;
                    if (sweep <= 0) sweep += 2.0 * M_PI;
                    for (int k = 0; k < 16; ++k) {
                        double t  = seg.start_angle + sweep * k / 16;
                        double lx = majorLen * std::cos(t);
                        double ly = minorLen * std::sin(t);
                        addPt(seg.center.x + lx*std::cos(rotA) - ly*std::sin(rotA),
                              seg.center.y + lx*std::sin(rotA) + ly*std::cos(rotA));
                    }
                    break;
                }
                case 4: { // SPLINE
                    if (seg.num_fitpts > 0 && seg.fitpts) {
                        for (BITCODE_BL fi = 0; fi < seg.num_fitpts; ++fi)
                            addPt(seg.fitpts[fi].x, seg.fitpts[fi].y);
                    } else if (seg.num_control_points > 0 && seg.control_points) {
                        for (BITCODE_BL ci = 0; ci < seg.num_control_points; ++ci)
                            addPt(seg.control_points[ci].point.x, seg.control_points[ci].point.y);
                    }
                    break;
                }
                default: break;
            }
        }
    }

    if (vertices.size() < 3) return nullptr;
    auto* h = new Hatch();
    h->SetPatternName(patternName);
    h->SetBoundary(std::move(vertices));
    h->SetPatternAngle(hatch->angle * 180.0 / M_PI); // radians → degrees
    h->SetPatternScale(hatch->scale_spacing > 0 ? hatch->scale_spacing : 1.0);
    return h;
}

/**
 * @brief ELLIPSE entity parse — Ellipse entity olarak sakla (tessellation render'da yapılır)
 *
 * LibreDWG: center, sm_axis (yarı-büyük eksen vektörü), axis_ratio, start/end_angle
 */
Entity* DWGReader::ParseEllipse(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.ELLIPSE) return nullptr;

    Dwg_Entity_ELLIPSE* ell = obj->tio.entity->tio.ELLIPSE;

    geom::Vec3 center(ell->center.x, ell->center.y, ell->center.z);
    double semiMajor = std::sqrt(ell->sm_axis.x * ell->sm_axis.x +
                                  ell->sm_axis.y * ell->sm_axis.y);
    if (semiMajor < 1e-12) return nullptr;

    double axisRatio = ell->axis_ratio;
    double rotAngle  = std::atan2(ell->sm_axis.y, ell->sm_axis.x);

    double startParam = ell->start_angle;
    double endParam   = ell->end_angle;

    // Tam ellips: start == end → normalize to [0, 2π]
    constexpr double twoPi = 6.283185307179586;
    bool full = (std::abs(endParam - startParam) < 1e-10) ||
                (std::abs(endParam - startParam - twoPi) < 1e-10);
    if (full) { startParam = 0.0; endParam = twoPi; }

    return new Ellipse(center, semiMajor, axisRatio, rotAngle, startParam, endParam);
}

/**
 * @brief SPLINE entity parse — Polyline olarak tessellate eder
 *
 * Fit noktaları varsa doğrudan kullanılır, yoksa kontrol noktaları ve
 * knot vektörü ile De Boor algoritması uygulanır.
 */
Entity* DWGReader::ParseSpline(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.SPLINE) return nullptr;

    Dwg_Entity_SPLINE* spl = obj->tio.entity->tio.SPLINE;

    std::vector<Polyline::Vertex> vertices;

    // Yöntem 1: Fit noktaları varsa doğrudan kullan
    if (spl->num_fit_pts > 1 && spl->fit_pts) {
        vertices.reserve(spl->num_fit_pts);
        for (BITCODE_BS i = 0; i < spl->num_fit_pts; ++i) {
            Polyline::Vertex pv;
            pv.pos = geom::Vec3(spl->fit_pts[i].x, spl->fit_pts[i].y, 0.0);
            vertices.push_back(pv);
        }
    }
    // Yöntem 2: Kontrol noktaları ile De Boor B-spline evaluation
    else if (spl->num_ctrl_pts > 1 && spl->ctrl_pts && spl->num_knots > 0 && spl->knots) {
        int n = spl->num_ctrl_pts;
        int p = spl->degree;
        int numKnots = spl->num_knots;

        // Parametrik aralık: [knots[p], knots[n]]
        double tStart = spl->knots[p];
        double tEnd = spl->knots[n]; // knots[num_ctrl_pts]
        if (tEnd <= tStart) return nullptr;

        int segments = std::max(32, n * 8);
        vertices.reserve(segments + 1);

        for (int i = 0; i <= segments; ++i) {
            double t = tStart + (tEnd - tStart) * i / segments;

            // De Boor algoritması
            // Knot aralığını bul: t ∈ [knots[k], knots[k+1])
            int k = p;
            for (int j = p; j < numKnots - 1; ++j) {
                if (t >= spl->knots[j] && t < spl->knots[j + 1]) { k = j; break; }
            }
            if (i == segments) k = n - 1; // son nokta

            // Kontrol noktalarını kopyala (De Boor geçici dizi)
            std::vector<double> dx(p + 1), dy(p + 1);
            for (int j = 0; j <= p; ++j) {
                int idx = k - p + j;
                if (idx < 0) idx = 0;
                if (idx >= n) idx = n - 1;
                dx[j] = spl->ctrl_pts[idx].x;
                dy[j] = spl->ctrl_pts[idx].y;
            }

            // Triangular computation
            for (int r = 1; r <= p; ++r) {
                for (int j = p; j >= r; --j) {
                    int knotIdx = k - p + j;
                    if (knotIdx < 0) knotIdx = 0;
                    int knotIdx2 = knotIdx + p - r + 1;
                    if (knotIdx2 >= numKnots) knotIdx2 = numKnots - 1;

                    double denom = spl->knots[knotIdx2] - spl->knots[knotIdx];
                    double alpha = (denom > 1e-12) ?
                        (t - spl->knots[knotIdx]) / denom : 0.0;

                    dx[j] = (1.0 - alpha) * dx[j - 1] + alpha * dx[j];
                    dy[j] = (1.0 - alpha) * dy[j - 1] + alpha * dy[j];
                }
            }

            Polyline::Vertex pv;
            pv.pos = geom::Vec3(dx[p], dy[p], 0.0);
            vertices.push_back(pv);
        }
    }

    if (vertices.size() < 2) return nullptr;

    bool closed = spl->closed_b != 0;
    return new Polyline(vertices, closed);
}

/**
 * @brief POINT entity parse — küçük çapraz (×) olarak 2 Line oluşturur
 */
Entity* DWGReader::ParsePoint(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.POINT) return nullptr;

    Dwg_Entity_POINT* pt = obj->tio.entity->tio.POINT;
    double armLength = 25.0; // 25mm kol

    geom::Vec3 center(pt->x, pt->y, 0.0);

    // Çapraz: iki çizgi. İlk çizgiyi döndür, ikincisini m_entities'e ekle
    auto* line1 = new Line(
        geom::Vec3(center.x - armLength, center.y, 0.0),
        geom::Vec3(center.x + armLength, center.y, 0.0)
    );
    auto* line2 = new Line(
        geom::Vec3(center.x, center.y - armLength, 0.0),
        geom::Vec3(center.x, center.y + armLength, 0.0)
    );

    // İkinci çizgiyi doğrudan entities'e ekle
    m_entities.push_back(std::unique_ptr<Entity>(line2));
    return line1; // İlk çizgi ParseEntityByType tarafından eklenir
}

/**
 * @brief MINSERT (dizi INSERT) genişletme
 *
 * INSERT parametreleri + num_rows × num_cols grid olarak genişletir.
 * Her grid hücresi için ayrı ExpandInsert çağrısı yapılır.
 */
void DWGReader::ExpandMInsert(void* obj_ptr, void* dwg_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    Dwg_Data* dwg = static_cast<Dwg_Data*>(dwg_ptr);

    if (!obj->tio.entity || !obj->tio.entity->tio.MINSERT) return;
    Dwg_Entity_MINSERT* mins = obj->tio.entity->tio.MINSERT;

    int numRows = std::max((int)mins->num_rows, 1);
    int numCols = std::max((int)mins->num_cols, 1);
    double rowSpacing = mins->row_spacing;
    double colSpacing = mins->col_spacing;

    double cosR = std::cos(mins->rotation);
    double sinR = std::sin(mins->rotation);

    // MINSERT'in kendi layer ismi — layer "0" child entity'leri bunu miras alır
    std::string minsLayerName = "0";
    {
        if (obj->tio.entity->layer) {
            Dwg_Object* layerObj = obj->tio.entity->layer->obj;
            if (layerObj && layerObj->tio.object && layerObj->tio.object->tio.LAYER) {
                const char* p = layerObj->tio.object->tio.LAYER->name;
                if (p) minsLayerName = Utf16LeToUtf8(p);
            }
        }
    }

    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numCols; ++col) {
            // Grid offset hesapla (MINSERT rotasyonuna göre döndür)
            double dx = col * colSpacing;
            double dy = row * rowSpacing;
            double worldDx = dx * cosR - dy * sinR;
            double worldDy = dx * sinR + dy * cosR;

            // Geçici INSERT nesnesi simüle et: InsertTransform oluştur
            InsertTransform xform;
            xform.insPoint = geom::Vec3(
                mins->ins_pt.x + worldDx,
                mins->ins_pt.y + worldDy, 0.0);
            xform.xScale = mins->scale.x;
            xform.yScale = mins->scale.y;
            xform.rotation = mins->rotation;

            if (std::abs(xform.xScale) < 1e-12) xform.xScale = 1.0;
            if (std::abs(xform.yScale) < 1e-12) xform.yScale = 1.0;

            // Block header çözümle
            if (!mins->block_header || !mins->block_header->obj) return;
            Dwg_Object* blockObj = mins->block_header->obj;
            if (blockObj->type != DWG_TYPE_BLOCK_HEADER) return;
            if (!blockObj->tio.object || !blockObj->tio.object->tio.BLOCK_HEADER) return;

            Dwg_Object_BLOCK_HEADER* bh = blockObj->tio.object->tio.BLOCK_HEADER;
            xform.basePt = geom::Vec3(bh->base_pt.x, bh->base_pt.y, 0.0);

            unsigned long blockHandle = blockObj->handle.value;

            // Transform chain başlat ve blok entity'lerini genişlet
            std::vector<InsertTransform> chain;
            chain.push_back(xform);

            for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
                Dwg_Object* child = &dwg->object[i];
                if (!child || child->supertype != DWG_SUPERTYPE_ENTITY) continue;
                if (!child->tio.entity) continue;

                unsigned long ownerHandle = child->tio.entity->ownerhandle ?
                                             child->tio.entity->ownerhandle->absolute_ref : 0;
                if (ownerHandle != blockHandle) continue;

                if (child->type == DWG_TYPE_INSERT) {
                    ExpandInsert(child, dwg, chain, 1);
                    continue;
                }

                Entity* entity = ParseEntityByType(child);
                if (!entity) continue;

                Color       savedColor = entity->GetColor();
                std::string savedLayer = entity->GetLayerName();

                // Transform zinciri uygula
                double chainScale = GetChainScale(chain);
                double chainRotation = GetChainRotation(chain);

                switch (entity->GetType()) {
                    case EntityType::Line: {
                        Line* line = static_cast<Line*>(entity);
                        geom::Vec3 s = ApplyTransformChain(line->GetStart(), chain);
                        geom::Vec3 e = ApplyTransformChain(line->GetEnd(), chain);
                        delete entity;
                        entity = new Line(s, e);
                        break;
                    }
                    case EntityType::Circle: {
                        Circle* circle = static_cast<Circle*>(entity);
                        geom::Vec3 c = ApplyTransformChain(circle->GetCenter(), chain);
                        double r = circle->GetRadius() * chainScale;
                        delete entity;
                        entity = new Circle(c, r);
                        break;
                    }
                    case EntityType::Arc: {
                        Arc* arc = static_cast<Arc*>(entity);
                        geom::Vec3 c = ApplyTransformChain(arc->GetCenter(), chain);
                        double r = arc->GetRadius() * chainScale;
                        double sa = arc->GetStartAngle() + chainRotation * (180.0 / M_PI);
                        double ea = arc->GetEndAngle() + chainRotation * (180.0 / M_PI);
                        delete entity;
                        entity = new Arc(c, r, sa, ea);
                        break;
                    }
                    case EntityType::Polyline: {
                        Polyline* poly = static_cast<Polyline*>(entity);
                        const auto& verts = poly->GetVertices();
                        std::vector<Polyline::Vertex> newVerts;
                        newVerts.reserve(verts.size());
                        for (const auto& v : verts) {
                            Polyline::Vertex nv;
                            nv.pos = ApplyTransformChain(v.pos, chain);
                            nv.bulge = v.bulge;
                            nv.width = v.width * chainScale;
                            newVerts.push_back(nv);
                        }
                        bool cl = poly->IsClosed();
                        delete entity;
                        entity = new Polyline(newVerts, cl);
                        break;
                    }
                    default:
                        break;
                }

                if (entity) {
                    // ByBlock → MINSERT color (ByBlock rengi desteklenmiyorsa ByLayer kalır)
                    // Layer "0" → MINSERT layer miras al
                    entity->SetColor(savedColor); // ByLayer veya explicit renk
                    std::string finalLayer = (savedLayer == "0" || savedLayer.empty()) ? minsLayerName : savedLayer;
                    entity->SetLayerName(finalLayer);
                    m_entities.push_back(std::unique_ptr<Entity>(entity));
                    m_stats.insertExpanded++;
                }
            }
        }
    }
}

void DWGReader::ExpandMInsertNested(void* obj_ptr, void* dwg_ptr,
                                    void* parentChain_ptr, int depth) {
    if (depth > 8) return;

    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    Dwg_Data*   dwg = static_cast<Dwg_Data*>(dwg_ptr);
    auto* parentChain = static_cast<std::vector<InsertTransform>*>(parentChain_ptr);

    if (!obj->tio.entity || !obj->tio.entity->tio.MINSERT) return;
    Dwg_Entity_MINSERT* mins = obj->tio.entity->tio.MINSERT;

    int    numRows    = std::max((int)mins->num_rows, 1);
    int    numCols    = std::max((int)mins->num_cols, 1);
    double rowSpacing = mins->row_spacing;
    double colSpacing = mins->col_spacing;
    double cosR       = std::cos(mins->rotation);
    double sinR       = std::sin(mins->rotation);

    if (!mins->block_header || !mins->block_header->obj) return;
    Dwg_Object* blockObj = mins->block_header->obj;
    if (blockObj->type != DWG_TYPE_BLOCK_HEADER) return;
    if (!blockObj->tio.object || !blockObj->tio.object->tio.BLOCK_HEADER) return;
    Dwg_Object_BLOCK_HEADER* bh = blockObj->tio.object->tio.BLOCK_HEADER;
    unsigned long blockHandle = blockObj->handle.value;

    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numCols; ++col) {
            double dx      = col * colSpacing;
            double dy      = row * rowSpacing;
            double worldDx = dx * cosR - dy * sinR;
            double worldDy = dx * sinR + dy * cosR;

            InsertTransform xform;
            xform.insPoint = geom::Vec3(mins->ins_pt.x + worldDx,
                                        mins->ins_pt.y + worldDy, 0.0);
            xform.xScale   = mins->scale.x;
            xform.yScale   = mins->scale.y;
            xform.rotation = mins->rotation;
            xform.basePt   = geom::Vec3(bh->base_pt.x, bh->base_pt.y, 0.0);
            if (std::abs(xform.xScale) < 1e-12) xform.xScale = 1.0;
            if (std::abs(xform.yScale) < 1e-12) xform.yScale = 1.0;

            // Build chain: parent chain + this cell's transform
            std::vector<InsertTransform> chain = *parentChain;
            chain.push_back(xform);

            for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
                Dwg_Object* child = &dwg->object[i];
                if (!child || child->supertype != DWG_SUPERTYPE_ENTITY) continue;
                if (!child->tio.entity) continue;
                unsigned long ownerHandle = child->tio.entity->ownerhandle ?
                                             child->tio.entity->ownerhandle->absolute_ref : 0;
                if (ownerHandle != blockHandle) continue;

                if (child->type == DWG_TYPE_INSERT) {
                    ExpandInsert(child, dwg, chain, depth + 1);
                    continue;
                }
                if (child->type == DWG_TYPE_MINSERT) {
                    ExpandMInsertNested(child, dwg, &chain, depth + 1);
                    continue;
                }

                Entity* entity = ParseEntityByType(child);
                if (!entity) continue;

                Color       savedColor = entity->GetColor();
                std::string savedLayer = entity->GetLayerName();

                double chainScale    = GetChainScale(chain);
                double chainRotation = GetChainRotation(chain);

                switch (entity->GetType()) {
                    case EntityType::Line: {
                        Line* ln = static_cast<Line*>(entity);
                        geom::Vec3 s = ApplyTransformChain(ln->GetStart(), chain);
                        geom::Vec3 e = ApplyTransformChain(ln->GetEnd(), chain);
                        delete entity; entity = new Line(s, e);
                        break;
                    }
                    case EntityType::Circle: {
                        Circle* ci = static_cast<Circle*>(entity);
                        geom::Vec3 c = ApplyTransformChain(ci->GetCenter(), chain);
                        delete entity; entity = new Circle(c, ci->GetRadius() * chainScale);
                        break;
                    }
                    case EntityType::Arc: {
                        Arc* ar = static_cast<Arc*>(entity);
                        geom::Vec3 c = ApplyTransformChain(ar->GetCenter(), chain);
                        double sa = ar->GetStartAngle() + chainRotation * (180.0 / M_PI);
                        double ea = ar->GetEndAngle()   + chainRotation * (180.0 / M_PI);
                        delete entity; entity = new Arc(c, ar->GetRadius() * chainScale, sa, ea);
                        break;
                    }
                    case EntityType::Polyline: {
                        Polyline* po = static_cast<Polyline*>(entity);
                        std::vector<Polyline::Vertex> nv;
                        for (const auto& v : po->GetVertices()) {
                            nv.push_back({ApplyTransformChain(v.pos, chain),
                                          v.bulge, v.width * chainScale});
                        }
                        bool cl = po->IsClosed();
                        delete entity; entity = new Polyline(nv, cl);
                        break;
                    }
                    default: break;
                }

                if (entity) {
                    entity->SetColor(savedColor);
                    entity->SetLayerName(savedLayer);
                    m_entities.push_back(std::unique_ptr<Entity>(entity));
                    m_stats.insertExpanded++;
                }
            }
        }
    }
}

void DWGReader::SetLayerFilter(const std::unordered_set<std::string>& layers) {
    m_layerFilter = layers;
}

bool DWGReader::PassesLayerFilter(const std::string& layerName) const {
    if (m_layerFilter.empty()) return true;
    return m_layerFilter.find(layerName) != m_layerFilter.end();
}

/**
 * @brief DWG dosyasından layer bilgilerini çıkarır
 *
 * DWG_TYPE_LAYER objelerini iterate eder, her layer için:
 * isim, ACI renk, frozen/on/locked durumları okunur.
 */
void DWGReader::ExtractLayers(void* dwg_ptr) {
    Dwg_Data* dwg = static_cast<Dwg_Data*>(dwg_ptr);
    if (!dwg) return;

    for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
        Dwg_Object* obj = &dwg->object[i];
        if (!obj || obj->type != DWG_TYPE_LAYER) continue;
        if (!obj->tio.object || !obj->tio.object->tio.LAYER) continue;

        Dwg_Object_LAYER* lay = obj->tio.object->tio.LAYER;
        if (!lay->name) continue;

        // R2007+ DWG: name alanı UTF-16LE char* olarak saklanır.
        // std::string(char*) ilk null byte'ta durur → tek karakter alınır.
        // Düzeltme: her iki byte'tan birini al (UTF-16LE → Latin/ASCII).
        std::string name;
        {
            const char* p = lay->name;
            // İlk iki byte kontrol: UTF-16 ise ikinci byte 0x00 olur
            bool isUtf16 = (p[0] != '\0' && p[1] == '\0');
            if (isUtf16) {
                // UTF-16LE → UTF-8 basit dönüşüm (BMP karakterler için)
                const uint16_t* wp = reinterpret_cast<const uint16_t*>(p);
                while (*wp) {
                    uint16_t ch = *wp++;
                    if (ch < 0x80) {
                        name += static_cast<char>(ch);
                    } else if (ch < 0x800) {
                        name += static_cast<char>(0xC0 | (ch >> 6));
                        name += static_cast<char>(0x80 | (ch & 0x3F));
                    } else {
                        name += static_cast<char>(0xE0 | (ch >> 12));
                        name += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        name += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                }
            } else {
                name = p; // zaten UTF-8 / ASCII
            }
        }
        if (name.empty()) continue;

        Layer layer(name, Color::White());

        // Renk çözümleme
        // DWG method byte: 0xC2=truecolor RGB, 0xC3=ACAD color number (ACI idx in low byte)
        const Dwg_Color& lc = lay->color;
        bool colorSet = false;
        if (lc.method == 0xC2 && lc.rgb != 0) {
            // True RGB (24-bit): 0x00RRGGBB
            uint8_t cr = (lc.rgb >> 16) & 0xFF;
            uint8_t cg = (lc.rgb >> 8)  & 0xFF;
            uint8_t cb =  lc.rgb        & 0xFF;
            layer.SetColor(Color(cr, cg, cb));
            colorSet = true;
        } else if (lc.method == 0xC3) {
            // ACAD Color Number: ACI index in low byte of rgb
            int aci = static_cast<int>(lc.rgb & 0xFF);
            if (aci >= 1 && aci <= 255) {
                layer.SetColorIndex(aci);
                colorSet = true;
            }
        }
        if (!colorSet) {
            // Standard ACI via index field (negatif = layer kapalı)
            int colorIndex = lc.index;
            if (colorIndex < 0) {
                layer.SetVisible(false);
                colorIndex = -colorIndex;
            }
            if (colorIndex >= 1 && colorIndex <= 255) {
                layer.SetColorIndex(colorIndex);
            }
        }

        // Layer durumları
        layer.SetFrozen(lay->frozen != 0);
        layer.SetLocked(lay->locked != 0);
        if (!lay->on) layer.SetVisible(false);

        m_layers[name] = layer;
    }

    m_stats.layerCount = m_layers.size();
    std::cout << "[DWGReader] " << m_stats.layerCount << " layer extracted" << std::endl;
}

std::vector<std::unique_ptr<Entity>> DWGReader::TakeEntities() {
    std::cout << "[DWGReader::TakeEntities] Moving " << m_entities.size() << " entities" << std::endl;
    return std::move(m_entities);
}

std::vector<Entity*> DWGReader::GetEntities() const {
    std::vector<Entity*> result;
    result.reserve(m_entities.size());
    for (const auto& entity : m_entities) {
        result.push_back(entity.get());
    }
    return result;
}

std::unordered_map<std::string, Layer> DWGReader::GetLayers() const {
    return m_layers;
}

} // namespace cad
} // namespace vkt
