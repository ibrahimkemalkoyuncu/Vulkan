/**
 * @file ScheduleGenerator.cpp
 * @brief Tesisat Metraj ve Rapor İmplementasyonu
 */

#include "mep/ScheduleGenerator.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <map>
#include <QPrinter>
#include <QTextDocument>
#include <QString>

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

// ═══════════════════════════════════════════════════════════
//  PDF EXPORT (Qt6::PrintSupport)
// ═══════════════════════════════════════════════════════════

bool ScheduleGenerator::ExportToPDF(const std::string& filePath, const std::string& projectName) const {
    QString report = QString::fromStdString(GenerateHydraulicReport());

    // Markdown → basit HTML dönüşümü (QPrinter HTML destekler)
    report.replace(QStringLiteral("# "),  QStringLiteral("<h1>"));
    report.replace(QStringLiteral("## "), QStringLiteral("<h2>"));
    report.replace(QStringLiteral("### "),QStringLiteral("<h3>"));
    report.replace(QStringLiteral("\n---\n"), QStringLiteral("<hr/>"));
    report.replace(QStringLiteral("**"), QStringLiteral("<b>"));
    // Satır sonlarını <br> yap
    report.replace(QStringLiteral("\n"), QStringLiteral("<br/>\n"));

    QString html = QStringLiteral(
        "<html><head><meta charset='utf-8'/>"
        "<style>body{font-family:Arial,sans-serif;font-size:10pt;}"
        "h1{color:#1a5276;}h2{color:#1f618d;}table{border-collapse:collapse;width:100%;}"
        "th,td{border:1px solid #ccc;padding:4px 8px;}th{background:#d6eaf8;}"
        "</style></head><body>%1</body></html>"
    ).arg(report);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(QString::fromStdString(filePath));
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);

    return true;
}

// ═══════════════════════════════════════════════════════════
//  EXCEL EXPORT (Office Open XML / SpreadsheetML)
// ═══════════════════════════════════════════════════════════

bool ScheduleGenerator::ExportToExcel(const std::string& filePath) const {
    // SpreadsheetML formatı: .xlsx uzantılı ama zip içermeyen XML.
    // Excel 2003+ ve LibreOffice bu formatı doğrudan açar.
    // Gerçek zip-tabanlı .xlsx için ileride libxlsxwriter eklenebilir.

    std::ofstream f(filePath);
    if (!f.is_open()) return false;

    auto xmlCell = [](const std::string& val, bool header = false) -> std::string {
        std::string style = header ? " ss:StyleID=\"header\"" : "";
        return "<Cell" + style + "><Data ss:Type=\"String\">" + val + "</Data></Cell>";
    };
    auto numCell = [](double val) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << val;
        return "<Cell><Data ss:Type=\"Number\">" + ss.str() + "</Data></Cell>";
    };

    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<?mso-application progid=\"Excel.Sheet\"?>\n"
      << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n"
      << " xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n"
      << "<Styles>\n"
      << " <Style ss:ID=\"header\"><Font ss:Bold=\"1\"/>"
      << "<Interior ss:Color=\"#D6EAF8\" ss:Pattern=\"Solid\"/></Style>\n"
      << "</Styles>\n";

    // --- Sayfa 1: Malzeme Listesi ---
    f << "<Worksheet ss:Name=\"Malzeme Listesi\"><Table>\n";
    f << "<Row>" << xmlCell("Malzeme/Armatür", true)
                 << xmlCell("Birim", true)
                 << xmlCell("Miktar", true) << "</Row>\n";
    for (const auto& item : GenerateMaterialList()) {
        f << "<Row>" << xmlCell(item.description)
                     << xmlCell(item.unit)
                     << numCell(item.quantity) << "</Row>\n";
    }
    f << "</Table></Worksheet>\n";

    // --- Sayfa 2: Temiz Su Boruları ---
    f << "<Worksheet ss:Name=\"Temiz Su\"><Table>\n";
    f << "<Row>" << xmlCell("Boru ID", true)
                 << xmlCell("Çap (mm)", true)
                 << xmlCell("Uzunluk (m)", true)
                 << xmlCell("Debi (L/s)", true)
                 << xmlCell("Hız (m/s)", true)
                 << xmlCell("Kayıp (m)", true)
                 << xmlCell("Malzeme", true) << "</Row>\n";
    for (const auto& e : m_network.GetEdges()) {
        if (e.type != EdgeType::Supply) continue;
        f << "<Row>" << xmlCell(std::to_string(e.id))
                     << numCell(e.diameter_mm)
                     << numCell(e.length_m)
                     << numCell(e.flowRate_m3s * 1000.0)
                     << numCell(e.velocity_ms)
                     << numCell(e.headLoss_m)
                     << xmlCell(e.material) << "</Row>\n";
    }
    f << "</Table></Worksheet>\n";

    // --- Sayfa 3: Drenaj Boruları ---
    f << "<Worksheet ss:Name=\"Drenaj\"><Table>\n";
    f << "<Row>" << xmlCell("Boru ID", true)
                 << xmlCell("Çap (mm)", true)
                 << xmlCell("Eğim (%)", true)
                 << xmlCell("Kümülatif DU", true)
                 << xmlCell("Debi (L/s)", true)
                 << xmlCell("Malzeme", true) << "</Row>\n";
    for (const auto& e : m_network.GetEdges()) {
        if (e.type != EdgeType::Drainage) continue;
        f << "<Row>" << xmlCell(std::to_string(e.id))
                     << numCell(e.diameter_mm)
                     << numCell(e.slope * 100.0)
                     << numCell(e.cumulativeDU)
                     << numCell(e.flowRate_m3s * 1000.0)
                     << xmlCell(e.material) << "</Row>\n";
    }
    f << "</Table></Worksheet>\n";

    f << "</Workbook>\n";
    return f.good();
}

} // namespace mep
} // namespace vkt
