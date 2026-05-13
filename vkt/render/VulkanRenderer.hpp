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
#include "cad/Entity.hpp"
#include "cad/Layer.hpp"
#include "mep/NetworkGraph.hpp"
#include "render/Gizmo.hpp"
#include <unordered_map>
#include <string>

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

/** G-Buffer geçişi için push constant: mvp + modelView = 128 bytes */
struct GBufferPushConstants {
    geom::Mat4 mvp;
    geom::Mat4 modelView;
};

/** PBR composite pass push constant: ışık yönü + malzeme parametreleri */
struct CompositePushConstants {
    float lightDir[4] = {0.577f, 0.577f, 0.577f, 0.0f}; // view-space dir (normalized)
    float roughness   = 0.45f;
    float metalness   = 0.0f;
    float ambient     = 0.25f;
    float pad         = 0.0f;
};

/** SSAO hesabı için push constant */
struct SSAOPushConstants {
    geom::Mat4 projection;
    float      radius;
    float      bias;
    float      power;
    float      noiseScaleX;
    float      noiseScaleY;
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

    // CAD entity rendering
    void RenderCADEntities(const std::vector<std::unique_ptr<cad::Entity>>& entities);
    void InvalidateCADData() { m_cadDirty = true; }

    // Gizmo
    Gizmo&       GetGizmo()               { return m_gizmo; }
    void         SetGizmoVisible(bool v)  { m_gizmoVisible = v; }
    bool         IsGizmoVisible() const   { return m_gizmoVisible; }
    GizmoAxis    PickGizmo(int screenX, int screenY) const;

    // PBR composite params
    void SetCompositeMaterial(float roughness, float metalness) {
        m_compositePC.roughness = roughness;
        m_compositePC.metalness = metalness;
    }
    CompositePushConstants& GetCompositePC() { return m_compositePC; }

    /** Global linetype scale ($LTSCALE) */
    void SetGlobalLtscale(float v) {
        if (std::abs(v - m_globalLtscale) > 1e-4f) {
            m_globalLtscale = (v > 0.0f) ? v : 1.0f;
            m_cadDirty = true;
        }
    }

    /** Zoom faktörü (pixel/world-unit) — screen-space lineweight için */
    void SetPixelsPerWorldUnit(float v) {
        if (std::abs(v - m_pixelsPerWorldUnit) > m_pixelsPerWorldUnit * 0.05f) {
            m_pixelsPerWorldUnit = v;
            m_cadDirty = true;
            m_gridDirty = true; // grid step zoom'la birlikte değişmeli
        }
    }

    /** Layer renk haritasını set et — ByLayer entity renklerini çözmek için */
    void SetLayerMap(const std::unordered_map<std::string, cad::Layer>& layers) {
        m_layerMap = layers;
        m_cadDirty = true; // vertex buffer'ı yeniden oluştur
    }

    // ── GPU Clash Detection ───────────────────────────────────────────────────

    struct GpuPipe {
        float posA[4];  // xyz + pad
        float posB[4];
        float radius;
        float pad[3];
    };

    struct GpuArchBox {
        float bbMin[4];
        float bbMax[4];
        uint32_t entityIdLo;
        uint32_t entityIdHi;
        uint32_t pad[2];
    };

    struct GpuClashResult {
        float clashPoint[4];
        uint32_t pipeIdx;
        uint32_t archIdx;
        uint32_t severity;   // 0=hard, 1=soft
        float overlap_mm;
    };

    /**
     * @brief GPU-accelerated clash detection.
     * Falls back gracefully if compute shader is unavailable.
     *
     * @param pipes       List of MEP pipe segments
     * @param arches      List of architectural element bounding boxes
     * @param tolerance   Soft-clash tolerance in world units (metres)
     * @param maxResults  Maximum number of results the output buffer holds
     * @return            Flat list of detected clashes
     */
    std::vector<GpuClashResult>
    RunClashDetectionGPU(const std::vector<GpuPipe>& pipes,
                         const std::vector<GpuArchBox>& arches,
                         float tolerance = 0.05f,
                         uint32_t maxResults = 4096);

    bool IsClashComputeReady() const { return m_clashComputeReady; }

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

