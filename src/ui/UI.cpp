/**
 * @file UI.cpp
 * @brief Kullanıcı Arayüzü İmplementasyonu
 */

#include "ui/UI.hpp"
#include "core/Window.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <iostream>

namespace vkt {
namespace ui {

PropertyPanel::PropertyPanel(QWidget* parent)
    : QDockWidget("Özellikler", parent) {
    
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->addWidget(new QLabel("Nesne özellikleri burada görünecek"));
    
    setWidget(widget);
}

void PropertyPanel::ShowNodeProperties(uint32_t nodeId) {
    std::cout << "Node özellikleri gösteriliyor: " << nodeId << std::endl;
}

void PropertyPanel::ShowEdgeProperties(uint32_t edgeId) {
    std::cout << "Edge özellikleri gösteriliyor: " << edgeId << std::endl;
}

UI::UI(core::Window* window)
    : m_window(window) {
    std::cout << "UI oluşturuldu." << std::endl;
}

UI::~UI() {
    std::cout << "UI yok edildi." << std::endl;
}

void UI::Initialize() {
    CreatePanels();
    CreateToolbars();
    std::cout << "UI başlatıldı." << std::endl;
}

void UI::Update() {
    // UI güncelleme
}

void UI::SetDrawMode(bool enabled) {
    std::cout << "Çizim modu: " << (enabled ? "AÇIK" : "KAPALI") << std::endl;
}

void UI::SetSelectMode(bool enabled) {
    std::cout << "Seçim modu: " << (enabled ? "AÇIK" : "KAPALI") << std::endl;
}

void UI::CreateToolbars() {
    // Toolbar'lar Window tarafından oluşturuluyor
}

void UI::CreatePanels() {
    m_propertyPanel = std::make_unique<PropertyPanel>(m_window);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_propertyPanel.get());
}

} // namespace ui
} // namespace vkt
