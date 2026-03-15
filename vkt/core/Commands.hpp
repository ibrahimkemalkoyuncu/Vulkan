/**
 * @file Commands.hpp
 * @brief Concrete Command siniflar - Undo/Redo destekli islemler
 */

#pragma once

#include "core/Command.hpp"
#include "mep/NetworkGraph.hpp"
#include <vector>
#include <memory>

namespace vkt {
namespace core {

/// Node ekleme komutu
class AddNodeCommand : public Command {
public:
    AddNodeCommand(mep::NetworkGraph& network, const mep::Node& node)
        : m_network(network), m_node(node), m_nodeId(0) {}

    void Execute() override { m_nodeId = m_network.AddNode(m_node); }
    void Undo() override { m_network.RemoveNode(m_nodeId); }

    uint32_t GetNodeId() const { return m_nodeId; }

private:
    mep::NetworkGraph& m_network;
    mep::Node m_node;
    uint32_t m_nodeId;
};

/// Edge ekleme komutu
class AddEdgeCommand : public Command {
public:
    AddEdgeCommand(mep::NetworkGraph& network, const mep::Edge& edge)
        : m_network(network), m_edge(edge), m_edgeId(0) {}

    void Execute() override { m_edgeId = m_network.AddEdge(m_edge); }
    void Undo() override { m_network.RemoveEdge(m_edgeId); }

    uint32_t GetEdgeId() const { return m_edgeId; }

private:
    mep::NetworkGraph& m_network;
    mep::Edge m_edge;
    uint32_t m_edgeId;
};

/// Node silme komutu (undo = yeniden ekle)
class DeleteNodeCommand : public Command {
public:
    DeleteNodeCommand(mep::NetworkGraph& network, uint32_t nodeId)
        : m_network(network), m_nodeId(nodeId) {}

    void Execute() override {
        // Node'u ve bagli edge'leri kaydet
        auto* node = m_network.GetNode(m_nodeId);
        if (node) m_savedNode = *node;

        auto edgeIds = m_network.GetConnectedEdges(m_nodeId);
        m_savedEdges.clear();
        for (uint32_t eid : edgeIds) {
            auto* edge = m_network.GetEdge(eid);
            if (edge) m_savedEdges.push_back(*edge);
        }

        m_network.RemoveNode(m_nodeId);
    }

    void Undo() override {
        // Node'u geri ekle
        m_network.AddNode(m_savedNode);
        // Bagli edge'leri geri ekle
        for (const auto& edge : m_savedEdges) {
            m_network.AddEdge(edge);
        }
    }

private:
    mep::NetworkGraph& m_network;
    uint32_t m_nodeId;
    mep::Node m_savedNode;
    std::vector<mep::Edge> m_savedEdges;
};

/// Edge silme komutu
class DeleteEdgeCommand : public Command {
public:
    DeleteEdgeCommand(mep::NetworkGraph& network, uint32_t edgeId)
        : m_network(network), m_edgeId(edgeId) {}

    void Execute() override {
        auto* edge = m_network.GetEdge(m_edgeId);
        if (edge) m_savedEdge = *edge;
        m_network.RemoveEdge(m_edgeId);
    }

    void Undo() override {
        m_network.AddEdge(m_savedEdge);
    }

private:
    mep::NetworkGraph& m_network;
    uint32_t m_edgeId;
    mep::Edge m_savedEdge;
};

/// Node taşıma komutu
class MoveNodeCommand : public Command {
public:
    MoveNodeCommand(mep::NetworkGraph& network, uint32_t nodeId, const geom::Vec3& newPos)
        : m_network(network), m_nodeId(nodeId), m_newPos(newPos) {}

    void Execute() override {
        auto* node = m_network.GetNode(m_nodeId);
        if (node) {
            m_oldPos = node->position;
            node->position = m_newPos;
        }
    }

    void Undo() override {
        auto* node = m_network.GetNode(m_nodeId);
        if (node) {
            node->position = m_oldPos;
        }
    }

private:
    mep::NetworkGraph& m_network;
    uint32_t m_nodeId;
    geom::Vec3 m_newPos;
    geom::Vec3 m_oldPos;
};

/// Edge ozellik degistirme komutu
class ModifyEdgeCommand : public Command {
public:
    ModifyEdgeCommand(mep::NetworkGraph& network, uint32_t edgeId, const mep::Edge& newProps)
        : m_network(network), m_edgeId(edgeId), m_newProps(newProps) {}

    void Execute() override {
        auto* edge = m_network.GetEdge(m_edgeId);
        if (edge) {
            m_oldProps = *edge;
            edge->diameter_mm = m_newProps.diameter_mm;
            edge->material = m_newProps.material;
            edge->zeta = m_newProps.zeta;
            edge->slope = m_newProps.slope;
        }
    }

    void Undo() override {
        auto* edge = m_network.GetEdge(m_edgeId);
        if (edge) {
            edge->diameter_mm = m_oldProps.diameter_mm;
            edge->material = m_oldProps.material;
            edge->zeta = m_oldProps.zeta;
            edge->slope = m_oldProps.slope;
        }
    }

private:
    mep::NetworkGraph& m_network;
    uint32_t m_edgeId;
    mep::Edge m_newProps;
    mep::Edge m_oldProps;
};

/// Birden fazla komutu tek adimda calistiran bileşik komut
class CompositeCommand : public Command {
public:
    void AddCommand(std::unique_ptr<Command> cmd) {
        m_commands.push_back(std::move(cmd));
    }

    void Execute() override {
        for (auto& cmd : m_commands) cmd->Execute();
    }

    void Undo() override {
        // Ters sirada geri al
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            (*it)->Undo();
        }
    }

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

} // namespace core
} // namespace vkt
