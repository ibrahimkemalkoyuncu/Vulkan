/**
 * @file Entity.hpp
 * @brief CAD Entity Temel Sınıfı - Tüm çizilebilir nesnelerin ata sınıfı
 * 
 * Bu dosya CAD çekirdeğinin en temel yapısını oluşturur. AutoCAD, BricsCAD gibi
 * profesyonel CAD yazılımlarındaki entity kavramını içerir.
 * 
 * Entity Nedir?
 * - Çizimde görünen her obje bir entity'dir (çizgi, daire, boru, armatür)
 * - Her entity bir layer'a aittir
 * - Seçilebilir, taşınabilir, kopyalanabilir, silinebilir
 * - Serialize edilebilir (kayıt/yükleme)
 * 
 * Kullanım Alanları:
 * - Mimari çizim elemanları (duvar, pencere, kapı)
 * - MEP elemanları (boru, armatür, vana)
 * - Genel CAD primitifleri (çizgi, çember, yay)
 */

#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "geom/Math.hpp"
#include "nlohmann/json.hpp"

namespace vkt {

// Forward declarations
namespace cad { class Layer; }

namespace cad {

// Entity ID tipi (benzersiz tanımlayıcı)
using EntityId = uint64_t;

/**
 * @enum EntityType
 * @brief Entity türleri - Nesnenin geometrik tipini belirtir
 */
enum class EntityType {
    Unknown = 0,
    
    // Temel CAD Primitifleri
    Point,              ///< Nokta
    Line,               ///< Çizgi segment
    Polyline,           ///< Çoklu çizgi (açık veya kapalı)
    Arc,                ///< Yay (daire parçası)
    Circle,             ///< Tam daire
    Ellipse,            ///< Elips
    Spline,             ///< Eğri (NURBS)
    
    // Metin ve Boyutlandırma
    Text,               ///< Tek satır metin
    MText,              ///< Çok satırlı metin (formatted)
    Dimension,          ///< Ölçü çizgisi
    Leader,             ///< İşaret çizgisi
    
    // MEP Özel Entity'ler
    PipeSegment,        ///< Boru segmenti (Line + çap bilgisi)
    Fitting,            ///< Bağlantı parçası (dirsek, T, redüksiyon)
    Fixture,            ///< Armatür (lavabo, klozet, vana)
    Equipment,          ///< Ekipman (pompa, tank, boiler)
    
    // Mimari Elemanlar
    Wall,               ///< Duvar (kalınlık bilgili polyline)
    Door,               ///< Kapı
    Window,             ///< Pencere
    Space,              ///< Mahal/Oda (kapalı poligon + bilgi)
    
    // Blok ve Referanslar
    Block,              ///< Blok tanımı
    Insert,             ///< Blok örneği (instance)
    
    // Yardımcı
    Hatch,              ///< Tarama/dolgu deseni
    Image,              ///< Raster görüntü (underlay)
    
    Count               ///< Toplam tip sayısı (enum guard)
};

/**
 * @enum EntityFlags
 * @brief Entity durum bayrakları (bit flags)
 */
enum EntityFlags : uint32_t {
    EF_None         = 0,
    EF_Selected     = 1 << 0,   ///< Seçili mi?
    EF_Visible      = 1 << 1,   ///< Görünür mü?
    EF_Locked       = 1 << 2,   ///< Kilitli mi? (düzenlenemez)
    EF_Highlighted  = 1 << 3,   ///< Vurgulanmış mı? (hover)
    EF_Modified     = 1 << 4,   ///< Değiştirilmiş mi? (kayıt gerekli)
    EF_Temporary    = 1 << 5,   ///< Geçici mi? (rubber band, preview)
    
    EF_Default      = EF_Visible ///< Varsayılan durum
};

/**
 * @struct BoundingBox
 * @brief Axis-Aligned Bounding Box - Entity'nin kapladığı alan
 * 
 * Kullanım:
 * - Hızlı çarpışma testi (seçim, zoom extents)
 * - Viewport culling (görünür entity filtreleme)
 * - Spatial indexing (QuadTree/OctTree)
 */
struct BoundingBox {
    geom::Vec3 min{1e9, 1e9, 1e9};    ///< Minimum köşe
    geom::Vec3 max{-1e9, -1e9, -1e9}; ///< Maximum köşe
    
    /** @brief Geçerli bir bounding box mu? */
    bool IsValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
    
