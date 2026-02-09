/**
 * @file Space.hpp
 * @brief Space (Mahal/Oda) Entity - Mimari Projelerde Oda/Mahal Tanımlaması
 * 
 * Space Nedir?
 * - Mimari projede bir odayı/mahal temsil eder
 * - Kapalı bir boundary (polyline) ile sınırlandırılır
 * - Alan, yükseklik, fonksiyon gibi bilgiler içerir
 * - Sıhhi tesisat projesi için fixture yerleşim referansıdır
 * 
 * Kullanım Senaryoları:
 * - DXF'den mimari projeyi import et
 * - Kapalı polyline'ları otomatik tespit et
 * - Space'lere dönüştür (mahal tanımla)
 * - Her space'e fixture'lar yerleştir (klozet, lavabo, vana)
 * - Otomatik boru routing yap (space'ler arası bağlantı)
 * 
 * FINE SANI Karşılığı:
 * - Mahal tanımlama
 * - Oda özellikleri (isim, tip, alan)
 * - Tesisat gereksinim tanımı
 */

#pragma once

#include "Entity.hpp"
#include "Polyline.hpp"
#include <string>
#include <memory>

namespace vkt::cad {

/**
 * @enum SpaceType
 * @brief Mahal tipi - Fonksiyonel kullanım amacı
 */
enum class SpaceType {
    Unknown = 0,
    
    // Yaşam Alanları
    LivingRoom,         ///< Oturma odası / Salon
    Bedroom,            ///< Yatak odası
    Kitchen,            ///< Mutfak
    DiningRoom,         ///< Yemek odası
    StudyRoom,          ///< Çalışma odası
    
    // Islak Hacimler (Tesisat Kritik)
    Bathroom,           ///< Banyo (tam donanımlı)
    WC,                 ///< Tuvalet (sadece klozet + lavabo)
    LaundryRoom,        ///< Çamaşır odası
    
    // Servis Alanları
    Corridor,           ///< Koridor
    Balcony,            ///< Balkon
    Terrace,            ///< Teras
    Storage,            ///< Depo
    
    // Teknik Alanlar
    MechanicalRoom,     ///< Makine dairesi
    ElectricalRoom,     ///< Elektrik odası
    BoilerRoom,         ///< Kazan dairesi
    
    // Ticari/Ofis
    Office,             ///< Ofis
    MeetingRoom,        ///< Toplantı odası
    Reception,          ///< Resepsiyon
    
    // Diğer
    Garage,             ///< Garaj
    Garden,             ///< Bahçe
    Void,               ///< Boşluk (merdiven boşluğu, havalandırma)
    Other               ///< Diğer
};

/**
 * @struct SpaceRequirements
 * @brief Mahal tesisat gereksinimleri - Hangi sistemler gerekli?
 */
struct SpaceRequirements {
    // Su sistemleri
    bool needsColdWater = false;    ///< Soğuk su gerekli mi?
    bool needsHotWater = false;     ///< Sıcak su gerekli mi?
    bool needsDrain = false;        ///< Atık su/kanalizasyon gerekli mi?
    
    // HVAC
    bool needsHeating = false;      ///< Isıtma gerekli mi?
    bool needsCooling = false;      ///< Soğutma gerekli mi?
    bool needsVentilation = false;  ///< Havalandırma gerekli mi?
    
    // Diğer
    bool needsFireSuppression = false; ///< Yangın söndürme sistemi
    bool needsGas = false;          ///< Doğalgaz gerekli mi?
    
    /**
     * @brief Mahal tipine göre varsayılan gereksinimleri ayarla
     */
    static SpaceRequirements GetDefaultForType(SpaceType type);
};

/**
 * @class Space
 * @brief Space (Mahal/Oda) entity - Kapalı alan tanımı
 * 
 * Mimari Bilgiler:
 * - Boundary (sınır polyline)
 * - Alan (m²)
 * - Yükseklik (m)
 * - Döşeme kotu
 * 
 * Fonksiyonel Bilgiler:
 * - İsim/numara
 * - Tip (banyo, mutfak, yatak odası...)
 * - Tesisat gereksinimleri
 * 
 * İlişkiler:
 * - Fixtures (bu mahaldeki armatürler)
 * - Adjacent spaces (komşu mahaller)
 * - Pipe connections (boru bağlantıları)
 */
class Space : public Entity {
public:
    /**
     * @brief Space oluştur (boş)
     */
    Space();
    
    /**
     * @brief Space oluştur (boundary ile)
     * @param boundary Mahal sınırı (kapalı polyline)
     * @param name Mahal ismi
     * @param type Mahal tipi
     */
    Space(std::unique_ptr<Polyline> boundary, const std::string& name = "", SpaceType type = SpaceType::Unknown);
    
    ~Space() override = default;
    
    // ==================== Entity Interface ====================
    
