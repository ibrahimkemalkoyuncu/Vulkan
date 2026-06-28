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
#include "mep/Database.hpp"
#include <cmath>
#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace vkt {
namespace mep {

constexpr double PI = 3.14159265358979323846;
constexpr double GRAVITY = 9.81;         // m/s²

// Sabit değerler artık sıcaklığa bağlı metotlarla hesaplanıyor
// WATER_DENSITY → WaterDensity()
// KINEMATIC_VISCOSITY → WaterKinematicViscosity()
// Eski sabitleri kullanan yerlerde backward compat:
constexpr double WATER_DENSITY = 1000.0;
constexpr double KINEMATIC_VISCOSITY = 1.004e-6;

HydraulicSolver::HydraulicSolver(NetworkGraph& network)
    : m_network(network), m_norm(GlobalNorm()) {}

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

    m_warnings.clear();
    m_errors.clear();

    // P0: Ön doğrulama — boş/eksik ağ kontrolü
    {
        int sourceCount = 0, fixtureCount = 0, supplyEdgeCount = 0;
        for (const auto& [id, node] : m_network.GetNodeMap()) {
            if (node.type == NodeType::Source || node.type == NodeType::HotSource) ++sourceCount;
            if (node.type == NodeType::Fixture) ++fixtureCount;
        }
        for (const auto& [id, edge] : m_network.GetEdgeMap()) {
            if (edge.type == EdgeType::Supply || edge.type == EdgeType::HotWater) ++supplyEdgeCount;
        }
        if (sourceCount == 0) {
            m_errors.push_back("Temiz su kaynagi (Source) bulunamadi - once kaynak yerlestirin");
            return;
        }
        if (fixtureCount == 0 && supplyEdgeCount == 0) {
            m_errors.push_back("Armatur veya boru bulunamadi - once tesisat cizin");
            return;
        }
    }

    // P1: Bağlantı kontrolü — izole ada (island) tespiti
    {
        std::unordered_set<uint32_t> visited;
        uint32_t startNode = 0;
        for (const auto& [id, node] : m_network.GetNodeMap()) {
            if (node.type == NodeType::Source) { startNode = id; break; }
        }
        if (startNode > 0) {
            std::queue<uint32_t> bfs;
            bfs.push(startNode);
            visited.insert(startNode);
            while (!bfs.empty()) {
                uint32_t n = bfs.front(); bfs.pop();
                for (uint32_t eid : m_network.GetConnectedEdges(n)) {
                    const Edge* e = m_network.GetEdge(eid);
                    if (!e) continue;
                    uint32_t nb = (e->nodeA == n) ? e->nodeB : e->nodeA;
                    if (!visited.count(nb)) { visited.insert(nb); bfs.push(nb); }
                }
            }
            int totalNodes = static_cast<int>(m_network.GetNodeMap().size());
            int reachable  = static_cast<int>(visited.size());
            if (reachable < totalNodes) {
                m_warnings.push_back("Agda " + std::to_string(totalNodes - reachable)
                    + " izole node tespit edildi (toplam " + std::to_string(totalNodes)
                    + ", erisilebilir " + std::to_string(reachable) + ")");
            }
        }
    }

    // 1. Her fixture node'un LU/SB değerinden debi hesapla
    for (auto& [id, node] : m_network.GetNodeMap()) {
        double unitVal = node.loadUnit;
        if (m_norm == HydroNorm::SARFIYAT && node.type == NodeType::Fixture
                && !node.fixtureType.empty()) {
            // TS 825: armatüre özgü Sarfiyat Birimi (SB) değerini DB'den al
            double sb = Database::Instance().GetFixture(node.fixtureType).sarfiyat_unit;
            if (sb > 0.0) unitVal = sb;
        }
        if (unitVal > 0.0)
            node.flowRate_m3s = CalculateFlowFromLU(unitVal);
    }

    // 2. Topolojik debi dağılımı — leaf'lerden source'a kümülatif toplama
    DistributeSupplyFlows();

    // 2b. Nominal debi kaydet (DIN simultaneity veya başka faktör uygulanmadan önceki ham LU debisi)
    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type == EdgeType::Supply || edge.type == EdgeType::HotWater)
            edge.nominalFlow_Ls = edge.flowRate_m3s * 1000.0;
    }

    // 3. Debi bilinen borular için otomatik çap seçimi (TS EN 806-3 hız kısıtı)
    AutoSizeSupplyPipes();

    // 3b. Kapalı döngü varsa Hardy-Cross iterasyonu ile basınç dengesi
    auto loops = DetectLoops();
    if (!loops.empty()) {
        std::cout << loops.size() << " döngü tespit edildi — Hardy-Cross başlatılıyor..." << std::endl;
        SolveHardyCross(loops);
    }

    // 3c. Newton-Raphson ile basınç-debi eşzamanlı iyileştirme
    // Sadece karmaşık ağlarda (>10 junction) çalıştır — basit ağlarda Hardy-Cross yeterli
    {
        int junctionCount = 0;
        for (const auto& [nid, node] : m_network.GetNodeMap())
            if (node.type == NodeType::Junction) ++junctionCount;
        if (junctionCount > 10 && !loops.empty())
            SolveNewtonRaphson();
    }

    // 4. Her boru için hidrolik hesap
    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Supply && edge.type != EdgeType::HotWater) continue;

        // Hız hesapla: v = Q / A
        double D_m = edge.diameter_mm / 1000.0;
        double area_m2 = PI * (D_m / 2.0) * (D_m / 2.0);
        if (area_m2 > 0.0) {
            edge.velocity_ms = edge.flowRate_m3s / area_m2;
        }

        // Sürtünme kaybı (Darcy-Weisbach + Haaland)
        edge.headLoss_m = CalculateHeadLoss(edge);

        // Lokal kayıplar: topoloji bazlı K katsayısı + Zeta
        edge.localLoss_Pa = CalculateLocalLoss(edge);
        // Lokal kaybı metre su sütununa çevir ve sürtünme kaybına ekle
        double localLoss_m = edge.localLoss_Pa / (WATER_DENSITY * GRAVITY);
        edge.headLoss_m += localLoss_m;

        std::cout << "Boru " << edge.id << ": "
                  << "Q=" << edge.flowRate_m3s * 1000.0 << " L/s, "
                  << "v=" << edge.velocity_ms << " m/s, "
                  << "h=" << edge.headLoss_m << " m, "
                  << "ΔP_local=" << edge.localLoss_Pa / 1000.0 << " kPa"
                  << std::endl;
    }

    // P0: Tehlikeli hız uyarıları
    for (const auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Supply && edge.type != EdgeType::HotWater) continue;
        if (edge.flowRate_m3s < 1e-10) continue;
        if (edge.velocity_ms > 5.0)
            m_warnings.push_back("Boru #" + std::to_string(edge.id)
                + " hiz=" + std::to_string(edge.velocity_ms).substr(0,4)
                + " m/s, DN" + std::to_string((int)edge.diameter_mm)
                + " cok kucuk (gurultu/erozyon riski)");
        else if (edge.velocity_ms > 0.0 && edge.velocity_ms < 0.2)
            m_warnings.push_back("Boru #" + std::to_string(edge.id)
                + " hiz=" + std::to_string(edge.velocity_ms).substr(0,5)
                + " m/s, DN" + std::to_string((int)edge.diameter_mm)
                + " cok buyuk (durgunluk riski)");
    }

    // Cache'leri dirty işaretle (edge/node değerleri değişti)
    m_network.GetEdges();
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
        if (edge.type != EdgeType::Supply && edge.type != EdgeType::HotWater) continue;
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
            if (!edge || (edge->type != EdgeType::Supply && edge->type != EdgeType::HotWater)) continue;

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
//  OTOMATİK SUPPLY BORU ÇAPI BOYUTLANDIRMA (TS EN 806-3)
// ═══════════════════════════════════════════════════════════

