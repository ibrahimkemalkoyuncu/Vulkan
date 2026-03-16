/**
 * @file DWGReader.cpp
 * @brief DWG Dosya Okuyucu - Basic LibreDWG Implementation
 */

#include "cad/DWGReader.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Arc.hpp"
#include "cad/Polyline.hpp"
#include <sstream>
#include <chrono>
#include <cstring>
#include <iostream>

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

// Entity'den renk bilgisini çıkar ve ata
static void ExtractEntityColor(void* obj_ptr, Entity* entity) {
    if (!entity) return;
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity) return;

    Dwg_Color& c = obj->tio.entity->color;

    if (c.method == 0xc3 && c.rgb != 0) {
        // Truecolor: RGB doğrudan
        float r = ((c.rgb >> 16) & 0xFF) / 255.0f;
        float g = ((c.rgb >> 8) & 0xFF) / 255.0f;
        float b = (c.rgb & 0xFF) / 255.0f;
        entity->SetColor(Color(r, g, b));
    } else if (c.index >= 1 && c.index <= 255) {
        // ACI renk indeksi
        entity->SetColor(Color::FromACI(c.index));
    }
    // index==256 (BYLAYER) veya 0 (BYBLOCK) → varsayılan renk kalır
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
            if (entity) m_stats.lineCount += 2; // çapraz = 2 çizgi
            break;
        default:
            break;
    }

    // Entity'ye renk bilgisi ata
    ExtractEntityColor(obj_ptr, entity);

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
            // İç içe MINSERT: önce transform chain'i pop sonra tekrar push yapılacak
            // Basitlik için: MINSERT'i ExpandInsert olarak genişlet (dizi offsetleri yoksay)
            // TODO: Tam MINSERT desteği nested olarak
            ExpandInsert(child, dwg, transformChain, depth + 1);
            continue;
        }

        // Parse entity in block-local coords
        Entity* entity = ParseEntityByType(child);
        if (!entity) continue;

        // Transform entity through the full chain to world coordinates
        double chainScale = GetChainScale(transformChain);
        double chainRotation = GetChainRotation(transformChain);

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

        if (entity) {
            m_entities.push_back(std::unique_ptr<Entity>(entity));
            m_stats.insertExpanded++;
        }
    }

    // Pop this transform when leaving this INSERT level
    transformChain.pop_back();
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
    return nullptr; // Henüz implementasyon yok — Faz 5'te eklenecek
}

/**
 * @brief ELLIPSE entity parse — Polyline olarak tessellate eder
 *
 * LibreDWG: center, sm_axis (yarı-büyük eksen vektörü), axis_ratio, start/end_angle
 * Tam ellips veya kısmi ellips (elliptical arc) desteklenir.
 */
Entity* DWGReader::ParseEllipse(void* obj_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    if (!obj->tio.entity || !obj->tio.entity->tio.ELLIPSE) return nullptr;

    Dwg_Entity_ELLIPSE* ell = obj->tio.entity->tio.ELLIPSE;

    geom::Vec3 center(ell->center.x, ell->center.y, 0.0);
    double semiMajor = std::sqrt(ell->sm_axis.x * ell->sm_axis.x +
                                  ell->sm_axis.y * ell->sm_axis.y);
    if (semiMajor < 1e-12) return nullptr;

    double semiMinor = semiMajor * ell->axis_ratio;
    double rotAngle = std::atan2(ell->sm_axis.y, ell->sm_axis.x);

    double startParam = ell->start_angle;
    double endParam = ell->end_angle;

    // Tam ellips kontrolü
    bool fullEllipse = (std::abs(endParam - startParam) < 1e-10) ||
                       (std::abs(endParam - startParam - 2.0 * M_PI) < 1e-10);
    if (fullEllipse) {
        startParam = 0.0;
        endParam = 2.0 * M_PI;
    }

    // Parametrik noktalar ile tessellate et
    int segments = 64;
    double sweep = endParam - startParam;
    if (sweep < 0) sweep += 2.0 * M_PI;

    std::vector<Polyline::Vertex> vertices;
    vertices.reserve(segments + 1);

    double cosR = std::cos(rotAngle);
    double sinR = std::sin(rotAngle);

    for (int i = 0; i <= segments; ++i) {
        double t = startParam + sweep * i / segments;
        double lx = semiMajor * std::cos(t);
        double ly = semiMinor * std::sin(t);
        // Rotate by ellipse rotation
        double wx = center.x + lx * cosR - ly * sinR;
        double wy = center.y + lx * sinR + ly * cosR;

        Polyline::Vertex pv;
        pv.pos = geom::Vec3(wx, wy, 0.0);
        vertices.push_back(pv);
    }

    return new Polyline(vertices, fullEllipse);
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

        std::string name(lay->name);
        Layer layer(name, Color::White());

        // ACI renk indeksi (negatif = layer kapalı)
        int colorIndex = lay->color.index;
        if (colorIndex < 0) {
            layer.SetVisible(false);
            colorIndex = -colorIndex;
        }
        if (colorIndex >= 1 && colorIndex <= 255) {
            layer.SetColorIndex(colorIndex);
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
