/**
 * @file VulkanRenderer.hpp
 * @brief Vulkan Tabanlı 3D Çizim Motoru
 *
 * Tesisat şebekesini Vulkan ile render eder.
 */

#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// Windows makroları Qt/C++ sınıf isimleriyle çakışır — temizle
#undef CreateWindow
#undef DrawState
#undef min
#undef max
#undef near
#undef far
#endif
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <array>
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
 * @struct PushConstants
 * @brief MVP matrisi push constant olarak shader'a gönderilir
 */
struct PushConstants {
    geom::Mat4 mvp;
};

/**
 * @class VulkanRenderer
 * @brief Vulkan rendering motoru
 *
 * GPU hızlandırmalı 2D/3D görselleştirme sağlar.
 */
class VulkanRenderer {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VulkanRenderer();
    ~VulkanRenderer();

    // Başlatma
    bool Initialize(void* windowHandle);
    void Shutdown();
    void OnResize(int width, int height);

    // Çizim
    void BeginFrame();
    void Render(const mep::NetworkGraph& network);
    void EndFrame();

    // MVP matrisi
    void SetViewProjectionMatrix(const geom::Mat4& vp) { m_viewProjection = vp; }

    // Kamera (geriye uyumluluk)
    void SetCamera(const geom::Camera& camera);
    geom::Camera& GetCamera() { return m_camera; }

    // Görünüm modu
    void SetViewMode(ViewMode mode);
    ViewMode GetViewMode() const { return m_viewMode; }

    // Seçim
    uint32_t Pick(int screenX, int screenY);

    bool IsInitialized() const { return m_initialized; }

private:
    // Vulkan instance & device
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;

    // Swapchain
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D m_swapchainExtent{};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    // Depth buffer
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    // Render pass & framebuffers
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    // Pipeline
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_linePipeline = VK_NULL_HANDLE;
    VkPipeline m_trianglePipeline = VK_NULL_HANDLE;

    // Command buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Synchronization
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;

    // Vertex buffers
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;
    uint32_t m_lineVertexCount = 0;
    uint32_t m_lineVertexOffset = 0;
    uint32_t m_triangleVertexCount = 0;
    uint32_t m_triangleVertexOffset = 0;

    // Grid vertex buffer
    VkBuffer m_gridVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_gridVertexMemory = VK_NULL_HANDLE;
    uint32_t m_gridVertexCount = 0;

    // Scene state
    geom::Camera m_camera;
    geom::Mat4 m_viewProjection;
    ViewMode m_viewMode = ViewMode::Plan;

    bool m_initialized = false;
    bool m_framebufferResized = false;
    void* m_windowHandle = nullptr;
    int m_width = 800;
    int m_height = 600;

    // Debug
#ifdef _DEBUG
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
#endif

    // Setup fonksiyonları
    bool CreateVulkanInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    void CleanupSwapchain();
    void RecreateSwapchain();
    bool CreateImageViews();
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreatePipelineLayout();
    bool CreateGraphicsPipelines();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    // Vertex buffer fonksiyonları
    void UpdateNetworkVertexData(const mep::NetworkGraph& network);
    void CreateGridVertexData();
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);
    void DestroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Draw fonksiyonları
    void DrawGrid(VkCommandBuffer cmd);
    void DrawNetwork(VkCommandBuffer cmd, const mep::NetworkGraph& network);

    // Shader
    VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code);

    // Node renkleri
    static std::array<float, 3> GetNodeColor(mep::NodeType type);
    static std::array<float, 3> GetEdgeColor(mep::EdgeType type);
};

} // namespace render
} // namespace vkt
