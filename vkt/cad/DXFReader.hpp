/**
 * @file DXFReader.hpp
 * @brief Minimal DXF Dosya Okuyucu - ASCII DXF R12/R14 Desteği
 * 
 * DXF (Drawing Exchange Format) Nedir?
 * - AutoCAD tarafından geliştirilen CAD dosya formatı
 * - ASCII (text) ve Binary olmak üzere iki format
 * - Tagged data format: (kod, değer) çiftleri
 * 
 * Bu Implementation:
 * - ASCII DXF okuma (R12, R14, R2000+)
 * - Sadece gerekli entity'ler (LWPOLYLINE, LINE, ARC, CIRCLE, TEXT)
 * - Layer filtreleme desteği
 * - Minimal dependency (standart C++ only)
 * 
 * Kullanım Senaryosu:
 * 1. Mimari proje DXF export
 * 2. DXFReader ile oku
 * 3. Layer filtrele ("DUVAR", "WALL")
 * 4. Entity'leri CAD sistemine import et
 * 5. Closed polyline'ları tespit et → Space'e dönüştür
 * 
 * DXF Format Özeti:
 * ```
 * 0
 * SECTION
 * 2
 * ENTITIES
 * 0
 * LWPOLYLINE
 * 8
 * DUVAR
 * 90
 * 4
 * 10
 * 0.0
 * 20
 * 0.0
 * ...
 * ```
 * 
 * Group Codes:
 * - 0: Entity type
 * - 8: Layer name
 * - 10,20,30: X,Y,Z coordinates
 * - 40: Radius
 * - 90: Vertex count
 */

#pragma once

#include "cad/Entity.hpp"
#include "cad/Layer.hpp"
#include "cad/LayerManager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <map>

namespace vkt::cad {

/**
 * @struct DXFCode
 * @brief DXF group code ve değer çifti
 */
struct DXFCode {
    int code;           ///< Group code (0-999)
    std::string value;  ///< Değer (string olarak)
    
    // Type conversion helpers
    double AsDouble() const;
    int AsInt() const;
    bool AsBool() const { return AsInt() != 0; }
};

/**
 * @struct DXFHeader
 * @brief DXF dosya başlık bilgileri
 */
struct DXFHeader {
    std::string version;        ///< AutoCAD version (AC1009 = R12, AC1015 = R2000)
    std::string units;          ///< Drawing units (string description)
    int insUnits = 4;           ///< $INSUNITS (4=mm, 6=m, 5=cm, 1=inch, 0=unitless)
    double ltscale = 1.0;       ///< $LTSCALE global linetype scale
    geom::Vec3 extMin;          ///< Extent minimum
    geom::Vec3 extMax;          ///< Extent maximum

    bool IsValid() const { return !version.empty(); }

    /// Returns how many mm one drawing unit equals.
    /// e.g. mm→1.0, cm→10.0, m→1000.0, inch→25.4
    double GetScaleToMM() const;

    /// Conversion factor: 1 drawing_unit² → m²
    double GetAreaToM2() const {
        double s = GetScaleToMM() / 1000.0; // mm → m
        return s * s;
    }
};

/**
 * @class DXFReader
 * @brief DXF dosya okuyucu - Minimal ASCII DXF parser
 * 
 * Desteklenen Entity'ler:
 * - LAYER (katman tanımları)
 * - LWPOLYLINE (hafif polyline - R14+)
 * - POLYLINE (klasik polyline - R12)
 * - LINE (çizgi segment)
 * - ARC (yay)
 * - CIRCLE (daire)
 * - TEXT (tek satır metin)
 * 
 * Kullanım:
 * ```cpp
 * DXFReader reader("mimari_proje.dxf");
 * if (reader.Read()) {
 *     auto layers = reader.GetLayers();
 *     auto entities = reader.GetEntities();
 *     
 *     // Layer filtrele
 *     reader.FilterByLayer({"DUVAR", "WALL"});
 *     auto wallEntities = reader.GetFilteredEntities();
 * }
 * ```
 */
class DXFReader {
public:
    /**
     * @brief DXF okuyucu oluştur
     * @param filename DXF dosya yolu
     */
    explicit DXFReader(const std::string& filename);
    ~DXFReader();
    
    /**
     * @brief DXF dosyasını oku
     * @return Başarılı ise true
     */
    bool Read();
    
    /**
     * @brief Son hata mesajı
     */
    const std::string& GetLastError() const { return m_lastError; }
    
    /**
     * @brief Dosya başlık bilgileri
     */
    const DXFHeader& GetHeader() const { return m_header; }
    
    // ==================== LAYER YÖNETİMİ ====================
    
    /**
     * @brief Okunan layer'ları döndür
     */
    const std::map<std::string, Layer>& GetLayers() const { return m_layers; }
    
    /**
     * @brief Layer isimlerini listele
     */
    std::vector<std::string> GetLayerNames() const;
    
    // ==================== ENTITY YÖNETİMİ ====================
    
    /**
     * @brief Okunan tüm entity'leri döndür
     */
    const std::vector<std::unique_ptr<Entity>>& GetEntities() const { return m_entities; }
    
    /**
     * @brief Entity sahipliğini devret (move semantics)
     */
    std::vector<std::unique_ptr<Entity>> TakeEntities() { return std::move(m_entities); }

