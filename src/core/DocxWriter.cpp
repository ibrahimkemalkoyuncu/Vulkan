/**
 * @file DocxWriter.cpp
 * @brief OOXML .docx tesisat raporu — ZIP stored (sıfır harici bağımlılık)
 *
 * ZIP yapısı: local header + stored data + central directory + EOCD.
 * CRC32 için kendi lookup tablosu; zlib/minizip gerekmez.
 */
#include "../../vkt/core/DocxWriter.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <set>
#include <cmath>

namespace vkt::core {

// ============================================================
// CRC32 — ISO 3309 / ZIP standard
// ============================================================

// CRC32 lookup table, initialised at startup
static uint32_t s_crc32Table[256];
static bool s_crcInit = false;

static void InitCrc32() {
    if (s_crcInit) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        s_crc32Table[i] = c;
    }
    s_crcInit = true;
}

static uint32_t Crc32(const uint8_t* data, size_t len) {
    InitCrc32();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = s_crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ============================================================
// ZIP Stored Writer
// ============================================================
using Bytes = std::vector<uint8_t>;

static void PutU16(Bytes& b, uint16_t v) {
    b.push_back(v & 0xFF);
    b.push_back((v >> 8) & 0xFF);
}
static void PutU32(Bytes& b, uint32_t v) {
    b.push_back(v & 0xFF);
    b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF);
    b.push_back((v >> 24) & 0xFF);
}
static void PutStr(Bytes& b, const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
}
static void PutBytes(Bytes& b, const uint8_t* data, size_t len) {
    b.insert(b.end(), data, data + len);
}

struct ZipEntry {
    std::string name;
    uint32_t    crc32;
    uint32_t    size;
    uint32_t    localOffset;
};

static void ZipAddFile(Bytes& zip, std::vector<ZipEntry>& entries,
                       const std::string& name, const std::string& content)
{
    const auto* data = reinterpret_cast<const uint8_t*>(content.data());
    uint32_t sz  = static_cast<uint32_t>(content.size());
    uint32_t crc = Crc32(data, sz);

    ZipEntry e;
    e.name        = name;
    e.crc32       = crc;
    e.size        = sz;
    e.localOffset = static_cast<uint32_t>(zip.size());
    entries.push_back(e);

    // Local file header
    PutU32(zip, 0x04034b50u); // signature
    PutU16(zip, 20);           // version needed (2.0)
    PutU16(zip, 0);            // flags
    PutU16(zip, 0);            // method: stored
    PutU16(zip, 0);            // mod time
    PutU16(zip, 0);            // mod date
    PutU32(zip, crc);
    PutU32(zip, sz);           // compressed = uncompressed
    PutU32(zip, sz);
    PutU16(zip, static_cast<uint16_t>(name.size()));
    PutU16(zip, 0);            // extra field length
    PutStr(zip, name);
    PutBytes(zip, data, sz);
}

static Bytes ZipFinalize(Bytes& zip, const std::vector<ZipEntry>& entries) {
    uint32_t cdOffset = static_cast<uint32_t>(zip.size());
    for (const auto& e : entries) {
        PutU32(zip, 0x02014b50u); // central dir signature
        PutU16(zip, 20);           // version made by
        PutU16(zip, 20);           // version needed
        PutU16(zip, 0);            // flags
        PutU16(zip, 0);            // method: stored
        PutU16(zip, 0);            // mod time
        PutU16(zip, 0);            // mod date
        PutU32(zip, e.crc32);
        PutU32(zip, e.size);
        PutU32(zip, e.size);
        PutU16(zip, static_cast<uint16_t>(e.name.size()));
        PutU16(zip, 0); // extra
        PutU16(zip, 0); // comment
        PutU16(zip, 0); // disk start
        PutU16(zip, 0); // internal attrs
        PutU32(zip, 0); // external attrs
        PutU32(zip, e.localOffset);
        PutStr(zip, e.name);
    }
    uint32_t cdSize = static_cast<uint32_t>(zip.size()) - cdOffset;
    // EOCD
    PutU32(zip, 0x06054b50u);
    PutU16(zip, 0);
    PutU16(zip, 0);
    PutU16(zip, static_cast<uint16_t>(entries.size()));
    PutU16(zip, static_cast<uint16_t>(entries.size()));
    PutU32(zip, cdSize);
    PutU32(zip, cdOffset);
    PutU16(zip, 0); // comment length
    return zip;
}

