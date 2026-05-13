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
#include <limits>

namespace vkt {
namespace cad {

SnapManager::SnapManager() {
    EnableAll();
}

uint32_t SnapManager::SnapTypeToBit(SnapType type) {
    return 1u << static_cast<uint32_t>(type);
}

// Lower number = higher priority (Endpoint wins over Grid)
int SnapManager::SnapPriority(SnapType type) {
    switch (type) {
        case SnapType::Endpoint:      return 0;
        case SnapType::Center:        return 1;
        case SnapType::Intersection:  return 2;
        case SnapType::Midpoint:      return 3;
        case SnapType::Perpendicular: return 4;
        case SnapType::Nearest:       return 5;
        case SnapType::Grid:          return 6;
        default:                      return 99;
    }
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

double SnapManager::AdaptiveGridSpacing(double viewScale) const {
    if (m_gridOverride != 0.0) return m_gridOverride;
    if (viewScale <= 0.0) return m_gridSpacing;

    // Target: ~40-80 pixels between grid lines.
    // Snap to nice round values: 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1, 5, 10...
    double rawSpacing = 60.0 / viewScale; // ~60 pixels apart
    static const double niceValues[] = {
        0.001, 0.002, 0.005,
        0.01,  0.02,  0.05,
        0.1,   0.2,   0.5,
        1.0,   2.0,   5.0,
        10.0,  20.0,  50.0,
        100.0, 200.0, 500.0
    };
    double best = niceValues[0];
    double bestDiff = std::abs(std::log(niceValues[0]) - std::log(rawSpacing));
    for (double v : niceValues) {
        double diff = std::abs(std::log(v) - std::log(rawSpacing));
        if (diff < bestDiff) { bestDiff = diff; best = v; }
    }
    return best;
}

SnapResult SnapManager::FindSnapPoint(const geom::Vec3& worldPos,
                                       const std::vector<Entity*>& entities,
                                       double viewScale) const {
    double tolerance = (viewScale > 0.0) ? m_aperture / viewScale : m_aperture;
    double gridSpacing = AdaptiveGridSpacing(viewScale);

    std::vector<SnapResult> candidates;

    for (const auto* entity : entities) {
        if (!entity || !entity->IsVisible()) continue;

        if (IsSnapEnabled(SnapType::Endpoint))
            FindEndpointSnaps(entity, worldPos, tolerance, candidates);
        if (IsSnapEnabled(SnapType::Midpoint))
            FindMidpointSnaps(entity, worldPos, tolerance, candidates);
        if (IsSnapEnabled(SnapType::Center))
            FindCenterSnaps(entity, worldPos, tolerance, candidates);
        if (IsSnapEnabled(SnapType::Nearest))
            FindNearestSnaps(entity, worldPos, tolerance, candidates);
        if (IsSnapEnabled(SnapType::Perpendicular))
            FindPerpendicularSnaps(entity, worldPos, tolerance, candidates);
    }

    // Intersection: pairwise check — only for lines/polyline edges (O(n²) but n is small)
    if (IsSnapEnabled(SnapType::Intersection)) {
        for (size_t i = 0; i < entities.size(); ++i) {
            if (!entities[i] || !entities[i]->IsVisible()) continue;
            for (size_t j = i + 1; j < entities.size(); ++j) {
                if (!entities[j] || !entities[j]->IsVisible()) continue;
                FindIntersectionSnaps(entities[i], entities[j], worldPos, tolerance, candidates);
            }
        }
    }

    if (IsSnapEnabled(SnapType::Grid))
        FindGridSnap(worldPos, tolerance, gridSpacing, candidates);

    if (candidates.empty()) return {};

    // Priority-first selection: among candidates within tolerance, pick highest-priority
    // type; break ties by distance.
    SnapResult best = candidates[0];
    for (const auto& c : candidates) {
        int cp = SnapPriority(c.type);
        int bp = SnapPriority(best.type);
        if (cp < bp || (cp == bp && c.distance < best.distance)) {
            best = c;
        }
    }
    return best;
}

// ── Endpoint ─────────────────────────────────────────────────────────────────

void SnapManager::FindEndpointSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                     double tolerance, std::vector<SnapResult>& results) const {
    auto add = [&](const geom::Vec3& pt) {
        double d = pt.DistanceTo(worldPos);
        if (d < tolerance)
            results.push_back({pt, SnapType::Endpoint, entity->GetId(), d});
    };

    switch (entity->GetType()) {
        case EntityType::Line: {
            auto* line = static_cast<const Line*>(entity);
            add(line->GetStart());
            add(line->GetEnd());
            break;
        }
        case EntityType::Arc: {
            auto* arc = static_cast<const Arc*>(entity);
            add(arc->GetStartPoint());
            add(arc->GetEndPoint());
            break;
        }
        case EntityType::Polyline: {
            auto* poly = static_cast<const Polyline*>(entity);
            for (size_t i = 0; i < poly->GetVertexCount(); ++i)
                add(poly->GetVertex(i).pos);
            break;
        }
        default:
            break;
    }
}

// ── Midpoint ─────────────────────────────────────────────────────────────────

void SnapManager::FindMidpointSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                     double tolerance, std::vector<SnapResult>& results) const {
    auto add = [&](const geom::Vec3& pt) {
        double d = pt.DistanceTo(worldPos);
        if (d < tolerance)
            results.push_back({pt, SnapType::Midpoint, entity->GetId(), d});
    };

    switch (entity->GetType()) {
        case EntityType::Line: {
            add(static_cast<const Line*>(entity)->GetMidPoint());
            break;
        }
        case EntityType::Arc: {
            add(static_cast<const Arc*>(entity)->GetMidPoint());
            break;
        }
        case EntityType::Polyline: {
            auto* poly = static_cast<const Polyline*>(entity);
            size_t n = poly->GetVertexCount();
            for (size_t i = 0; i + 1 < n; ++i)
                add((poly->GetVertex(i).pos + poly->GetVertex(i + 1).pos) * 0.5);
            if (poly->IsClosed() && n > 2)
                add((poly->GetVertex(n - 1).pos + poly->GetVertex(0).pos) * 0.5);
            break;
        }
        default:
            break;
    }
}

// ── Center ───────────────────────────────────────────────────────────────────

void SnapManager::FindCenterSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                   double tolerance, std::vector<SnapResult>& results) const {
    auto add = [&](const geom::Vec3& pt) {
        double d = pt.DistanceTo(worldPos);
        if (d < tolerance)
            results.push_back({pt, SnapType::Center, entity->GetId(), d});
    };

    switch (entity->GetType()) {
        case EntityType::Circle:
            add(static_cast<const Circle*>(entity)->GetCenter()); break;
        case EntityType::Arc:
            add(static_cast<const Arc*>(entity)->GetCenter()); break;
        default:
            break;
    }
}

