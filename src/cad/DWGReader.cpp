/**
 * @file DWGReader.cpp
 * @brief DWG Dosya Okuyucu - Basic LibreDWG Implementation
 */

#include "cad/DWGReader.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Polyline.hpp"
#include <sstream>
#include <chrono>
#include <cstring>

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
    
    if (m_stats.entityCount == 0) {
        m_errorMessage = "DWG dosyası okundu ancak desteklenen entity bulunamadı. Desteklenenler: LINE, CIRCLE, LWPOLYLINE.";
        return false;
    }
    
    return true;
}

bool DWGReader::ParseEntities(void* dwg_ptr) {
    Dwg_Data* dwg = static_cast<Dwg_Data*>(dwg_ptr);
    
    // Iterate all objects
    for (BITCODE_BL i = 0; i < dwg->num_objects; ++i) {
        Dwg_Object* obj = &dwg->object[i];
        if (!obj) continue;
        
        // Check if it's an entity
        if (obj->supertype != DWG_SUPERTYPE_ENTITY) continue;
        
        Entity* entity = nullptr;
        
        // Parse only simple entities
        switch (obj->type) {
            case DWG_TYPE_LINE:
                entity = ParseLine(obj);
                if (entity) m_stats.lineCount++;
                break;
                
            case DWG_TYPE_CIRCLE:
                entity = ParseCircle(obj);
                if (entity) m_stats.circleCount++;
                break;
                
            case DWG_TYPE_LWPOLYLINE:
                entity = ParseLWPolyline(obj);
                if (entity) m_stats.polylineCount++;
                break;
                
            default:
                // Other entity types not supported yet
                break;
        }
        
        if (entity) {
            m_entities.push_back(std::unique_ptr<Entity>(entity));
        }
    }
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
    
    // Skip if not enough points
    if (lwpoly->num_points < 3) return nullptr;
    
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
    return nullptr; // Not implemented yet
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
