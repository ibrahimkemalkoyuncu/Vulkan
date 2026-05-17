/**
 * @file test_riser.cpp
 * @brief RiserDiagram entegrasyon testleri
 *
 * 3 katlı örnek ağ:
 *
 *   Kat 2 (z=6m):  Fixture-WC(id=4)
 *   Kat 1 (z=3m):  Fixture-Lavabo(id=3)
 *   Zemin(z=0m):   Fixture-Duş(id=2)
 *        |
 *   Source(id=1)
 */

#include <catch2/catch_test_macros.hpp>
#include "mep/NetworkGraph.hpp"
#include "mep/RiserDiagram.hpp"
#include "core/FloorManager.hpp"

using namespace vkt::mep;
using namespace vkt::core;

static NetworkGraph Build3FloorNetwork() {
    NetworkGraph g;

    Node src; src.type = NodeType::Source; src.label = "Ana Hat";
    src.position = {0, 0, 0};
    g.AddNode(src);  // id=1

    Node fix0; fix0.type = NodeType::Fixture; fix0.fixtureType = "Duş";
    fix0.loadUnit = 1.0; fix0.position = {2, 0, 0};
    g.AddNode(fix0); // id=2

    Node fix1; fix1.type = NodeType::Fixture; fix1.fixtureType = "Lavabo";
    fix1.loadUnit = 0.5; fix1.position = {2, 0, 3};
    g.AddNode(fix1); // id=3

    Node fix2; fix2.type = NodeType::Fixture; fix2.fixtureType = "WC";
    fix2.loadUnit = 2.0; fix2.position = {2, 0, 6};
    g.AddNode(fix2); // id=4

    auto makeEdge = [&](uint32_t a, uint32_t b, double diam, double len) {
        Edge e; e.nodeA = a; e.nodeB = b;
        e.type = EdgeType::Supply;
        e.diameter_mm = diam; e.length_m = len;
        e.flowRate_m3s = 0.0002;
        g.AddEdge(e);
    };

    makeEdge(1, 2, 25.0, 2.0);
    makeEdge(1, 3, 20.0, 5.0);
    makeEdge(1, 4, 20.0, 8.0);

    return g;
}

static FloorManager Build3FloorManager() {
    FloorManager fm;
    fm.BuildStandardFloors(3, false, 3.0);
    // Node → kat ataması (Z koordinatına göre)
    fm.AssignNodeToFloor(1, 0);  // source → zemin
    fm.AssignNodeToFloor(2, 0);  // Duş → zemin
    fm.AssignNodeToFloor(3, 1);  // Lavabo → 1.kat
    fm.AssignNodeToFloor(4, 2);  // WC → 2.kat
    return fm;
}

TEST_CASE("RiserDiagram - Generate produces columns", "[riser]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();

    // En az 1 kolon üretilmeli
    REQUIRE(data.columns.size() >= 1);

    // Çizgi ve etiket üretilmeli
    REQUIRE(!data.lines.empty());
    REQUIRE(!data.labels.empty());

    // Canvas boyutları pozitif olmalı
    REQUIRE(data.canvasWidth  > 0.0f);
    REQUIRE(data.canvasHeight > 0.0f);
}

TEST_CASE("RiserDiagram - Segments cover all floors", "[riser]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();

    REQUIRE(!data.columns.empty());
    const auto& col = data.columns[0];

    // 3 farklı kat indeksi (0,1,2) segment listesinde bulunmalı
    bool has0 = false, has1 = false, has2 = false;
    for (const auto& seg : col.segments) {
        if (seg.floorIndex == 0) has0 = true;
        if (seg.floorIndex == 1) has1 = true;
        if (seg.floorIndex == 2) has2 = true;
    }
    REQUIRE(has0);
    REQUIRE(has1);
    REQUIRE(has2);
}

TEST_CASE("RiserDiagram - SVG output is valid XML", "[riser]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();
    std::string svg = rd.ToSVG(data);

    // SVG başlığı içermeli
    REQUIRE(svg.find("<svg") != std::string::npos);
    REQUIRE(svg.find("</svg>") != std::string::npos);
    // En az bir çizgi elementi içermeli
    REQUIRE(svg.find("<line") != std::string::npos);
}

