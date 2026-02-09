#include "cad/SelectionManager.hpp"
#include "cad/Line.hpp"
#include "cad/Layer.hpp"
#include <algorithm>

namespace vkt::cad {

SelectionManager::SelectionManager() {}

void SelectionManager::SelectEntity(EntityPtr entity, bool addToSelection) {
    if (!entity) return;
    
    if (!addToSelection) {
        ClearSelection();
    }
    
    if (!PassesFilter(entity.get())) return;
    
    size_t id = entity->GetId();
    if (m_selectedEntityIds.find(id) == m_selectedEntityIds.end()) {
        m_selectedEntityIds.insert(id);
        m_selectedEntities.push_back(entity);
        entity->SetSelected(true);
    }
    
    NotifySelectionChanged();
}

void SelectionManager::SelectEntities(const std::vector<EntityPtr>& entities, bool addToSelection) {
    if (!addToSelection) {
        ClearSelection();
    }
    
    for (const auto& entity : entities) {
        if (!entity || !PassesFilter(entity.get())) continue;
        
        size_t id = entity->GetId();
        if (m_selectedEntityIds.find(id) == m_selectedEntityIds.end()) {
            m_selectedEntityIds.insert(id);
            m_selectedEntities.push_back(entity);
            entity->SetSelected(true);
        }
    }
    
    NotifySelectionChanged();
}

void SelectionManager::DeselectEntity(EntityPtr entity) {
    if (!entity) return;
    
    size_t id = entity->GetId();
    auto it = m_selectedEntityIds.find(id);
    if (it != m_selectedEntityIds.end()) {
        m_selectedEntityIds.erase(it);
        
        auto vecIt = std::find_if(m_selectedEntities.begin(), m_selectedEntities.end(),
            [id](const EntityPtr& e) { return e->GetId() == id; });
        if (vecIt != m_selectedEntities.end()) {
            m_selectedEntities.erase(vecIt);
        }
        
        entity->SetSelected(false);
    }
    
    NotifySelectionChanged();
}

void SelectionManager::DeselectEntities(const std::vector<EntityPtr>& entities) {
    for (const auto& entity : entities) {
        if (!entity) continue;
        
        size_t id = entity->GetId();
        m_selectedEntityIds.erase(id);
        entity->SetSelected(false);
    }
    
    m_selectedEntities.erase(
        std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(),
            [this](const EntityPtr& e) {
                return m_selectedEntityIds.find(e->GetId()) == m_selectedEntityIds.end();
            }),
        m_selectedEntities.end()
    );
    
    NotifySelectionChanged();
}

void SelectionManager::ClearSelection() {
    for (auto& entity : m_selectedEntities) {
        entity->SetSelected(false);
    }
    
    m_selectedEntityIds.clear();
    m_selectedEntities.clear();
    
    NotifySelectionChanged();
}

void SelectionManager::InvertSelection(const std::vector<EntityPtr>& allEntities) {
    std::unordered_set<size_t> newSelection;
    std::vector<EntityPtr> newEntities;
    
    for (const auto& entity : allEntities) {
        if (!entity || !PassesFilter(entity.get())) continue;
        
        size_t id = entity->GetId();
        bool wasSelected = m_selectedEntityIds.find(id) != m_selectedEntityIds.end();
        
        if (!wasSelected) {
            newSelection.insert(id);
            newEntities.push_back(entity);
            entity->SetSelected(true);
        } else {
            entity->SetSelected(false);
        }
    }
    
    m_selectedEntityIds = newSelection;
    m_selectedEntities = newEntities;
    
    NotifySelectionChanged();
}

void SelectionManager::SelectAll(const std::vector<EntityPtr>& allEntities) {
    ClearSelection();
    
    for (const auto& entity : allEntities) {
        if (!entity || !PassesFilter(entity.get())) continue;
        
        size_t id = entity->GetId();
        m_selectedEntityIds.insert(id);
        m_selectedEntities.push_back(entity);
        entity->SetSelected(true);
    }
    
    NotifySelectionChanged();
}

