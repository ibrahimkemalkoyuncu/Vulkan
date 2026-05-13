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

/**
 * @brief ACI renk indeksinden layer rengini ayarla
 *
 * Color::FromACI() tam 256-renk tablosunu kullanır.
 * Eski basitleştirilmiş hesaplama kaldırıldı.
 */
void Layer::SetColorIndex(int index) {
    m_colorIndex = index;
    if (index >= 1 && index <= 255) {
        m_color = Color::FromACI(index);
    }
}

/**
 * @brief ACI renk indeksinden Color nesnesi oluştur (statik)
 *
 * Color::FromACI() tam 256-renk tablosunu kullanır.
 */
Color Layer::GetColorFromACI(int aci) {
    return Color::FromACI(aci);
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
