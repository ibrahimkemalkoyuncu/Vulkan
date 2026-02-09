/**
 * @file Layer.hpp
 * @brief CAD Layer (Katman) Sistemi
 * 
 * Layer Nedir?
 * - CAD çizimlerinde organizasyon aracı (AutoCAD, BricsCAD standardı)
 * - Her entity bir layer'a aittir
 * - Layer özellikleri (renk, çizgi tipi, kalınlık) entity'lere kalıtsallanır
 * - Layer açma/kapatma ile görünürlük kontrolü
 * 
 * Örnek Layer İsimleri (MEP Standartları):
 * - "DUVAR" : Mimari duvarlar
 * - "BORU-TEMIZ-SU" : Temiz su hatları
 * - "BORU-ATIK-SU" : Atık su hatları
 * - "ARMATUR" : Armatürler (lavabo, klozet)
 * - "BOYUT" : Ölçü çizgileri
 * - "METIN" : Açıklama metinleri
 * 
 * Layer Durumları:
 * - Visible: Görünür/görünmez (On/Off)
 * - Frozen: Donmuş (görünmez + seçilemez + hesaba katılmaz)
 * - Locked: Kilitli (görünür ama düzenlenemez)
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "Entity.hpp"

namespace vkt {
namespace cad {

/**
 * @enum LineType
 * @brief Çizgi tipi (CAD standardı)
 */
enum class LineType {
    Continuous,     ///< Sürekli çizgi (—————————)
    Dashed,         ///< Kesik çizgi (— — — —)
    Dotted,         ///< Noktalı çizgi (· · · · ·)
    DashDot,        ///< Çizgi-nokta (—·—·—·)
    DashDotDot,     ///< Çizgi-iki nokta (—··—··)
    Center,         ///< Merkez çizgisi (____ _ ____ _)
    Hidden,         ///< Gizli çizgi (küçük kesikler)
    Phantom         ///< Hayalet çizgi (uzun kesik-nokta)
};

/**
 * @enum LineWeight
 * @brief Çizgi kalınlığı (milimetre cinsinden)
 */
enum class LineWeight : int16_t {
    ByLayer     = -1,   ///< Layer kalınlığını kullan
    ByBlock     = -2,   ///< Blok kalınlığını kullan
    Default     = -3,   ///< Varsayılan (0.25mm)
    
    Hairline    = 0,    ///< En ince (ekran piksel)
    W0_13mm     = 13,   ///< 0.13 mm
    W0_18mm     = 18,   ///< 0.18 mm
    W0_25mm     = 25,   ///< 0.25 mm (varsayılan)
    W0_35mm     = 35,   ///< 0.35 mm
    W0_50mm     = 50,   ///< 0.50 mm
    W0_70mm     = 70,   ///< 0.70 mm
    W1_00mm     = 100,  ///< 1.00 mm
    W1_40mm     = 140,  ///< 1.40 mm
    W2_00mm     = 200   ///< 2.00 mm
};

/**
 * @class Layer
 * @brief CAD katman sınıfı
 * 
 * Özellikler:
 * - Benzersiz isim (case-insensitive karşılaştırma)
 * - Renk, çizgi tipi, kalınlık (entity'lere kalıtsallanır)
 * - Görünürlük kontrolü (visible, frozen, locked)
 * - Entity koleksiyonu (hangi objeler bu layer'da)
 */
class Layer {
public:
    /**
     * @brief Layer oluştur (varsayılan)
     */
    Layer();
    
    /**
     * @brief Layer oluştur
     * @param name Layer ismi (örn: "DUVAR", "BORU-TEMIZ-SU")
     * @param color Varsayılan renk
     */
    Layer(const std::string& name, const Color& color = Color::White());
    
    ~Layer() = default;
    
    // ==================== GETTERLERImpl ====================
    
