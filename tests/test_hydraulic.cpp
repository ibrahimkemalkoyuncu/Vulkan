/**
 * @file test_hydraulic.cpp
 * @brief HydraulicSolver birim testleri
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "mep/HydraulicSolver.hpp"

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
    std::vector<double> stdDiams = {40, 50, 75, 110, 125, 160, 200, 250, 315};
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
    REQUIRE_THAT(result.requiredPumpHead_m, Catch::Matchers::WithinAbs(20.0, 0.01));
}
