/**
 * @file ChartGenerator.cpp
 * @brief SVG chart generation for MEP analysis visualization
 */

#include "mep/ChartGenerator.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vkt {
namespace mep {

// ── Color palette ───────────────────────────────────────────────

static const char* kBarColors[] = {
    "#2196F3", "#4CAF50", "#FF9800", "#9C27B0", "#F44336",
    "#00BCD4", "#795548", "#607D8B", "#E91E63", "#3F51B5"
};
static const int kBarColorCount = 10;

static const char* kPieColors[] = {
    "#1976D2", "#388E3C", "#F57C00", "#7B1FA2", "#D32F2F",
    "#0097A7", "#5D4037", "#455A64", "#C2185B", "#303F9F",
    "#689F38", "#FFA000"
};
static const int kPieColorCount = 12;

// ── Bar Chart ───────────────────────────────────────────────────

std::string ChartGenerator::BarChartSVG(const std::map<int, double>& dnLengths,
                                         const std::string& title) {
    if (dnLengths.empty()) {
        return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 50\">"
               "<text x=\"50\" y=\"25\" text-anchor=\"middle\" font-size=\"8\">Veri yok</text></svg>";
    }

    const int W = 600, H = 400;
    const int marginL = 70, marginR = 20, marginT = 50, marginB = 60;
    const int plotW = W - marginL - marginR;
    const int plotH = H - marginT - marginB;

    double maxVal = 0;
    for (const auto& [dn, len] : dnLengths)
        maxVal = std::max(maxVal, len);
    if (maxVal <= 0) maxVal = 1.0;

    int barCount = static_cast<int>(dnLengths.size());
    double barWidth = static_cast<double>(plotW) / barCount * 0.7;
    double gap = static_cast<double>(plotW) / barCount * 0.3;

    std::ostringstream svg;
    svg << std::fixed << std::setprecision(1);

    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
        << W << " " << H << "\" width=\"" << W << "\" height=\"" << H << "\">\n";

    // Background
    svg << "<rect width=\"" << W << "\" height=\"" << H
        << "\" fill=\"white\" stroke=\"#ccc\" stroke-width=\"1\"/>\n";

    // Title
    svg << "<text x=\"" << W / 2 << "\" y=\"30\" text-anchor=\"middle\" "
        << "font-family=\"Arial\" font-size=\"16\" font-weight=\"bold\" fill=\"#333\">"
        << title << "</text>\n";

    // Y axis
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT
        << "\" x2=\"" << marginL << "\" y2=\"" << marginT + plotH
        << "\" stroke=\"#666\" stroke-width=\"1\"/>\n";

    // X axis
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT + plotH
        << "\" x2=\"" << marginL + plotW << "\" y2=\"" << marginT + plotH
        << "\" stroke=\"#666\" stroke-width=\"1\"/>\n";

    // Y axis labels (5 ticks)
    for (int i = 0; i <= 4; ++i) {
        double val = maxVal * i / 4.0;
        int y = marginT + plotH - static_cast<int>(plotH * i / 4.0);
        svg << "<text x=\"" << marginL - 5 << "\" y=\"" << y + 4
            << "\" text-anchor=\"end\" font-size=\"10\" fill=\"#666\">"
            << val << " m</text>\n";
        svg << "<line x1=\"" << marginL << "\" y1=\"" << y
            << "\" x2=\"" << marginL + plotW << "\" y2=\"" << y
            << "\" stroke=\"#eee\" stroke-width=\"1\"/>\n";
    }

    // Bars
    int idx = 0;
    for (const auto& [dn, len] : dnLengths) {
        double x = marginL + idx * (barWidth + gap) + gap / 2.0;
        double barH = (len / maxVal) * plotH;
        double y = marginT + plotH - barH;

        svg << "<rect x=\"" << x << "\" y=\"" << y
            << "\" width=\"" << barWidth << "\" height=\"" << barH
            << "\" fill=\"" << kBarColors[idx % kBarColorCount]
            << "\" rx=\"2\" ry=\"2\"/>\n";

        // Value on top of bar
        svg << "<text x=\"" << x + barWidth / 2 << "\" y=\"" << y - 5
            << "\" text-anchor=\"middle\" font-size=\"9\" fill=\"#333\">"
            << len << "</text>\n";

        // DN label below
        svg << "<text x=\"" << x + barWidth / 2 << "\" y=\"" << marginT + plotH + 18
            << "\" text-anchor=\"middle\" font-size=\"10\" fill=\"#333\">DN"
            << dn << "</text>\n";

        idx++;
    }

    svg << "</svg>";
    return svg.str();
}

// ── Line Chart ──────────────────────────────────────────────────

std::string ChartGenerator::PressureProfileSVG(const std::vector<double>& pressures,
                                                const std::vector<std::string>& labels,
                                                const std::string& title) {
    if (pressures.empty()) {
        return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 50\">"
               "<text x=\"50\" y=\"25\" text-anchor=\"middle\" font-size=\"8\">Veri yok</text></svg>";
    }

    const int W = 700, H = 400;
    const int marginL = 70, marginR = 30, marginT = 50, marginB = 80;
    const int plotW = W - marginL - marginR;
    const int plotH = H - marginT - marginB;

    double maxP = *std::max_element(pressures.begin(), pressures.end());
    double minP = *std::min_element(pressures.begin(), pressures.end());
    if (std::abs(maxP - minP) < 1e-6) { maxP += 1.0; minP -= 1.0; }
    double range = maxP - minP;

    int n = static_cast<int>(pressures.size());

    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);

    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
        << W << " " << H << "\" width=\"" << W << "\" height=\"" << H << "\">\n";

