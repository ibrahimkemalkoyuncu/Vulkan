/**
 * @file test_export.cpp
 * @brief RTF export yardimci algoritmasi birim testleri
 *
 * OnWordRapor() icerisindeki RTF Unicode escape lambda'sini ayni mantikla
 * test eder. Qt bagimliligi olmadan saf C++ ile calisir.
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