std::vector<SelectionManager::EntityPtr> SelectionManager::SelectByRectangle(
    const geom::Vec3& corner1,
    const geom::Vec3& corner2,
    SelectionMode mode,
    const std::vector<EntityPtr>& entities
) {
    std::vector<EntityPtr> selected;
    
    BoundingBox rect;
    rect.min = geom::Vec3(
        std::min(corner1.x, corner2.x),
        std::min(corner1.y, corner2.y),
        std::min(corner1.z, corner2.z)
    );
    rect.max = geom::Vec3(
        std::max(corner1.x, corner2.x),
        std::max(corner1.y, corner2.y),
        std::max(corner1.z, corner2.z)
    );
    
    for (const auto& entity : entities) {
        if (!entity || !PassesFilter(entity.get())) continue;
        
        if (IsEntityInRectangle(entity.get(), rect, mode)) {
            selected.push_back(entity);
        }
    }
    
    return selected;
}

std::vector<SelectionManager::EntityPtr> SelectionManager::SelectByPolygon(
    const std::vector<geom::Vec3>& polygon,
    SelectionMode mode,
    const std::vector<EntityPtr>& entities
) {
    std::vector<EntityPtr> selected;
    
    for (const auto& entity : entities) {
        if (!entity || !PassesFilter(entity.get())) continue;
        
        if (IsEntityInPolygon(entity.get(), polygon, mode)) {
            selected.push_back(entity);
        }
    }
    
    return selected;
}

SelectionManager::EntityPtr SelectionManager::SelectByPoint(
    const geom::Vec3& point,
    double aperture,
    const std::vector<EntityPtr>& entities
) {
    EntityPtr closest = nullptr;
    double minDist = aperture;
    
    for (const auto& entity : entities) {
        if (!entity || !PassesFilter(entity.get())) continue;
        
        double dist = DistanceToEntity(point, entity.get());
        if (dist < minDist) {
            minDist = dist;
            closest = entity;
        }
    }
    
    return closest;
}

void SelectionManager::SetFilter(SelectionFilter filter) {
    m_filterType = filter;
}

void SelectionManager::SetTypeFilter(EntityType type) {
    m_filterType = SelectionFilter::ByType;
    m_typeFilter = {type};
}

void SelectionManager::SetTypeFilter(const std::vector<EntityType>& types) {
    m_filterType = SelectionFilter::ByType;
    m_typeFilter = types;
}

void SelectionManager::SetLayerFilter(const std::string& layerName) {
    m_filterType = SelectionFilter::ByLayer;
    m_layerFilter = {layerName};
}

void SelectionManager::SetLayerFilter(const std::vector<std::string>& layerNames) {
    m_filterType = SelectionFilter::ByLayer;
    m_layerFilter = layerNames;
}

void SelectionManager::SetCustomFilter(FilterFunc filterFunc) {
    m_filterType = SelectionFilter::Custom;
    m_customFilter = filterFunc;
}

void SelectionManager::ClearFilter() {
    m_filterType = SelectionFilter::None;
    m_typeFilter.clear();
    m_layerFilter.clear();
    m_customFilter = nullptr;
}

size_t SelectionManager::GetSelectionCount() const {
    return m_selectedEntities.size();
}

bool SelectionManager::HasSelection() const {
    return !m_selectedEntities.empty();
}

bool SelectionManager::IsSelected(const Entity* entity) const {
    if (!entity) return false;
    return m_selectedEntityIds.find(entity->GetId()) != m_selectedEntityIds.end();
}

std::vector<SelectionManager::EntityPtr> SelectionManager::GetSelectedEntities() const {
    return m_selectedEntities;
}

std::vector<size_t> SelectionManager::GetSelectedEntityIds() const {
    return std::vector<size_t>(m_selectedEntityIds.begin(), m_selectedEntityIds.end());
}

SelectionManager::EntityPtr SelectionManager::GetFirstSelected() const {
    return m_selectedEntities.empty() ? nullptr : m_selectedEntities.front();
}

SelectionManager::EntityPtr SelectionManager::GetLastSelected() const {
    return m_selectedEntities.empty() ? nullptr : m_selectedEntities.back();
}

void SelectionManager::SaveSelectionSet(const std::string& name) {
    m_selectionSets[name] = GetSelectedEntityIds();
}

void SelectionManager::LoadSelectionSet(const std::string& name) {
    auto it = m_selectionSets.find(name);
    if (it != m_selectionSets.end()) {
        m_selectedEntityIds.clear();
        m_selectedEntityIds.insert(it->second.begin(), it->second.end());
        UpdateSelectedEntitiesFlags();
        NotifySelectionChanged();
    }
}

