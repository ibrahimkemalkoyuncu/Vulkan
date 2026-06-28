/**
 * @file ChartGenerator.hpp
 * @brief SVG chart generation for MEP analysis visualization
 *
 * Generates clean SVG charts: bar (DN distribution), line (pressure profile),
 * pie (material distribution). No external dependencies.
 */

#pragma once

#include <string>
#include <map>
#include <vector>

namespace vkt {
namespace mep {

class ChartGenerator {
public:
    /**
     * @brief Bar chart: DN distribution (pipe lengths by nominal diameter)
     * @param dnLengths Map of DN (mm) -> total length (m)
     * @param title Chart title
     * @return SVG string
     */
    static std::string BarChartSVG(const std::map<int, double>& dnLengths,
                                    const std::string& title = "DN Dagilimi");

    /**
     * @brief Line chart: pressure profile along critical path
     * @param pressures Pressure values at each point (mSS)
     * @param labels Point labels (node names)
     * @param title Chart title
     * @return SVG string
     */
    static std::string PressureProfileSVG(const std::vector<double>& pressures,
                                           const std::vector<std::string>& labels,
                                           const std::string& title = "Basinc Profili");

    /**
     * @brief Pie chart: material distribution (by total length)
     * @param materials Map of material name -> total length (m)
     * @param title Chart title
     * @return SVG string
     */
    static std::string PieChartSVG(const std::map<std::string, double>& materials,
                                    const std::string& title = "Malzeme Dagilimi");
};

} // namespace mep
} // namespace vkt