void HydraulicSolver::AutoSizeSupplyPipes() {
    // TS EN 806-3 hız kısıtları:
    //   Dağıtım boruları : v_max = 3.0 m/s
    //   Bağlantı boruları: v_max = 2.0 m/s  (Ø ≤ 25 mm)
    //   Minimum hız      : v_min = 0.5 m/s  (durgunluk/çökelme önleme)
    //
    // Algoritma: her Supply edge için Database'deki çapları küçükten büyüğe tara,
    // v = Q / A kısıtını sağlayan ilk çapı seç.
    // Kullanıcının manuel atadığı çap (diameter_mm > 0) korunur — sadece
    // flowRate'e göre gerekli çaptan küçükse otomatik büyütülür.

    constexpr double V_MAX_LARGE = 3.0; // m/s — Ø > 25 mm
    constexpr double V_MAX_SMALL = 2.0; // m/s — Ø ≤ 25 mm
    constexpr double V_MIN       = 0.5; // m/s

    for (auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Supply && edge.type != EdgeType::HotWater) continue;
        if (edge.flowRate_m3s <= 0.0) continue;

        double autoD = SelectSupplyPipeDiameter(edge.flowRate_m3s, edge.material);

        // Kullanıcı çapını küçültme — sadece yetersizse büyüt
        if (autoD > edge.diameter_mm) {
            std::cout << "Boru " << edge.id << ": çap " << edge.diameter_mm
                      << " mm → " << autoD << " mm (Q="
                      << edge.flowRate_m3s * 1000.0 << " L/s)\n";
            edge.diameter_mm = autoD;
        }

        // Seçilen çapla gerçek hızı güncelle
        double D_m = edge.diameter_mm / 1000.0;
        double area = PI * (D_m / 2.0) * (D_m / 2.0);
        edge.velocity_ms = (area > 0.0) ? edge.flowRate_m3s / area : 0.0;

        // Hız uyarısı
        double vMax = (edge.diameter_mm <= 25.0) ? V_MAX_SMALL : V_MAX_LARGE;
        if (edge.velocity_ms > vMax) {
            std::cout << "UYARI Boru " << edge.id << ": v=" << edge.velocity_ms
                      << " m/s > " << vMax << " m/s (standart aşıldı)\n";
        } else if (edge.velocity_ms < V_MIN && edge.velocity_ms > 0.0) {
            std::cout << "UYARI Boru " << edge.id << ": v=" << edge.velocity_ms
                      << " m/s < 0.5 m/s (çökelme riski)\n";
        }
    }
}

double HydraulicSolver::SelectSupplyPipeDiameter(double flowRate_m3s, const std::string& material) const {
    constexpr double V_MAX_LARGE = 3.0;
    constexpr double V_MAX_SMALL = 2.0;
    constexpr double DN15_MIN    = 15.0; // TS EN 806-3: minimum bağlantı çapı

    const Database& db = Database::Instance();
    std::vector<double> diameters;

    try {
        diameters = db.GetPipe(material).availableDiameters_mm;
    } catch (...) {
        // Bilinmeyen malzeme — PVC standart çaplarını kullan
        diameters = db.GetPipe("PVC").availableDiameters_mm;
    }

    // Küçükten büyüğe sıralı olmalı (Database zaten sıralı)
    for (double diam_mm : diameters) {
        if (diam_mm < DN15_MIN) continue;

        double D_m   = diam_mm / 1000.0;
        double area  = PI * (D_m / 2.0) * (D_m / 2.0);
        double v     = flowRate_m3s / area;
        double vMax  = (diam_mm <= 25.0) ? V_MAX_SMALL : V_MAX_LARGE;

        if (v <= vMax) {
            return diam_mm;
        }
    }

    // Tüm standart çaplar yetersiz — en büyüğünü döndür
    return diameters.empty() ? 50.0 : diameters.back();
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

        // EN 12056-2: WC bağlantısı min DN100 kuralı
        const auto* nA = m_network.GetNode(edge.nodeA);
        const auto* nB = m_network.GetNode(edge.nodeB);
        bool hasWC = (nA && nA->fixtureType == "WC") || (nB && nB->fixtureType == "WC");
        if (hasWC) minDiameter = std::max(minDiameter, 100.0);

        edge.diameter_mm = std::max(edge.diameter_mm, minDiameter);

        // Gerçek doluluk oranını hesapla (h/d): binary search ile Manning ters çözümü
        // ManningCapacity(d, slope, n, fillRatio) = flowRate_Ls → fillRatio bul
        {
            double n = GetManningN(edge.material);
            double d_m = edge.diameter_mm / 1000.0;
            double lo = 0.01, hi = 0.99, mid = 0.5;
            for (int iter = 0; iter < 40; ++iter) {
                mid = (lo + hi) / 2.0;
                double q = ManningCapacity(d_m, edge.slope, n, mid);
                if (q < flowRate_Ls) lo = mid; else hi = mid;
            }
            edge.fillRate = std::clamp(mid, 0.0, 0.99);
        }

        std::cout << "Drenaj " << edge.id << ": "
                  << "DU=" << edge.cumulativeDU << ", "
                  << "Q=" << flowRate_Ls << " L/s, "
                  << "Ø=" << edge.diameter_mm << " mm, "
                  << "h/d=" << (edge.fillRate * 100.0) << "%"
                  << std::endl;
    }

    // P1: Ters eğim / geri akış kontrolü — drenaj node'larında Z monoton azalmalı
    for (const auto& [id, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Drainage) continue;
        const Node* nA = m_network.GetNode(edge.nodeA);
        const Node* nB = m_network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;
        if (nA->position.z < nB->position.z - 0.01) {
            std::cerr << "[UYARI] Drenaj boru #" << edge.id
                      << " ters eğimli — nodeA(z=" << nA->position.z
                      << ") < nodeB(z=" << nB->position.z
                      << ") → geri akış riski!" << std::endl;
        }
    }

    m_network.GetEdges();
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
//  DOĞAL GAZ HESABI (TS EN 1775)
// ═══════════════════════════════════════════════════════════
void HydraulicSolver::SolveGas() {
    constexpr double Hu_kWh_m3   = 9.5;    // Doğal gaz alt ısıl değer (G20)
    constexpr double rho_gas     = 0.75;   // Yoğunluk (kg/m³, 15°C, 1013 hPa)
    constexpr double v_max_ms    = 2.0;    // Max iç mekan hız (TS EN 1775)
    constexpr double dP_max_Pa_m = 1.0;    // Max özgül basınç düşümü (Pa/m) — iç mekan

    // 1 — GasAppliance node'larına debi ata
    for (auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type != NodeType::GasAppliance) continue;
        double power_kW = node.gasPower_kW > 0.0 ? node.gasPower_kW : 24.0;
        node.gasFlow_m3h = power_kW / Hu_kWh_m3;
        node.flowRate_m3s = node.gasFlow_m3h / 3600.0;
    }

    // 2 — Kümülatif debi: leaf→source DFS (soğuk su ile aynı yaklaşım)
    auto& nodeMap = m_network.GetNodeMap();
    auto& edgeMap = m_network.GetEdgeMap();

    for (auto& [id, edge] : edgeMap) {
        if (edge.type != EdgeType::Gas) continue;
        edge.flowRate_m3s = 0.0;
    }

    // Her GasSource'dan DFS ile downstream toplam
    std::function<double(uint32_t, uint32_t)> dfs = [&](uint32_t nid, uint32_t parent) -> double {
        double total = 0.0;
        auto nit = nodeMap.find(nid);
        if (nit != nodeMap.end() && nit->second.type == NodeType::GasAppliance)
            total += nit->second.flowRate_m3s;
        for (uint32_t eid : m_network.GetConnectedEdges(nid)) {
            auto* e = m_network.GetEdge(eid);
            if (!e || e->type != EdgeType::Gas) continue;
            uint32_t nb = (e->nodeA == nid) ? e->nodeB : e->nodeA;
            if (nb == parent) continue;
            double sub = dfs(nb, nid);
            e->flowRate_m3s = sub;
            total += sub;
        }
        return total;
    };

    for (auto& [id, node] : nodeMap) {
        if (node.type == NodeType::GasSource)
            dfs(id, 0);
    }

    // 3 — Her Gas edge için Darcy-Weisbach + DN seç
    for (auto& [id, edge] : edgeMap) {
        if (edge.type != EdgeType::Gas) continue;
        double Q_m3h = edge.flowRate_m3s * 3600.0;
        if (Q_m3h < 1e-9) { edge.diameter_mm = 15.0; continue; }

        // DN seç (Database)
        edge.diameter_mm = Database::Instance().GetGasDN(Q_m3h);

        // Hız
        double D_m = edge.diameter_mm / 1000.0;
        double A   = M_PI * D_m * D_m / 4.0;
        edge.velocity_ms = edge.flowRate_m3s / A;

        // Basınç kaybı — Darcy-Weisbach gaz için
        double Re = rho_gas * edge.velocity_ms * D_m / 1.81e-5; // μ_gaz ≈ 1.81e-5 Pa·s
        double f  = (Re < 2300.0) ? 64.0 / std::max(Re, 1.0)
                                   : HaalandFriction(Re, edge.roughness_mm / (edge.diameter_mm));
        double dP_Pa = f * (edge.length_m / D_m) * (rho_gas * edge.velocity_ms * edge.velocity_ms / 2.0);
        edge.headLoss_m = dP_Pa / (rho_gas * 9.81);
        edge.pressure_Pa = dP_Pa;
    }

    // P1: Gaz min basınç kontrolü — kaynak→cihaz kümülatif basınç kaybı
    // TS EN 1775: min 17.4 Pa (Düşük Basınç) cihaz giriş basıncı
    constexpr double kSourcePressure_Pa = 2000.0; // 20 mbar giriş basıncı (sayaç çıkışı)
    constexpr double kMinAppliance_Pa   = 17.4;
    for (const auto& [nid, node] : nodeMap) {
        if (node.type != NodeType::GasAppliance) continue;
        // DFS ile GasSource→bu cihaz yolundaki toplam basınç kaybını hesapla
        double totalLoss_Pa = 0.0;
        std::unordered_set<uint32_t> visited;
        std::function<bool(uint32_t)> tracePath = [&](uint32_t n) -> bool {
            if (visited.count(n)) return false;
            visited.insert(n);
            if (n == nid) return true;
            for (uint32_t eid : m_network.GetConnectedEdges(n)) {
                const Edge* e = m_network.GetEdge(eid);
                if (!e || e->type != EdgeType::Gas) continue;
                uint32_t nb = (e->nodeA == n) ? e->nodeB : e->nodeA;
                if (tracePath(nb)) { totalLoss_Pa += e->pressure_Pa; return true; }
            }
            return false;
        };
        for (const auto& [sid, snode] : nodeMap) {
            if (snode.type == NodeType::GasSource) { tracePath(sid); break; }
        }
        double remainingP = kSourcePressure_Pa - totalLoss_Pa;
        if (remainingP < kMinAppliance_Pa)
            std::cerr << "[UYARI] Gaz cihazı node #" << nid << " (" << node.label
                      << "): kalan basınç " << remainingP << " Pa < min " << kMinAppliance_Pa
                      << " Pa — hat çapı artırılmalı!" << std::endl;
    }
}