    svg << "<rect width=\"" << W << "\" height=\"" << H
        << "\" fill=\"white\" stroke=\"#ccc\" stroke-width=\"1\"/>\n";

    svg << "<text x=\"" << W / 2 << "\" y=\"30\" text-anchor=\"middle\" "
        << "font-family=\"Arial\" font-size=\"16\" font-weight=\"bold\" fill=\"#333\">"
        << title << "</text>\n";

    // Axes
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT
        << "\" x2=\"" << marginL << "\" y2=\"" << marginT + plotH
        << "\" stroke=\"#666\" stroke-width=\"1\"/>\n";
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT + plotH
        << "\" x2=\"" << marginL + plotW << "\" y2=\"" << marginT + plotH
        << "\" stroke=\"#666\" stroke-width=\"1\"/>\n";

    // Build polyline points
    std::string polyPoints;
    for (int i = 0; i < n; ++i) {
        double px = marginL + (n > 1 ? static_cast<double>(i) / (n - 1) * plotW : plotW / 2.0);
        double py = marginT + plotH - ((pressures[i] - minP) / range) * plotH;
        polyPoints += std::to_string(static_cast<int>(px)) + "," +
                      std::to_string(static_cast<int>(py)) + " ";
    }

    svg << "<polyline points=\"" << polyPoints
        << "\" fill=\"none\" stroke=\"#1976D2\" stroke-width=\"2\" stroke-linecap=\"round\"/>\n";

    // Data points + labels
    for (int i = 0; i < n; ++i) {
        double px = marginL + (n > 1 ? static_cast<double>(i) / (n - 1) * plotW : plotW / 2.0);
        double py = marginT + plotH - ((pressures[i] - minP) / range) * plotH;

        svg << "<circle cx=\"" << static_cast<int>(px) << "\" cy=\"" << static_cast<int>(py)
            << "\" r=\"4\" fill=\"#1976D2\"/>\n";

        // Value label
        svg << "<text x=\"" << static_cast<int>(px) << "\" y=\"" << static_cast<int>(py) - 8
            << "\" text-anchor=\"middle\" font-size=\"9\" fill=\"#333\">"
            << pressures[i] << "</text>\n";

        // X label
        if (i < static_cast<int>(labels.size())) {
            svg << "<text x=\"" << static_cast<int>(px) << "\" y=\"" << marginT + plotH + 18
                << "\" text-anchor=\"middle\" font-size=\"9\" fill=\"#666\" "
                << "transform=\"rotate(30 " << static_cast<int>(px) << " "
                << marginT + plotH + 18 << ")\">"
                << labels[i] << "</text>\n";
        }
    }

    svg << "</svg>";
    return svg.str();
}

// ── Pie Chart ───────────────────────────────────────────────────

std::string ChartGenerator::PieChartSVG(const std::map<std::string, double>& materials,
                                         const std::string& title) {
    if (materials.empty()) {
        return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 50\">"
               "<text x=\"50\" y=\"25\" text-anchor=\"middle\" font-size=\"8\">Veri yok</text></svg>";
    }

    const int W = 500, H = 400;
    const double cx = 200, cy = 210, r = 140;

    double total = 0;
    for (const auto& [name, val] : materials) total += val;
    if (total <= 0) total = 1.0;

    std::ostringstream svg;
    svg << std::fixed << std::setprecision(1);

    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
        << W << " " << H << "\" width=\"" << W << "\" height=\"" << H << "\">\n";

    svg << "<rect width=\"" << W << "\" height=\"" << H
        << "\" fill=\"white\" stroke=\"#ccc\" stroke-width=\"1\"/>\n";

    svg << "<text x=\"" << W / 2 << "\" y=\"30\" text-anchor=\"middle\" "
        << "font-family=\"Arial\" font-size=\"16\" font-weight=\"bold\" fill=\"#333\">"
        << title << "</text>\n";

    double startAngle = 0;
    int idx = 0;
    double legendY = 60;

    for (const auto& [name, val] : materials) {
        double fraction = val / total;
        double sweepAngle = fraction * 2.0 * M_PI;

        double x1 = cx + r * std::cos(startAngle);
        double y1 = cy + r * std::sin(startAngle);
        double x2 = cx + r * std::cos(startAngle + sweepAngle);
        double y2 = cy + r * std::sin(startAngle + sweepAngle);

        int largeArc = (sweepAngle > M_PI) ? 1 : 0;

        svg << "<path d=\"M " << cx << " " << cy
            << " L " << x1 << " " << y1
            << " A " << r << " " << r << " 0 " << largeArc << " 1 "
            << x2 << " " << y2 << " Z\""
            << " fill=\"" << kPieColors[idx % kPieColorCount] << "\""
            << " stroke=\"white\" stroke-width=\"2\"/>\n";

        // Legend
        double legendX = 360;
        svg << "<rect x=\"" << legendX << "\" y=\"" << legendY - 8
            << "\" width=\"12\" height=\"12\" fill=\""
            << kPieColors[idx % kPieColorCount] << "\"/>\n";
        svg << "<text x=\"" << legendX + 16 << "\" y=\"" << legendY + 2
            << "\" font-size=\"11\" fill=\"#333\">"
            << name << " (" << fraction * 100.0 << "%)</text>\n";

        startAngle += sweepAngle;
        legendY += 22;
        idx++;
    }

    svg << "</svg>";
    return svg.str();
}

} // namespace mep
} // namespace vkt
