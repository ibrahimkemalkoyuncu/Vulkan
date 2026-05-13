/**
 * @file LayerManager.cpp
 * @brief LayerManager implementasyonu
 */

#include "cad/LayerManager.hpp"
#include <algorithm>
#include <iostream>

namespace vkt {
namespace cad {

LayerManager::LayerManager() {
    CreateDefaultLayer();
}

void LayerManager::CreateDefaultLayer() {
    // "0" layer'ı oluştur (AutoCAD standardı)
    auto layer0 = std::make_unique<Layer>("0", Color::White());
    layer0->SetDescription("Varsayılan layer (silinmez)");
    m_activeLayer = layer0.get();
    m_layers["0"] = std::move(layer0);
}

Layer* LayerManager::CreateLayer(const std::string& name, const Color& color) {
    if (name.empty()) {
        std::cerr << "HATA: Layer ismi boş olamaz!" << std::endl;
        return nullptr;
    }
    
    // Zaten var mı kontrol et
    auto it = m_layers.find(name);
    if (it != m_layers.end()) {
        std::cout << "INFO: '" << name << "' layer'ı zaten mevcut." << std::endl;
        return it->second.get();
    }
    
    // Yeni layer oluştur
    auto layer = std::make_unique<Layer>(name, color);
    Layer* layerPtr = layer.get();
    m_layers[name] = std::move(layer);
    
    std::cout << "Layer oluşturuldu: " << name << std::endl;
    return layerPtr;
}

bool LayerManager::DeleteLayer(const std::string& name, size_t entityCount) {
    // "0" layer'ı silinemez
    if (name == "0") {
        std::cerr << "HATA: '0' layer'ı silinemez!" << std::endl;
        return false;
    }

    auto it = m_layers.find(name);
    if (it == m_layers.end()) {
        std::cerr << "HATA: '" << name << "' layer'ı bulunamadı!" << std::endl;
        return false;
    }

    // Aktif layer silinemez
    if (it->second.get() == m_activeLayer) {
        std::cerr << "HATA: Aktif layer silinemez! Önce başka bir layer'ı aktif yapın." << std::endl;
        return false;
    }

    // Entity içeren layer silinemez — caller entity sayısını geçirmelidir
    if (entityCount > 0) {
        std::cerr << "HATA: '" << name << "' layer'ı " << entityCount
                  << " entity içeriyor. Önce entity'leri başka layer'a taşıyın veya silin." << std::endl;
        return false;
    }

    m_layers.erase(it);
    std::cout << "Layer silindi: " << name << std::endl;
    return true;
}

void LayerManager::Clear() {
    m_layers.clear();
    CreateDefaultLayer();
}

Layer* LayerManager::GetLayer(const std::string& name) {
    auto it = m_layers.find(name);
    return (it != m_layers.end()) ? it->second.get() : nullptr;
}

const Layer* LayerManager::GetLayer(const std::string& name) const {
    auto it = m_layers.find(name);
    return (it != m_layers.end()) ? it->second.get() : nullptr;
}

bool LayerManager::HasLayer(const std::string& name) const {
    return m_layers.find(name) != m_layers.end();
}

std::vector<Layer*> LayerManager::GetAllLayers() {
    std::vector<Layer*> layers;
    layers.reserve(m_layers.size());
    for (auto& pair : m_layers) {
        layers.push_back(pair.second.get());
    }
    return layers;
}

std::vector<const Layer*> LayerManager::GetAllLayers() const {
    std::vector<const Layer*> layers;
    layers.reserve(m_layers.size());
    for (const auto& pair : m_layers) {
        layers.push_back(pair.second.get());
    }
    return layers;
}

std::vector<std::string> LayerManager::GetLayerNames() const {
    std::vector<std::string> names;
    names.reserve(m_layers.size());
    for (const auto& pair : m_layers) {
        names.push_back(pair.first);
    }
    return names;
}

bool LayerManager::SetActiveLayer(const std::string& name) {
    Layer* layer = GetLayer(name);
    if (!layer) {
        std::cerr << "HATA: '" << name << "' layer'ı bulunamadı!" << std::endl;
        return false;
    }
    
    m_activeLayer = layer;
    std::cout << "Aktif layer: " << name << std::endl;
    return true;
}

std::string LayerManager::GetActiveLayerName() const {
    if (!m_activeLayer) return "";
    return m_activeLayer->GetName();
}

void LayerManager::SetAllVisible(bool visible) {
    for (auto& pair : m_layers) {
        pair.second->SetVisible(visible);
    }
}

void LayerManager::SetAllFrozen(bool frozen) {
    for (auto& pair : m_layers) {
        pair.second->SetFrozen(frozen);
    }
}

void LayerManager::IsolateLayer(const std::string& name) {
    // Mevcut durumu kaydet
    m_savedVisibility.clear();
    for (const auto& pair : m_layers) {
        m_savedVisibility[pair.first] = pair.second->IsVisible();
    }
    
    // Sadece belirtilen layer'ı görünür yap
    for (auto& pair : m_layers) {
        pair.second->SetVisible(pair.first == name);
    }
}

void LayerManager::RestoreLayerStates() {
    for (auto& pair : m_layers) {
        auto it = m_savedVisibility.find(pair.first);
        if (it != m_savedVisibility.end()) {
            pair.second->SetVisible(it->second);
        }
    }
    m_savedVisibility.clear();
}

void LayerManager::CreateStandardMEPLayers() {
    std::cout << "MEP standart layer'ları oluşturuluyor..." << std::endl;
    
    // Mimari
    CreateLayer("DUVAR", Color::White())->SetDescription("Mimari duvarlar");
    CreateLayer("PENCERE", Color::Cyan())->SetDescription("Pencereler");
    CreateLayer("KAPI", Color::Green())->SetDescription("Kapılar");
    
    // Temiz Su
    auto* temizSu = CreateLayer("BORU-TEMIZ-SU", Color::Blue());
    temizSu->SetDescription("Temiz su ana hatları");
    temizSu->SetLineWeight(LineWeight::W0_35mm);
    
    // Atık Su
    auto* atikSu = CreateLayer("BORU-ATIK-SU", Color{139, 69, 19, 255}); // Kahverengi
    atikSu->SetDescription("Atık su hatları");
    atikSu->SetLineWeight(LineWeight::W0_50mm);
    
    // Drenaj
    auto* drenaj = CreateLayer("BORU-DRENAJ", Color::Red());
    drenaj->SetDescription("Drenaj hatları (yağmur suyu)");
    drenaj->SetLineWeight(LineWeight::W0_50mm);
    
    // Armatürler
    CreateLayer("ARMATUR", Color::Magenta())->SetDescription("Musluklar, valfler");
    
    // Ekipman
    CreateLayer("EKIPMAN", Color::Yellow())->SetDescription("Pompalar, tanklar, boiler");
    
    // Boyutlandırma
    auto* boyut = CreateLayer("BOYUT", Color::Green());
    boyut->SetDescription("Ölçü çizgileri");
    boyut->SetLineWeight(LineWeight::W0_13mm);
    
    // Metin
    CreateLayer("METIN", Color::White())->SetDescription("Açıklama metinleri");
    
    std::cout << "MEP layer'ları oluşturuldu (" << m_layers.size() << " adet)" << std::endl;
}

void LayerManager::CreateStandardArchitecturalLayers() {
    std::cout << "Mimari standart layer'ları oluşturuluyor..." << std::endl;
    
    CreateLayer("A-WALL", Color::White())->SetDescription("Duvarlar");
    CreateLayer("A-DOOR", Color::Green())->SetDescription("Kapılar");
    CreateLayer("A-WIND", Color::Cyan())->SetDescription("Pencereler");
    CreateLayer("A-FLOR", Color{200, 200, 200, 255})->SetDescription("Döşeme");
    CreateLayer("A-STAIR", Color::Yellow())->SetDescription("Merdivenler");
    CreateLayer("A-FURN", Color{165, 42, 42, 255})->SetDescription("Mobilyalar");
    CreateLayer("A-ANNO", Color::White())->SetDescription("Açıklamalar");
    CreateLayer("A-DIMS", Color::Green())->SetDescription("Boyutlar");
    
    std::cout << "Mimari layer'lar oluşturuldu (" << m_layers.size() << " adet)" << std::endl;
}

} // namespace cad
} // namespace vkt