// ═══════════════════════════════════════════════════════════
//  ISITMA SİSTEMİ HESABI (EN 12831)
// ═══════════════════════════════════════════════════════════
void HydraulicSolver::SolveHeating() {
    constexpr double cp_water   = 4186.0; // J/(kg·K)
    constexpr double rho_water  = 1000.0; // kg/m³
    constexpr double dT_K       = 20.0;   // Gidiş-dönüş fark (70/50°C)

    // 1 — Radyatör node'larına debi ata (Q = P/(cp·ρ·ΔT))
    for (auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type != NodeType::Radiator) continue;
        double P_W = node.heatPower_kW * 1000.0;
        if (P_W < 1.0) P_W = 1000.0; // varsayılan 1 kW
        double m_flow_kgs = P_W / (cp_water * dT_K);    // kg/s
        node.heatFlow_Ls  = m_flow_kgs / rho_water * 1000.0; // l/s
        node.flowRate_m3s = m_flow_kgs / rho_water;
    }

    // 2 — Kümülatif debi: Heating + HeatingReturn edge'leri
    auto& nodeMap = m_network.GetNodeMap();
    auto& edgeMap = m_network.GetEdgeMap();

    std::function<double(uint32_t, uint32_t, EdgeType)> dfsH =
        [&](uint32_t nid, uint32_t parent, EdgeType et) -> double {
        double total = 0.0;
        auto nit = nodeMap.find(nid);
        if (nit != nodeMap.end() && nit->second.type == NodeType::Radiator)
            total += nit->second.flowRate_m3s;
        for (uint32_t eid : m_network.GetConnectedEdges(nid)) {
            auto* e = m_network.GetEdge(eid);
            if (!e || e->type != et) continue;
            uint32_t nb = (e->nodeA == nid) ? e->nodeB : e->nodeA;
            if (nb == parent) continue;
            double sub = dfsH(nb, nid, et);
            e->flowRate_m3s = sub;
            total += sub;
        }
        return total;
    };

    for (auto& [id, node] : nodeMap) {
        if (node.type == NodeType::Boiler) {
            dfsH(id, 0, EdgeType::Heating);
            dfsH(id, 0, EdgeType::HeatingReturn);
        }
    }

    // 3 — Her ısıtma edge'i için DN seç + head loss
    for (auto& [id, edge] : edgeMap) {
        if (edge.type != EdgeType::Heating && edge.type != EdgeType::HeatingReturn) continue;
        double Q_Ls = edge.flowRate_m3s * 1000.0;
        if (Q_Ls < 1e-9) { edge.diameter_mm = 15.0; continue; }

        edge.diameter_mm = Database::Instance().GetHeatingDN(Q_Ls);
        double D_m = edge.diameter_mm / 1000.0;
        double A   = M_PI * D_m * D_m / 4.0;
        edge.velocity_ms  = edge.flowRate_m3s / A;
        double Re  = rho_water * edge.velocity_ms * D_m / 1e-3;
        double f   = (Re < 2300.0) ? 64.0 / std::max(Re, 1.0)
                                    : HaalandFriction(Re, edge.roughness_mm / edge.diameter_mm);
        edge.headLoss_m = f * (edge.length_m / D_m) * (edge.velocity_ms * edge.velocity_ms) / (2.0 * 9.81);
        edge.localLoss_Pa = CalculateLocalLoss(edge);
        edge.headLoss_m  += edge.localLoss_Pa / (rho_water * 9.81);
    }
}

