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
