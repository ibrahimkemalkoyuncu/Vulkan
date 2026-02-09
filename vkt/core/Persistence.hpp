/**
 * @file Persistence.hpp
 * @brief Proje Dosya Serileştirme
 * 
 * JSON tabanlı kaydetme/yükleme işlemleri.
 */

#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "mep/NetworkGraph.hpp"

using json = nlohmann::json;

namespace vkt {
namespace core {

/**
 * @class Persistence
 * @brief Proje verilerini JSON formatında serileştiren sınıf
 */
class Persistence {
public:
    /**
     * @brief Tesisat ağını JSON'a dönüştürür
     */
    static json SerializeNetwork(const mep::NetworkGraph& network);

    /**
     * @brief JSON'dan tesisat ağını yükler
     */
    static mep::NetworkGraph DeserializeNetwork(const json& j);

    /**
     * @brief Tüm proje dosyasını kaydeder
     */
    static bool SaveProject(const std::string& path, const mep::NetworkGraph& network);

    /**
     * @brief Proje dosyasını yükler
     */
    static bool LoadProject(const std::string& path, mep::NetworkGraph& network);

private:
    static json SerializeNode(const mep::Node& node);
    static json SerializeEdge(const mep::Edge& edge);
    static mep::Node DeserializeNode(const json& j);
    static mep::Edge DeserializeEdge(const json& j);
};

} // namespace core
} // namespace vkt
