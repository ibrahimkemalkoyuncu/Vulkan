/**
 * @file BRep.hpp
 * @brief Boundary Representation (Sınır Gösterimi) Motoru
 * 
 * DXF'den mahal (space) tespiti ve alan hesabı.
 */

#pragma once

#include <vector>
#include <string>
#include "Math.hpp"

namespace vkt {
namespace geom {

/**
 * @struct Polyline
 * @brief Kapalı poligon (LWPolyline)
 */
struct Polyline {
    std::vector<Vec3> vertices;
    bool closed = true;
    std::string layer;
};

/**
 * @class BRep
 * @brief Boundary Representation işlemleri
 * 
 * Mimari projedeki kapalı poligonları analiz ederek
 * oda/mahal tanımlaması yapar.
 */
class BRep {
public:
    /**
     * @brief DXF dosyasından poligonları yükle
     */
    static std::vector<Polyline> LoadFromDXF(const std::string& path);
    
    /**
     * @brief Poligon alanını hesapla (Shoelace Algorithm)
     */
    static double CalculateArea(const Polyline& poly);
    
    /**
     * @brief Nokta poligon içinde mi? (Point-in-Polygon)
     */
    static bool IsPointInside(const Vec3& point, const Polyline& poly);
    
    /**
     * @brief Poligon merkezini bul (Centroid)
     */
    static Vec3 CalculateCentroid(const Polyline& poly);
    
    /**
     * @brief İki poligon kesişiyor mu?
     */
    static bool DoPolygonsIntersect(const Polyline& a, const Polyline& b);

private:
    static double Cross2D(const Vec3& a, const Vec3& b);
};

} // namespace geom
} // namespace vkt