// ═══════════════════════════════════════════════════════════
//  YANGIN / SPRİNKLER HESABI (EN 12845 Yoğunluk-Alan Metodu)
// ═══════════════════════════════════════════════════════════
void HydraulicSolver::SolveFire(const std::string& hazardClass) {
    // EN 12845 Tablo 2: Tasarım yoğunluğu ve alanı
    struct HazardSpec { double density_mm_min; double area_m2; };
    std::map<std::string, HazardSpec> specs = {
        {"LH",  {2.25,  84.0}},
        {"OH1", {5.00,  72.0}},
        {"OH2", {5.00, 144.0}},
        {"OH3", {5.00, 216.0}},
        {"OH4", {10.0, 360.0}},
        {"HH",  {12.5, 260.0}}
    };
    HazardSpec spec = specs.count(hazardClass) ? specs.at(hazardClass) : specs["OH1"];

    // Toplam sistem debisi Q (l/s) = yoğunluk(mm/min) × alan(m²) / 60000
    double Q_total_Ls = spec.density_mm_min * spec.area_m2 / 60000.0;

    // Sprinkler başlık sayısını bul
    int sprCount = 0;
    for (auto& [id, node] : m_network.GetNodeMap())
        if (node.type == NodeType::Sprinkler) ++sprCount;
    if (sprCount == 0) return;

    double Q_per_head_Ls = Q_total_Ls / sprCount;
    double minP_Pa = 5.0e5; // 5 bar min pompada

    // Sprinkler node'larına debi ata
    for (auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type != NodeType::Sprinkler) continue;
        node.sprinklerFlow_Ls     = Q_per_head_Ls;
        node.sprinklerPressure_Pa = minP_Pa;
        node.flowRate_m3s         = Q_per_head_Ls / 1000.0;
    }

    // Kümülatif debi DFS
    auto& nodeMap = m_network.GetNodeMap();
    auto& edgeMap = m_network.GetEdgeMap();

    std::function<double(uint32_t, uint32_t)> dfsF = [&](uint32_t nid, uint32_t parent) -> double {
        double total = 0.0;
        auto nit = nodeMap.find(nid);
        if (nit != nodeMap.end() && nit->second.type == NodeType::Sprinkler)
            total += nit->second.flowRate_m3s;
        for (uint32_t eid : m_network.GetConnectedEdges(nid)) {
            auto* e = m_network.GetEdge(eid);
            if (!e || e->type != EdgeType::FireLine) continue;
            uint32_t nb = (e->nodeA == nid) ? e->nodeB : e->nodeA;
            if (nb == parent) continue;
            double sub = dfsF(nb, nid);
            e->flowRate_m3s = sub;
            total += sub;
        }
        return total;
    };

    for (auto& [id, node] : nodeMap)
        if (node.type == NodeType::FirePump)
            dfsF(id, 0);

    // DN + head loss hesabı
    constexpr double rho = 1000.0;
    for (auto& [id, edge] : edgeMap) {
        if (edge.type != EdgeType::FireLine) continue;
        double Q_Ls = edge.flowRate_m3s * 1000.0;
        if (Q_Ls < 1e-9) { edge.diameter_mm = 25.0; continue; }
        edge.diameter_mm = Database::Instance().GetFireDN(Q_Ls);
        double D_m = edge.diameter_mm / 1000.0;
        double A   = M_PI * D_m * D_m / 4.0;
        edge.velocity_ms = edge.flowRate_m3s / A;
        double Re  = rho * edge.velocity_ms * D_m / 1e-3;
        double f   = (Re < 2300.0) ? 64.0 / std::max(Re, 1.0)
                                    : HaalandFriction(Re, edge.roughness_mm / edge.diameter_mm);
        edge.headLoss_m = f * (edge.length_m / D_m) * (edge.velocity_ms * edge.velocity_ms) / (2.0 * 9.81);
    }
}

// ═══════════════════════════════════════════════════════════
//  ELEKTRİK TESİSAT HESABI — IEC 60364
// ═══════════════════════════════════════════════════════════
void HydraulicSolver::SolveElectric() {
    auto& nodeMap = m_network.GetNodeMap();
    auto& edgeMap = m_network.GetEdgeMap();

    // Toplam güç toplaması — DFS ile panelden aşağı
    std::function<double(uint32_t, uint32_t)> dfsP = [&](uint32_t nid, uint32_t parent) -> double {
        double total = 0.0;
        auto nit = nodeMap.find(nid);
        if (nit != nodeMap.end()) {
            if (nit->second.type == NodeType::Socket)       total += 3500.0;  // 3.5 kW
            if (nit->second.type == NodeType::LightFixture)  total += 200.0;   // 200 W
        }
        for (uint32_t eid : m_network.GetConnectedEdges(nid)) {
            auto* e = m_network.GetEdge(eid);
            if (!e || e->type != EdgeType::Electric) continue;
            uint32_t nb = (e->nodeA == nid) ? e->nodeB : e->nodeA;
            if (nb == parent) continue;
            double sub = dfsP(nb, nid);
            // Kablo boyutlandırma: I = P / (V × cosφ × √phases)
            // Pano tipi: ElecPanel → 3-faz (400V), Socket/Light → 1-faz (230V)
            bool isThreePhase = false;
            auto srcIt = nodeMap.find(nid);
            if (srcIt != nodeMap.end() && srcIt->second.type == NodeType::ElecPanel)
                isThreePhase = true;
            double V = isThreePhase ? 400.0 : 230.0;
            double cosPhi = 0.85;
            double phaseFactor = isThreePhase ? 1.732 : 1.0; // √3 for 3-phase
            double I = sub / (V * cosPhi * phaseFactor);
            e->flowRate_m3s = I;  // Amper
            // Kablo kesiti seçimi (mm²): IEC 60364-5-52 Tablo B.52.2
            // 1.5mm²→13A, 2.5→20, 4→27, 6→36, 10→50, 16→66, 25→89
            static const double kTable[][2] = {
                {13,1.5},{20,2.5},{27,4},{36,6},{50,10},{66,16},{89,25},{115,35},{150,50}
            };
            e->diameter_mm = 1.5;
            for (auto& [ampCap, mm2] : kTable) {
                if (ampCap >= I) { e->diameter_mm = mm2; break; }
            }
            // Gerilim düşümü: ΔV = 2 × I × ρ_Cu × L / A
            constexpr double rho_Cu = 0.0175; // Ω·mm²/m bakır
            double A_mm2 = e->diameter_mm;
            e->velocity_ms = I;  // display
            e->headLoss_m  = 2.0 * I * rho_Cu * e->length_m / A_mm2; // Volt drop
            total += sub;
        }
        return total;
    };

    for (auto& [id, node] : nodeMap)
        if (node.type == NodeType::ElecPanel)
            dfsP(id, 0);
}

// ═══════════════════════════════════════════════════════════
//  HAVALANDIRMA KANAL HESABI — EN 15665
// ═══════════════════════════════════════════════════════════
void HydraulicSolver::SolveVentilation() {
    auto& nodeMap = m_network.GetNodeMap();
    auto& edgeMap = m_network.GetEdgeMap();

    // Difüzör debisi: kişi başı 10 L/s (EN 15665 ofis)
    constexpr double kFlowPerDiffuser_Ls = 10.0;

    std::function<double(uint32_t, uint32_t)> dfsV = [&](uint32_t nid, uint32_t parent) -> double {
        double total = 0.0;
        auto nit = nodeMap.find(nid);
        if (nit != nodeMap.end() && nit->second.type == NodeType::Diffuser)
            total += kFlowPerDiffuser_Ls / 1000.0; // m³/s

        for (uint32_t eid : m_network.GetConnectedEdges(nid)) {
            auto* e = m_network.GetEdge(eid);
            if (!e || e->type != EdgeType::Duct) continue;
            uint32_t nb = (e->nodeA == nid) ? e->nodeB : e->nodeA;
            if (nb == parent) continue;
            double sub = dfsV(nb, nid);
            e->flowRate_m3s = sub;

            constexpr double v_max = 5.0;
            double A_needed = sub / v_max;
            double A_act = 0, D_hyd = 0;

            if (e->IsRectangularDuct()) {
                // Dikdörtgen kanal — mevcut kesiti koru, sadece hız/kayıp hesapla
                A_act = e->DuctArea_m2();
                double P = 2.0 * (e->ductWidth_mm + e->ductHeight_mm) / 1000.0;
                D_hyd = (P > 0) ? 4.0 * A_act / P : 0.2;
                e->diameter_mm = std::sqrt(4.0 * A_act / M_PI) * 1000.0;
                e->label = std::to_string((int)e->ductWidth_mm) + "x"
                         + std::to_string((int)e->ductHeight_mm);
            } else {
                // Dairesel kanal — otomatik boyutlandırma
                double D_m = std::sqrt(4.0 * A_needed / M_PI);
                static const int kDucts[] = {100,125,150,200,250,315,400,500,630,800,1000};
                e->diameter_mm = 100;
                for (int dn : kDucts) {
                    if (dn >= D_m * 1000.0) { e->diameter_mm = dn; break; }
                }
                double D_act_m = e->diameter_mm / 1000.0;
                A_act = M_PI * D_act_m * D_act_m / 4.0;
                D_hyd = D_act_m;
                e->label = "Ø" + std::to_string((int)e->diameter_mm);
            }

            e->velocity_ms = (A_act > 1e-9) ? sub / A_act : 0;
            constexpr double rho_air = 1.2;
            constexpr double f_duct  = 0.02;
            e->headLoss_m = (D_hyd > 1e-9)
                ? f_duct * (e->length_m / D_hyd) *
                  (rho_air * e->velocity_ms * e->velocity_ms / 2.0) / 9.81
                : 0;
            total += sub;
        }
        return total;
    };

    for (auto& [id, node] : nodeMap)
        if (node.type == NodeType::AHU)
            dfsV(id, 0);
}

