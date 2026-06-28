/**
 * @file IfcExporter.hpp
 * @brief IFC2x3 STEP Physical File (SPF) export — MEP boru ve armatür verileri
 *
 * Minimal IFC export: IfcPipeSegment, IfcFlowTerminal, IfcProject/IfcSite/IfcBuilding
 * hiyerarşisi. BIM yazılımlarıyla (Revit, ArchiCAD, BIMcollab) interoperabilite sağlar.
 */

#pragma once

#include "mep/NetworkGraph.hpp"
#include "core/FloorManager.hpp"
#include <string>
#include <vector>
#include <sstream>

namespace vkt {
namespace core {

class IfcExporter {
public:
    IfcExporter(const mep::NetworkGraph& network, const FloorManager& floors);

    bool Export(const std::string& filePath,
                const std::string& projectName = "VKT Projesi",
                const std::string& author = "") const;

private:
    void WriteHeader(std::ostream& out, const std::string& filePath,
                     const std::string& author) const;
    int  WriteProject(std::ostream& out, const std::string& projectName) const;
    int  WriteSite(std::ostream& out, int projectId) const;
    int  WriteBuilding(std::ostream& out, int siteId) const;
    int  WriteBuildingStorey(std::ostream& out, int buildingId,
                             const std::string& name, double elevation) const;
    int  WritePipeSegment(std::ostream& out, int storeyId,
                          const mep::Edge& edge,
                          const mep::Node& nodeA,
                          const mep::Node& nodeB) const;
    int  WriteFlowTerminal(std::ostream& out, int storeyId,
                           const mep::Node& node) const;

    const mep::NetworkGraph& m_network;
    const FloorManager&      m_floors;
    mutable int m_nextId = 1;
    int NextId() const { return m_nextId++; }

    static std::string EdgeTypeToIfcClass(mep::EdgeType type);
    static std::string NodeTypeToIfcClass(mep::NodeType type);
    static std::string FormatFloat(double v);
    static std::string Timestamp();
};

} // namespace core
} // namespace vkt
