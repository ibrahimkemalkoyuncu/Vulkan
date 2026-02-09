/**
 * @file Space.cpp
 * @brief Space entity implementasyonu
 */

#include "cad/Space.hpp"
#include "cad/Layer.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>

namespace vkt::cad {

// SpaceRequirements static method
SpaceRequirements SpaceRequirements::GetDefaultForType(SpaceType type) {
    SpaceRequirements req;
    
    switch (type) {
        case SpaceType::Bathroom:
            req.needsColdWater = true;
            req.needsHotWater = true;
            req.needsDrain = true;
            req.needsHeating = true;
            req.needsVentilation = true;
            break;
            
        case SpaceType::WC:
            req.needsColdWater = true;
            req.needsDrain = true;
            req.needsVentilation = true;
            break;
            
        case SpaceType::Kitchen:
            req.needsColdWater = true;
            req.needsHotWater = true;
            req.needsDrain = true;
            req.needsHeating = true;
            req.needsVentilation = true;
            req.needsGas = true;
            break;
            
        case SpaceType::LaundryRoom:
            req.needsColdWater = true;
            req.needsHotWater = true;
            req.needsDrain = true;
            req.needsVentilation = true;
            break;
            
        case SpaceType::LivingRoom:
        case SpaceType::Bedroom:
        case SpaceType::DiningRoom:
        case SpaceType::StudyRoom:
            req.needsHeating = true;
            break;
            
        case SpaceType::MechanicalRoom:
        case SpaceType::BoilerRoom:
            req.needsColdWater = true;
            req.needsDrain = true;
            req.needsVentilation = true;
            break;
            
        default:
            // Unknown or other types - no requirements by default
            break;
    }
    
    return req;
}

// Space implementation
Space::Space()
    : Entity(), m_boundary(nullptr),
      m_name(""), m_number(""),
      m_spaceType(SpaceType::Unknown),
      m_height(3.0), m_floorLevel(0.0), m_ceilingHeight(2.8),
      m_requirements() {}

Space::Space(std::unique_ptr<Polyline> boundary, const std::string& name, SpaceType type)
    : Entity(), m_boundary(std::move(boundary)),
      m_name(name), m_number(""),
      m_spaceType(type),
      m_height(3.0), m_floorLevel(0.0), m_ceilingHeight(2.8),
      m_requirements(SpaceRequirements::GetDefaultForType(type)) {}

std::unique_ptr<Entity> Space::Clone() const {
    auto clone = std::make_unique<Space>();
    
    // Boundary'yi deep copy
    if (m_boundary) {
        clone->m_boundary = std::make_unique<Polyline>(*m_boundary);
    }
    
    // Diğer property'leri kopyala
    clone->m_name = m_name;
    clone->m_number = m_number;
    clone->m_spaceType = m_spaceType;
    clone->m_height = m_height;
    clone->m_floorLevel = m_floorLevel;
    clone->m_ceilingHeight = m_ceilingHeight;
    clone->m_requirements = m_requirements;
    clone->m_fixtureIds = m_fixtureIds;
    clone->m_adjacentSpaceIds = m_adjacentSpaceIds;
    
    // Entity properties
    clone->SetLayer(const_cast<Layer*>(GetLayer()));
    clone->SetColor(GetColor());
    clone->SetVisible(IsVisible());
    clone->SetLocked(IsLocked());
    
    return clone;
}

BoundingBox Space::GetBounds() const {
    if (m_boundary) {
        return m_boundary->GetBounds();
    }
    return BoundingBox();
}

void Space::Serialize(nlohmann::json& j) const {
    j["type"] = "Space";
    
    // Boundary
    if (m_boundary) {
        nlohmann::json boundaryJson;
        m_boundary->Serialize(boundaryJson);
        j["boundary"] = boundaryJson;
    }
    
    // Identity
    j["name"] = m_name;
    j["number"] = m_number;
    j["spaceType"] = static_cast<int>(m_spaceType);
    
    // Dimensions
    j["height"] = m_height;
    j["floorLevel"] = m_floorLevel;
    j["ceilingHeight"] = m_ceilingHeight;
    
    // Requirements
    j["requirements"] = {
        {"coldWater", m_requirements.needsColdWater},
        {"hotWater", m_requirements.needsHotWater},
        {"drain", m_requirements.needsDrain},
        {"heating", m_requirements.needsHeating},
        {"cooling", m_requirements.needsCooling},
        {"ventilation", m_requirements.needsVentilation},
        {"fireSuppression", m_requirements.needsFireSuppression},
        {"gas", m_requirements.needsGas}
    };
    
    // Relations
    j["fixtureIds"] = m_fixtureIds;
    j["adjacentSpaceIds"] = m_adjacentSpaceIds;
}

void Space::Deserialize(const nlohmann::json& j) {
    // Boundary
    if (j.contains("boundary")) {
        m_boundary = std::make_unique<Polyline>();
        m_boundary->Deserialize(j["boundary"]);
    }
    
    // Identity
    m_name = j.value("name", "");
    m_number = j.value("number", "");
    m_spaceType = static_cast<SpaceType>(j.value("spaceType", 0));
    
    // Dimensions
    m_height = j.value("height", 3.0);
    m_floorLevel = j.value("floorLevel", 0.0);
    m_ceilingHeight = j.value("ceilingHeight", 2.8);
    
    // Requirements
    if (j.contains("requirements")) {
        const auto& req = j["requirements"];
        m_requirements.needsColdWater = req.value("coldWater", false);
        m_requirements.needsHotWater = req.value("hotWater", false);
        m_requirements.needsDrain = req.value("drain", false);
        m_requirements.needsHeating = req.value("heating", false);
        m_requirements.needsCooling = req.value("cooling", false);
        m_requirements.needsVentilation = req.value("ventilation", false);
        m_requirements.needsFireSuppression = req.value("fireSuppression", false);
        m_requirements.needsGas = req.value("gas", false);
    }
    
    // Relations
    if (j.contains("fixtureIds")) {
        m_fixtureIds = j["fixtureIds"].get<std::vector<size_t>>();
    }
    if (j.contains("adjacentSpaceIds")) {
        m_adjacentSpaceIds = j["adjacentSpaceIds"].get<std::vector<size_t>>();
    }
}

void Space::SetBoundary(std::unique_ptr<Polyline> boundary) {
    m_boundary = std::move(boundary);
    SetFlags(GetFlags() | EF_Modified);
}

double Space::GetArea() const {
    if (!m_boundary) return 0.0;
    return m_boundary->GetArea();
}

double Space::GetPerimeter() const {
    if (!m_boundary) return 0.0;
    return m_boundary->GetLength();
}

double Space::GetVolume() const {
    return GetArea() * m_ceilingHeight;
}

bool Space::ContainsPoint(const geom::Vec3& point) const {
    if (!m_boundary) return false;
    
    // Point-in-polygon test using ray casting
    const auto& vertices = m_boundary->GetVertices();
    if (vertices.size() < 3) return false;
    
    bool inside = false;
    size_t j = vertices.size() - 1;
    
    for (size_t i = 0; i < vertices.size(); j = i++) {
        const auto& vi = vertices[i].pos;
        const auto& vj = vertices[j].pos;
        
        if (((vi.y > point.y) != (vj.y > point.y)) &&
            (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x)) {
            inside = !inside;
        }
    }
    
    return inside;
}

geom::Vec3 Space::GetCenter() const {
    if (!m_boundary) return geom::Vec3();
    return m_boundary->GetCentroid();
}

void Space::SetSpaceType(SpaceType type) {
    m_spaceType = type;
    // Tip değiştiğinde varsayılan gereksinimleri güncelle
    m_requirements = SpaceRequirements::GetDefaultForType(type);
    SetFlags(GetFlags() | EF_Modified);
}

void Space::AddFixture(size_t fixtureEntityId) {
    if (std::find(m_fixtureIds.begin(), m_fixtureIds.end(), fixtureEntityId) == m_fixtureIds.end()) {
        m_fixtureIds.push_back(fixtureEntityId);
        SetFlags(GetFlags() | EF_Modified);
    }
}

void Space::RemoveFixture(size_t fixtureEntityId) {
    auto it = std::find(m_fixtureIds.begin(), m_fixtureIds.end(), fixtureEntityId);
    if (it != m_fixtureIds.end()) {
        m_fixtureIds.erase(it);
        SetFlags(GetFlags() | EF_Modified);
    }
}

void Space::ClearFixtures() {
    m_fixtureIds.clear();
    SetFlags(GetFlags() | EF_Modified);
}

void Space::AddAdjacentSpace(size_t spaceId) {
    if (std::find(m_adjacentSpaceIds.begin(), m_adjacentSpaceIds.end(), spaceId) == m_adjacentSpaceIds.end()) {
        m_adjacentSpaceIds.push_back(spaceId);
        SetFlags(GetFlags() | EF_Modified);
    }
}

bool Space::AreAdjacent(const Space& space1, const Space& space2, double tolerance) {
    // İki space'in boundary'leri paylaşılan segment var mı?
    // Basitleştirilmiş: Bounding box'ları kesişiyorsa komşu kabul et
    BoundingBox bb1 = space1.GetBounds();
    BoundingBox bb2 = space2.GetBounds();
    
    // Genişletilmiş bounding box (tolerance ile)
    bb1.min.x -= tolerance;
    bb1.min.y -= tolerance;
    bb1.max.x += tolerance;
    bb1.max.y += tolerance;
    
    return bb1.Intersects(bb2);
}

std::string Space::SpaceTypeToString(SpaceType type) {
    switch (type) {
        case SpaceType::LivingRoom: return "Oturma Odası";
        case SpaceType::Bedroom: return "Yatak Odası";
        case SpaceType::Kitchen: return "Mutfak";
        case SpaceType::DiningRoom: return "Yemek Odası";
        case SpaceType::StudyRoom: return "Çalışma Odası";
        case SpaceType::Bathroom: return "Banyo";
        case SpaceType::WC: return "Tuvalet";
        case SpaceType::LaundryRoom: return "Çamaşır Odası";
        case SpaceType::Corridor: return "Koridor";
        case SpaceType::Balcony: return "Balkon";
        case SpaceType::Terrace: return "Teras";
        case SpaceType::Storage: return "Depo";
        case SpaceType::MechanicalRoom: return "Makine Dairesi";
        case SpaceType::ElectricalRoom: return "Elektrik Odası";
        case SpaceType::BoilerRoom: return "Kazan Dairesi";
        case SpaceType::Office: return "Ofis";
        case SpaceType::MeetingRoom: return "Toplantı Odası";
        case SpaceType::Reception: return "Resepsiyon";
        case SpaceType::Garage: return "Garaj";
        case SpaceType::Garden: return "Bahçe";
        case SpaceType::Void: return "Boşluk";
        case SpaceType::Other: return "Diğer";
        default: return "Bilinmeyen";
    }
}

SpaceType Space::StringToSpaceType(const std::string& str) {
    if (str == "Oturma Odası" || str == "LivingRoom") return SpaceType::LivingRoom;
    if (str == "Yatak Odası" || str == "Bedroom") return SpaceType::Bedroom;
    if (str == "Mutfak" || str == "Kitchen") return SpaceType::Kitchen;
    if (str == "Yemek Odası" || str == "DiningRoom") return SpaceType::DiningRoom;
    if (str == "Çalışma Odası" || str == "StudyRoom") return SpaceType::StudyRoom;
    if (str == "Banyo" || str == "Bathroom") return SpaceType::Bathroom;
    if (str == "Tuvalet" || str == "WC") return SpaceType::WC;
    if (str == "Çamaşır Odası" || str == "LaundryRoom") return SpaceType::LaundryRoom;
    if (str == "Koridor" || str == "Corridor") return SpaceType::Corridor;
    if (str == "Balkon" || str == "Balcony") return SpaceType::Balcony;
    if (str == "Teras" || str == "Terrace") return SpaceType::Terrace;
    if (str == "Depo" || str == "Storage") return SpaceType::Storage;
    if (str == "Makine Dairesi" || str == "MechanicalRoom") return SpaceType::MechanicalRoom;
    if (str == "Elektrik Odası" || str == "ElectricalRoom") return SpaceType::ElectricalRoom;
    if (str == "Kazan Dairesi" || str == "BoilerRoom") return SpaceType::BoilerRoom;
    if (str == "Ofis" || str == "Office") return SpaceType::Office;
    if (str == "Toplantı Odası" || str == "MeetingRoom") return SpaceType::MeetingRoom;
    if (str == "Resepsiyon" || str == "Reception") return SpaceType::Reception;
    if (str == "Garaj" || str == "Garage") return SpaceType::Garage;
    if (str == "Bahçe" || str == "Garden") return SpaceType::Garden;
    if (str == "Boşluk" || str == "Void") return SpaceType::Void;
    if (str == "Diğer" || str == "Other") return SpaceType::Other;
    return SpaceType::Unknown;
}

bool Space::IsValid() const {
    return GetValidationErrors().empty();
}

std::vector<std::string> Space::GetValidationErrors() const {
    std::vector<std::string> errors;
    
    // Boundary kontrolü
    if (!m_boundary) {
        errors.push_back("Mahal sınırı (boundary) tanımlı değil");
    } else {
        if (m_boundary->GetVertexCount() < 3) {
            errors.push_back("Mahal sınırı en az 3 noktadan oluşmalı");
        }
        if (!m_boundary->IsClosed()) {
            errors.push_back("Mahal sınırı kapalı değil");
        }
    }
    
    // Alan kontrolü
    double area = GetArea();
    if (area < 0.5) {
        errors.push_back("Mahal alanı çok küçük (< 0.5 m²)");
    }
    if (area > 1000.0) {
        errors.push_back("Mahal alanı çok büyük (> 1000 m²) - kontrol edin");
    }
    
    // Yükseklik kontrolü
    if (m_ceilingHeight < 2.0) {
        errors.push_back("Tavan yüksekliği çok düşük (< 2.0 m)");
    }
    if (m_ceilingHeight > 6.0) {
        errors.push_back("Tavan yüksekliği çok yüksek (> 6.0 m)");
    }
    
    return errors;
}

} // namespace vkt::cad