// ── Nearest ──────────────────────────────────────────────────────────────────

void SnapManager::FindNearestSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                    double tolerance, std::vector<SnapResult>& results) const {
    geom::Vec3 closest;
    bool found = false;

    switch (entity->GetType()) {
        case EntityType::Line:
            closest = static_cast<const Line*>(entity)->GetClosestPoint(worldPos); found = true; break;
        case EntityType::Arc:
            closest = static_cast<const Arc*>(entity)->GetClosestPoint(worldPos);  found = true; break;
        case EntityType::Circle:
            closest = static_cast<const Circle*>(entity)->GetClosestPoint(worldPos); found = true; break;
        case EntityType::Polyline:
            closest = static_cast<const Polyline*>(entity)->GetClosestPoint(worldPos); found = true; break;
        default:
            break;
    }

    if (found) {
        double d = closest.DistanceTo(worldPos);
        if (d < tolerance)
            results.push_back({closest, SnapType::Nearest, entity->GetId(), d});
    }
}

// ── Perpendicular ─────────────────────────────────────────────────────────────
// Projects worldPos onto the infinite extension of a line segment.

void SnapManager::FindPerpendicularSnaps(const Entity* entity, const geom::Vec3& worldPos,
                                          double tolerance, std::vector<SnapResult>& results) const {
    if (entity->GetType() != EntityType::Line) return;

    auto* line = static_cast<const Line*>(entity);
    geom::Vec3 s = line->GetStart();
    geom::Vec3 e = line->GetEnd();
    geom::Vec3 d = e - s;
    double len2 = d.x*d.x + d.y*d.y + d.z*d.z;
    if (len2 < 1e-12) return;

    double t = ((worldPos.x - s.x)*d.x + (worldPos.y - s.y)*d.y + (worldPos.z - s.z)*d.z) / len2;
    // Allow projection onto segment extensions (standard CAD behaviour)
    geom::Vec3 foot{ s.x + t*d.x, s.y + t*d.y, s.z + t*d.z };

    double dist = foot.DistanceTo(worldPos);
    if (dist < tolerance)
        results.push_back({foot, SnapType::Perpendicular, entity->GetId(), dist});
}

