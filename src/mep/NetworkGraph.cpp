/**
 * @file NetworkGraph.cpp
 * @brief Tesisat Şebeke Veri Yapısı İmplementasyonu
 */

#include "mep/NetworkGraph.hpp"
#include <algorithm>

namespace vkt {
namespace mep {

uint32_t NetworkGraph::AddNode(const Node& node) {
    Node newNode = node;
    newNode.id = m_nextNodeId++;
    m_nodes.push_back(newNode);
    return newNode.id;
}

void NetworkGraph::RemoveNode(uint32_t nodeId) {
    // Önce bağlı edgeleri sil
    auto edges = GetConnectedEdges(nodeId);
    for (uint32_t edgeId : edges) {
        RemoveEdge(edgeId);
    }
    
    // Node'u sil
    m_nodes.erase(
        std::remove_if(m_nodes.begin(), m_nodes.end(),
            [nodeId](const Node& n) { return n.id == nodeId; }),
        m_nodes.end()
    );
}

Node* NetworkGraph::GetNode(uint32_t nodeId) {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
        [nodeId](const Node& n) { return n.id == nodeId; });
    return (it != m_nodes.end()) ? &(*it) : nullptr;
}

const Node* NetworkGraph::GetNode(uint32_t nodeId) const {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
        [nodeId](const Node& n) { return n.id == nodeId; });
    return (it != m_nodes.end()) ? &(*it) : nullptr;
}

uint32_t NetworkGraph::AddEdge(const Edge& edge) {
    Edge newEdge = edge;
    newEdge.id = m_nextEdgeId++;
    m_edges.push_back(newEdge);
    return newEdge.id;
}

void NetworkGraph::RemoveEdge(uint32_t edgeId) {
    m_edges.erase(
        std::remove_if(m_edges.begin(), m_edges.end(),
            [edgeId](const Edge& e) { return e.id == edgeId; }),
        m_edges.end()
    );
}

Edge* NetworkGraph::GetEdge(uint32_t edgeId) {
    auto it = std::find_if(m_edges.begin(), m_edges.end(),
        [edgeId](const Edge& e) { return e.id == edgeId; });
    return (it != m_edges.end()) ? &(*it) : nullptr;
}

const Edge* NetworkGraph::GetEdge(uint32_t edgeId) const {
    auto it = std::find_if(m_edges.begin(), m_edges.end(),
        [edgeId](const Edge& e) { return e.id == edgeId; });
    return (it != m_edges.end()) ? &(*it) : nullptr;
}

std::vector<uint32_t> NetworkGraph::GetConnectedEdges(uint32_t nodeId) const {
    std::vector<uint32_t> result;
    for (const auto& edge : m_edges) {
        if (edge.nodeA == nodeId || edge.nodeB == nodeId) {
            result.push_back(edge.id);
        }
    }
    return result;
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
    m_nodes.clear();
    m_edges.clear();
    m_nextNodeId = 1;
    m_nextEdgeId = 1;
}

} // namespace mep
} // namespace vkt
