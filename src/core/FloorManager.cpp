/**
 * @file FloorManager.cpp
 * @brief Çok katlı bina kat yönetimi implementasyonu
 */

#include "core/FloorManager.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <sstream>

namespace vkt {
namespace core {

FloorManager::FloorManager() {}

void FloorManager::BuildStandardFloors(int numFloors, bool hasBasement, double floorHeight) {
    m_floors.clear();

    double elevation = 0.0;

    if (hasBasement) {
        Floor basement;
        basement.index       = -1;
        basement.label       = "Bodrum Kat";
        basement.elevation_m = -floorHeight;
        basement.height_m    = floorHeight;
        m_floors.push_back(basement);
    }

    for (int i = 0; i < numFloors; ++i) {
        Floor f;
        f.index       = i;
        f.height_m    = floorHeight;
        f.elevation_m = elevation;

        if (i == 0) {
            f.label = "Zemin Kat";
        } else {
            f.label = std::to_string(i) + ". Kat";
        }

        m_floors.push_back(f);
        elevation += floorHeight;
    }

    SortFloors();
}

void FloorManager::AddFloor(const Floor& floor) {
    m_floors.push_back(floor);
    SortFloors();
}

void FloorManager::Clear() {
    m_floors.clear();
    m_nodeFloorMap.clear();
    m_entityFloorMap.clear();
}

int FloorManager::GetFloorIndexAtElevation(double z_m) const {
    for (const auto& f : m_floors) {
        double top = f.elevation_m + f.height_m;
        if (z_m >= f.elevation_m && z_m < top) {
            return f.index;
        }
    }
    return -999;
}

const Floor* FloorManager::GetFloor(int index) const {
    for (const auto& f : m_floors) {
        if (f.index == index) return &f;
    }
    return nullptr;
}

void FloorManager::AssignNodeToFloor(uint32_t nodeId, int floorIndex) {
    m_nodeFloorMap[nodeId] = floorIndex;
}

int FloorManager::GetFloorOfNode(uint32_t nodeId) const {
    auto it = m_nodeFloorMap.find(nodeId);
    return (it != m_nodeFloorMap.end()) ? it->second : -999;
}

void FloorManager::AssignEntityToFloor(uint64_t entityId, int floorIndex) {
    m_entityFloorMap[entityId] = floorIndex;
}

int FloorManager::GetFloorOfEntity(uint64_t entityId) const {
    auto it = m_entityFloorMap.find(entityId);
    return (it != m_entityFloorMap.end()) ? it->second : -999;
}

std::vector<uint32_t> FloorManager::GetNodesOnFloor(int floorIndex) const {
    std::vector<uint32_t> result;
    for (const auto& [nodeId, fi] : m_nodeFloorMap) {
        if (fi == floorIndex) result.push_back(nodeId);
    }
    return result;
}

std::string FloorManager::Serialize() const {
    nlohmann::json root;
    root["floors"] = nlohmann::json::array();
    for (const auto& f : m_floors) {
        root["floors"].push_back({
            {"index",       f.index},
            {"label",       f.label},
            {"elevation_m", f.elevation_m},
            {"height_m",    f.height_m}
        });
    }

    root["nodeFloorMap"] = nlohmann::json::object();
    for (const auto& [nodeId, fi] : m_nodeFloorMap) {
        root["nodeFloorMap"][std::to_string(nodeId)] = fi;
    }

    root["entityFloorMap"] = nlohmann::json::object();
    for (const auto& [eid, fi] : m_entityFloorMap) {
        root["entityFloorMap"][std::to_string(eid)] = fi;
    }

    return root.dump(2);
}

bool FloorManager::Deserialize(const std::string& jsonStr) {
    try {
        auto root = nlohmann::json::parse(jsonStr);
        m_floors.clear();
        m_nodeFloorMap.clear();
        m_entityFloorMap.clear();

        for (const auto& fj : root.value("floors", nlohmann::json::array())) {
            Floor f;
            f.index       = fj.value("index", 0);
            f.label       = fj.value("label", "");
            f.elevation_m = fj.value("elevation_m", 0.0);
            f.height_m    = fj.value("height_m", 3.0);
            m_floors.push_back(f);
        }
        SortFloors();

        for (auto& [key, val] : root.value("nodeFloorMap", nlohmann::json::object()).items()) {
            m_nodeFloorMap[static_cast<uint32_t>(std::stoul(key))] = val.get<int>();
        }
        for (auto& [key, val] : root.value("entityFloorMap", nlohmann::json::object()).items()) {
            m_entityFloorMap[static_cast<uint64_t>(std::stoull(key))] = val.get<int>();
        }

        return true;
    } catch (...) {
        return false;
    }
}

void FloorManager::SortFloors() {
    std::sort(m_floors.begin(), m_floors.end(),
              [](const Floor& a, const Floor& b) {
                  return a.elevation_m < b.elevation_m;
              });
}

} // namespace core
} // namespace vkt
