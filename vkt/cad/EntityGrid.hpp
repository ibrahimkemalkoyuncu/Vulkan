#pragma once
/**
 * @file EntityGrid.hpp
 * @brief Düzgün ızgara (uniform grid) tabanlı mekansal indeks — O(1) entity pick.
 *
 * 15K+ entity'li DXF projelerinde ham O(n) pick yerine:
 *   - Import/rebuild sonrası Build() çağrısı ile ızgara oluşturulur
 *   - QueryNear(worldX, worldY, radius) yalnızca yakın hücreleri döndürür
 *
 * Implementasyon: sabit hücre boyutlu 2D ızgara (bucket grid).
 */

#include <vector>
#include <unordered_map>
#include <cmath>
#include "cad/Entity.hpp"
#include "geom/Math.hpp"

namespace vkt {
namespace cad {

class EntityGrid {
public:
    EntityGrid() = default;

    /// Tüm entity'leri indeksle. cadEntities ömrü boyunca geçerli pointer'lar tutulur.
    void Build(const std::vector<std::unique_ptr<Entity>>& entities, double cellSizeMM = 2000.0) {
        m_cells.clear();
        m_cellSize = cellSizeMM > 0 ? cellSizeMM : 2000.0;
        for (const auto& e : entities) {
            if (!e || !e->IsVisible()) continue;
            auto bb = e->GetBounds();
            int x0 = cellIdx(bb.min.x), y0 = cellIdx(bb.min.y);
            int x1 = cellIdx(bb.max.x), y1 = cellIdx(bb.max.y);
            for (int xi = x0; xi <= x1; ++xi)
                for (int yi = y0; yi <= y1; ++yi)
                    m_cells[key(xi, yi)].push_back(e.get());
        }
    }

    /// Verilen dünya noktasına en fazla radiusMM mesafedeki entity'leri döndürür.
    std::vector<Entity*> QueryNear(double wx, double wy, double radiusMM) const {
        std::vector<Entity*> result;
        int r  = static_cast<int>(std::ceil(radiusMM / m_cellSize));
        int cx = cellIdx(wx), cy = cellIdx(wy);
        for (int xi = cx - r; xi <= cx + r; ++xi) {
            for (int yi = cy - r; yi <= cy + r; ++yi) {
                auto it = m_cells.find(key(xi, yi));
                if (it == m_cells.end()) continue;
                for (Entity* e : it->second) result.push_back(e);
            }
        }
        // Tekrarları kaldır (entity birden fazla hücreye girebilir)
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    bool IsBuilt() const { return !m_cells.empty(); }
    void Clear()         { m_cells.clear(); }

private:
    int    cellIdx(double v) const { return static_cast<int>(std::floor(v / m_cellSize)); }
    size_t key(int x, int y) const {
        // Cantor pairing — 64-bit key
        uint32_t ux = static_cast<uint32_t>(x + 100000);
        uint32_t uy = static_cast<uint32_t>(y + 100000);
        return (static_cast<size_t>(ux) << 32) | uy;
    }

    double m_cellSize = 2000.0;
    std::unordered_map<size_t, std::vector<Entity*>> m_cells;
};

} // namespace cad
} // namespace vkt