    EntityType GetType() const override { return EntityType::Space; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    
    // ==================== GEOMETRIK BİLGİLER ====================
    
    /**
     * @brief Mahal sınırı (boundary polyline)
     */
    const Polyline* GetBoundary() const { return m_boundary.get(); }
    Polyline* GetBoundary() { return m_boundary.get(); }
    void SetBoundary(std::unique_ptr<Polyline> boundary);
    
    /**
     * @brief Mahal alanını hesapla (m²)
     * @return Alan değeri, boundary yoksa 0
     */
    double GetArea() const;
    
    /**
     * @brief Mahal çevresini hesapla (m)
     */
    double GetPerimeter() const;
    
    /**
     * @brief Mahal hacmini hesapla (m³)
     */
    double GetVolume() const;
    
    /**
     * @brief Bir noktanın mahal içinde olup olmadığını kontrol eder
     */
    bool ContainsPoint(const geom::Vec3& point) const;
    
    /**
     * @brief Mahal merkezini döndürür (centroid)
     */
    geom::Vec3 GetCenter() const;
    
    // ==================== MAHAL BİLGİLERİ ====================
    
    /**
     * @brief Mahal ismi (ör: "Yatak Odası 1", "Ana Banyo")
     */
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; SetFlags(GetFlags() | EF_Modified); }
    
    /**
     * @brief Mahal numarası (ör: "101", "A-12")
     */
    const std::string& GetNumber() const { return m_number; }
    void SetNumber(const std::string& number) { m_number = number; SetFlags(GetFlags() | EF_Modified); }
    
    /**
     * @brief Mahal tipi
     */
    SpaceType GetSpaceType() const { return m_spaceType; }
    void SetSpaceType(SpaceType type);
    
    /**
     * @brief Kat yüksekliği (m)
     */
    double GetHeight() const { return m_height; }
    void SetHeight(double height) { m_height = height; SetFlags(GetFlags() | EF_Modified); }
    
    /**
     * @brief Döşeme kotu (m) - Sıfır kat döşemesinden yükseklik
     */
    double GetFloorLevel() const { return m_floorLevel; }
    void SetFloorLevel(double level) { m_floorLevel = level; SetFlags(GetFlags() | EF_Modified); }
    
    /**
     * @brief Tavan yüksekliği (m) - Döşemeden net yükseklik
     */
    double GetCeilingHeight() const { return m_ceilingHeight; }
    void SetCeilingHeight(double height) { m_ceilingHeight = height; SetFlags(GetFlags() | EF_Modified); }
    
    // ==================== TESİSAT GEREKSİNİMLERİ ====================
    
    /**
     * @brief Tesisat gereksinimleri
     */
    const SpaceRequirements& GetRequirements() const { return m_requirements; }
    SpaceRequirements& GetRequirements() { return m_requirements; }
    void SetRequirements(const SpaceRequirements& req) { m_requirements = req; SetFlags(GetFlags() | EF_Modified); }
    
    // ==================== FIXTURE YÖNETİMİ ====================
    
    /**
     * @brief Bu mahaldeki fixture'ları listele
     */
    const std::vector<size_t>& GetFixtureIds() const { return m_fixtureIds; }
    
    /**
     * @brief Fixture ekle
     */
    void AddFixture(size_t fixtureEntityId);
    
    /**
     * @brief Fixture çıkar
     */
    void RemoveFixture(size_t fixtureEntityId);
    
    /**
     * @brief Tüm fixture'ları temizle
     */
    void ClearFixtures();
    
    /**
     * @brief Fixture sayısı
     */
    size_t GetFixtureCount() const { return m_fixtureIds.size(); }
    
    // ==================== KOMŞULUK İLİŞKİLERİ ====================
    
    /**
     * @brief Komşu mahal ID'leri (duvar paylaşanlar)
     */
    const std::vector<size_t>& GetAdjacentSpaceIds() const { return m_adjacentSpaceIds; }
    
    /**
     * @brief Komşu mahal ekle
     */
    void AddAdjacentSpace(size_t spaceId);
    
    /**
     * @brief İki space'in komşu olup olmadığını test et
     */
    static bool AreAdjacent(const Space& space1, const Space& space2, double tolerance = 0.01);
    
    // ==================== YARDIMCI FONKSİYONLAR ====================
    
    /**
     * @brief SpaceType'ı string'e çevir
     */
    static std::string SpaceTypeToString(SpaceType type);
    
    /**
     * @brief String'i SpaceType'a çevir
     */
    static SpaceType StringToSpaceType(const std::string& str);
    
    /**
     * @brief Mahal validasyonu - Geçerli bir space mi?
     */
    bool IsValid() const;
    
    /**
     * @brief Validation hata mesajları
     */
    std::vector<std::string> GetValidationErrors() const;

private:
    // Geometri
    std::unique_ptr<Polyline> m_boundary;   ///< Mahal sınırı (kapalı polyline)
    
    // Kimlik bilgileri
    std::string m_name;                     ///< Mahal ismi
    std::string m_number;                   ///< Mahal numarası
    SpaceType m_spaceType;                  ///< Mahal tipi
    
    // Boyutsal bilgiler
    double m_height;                        ///< Kat yüksekliği (m)
    double m_floorLevel;                    ///< Döşeme kotu (m)
    double m_ceilingHeight;                 ///< Tavan yüksekliği (m)
    
    // Tesisat gereksinimleri
    SpaceRequirements m_requirements;
    
    // İlişkiler (ID listesi - gerçek entity'ler Document'te)
    std::vector<size_t> m_fixtureIds;       ///< Bu mahaldeki fixture entity ID'leri
    std::vector<size_t> m_adjacentSpaceIds; ///< Komşu mahal ID'leri
};

} // namespace vkt::cad
