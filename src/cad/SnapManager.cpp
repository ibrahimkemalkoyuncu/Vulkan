/**
 * @file SnapManager.cpp
 * @brief Snap sistemi implementasyonu
 */

#include "cad/SnapManager.hpp"
#include "cad/Line.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include "cad/Polyline.hpp"
#include <algorithm>
#include <cmath>

namespace vkt {
namespace cad {

SnapManager::SnapManager() {
    EnableAll();
}

uint32_t SnapManager::SnapTypeToBit(SnapType type) {
    return 1u << static_cast<uint32_t>(type);
}

void SnapManager::SetActiveSnaps(const std::vector<SnapType>& snaps) {
    m_activeSnaps = 0;
    for (auto s : snaps) {
        m_activeSnaps |= SnapTypeToBit(s);
    }
}

void SnapManager::EnableSnap(SnapType type) {
    m_activeSnaps |= SnapTypeToBit(type);
}

void SnapManager::DisableSnap(SnapType type) {
    m_activeSnaps &= ~SnapTypeToBit(type);
}

bool SnapManager::IsSnapEnabled(SnapType type) const {
    return (m_activeSnaps & SnapTypeToBit(type)) != 0;
}

void SnapManager::EnableAll() {
    m_activeSnaps = 0xFFFFFFFF;
}

void SnapManager::DisableAll() {
    m_activeSnaps = 0;
}

SnapResult SnapManager::FindSnapPoint(const geom::Vec3& worldPos,
                                       const std::vector<Entity*>& entities,
                                       double viewScale) const {
    // Tolerance = aperture / viewScale (dünya birimi cinsinden)
    double tolerance = (viewScale > 0.0) ? m_aperture / viewScale : m_aperture;

    std::vector<SnapResult> candidates;

    // Her entity için snap noktalarını hesapla
    for (const auto* entity : entities) {
        if (!entity || !entity->IsVisible()) continue;

        if (IsSnapEnabled(SnapType::Endpoint)) {
            FindEndpointSnaps(entity, worldPos, tolerance, candidates);
        }
        if (IsSnapEnabled(SnapType::Midpoint)) {
            FindMidpointSnaps(entity, worldPos, tolerance, candidates);
        }
        if (IsSnapEnabled(SnapType::Center)) {
            FindCenterSnaps(entity, worldPos, tolerance, candidates);
        }
        if (IsSnapEnabled(SnapType::Nearest)) {
            FindNearestSnaps(entity, worldPos, tolerance, candidates);
        }
    }

    // Grid snap
    if (IsSnapEnabled(SnapType::Grid)) {
        FindGridSnap(worldPos, tolerance, candidates);
    }

    // En yakın snap noktasını seç
    // Öncelik: Endpoint > Midpoint > Center > Intersection > Perpendicular > Nearest > Grid
    SnapResult best;
    best.distance = tolerance;

    for (const auto& candidate : candidates) {
        if (candidate.distance < best.distance) {
            best = candidate;
        }
    }

    return best;
}

void SnapManager::FindEndpointSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                     double tolerance, std::vector<SnapResult>& results) const {
    auto addPoint = [&](const geom::Vec3& pt) {
        double dist = pt.DistanceTo(worldPos);
        if (dist < tolerance) {
            results.push_back({pt, SnapType::Endpoint, entity->GetId(), dist});
        }
    };

    switch (entity->GetType()) {
        case EntityType::Line: {
            auto* line = static_cast<const Line*>(entity);
            addPoint(line->GetStart());
            addPoint(line->GetEnd());
            break;
        }
        case EntityType::Arc: {
            auto* arc = static_cast<const Arc*>(entity);
            addPoint(arc->GetStartPoint());
            addPoint(arc->GetEndPoint());
            break;
        }
        case EntityType::Polyline: {
            auto* poly = static_cast<const Polyline*>(entity);
            for (size_t i = 0; i < poly->GetVertexCount(); ++i) {
                addPoint(poly->GetVertex(i).pos);
            }
            break;
        }
        default:
            break;
    }
}

void SnapManager::FindMidpointSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                     double tolerance, std::vector<SnapResult>& results) const {
    auto addPoint = [&](const geom::Vec3& pt) {
        double dist = pt.DistanceTo(worldPos);
        if (dist < tolerance) {
            results.push_back({pt, SnapType::Midpoint, entity->GetId(), dist});
        }
    };

    switch (entity->GetType()) {
        case EntityType::Line: {
            auto* line = static_cast<const Line*>(entity);
            addPoint(line->GetMidPoint());
            break;
        }
        case EntityType::Arc: {
            auto* arc = static_cast<const Arc*>(entity);
            addPoint(arc->GetMidPoint());
            break;
        }
        case EntityType::Polyline: {
            auto* poly = static_cast<const Polyline*>(entity);
            // Her segment'in orta noktası
            for (size_t i = 0; i + 1 < poly->GetVertexCount(); ++i) {
                geom::Vec3 mid = (poly->GetVertex(i).pos + poly->GetVertex(i + 1).pos) * 0.5;
                addPoint(mid);
            }
            if (poly->IsClosed() && poly->GetVertexCount() > 2) {
                geom::Vec3 mid = (poly->GetVertex(poly->GetVertexCount() - 1).pos +
                                  poly->GetVertex(0).pos) * 0.5;
                addPoint(mid);
            }
            break;
        }
        default:
            break;
    }
}