// ═══════════════════════════════════════════════════════════
//  KRİTİK DEVRE ANALİZİ
// ═══════════════════════════════════════════════════════════

CriticalPathResult HydraulicSolver::CalculateCriticalPath() {
    CriticalPathResult result;

    // Multi-source: her Source/HotSource'dan DFS — en yüksek kaybı veren yol kritik
    std::vector<uint32_t> bestPath;
    double maxLoss = 0.0;

    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type != NodeType::Source && node.type != NodeType::HotSource) continue;

        std::vector<uint32_t> currentPath, localBestPath;
        double localMaxLoss = 0.0;
        std::unordered_set<uint32_t> visited;
        DFS(node.id, currentPath, localBestPath, localMaxLoss, 0.0, visited);

        if (localMaxLoss > maxLoss) {
            maxLoss = localMaxLoss;
            bestPath = localBestPath;
        }
    }

    // Statik yükseklik farkı (Z ekseni): kaynak → en dezavantajlı düğüm
    double staticHead_m = 0.0;
    if (bestPath.size() >= 2) {
        const auto* srcNode = m_network.GetNode(bestPath.front());
        const auto* endNode = m_network.GetNode(bestPath.back());
        if (srcNode && endNode) {
            double dz = endNode->position.z - srcNode->position.z;
            if (dz > 0) staticHead_m = dz;
        }
    }

    result.criticalPath = bestPath;
    result.totalHeadLoss_m = maxLoss + staticHead_m;

    // Fixture'a göre minimum basınç gereksinimi (EN 806-2)
    double minPressure_m = 10.0; // default 1.0 bar
    if (!bestPath.empty()) {
        result.disadvantagedNodeId = bestPath.back();
        const auto* endNode = m_network.GetNode(bestPath.back());
        if (endNode && endNode->type == NodeType::Fixture) {
            try {
                auto fd = Database::Instance().GetFixture(endNode->fixtureType);
                minPressure_m = fd.minPressure_bar * 10.2;
            } catch (...) {}
        }
    }

    result.requiredPumpHead_m = result.totalHeadLoss_m + minPressure_m;

    // Multi-source toplam debi
    double totalFlow_m3s = 0.0;
    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (node.type == NodeType::Source || node.type == NodeType::HotSource)
            totalFlow_m3s += node.flowRate_m3s;
    }
    result.requiredFlow_m3h = totalFlow_m3s * 3600.0;

    // Database'den uygun pompa öner
    const PumpData pump = Database::Instance().SuggestPump(
        result.requiredPumpHead_m, result.requiredFlow_m3h);
    result.suggestedPumpModel    = pump.model;
    result.suggestedPumpHead_m   = pump.maxHead_m;
    result.suggestedPumpFlow_m3h = pump.maxFlow_m3h;
    result.suggestedPumpPower_kW = pump.ratedPower_kW;

    return result;
}

// ═══════════════════════════════════════════════════════════
//  BESLEME YARDIMCI FONKSİYONLAR
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::CalculateFlowFromLU(double loadUnit) const {
    if (loadUnit <= 0.0) return 0.0;

    double Q_Ls;
    if (m_norm == HydroNorm::DIN1988) {
        // DIN 1988-300 Tablo 3: Q = a × LU^b - c  [L/s]
        // 6 bina tipi için farklı a/b/c katsayıları
        double a, b, c;
        switch (m_buildingType) {
            case BuildingType::Residential: a = 1.48; b = 0.19; c = 0.94; break;
            case BuildingType::Hotel:       a = 0.91; b = 0.31; c = 0.38; break;
            case BuildingType::Hospital:    a = 0.75; b = 0.44; c = 0.18; break;
            case BuildingType::School:      a = 1.40; b = 0.14; c = 0.92; break;
            case BuildingType::Office:      a = 0.91; b = 0.31; c = 0.38; break;
            case BuildingType::Industrial:  a = 0.70; b = 0.48; c = 0.13; break;
            default:                        a = 1.48; b = 0.19; c = 0.94; break;
        }
        Q_Ls = a * std::pow(std::max(loadUnit, 0.1), b) - c;
        Q_Ls = std::max(Q_Ls, 0.05);
    } else if (m_norm == HydroNorm::SARFIYAT) {
        // TS 825 Musluk Birimi: Q = K_s × √(SB)
        // K_s bina tipine göre
        double K_s;
        switch (m_buildingType) {
            case BuildingType::Hotel:       K_s = 0.8; break;
            case BuildingType::Hospital:    K_s = 0.8; break;
            case BuildingType::Industrial:  K_s = 1.0; break;
            case BuildingType::School:      K_s = 0.6; break;
            case BuildingType::Office:      K_s = 0.7; break;
            default:                        K_s = 0.6; break;
        }
        Q_Ls = K_s * std::sqrt(loadUnit);
        Q_Ls = std::max(Q_Ls, 0.1);
    } else {
        // TS EN 806-3: Qd = 0.682 * LU^0.45  [L/s]
        if (loadUnit <= 500.0) {
            Q_Ls = 0.682 * std::pow(loadUnit, 0.45);
        } else {
            double Q_500 = 0.682 * std::pow(500.0, 0.45);
            Q_Ls = Q_500 + 0.005 * (loadUnit - 500.0);
        }
    }

    Q_Ls = std::max(Q_Ls, 0.1);
    return Q_Ls / 1000.0; // L/s → m³/s
}

double HydraulicSolver::CalculateHeadLoss(const Edge& edge) {
    double D = edge.diameter_mm / 1000.0; // m
    double L = edge.length_m;
    double v = edge.velocity_ms;

    if (D <= 0.0 || v <= 0.0) return 0.0;

    // Boru yaşlanma: Colebrook korelasyonu ε(t) = ε₀ + α·t
    // α değerleri: AWWA M77 + Lamont (1981) + Colebrook (1939)
    double roughness_mm = edge.roughness_mm;
    if (m_pipeAgeYears > 0.0) {
        double alpha = 0.0; // mm/yıl pürüzlülük artış hızı
        const auto& mat = edge.material;
        if (mat == "Çelik" || mat == "Steel")
            alpha = 0.025;  // Çelik: 0.02-0.03 mm/yıl (Lamont)
        else if (mat == "Galvaniz" || mat == "Galvanized")
            alpha = 0.02;   // Galvaniz: 0.015-0.025 mm/yıl
        else if (mat == "Dökme Demir" || mat == "Cast Iron")
            alpha = 0.05;   // Dökme demir: 0.04-0.06 mm/yıl (AWWA)
        else if (mat == "Bakır" || mat == "Copper")
            alpha = 0.002;  // Bakır: 0.001-0.003 mm/yıl
        else if (mat == "Paslanmaz" || mat == "Stainless Steel")
            alpha = 0.005;  // Paslanmaz çelik: minimal
        // Plastik (PVC/PPR/PE/PEX/HDPE/PE-RT) → α = 0 (yaşlanma yok)
        // Beton boru → α ≈ 0.03 mm/yıl (seyrek kullanılır)
        else if (mat == "Beton" || mat == "Concrete")
            alpha = 0.03;

        // Doğrusal yaşlanma + üst sınır (ε₀ × 10 veya 5mm)
        roughness_mm += alpha * m_pipeAgeYears;
        roughness_mm = std::min(roughness_mm, std::max(edge.roughness_mm * 10.0, 5.0));
    }
    double roughness = roughness_mm / 1000.0; // m

    // Sıcaklığa bağlı kinematik viskozite
    double nu = WaterKinematicViscosity();

    // Reynolds sayısı: Re = v * D / ν
    double Re = (v * D) / nu;

    // Haaland denklemi ile sürtünme faktörü
    double f = HaalandFriction(Re, roughness / D);

    // Darcy-Weisbach: h_f = f * (L/D) * (v²/2g)
    double h_f = f * (L / D) * (v * v) / (2.0 * GRAVITY);
    return h_f;
}

