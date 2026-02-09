#include "cad/Polyline.hpp"
#include "cad/Line.hpp"
#include "cad/Layer.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>

namespace vkt::cad {

Polyline::Polyline() : Entity(), m_closed(false) {}

Polyline::Polyline(const std::vector<geom::Vec3>& points, bool closed)
    : Entity(), m_closed(closed) {
    for (const auto& p : points) {
        m_vertices.push_back({p, 0.0, 0.0});
    }
}

Polyline::Polyline(const std::vector<Vertex>& vertices, bool closed)
    : Entity(), m_vertices(vertices), m_closed(closed) {}

std::unique_ptr<Entity> Polyline::Clone() const {
    auto clone = std::make_unique<Polyline>(m_vertices, m_closed);
    clone->SetLayer(const_cast<Layer*>(GetLayer()));
    clone->SetColor(GetColor());
    clone->SetVisible(IsVisible());
    clone->SetLocked(IsLocked());
    return clone;
}

BoundingBox Polyline::GetBounds() const {
    if (m_vertices.empty()) {
        return BoundingBox();
    }
    
    BoundingBox bounds;
    bounds.min = m_vertices[0].pos;
    bounds.max = m_vertices[0].pos;
    
    for (const auto& v : m_vertices) {
        bounds.Expand(v.pos);
    }
    
    return bounds;
}

void Polyline::Serialize(nlohmann::json& j) const {
    j["type"] = "Polyline";
    j["closed"] = m_closed;
    
    nlohmann::json vertices = nlohmann::json::array();
    for (const auto& v : m_vertices) {
        vertices.push_back({
            {"x", v.pos.x}, {"y", v.pos.y}, {"z", v.pos.z},
            {"bulge", v.bulge}, {"width", v.width}
        });
    }
    j["vertices"] = vertices;
}

void Polyline::Deserialize(const nlohmann::json& j) {
    m_closed = j.value("closed", false);
    
    m_vertices.clear();
    if (j.contains("vertices")) {
        for (const auto& vj : j["vertices"]) {
            Vertex v;
            v.pos.x = vj.value("x", 0.0);
            v.pos.y = vj.value("y", 0.0);
            v.pos.z = vj.value("z", 0.0);
            v.bulge = vj.value("bulge", 0.0);
            v.width = vj.value("width", 0.0);
            m_vertices.push_back(v);
        }
    }
}

void Polyline::AddVertex(const geom::Vec3& pos, double bulge, double width) {
    m_vertices.push_back({pos, bulge, width});
}

void Polyline::AddVertex(const Vertex& vertex) {
    m_vertices.push_back(vertex);
}

void Polyline::InsertVertex(size_t index, const Vertex& vertex) {
    if (index <= m_vertices.size()) {
        m_vertices.insert(m_vertices.begin() + index, vertex);
    }
}

void Polyline::RemoveVertex(size_t index) {
    if (index < m_vertices.size()) {
        m_vertices.erase(m_vertices.begin() + index);
    }
}

void Polyline::ClearVertices() {
    m_vertices.clear();
}

const Polyline::Vertex& Polyline::GetVertex(size_t index) const {
    return m_vertices.at(index);
}

Polyline::Vertex& Polyline::GetVertex(size_t index) {
    return m_vertices.at(index);
}

void Polyline::SetVertex(size_t index, const Vertex& vertex) {
    if (index < m_vertices.size()) {
        m_vertices[index] = vertex;
    }
}

void Polyline::SetVertexPosition(size_t index, const geom::Vec3& pos) {
    if (index < m_vertices.size()) {
        m_vertices[index].pos = pos;
    }
}

void Polyline::SetVertexBulge(size_t index, double bulge) {
    if (index < m_vertices.size()) {
        m_vertices[index].bulge = bulge;
    }
}

double Polyline::GetLength() const {
    if (m_vertices.size() < 2) return 0.0;
    
    double length = 0.0;
    size_t count = m_closed ? m_vertices.size() : m_vertices.size() - 1;
    
    for (size_t i = 0; i < count; ++i) {
        size_t next = (i + 1) % m_vertices.size();
        length += GetSegmentLength(m_vertices[i], m_vertices[next]);
    }
    
    return length;
}

double Polyline::GetSegmentLength(const Vertex& v1, const Vertex& v2) const {
    if (std::abs(v1.bulge) < 1e-10) {
        // Straight line
        return v1.pos.DistanceTo(v2.pos);
    }
    
    // Arc segment - approximate
    double chord = v1.pos.DistanceTo(v2.pos);
    double angle = 4.0 * std::atan(std::abs(v1.bulge));
    double radius = chord / (2.0 * std::sin(angle / 2.0));
    return radius * angle;
}

geom::Vec3 Polyline::GetPointAt(double t) const {
    if (m_vertices.empty()) return geom::Vec3();
    if (m_vertices.size() == 1) return m_vertices[0].pos;
    
    double totalLength = GetLength();
    double targetDist = t * totalLength;
    
    return GetPointAtDistance(targetDist);
}

geom::Vec3 Polyline::GetPointAtDistance(double distance) const {
    if (m_vertices.size() < 2) return m_vertices.empty() ? geom::Vec3() : m_vertices[0].pos;
    
    auto segInfo = BuildSegmentInfo();
    
    for (const auto& seg : segInfo) {
        if (distance <= seg.accumulatedLength) {
            double localT = (distance - (seg.accumulatedLength - seg.length)) / seg.length;
            size_t next = (seg.index + 1) % m_vertices.size();
            return EvaluateBulgeSegment(m_vertices[seg.index], m_vertices[next], localT);
        }
    }
    
    return m_vertices.back().pos;
}

std::vector<Polyline::SegmentInfo> Polyline::BuildSegmentInfo() const {
    std::vector<SegmentInfo> info;
    double accum = 0.0;
    
    size_t count = m_closed ? m_vertices.size() : m_vertices.size() - 1;
    for (size_t i = 0; i < count; ++i) {
        size_t next = (i + 1) % m_vertices.size();
        double len = GetSegmentLength(m_vertices[i], m_vertices[next]);
        accum += len;
        info.push_back({i, len, accum});
    }
    
    return info;
}

geom::Vec3 Polyline::EvaluateBulgeSegment(const Vertex& v1, const Vertex& v2, double t) const {
    if (std::abs(v1.bulge) < 1e-10) {
        // Linear interpolation
        return geom::Vec3(
            v1.pos.x + t * (v2.pos.x - v1.pos.x),
            v1.pos.y + t * (v2.pos.y - v1.pos.y),
            v1.pos.z + t * (v2.pos.z - v1.pos.z)
        );
    }
    
    // Arc interpolation (simplified)
    double angle = 4.0 * std::atan(v1.bulge) * t;
    geom::Vec3 mid = geom::Vec3(
        (v1.pos.x + v2.pos.x) * 0.5,
        (v1.pos.y + v2.pos.y) * 0.5,
        (v1.pos.z + v2.pos.z) * 0.5
    );
    
    // Simplified arc calculation
    return geom::Vec3(
        v1.pos.x + t * (v2.pos.x - v1.pos.x),
        v1.pos.y + t * (v2.pos.y - v1.pos.y),
        v1.pos.z + t * (v2.pos.z - v1.pos.z)
    );
}

geom::Vec3 Polyline::GetClosestPoint(const geom::Vec3& point) const {
    if (m_vertices.empty()) return geom::Vec3();
    if (m_vertices.size() == 1) return m_vertices[0].pos;
    
    geom::Vec3 closest = m_vertices[0].pos;
    double minDist = point.DistanceTo(closest);
    
    size_t count = m_closed ? m_vertices.size() : m_vertices.size() - 1;
    for (size_t i = 0; i < count; ++i) {
        size_t next = (i + 1) % m_vertices.size();
        
        Line seg(m_vertices[i].pos, m_vertices[next].pos);
        geom::Vec3 segClose = seg.GetClosestPoint(point);
        double dist = point.DistanceTo(segClose);
        
        if (dist < minDist) {
            minDist = dist;
            closest = segClose;
        }
    }
    
    return closest;
}

double Polyline::GetArea() const {
    if (!m_closed || m_vertices.size() < 3) return 0.0;
    
    // Shoelace formula (2D)
    double area = 0.0;
    for (size_t i = 0; i < m_vertices.size(); ++i) {
        size_t next = (i + 1) % m_vertices.size();
        area += m_vertices[i].pos.x * m_vertices[next].pos.y;
        area -= m_vertices[next].pos.x * m_vertices[i].pos.y;
    }
    
    return std::abs(area) * 0.5;
}

geom::Vec3 Polyline::GetCentroid() const {
    if (m_vertices.empty()) return geom::Vec3();
    
    geom::Vec3 sum;
    for (const auto& v : m_vertices) {
        sum.x += v.pos.x;
        sum.y += v.pos.y;
        sum.z += v.pos.z;
    }
    
    double count = static_cast<double>(m_vertices.size());
    return geom::Vec3(sum.x / count, sum.y / count, sum.z / count);
}

bool Polyline::IsCounterClockwise() const {
    return GetArea() > 0.0;
}

void Polyline::Reverse() {
    std::reverse(m_vertices.begin(), m_vertices.end());
    
    // Bulge yönlerini ters çevir
    for (auto& v : m_vertices) {
        v.bulge = -v.bulge;
    }
}

void Polyline::Simplify(double tolerance) {
    // Douglas-Peucker algorithm (simplified)
    if (m_vertices.size() <= 2) return;
    // TODO: Implement full Douglas-Peucker
}

void Polyline::RemoveRedundantVertices(double angleTolerance) {
    // TODO: Remove collinear vertices
}

void Polyline::Move(const geom::Vec3& delta) {
    for (auto& v : m_vertices) {
        v.pos.x += delta.x;
        v.pos.y += delta.y;
        v.pos.z += delta.z;
    }
}

void Polyline::Scale(const geom::Vec3& scale) {
    for (auto& v : m_vertices) {
        v.pos.x *= scale.x;
        v.pos.y *= scale.y;
        v.pos.z *= scale.z;
    }
}

void Polyline::Rotate(double angle) {
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    
    for (auto& v : m_vertices) {
        double x = v.pos.x * cosA - v.pos.y * sinA;
        double y = v.pos.x * sinA + v.pos.y * cosA;
        v.pos.x = x;
        v.pos.y = y;
    }
}

void Polyline::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    for (auto& v : m_vertices) {
        Line mirrorLine(p1, p2);
        geom::Vec3 closest = mirrorLine.GetClosestPoint(v.pos);
        geom::Vec3 delta = closest - v.pos;
        v.pos = closest + delta;
        v.bulge = -v.bulge;  // Reverse arc direction
    }
}

