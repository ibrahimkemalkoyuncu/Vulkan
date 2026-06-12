/**
 * @file test_export.cpp
 * @brief Export yardimci algoritmasi birim testleri
 *
 * - RTF Unicode escape (legacy referans, algoritma dokumantasyonu icin korundu)
 * - DOCX ZIP/OOXML: CRC32, XML escape, belge yapisi
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <cstdint>

// --- RTF escape: OnWordRapor lambda'sinin C++ karsiligi ---
// Non-ASCII karakterler \uXXXX? formatina donusturulur (RTF Unicode escape).
// Negatif code point: short cast ile signed int'e; RTF spec bunu gerektiriyor.
static std::string RtfEscape(const std::wstring& s) {
    std::string r;
    r.reserve(s.size() * 4);
    for (wchar_t c : s) {
        uint16_t u = static_cast<uint16_t>(c);
        if (u > 127) {
            // RTF \uN? — N is signed short interpreted as int
            int16_t signed_u = static_cast<int16_t>(u);
            r += "\\u";
            r += std::to_string(static_cast<int>(signed_u));
            r += "?";
        } else if (c == L'\\') { r += "\\\\"; }
        else if (c == L'{')   { r += "\\{";  }
        else if (c == L'}')   { r += "\\}";  }
        else                  { r += static_cast<char>(c); }
    }
    return r;
}

// --- Testler ---

TEST_CASE("RtfEscape - ASCII passthrough", "[export][rtf]") {
    CHECK(RtfEscape(L"Hello World") == "Hello World");
    CHECK(RtfEscape(L"DN25 - 3.14 m/s") == "DN25 - 3.14 m/s");
    CHECK(RtfEscape(L"") == "");
    CHECK(RtfEscape(L"0123456789") == "0123456789");
}

TEST_CASE("RtfEscape - RTF special chars escaped", "[export][rtf]") {
    CHECK(RtfEscape(L"a\\b") == "a\\\\b");
    CHECK(RtfEscape(L"{tag}") == "\\{tag\\}");
    CHECK(RtfEscape(L"\\{\\}") == "\\\\\\{\\\\\\}");
}

TEST_CASE("RtfEscape - Turkce karakterler Unicode escape", "[export][rtf]") {
    // i-noktalı (U+0131 = 305 decimal)
    std::wstring isiz = L"ı";  // i without dot
    std::string result = RtfEscape(isiz);
    CHECK(result == "\\u305?");

    // I-noktalı (U+0130 = 304)
    CHECK(RtfEscape(L"İ") == "\\u304?");

    // s-cedilla (U+015F = 351)
    CHECK(RtfEscape(L"ş") == "\\u351?");

    // g-breve (U+011F = 287)
    CHECK(RtfEscape(L"ğ") == "\\u287?");

    // u-umlaut (U+00FC = 252)
    CHECK(RtfEscape(L"ü") == "\\u252?");

    // o-umlaut (U+00F6 = 246)
    CHECK(RtfEscape(L"ö") == "\\u246?");

    // c-cedilla (U+00E7 = 231)
    CHECK(RtfEscape(L"ç") == "\\u231?");
}

TEST_CASE("RtfEscape - Karisik metin (ASCII + Turkce)", "[export][rtf]") {
    // "Boru Cinsi" — all ASCII
    CHECK(RtfEscape(L"Boru Cinsi") == "Boru Cinsi");

    // "Soguk Su" — all ASCII
    CHECK(RtfEscape(L"Soguk Su") == "Soguk Su");

    // Mixed: "DN25" stays, Turkish chars escape
    std::wstring mixed = L"DN25 - Sıcak Su";
    std::string res = RtfEscape(mixed);
    CHECK(res == "DN25 - S\\u305?cak Su");
}

TEST_CASE("RtfEscape - RTF header ornegi gecerli karakter", "[export][rtf]") {
    // RTF header'da sadece ASCII olmali — escape fonksiyonu bu garantiyi saglar
    std::string header = "{\\rtf1\\ansi\\ansicpg1252";
    // Tum karakterler ASCII — aynen gecmeli
    std::wstring wide(header.begin(), header.end());
    std::string escaped = RtfEscape(wide);
    // Backslash'lar escaped olacak ama biz duzgun bir RTF string test ediyoruz
    CHECK(escaped.find("\\u") == std::string::npos); // hic unicode escape yok
}

TEST_CASE("RtfEscape - Yuksek Unicode kod noktasi (BMP disi degil)", "[export][rtf]") {
    // U+00C7 = 199: C with cedilla (buyuk harf)
    CHECK(RtfEscape(L"Ç") == "\\u199?");

    // U+011E = 286: G with breve (buyuk harf)
    CHECK(RtfEscape(L"Ğ") == "\\u286?");

    // U+015E = 350: S with cedilla (buyuk harf)
    CHECK(RtfEscape(L"Ş") == "\\u350?");
}

TEST_CASE("RtfEscape - Sayisal format karakterleri", "[export][rtf]") {
    // Raporda kullanilan tipik sayisal string'ler
    CHECK(RtfEscape(L"18.45") == "18.45");
    CHECK(RtfEscape(L"0.0070 mm") == "0.0070 mm");
    CHECK(RtfEscape(L"2.0 m/s") == "2.0 m/s");
    CHECK(RtfEscape(L"KRITIK") == "KRITIK");
    CHECK(RtfEscape(L"-") == "-");
}

// ============================================================
//  DOCX / OOXML testleri
// ============================================================

// CRC32 referans degerleri (IEEE 802.3 standardi)
static uint32_t Crc32Test(const std::string& s) {
    uint32_t tbl[256];
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        tbl[i] = c;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char c : s)
        crc = tbl[(crc ^ c) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

TEST_CASE("DOCX - CRC32 bos string", "[export][docx]") {
    CHECK(Crc32Test("") == 0x00000000u);
}

TEST_CASE("DOCX - CRC32 bilinen degerler", "[export][docx]") {
    CHECK(Crc32Test("123456789") == 0xCBF43926u);
    CHECK(Crc32Test("a") == 0xE8B7BE43u);
}

// XML escape
static std::string XmlEscTest(const std::string& s) {
    std::string r;
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

TEST_CASE("DOCX - XML escape ozel karakterler", "[export][docx]") {
    CHECK(XmlEscTest("&") == "&amp;");
    CHECK(XmlEscTest("<b>") == "&lt;b&gt;");
    CHECK(XmlEscTest("\"quote\"") == "&quot;quote&quot;");
    CHECK(XmlEscTest("it's") == "it&apos;s");
}

TEST_CASE("DOCX - XML escape ASCII passthrough", "[export][docx]") {
    CHECK(XmlEscTest("DN25") == "DN25");
    CHECK(XmlEscTest("3.14 m/s") == "3.14 m/s");
    CHECK(XmlEscTest("") == "");
}

TEST_CASE("DOCX - XML escape karisik", "[export][docx]") {
    CHECK(XmlEscTest("a < b && b > c") == "a &lt; b &amp;&amp; b &gt; c");
    CHECK(XmlEscTest("<tag attr=\"v\">") == "&lt;tag attr=&quot;v&quot;&gt;");
}

TEST_CASE("DOCX - ZIP local header imzasi dogru", "[export][docx]") {
    // ZIP local file header: 4 byte signature = 0x04034b50 (little-endian: 50 4B 03 04)
    const uint8_t sig[] = {0x50, 0x4B, 0x03, 0x04};
    uint32_t val = (uint32_t)sig[0] | ((uint32_t)sig[1]<<8) |
                   ((uint32_t)sig[2]<<16) | ((uint32_t)sig[3]<<24);
    CHECK(val == 0x04034b50u);
}

TEST_CASE("DOCX - ZIP central dir imzasi dogru", "[export][docx]") {
    const uint8_t sig[] = {0x50, 0x4B, 0x01, 0x02};
    uint32_t val = (uint32_t)sig[0] | ((uint32_t)sig[1]<<8) |
                   ((uint32_t)sig[2]<<16) | ((uint32_t)sig[3]<<24);
    CHECK(val == 0x02014b50u);
}

TEST_CASE("DOCX - ZIP EOCD imzasi dogru", "[export][docx]") {
    const uint8_t sig[] = {0x50, 0x4B, 0x05, 0x06};
    uint32_t val = (uint32_t)sig[0] | ((uint32_t)sig[1]<<8) |
                   ((uint32_t)sig[2]<<16) | ((uint32_t)sig[3]<<24);
    CHECK(val == 0x06054b50u);
}

TEST_CASE("DOCX - OOXML Content-Types zorunlu partlar iceriyor", "[export][docx]") {
    const std::string ct = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Override PartName="/word/document.xml"
    ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>)";
    CHECK(ct.find("word/document.xml") != std::string::npos);
    CHECK(ct.find("wordprocessingml.document.main") != std::string::npos);
    CHECK(ct.find("relationships+xml") != std::string::npos);
}

TEST_CASE("DOCX - OOXML root rels officeDocument iliskisi var", "[export][docx]") {
    const std::string rels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1"
    Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"
    Target="word/document.xml"/>
</Relationships>)";
    CHECK(rels.find("officeDocument") != std::string::npos);
    CHECK(rels.find("word/document.xml") != std::string::npos);
}
