/**
 * @file UI.hpp
 * @brief Kullanıcı Arayüzü Yöneticisi
 * 
 * Qt tabanlı CAD arayüzü bileşenleri.
 */

#pragma once

#include <QWidget>
#include <QToolBar>
#include <QDockWidget>
#include <memory>

namespace vkt {

// Forward declarations
namespace core { class Window; }
namespace mep { class NetworkGraph; }

namespace ui {

/**
 * @class PropertyPanel
 * @brief Nesne özellik paneli
 */
class PropertyPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit PropertyPanel(QWidget* parent = nullptr);
    void ShowNodeProperties(uint32_t nodeId);
    void ShowEdgeProperties(uint32_t edgeId);
    
signals:
    void PropertyChanged();
};

/**
 * @class UI
 * @brief Ana kullanıcı arayüzü yöneticisi
 * 
 * Toolbar, menu ve panelleri yönetir.
 */
class UI {
public:
    explicit UI(core::Window* window);
    ~UI();
    
    void Initialize();
    void Update();
    
    // Panel erişimi
    PropertyPanel* GetPropertyPanel() { return m_propertyPanel.get(); }
    
    // Mod yönetimi
    void SetDrawMode(bool enabled);
    void SetSelectMode(bool enabled);
    
private:
    void CreateToolbars();
    void CreatePanels();
    
    core::Window* m_window;
    std::unique_ptr<PropertyPanel> m_propertyPanel;
};

} // namespace ui
} // namespace vkt
