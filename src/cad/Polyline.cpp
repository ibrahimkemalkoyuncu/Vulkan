#include "cad/Polyline.hpp"
#include "cad/Line.hpp"
#include "cad/Layer.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

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
    if (m_vertices.size() <= 2) return;

    // Douglas-Peucker recursive implementation using index ranges
    const double tol2 = tolerance * tolerance;

    // Returns indices to keep (always keeps first and last)
    std::function<void(size_t, size_t, std::vector<bool>&)> dp =
        [&](size_t start, size_t end, std::vector<bool>& keep) {
            if (end <= start + 1) return;

            const geom::Vec3& p1 = m_vertices[start].pos;
            const geom::Vec3& p2 = m_vertices[end].pos;

            double ex   = p2.x - p1.x;
            double ey   = p2.y - p1.y;
            double len2 = ex*ex + ey*ey;

            double maxDist2 = 0.0;
            size_t maxIdx   = start;

            for (size_t i = start + 1; i < end; ++i) {
                const geom::Vec3& p = m_vertices[i].pos;
                double dist2;
                if (len2 < geom::EPSILON) {
                    double dx = p.x - p1.x, dy = p.y - p1.y;
                    dist2 = dx*dx + dy*dy;
                } else {
                    double t  = ((p.x - p1.x)*ex + (p.y - p1.y)*ey) / len2;
                    t         = std::max(0.0, std::min(1.0, t));
                    double cx = p1.x + t*ex, cy = p1.y + t*ey;
                    double dx = p.x - cx,    dy = p.y - cy;
                    dist2 = dx*dx + dy*dy;
                }
                if (dist2 > maxDist2) { maxDist2 = dist2; maxIdx = i; }
            }

            if (maxDist2 > tol2) {
                keep[maxIdx] = true;
                dp(start, maxIdx, keep);
                dp(maxIdx, end, keep);
            }
        };

    std::vector<bool> keep(m_vertices.size(), false);
    keep.front() = true;
    keep.back()  = true;
    dp(0, m_vertices.size() - 1, keep);

    std::vector<Vertex> result;
    result.reserve(m_vertices.size());
    for (size_t i = 0; i < m_vertices.size(); ++i) {
        if (keep[i]) result.push_back(m_vertices[i]);
    }
    m_vertices = std::move(result);
}

