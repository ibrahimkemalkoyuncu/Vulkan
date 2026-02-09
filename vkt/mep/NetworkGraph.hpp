/**
 * @file NetworkGraph.hpp
 * @brief Tesisat Şebeke Veri Yapısı
 * 
 * Borular, bağlantılar ve cihazlardan oluşan graf yapısı.
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "geom/Math.hpp"

namespace vkt {
namespace mep {

/**
 * @enum NodeType
 * @brief Düğüm tipleri
 */
enum class NodeType {
    Junction,      ///< Bağlantı noktası
    Fixture,       ///< Armatür (Lavabo, WC, vs.)
    Source,        ///< Su kaynağı / Şebeke giriş
    Tank,          ///< Depo
    Pump,          ///< Pompa
    Drain          ///< Atık su tahliye noktası
};

/**
 * @enum EdgeType
 * @brief Kenar (boru) tipleri
 */
enum class EdgeType {
    Supply,        ///< Temiz su besleme
    Drainage,      ///< Pis su / Atık su
    Vent           ///< Havalandırma
};

/**
 * @struct Node
 * @brief Şebeke düğümü
 */
struct Node {
    uint32_t id = 0;
    NodeType type = NodeType::Junction;
    geom::Vec3 position{0, 0, 0};
    std::string label;
    
    // Hidrolik veriler
    double pressureDesired_Pa = 0.0;   ///< İstenen basınç (Pa)
    double flowRate_m3s = 0.0;         ///< Debi (m³/s)
    double loadUnit = 0.0;              ///< Yük birimi (LU için Besleme, DU için Drenaj)
    
    // Armatür özellikleri
    std::string fixtureType;            ///< "Lavabo", "WC", "Duş"
    bool isSimultaneous = false;        ///< Eşzamanlı kullanım
};

/**
 * @struct Edge
 * @brief Şebeke kenarı (Boru)
 */
struct Edge {
    uint32_t id = 0;
    uint32_t nodeA = 0;                 ///< Başlangıç düğümü
    uint32_t nodeB = 0;                 ///< Bitiş düğümü
    EdgeType type = EdgeType::Supply;
    
    // Fiziksel özellikler
    double diameter_mm = 20.0;          ///< Boru çapı (mm)
    double length_m = 1.0;              ///< Uzunluk (m)
    double roughness_mm = 0.0015;       ///< Pürüzlülük (mm) - Plastik boru
    
    // Hidrolik veriler
    double flowRate_m3s = 0.0;          ///< Debi (m³/s)
    double velocity_ms = 0.0;           ///< Hız (m/s)
    double headLoss_m = 0.0;            ///< Kayıp (m su sütunu)
    double pressure_Pa = 0.0;           ///< Basınç (Pa)
    
    // Mühendislik verileri (FINE SANI++)
    double zeta = 0.0;                  ///< Lokal kayıp katsayısı (Dirsek, T, vb.)
    double localLoss_Pa = 0.0;          ///< Lokal basınç kaybı (Pa)
    double slope = 0.02;                ///< Eğim (drenaj için, %2 = 0.02)
    double cumulativeDU = 0.0;          ///< Kümülatif DU (EN 12056 için)
    
    std::string material = "PVC";       ///< Boru malzemesi
    std::string label;                  ///< Etiket
};

/**
 * @class NetworkGraph
 * @brief Tesisat şebekesi graf yapısı
 */
class NetworkGraph {
public:
    NetworkGraph() = default;
    
    // Node işlemleri
    uint32_t AddNode(const Node& node);
    void RemoveNode(uint32_t nodeId);
    Node* GetNode(uint32_t nodeId);
    const Node* GetNode(uint32_t nodeId) const;
    std::vector<Node>& GetNodes() { return m_nodes; }
    const std::vector<Node>& GetNodes() const { return m_nodes; }
    
    // Edge işlemleri
    uint32_t AddEdge(const Edge& edge);
    void RemoveEdge(uint32_t edgeId);
    Edge* GetEdge(uint32_t edgeId);
    const Edge* GetEdge(uint32_t edgeId) const;
    std::vector<Edge>& GetEdges() { return m_edges; }
    const std::vector<Edge>& GetEdges() const { return m_edges; }
    
    // Topolojik sorgular
    std::vector<uint32_t> GetConnectedEdges(uint32_t nodeId) const;
    std::vector<uint32_t> GetUpstreamNodes(uint32_t nodeId) const;
    std::vector<uint32_t> GetDownstreamNodes(uint32_t nodeId) const;
    
    void Clear();

private:
    std::vector<Node> m_nodes;
    std::vector<Edge> m_edges;
    uint32_t m_nextNodeId = 1;
    uint32_t m_nextEdgeId = 1;
};

} // namespace mep
} // namespace vkt
