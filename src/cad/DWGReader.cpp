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
        << "  Circles: " << circleCount << "\n"
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
    
    // Parse entities
    ParseEntities(dwg);

    auto end = std::chrono::high_resolution_clock::now();
    m_stats.readTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    m_stats.entityCount = m_entities.size();

    std::cout << "[DWGReader] Parse complete: " << m_entities.size() << " entities"
              << " (LINE=" << m_stats.lineCount
              << " ARC=" << m_stats.arcCount
              << " CIRCLE=" << m_stats.circleCount
              << " POLY=" << m_stats.polylineCount << ")" << std::endl;
    
    if (m_stats.entityCount == 0) {
        m_errorMessage = "DWG dosyası okundu ancak desteklenen entity bulunamadı. Desteklenenler: LINE, ARC, CIRCLE, LWPOLYLINE.";
        return false;
    }
    
    return true;
}

// Transform a point by INSERT parameters (scale, rotate, translate)
static geom::Vec3 TransformInsertPoint(const geom::Vec3& pt,
                                        const geom::Vec3& insPoint,
                                        double xScale, double yScale,
                                        double rotation) {
    double x = pt.x * xScale;
    double y = pt.y * yScale;
    double cosR = std::cos(rotation);
    double sinR = std::sin(rotation);
    return geom::Vec3(
        insPoint.x + x * cosR - y * sinR,
        insPoint.y + x * sinR + y * cosR,
        pt.z
    );
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
        default:
            break;
    }
    return entity;
}

// Expand INSERT entity: parse block entities and transform to world coordinates
void DWGReader::ExpandInsert(void* obj_ptr, void* dwg_ptr) {
    Dwg_Object* obj = static_cast<Dwg_Object*>(obj_ptr);
    Dwg_Data* dwg = static_cast<Dwg_Data*>(dwg_ptr);

    if (!obj->tio.entity || !obj->tio.entity->tio.INSERT) return;
    Dwg_Entity_INSERT* ins = obj->tio.entity->tio.INSERT;

    // Get insertion parameters
    geom::Vec3 insPoint(ins->ins_pt.x, ins->ins_pt.y, 0.0);
    double xScale = ins->scale.x;
    double yScale = ins->scale.y;
    double rotation = ins->rotation; // radians

    // Default scale if zero
    if (std::abs(xScale) < 1e-12) xScale = 1.0;
    if (std::abs(yScale) < 1e-12) yScale = 1.0;

    // Resolve block header
    if (!ins->block_header || !ins->block_header->obj) return;
    Dwg_Object* blockObj = ins->block_header->obj;
    if (blockObj->type != DWG_TYPE_BLOCK_HEADER) return;
    if (!blockObj->tio.object || !blockObj->tio.object->tio.BLOCK_HEADER) return;

    unsigned long blockHandle = blockObj->handle.value;

    // Find entities owned by this block
    for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
        Dwg_Object* child = &dwg->object[i];
        if (!child || child->supertype != DWG_SUPERTYPE_ENTITY) continue;
        if (!child->tio.entity) continue;

        // Check if entity belongs to this block
        unsigned long ownerHandle = child->tio.entity->ownerhandle ?
                                     child->tio.entity->ownerhandle->absolute_ref : 0;
        if (ownerHandle != blockHandle) continue;

        // Skip nested INSERTs for now (avoid infinite recursion)
        if (child->type == DWG_TYPE_INSERT) continue;

        // Parse entity in block-local coords
        Entity* entity = ParseEntityByType(child);
        if (!entity) continue;

        // Transform entity to world coordinates based on type
        switch (entity->GetType()) {
            case EntityType::Line: {
                Line* line = static_cast<Line*>(entity);
                geom::Vec3 s = TransformInsertPoint(line->GetStart(), insPoint, xScale, yScale, rotation);
                geom::Vec3 e = TransformInsertPoint(line->GetEnd(), insPoint, xScale, yScale, rotation);
                delete entity;
                entity = new Line(s, e);
                break;
            }
            case EntityType::Circle: {
                Circle* circle = static_cast<Circle*>(entity);
                geom::Vec3 c = TransformInsertPoint(circle->GetCenter(), insPoint, xScale, yScale, rotation);
                double r = circle->GetRadius() * std::abs(xScale);
                delete entity;
                entity = new Circle(c, r);
                break;
            }
            case EntityType::Arc: {
                Arc* arc = static_cast<Arc*>(entity);
                geom::Vec3 c = TransformInsertPoint(arc->GetCenter(), insPoint, xScale, yScale, rotation);
                double r = arc->GetRadius() * std::abs(xScale);
                double sa = arc->GetStartAngle() + rotation * (180.0 / M_PI);
                double ea = arc->GetEndAngle() + rotation * (180.0 / M_PI);
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
                    nv.pos = TransformInsertPoint(v.pos, insPoint, xScale, yScale, rotation);
                    nv.bulge = v.bulge;
                    nv.width = v.width * std::abs(xScale);
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
            // Expand INSERT: flatten block entities to world coordinates
            ExpandInsert(obj, dwg);
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

Entity* DWGReader::ParsePolyline(void* obj_ptr) {
    return nullptr; // Not implemented yet
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
    return nullptr; // Not implemented yet
}

void DWGReader::SetLayerFilter(const std::unordered_set<std::string>& layers) {
    m_layerFilter = layers;
}

bool DWGReader::PassesLayerFilter(const std::string& layerName) const {
    if (m_layerFilter.empty()) return true;
    return m_layerFilter.find(layerName) != m_layerFilter.end();
}

void DWGReader::ExtractLayers(void* dwg_ptr) {
    // Not implemented yet - using default layer only
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
