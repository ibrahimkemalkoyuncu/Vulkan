/**
 * @file DWGWriter.hpp
 * @brief DWG-compatible DXF R2000 writer
 *
 * Since writing native DWG format requires LibreDWG write API (not available),
 * this module writes DXF R2000 format. AutoCAD can open DXF R2000 files natively.
 * The Write() method auto-detects .dwg extension and falls back to DXF compat.
 */

#pragma once

#include "Entity.hpp"
#include "Block.hpp"
#include "Layer.hpp"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <memory>

namespace vkt {
namespace cad {

class DWGWriter {
public:
    /**
     * @brief Write entities to file
     * @param filepath Output file path (.dwg or .dxf)
     * @param entities CAD entities to write
     * @param layers Layer definitions with colors
     * @param registry Optional block registry
     * @return true on success
     *
     * If filepath ends with .dwg, writes DXF R2000 format (AutoCAD compatible).
     */
    static bool Write(const std::string& filepath,
                      const std::vector<std::unique_ptr<Entity>>& entities,
                      const std::map<std::string, Layer>& layers,
                      const BlockRegistry* registry = nullptr);

    /**
     * @brief Write DXF R2000 format (always, regardless of extension)
     */
    static bool WriteDxfCompat(const std::string& filepath,
                                const std::vector<std::unique_ptr<Entity>>& entities,
                                const std::map<std::string, Layer>& layers);

private:
    static void WriteHeader(std::ofstream& f, const std::map<std::string, Layer>& layers);
    static void WriteTables(std::ofstream& f, const std::map<std::string, Layer>& layers);
    static void WriteGroup(std::ofstream& f, int code, const std::string& value);
    static void WriteGroup(std::ofstream& f, int code, double value);
    static void WriteGroup(std::ofstream& f, int code, int value);
    static int  ColorToACI(int r, int g, int b);
};

} // namespace cad
} // namespace vkt
