/**
 * @file VulkanRenderer.hpp
 * @brief Vulkan Tabanlı 3D Çizim Motoru
 * 
 * Tesisat şebekesini Vulkan ile render eder.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "geom/Math.hpp"
#include "mep/NetworkGraph.hpp"

namespace vkt {
namespace render {

/**
 * @enum ViewMode
 * @brief Görünüm modu
 */
enum class ViewMode {
    Plan,           ///< Plan görünümü (2D)
    Isometric,      ///< İzometrik 3D
    Perspective     ///< Perspektif 3D
};

/**
 * @class VulkanRenderer
 * @brief Vulkan rendering motoru
 * 
 * GPU hızlandırmalı 3D görselleştirme sağlar.
 */
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();
    
    // Başlatma
    bool Initialize(void* windowHandle);
    void Shutdown();
    
    // Çizim
    void BeginFrame();
    void Render(const mep::NetworkGraph& network);
    void EndFrame();
    
    // Kamera
    void SetCamera(const geom::Camera& camera);
    geom::Camera& GetCamera() { return m_camera; }
    
    // Görünüm modu
    void SetViewMode(ViewMode mode);
    ViewMode GetViewMode() const { return m_viewMode; }
    
    // Seçim
    uint32_t Pick(int screenX, int screenY);
    
private:
    // Vulkan nesneleri
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkFramebuffer> m_framebuffers;
    
    // Sahne verisi
    geom::Camera m_camera;
    ViewMode m_viewMode = ViewMode::Plan;
    
    // Yardımcı fonksiyonlar
    bool CreateVulkanInstance();
    bool CreateDevice();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreatePipeline();
    void CreateCommandBuffers();
    
    void DrawNetwork(VkCommandBuffer cmd, const mep::NetworkGraph& network);
    void DrawNode(VkCommandBuffer cmd, const mep::Node& node);
    void DrawEdge(VkCommandBuffer cmd, const mep::Edge& edge);
};

} // namespace render
} // namespace vkt