// ============================================================
// XML helpers
// ============================================================
static std::string XmlEsc(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '&':  r += "&amp;";  break;
            case '<':  r += "&lt;";   break;
            case '>':  r += "&gt;";   break;
            case '"':  r += "&quot;"; break;
            case '\'': r += "&apos;"; break;
            default:   r += c;        break;
        }
    }
    return r;
}

static std::string Fmt(double v, int dec = 2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", dec, v);
    return buf;
}

// ============================================================
// OOXML parts
// ============================================================
static std::string ContentTypes() {
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml"  ContentType="application/xml"/>
  <Override PartName="/word/document.xml"
    ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
  <Override PartName="/word/styles.xml"
    ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>
</Types>)";
}

static std::string RelsRoot() {
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1"
    Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"
    Target="word/document.xml"/>
</Relationships>)";
}

static std::string RelsDocument() {
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1"
    Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles"
    Target="styles.xml"/>
</Relationships>)";
}

static std::string Styles() {
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:style w:type="paragraph" w:styleId="Normal" w:default="1">
    <w:name w:val="Normal"/>
    <w:rPr><w:sz w:val="22"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading1">
    <w:name w:val="heading 1"/>
    <w:rPr><w:b/><w:sz w:val="28"/><w:color w:val="1F5599"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading2">
    <w:name w:val="heading 2"/>
    <w:rPr><w:b/><w:sz w:val="24"/><w:color w:val="1F5599"/></w:rPr>
  </w:style>
  <w:style w:type="table" w:styleId="TableGrid">
    <w:name w:val="Table Grid"/>
    <w:tblPr>
      <w:tblBorders>
        <w:top    w:val="single" w:sz="4" w:space="0" w:color="AAAAAA"/>
        <w:left   w:val="single" w:sz="4" w:space="0" w:color="AAAAAA"/>
        <w:bottom w:val="single" w:sz="4" w:space="0" w:color="AAAAAA"/>
        <w:right  w:val="single" w:sz="4" w:space="0" w:color="AAAAAA"/>
        <w:insideH w:val="single" w:sz="4" w:space="0" w:color="AAAAAA"/>
        <w:insideV w:val="single" w:sz="4" w:space="0" w:color="AAAAAA"/>
      </w:tblBorders>
    </w:tblPr>
  </w:style>
</w:styles>)";
}

// OOXML paragraph helpers
static std::string Para(const std::string& text,
                        const std::string& styleId = "Normal",
                        bool bold = false,
                        const std::string& colorHex = "") {
    std::string rpr;
    if (bold)           rpr += "<w:b/>";
    if (!colorHex.empty()) rpr += "<w:color w:val=\"" + colorHex + "\"/>";
    if (styleId == "Heading1") { rpr += "<w:sz w:val=\"28\"/>"; }
    if (styleId == "Heading2") { rpr += "<w:sz w:val=\"24\"/>"; }

    std::string ppr = styleId.empty() ? "" :
        "<w:pPr><w:pStyle w:val=\"" + styleId + "\"/></w:pPr>";

    return "<w:p>" + ppr +
           "<w:r>" +
           (rpr.empty() ? "" : "<w:rPr>" + rpr + "</w:rPr>") +
           "<w:t xml:space=\"preserve\">" + XmlEsc(text) + "</w:t>" +
           "</w:r></w:p>\n";
}

