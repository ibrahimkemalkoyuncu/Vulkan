/**
 * @file Entity.cpp
 * @brief Entity sınıfının implementasyonu
 */

#include "cad/Entity.hpp"

namespace vkt {
namespace cad {

// Static ID counter başlangıcı
uint64_t Entity::s_nextId = 1;

Entity::Entity(EntityType type, Layer* layer)
    : m_type(type)
    , m_id(GenerateId())
    , m_layer(layer)
{
    // Varsayılan durum: görünür
    m_flags = EF_Visible;
}

Entity::Entity()
    : m_type(EntityType::Unknown)
    , m_id(GenerateId())
    , m_layer(nullptr)
{
    // Varsayılan durum: görünür
    m_flags = EF_Visible;
}

} // namespace cad
} // namespace vkt
