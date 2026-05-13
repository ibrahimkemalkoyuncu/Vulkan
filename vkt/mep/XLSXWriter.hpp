#pragma once

#include "mep/NetworkGraph.hpp"
#include "mep/HydraulicSolver.hpp"
#include <string>

namespace vkt::mep {

/**
 * @class XLSXWriter
 * @brief Tesisat şebekesini Excel XML Spreadsheet 2003 formatında dışa aktarır.
 *
 * Üretilen dosya (.xls uzantısıyla) Excel 2003+ ve LibreOffice Calc tarafından
 * doğrudan açılabilir. Harici kütüphane gerekmez.
 */
class XLSXWriter {
public:
    /**
     * @brief Boru metrajı + armatür listesi raporu
     * @param path Kayıt yolu (.xls)
     * @param network Kaynak şebeke
     * @return Başarı durumu
     */
    static bool ExportQuantitySurvey(const std::string& path, const NetworkGraph& network);

    /**
     * @brief Hidrolik analiz sonuçları dahil tam proje raporu
     * @param path Kayıt yolu (.xls)
     * @param network Kaynak şebeke
     * @param solver Son çalıştırılan hidrolik çözücü (sonuçlar için)
     * @return Başarı durumu
     */
    static bool ExportProjectReport(const std::string& path,
                                    const NetworkGraph& network,
                                    const HydraulicSolver* solver = nullptr);
};

} // namespace vkt::mep
