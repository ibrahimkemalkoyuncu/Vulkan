#pragma once

#include "geom/Math.hpp"
#include <cstdint>

namespace vkt::cad {

/**
 * @brief Viewport - Ekran ↔ World koordinat dönüşümleri
 * 
 * AutoCAD benzeri viewport sistemi:
 * - Pan (kaydırma)
 * - Zoom (yakınlaştırma/uzaklaştırma)
 * - Zoom Extents (tüm objeleri göster)
 * - World ↔ Screen koordinat dönüşümleri
 */
class Viewport {
public:
    Viewport();
    Viewport(int width, int height);
    ~Viewport() = default;
    
    // Viewport boyutu
    void SetSize(int width, int height);
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    double GetAspectRatio() const;
    
    // Kamera/görüş kontrolü
    /**
     * @brief Viewport'un merkez noktasını ayarlar (world coords)
     */
    void SetCenter(const geom::Vec3& center);
    const geom::Vec3& GetCenter() const { return m_center; }
    
    /**
     * @brief Zoom seviyesini ayarlar (birim: pixel per world unit)
     * Yüksek zoom = yakın görünüm
     */
    void SetZoom(double zoom);
    double GetZoom() const { return m_zoom; }
    
    /**
     * @brief Viewport'un gösterdiği world alanını döndürür
     */
    struct ViewBounds {
        geom::Vec3 min;
        geom::Vec3 max;
        double width;
        double height;
    };
    ViewBounds GetViewBounds() const;
    
    // Pan işlemleri
    /**
     * @brief Viewport'u kaydırır (ekran koordinatlarında)
     * @param dx Pixel cinsinden X delta
     * @param dy Pixel cinsinden Y delta
     */
    void Pan(double dx, double dy);
    
    /**
     * @brief Viewport'u kaydırır (world koordinatlarında)
     */
    void PanWorld(const geom::Vec3& delta);
    
    // Zoom işlemleri
    /**
     * @brief Belirli bir noktaya zoom yapar
     * @param factor Zoom faktörü (>1: zoom in, <1: zoom out)
     * @param screenX Screen X koordinatı
     * @param screenY Screen Y koordinatı
     */
    void ZoomAt(double factor, double screenX, double screenY);
    
    /**
     * @brief Merkeze zoom yapar
     */
    void ZoomCenter(double factor);
    
    /**
     * @brief Zoom in (1.25x)
     */
    void ZoomIn();
    
    /**
     * @brief Zoom out (0.8x)
     */
    void ZoomOut();
    
    /**
     * @brief Verilen bounding box'ı gösterecek şekilde zoom yapar
     * @param bounds Gösterilecek alan
     * @param margin Margin faktörü (0.1 = %10 boşluk)
     */
    void ZoomToBounds(const geom::Vec3& min, const geom::Vec3& max, double margin = 0.1);
    
    /**
     * @brief Zoom seviyesini sıfırlar (1:1)
     */
    void ZoomReset();
    
    /**
     * @brief Window zoom (dikdörtgen alan seçimi)
     */
    void ZoomWindow(const geom::Vec3& corner1, const geom::Vec3& corner2);
    
    /**
     * @brief Tüm objeleri gösterecek şekilde zoom yapar (Zoom Extents)
     */
    void ZoomExtents(const geom::Vec3& min, const geom::Vec3& max, double margin = 0.1);
    
    // Koordinat dönüşümleri
    /**
     * @brief World koordinatlarını screen koordinatlarına dönüştürür
     * @param world World koordinatı
     * @return Screen koordinatı (pixel, sol-üst köşe origin)
     */
    geom::Vec3 WorldToScreen(const geom::Vec3& world) const;
    
    /**
     * @brief Screen koordinatlarını world koordinatlarına dönüştürür
     * @param screen Screen koordinatı (pixel)
     * @return World koordinatı
     */
    geom::Vec3 ScreenToWorld(const geom::Vec3& screen) const;
    
    /**
     * @brief Screen koordinatlarını world koordinatlarına dönüştürür (2D overload)
     */
    geom::Vec3 ScreenToWorld(double screenX, double screenY) const;
    
    /**
     * @brief World mesafesini screen mesafesine dönüştürür
     */
    double WorldToScreenDistance(double worldDistance) const;
    
    /**
     * @brief Screen mesafesini world mesafesine dönüştürür
     */
    double ScreenToWorldDistance(double screenDistance) const;
    
    // Hit testing yardımcıları
    /**
     * @brief Screen noktasından ray cast yapar (3D için)
     * @return Ray başlangıcı ve yönü
     */
    struct Ray {
        geom::Vec3 origin;
        geom::Vec3 direction;
    };
    Ray GetRay(double screenX, double screenY) const;
    
    /**
     * @brief Bir noktanın viewport içinde olup olmadığını kontrol eder
     */
    bool IsPointInView(const geom::Vec3& worldPoint) const;
    
    /**
     * @brief Bir bounding box'ın viewport ile kesişip kesişmediğini kontrol eder
     */
    bool IntersectsView(const geom::Vec3& min, const geom::Vec3& max) const;
    
    // Matris dönüşümleri (3D rendering için)
    /**
     * @brief View matrisini döndürür
     */
    geom::Mat4 GetViewMatrix() const;
    
    /**
     * @brief Projection matrisini döndürür (orthographic)
     */
    geom::Mat4 GetProjectionMatrix() const;
    
    /**
     * @brief View-Projection matrisini döndürür
     */
    geom::Mat4 GetViewProjectionMatrix() const;
    
    // Viewport state
    /**
     * @brief Viewport state'ini kaydeder (undo/redo için)
     */
    struct ViewportState {
        geom::Vec3 center;
        double zoom;
        int width;
        int height;
    };
    
    ViewportState GetState() const;
    void SetState(const ViewportState& state);
    
    // Settings
    /**
     * @brief Minimum ve maksimum zoom limitleri
     */
    void SetZoomLimits(double minZoom, double maxZoom);
    double GetMinZoom() const { return m_minZoom; }
    double GetMaxZoom() const { return m_maxZoom; }
    
    /**
     * @brief Smooth zoom/pan animasyon için interpolasyon faktörü
     */
    void SetSmoothFactor(double factor) { m_smoothFactor = factor; }
    double GetSmoothFactor() const { return m_smoothFactor; }
    
private:
    int m_width;
    int m_height;
    geom::Vec3 m_center;      // World koordinatlarında merkez
    double m_zoom;            // Zoom seviyesi (pixels per world unit)
    
    // Limits
    double m_minZoom;
    double m_maxZoom;
    
    // Animation/smooth movement
    double m_smoothFactor;    // 0.0 - 1.0 (0: ani, 1: çok smooth)
    
    // Helper fonksiyonlar
    double ClampZoom(double zoom) const;
};

} // namespace vkt::cad
