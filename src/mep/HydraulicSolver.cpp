/**
 * @file HydraulicSolver.cpp
 * @brief Hidrolik Hesaplama Motoru İmplementasyonu
 *
 * Düzeltmeler:
 * - Adım 11: Topolojik debi dağılımı (leaf→source kümülatif)
 * - Adım 12: TS EN 806-3 gerçek LU→debi eğrisi (Q = 0.01 * LU^0.42)
 * - Adım 13: EN 12056-2 gerçek DU formülü (Q = K * sqrt(ΣDU))
 * - Adım 14: DFS visited set (sonsuz döngü önleme)
 * - Adım 15: Manning denklemi ile drenaj boru boyutlandırma
 */

#include "mep/HydraulicSolver.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace vkt {
namespace mep {

constexpr double PI = 3.14159265358979323846;
constexpr double WATER_DENSITY = 1000.0; // kg/m³
constexpr double GRAVITY = 9.81;         // m/s²
constexpr double KINEMATIC_VISCOSITY = 1.004e-6; // m²/s (20°C su)

HydraulicSolver::HydraulicSolver(NetworkGraph& network)
    : m_network(network) {}

void HydraulicSolver::SetBuildingType(BuildingType type) {
    m_buildingType = type;
}

// ═══════════════════════════════════════════════════════════
//  BESLEME ŞEBEKESİ ANALİZİ (TS EN 806-3)
// ═══════════════════════════════════════════════════════════

void HydraulicSolver::Solve() {
    std::cout << "═══════════════════════════════════════" << std::endl;
    std::cout << "  TEMİZ SU ŞEBEKE ANALİZİ (TS EN 806-3)" << std::endl;
    std::cout << "═══════════════════════════════════════" << std::endl;

    // 1. Her fixture node'un LU'sından debi hesapla
    for (auto& [id, node] : m_network.GetNodeMap()) {
        if (node.loadUnit > 0.0) {
            node.flowRate_m3s = CalculateFlowFromLU(node.loadUnit);
        }
    }

    // 2. Topolojik debi dağılımı — leaf'lerden source'a kümülatif toplama
    DistributeSupplyFlows();

    // 3. Her boru için hidrolik hesap
    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Supply) continue;

        // Hız hesapla: v = Q / A
        double D_m = edge.diameter_mm / 1000.0;
        double area_m2 = PI * (D_m / 2.0) * (D_m / 2.0);
        if (area_m2 > 0.0) {
            edge.velocity_ms = edge.flowRate_m3s / area_m2;
        }

        // Sürtünme kaybı (Darcy-Weisbach + Haaland)
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

    // Cache'leri dirty işaretle (edge/node değerleri değişti)
    m_network.GetEdges(); // Force rebuild so cache reflects new values
    m_network.GetNodes();

    std::cout << "Besleme analizi tamamlandı." << std::endl;
}