    /**
     * @brief Entity sayısı
     */
    size_t GetEntityCount() const { return m_entities.size(); }
    
    /**
     * @brief Entity'leri tipe göre say
     */
    std::map<EntityType, size_t> GetEntityCountByType() const;
    
    // ==================== FİLTRELEME ====================
    
    /**
     * @brief Layer'a göre filtrele
     * @param layerNames Gösterilecek layer isimleri
     */
    void FilterByLayer(const std::vector<std::string>& layerNames);
    
    /**
     * @brief Entity tipine göre filtrele
     */
    void FilterByType(const std::vector<EntityType>& types);
    
    /**
     * @brief Tüm filtreleri temizle
     */
    void ClearFilters();
    
    /**
     * @brief Filtrelenmiş entity'leri döndür
     */
    std::vector<Entity*> GetFilteredEntities() const;
    
    // ==================== LAYER MANAGER ENTEGRASYONU ====================
    
    /**
     * @brief Okunan layer'ları LayerManager'a aktar
     * @param layerManager Hedef layer manager
     */
    void ImportLayersTo(LayerManager& layerManager);
    
    // ==================== İSTATİSTİKLER ====================
    
    /**
     * @brief Okuma istatistikleri
     */
    struct Statistics {
        size_t totalLines;          ///< Toplam satır sayısı
        size_t totalEntities;       ///< Toplam entity sayısı
        size_t totalLayers;         ///< Toplam layer sayısı
        size_t skippedEntities;     ///< Atlanan entity'ler (desteklenmeyen)
        double readTimeMs;          ///< Okuma süresi (ms)
        
        std::string ToString() const;
    };
    
    const Statistics& GetStatistics() const { return m_stats; }

private:
    // Dosya okuma
    std::string m_filename;
    std::ifstream m_file;
    std::string m_lastError;

    // Pushback mekanizması (entity sınırlarında pozisyon kaybını önler)
    bool m_hasPendingCode = false;
    DXFCode m_pendingCode;

    /**
     * @brief Okunan kodu geri koy (sonraki ReadCode çağrısında döndürülür)
     */
    void PushBackCode(const DXFCode& code);
    
    // DXF içerik
    DXFHeader m_header;
    std::map<std::string, Layer> m_layers;
    std::vector<std::unique_ptr<Entity>> m_entities;

    // Block tanımları: blockName → entity listesi
    std::map<std::string, std::vector<std::unique_ptr<Entity>>> m_blocks;
    
    // Filtreleme
    std::vector<std::string> m_layerFilter;
    std::vector<EntityType> m_typeFilter;
    
    // İstatistikler
    Statistics m_stats;
    
    // ==================== OKUMA FONKSİYONLARI ====================
    
    /**
     * @brief Bir group code okur (kod + değer)
     */
    bool ReadCode(DXFCode& code);
    
    /**
     * @brief HEADER section'ı oku
     */
    bool ReadHeader();
    
    /**
     * @brief TABLES section'ı oku (LAYER tanımları)
     */
    bool ReadTables();
    
    /**
     * @brief ENTITIES section'ı oku
     */
    bool ReadEntities();
    
    /**
     * @brief Tek bir entity oku
     * @param entityType Entity tipi (0 code'dan sonra gelen değer)
     */
    std::unique_ptr<Entity> ReadEntity(const std::string& entityType);
    
    /**
     * @brief LWPOLYLINE entity oku
     */
    std::unique_ptr<Entity> ReadLWPolyline();
    
    /**
     * @brief LINE entity oku
     */
    std::unique_ptr<Entity> ReadLine();
    std::unique_ptr<Entity> ReadArc();
    std::unique_ptr<Entity> ReadCircle();
    std::unique_ptr<Entity> ReadEllipse();
    std::unique_ptr<Entity> ReadSpline();
    std::unique_ptr<Entity> ReadText();
    std::unique_ptr<Entity> ReadMText();
    std::unique_ptr<Entity> ReadHatch();
    
    /**
     * @brief BLOCKS section'ı oku
     */
    bool ReadBlocks();

    /**
     * @brief INSERT entity oku (block instance)
     */
    std::unique_ptr<Entity> ReadInsert();

    /**
     * @brief Desteklenmeyen entity'yi atla
     */
    void SkipEntity();
    
    // ==================== YARDIMCI FONKSİYONLAR ====================

    /** Entity ortak özellikleri: layer, renk, lineweight, ltype_scale */
    struct EntityProps {
        std::string layer     = "0";
        Color       color     = Color::ByLayer();
        double      lineweight = -1.0;   // mm, -1=ByLayer
        double      ltScale    = 1.0;    // entity ltype scale
        bool        hasExplicitColor = false;
    };

    /** DXF group code 8/62/420/370/48 ortak okuma — entity loop'una entegre */
    bool ReadEntityProp(const DXFCode& code, EntityProps& props);

    /** EntityProps'u entity'ye uygula */
    void ApplyProps(Entity* entity, const EntityProps& props);

    /**
     * @brief Layer ismine göre Layer* al, yoksa default layer döndür
     */
    Layer* GetOrCreateLayer(const std::string& layerName);
    
    /**
     * @brief Entity filtreyi geçiyor mu?
     */
    bool PassesFilter(const Entity* entity) const;
    
    /**
     * @brief Satır sonunu trim et
     */
    static std::string Trim(const std::string& str);
};

} // namespace vkt::cad
