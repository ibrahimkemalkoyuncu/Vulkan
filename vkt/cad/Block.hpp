#pragma once
/**
 * @file Block.hpp
 * @brief CAD Blok tanımı ve Insert (blok örneği) sınıfları.
 *
 * Block: isimli entity koleksiyonu — yeniden kullanılabilir sembol.
 * Insert: belirli koordinat/ölçek/dönüş ile yerleştirilen blok örneği.
 *
 * DXF'te BLOCK/ENDBLK section; DWG'de BLOCK_HEADER entity'sine karşılık gelir.
 */

#include "cad/Entity.hpp"
#include "geom/Math.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace vkt {
namespace cad {

/// Blok tanımı: isimli entity koleksiyonu
struct BlockDef {
    std::string name;               ///< Blok adı (örn. "LAVABO", "WC")
    geom::Vec3  basePoint{};        ///< Baz nokta (INSERT yerleştirme referansı)
    std::vector<std::unique_ptr<Entity>> entities; ///< İçerik entity'leri

    BlockDef() = default;
    BlockDef(const BlockDef&) = delete;
    BlockDef& operator=(const BlockDef&) = delete;
    BlockDef(BlockDef&&) = default;
    BlockDef& operator=(BlockDef&&) = default;
};

/// Blok kaydı (block name → BlockDef)
class BlockRegistry {
public:
    BlockRegistry() = default;

    void AddBlock(BlockDef def) {
        m_blocks[def.name] = std::move(def);
    }

    const BlockDef* GetBlock(const std::string& name) const {
        auto it = m_blocks.find(name);
        return (it != m_blocks.end()) ? &it->second : nullptr;
    }

    bool HasBlock(const std::string& name) const {
        return m_blocks.count(name) > 0;
    }

    std::vector<std::string> GetBlockNames() const {
        std::vector<std::string> names;
        names.reserve(m_blocks.size());
        for (const auto& [k, v] : m_blocks) names.push_back(k);
        return names;
    }

    void Clear() { m_blocks.clear(); }
    size_t Size() const { return m_blocks.size(); }

private:
    std::unordered_map<std::string, BlockDef> m_blocks;
};

/// Insert entity — bir bloğun belirli transform ile yerleştirilmiş örneği
class Insert : public Entity {
public:
    Insert(const std::string& blockName,
           const geom::Vec3& insertPt,
           double scaleX = 1.0, double scaleY = 1.0,
           double rotationDeg = 0.0)
        : m_blockName(blockName)
        , m_insertPt(insertPt)
        , m_scaleX(scaleX), m_scaleY(scaleY)
        , m_rotDeg(rotationDeg)
    {}

    EntityType GetType() const override { return EntityType::Insert; }

    std::unique_ptr<Entity> Clone() const override {
        return std::make_unique<Insert>(m_blockName, m_insertPt,
                                        m_scaleX, m_scaleY, m_rotDeg);
    }

    BoundingBox GetBounds() const override {
        BoundingBox bb;
        bb.min = m_insertPt;
        bb.max = m_insertPt;
        return bb;
    }

    // Transform
    void Move(const geom::Vec3& delta) override {
        m_insertPt.x += delta.x;
        m_insertPt.y += delta.y;
        m_insertPt.z += delta.z;
    }
    void Scale(const geom::Vec3& s) override {
        m_scaleX *= s.x; m_scaleY *= s.y;
    }
    void Rotate(double angleDeg) override { m_rotDeg += angleDeg; }
    void Mirror(const geom::Vec3&, const geom::Vec3&) override { m_scaleX = -m_scaleX; }

    // Serialization
    void Serialize(nlohmann::json& j) const override {
        j["blockName"] = m_blockName;
        j["insertPt"]  = {m_insertPt.x, m_insertPt.y, m_insertPt.z};
        j["scaleX"]    = m_scaleX; j["scaleY"] = m_scaleY;
        j["rotDeg"]    = m_rotDeg;
    }
    void Deserialize(const nlohmann::json& j) override {
        m_blockName = j.value("blockName", "");
        auto ip = j.value("insertPt", std::vector<double>{0,0,0});
        m_insertPt = {ip[0], ip[1], ip[2]};
        m_scaleX   = j.value("scaleX", 1.0);
        m_scaleY   = j.value("scaleY", 1.0);
        m_rotDeg   = j.value("rotDeg", 0.0);
    }

    // Getters
    const std::string& GetBlockName()  const { return m_blockName; }
    const geom::Vec3&  GetInsertPt()   const { return m_insertPt; }
    double             GetScaleX()     const { return m_scaleX; }
    double             GetScaleY()     const { return m_scaleY; }
    double             GetRotDeg()     const { return m_rotDeg; }

private:
    std::string m_blockName;
    geom::Vec3  m_insertPt{};
    double      m_scaleX = 1.0, m_scaleY = 1.0;
    double      m_rotDeg = 0.0;
};

} // namespace cad
} // namespace vkt