static std::string EmptyPara() { return "<w:p/>\n"; }

// Table cell
static std::string Cell(const std::string& text,
                        int widthTwips,
                        bool header = false,
                        bool critical = false) {
    std::string shd = header
        ? "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"1F5599\"/>"
        : (critical ? "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"FFE0E0\"/>" : "");
    std::string textColor = header ? "<w:color w:val=\"FFFFFF\"/>" :
                            (critical ? "<w:color w:val=\"AA0000\"/>" : "");
    std::string bold = (header || critical) ? "<w:b/>" : "";
    std::string sz   = "<w:sz w:val=\"18\"/>";

    return "<w:tc>"
           "<w:tcPr>"
           "<w:tcW w:w=\"" + std::to_string(widthTwips) + "\" w:type=\"dxa\"/>" +
           shd +
           "</w:tcPr>"
           "<w:p><w:r>"
           "<w:rPr>" + bold + textColor + sz + "</w:rPr>"
           "<w:t xml:space=\"preserve\">" + XmlEsc(text) + "</w:t>"
           "</w:r></w:p>"
           "</w:tc>\n";
}

using RowList = std::vector<std::vector<std::string>>;

// Table with column widths; rows = vector of {cells}
static std::string Table(std::initializer_list<int> colWidths,
                         RowList rows,
                         int headerRows = 1) {
    std::vector<int> colW(colWidths);
    int totalW = 0;
    for (int w : colW) totalW += w;

    std::string tbl = "<w:tbl>\n"
        "<w:tblPr>"
        "<w:tblStyle w:val=\"TableGrid\"/>"
        "<w:tblW w:w=\"" + std::to_string(totalW) + "\" w:type=\"dxa\"/>"
        "<w:tblBorders>"
        "<w:top    w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"AAAAAA\"/>"
        "<w:left   w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"AAAAAA\"/>"
        "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"AAAAAA\"/>"
        "<w:right  w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"AAAAAA\"/>"
        "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"AAAAAA\"/>"
        "<w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"AAAAAA\"/>"
        "</w:tblBorders>"
        "</w:tblPr>\n";

    for (int ri = 0; ri < (int)rows.size(); ++ri) {
        bool isHeader = ri < headerRows;
        tbl += "<w:tr>\n";
        for (int ci = 0; ci < (int)rows[ri].size(); ++ci) {
            int w = ci < (int)colW.size() ? colW[ci] : 1440;
            // critical row: rows with last cell == "KRITIK"
            bool isCrit = !isHeader &&
                          !rows[ri].empty() &&
                          rows[ri].back() == "KRITIK" && ci == (int)rows[ri].size()-1;
            tbl += Cell(rows[ri][ci], w, isHeader, isCrit);
        }
        tbl += "</w:tr>\n";
    }
    tbl += "</w:tbl>\n";
    return tbl;
}

