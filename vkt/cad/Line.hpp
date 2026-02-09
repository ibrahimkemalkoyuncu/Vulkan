/**
 * @file Line.hpp
 * @brief Line Entity - CAD Çizgi Segmenti
 * 
 * Line (Çizgi) Nedir?
 * - İki nokta arasındaki düz çizgi (segment)
 * - CAD'in en temel geometrik primitive'i
 * - Start point (başlangıç) ve End point (bitiş) ile tanımlanır
 * 
 * Kullanım Alanları:
 * - Mimari çizimler: duvar konturu, pencere, kapı
 * - MEP: boru segmentleri (ek olarak çap bilgisi taşır)
 * - Boyutlandırma: ölçü çizgisi uzantıları
 * - Referans çizgileri: grid, construction lines
 * 
 * Özellikler:
 * - 2D/3D destekli (Z koordinatı kullanılabilir)
 * - Uzunluk hesaplama
 * - En yakın nokta bulma (projection)
 * - Kesişim testi (line-line intersection)
 * 
 * AutoCAD Komut Karşılığı: LINE
 */

#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include "nlohmann/json.hpp"

namespace vkt {
namespace cad {

/**
 * @class Line
 * @brief İki nokta arasındaki çizgi segment entity
 * 
 * Geometrik Tanım:
 * - P(t) = start + t * (end - start), t ∈ [0,1]
 * - t=0 ise start point
 * - t=1 ise end point
 */
class Line : public Entity {
public:
    /**
     * @brief Line oluştur
     * @param start Başlangıç noktası
     * @param end Bitiş noktası
     * @param layer Hangi katmanda (nullptr ise default)
     */
    Line(const geom::Vec3& start, const geom::Vec3& end, Layer* layer = nullptr);
    
    /** @brief Destructor */
    ~Line() override = default;
    
    // ==================== Entity Interface ====================
    
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    
    // ==================== GEOMETRIK ÖZELLİKLER ====================
    
    /** @brief Başlangıç noktası */
    geom::Vec3 GetStart() const { return m_start; }
    void SetStart(const geom::Vec3& start) { 
        m_start = start; 
        SetFlags(GetFlags() | EF_Modified);
    }
    
    /** @brief Bitiş noktası */
    geom::Vec3 GetEnd() const { return m_end; }
    void SetEnd(const geom::Vec3& end) { 
        m_end = end; 
        SetFlags(GetFlags() | EF_Modified);
    }
    
    /** @brief Her iki noktayı birden ayarla */
    void SetPoints(const geom::Vec3& start, const geom::Vec3& end) {
        m_start = start;
        m_end = end;
        SetFlags(GetFlags() | EF_Modified);
    }
    
    // ==================== HESAPLAMALAR ====================
    
    /**
     * @brief Çizgi uzunluğu
     * @return Öklidyen mesafe (metre cinsinden)
     */
    double GetLength() const;
    
    /**
     * @brief Çizgi yönü (birim vektör)
     * @return Normalleştirilmiş yön vektörü (end - start)
     */
    geom::Vec3 GetDirection() const;
    
    /**
     * @brief Orta nokta
     * @return (start + end) / 2
     */
    geom::Vec3 GetMidPoint() const;
    
    /**
     * @brief Parametrik nokta hesapla
     * @param t Parametre [0,1]
     * @return P(t) = start + t * (end - start)
     * 
     * Örnekler:
     * - t=0.0 → start point
     * - t=0.5 → mid point
     * - t=1.0 → end point
     */
    geom::Vec3 GetPointAt(double t) const;
    
    /**
     * @brief Noktanın çizgiye en yakın konumu
     * @param point Test noktası
     * @param t Çıktı: parametrik konum [0,1]
     * @return En yakın nokta
     * 
     * Kullanım: Snap (nearest point on line)
     */
    geom::Vec3 GetClosestPoint(const geom::Vec3& point, double* t = nullptr) const;
    
    /**
     * @brief Noktanın çizgiye uzaklığı
     * @param point Test noktası
     * @return Dik mesafe (metre)
     */
    double DistanceToPoint(const geom::Vec3& point) const;
    
    // ==================== KESİŞİM TESTLERİ ====================
    
    /**
     * @brief İki çizgi kesişiyor mu? (2D, Z yoksayılır)
     * @param other Diğer çizgi
     * @param intersection Çıktı: kesişim noktası (varsa)
     * @param tolerance Tolerans (metre)
     * @return Kesişim var ise true
     * 
     * Durumlar:
     * - Paralel çizgiler: false
     * - Uzantıları kesişir ama segmentler kesişmez: false
     * - Gerçek kesişim: true + intersection noktası döner
     */
    bool IntersectsWith(const Line& other, geom::Vec3* intersection = nullptr, 
                        double tolerance = 1e-6) const;
    
    /**
     * @brief Çizgiler paralel mi?
     * @param other Diğer çizgi
     * @param tolerance Açı toleransı (radyan)
     * @return Paralel ise true
     */
    bool IsParallelTo(const Line& other, double tolerance = 1e-6) const;
    
    /**
     * @brief Çizgiler dik mi?
     * @param other Diğer çizgi
     * @param tolerance Açı toleransı (radyan)
     * @return Dik ise true (90 derece)
     */
    bool IsPerpendicularTo(const Line& other, double tolerance = 1e-6) const;
    
    // ==================== TRANSFORMASYONLARImpl ====================
    
    /**
     * @brief Çizgiyi taşı (offset)
     * @param delta Hareket vektörü
     */
    void Move(const geom::Vec3& delta);
    
    /**
     * @brief Çizgiyi ölçekle
     * @param origin Ölçek merkezi
     * @param factor Ölçek faktörü
     */
    void Scale(const geom::Vec3& origin, double factor);
    
    /**
     * @brief Çizgiyi döndür (Z ekseni etrafında)
     * @param center Dönüş merkezi
     * @param angleDegrees Açı (derece)
     */
    void Rotate(const geom::Vec3& center, double angleDegrees);
    
    /**
     * @brief Çizgiyi aynala
     * @param axis Aynalama ekseni (başka bir line)
     */
    void Mirror(const Line& axis);

private:
    geom::Vec3 m_start{0, 0, 0};  ///< Başlangıç noktası
    geom::Vec3 m_end{0, 0, 0};    ///< Bitiş noktası
};

} // namespace cad
} // namespace vkt
