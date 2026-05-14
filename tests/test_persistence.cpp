/**
 * @file test_persistence.cpp
 * @brief Document Save/Load round-trip testleri
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Document.hpp"
#include "core/Commands.hpp"
#include "mep/NetworkGraph.hpp"
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
