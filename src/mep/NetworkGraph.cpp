/**
 * @file NetworkGraph.cpp
 * @brief Tesisat Şebeke Veri Yapısı İmplementasyonu
 *
 * unordered_map + adjacency list tabanlı O(1) lookup.
 */

#include "mep/NetworkGraph.hpp"
#include <algorithm>

namespace vkt {
namespace mep {

// ==================== NODE İŞLEMLERİ ====================

uint32_t NetworkGraph::AddNode(const Node& node) {
    Node newNode = node;
    newNode.id = m_nextNodeId++;
    m_nodeMap[newNode.id] = newNode;
    m_adjacency[newNode.id]; // Boş adjacency entry oluştur
    m_nodeCacheDirty = true;
    return newNode.id;
}

void NetworkGraph::RemoveNode(uint32_t nodeId) {
    // Önce bağlı edge'leri sil
    auto adjIt = m_adjacency.find(nodeId);
    if (adjIt != m_adjacency.end()) {
        // Kopyasını al çünkü RemoveEdge adjacency'yi değiştirir
        auto edgeIds = adjIt->second;
        for (uint32_t edgeId : edgeIds) {
            RemoveEdge(edgeId);
        }
    }

    m_nodeMap.erase(nodeId);
    m_adjacency.erase(nodeId);
    m_nodeCacheDirty = true;
}

Node* NetworkGraph::GetNode(uint32_t nodeId) {
    auto it = m_nodeMap.find(nodeId);
    return (it != m_nodeMap.end()) ? &it->second : nullptr;
}

const Node* NetworkGraph::GetNode(uint32_t nodeId) const {
    auto it = m_nodeMap.find(nodeId);
    return (it != m_nodeMap.end()) ? &it->second : nullptr;
}

std::vector<Node> NetworkGraph::GetNodeList() const {
    std::vector<Node> result;
    result.reserve(m_nodeMap.size());
    for (const auto& pair : m_nodeMap) {
        result.push_back(pair.second);
    }
    return result;
}

void NetworkGraph::RebuildNodeCache() const {
    m_nodeCache.clear();
    m_nodeCache.reserve(m_nodeMap.size());
    for (auto& pair : m_nodeMap) {
        m_nodeCache.push_back(pair.second);
    }
    m_nodeCacheDirty = false;
}

std::vector<Node>& NetworkGraph::GetNodes() {
    if (m_nodeCacheDirty) RebuildNodeCache();
    // Sync back: caller may mutate cache entries
    // After mutation the cache is stale, but this is backward-compat behavior
    return m_nodeCache;
}

const std::vector<Node>& NetworkGraph::GetNodes() const {
    if (m_nodeCacheDirty) RebuildNodeCache();
    return m_nodeCache;
}

// ==================== EDGE İŞLEMLERİ ====================

uint32_t NetworkGraph::AddEdge(const Edge& edge) {
    Edge newEdge = edge;
    newEdge.id = m_nextEdgeId++;
    m_edgeMap[newEdge.id] = newEdge;

    // Adjacency list güncelle
    m_adjacency[newEdge.nodeA].push_back(newEdge.id);
    m_adjacency[newEdge.nodeB].push_back(newEdge.id);

    m_edgeCacheDirty = true;
    return newEdge.id;
}

void NetworkGraph::RemoveEdge(uint32_t edgeId) {
    auto it = m_edgeMap.find(edgeId);
    if (it == m_edgeMap.end()) return;

    const Edge& edge = it->second;

    // Adjacency list'ten kaldır
    auto removeFromAdj = [&](uint32_t nodeId) {
        auto adjIt = m_adjacency.find(nodeId);
        if (adjIt != m_adjacency.end()) {
            auto& vec = adjIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), edgeId), vec.end());
        }
    };
    removeFromAdj(edge.nodeA);
    removeFromAdj(edge.nodeB);

    m_edgeMap.erase(it);
    m_edgeCacheDirty = true;
}

Edge* NetworkGraph::GetEdge(uint32_t edgeId) {
    auto it = m_edgeMap.find(edgeId);
    return (it != m_edgeMap.end()) ? &it->second : nullptr;
}

const Edge* NetworkGraph::GetEdge(uint32_t edgeId) const {
    auto it = m_edgeMap.find(edgeId);
    return (it != m_edgeMap.end()) ? &it->second : nullptr;
}

std::vector<Edge> NetworkGraph::GetEdgeList() const {
    std::vector<Edge> result;
    result.reserve(m_edgeMap.size());
    for (const auto& pair : m_edgeMap) {
        result.push_back(pair.second);
    }
    return result;
}

void NetworkGraph::RebuildEdgeCache() const {
    m_edgeCache.clear();
    m_edgeCache.reserve(m_edgeMap.size());
    for (auto& pair : m_edgeMap) {
        m_edgeCache.push_back(pair.second);
    }
    m_edgeCacheDirty = false;
}

std::vector<Edge>& NetworkGraph::GetEdges() {
    if (m_edgeCacheDirty) RebuildEdgeCache();
    return m_edgeCache;
}

const std::vector<Edge>& NetworkGraph::GetEdges() const {
    if (m_edgeCacheDirty) RebuildEdgeCache();
    return m_edgeCache;
}

// ==================== TOPOLOJİK SORGULAR ====================

std::vector<uint32_t> NetworkGraph::GetConnectedEdges(uint32_t nodeId) const {
    auto it = m_adjacency.find(nodeId);
    if (it != m_adjacency.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint32_t> NetworkGraph::GetUpstreamNodes(uint32_t nodeId) const {
    std::vector<uint32_t> result;
    auto edges = GetConnectedEdges(nodeId);
    for (uint32_t edgeId : edges) {
        const auto* edge = GetEdge(edgeId);
        if (edge && edge->nodeB == nodeId) {
            result.push_back(edge->nodeA);
        }
    }
    return result;
}

std::vector<uint32_t> NetworkGraph::GetDownstreamNodes(uint32_t nodeId) const {
    std::vector<uint32_t> result;
    auto edges = GetConnectedEdges(nodeId);
    for (uint32_t edgeId : edges) {
        const auto* edge = GetEdge(edgeId);
        if (edge && edge->nodeA == nodeId) {
            result.push_back(edge->nodeB);
        }
    }
    return result;
}

void NetworkGraph::Clear() {
    m_nodeMap.clear();
    m_edgeMap.clear();
    m_adjacency.clear();
    m_nodeCache.clear();
    m_edgeCache.clear();
    m_nodeCacheDirty = true;
    m_edgeCacheDirty = true;
    m_nextNodeId = 1;
    m_nextEdgeId = 1;
}

} // namespace mep
} // namespace vkt