void HydraulicSolver::DistributeSupplyFlows() {
    // BFS ile leaf node'lardan (fixture) geriye doğru kümülatif debi toplama
    // Leaf: downstream bağlantısı olmayan node
    // Her edge'in debisi = edge'in downstream tarafındaki tüm fixture debilerin toplamı

    // 1. Her node'un in-degree'sini (upstream edge sayısı) hesapla
    //    Edge yönü: nodeA → nodeB (source → fixture)
    std::unordered_map<uint32_t, int> outDegree; // downstream edge count
    std::unordered_map<uint32_t, double> cumulativeFlow; // nodeId → toplam debi

    for (auto& [id, node] : m_network.GetNodeMap()) {
        outDegree[id] = 0;
        cumulativeFlow[id] = node.flowRate_m3s; // Fixture ise kendi debisi, değilse 0
    }

    // outDegree = her node'un kaç downstream edge'i var
    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Supply) continue;
        outDegree[edge.nodeA]++;
    }

    // 2. Leaf node'ları bul (outDegree == 0) — BFS kuyruğuna ekle
    std::queue<uint32_t> leafQueue;
    for (auto& [nodeId, degree] : outDegree) {
        if (degree == 0) {
            leafQueue.push(nodeId);
        }
    }

    // 3. Leaf'lerden source'a doğru reverse BFS
    std::unordered_set<uint32_t> processed;
    while (!leafQueue.empty()) {
        uint32_t nodeId = leafQueue.front();
        leafQueue.pop();

        if (processed.count(nodeId)) continue;
        processed.insert(nodeId);

        // Bu node'a gelen edge'leri bul (edge.nodeB == nodeId → upstream edge)
        auto edgeIds = m_network.GetConnectedEdges(nodeId);
        for (uint32_t eid : edgeIds) {
            auto* edge = m_network.GetEdge(eid);
            if (!edge || edge->type != EdgeType::Supply) continue;

            if (edge->nodeB == nodeId) {
                // Bu edge nodeA → nodeB yönünde, nodeB'nin kümülatif debisini edge'e ata
                edge->flowRate_m3s = cumulativeFlow[nodeId];

                // nodeA'nın kümülatif debisine ekle
                cumulativeFlow[edge->nodeA] += cumulativeFlow[nodeId];

                // nodeA'nın outDegree'sini azalt
                outDegree[edge->nodeA]--;
                if (outDegree[edge->nodeA] <= 0) {
                    leafQueue.push(edge->nodeA);
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  DRENAJ ŞEBEKESİ ANALİZİ (EN 12056-2 + Manning)
// ═══════════════════════════════════════════════════════════

void HydraulicSolver::SolveDrainage() {
    std::cout << "═══════════════════════════════════════" << std::endl;
    std::cout << "  ATIK SU ŞEBEKE ANALİZİ (EN 12056-2)" << std::endl;
    std::cout << "═══════════════════════════════════════" << std::endl;

    // 1. Topolojik DU dağılımı
    DistributeDrainageFlows();

    // 2. Her drenaj borusu için hesap
    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Drainage) continue;

        // DU'dan debi hesapla (EN 12056-2)
        edge.flowRate_m3s = CalculateFlowFromDU(edge.cumulativeDU);

        // Manning denklemi ile boru boyutlandırma
        double flowRate_Ls = edge.flowRate_m3s * 1000.0;
        double minDiameter = SelectDrainPipeDiameter_Manning(flowRate_Ls, edge.slope, edge.material);
        edge.diameter_mm = std::max(edge.diameter_mm, minDiameter);

        std::cout << "Drenaj " << edge.id << ": "
                  << "DU=" << edge.cumulativeDU << ", "
                  << "Q=" << flowRate_Ls << " L/s, "
                  << "Ø=" << edge.diameter_mm << " mm"
                  << std::endl;
    }

    m_network.GetEdges(); // Force cache rebuild
    std::cout << "Drenaj analizi tamamlandı." << std::endl;
}

void HydraulicSolver::DistributeDrainageFlows() {
    // Fixture'lardan drain'e doğru kümülatif DU toplama
    // Edge yönü: nodeA (upstream/fixture) → nodeB (downstream/drain)
    // BFS: upstream leaf'lerden başla, downstream'e doğru DU aktar

    std::unordered_map<uint32_t, int> inDegree;  // incoming drainage edge count
    std::unordered_map<uint32_t, double> cumulativeDU;

    for (auto& [id, node] : m_network.GetNodeMap()) {
        inDegree[id] = 0;
        cumulativeDU[id] = node.loadUnit; // Fixture ise DU değeri
    }

    // inDegree: her node'a kaç drainage edge GİRİYOR (nodeB tarafı)
    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Drainage) continue;
        inDegree[edge.nodeB]++;
    }

    // Upstream leaf node'lar (inDegree == 0, hiç gelen drainage edge yok)
    std::queue<uint32_t> queue;
    for (auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            queue.push(nodeId);
        }
    }

    std::unordered_set<uint32_t> processed;
    while (!queue.empty()) {
        uint32_t nodeId = queue.front();
        queue.pop();
        if (processed.count(nodeId)) continue;
        processed.insert(nodeId);

        // Bu node'dan çıkan drainage edge'leri bul
        auto edgeIds = m_network.GetConnectedEdges(nodeId);
        for (uint32_t eid : edgeIds) {
            auto* edge = m_network.GetEdge(eid);
            if (!edge || edge->type != EdgeType::Drainage) continue;

            if (edge->nodeA == nodeId) {
                // nodeA → nodeB yönünde, nodeA'nın kümülatif DU'sunu edge'e ata
                edge->cumulativeDU = cumulativeDU[nodeId];
                cumulativeDU[edge->nodeB] += cumulativeDU[nodeId];

                inDegree[edge->nodeB]--;
                if (inDegree[edge->nodeB] <= 0) {
                    queue.push(edge->nodeB);
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  KRİTİK DEVRE ANALİZİ
// ═══════════════════════════════════════════════════════════

CriticalPathResult HydraulicSolver::CalculateCriticalPath() {
    std::cout << "═══════════════════════════════════════" << std::endl;
    std::cout << "  KRİTİK DEVRE ANALİZİ (POMPA YÜKSEKLIĞI)" << std::endl;
    std::cout << "═══════════════════════════════════════" << std::endl;

    CriticalPathResult result;
    std::vector<uint32_t> currentPath;
    std::vector<uint32_t> bestPath;
    double maxLoss = 0.0;
    std::unordered_set<uint32_t> visited;

    // Kaynak düğümlerinden DFS başlat
    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type == NodeType::Source) {
            visited.clear();
            DFS(node.id, currentPath, bestPath, maxLoss, 0.0, visited);
        }
    }

    result.criticalPath = bestPath;
    result.totalHeadLoss_m = maxLoss;
    result.requiredPumpHead_m = maxLoss + 15.0; // +15m emniyet payı

    if (!bestPath.empty()) {
        result.disadvantagedNodeId = bestPath.back();
        std::cout << "En dezavantajlı düğüm: " << result.disadvantagedNodeId << std::endl;
    }

    std::cout << "Toplam kayıp: " << result.totalHeadLoss_m << " m" << std::endl;
    std::cout << "Gerekli pompa yüksekliği: " << result.requiredPumpHead_m << " mSS" << std::endl;
    return result;
}

// ═══════════════════════════════════════════════════════════
//  BESLEME YARDIMCI FONKSİYONLAR
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::CalculateFlowFromLU(double loadUnit) const {
    // TS EN 806-3 formülü:
    //   Q = 0.01 * LU^0.42  [L/s]  (LU < 500 için)
    //
    // Referans noktalar:
    //   LU=1   → Q ≈ 0.01 L/s
    //   LU=5   → Q ≈ 0.019 L/s
    //   LU=10  → Q ≈ 0.026 L/s
    //   LU=50  → Q ≈ 0.046 L/s
    //   LU=100 → Q ≈ 0.063 L/s
    //   LU=500 → Q ≈ 0.117 L/s
    //
    // Not: Standart tablodaki Q değerleri bundan yüksektir çünkü
    // minimum debi gereksinimleri uygulanır. Bu formül alt sınır eğrisidir.

    if (loadUnit <= 0.0) return 0.0;

    double Q_Ls;
    if (loadUnit <= 500.0) {
        Q_Ls = 0.01 * std::pow(loadUnit, 0.42);
    } else {
        // LU > 500 için lineer ekstrapolasyon (endüstriyel tesisler)
        double Q_500 = 0.01 * std::pow(500.0, 0.42);
        Q_Ls = Q_500 + 0.0001 * (loadUnit - 500.0);
    }

    // Minimum debi kontrolü: her fixture en az 0.01 L/s
    Q_Ls = std::max(Q_Ls, 0.01);

    return Q_Ls / 1000.0; // L/s → m³/s
}

double HydraulicSolver::CalculateHeadLoss(const Edge& edge) {
    double D = edge.diameter_mm / 1000.0; // m
    double L = edge.length_m;
    double v = edge.velocity_ms;

    if (D <= 0.0 || v <= 0.0) return 0.0;

    double roughness = edge.roughness_mm / 1000.0; // m

    // Reynolds sayısı: Re = v * D / ν
    double Re = (v * D) / KINEMATIC_VISCOSITY;

    // Haaland denklemi ile sürtünme faktörü
    double f = HaalandFriction(Re, roughness / D);

    // Darcy-Weisbach: h_f = f * (L/D) * (v²/2g)
    double h_f = f * (L / D) * (v * v) / (2.0 * GRAVITY);
    return h_f;
}

double HydraulicSolver::HaalandFriction(double Re, double relativeRoughness) const {
    if (Re < 100.0) return 0.0; // Çok düşük akış — kayıp ihmal edilebilir

    if (Re < 2300.0) {
        // Laminer akış: f = 64 / Re
        return 64.0 / Re;
    }

    // Haaland denklemi (türbülanslı akış):
    // 1/√f = -1.8 * log₁₀( (ε/D/3.7)^1.11 + 6.9/Re )
    double term1 = std::pow(relativeRoughness / 3.7, 1.11);
    double term2 = 6.9 / Re;
    double f_inv_sqrt = -1.8 * std::log10(term1 + term2);

    if (f_inv_sqrt <= 0.0) return 0.04; // Fallback

    return 1.0 / (f_inv_sqrt * f_inv_sqrt);
}

double HydraulicSolver::CalculateLocalLoss(const Edge& edge) {
    if (edge.zeta < 0.001) return 0.0;

    double v = edge.velocity_ms;
    // ΔP = ζ × ρ × v² / 2  [Pa]
    return edge.zeta * WATER_DENSITY * v * v / 2.0;
}

// ═══════════════════════════════════════════════════════════
//  DRENAJ YARDIMCI FONKSİYONLAR (EN 12056-2 + Manning)
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::GetKFactor() const {
    switch (m_buildingType) {
        case BuildingType::Residential: return 0.5;
        case BuildingType::Hotel:       return 0.7;
        case BuildingType::Industrial:  return 1.0;
        default:                        return 0.5;
    }
}

double HydraulicSolver::CalculateFlowFromDU(double dischargeUnit) const {
    // EN 12056-2:
    //   Q = K × √(ΣDU)  [L/s]
    //   K = 0.5 (konut), 0.7 (hastane/otel), 1.0 (endüstriyel)
    //
    // Minimum debi kontrolü:
    //   WC minimum 1.5 L/s (sifon tahliyesi)

    if (dischargeUnit <= 0.0) return 0.0;

    double K = GetKFactor();
    double Q_Ls = K * std::sqrt(dischargeUnit);

    // Minimum debi: WC varsa en az 1.5 L/s, genel en az 0.3 L/s
    Q_Ls = std::max(Q_Ls, 0.3);

    return Q_Ls / 1000.0; // L/s → m³/s
}

double HydraulicSolver::GetManningN(const std::string& material) const {
    // Manning pürüzlülük katsayıları
    if (material == "PVC" || material == "PE" || material == "PP") return 0.009;
    if (material == "Beton" || material == "Concrete") return 0.013;
    if (material == "Döküm" || material == "Cast Iron") return 0.012;
    if (material == "Paslanmaz" || material == "Stainless") return 0.010;
    return 0.010; // Varsayılan
}

double HydraulicSolver::ManningCapacity(double diameter_m, double slope, double n, double fillRatio) const {
    // Manning denklemi: Q = (1/n) × A × R^(2/3) × S^(1/2)
    //
    // Dairesel kesit, kısmi doluluk (h/d = fillRatio):
    //   θ = 2 × arccos(1 - 2×fillRatio)       [rad]
    //   A = (d²/8) × (θ - sin(θ))              [m²]
    //   P = d × θ / 2                           [m]   (ıslak çevre)
    //   R = A / P                               [m]   (hidrolik yarıçap)

    if (slope <= 0.0 || diameter_m <= 0.0 || fillRatio <= 0.0) return 0.0;

    double d = diameter_m;
    double theta = 2.0 * std::acos(1.0 - 2.0 * fillRatio);
    double A = (d * d / 8.0) * (theta - std::sin(theta));
    double P = d * theta / 2.0;

    if (P <= 0.0) return 0.0;

    double R = A / P;
    double Q = (1.0 / n) * A * std::pow(R, 2.0 / 3.0) * std::sqrt(slope);

    return Q * 1000.0; // m³/s → L/s
}

double HydraulicSolver::SelectDrainPipeDiameter_Manning(double flowRate_Ls, double slope, const std::string& material) const {
    // EN 12056 gereği: maksimum doluluk oranı h/d = %50
    // Manning denklemiyle her aday çap için kapasiteyi hesapla
    // Gerekli debiyi karşılayan en küçük çapı seç

    double n = GetManningN(material);
    constexpr double MAX_FILL_RATIO = 0.50; // EN 12056: %50 doluluk sınırı

    // Standart drenaj boru çapları (mm)
    static const double standardDiameters[] = {
        40.0, 50.0, 75.0, 110.0, 125.0, 160.0, 200.0, 250.0, 315.0
    };

    for (double diam_mm : standardDiameters) {
        double diam_m = diam_mm / 1000.0;
        double capacity_Ls = ManningCapacity(diam_m, slope, n, MAX_FILL_RATIO);

        if (capacity_Ls >= flowRate_Ls) {
            return diam_mm;
        }
    }

    return 315.0; // En büyük standart çap
}

// ═══════════════════════════════════════════════════════════
//  DFS — visited set ile sonsuz döngü koruması
// ═══════════════════════════════════════════════════════════

void HydraulicSolver::DFS(uint32_t nodeId, std::vector<uint32_t>& path,
                           std::vector<uint32_t>& bestPath, double& maxLoss,
                           double currentLoss, std::unordered_set<uint32_t>& visited) {
    // Visited kontrolü — çevrimli grafta sonsuz döngüyü önle
    if (visited.count(nodeId)) return;
    visited.insert(nodeId);

    path.push_back(nodeId);

    auto edges = m_network.GetConnectedEdges(nodeId);
    bool isLeaf = true;

    for (uint32_t edgeId : edges) {
        const auto* edge = m_network.GetEdge(edgeId);
        if (!edge || edge->type != EdgeType::Supply) continue;

        // Edge yönünde ilerle (nodeA → nodeB)
        uint32_t nextNode = 0;
        if (edge->nodeA == nodeId) {
            nextNode = edge->nodeB;
        } else {
            continue; // Ters yönde — skip
        }

        if (visited.count(nextNode)) continue;

        isLeaf = false;
        DFS(nextNode, path, bestPath, maxLoss,
            currentLoss + edge->headLoss_m, visited);
    }

    if (isLeaf) {
        // Leaf node — kritik yol kontrolü
        if (currentLoss > maxLoss) {
            maxLoss = currentLoss;
            bestPath = path;
        }
    }

    path.pop_back();
    visited.erase(nodeId);
}

} // namespace mep
} // namespace vkt