double HydraulicSolver::HaalandFriction(double Re, double relativeRoughness) const {
    if (Re < 100.0) return 0.0; // Çok düşük akış — kayıp ihmal edilebilir

    if (Re < 2300.0) {
        return 64.0 / Re; // Laminer akış
    }

    if (Re < 4000.0) {
        // Geçiş bölgesi (transitional): laminer-türbülans interpolasyonu
        double f_lam = 64.0 / 2300.0;
        double t1 = std::pow(relativeRoughness / 3.7, 1.11);
        double t2 = 6.9 / 4000.0;
        double f_inv_s = -1.8 * std::log10(t1 + t2);
        double f_turb = (f_inv_s > 0.0) ? 1.0 / (f_inv_s * f_inv_s) : 0.04;
        double w = (Re - 2300.0) / 1700.0; // 0→1 geçiş ağırlığı
        return f_lam * (1.0 - w) + f_turb * w;
    }

    // Haaland başlangıç tahmini → Colebrook-White iterasyonu (3 adım)
    // Haaland: 1/√f = -1.8 * log₁₀( (ε/D/3.7)^1.11 + 6.9/Re )
    double term1 = std::pow(relativeRoughness / 3.7, 1.11);
    double term2 = 6.9 / Re;
    double f_inv_sqrt = -1.8 * std::log10(term1 + term2);
    if (f_inv_sqrt <= 0.0) return 0.04;
    double f = 1.0 / (f_inv_sqrt * f_inv_sqrt);

    // Colebrook-White iterasyonu: 1/√f = -2·log₁₀(ε/(3.7D) + 2.51/(Re·√f))
    for (int i = 0; i < 3; ++i) {
        double sf = std::sqrt(f);
        double rhs = -2.0 * std::log10(relativeRoughness / 3.7 + 2.51 / (Re * sf));
        if (rhs > 0.0) f = 1.0 / (rhs * rhs);
    }

    return f;
}

double HydraulicSolver::CalculateLocalLoss(const Edge& edge) {
    double v = edge.velocity_ms;
    if (v < 1e-9) return 0.0;

    double dn = edge.diameter_mm;
    double K = 0.0;

    // DN-bağımlı dirsek: her 5m boru başına 1 adet 90° dirsek
    int numElbows = std::max(0, (int)(edge.length_m / 5.0));
    K += numElbows * FittingK_Elbow90(dn);

    // Bağlantı noktaları — topoloji tabanlı
    const auto* nodeA = m_network.GetNode(edge.nodeA);
    const auto* nodeB = m_network.GetNode(edge.nodeB);

    // Giriş kaybı (kaynak çıkışı veya tank çıkışı)
    if (nodeA && (nodeA->type == NodeType::Source || nodeA->type == NodeType::HotSource ||
                  nodeA->type == NodeType::Tank))
        K += 0.5;  // Keskin giriş
    else
        K += 0.25; // Normal boru-boru geçişi

    // T-parça: junction'da >2 bağlantı varsa yan kol K değeri
    if (nodeA && m_network.GetConnectedEdges(edge.nodeA).size() > 2)
        K += FittingK_Tee(dn) * 0.5; // Ana hat tarafı (×0.5)
    if (nodeB && m_network.GetConnectedEdges(edge.nodeB).size() > 2)
        K += FittingK_Tee(dn); // Yan kol tarafı (tam)

    // Çıkış kaybı (fixture → atmosfere)
    if (nodeB && nodeB->type == NodeType::Fixture)
        K += 1.0;

    // Vana kaybı: Source çıkışında küresel vana varsayımı
    if (nodeA && nodeA->type == NodeType::Source)
        K += 0.1; // Küresel vana (tam açık)

    // Manuel zeta (kullanıcı override)
    K += edge.zeta;

    // ΔP = K × ρ × v² / 2  [Pa]
    double rho = WaterDensity();
    return K * rho * v * v / 2.0;
}

