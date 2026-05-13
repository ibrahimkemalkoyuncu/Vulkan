/**
 * @file SpaceManager.cpp
 * @brief Mahal yönetim sistemi implementasyonu
 */

#include "cad/SpaceManager.hpp"
#include "cad/Line.hpp"
#include "cad/Text.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <set>

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
    SpaceDetectionOptions scaled = options;
    scaled.drawingUnitsToM2 = reader.GetHeader().GetAreaToM2();
    return DetectSpacesFromEntities(reader.GetFilteredEntities(), scaled);
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
        
        // Alan kontrolü — drawing units² → m²
        double rawArea = polyline->GetArea();
        double area = std::abs(rawArea) * options.drawingUnitsToM2;
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

// Ray-casting point-in-polygon (2D, ignores Z)
static bool PolyContainsPoint(const Polyline* poly, const geom::Vec3& pt) {
    if (!poly) return false;
    const auto& verts = poly->GetVertices();
    size_t n = verts.size();
    if (n < 3) return false;
    bool inside = false;
    double px = pt.x, py = pt.y;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double xi = verts[i].pos.x, yi = verts[i].pos.y;
        double xj = verts[j].pos.x, yj = verts[j].pos.y;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

void SpaceManager::RemoveOverlappingCandidates(
    std::vector<SpaceCandidate>& candidates,
    double overlapRatio)
{
    if (candidates.size() < 2) return;

    // Alan büyükten küçüğe sırala — dış çerçeveler önce gelir
    std::sort(candidates.begin(), candidates.end(),
        [](const SpaceCandidate& a, const SpaceCandidate& b) {
            return a.area > b.area;
        });

    // Aday j'nin merkezi, daha büyük aday i'nin içindeyse
    // ve j.area < i.area * overlapRatio → i dış çerçeve → kaldır
    std::vector<bool> remove(candidates.size(), false);

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (remove[i] || !candidates[i].boundary) continue;
        for (size_t j = i + 1; j < candidates.size(); ++j) {
            if (remove[j] || !candidates[j].boundary) continue;
            if (candidates[j].area < candidates[i].area * overlapRatio) {
                if (PolyContainsPoint(candidates[i].boundary, candidates[j].center)) {
                    remove[i] = true;
                    break;
                }
            }
        }
    }

    size_t write = 0;
    for (size_t k = 0; k < candidates.size(); ++k) {
        if (!remove[k]) candidates[write++] = std::move(candidates[k]);
    }
    candidates.resize(write);
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

bool SpaceManager::Deserialize(const std::string& jsonStr) {
    try {
        auto root = nlohmann::json::parse(jsonStr);
        if (!root.contains("spaces")) return false;

        m_spaces.clear();

        for (const auto& spaceJson : root["spaces"]) {
            auto space = std::make_unique<Space>();
            space->Deserialize(spaceJson);
            EntityId id = space->GetId();
            m_spaces[id] = std::move(space);
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// ==================== SEGMENT BİRLEŞTİRME ====================

std::vector<SpaceCandidate> SpaceManager::DetectSpacesFromSegments(
    const std::vector<Entity*>& entities,
    const SpaceDetectionOptions& options,
    double snapTolerance)
{
    // 1. Filtrelenmiş segmentlerin uç noktalarını topla
    struct Seg { geom::Vec3 a, b; };
    std::vector<Seg> segs;

    for (Entity* e : entities) {
        if (!e) continue;
        if (!MatchesLayerFilter(e, options.wallLayerNames)) continue;

        if (e->GetType() == EntityType::Line) {
            auto* line = static_cast<cad::Line*>(e);
            segs.push_back({line->GetStart(), line->GetEnd()});
        } else if (e->GetType() == EntityType::Polyline) {
            auto* poly = static_cast<Polyline*>(e);
            const auto& verts = poly->GetVertices();
            for (size_t i = 0; i + 1 < verts.size(); ++i)
                segs.push_back({verts[i].pos, verts[i+1].pos});
            if (poly->IsClosed() && verts.size() > 2)
                segs.push_back({verts.back().pos, verts[0].pos});
        }
    }

    if (segs.empty()) return {};

    // 2. Uç noktaları snap toleransıyla düğümlere birleştir
    std::vector<geom::Vec3> nodePos;
    double tol2 = snapTolerance * snapTolerance;

    auto snapKey = [&](const geom::Vec3& p) -> size_t {
        for (size_t i = 0; i < nodePos.size(); ++i) {
            double dx = nodePos[i].x - p.x, dy = nodePos[i].y - p.y;
            if (dx*dx + dy*dy <= tol2) return i;
        }
        nodePos.push_back(p);
        return nodePos.size() - 1;
    };

    // Adjacency list (undirected)
    std::vector<std::vector<size_t>> adj;
    auto ensureNode = [&](size_t id) {
        while (adj.size() <= id) adj.push_back({});
    };

    for (const auto& s : segs) {
        size_t ia = snapKey(s.a), ib = snapKey(s.b);
        if (ia == ib) continue;
        ensureNode(ia); ensureNode(ib);
        // Çift kenar ekleme önlemi
        if (std::find(adj[ia].begin(), adj[ia].end(), ib) == adj[ia].end()) {
            adj[ia].push_back(ib);
            adj[ib].push_back(ia);
        }
    }

    size_t N = adj.size();
    if (N < 3) return {};

    // 3. Her düğümün komşularını CCW açısal sıraya göre sırala (planar embedding)
    std::vector<std::vector<size_t>> adjSorted(N);
    for (size_t v = 0; v < N; ++v) {
        adjSorted[v] = adj[v];
        const geom::Vec3& vp = nodePos[v];
        std::sort(adjSorted[v].begin(), adjSorted[v].end(),
            [&](size_t a, size_t b) {
                double angA = std::atan2(nodePos[a].y - vp.y, nodePos[a].x - vp.x);
                double angB = std::atan2(nodePos[b].y - vp.y, nodePos[b].x - vp.x);
                return angA < angB;
            });
    }

    // Half-edge "next": yönlü kenar u→v için yüz içindeki sonraki kenar.
    // v'nin CCW listesinde u'dan önceki komşu (= u'nun CW yönündeki komşusu).
    auto nextHE = [&](size_t u, size_t v) -> std::pair<size_t, size_t> {
        const auto& nbrs = adjSorted[v];
        auto it = std::find(nbrs.begin(), nbrs.end(), u);
        size_t w = (it == nbrs.begin()) ? nbrs.back() : *std::prev(it);
        return {v, w};
    };

    // 4. Tüm yüzleri half-edge traversal ile bul (her yönlü kenar tam bir kez ziyaret edilir)
    std::set<std::pair<size_t, size_t>> visitedHE;
    std::vector<std::vector<size_t>> faces;

    for (size_t u = 0; u < N; ++u) {
        for (size_t v : adjSorted[u]) {
            if (visitedHE.count({u, v})) continue;
            std::vector<size_t> face;
            size_t cu = u, cv = v;
            size_t guard = 0;
            while (!visitedHE.count({cu, cv})) {
                if (++guard > 2 * N + 4) break; // sonsuz döngü koruması
                visitedHE.insert({cu, cv});
                face.push_back(cu);
                auto [nu, nv] = nextHE(cu, cv);
                cu = nu; cv = nv;
            }
            if (face.size() >= 3) faces.push_back(std::move(face));
        }
    }

    // 5. Her yüzden SpaceCandidate üret
    // Shoelace alanı > 0 → CCW sarım → iç yüz (oda).
    // < 0 → dış/sınırsız yüz → atla.
    std::vector<SpaceCandidate> candidates;

    for (const auto& face : faces) {
        if (face.size() < 3) continue;

        // Shoelace imzalı alan
        double signedArea = 0.0;
        for (size_t k = 0; k < face.size(); ++k) {
            const geom::Vec3& a = nodePos[face[k]];
            const geom::Vec3& b = nodePos[face[(k + 1) % face.size()]];
            signedArea += (a.x * b.y - b.x * a.y);
        }
        signedArea *= 0.5;
        if (signedArea <= 0.0) continue; // dış yüz

        double area = signedArea * options.drawingUnitsToM2;
        if (!IsValidArea(area, options.minArea, options.maxArea)) continue;

        auto poly = std::make_unique<Polyline>();
        for (size_t idx : face)
            poly->AddVertex(nodePos[idx]);
        poly->SetClosed(true);

        SpaceCandidate candidate;
        candidate.boundary = poly.get();
        candidate.area     = area;
        candidate.center   = poly->GetCenter();

        if (options.detectNamesFromText)
            candidate.suggestedName = DetectNameFromText(
                candidate.center, entities, options.textSearchRadius);
        if (options.autoInferTypes && !candidate.suggestedName.empty())
            candidate.InferTypeFromName();

        if (area < 0.5) {
            candidate.isValid = false;
            candidate.warnings.push_back("Alan çok küçük (<0.5 m²)");
        }

        m_syntheticPolylines.push_back(std::move(poly));
        candidates.push_back(candidate);
    }

    return candidates;
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
    // Boyut değeri mi? (sadece rakam/nokta/virgül/+/- → ölçü yazısı, oda ismi değil)
    auto looksLikeDimension = [](const std::string& s) -> bool {
        return s.find_first_not_of("0123456789.,+- \t") == std::string::npos;
    };

    std::string bestName;
    double bestDist = searchRadius * searchRadius;

    for (const Entity* e : entities) {
        if (!e || e->GetType() != EntityType::Text) continue;

        const Text* txt = static_cast<const Text*>(e);
        const std::string& content = txt->GetText();

        // Boş, çok kısa veya saf sayısal metinleri atla
        if (content.size() < 2) continue;
        if (looksLikeDimension(content)) continue;

        const geom::Vec3& ip = txt->GetInsertPoint();
        double dx = ip.x - center.x;
        double dy = ip.y - center.y;
        double dist2 = dx*dx + dy*dy;

        if (dist2 < bestDist) {
            bestDist = dist2;
            bestName = content;
        }
    }

    return bestName;
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