    /** @brief Noktayı kapsayacak şekilde genişlet */
    void Extend(const geom::Vec3& point) {
        if (point.x < min.x) min.x = point.x;
        if (point.y < min.y) min.y = point.y;
        if (point.z < min.z) min.z = point.z;
        if (point.x > max.x) max.x = point.x;
        if (point.y > max.y) max.y = point.y;
        if (point.z > max.z) max.z = point.z;
    }
    
    /** @brief Alias for Extend - AutoCAD compatible */
    void Expand(const geom::Vec3& point) { Extend(point); }
    
    /** @brief Merkez noktasını döndür */
    geom::Vec3 Center() const {
        return geom::Vec3{
            (min.x + max.x) * 0.5,
            (min.y + max.y) * 0.5,
            (min.z + max.z) * 0.5
        };
    }
    
    /** @brief Alias for Center */
    geom::Vec3 GetCenter() const { return Center(); }
    
    /** @brief Boyutları döndür */
    geom::Vec3 Size() const {
        return geom::Vec3{max.x - min.x, max.y - min.y, max.z - min.z};
    }
    
    /** @brief Nokta içinde mi? */
    bool Contains(const geom::Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
    
    /** @brief İki bounding box kesişiyor mu? */
    bool Intersects(const BoundingBox& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }
};

/**
 * @struct Color
 * @brief RGBA renk
 */
struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    
    static Color Red()      { return {255, 0, 0, 255}; }
    static Color Green()    { return {0, 255, 0, 255}; }
    static Color Blue()     { return {0, 0, 255, 255}; }
    static Color White()    { return {255, 255, 255, 255}; }
    static Color Black()    { return {0, 0, 0, 255}; }
    static Color Yellow()   { return {255, 255, 0, 255}; }
    static Color Cyan()     { return {0, 255, 255, 255}; }
    static Color Magenta()  { return {255, 0, 255, 255}; }
    static Color ByLayer()  { return {0, 0, 0, 0}; } ///< Layer rengini kullan (alpha=0 flag)
};

/**
 * @class Entity
 * @brief Tüm CAD varlıklarının temel sınıfı (abstract base class)
 * 
 * Tasarım Prensipleri:
 * - Pure virtual: Clone, GetBounds, Serialize (alt sınıflar implement eder)
 * - Virtual: Draw (renderer'a bağımlı, override edilebilir)
 * - Non-virtual: GetLayer, SetLayer (ortak implementasyon)
 * 
 * Bellek Yönetimi:
 * - Polymorphic, bu nedenle heap'te (unique_ptr/shared_ptr) saklanmalı
 * - Clone ile deep copy yapılır
 */
class Entity {
public:
    /**
     * @brief Constructor - Yeni entity oluştur
     * @param type Entity türü
     * @param layer Hangi katmanda (nullptr ise default layer)
     */
    Entity(EntityType type, Layer* layer = nullptr);
    
    /**
     * @brief Protected default constructor - Alt sınıflar için
     */
    Entity();
    
    /** @brief Virtual destructor (polymorphism için gerekli) */
    virtual ~Entity() = default;
    
    // ==================== SOYUT METODLAR (Alt sınıflar implement eder) ====================
    
    /**
     * @brief Entity'nin derin kopyasını oluştur
     * @return Yeni entity nesnesi (unique_ptr)
     * 
     * Kullanım: Copy, Mirror, Array komutlarında
     */
    virtual std::unique_ptr<Entity> Clone() const = 0;
    
    /**
     * @brief Entity'nin kapladığı alanı hesapla
     * @return Bounding box
     * 
     * Kullanım: Seçim testi, viewport culling, zoom extents
     */
    virtual BoundingBox GetBounds() const = 0;
    
    /**
     * @brief Entity'yi JSON formatına dönüştür (kayıt için)
     * @return JSON object
     */
    virtual void Serialize(nlohmann::json& j) const = 0;
    
    /**
     * @brief JSON'dan entity'yi yükle
     * @param j JSON object
     */
    virtual void Deserialize(const nlohmann::json& j) = 0;
    
    // ==================== ORTAK METODLAR ====================
    
    /** @brief Entity türünü döndür */
    virtual EntityType GetType() const { return m_type; }
    
    /** @brief Unique ID */
    EntityId GetId() const { return m_id; }
    
