/**
 * @file SpaceManager.hpp
 * @brief Mahal (Space) yönetim sistemi
 * 
 * SpaceManager Görevleri:
 * 1. CRUD operasyonları (Create, Read, Update, Delete)
 * 2. DXF'ten otomatik mahal tespiti
 * 3. Mahal boundary validation
 * 4. Mahal-fixture ilişkileri
 * 5. Komşuluk tespiti
 * 
 * Otomatik Tespit Stratejisi:
 * 1. DXF'ten kapalı polyline'ları bul
 * 2. Layer filtresi uygula ("DUVAR", "WALL", "Mimari-Duvar")
 * 3. Alan validasyonu (min 2m², max 1000m²)
 * 4. TEXT entity'lerden mahal isimlerini eşleştir
 * 5. Kullanıcıya onay için sun
 */

#pragma once

#include "Space.hpp"
#include "DXFReader.hpp"
#include "LayerManager.hpp"
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace vkt::cad {

/**
 * @struct SpaceCandidate
 * @brief Tespit edilen mahal adayı
 */
struct SpaceCandidate {
    Polyline* boundary = nullptr;           ///< Sınır polyline
    std::string suggestedName;              ///< Tespit edilen isim (TEXT entity'den)
    SpaceType suggestedType = SpaceType::Unknown; ///< Tahmin edilen tip
    double area = 0.0;                      ///< Alan (m²)
    geom::Vec3 center;                      ///< Merkez nokta
    
    bool isValid = true;                    ///< Geçerlilik durumu
    std::vector<std::string> warnings;      ///< Uyarılar
    
    /**
     * @brief Mahal tipini isme göre tahmin et
     * Örnek: "WC" içeriyorsa SpaceType::WC
     */
    void InferTypeFromName();
};

/**
 * @struct SpaceDetectionOptions
 * @brief Otomatik tespit ayarları
 */
struct SpaceDetectionOptions {
    std::vector<std::string> wallLayerNames = {"DUVAR", "WALL", "Mimari-Duvar", "A-WALL"}; ///< Duvar layer'ları
    double minArea = 2.0;                   ///< Minimum alan (m²)
    double maxArea = 1000.0;                ///< Maximum alan (m²)
    bool detectNamesFromText = true;        ///< TEXT entity'lerden isim tespiti
    double textSearchRadius = 500.0;        ///< TEXT arama yarıçapı (mm)
    bool requireClosedPolylines = true;     ///< Sadece kapalı polyline'lar
    bool autoInferTypes = true;             ///< Otomatik tip tahmini
};

/**
 * @class SpaceManager
 * @brief Mahal yönetim sistemi
 * 
 * Kullanım:
 * ```cpp
 * SpaceManager spaceMgr;
 * 
 * // DXF'ten otomatik tespit
 * DXFReader reader("mimariprojesi.dxf");
 * reader.Read();
 * auto candidates = spaceMgr.DetectSpacesFromDXF(reader);
 * 
 * // Kullanıcı onayı sonrası oluştur
 * for (auto& candidate : candidates) {
 *     auto space = spaceMgr.CreateSpace(candidate.boundary, candidate.suggestedName);
 *     space->SetSpaceType(candidate.suggestedType);
 * }
 * ```
 */
class SpaceManager {
public:
    SpaceManager();
    ~SpaceManager();
    
    // ==================== CRUD OPERASYONLARI ====================
    
    /**
     * @brief Yeni mahal oluştur
     * @param boundary Sınır polyline (ownership kopyalanır)
     * @param name Mahal ismi
     * @return Oluşturulan mahal pointer'ı
     */
    Space* CreateSpace(const Polyline* boundary, const std::string& name = "");
    
    /**
     * @brief Mahal sil
     * @param spaceId Silinecek mahal ID'si
     * @return Başarılı ise true
     */
    bool DeleteSpace(EntityId spaceId);
    
    /**
     * @brief Mahal bul
     * @param spaceId Mahal ID'si
     * @return Mahal pointer'ı veya nullptr
     */
    Space* GetSpace(EntityId spaceId);
    const Space* GetSpace(EntityId spaceId) const;
    
    /**
     * @brief Tüm mahalleri getir
     */
    std::vector<Space*> GetAllSpaces();
    std::vector<const Space*> GetAllSpaces() const;
    
    /**
     * @brief Mahal sayısı
     */
    size_t GetSpaceCount() const { return m_spaces.size(); }
    
    /**
     * @brief Tüm mahalleri temizle
     */
    void Clear();
    
    // ==================== OTOMATİK TESPİT ====================
    
    /**
     * @brief DXF'ten mahal adaylarını tespit et
     * @param reader DXF dosyası okuyucu
     * @param options Tespit ayarları
     * @return Mahal adayları listesi
     */
    std::vector<SpaceCandidate> DetectSpacesFromDXF(
        const DXFReader& reader,
        const SpaceDetectionOptions& options = SpaceDetectionOptions{}
    );
    
    /**
     * @brief Entity listesinden mahal adaylarını tespit et
     * @param entities Entity listesi
     * @param options Tespit ayarları
     * @return Mahal adayları listesi
     */
    std::vector<SpaceCandidate> DetectSpacesFromEntities(
        const std::vector<Entity*>& entities,
        const SpaceDetectionOptions& options = SpaceDetectionOptions{}
    );
    
    /**
     * @brief Mahal adayını onaylayıp Space nesnesi oluştur
     * @param candidate Onaylanan aday
     * @return Oluşturulan mahal
     */
    Space* AcceptCandidate(const SpaceCandidate& candidate);
    
    // ==================== SORGU OPERASYONLARI ====================
    
    /**
     * @brief Tipe göre mahalleri filtrele
     * @param type Mahal tipi
     * @return Filtrelenmiş liste
     */
    std::vector<Space*> GetSpacesByType(SpaceType type);
    
    /**
     * @brief İsme göre mahal ara (kısmi eşleşme)
     * @param name Aranacak isim
     * @return Bulunan mahaller
     */
    std::vector<Space*> FindSpacesByName(const std::string& name);
    
    /**
     * @brief Nokta hangi mahalde?
     * @param point Test edilecek nokta
     * @return Mahal pointer'ı veya nullptr
     */
    Space* FindSpaceAtPoint(const geom::Vec3& point);
    
    /**
     * @brief İki mahal komşu mu?
     * @param spaceId1 Birinci mahal
     * @param spaceId2 İkinci mahal
     * @param tolerance Tolerans (mm)
     * @return Komşu ise true
     */
    bool AreAdjacent(EntityId spaceId1, EntityId spaceId2, double tolerance = 10.0) const;
    
    /**
     * @brief Tüm komşulukları tespit et ve kaydet
     * @param tolerance Tolerans (mm)
     */
    void DetectAllAdjacencies(double tolerance = 10.0);
    
    // ==================== VALİDASYON ====================
    
    /**
     * @brief Tüm mahalleri validate et
     * @return Geçersiz mahal sayısı
     */
    size_t ValidateAll();
    
    /**
     * @brief Geçersiz mahalleri getir
     * @return Geçersiz mahaller
     */
    std::vector<Space*> GetInvalidSpaces();
    
    // ==================== İSTATİSTİKLER ====================
    
    /**
     * @brief İstatistik bilgileri
     */
    struct Statistics {
        size_t totalSpaces = 0;
        size_t validSpaces = 0;
        size_t invalidSpaces = 0;
        double totalArea = 0.0;             ///< Toplam alan (m²)
        double totalVolume = 0.0;           ///< Toplam hacim (m³)
        std::map<SpaceType, size_t> countsByType; ///< Tip başına sayı
        
        std::string ToString() const;
    };
    
    /**
     * @brief İstatistikleri hesapla
     */
    Statistics GetStatistics() const;
    
    // ==================== SERİALİZASYON ====================
    
    /**
     * @brief JSON olarak serialize et
     * @return JSON string
     */
    std::string Serialize() const;
    
    /**
     * @brief JSON'dan deserialize et
     * @param json JSON string
     * @return Başarılı ise true
     */
    bool Deserialize(const std::string& json);

private:
    std::map<EntityId, std::unique_ptr<Space>> m_spaces; ///< Mahal koleksiyonu
    
    /**
     * @brief Kapalı polyline'ları filtrele
     */
    std::vector<Polyline*> FilterClosedPolylines(const std::vector<Entity*>& entities);
    
    /**
     * @brief Layer'a göre filtrele
     */
    bool MatchesLayerFilter(const Entity* entity, const std::vector<std::string>& layerNames) const;
    
    /**
     * @brief Alan validasyonu
     */
    bool IsValidArea(double area, double minArea, double maxArea) const;
    
    /**
     * @brief TEXT entity'den mahal ismi tespiti
     * @param center Mahal merkezi
     * @param entities Tüm entity'ler
     * @param searchRadius Arama yarıçapı
     * @return Bulunan isim (boş ise bulunamadı)
     */
    std::string DetectNameFromText(const geom::Vec3& center, 
                                    const std::vector<Entity*>& entities,
                                    double searchRadius) const;
    
    /**
     * @brief Benzersiz mahal ismi oluştur
     * @param baseName Temel isim
     * @return Benzersiz isim (örn: "Yatak Odası 1", "Yatak Odası 2")
     */
    std::string GenerateUniqueName(const std::string& baseName);
    
    /**
     * @brief Mahal numarası üret
     * @param type Mahal tipi
     * @return Numara (örn: "101", "102")
     */
    std::string GenerateSpaceNumber(SpaceType type);
};

} // namespace vkt::cad
