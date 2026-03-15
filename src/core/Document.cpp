/**
 * @file Document.cpp
 * @brief CAD Belge İmplementasyonu
 */

#include "core/Document.hpp"
#include "core/Persistence.hpp"
#include <iostream>

namespace vkt {
namespace core {

Document::Document() {
    std::cout << "Document oluşturuldu." << std::endl;
}

Document::~Document() {
    std::cout << "Document yok edildi." << std::endl;
}

bool Document::Save(const std::string& path) {
    try {
        bool success = Persistence::SaveProject(path, m_network);
        if (success) {
            m_filePath = path;
            m_modified = false;
            std::cout << "Proje kaydedildi: " << path << std::endl;
        }
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Kaydetme hatası: " << e.what() << std::endl;
        return false;
    }
}

bool Document::Load(const std::string& path) {
    try {
        bool success = Persistence::LoadProject(path, m_network);
        if (success) {
            m_filePath = path;
            m_modified = false;
            std::cout << "Proje yüklendi: " << path << std::endl;
        }
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Yükleme hatası: " << e.what() << std::endl;
        return false;
    }
}

void Document::ExecuteCommand(std::unique_ptr<Command> cmd) {
    if (!cmd) return;
    
    cmd->Execute();
    
    // Redo history'yi temizle
    m_commandHistory.erase(
        m_commandHistory.begin() + m_commandIndex,
        m_commandHistory.end()
    );
    
    m_commandHistory.push_back(std::move(cmd));
    m_commandIndex = m_commandHistory.size();
    m_modified = true;
}

void Document::Undo() {
    if (!CanUndo()) return;
    
    m_commandIndex--;
    m_commandHistory[m_commandIndex]->Undo();
    m_modified = true;
}

void Document::Redo() {
    if (!CanRedo()) return;
    
    m_commandHistory[m_commandIndex]->Execute();
    m_commandIndex++;
    m_modified = true;
}

bool Document::CanUndo() const {
    return m_commandIndex > 0;
}

bool Document::CanRedo() const {
    return m_commandIndex < m_commandHistory.size();
}

void Document::SetCADEntities(std::vector<std::unique_ptr<cad::Entity>> entities) {
    m_cadEntities = std::move(entities);
    std::cout << "CAD entity'leri yüklendi: " << m_cadEntities.size() << " adet" << std::endl;
}

void Document::GetCADExtents(geom::Vec3& outMin, geom::Vec3& outMax) const {
    outMin = geom::Vec3(1e9, 1e9, 0);
    outMax = geom::Vec3(-1e9, -1e9, 0);

    for (const auto& entity : m_cadEntities) {
        if (!entity) continue;
        auto bounds = entity->GetBounds();
        if (!bounds.IsValid()) continue;
        if (bounds.min.x < outMin.x) outMin.x = bounds.min.x;
        if (bounds.min.y < outMin.y) outMin.y = bounds.min.y;
        if (bounds.max.x > outMax.x) outMax.x = bounds.max.x;
        if (bounds.max.y > outMax.y) outMax.y = bounds.max.y;
    }
}

} // namespace core
} // namespace vkt
