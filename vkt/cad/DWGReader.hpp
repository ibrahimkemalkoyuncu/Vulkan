/**
 * @file DWGReader.hpp
 * @brief DWG Dosya Okuyucu - LibreDWG Kullanarak
 * 
 * DWG (Drawing) Nedir?
 * - AutoCAD'in native binary dosya formatı
 * - DXF'ten daha kompakt ve hızlı
 * - R13, R14, R2000, R2004, R2007, R2010, R2013, R2018, R2021 versiyonları
 * 
 * Bu Implementation:
 * - LibreDWG kütüphanesi kullanır
 * - DXFReader ile aynı API'yi paylaşır
 * - Entity'leri CAD sistemine dönüştürür
 * - Layer filtreleme desteği
 * 
 * Kullanım:
 * ```cpp
 * DWGReader reader;
 * if (reader.Read("mimari_proje.dwg")) {
 *     auto entities = reader.GetEntities();
 *     auto stats = reader.GetStatistics();
 * }
 * ```
 * 
 * Desteklenen Entity'ler:
 * - LWPOLYLINE / POLYLINE
 * - LINE
 * - ARC
 * - CIRCLE
 * - TEXT (gelecekte)
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Entity.hpp"
#include "Layer.hpp"

namespace vkt {
namespace cad {

/**
 * @struct DWGStatistics
 * @brief DWG okuma istatistikleri
 */
struct DWGStatistics {
    size_t entityCount = 0;
    size_t layerCount = 0;
    size_t lineCount = 0;
    size_t polylineCount = 0;
    size_t arcCount = 0;
    size_t circleCount = 0;
    size_t ellipseCount = 0;
    size_t splineCount = 0;
    size_t textCount = 0;
    size_t mtextCount = 0;
    size_t hatchCount = 0;
    size_t insertExpanded = 0;
    double readTimeMs = 0.0;
    std::string dwgVersion;
    
    std::string ToString() const;
};

/**
 * @class DWGReader
 * @brief DWG dosya okuyucu (LibreDWG wrapper)
 * 
 * Thread-safe değil. Her thread kendi reader'ını kullanmalı.
 */
class DWGReader {
public:
    DWGReader() = default;
    ~DWGReader();
    
    // Non-copyable
    DWGReader(const DWGReader&) = delete;
    DWGReader& operator=(const DWGReader&) = delete;
    
    /**
     * @brief DWG dosyasını oku
     * 
     * @param filepath DWG dosya yolu
     * @return Başarılı ise true
     */
    bool Read(const std::string& filepath);
    
    /**
     * @brief Okunan entity'leri getir
     * 
     * @return Entity pointer'ları (caller sorumlu değil, Reader yönetir)
     */
    std::vector<Entity*> GetEntities() const;
    
    /**
     * @brief Belirli layer'daki entity'leri getir
     */
    std::vector<Entity*> GetEntitiesOnLayer(const std::string& layerName) const;
    
    /**
     * @brief Layer filtreleme ayarla
     * 
     * @param layers Sadece bu layer'lar okunur (boşsa hepsi)
     */
    void SetLayerFilter(const std::unordered_set<std::string>& layers);
    
    /**
     * @brief Okuma istatistikleri
     */
    const DWGStatistics& GetStatistics() const { return m_stats; }
    
    /**
     * @brief Layer bilgilerini getir
     */
    std::unordered_map<std::string, Layer> GetLayers() const;
    
    /**
     * @brief Hata mesajı (Read() false dönerse)
     */
    const std::string& GetError() const { return m_errorMessage; }
    
    /**
     * @brief Entity sahipliğini devret (move semantics)
     */
    std::vector<std::unique_ptr<Entity>> TakeEntities();

    /**
     * @brief Bellek temizle
     */
    void Clear();

    /** @brief Yüklenemeyen xref dosya yolları (Read() sonrası kontrol et) */
    const std::vector<std::string>& GetMissingXrefs() const { return m_missingXrefs; }

    /** @brief Xref çözümlemesi için ek arama dizinleri (Read() öncesi set et) */
    void AddXrefSearchPath(const std::string& dir) { m_xrefSearchPaths.push_back(dir); }
    void ClearXrefSearchPaths()                    { m_xrefSearchPaths.clear(); }

private:
    // Entity parsing helpers
    bool ParseEntities(void* dwg_data);
    Entity* ParseEntityByType(void* obj_ptr);
    void ExpandInsert(void* obj_ptr, void* dwg_ptr,
                       std::vector<struct InsertTransform>& transformChain, int depth);
    Entity* ParseLine(void* ent);
    Entity* ParsePolyline(void* ent);
    Entity* ParseLWPolyline(void* ent);
    Entity* ParseArc(void* ent);
    Entity* ParseCircle(void* ent);
    Entity* ParseText(void* ent);
    Entity* ParseMText(void* ent);
    Entity* ParseHatch(void* ent);
    Entity* ParseEllipse(void* ent);
    Entity* ParseSpline(void* ent);
    Entity* ParsePoint(void* ent);
    void ExpandMInsert(void* obj_ptr, void* dwg_ptr);
    void ExpandMInsertNested(void* obj_ptr, void* dwg_ptr, void* parentChain, int depth);
    
    // Layer ve renk management
    void ExtractLayers(void* dwg_data);
    void ExtractEntityColorAndLayer(void* obj_ptr, Entity* entity);
    bool PassesLayerFilter(const std::string& layerName) const;
    
    // Xref resolution: load external DWG referenced by INSERT
    void ExpandXref(const std::string& xrefPath,
                    std::vector<struct InsertTransform>& transformChain, int depth);

    // Data
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::unordered_map<std::string, Layer> m_layers;
    std::unordered_set<std::string> m_layerFilter;
    DWGStatistics m_stats;
    std::string m_errorMessage;
    std::string m_filePath; ///< Path to the current DWG file (for xref resolution)

    // Xref state (reset on each Read() call)
    std::unordered_set<std::string> m_visitedXrefs;  ///< Circular ref koruması
    std::vector<std::string>        m_missingXrefs;   ///< Bulunamayan xref dosyaları
    std::vector<std::string>        m_xrefSearchPaths; ///< Kullanıcı tanımlı ek arama dizinleri
    
    // LibreDWG data handle (opaque pointer)
    void* m_dwgData = nullptr;
};

} // namespace cad
} // namespace vkt