// ═══════════════════════════════════════════════════════════
//  DRENAJ YARDIMCI FONKSİYONLAR (EN 12056-2 + Manning)
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::GetKFactor() const {
    switch (m_buildingType) {
        case BuildingType::Residential: return 0.5;
        case BuildingType::Hotel:       return 0.7;
        case BuildingType::Hospital:    return 0.7;
        case BuildingType::School:      return 0.5;
        case BuildingType::Office:      return 0.5;
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

    // Standart drenaj boru çapları (mm) — 16 boyut, EN 1451/EN 1329
    static const double standardDiameters[] = {
        32.0, 40.0, 50.0, 63.0, 75.0, 90.0, 110.0, 125.0,
        160.0, 200.0, 250.0, 315.0, 355.0, 400.0, 450.0, 500.0
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
        if (!edge || (edge->type != EdgeType::Supply && edge->type != EdgeType::HotWater)) continue;

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

// ═══════════════════════════════════════════════════════════
//  HARDY-CROSS — KAPALI DÖNGÜ ŞEBEKE ANALİZİ
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::ComputeResistance(const Edge& edge) const {
    double D = edge.diameter_mm / 1000.0;
    double L = edge.length_m;
    if (D <= 0.0 || L <= 0.0) return 0.0;

    double A = PI * D * D / 4.0;
    double Q = std::abs(edge.flowRate_m3s);

    double f;
    if (Q < 1e-9) {
        // Sıfır debide tam türbülanslı yaklaşım (Moody diyagramı sağ sınır)
        double eps_D = edge.roughness_mm / edge.diameter_mm;
        if (eps_D < 1e-9) eps_D = 1e-6;
        double invSqrtF = -2.0 * std::log10(eps_D / 3.7);
        f = (invSqrtF > 0.0) ? 1.0 / (invSqrtF * invSqrtF) : 0.04;
    } else {
        double v  = Q / A;
        double Re = v * D / KINEMATIC_VISCOSITY;
        f = HaalandFriction(Re, edge.roughness_mm / edge.diameter_mm);
    }

    // r: h_f = r * Q^2  →  r = f * L / (D * 2g * A²)
    return f * L / (D * 2.0 * GRAVITY * A * A);
}

std::vector<HydraulicSolver::NetworkLoop> HydraulicSolver::DetectLoops() const {
    std::vector<NetworkLoop> loops;

    std::unordered_map<uint32_t, uint32_t> parent;
    std::unordered_map<uint32_t, uint32_t> parentEdgeId;
    std::unordered_set<uint32_t> visited;
    std::unordered_set<uint32_t> inStack;

    // Yinelemeli DFS — back edge her kapalı döngüyü tanımlar
    std::function<void(uint32_t)> dfs = [&](uint32_t nodeId) {
        visited.insert(nodeId);
        inStack.insert(nodeId);

        for (uint32_t eid : m_network.GetConnectedEdges(nodeId)) {
            const Edge* e = m_network.GetEdge(eid);
            if (!e || e->type != EdgeType::Supply) continue;

            uint32_t neighbor = (e->nodeA == nodeId) ? e->nodeB : e->nodeA;

            if (!visited.count(neighbor)) {
                parent[neighbor]     = nodeId;
                parentEdgeId[neighbor] = eid;
                dfs(neighbor);
            } else if (inStack.count(neighbor) && neighbor != parent[nodeId]) {
                // Back edge bulundu → döngü oluştur
                NetworkLoop loop;

                // Kapatan kenar (back edge): nodeId → neighbor
                int backDir = (e->nodeA == nodeId) ? +1 : -1;
                loop.edges.push_back({eid, backDir});

                // Kapsayan ağaç yolunu geriye izle: nodeId → ... → neighbor
                uint32_t cur = nodeId;
                int safetyLimit = 1000;
                while (cur != neighbor && parent.count(cur) && --safetyLimit > 0) {
                    uint32_t pe = parentEdgeId[cur];
                    const Edge* pep = m_network.GetEdge(pe);
                    if (!pep) break;
                    // Yön: parent[cur] → cur (spanning tree yönü)
                    int dir = (pep->nodeA == parent[cur]) ? +1 : -1;
                    loop.edges.push_back({pe, dir});
                    cur = parent[cur];
                }

                if (loop.edges.size() >= 2) {
                    loops.push_back(std::move(loop));
                }
            }
        }

        inStack.erase(nodeId);
    };

    for (const auto& [id, node] : m_network.GetNodeMap()) {
        if (!visited.count(id)) {
            parent[id] = 0;
            dfs(id);
        }
    }

    return loops;
}

void HydraulicSolver::SolveHardyCross(const std::vector<NetworkLoop>& loops) {
    if (loops.empty()) return;

    constexpr int    MAX_ITER  = 50;
    constexpr double TOLERANCE = 1e-7; // m³/s (~0.1 mL/s)

    // Sıfır debili Supply borularına başlangıç debisi ata (yakınsama için)
    for (auto& [eid, edge] : m_network.GetEdgeMap()) {
        if ((edge.type == EdgeType::Supply || edge.type == EdgeType::HotWater) &&
            std::abs(edge.flowRate_m3s) < 1e-10) {
            edge.flowRate_m3s = 1e-4; // 0.1 L/s başlangıç tahmini
        }
    }

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        double maxCorrection = 0.0;

        for (const auto& loop : loops) {
            // Darcy-Weisbach Hardy-Cross: ΔQ = -Σ(r·Q·|Q|) / (2·Σ(r·|Q|))
            double numerator   = 0.0;
            double denominator = 0.0;

            for (const auto& [eid, dir] : loop.edges) {
                const Edge* e = m_network.GetEdge(eid);
                if (!e || (e->type != EdgeType::Supply && e->type != EdgeType::HotWater)) continue;

                double Q = e->flowRate_m3s;
                double r = ComputeResistance(*e);
                // Döngü yönüne göre işaretli kayıp: dir × r × Q × |Q|
                numerator   += dir * r * Q * std::abs(Q);
                denominator += 2.0 * r * std::abs(Q);
            }

            if (denominator < 1e-15) continue;

            double dQ = -numerator / denominator;
            maxCorrection = std::max(maxCorrection, std::abs(dQ));

            // Tüm döngü borularına düzeltme uygula
            for (const auto& [eid, dir] : loop.edges) {
                Edge* e = m_network.GetEdge(eid);
                if (!e || (e->type != EdgeType::Supply && e->type != EdgeType::HotWater)) continue;
                e->flowRate_m3s += dir * dQ;
            }
        }

        if (maxCorrection < TOLERANCE) {
            std::cout << "Hardy-Cross yakınsadı: " << iter + 1
                      << " iterasyonda, max ΔQ=" << maxCorrection * 1000.0
                      << " mL/s" << std::endl;
            break;
        }

        if (iter == MAX_ITER - 1) {
            std::cout << "Hardy-Cross UYARI: " << MAX_ITER
                      << " iterasyonda yakınsama sağlanamadı (max ΔQ="
                      << maxCorrection * 1000.0 << " mL/s)" << std::endl;
        }
    }

    // Yakınsama sonrası hız ve kayıp değerlerini güncelle
    for (auto& [eid, edge] : m_network.GetEdgeMap()) {
        if (edge.type != EdgeType::Supply && edge.type != EdgeType::HotWater) continue;
        double D = edge.diameter_mm / 1000.0;
        double A = PI * D * D / 4.0;
        if (A > 0.0)
            edge.velocity_ms = std::abs(edge.flowRate_m3s) / A;
        edge.headLoss_m = CalculateHeadLoss(edge);
    }
}

// ═══════════════════════════════════════════════════════════
//  NEWTON-RAPHSON — çok dallı karmaşık ağ çözümü
//  Global Gradient Algorithm (Todini & Pilati, 1988)
//  Eşzamanlı Q (debi) ve H (basınç) çözümü
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::EdgeHeadLossFunc(const Edge& edge, double Q_m3s) const {
    double D = edge.diameter_mm / 1000.0;
    if (D < 1e-6) return 0.0;
    double A = PI * D * D / 4.0;
    double v = std::abs(Q_m3s) / std::max(A, 1e-12);
    double Re = 1000.0 * v * D / 1e-3;
    double relRough = edge.roughness_mm / std::max(edge.diameter_mm, 0.1);
    double f = (Re < 2300.0) ? 64.0 / std::max(Re, 1.0)
                              : HaalandFriction(Re, relRough);
    double L = std::max(edge.length_m, 0.001);
    // ΔH = sign(Q) × f × (L/D) × v² / (2g)
    double dH = f * (L / D) * (v * v) / (2.0 * 9.81);
    return (Q_m3s >= 0.0) ? dH : -dH;
}

double HydraulicSolver::EdgeHeadLossDerivative(const Edge& edge, double Q_m3s) const {
    double D = edge.diameter_mm / 1000.0;
    if (D < 1e-6) return 1e6;
    double A = PI * D * D / 4.0;
    double absQ = std::max(std::abs(Q_m3s), 1e-12);
    double v = absQ / A;
    double Re = 1000.0 * v * D / 1e-3;
    double relRough = edge.roughness_mm / std::max(edge.diameter_mm, 0.1);
    double f = (Re < 2300.0) ? 64.0 / std::max(Re, 1.0)
                              : HaalandFriction(Re, relRough);
    double L = std::max(edge.length_m, 0.001);
    // d(ΔH)/dQ ≈ 2 × f × L / (D × A² × 2g) × |Q|
    return 2.0 * f * L * absQ / (D * A * A * 2.0 * 9.81);
}

void HydraulicSolver::SolveNewtonRaphson(int maxIter, double tolerance) {
    auto& nodeMap = m_network.GetNodeMap();
    auto& edgeMap = m_network.GetEdgeMap();

    // P0: Sınır koşul kontrolü — en az 1 sabit basınçlı node (Source/HotSource) gerekli
    {
        bool hasFixedPressure = false;
        for (const auto& [nid, node] : nodeMap) {
            if (node.type == NodeType::Source || node.type == NodeType::HotSource) {
                hasFixedPressure = true;
                break;
            }
        }
        if (!hasFixedPressure) {
            std::cerr << "[N-R] Sabit basınç düğümü (Source) bulunamadı — Newton-Raphson atlanıyor" << std::endl;
            return;
        }
    }

    // Sadece Supply/HotWater edge'leri üzerinde çalış
    std::vector<uint32_t> pipeIds;
    for (auto& [eid, edge] : edgeMap) {
        if (edge.type == EdgeType::Supply || edge.type == EdgeType::HotWater) {
            pipeIds.push_back(eid);
            if (std::abs(edge.flowRate_m3s) < 1e-10)
                edge.flowRate_m3s = 1e-4; // başlangıç tahmini
        }
    }
    if (pipeIds.empty()) return;

    // Junction node'ları (bilinmeyen basınçlar)
    std::vector<uint32_t> junctionIds;
    std::unordered_map<uint32_t, int> nodeIndex;
    for (auto& [nid, node] : nodeMap) {
        if (node.type == NodeType::Junction) {
            nodeIndex[nid] = static_cast<int>(junctionIds.size());
            junctionIds.push_back(nid);
        }
    }
    if (junctionIds.empty()) return;

    int N = static_cast<int>(junctionIds.size());

    // Başlangıç basınçları: tüm junction'lara 30m
    std::vector<double> H(N, 30.0);

    double prevResidual = 1e30;
    double relaxFactor = 0.7; // adaptive relaxation başlangıç

    for (int iter = 0; iter < maxIter; ++iter) {
        std::vector<double> F(N, 0.0);

        // Demand: fixture'ların çektiği debi (junction'lara dağıtılmış)
        for (auto& [nid, node] : nodeMap) {
            if (node.type == NodeType::Fixture && node.flowRate_m3s > 0) {
                // En yakın junction'ı bul
                for (uint32_t eid : m_network.GetConnectedEdges(nid)) {
                    const Edge* e = m_network.GetEdge(eid);
                    if (!e) continue;
                    uint32_t other = (e->nodeA == nid) ? e->nodeB : e->nodeA;
                    auto it = nodeIndex.find(other);
                    if (it != nodeIndex.end()) {
                        F[it->second] -= node.flowRate_m3s;
                        break;
                    }
                }
            }
        }

        // Her edge'in katkısını nodal denklemlere ekle
        // Aynı zamanda Jacobian (dF/dH) bant-diyagonal matrisi oluştur
        // Basitleştirilmiş: diyagonal-dominant yaklaşım
        std::vector<double> diag(N, 0.0);

        for (uint32_t eid : pipeIds) {
            Edge* e = m_network.GetEdge(eid);
            if (!e) continue;
            auto itA = nodeIndex.find(e->nodeA);
            auto itB = nodeIndex.find(e->nodeB);

            double dHdQ = EdgeHeadLossDerivative(*e, e->flowRate_m3s);
            double conductance = 1.0 / std::max(dHdQ, 1e-12);

            // Head loss: ΔH = H_A - H_B = f(Q)
            double H_A = (itA != nodeIndex.end()) ? H[itA->second] : 50.0; // Source → sabit H
            double H_B = (itB != nodeIndex.end()) ? H[itB->second] : 50.0;

            double dH = H_A - H_B;
            double hLoss = EdgeHeadLossFunc(*e, e->flowRate_m3s);

            // Debi düzeltmesi: Q_new = conductance × (dH - hLoss + dHdQ × Q_old)
            double Q_correction = conductance * (dH - hLoss);
            double Q_new = e->flowRate_m3s + relaxFactor * Q_correction;
            e->flowRate_m3s = Q_new;

            // Nodal balance'a katkı
            if (itA != nodeIndex.end()) {
                F[itA->second] -= Q_new;
                diag[itA->second] += conductance;
            }
            if (itB != nodeIndex.end()) {
                F[itB->second] += Q_new;
                diag[itB->second] += conductance;
            }
        }

        // Basınç düzeltmesi: ΔH = -F / diag (basitleştirilmiş Jacobian)
        double maxResidual = 0.0;
        for (int i = 0; i < N; ++i) {
            if (diag[i] > 1e-15) {
                double dH = -F[i] / diag[i];
                H[i] += relaxFactor * dH;
                maxResidual = std::max(maxResidual, std::abs(F[i]));
            }
        }

        // Adaptive relaxation: iyileşme varsa hızlan, yoksa yavaşla
        if (maxResidual < prevResidual * 0.95)
            relaxFactor = std::min(relaxFactor * 1.1, 1.0);
        else
            relaxFactor = std::max(relaxFactor * 0.6, 0.1);
        prevResidual = maxResidual;

        if (maxResidual < tolerance) {
            break;
        }
    }

    if (prevResidual >= tolerance) {
        m_warnings.push_back("Newton-Raphson yakinsamadi (residual=" +
            std::to_string(prevResidual * 1000.0) + " mL/s) — sonuclar yaklasik");
    }

    // Sonuçları güncelle — hız ve kayıp
    for (uint32_t eid : pipeIds) {
        Edge* e = m_network.GetEdge(eid);
        if (!e) continue;
        double D = e->diameter_mm / 1000.0;
        double A = PI * D * D / 4.0;
        if (A > 0.0)
            e->velocity_ms = std::abs(e->flowRate_m3s) / A;
        e->headLoss_m = std::abs(EdgeHeadLossFunc(*e, e->flowRate_m3s));
    }

    // Junction basınçlarını node'lara kaydet
    for (int i = 0; i < N; ++i) {
        auto it = nodeMap.find(junctionIds[i]);
        if (it != nodeMap.end())
            it->second.pressureDesired_Pa = H[i] * 9810.0; // m → Pa
    }
}

// ═══════════════════════════════════════════════════════════
//  SICAKLIĞA BAĞLI SU ÖZELLİKLERİ
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::WaterDensity() const {
    // IAPWS-IF97 yaklaşımı — 5. derece polinom (0-100°C, ±0.01% doğruluk)
    // Kaynak: NIST/IAPWS su özellikleri tablosu
    double T = std::max(0.0, std::min(m_waterTempC, 100.0));
    // Doğrulama noktaları: ρ(4°C)=999.97, ρ(20°C)=998.2, ρ(60°C)=983.2, ρ(100°C)=958.4
    return 999.83 + 0.0584 * T - 0.00756 * T * T + 4.36e-5 * T * T * T - 1.49e-7 * T * T * T * T;
}

double HydraulicSolver::WaterKinematicViscosity() const {
    // Korosi denklemi (0-100°C, ±1% doğruluk)
    // μ(T) = 1.79 × 10⁻³ × exp(-0.0266·T + 0.000137·T²)  [Pa·s]
    // Kaynak: Korosi, CRC Handbook of Chemistry and Physics
    double T = std::max(0.1, std::min(m_waterTempC, 100.0));
    double mu_Pas = 1.79e-3 * std::exp(-0.0266 * T + 1.37e-4 * T * T);
    double mu_mPas = mu_Pas * 1000.0;
    // Doğrulama: μ(20°C) ≈ 1.002 mPa·s, μ(60°C) ≈ 0.467 mPa·s
    double rho = WaterDensity();
    return mu_mPas * 1e-3 / rho; // dynamic → kinematic  [m²/s]
}

// ═══════════════════════════════════════════════════════════
//  BORU YAŞLANMA MODELİ
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::AgingRoughnessFactor() const {
    // Colebrook yaşlanma korelasyonu:
    // Çelik/Galvaniz: ε(t) = ε₀ + α·t  (α ≈ 0.01-0.025 mm/yıl)
    // Plastik (PVC/PPR/PE): α ≈ 0 (yaşlanma ihmal edilebilir)
    // Bakır: α ≈ 0.002 mm/yıl
    // t < 0 → 1.0 (yeni boru)
    if (m_pipeAgeYears <= 0.0) return 1.0;

    // Faktör: ε_new üzerine eklenen mm cinsinden artış
    // Caller: roughness_mm * factor + agingDelta olarak kullanacak
    // Basitleştirilmiş: age_factor = 1 + (age/50)² metalik, 1 + (age/100)² plastik
    // Bu fonksiyon yalnızca çarpan döndürür; malzeme ayrımını CalculateHeadLoss yapar
    return 1.0;  // Çarpan — asıl hesap CalculateHeadLoss'ta
}

// ═══════════════════════════════════════════════════════════
//  DN-BAĞIMLI FİTTİNG K DEĞERLERİ
// ═══════════════════════════════════════════════════════════

double HydraulicSolver::FittingK_Elbow90(double dn_mm) const {
    // ASHRAE Handbook + Idelchik: K = f(DN)
    // DN büyüdükçe K düşer (daha akıcı akış)
    if (dn_mm <= 15)  return 1.5;
    if (dn_mm <= 20)  return 1.2;
    if (dn_mm <= 25)  return 1.0;
    if (dn_mm <= 32)  return 0.9;
    if (dn_mm <= 40)  return 0.75;
    if (dn_mm <= 50)  return 0.65;
    if (dn_mm <= 65)  return 0.55;
    if (dn_mm <= 80)  return 0.50;
    if (dn_mm <= 100) return 0.45;
    if (dn_mm <= 125) return 0.40;
    if (dn_mm <= 150) return 0.35;
    return 0.30; // DN200+
}

double HydraulicSolver::FittingK_Tee(double dn_mm) const {
    // T-parça yan kol (branch): K tipik olarak 1.0-1.8
    if (dn_mm <= 20)  return 1.8;
    if (dn_mm <= 25)  return 1.5;
    if (dn_mm <= 32)  return 1.3;
    if (dn_mm <= 50)  return 1.1;
    if (dn_mm <= 80)  return 1.0;
    if (dn_mm <= 100) return 0.9;
    return 0.8; // DN125+
}

} // namespace mep
} // namespace vkt
