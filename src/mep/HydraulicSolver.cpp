/**
 * @file HydraulicSolver.cpp
 * @brief Hidrolik Hesaplama Motoru İmplementasyonu
 */

#include "mep/HydraulicSolver.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace vkt {
namespace mep {

constexpr double PI = 3.14159265358979323846;
constexpr double WATER_DENSITY = 1000.0; // kg/m³
constexpr double GRAVITY = 9.81; // m/s²

HydraulicSolver::HydraulicSolver(NetworkGraph& network)
    : m_network(network) {
}

void HydraulicSolver::Solve() {
    std::cout << "═══════════════════════════════════════" << std::endl;
    std::cout << "  TEMİZ SU ŞEBEKE ANALİZİ (TS EN 806-3)" << std::endl;
    std::cout << "═══════════════════════════════════════" << std::endl;

    // 1. Load Unit'lerden debi hesapla
    for (auto& node : m_network.GetNodes()) {
        if (node.loadUnit > 0.0) {
            node.flowRate_m3s = CalculateFlowFromLU(node.loadUnit);
        }
    }

    // 2. Her boru için hidrolik hesap
    for (auto& edge : m_network.GetEdges()) {
        if (edge.type != EdgeType::Supply) continue;

        // Debi ata
        edge.flowRate_m3s = 0.001; // Örnek, gerçekte topological hesap gerekli

        // Hız hesapla
        double area_m2 = PI * std::pow(edge.diameter_mm / 2000.0, 2);
        edge.velocity_ms = edge.flowRate_m3s / area_m2;

        // Sürtünme kaybı (Darcy-Weisbach)
        edge.headLoss_m = CalculateHeadLoss(edge);

        // Lokal kayıplar (Zeta)
        edge.localLoss_Pa = CalculateLocalLoss(edge);

        std::cout << "Boru " << edge.id << ": "
                  << "Q=" << edge.flowRate_m3s * 1000.0 << " L/s, "
                  << "v=" << edge.velocity_ms << " m/s, "
                  << "h=" << edge.headLoss_m << " m, "
                  << "ΔP_local=" << edge.localLoss_Pa / 1000.0 << " kPa"
                  << std::endl;
    }

    std::cout << "Besleme analizi tamamlandı." << std::endl;
}

void HydraulicSolver::SolveDrainage() {
    std::cout << "═══════════════════════════════════════" << std::endl;
    std::cout << "  ATIK SU ŞEBEKE ANALİZİ (EN 12056)" << std::endl;
    std::cout << "═══════════════════════════════════════" << std::endl;

    for (auto& edge : m_network.GetEdges()) {
        if (edge.type != EdgeType::Drainage) continue;

        // DU'dan debi hesapla
        edge.flowRate_m3s = CalculateFlowFromDU(edge.cumulativeDU);

        // Eğime göre boru seç
        double minDiameter = SelectDrainPipeDiameter(edge.flowRate_m3s * 1000.0, edge.slope);
        edge.diameter_mm = std::max(edge.diameter_mm, minDiameter);

        std::cout << "Drenaj Borusu " << edge.id << ": "
                  << "DU=" << edge.cumulativeDU << ", "
                  << "Q=" << edge.flowRate_m3s * 1000.0 << " L/s, "
                  << "Ø=" << edge.diameter_mm << " mm"
                  << std::endl;
    }

    std::cout << "Drenaj analizi tamamlandı." << std::endl;
}

CriticalPathResult HydraulicSolver::CalculateCriticalPath() {
    std::cout << "═══════════════════════════════════════" << std::endl;
    std::cout << "  KRİTİK DEVRE ANALİZİ (POMPA YÜKSEKLIĞI)" << std::endl;
    std::cout << "═══════════════════════════════════════" << std::endl;

    CriticalPathResult result;
    std::vector<uint32_t> currentPath;
    std::vector<uint32_t> bestPath;
    double maxLoss = 0.0;

    // Kaynak düğümünü bul
    for (const auto& node : m_network.GetNodes()) {
        if (node.type == NodeType::Source) {
            DFS(node.id, currentPath, bestPath, maxLoss, 0.0);
        }
    }

    result.criticalPath = bestPath;
    result.totalHeadLoss_m = maxLoss;
    result.requiredPumpHead_m = maxLoss + 15.0; // +15m emniyet

    if (!bestPath.empty()) {
        result.disadvantagedNodeId = bestPath.back();
        std::cout << "En dezavantajlı düğüm: " << result.disadvantagedNodeId << std::endl;
    }

    std::cout << "Toplam kayıp: " << result.totalHeadLoss_m << " m" << std::endl;
    std::cout << "Gerekli pompa yüksekliği: " << result.requiredPumpHead_m << " mSS" << std::endl;
    return result;
}

double HydraulicSolver::CalculateFlowFromLU(double loadUnit) const {
    // Basitleştirilmiş formül (gerçekte eğri tablosu gerekli)
    return 0.1 * std::sqrt(loadUnit) / 1000.0; // m³/s
}

double HydraulicSolver::CalculateHeadLoss(const Edge& edge) {
    double D = edge.diameter_mm / 1000.0; // m
    double L = edge.length_m;
    double v = edge.velocity_ms;
    double roughness = edge.roughness_mm / 1000.0; // m

    // Reynolds sayısı
    double Re = (v * D) / 1e-6; // Kinematik viskozite ~1e-6 m²/s

    // Haaland denklemi ile sürtünme faktörü
    double f = HaalandFriction(Re, roughness / D);

    // Darcy-Weisbach
    double h_f = f * (L / D) * (v * v) / (2.0 * GRAVITY);
    return h_f;
}

double HydraulicSolver::HaalandFriction(double Re, double relativeRoughness) const {
    if (Re < 2300.0) {
        // Laminar akış
        return 64.0 / Re;
    }

    // Haaland denklemi (türbülanslı)
    double term1 = relativeRoughness / 3.7;
    double term2 = 6.9 / Re;
    double f_inv = -1.8 * std::log10(std::pow(term1, 1.11) + term2);
    return 1.0 / (f_inv * f_inv);
}

double HydraulicSolver::CalculateLocalLoss(const Edge& edge) {
    if (edge.zeta < 0.01) return 0.0;

    double v = edge.velocity_ms;
    // ΔP = ζ × ρ × v² / 2
    return edge.zeta * WATER_DENSITY * v * v / 2.0; // Pa
}

double HydraulicSolver::CalculateFlowFromDU(double dischargeUnit) const {
    // EN 12056 eğrisi (basitleştirilmiş)
    return 0.5 * std::sqrt(dischargeUnit) / 1000.0; // m³/s
}

double HydraulicSolver::SelectDrainPipeDiameter(double flowRate_Ls, double slope) const {
    // EN 12056 tablo 2 (Basitleştirilmiş)
    if (flowRate_Ls < 1.0) return 50.0;
    if (flowRate_Ls < 2.0) return 75.0;
    if (flowRate_Ls < 5.0) return 110.0;
    if (flowRate_Ls < 10.0) return 160.0;
    return 200.0;
}

void HydraulicSolver::DFS(uint32_t nodeId, std::vector<uint32_t>& path,
                          std::vector<uint32_t>& bestPath, double& maxLoss, double currentLoss) {
    path.push_back(nodeId);

    auto edges = m_network.GetConnectedEdges(nodeId);
    if (edges.empty()) {
        // Leaf node - kontrol et
        if (currentLoss > maxLoss) {
            maxLoss = currentLoss;
            bestPath = path;
        }
    } else {
        for (uint32_t edgeId : edges) {
            const auto* edge = m_network.GetEdge(edgeId);
            if (edge && edge->type == EdgeType::Supply) {
                uint32_t nextNode = (edge->nodeA == nodeId) ? edge->nodeB : edge->nodeA;
                DFS(nextNode, path, bestPath, maxLoss, currentLoss + edge->headLoss_m);
            }
        }
    }

    path.pop_back();
}

} // namespace mep
} // namespace vkt