    /** @brief Layer erişimi */
    Layer* GetLayer() { return m_layer; }
    const Layer* GetLayer() const { return m_layer; }
    void SetLayer(Layer* layer) { m_layer = layer; m_flags |= EF_Modified; }
    
    /** @brief Renk */
    Color GetColor() const { return m_color; }
    void SetColor(const Color& color) { m_color = color; m_flags |= EF_Modified; }
    
    /** @brief Pozisyon (entity'nin anchor noktası) */
    geom::Vec3 GetPosition() const { return m_position; }
    void SetPosition(const geom::Vec3& pos) { m_position = pos; m_flags |= EF_Modified; }
    
    /** @brief Rotasyon (Z ekseni etrafında, derece) */
    double GetRotation() const { return m_rotation; }
    void SetRotation(double rot) { m_rotation = rot; m_flags |= EF_Modified; }
    
    /** @brief Ölçek */
    geom::Vec3 GetScale() const { return m_scale; }
    void SetScale(const geom::Vec3& scale) { m_scale = scale; m_flags |= EF_Modified; }
    
    // ==================== TRANSFORMATIONS (Virtual - Overridable) ====================
    
    /**
     * @brief Entity'yi kaydır
     *  @param delta Hareket vektörü
     */
    virtual void Move(const geom::Vec3& delta) {
        m_position.x += delta.x;
        m_position.y += delta.y;
        m_position.z += delta.z;
        m_flags |= EF_Modified;
    }
    
    /**
     * @brief Entity'yi ölçekle
     * @param scale Ölçek faktörü
     */
    virtual void Scale(const geom::Vec3& scale) {
        m_scale.x *= scale.x;
        m_scale.y *= scale.y;
        m_scale.z *= scale.z;
        m_flags |= EF_Modified;
    }
    
    /**
     * @brief Entity'yi döndür (Z ekseni etrafında)
     * @param angle Radyan cinsinden açı
     */
    virtual void Rotate(double angle) {
        m_rotation += angle;
        m_flags |= EF_Modified;
    }
    
    /**
     * @brief Entity'yi bir çizgiye göre aynala
     * @param p1 Ayna çizgisi başlangıç noktası
     * @param p2 Ayna çizgisi bitiş noktası
     */
    virtual void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
        // Default implementation - override in subclasses
        m_flags |= EF_Modified;
    }
    
    // ==================== DURUM BAYRAKLARI ====================
    
    bool IsSelected() const { return m_flags & EF_Selected; }
    void SetSelected(bool sel) { 
        if (sel) m_flags |= EF_Selected; 
        else m_flags &= ~EF_Selected; 
    }
    
    bool IsVisible() const { return m_flags & EF_Visible; }
    void SetVisible(bool vis) { 
        if (vis) m_flags |= EF_Visible; 
        else m_flags &= ~EF_Visible; 
    }
    
    bool IsLocked() const { return m_flags & EF_Locked; }
    void SetLocked(bool lock) { 
        if (lock) m_flags |= EF_Locked; 
        else m_flags &= ~EF_Locked; 
    }
    
    bool IsHighlighted() const { return m_flags & EF_Highlighted; }
    void SetHighlighted(bool hl) { 
        if (hl) m_flags |= EF_Highlighted; 
        else m_flags &= ~EF_Highlighted; 
    }
    
    bool IsModified() const { return m_flags & EF_Modified; }
    void ClearModified() { m_flags &= ~EF_Modified; }
    
    uint32_t GetFlags() const { return m_flags; }
    void SetFlags(uint32_t flags) { m_flags = flags; }

protected:
    EntityType m_type;              ///< Entity türü
    EntityId m_id;                  ///< Benzersiz kimlik
    Layer* m_layer;                 ///< Ait olduğu katman
    
    // Transform bilgileri
    geom::Vec3 m_position{0, 0, 0}; ///< Dünya koordinatında pozisyon
    double m_rotation = 0.0;        ///< Z ekseni rotasyonu (derece)
    geom::Vec3 m_scale{1, 1, 1};    ///< Ölçek faktörü
    
    // Görünüm
    Color m_color = Color::ByLayer(); ///< Renk (ByLayer ise layer rengini kullan)
    uint32_t m_flags = EF_Default;    ///< Durum bayrakları
    
    // ID generator (static)
    static EntityId s_nextId;
    static EntityId GenerateId() { return s_nextId++; }
};

} // namespace cad
} // namespace vkt
