/**
 * @file DXFWriter.hpp
 * @brief AutoCAD uyumlu ASCII DXF R2000 dosya yazıcısı
 *
 * Desteklenen entity'ler: LINE, LWPOLYLINE, CIRCLE, ARC, ELLIPSE, TEXT, INSERT
 * BLOCKS section: BlockRegistry içindeki tüm blok tanımları dışa aktarılır.
 *
 * Format: DXF R2000 (AC1015) — AutoCAD 2000+ ve tüm CAD yazılımları okur
 */

#pragma once

#include "Entity.hpp"
#include "Block.hpp"
#include "Layer.hpp"
#include "mep/NetworkGraph.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace vkt {
namespace cad {

class DXFWriter {
public:
    DXFWriter() = default;

    /**
     * @brief DXF dosyasına yaz
     * @param filePath    Çıktı dosya yolu (.dxf)
     * @param entities    CAD arka plan entity'leri
     * @param network     MEP tesisat şebekesi
     * @param projectName Başlık bloğuna yazılacak proje adı
     * @param registry    Blok tanımları (nullptr ise boş BLOCKS section)
     * @return Başarılı ise true
     */
    bool Write(const std::string& filePath,
               const std::vector<std::unique_ptr<Entity>>& entities,
               const mep::NetworkGraph& network,
               const std::string& projectName = "VKT Projesi",
               const BlockRegistry* registry = nullptr,
               const std::unordered_map<std::string, Layer>* layerMap = nullptr) const;

    /**
     * @brief Sadece MEP şebekesini DXF'e yaz (CAD arka plan olmadan)
     */
    bool WriteNetwork(const std::string& filePath,
                      const mep::NetworkGraph& network) const;

private:
    static void WriteGroup(std::ostream& out, int code, const std::string& value);
    static void WriteGroup(std::ostream& out, int code, double value);
    static void WriteGroup(std::ostream& out, int code, int value);

    void WriteHeader(std::ostream& out, const std::string& projectName) const;
    void WriteTables(std::ostream& out,
                     const std::vector<std::unique_ptr<Entity>>& entities,
                     const BlockRegistry* registry,
                     const std::unordered_map<std::string, Layer>* layerMap = nullptr) const;
    void WriteBlocksSection(std::ostream& out, const BlockRegistry* registry) const;
    void WriteBlockEntities(std::ostream& out,
                            const std::vector<std::unique_ptr<Entity>>& blockEntities) const;
    void WriteEntitiesSection(std::ostream& out,
                              const std::vector<std::unique_ptr<Entity>>& entities,
                              const mep::NetworkGraph& network) const;

    void WriteEntityLine(std::ostream& out, const Entity& e) const;
    void WriteEntityPolyline(std::ostream& out, const Entity& e) const;
    void WriteEntityArc(std::ostream& out, const Entity& e) const;
    void WriteEntityCircle(std::ostream& out, const Entity& e) const;
    void WriteEntityEllipse(std::ostream& out, const Entity& e) const;
    void WriteEntityText(std::ostream& out, const Entity& e) const;
    void WriteEntityInsert(std::ostream& out, const Entity& e) const;
    void WriteEntityDimension(std::ostream& out, const Entity& e) const;
    void WriteEntityLeader(std::ostream& out, const Entity& e) const;
    void WriteEntityHatch(std::ostream& out, const Entity& e) const;
    void WriteEntitySpline(std::ostream& out, const Entity& e) const;

    void WriteNetworkEdges(std::ostream& out, const mep::NetworkGraph& net) const;
    void WriteNetworkNodes(std::ostream& out, const mep::NetworkGraph& net) const;

    static int ColorToACI(int r, int g, int b);
};

} // namespace cad
} // namespace vkt
