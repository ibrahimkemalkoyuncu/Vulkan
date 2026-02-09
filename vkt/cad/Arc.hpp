#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"

namespace vkt::cad {

/**
 * @brief Arc (yay) entity'si
 * 
 * Merkez, yarıçap, başlangıç ve bitiş açıları ile tanımlanır.
 * Açılar derece cinsinden (AutoCAD standardı).
 */
class Arc : public Entity {
public:
    Arc();
    Arc(const geom::Vec3& center, double radius, double startAngle, double endAngle);
    
    // Entity interface
    EntityType GetType() const override { return EntityType::Arc; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    
    // Arc parametreleri
    const geom::Vec3& GetCenter() const { return m_center; }
    void SetCenter(const geom::Vec3& center) { m_center = center; }
    
    double GetRadius() const { return m_radius; }
    void SetRadius(double radius) { m_radius = radius; }
    
    double GetStartAngle() const { return m_startAngle; }
    void SetStartAngle(double angle) { m_startAngle = angle; }
    
    double GetEndAngle() const { return m_endAngle; }
    void SetEndAngle(double angle) { m_endAngle = angle; }
    
    // Geometrik hesaplamalar
    /**
     * @brief Arc'ın başlangıç noktasını döndürür
     */
    geom::Vec3 GetStartPoint() const;
    
    /**
     * @brief Arc'ın bitiş noktasını döndürür
     */
    geom::Vec3 GetEndPoint() const;
    
    /**
     * @brief Arc'ın orta noktasını döndürür
     */
    geom::Vec3 GetMidPoint() const;
    
    /**
     * @brief Arc'ın uzunluğunu hesaplar
     */
    double GetLength() const;
    
    /**
     * @brief Arc'ın kapsadığı açıyı hesaplar (derece)
     */
    double GetSweepAngle() const;
    
    /**
     * @brief Arc'ın kapsadığı açıyı hesaplar (radyan)
     */
    double GetSweepAngleRadians() const;
    
    /**
     * @brief Arc üzerinde parametrik konumdaki noktayı döndürür
     * @param t Parametrik değer [0,1] aralığında
     */
    geom::Vec3 GetPointAt(double t) const;
    
    /**
     * @brief Verilen açıdaki noktayı döndürür
     * @param angle Açı (derece)
     */
    geom::Vec3 GetPointAtAngle(double angle) const;
    
    /**
     * @brief Bir noktanın arc üzerinde olup olmadığını kontrol eder
     */
    bool ContainsPoint(const geom::Vec3& point, double tolerance = 1e-6) const;
    
    /**
     * @brief Bir noktaya en yakın arc noktasını bulur
     */
    geom::Vec3 GetClosestPoint(const geom::Vec3& point) const;
    
    /**
     * @brief Arc'ın ağırlık merkezini hesaplar
     */
    geom::Vec3 GetCentroid() const;
    
    // Yön kontrolü
    /**
     * @brief Arc'ın yönü CCW mi? (counter-clockwise)
     */
    bool IsCounterClockwise() const;
    
    /**
     * @brief Arc yönünü tersine çevirir
     */
    void Reverse();
    
    // Transformations
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;
    
    // Conversion
    /**
     * @brief Arc'ı çoklu Line segmentlerine dönüştürür
     * @param segments Segment sayısı
     * @return Line entity'lerinin vektörü
     */
    std::vector<std::unique_ptr<Entity>> Tessellate(int segments = 16) const;
    
    /**
     * @brief Arc'ı Circle'a dönüştürür (360 derece yap)
     */
    std::unique_ptr<Entity> ToCircle() const;
    
    // Statik helper'lar
    /**
     * @brief 3 noktadan arc oluşturur
     */
    static std::unique_ptr<Arc> CreateFrom3Points(
        const geom::Vec3& start,
        const geom::Vec3& mid,
        const geom::Vec3& end
    );
    
    /**
     * @brief Başlangıç, bitiş noktası ve bulge'dan arc oluşturur
     */
    static std::unique_ptr<Arc> CreateFromBulge(
        const geom::Vec3& start,
        const geom::Vec3& end,
        double bulge
    );

private:
    geom::Vec3 m_center;
    double m_radius;
    double m_startAngle;  // Derece cinsinden
    double m_endAngle;    // Derece cinsinden
    
    // Helper fonksiyon
    double NormalizeAngle(double angle) const;
};

} // namespace vkt::cad
