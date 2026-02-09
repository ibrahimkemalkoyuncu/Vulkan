#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <vector>
#include <unordered_set>
#include <memory>
#include <functional>

namespace vkt::cad {

/**
 * @brief Selection mode
 */
enum class SelectionMode {
    Pick,      ///< Tek entity seçimi (click)
    Window,    ///< Window selection (soldan sağa, tamamen içerde olanlar)
    Crossing,  ///< Crossing selection (sağdan sola, kesişen her şey)
    Fence,     ///< Fence selection (polyline ile kesişenler)
    All        ///< Tümünü seç
};

/**
 * @brief Selection filtreleme türü
 */
enum class SelectionFilter {
    None,           ///< Filtre yok
    ByType,         ///< Entity tipine göre
    ByLayer,        ///< Layer'a göre
    ByColor,        ///< Renge göre
    ByLineType,     ///< Çizgi tipine göre
    Custom          ///< Özel filtre fonksiyonu
};

/**
 * @brief Selection Manager - Entity seçim sistemini yönetir
 * 
 * AutoCAD benzeri seçim mekanizması:
 * - Pick selection (tek entity)
 * - Window selection (soldan sağa)
 * - Crossing selection (sağdan sola)
 * - Fence selection (polyline ile)
 * - Selection filtreleme
 * - Selection set yönetimi
 */
class SelectionManager {
public:
    using EntityPtr = std::shared_ptr<Entity>;
    using FilterFunc = std::function<bool(const Entity*)>;
    
    SelectionManager();
    ~SelectionManager() = default;
    
    // Selection işlemleri
    /**
     * @brief Tek entity seçer (pick mode)
     * @param entity Seçilecek entity
     * @param addToSelection true: Mevcut seçime ekle, false: Yeni seçim
     */
    void SelectEntity(EntityPtr entity, bool addToSelection = false);
    
    /**
     * @brief Birden fazla entity seçer
     */
    void SelectEntities(const std::vector<EntityPtr>& entities, bool addToSelection = false);
    
    /**
     * @brief Entity seçimini kaldırır
     */
    void DeselectEntity(EntityPtr entity);
    
    /**
     * @brief Birden fazla entity seçimini kaldırır
     */
    void DeselectEntities(const std::vector<EntityPtr>& entities);
    
    /**
     * @brief Tüm seçimi temizler
     */
    void ClearSelection();
    
    /**
     * @brief Seçimi tersine çevirir
     */
    void InvertSelection(const std::vector<EntityPtr>& allEntities);
    
    /**
     * @brief Tüm entity'leri seçer
     */
    void SelectAll(const std::vector<EntityPtr>& allEntities);
    
    // Window/Crossing selection
    /**
     * @brief Dikdörtgen alan seçimi
     * @param corner1 İlk köşe
     * @param corner2 İkinci köşe
     * @param mode Window veya Crossing mode
     * @param entities Test edilecek entity listesi
     * @return Seçilen entity'ler
     */
    std::vector<EntityPtr> SelectByRectangle(
        const geom::Vec3& corner1,
        const geom::Vec3& corner2,
        SelectionMode mode,
        const std::vector<EntityPtr>& entities
    );
    
    /**
     * @brief Polygon alan seçimi (fence/crossing polygon)
     * @param polygon Polygon köşe noktaları
     * @param mode Crossing veya Fence
     * @param entities Test edilecek entity listesi
     * @return Seçilen entity'ler
     */
    std::vector<EntityPtr> SelectByPolygon(
        const std::vector<geom::Vec3>& polygon,
        SelectionMode mode,
        const std::vector<EntityPtr>& entities
    );
    
    /**
     * @brief Noktaya yakın entity seçimi (pick aperture)
     * @param point Seçim noktası
     * @param aperture Seçim aperture boyutu (pixel cinsinden)
     * @param entities Test edilecek entity listesi
     * @return En yakın entity (veya nullptr)
     */
    EntityPtr SelectByPoint(
        const geom::Vec3& point,
        double aperture,
        const std::vector<EntityPtr>& entities
    );
    