void Polyline::FlattenBulges() {
    for (auto& v : m_vertices) {
        v.bulge = 0.0;
    }
}

bool Polyline::HasArcs() const {
    for (const auto& v : m_vertices) {
        if (std::abs(v.bulge) > 1e-10) return true;
    }
    return false;
}

void Polyline::ConvertArcsToLines(int segmentsPerArc) {
    // TODO: Convert arc segments to line segments
}

std::vector<std::unique_ptr<Entity>> Polyline::Explode() const {
    std::vector<std::unique_ptr<Entity>> lines;
    
    if (m_vertices.size() < 2) return lines;
    
    size_t count = m_closed ? m_vertices.size() : m_vertices.size() - 1;
    for (size_t i = 0; i < count; ++i) {
        size_t next = (i + 1) % m_vertices.size();
        auto line = std::make_unique<Line>(m_vertices[i].pos, m_vertices[next].pos);
        line->SetLayer(const_cast<Layer*>(GetLayer()));
        line->SetColor(GetColor());
        lines.push_back(std::move(line));
    }
    
    return lines;
}

std::unique_ptr<Polyline> Polyline::Offset(double distance) const {
    // TODO: Implement polyline offset (complex operation)
    return std::make_unique<Polyline>(*this);
}

} // namespace vkt::cad
