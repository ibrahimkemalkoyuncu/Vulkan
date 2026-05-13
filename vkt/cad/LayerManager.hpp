/**
 * @file LayerManager.hpp
 * @brief CAD Layer Yönetim Sistemi
 * 
 * Sorumluluğu:
 * - Tüm layer'ları merkezi olarak yönetme
 * - Layer oluşturma, silme, bulma
 * - Aktif layer kontrolü (yeni entity'ler hangi layer'a eklenecek)
 * - Layer görünürlük/durum toplu kontrolü
 * 
 * Tasarım Deseni: Singleton veya Document-owned
 * Bu implementasyonda Document-owned (her döküman kendi layer manager'ını tutar)
 * 
 * Kullanım Senaryoları:
 * 1. Yeni layer oluştur: CreateLayer("BORU-TEMIZ-SU", Color::Blue())
 * 2. Aktif layer değiştir: SetActiveLayer("ARMATUR")
 * 3. Layer durumu değiştir: GetLayer("DUVAR")->SetVisible(false)
 * 4. DXF import: Dosyadaki tüm layer'ları CreateLayer ile ekle
 */

#pragma once

#include "Layer.hpp"
#include <map>
#include <vector>
#include <memory>
#include <string>

namespace vkt {
namespace cad {

/**
 * @class LayerManager
 * @brief Layer kolleksiyonunu yöneten sınıf
 * 
 * Özellikler:
 * - Layer'ları isimle indeksler (std::map)
 * - Aktif layer kavramı (yeni çizilen entity'ler buraya gider)
 * - "0" layer'ı daima vardır (AutoCAD standardı)
 * - Layer silme kuralları: aktif veya entity içeren layer silinemez
 */
class LayerManager {
public:
    LayerManager();
    ~LayerManager() = default;
    
    // ==================== LAYER OLUŞTURMA/SİLME ====================
    
    /**
     * @brief Yeni layer oluştur
     * @param name Layer ismi (benzersiz olmalı)
     * @param color Varsayılan renk
     * @return Oluşturulan layer ptr (başarısız ise nullptr)
     * 
     * Hata durumları:
     * - İsim boş ise nullptr
     * - Aynı isimde layer varsa mevcut olanı döner (yeni oluşturmaz)
     */
    Layer* CreateLayer(const std::string& name, const Color& color = Color::White());
    
    /**
     * @brief Layer'ı sil
     * @param name Silinecek layer ismi
     * @param entityCount Bu layer'daki entity sayısı (0 ise silinir, >0 reddedilir)
     * @return Başarılı ise true
     *
     * Silme engelleri:
     * - "0" layer'ı silinemez
     * - Aktif layer silinemez
     * - Entity içeren layer silinemez (önce entity'ler taşınmalı/silinmeli)
     */
    bool DeleteLayer(const std::string& name, size_t entityCount = 0);
    
    /**
     * @brief Tüm layer'ları temizle (sadece "0" kalır)
     */
    void Clear();
    
    // ==================== LAYER BULMA ====================
    
    /**
     * @brief İsimle layer bul
     * @param name Layer ismi (case-insensitive)
     * @return Layer ptr veya nullptr (bulunamazsa)
     */
    Layer* GetLayer(const std::string& name);
    const Layer* GetLayer(const std::string& name) const;
    
    /**
     * @brief Layer var mı kontrolü
     */
    bool HasLayer(const std::string& name) const;
    
    /**
     * @brief Tüm layer'ları döndür
     * @return Layer ptr vektörü
     */
    std::vector<Layer*> GetAllLayers();
    std::vector<const Layer*> GetAllLayers() const;
    
    /**
     * @brief Layer isimlerini döndür (alfabetik sıralı)
     */
    std::vector<std::string> GetLayerNames() const;
    
    /**
     * @brief Layer sayısı
     */
    size_t GetLayerCount() const { return m_layers.size(); }
    
    // ==================== AKTİF LAYER ====================
    
    /**
     * @brief Aktif layer (yeni entity'ler buraya çizilir)
     * @return Aktif layer ptr (asla nullptr olmaz, en az "0" layer vardır)
     */
    Layer* GetActiveLayer() { return m_activeLayer; }
    const Layer* GetActiveLayer() const { return m_activeLayer; }
    
    /**
     * @brief Aktif layer'ı değiştir
     * @param name Yeni aktif layer ismi
     * @return Başarılı ise true
     * 
     * Hata: İsim bulunamazsa false döner, aktif layer değişmez
     */
    bool SetActiveLayer(const std::string& name);
    
    /**
     * @brief Aktif layer ismini döndür
     */
    std::string GetActiveLayerName() const;
    
    // ==================== TOPLU İŞLEMLER ====================
    
    /**
     * @brief Tüm layer'ları görünür/gizli yap
     */
    void SetAllVisible(bool visible);
    
    /**
     * @brief Tüm layer'ları dondur/çöz
     */
    void SetAllFrozen(bool frozen);
    
    /**
     * @brief Sadece belirtilen layer'ı görünür yap, diğerlerini gizle
     * Kullanım: Tek bir katmanı incelemek için (mimari, tesisat vs.)
     */
    void IsolateLayer(const std::string& name);
    
    /**
     * @brief Görünürlük durumunu eski haline getir
     * IsolateLayer sonrası geri dönmek için
     */
    void RestoreLayerStates();
    
    // ==================== STANDART LAYER'LAR ====================
    
    /**
     * @brief MEP projeleri için standart layer'ları oluştur
     * 
     * Oluşturulan layer'lar:
     * - DUVAR (beyaz): Mimari duvarlar
     * - PENCERE (cyan): Pencereler
     * - KAPI (yeşil): Kapılar
     * - BORU-TEMIZ-SU (mavi): Temiz su hatları
     * - BORU-ATIK-SU (kahverengi): Atık su hatları
     * - BORU-DRENAJ (kırmızı): Drenaj hatları
     * - ARMATUR (magenta): Armatürler
     * - EKIPMAN (sarı): Pompalar, tanklar
     * - BOYUT (yeşil): Ölçü çizgileri
     * - METIN (beyaz): Açıklama metinleri
     */
    void CreateStandardMEPLayers();
    
    /**
     * @brief Mimari projeler için standart layer'ları oluştur
     */
    void CreateStandardArchitecturalLayers();

private:
    /** @brief Layer koleksiyonu (isim -> layer ptr) */
    std::map<std::string, std::unique_ptr<Layer>, LayerNameCompare> m_layers;
    
    /** @brief Aktif layer ptr (asla nullptr) */
    Layer* m_activeLayer = nullptr;
    
    /** @brief Layer görünürlük snapshot (IsolateLayer için) */
    std::map<std::string, bool> m_savedVisibility;
    
    /**
     * @brief "0" layer'ını oluştur (AutoCAD standardı)
     * Her döküman mutlaka "0" layer'ına sahip olmalı
     */
    void CreateDefaultLayer();
};

} // namespace cad
} // namespace vkt