    // MSAA
    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage        m_msaaColorImage  = VK_NULL_HANDLE;
    VkDeviceMemory m_msaaColorMemory = VK_NULL_HANDLE;
    VkImageView    m_msaaColorView   = VK_NULL_HANDLE;

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
    VkPipeline m_hatchPipeline = VK_NULL_HANDLE; ///< Alpha-blend triangle pipeline for hatch fills

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

    // CAD entity vertex buffer (arka plan çizimi) — ince çizgiler, LINE_LIST
    VkBuffer m_cadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_cadVertexMemory = VK_NULL_HANDLE;
    uint32_t m_cadLineVertexCount = 0;

    // CAD fat line vertex buffer — kalın çizgiler quad olarak, TRIANGLE_LIST
    VkBuffer m_cadFatVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_cadFatVertexMemory = VK_NULL_HANDLE;
    uint32_t m_cadFatVertexCount = 0;
    VkBuffer m_hatchVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_hatchVertexMemory = VK_NULL_HANDLE;
    uint32_t m_hatchVertexCount = 0;

    bool m_cadDirty  = true;
    bool m_gridDirty = true;

    // Cihaz limitleri — linewidth için
    float m_deviceMaxLineWidth = 1.0f;
    bool  m_deviceWideLines    = false;

    // Screen-space lineweight — MainWindow'dan zoom değeri
    float m_pixelsPerWorldUnit = 1.0f;
    // Global linetype scale ($LTSCALE)
    float m_globalLtscale = 1.0f;

    // Layer renk haritası — ByLayer entity renklerini çözmek için
    std::unordered_map<std::string, cad::Layer> m_layerMap;

    // ── SSAO ─────────────────────────────────────────────────────────────────
    struct SSAOImage {
        VkImage        image  = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
    };

    // G-buffer offscreen attachments
    SSAOImage m_gColor;     // RGBA8_UNORM  — scene color
    SSAOImage m_gPosition;  // RGBA32F      — view-space position
    SSAOImage m_gNormal;    // RGBA16F      — view-space normal (packed)
    SSAOImage m_gbufDepth;  // D32_SFLOAT   — shared depth for G-buffer pass
    SSAOImage m_ssaoRaw;    // RGBA8_UNORM  — raw AO output
    SSAOImage m_ssaoBlur;   // RGBA8_UNORM  — blurred AO

    // 4×4 RGBA noise texture for TBN rotation
    SSAOImage      m_ssaoNoise;
    VkBuffer       m_ssaoNoiseStagingBuf    = VK_NULL_HANDLE;
    VkDeviceMemory m_ssaoNoiseStagingMem    = VK_NULL_HANDLE;

    // Samplers
    VkSampler m_linearSampler  = VK_NULL_HANDLE; // clamp-to-edge, linear
    VkSampler m_nearestSampler = VK_NULL_HANDLE; // repeat, nearest (for noise)

    // Render passes
    VkRenderPass m_gbufferRenderPass   = VK_NULL_HANDLE; // 3 color + depth
    VkRenderPass m_ssaoRenderPass      = VK_NULL_HANDLE; // 1 color (ssaoRaw)
    VkRenderPass m_blurRenderPass      = VK_NULL_HANDLE; // 1 color (ssaoBlur)
    VkRenderPass m_compositeRenderPass = VK_NULL_HANDLE; // swapchain color only

    // Framebuffers
    VkFramebuffer              m_gbufferFB  = VK_NULL_HANDLE;
    VkFramebuffer              m_ssaoFB     = VK_NULL_HANDLE;
    VkFramebuffer              m_blurFB     = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_compositeFBs; // per swapchain image, no depth

    // Descriptor set layouts
    VkDescriptorSetLayout m_ssaoDescLayout      = VK_NULL_HANDLE; // set0: 3 samplers
    VkDescriptorSetLayout m_kernelDescLayout    = VK_NULL_HANDLE; // set1: kernel UBO
    VkDescriptorSetLayout m_blurDescLayout      = VK_NULL_HANDLE; // set0: 1 sampler
    VkDescriptorSetLayout m_compositeDescLayout = VK_NULL_HANDLE; // set0: 2 samplers

