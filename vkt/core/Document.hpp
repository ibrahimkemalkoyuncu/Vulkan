/**
 * @file Document.hpp
 * @brief CAD Belge Yönetimi
 * 
 * Proje verilerini ve tesisat şemasını içeren belge sınıfı.
 */

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "mep/NetworkGraph.hpp"
#include "cad/Entity.hpp"
#include "cad/Layer.hpp"
#include "geom/Math.hpp"
#include "Command.hpp"
#include "core/FloorManager.hpp"

namespace vkt {
namespace core {

/**
 * @class Document
 * @brief Bir CAD projesinin tüm verilerini tutan belge
 * 
 * MEP network, command history ve metadata'yı içerir.
 */
class Document {
public:
    Document();
    ~Document();

    // Dosya işlemleri
    bool Save(const std::string& path);
    bool Load(const std::string& path);
    bool IsModified() const { return m_modified; }
    void SetModified(bool mod) { m_modified = mod; }

    std::string GetFilePath() const { return m_filePath; }
    std::string GetTitle() const { return m_title; }

    // Command pattern
    void ExecuteCommand(std::unique_ptr<Command> cmd);
    void TrackExecuted(std::unique_ptr<Command> cmd); // Zaten execute edilmiş komutu history'ye ekle
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    // MEP Network erişimi
    mep::NetworkGraph& GetNetwork() { return m_network; }
    const mep::NetworkGraph& GetNetwork() const { return m_network; }

    // CAD entity erişimi (arka plan çizimi)
    void SetCADEntities(std::vector<std::unique_ptr<cad::Entity>> entities);
    const std::vector<std::unique_ptr<cad::Entity>>& GetCADEntities() const { return m_cadEntities; }
    void GetCADExtents(geom::Vec3& outMin, geom::Vec3& outMax) const;

    // Koordinat normalizasyonu: centroid orijinden uzaksa tüm entity'leri kaydır
    // Büyük koordinatlı çizimlerde (ulusal grid sistemi vb.) float32 hassasiyetini korur
    geom::Vec3 NormalizeCoordinates();
    const geom::Vec3& GetWorldOffset() const { return m_worldOffset; }

    // Layer yönetimi (DWG'den okunan katman bilgileri)
    void SetLayers(const std::unordered_map<std::string, cad::Layer>& layers) { m_layers = layers; }
    const std::unordered_map<std::string, cad::Layer>& GetLayers() const { return m_layers; }

    // Kat yönetimi
    FloorManager& GetFloorManager() { return m_floorManager; }
    const FloorManager& GetFloorManager() const { return m_floorManager; }

private:
    std::string m_filePath;
    std::string m_title = "Untitled";
    bool m_modified = false;

    mep::NetworkGraph m_network;
    std::vector<std::unique_ptr<cad::Entity>> m_cadEntities;
    std::unordered_map<std::string, cad::Layer> m_layers;
    FloorManager m_floorManager;
    geom::Vec3 m_worldOffset;  ///< Normalizasyon sonrası orijinal centroid offset

    std::vector<std::unique_ptr<Command>> m_commandHistory;
    size_t m_commandIndex = 0;
};

} // namespace core
} // namespace vkt
