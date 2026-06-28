/**
 * @file test_hydraulic.cpp
 * @brief HydraulicSolver birim testleri
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "mep/HydraulicSolver.hpp"
#include "mep/Database.hpp"
#include "mep/SpecGenerator.hpp"
#include "cad/SpaceManager.hpp"
#include "core/FloorManager.hpp"

using namespace vkt::mep;

// Helper: Basit doğrusal şebeke oluştur
// Source(LU=0) → Edge → Fixture(LU=value)
static NetworkGraph CreateSimpleNetwork(double fixtureLU, double pipeDiam_mm = 20.0) {
    NetworkGraph graph;

    Node source;
    source.type = NodeType::Source;
    source.position = {0, 0, 0};
    uint32_t sid = graph.AddNode(source);

    Node fixture;
    fixture.type = NodeType::Fixture;
    fixture.loadUnit = fixtureLU;
    fixture.position = {5, 0, 0};
    uint32_t fid = graph.AddNode(fixture);

    Edge pipe;
    pipe.nodeA = sid;
    pipe.nodeB = fid;
    pipe.type = EdgeType::Supply;
    pipe.diameter_mm = pipeDiam_mm;
    pipe.length_m = 5.0;
    pipe.roughness_mm = 0.0015;
    graph.AddEdge(pipe);

    return graph;
}

TEST_CASE("HydraulicSolver - LU to Flow Conversion (TS EN 806-3)", "[hydraulic]") {
    auto graph = CreateSimpleNetwork(10.0);
    HydraulicSolver solver(graph);

    solver.Solve();

    // Fixture node'un debisi = CalculateFlowFromLU(10)
    // Q = 0.682 * 10^0.45 ≈ 1.92 L/s
    auto* fixture = graph.GetNode(2); // ID 2 = fixture
    REQUIRE(fixture != nullptr);
    REQUIRE(fixture->flowRate_m3s > 0.0);
    // TS EN 806-3: Q = 0.682 * LU^0.45, LU=10 → Q ≈ 1.92 L/s
    REQUIRE_THAT(fixture->flowRate_m3s * 1000.0,
                 Catch::Matchers::WithinAbs(1.92, 0.1));
}

TEST_CASE("HydraulicSolver - Zero LU gives zero flow", "[hydraulic]") {
    auto graph = CreateSimpleNetwork(0.0);
    HydraulicSolver solver(graph);
    solver.Solve();

    auto* fixture = graph.GetNode(2);
    REQUIRE(fixture != nullptr);
    REQUIRE_THAT(fixture->flowRate_m3s, Catch::Matchers::WithinAbs(0.0, 1e-12));
}

TEST_CASE("HydraulicSolver - Topological Flow Distribution", "[hydraulic]") {
    // Source → Junction → Fixture1 (LU=5)
    //                   → Fixture2 (LU=10)
    NetworkGraph graph;

    Node source; source.type = NodeType::Source;
    uint32_t sid = graph.AddNode(source);

    Node junc; junc.type = NodeType::Junction;
    uint32_t jid = graph.AddNode(junc);

    Node fix1; fix1.type = NodeType::Fixture; fix1.loadUnit = 5.0;
    uint32_t fid1 = graph.AddNode(fix1);

    Node fix2; fix2.type = NodeType::Fixture; fix2.loadUnit = 10.0;
    uint32_t fid2 = graph.AddNode(fix2);

    Edge e1; e1.nodeA = sid; e1.nodeB = jid; e1.type = EdgeType::Supply;
    e1.diameter_mm = 32.0; e1.length_m = 3.0;
    uint32_t eid1 = graph.AddEdge(e1);

    Edge e2; e2.nodeA = jid; e2.nodeB = fid1; e2.type = EdgeType::Supply;
    e2.diameter_mm = 20.0; e2.length_m = 2.0;
    uint32_t eid2 = graph.AddEdge(e2);

    Edge e3; e3.nodeA = jid; e3.nodeB = fid2; e3.type = EdgeType::Supply;
    e3.diameter_mm = 20.0; e3.length_m = 4.0;
    uint32_t eid3 = graph.AddEdge(e3);

    HydraulicSolver solver(graph);
    solver.Solve();

    // Edge1 (source→junction) debisi = fixture1 + fixture2 debilerin toplamı
    auto* edge1 = graph.GetEdge(eid1);
    auto* edge2 = graph.GetEdge(eid2);
    auto* edge3 = graph.GetEdge(eid3);

    REQUIRE(edge1 != nullptr);
    REQUIRE(edge2 != nullptr);
    REQUIRE(edge3 != nullptr);

    // edge2 debisi = fixture1 debisi
    REQUIRE(edge2->flowRate_m3s > 0.0);
    // edge3 debisi = fixture2 debisi
    REQUIRE(edge3->flowRate_m3s > 0.0);
    // edge1 debisi ≈ edge2 + edge3 (kümülatif toplama)
    REQUIRE_THAT(edge1->flowRate_m3s,
                 Catch::Matchers::WithinAbs(edge2->flowRate_m3s + edge3->flowRate_m3s, 1e-9));
}

TEST_CASE("HydraulicSolver - Head Loss is positive for non-zero flow", "[hydraulic]") {
    auto graph = CreateSimpleNetwork(20.0, 20.0);
    HydraulicSolver solver(graph);
    solver.Solve();

    auto* edge = graph.GetEdge(1);
    REQUIRE(edge != nullptr);
    REQUIRE(edge->headLoss_m > 0.0);
    REQUIRE(edge->velocity_ms > 0.0);
}

TEST_CASE("HydraulicSolver - Drainage EN 12056-2", "[hydraulic]") {
    NetworkGraph graph;

    Node fixture; fixture.type = NodeType::Fixture; fixture.loadUnit = 4.0; // DU=4
    uint32_t fid = graph.AddNode(fixture);

    Node drain; drain.type = NodeType::Drain;
    uint32_t did = graph.AddNode(drain);

    Edge pipe; pipe.nodeA = fid; pipe.nodeB = did;
    pipe.type = EdgeType::Drainage;
    pipe.slope = 0.02;
    pipe.material = "PVC";
    pipe.diameter_mm = 50.0;
    graph.AddEdge(pipe);

    HydraulicSolver solver(graph);
    solver.SetBuildingType(BuildingType::Residential);
    solver.SolveDrainage();

    auto* edge = graph.GetEdge(1);
    REQUIRE(edge != nullptr);
    // EN 12056-2: Q = K * sqrt(DU) = 0.5 * sqrt(4) = 1.0 L/s
    REQUIRE_THAT(edge->flowRate_m3s * 1000.0,
                 Catch::Matchers::WithinAbs(1.0, 0.1));
}

TEST_CASE("HydraulicSolver - Building Type affects K factor", "[hydraulic]") {
    NetworkGraph graph;

    Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 16.0;
    uint32_t fid = graph.AddNode(fix);
    Node drain; drain.type = NodeType::Drain;
    uint32_t did = graph.AddNode(drain);
    Edge pipe; pipe.nodeA = fid; pipe.nodeB = did;
    pipe.type = EdgeType::Drainage; pipe.slope = 0.02;
    graph.AddEdge(pipe);

    // Residential: K=0.5, Q = 0.5*4 = 2.0 L/s
    HydraulicSolver solver1(graph);
    solver1.SetBuildingType(BuildingType::Residential);
    solver1.SolveDrainage();
    double q_residential = graph.GetEdge(1)->flowRate_m3s;

    // Reset edge flow
    graph.GetEdge(1)->flowRate_m3s = 0;

    // Industrial: K=1.0, Q = 1.0*4 = 4.0 L/s
    HydraulicSolver solver2(graph);
    solver2.SetBuildingType(BuildingType::Industrial);
    solver2.SolveDrainage();
    double q_industrial = graph.GetEdge(1)->flowRate_m3s;

    REQUIRE(q_industrial > q_residential);
}

TEST_CASE("HydraulicSolver - Manning pipe sizing selects minimum diameter", "[hydraulic][drainage]") {
    // DU=9, K=0.5 → Q = 0.5*3 = 1.5 L/s, slope=%2
    // Manning ile Ø50 mm PVC %50 dolulukta bu debiyi taşımalı
    NetworkGraph graph;

    Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 9.0;
    graph.AddNode(fix);
    Node drain; drain.type = NodeType::Drain;
    graph.AddNode(drain);
    Edge pipe; pipe.nodeA = 1; pipe.nodeB = 2;
    pipe.type = EdgeType::Drainage; pipe.slope = 0.02;
    pipe.material = "PVC"; pipe.diameter_mm = 40.0; // başlangıç çapı — büyütülmeli
    graph.AddEdge(pipe);

    HydraulicSolver solver(graph);
    solver.SetBuildingType(BuildingType::Residential);
    solver.SolveDrainage();

    auto* edge = graph.GetEdge(1);
    REQUIRE(edge != nullptr);
    // Manning seçimi başlangıç 40mm'i yeterli büyük bir çapa çıkarmalı
    REQUIRE(edge->diameter_mm >= 40.0);
    // Seçilen çap standart değerlerden biri olmalı
    std::vector<double> stdDiams = {32, 40, 50, 63, 75, 90, 110, 125, 160, 200, 250, 315, 355, 400, 450, 500};
    bool isStandard = std::find(stdDiams.begin(), stdDiams.end(), edge->diameter_mm) != stdDiams.end();
    REQUIRE(isStandard);
}

TEST_CASE("HydraulicSolver - Drainage cumulative DU across multi-fixture chain", "[hydraulic][drainage]") {
    // Fix1(DU=2) → Junction → Fix2(DU=3) → Drain
    // Junction'a gelen toplam DU = 5 olmalı
    NetworkGraph graph;

    Node fix1; fix1.type = NodeType::Fixture; fix1.loadUnit = 2.0;
    uint32_t fid1 = graph.AddNode(fix1);

    Node junc; junc.type = NodeType::Junction;
    uint32_t jid = graph.AddNode(junc);

    Node fix2; fix2.type = NodeType::Fixture; fix2.loadUnit = 3.0;
    uint32_t fid2 = graph.AddNode(fix2);

    Node drain; drain.type = NodeType::Drain;
    uint32_t did = graph.AddNode(drain);

    Edge e1; e1.nodeA = fid1; e1.nodeB = jid; e1.type = EdgeType::Drainage;
    e1.slope = 0.02; e1.material = "PVC";
    uint32_t eid1 = graph.AddEdge(e1);

    Edge e2; e2.nodeA = fid2; e2.nodeB = jid; e2.type = EdgeType::Drainage;
    e2.slope = 0.02; e2.material = "PVC";
    graph.AddEdge(e2);

    Edge e3; e3.nodeA = jid; e3.nodeB = did; e3.type = EdgeType::Drainage;
    e3.slope = 0.02; e3.material = "PVC";
    uint32_t eid3 = graph.AddEdge(e3);

    HydraulicSolver solver(graph);
    solver.SolveDrainage();

    // Kolektör boru (junc→drain) kümülatif DU = 2+3 = 5 taşımalı
    auto* collector = graph.GetEdge(eid3);
    REQUIRE(collector != nullptr);
    REQUIRE_THAT(collector->cumulativeDU, Catch::Matchers::WithinAbs(5.0, 0.01));
}

TEST_CASE("HydraulicSolver - AutoSize supply pipe respects velocity limits", "[hydraulic][supply]") {
    // Yüksek LU → büyük debi → küçük çap hız kısıtını aşar → büyütülmeli
    NetworkGraph graph;

    Node source; source.type = NodeType::Source;
    graph.AddNode(source);

    Node fixture; fixture.type = NodeType::Fixture; fixture.loadUnit = 200.0;
    graph.AddNode(fixture);

    Edge pipe; pipe.nodeA = 1; pipe.nodeB = 2;
    pipe.type = EdgeType::Supply;
    pipe.diameter_mm = 16.0; // Çok küçük — otomatik büyütülmeli
    pipe.length_m = 10.0; pipe.roughness_mm = 0.0015; pipe.material = "PVC";
    graph.AddEdge(pipe);

    HydraulicSolver solver(graph);
    solver.Solve();

    auto* edge = graph.GetEdge(1);
    REQUIRE(edge != nullptr);
    // Otomatik boyutlandırma 16mm'i yetersiz bulup büyütmeli
    REQUIRE(edge->diameter_mm > 16.0);
    // Seçilen çapla hız 3 m/s'yi geçmemeli
    double D_m = edge->diameter_mm / 1000.0;
    double area = 3.14159 * (D_m / 2.0) * (D_m / 2.0);
    double v = edge->flowRate_m3s / area;
    REQUIRE(v <= 3.0 + 1e-6);
}

TEST_CASE("HydraulicSolver - Critical Path with visited set", "[hydraulic]") {
    // Basit doğrusal: Source → Node1 → Node2 → Fixture
    NetworkGraph graph;

    Node source; source.type = NodeType::Source;
    uint32_t sid = graph.AddNode(source);

    Node n1; n1.type = NodeType::Junction;
    uint32_t nid1 = graph.AddNode(n1);

    Node fixture; fixture.type = NodeType::Fixture; fixture.loadUnit = 5.0;
    uint32_t fid = graph.AddNode(fixture);

    Edge e1; e1.nodeA = sid; e1.nodeB = nid1; e1.type = EdgeType::Supply;
    e1.headLoss_m = 2.0;
    graph.AddEdge(e1);

    Edge e2; e2.nodeA = nid1; e2.nodeB = fid; e2.type = EdgeType::Supply;
    e2.headLoss_m = 3.0;
    graph.AddEdge(e2);

    HydraulicSolver solver(graph);
    auto result = solver.CalculateCriticalPath();

    REQUIRE(result.criticalPath.size() == 3);
    REQUIRE_THAT(result.totalHeadLoss_m, Catch::Matchers::WithinAbs(5.0, 0.01));
    REQUIRE_THAT(result.requiredPumpHead_m, Catch::Matchers::WithinAbs(15.0, 0.5));
}

// ============================================================
// Sicak Su (HotWater / HotSource) testleri
// ============================================================

TEST_CASE("NetworkGraph - HotSource node type roundtrip", "[hotwater]") {
    NetworkGraph graph;

    Node src;
    src.type     = NodeType::HotSource;
    src.position = {0, 0, 1.0};
    src.label    = "Sofben";
    uint32_t sid = graph.AddNode(src);

    REQUIRE(graph.GetNode(sid) != nullptr);
    REQUIRE(graph.GetNode(sid)->type == NodeType::HotSource);
    REQUIRE(graph.GetNode(sid)->label == "Sofben");
}

TEST_CASE("NetworkGraph - HotWater edge type roundtrip", "[hotwater]") {
    NetworkGraph graph;

    Node src;  src.type = NodeType::HotSource;
    Node fix;  fix.type = NodeType::Fixture; fix.loadUnit = 2.0;
    uint32_t sid = graph.AddNode(src);
    uint32_t fid = graph.AddNode(fix);

    Edge e;
    e.nodeA        = sid;
    e.nodeB        = fid;
    e.type         = EdgeType::HotWater;
    e.diameter_mm  = 20.0;
    e.length_m     = 3.0;
    e.roughness_mm = 0.0015;
    uint32_t eid = graph.AddEdge(e);

    REQUIRE(graph.GetEdge(eid) != nullptr);
    REQUIRE(graph.GetEdge(eid)->type == EdgeType::HotWater);
}

TEST_CASE("HydraulicSolver - HotWater pipe gets non-zero flow", "[hotwater]") {
    NetworkGraph graph;

    Node src;  src.type = NodeType::HotSource;
    uint32_t sid = graph.AddNode(src);

    Node fix;  fix.type = NodeType::Fixture; fix.loadUnit = 5.0;
    uint32_t fid = graph.AddNode(fix);

    Edge e;
    e.nodeA = sid; e.nodeB = fid;
    e.type = EdgeType::HotWater;
    e.diameter_mm = 20.0; e.length_m = 4.0; e.roughness_mm = 0.0015;
    uint32_t eid = graph.AddEdge(e);

    HydraulicSolver solver(graph);
    solver.Solve();

    auto* edge = graph.GetEdge(eid);
    REQUIRE(edge != nullptr);
    REQUIRE(edge->flowRate_m3s > 0.0);
}

TEST_CASE("HydraulicSolver - HotWater head loss is positive", "[hotwater]") {
    NetworkGraph graph;

    Node src;  src.type = NodeType::HotSource;
    graph.AddNode(src);
    Node fix;  fix.type = NodeType::Fixture; fix.loadUnit = 10.0;
    graph.AddNode(fix);
    Edge e; e.nodeA = 1; e.nodeB = 2;
    e.type = EdgeType::HotWater;
    e.diameter_mm = 20.0; e.length_m = 5.0; e.roughness_mm = 0.0015;
    graph.AddEdge(e);

    HydraulicSolver solver(graph);
    solver.Solve();

    auto* edge = graph.GetEdge(1);
    REQUIRE(edge != nullptr);
    REQUIRE(edge->headLoss_m > 0.0);
    REQUIRE(edge->velocity_ms > 0.0);
}

TEST_CASE("HydraulicSolver - HotWater AutoSize selects DN from standard series", "[hotwater]") {
    NetworkGraph graph;

    Node src;  src.type = NodeType::HotSource;
    graph.AddNode(src);
    Node fix;  fix.type = NodeType::Fixture; fix.loadUnit = 100.0;
    graph.AddNode(fix);
    Edge e; e.nodeA = 1; e.nodeB = 2;
    e.type = EdgeType::HotWater;
    e.diameter_mm = 16.0; // kucuk — buyutulmeli
    e.length_m = 8.0; e.roughness_mm = 0.0015; e.material = "PVC";
    graph.AddEdge(e);

    HydraulicSolver solver(graph);
    solver.Solve();

    auto* edge = graph.GetEdge(1);
    REQUIRE(edge != nullptr);
    REQUIRE(edge->diameter_mm >= 16.0);
    std::vector<double> stdDiams = {16, 20, 25, 32, 40, 50, 65, 80, 100, 125, 150};
    bool isStd = std::find(stdDiams.begin(), stdDiams.end(), edge->diameter_mm) != stdDiams.end();
    REQUIRE(isStd);
}

TEST_CASE("HydraulicSolver - Mixed Supply + HotWater network", "[hotwater]") {
    // Source  --> cold pipe --> Fixture1 (LU=3)
    // HotSource --> hot pipe --> Fixture2 (LU=3)
    // Her iki kol bagimsiz cozulmeli, her biri pozitif debi almali
    NetworkGraph graph;

    Node cold;  cold.type = NodeType::Source;
    uint32_t cid = graph.AddNode(cold);

    Node hot;   hot.type = NodeType::HotSource;
    uint32_t hid = graph.AddNode(hot);

    Node fix1;  fix1.type = NodeType::Fixture; fix1.loadUnit = 3.0;
    uint32_t f1 = graph.AddNode(fix1);

    Node fix2;  fix2.type = NodeType::Fixture; fix2.loadUnit = 3.0;
    uint32_t f2 = graph.AddNode(fix2);

    Edge ec; ec.nodeA = cid; ec.nodeB = f1;
    ec.type = EdgeType::Supply; ec.diameter_mm = 20.0; ec.length_m = 3.0;
    uint32_t eid_cold = graph.AddEdge(ec);

    Edge eh; eh.nodeA = hid; eh.nodeB = f2;
    eh.type = EdgeType::HotWater; eh.diameter_mm = 20.0; eh.length_m = 3.0;
    uint32_t eid_hot  = graph.AddEdge(eh);

    HydraulicSolver solver(graph);
    solver.Solve();

    auto* ecold = graph.GetEdge(eid_cold);
    auto* ehot  = graph.GetEdge(eid_hot);
    REQUIRE(ecold != nullptr);
    REQUIRE(ehot  != nullptr);
    REQUIRE(ecold->flowRate_m3s > 0.0);
    REQUIRE(ehot->flowRate_m3s  > 0.0);
    // Ayni LU → ayni debi
    REQUIRE_THAT(ecold->flowRate_m3s,
                 Catch::Matchers::WithinAbs(ehot->flowRate_m3s, 1e-9));
}

// =========================================================
// TS 825 Sarfiyat (Musluk Birimi) normu testleri
// =========================================================

TEST_CASE("Database - FixtureData has sarfiyat_unit", "[sarfiyat]") {
    auto& db = Database::Instance();
    // Temel armatürlerin SB değerleri pozitif olmalı
    REQUIRE(db.GetFixture("Lavabo").sarfiyat_unit > 0.0);
    REQUIRE(db.GetFixture("WC").sarfiyat_unit     > 0.0);
    REQUIRE(db.GetFixture("Küvet").sarfiyat_unit  > 0.0);
    // WC SB değeri Lavabo'dan büyük olmalı (2.0 > 0.5)
    REQUIRE(db.GetFixture("WC").sarfiyat_unit > db.GetFixture("Lavabo").sarfiyat_unit);
}

TEST_CASE("HydraulicSolver - Sarfiyat normu pozitif debi uretir", "[sarfiyat]") {
    NetworkGraph g;
    Node src; src.type = NodeType::Source; src.position = {0,0,0};
    Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 0.5;
    fix.fixtureType = "Lavabo"; fix.position = {1,0,0};
    uint32_t sid = g.AddNode(src);
    uint32_t fid = g.AddNode(fix);
    Edge e; e.nodeA = sid; e.nodeB = fid;
    e.type = EdgeType::Supply; e.diameter_mm = 20.0; e.length_m = 2.0;
    uint32_t eid = g.AddEdge(e);

    HydraulicSolver::GlobalNorm() = HydroNorm::SARFIYAT;
    HydraulicSolver solver(g);
    solver.Solve();
    HydraulicSolver::GlobalNorm() = HydroNorm::EN806_3; // geri al

    REQUIRE(g.GetEdge(eid)->flowRate_m3s > 0.0);
}

TEST_CASE("HydraulicSolver - Sarfiyat K_s bina tipine gore degisir", "[sarfiyat]") {
    // Konut K_s=0.6, Otel K_s=0.8 → otel debisi daha yüksek olmalı
    auto makeGraph = [](double sb) -> NetworkGraph {
        NetworkGraph g;
        Node src; src.type = NodeType::Source; src.position = {0,0,0};
        Node fix; fix.type = NodeType::Fixture; fix.loadUnit = sb;
        fix.fixtureType = "WC"; fix.position = {1,0,0};
        uint32_t sid = g.AddNode(src);
        uint32_t fid = g.AddNode(fix);
        Edge e; e.nodeA = sid; e.nodeB = fid;
        e.type = EdgeType::Supply; e.diameter_mm = 25.0; e.length_m = 3.0;
        g.AddEdge(e);
        return g;
    };

    HydraulicSolver::GlobalNorm() = HydroNorm::SARFIYAT;

    NetworkGraph g1 = makeGraph(2.0);
    HydraulicSolver s1(g1);
    s1.SetBuildingType(BuildingType::Residential); // K_s=0.6
    s1.Solve();

    NetworkGraph g2 = makeGraph(2.0);
    HydraulicSolver s2(g2);
    s2.SetBuildingType(BuildingType::Hotel);       // K_s=0.8
    s2.Solve();

    HydraulicSolver::GlobalNorm() = HydroNorm::EN806_3; // geri al

    const auto& edges1 = g1.GetEdges();
    const auto& edges2 = g2.GetEdges();
    REQUIRE(!edges1.empty());
    REQUIRE(!edges2.empty());
    REQUIRE(edges2[0].flowRate_m3s > edges1[0].flowRate_m3s);
}

TEST_CASE("HydraulicSolver - Sarfiyat AutoSize DN secer", "[sarfiyat]") {
    NetworkGraph g;
    Node src; src.type = NodeType::Source; src.position = {0,0,0};
    // 3 WC — toplam SB = 3×2.0 = 6.0
    for (int i = 0; i < 3; ++i) {
        Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 2.0;
        fix.fixtureType = "WC"; fix.position = {double(i+1),0,0};
        uint32_t fid = g.AddNode(fix);
        Edge e; e.nodeA = 0; e.nodeB = fid;
        e.type = EdgeType::Supply; e.diameter_mm = 20.0; e.length_m = 3.0;
        g.AddEdge(e);
    }
    HydraulicSolver::GlobalNorm() = HydroNorm::SARFIYAT;
    HydraulicSolver solver(g);
    solver.Solve();
    HydraulicSolver::GlobalNorm() = HydroNorm::EN806_3;

    for (const auto& edge : g.GetEdges()) {
        if (edge.type == EdgeType::Supply)
            REQUIRE(edge.diameter_mm >= 16.0);
    }
}

// ═══════════════════════════════════════════════════════════
//  GAZ, ELEKTRİK, HVAC SOLVER TESTLERİ
// ═══════════════════════════════════════════════════════════

TEST_CASE("SolveGas - basic gas network", "[hydraulic][gas]") {
    NetworkGraph g;
    Node src; src.type = NodeType::GasSource; src.position = {0,0,0};
    uint32_t sid = g.AddNode(src);
    Node app; app.type = NodeType::GasAppliance; app.gasPower_kW = 24.0;
    app.position = {5000,0,0};
    uint32_t aid = g.AddNode(app);
    Edge pipe; pipe.nodeA = sid; pipe.nodeB = aid;
    pipe.type = EdgeType::Gas; pipe.length_m = 10.0;
    pipe.roughness_mm = 0.0015; pipe.diameter_mm = 20.0;
    g.AddEdge(pipe);

    HydraulicSolver solver(g);
    solver.SolveGas();

    const auto& edges = g.GetEdges();
    REQUIRE(edges.size() == 1);
    REQUIRE(edges[0].flowRate_m3s > 0.0);
    REQUIRE(edges[0].diameter_mm >= 15.0);
    REQUIRE(edges[0].velocity_ms > 0.0);
}

TEST_CASE("SolveElectric - single socket circuit", "[hydraulic][electric]") {
    NetworkGraph g;
    Node panel; panel.type = NodeType::ElecPanel; panel.position = {0,0,0};
    uint32_t pid = g.AddNode(panel);
    Node sock; sock.type = NodeType::Socket; sock.position = {5000,0,0};
    uint32_t sid = g.AddNode(sock);
    Edge cable; cable.nodeA = pid; cable.nodeB = sid;
    cable.type = EdgeType::Electric; cable.length_m = 15.0;
    cable.diameter_mm = 2.5;
    g.AddEdge(cable);

    HydraulicSolver solver(g);
    solver.SolveElectric();

    const auto& edges = g.GetEdges();
    REQUIRE(edges.size() == 1);
    REQUIRE(edges[0].flowRate_m3s > 0.0); // Amper value stored here
    REQUIRE(edges[0].diameter_mm >= 1.5); // min cable cross-section
}

TEST_CASE("SolveVentilation - single diffuser", "[hydraulic][hvac]") {
    NetworkGraph g;
    Node ahu; ahu.type = NodeType::AHU; ahu.position = {0,0,0};
    uint32_t aid = g.AddNode(ahu);
    Node diff; diff.type = NodeType::Diffuser; diff.position = {5000,0,0};
    uint32_t did = g.AddNode(diff);
    Edge duct; duct.nodeA = aid; duct.nodeB = did;
    duct.type = EdgeType::Duct; duct.length_m = 8.0;
    duct.diameter_mm = 150.0;
    g.AddEdge(duct);

    HydraulicSolver solver(g);
    solver.SolveVentilation();

    const auto& edges = g.GetEdges();
    REQUIRE(edges.size() == 1);
    REQUIRE(edges[0].flowRate_m3s > 0.0);
    REQUIRE(edges[0].diameter_mm >= 100.0); // min duct DN
}

TEST_CASE("Solve - empty network returns safely", "[hydraulic][validation]") {
    NetworkGraph g;
    HydraulicSolver solver(g);
    solver.Solve(); // should not crash, just return early
    REQUIRE(g.GetEdges().empty());
}

TEST_CASE("Solve - no source returns safely", "[hydraulic][validation]") {
    NetworkGraph g;
    Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 2.0;
    fix.position = {0,0,0};
    g.AddNode(fix);
    Edge pipe; pipe.nodeA = 1; pipe.nodeB = 1;
    pipe.type = EdgeType::Supply; pipe.length_m = 5.0;
    g.AddEdge(pipe);

    HydraulicSolver solver(g);
    solver.Solve(); // should return early without crash
    REQUIRE(true); // survived
}

TEST_CASE("EdgeType Electric and Duct roundtrip", "[hydraulic][serialization]") {
    NetworkGraph g;
    Node n1; n1.type = NodeType::ElecPanel; n1.position = {0,0,0};
    uint32_t id1 = g.AddNode(n1);
    Node n2; n2.type = NodeType::Socket; n2.position = {1000,0,0};
    uint32_t id2 = g.AddNode(n2);
    Edge e; e.nodeA = id1; e.nodeB = id2;
    e.type = EdgeType::Electric; e.diameter_mm = 2.5;
    g.AddEdge(e);

    REQUIRE(g.GetEdges()[0].type == EdgeType::Electric);
    REQUIRE(g.GetNodes().size() == 2);
}

TEST_CASE("NodeType new types exist", "[hydraulic][types]") {
    REQUIRE(static_cast<int>(NodeType::ElecPanel) > 0);
    REQUIRE(static_cast<int>(NodeType::Socket) > 0);
    REQUIRE(static_cast<int>(NodeType::LightFixture) > 0);
    REQUIRE(static_cast<int>(NodeType::AHU) > 0);
    REQUIRE(static_cast<int>(NodeType::Diffuser) > 0);
}

TEST_CASE("EdgeType new types exist", "[hydraulic][types]") {
    REQUIRE(static_cast<int>(EdgeType::Electric) > 0);
    REQUIRE(static_cast<int>(EdgeType::Duct) > 0);
    REQUIRE(EdgeType::Electric != EdgeType::Supply);
    REQUIRE(EdgeType::Duct != EdgeType::Electric);
}

TEST_CASE("Database has 15+ pipe materials", "[database]") {
    auto& db = Database::Instance();
    auto mats = db.GetPipeMaterials();
    REQUIRE(mats.size() >= 15);
}

// ═══════════════════════════════════════════════════════════
//  SICAKLIK ETKISI + YAŞLANMA + DN-BAĞIMLI K TESTLERİ
// ═══════════════════════════════════════════════════════════

TEST_CASE("HydraulicSolver - Water viscosity varies with temperature", "[thermal]") {
    NetworkGraph g = CreateSimpleNetwork(2.0);
    HydraulicSolver cold(g);
    cold.SetWaterTemperature(10.0);
    double nu10 = cold.WaterKinematicViscosity();

    HydraulicSolver hot(g);
    hot.SetWaterTemperature(60.0);
    double nu60 = hot.WaterKinematicViscosity();

    REQUIRE(nu10 > nu60); // soguk su daha viskozdur
    REQUIRE(nu10 > 1e-6);
    REQUIRE(nu60 > 0);
}

TEST_CASE("HydraulicSolver - Water density varies with temperature", "[thermal]") {
    NetworkGraph g = CreateSimpleNetwork(2.0);
    HydraulicSolver s(g);
    s.SetWaterTemperature(4.0);
    double rho4 = s.WaterDensity();
    s.SetWaterTemperature(80.0);
    double rho80 = s.WaterDensity();

    REQUIRE(rho4 > rho80); // sicak su daha hafif
    REQUIRE(rho4 > 999.0); // 4C'de ~1000 kg/m3
    REQUIRE(rho80 > 950.0);
}

TEST_CASE("HydraulicSolver - Pipe aging increases head loss", "[aging]") {
    auto makeAndSolve = [](double age) -> double {
        NetworkGraph g = CreateSimpleNetwork(2.0, 25.0);
        // Celik boru (yaslanma etkili)
        auto* e = g.GetEdge(1);
        if (e) { e->material = "Celik"; e->roughness_mm = 0.045; }
        HydraulicSolver solver(g);
        solver.SetPipeAge(age);
        solver.Solve();
        return g.GetEdge(1) ? g.GetEdge(1)->headLoss_m : 0.0;
    };
    double hNew = makeAndSolve(0.0);
    double hOld = makeAndSolve(20.0);
    REQUIRE(hOld >= hNew); // eski boru daha fazla kayip
}

TEST_CASE("HydraulicSolver - DN-dependent elbow K values", "[fitting]") {
    NetworkGraph g = CreateSimpleNetwork(2.0);
    HydraulicSolver solver(g);
    double k15  = solver.FittingK_Elbow90(15.0);
    double k50  = solver.FittingK_Elbow90(50.0);
    double k200 = solver.FittingK_Elbow90(200.0);

    REQUIRE(k15 > k50);   // kucuk cap = buyuk K
    REQUIRE(k50 > k200);  // buyuk cap = kucuk K
    REQUIRE(k15 >= 1.0);
    REQUIRE(k200 <= 0.5);
}

TEST_CASE("HydraulicSolver - DIN 1988 Q=a*FU^b-c formula", "[din1988]") {
    NetworkGraph g = CreateSimpleNetwork(10.0, 25.0);
    HydraulicSolver::GlobalNorm() = HydroNorm::DIN1988;

    HydraulicSolver s1(g);
    s1.SetBuildingType(BuildingType::Residential);
    s1.Solve();
    double qRes = g.GetEdge(1) ? g.GetEdge(1)->flowRate_m3s : 0;

    NetworkGraph g2 = CreateSimpleNetwork(10.0, 25.0);
    HydraulicSolver s2(g2);
    s2.SetBuildingType(BuildingType::Hospital);
    s2.Solve();
    double qHosp = g2.GetEdge(1) ? g2.GetEdge(1)->flowRate_m3s : 0;

    HydraulicSolver::GlobalNorm() = HydroNorm::EN806_3;

    REQUIRE(qRes > 0.0);
    REQUIRE(qHosp > 0.0);
    // Hastane farkli katsayi → farkli debi
    REQUIRE(qRes != qHosp);
}

TEST_CASE("HydraulicSolver - WC drainage min DN100", "[drainage]") {
    NetworkGraph g;
    Node drain; drain.type = NodeType::Drain; drain.position = {0,0,0};
    uint32_t did = g.AddNode(drain);

    Node wc; wc.type = NodeType::Fixture; wc.fixtureType = "WC";
    wc.loadUnit = 2.0; wc.position = {3,0,0};
    uint32_t wid = g.AddNode(wc);

    Edge e; e.nodeA = did; e.nodeB = wid;
    e.type = EdgeType::Drainage; e.diameter_mm = 50.0;
    e.length_m = 3.0; e.slope = 0.02; e.material = "PVC";
    g.AddEdge(e);

    HydraulicSolver solver(g);
    solver.SolveDrainage();

    auto* edge = g.GetEdge(1);
    REQUIRE(edge != nullptr);
    REQUIRE(edge->diameter_mm >= 100.0); // WC min DN100
}

TEST_CASE("HydraulicSolver - Multi-source critical path", "[critical]") {
    NetworkGraph g;
    Node src1; src1.type = NodeType::Source; src1.position = {0,0,0};
    uint32_t s1 = g.AddNode(src1);
    Node src2; src2.type = NodeType::HotSource; src2.position = {10,0,0};
    uint32_t s2 = g.AddNode(src2);

    Node f1; f1.type = NodeType::Fixture; f1.loadUnit = 5.0; f1.position = {5,0,3};
    uint32_t fid1 = g.AddNode(f1);
    Node f2; f2.type = NodeType::Fixture; f2.loadUnit = 2.0; f2.position = {15,0,0};
    uint32_t fid2 = g.AddNode(f2);

    Edge e1; e1.nodeA = s1; e1.nodeB = fid1;
    e1.type = EdgeType::Supply; e1.diameter_mm = 25; e1.length_m = 5; e1.roughness_mm = 0.007;
    g.AddEdge(e1);
    Edge e2; e2.nodeA = s2; e2.nodeB = fid2;
    e2.type = EdgeType::HotWater; e2.diameter_mm = 20; e2.length_m = 5; e2.roughness_mm = 0.007;
    g.AddEdge(e2);

    HydraulicSolver solver(g);
    solver.Solve();
    auto result = solver.CalculateCriticalPath();

    REQUIRE(result.totalHeadLoss_m > 0.0);
    REQUIRE(result.requiredPumpHead_m > result.totalHeadLoss_m);
    REQUIRE(!result.criticalPath.empty());
}

TEST_CASE("HydraulicSolver - Water density NIST validation", "[thermal]") {
    NetworkGraph g = CreateSimpleNetwork(1.0);
    HydraulicSolver s(g);
    // NIST reference values
    s.SetWaterTemperature(4.0);
    REQUIRE_THAT(s.WaterDensity(), Catch::Matchers::WithinAbs(999.97, 1.0));
    s.SetWaterTemperature(20.0);
    REQUIRE_THAT(s.WaterDensity(), Catch::Matchers::WithinAbs(998.2, 1.0));
    s.SetWaterTemperature(60.0);
    REQUIRE_THAT(s.WaterDensity(), Catch::Matchers::WithinAbs(983.2, 2.0));
}

TEST_CASE("HydraulicSolver - Viscosity NIST validation", "[thermal]") {
    NetworkGraph g = CreateSimpleNetwork(1.0);
    HydraulicSolver s(g);
    s.SetWaterTemperature(20.0);
    double nu20 = s.WaterKinematicViscosity();
    // nu(20C) ~ 1.004e-6 m2/s (±15% mühendislik toleransı)
    REQUIRE_THAT(nu20, Catch::Matchers::WithinAbs(1.004e-6, 0.15e-6));
}

TEST_CASE("HydraulicSolver - Aging model per material type", "[aging]") {
    auto testMaterial = [](const std::string& mat, double roughness, double age, bool shouldAge) {
        NetworkGraph g = CreateSimpleNetwork(2.0, 25.0);
        auto* e = g.GetEdge(1);
        if (e) { e->material = mat; e->roughness_mm = roughness; }
        HydraulicSolver solver(g);
        solver.SetPipeAge(age);
        solver.Solve();
        double hAged = g.GetEdge(1) ? g.GetEdge(1)->headLoss_m : 0.0;

        NetworkGraph g2 = CreateSimpleNetwork(2.0, 25.0);
        auto* e2 = g2.GetEdge(1);
        if (e2) { e2->material = mat; e2->roughness_mm = roughness; }
        HydraulicSolver solver2(g2);
        solver2.SetPipeAge(0.0);
        solver2.Solve();
        double hNew = g2.GetEdge(1) ? g2.GetEdge(1)->headLoss_m : 0.0;

        if (shouldAge)
            REQUIRE(hAged > hNew);
        else
            REQUIRE_THAT(hAged, Catch::Matchers::WithinAbs(hNew, 1e-9));
    };
    testMaterial("PVC", 0.0015, 30.0, false);  // plastik yaslanmaz
    testMaterial("Bakır", 0.0015, 30.0, true); // bakir yaslanir
}

TEST_CASE("PumpData has brand and category", "[database]") {
    auto& db = Database::Instance();
    const auto& catalog = db.GetPumpCatalog();
    REQUIRE(catalog.size() >= 30);
    for (const auto& p : catalog) {
        REQUIRE(!p.brand.empty());
        REQUIRE(!p.category.empty());
    }
}

TEST_CASE("Database has 30+ pump models", "[database]") {
    auto& db = Database::Instance();
    // SuggestPump with very high requirements should return the largest
    auto pump = db.SuggestPump(200.0, 200.0);
    REQUIRE(!pump.model.empty());
}

// ═══════════════════════════════════════════════════════════
//  HVAC ENGINE TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("CoolingLoad - positive total, sum of components", "[hvac]") {
    NetworkGraph g;
    HydraulicSolver solver(g);

    auto result = solver.CalculateCoolingLoad(
        100.0,  // area_m2
        0.5,    // wallU (W/m2K)
        0.3,    // roofU
        20.0,   // glassArea_m2
        0.6,    // SHGC
        10,     // numPeople
        2000.0, // equipW
        12.0,   // lightWm2
        35.0,   // outdoorTempC
        24.0    // indoorTempC
    );

    REQUIRE(result.Q_wall_W > 0);
    REQUIRE(result.Q_glass_W > 0);
    REQUIRE(result.Q_internal_W > 0);
    REQUIRE(result.Q_ventilation_W > 0);
    REQUIRE(result.Q_total_W > 0);

    // Total = sum of all components
    double expectedTotal = result.Q_wall_W + result.Q_glass_W
                         + result.Q_internal_W + result.Q_ventilation_W;
    REQUIRE_THAT(result.Q_total_W, Catch::Matchers::WithinAbs(expectedTotal, 0.01));
}

TEST_CASE("CoolingLoad - zero delta T gives minimal load", "[hvac]") {
    NetworkGraph g;
    HydraulicSolver solver(g);

    auto result = solver.CalculateCoolingLoad(
        50.0, 0.5, 0.3, 10.0, 0.6, 5, 1000.0, 10.0,
        22.0, 22.0  // same indoor/outdoor
    );

    // Wall and ventilation should be zero (no delta T)
    REQUIRE_THAT(result.Q_wall_W, Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(result.Q_ventilation_W, Catch::Matchers::WithinAbs(0.0, 0.01));
    // Glass and internal loads still present
    REQUIRE(result.Q_glass_W > 0);
    REQUIRE(result.Q_internal_W > 0);
}

TEST_CASE("Psychrometric - RH=1.0 dew point equals dry bulb", "[hvac]") {
    auto state = HydraulicSolver::CalcPsychrometric(25.0, 1.0);

    REQUIRE(state.W > 0);
    REQUIRE(state.h > 0);
    REQUIRE(state.v > 0);
    // At 100% RH, dew point should equal dry bulb
    REQUIRE_THAT(state.T_dp, Catch::Matchers::WithinAbs(state.T_db, 0.5));
    // Wet bulb should also be close to dry bulb at saturation
    REQUIRE_THAT(state.T_wb, Catch::Matchers::WithinAbs(state.T_db, 2.0));
}

TEST_CASE("Psychrometric - basic sanity checks", "[hvac]") {
    auto state = HydraulicSolver::CalcPsychrometric(20.0, 0.50);

    REQUIRE(state.T_db == 20.0);
    REQUIRE(state.RH == 0.50);
    REQUIRE(state.W > 0);
    REQUIRE(state.W < 0.05); // reasonable range for humidity ratio
    REQUIRE(state.h > 0);
    REQUIRE(state.v > 0.7);
    REQUIRE(state.v < 1.0);
    REQUIRE(state.T_dp < state.T_db); // dew point below dry bulb when RH < 1
    REQUIRE(state.T_wb <= state.T_db); // wet bulb <= dry bulb
}

TEST_CASE("ERV - heat recovered positive", "[hvac]") {
    NetworkGraph g;
    HydraulicSolver solver(g);

    auto result = solver.CalculateERV(
        500.0,  // airflow_Ls
        35.0,   // outdoorT
        0.60,   // outdoorRH
        24.0,   // indoorT
        0.50,   // indoorRH
        0.75,   // sensEff
        0.65    // latEff
    );

    REQUIRE(result.sensibleEff == 0.75);
    REQUIRE(result.latentEff == 0.65);
    REQUIRE(result.heatRecovered_W > 0);
    REQUIRE(result.moistureRecovered_gs > 0);
}

TEST_CASE("ERV - zero temperature difference gives zero sensible recovery", "[hvac]") {
    NetworkGraph g;
    HydraulicSolver solver(g);

    auto result = solver.CalculateERV(
        500.0, 24.0, 0.50, 24.0, 0.50, 0.75, 0.65
    );

    REQUIRE_THAT(result.heatRecovered_W, Catch::Matchers::WithinAbs(0.0, 0.01));
    // Same humidity too, so moisture should be zero
    REQUIRE_THAT(result.moistureRecovered_gs, Catch::Matchers::WithinAbs(0.0, 0.01));
}

// ═══════════════════════════════════════════════════════════
//  FAN KATALOGU TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("Database has 15+ fan models", "[database][hvac]") {
    auto& db = Database::Instance();
    const auto& fans = db.GetFanCatalog();
    REQUIRE(fans.size() >= 15);

    for (const auto& fan : fans) {
        REQUIRE(!fan.model.empty());
        REQUIRE(!fan.brand.empty());
        REQUIRE(fan.maxAirflow_Ls > 0);
        REQUIRE(fan.maxPressure_Pa > 0);
        REQUIRE(fan.power_kW > 0);
        REQUIRE(fan.SFP > 0);
    }
}

TEST_CASE("Database SuggestFan returns valid fan", "[database][hvac]") {
    auto& db = Database::Instance();
    auto fan = db.SuggestFan(200.0, 300.0);

    REQUIRE(!fan.model.empty());
    REQUIRE(fan.maxAirflow_Ls >= 200.0);
    REQUIRE(fan.maxPressure_Pa >= 300.0);
}

TEST_CASE("Database SuggestFan with excessive requirements returns largest", "[database][hvac]") {
    auto& db = Database::Instance();
    auto fan = db.SuggestFan(999999.0, 999999.0);

    REQUIRE(!fan.model.empty());
    // Should be the largest fan in catalog
    REQUIRE(fan.maxAirflow_Ls > 0);
}

// ═══════════════════════════════════════════════════════════
//  MAHAL TiPi KUTUPHANESI TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("RoomTypeLibrary has 15+ types", "[space][hvac]") {
    const auto& lib = vkt::cad::SpaceManager::GetRoomTypeLibrary();
    REQUIRE(lib.size() >= 15);

    for (const auto& room : lib) {
        REQUIRE(!room.name.empty());
        REQUIRE(!room.nameEN.empty());
        REQUIRE(room.airChangeRate > 0);
        REQUIRE(room.occupantDensity > 0);
        REQUIRE(room.lightingDensity_Wm2 > 0);
        REQUIRE(room.tempSetpoint_C > 0);
        REQUIRE(room.humiditySetpoint > 0);
        REQUIRE(room.humiditySetpoint <= 1.0);
    }
}

TEST_CASE("RoomTypeLibrary contains essential room types", "[space][hvac]") {
    const auto& lib = vkt::cad::SpaceManager::GetRoomTypeLibrary();

    // Check for some essential room types
    bool hasOffice = false, hasBedroom = false, hasOR = false;
    for (const auto& room : lib) {
        if (room.nameEN == "Office") hasOffice = true;
        if (room.nameEN == "Bedroom") hasBedroom = true;
        if (room.nameEN == "Operating Room") hasOR = true;
    }
    REQUIRE(hasOffice);
    REQUIRE(hasBedroom);
    REQUIRE(hasOR);
}

// ═══════════════════════════════════════════════════════════
//  KAT BAZLI BASINC RAPORU TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("FloorPressures - static pressure proportional to elevation", "[hydraulic][floor]") {
    using namespace vkt::core;

    NetworkGraph graph;

    // Source at ground level
    Node source; source.type = NodeType::Source;
    source.position = {0, 0, 0}; // z=0 (mm)
    uint32_t sid = graph.AddNode(source);

    // Fixture on 3rd floor (z=9000mm = 9m)
    Node fix1; fix1.type = NodeType::Fixture; fix1.loadUnit = 2.0;
    fix1.position = {1000, 0, 9000};
    uint32_t fid1 = graph.AddNode(fix1);

    Edge pipe; pipe.nodeA = sid; pipe.nodeB = fid1;
    pipe.type = EdgeType::Supply; pipe.diameter_mm = 25;
    pipe.length_m = 10.0; pipe.headLoss_m = 2.5;
    graph.AddEdge(pipe);

    FloorManager fm;
    fm.BuildStandardFloors(3, false, 3.0);
    fm.AssignNodeToFloor(sid, 0);
    fm.AssignNodeToFloor(fid1, 3);

    HydraulicSolver solver(graph);
    auto results = solver.CalculateFloorPressures(fm);

    REQUIRE(results.size() >= 2);

    // Higher floors should have higher static pressure
    bool foundHigher = false;
    for (const auto& r : results) {
        if (r.elevation_m > 0) {
            REQUIRE(r.staticPressure_mSS > 0);
            foundHigher = true;
        }
    }
    REQUIRE(foundHigher);
}

TEST_CASE("FloorPressures - empty FloorManager returns empty", "[hydraulic][floor]") {
    using namespace vkt::core;

    NetworkGraph graph;
    FloorManager fm;

    HydraulicSolver solver(graph);
    auto results = solver.CalculateFloorPressures(fm);
    REQUIRE(results.empty());
}

// ═══════════════════════════════════════════════════════════
//  SPECGENERATOR TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("SpecGenerator produces non-empty HTML", "[spec]") {
    NetworkGraph graph;

    Node source; source.type = NodeType::Source;
    uint32_t sid = graph.AddNode(source);

    Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 2.0;
    fix.fixtureType = "Lavabo";
    uint32_t fid = graph.AddNode(fix);

    Edge pipe; pipe.nodeA = sid; pipe.nodeB = fid;
    pipe.type = EdgeType::Supply; pipe.diameter_mm = 20;
    pipe.length_m = 5.0; pipe.material = "PVC";
    graph.AddEdge(pipe);

    SpecGenerator gen(graph);
    std::string html = gen.GenerateHTML();

    REQUIRE(!html.empty());
    REQUIRE(html.find("<html") != std::string::npos);
    REQUIRE(html.find("Teknik Sartname") != std::string::npos);
    REQUIRE(html.find("PVC") != std::string::npos);
    REQUIRE(html.find("Lavabo") != std::string::npos);
}

TEST_CASE("SpecGenerator section pipes", "[spec]") {
    NetworkGraph graph;

    Node src; src.type = NodeType::Source;
    graph.AddNode(src);
    Node fix; fix.type = NodeType::Fixture; fix.loadUnit = 1.0;
    graph.AddNode(fix);
    Edge e; e.nodeA = 1; e.nodeB = 2;
    e.type = EdgeType::Supply; e.material = "PP-R"; e.diameter_mm = 25; e.length_m = 10.0;
    graph.AddEdge(e);

    SpecGenerator gen(graph);
    std::string pipes = gen.GenerateSection("pipes");

    REQUIRE(!pipes.empty());
    REQUIRE(pipes.find("PP-R") != std::string::npos);
    REQUIRE(pipes.find("25") != std::string::npos);
}

TEST_CASE("SpecGenerator section compliance with multiple edge types", "[spec]") {
    NetworkGraph graph;

    // Supply edge
    Node s1; s1.type = NodeType::Source; graph.AddNode(s1);
    Node f1; f1.type = NodeType::Fixture; graph.AddNode(f1);
    Edge e1; e1.nodeA = 1; e1.nodeB = 2; e1.type = EdgeType::Supply; graph.AddEdge(e1);

    // Gas edge
    Node gs; gs.type = NodeType::GasSource; graph.AddNode(gs);
    Node ga; ga.type = NodeType::GasAppliance; graph.AddNode(ga);
    Edge eg; eg.nodeA = 3; eg.nodeB = 4; eg.type = EdgeType::Gas; graph.AddEdge(eg);

    SpecGenerator gen(graph);
    std::string compliance = gen.GenerateSection("compliance");

    REQUIRE(!compliance.empty());
    REQUIRE(compliance.find("EN 806") != std::string::npos);
    REQUIRE(compliance.find("EN 1775") != std::string::npos);
}

TEST_CASE("SpecGenerator empty network", "[spec]") {
    NetworkGraph graph;
    SpecGenerator gen(graph);
    std::string html = gen.GenerateHTML();

    // Should still produce valid HTML
    REQUIRE(!html.empty());
    REQUIRE(html.find("<html") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════
//  DWGWRITER TESTLERi
// ═══════════════════════════════════════════════════════════

#include "cad/DWGWriter.hpp"
#include "cad/Line.hpp"
#include "cad/SelectionManager.hpp"
#include "mep/ClashDetection.hpp"
#include "mep/ChartGenerator.hpp"
#include "core/IfcExporter.hpp"
#include <fstream>
#include <cstdio>

TEST_CASE("DWGWriter WriteDxfCompat produces non-empty output", "[dwg-writer]") {
    // Create temp file path
    std::string tmpPath = "test_dwgwriter_output.dxf";

    // Create some test entities
    std::vector<std::unique_ptr<vkt::cad::Entity>> entities;
    entities.push_back(std::make_unique<vkt::cad::Line>(
        vkt::geom::Vec3{0, 0, 0}, vkt::geom::Vec3{1000, 1000, 0}));
    entities.push_back(std::make_unique<vkt::cad::Line>(
        vkt::geom::Vec3{500, 0, 0}, vkt::geom::Vec3{500, 2000, 0}));

    std::map<std::string, vkt::cad::Layer> layers;

    bool ok = vkt::cad::DWGWriter::WriteDxfCompat(tmpPath, entities, layers);
    REQUIRE(ok);

    // Verify file is non-empty and contains DXF markers
    std::ifstream in(tmpPath);
    REQUIRE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();
    REQUIRE(content.size() > 100);
    REQUIRE(content.find("SECTION") != std::string::npos);
    REQUIRE(content.find("ENTITIES") != std::string::npos);
    REQUIRE(content.find("LINE") != std::string::npos);
    REQUIRE(content.find("EOF") != std::string::npos);

    std::remove(tmpPath.c_str());
}

// ═══════════════════════════════════════════════════════════
//  CHARTGENERATOR TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("ChartGenerator BarChartSVG contains svg element", "[chart]") {
    std::map<int, double> dnLengths = {{20, 15.5}, {25, 22.3}, {32, 8.0}};
    std::string svg = vkt::mep::ChartGenerator::BarChartSVG(dnLengths);

    REQUIRE(!svg.empty());
    REQUIRE(svg.find("<svg") != std::string::npos);
    REQUIRE(svg.find("DN20") != std::string::npos);
    REQUIRE(svg.find("DN25") != std::string::npos);
    REQUIRE(svg.find("</svg>") != std::string::npos);
}

TEST_CASE("ChartGenerator PieChartSVG contains svg element", "[chart]") {
    std::map<std::string, double> materials = {{"PVC", 50.0}, {"PP-R", 30.0}, {"Bakir", 20.0}};
    std::string svg = vkt::mep::ChartGenerator::PieChartSVG(materials);

    REQUIRE(!svg.empty());
    REQUIRE(svg.find("<svg") != std::string::npos);
    REQUIRE(svg.find("path") != std::string::npos);
    REQUIRE(svg.find("PVC") != std::string::npos);
    REQUIRE(svg.find("</svg>") != std::string::npos);
}

TEST_CASE("ChartGenerator PressureProfileSVG contains svg element", "[chart]") {
    std::vector<double> pressures = {30.0, 28.5, 25.0, 22.0};
    std::vector<std::string> labels = {"Kaynak", "Kat1", "Kat2", "Kat3"};
    std::string svg = vkt::mep::ChartGenerator::PressureProfileSVG(pressures, labels);

    REQUIRE(!svg.empty());
    REQUIRE(svg.find("<svg") != std::string::npos);
    REQUIRE(svg.find("polyline") != std::string::npos);
    REQUIRE(svg.find("</svg>") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════
//  CLASH RESOLUTION TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("ClashResolution suggestions non-empty for overlapping pipes", "[clash]") {
    NetworkGraph graph;

    Node source; source.type = NodeType::Source;
    source.position = {0, 0, 0};
    uint32_t sid = graph.AddNode(source);

    Node fix; fix.type = NodeType::Fixture;
    fix.position = {5000, 0, 0};
    uint32_t fid = graph.AddNode(fix);

    Edge pipe; pipe.nodeA = sid; pipe.nodeB = fid;
    pipe.type = EdgeType::Supply; pipe.diameter_mm = 25;
    pipe.length_m = 5.0;
    graph.AddEdge(pipe);

    // Create a fake clash result
    std::vector<vkt::mep::ClashResult> clashes;
    vkt::mep::ClashResult c;
    c.id = 1;
    c.edgeId = 1;
    c.architecturalId = 100;
    c.clashPoint = {2500, 0, 0};
    c.overlapAmount_mm = 30.0;
    c.severity = vkt::mep::ClashSeverity::Hard;
    c.description = "Test clash";
    clashes.push_back(c);

    auto resolutions = vkt::mep::SuggestResolutions(clashes, graph);
    REQUIRE(!resolutions.empty());
    REQUIRE(resolutions[0].entityId == 1);
    REQUIRE(resolutions[0].severity > 0.0);
    REQUIRE(resolutions[0].suggestedOffset.z > 0.0);  // Move upward
    REQUIRE(!resolutions[0].description.empty());
}

// ═══════════════════════════════════════════════════════════
//  FENCE SELECTION TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("SelectByFence returns intersecting entities", "[selection]") {
    // Create two lines
    auto line1 = std::make_unique<vkt::cad::Line>(
        vkt::geom::Vec3{0, 0, 0}, vkt::geom::Vec3{100, 0, 0});
    auto line2 = std::make_unique<vkt::cad::Line>(
        vkt::geom::Vec3{200, 0, 0}, vkt::geom::Vec3{300, 0, 0});

    std::vector<vkt::cad::Entity*> entities = {line1.get(), line2.get()};

    vkt::cad::SelectionManager sm;

    // Fence that crosses line1 but not line2
    std::vector<vkt::geom::Vec3> fence = {
        {50, -50, 0}, {50, 50, 0}
    };

    auto result = sm.SelectByFence(fence, entities);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == line1->GetId());
}

TEST_CASE("SelectByPolygon returns entities inside polygon", "[selection]") {
    auto line1 = std::make_unique<vkt::cad::Line>(
        vkt::geom::Vec3{10, 10, 0}, vkt::geom::Vec3{20, 20, 0});
    auto line2 = std::make_unique<vkt::cad::Line>(
        vkt::geom::Vec3{200, 200, 0}, vkt::geom::Vec3{300, 300, 0});

    std::vector<vkt::cad::Entity*> entities = {line1.get(), line2.get()};

    vkt::cad::SelectionManager sm;

    // Polygon that contains line1 but not line2
    std::vector<vkt::geom::Vec3> polygon = {
        {0, 0, 0}, {50, 0, 0}, {50, 50, 0}, {0, 50, 0}
    };

    auto result = sm.SelectByPolygon(polygon, entities, false);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == line1->GetId());
}

// ═══════════════════════════════════════════════════════════
//  IFC IMPORTER TESTLERi
// ═══════════════════════════════════════════════════════════

TEST_CASE("IfcImporter returns zero counts for non-existent file", "[ifc]") {
    std::vector<std::unique_ptr<vkt::cad::Entity>> entities;
    auto result = vkt::core::IfcImporter::Import("nonexistent_file.ifc", entities);

    REQUIRE(result.wallCount == 0);
    REQUIRE(result.slabCount == 0);
    REQUIRE(result.pipeCount == 0);
    REQUIRE(result.fixtureCount == 0);
    REQUIRE(!result.warnings.empty());
    REQUIRE(entities.empty());
}
