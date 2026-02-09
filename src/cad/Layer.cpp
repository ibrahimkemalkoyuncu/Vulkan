/**
 * @file Layer.cpp
 * @brief Layer sınıfının implementasyonu
 */

#include "cad/Layer.hpp"
#include <algorithm>
#include <cctype>

namespace vkt {
namespace cad {

Layer::Layer()
    : m_name("0")
    , m_color(Color::White())
{
    // Varsayılan layer "0" (AutoCAD standardı)
}

Layer::Layer(const std::string& name, const Color& color)
    : m_name(name)
    , m_color(color)
{
    // Varsayılan ayarlar zaten header'da
}

void Layer::SetColorIndex(int index) {
    m_colorIndex = index;
    
    // AutoCAD standart renk paleti (basitleştirilmiş)
    if (index >= 1 && index <= 255) {
        // İlk 7 renk standart
        switch (index) {
            case 1: m_color = Color::Red(); break;
            case 2: m_color = Color::Yellow(); break;
            case 3: m_color = Color::Green(); break;
            case 4: m_color = Color::Cyan(); break;
            case 5: m_color = Color::Blue(); break;
            case 6: m_color = Color::Magenta(); break;
            case 7: m_color = Color::White(); break;
            default:
                // Diğer renkler için basit hesaplama
                // Gerçek AutoCAD paleti daha karmaşık
                m_color.r = (index * 37) % 256;
                m_color.g = (index * 71) % 256;
                m_color.b = (index * 113) % 256;
                break;
        }
    }
}

Color Layer::GetColorFromACI(int aci) {
    // AutoCAD standart renk paleti
    switch (aci) {
        case 1: return Color::Red();
        case 2: return Color::Yellow();
        case 3: return Color::Green();
        case 4: return Color::Cyan();
        case 5: return Color::Blue();
        case 6: return Color::Magenta();
        case 7: return Color::White();
        default:
            // Diğer renkler için basit hesaplama
            Color color;
            color.r = (aci * 37) % 256;
            color.g = (aci * 71) % 256;
            color.b = (aci * 113) % 256;
            return color;
    }
}

bool LayerNameCompare::operator()(const std::string& a, const std::string& b) const {
    // Case-insensitive karşılaştırma
    std::string aLower = a;
    std::string bLower = b;
    
    std::transform(aLower.begin(), aLower.end(), aLower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    std::transform(bLower.begin(), bLower.end(), bLower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    return aLower < bLower;
}

} // namespace cad
} // namespace vkt
