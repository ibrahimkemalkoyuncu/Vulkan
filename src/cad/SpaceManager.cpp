/**
 * @file SpaceManager.cpp
 * @brief Mahal yönetim sistemi implementasyonu
 */

#include "cad/SpaceManager.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace vkt::cad {

// ==================== SpaceCandidate ====================

void SpaceCandidate::InferTypeFromName() {
    // İsimden tip tahmini (Türkçe + İngilizce)
    std::string lowerName = suggestedName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (lowerName.find("wc") != std::string::npos) {
        suggestedType = SpaceType::WC;
    } else if (lowerName.find("banyo") != std::string::npos || 
               lowerName.find("bathroom") != std::string::npos) {
        suggestedType = SpaceType::Bathroom;
    } else if (lowerName.find("mutfak") != std::string::npos || 
               lowerName.find("kitchen") != std::string::npos) {
        suggestedType = SpaceType::Kitchen;
    } else if (lowerName.find("yatak") != std::string::npos || 
               lowerName.find("bedroom") != std::string::npos) {
        suggestedType = SpaceType::Bedroom;
    } else if (lowerName.find("salon") != std::string::npos || 
               lowerName.find("living") != std::string::npos) {
        suggestedType = SpaceType::LivingRoom;
    } else if (lowerName.find("koridor") != std::string::npos || 
               lowerName.find("corridor") != std::string::npos) {
        suggestedType = SpaceType::Corridor;
    } else if (lowerName.find("balkon") != std::string::npos || 
               lowerName.find("balcony") != std::string::npos) {
        suggestedType = SpaceType::Balcony;
    } else if (lowerName.find("ofis") != std::string::npos || 
               lowerName.find("office") != std::string::npos) {
        suggestedType = SpaceType::Office;
    } else if (lowerName.find("depo") != std::string::npos || 
               lowerName.find("storage") != std::string::npos) {
        suggestedType = SpaceType::Storage;
    } else if (lowerName.find("garaj") != std::string::npos || 
               lowerName.find("garage") != std::string::npos) {
        suggestedType = SpaceType::Garage;
    } else if (lowerName.find("çamaşır") != std::string::npos || 
               lowerName.find("laundry") != std::string::npos) {
        suggestedType = SpaceType::LaundryRoom;
    } else if (lowerName.find("kazan") != std::string::npos || 
               lowerName.find("boiler") != std::string::npos) {
        suggestedType = SpaceType::BoilerRoom;
    } else if (lowerName.find("mekanik") != std::string::npos || 
               lowerName.find("mechanical") != std::string::npos) {
        suggestedType = SpaceType::MechanicalRoom;
    } else {
        suggestedType = SpaceType::Unknown;
    }
}

// ==================== SpaceManager ====================

SpaceManager::SpaceManager() = default;
SpaceManager::~SpaceManager() = default;

Space* SpaceManager::CreateSpace(const Polyline* boundary, const std::string& name) {
    if (!boundary || !boundary->IsClosed()) {
        return nullptr;
    }
    
    // Boundary'yi kopyala
    auto copiedBoundary = std::make_unique<Polyline>(*boundary);
    
    // Space oluştur
    auto space = std::make_unique<Space>(std::move(copiedBoundary));
    
    // İsim ata
    if (!name.empty()) {
        space->SetName(name);
    } else {
        space->SetName(GenerateUniqueName("Mahal"));
    }
    
    // Numara üret
    space->SetNumber(GenerateSpaceNumber(space->GetSpaceType()));
    
    // Kaydet
    EntityId id = space->GetId();
    Space* spacePtr = space.get();
    m_spaces[id] = std::move(space);
    
    return spacePtr;
}

bool SpaceManager::DeleteSpace(EntityId spaceId) {
    auto it = m_spaces.find(spaceId);
    if (it == m_spaces.end()) {
        return false;
    }
    
    m_spaces.erase(it);
    return true;
}

Space* SpaceManager::GetSpace(EntityId spaceId) {
    auto it = m_spaces.find(spaceId);
    return (it != m_spaces.end()) ? it->second.get() : nullptr;
}

const Space* SpaceManager::GetSpace(EntityId spaceId) const {
    auto it = m_spaces.find(spaceId);
    return (it != m_spaces.end()) ? it->second.get() : nullptr;
}

std::vector<Space*> SpaceManager::GetAllSpaces() {
    std::vector<Space*> result;
    result.reserve(m_spaces.size());
    for (auto& pair : m_spaces) {
        result.push_back(pair.second.get());
    }
    return result;
}

std::vector<const Space*> SpaceManager::GetAllSpaces() const {
    std::vector<const Space*> result;
    result.reserve(m_spaces.size());
    for (const auto& pair : m_spaces) {
        result.push_back(pair.second.get());
    }
    return result;
}

void SpaceManager::Clear() {
    m_spaces.clear();
}

// ==================== OTOMATİK TESPİT ====================

