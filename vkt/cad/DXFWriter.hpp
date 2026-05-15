/**
 * @file DXFWriter.hpp
 * @brief AutoCAD uyumlu ASCII DXF dosya yazıcısı
 *
 * Desteklenen entity'ler: LINE, LWPOLYLINE, CIRCLE, TEXT
 * NetworkGraph boru/node geometrisi + DXF background entity'leri
 *
 * Format: DXF R2000 (AC1015) — AutoCAD 2000+ ve tüm CAD yazılımları okur
 */

#pragma once

#include "Entity.hpp"
#include "mep/NetworkGraph.hpp"
#include <string>
#include <vector>

namespace vkt {
namespace cad {

class DXFWriter {
public:
    DXFWriter() = default;

    /**
     * @brief DXF dosyasına yaz
     * @param filePath  Çıktı dosya yolu (.dxf)
     * @param entities  CAD arka plan entity'leri (DXF'ten okunmuş çizim)
     * @param network   MEP tesisat şebekesi (borular + node'lar)
     * @param projectName Başlık bloğuna yazılacak proje adı
     * @return Başarılı ise true
     */
    bool Write(const std::string& filePath,
               const std::vector<std::unique_ptr<Entity>>& entities,
               const mep::NetworkGraph& network,
               const std::string& projectName = "VKT Projesi") const;

    /**
     * @brief Sadece MEP şebekesini DXF'e yaz (CAD arka plan olmadan)
     */
    bool WriteNetwork(const std::string& filePath,
                      const mep::NetworkGraph& network) const;

private:
    // DXF grup kodu yazan yardımcılar
    static void WriteGroup(std::ostream& out, int code, const std::string& value);
    static void WriteGroup(std::ostream& out, int code, double value);
    static void WriteGroup(std::ostream& out, int code, int value);

    // Bölüm yazıcıları
    void WriteHeader(std::ostream& out, const std::string& projectName) const;
    void WriteTables(std::ostream& out,
                     const std::vector<std::unique_ptr<Entity>>& entities) const;
    void WriteEntitiesSection(std::ostream& out,
                              const std::vector<std::unique_ptr<Entity>>& entities,
                              const mep::NetworkGraph& network) const;

    // Entity yazıcıları
    void WriteEntityLine(std::ostream& out, const Entity& e) const;
    void WriteEntityPolyline(std::ostream& out, const Entity& e) const;
    void WriteEntityArc(std::ostream& out, const Entity& e) const;
    void WriteEntityCircle(std::ostream& out, const Entity& e) const;
    void WriteEntityEllipse(std::ostream& out, const Entity& e) const;
    void WriteEntityText(std::ostream& out, const Entity& e) const;

    // MEP network yazıcıları
    void WriteNetworkEdges(std::ostream& out, const mep::NetworkGraph& net) const;
    void WriteNetworkNodes(std::ostream& out, const mep::NetworkGraph& net) const;

    // Renk: entity rengi → ACI (AutoCAD Color Index)
    static int ColorToACI(int r, int g, int b);
};

} // namespace cad
} // namespace vkt
