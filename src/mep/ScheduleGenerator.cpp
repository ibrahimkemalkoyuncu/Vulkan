/**
 * @file ScheduleGenerator.cpp
 * @brief Tesisat Metraj ve Rapor İmplementasyonu
 */

#include "mep/ScheduleGenerator.hpp"
#include <sstream>
#include <iomanip>
#include <map>

namespace vkt {
namespace mep {

ScheduleGenerator::ScheduleGenerator(const NetworkGraph& network)
    : m_network(network) {
}

std::string ScheduleGenerator::GenerateHydraulicReport() const {
    std::ostringstream ss;
    
    ss << "# HİDROLİK ANALİZ RAPORU\n\n";
    ss << "**Proje:** Mekanik Tesisat Projesi\n";
    ss << "**Standart:** TS EN 806-3 + EN 12056\n";
    ss << "**Tarih:** " << __DATE__ << "\n\n";
    
    ss << "---\n\n";
    
    ss << "## 1. TEMİZ SU SİSTEMİ\n\n";
    ss << FormatSupplySystem();
    
    ss << "\n## 2. ATIK SU SİSTEMİ\n\n";
    ss << FormatDrainageSystem();
    
    ss << "\n## 3. DÜĞÜM LİSTESİ\n\n";
    ss << FormatNodeTable();
    
    ss << "\n## 4. BORU LİSTESİ\n\n";
    ss << FormatEdgeTable();
    
    return ss.str();
}

std::string ScheduleGenerator::FormatSupplySystem() const {
    std::ostringstream ss;
    
    ss << "| Boru ID | Çap (mm) | Uzunluk (m) | Debi (L/s) | Hız (m/s) | Kayıp (m) | Zeta | ΔP_lokal (kPa) |\n";
    ss << "|---------|----------|-------------|------------|-----------|-----------|------|----------------|\n";
    
    for (const auto& edge : m_network.GetEdges()) {
        if (edge.type != EdgeType::Supply) continue;
        
        ss << "| " << edge.id 
           << " | " << std::fixed << std::setprecision(1) << edge.diameter_mm
           << " | " << std::fixed << std::setprecision(2) << edge.length_m
           << " | " << std::fixed << std::setprecision(3) << edge.flowRate_m3s * 1000.0
           << " | " << std::fixed << std::setprecision(2) << edge.velocity_ms
           << " | " << std::fixed << std::setprecision(3) << edge.headLoss_m
           << " | " << std::fixed << std::setprecision(2) << edge.zeta
           << " | " << std::fixed << std::setprecision(2) << edge.localLoss_Pa / 1000.0
           << " |\n";
    }
    
    return ss.str();
}

std::string ScheduleGenerator::FormatDrainageSystem() const {
    std::ostringstream ss;
    
    ss << "| Boru ID | Çap (mm) | Eğim (%) | Kümülatif DU | Debi (L/s) | Malzeme |\n";
    ss << "|---------|----------|----------|--------------|------------|----------|\n";
    
    for (const auto& edge : m_network.GetEdges()) {
        if (edge.type != EdgeType::Drainage) continue;
        
        ss << "| " << edge.id
           << " | " << std::fixed << std::setprecision(1) << edge.diameter_mm
           << " | " << std::fixed << std::setprecision(1) << (edge.slope * 100.0)
           << " | " << std::fixed << std::setprecision(1) << edge.cumulativeDU
           << " | " << std::fixed << std::setprecision(3) << edge.flowRate_m3s * 1000.0
           << " | " << edge.material
           << " |\n";
    }
    
    return ss.str();
}

std::string ScheduleGenerator::FormatNodeTable() const {
    std::ostringstream ss;
    
    ss << "| ID | Tip | Pozisyon (x,y,z) | LU/DU | İstenilen Basınç (bar) |\n";
    ss << "|----|-----|------------------|-------|------------------------|\n";
    
    for (const auto& node : m_network.GetNodes()) {
        std::string typeStr;
        switch (node.type) {
            case NodeType::Source: typeStr = "Kaynak"; break;
            case NodeType::Fixture: typeStr = "Armatür"; break;
            case NodeType::Junction: typeStr = "Bağlantı"; break;
            case NodeType::Tank: typeStr = "Depo"; break;
            case NodeType::Pump: typeStr = "Pompa"; break;
            case NodeType::Drain: typeStr = "Tahliye"; break;
            default: typeStr = "Bilinmeyen"; break;
        }
        
        ss << "| " << node.id
           << " | " << typeStr
           << " | (" << std::fixed << std::setprecision(1) 
           << node.position.x << "," << node.position.y << "," << node.position.z << ")"
           << " | " << std::fixed << std::setprecision(2) << node.loadUnit
           << " | " << std::fixed << std::setprecision(1) << (node.pressureDesired_Pa / 100000.0)
           << " |\n";
    }
    
    return ss.str();
}

std::string ScheduleGenerator::FormatEdgeTable() const {
    std::ostringstream ss;
    
    ss << "| ID | Başlangıç | Bitiş | Tip | Çap (mm) | Uzunluk (m) | Malzeme |\n";
    ss << "|----|-----------|-------|-----|----------|-------------|---------|\n";
    
    for (const auto& edge : m_network.GetEdges()) {
        std::string typeStr;
        switch (edge.type) {
            case EdgeType::Supply: typeStr = "Besleme"; break;
            case EdgeType::Drainage: typeStr = "Drenaj"; break;
            case EdgeType::Vent: typeStr = "Havalandırma"; break;
            default: typeStr = "Bilinmeyen"; break;
        }
        
        ss << "| " << edge.id
           << " | " << edge.nodeA
           << " | " << edge.nodeB
           << " | " << typeStr
           << " | " << std::fixed << std::setprecision(1) << edge.diameter_mm
           << " | " << std::fixed << std::setprecision(2) << edge.length_m
           << " | " << edge.material
           << " |\n";
    }
    
    return ss.str();
}

std::vector<MaterialItem> ScheduleGenerator::GenerateMaterialList() const {
    std::vector<MaterialItem> items;
    std::map<std::string, double> pipesByDiameter;
    std::map<std::string, int> fixturesByType;
    
    // Boruları topla
    for (const auto& edge : m_network.GetEdges()) {
        std::ostringstream key;
        key << edge.material << " Ø" << edge.diameter_mm << " mm";
        pipesByDiameter[key.str()] += edge.length_m;
    }
    
    // Armatürleri topla
    for (const auto& node : m_network.GetNodes()) {
        if (node.type == NodeType::Fixture) {
            fixturesByType[node.fixtureType]++;
        }
    }
    
    // Malzeme listesine ekle
    for (const auto& pair : pipesByDiameter) {
        MaterialItem item;
        item.description = pair.first + " Boru";
        item.unit = "m";
        item.quantity = pair.second;
        items.push_back(item);
    }
    
    for (const auto& pair : fixturesByType) {
        MaterialItem item;
        item.description = pair.first;
        item.unit = "adet";
        item.quantity = pair.second;
        items.push_back(item);
    }
    
    return items;
}

std::string ScheduleGenerator::ExportToCSV() const {
    std::ostringstream ss;
    
    ss << "Malzeme,Birim,Miktar,Birim Fiyat,Toplam\n";
    
    auto items = GenerateMaterialList();
    for (const auto& item : items) {
        ss << item.description << ","
           << item.unit << ","
           << std::fixed << std::setprecision(2) << item.quantity << ","
           << "0.00,"
           << "0.00\n";
    }
    
    return ss.str();
}

} // namespace mep
} // namespace vkt