// ── Intersection ──────────────────────────────────────────────────────────────
// 2D line-line intersection for Line and Polyline edges.

namespace {
// Returns true and sets `out` if segments (p1→p2) and (p3→p4) intersect (extended infinitely).
bool LineLineIntersect2D(const geom::Vec3& p1, const geom::Vec3& p2,
                          const geom::Vec3& p3, const geom::Vec3& p4,
                          geom::Vec3& out) {
    double d1x = p2.x - p1.x, d1y = p2.y - p1.y;
    double d2x = p4.x - p3.x, d2y = p4.y - p3.y;
    double denom = d1x * d2y - d1y * d2x;
    if (std::abs(denom) < 1e-10) return false; // parallel
    double dx = p3.x - p1.x, dy = p3.y - p1.y;
    double t = (dx * d2y - dy * d2x) / denom;
    double s = (dx * d1y - dy * d1x) / denom;
    if (t < 0.0 || t > 1.0 || s < 0.0 || s > 1.0) return false; // outside segments
    out = { p1.x + t*d1x, p1.y + t*d1y, 0.0 };
    return true;
}

// Collect all segments (start, end) from a Line or Polyline entity.
void CollectSegments(const Entity* ent,
                     std::vector<std::pair<geom::Vec3, geom::Vec3>>& segs) {
    if (ent->GetType() == EntityType::Line) {
        auto* l = static_cast<const Line*>(ent);
        segs.push_back({l->GetStart(), l->GetEnd()});
    } else if (ent->GetType() == EntityType::Polyline) {
        auto* p = static_cast<const Polyline*>(ent);
        size_t n = p->GetVertexCount();
        for (size_t i = 0; i + 1 < n; ++i)
            segs.push_back({p->GetVertex(i).pos, p->GetVertex(i+1).pos});
        if (p->IsClosed() && n > 2)
            segs.push_back({p->GetVertex(n-1).pos, p->GetVertex(0).pos});
    }
}
} // anonymous namespace

void SnapManager::FindIntersectionSnaps(const Entity* a, const Entity* b,
                                         const geom::Vec3& worldPos, double tolerance,
                                         std::vector<SnapResult>& results) const {
    std::vector<std::pair<geom::Vec3, geom::Vec3>> segsA, segsB;
    CollectSegments(a, segsA);
    CollectSegments(b, segsB);

    for (auto& [a1, a2] : segsA) {
        for (auto& [b1, b2] : segsB) {
            geom::Vec3 pt;
            if (LineLineIntersect2D(a1, a2, b1, b2, pt)) {
                double d = pt.DistanceTo(worldPos);
                if (d < tolerance)
                    results.push_back({pt, SnapType::Intersection, a->GetId(), d});
            }
        }
    }
}

// ── Grid ─────────────────────────────────────────────────────────────────────

void SnapManager::FindGridSnap(const geom::Vec3& worldPos, double tolerance,
                                double gridSpacing, std::vector<SnapResult>& results) const {
    if (gridSpacing <= 0.0) return;
    double gx = std::round(worldPos.x / gridSpacing) * gridSpacing;
    double gy = std::round(worldPos.y / gridSpacing) * gridSpacing;
    geom::Vec3 pt(gx, gy, 0.0);
    double d = pt.DistanceTo(worldPos);
    if (d < tolerance)
        results.push_back({pt, SnapType::Grid, 0, d});
}

} // namespace cad
} // namespace vkt