    /** @brief Layer ismi */
    std::string GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }
    
    /** @brief Varsayılan renk */
    Color GetColor() const { return m_color; }
    void SetColor(const Color& color) { m_color = color; }
    
    /** @brief Çizgi tipi */
    LineType GetLineType() const { return m_lineType; }
    void SetLineType(LineType type) { m_lineType = type; }
    
    /** @brief Çizgi kalınlığı */
    LineWeight GetLineWeight() const { return m_lineWeight; }
    void SetLineWeight(LineWeight weight) { m_lineWeight = weight; }
    
    // ==================== DURUM KONTROLÜ ====================
    
    /**
     * @brief Görünür mü?
     * Kapalı layer'daki entity'ler çizilmez ama seçilebilir
     */
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }
    
    /**
     * @brief Donmuş mu?
     * Frozen layer'daki entity'ler: görünmez, seçilemez, hesaplamaya katılmaz
     * Kullanım: Büyük çizimlerde performans için
     */
    bool IsFrozen() const { return m_frozen; }
    void SetFrozen(bool frozen) { m_frozen = frozen; }
    
    /**
     * @brief Kilitli mi?
     * Locked layer'daki entity'ler: görünür, seçilebilir AMA düzenlenemez
     * Kullanım: Referans çizimler (altlık), hata yapılması istenmeyen alanlar
     */
    bool IsLocked() const { return m_locked; }
    void SetLocked(bool locked) { m_locked = locked; }
    
    /**
     * @brief Yazdırılabilir mi?
     * Plot/Print edilebilir mi? (false ise sadece ekranda görünür)
     */
    bool IsPlottable() const { return m_plottable; }
    void SetPlottable(bool plottable) { m_plottable = plottable; }
    
    // ==================== ÖZELLİKLER ====================
    
    /**
     * @brief Açıklama metni
     * Kullanım: Layer'ın ne için kullanıldığını açıklama
     * Örnek: "Temiz su ana besleme hatları (Ø20-50mm)"
     */
    std::string GetDescription() const { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }
    
    /**
     * @brief Şeffaflık (0-255, 0=tamamen şeffaf, 255=opak)
     */
    uint8_t GetTransparency() const { return m_transparency; }
    void SetTransparency(uint8_t alpha) { m_transparency = alpha; }
    
    // ==================== YARDIMCI METODLAR ====================
    
    /**
     * @brief Bu layer kullanılabilir mi?
     * frozen veya locked ise düzenlenemez
     */
    bool IsEditable() const { return !m_frozen && !m_locked; }
    
    /**
     * @brief Bu layer çizilebilir mi?
     * visible ve frozen değil ise çizilir
     */
    bool IsDrawable() const { return m_visible && !m_frozen; }
    
    /**
     * @brief AutoCAD renk indeksi (1-255)
     * 0 = ByBlock, 256 = ByLayer
     * Kullanım: DXF import/export
     */
    void SetColorIndex(int index);
    int GetColorIndex() const { return m_colorIndex; }
    
    /**
     * @brief AutoCAD renk indeksinden Color nesnesi oluştur
     * @param aci AutoCAD Color Index (1-255)
     * @return Karşılık gelen Color nesnesi
     */
    static Color GetColorFromACI(int aci);

private:
    std::string m_name;                     ///< Benzersiz layer ismi
    std::string m_description;              ///< Açıklama
    
    // Görünüm özellikleri
    Color m_color = Color::White();         ///< Varsayılan renk
    int m_colorIndex = 7;                   ///< AutoCAD renk indeksi (7=white)
    LineType m_lineType = LineType::Continuous; ///< Çizgi tipi
    LineWeight m_lineWeight = LineWeight::Default; ///< Çizgi kalınlığı
    uint8_t m_transparency = 255;           ///< Şeffaflık (255=opak)
    
    // Durum bayrakları
    bool m_visible = true;                  ///< Görünür mü?
    bool m_frozen = false;                  ///< Donmuş mu?
    bool m_locked = false;                  ///< Kilitli mi?
    bool m_plottable = true;                ///< Yazdırılabilir mi?
};

/**
 * @brief Layer ismi karşılaştırma (case-insensitive)
 * Kullanım: std::map<std::string, Layer> ile layer yönetimi
 */
struct LayerNameCompare {
    bool operator()(const std::string& a, const std::string& b) const;
};

} // namespace cad
} // namespace vkt