std::vector<SpaceCandidate> SpaceManager::DetectSpacesFromDXF(
    const DXFReader& reader,
    const SpaceDetectionOptions& options)
{
    return DetectSpacesFromEntities(reader.GetFilteredEntities(), options);
}

std::vector<SpaceCandidate> SpaceManager::DetectSpacesFromEntities(
    const std::vector<Entity*>& entities,
    const SpaceDetectionOptions& options)
{
    std::vector<SpaceCandidate> candidates;
    
    // 1. Kapalı polyline'ları filtrele
    auto polylines = FilterClosedPolylines(entities);
    
    for (Polyline* polyline : polylines) {
        // Layer kontrolü
        if (!MatchesLayerFilter(polyline, options.wallLayerNames)) {
            continue;
        }
        
        // Alan kontrolü
        double area = polyline->GetArea();
        if (!IsValidArea(area, options.minArea, options.maxArea)) {
            continue;
        }
        
        // Aday oluştur
        SpaceCandidate candidate;
        candidate.boundary = polyline;
        candidate.area = area;
        candidate.center = polyline->GetCenter();
        
        // TEXT entity'lerden isim tespiti
        if (options.detectNamesFromText) {
            candidate.suggestedName = DetectNameFromText(
                candidate.center, 
                entities, 
                options.textSearchRadius
            );
        }
        
        // Otomatik tip tahmini
        if (options.autoInferTypes && !candidate.suggestedName.empty()) {
            candidate.InferTypeFromName();
        }
        
        // Validasyon
        if (area < 0.5) {
            candidate.isValid = false;
            candidate.warnings.push_back("Alan çok küçük (<0.5 m²)");
        }
        if (area > 1000.0) {
            candidate.isValid = false;
            candidate.warnings.push_back("Alan çok büyük (>1000 m²)");
        }
        
        candidates.push_back(candidate);
    }
    
    return candidates;
}

Space* SpaceManager::AcceptCandidate(const SpaceCandidate& candidate) {
    if (!candidate.boundary || !candidate.isValid) {
        return nullptr;
    }
    
    Space* space = CreateSpace(candidate.boundary, candidate.suggestedName);
    if (space) {
        space->SetSpaceType(candidate.suggestedType);
    }
    
    return space;
}

// ==================== SORGU OPERASYONLARI ====================

std::vector<Space*> SpaceManager::GetSpacesByType(SpaceType type) {
    std::vector<Space*> result;
    for (auto& pair : m_spaces) {
        if (pair.second->GetSpaceType() == type) {
            result.push_back(pair.second.get());
        }
    }
    return result;
}

std::vector<Space*> SpaceManager::FindSpacesByName(const std::string& name) {
    std::vector<Space*> result;
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    for (auto& pair : m_spaces) {
        std::string spaceName = pair.second->GetName();
        std::transform(spaceName.begin(), spaceName.end(), spaceName.begin(), ::tolower);
        
        if (spaceName.find(lowerName) != std::string::npos) {
            result.push_back(pair.second.get());
        }
    }
    
    return result;
}

Space* SpaceManager::FindSpaceAtPoint(const geom::Vec3& point) {
    for (auto& pair : m_spaces) {
        if (pair.second->ContainsPoint(point)) {
            return pair.second.get();
        }
    }
    return nullptr;
}

bool SpaceManager::AreAdjacent(EntityId spaceId1, EntityId spaceId2, double tolerance) const {
    const Space* space1 = GetSpace(spaceId1);
    const Space* space2 = GetSpace(spaceId2);
    
    if (!space1 || !space2) {
        return false;
    }
    
    return Space::AreAdjacent(*space1, *space2, tolerance);
}

void SpaceManager::DetectAllAdjacencies(double tolerance) {
    // Tüm mahaller arasında komşuluk tespiti
    std::vector<Space*> spaces = GetAllSpaces();
    
    for (size_t i = 0; i < spaces.size(); ++i) {
        for (size_t j = i + 1; j < spaces.size(); ++j) {
            Space* space1 = spaces[i];
            Space* space2 = spaces[j];
            
            if (Space::AreAdjacent(*space1, *space2, tolerance)) {
                // İki yönlü komşuluk kaydı
                space1->AddAdjacentSpace(space2->GetId());
                space2->AddAdjacentSpace(space1->GetId());
            }
        }
    }
}

// ==================== VALİDASYON ====================

size_t SpaceManager::ValidateAll() {
    size_t invalidCount = 0;
    for (auto& pair : m_spaces) {
        if (!pair.second->IsValid()) {
            invalidCount++;
        }
    }
    return invalidCount;
}

std::vector<Space*> SpaceManager::GetInvalidSpaces() {
    std::vector<Space*> result;
    for (auto& pair : m_spaces) {
        if (!pair.second->IsValid()) {
            result.push_back(pair.second.get());
        }
    }
    return result;
}

// ==================== İSTATİSTİKLER ====================

