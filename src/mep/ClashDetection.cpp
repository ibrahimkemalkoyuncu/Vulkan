/**
 * @file ClashDetection.cpp
 * @brief MEP boru – mimari eleman çakışma analiz motoru
 */

#include "mep/ClashDetection.hpp"
#include <iostream>
#include <algorithm>

namespace vkt {
namespace mep {

ClashEngine::ClashEngine(NetworkGraph& network,
                         const std::vector<std::unique_ptr<cad::Entity>>& staticEntities)
    : m_network(network), m_archEntities(staticEntities) {}

std::vector<ClashResult> ClashEngine::RunAnalysis() {
    std::vector<ClashResult> results;
    uint32_t counter = 0;

    const auto& edges = m_network.GetEdges();

    for (const auto& pipe : edges) {
        const Node* nodeA = m_network.GetNode(pipe.nodeA);
        const Node* nodeB = m_network.GetNode(pipe.nodeB);
        if (!nodeA || !nodeB) continue;

        // Build pipe AABB expanded by pipe radius + soft-clash tolerance
        cad::BoundingBox pipeBox;
        pipeBox.Extend(nodeA->position);
        pipeBox.Extend(nodeB->position);

        double expand = pipe.diameter_mm / 2000.0 + m_tolerance / 1000.0;
        pipeBox.min.x -= expand; pipeBox.min.y -= expand; pipeBox.min.z -= expand;
        pipeBox.max.x += expand; pipeBox.max.y += expand; pipeBox.max.z += expand;

        for (const auto& arch : m_archEntities) {
            if (!arch) continue;

            cad::BoundingBox archBox = arch->GetBounds();
            if (!pipeBox.Intersects(archBox)) continue;

            ClashResult clash;
            if (CheckIntersection(pipe, arch.get(), clash)) {
                clash.id            = ++counter;
                clash.edgeId        = pipe.id;
                clash.architecturalId = arch->GetId();
                results.push_back(clash);
            }
        }
    }

    std::cout << "[ClashEngine] Analiz tamamlandı. Toplam çakışma: "
              << results.size() << std::endl;
    return results;
}

bool ClashEngine::CheckIntersection(const Edge& pipe, const cad::Entity* arch,
                                    ClashResult& result) {
    const Node* nA = m_network.GetNode(pipe.nodeA);
    const Node* nB = m_network.GetNode(pipe.nodeB);
    if (!nA || !nB) return false;

    double pipeRadius = pipe.diameter_mm / 2000.0; // m

    // Segment–AABB test: project segment onto each axis and check overlap
    cad::BoundingBox box = arch->GetBounds();

    // Expand box by pipe radius for hard-clash check
    cad::BoundingBox expanded = box;
    expanded.min.x -= pipeRadius; expanded.min.y -= pipeRadius; expanded.min.z -= pipeRadius;
    expanded.max.x += pipeRadius; expanded.max.y += pipeRadius; expanded.max.z += pipeRadius;

    // Check if segment midpoint is inside the expanded box (simplified test)
    geom::Vec3 mid{(nA->position.x + nB->position.x) * 0.5,
                   (nA->position.y + nB->position.y) * 0.5,
                   (nA->position.z + nB->position.z) * 0.5};

    if (expanded.Contains(mid)) {
        result.severity        = ClashSeverity::Hard;
        result.clashPoint      = mid;
        result.overlapAmount_mm = pipeRadius * 1000.0;
        result.description     = "Boru (" + std::to_string(pipe.id) +
                                 ") mimari eleman içinden geçiyor.";
        return true;
    }

    // Soft clash: check if either endpoint is within tolerance zone
    cad::BoundingBox softBox = box;
    softBox.min.x -= m_tolerance / 1000.0; softBox.min.y -= m_tolerance / 1000.0;
    softBox.max.x += m_tolerance / 1000.0; softBox.max.y += m_tolerance / 1000.0;

    if (softBox.Contains(nA->position) || softBox.Contains(nB->position)) {
        result.severity        = ClashSeverity::Soft;
        result.clashPoint      = softBox.Contains(nA->position) ? nA->position : nB->position;
        result.overlapAmount_mm = 0.0;
        result.description     = "Boru (" + std::to_string(pipe.id) +
                                 ") tolerans mesafesini ihlal ediyor.";
        return true;
    }

    return false;
}

// ═══════════════════════════════════════════════════════════
//  CLASH RESOLUTION SUGGESTIONS
// ═══════════════════════════════════════════════════════════

std::vector<ClashResolution> SuggestResolutions(
    const std::vector<ClashResult>& clashes,
    const NetworkGraph& network)
{
    std::vector<ClashResolution> resolutions;

    for (const auto& clash : clashes) {
        ClashResolution res;
        res.entityId = clash.edgeId;

        // Get pipe info for severity and offset calculation
        const Edge* edge = network.GetEdge(clash.edgeId);
        double dn = edge ? edge->diameter_mm : 25.0;
        EdgeType etype = edge ? edge->type : EdgeType::Supply;

        // Severity based on pipe type
        switch (etype) {
            case EdgeType::FireLine:     res.severity = 1.0; break;
            case EdgeType::Drainage:     res.severity = 0.8; break;
            case EdgeType::Gas:          res.severity = 0.9; break;
            case EdgeType::Heating:
            case EdgeType::HeatingReturn:res.severity = 0.6; break;
            case EdgeType::Supply:
            case EdgeType::HotWater:     res.severity = 0.5; break;
            default:                     res.severity = 0.4; break;
        }

        // Offset: overlap + 50mm clearance, move upward (Z+)
        double offsetMM = clash.overlapAmount_mm + 50.0;
        double offsetM = offsetMM / 1000.0;
        res.suggestedOffset = {0.0, 0.0, offsetM};

        // Description
        int dnInt = static_cast<int>(dn);
        res.description = "Boru #" + std::to_string(clash.edgeId) +
                          " (DN" + std::to_string(dnInt) +
                          ") yukari " + std::to_string(static_cast<int>(offsetMM)) +
                          "mm kaydirin";

        resolutions.push_back(res);
    }

    return resolutions;
}

} // namespace mep
} // namespace vkt
