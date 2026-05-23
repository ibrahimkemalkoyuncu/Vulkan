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
    m_renderer.SetPixelsPerWorldUnit(static_cast<float>(m_viewport.GetZoom()));

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
        m_panMoved = false;
        m_lastMousePos = event->pos();
        // Cursor henüz hareket etmedi — "hazır" işareti
        setCursor(Qt::OpenHandCursor);
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
        m_panMoved  = false;
        setCursor(Qt::ArrowCursor);
        if (m_onViewportChange) m_onViewportChange();
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
        if (!m_panMoved && (std::abs(dx) > 2 || std::abs(dy) > 2)) {
            // İlk gerçek hareket — "sürükleniyor" imleci
            setCursor(Qt::ClosedHandCursor);
            m_panMoved = true;
        }
        m_viewport.Pan(dx, dy);
        m_lastMousePos = event->pos();
        if (m_onViewportChange) m_onViewportChange();
        return;
    }

    if (m_onMouseMove) {
        geom::Vec3 worldPos = m_viewport.ScreenToWorld(event->pos().x(), event->pos().y());
        m_onMouseMove(worldPos.x, worldPos.y);
    }
}

// Çift tık orta tuş → Zoom Extents (AutoCAD standart davranışı)
void VulkanWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        if (m_onZoomExtents) m_onZoomExtents();
        return;
    }
    QWindow::mouseDoubleClickEvent(event);
}

/**
 * @brief Mouse tekerleği ile zoom/pan — AutoCAD benzeri hassasiyet
 *
 * - Normal scroll  → imleç konumuna doğru zoom
 * - Shift+scroll   → yatay pan (AutoCAD Shift+scroll eşdeğeri)
 * - 120 birim = 1 standart notch; trackpad küçük delta gönderir
 */
void VulkanWindow::wheelEvent(QWheelEvent* event) {
    const bool shiftHeld = (event->modifiers() & Qt::ShiftModifier) != 0;

    if (shiftHeld) {
        // Shift+scroll → yatay pan
        double hDelta = event->angleDelta().x() != 0
                        ? event->angleDelta().x()
                        : event->angleDelta().y();
        // Her notch (120 birim) ≈ ekran genişliğinin %5'i kadar pan
        double panPx = (hDelta / 120.0) * width() * 0.05;
        m_viewport.Pan(panPx, 0);
        if (m_onViewportChange) m_onViewportChange();
        return;
    }

    double delta = event->angleDelta().y();
    double scrollUnits = delta / 120.0;
    // Her notch ~%15 zoom — AutoCAD varsayılanı
    double factor = std::pow(1.15, scrollUnits);
    QPointF pos = event->position();
    m_viewport.ZoomAt(factor, pos.x(), pos.y());
    if (m_onViewportChange) m_onViewportChange();
}

void VulkanWindow::keyPressEvent(QKeyEvent* event) {
    // Escape tuşu ile çizim iptal vs. üst seviyede handle edilecek
    QWindow::keyPressEvent(event);
}

} // namespace render
} // namespace vkt
