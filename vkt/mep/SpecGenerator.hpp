/**
 * @file SpecGenerator.hpp
 * @brief Teknik Şartname Üreteci
 *
 * MEP şebekesinden otomatik teknik şartname HTML çıktısı üretir.
 * Boru malzeme spesifikasyonları, armatür çizelgeleri, pompa özellikleri
 * ve uygunluk notları (TS EN 806-3, EN 12056-2) dahildir.
 */

#pragma once

#include "NetworkGraph.hpp"
#include <string>

namespace vkt {
namespace mep {

/**
 * @class SpecGenerator
 * @brief Teknik şartname HTML üreteci
 *
 * Kullanım:
 * ```cpp
 * SpecGenerator gen(network);
 * std::string html = gen.GenerateHTML();
 * std::string pipes = gen.GenerateSection("pipes");
 * ```
 */
class SpecGenerator {
public:
    explicit SpecGenerator(const NetworkGraph& network);

    /// Tam teknik şartname HTML çıktısı
    std::string GenerateHTML() const;

    /// Belirli bir bölüm: "pipes", "fixtures", "pumps", "compliance"
    std::string GenerateSection(const std::string& section) const;

private:
    const NetworkGraph& m_network;

    std::string GeneratePipesSection() const;
    std::string GenerateFixturesSection() const;
    std::string GeneratePumpsSection() const;
    std::string GenerateComplianceSection() const;
    std::string GenerateHeader() const;
    std::string GenerateFooter() const;
};

} // namespace mep
} // namespace vkt
