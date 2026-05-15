#include "cad/Spline.hpp"
#include <cmath>
#include <algorithm>

namespace vkt::cad {

Spline::Spline()
    : Entity(EntityType::Spline)
{}

// ── Tessellate ────────────────────────────────────────────────────────────────

std::vector<geom::Vec3> Spline::Tessellate(int segments) const {
    // A) Fit noktaları varsa doğrudan döndür (already on curve)
    if (HasFitPoints()) {
        return m_fitPts;
    }

    // B) De Boor B-spline evaluation
    if (!HasCtrlPoints() || m_knots.empty()) return {};

    int n = static_cast<int>(m_ctrlPts.size());
    int p = m_degree;
    int numKnots = static_cast<int>(m_knots.size());
    if (n < 2 || p < 1 || numKnots < n + p + 1) return {};

    double tStart = m_knots[p];
    double tEnd   = m_knots[n];  // knots[num_ctrl_pts]
    if (tEnd <= tStart) return {};

    if (segments <= 0) segments = std::max(32, n * 8);

    std::vector<geom::Vec3> pts;
    pts.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        double t = tStart + (tEnd - tStart) * static_cast<double>(i) / segments;
        pts.push_back(DeBoor(t, tStart, tEnd));
    }
    return pts;
}

geom::Vec3 Spline::DeBoor(double t, double /*tStart*/, double tEnd) const {
    int n = static_cast<int>(m_ctrlPts.size());
    int p = m_degree;
    int numKnots = static_cast<int>(m_knots.size());

    // Clamp t to avoid overrun at the end
    if (t >= tEnd) t = tEnd - 1e-10;

    // Find knot span k such that knots[k] <= t < knots[k+1]
    int k = p;
    for (int j = p; j < numKnots - 1; ++j) {
        if (t >= m_knots[j] && t < m_knots[j + 1]) { k = j; break; }
    }

    // De Boor triangular algorithm
    std::vector<double> dx(p + 1), dy(p + 1);
    for (int j = 0; j <= p; ++j) {
        int idx = std::clamp(k - p + j, 0, n - 1);
        dx[j] = m_ctrlPts[idx].x;
        dy[j] = m_ctrlPts[idx].y;
    }
    for (int r = 1; r <= p; ++r) {
        for (int j = p; j >= r; --j) {
            int ki  = std::clamp(k - p + j,         0, numKnots - 1);
            int ki2 = std::clamp(k - p + j + p - r + 1, 0, numKnots - 1);
            double denom = m_knots[ki2] - m_knots[ki];
            double alpha = (denom > 1e-12) ? (t - m_knots[ki]) / denom : 0.0;
            dx[j] = (1.0 - alpha) * dx[j - 1] + alpha * dx[j];
            dy[j] = (1.0 - alpha) * dy[j - 1] + alpha * dy[j];
        }
    }
    return geom::Vec3(dx[p], dy[p], 0.0);
}

// ── Entity interface ──────────────────────────────────────────────────────────

std::unique_ptr<Entity> Spline::Clone() const {
    auto s = std::make_unique<Spline>();
    s->m_fitPts  = m_fitPts;
    s->m_ctrlPts = m_ctrlPts;
    s->m_knots   = m_knots;
    s->m_degree  = m_degree;
    s->m_closed  = m_closed;
    s->m_color   = m_color;
    s->m_layer   = m_layer;
    return s;
}

BoundingBox Spline::GetBounds() const {
    BoundingBox bb;
    auto pts = Tessellate(64);
    for (const auto& p : pts) bb.Expand(p);
    return bb;
}

void Spline::Serialize(nlohmann::json& j) const {
    j["type"]   = "Spline";
    j["degree"] = m_degree;
    j["closed"] = m_closed;

    auto toArr = [](const std::vector<geom::Vec3>& v) {
        nlohmann::json a = nlohmann::json::array();
        for (const auto& p : v) a.push_back({p.x, p.y, p.z});
        return a;
    };
    j["fitPts"]  = toArr(m_fitPts);
    j["ctrlPts"] = toArr(m_ctrlPts);
    j["knots"]   = m_knots;
}

void Spline::Deserialize(const nlohmann::json& j) {
    m_degree = j.value("degree", 3);
    m_closed = j.value("closed", false);

    auto fromArr = [](const nlohmann::json& a) {
        std::vector<geom::Vec3> v;
        for (const auto& p : a)
            v.emplace_back(p[0].get<double>(), p[1].get<double>(), p[2].get<double>());
        return v;
    };
    if (j.contains("fitPts"))  m_fitPts  = fromArr(j["fitPts"]);
    if (j.contains("ctrlPts")) m_ctrlPts = fromArr(j["ctrlPts"]);
    if (j.contains("knots"))   m_knots   = j["knots"].get<std::vector<double>>();
}

// ── Transformations ───────────────────────────────────────────────────────────

template<typename Fn>
void Spline::TransformPts(std::vector<geom::Vec3>& pts, Fn fn) {
    for (auto& p : pts) fn(p);
}

void Spline::Move(const geom::Vec3& d) {
    auto mv = [&](geom::Vec3& p){ p.x += d.x; p.y += d.y; p.z += d.z; };
    TransformPts(m_fitPts,  mv);
    TransformPts(m_ctrlPts, mv);
}

void Spline::Scale(const geom::Vec3& s) {
    auto sc = [&](geom::Vec3& p){ p.x *= s.x; p.y *= s.y; p.z *= s.z; };
    TransformPts(m_fitPts,  sc);
    TransformPts(m_ctrlPts, sc);
}

void Spline::Rotate(double angleDeg) {
    double rad  = angleDeg * 3.14159265358979323846 / 180.0;
    double cosA = std::cos(rad), sinA = std::sin(rad);
    auto rot = [&](geom::Vec3& p) {
        double nx = p.x * cosA - p.y * sinA;
        double ny = p.x * sinA + p.y * cosA;
        p.x = nx; p.y = ny;
    };
    TransformPts(m_fitPts,  rot);
    TransformPts(m_ctrlPts, rot);
}

void Spline::Mirror(const geom::Vec3& p1, const geom::Vec3& p2) {
    double dx = p2.x - p1.x, dy = p2.y - p1.y;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-24) return;
    auto mir = [&](geom::Vec3& p) {
        double t = ((p.x - p1.x) * dx + (p.y - p1.y) * dy) / len2;
        p.x = 2.0 * (p1.x + t * dx) - p.x;
        p.y = 2.0 * (p1.y + t * dy) - p.y;
    };
    TransformPts(m_fitPts,  mir);
    TransformPts(m_ctrlPts, mir);
}

} // namespace vkt::cad
