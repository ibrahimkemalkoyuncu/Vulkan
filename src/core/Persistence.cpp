/**
 * @file Persistence.cpp
 * @brief Proje Serileştirme İmplementasyonu
 */

#include "core/Persistence.hpp"
#include <fstream>
#include <iostream>

namespace vkt {
namespace core {

json Persistence::SerializeNetwork(const mep::NetworkGraph& network) {
    json j;
    
    // Nodes serileştir
    json nodesArray = json::array();
    for (const auto& node : network.GetNodes()) {
        nodesArray.push_back(SerializeNode(node));
    }
    j["nodes"] = nodesArray;
    
    // Edges serileştir
    json edgesArray = json::array();
    for (const auto& edge : network.GetEdges()) {
        edgesArray.push_back(SerializeEdge(edge));
    }
    j["edges"] = edgesArray;
    
    return j;
}

mep::NetworkGraph Persistence::DeserializeNetwork(const json& j) {
    mep::NetworkGraph network;
    
    // Nodes yükle
    if (j.contains("nodes")) {
        for (const auto& nodeJson : j["nodes"]) {
            auto node = DeserializeNode(nodeJson);
            network.AddNode(node);
        }
    }
    
    // Edges yükle
    if (j.contains("edges")) {
        for (const auto& edgeJson : j["edges"]) {
            auto edge = DeserializeEdge(edgeJson);
            network.AddEdge(edge);
        }
    }
    
    return network;
}

bool Persistence::SaveProject(const std::string& path, const mep::NetworkGraph& network) {
    try {
        json j;
        j["version"] = "1.0.0";
        j["type"] = "VKT_Project";
        j["network"] = SerializeNetwork(network);
        
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "Dosya açılamadı: " << path << std::endl;
            return false;
        }
        
        file << j.dump(2); // Pretty print with 2 spaces
        file.close();
        
        std::cout << "Proje kaydedildi: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Kaydetme hatası: " << e.what() << std::endl;
        return false;
    }
}

bool Persistence::LoadProject(const std::string& path, mep::NetworkGraph& network) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Dosya açılamadı: " << path << std::endl;
            return false;
        }
        
        json j;
        file >> j;
        file.close();
        
        if (j.contains("network")) {
            network = DeserializeNetwork(j["network"]);
        }
        
        std::cout << "Proje yüklendi: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Yükleme hatası: " << e.what() << std::endl;
        return false;
    }
}

json Persistence::SerializeNode(const mep::Node& node) {
    json j;
    j["id"] = node.id;
    j["type"] = static_cast<int>(node.type);
    j["position"] = {node.position.x, node.position.y, node.position.z};
    j["label"] = node.label;
    j["pressureDesired_Pa"] = node.pressureDesired_Pa;
    j["flowRate_m3s"] = node.flowRate_m3s;
    j["loadUnit"] = node.loadUnit;
    j["fixtureType"] = node.fixtureType;
    j["isSimultaneous"] = node.isSimultaneous;
    return j;
}

json Persistence::SerializeEdge(const mep::Edge& edge) {
    json j;
    j["id"] = edge.id;
    j["nodeA"] = edge.nodeA;
    j["nodeB"] = edge.nodeB;
    j["type"] = static_cast<int>(edge.type);
    j["diameter_mm"] = edge.diameter_mm;
    j["length_m"] = edge.length_m;
    j["roughness_mm"] = edge.roughness_mm;
    j["flowRate_m3s"] = edge.flowRate_m3s;
    j["velocity_ms"] = edge.velocity_ms;
    j["headLoss_m"] = edge.headLoss_m;
    j["pressure_Pa"] = edge.pressure_Pa;
    j["zeta"] = edge.zeta;
    j["localLoss_Pa"] = edge.localLoss_Pa;
    j["slope"] = edge.slope;
    j["cumulativeDU"] = edge.cumulativeDU;
    j["material"] = edge.material;
    j["label"] = edge.label;
    return j;
}

mep::Node Persistence::DeserializeNode(const json& j) {
    mep::Node node;
    node.id = j.value("id", 0u);
    node.type = static_cast<mep::NodeType>(j.value("type", 0));
    
    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
        node.position.x = j["position"][0];
        node.position.y = j["position"][1];
        node.position.z = j["position"][2];
    }
    
    node.label = j.value("label", "");
    node.pressureDesired_Pa = j.value("pressureDesired_Pa", 0.0);
    node.flowRate_m3s = j.value("flowRate_m3s", 0.0);
    node.loadUnit = j.value("loadUnit", 0.0);
    node.fixtureType = j.value("fixtureType", "");
    node.isSimultaneous = j.value("isSimultaneous", false);
    
    return node;
}

mep::Edge Persistence::DeserializeEdge(const json& j) {
    mep::Edge edge;
    edge.id = j.value("id", 0u);
    edge.nodeA = j.value("nodeA", 0u);
    edge.nodeB = j.value("nodeB", 0u);
    edge.type = static_cast<mep::EdgeType>(j.value("type", 0));
    edge.diameter_mm = j.value("diameter_mm", 20.0);
    edge.length_m = j.value("length_m", 1.0);
    edge.roughness_mm = j.value("roughness_mm", 0.0015);
    edge.flowRate_m3s = j.value("flowRate_m3s", 0.0);
    edge.velocity_ms = j.value("velocity_ms", 0.0);
    edge.headLoss_m = j.value("headLoss_m", 0.0);
    edge.pressure_Pa = j.value("pressure_Pa", 0.0);
    edge.zeta = j.value("zeta", 0.0);
    edge.localLoss_Pa = j.value("localLoss_Pa", 0.0);
    edge.slope = j.value("slope", 0.02);
    edge.cumulativeDU = j.value("cumulativeDU", 0.0);
    edge.material = j.value("material", "PVC");
    edge.label = j.value("label", "");
    
    return edge;
}

} // namespace core
} // namespace vkt