// ============================================================
// Document XML builder
// ============================================================
static std::string BuildDocument(const mep::NetworkGraph& network,
                                 const mep::CriticalPathResult& crit,
                                 const DocxReportParams& p)
{
    std::string body;

    // Title
    body += Para(p.projectName.empty() ? "VKT Tesisat Projesi" : p.projectName,
                 "Heading1", true, "1F5599");
    body += Para(p.date + "  |  Norm: " + p.norm + "  |  Malzeme: " + p.mainMaterial,
                 "Normal", false, "666666");
    body += EmptyPara();

    // 1. Devre Parametreleri
    body += Para("1. Devre Parametreleri", "Heading2", true, "1F5599");
    body += Table(
        {3600, 5400},
        {
            {"Parametre", "Deger"},
            {"Bina Tipi",          p.buildingType},
            {"Ana Boru Cinsi",     p.mainMaterial},
            {"Ikincil Boru Cinsi", p.branchMaterial},
            {"Boru Puruzlulugu",   Fmt(p.roughness_mm, 4) + " mm"},
            {"Maks. Su Hizi",      Fmt(p.maxVelocity_ms, 1) + " m/s"},
            {"Hesap Normu",        p.norm}
        }
    );
    body += EmptyPara();

    // 2. Boru Hesap Föyü
    body += Para("2. Boru Hesap Foyu", "Heading2", true, "1F5599");
    std::set<uint32_t> critSet(crit.criticalPath.begin(), crit.criticalPath.end());
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Boru No","Tip","DN","L (m)","Q (L/s)","v (m/s)","DH (m)","Durum"});
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type == mep::EdgeType::Drainage || edge.type == mep::EdgeType::Vent) continue;
        std::string lbl = edge.label.empty()
            ? "E" + std::to_string(eid) : edge.label;
        std::string tip = (edge.type == mep::EdgeType::HotWater) ? "Sicak" : "Soguk";
        bool isCrit = critSet.count(eid) > 0;
        rows.push_back({
            lbl, tip,
            std::to_string(static_cast<int>(std::round(edge.diameter_mm))),
            Fmt(edge.length_m),
            Fmt(edge.flowRate_m3s * 1000.0, 3),
            Fmt(edge.velocity_ms),
            Fmt(edge.headLoss_m, 4),
            isCrit ? "KRITIK" : "-"
        });
    }
    body += Table({900,900,700,900,1000,900,1000,1200}, rows);
    body += EmptyPara();

    // 3. Kritik Devre Özeti
    body += Para("3. Kritik Devre ve Hidrofor", "Heading2", true, "1F5599");
    body += Table(
        {4500, 4500},
        {
            {"Parametre", "Deger"},
            {"Kritik Hat Toplam Kayip",         Fmt(crit.totalHeadLoss_m) + " m"},
            {"Gerekli Pompa Basma Yuksekligi",  Fmt(crit.requiredPumpHead_m) + " m"}
        }
    );
    body += EmptyPara();

    // 4. Armatür Listesi
    body += Para("4. Armatur Listesi", "Heading2", true, "1F5599");
    std::vector<std::vector<std::string>> nodeRows;
    nodeRows.push_back({"ID","Tip","LU","Debi (L/s)"});
    for (const auto& [nid, node] : network.GetNodeMap()) {
        if (node.type != mep::NodeType::Fixture) continue;
        nodeRows.push_back({
            std::to_string(nid),
            node.fixtureType,
            Fmt(node.loadUnit, 1),
            Fmt(node.flowRate_m3s * 1000.0, 3)
        });
    }
    body += Table({1800,2700,1800,2700}, nodeRows);
    body += EmptyPara();

    // Footer
    body += Para("VKT v1.0.0 - " + p.date + " - " + p.norm + " uyumlu",
                 "Normal", false, "AAAAAA");

    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"\n"
           "            xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
           "<w:body>\n" + body + "</w:body>\n</w:document>\n";
}

// ============================================================
// DocxWriter::Write
// ============================================================
bool DocxWriter::Write(const std::string& filePath,
                       const mep::NetworkGraph& network,
                       const mep::CriticalPathResult& crit,
                       const DocxReportParams& params)
{
    Bytes zip;
    zip.reserve(64 * 1024);
    std::vector<ZipEntry> entries;

    ZipAddFile(zip, entries, "[Content_Types].xml",        ContentTypes());
    ZipAddFile(zip, entries, "_rels/.rels",                RelsRoot());
    ZipAddFile(zip, entries, "word/_rels/document.xml.rels", RelsDocument());
    ZipAddFile(zip, entries, "word/styles.xml",            Styles());
    ZipAddFile(zip, entries, "word/document.xml",
               BuildDocument(network, crit, params));

    ZipFinalize(zip, entries);

    std::ofstream f(filePath, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(zip.data()),
            static_cast<std::streamsize>(zip.size()));
    return f.good();
}

} // namespace vkt::core
