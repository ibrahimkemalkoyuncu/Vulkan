/**
 * @file DocxWriter.hpp
 * @brief OOXML .docx tesisat raporu yazıcısı — ZIP stored, sıfır harici bağımlılık
 */
#pragma once
#include <string>
#include "../mep/NetworkGraph.hpp"
#include "../mep/HydraulicSolver.hpp"

namespace vkt::core {

struct DocxReportParams {
    std::string projectName;
    std::string norm;
    std::string mainMaterial;
    std::string branchMaterial;
    std::string buildingType;
    std::string date;
    double roughness_mm   = 0.007;
    double maxVelocity_ms = 2.0;
};

class DocxWriter {
public:
    /// @brief Ağ verilerinden .docx raporu yazar.
    /// @return true başarılı, false dosya yazma hatası
    static bool Write(const std::string& filePath,
                      const mep::NetworkGraph& network,
                      const mep::CriticalPathResult& crit,
                      const DocxReportParams& params);
};

} // namespace vkt::core