    // Descriptor pool + sets
    VkDescriptorPool m_ssaoDescPool     = VK_NULL_HANDLE;
    VkDescriptorSet  m_ssaoDescSet0     = VK_NULL_HANDLE; // gPos+gNorm+noise
    VkDescriptorSet  m_ssaoDescSet1     = VK_NULL_HANDLE; // kernel UBO
    VkDescriptorSet  m_blurDescSet      = VK_NULL_HANDLE; // ssaoRaw
    VkDescriptorSet  m_compositeDescSet = VK_NULL_HANDLE; // gColor+ssaoBlur

    // Kernel UBO (64 vec4 hemisphere samples)
    VkBuffer       m_kernelUboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_kernelUboMemory = VK_NULL_HANDLE;
    float          m_kernelData[64 * 4]{};

    // Pipeline layouts
    VkPipelineLayout m_gbufPipelineLayout      = VK_NULL_HANDLE;
    VkPipelineLayout m_ssaoPipelineLayout      = VK_NULL_HANDLE;
    VkPipelineLayout m_blurPipelineLayout      = VK_NULL_HANDLE;
    VkPipelineLayout m_compositePipelineLayout = VK_NULL_HANDLE;

    // Pipelines
    VkPipeline m_gbufLinePipeline  = VK_NULL_HANDLE;
    VkPipeline m_gbufTriPipeline   = VK_NULL_HANDLE;
    VkPipeline m_ssaoPipeline      = VK_NULL_HANDLE;
    VkPipeline m_blurPipeline      = VK_NULL_HANDLE;
    VkPipeline m_compositePipeline = VK_NULL_HANDLE;

    bool m_ssaoInitialized = false;

    // PBR
    CompositePushConstants m_compositePC;

    // Gizmo
    Gizmo          m_gizmo;
    bool           m_gizmoVisible       = false;
    VkBuffer       m_gizmoVertexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory m_gizmoVertexMemory  = VK_NULL_HANDLE;
    uint32_t       m_gizmoVertexCount   = 0;

    // Scene state
    const mep::NetworkGraph* m_lastNetwork = nullptr; // set each Render() call, used by Pick()
    geom::Camera m_camera;
    geom::Mat4 m_viewProjection;
    ViewMode m_viewMode = ViewMode::Plan;

    bool m_initialized = false;
    bool m_frameActive = false;       // true only between successful BeginFrame..EndFrame
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
    bool CreateMSAAResources();
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
    void UpdateCADVertexData(const std::vector<std::unique_ptr<cad::Entity>>& entities);
    void DrawCAD(VkCommandBuffer cmd);
    void DrawGizmoLines(VkCommandBuffer cmd);

    // G-Buffer draw variants (SSAO path)
    void DrawGridGBuffer(VkCommandBuffer cmd);
    void DrawNetworkGBuffer(VkCommandBuffer cmd);

    // ── Clash compute pipeline ────────────────────────────────────────────────
    VkPipeline            m_clashComputePipeline  = VK_NULL_HANDLE;
    VkPipelineLayout      m_clashComputeLayout    = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_clashComputeDSLayout  = VK_NULL_HANDLE;
    VkDescriptorPool      m_clashComputePool      = VK_NULL_HANDLE;
    bool                  m_clashComputeReady     = false;

    bool InitializeClashCompute();
    void ShutdownClashCompute();

    // SSAO lifecycle
    bool InitializeSSAO();
    void ShutdownSSAO();

    // SSAO helpers
    bool CreateSSAOImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage,
                         VkImageAspectFlags aspect, SSAOImage& out);
    void DestroySSAOImage(SSAOImage& img);
    bool UploadNoiseTexture();
    void GenerateKernel();
    VkRenderPass CreateSingleColorRenderPass(VkFormat fmt,
                                             VkImageLayout initialLayout,
                                             VkImageLayout finalLayout);
    bool CreateSSAOPipelines(VkShaderModule fsVert, VkShaderModule ssaoFrag,
                             VkShaderModule blurFrag, VkShaderModule compFrag);

    // Shader
    VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code);
    static std::vector<uint32_t> LoadShaderSPV(const char* filename);

    // Node renkleri
    static std::array<float, 3> GetNodeColor(mep::NodeType type);
    static std::array<float, 3> GetEdgeColor(mep::EdgeType type);
    static std::array<float, 3> GetACIColor(int colorIndex);
};

} // namespace render
} // namespace vkt