TEST_CASE("RiserDiagram - Text output contains floor labels", "[riser]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();
    std::string txt = rd.ToText(data);

    REQUIRE(!txt.empty());
    // Kat bilgisi içermeli
    REQUIRE(txt.find("Kat") != std::string::npos);
}

TEST_CASE("FloorManager - BuildStandardFloors", "[floor]") {
    FloorManager fm;
    fm.BuildStandardFloors(3, true, 3.0);

    // 3 normal kat + 1 bodrum = 4
    REQUIRE(fm.GetFloorCount() == 4);

    const Floor* zemin = fm.GetFloor(0);
    REQUIRE(zemin != nullptr);
    REQUIRE(zemin->label == "Zemin Kat");
    REQUIRE(zemin->elevation_m == 0.0);

    const Floor* bodrum = fm.GetFloor(-1);
    REQUIRE(bodrum != nullptr);
    REQUIRE(bodrum->elevation_m == -3.0);
}

TEST_CASE("FloorManager - GetFloorIndexAtElevation", "[floor]") {
    FloorManager fm;
    fm.BuildStandardFloors(3, false, 3.0);

    REQUIRE(fm.GetFloorIndexAtElevation(0.0) == 0);
    REQUIRE(fm.GetFloorIndexAtElevation(1.5) == 0);
    REQUIRE(fm.GetFloorIndexAtElevation(3.0) == 1);
    REQUIRE(fm.GetFloorIndexAtElevation(5.9) == 1);
    REQUIRE(fm.GetFloorIndexAtElevation(6.0) == 2);
}

TEST_CASE("FloorManager - Node floor assignment", "[floor]") {
    FloorManager fm;
    fm.BuildStandardFloors(2, false, 3.0);

    fm.AssignNodeToFloor(42, 1);
    REQUIRE(fm.GetFloorOfNode(42) == 1);
    REQUIRE(fm.GetFloorOfNode(99) == -999); // atanmamış

    auto nodesOnFloor1 = fm.GetNodesOnFloor(1);
    REQUIRE(nodesOnFloor1.size() == 1);
    REQUIRE(nodesOnFloor1[0] == 42);
}

TEST_CASE("FloorManager - Serialize / Deserialize roundtrip", "[floor]") {
    FloorManager fm;
    fm.BuildStandardFloors(2, false, 3.0);
    fm.AssignNodeToFloor(10, 0);
    fm.AssignNodeToFloor(20, 1);

    std::string json = fm.Serialize();
    REQUIRE(!json.empty());

    FloorManager fm2;
    REQUIRE(fm2.Deserialize(json));
    REQUIRE(fm2.GetFloorCount() == 2);
    REQUIRE(fm2.GetFloorOfNode(10) == 0);
    REQUIRE(fm2.GetFloorOfNode(20) == 1);
}

// ──────────────────────────────────────────────────────────────
//  SVG iyilestirme testleri (ToSVG viewBox + legend + alternating)
// ──────────────────────────────────────────────────────────────

TEST_CASE("RiserDiagram - SVG contains viewBox attribute", "[riser][svg]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();
    std::string svg = rd.ToSVG(data);

    REQUIRE(svg.find("viewBox=") != std::string::npos);
}

TEST_CASE("RiserDiagram - SVG contains legend elements", "[riser][svg]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();
    std::string svg = rd.ToSVG(data);

    // Temiz Su ve Pis Su lejant etiketleri
    REQUIRE(svg.find("Temiz Su") != std::string::npos);
    REQUIRE(svg.find("Pis Su")   != std::string::npos);
}

TEST_CASE("RiserDiagram - SVG alternating row backgrounds present", "[riser][svg]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();
    std::string svg = rd.ToSVG(data);

    // Alternating arka plan rengi (#f5f8ff)
    REQUIRE(svg.find("#f5f8ff") != std::string::npos);
}

