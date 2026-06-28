/**
 * @file SpecGenerator.cpp
 * @brief Teknik Sartname Ureteci Implementasyonu
 */

#include "mep/SpecGenerator.hpp"
#include <sstream>
#include <iomanip>
#include <map>
#include <set>

namespace vkt {
namespace mep {

SpecGenerator::SpecGenerator(const NetworkGraph& network)
    : m_network(network) {}

std::string SpecGenerator::GenerateHeader() const {
    return R"(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<title>VKT Teknik Sartname</title>
<style>
body { font-family: 'Segoe UI', Tahoma, sans-serif; margin: 20px; color: #333; }
h1 { color: #0D1B2A; border-bottom: 2px solid #0D1B2A; padding-bottom: 8px; }
h2 { color: #1B3A5C; margin-top: 24px; }
table { border-collapse: collapse; width: 100%; margin: 12px 0; }
th, td { border: 1px solid #ccc; padding: 6px 10px; text-align: left; }
th { background: #e8f0fe; font-weight: 600; }
tr:nth-child(even) { background: #f9f9f9; }
.note { background: #fff3cd; border-left: 4px solid #ffc107; padding: 8px 12px; margin: 12px 0; }
.compliance { background: #d4edda; border-left: 4px solid #28a745; padding: 8px 12px; margin: 12px 0; }
</style>
</head>
<body>
<h1>VKT Mekanik Tesisat - Teknik Sartname</h1>
)";
}

std::string SpecGenerator::GenerateFooter() const {
    return R"(
<hr>
<p style="font-size:0.85em;color:#888;">VKT Mekanik Tesisat Draw v1.0 tarafindan otomatik uretilmistir.</p>
</body>
</html>
)";
}

std::string SpecGenerator::GeneratePipesSection() const {
    std::ostringstream ss;
    ss << "<h2>1. Boru Malzeme Spesifikasyonlari</h2>\n";

    // Collect unique materials and DN values per edge type
    struct PipeInfo {
        std::string material;
        double diameter_mm;
        double totalLength_m;
        int count;
    };

    // Group by material+DN
    std::map<std::string, PipeInfo> pipeMap;
    for (const auto& [id, edge] : m_network.GetEdgeMap()) {
        std::string key = edge.material + "_" + std::to_string(static_cast<int>(edge.diameter_mm));
        auto& info = pipeMap[key];
        info.material = edge.material;
        info.diameter_mm = edge.diameter_mm;
        info.totalLength_m += edge.length_m;
        info.count++;
    }

    if (pipeMap.empty()) {
        ss << "<p>Sebekede boru tanimli degil.</p>\n";
        return ss.str();
    }

    ss << "<table>\n";
    ss << "<tr><th>Malzeme</th><th>DN (mm)</th><th>Toplam Uzunluk (m)</th><th>Adet</th></tr>\n";
    for (const auto& [key, info] : pipeMap) {
        ss << "<tr><td>" << info.material
           << "</td><td>" << static_cast<int>(info.diameter_mm)
           << "</td><td>" << std::fixed << std::setprecision(1) << info.totalLength_m
           << "</td><td>" << info.count << "</td></tr>\n";
    }
    ss << "</table>\n";

    ss << "<div class=\"note\"><strong>Not:</strong> Tum borular TS EN ISO 15874 (PP-R) / "
       << "TS EN 1452 (PVC-U) / TS EN ISO 21003 (PE-Xc) standartlarina uygun olmalidir.</div>\n";

    return ss.str();
}

std::string SpecGenerator::GenerateFixturesSection() const {
    std::ostringstream ss;
    ss << "<h2>2. Armatur Cizelgesi</h2>\n";

    // Count fixture types
    std::map<std::string, int> fixtureCounts;
    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type == NodeType::Fixture) {
            std::string ftype = node.fixtureType.empty() ? "Bilinmeyen" : node.fixtureType;
            fixtureCounts[ftype]++;
        } else if (node.type == NodeType::Sprinkler) {
            fixtureCounts["Sprinkler"]++;
        } else if (node.type == NodeType::Radiator) {
            fixtureCounts["Radyator"]++;
        } else if (node.type == NodeType::GasAppliance) {
            std::string label = node.label.empty() ? "Gaz Cihazi" : node.label;
            fixtureCounts[label]++;
        }
    }

    if (fixtureCounts.empty()) {
        ss << "<p>Sebekede armatur tanimli degil.</p>\n";
        return ss.str();
    }

    ss << "<table>\n";
    ss << "<tr><th>Armatur Tipi</th><th>Adet</th></tr>\n";
    for (const auto& [ftype, count] : fixtureCounts) {
        ss << "<tr><td>" << ftype << "</td><td>" << count << "</td></tr>\n";
    }
    ss << "</table>\n";

    ss << "<div class=\"note\"><strong>Not:</strong> Armaturler TS EN 200 / TS EN 817 / "
       << "TS EN 997 standartlarina uygun olmalidir. Tum armaturler TSE belgeli olmalidir.</div>\n";

    return ss.str();
}

std::string SpecGenerator::GeneratePumpsSection() const {
    std::ostringstream ss;
    ss << "<h2>3. Pompa / Ekipman Spesifikasyonlari</h2>\n";

    // List pumps and sources
    int pumpCount = 0;
    int sourceCount = 0;
    int boilerCount = 0;
    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type == NodeType::Pump) pumpCount++;
        if (node.type == NodeType::Source || node.type == NodeType::HotSource) sourceCount++;
        if (node.type == NodeType::Boiler) boilerCount++;
        if (node.type == NodeType::FirePump) pumpCount++;
    }

    ss << "<table>\n";
    ss << "<tr><th>Ekipman</th><th>Adet</th><th>Aciklama</th></tr>\n";
    if (sourceCount > 0)
        ss << "<tr><td>Su Kaynagi</td><td>" << sourceCount << "</td><td>Sebeke baglantisi / depo</td></tr>\n";
    if (pumpCount > 0)
        ss << "<tr><td>Pompa</td><td>" << pumpCount << "</td><td>Hidrofor / yangin pompasi</td></tr>\n";
    if (boilerCount > 0)
        ss << "<tr><td>Kazan</td><td>" << boilerCount << "</td><td>Isitma kaynagi (EN 12831)</td></tr>\n";
    ss << "</table>\n";

    return ss.str();
}

std::string SpecGenerator::GenerateComplianceSection() const {
    std::ostringstream ss;
    ss << "<h2>4. Standart Uygunluk Notlari</h2>\n";

    // Check which edge types exist
    std::set<EdgeType> edgeTypes;
    for (const auto& [id, edge] : m_network.GetEdgeMap()) {
        edgeTypes.insert(edge.type);
    }

    ss << "<div class=\"compliance\">\n";
    ss << "<ul>\n";

    if (edgeTypes.count(EdgeType::Supply) || edgeTypes.count(EdgeType::HotWater)) {
        ss << "<li><strong>TS EN 806-3:2006</strong> - Bina ici icme suyu tesisati - "
           << "Boru boyutlandirma (LU yontemi, Darcy-Weisbach)</li>\n";
    }
    if (edgeTypes.count(EdgeType::Drainage)) {
        ss << "<li><strong>EN 12056-2:2000</strong> - Bina ici atik su drenaj sistemi - "
           << "Manning denklemi, DU yontemi, K faktoru</li>\n";
    }
    if (edgeTypes.count(EdgeType::Gas)) {
        ss << "<li><strong>TS EN 1775:2008</strong> - Bina ici dogal gaz tesisati - "
           << "Max hiz 2 m/s, max dP 200 Pa</li>\n";
    }
    if (edgeTypes.count(EdgeType::Heating) || edgeTypes.count(EdgeType::HeatingReturn)) {
        ss << "<li><strong>EN 12831:2003</strong> - Isitma sistemi boyutlandirma - "
           << "dT=20K (70/50 C), radyator secimi EN 442</li>\n";
    }
    if (edgeTypes.count(EdgeType::FireLine)) {
        ss << "<li><strong>EN 12845:2015</strong> - Sabit yangin sondurme sistemleri - "
           << "Yogunluk-alan metodu, sprinkler tasarimi</li>\n";
    }
    if (edgeTypes.count(EdgeType::Duct)) {
        ss << "<li><strong>EN 15665:2009</strong> - Havalandirma sistemleri - "
           << "Kanal boyutlandirma, v_max = 5 m/s</li>\n";
    }

    ss << "<li><strong>TS 11153</strong> - Mekanik tesisat projeleri - Genel kurallar</li>\n";
    ss << "</ul>\n";
    ss << "</div>\n";

    return ss.str();
}

std::string SpecGenerator::GenerateHTML() const {
    std::string html;
    html += GenerateHeader();
    html += GeneratePipesSection();
    html += GenerateFixturesSection();
    html += GeneratePumpsSection();
    html += GenerateComplianceSection();
    html += GenerateFooter();
    return html;
}

std::string SpecGenerator::GenerateSection(const std::string& section) const {
    if (section == "pipes")      return GeneratePipesSection();
    if (section == "fixtures")   return GenerateFixturesSection();
    if (section == "pumps")      return GeneratePumpsSection();
    if (section == "compliance") return GenerateComplianceSection();
    return "<p>Bilinmeyen bolum: " + section + "</p>";
}

} // namespace mep
} // namespace vkt
