/**
 * @file DXFReader.cpp
 * @brief DXF okuyucu implementasyonu
 */

#include "cad/DXFReader.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cctype>

namespace vkt::cad {

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
    if (entityType == "LWPOLYLINE") {
        return ReadLWPolyline();
    } else if (entityType == "LINE") {
        return ReadLine();
    } else if (entityType == "ARC") {
        return ReadArc();
    } else if (entityType == "CIRCLE") {
        return ReadCircle();
    } else if (entityType == "INSERT") {
        return ReadInsert();
    } else {
        SkipEntity();
        return nullptr;
    }
}

std::unique_ptr<Entity> DXFReader::ReadLWPolyline() {
    DXFCode code;
    
    std::string layerName = "0";
    std::vector<geom::Vec3> vertices;
    std::vector<double> bulges;
    bool closed = false;
    
    while (ReadCode(code)) {
        if (code.code == 0) {
            // Bir sonraki entity başladı — geri koy ve çık
            PushBackCode(code);
            break;
        }

        if (code.code == 8) {
            layerName = code.value;
        } else if (code.code == 70) {
            closed = (code.AsInt() & 1) != 0;
        } else if (code.code == 90) {
            int count = code.AsInt();
            vertices.reserve(count);
            bulges.reserve(count);
        } else if (code.code == 10) {
            double x = code.AsDouble();
            if (ReadCode(code) && code.code == 20) {
                double y = code.AsDouble();
                vertices.push_back(geom::Vec3(x, y, 0.0));
                bulges.push_back(0.0);
            }
        } else if (code.code == 42) {
            if (!bulges.empty()) {
                bulges.back() = code.AsDouble();
            }
        }
    }
    
    if (vertices.size() < 2) {
        return nullptr;
    }
    
    // Polyline entity oluştur
    std::vector<Polyline::Vertex> polyVertices;
    for (size_t i = 0; i < vertices.size(); ++i) {
        Polyline::Vertex v;
        v.pos = vertices[i];
        v.bulge = i < bulges.size() ? bulges[i] : 0.0;
        polyVertices.push_back(v);
    }
    
    auto polyline = std::make_unique<Polyline>(polyVertices, closed);
    polyline->SetLayer(GetOrCreateLayer(layerName));
    
    return polyline;
}

std::unique_ptr<Entity> DXFReader::ReadLine() {
    DXFCode code;
    
    std::string layerName = "0";
    geom::Vec3 start, end;
    bool hasStart = false, hasEnd = false;
    
    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }

        if (code.code == 8) {
            layerName = code.value;
        } else if (code.code == 10) {
            start.x = code.AsDouble();
            hasStart = true;
        } else if (code.code == 20) {
            start.y = code.AsDouble();
        } else if (code.code == 30) {
            start.z = code.AsDouble();
        } else if (code.code == 11) {
            end.x = code.AsDouble();
            hasEnd = true;
        } else if (code.code == 21) {
            end.y = code.AsDouble();
        } else if (code.code == 31) {
            end.z = code.AsDouble();
        }
    }
    
    if (!hasStart || !hasEnd) {
        return nullptr;
    }
    
    auto line = std::make_unique<Line>(start, end);
    line->SetLayer(GetOrCreateLayer(layerName));
    
    return line;
}

std::unique_ptr<Entity> DXFReader::ReadArc() {
    DXFCode code;
    
    std::string layerName = "0";
    geom::Vec3 center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 360.0;
    
    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }

        if (code.code == 8) {
            layerName = code.value;
        } else if (code.code == 10) {
            center.x = code.AsDouble();
        } else if (code.code == 20) {
            center.y = code.AsDouble();
        } else if (code.code == 30) {
            center.z = code.AsDouble();
        } else if (code.code == 40) {
            radius = code.AsDouble();
        } else if (code.code == 50) {
            startAngle = code.AsDouble();
        } else if (code.code == 51) {
            endAngle = code.AsDouble();
        }
    }

    if (radius <= 0.0) {
        return nullptr;
    }

    auto arc = std::make_unique<Arc>(center, radius, startAngle, endAngle);
    arc->SetLayer(GetOrCreateLayer(layerName));
    
    return arc;
}

std::unique_ptr<Entity> DXFReader::ReadCircle() {
    DXFCode code;
    
    std::string layerName = "0";
    geom::Vec3 center;
    double radius = 0.0;
    
    while (ReadCode(code)) {
        if (code.code == 0) { PushBackCode(code); break; }

        if (code.code == 8) {
            layerName = code.value;
        } else if (code.code == 10) {
            center.x = code.AsDouble();
        } else if (code.code == 20) {
            center.y = code.AsDouble();
        } else if (code.code == 30) {
            center.z = code.AsDouble();
        } else if (code.code == 40) {
            radius = code.AsDouble();
        }
    }

    if (radius <= 0.0) {
        return nullptr;
    }

    auto circle = std::make_unique<Circle>(center, radius);
    circle->SetLayer(GetOrCreateLayer(layerName));
    
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

    // Block tanımını bul ve entity'lerini kopyala + transform uygula
    auto blockIt = m_blocks.find(blockName);
    if (blockIt == m_blocks.end() || blockIt->second.empty()) {
        return nullptr; // Block bulunamadı
    }

    // Block'taki ilk entity'yi kopyala (basit yaklaşım)
    // Birden fazla entity varsa, ilkini INSERT noktasına taşı
    // Gelişmiş: CompositeEntity oluşturulabilir
    if (blockIt->second.size() == 1) {
        auto clone = blockIt->second[0]->Clone();
        clone->SetLayer(GetOrCreateLayer(layerName));
        // Transform uygula: scale → rotate → translate
        clone->Scale(scale);
        clone->Rotate(rotation);
        clone->Move(insertionPoint);
        return clone;
    }

    // Çoklu entity içeren block: Polyline olarak birleştir veya ilk entity'yi döndür
    // Şimdilik ilk entity'yi döndür
    auto clone = blockIt->second[0]->Clone();
    clone->SetLayer(GetOrCreateLayer(layerName));
    clone->Scale(scale);
    clone->Rotate(rotation);
    clone->Move(insertionPoint);
    return clone;
}

Layer* DXFReader::GetOrCreateLayer(const std::string& layerName) {
    auto it = m_layers.find(layerName);
    if (it != m_layers.end()) {
        return &it->second;
    }
    
    // Yeni layer oluştur
    Layer newLayer;
    m_layers[layerName] = newLayer;
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
        std::string layerName = ""; // Layer::GetName() yok - TODO
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
