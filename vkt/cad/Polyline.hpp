#pragma once

#include "Entity.hpp"
#include "geom/Math.hpp"
#include <vector>

namespace vkt::cad {

/**
 * @brief Çoklu segment çizgi entity'si (AutoCAD LWPOLYLINE benzeri)
 * 
 * Polyline birden fazla noktayı birleştiren çizgi segmentleridir.
 * Açık veya kapalı olabilir.
 */
class Polyline : public Entity {
public:
    /**
     * @brief Vertex bilgisi (bulge desteği ile)
     * 
     * Bulge: Arc segment için kullanılır
     * bulge = 0: Düz çizgi
     * bulge > 0: Saat yönünün tersine kavis
     * bulge < 0: Saat yönünde kavis
     * bulge = tan(angle/4)
     */
    struct Vertex {
        geom::Vec3 pos;      ///< Vertex konumu
        double bulge = 0.0;  ///< Arc bulge değeri
        double width = 0.0;  ///< Segment genişliği (variable width için)
    };

    // Constructor
    Polyline();
    Polyline(const std::vector<geom::Vec3>& points, bool closed = false);
    Polyline(const std::vector<Vertex>& vertices, bool closed = false);
    
    // Entity interface implementation
    EntityType GetType() const override { return EntityType::Polyline; }
    std::unique_ptr<Entity> Clone() const override;
    BoundingBox GetBounds() const override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    
    // Vertex yönetimi
    void AddVertex(const geom::Vec3& pos, double bulge = 0.0, double width = 0.0);
    void AddVertex(const Vertex& vertex);
    void InsertVertex(size_t index, const Vertex& vertex);
    void RemoveVertex(size_t index);
    void ClearVertices();
    
    size_t GetVertexCount() const { return m_vertices.size(); }
    const Vertex& GetVertex(size_t index) const;
    Vertex& GetVertex(size_t index);
    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    
    void SetVertex(size_t index, const Vertex& vertex);
    void SetVertexPosition(size_t index, const geom::Vec3& pos);
    void SetVertexBulge(size_t index, double bulge);
    
    // Kapalı/açık polyline
    bool IsClosed() const { return m_closed; }
    void SetClosed(bool closed) { m_closed = closed; }
    void Close() { m_closed = true; }
    void Open() { m_closed = false; }
    
    // Geometrik işlemler
    /**
     * @brief Polyline'ın toplam uzunluğunu hesaplar
     * @return Tüm segmentlerin toplam uzunluğu
     */
    double GetLength() const;
    
    /**
     * @brief Polyline üzerinde parametrik konumdaki noktayı döndürür
     * @param t Parametrik değer [0,1] aralığında, tüm polyline boyunca
     * @return Hesaplanan nokta koordinatı
     */
    geom::Vec3 GetPointAt(double t) const;
    
    /**
     * @brief Polyline üzerinde belirli bir uzunlıktaki noktayı döndürür
     * @param distance Başlangıçtan itibaren mesafe
     * @return Hesaplanan nokta koordinatı
     */
    geom::Vec3 GetPointAtDistance(double distance) const;
    
    /**
     * @brief Bir noktaya en yakın polyline noktasını bulur
     * @param point Test noktası
     * @return En yakın nokta
     */
    geom::Vec3 GetClosestPoint(const geom::Vec3& point) const;
    
    /**
     * @brief Polyline'ın alanını hesaplar (kapalı polyline için)
     * @return Alan değeri (pozitif: CCW, negatif: CW)
     */
    double GetArea() const;
    
    /**
     * @brief Polyline'ın ağırlık merkezini hesaplar
     * @return Centroid koordinatı
     */
    geom::Vec3 GetCentroid() const;
    
    /**
     * @brief Polyline'ın merkez noktasını döndürür (alias for GetCentroid)
     * @return Merkez koordinatı
     */
    geom::Vec3 GetCenter() const { return GetCentroid(); }
    
    /**
     * @brief Polyline'ın yönünü kontrol eder (kapalı için)
     * @return true: Counter-clockwise, false: Clockwise
     */
    bool IsCounterClockwise() const;
    
    /**
     * @brief Polyline yönünü tersine çevirir
     */
    void Reverse();
    
    // Simplification
    /**
     * @brief Douglas-Peucker algoritması ile sadeleştirme
     * @param tolerance Tolerans değeri
     */
    void Simplify(double tolerance);
    
    /**
     * @brief Düz segmentlerdeki ara noktaları kaldırır
     * @param angleTolerance Açı toleransı (radyan)
     */
    void RemoveRedundantVertices(double angleTolerance = 1e-6);
    
    // Transformations (Entity base class'ı override eder)
    void Move(const geom::Vec3& delta) override;
    void Scale(const geom::Vec3& scale) override;
    void Rotate(double angle) override;
    void Mirror(const geom::Vec3& p1, const geom::Vec3& p2) override;
    
    // Bulge işlemleri
    /**
     * @brief Tüm bulge'ları sıfırlar (düz çizgiler yapar)
     */
    void FlattenBulges();
    
    /**
     * @brief Polyline'da arc segment var mı?
     * @return true: En az bir bulge != 0
     */
    bool HasArcs() const;
    
    /**
     * @brief Arc segmentleri düz çizgilere dönüştürür
     * @param segmentsPerArc Her arc için segment sayısı
     */
    void ConvertArcsToLines(int segmentsPerArc = 8);
    
    // Conversion
    /**
     * @brief Polyline'ı Line entity'lerine çevirir
     * @return Line entity'lerinin vektörü
     */
    std::vector<std::unique_ptr<Entity>> Explode() const;
    
    // Offset işlemi
    /**
     * @brief Polyline'ı offset eder (paralel kopyalama)
     * @param distance Offset mesafesi (pozitif: sol, negatif: sağ)
     * @return Offset edilmiş polyline
     */
    std::unique_ptr<Polyline> Offset(double distance) const;
    
private:
    std::vector<Vertex> m_vertices;
    bool m_closed;
    
    // Helper fonksiyonlar
    struct SegmentInfo {
        size_t index;
        double length;
        double accumulatedLength;
    };
    
    std::vector<SegmentInfo> BuildSegmentInfo() const;
    geom::Vec3 EvaluateBulgeSegment(const Vertex& v1, const Vertex& v2, double t) const;
    double GetSegmentLength(const Vertex& v1, const Vertex& v2) const;
};

} // namespace vkt::cad
