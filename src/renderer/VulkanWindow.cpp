/**
 * @file VulkanWindow.cpp
 * @brief Qt-Vulkan pencere implementasyonu
 */

#include "render/VulkanWindow.hpp"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <iostream>
#include <cmath>

namespace vkt {
namespace render {

VulkanWindow::VulkanWindow(QWindow* parent)
    : QWindow(parent)
{
    setSurfaceType(QWindow::VulkanSurface);

    // Render timer: ~60 FPS
    connect(&m_renderTimer, &QTimer::timeout, this, &VulkanWindow::RenderFrame);
}

VulkanWindow::~VulkanWindow() {
    m_renderTimer.stop();
    m_renderer.Shutdown();
}

bool VulkanWindow::InitializeRenderer() {
    if (m_initialized) return true;

    // Platform-specific window handle
    void* handle = reinterpret_cast<void*>(winId());
    if (!m_renderer.Initialize(handle)) {
        std::cerr << "VulkanWindow: Renderer initialization failed" << std::endl;
        return false;
    }

    m_viewport.SetSize(width(), height());
    m_initialized = true;

    // Render loop başlat
    m_renderTimer.start(16); // ~60fps

    std::cout << "VulkanWindow: Renderer initialized, render loop started" << std::endl;
    return true;
}

void VulkanWindow::RenderFrame() {
    if (!m_initialized || !isExposed()) return;

    // VP matrisini renderer'a set et
    geom::Mat4 vp = m_viewport.GetViewProjectionMatrix();
    m_renderer.SetViewProjectionMatrix(vp);

    m_renderer.BeginFrame();

    // CAD entities first (background layer)
    if (m_cadEntities && !m_cadEntities->empty()) {
        static bool logOnce = true;
        if (logOnce) {
            std::cout << "[VulkanWindow::RenderFrame] Rendering " << m_cadEntities->size()
                      << " CAD entities" << std::endl;
            logOnce = false;
        }
        m_renderer.RenderCADEntities(*m_cadEntities);
    }

    // MEP network on top
    if (m_network) {
        m_renderer.Render(*m_network);
    }

    m_renderer.EndFrame();

    emit FrameRendered();
}

void VulkanWindow::exposeEvent(QExposeEvent* /*event*/) {
    if (isExposed() && !m_initialized) {
        InitializeRenderer();
    }
}

void VulkanWindow::resizeEvent(QResizeEvent* /*event*/) {
    m_viewport.SetSize(width(), height());

    if (m_initialized) {
        m_renderer.OnResize(width(), height());
    }
}

void VulkanWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (m_onMousePress) {
        geom::Vec3 worldPos = m_viewport.ScreenToWorld(event->pos().x(), event->pos().y());
        m_onMousePress(worldPos.x, worldPos.y, event->button());
    }
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }

    if (m_onMouseRelease) {
        geom::Vec3 worldPos = m_viewport.ScreenToWorld(event->pos().x(), event->pos().y());
        m_onMouseRelease(worldPos.x, worldPos.y, event->button());
    }
}

void VulkanWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        double dx = event->pos().x() - m_lastMousePos.x();
        double dy = event->pos().y() - m_lastMousePos.y();
        m_viewport.Pan(dx, dy);
        m_lastMousePos = event->pos();
        return;
    }

    if (m_onMouseMove) {
        geom::Vec3 worldPos = m_viewport.ScreenToWorld(event->pos().x(), event->pos().y());
        m_onMouseMove(worldPos.x, worldPos.y);
    }
}

/**
 * @brief Mouse tekerleği ile zoom — AutoCAD benzeri hassasiyet
 *
 * angleDelta 120'ye bölünerek kademeli scroll desteği sağlanır.
 * Trackpad'ler ve yüksek çözünürlüklü fare tekerlekleri doğru çalışır.
 */
void VulkanWindow::wheelEvent(QWheelEvent* event) {
    double delta = event->angleDelta().y();
    // 120 birim = 1 standart scroll notch, trackpad'ler daha küçük delta gönderir
    double scrollUnits = delta / 120.0;
    // AutoCAD benzeri: her notch ~%15 zoom, kademeli
    double factor = std::pow(1.15, scrollUnits);

    QPointF pos = event->position();
    m_viewport.ZoomAt(factor, pos.x(), pos.y());
}

void VulkanWindow::keyPressEvent(QKeyEvent* event) {
    // Escape tuşu ile çizim iptal vs. üst seviyede handle edilecek
    QWindow::keyPressEvent(event);
}

} // namespace render
} // namespace vkt