void SelectionManager::DeleteSelectionSet(const std::string& name) {
    m_selectionSets.erase(name);
}

std::vector<std::string> SelectionManager::GetSelectionSetNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_selectionSets) {
        names.push_back(pair.first);
    }
    return names;
}

std::map<EntityType, std::vector<SelectionManager::EntityPtr>> SelectionManager::GroupSelectionByType() const {
    std::map<EntityType, std::vector<EntityPtr>> groups;
    
    for (const auto& entity : m_selectedEntities) {
        groups[entity->GetType()].push_back(entity);
    }
    
    return groups;
}

std::map<std::string, std::vector<SelectionManager::EntityPtr>> SelectionManager::GroupSelectionByLayer() const {
    std::map<std::string, std::vector<EntityPtr>> groups;
    
    for (const auto& entity : m_selectedEntities) {
        const Layer* layer = entity->GetLayer();
        std::string layerName = layer ? layer->GetName() : "0";
        groups[layerName].push_back(entity);
    }
    
    return groups;
}

bool SelectionManager::PassesFilter(const Entity* entity) const {
    if (!entity) return false;
    
    switch (m_filterType) {
        case SelectionFilter::None:
            return true;
            
        case SelectionFilter::ByType:
            return std::find(m_typeFilter.begin(), m_typeFilter.end(), entity->GetType()) != m_typeFilter.end();
            
        case SelectionFilter::ByLayer: {
            const Layer* layer = entity->GetLayer();
            if (!layer) return false;
            std::string layerName = layer->GetName();
            return std::find(m_layerFilter.begin(), m_layerFilter.end(), layerName) != m_layerFilter.end();
        }
            
        case SelectionFilter::Custom:
            return m_customFilter ? m_customFilter(entity) : true;
            
        default:
            return true;
    }
}

bool SelectionManager::IsEntityInRectangle(const Entity* entity, const BoundingBox& rect, SelectionMode mode) const {
    BoundingBox entityBounds = entity->GetBounds();
    
    if (mode == SelectionMode::Window) {
        // Entity must be completely inside
        return entityBounds.min.x >= rect.min.x && entityBounds.max.x <= rect.max.x &&
               entityBounds.min.y >= rect.min.y && entityBounds.max.y <= rect.max.y;
    } else if (mode == SelectionMode::Crossing) {
        // Entity can intersect
        return entityBounds.Intersects(rect);
    }
    
    return false;
}

bool SelectionManager::IsEntityInPolygon(const Entity* entity, const std::vector<geom::Vec3>& polygon, SelectionMode mode) const {
    BoundingBox bounds = entity->GetBounds();
    geom::Vec3 center = bounds.GetCenter();
    
    return IsPointInPolygon(center, polygon);
}

bool SelectionManager::IsPointInRectangle(const geom::Vec3& point, const BoundingBox& rect) const {
    return point.x >= rect.min.x && point.x <= rect.max.x &&
           point.y >= rect.min.y && point.y <= rect.max.y;
}

bool SelectionManager::IsPointInPolygon(const geom::Vec3& point, const std::vector<geom::Vec3>& polygon) const {
    // Ray casting algorithm
    bool inside = false;
    size_t j = polygon.size() - 1;
    
    for (size_t i = 0; i < polygon.size(); j = i++) {
        if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
            (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / 
                       (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    
    return inside;
}

double SelectionManager::DistanceToEntity(const geom::Vec3& point, const Entity* entity) const {
    if (entity->GetType() == EntityType::Line) {
        const Line* line = static_cast<const Line*>(entity);
        geom::Vec3 closest = line->GetClosestPoint(point);
        return point.DistanceTo(closest);
    }
    
    // Default: distance to bounding box center
    BoundingBox bounds = entity->GetBounds();
    return point.DistanceTo(bounds.GetCenter());
}

void SelectionManager::NotifySelectionChanged() {
    if (m_selectionChangedCallback) {
        m_selectionChangedCallback(m_selectedEntities);
    }
}

void SelectionManager::UpdateSelectedEntitiesFlags() {
    for (auto& entity : m_selectedEntities) {
        entity->SetSelected(true);
    }
}

} // namespace vkt::cad