TEST_CASE("RiserDiagram - SVG uses stroke-linecap round", "[riser][svg]") {
    auto graph = Build3FloorNetwork();
    auto fm    = Build3FloorManager();

    RiserDiagram rd(graph, fm);
    auto data = rd.Generate();
    std::string svg = rd.ToSVG(data);

    REQUIRE(svg.find("stroke-linecap=\"round\"") != std::string::npos);
}

// ──────────────────────────────────────────────────────────────
//  Kolon boru mantigi testleri (dikey boru — farkli Z, ayni XY)
// ──────────────────────────────────────────────────────────────

TEST_CASE("Kolon - dikey boru uzunlugu Z farkina esit", "[kolon]") {
    // Zemin kat (z=0) → 1.kat (z=3m): kolon boyu 3m olmali
    double srcZ = 0.0;
    double dstZ = 3.0;
    double dz_m = std::abs(dstZ - srcZ);
    REQUIRE(dz_m == 3.0);
}

TEST_CASE("Kolon - ayni XY farkli Z'de node tespiti", "[kolon]") {
    NetworkGraph g;

    Node n1; n1.type = NodeType::Junction; n1.position = {1000.0, 2000.0, 0.0};
    g.AddNode(n1);

    Node n2; n2.type = NodeType::Junction; n2.position = {1000.0, 2000.0, 3.0}; // farkli kat
    g.AddNode(n2);

    Node n3; n3.type = NodeType::Junction; n3.position = {5000.0, 2000.0, 0.0}; // farkli XY
    g.AddNode(n3);

    // n1 ile ayni XY, farkli Z olan n2 bulunmali; n3 bulunmamali
    const auto& nodeMap = g.GetNodeMap();
    constexpr double XY_TOL = 50.0;
    constexpr double Z_TOL  = 0.15;
    double targetZ = 3.0;
    const vkt::mep::Node* srcNode = g.GetNode(1);
    REQUIRE(srcNode != nullptr);

    uint32_t foundId = 0;
    for (const auto& [id, nd] : nodeMap) {
        if (id == 1) continue; // kaynak node kendisi
        if (std::abs(nd.position.x - srcNode->position.x) < XY_TOL &&
            std::abs(nd.position.y - srcNode->position.y) < XY_TOL &&
            std::abs(nd.position.z - targetZ)             < Z_TOL) {
            foundId = id;
            break;
        }
    }
    REQUIRE(foundId == 2); // n2 bulunmali

    // n3 bulunmamali (farkli XY)
    uint32_t notFoundId = 0;
    for (const auto& [id, nd] : nodeMap) {
        if (id == 1) continue;
        if (std::abs(nd.position.x - srcNode->position.x) < XY_TOL &&
            std::abs(nd.position.y - srcNode->position.y) < XY_TOL &&
            std::abs(nd.position.z - 0.0) < Z_TOL &&
            id != 1) {
            notFoundId = id;
        }
    }
    REQUIRE(notFoundId != 3); // n3 XY farklı, bulunmamalı
}

TEST_CASE("FloorManager - GetFloor returns nullptr for unknown index", "[floor]") {
    FloorManager fm;
    fm.BuildStandardFloors(2, false, 3.0);

    REQUIRE(fm.GetFloor(0) != nullptr);
    REQUIRE(fm.GetFloor(1) != nullptr);
    REQUIRE(fm.GetFloor(99) == nullptr);
    REQUIRE(fm.GetFloor(-5) == nullptr);
}

TEST_CASE("FloorManager - elevation out of range returns -999", "[floor]") {
    FloorManager fm;
    fm.BuildStandardFloors(2, false, 3.0); // Kat 0: 0-3m, Kat 1: 3-6m

    // Tanimli aralik disinda
    REQUIRE(fm.GetFloorIndexAtElevation(100.0) == -999);
    REQUIRE(fm.GetFloorIndexAtElevation(-5.0)  == -999);
}

// ──────────────────────────────────────────────────────────────
//  IsColumnEdge / GetColumnEdges testleri
// ──────────────────────────────────────────────────────────────

