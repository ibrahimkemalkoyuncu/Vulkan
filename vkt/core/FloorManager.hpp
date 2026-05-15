/**
 * @file FloorManager.hpp
 * @brief Çok katlı bina kat yönetimi
 *
 * Her kat bir yükseklik aralığına karşılık gelir.
 * NetworkGraph node'larına ve CAD entity'lerine kat indeksi atanır.
 * RiserDiagram bu bilgiyi kullanarak kolon şeması üretir.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace vkt {
namespace core {

/**
 * @struct Floor
 * @brief Tek kat tanımı
 */
struct Floor {
    int     index      = 0;      ///< Kat numarası (0=zemin, -1=bodrum, 1=1.kat ...)
    std::string label;            ///< Görünen ad ("Zemin Kat", "1. Kat" ...)
    double  elevation_m = 0.0;   ///< Döşeme kotu (metre)
    double  height_m    = 3.0;   ///< Kat yüksekliği (metre)
    /// W-Block referans noktası — bu noktanın CAD koordinatı projenin (0,0)'ına eşlenir.
    /// Tüm katlarda aynı fiziksel noktanın (kolon köşesi, asansör kenarı vb.) seçilmesi
    /// gerekir; bu sayede 3D görünümde katlar doğru hizalanır.
    double  refX = 0.0;          ///< W-Block baz noktası X (CAD birimi)
    double  refY = 0.0;          ///< W-Block baz noktası Y (CAD birimi)
};

/**
 * @class FloorManager
 * @brief Kat listesini yönetir ve entity/node → kat eşlemesini sağlar
 */
class FloorManager {
public:
    FloorManager();

    /// Standart kat dizisi oluştur (bodrum dahil)
    /// @param numFloors  Zemin üstü kat sayısı
    /// @param hasBasement Bodrum kat var mı
    /// @param floorHeight Her katın yüksekliği (m)
    void BuildStandardFloors(int numFloors, bool hasBasement = false,
                             double floorHeight = 3.0);

    /// Manuel kat ekle
    void AddFloor(const Floor& floor);

    /// Kat listesini temizle
    void Clear();

    /// Kat sayısı
    size_t GetFloorCount() const { return m_floors.size(); }

    /// Tüm katları sıralı döndür (elevation ascending)
    const std::vector<Floor>& GetFloors() const { return m_floors; }

    /// Kotu (Z koordinatı) verilen bir noktanın hangi katta olduğunu döndür.
    /// Hiçbir kata uymuyorsa -999 döner.
    int GetFloorIndexAtElevation(double z_m) const;

    /// Kat indeksine göre Floor döndür (bulunamazsa nullptr)
    const Floor* GetFloor(int index) const;

    /// NetworkGraph node'u → kat indeksi ataması
    void AssignNodeToFloor(uint32_t nodeId, int floorIndex);
    int  GetFloorOfNode(uint32_t nodeId) const;

    /// CAD entity → kat indeksi ataması (EntityId = uint64_t)
    void AssignEntityToFloor(uint64_t entityId, int floorIndex);
    int  GetFloorOfEntity(uint64_t entityId) const;

    /// Belirli kattaki node ID'lerini döndür
    std::vector<uint32_t> GetNodesOnFloor(int floorIndex) const;

    /// JSON serialize / deserialize
    std::string Serialize() const;
    bool        Deserialize(const std::string& json);

private:
    std::vector<Floor> m_floors;                       ///< Sıralı kat listesi
    std::map<uint32_t, int> m_nodeFloorMap;            ///< nodeId → floorIndex
    std::map<uint64_t, int> m_entityFloorMap;          ///< entityId → floorIndex

    void SortFloors();
};

} // namespace core
} // namespace vkt
