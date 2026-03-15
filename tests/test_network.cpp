/**
 * @file test_network.cpp
 * @brief NetworkGraph birim testleri
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "mep/NetworkGraph.hpp"

using namespace vkt::mep;

TEST_CASE("NetworkGraph - Node Operations", "[network]") {
    NetworkGraph graph;

    SECTION("AddNode returns unique IDs") {
        Node n1; n1.type = NodeType::Junction;
        Node n2; n2.type = NodeType::Fixture;

        uint32_t id1 = graph.AddNode(n1);
        uint32_t id2 = graph.AddNode(n2);

        REQUIRE(id1 != id2);
        REQUIRE(id1 > 0);
        REQUIRE(id2 > 0);
    }

    SECTION("GetNode returns correct node") {
        Node n; n.type = NodeType::Source;
        n.label = "TestSource";
        n.position = {10.0, 20.0, 0.0};

        uint32_t id = graph.AddNode(n);
        auto* retrieved = graph.GetNode(id);

        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->type == NodeType::Source);
        REQUIRE(retrieved->label == "TestSource");
        REQUIRE_THAT(retrieved->position.x, Catch::Matchers::WithinAbs(10.0, 1e-9));
    }

    SECTION("GetNode returns nullptr for invalid ID") {
        REQUIRE(graph.GetNode(9999) == nullptr);
    }

    SECTION("RemoveNode removes the node") {
        Node n;
        uint32_t id = graph.AddNode(n);
        REQUIRE(graph.GetNode(id) != nullptr);

        graph.RemoveNode(id);
        REQUIRE(graph.GetNode(id) == nullptr);
    }

    SECTION("RemoveNode also removes connected edges") {
        Node n1, n2;
        uint32_t nid1 = graph.AddNode(n1);
        uint32_t nid2 = graph.AddNode(n2);

        Edge e; e.nodeA = nid1; e.nodeB = nid2;
        uint32_t eid = graph.AddEdge(e);

        graph.RemoveNode(nid1);
        REQUIRE(graph.GetEdge(eid) == nullptr);
    }

    SECTION("GetNodeCount reflects additions and removals") {
        REQUIRE(graph.GetNodeCount() == 0);

        Node n;
        uint32_t id1 = graph.AddNode(n);
        uint32_t id2 = graph.AddNode(n);
        REQUIRE(graph.GetNodeCount() == 2);

        graph.RemoveNode(id1);
        REQUIRE(graph.GetNodeCount() == 1);
    }
}

TEST_CASE("NetworkGraph - Edge Operations", "[network]") {
    NetworkGraph graph;
    Node n1, n2, n3;
    uint32_t nid1 = graph.AddNode(n1);
    uint32_t nid2 = graph.AddNode(n2);
    uint32_t nid3 = graph.AddNode(n3);

    SECTION("AddEdge returns unique IDs") {
        Edge e1; e1.nodeA = nid1; e1.nodeB = nid2;
        Edge e2; e2.nodeA = nid2; e2.nodeB = nid3;

        uint32_t eid1 = graph.AddEdge(e1);
        uint32_t eid2 = graph.AddEdge(e2);

        REQUIRE(eid1 != eid2);
    }

    SECTION("GetEdge returns correct edge") {
        Edge e; e.nodeA = nid1; e.nodeB = nid2;
        e.diameter_mm = 32.0;
        e.material = "PPR";

        uint32_t eid = graph.AddEdge(e);
        auto* retrieved = graph.GetEdge(eid);

        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->nodeA == nid1);
        REQUIRE(retrieved->nodeB == nid2);
        REQUIRE_THAT(retrieved->diameter_mm, Catch::Matchers::WithinAbs(32.0, 1e-9));
        REQUIRE(retrieved->material == "PPR");
    }

    SECTION("RemoveEdge removes the edge") {
        Edge e; e.nodeA = nid1; e.nodeB = nid2;
        uint32_t eid = graph.AddEdge(e);

        graph.RemoveEdge(eid);
        REQUIRE(graph.GetEdge(eid) == nullptr);
    }
}

TEST_CASE("NetworkGraph - Topology Queries", "[network]") {
    NetworkGraph graph;

    // Source → Junction → Fixture1
    //                   → Fixture2
    Node source; source.type = NodeType::Source;
    Node junc; junc.type = NodeType::Junction;
    Node fix1; fix1.type = NodeType::Fixture;
    Node fix2; fix2.type = NodeType::Fixture;

    uint32_t sid = graph.AddNode(source);
    uint32_t jid = graph.AddNode(junc);
    uint32_t fid1 = graph.AddNode(fix1);
    uint32_t fid2 = graph.AddNode(fix2);

    Edge e1; e1.nodeA = sid; e1.nodeB = jid;
    Edge e2; e2.nodeA = jid; e2.nodeB = fid1;
    Edge e3; e3.nodeA = jid; e3.nodeB = fid2;

    graph.AddEdge(e1);
    graph.AddEdge(e2);
    graph.AddEdge(e3);

    SECTION("GetConnectedEdges returns all edges for a node") {
        auto edges = graph.GetConnectedEdges(jid);
        REQUIRE(edges.size() == 3); // e1, e2, e3 all connect to junction
    }

    SECTION("GetDownstreamNodes returns correct nodes") {
        auto downstream = graph.GetDownstreamNodes(jid);
        REQUIRE(downstream.size() == 2);
    }

    SECTION("GetUpstreamNodes returns correct nodes") {
        auto upstream = graph.GetUpstreamNodes(jid);
        REQUIRE(upstream.size() == 1);
        REQUIRE(upstream[0] == sid);
    }
}

TEST_CASE("NetworkGraph - Clear", "[network]") {
    NetworkGraph graph;
    Node n; Edge e;
    uint32_t nid1 = graph.AddNode(n);
    uint32_t nid2 = graph.AddNode(n);
    e.nodeA = nid1; e.nodeB = nid2;
    graph.AddEdge(e);

    graph.Clear();

    REQUIRE(graph.GetNodeCount() == 0);
    REQUIRE(graph.GetEdgeCount() == 0);
    REQUIRE(graph.GetNode(nid1) == nullptr);
}
