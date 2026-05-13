/**
 * @file Document.cpp
 * @brief CAD Belge İmplementasyonu
 */

#include "core/Document.hpp"
#include "core/Persistence.hpp"
#include <iostream>
#include <algorithm>
#include <vector>

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

    // Collect entity CENTROIDS (not min/max corners) for IQR-based outlier rejection.
    // Using centroids prevents large entities (xrefs, annotation frames) from skewing bounds.
    std::vector<double> xVals, yVals;
    xVals.reserve(m_cadEntities.size());
    yVals.reserve(m_cadEntities.size());

    for (const auto& entity : m_cadEntities) {
        if (!entity) continue;
        auto bounds = entity->GetBounds();
        if (!bounds.IsValid()) continue;
        xVals.push_back((bounds.min.x + bounds.max.x) * 0.5);
        yVals.push_back((bounds.min.y + bounds.max.y) * 0.5);
    }

    if (xVals.empty()) return;

    std::sort(xVals.begin(), xVals.end());
    std::sort(yVals.begin(), yVals.end());

    size_t n = xVals.size();

    // IQR-based outlier fence: [Q1 - 1.5*IQR, Q3 + 1.5*IQR]
    auto iqrFence = [&](const std::vector<double>& v, double& fenceLo, double& fenceHi) {
        size_t q1i = n / 4;
        size_t q3i = (3 * n) / 4;
        double q1 = v[q1i], q3 = v[std::min(q3i, n - 1)];
        double iqr = q3 - q1;
        if (iqr < 1e-9) iqr = std::abs(q3) * 0.01 + 1.0; // degenerate: single cluster
        fenceLo = q1 - 1.5 * iqr;
        fenceHi = q3 + 1.5 * iqr;
    };

    double xLo, xHi, yLo, yHi;
    iqrFence(xVals, xLo, xHi);
    iqrFence(yVals, yLo, yHi);

    // Collect inlier centroids and find their true min/max
    for (const auto& entity : m_cadEntities) {
        if (!entity) continue;
        auto bounds = entity->GetBounds();
        if (!bounds.IsValid()) continue;
        double cx = (bounds.min.x + bounds.max.x) * 0.5;
        double cy = (bounds.min.y + bounds.max.y) * 0.5;
        if (cx < xLo || cx > xHi || cy < yLo || cy > yHi) continue; // outlier
        outMin.x = std::min(outMin.x, bounds.min.x);
        outMin.y = std::min(outMin.y, bounds.min.y);
        outMax.x = std::max(outMax.x, bounds.max.x);
        outMax.y = std::max(outMax.y, bounds.max.y);
    }

    if (outMin.x > outMax.x) { // all filtered — fall back to raw centroid range
        outMin.x = xVals.front(); outMax.x = xVals.back();
        outMin.y = yVals.front(); outMax.y = yVals.back();
    }
}

geom::Vec3 Document::NormalizeCoordinates() {
    if (m_cadEntities.empty()) return m_worldOffset;

    geom::Vec3 minP, maxP;
    GetCADExtents(minP, maxP);

    geom::Vec3 centroid(
        (minP.x + maxP.x) * 0.5,
        (minP.y + maxP.y) * 0.5,
        0.0
    );

    // Only normalize if centroid is significantly far from origin (>1000 units)
    double distSq = centroid.x * centroid.x + centroid.y * centroid.y;
    if (distSq < 1000.0 * 1000.0) return m_worldOffset;

    std::cout << "[Document] Koordinat normalizasyonu: centroid=("
              << centroid.x << ", " << centroid.y << "), "
              << m_cadEntities.size() << " entity kaydırılıyor." << std::endl;

    geom::Vec3 delta(-centroid.x, -centroid.y, 0.0);
    for (auto& e : m_cadEntities) {
        if (e) e->Move(delta);
    }
    m_worldOffset = centroid;
    return m_worldOffset;
}

} // namespace core
} // namespace vkt