void Polyline::RemoveRedundantVertices(double angleTolerance) {
    if (m_vertices.size() < 3) return;

    std::vector<Vertex> result;
    result.push_back(m_vertices.front());

    for (size_t i = 1; i + 1 < m_vertices.size(); ++i) {
        const geom::Vec3& prev = result.back().pos;
        const geom::Vec3& curr = m_vertices[i].pos;
        const geom::Vec3& next = m_vertices[i + 1].pos;

        // Skip vertex only if it has no bulge and is collinear within tolerance
        if (std::abs(m_vertices[i].bulge) > geom::EPSILON) {
            result.push_back(m_vertices[i]);
            continue;
        }

        double ax = curr.x - prev.x, ay = curr.y - prev.y;
        double bx = next.x - curr.x, by = next.y - curr.y;
        double cross = ax*by - ay*bx; // |a||b|sin(angle)
        double dot   = ax*bx + ay*by;
        double angle = std::atan2(std::abs(cross), dot);
        if (angle > angleTolerance) {
            result.push_back(m_vertices[i]);
        }
    }

    result.push_back(m_vertices.back());
    m_vertices = std::move(result);
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
    if (!HasArcs()) return;
    if (segmentsPerArc < 1) segmentsPerArc = 1;

    std::vector<Vertex> result;

    size_t count = m_closed ? m_vertices.size() : m_vertices.size() - 1;
    for (size_t i = 0; i < count; ++i) {
        const Vertex& v1 = m_vertices[i];
        const Vertex& v2 = m_vertices[(i + 1) % m_vertices.size()];

        result.push_back({v1.pos, 0.0, v1.width});

        if (std::abs(v1.bulge) > geom::EPSILON) {
            // Convert bulge arc to polyline segments
            // bulge = tan(theta/4), theta = included angle
            double theta    = 4.0 * std::atan(v1.bulge);
            double dx       = v2.pos.x - v1.pos.x;
            double dy       = v2.pos.y - v1.pos.y;
            double chordLen = std::sqrt(dx*dx + dy*dy);
            if (chordLen < geom::EPSILON) continue;

            double radius = chordLen / (2.0 * std::sin(theta / 2.0));
            // Midpoint of chord
            double mx     = (v1.pos.x + v2.pos.x) * 0.5;
            double my     = (v1.pos.y + v2.pos.y) * 0.5;
            // Perpendicular direction (towards arc center)
            double perpx  = -(v2.pos.y - v1.pos.y) / chordLen;
            double perpy  =  (v2.pos.x - v1.pos.x) / chordLen;
            double sagitta = radius - std::sqrt(radius*radius - (chordLen/2.0)*(chordLen/2.0));
            double sign    = (v1.bulge > 0) ? 1.0 : -1.0;
            double cx      = mx + sign * perpx * (radius - sagitta);
            double cy      = my + sign * perpy * (radius - sagitta);

            double startAngle = std::atan2(v1.pos.y - cy, v1.pos.x - cx);
            double endAngle   = std::atan2(v2.pos.y - cy, v2.pos.x - cx);

            // Normalize angle sweep direction
            if (v1.bulge > 0 && endAngle < startAngle) endAngle += 2.0 * geom::PI;
            if (v1.bulge < 0 && endAngle > startAngle) endAngle -= 2.0 * geom::PI;

            for (int s = 1; s < segmentsPerArc; ++s) {
                double t   = static_cast<double>(s) / segmentsPerArc;
                double ang = startAngle + t * (endAngle - startAngle);
                geom::Vec3 pt(cx + radius * std::cos(ang), cy + radius * std::sin(ang), v1.pos.z);
                result.push_back({pt, 0.0, v1.width});
            }
        }
    }

    // Add last vertex
    if (!m_closed) result.push_back({m_vertices.back().pos, 0.0, m_vertices.back().width});

    m_vertices = std::move(result);
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
    if (m_vertices.size() < 2) return std::make_unique<Polyline>(*this);

    // Per-segment outward normal offset: compute offset point for each vertex
    // using the bisector of the two adjacent segment normals.
    size_t n = m_vertices.size();
    std::vector<geom::Vec3> offsetPts;
    offsetPts.reserve(n);

    auto segNormal = [&](size_t i) -> std::pair<double, double> {
        size_t j = (i + 1) % n;
        double dx = m_vertices[j].pos.x - m_vertices[i].pos.x;
        double dy = m_vertices[j].pos.y - m_vertices[i].pos.y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len < geom::EPSILON) return {0.0, 0.0};
        return {-dy / len, dx / len}; // left-hand normal
    };

    for (size_t i = 0; i < n; ++i) {
        bool hasPrev = m_closed || (i > 0);
        bool hasNext = m_closed || (i < n - 1);

        double nx = 0, ny = 0;
        int    cnt = 0;

        if (hasPrev) {
            size_t pi = (i + n - 1) % n;
            auto [px, py] = segNormal(pi);
            nx += px; ny += py; ++cnt;
        }
        if (hasNext) {
            auto [px, py] = segNormal(i);
            nx += px; ny += py; ++cnt;
        }
        if (cnt > 0) { nx /= cnt; ny /= cnt; }

        // Normalize bisector
        double blen = std::sqrt(nx*nx + ny*ny);
        if (blen > geom::EPSILON) { nx /= blen; ny /= blen; }

        // Miter scale so offset is constant distance from both segments
        double miter = 1.0;
        if (hasPrev && hasNext) {
            size_t pi = (i + n - 1) % n;
            auto [ax, ay] = segNormal(pi);
            double dot = ax*nx + ay*ny;
            if (std::abs(dot) > 0.1) miter = 1.0 / dot;
            miter = std::min(miter, 5.0); // clamp extreme miters
        }

        offsetPts.push_back({
            m_vertices[i].pos.x + nx * distance * miter,
            m_vertices[i].pos.y + ny * distance * miter,
            m_vertices[i].pos.z
        });
    }

    auto result = std::make_unique<Polyline>(offsetPts, m_closed);
    result->SetLayer(const_cast<Layer*>(GetLayer()));
    result->SetColor(GetColor());
    return result;
}

} // namespace vkt::cad
