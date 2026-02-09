/**
 * @file ScheduleGenerator.hpp
 * @brief Tesisat Metraj ve Rapor Üretici
 * 
 * Proje metrajı, malzeme listesi ve hidrolik rapor oluşturur.
 */

#pragma once

#include "NetworkGraph.hpp"
#include <string>
#include <vector>

namespace vkt {
namespace mep {

/**
 * @struct MaterialItem
 * @brief Malzeme listesi satırı
 */
struct MaterialItem {
    std::string description;    ///< Tanım
    std::string unit;           ///< Birim (m, adet, vs.)
    double quantity = 0.0;      ///< Miktar
    std::string notes;          ///< Notlar
};

/**
 * @class ScheduleGenerator
 * @brief Metraj ve rapor üretici
 * 
 * Şebekeden malzeme listesi ve hidrolik rapor oluşturur.
 */
class ScheduleGenerator {
public:
    explicit ScheduleGenerator(const NetworkGraph& network);
    
    /**
     * @brief Hidrolik analiz raporu oluştur
     * @return Markdown formatında rapor metni
     */
    std::string GenerateHydraulicReport() const;
    
    /**
     * @brief Malzeme listesi oluştur
     * @return Malzeme kalemleri listesi
     */
    std::vector<MaterialItem> GenerateMaterialList() const;
    
    /**
     * @brief CSV formatında metraj çıktısı
     */
    std::string ExportToCSV() const;

private:
    const NetworkGraph& m_network;
    
    std::string FormatNodeTable() const;
    std::string FormatEdgeTable() const;
    std::string FormatSupplySystem() const;
    std::string FormatDrainageSystem() const;
};

} // namespace mep
} // namespace vkt
