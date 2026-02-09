#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"

namespace vkt::cad {

/**
 * @brief Circle (daire) entity'si
 * 
 * Merkez ve yarıçap ile tanımlanır.
 */
class Circle : public Entity {
public:
    Circle();
    Circle(const geom::Vec3& center, double radius);
    
    // Entity interface
    EntityType GetType() const override { return EntityType::Circle; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    
    // Circle parametreleri
    const geom::Vec3& GetCenter() const { return m_center; }
    void SetCenter(const geom::Vec3& center) { m_center = center; }
    
    double GetRadius() const { return m_radius; }
    void SetRadius(double radius) { m_radius = radius; }
    
    // Geometrik hesaplamalar
    /**
     * @brief Dairenin çevresini hesaplar
     */
    double GetCircumference() const;
    
    /**
     * @brief Dairenin alanını hesaplar
     */
    double GetArea() const;
    
    /**
     * @brief Dairenin çapını döndürür
     */
    double GetDiameter() const;
    
    /**
     * @brief Verilen açıdaki noktayı döndürür (derece)
     */
    geom::Vec3 GetPointAtAngle(double angle) const;
    
    /**
     * @brief Parametrik konumdaki noktayı döndürür
     * @param t Parametrik değer [0,1] aralığında
     */
    geom::Vec3 GetPointAt(double t) const;
    
    /**
     * @brief Bir noktanın daire üzerinde olup olmadığını kontrol eder
     */
    bool ContainsPoint(const geom::Vec3& point, double tolerance = 1e-6) const;
    
    /**
     * @brief Bir noktanın daire içinde olup olmadığını kontrol eder
     */
    bool ContainsPointInside(const geom::Vec3& point) const;
    
    /**
     * @brief Bir noktaya en yakın çember noktasını bulur
     */
    geom::Vec3 GetClosestPoint(const geom::Vec3& point) const;
    
    /**
     * @brief Verilen noktadaki teğet çizginin yönünü döndürür
     */
    geom::Vec3 GetTangentAt(const geom::Vec3& point) const;
    
    /**
     * @brief Verilen noktadaki normal vektörü döndürür
     */
    geom::Vec3 GetNormalAt(const geom::Vec3& point) const;
    
    // İntersection testleri
    /**
     * @brief Başka bir circle ile kesişme noktalarını bulur
     * @return Kesişme noktaları (0, 1 veya 2 nokta)
     */
    std::vector<geom::Vec3> IntersectWith(const Circle& other, double tolerance = 1e-6) const;
    
    /**
     * @brief Bir çizgi ile kesişme noktalarını bulur
     * @param lineStart Çizgi başlangıç noktası
     * @param lineEnd Çizgi bitiş noktası
     * @return Kesişme noktaları (0, 1 veya 2 nokta)
     */
    std::vector<geom::Vec3> IntersectWithLine(
        const geom::Vec3& lineStart,
        const geom::Vec3& lineEnd,
        double tolerance = 1e-6
    ) const;
    
    /**
     * @brief Başka bir circle ile teğet mi?
     */
    bool IsTangentTo(const Circle& other, double tolerance = 1e-6) const;
    
    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;
    
    // Conversion
    /**
     * @brief Circle'ı çoklu Line segmentlerine dönüştürür
     * @param segments Segment sayısı
     * @return Line entity'lerinin vektörü
     */
    std::vector<std::unique_ptr<Entity>> Tessellate(int segments = 32) const;
    
    /**
     * @brief Circle'ı Arc'lara böler
     * @param numArcs Arc sayısı (tipik: 2 veya 4)
     * @return Arc entity'lerinin vektörü
     */
    std::vector<std::unique_ptr<Entity>> SplitToArcs(int numArcs = 4) const;
    
    // Statik helper'lar
    /**
     * @brief 3 noktadan geçen circle oluşturur
     */
    static std::unique_ptr<Circle> CreateFrom3Points(
        const geom::Vec3& p1,
        const geom::Vec3& p2,
        const geom::Vec3& p3
    );
    
    /**
     * @brief 2 nokta ve yarıçaptan circle oluşturur
     */
    static std::unique_ptr<Circle> CreateFrom2PointsAndRadius(
        const geom::Vec3& p1,
        const geom::Vec3& p2,
        double radius,
        bool leftSide = true
    );

private:
    geom::Vec3 m_center;
    double m_radius;
};

} // namespace vkt::cad