void SnapManager::FindCenterSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                   double tolerance, std::vector<SnapResult>& results) const {
    auto addPoint = [&](const geom::Vec3& pt) {
        double dist = pt.DistanceTo(worldPos);
        if (dist < tolerance) {
            results.push_back({pt, SnapType::Center, entity->GetId(), dist});
        }
    };

    switch (entity->GetType()) {
        case EntityType::Circle: {
            auto* circle = static_cast<const Circle*>(entity);
            addPoint(circle->GetCenter());
            break;
        }
        case EntityType::Arc: {
            auto* arc = static_cast<const Arc*>(entity);
            addPoint(arc->GetCenter());
            break;
        }
        default:
            break;
    }
}

void SnapManager::FindNearestSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                    double tolerance, std::vector<SnapResult>& results) const {
    geom::Vec3 closest;
    bool found = false;

    switch (entity->GetType()) {
        case EntityType::Line: {
            auto* line = static_cast<const Line*>(entity);
            closest = line->GetClosestPoint(worldPos);
            found = true;
            break;
        }
        case EntityType::Arc: {
            auto* arc = static_cast<const Arc*>(entity);
            closest = arc->GetClosestPoint(worldPos);
            found = true;
            break;
        }
        case EntityType::Circle: {
            auto* circle = static_cast<const Circle*>(entity);
            closest = circle->GetClosestPoint(worldPos);
            found = true;
            break;
        }
        case EntityType::Polyline: {
            auto* poly = static_cast<const Polyline*>(entity);
            closest = poly->GetClosestPoint(worldPos);
            found = true;
            break;
        }
        default:
            break;
    }

    if (found) {
        double dist = closest.DistanceTo(worldPos);
        if (dist < tolerance) {
            results.push_back({closest, SnapType::Nearest, entity->GetId(), dist});
        }
    }
}

void SnapManager::FindGridSnap(const geom::Vec3& worldPos, double tolerance,
                                std::vector<SnapResult>& results) const {
    if (m_gridSpacing <= 0.0) return;

    // En yakın grid noktası
    double gx = std::round(worldPos.x / m_gridSpacing) * m_gridSpacing;
    double gy = std::round(worldPos.y / m_gridSpacing) * m_gridSpacing;
    geom::Vec3 gridPoint(gx, gy, 0.0);

    double dist = gridPoint.DistanceTo(worldPos);
    if (dist < tolerance) {
        results.push_back({gridPoint, SnapType::Grid, 0, dist});
    }
}

} // namespace cad
} // namespace vkt