    // Filtreleme
    /**
     * @brief Selection filter ayarlar
     */
    void SetFilter(SelectionFilter filter);
    
    /**
     * @brief Entity tipine göre filter ayarlar
     */
    void SetTypeFilter(EntityType type);
    void SetTypeFilter(const std::vector<EntityType>& types);
    
    /**
     * @brief Layer'a göre filter ayarlar
     */
    void SetLayerFilter(const std::string& layerName);
    void SetLayerFilter(const std::vector<std::string>& layerNames);
    
    /**
     * @brief Özel filter fonksiyonu ayarlar
     */
    void SetCustomFilter(FilterFunc filterFunc);
    
    /**
     * @brief Filtreyi temizler
     */
    void ClearFilter();
    
    // Query
    /**
     * @brief Seçili entity sayısı
     */
    size_t GetSelectionCount() const;
    
    /**
     * @brief Herhangi bir entity seçili mi?
     */
    bool HasSelection() const;
    
    /**
     * @brief Entity seçili mi?
     */
    bool IsSelected(const Entity* entity) const;
    
    /**
     * @brief Seçili entity'lerin listesini döndürür
     */
    std::vector<EntityPtr> GetSelectedEntities() const;
    
    /**
     * @brief Seçili entity'lerin ID listesini döndürür
     */
    std::vector<size_t> GetSelectedEntityIds() const;
    
    /**
     * @brief İlk seçili entity'yi döndürür
     */
    EntityPtr GetFirstSelected() const;
    
    /**
     * @brief Son seçili entity'yi döndürür
     */
    EntityPtr GetLastSelected() const;
    
    // Selection set yönetimi
    /**
     * @brief Mevcut seçimi named set olarak kaydeder
     */
    void SaveSelectionSet(const std::string& name);
    
    /**
     * @brief Kayıtlı selection set'i yükler
     */
    void LoadSelectionSet(const std::string& name);
    
    /**
     * @brief Selection set'i siler
     */
    void DeleteSelectionSet(const std::string& name);
    
    /**
     * @brief Tüm selection set'lerin listesini döndürür
     */
    std::vector<std::string> GetSelectionSetNames() const;
    
    // Gruplama
    /**
     * @brief Seçili entity'leri tipine göre gruplar
     */
    std::map<EntityType, std::vector<EntityPtr>> GroupSelectionByType() const;
    
    /**
     * @brief Seçili entity'leri layer'a göre gruplar
     */
    std::map<std::string, std::vector<EntityPtr>> GroupSelectionByLayer() const;
    
    // Events (callback pattern)
    using SelectionChangedCallback = std::function<void(const std::vector<EntityPtr>&)>;
    
    void SetSelectionChangedCallback(SelectionChangedCallback callback) {
        m_selectionChangedCallback = callback;
    }
    
private:
    std::unordered_set<size_t> m_selectedEntityIds;  // Seçili entity ID'leri
    std::vector<EntityPtr> m_selectedEntities;        // Seçili entity'lerin referansları
    
    // Filtreleme
    SelectionFilter m_filterType = SelectionFilter::None;
    std::vector<EntityType> m_typeFilter;
    std::vector<std::string> m_layerFilter;
    FilterFunc m_customFilter;
    
    // Named selection sets
    std::map<std::string, std::vector<size_t>> m_selectionSets;
    
    // Callback
    SelectionChangedCallback m_selectionChangedCallback;
    
    // Helper fonksiyonlar
    bool PassesFilter(const Entity* entity) const;
    bool IsEntityInRectangle(const Entity* entity, const BoundingBox& rect, SelectionMode mode) const;
    bool IsEntityInPolygon(const Entity* entity, const std::vector<geom::Vec3>& polygon, SelectionMode mode) const;
    bool IsPointInRectangle(const geom::Vec3& point, const BoundingBox& rect) const;
    bool IsPointInPolygon(const geom::Vec3& point, const std::vector<geom::Vec3>& polygon) const;
    double DistanceToEntity(const geom::Vec3& point, const Entity* entity) const;
    
    void NotifySelectionChanged();
    void UpdateSelectedEntitiesFlags();
};

} // namespace vkt::cad