SpaceManager::Statistics SpaceManager::GetStatistics() const {
    Statistics stats;
    stats.totalSpaces = m_spaces.size();
    
    for (const auto& pair : m_spaces) {
        const Space* space = pair.second.get();
        
        if (space->IsValid()) {
            stats.validSpaces++;
        } else {
            stats.invalidSpaces++;
        }
        
        stats.totalArea += space->GetArea();
        stats.totalVolume += space->GetVolume();
        stats.countsByType[space->GetSpaceType()]++;
    }
    
    return stats;
}

std::string SpaceManager::Statistics::ToString() const {
    std::stringstream ss;
    ss << "Mahal İstatistikleri:\n";
    ss << "  Toplam: " << totalSpaces << "\n";
    ss << "  Geçerli: " << validSpaces << "\n";
    ss << "  Geçersiz: " << invalidSpaces << "\n";
    ss << "  Toplam Alan: " << totalArea << " m²\n";
    ss << "  Toplam Hacim: " << totalVolume << " m³\n";
    ss << "\nTip Dağılımı:\n";
    for (const auto& pair : countsByType) {
        ss << "  " << Space::SpaceTypeToString(pair.first) << ": " << pair.second << "\n";
    }
    return ss.str();
}

// ==================== SERİALİZASYON ====================

std::string SpaceManager::Serialize() const {
    nlohmann::json root;
    root["spaces"] = nlohmann::json::array();
    
    for (const auto& pair : m_spaces) {
        nlohmann::json spaceJson;
        pair.second->Serialize(spaceJson);
        root["spaces"].push_back(spaceJson);
    }
    
    return root.dump(2); // Pretty print with 2 spaces indent
}

bool SpaceManager::Deserialize(const std::string& json) {
    // TODO: JSON parsing implementasyonu
    // Şimdilik basitleştirilmiş - gerçek projelerde nlohmann/json kullanılır
    return false;
}

// ==================== PRIVATE HELPER'LAR ====================

std::vector<Polyline*> SpaceManager::FilterClosedPolylines(const std::vector<Entity*>& entities) {
    std::vector<Polyline*> result;
    
    for (Entity* entity : entities) {
        if (entity->GetType() == EntityType::Polyline) {
            Polyline* polyline = static_cast<Polyline*>(entity);
            if (polyline->IsClosed()) {
                result.push_back(polyline);
            }
        }
    }
    
    return result;
}

bool SpaceManager::MatchesLayerFilter(const Entity* entity, const std::vector<std::string>& layerNames) const {
    if (layerNames.empty()) {
        return true; // Filtre yok, hepsine izin ver
    }
    
    const Layer* layer = entity->GetLayer();
    if (!layer) {
        return false;
    }
    
    std::string entityLayerName = layer->GetName();
    std::transform(entityLayerName.begin(), entityLayerName.end(), entityLayerName.begin(), ::tolower);
    
    for (const std::string& filterName : layerNames) {
        std::string lowerFilter = filterName;
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        
        if (entityLayerName.find(lowerFilter) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool SpaceManager::IsValidArea(double area, double minArea, double maxArea) const {
    return area >= minArea && area <= maxArea;
}

std::string SpaceManager::DetectNameFromText(const geom::Vec3& center, 
                                              const std::vector<Entity*>& entities,
                                              double searchRadius) const {
    // TODO: TEXT entity implementasyonu yoksa şimdilik boş döner
    // TEXT entity oluşturulduktan sonra bu metot spaceName'leri tespit edecek
    
    // Yaklaşık mantık:
    // 1. Tüm TEXT entity'leri filtrele
    // 2. center'a searchRadius içinde olanları bul
    // 3. En yakın olanın text değerini döndür
    
    return "";
}

std::string SpaceManager::GenerateUniqueName(const std::string& baseName) {
    int counter = 1;
    std::string uniqueName;
    
    while (true) {
        uniqueName = baseName + " " + std::to_string(counter);
        
        // İsim benzersiz mi kontrol et
        bool exists = false;
        for (const auto& pair : m_spaces) {
            if (pair.second->GetName() == uniqueName) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            break;
        }
        
        counter++;
    }
    
    return uniqueName;
}

std::string SpaceManager::GenerateSpaceNumber(SpaceType type) {
    // Tip başına sayaç tut
    static std::map<SpaceType, int> typeCounters;
    
    int counter = ++typeCounters[type];
    
    // Tip koduna göre numara üret
    // Örnek: Bathroom -> B01, B02, Kitchen -> K01, K02
    std::string prefix;
    switch (type) {
        case SpaceType::Bathroom: prefix = "B"; break;
        case SpaceType::WC: prefix = "W"; break;
        case SpaceType::Kitchen: prefix = "K"; break;
        case SpaceType::Bedroom: prefix = "BR"; break;
        case SpaceType::LivingRoom: prefix = "LR"; break;
        case SpaceType::Office: prefix = "O"; break;
        case SpaceType::Corridor: prefix = "C"; break;
        case SpaceType::Storage: prefix = "S"; break;
        default: prefix = "R"; break;
    }
    
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%s%02d", prefix.c_str(), counter);
    return std::string(buffer);
}

} // namespace vkt::cad
