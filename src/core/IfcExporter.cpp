/**
 * @file IfcExporter.cpp
 * @brief IFC2x3 STEP Physical File export — MEP boru + armatür
 */

#include "core/IfcExporter.hpp"
#include <fstream>
#include <iomanip>
#include <ctime>
#include <cmath>

namespace vkt {
namespace core {

IfcExporter::IfcExporter(const mep::NetworkGraph& network, const FloorManager& floors)
    : m_network(network), m_floors(floors) {}

std::string IfcExporter::FormatFloat(double v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4) << v;
    return ss.str();
}

std::string IfcExporter::Timestamp() {
    auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
    return buf;
}

std::string IfcExporter::EdgeTypeToIfcClass(mep::EdgeType type) {
    switch (type) {
        case mep::EdgeType::Supply:
        case mep::EdgeType::HotWater:   return "IfcPipeSegment";
        case mep::EdgeType::Drainage:   return "IfcPipeSegment";
        case mep::EdgeType::Gas:        return "IfcPipeSegment";
        case mep::EdgeType::Heating:
        case mep::EdgeType::HeatingReturn: return "IfcPipeSegment";
        case mep::EdgeType::FireLine:   return "IfcPipeSegment";
        case mep::EdgeType::Electric:   return "IfcCableSegment";
        case mep::EdgeType::Duct:       return "IfcDuctSegment";
        default:                        return "IfcPipeSegment";
    }
}

std::string IfcExporter::NodeTypeToIfcClass(mep::NodeType type) {
    switch (type) {
        case mep::NodeType::Fixture:     return "IfcSanitaryTerminal";
        case mep::NodeType::Source:
        case mep::NodeType::HotSource:   return "IfcFlowTerminal";
        case mep::NodeType::Pump:
        case mep::NodeType::FirePump:    return "IfcPump";
        case mep::NodeType::Drain:       return "IfcWasteTerminal";
        case mep::NodeType::Boiler:      return "IfcBoiler";
        case mep::NodeType::Radiator:    return "IfcSpaceHeater";
        case mep::NodeType::Sprinkler:   return "IfcFireSuppressionTerminal";
        case mep::NodeType::GasAppliance:return "IfcBurner";
        case mep::NodeType::ElecPanel:   return "IfcElectricDistributionBoard";
        case mep::NodeType::Socket:      return "IfcOutlet";
        case mep::NodeType::LightFixture:return "IfcLightFixture";
        case mep::NodeType::AHU:         return "IfcAirTerminalBox";
        case mep::NodeType::Diffuser:    return "IfcAirTerminal";
        default:                         return "IfcFlowTerminal";
    }
}

void IfcExporter::WriteHeader(std::ostream& out, const std::string& filePath,
                               const std::string& author) const {
    out << "ISO-10303-21;\n"
        << "HEADER;\n"
        << "FILE_DESCRIPTION(('ViewDefinition [CoordinationView]'),'2;1');\n"
        << "FILE_NAME('" << filePath << "','" << Timestamp() << "',('"
        << (author.empty() ? "VKT User" : author)
        << "'),('VKT Mekanik Tesisat Draw'),'VKT IFC Exporter 1.0','VKT','');\n"
        << "FILE_SCHEMA(('IFC2X3'));\n"
        << "ENDSEC;\n\n"
        << "DATA;\n";
}

int IfcExporter::WriteProject(std::ostream& out, const std::string& projectName) const {
    int ownerHistory = NextId();
    out << "#" << ownerHistory << "=IFCOWNERHISTORY($,$,$,.ADDED.,$,$,$,0);\n";

    int unitMM = NextId();
    out << "#" << unitMM << "=IFCSIUNIT(*,.LENGTHUNIT.,.MILLI.,.METRE.);\n";

    int unitAssign = NextId();
    out << "#" << unitAssign << "=IFCUNITASSIGNMENT((#" << unitMM << "));\n";

    int ctx = NextId();
    out << "#" << ctx << "=IFCGEOMETRICREPRESENTATIONCONTEXT($,'Model',3,1.E-05,#"
        << NextId() << ",$);\n";
    int axis = m_nextId - 1;
    out << "#" << axis << "=IFCAXIS2PLACEMENT3D(#" << NextId() << ",$,$);\n";
    int origin = m_nextId - 1;
    out << "#" << origin << "=IFCCARTESIANPOINT((0.,0.,0.));\n";

    int projectId = NextId();
    out << "#" << projectId << "=IFCPROJECT('"
        << "VKT" << std::to_string(projectId)
        << "',#" << ownerHistory << ",'" << projectName
        << "',$,$,$,$,(#" << ctx << "),#" << unitAssign << ");\n";

    return projectId;
}

int IfcExporter::WriteSite(std::ostream& out, int projectId) const {
    int siteId = NextId();
    out << "#" << siteId << "=IFCSITE('"
        << "Site" << std::to_string(siteId)
        << "',#1,'Arsa',$,$,$,$,$,.ELEMENT.,$,$,$,$,$);\n";
    int relId = NextId();
    out << "#" << relId << "=IFCRELAGGREGATES('"
        << "Rel" << std::to_string(relId)
        << "',#1,$,$,#" << projectId << ",(#" << siteId << "));\n";
    return siteId;
}

int IfcExporter::WriteBuilding(std::ostream& out, int siteId) const {
    int buildingId = NextId();
    out << "#" << buildingId << "=IFCBUILDING('"
        << "Bld" << std::to_string(buildingId)
        << "',#1,'Bina',$,$,$,$,$,.ELEMENT.,$,$,$);\n";
    int relId = NextId();
    out << "#" << relId << "=IFCRELAGGREGATES('"
        << "Rel" << std::to_string(relId)
        << "',#1,$,$,#" << siteId << ",(#" << buildingId << "));\n";
    return buildingId;
}

int IfcExporter::WriteBuildingStorey(std::ostream& out, int buildingId,
                                      const std::string& name, double elevation) const {
    int placePt = NextId();
    out << "#" << placePt << "=IFCCARTESIANPOINT(("
        << FormatFloat(0) << "," << FormatFloat(0) << "," << FormatFloat(elevation) << "));\n";
    int axis = NextId();
    out << "#" << axis << "=IFCAXIS2PLACEMENT3D(#" << placePt << ",$,$);\n";
    int placement = NextId();
    out << "#" << placement << "=IFCLOCALPLACEMENT($,#" << axis << ");\n";

    int storeyId = NextId();
    out << "#" << storeyId << "=IFCBUILDINGSTOREY('"
        << "Storey" << std::to_string(storeyId)
        << "',#1,'" << name << "',$,$,#" << placement << ",$,$,.ELEMENT.,"
        << FormatFloat(elevation) << ");\n";
    return storeyId;
}

int IfcExporter::WritePipeSegment(std::ostream& out, int storeyId,
                                   const mep::Edge& edge,
                                   const mep::Node& nodeA,
                                   const mep::Node& nodeB) const {
    // Geometry: IfcPolyline between two endpoints
    int ptA = NextId();
    out << "#" << ptA << "=IFCCARTESIANPOINT(("
        << FormatFloat(nodeA.position.x) << ","
        << FormatFloat(nodeA.position.y) << ","
        << FormatFloat(nodeA.position.z * 1000.0) << "));\n";

    int ptB = NextId();
    out << "#" << ptB << "=IFCCARTESIANPOINT(("
        << FormatFloat(nodeB.position.x) << ","
        << FormatFloat(nodeB.position.y) << ","
        << FormatFloat(nodeB.position.z * 1000.0) << "));\n";

    int polyline = NextId();
    out << "#" << polyline << "=IFCPOLYLINE((#" << ptA << ",#" << ptB << "));\n";

    int shapeRep = NextId();
    out << "#" << shapeRep << "=IFCSHAPEREPRESENTATION($,'Axis','Curve3D',(#" << polyline << "));\n";

    int prodShape = NextId();
    out << "#" << prodShape << "=IFCPRODUCTDEFINITIONSHAPE($,$,(#" << shapeRep << "));\n";

    std::string ifcClass = EdgeTypeToIfcClass(edge.type);
    int pipeId = NextId();
    out << "#" << pipeId << "=" << ifcClass << "('"
        << "Pipe" << std::to_string(pipeId)
        << "',#1,'DN" << static_cast<int>(edge.diameter_mm)
        << "',$,$,$,#" << prodShape << ",$,$);\n";

    // Contain in storey
    int relId = NextId();
    out << "#" << relId << "=IFCRELCONTAINEDINSPATIALSTRUCTURE('"
        << "Rel" << std::to_string(relId)
        << "',#1,$,$,(#" << pipeId << "),#" << storeyId << ");\n";

    return pipeId;
}

int IfcExporter::WriteFlowTerminal(std::ostream& out, int storeyId,
                                    const mep::Node& node) const {
    int pt = NextId();
    out << "#" << pt << "=IFCCARTESIANPOINT(("
        << FormatFloat(node.position.x) << ","
        << FormatFloat(node.position.y) << ","
        << FormatFloat(node.position.z * 1000.0) << "));\n";

    int axis = NextId();
    out << "#" << axis << "=IFCAXIS2PLACEMENT3D(#" << pt << ",$,$);\n";
    int placement = NextId();
    out << "#" << placement << "=IFCLOCALPLACEMENT($,#" << axis << ");\n";

    std::string ifcClass = NodeTypeToIfcClass(node.type);
    std::string label = node.label.empty() ? node.fixtureType : node.label;

    int termId = NextId();
    out << "#" << termId << "=" << ifcClass << "('"
        << "Term" << std::to_string(termId)
        << "',#1,'" << label << "',$,$,#" << placement << ",$,$,$);\n";

    int relId = NextId();
    out << "#" << relId << "=IFCRELCONTAINEDINSPATIALSTRUCTURE('"
        << "Rel" << std::to_string(relId)
        << "',#1,$,$,(#" << termId << "),#" << storeyId << ");\n";

    return termId;
}

bool IfcExporter::Export(const std::string& filePath,
                          const std::string& projectName,
                          const std::string& author) const {
    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    m_nextId = 1;

    WriteHeader(out, filePath, author);
    int projectId  = WriteProject(out, projectName);
    int siteId     = WriteSite(out, projectId);
    int buildingId = WriteBuilding(out, siteId);

    // Kat -> storeyId map
    std::unordered_map<int, int> storeyMap;
    int floorCount = m_floors.GetFloorCount();
    if (floorCount > 0) {
        for (int fi = 0; fi < floorCount; ++fi) {
            const Floor* fl = m_floors.GetFloor(fi);
            std::string name = fl ? fl->label : ("Kat " + std::to_string(fi));
            double elev = fl ? fl->elevation_m * 1000.0 : fi * 3000.0;
            storeyMap[fi] = WriteBuildingStorey(out, buildingId, name, elev);
        }
    } else {
        storeyMap[0] = WriteBuildingStorey(out, buildingId, "Zemin", 0.0);
    }

    // Aggregate storeys into building
    {
        std::string ids;
        for (auto& [fi, sid] : storeyMap) {
            if (!ids.empty()) ids += ",";
            ids += "#" + std::to_string(sid);
        }
        int relId = NextId();
        out << "#" << relId << "=IFCRELAGGREGATES('"
            << "Rel" << std::to_string(relId)
            << "',#1,$,$,#" << buildingId << ",(" << ids << "));\n";
    }

    // Edge'leri IFC'ye yaz
    for (const auto& edge : m_network.GetEdges()) {
        const mep::Node* nA = m_network.GetNode(edge.nodeA);
        const mep::Node* nB = m_network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        // Z'den kat tahmini
        double z = (nA->position.z + nB->position.z) * 0.5;
        int fi = static_cast<int>(std::round(z / 3.0));
        int storeyId = storeyMap.count(fi) ? storeyMap[fi]
                     : storeyMap.begin()->second;

        WritePipeSegment(out, storeyId, edge, *nA, *nB);
    }

    // Node'ları IFC'ye yaz (Source, Fixture, Drain, vb.)
    for (const auto& [nid, node] : m_network.GetNodeMap()) {
        if (node.type == mep::NodeType::Junction) continue; // Junction skip

        double z = node.position.z;
        int fi = static_cast<int>(std::round(z / 3.0));
        int storeyId = storeyMap.count(fi) ? storeyMap[fi]
                     : storeyMap.begin()->second;

        WriteFlowTerminal(out, storeyId, node);
    }

    out << "ENDSEC;\n"
        << "END-ISO-10303-21;\n";

    return true;
}

} // namespace core
} // namespace vkt
