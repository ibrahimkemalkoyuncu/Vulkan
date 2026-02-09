/**
 * @file BRep.cpp
 * @brief Boundary Representation İmplementasyonu
 */

#include "geom/BRep.hpp"
#include <fstream>
#include <sstream>
#include <cmath>

namespace vkt {
namespace geom {

std::vector<Polyline> BRep::LoadFromDXF(const std::string& path) {
    std::vector<Polyline> polylines;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return polylines;
    }
    
    // Basitleştirilmiş DXF parser
    // Gerçek implementasyon için tam DXF parser kütüphanesi gerekli
    std::string line;
    Polyline currentPoly;
    bool inLWPolyline = false;
    
    while (std::getline(file, line)) {
        if (line.find("LWPOLYLINE") != std::string::npos) {
            if (inLWPolyline && !currentPoly.vertices.empty()) {
                polylines.push_back(currentPoly);
            }
            currentPoly = Polyline();
            inLWPolyline = true;
        }
        
        // Vertex okuma mantığı buraya eklenecek
        // DXF formatı: kod 10 = X, kod 20 = Y, kod 30 = Z
    }
    
    if (inLWPolyline && !currentPoly.vertices.empty()) {
        polylines.push_back(currentPoly);
    }
    
    file.close();
    return polylines;
}

double BRep::CalculateArea(const Polyline& poly) {
    if (poly.vertices.size() < 3) {
        return 0.0;
    }
    
    // Shoelace (Gauss) formülü
    double area = 0.0;
    size_t n = poly.vertices.size();
    
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += poly.vertices[i].x * poly.vertices[j].y;
        area -= poly.vertices[j].x * poly.vertices[i].y;
    }
    
    return std::abs(area) / 2.0;
}

bool BRep::IsPointInside(const Vec3& point, const Polyline& poly) {
    if (poly.vertices.size() < 3) {
        return false;
    }
    
    // Ray casting algorithm
    int crossings = 0;
    size_t n = poly.vertices.size();
    
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        
        const Vec3& vi = poly.vertices[i];
        const Vec3& vj = poly.vertices[j];
        
        // Ray: horizontal, sağa doğru
        if (((vi.y > point.y) != (vj.y > point.y)) &&
            (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x)) {
            crossings++;
        }
    }
    
    return (crossings % 2) == 1;
}

Vec3 BRep::CalculateCentroid(const Polyline& poly) {
    if (poly.vertices.empty()) {
        return Vec3{0, 0, 0};
    }
    
    Vec3 centroid{0, 0, 0};
    double signedArea = 0.0;
    size_t n = poly.vertices.size();
    
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        double a = poly.vertices[i].x * poly.vertices[j].y - 
                   poly.vertices[j].x * poly.vertices[i].y;
        signedArea += a;
        centroid.x += (poly.vertices[i].x + poly.vertices[j].x) * a;
        centroid.y += (poly.vertices[i].y + poly.vertices[j].y) * a;
    }
    
    signedArea *= 0.5;
    if (std::abs(signedArea) < 1e-10) {
        // Fallback: simple average
        for (const auto& v : poly.vertices) {
            centroid.x += v.x;
            centroid.y += v.y;
            centroid.z += v.z;
        }
        centroid.x /= poly.vertices.size();
        centroid.y /= poly.vertices.size();
        centroid.z /= poly.vertices.size();
    } else {
        centroid.x /= (6.0 * signedArea);
        centroid.y /= (6.0 * signedArea);
    }
    
    return centroid;
}

bool BRep::DoPolygonsIntersect(const Polyline& a, const Polyline& b) {
    // Basit kontrol: herhangi bir vertex karşı poligonda mı?
    for (const auto& vertex : a.vertices) {
        if (IsPointInside(vertex, b)) {
            return true;
        }
    }
    
    for (const auto& vertex : b.vertices) {
        if (IsPointInside(vertex, a)) {
            return true;
        }
    }
    
    // Kenar-kenar kesişim kontrolü buraya eklenebilir
    return false;
}

double BRep::Cross2D(const Vec3& a, const Vec3& b) {
    return a.x * b.y - a.y * b.x;
}

} // namespace geom
} // namespace vkt
