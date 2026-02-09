/**
 * @file Window.hpp
 * @brief CAD Pencere Yönetimi
 * 
 * Qt tabanlı ana uygulama penceresi.
 */

#pragma once

#include <QMainWindow>
#include <memory>
#include "Document.hpp"
#include "render/VulkanRenderer.hpp"
#include "ui/UI.hpp"

namespace vkt {
namespace core {

/**
 * @class Window
 * @brief Ana CAD penceresi
 * 
 * Qt MainWindow türevi, Vulkan viewport ve UI içerir.
 */
class Window : public QMainWindow {
    Q_OBJECT

public:
    explicit Window(QWidget* parent = nullptr);
    ~Window();

    void SetDocument(Document* doc);
    Document* GetDocument() const { return m_document; }

    render::VulkanRenderer* GetRenderer() { return m_renderer.get(); }
    ui::UI* GetUI() { return m_ui.get(); }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnNew();
    void OnOpen();
    void OnSave();
    void OnSaveAs();
    void OnUndo();
    void OnRedo();

private:
    void SetupUI();
    void CreateActions();
    void CreateMenus();
    void CreateToolbars();

    Document* m_document = nullptr;
    std::unique_ptr<render::VulkanRenderer> m_renderer;
    std::unique_ptr<ui::UI> m_ui;
};

} // namespace core
} // namespace vkt