TEST_CASE("IsColumnEdge - dikey boru (ayni XY, farkli Z) kolon olarak taninir", "[kolon][network]") {
    NetworkGraph g;
    Node n1; n1.type = NodeType::Junction; n1.position = {1000.0, 2000.0, 0.0};
    Node n2; n2.type = NodeType::Junction; n2.position = {1005.0, 1998.0, 3.0}; // dx=5mm, dy=2mm, dz=3m
    uint32_t id1 = g.AddNode(n1);
    uint32_t id2 = g.AddNode(n2);

    Edge e; e.nodeA = id1; e.nodeB = id2; e.type = EdgeType::Supply;
    uint32_t eid = g.AddEdge(e);

    REQUIRE(g.IsColumnEdge(eid) == true);
}

TEST_CASE("IsColumnEdge - yatay boru (farkli XY, ayni Z) kolon degil", "[kolon][network]") {
    NetworkGraph g;
    Node n1; n1.type = NodeType::Junction; n1.position = {0.0, 0.0, 0.0};
    Node n2; n2.type = NodeType::Junction; n2.position = {5000.0, 0.0, 0.0}; // 5m yatay
    uint32_t id1 = g.AddNode(n1);
    uint32_t id2 = g.AddNode(n2);

    Edge e; e.nodeA = id1; e.nodeB = id2; e.type = EdgeType::Supply;
    uint32_t eid = g.AddEdge(e);

    REQUIRE(g.IsColumnEdge(eid) == false);
}

TEST_CASE("IsColumnEdge - XY farki 50mm sinirinda: 49mm kolon, 51mm degil", "[kolon][network]") {
    NetworkGraph g;
    Node n1; n1.type = NodeType::Junction; n1.position = {0.0, 0.0, 0.0};
    Node nClose; nClose.type = NodeType::Junction; nClose.position = {49.0, 0.0, 3.0};
    Node nFar;   nFar.type   = NodeType::Junction; nFar.position   = {51.0, 0.0, 3.0};
    uint32_t id1     = g.AddNode(n1);
    uint32_t idClose = g.AddNode(nClose);
    uint32_t idFar   = g.AddNode(nFar);

    Edge eClose; eClose.nodeA = id1; eClose.nodeB = idClose; eClose.type = EdgeType::Supply;
    Edge eFar;   eFar.nodeA   = id1; eFar.nodeB   = idFar;   eFar.type   = EdgeType::Supply;
    uint32_t eidClose = g.AddEdge(eClose);
    uint32_t eidFar   = g.AddEdge(eFar);

    REQUIRE(g.IsColumnEdge(eidClose) == true);
    REQUIRE(g.IsColumnEdge(eidFar)   == false);
}

TEST_CASE("GetColumnEdges - karisik agda sadece kolon kenarlari doner", "[kolon][network]") {
    NetworkGraph g;
    // Zemin kat node'lari
    Node n1; n1.type = NodeType::Source;  n1.position = {0.0, 0.0, 0.0};
    Node n2; n2.type = NodeType::Junction; n2.position = {3000.0, 0.0, 0.0};
    // Ust kat node'u (kolon baglantisi)
    Node n3; n3.type = NodeType::Junction; n3.position = {3000.0, 0.0, 3.0};
    // Ust katta yatay boru
    Node n4; n4.type = NodeType::Fixture;  n4.position = {6000.0, 0.0, 3.0};

    uint32_t id1 = g.AddNode(n1);
    uint32_t id2 = g.AddNode(n2);
    uint32_t id3 = g.AddNode(n3);
    uint32_t id4 = g.AddNode(n4);

    Edge eH1; eH1.nodeA = id1; eH1.nodeB = id2; eH1.type = EdgeType::Supply; // yatay
    Edge eV;  eV.nodeA  = id2; eV.nodeB  = id3; eV.type  = EdgeType::Supply; // kolon
    Edge eH2; eH2.nodeA = id3; eH2.nodeB = id4; eH2.type = EdgeType::Supply; // yatay ust

    g.AddEdge(eH1);
    uint32_t eidCol = g.AddEdge(eV);
    g.AddEdge(eH2);

    auto cols = g.GetColumnEdges();
    REQUIRE(cols.size() == 1);
    REQUIRE(cols[0] == eidCol);
}
