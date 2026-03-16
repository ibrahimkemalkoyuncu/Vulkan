#include "cad/Viewport.hpp"
#include <algorithm>
#include <cmath>

namespace vkt::cad {

Viewport::Viewport()
    : m_width(1920), m_height(1080),
      m_center(), m_zoom(1.0),
      m_minZoom(1e-6), m_maxZoom(1000.0),
      m_smoothFactor(0.2) {}

Viewport::Viewport(int width, int height)
    : m_width(width), m_height(height),
      m_center(), m_zoom(1.0),
      m_minZoom(1e-6), m_maxZoom(1000.0),
      m_smoothFactor(0.2) {}

void Viewport::SetSize(int width, int height) {
    m_width = width;
    m_height = height;
}

double Viewport::GetAspectRatio() const {
    return m_height > 0 ? static_cast<double>(m_width) / m_height : 1.0;
}

void Viewport::SetCenter(const geom::Vec3& center) {
    m_center = center;
}

void Viewport::SetZoom(double zoom) {
    m_zoom = ClampZoom(zoom);
}

Viewport::ViewBounds Viewport::GetViewBounds() const {
    ViewBounds bounds;
    
    double halfWidth = (m_width / 2.0) / m_zoom;
    double halfHeight = (m_height / 2.0) / m_zoom;
    
    bounds.min = geom::Vec3(
        m_center.x - halfWidth,
        m_center.y - halfHeight,
        m_center.z
    );
    
    bounds.max = geom::Vec3(
        m_center.x + halfWidth,
        m_center.y + halfHeight,
        m_center.z
    );
    
    bounds.width = halfWidth * 2.0;
    bounds.height = halfHeight * 2.0;
    
    return bounds;
}

void Viewport::Pan(double dx, double dy) {
    m_center.x -= dx / m_zoom;
    m_center.y += dy / m_zoom;  // Screen Y is inverted
}

void Viewport::PanWorld(const geom::Vec3& delta) {
    m_center.x += delta.x;
    m_center.y += delta.y;
    m_center.z += delta.z;
}

void Viewport::ZoomAt(double factor, double screenX, double screenY) {
    // Get world position before zoom
    geom::Vec3 worldPosBeforeZoom = ScreenToWorld(screenX, screenY);
    
    // Apply zoom
    double newZoom = ClampZoom(m_zoom * factor);
    m_zoom = newZoom;
    
    // Get world position after zoom
    geom::Vec3 worldPosAfterZoom = ScreenToWorld(screenX, screenY);
    
    // Adjust center to keep point under cursor
    m_center.x += worldPosBeforeZoom.x - worldPosAfterZoom.x;
    m_center.y += worldPosBeforeZoom.y - worldPosAfterZoom.y;
}

void Viewport::ZoomCenter(double factor) {
    m_zoom = ClampZoom(m_zoom * factor);
}

void Viewport::ZoomIn() {
    ZoomCenter(1.25);
}

void Viewport::ZoomOut() {
    ZoomCenter(0.8);
}

void Viewport::ZoomToBounds(const geom::Vec3& min, const geom::Vec3& max, double margin) {
    m_center = geom::Vec3(
        (min.x + max.x) * 0.5,
        (min.y + max.y) * 0.5,
        (min.z + max.z) * 0.5
    );
    
    double width = max.x - min.x;
    double height = max.y - min.y;
    
    if (width < 1e-6 && height < 1e-6) {
        m_zoom = 1.0;
        return;
    }
    
    double scaleX = m_width / (width * (1.0 + margin));
    double scaleY = m_height / (height * (1.0 + margin));
    
    m_zoom = ClampZoom(std::min(scaleX, scaleY));
}

void Viewport::ZoomReset() {
    m_zoom = 1.0;
}

void Viewport::ZoomWindow(const geom::Vec3& corner1, const geom::Vec3& corner2) {
    geom::Vec3 min = geom::Vec3(
        std::min(corner1.x, corner2.x),
        std::min(corner1.y, corner2.y),
        std::min(corner1.z, corner2.z)
    );
    
    geom::Vec3 max = geom::Vec3(
        std::max(corner1.x, corner2.x),
        std::max(corner1.y, corner2.y),
        std::max(corner1.z, corner2.z)
    );
    
    ZoomToBounds(min, max, 0.0);
}

void Viewport::ZoomExtents(const geom::Vec3& min, const geom::Vec3& max, double margin) {
    ZoomToBounds(min, max, margin);
}

geom::Vec3 Viewport::WorldToScreen(const geom::Vec3& world) const {
    double screenX = (world.x - m_center.x) * m_zoom + m_width / 2.0;
    double screenY = (m_center.y - world.y) * m_zoom + m_height / 2.0;  // Y inverted
    
    return geom::Vec3(screenX, screenY, 0.0);
}

geom::Vec3 Viewport::ScreenToWorld(const geom::Vec3& screen) const {
    double worldX = (screen.x - m_width / 2.0) / m_zoom + m_center.x;
    double worldY = m_center.y - (screen.y - m_height / 2.0) / m_zoom;  // Y inverted
    
    return geom::Vec3(worldX, worldY, m_center.z);
}

geom::Vec3 Viewport::ScreenToWorld(double screenX, double screenY) const {
    return ScreenToWorld(geom::Vec3(screenX, screenY, 0.0));
}

double Viewport::WorldToScreenDistance(double worldDistance) const {
    return worldDistance * m_zoom;
}

double Viewport::ScreenToWorldDistance(double screenDistance) const {
    return screenDistance / m_zoom;
}

Viewport::Ray Viewport::GetRay(double screenX, double screenY) const {
    Ray ray;
    ray.origin = ScreenToWorld(screenX, screenY);
    ray.direction = geom::Vec3(0.0, 0.0, -1.0);  // Orthographic - rays are parallel
    return ray;
}

bool Viewport::IsPointInView(const geom::Vec3& worldPoint) const {
    ViewBounds bounds = GetViewBounds();
    return worldPoint.x >= bounds.min.x && worldPoint.x <= bounds.max.x &&
           worldPoint.y >= bounds.min.y && worldPoint.y <= bounds.max.y;
}

bool Viewport::IntersectsView(const geom::Vec3& min, const geom::Vec3& max) const {
    ViewBounds bounds = GetViewBounds();
    
    // AABB intersection test
    return !(max.x < bounds.min.x || min.x > bounds.max.x ||
             max.y < bounds.min.y || min.y > bounds.max.y);
}

geom::Mat4 Viewport::GetViewMatrix() const {
    // Orthographic 2D view matrix
    geom::Mat4 view;
    view.Identity();
    
    // Translation to center
    view.data[12] = -m_center.x;
    view.data[13] = -m_center.y;
    view.data[14] = -m_center.z;
    
    return view;
}

geom::Mat4 Viewport::GetProjectionMatrix() const {
    // Orthographic projection matrix
    ViewBounds bounds = GetViewBounds();
    
    geom::Mat4 proj;
    proj.Identity();
    
    double width = bounds.width;
    double height = bounds.height;
    double near = -1000.0;
    double far = 1000.0;
    
    proj.data[0] = 2.0 / width;
    proj.data[5] = 2.0 / height;
    proj.data[10] = -2.0 / (far - near);
    proj.data[14] = -(far + near) / (far - near);
    
    return proj;
}

geom::Mat4 Viewport::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

Viewport::ViewportState Viewport::GetState() const {
    return ViewportState{m_center, m_zoom, m_width, m_height};
}

void Viewport::SetState(const ViewportState& state) {
    m_center = state.center;
    m_zoom = ClampZoom(state.zoom);
    m_width = state.width;
    m_height = state.height;
}

void Viewport::SetZoomLimits(double minZoom, double maxZoom) {
    m_minZoom = minZoom;
    m_maxZoom = maxZoom;
    m_zoom = ClampZoom(m_zoom);
}

double Viewport::ClampZoom(double zoom) const {
    return std::max(m_minZoom, std::min(m_maxZoom, zoom));
}

} // namespace vkt::cad
