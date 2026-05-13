/**
 * @file VulkanWindow.hpp
 * @brief Qt-Vulkan entegrasyonu: QWindow tabanlı Vulkan rendering penceresi
 */

#pragma once

#include <QWindow>
#include <QTimer>
#include <functional>
#include "cad/Viewport.hpp"
#include "cad/Entity.hpp"
#include "render/VulkanRenderer.hpp"
#include "mep/NetworkGraph.hpp"

namespace vkt {
namespace render {

/**
 * @class VulkanWindow
 * @brief Vulkan surface barındıran QWindow.
 *
 * Mouse/keyboard event'lerini yakalar ve Viewport'a iletir.
 * VulkanRenderer ile render loop yönetir.
 */
class VulkanWindow : public QWindow {
    Q_OBJECT

public:
    explicit VulkanWindow(QWindow* parent = nullptr);
    ~VulkanWindow() override;

    /// Vulkan renderer'ı başlatır
    bool InitializeRenderer();

    /// Renderer erişimi
    VulkanRenderer* GetRenderer() { return &m_renderer; }
    const VulkanRenderer* GetRenderer() const { return &m_renderer; }

    /// Viewport erişimi
    cad::Viewport& GetViewport() { return m_viewport; }
    const cad::Viewport& GetViewport() const { return m_viewport; }

    /// Aktif network referansı (render için)
    void SetNetwork(const mep::NetworkGraph* network) { m_network = network; }

    /// CAD entity referansı (arka plan çizimi)
    void SetCADEntities(const std::vector<std::unique_ptr<cad::Entity>>* entities) {
        m_cadEntities = entities;
        if (m_initialized) m_renderer.InvalidateCADData();
    }

    /// Mouse callback'leri (DrawTool/SelectionManager bağlantısı için)
    using MouseCallback = std::function<void(double worldX, double worldY, Qt::MouseButton button)>;
    using MoveCallback = std::function<void(double worldX, double worldY)>;

    void SetMousePressCallback(MouseCallback cb) { m_onMousePress = std::move(cb); }
    void SetMouseReleaseCallback(MouseCallback cb) { m_onMouseRelease = std::move(cb); }
    void SetMouseMoveCallback(MoveCallback cb) { m_onMouseMove = std::move(cb); }

    using ViewportChangeCallback = std::function<void()>;
    void SetViewportChangeCallback(ViewportChangeCallback cb) { m_onViewportChange = std::move(cb); }

signals:
    void FrameRendered();

protected:
    void exposeEvent(QExposeEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void RenderFrame();

private:
    VulkanRenderer m_renderer;
    cad::Viewport m_viewport;
    const mep::NetworkGraph* m_network = nullptr;
    const std::vector<std::unique_ptr<cad::Entity>>* m_cadEntities = nullptr;

    QTimer m_renderTimer;
    bool m_initialized = false;

    // Pan state
    bool m_isPanning = false;
    QPoint m_lastMousePos;

    // Callbacks
    MouseCallback m_onMousePress;
    MouseCallback m_onMouseRelease;
    MoveCallback m_onMouseMove;
    ViewportChangeCallback m_onViewportChange;
};

} // namespace render
} // namespace vkt
