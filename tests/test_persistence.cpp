/**
 * @file test_persistence.cpp
 * @brief Document Save/Load round-trip testleri
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Document.hpp"
#include "core/Commands.hpp"
#include "mep/NetworkGraph.hpp"
#include "mep/HydraulicSolver.hpp"
#include "cad/Line.hpp"
#include <filesystem>
#include <string>

using namespace vkt;
using namespace vkt::core;
using namespace vkt::mep;
using Catch::Matchers::WithinAbs;

static std::string TempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

// ============================================================
// NETWORK GRAPH ROUND-TRIP
// ============================================================

TEST_CASE("Document Save/Load - empty document", "[persistence]") {
    Document doc;
    std::string path = TempPath("vkt_test_empty.json");

    REQUIRE(doc.Save(path));

    Document doc2;
    REQUIRE(doc2.Load(path));

    REQUIRE(doc2.GetNetwork().GetNodeCount() == 0);
    REQUIRE(doc2.GetNetwork().GetEdgeCount() == 0);

    std::filesystem::remove(path);
}

TEST_CASE("Document Save/Load - nodes and edges preserved", "[persistence]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node n1; n1.type = NodeType::Source;  n1.position = {0, 0, 0}; n1.label = "Kaynak";
    Node n2; n2.type = NodeType::Fixture; n2.position = {5, 3, 0}; n2.label = "Lavabo";
    n2.loadUnit = 1.5;

    uint32_t id1 = net.AddNode(n1);
    uint32_t id2 = net.AddNode(n2);

    Edge edge; edge.nodeA = id1; edge.nodeB = id2;
    edge.diameter_mm = 25.0; edge.length_m = 6.0; edge.material = "PPR";
    uint32_t eid = net.AddEdge(edge);

    std::string path = TempPath("vkt_test_nodes.json");
    REQUIRE(doc.Save(path));

    Document doc2;
    REQUIRE(doc2.Load(path));

    auto& net2 = doc2.GetNetwork();
    REQUIRE(net2.GetNodeCount() == 2);
    REQUIRE(net2.GetEdgeCount() == 1);

    // Verify node data preserved
    const Node* loadedSource = nullptr;
    const Node* loadedFixture = nullptr;
    for (const auto& [id, node] : net2.GetNodeMap()) {
        if (node.type == NodeType::Source)  loadedSource  = &node;
        if (node.type == NodeType::Fixture) loadedFixture = &node;
    }
    REQUIRE(loadedSource  != nullptr);
    REQUIRE(loadedFixture != nullptr);
    REQUIRE_THAT(loadedFixture->loadUnit,    WithinAbs(1.5, 1e-9));
    REQUIRE_THAT(loadedFixture->position.x,  WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(loadedFixture->position.y,  WithinAbs(3.0, 1e-9));

    // Verify edge data preserved
    auto edges = net2.GetEdgeList();
    REQUIRE(edges.size() == 1);
    REQUIRE_THAT(edges[0].diameter_mm, WithinAbs(25.0, 1e-9));
    REQUIRE_THAT(edges[0].length_m,    WithinAbs(6.0,  1e-9));
    REQUIRE(edges[0].material == "PPR");

    std::filesystem::remove(path);
}

TEST_CASE("Document Save/Load - multiple nodes", "[persistence]") {
    Document doc;
    auto& net = doc.GetNetwork();

    for (int i = 0; i < 10; ++i) {
        Node n; n.type = NodeType::Junction;
        n.position = {(double)i, (double)(i*2), 0};
        n.label = "J" + std::to_string(i);
        net.AddNode(n);
    }

    std::string path = TempPath("vkt_test_multi.json");
    REQUIRE(doc.Save(path));

    Document doc2;
    REQUIRE(doc2.Load(path));
    REQUIRE(doc2.GetNetwork().GetNodeCount() == 10);

    std::filesystem::remove(path);
}

// ============================================================
// COMMAND PATTERN — UNDO/REDO
// ============================================================

TEST_CASE("Command AddNode is undoable", "[commands]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node n; n.type = NodeType::Junction; n.position = {1,1,0};
    auto cmd = std::make_unique<AddNodeCommand>(net, n);
    doc.ExecuteCommand(std::move(cmd));

    REQUIRE(net.GetNodeCount() == 1);
    REQUIRE(doc.CanUndo());

    doc.Undo();
    REQUIRE(net.GetNodeCount() == 0);
    REQUIRE_FALSE(doc.CanUndo());
}

TEST_CASE("Command AddEdge is undoable", "[commands]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node n1; n1.type = NodeType::Source;  n1.position = {0,0,0};
    Node n2; n2.type = NodeType::Fixture; n2.position = {3,0,0};
    uint32_t id1 = net.AddNode(n1);
    uint32_t id2 = net.AddNode(n2);

    Edge e; e.nodeA = id1; e.nodeB = id2; e.diameter_mm = 20.0;
    auto cmd = std::make_unique<AddEdgeCommand>(net, e);
    doc.ExecuteCommand(std::move(cmd));

    REQUIRE(net.GetEdgeCount() == 1);
    doc.Undo();
    REQUIRE(net.GetEdgeCount() == 0);
}

TEST_CASE("Command DeleteNode removes connected edges and is undoable", "[commands]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node n1; n1.type = NodeType::Junction; n1.position = {0,0,0};
    Node n2; n2.type = NodeType::Junction; n2.position = {1,0,0};
    Node n3; n3.type = NodeType::Junction; n3.position = {2,0,0};
    uint32_t id1 = net.AddNode(n1);
    uint32_t id2 = net.AddNode(n2);
    uint32_t id3 = net.AddNode(n3);

    Edge e1; e1.nodeA = id1; e1.nodeB = id2;
    Edge e2; e2.nodeA = id2; e2.nodeB = id3;
    net.AddEdge(e1);
    net.AddEdge(e2);

    // Delete center node — both edges should go with it
    auto cmd = std::make_unique<DeleteNodeCommand>(net, id2);
    doc.ExecuteCommand(std::move(cmd));

    REQUIRE(net.GetNodeCount() == 2);
    REQUIRE(net.GetEdgeCount() == 0);

    // Undo restores node + edges
    doc.Undo();
    REQUIRE(net.GetNodeCount() == 3);
    REQUIRE(net.GetEdgeCount() == 2);
}

TEST_CASE("Redo re-executes undone command", "[commands]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node n; n.type = NodeType::Junction; n.position = {0,0,0};
    doc.ExecuteCommand(std::make_unique<AddNodeCommand>(net, n));

    REQUIRE(net.GetNodeCount() == 1);
    doc.Undo();
    REQUIRE(net.GetNodeCount() == 0);
    REQUIRE(doc.CanRedo());
    doc.Redo();
    REQUIRE(net.GetNodeCount() == 1);
}

TEST_CASE("New command after undo clears redo stack", "[commands]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node n1; n1.type = NodeType::Junction; n1.position = {0,0,0};
    Node n2; n2.type = NodeType::Junction; n2.position = {1,0,0};

    doc.ExecuteCommand(std::make_unique<AddNodeCommand>(net, n1));
    doc.ExecuteCommand(std::make_unique<AddNodeCommand>(net, n2));
    REQUIRE(net.GetNodeCount() == 2);

    doc.Undo(); // undo n2 add
    REQUIRE(doc.CanRedo());

    // New command clears redo history
    Node n3; n3.type = NodeType::Junction; n3.position = {5,0,0};
    doc.ExecuteCommand(std::make_unique<AddNodeCommand>(net, n3));
    REQUIRE_FALSE(doc.CanRedo());
    REQUIRE(net.GetNodeCount() == 2);
}

// ============================================================
// ENTEGRASYON TESTLERI - ROUND-TRIP + UNDO + DUCT
// ============================================================

TEST_CASE("Save/Load roundtrip - duct rectangular dimensions", "[persistence][integration]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node src; src.type = NodeType::AHU; src.position = {0,0,0};
    Node diff; diff.type = NodeType::Diffuser; diff.position = {5000,0,0};
    uint32_t nA = net.AddNode(src);
    uint32_t nB = net.AddNode(diff);

    Edge duct;
    duct.type = EdgeType::Duct;
    duct.nodeA = nA; duct.nodeB = nB;
    duct.ductWidth_mm = 500;
    duct.ductHeight_mm = 300;
    duct.material = "Galvaniz Kanal";
    net.AddEdge(duct);

    std::string path = TempPath("vkt_test_duct_roundtrip.json");
    REQUIRE(doc.Save(path));

    Document doc2;
    REQUIRE(doc2.Load(path));
    auto& net2 = doc2.GetNetwork();
    REQUIRE(net2.GetEdgeCount() == 1);

    auto edges = net2.GetEdgeList();
    REQUIRE(edges[0].ductWidth_mm == 500.0);
    REQUIRE(edges[0].ductHeight_mm == 300.0);
    REQUIRE(edges[0].type == EdgeType::Duct);
    REQUIRE(edges[0].material == "Galvaniz Kanal");
    REQUIRE(edges[0].IsRectangularDuct());
    REQUIRE(edges[0].DuctArea_m2() > 0.14); // 0.5 * 0.3 = 0.15

    std::filesystem::remove(path);
}

TEST_CASE("Save/Load roundtrip - node types (HVAC)", "[persistence][integration]") {
    Document doc;
    auto& net = doc.GetNetwork();

    Node plenum; plenum.type = NodeType::Plenum; plenum.position = {100,0,0};
    Node damper; damper.type = NodeType::Damper; damper.position = {200,0,0};
    Node vav;    vav.type = NodeType::VAVBox;    vav.position = {300,0,0};
    net.AddNode(plenum);
    net.AddNode(damper);
    net.AddNode(vav);

    std::string path = TempPath("vkt_test_hvac_nodes.json");
    REQUIRE(doc.Save(path));

    Document doc2;
    REQUIRE(doc2.Load(path));
    auto nodes = doc2.GetNetwork().GetNodeList();
    REQUIRE(nodes.size() == 3);

    bool foundPlenum = false, foundDamper = false, foundVAV = false;
    for (auto& n : nodes) {
        if (n.type == NodeType::Plenum)  foundPlenum = true;
        if (n.type == NodeType::Damper)  foundDamper = true;
        if (n.type == NodeType::VAVBox)  foundVAV = true;
    }
    REQUIRE(foundPlenum);
    REQUIRE(foundDamper);
    REQUIRE(foundVAV);

    std::filesystem::remove(path);
}

TEST_CASE("HydraulicSolver - error reporting on empty network", "[solver][integration]") {
    NetworkGraph net;
    // No source, no fixture
    HydraulicSolver solver(net);
    solver.Solve();

    REQUIRE(solver.HasErrors());
    REQUIRE(solver.GetErrors().size() >= 1);
}

TEST_CASE("HydraulicSolver - warning on isolated nodes", "[solver][integration]") {
    NetworkGraph net;

    Node src; src.type = NodeType::Source; src.position = {0,0,0};
    Node fix; fix.type = NodeType::Fixture; fix.position = {1000,0,0}; fix.loadUnit = 1.0;
    Node iso; iso.type = NodeType::Junction; iso.position = {5000,5000,0};
    uint32_t nA = net.AddNode(src);
    uint32_t nB = net.AddNode(fix);
    net.AddNode(iso); // isolated

    Edge e; e.nodeA = nA; e.nodeB = nB; e.type = EdgeType::Supply;
    net.AddEdge(e);

    HydraulicSolver solver(net);
    solver.Solve();

    REQUIRE(solver.GetWarnings().size() >= 1);
}

TEST_CASE("DeleteCADEntitiesCommand - undo restores entities", "[commands][integration]") {
    Document doc;
    auto line1 = std::make_unique<cad::Line>(geom::Vec3{0,0,0}, geom::Vec3{100,0,0});
    auto line2 = std::make_unique<cad::Line>(geom::Vec3{0,100,0}, geom::Vec3{100,100,0});
    cad::EntityId id1 = line1->GetId();
    cad::EntityId id2 = line2->GetId();
    doc.AddCADEntity(std::move(line1));
    doc.AddCADEntity(std::move(line2));
    REQUIRE(doc.GetCADEntities().size() == 2);

    // Delete entity 1
    auto& entities = doc.GetCADEntitiesMutable();
    std::vector<cad::EntityId> delIds = {id1};
    auto cmd = std::make_unique<DeleteCADEntitiesCommand>(entities, delIds);
    std::vector<std::unique_ptr<cad::Entity>> removed;
    for (auto it = entities.begin(); it != entities.end(); ++it) {
        if (*it && (*it)->GetId() == id1) {
            removed.push_back(std::move(*it));
            entities.erase(it);
            break;
        }
    }
    cmd->StashRemoved(std::move(removed));
    doc.ExecuteCommand(std::move(cmd));
    REQUIRE(doc.GetCADEntities().size() == 1);

    // Undo restores
    doc.Undo();
    REQUIRE(doc.GetCADEntities().size() == 2);

    // Redo removes again
    doc.Redo();
    REQUIRE(doc.GetCADEntities().size() == 1);
}

TEST_CASE("AddCADEntitiesCommand - undo removes added entities", "[commands][integration]") {
    Document doc;
    auto& entities = doc.GetCADEntitiesMutable();

    auto line = std::make_unique<cad::Line>(geom::Vec3{0,0,0}, geom::Vec3{100,0,0});
    cad::EntityId addedId = line->GetId();
    entities.push_back(std::move(line));

    auto cmd = std::make_unique<AddCADEntitiesCommand>(entities, std::vector<cad::EntityId>{addedId});
    doc.ExecuteCommand(std::move(cmd));
    REQUIRE(doc.GetCADEntities().size() == 1);

    // Undo removes
    doc.Undo();
    REQUIRE(doc.GetCADEntities().size() == 0);

    // Redo restores
    doc.Redo();
    REQUIRE(doc.GetCADEntities().size() == 1);
}

TEST_CASE("Atomic save creates temp then renames", "[persistence][integration]") {
    Document doc;
    auto& net = doc.GetNetwork();
    Node n; n.type = NodeType::Source; n.position = {0,0,0};
    net.AddNode(n);

    std::string path = TempPath("vkt_test_atomic_save.json");
    REQUIRE(doc.Save(path));

    // File should exist
    REQUIRE(std::filesystem::exists(path));
    // Temp should NOT exist
    REQUIRE_FALSE(std::filesystem::exists(path + ".tmp"));
    // Backup should NOT exist (cleaned up)
    REQUIRE_FALSE(std::filesystem::exists(path + ".bak"));

    // File should be valid JSON loadable
    Document doc2;
    REQUIRE(doc2.Load(path));
    REQUIRE(doc2.GetNetwork().GetNodeCount() == 1);

    std::filesystem::remove(path);
}

TEST_CASE("Edge IsRectangularDuct and DuctArea", "[network][integration]") {
    Edge e;
    e.type = EdgeType::Duct;
    e.ductWidth_mm = 400;
    e.ductHeight_mm = 300;

    REQUIRE(e.IsRectangularDuct());
    REQUIRE_THAT(e.DuctArea_m2(), WithinAbs(0.12, 0.001)); // 0.4 * 0.3

    // Circular duct
    Edge e2;
    e2.type = EdgeType::Duct;
    e2.diameter_mm = 250;
    REQUIRE_FALSE(e2.IsRectangularDuct());
    REQUIRE(e2.DuctArea_m2() > 0.04); // pi * 0.125^2 ~ 0.049
}

TEST_CASE("NetworkGraph AddEdge sets Duct defaults", "[network][integration]") {
    NetworkGraph net;
    Node n1; n1.position = {0,0,0};
    Node n2; n2.position = {1000,0,0};
    uint32_t a = net.AddNode(n1);
    uint32_t b = net.AddNode(n2);

    Edge e;
    e.nodeA = a; e.nodeB = b;
    e.type = EdgeType::Duct;
    uint32_t eid = net.AddEdge(e);

    const Edge* added = net.GetEdge(eid);
    REQUIRE(added != nullptr);
    REQUIRE(added->ductWidth_mm == 400.0);
    REQUIRE(added->ductHeight_mm == 300.0);
    REQUIRE(added->material == "Galvaniz Kanal");
}
