/**
 * @file VulkanRenderer.cpp
 * @brief Vulkan Renderer implementasyonu — gerçek GPU rendering
 */

#include "render/VulkanRenderer.hpp"
#include "cad/Line.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include "cad/Ellipse.hpp"
#include "cad/Polyline.hpp"
#include "cad/Dimension.hpp"
#include "cad/Hatch.hpp"
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <random>

static void LogCAD(const std::string& msg) {
    std::cout << "[VulkanRenderer] " << msg << std::endl;
}
static int g_cadFrameCounter = 0;

#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
#include <windows.h>
#endif

// Embedded SPIR-V shaders (glslc ile derlenmiş)
// basic.vert -> SPIR-V
static const uint32_t VERT_SHADER_SPV[] = {
    // #version 450
    // layout(location=0) in vec3 inPos;
    // layout(location=1) in vec3 inNormal;
    // layout(location=2) in vec3 inColor;
    // layout(push_constant) uniform PC { mat4 mvp; } pc;
    // layout(location=0) out vec3 fragColor;
    // void main() { gl_Position = pc.mvp * vec4(inPos,1.0); fragColor = inColor; }
    0x07230203, 0x00010000, 0x0008000b, 0x00000028,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x000a000f, 0x00000000, 0x00000004, 0x6e69616d,
    0x00000000, 0x0000000d, 0x00000011, 0x0000001d,
    0x00000024, 0x00000026, 0x00030003, 0x00000002,
    0x000001c2, 0x000a0004, 0x475f4c47, 0x4c474f4f,
    0x70635f45, 0x74735f70, 0x5f656c79, 0x656e696c,
    0x7269645f, 0x69746365, 0x00006576, 0x00080004,
    0x475f4c47, 0x4c474f4f, 0x6e695f45, 0x64756c63,
    0x69645f65, 0x74636572, 0x00657669, 0x00040005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00060005,
    0x0000000b, 0x505f6c67, 0x65567265, 0x78657472,
    0x00000000, 0x00060006, 0x0000000b, 0x00000000,
    0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006,
    0x0000000b, 0x00000001, 0x505f6c67, 0x746e696f,
    0x657a6953, 0x00000000, 0x00070006, 0x0000000b,
    0x00000002, 0x435f6c67, 0x4470696c, 0x61747369,
    0x0065636e, 0x00070006, 0x0000000b, 0x00000003,
    0x435f6c67, 0x446c6c75, 0x61747369, 0x0065636e,
    0x00030005, 0x0000000d, 0x00000000, 0x00060005,
    0x0000000f, 0x68737550, 0x736e6f43, 0x746e6174,
    0x00000073, 0x00050006, 0x0000000f, 0x00000000,
    0x0070766d, 0x00000000, 0x00030005, 0x00000011,
    0x00006370, 0x00040005, 0x0000001d, 0x6f506e69,
    0x00000073, 0x00050005, 0x00000024, 0x67617266,
    0x6f6c6f43, 0x00000072, 0x00050005, 0x00000026,
    0x6f436e69, 0x00726f6c, 0x00000000, 0x00050048,
    0x0000000b, 0x00000000, 0x0000000b, 0x00000000,
    0x00050048, 0x0000000b, 0x00000001, 0x0000000b,
    0x00000001, 0x00050048, 0x0000000b, 0x00000002,
    0x0000000b, 0x00000003, 0x00050048, 0x0000000b,
    0x00000003, 0x0000000b, 0x00000004, 0x00030047,
    0x0000000b, 0x00000002, 0x00040048, 0x0000000f,
    0x00000000, 0x00000005, 0x00050048, 0x0000000f,
    0x00000000, 0x00000023, 0x00000000, 0x00050048,
    0x0000000f, 0x00000000, 0x00000007, 0x00000010,
    0x00030047, 0x0000000f, 0x00000002, 0x00040047,
    0x0000001d, 0x0000001e, 0x00000000, 0x00040047,
    0x00000024, 0x0000001e, 0x00000000, 0x00040047,
    0x00000026, 0x0000001e, 0x00000002, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00030016, 0x00000006, 0x00000020, 0x00040017,
    0x00000007, 0x00000006, 0x00000004, 0x00040015,
    0x00000008, 0x00000020, 0x00000000, 0x0004002b,
    0x00000008, 0x00000009, 0x00000001, 0x0004001c,
    0x0000000a, 0x00000006, 0x00000009, 0x0006001e,
    0x0000000b, 0x00000007, 0x00000006, 0x0000000a,
    0x0000000a, 0x00040020, 0x0000000c, 0x00000003,
    0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d,
    0x00000003, 0x00040015, 0x0000000e, 0x00000020,
    0x00000001, 0x00040018, 0x00000010, 0x00000007,
    0x00000004, 0x0003001e, 0x0000000f, 0x00000010,
    0x00040020, 0x00000012, 0x00000009, 0x0000000f,
    0x0004003b, 0x00000012, 0x00000011, 0x00000009,
    0x0004002b, 0x0000000e, 0x00000013, 0x00000000,
    0x00040020, 0x00000014, 0x00000009, 0x00000010,
    0x00040017, 0x0000001b, 0x00000006, 0x00000003,
    0x00040020, 0x0000001c, 0x00000001, 0x0000001b,
    0x0004003b, 0x0000001c, 0x0000001d, 0x00000001,
    0x0004002b, 0x00000006, 0x0000001f, 0x3f800000,
    0x00040020, 0x00000023, 0x00000003, 0x0000001b,
    0x0004003b, 0x00000023, 0x00000024, 0x00000003,
    0x0004003b, 0x0000001c, 0x00000026, 0x00000001,
    0x00040020, 0x00000021, 0x00000003, 0x00000007,
    0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x00050041,
    0x00000014, 0x00000015, 0x00000011, 0x00000013,
    0x0004003d, 0x00000010, 0x00000016, 0x00000015,
    0x0004003d, 0x0000001b, 0x0000001e, 0x0000001d,
    0x00050051, 0x00000006, 0x00000019, 0x0000001e,
    0x00000000, 0x00050051, 0x00000006, 0x0000001a,
    0x0000001e, 0x00000001, 0x00050051, 0x00000006,
    0x00000020, 0x0000001e, 0x00000002, 0x00070050,
    0x00000007, 0x00000022, 0x00000019, 0x0000001a,
    0x00000020, 0x0000001f, 0x00050091, 0x00000007,
    0x00000017, 0x00000016, 0x00000022, 0x00050041,
    0x00000021, 0x00000018, 0x0000000d, 0x00000013,
    0x0003003e, 0x00000018, 0x00000017, 0x0004003d,
    0x0000001b, 0x00000027, 0x00000026, 0x0003003e,
    0x00000024, 0x00000027, 0x000100fd, 0x00010038
};

// basic.frag -> SPIR-V
static const uint32_t FRAG_SHADER_SPV[] = {
    // #version 450
    // layout(location=0) in vec3 fragColor;
    // layout(location=0) out vec4 outColor;
    // void main() { outColor = vec4(fragColor, 1.0); }
    0x07230203, 0x00010000, 0x0008000b, 0x00000013,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000004, 0x6e69616d,
    0x00000000, 0x00000009, 0x0000000b, 0x00030010,
    0x00000004, 0x00000007, 0x00030003, 0x00000002,
    0x000001c2, 0x000a0004, 0x475f4c47, 0x4c474f4f,
    0x70635f45, 0x74735f70, 0x5f656c79, 0x656e696c,
    0x7269645f, 0x69746365, 0x00006576, 0x00080004,
    0x475f4c47, 0x4c474f4f, 0x6e695f45, 0x64756c63,
    0x69645f65, 0x74636572, 0x00657669, 0x00040005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00050005,
    0x00000009, 0x4374756f, 0x726f6c6f, 0x00000000,
    0x00050005, 0x0000000b, 0x67617266, 0x6f6c6f43,
    0x00000072, 0x00040047, 0x00000009, 0x0000001e,
    0x00000000, 0x00040047, 0x0000000b, 0x0000001e,
    0x00000000, 0x00020013, 0x00000002, 0x00030021,
    0x00000003, 0x00000002, 0x00030016, 0x00000006,
    0x00000020, 0x00040017, 0x00000007, 0x00000006,
    0x00000004, 0x00040020, 0x00000008, 0x00000003,
    0x00000007, 0x0004003b, 0x00000008, 0x00000009,
    0x00000003, 0x00040017, 0x0000000a, 0x00000006,
    0x00000003, 0x00040020, 0x0000000c, 0x00000001,
    0x0000000a, 0x0004003b, 0x0000000c, 0x0000000b,
    0x00000001, 0x0004002b, 0x00000006, 0x0000000e,
    0x3f800000, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200f8, 0x00000005,
    0x0004003d, 0x0000000a, 0x0000000d, 0x0000000b,
    0x00050051, 0x00000006, 0x0000000f, 0x0000000d,
    0x00000000, 0x00050051, 0x00000006, 0x00000010,
    0x0000000d, 0x00000001, 0x00050051, 0x00000006,
    0x00000011, 0x0000000d, 0x00000002, 0x00070050,
    0x00000007, 0x00000012, 0x0000000f, 0x00000010,
    0x00000011, 0x0000000e, 0x0003003e, 0x00000009,
    0x00000012, 0x000100fd, 0x00010038
};

namespace vkt {
namespace render {

namespace {
    constexpr float PI_F = 3.14159265f;
}

VulkanRenderer::VulkanRenderer() {
}

VulkanRenderer::~VulkanRenderer() {
    Shutdown();
}

bool VulkanRenderer::Initialize(void* windowHandle) {
    if (m_initialized) return true;

    m_windowHandle = windowHandle;
    std::cout << "Vulkan baslatiliyor..." << std::endl;

    try {
        if (!CreateVulkanInstance()) throw std::runtime_error("Instance olusturulamadi");
        if (!CreateSurface()) throw std::runtime_error("Surface olusturulamadi");
        if (!PickPhysicalDevice()) throw std::runtime_error("Physical device bulunamadi");
        if (!CreateLogicalDevice()) throw std::runtime_error("Logical device olusturulamadi");
        if (!CreateSwapchain()) throw std::runtime_error("Swapchain olusturulamadi");
        if (!CreateImageViews()) throw std::runtime_error("ImageView olusturulamadi");
        if (!CreateMSAAResources()) throw std::runtime_error("MSAA resources olusturulamadi");
        if (!CreateDepthResources()) throw std::runtime_error("Depth buffer olusturulamadi");
        if (!CreateRenderPass()) throw std::runtime_error("RenderPass olusturulamadi");
        if (!CreateFramebuffers()) throw std::runtime_error("Framebuffer olusturulamadi");
        if (!CreatePipelineLayout()) throw std::runtime_error("PipelineLayout olusturulamadi");
        if (!CreateGraphicsPipelines()) throw std::runtime_error("Pipeline olusturulamadi");
        if (!CreateCommandPool()) throw std::runtime_error("CommandPool olusturulamadi");
        if (!CreateCommandBuffers()) throw std::runtime_error("CommandBuffer olusturulamadi");
        if (!CreateSyncObjects()) throw std::runtime_error("Sync nesneleri olusturulamadi");

        CreateGridVertexData();

        // SSAO — optional, disabled if shaders not found
        InitializeSSAO();
        // Clash compute — optional, disabled if shader not found
        InitializeClashCompute();

        m_initialized = true;
        std::cout << "Vulkan basariyla baslatildi." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Vulkan baslatma hatasi: " << e.what() << std::endl;
        Shutdown();
        return false;
    }
}

void VulkanRenderer::Shutdown() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
    ShutdownSSAO();
    ShutdownClashCompute();

    // Vertex buffers
    DestroyBuffer(m_vertexBuffer, m_vertexMemory);
    DestroyBuffer(m_gridVertexBuffer, m_gridVertexMemory);
    DestroyBuffer(m_cadVertexBuffer, m_cadVertexMemory);
    DestroyBuffer(m_cadFatVertexBuffer, m_cadFatVertexMemory);
    DestroyBuffer(m_hatchVertexBuffer, m_hatchVertexMemory);
    DestroyBuffer(m_gizmoVertexBuffer, m_gizmoVertexMemory);

    // Sync objects
    for (auto sem : m_imageAvailableSemaphores) vkDestroySemaphore(m_device, sem, nullptr);
    for (auto sem : m_renderFinishedSemaphores) vkDestroySemaphore(m_device, sem, nullptr);
    for (auto fence : m_inFlightFences) vkDestroyFence(m_device, fence, nullptr);
    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();

    if (m_commandPool) { vkDestroyCommandPool(m_device, m_commandPool, nullptr); m_commandPool = VK_NULL_HANDLE; }

    CleanupSwapchain();

    if (m_linePipeline) { vkDestroyPipeline(m_device, m_linePipeline, nullptr); m_linePipeline = VK_NULL_HANDLE; }
    if (m_trianglePipeline) { vkDestroyPipeline(m_device, m_trianglePipeline, nullptr); m_trianglePipeline = VK_NULL_HANDLE; }
    if (m_hatchPipeline) { vkDestroyPipeline(m_device, m_hatchPipeline, nullptr); m_hatchPipeline = VK_NULL_HANDLE; }
    if (m_pipelineLayout) { vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr); m_pipelineLayout = VK_NULL_HANDLE; }
    if (m_renderPass) { vkDestroyRenderPass(m_device, m_renderPass, nullptr); m_renderPass = VK_NULL_HANDLE; }

    if (m_device) { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; }
    if (m_surface) { vkDestroySurfaceKHR(m_instance, m_surface, nullptr); m_surface = VK_NULL_HANDLE; }
    if (m_instance) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }

    m_initialized = false;
    std::cout << "Vulkan kapatildi." << std::endl;
}

void VulkanRenderer::OnResize(int width, int height) {
    m_width = width;
    m_height = height;
    m_framebufferResized = true;
}

// ============================================================
// FRAME RENDERING
// ============================================================

void VulkanRenderer::BeginFrame() {
    m_frameActive = false;
    if (!m_initialized) return;

    if (m_gridDirty) {
        CreateGridVertexData();
        m_gridDirty = false;
    }

    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    }

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass;
    rpInfo.framebuffer = m_framebuffers[m_imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_swapchainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Koyu gri arka plan
    clearValues[1].depthStencil = {1.0f, 0};

    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    const bool useSSAO = m_ssaoInitialized && m_viewMode != ViewMode::Plan;

    if (useSSAO) {
        // G-buffer pass: 3 color attachments + depth
        std::array<VkClearValue, 4> gbufClear{};
        gbufClear[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}}; // gColor
        gbufClear[1].color        = {{0.0f, 0.0f, 0.0f, 0.0f}};    // gPosition
        gbufClear[2].color        = {{0.5f, 0.5f, 1.0f, 0.0f}};    // gNormal (up)
        gbufClear[3].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo gbufRP{};
        gbufRP.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        gbufRP.renderPass        = m_gbufferRenderPass;
        gbufRP.framebuffer       = m_gbufferFB;
        gbufRP.renderArea.offset = {0, 0};
        gbufRP.renderArea.extent = m_swapchainExtent;
        gbufRP.clearValueCount   = static_cast<uint32_t>(gbufClear.size());
        gbufRP.pClearValues      = gbufClear.data();
        vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &gbufRP, VK_SUBPASS_CONTENTS_INLINE);
    } else {
        vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Viewport & scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);

    m_frameActive = true;
}

void VulkanRenderer::Render(const mep::NetworkGraph& network) {
    if (!m_initialized || !m_frameActive) return;
    m_lastNetwork = &network;

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    const bool useSSAO = m_ssaoInitialized && m_viewMode != ViewMode::Plan;

    UpdateNetworkVertexData(network);

    if (useSSAO) {
        DrawGridGBuffer(cmd);
        DrawNetworkGBuffer(cmd);
    } else {
        DrawGrid(cmd);
        DrawNetwork(cmd, network);
    }

    DrawGizmoLines(cmd);
}

void VulkanRenderer::EndFrame() {
    if (!m_initialized || !m_frameActive) return;
    m_frameActive = false;

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    const bool useSSAO = m_ssaoInitialized && m_viewMode != ViewMode::Plan;

    vkCmdEndRenderPass(cmd);

    if (useSSAO) {
        // ── SSAO pass ──────────────────────────────────────────────────────
        VkClearValue ssaoClear{};
        ssaoClear.color = {{1.0f, 0.0f, 0.0f, 1.0f}}; // default AO = 1 (no occlusion)

        VkRenderPassBeginInfo ssaoRP{};
        ssaoRP.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        ssaoRP.renderPass        = m_ssaoRenderPass;
        ssaoRP.framebuffer       = m_ssaoFB;
        ssaoRP.renderArea.offset = {0, 0};
        ssaoRP.renderArea.extent = m_swapchainExtent;
        ssaoRP.clearValueCount   = 1;
        ssaoRP.pClearValues      = &ssaoClear;
        vkCmdBeginRenderPass(cmd, &ssaoRP, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline);

        VkDescriptorSet ssaoSets[] = {m_ssaoDescSet0, m_ssaoDescSet1};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_ssaoPipelineLayout, 0, 2, ssaoSets, 0, nullptr);

        SSAOPushConstants ssaoPC{};
        ssaoPC.projection  = m_camera.GetProjectionMatrix();
        ssaoPC.radius      = 0.35f;  // tighter = crisper contact shadows
        ssaoPC.bias        = 0.015f; // lower = less false darkening on flat surfaces
        ssaoPC.power       = 1.8f;   // softer contrast, fits technical/CAD look
        ssaoPC.noiseScaleX = static_cast<float>(m_swapchainExtent.width)  / 4.0f;
        ssaoPC.noiseScaleY = static_cast<float>(m_swapchainExtent.height) / 4.0f;
        vkCmdPushConstants(cmd, m_ssaoPipelineLayout,
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(SSAOPushConstants), &ssaoPC);

        vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen triangle
        vkCmdEndRenderPass(cmd);

        // ── Blur pass ──────────────────────────────────────────────────────
        VkRenderPassBeginInfo blurRP{};
        blurRP.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        blurRP.renderPass        = m_blurRenderPass;
        blurRP.framebuffer       = m_blurFB;
        blurRP.renderArea.offset = {0, 0};
        blurRP.renderArea.extent = m_swapchainExtent;
        blurRP.clearValueCount   = 1;
        blurRP.pClearValues      = &ssaoClear;
        vkCmdBeginRenderPass(cmd, &blurRP, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_blurPipelineLayout, 0, 1, &m_blurDescSet, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        // ── Composite pass → swapchain ─────────────────────────────────────
        VkClearValue compClear{};
        compClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo compRP{};
        compRP.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        compRP.renderPass        = m_compositeRenderPass;
        compRP.framebuffer       = m_compositeFBs[m_imageIndex];
        compRP.renderArea.offset = {0, 0};
        compRP.renderArea.extent = m_swapchainExtent;
        compRP.clearValueCount   = 1;
        compRP.pClearValues      = &compClear;
        vkCmdBeginRenderPass(cmd, &compRP, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.width    = static_cast<float>(m_swapchainExtent.width);
        vp.height   = static_cast<float>(m_swapchainExtent.height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D sc{};
        sc.extent = m_swapchainExtent;
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_compositePipelineLayout, 0, 1, &m_compositeDescSet, 0, nullptr);
        vkCmdPushConstants(cmd, m_compositePipelineLayout,
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(CompositePushConstants), &m_compositePC);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    vkEndCommandBuffer(cmd);

    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        RecreateSwapchain();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::SetCamera(const geom::Camera& camera) {
    m_camera = camera;
}

void VulkanRenderer::SetViewMode(ViewMode mode) {
    m_viewMode = mode;
}

uint32_t VulkanRenderer::Pick(int screenX, int screenY) {
    if (!m_lastNetwork) return 0;

    const float NODE_PICK_PX = 8.0f;
    const float EDGE_PICK_PX = 5.0f;
    const float w = static_cast<float>(m_width);
    const float h = static_cast<float>(m_height);

    // Node picking — project world position to screen, find nearest within threshold
    uint32_t bestNodeId = 0;
    float    bestNodeDist = NODE_PICK_PX;

    for (const auto& [id, node] : m_lastNetwork->GetNodeMap()) {
        geom::Vec3 s = m_camera.WorldToScreen(node.position, w, h);
        float dx = s.x - static_cast<float>(screenX);
        float dy = s.y - static_cast<float>(screenY);
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < bestNodeDist) {
            bestNodeDist = dist;
            bestNodeId   = id;
        }
    }
    if (bestNodeId != 0) return bestNodeId;

    // Edge picking — point-to-segment distance in screen space (bit 31 = edge flag)
    uint32_t bestEdgeId   = 0;
    float    bestEdgeDist = EDGE_PICK_PX;

    for (const auto& [id, edge] : m_lastNetwork->GetEdgeMap()) {
        const mep::Node* nA = m_lastNetwork->GetNode(edge.nodeA);
        const mep::Node* nB = m_lastNetwork->GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        geom::Vec3 sA = m_camera.WorldToScreen(nA->position, w, h);
        geom::Vec3 sB = m_camera.WorldToScreen(nB->position, w, h);

        float ex   = sB.x - sA.x;
        float ey   = sB.y - sA.y;
        float len2 = ex*ex + ey*ey;

        float dist;
        if (len2 < 0.001f) {
            float dx = sA.x - screenX, dy = sA.y - screenY;
            dist = std::sqrt(dx*dx + dy*dy);
        } else {
            float t  = ((screenX - sA.x)*ex + (screenY - sA.y)*ey) / len2;
            t        = std::max(0.0f, std::min(1.0f, t));
            float px = sA.x + t*ex;
            float py = sA.y + t*ey;
            float dx = px - screenX, dy = py - screenY;
            dist = std::sqrt(dx*dx + dy*dy);
        }

        if (dist < bestEdgeDist) {
            bestEdgeDist = dist;
            bestEdgeId   = id;
        }
    }

    // Encode: bit 31 set → edge ID; bit 31 clear → node ID
    return bestEdgeId != 0 ? (bestEdgeId | 0x80000000u) : 0u;
}

// ── Gizmo picking ─────────────────────────────────────────────────────────────

GizmoAxis VulkanRenderer::PickGizmo(int screenX, int screenY) const {
    if (!m_gizmoVisible) return GizmoAxis::None;
    geom::Ray ray = m_camera.ScreenPointToRay(
        static_cast<float>(screenX), static_cast<float>(screenY),
        static_cast<float>(m_width),  static_cast<float>(m_height));
    return m_gizmo.HitTest(ray);
}

// ── Gizmo draw ────────────────────────────────────────────────────────────────

void VulkanRenderer::DrawGizmoLines(VkCommandBuffer cmd) {
    if (!m_gizmoVisible || !m_linePipeline) return;

    std::vector<GizmoVertex> gverts;
    std::vector<uint32_t>    gidx;
    m_gizmo.GenerateGeometry(gverts, gidx);
    if (gverts.empty()) return;

    // Convert GizmoVertex → Vertex (zero normals) and pack into line pairs via indices
    std::vector<geom::Vertex> verts;
    verts.reserve(gidx.size());
    for (uint32_t i : gidx) {
        const GizmoVertex& gv = gverts[i];
        geom::Vertex v{};
        v.pos[0] = static_cast<float>(gv.position.x);
        v.pos[1] = static_cast<float>(gv.position.y);
        v.pos[2] = static_cast<float>(gv.position.z);
        v.normal[0] = 0.0f; v.normal[1] = 0.0f; v.normal[2] = 1.0f;
        v.color[0] = static_cast<float>(gv.color.x);
        v.color[1] = static_cast<float>(gv.color.y);
        v.color[2] = static_cast<float>(gv.color.z);
        verts.push_back(v);
    }

    VkDeviceSize bufSize = sizeof(geom::Vertex) * verts.size();

    // Reallocate gizmo buffer if needed
    if (m_gizmoVertexBuffer == VK_NULL_HANDLE || verts.size() > m_gizmoVertexCount) {
        DestroyBuffer(m_gizmoVertexBuffer, m_gizmoVertexMemory);
        CreateBuffer(bufSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_gizmoVertexBuffer, m_gizmoVertexMemory);
        m_gizmoVertexCount = static_cast<uint32_t>(verts.size());
    }

    void* data;
    vkMapMemory(m_device, m_gizmoVertexMemory, 0, bufSize, 0, &data);
    std::memcpy(data, verts.data(), bufSize);
    vkUnmapMemory(m_device, m_gizmoVertexMemory);

    PushConstants pc{};
    pc.mvp = m_camera.GetProjectionMatrix() * m_camera.GetViewMatrix();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_gizmoVertexBuffer, &offset);
    vkCmdDraw(cmd, static_cast<uint32_t>(verts.size()), 1, 0, 0);
}

// ============================================================
// VULKAN SETUP
// ============================================================

bool VulkanRenderer::CreateVulkanInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VKT Mekanik Tesisat CAD";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VKT Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    return vkCreateInstance(&createInfo, nullptr, &m_instance) == VK_SUCCESS;
}

bool VulkanRenderer::CreateSurface() {
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd = reinterpret_cast<HWND>(m_windowHandle);
    surfaceInfo.hinstance = GetModuleHandle(nullptr);

    return vkCreateWin32SurfaceKHR(m_instance, &surfaceInfo, nullptr, &m_surface) == VK_SUCCESS;
#else
    return false;
#endif
}

bool VulkanRenderer::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        // Queue family kontrolü
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        bool hasGraphics = false;
        bool hasPresent = false;

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_graphicsQueueFamily = i;
                hasGraphics = true;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (presentSupport) {
                m_presentQueueFamily = i;
                hasPresent = true;
            }

            if (hasGraphics && hasPresent) break;
        }

        if (hasGraphics && hasPresent) {
            m_physicalDevice = device;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            std::cout << "GPU: " << props.deviceName << std::endl;

            // Linewidth limitleri sorgula
            VkPhysicalDeviceFeatures feats{};
            vkGetPhysicalDeviceFeatures(device, &feats);
            m_deviceWideLines    = (feats.wideLines == VK_TRUE);
            m_deviceMaxLineWidth = props.limits.lineWidthRange[1];
            std::cout << "[VulkanRenderer] wideLines=" << m_deviceWideLines
                      << " maxLineWidth=" << m_deviceMaxLineWidth << std::endl;

            // MSAA: cihazın desteklediği max sample count, 4x ile sınırla
            VkSampleCountFlags counts =
                props.limits.framebufferColorSampleCounts &
                props.limits.framebufferDepthSampleCounts;
            if (counts & VK_SAMPLE_COUNT_4_BIT)
                m_msaaSamples = VK_SAMPLE_COUNT_4_BIT;
            else if (counts & VK_SAMPLE_COUNT_2_BIT)
                m_msaaSamples = VK_SAMPLE_COUNT_2_BIT;
            else
                m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
            std::cout << "[VulkanRenderer] MSAA=" << m_msaaSamples << "x" << std::endl;
            return true;
        }
    }
    return false;
}

bool VulkanRenderer::CreateLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;

    // Benzersiz queue family'ler
    std::vector<uint32_t> uniqueFamilies;
    uniqueFamilies.push_back(m_graphicsQueueFamily);
    if (m_presentQueueFamily != m_graphicsQueueFamily) {
        uniqueFamilies.push_back(m_presentQueueFamily);
    }

    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.wideLines       = m_deviceWideLines ? VK_TRUE : VK_FALSE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    return true;
}

bool VulkanRenderer::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    // Format seçimi
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    m_swapchainFormat = formats[0].format;
    VkColorSpaceKHR colorSpace = formats[0].colorSpace;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_swapchainFormat = f.format;
            colorSpace = f.colorSpace;
            break;
        }
    }

    // Extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        m_swapchainExtent = capabilities.currentExtent;
    } else {
        m_swapchainExtent.width = std::clamp(static_cast<uint32_t>(m_width),
            capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        m_swapchainExtent.height = std::clamp(static_cast<uint32_t>(m_height),
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_swapchainFormat;
    createInfo.imageColorSpace = colorSpace;
    createInfo.imageExtent = m_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (m_graphicsQueueFamily != m_presentQueueFamily) {
        uint32_t queueFamilyIndices[] = {m_graphicsQueueFamily, m_presentQueueFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        return false;
    }

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    return true;
}

void VulkanRenderer::CleanupSwapchain() {
    if (m_msaaColorView)   { vkDestroyImageView(m_device, m_msaaColorView, nullptr);   m_msaaColorView   = VK_NULL_HANDLE; }
    if (m_msaaColorImage)  { vkDestroyImage(m_device, m_msaaColorImage, nullptr);       m_msaaColorImage  = VK_NULL_HANDLE; }
    if (m_msaaColorMemory) { vkFreeMemory(m_device, m_msaaColorMemory, nullptr);        m_msaaColorMemory = VK_NULL_HANDLE; }

    if (m_depthImageView) { vkDestroyImageView(m_device, m_depthImageView, nullptr); m_depthImageView = VK_NULL_HANDLE; }
    if (m_depthImage) { vkDestroyImage(m_device, m_depthImage, nullptr); m_depthImage = VK_NULL_HANDLE; }
    if (m_depthMemory) { vkFreeMemory(m_device, m_depthMemory, nullptr); m_depthMemory = VK_NULL_HANDLE; }

    for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();

    for (auto iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();

    if (m_swapchain) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
}

void VulkanRenderer::RecreateSwapchain() {
    vkDeviceWaitIdle(m_device);
    ShutdownSSAO();
    CleanupSwapchain();
    CreateSwapchain();
    CreateImageViews();
    CreateMSAAResources();
    CreateDepthResources();
    CreateFramebuffers();
    InitializeSSAO();
}

bool VulkanRenderer::CreateImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CreateMSAAResources() {
    if (m_msaaSamples == VK_SAMPLE_COUNT_1_BIT) return true; // MSAA devre dışı

    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = m_swapchainFormat;
    ci.extent        = {m_swapchainExtent.width, m_swapchainExtent.height, 1};
    ci.mipLevels     = 1;
    ci.arrayLayers   = 1;
    ci.samples       = m_msaaSamples;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(m_device, &ci, nullptr, &m_msaaColorImage) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(m_device, m_msaaColorImage, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = FindMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_device, &ai, nullptr, &m_msaaColorMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(m_device, m_msaaColorImage, m_msaaColorMemory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image                           = m_msaaColorImage;
    vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    vi.format                          = m_swapchainFormat;
    vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount     = 1;
    vi.subresourceRange.layerCount     = 1;
    return vkCreateImageView(m_device, &vi, nullptr, &m_msaaColorView) == VK_SUCCESS;
}

bool VulkanRenderer::CreateDepthResources() {
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_swapchainExtent.width;
    imageInfo.extent.height = m_swapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = m_msaaSamples;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) return false;

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_depthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(m_device, m_depthImage, m_depthMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) == VK_SUCCESS;
}

bool VulkanRenderer::CreateRenderPass() {
    bool useMSAA = (m_msaaSamples != VK_SAMPLE_COUNT_1_BIT);

    // Attachment 0: MSAA color (veya swapchain direkt, MSAA kapalıysa)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_swapchainFormat;
    colorAttachment.samples        = m_msaaSamples;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = useMSAA ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = useMSAA ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                             : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Attachment 1: Depth (MSAA sample count)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = m_msaaSamples;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachment 2: Resolve (swapchain, 1 sample) — sadece MSAA açıkken
    VkAttachmentDescription resolveAttachment{};
    VkAttachmentReference   resolveRef{};
    if (useMSAA) {
        resolveAttachment.format         = m_swapchainFormat;
        resolveAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        resolveRef.attachment = 2;
        resolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    subpass.pResolveAttachments     = useMSAA ? &resolveRef : nullptr;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments = {colorAttachment, depthAttachment};
    if (useMSAA) attachments.push_back(resolveAttachment);

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    return vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass) == VK_SUCCESS;
}

bool VulkanRenderer::CreateFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());
    bool useMSAA = (m_msaaSamples != VK_SAMPLE_COUNT_1_BIT);

    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        // MSAA: [msaaColor, depth, resolve(swapchain)]
        // No MSAA: [swapchain, depth]
        std::vector<VkImageView> attachments;
        if (useMSAA) {
            attachments = {m_msaaColorView, m_depthImageView, m_swapchainImageViews[i]};
        } else {
            attachments = {m_swapchainImageViews[i], m_depthImageView};
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = m_swapchainExtent.width;
        fbInfo.height          = m_swapchainExtent.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CreatePipelineLayout() {
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;

    return vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) == VK_SUCCESS;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule module;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

bool VulkanRenderer::CreateGraphicsPipelines() {
    // Shader modülleri
    std::vector<uint32_t> vertCode(VERT_SHADER_SPV, VERT_SHADER_SPV + sizeof(VERT_SHADER_SPV) / sizeof(uint32_t));
    std::vector<uint32_t> fragCode(FRAG_SHADER_SPV, FRAG_SHADER_SPV + sizeof(FRAG_SHADER_SPV) / sizeof(uint32_t));

    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (!vertModule || !fragModule) return false;

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // Vertex input
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(geom::Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(geom::Vertex, pos);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(geom::Vertex, normal);

    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(geom::Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    // Input assembly — LINE_LIST için
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport & scissor
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 2.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = m_msaaSamples;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlend{};
    colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlend.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlend;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    if (m_deviceWideLines)
        dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    // Line pipeline
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_linePipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
        return false;
    }

    // Triangle pipeline
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    rasterizer.lineWidth = 1.0f;
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_trianglePipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
        return false;
    }

    // Hatch fill pipeline — same as triangle but with src_constant_alpha blend
    // Uses blend constants[3] = 0.3 for 30% opacity hatch fill
    VkPipelineColorBlendAttachmentState hatchBlend{};
    hatchBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    hatchBlend.blendEnable         = VK_TRUE;
    hatchBlend.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
    hatchBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    hatchBlend.colorBlendOp        = VK_BLEND_OP_ADD;
    hatchBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    hatchBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    hatchBlend.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo hatchColorBlending{};
    hatchColorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    hatchColorBlending.logicOpEnable     = VK_FALSE;
    hatchColorBlending.attachmentCount   = 1;
    hatchColorBlending.pAttachments      = &hatchBlend;
    hatchColorBlending.blendConstants[0] = 1.0f;
    hatchColorBlending.blendConstants[1] = 1.0f;
    hatchColorBlending.blendConstants[2] = 1.0f;
    hatchColorBlending.blendConstants[3] = 0.3f; // 30% opacity

    pipelineInfo.pColorBlendState = &hatchColorBlending;
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_hatchPipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
        return false;
    }
    pipelineInfo.pColorBlendState = &colorBlending; // restore

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    return true;
}

bool VulkanRenderer::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

    return vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) == VK_SUCCESS;
}

bool VulkanRenderer::CreateCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    return vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) == VK_SUCCESS;
}

bool VulkanRenderer::CreateSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

// ============================================================
// VERTEX BUFFER & DRAWING
// ============================================================

bool VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties,
                                   VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

void VulkanRenderer::DestroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer && m_device) { vkDestroyBuffer(m_device, buffer, nullptr); buffer = VK_NULL_HANDLE; }
    if (memory && m_device) { vkFreeMemory(m_device, memory, nullptr); memory = VK_NULL_HANDLE; }
}

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void VulkanRenderer::CreateGridVertexData() {
    // Viewport-aware grid: step ve extent zoom'a göre hesaplanır.
    // Hedef: ekranda 30-80 grid çizgisi görünsün.
    float ppu = std::max(m_pixelsPerWorldUnit, 1e-6f);
    float vwPx = (m_swapchainExtent.width  > 0) ? static_cast<float>(m_swapchainExtent.width)  : 1280.0f;
    float vhPx = (m_swapchainExtent.height > 0) ? static_cast<float>(m_swapchainExtent.height) : 720.0f;
    float worldW = vwPx / ppu; // viewport genişliği (dünya birimi)
    float worldH = vhPx / ppu;

    // Hedef: ~50 çizgi ekran genişliğinde → ideal_step = worldW / 50
    // "Nice" adım: {1,2,5} × 10^n serisinden en yakın değer
    float ideal = std::max(worldW, worldH) / 50.0f;
    float mag = std::pow(10.0f, std::floor(std::log10(std::max(ideal, 1e-9f))));
    float nice = mag;
    if      (ideal / mag > 5.0f) nice = mag * 10.0f;
    else if (ideal / mag > 2.0f) nice = mag * 5.0f;
    else if (ideal / mag > 1.0f) nice = mag * 2.0f;
    float step   = std::max(nice, 1e-6f);
    float extent = std::max(worldW, worldH) * 2.0f; // viewport'un 2 katı kapsansın

    const float gridColor[3]  = {0.25f, 0.25f, 0.30f};
    const float axisColorX[3] = {0.6f,  0.2f,  0.2f};
    const float axisColorY[3] = {0.2f,  0.6f,  0.2f};

    std::vector<geom::Vertex> vertices;
    // Güvenlik: maksimum 400 çizgi / yön
    int maxLines = 400;
    int lineCount = 0;

    float start = std::ceil(-extent / step) * step;
    for (float i = start; i <= extent && lineCount < maxLines; i += step, ++lineCount) {
        if (std::abs(i) < step * 0.01f) continue; // ekseni ayrı çiz

        geom::Vertex v1{}, v2{};
        // Yatay çizgi
        v1.pos[0] = -extent; v1.pos[1] = i; v1.pos[2] = 0.0f;
        v2.pos[0] =  extent; v2.pos[1] = i; v2.pos[2] = 0.0f;
        std::copy(gridColor, gridColor + 3, v1.color);
        std::copy(gridColor, gridColor + 3, v2.color);
        vertices.push_back(v1); vertices.push_back(v2);
        // Dikey çizgi
        v1.pos[0] = i; v1.pos[1] = -extent; v1.pos[2] = 0.0f;
        v2.pos[0] = i; v2.pos[1] =  extent; v2.pos[2] = 0.0f;
        vertices.push_back(v1); vertices.push_back(v2);
    }

    // X ekseni
    { geom::Vertex v1{}, v2{};
      v1.pos[0] = -extent; v1.pos[1] = 0; v1.pos[2] = 0;
      v2.pos[0] =  extent; v2.pos[1] = 0; v2.pos[2] = 0;
      std::copy(axisColorX, axisColorX+3, v1.color); std::copy(axisColorX, axisColorX+3, v2.color);
      vertices.push_back(v1); vertices.push_back(v2); }
    // Y ekseni
    { geom::Vertex v1{}, v2{};
      v1.pos[0] = 0; v1.pos[1] = -extent; v1.pos[2] = 0;
      v2.pos[0] = 0; v2.pos[1] =  extent; v2.pos[2] = 0;
      std::copy(axisColorY, axisColorY+3, v1.color); std::copy(axisColorY, axisColorY+3, v2.color);
      vertices.push_back(v1); vertices.push_back(v2); }

    m_gridVertexCount = static_cast<uint32_t>(vertices.size());

    if (m_gridVertexCount > 0) {
        VkDeviceSize bufferSize = sizeof(geom::Vertex) * m_gridVertexCount;
        DestroyBuffer(m_gridVertexBuffer, m_gridVertexMemory);
        CreateBuffer(bufferSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_gridVertexBuffer, m_gridVertexMemory);

        void* data;
        vkMapMemory(m_device, m_gridVertexMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), bufferSize);
        vkUnmapMemory(m_device, m_gridVertexMemory);
    }
}

std::array<float, 3> VulkanRenderer::GetNodeColor(mep::NodeType type) {
    switch (type) {
        case mep::NodeType::Source:  return {0.2f, 0.5f, 1.0f};  // Mavi
        case mep::NodeType::Fixture: return {0.2f, 0.9f, 0.3f};  // Yeşil
        case mep::NodeType::Junction:return {0.7f, 0.7f, 0.7f};  // Gri
        case mep::NodeType::Drain:   return {0.7f, 0.4f, 0.2f};  // Kahverengi
        case mep::NodeType::Pump:    return {1.0f, 0.8f, 0.2f};  // Sarı
        case mep::NodeType::Tank:    return {0.3f, 0.3f, 0.9f};  // Koyu mavi
        default:                     return {0.5f, 0.5f, 0.5f};
    }
}

std::array<float, 3> VulkanRenderer::GetEdgeColor(mep::EdgeType type) {
    switch (type) {
        case mep::EdgeType::Supply:   return {0.2f, 0.6f, 1.0f};  // Açık mavi
        case mep::EdgeType::Drainage: return {0.6f, 0.4f, 0.2f};  // Kahverengi
        case mep::EdgeType::Vent:     return {0.5f, 0.5f, 0.5f};  // Gri
        default:                      return {0.5f, 0.5f, 0.5f};
    }
}

void VulkanRenderer::UpdateNetworkVertexData(const mep::NetworkGraph& network) {
    std::vector<geom::Vertex> lineVertices;
    std::vector<geom::Vertex> triVertices;

    const auto& nodes = network.GetNodes();
    const auto& edges = network.GetEdges();

    // Edge'ler: Çizgi olarak çiz
    for (const auto& edge : edges) {
        const mep::Node* nodeA = network.GetNode(edge.nodeA);
        const mep::Node* nodeB = network.GetNode(edge.nodeB);
        if (!nodeA || !nodeB) continue;

        auto color = GetEdgeColor(edge.type);

        geom::Vertex v1{}, v2{};
        v1.pos[0] = static_cast<float>(nodeA->position.x);
        v1.pos[1] = static_cast<float>(nodeA->position.y);
        v1.pos[2] = static_cast<float>(nodeA->position.z);
        v2.pos[0] = static_cast<float>(nodeB->position.x);
        v2.pos[1] = static_cast<float>(nodeB->position.y);
        v2.pos[2] = static_cast<float>(nodeB->position.z);
        std::copy(color.begin(), color.end(), v1.color);
        std::copy(color.begin(), color.end(), v2.color);

        lineVertices.push_back(v1);
        lineVertices.push_back(v2);
    }

    // Node'lar: Küçük kare (2 triangle = 6 vertex) olarak çiz
    const float nodeSize = 0.15f; // 15cm çap
    for (const auto& node : nodes) {
        auto color = GetNodeColor(node.type);
        float x = static_cast<float>(node.position.x);
        float y = static_cast<float>(node.position.y);
        float z = static_cast<float>(node.position.z);

        // 8-segment çember yaklaşımı (triangle fan → triangle list)
        constexpr int segments = 8;
        for (int i = 0; i < segments; i++) {
            float angle1 = (2.0f * PI_F * i) / segments;
            float angle2 = (2.0f * PI_F * (i + 1)) / segments;

            geom::Vertex center{}, p1{}, p2{};
            center.pos[0] = x; center.pos[1] = y; center.pos[2] = z;
            p1.pos[0] = x + nodeSize * std::cos(angle1);
            p1.pos[1] = y + nodeSize * std::sin(angle1);
            p1.pos[2] = z;
            p2.pos[0] = x + nodeSize * std::cos(angle2);
            p2.pos[1] = y + nodeSize * std::sin(angle2);
            p2.pos[2] = z;

            std::copy(color.begin(), color.end(), center.color);
            std::copy(color.begin(), color.end(), p1.color);
            std::copy(color.begin(), color.end(), p2.color);

            triVertices.push_back(center);
            triVertices.push_back(p1);
            triVertices.push_back(p2);
        }
    }

    m_lineVertexOffset = 0;
    m_lineVertexCount = static_cast<uint32_t>(lineVertices.size());
    m_triangleVertexOffset = m_lineVertexCount;
    m_triangleVertexCount = static_cast<uint32_t>(triVertices.size());

    uint32_t totalCount = m_lineVertexCount + m_triangleVertexCount;
    if (totalCount == 0) return;

    // Vertex buffer güncelle
    VkDeviceSize bufferSize = sizeof(geom::Vertex) * totalCount;
    DestroyBuffer(m_vertexBuffer, m_vertexMemory);
    CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_vertexBuffer, m_vertexMemory);

    void* data;
    vkMapMemory(m_device, m_vertexMemory, 0, bufferSize, 0, &data);
    auto* dest = static_cast<geom::Vertex*>(data);
    if (m_lineVertexCount > 0) memcpy(dest, lineVertices.data(), sizeof(geom::Vertex) * m_lineVertexCount);
    if (m_triangleVertexCount > 0) memcpy(dest + m_lineVertexCount, triVertices.data(), sizeof(geom::Vertex) * m_triangleVertexCount);
    vkUnmapMemory(m_device, m_vertexMemory);
}

void VulkanRenderer::DrawGrid(VkCommandBuffer cmd) {
    if (m_gridVertexCount == 0 || !m_gridVertexBuffer) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);

    PushConstants pc;
    pc.mvp = m_viewProjection;
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_gridVertexBuffer, offsets);
    vkCmdDraw(cmd, m_gridVertexCount, 1, 0, 0);
}

void VulkanRenderer::DrawNetwork(VkCommandBuffer cmd, const mep::NetworkGraph& /*network*/) {
    if (!m_vertexBuffer) return;

    PushConstants pc;
    pc.mvp = m_viewProjection;

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, offsets);

    // Çizgiler (borular)
    if (m_lineVertexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdDraw(cmd, m_lineVertexCount, 1, m_lineVertexOffset, 0);
    }

    // Üçgenler (node'lar)
    if (m_triangleVertexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdDraw(cmd, m_triangleVertexCount, 1, m_triangleVertexOffset, 0);
    }
}

// ============================================================
// CAD ENTITY RENDERING
// ============================================================

void VulkanRenderer::RenderCADEntities(const std::vector<std::unique_ptr<cad::Entity>>& entities) {
    if (!m_initialized || !m_frameActive || entities.empty()) {
        static bool warned = false;
        if (!warned) {
            LogCAD("[RenderCADEntities] SKIP: initialized=" + std::to_string(m_initialized)
                   + " entities.size=" + std::to_string(entities.size()));
            warned = true;
        }
        return;
    }

    if (m_cadDirty) {
        LogCAD("[RenderCADEntities] m_cadDirty=true, updating vertex data for "
               + std::to_string(entities.size()) + " entities");
        UpdateCADVertexData(entities);
        m_cadDirty = false;
        LogCAD("[RenderCADEntities] After update: vertexCount=" + std::to_string(m_cadLineVertexCount)
               + " buffer=" + std::string(m_cadVertexBuffer ? "valid" : "NULL"));
    }

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    DrawCAD(cmd);
}

void VulkanRenderer::UpdateCADVertexData(const std::vector<std::unique_ptr<cad::Entity>>& entities) {
    std::vector<geom::Vertex> vertices;
    vertices.reserve(entities.size() * 8);

    // Kalın çizgiler için quad (TRIANGLE_LIST) buffer
    std::vector<geom::Vertex> fatVertices;
    fatVertices.reserve(64);

    // Hatch fill triangles (alpha-blend pipeline, separate buffer)
    std::vector<geom::Vertex> hatchVertices;
    hatchVertices.reserve(64);

    // Kalın çizgi eşiği: bu mm değerinin üstündeki explicit lineweight'ler quad olarak render edilir
    constexpr float kFatLineThresholdMM = 0.25f;

    // İki nokta arası düz kalın segment → 2 triangle (quad) üret
    auto AddFatSegment = [&](geom::Vec3 A, geom::Vec3 B,
                              float r, float g, float b, float halfW) {
        double dx = B.x - A.x, dy = B.y - A.y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-9) return;
        float nx = static_cast<float>(-dy / len * halfW);
        float ny = static_cast<float>( dx / len * halfW);

        // Quad: A0, A1, B1, B0  (A0=A-perp, A1=A+perp, B1=B+perp, B0=B-perp)
        auto v = [&](float px, float py, float pz) {
            geom::Vertex vx{};
            vx.pos[0]=px; vx.pos[1]=py; vx.pos[2]=pz;
            vx.color[0]=r; vx.color[1]=g; vx.color[2]=b;
            return vx;
        };
        auto A0 = v(static_cast<float>(A.x)-nx, static_cast<float>(A.y)-ny, static_cast<float>(A.z));
        auto A1 = v(static_cast<float>(A.x)+nx, static_cast<float>(A.y)+ny, static_cast<float>(A.z));
        auto B0 = v(static_cast<float>(B.x)-nx, static_cast<float>(B.y)-ny, static_cast<float>(B.z));
        auto B1 = v(static_cast<float>(B.x)+nx, static_cast<float>(B.y)+ny, static_cast<float>(B.z));
        // Triangle 1: A0, A1, B1
        fatVertices.push_back(A0); fatVertices.push_back(A1); fatVertices.push_back(B1);
        // Triangle 2: A0, B1, B0
        fatVertices.push_back(A0); fatVertices.push_back(B1); fatVertices.push_back(B0);
    };

    // Linetype dash pattern tablosu (mm cinsinden: +dash, -gap çiftleri)
    // AutoCAD ACAD linetype definitions ile uyumlu
    struct DashPattern { const char* name; std::vector<float> pattern; };
    static const DashPattern kPatterns[] = {
        {"DASHED",   {6.0f, 3.0f}},
        {"DASHED2",  {3.0f, 1.5f}},
        {"DASHEDX2", {12.0f, 6.0f}},
        {"HIDDEN",   {3.0f, 1.5f}},
        {"HIDDEN2",  {1.5f, 0.75f}},
        {"HIDDENX2", {6.0f, 3.0f}},
        {"CENTER",   {9.0f, 3.0f, 3.0f, 3.0f}},
        {"CENTER2",  {4.5f, 1.5f, 1.5f, 1.5f}},
        {"CENTERX2", {18.0f, 6.0f, 6.0f, 6.0f}},
        {"PHANTOM",  {12.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f}},
        {"PHANTOM2", {6.0f,  1.5f, 1.5f, 1.5f, 1.5f, 1.5f}},
        {"DOT",      {0.0f,  1.5f}},
        {"DOTX2",    {0.0f,  3.0f}},
    };

    // Dash pattern lookup: linetype adına göre pattern döndür (nullptr = solid)
    auto GetPattern = [&](const std::string& lt) -> const std::vector<float>* {
        for (auto& dp : kPatterns) {
            if (lt == dp.name) return &dp.pattern;
        }
        return nullptr;
    };

    // İki nokta arası dash segmentleri üret
    auto AddDashedLine = [&](geom::Vec3 A, geom::Vec3 B,
                              float r, float g, float b,
                              const std::vector<float>& pat,
                              double ltScale = 1.0) {
        double dx = B.x - A.x, dy = B.y - A.y, dz = B.z - A.z;
        double totalLen = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (totalLen < 1e-9) return;
        double ux = dx/totalLen, uy = dy/totalLen, uz = dz/totalLen;

        double pos = 0.0;
        size_t pi = 0;
        bool onDash = true;
        while (pos < totalLen) {
            double segLen = pat[pi % pat.size()] * ltScale;
            pi++;
            if (segLen < 1e-6) segLen = 0.3 * ltScale; // DOT → tiny dash
            double end = std::min(pos + segLen, totalLen);
            if (onDash && end > pos) {
                geom::Vertex v0{}, v1{};
                v0.pos[0] = static_cast<float>(A.x + ux*pos);
                v0.pos[1] = static_cast<float>(A.y + uy*pos);
                v0.pos[2] = static_cast<float>(A.z + uz*pos);
                v0.color[0]=r; v0.color[1]=g; v0.color[2]=b;
                v1.pos[0] = static_cast<float>(A.x + ux*end);
                v1.pos[1] = static_cast<float>(A.y + uy*end);
                v1.pos[2] = static_cast<float>(A.z + uz*end);
                v1.color[0]=r; v1.color[1]=g; v1.color[2]=b;
                vertices.push_back(v0);
                vertices.push_back(v1);
            }
            pos = end;
            onDash = !onDash;
        }
    };

    int lineCount = 0, arcCount = 0, circleCount = 0, polyCount = 0, skipCount = 0;

    // Debug stats (ilk render'da bir kez yazdır)
    int dbgByLayer = 0, dbgByBlock = 0, dbgExplicit = 0, dbgMissLayer = 0, dbgWhiteExplicit = 0;
    static bool s_debugPrinted = false;

    for (const auto& entity : entities) {
        if (!entity) { skipCount++; continue; }

        size_t prevSize = vertices.size();

        // Determine color from entity — resolve ByLayer and residual ByBlock
        cad::Color col = entity->GetColor();
        float r, g, b;
        if (col.IsByLayer() || col.IsByBlock()) {
            col.IsByLayer() ? dbgByLayer++ : dbgByBlock++;
            // ByLayer veya atanamamış ByBlock → layer rengi
            auto it = m_layerMap.find(entity->GetLayerName());
            if (it != m_layerMap.end()) {
                cad::Color lc = it->second.GetColor();
                r = lc.r / 255.0f;
                g = lc.g / 255.0f;
                b = lc.b / 255.0f;
            } else {
                dbgMissLayer++;
                r = 1.0f; g = 1.0f; b = 1.0f;
            }
        } else {
            dbgExplicit++;
            if (col.r == 255 && col.g == 255 && col.b == 255) dbgWhiteExplicit++;
            r = col.r / 255.0f;
            g = col.g / 255.0f;
            b = col.b / 255.0f;
        }

        auto addVertex = [&](double x, double y, double z = 0.0) {
            geom::Vertex v{};
            v.pos[0] = static_cast<float>(x);
            v.pos[1] = static_cast<float>(y);
            v.pos[2] = static_cast<float>(z);
            v.color[0] = r; v.color[1] = g; v.color[2] = b;
            vertices.push_back(v);
        };

        const std::vector<float>* dashPat = GetPattern(entity->GetLinetype());
        double lw = entity->GetLineweight(); // mm, -1=ByLayer
        bool useFat = (lw >= kFatLineThresholdMM);
        // Screen-space: lineweight_mm × (96dpi / 25.4) / zoom → world-space half-width
        // Böylece zoom değişse de ekrandaki piksel kalınlığı sabit kalır (AutoCAD davranışı)
        constexpr float kScreenDPImm = 96.0f / 25.4f;
        float ppu = std::max(m_pixelsPerWorldUnit, 0.001f);
        float halfW = useFat ? static_cast<float>(lw * kScreenDPImm * 0.5 / ppu) : 0.0f;
        // Combined linetype scale: global $LTSCALE × entity ltype scale (code 48)
        double entityLtScale = m_globalLtscale * entity->GetLinetypeScale();

        switch (entity->GetType()) {
        case cad::EntityType::Line: {
            const auto* line = static_cast<const cad::Line*>(entity.get());
            auto s = line->GetStart();
            auto e = line->GetEnd();
            if (useFat) {
                AddFatSegment(s, e, r, g, b, halfW);
            } else if (dashPat) {
                AddDashedLine(s, e, r, g, b, *dashPat, entityLtScale);
            } else {
                addVertex(s.x, s.y, s.z);
                addVertex(e.x, e.y, e.z);
            }
            lineCount++;
            break;
        }
        case cad::EntityType::Arc: {
            const auto* arc = static_cast<const cad::Arc*>(entity.get());
            double sweep = arc->GetSweepAngleRadians();
            // Adaptive: aim for ≤4 px chord at current zoom, clamped [4,128]
            double screenR = arc->GetRadius() * static_cast<double>(ppu);
            int fullCircSeg = static_cast<int>(2.0 * M_PI * screenR / 4.0);
            fullCircSeg = std::max(8, std::min(128, fullCircSeg));
            int segments = std::max(4, static_cast<int>(std::abs(sweep) / (2.0 * M_PI) * fullCircSeg));
            segments = std::min(segments, 128);
            for (int i = 0; i < segments; ++i) {
                double t0 = static_cast<double>(i) / segments;
                double t1 = static_cast<double>(i + 1) / segments;
                auto p0 = arc->GetPointAt(t0);
                auto p1 = arc->GetPointAt(t1);
                addVertex(p0.x, p0.y, p0.z);
                addVertex(p1.x, p1.y, p1.z);
            }
            arcCount++;
            break;
        }
        case cad::EntityType::Circle: {
            const auto* circle = static_cast<const cad::Circle*>(entity.get());
            auto center = circle->GetCenter();
            double radius = circle->GetRadius();
            // Adaptive: aim for ≤4 px chord at current zoom, clamped [8,128]
            double screenR = radius * static_cast<double>(ppu);
            int segments = static_cast<int>(2.0 * M_PI * screenR / 4.0);
            segments = std::max(8, std::min(128, segments));
            for (int i = 0; i < segments; ++i) {
                double a0 = (2.0 * M_PI * i) / segments;
                double a1 = (2.0 * M_PI * (i + 1)) / segments;
                addVertex(center.x + radius * std::cos(a0),
                          center.y + radius * std::sin(a0));
                addVertex(center.x + radius * std::cos(a1),
                          center.y + radius * std::sin(a1));
            }
            circleCount++;
            break;
        }
        case cad::EntityType::Ellipse: {
            const auto* ell = static_cast<const cad::Ellipse*>(entity.get());
            // Adaptive segment count based on semi-major axis screen size
            double screenA = ell->GetSemiMajor() * static_cast<double>(ppu);
            int segments = static_cast<int>(2.0 * M_PI * screenA / 4.0);
            segments = std::max(8, std::min(128, segments));
            if (!ell->IsFullEllipse()) segments = std::max(4, segments / 4);
            auto pts = ell->Tessellate(segments);
            for (size_t i = 0; i + 1 < pts.size(); ++i) {
                addVertex(pts[i].x,     pts[i].y,     pts[i].z);
                addVertex(pts[i+1].x,   pts[i+1].y,   pts[i+1].z);
            }
            break;
        }
        case cad::EntityType::Polyline: {
            const auto* poly = static_cast<const cad::Polyline*>(entity.get());
            const auto& verts = poly->GetVertices();
            if (verts.size() < 2) break;

            size_t segCount = poly->IsClosed() ? verts.size() : verts.size() - 1;
            for (size_t i = 0; i < segCount; ++i) {
                const auto& v0 = verts[i];
                const auto& v1 = verts[(i + 1) % verts.size()];

                if (std::abs(v0.bulge) < 1e-9) {
                    // Straight segment
                    if (useFat) {
                        AddFatSegment(v0.pos, v1.pos, r, g, b, halfW);
                    } else if (dashPat) {
                        AddDashedLine(v0.pos, v1.pos, r, g, b, *dashPat, entityLtScale);
                    } else {
                        addVertex(v0.pos.x, v0.pos.y, v0.pos.z);
                        addVertex(v1.pos.x, v1.pos.y, v1.pos.z);
                    }
                } else {
                    // Arc segment from bulge
                    double dx = v1.pos.x - v0.pos.x;
                    double dy = v1.pos.y - v0.pos.y;
                    double chord = std::sqrt(dx * dx + dy * dy);
                    if (chord < 1e-9) continue;

                    double sagitta = std::abs(v0.bulge) * chord / 2.0;
                    double radius = (chord * chord / 4.0 + sagitta * sagitta) / (2.0 * sagitta);
                    double halfAngle = 2.0 * std::atan(std::abs(v0.bulge));
                    double sweepAngle = 4.0 * halfAngle;

                    // Center of arc
                    double mx = (v0.pos.x + v1.pos.x) / 2.0;
                    double my = (v0.pos.y + v1.pos.y) / 2.0;
                    double nx = -dy / chord;
                    double ny = dx / chord;
                    double d = radius - sagitta;
                    double sign = (v0.bulge > 0) ? 1.0 : -1.0;
                    double cx = mx + sign * d * nx;
                    double cy = my + sign * d * ny;

                    // Start angle
                    double startAngle = std::atan2(v0.pos.y - cy, v0.pos.x - cx);

                    int arcSegs = std::max(4, static_cast<int>(sweepAngle / (M_PI / 16.0)));
                    double dir = (v0.bulge > 0) ? 1.0 : -1.0;
                    for (int s = 0; s < arcSegs; ++s) {
                        double a0 = startAngle + dir * sweepAngle * s / arcSegs;
                        double a1 = startAngle + dir * sweepAngle * (s + 1) / arcSegs;
                        addVertex(cx + radius * std::cos(a0),
                                  cy + radius * std::sin(a0));
                        addVertex(cx + radius * std::cos(a1),
                                  cy + radius * std::sin(a1));
                    }
                }
            }
            polyCount++;
            break;
        }
        case cad::EntityType::Hatch: {
            const auto* h = static_cast<const cad::Hatch*>(entity.get());
            const auto& bnd = h->GetBoundary();
            if (bnd.size() < 3) break;

            if (h->IsSolid()) {
                // Fan tessellation: triangles go to hatchVertices (alpha-blend pipeline)
                const auto& c = bnd[0].pos;
                for (size_t bi = 1; bi + 1 < bnd.size(); ++bi) {
                    auto addHV = [&](const geom::Vec3& p) {
                        geom::Vertex v{};
                        v.pos[0] = static_cast<float>(p.x);
                        v.pos[1] = static_cast<float>(p.y);
                        v.pos[2] = static_cast<float>(p.z);
                        v.color[0] = r; v.color[1] = g; v.color[2] = b; // full color, alpha set by pipeline blend constant
                        hatchVertices.push_back(v);
                    };
                    addHV(c);
                    addHV(bnd[bi].pos);
                    addHV(bnd[bi + 1].pos);
                }
                // Sınır çizgisi de çiz
                for (size_t bi = 0; bi < bnd.size(); ++bi) {
                    const auto& v0 = bnd[bi].pos;
                    const auto& v1 = bnd[(bi + 1) % bnd.size()].pos;
                    addVertex(v0.x, v0.y, v0.z);
                    addVertex(v1.x, v1.y, v1.z);
                }
            } else {
                // Non-solid: scanline fill with parallel lines at pattern angle/scale
                constexpr double kBaseSpacingMM = 3.175; // ANSI standard 1/8 inch

                // Look up pattern base angle from name table
                double baseAngleDeg = h->GetPatternAngle();
                bool isNet = false;
                struct KnownPattern { const char* name; double angleDeg; bool net; };
                static const KnownPattern kKnown[] = {
                    {"ANSI31", 45.0, false}, {"ANSI32", 45.0, false}, {"ANSI33", 45.0, false},
                    {"ANSI34", 45.0, false}, {"ANSI35", 45.0, false}, {"ANSI36", 45.0, false},
                    {"ANSI37", 45.0, false}, {"ANSI38", 45.0, false},
                    {"STEEL",  45.0, false}, {"EARTH",  45.0, false},
                    {"NET",     0.0, true },  {"NET3",   60.0, true },
                    {"DOTS",    0.0, false},  {"LINE",   0.0, false},
                    {"ANGLE",  45.0, false},
                };
                for (auto& kp : kKnown) {
                    if (h->GetPatternName() == kp.name) {
                        baseAngleDeg = kp.angleDeg;
                        isNet = kp.net;
                        break;
                    }
                }

                double spacing = kBaseSpacingMM * h->GetPatternScale();
                if (spacing < 0.01) spacing = kBaseSpacingMM;

                // Scanline fill at a given angle — even-odd rule
                auto runScanlines = [&](double angleDeg) {
                    double thetaRad = angleDeg * M_PI / 180.0;
                    double cosT = std::cos(-thetaRad), sinT = std::sin(-thetaRad);

                    auto rotFwd = [&](double x, double y) -> std::pair<double,double> {
                        return {x*cosT - y*sinT, x*sinT + y*cosT};
                    };
                    auto rotBack = [&](double x, double y) -> std::pair<double,double> {
                        double ct = std::cos(thetaRad), st = std::sin(thetaRad);
                        return {x*ct - y*st, x*st + y*ct};
                    };

                    double minY = 1e18, maxY = -1e18;
                    std::vector<std::pair<double,double>> rotPts;
                    rotPts.reserve(bnd.size());
                    for (const auto& bv : bnd) {
                        auto rp = rotFwd(bv.pos.x, bv.pos.y);
                        rotPts.push_back(rp);
                        minY = std::min(minY, rp.second);
                        maxY = std::max(maxY, rp.second);
                    }
                    size_t N = rotPts.size();

                    for (double sy = std::ceil(minY / spacing) * spacing; sy <= maxY; sy += spacing) {
                        std::vector<double> xs;
                        for (size_t ei = 0; ei < N; ++ei) {
                            const auto& a = rotPts[ei];
                            const auto& b = rotPts[(ei + 1) % N];
                            double ay = a.second, by = b.second;
                            if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
                                double t = (sy - ay) / (by - ay);
                                xs.push_back(a.first + t * (b.first - a.first));
                            }
                        }
                        std::sort(xs.begin(), xs.end());
                        for (size_t xi = 0; xi + 1 < xs.size(); xi += 2) {
                            auto [wx0, wy0] = rotBack(xs[xi],     sy);
                            auto [wx1, wy1] = rotBack(xs[xi + 1], sy);
                            addVertex(wx0, wy0);
                            addVertex(wx1, wy1);
                        }
                    }
                };

                runScanlines(baseAngleDeg);
                if (isNet) runScanlines(baseAngleDeg + 90.0); // NET = cross-hatch

                // Also draw boundary outline
                for (size_t bi = 0; bi < bnd.size(); ++bi) {
                    const auto& v0 = bnd[bi].pos;
                    const auto& v1 = bnd[(bi + 1) % bnd.size()].pos;
                    addVertex(v0.x, v0.y, v0.z);
                    addVertex(v1.x, v1.y, v1.z);
                }
            }
            break;
        }
        case cad::EntityType::Dimension: {
            const auto* dim = static_cast<const cad::Dimension*>(entity.get());
            const auto& p1 = dim->GetPoint1();
            const auto& p2 = dim->GetPoint2();
            const auto& dl = dim->GetDimLinePos();

            // Yön ve dik vektör hesapla
            double dx = p2.x - p1.x, dy = p2.y - p1.y;
            double len = std::sqrt(dx*dx + dy*dy);
            if (len < 1e-9) break;
            double ux = dx/len, uy = dy/len; // birleşik yön
            double nx = -uy, ny = ux;         // dik

            // dimLinePos → p1-p2 hattına projeksiyon için normal offset
            double offset = (dl.x - p1.x)*nx + (dl.y - p1.y)*ny;

            // Uzatma çizgisi bitiş noktaları (ölçü çizgisi üzerinde)
            double ext1x = p1.x + nx * offset;
            double ext1y = p1.y + ny * offset;
            double ext2x = p2.x + nx * offset;
            double ext2y = p2.y + ny * offset;

            // Uzatma çizgileri (p1 → ext1, p2 → ext2)
            addVertex(p1.x, p1.y, p1.z);
            addVertex(ext1x, ext1y, p1.z);
            addVertex(p2.x, p2.y, p2.z);
            addVertex(ext2x, ext2y, p2.z);

            // Ölçü çizgisi (ext1 → ext2)
            addVertex(ext1x, ext1y, p1.z);
            addVertex(ext2x, ext2y, p2.z);

            // Ok başları — küçük V çizgileri her uçta
            double arrowLen = dim->GetArrowSize();
            auto addArrow = [&](double tx, double ty, double ax, double ay) {
                // ax,ay: ok yönü (normalize)
                double wx = ax*arrowLen, wy = ay*arrowLen;
                double pw = arrowLen * 0.25;
                addVertex(tx, ty, 0.0);
                addVertex(tx - wx + ny*pw, ty - wy - nx*pw, 0.0);
                addVertex(tx, ty, 0.0);
                addVertex(tx - wx - ny*pw, ty - wy + nx*pw, 0.0);
            };
            // ext1 → ext2 yönü boyunca oklar
            double dex = ext2x - ext1x, dey = ext2y - ext1y;
            double dlen = std::sqrt(dex*dex + dey*dey);
            if (dlen > 1e-9) {
                double eux = dex/dlen, euy = dey/dlen;
                addArrow(ext1x, ext1y,  eux,  euy);  // ext1'den ext2'ye bakan ok
                addArrow(ext2x, ext2y, -eux, -euy);  // ext2'den ext1'e bakan ok
            }
            break;
        }
        default:
            skipCount++;
            break;
        }

        // Log first few entities' coordinates for debugging
        if (lineCount + arcCount + circleCount + polyCount <= 5) {
            LogCAD("[UpdateCAD] Entity type=" + std::to_string(static_cast<int>(entity->GetType()))
                   + " generated " + std::to_string(vertices.size() - prevSize) + " vertices");
            if (vertices.size() >= 2) {
                auto& v = vertices[vertices.size()-2];
                LogCAD("  vertex pair: (" + std::to_string(v.pos[0]) + "," + std::to_string(v.pos[1]) + ")"
                       + " -> (" + std::to_string(vertices.back().pos[0]) + "," + std::to_string(vertices.back().pos[1]) + ")"
                       + " color=(" + std::to_string(v.color[0]) + "," + std::to_string(v.color[1]) + "," + std::to_string(v.color[2]) + ")");
            }
        }
    }

    LogCAD("[UpdateCAD] Summary: LINE=" + std::to_string(lineCount) + " ARC=" + std::to_string(arcCount)
           + " CIRCLE=" + std::to_string(circleCount) + " POLY=" + std::to_string(polyCount)
           + " skip=" + std::to_string(skipCount) + " totalVerts=" + std::to_string(vertices.size()));

    // Renk dağılımı: sorun tespiti için (ilk render'da bir kez)
    if (!s_debugPrinted && (lineCount + arcCount + circleCount + polyCount) > 10) {
        s_debugPrinted = true;
        LogCAD("[COLOR DEBUG] ByLayer=" + std::to_string(dbgByLayer)
               + " ByBlock=" + std::to_string(dbgByBlock)
               + " Explicit=" + std::to_string(dbgExplicit)
               + " (ExplicitWhite=" + std::to_string(dbgWhiteExplicit) + ")"
               + " LayerMiss=" + std::to_string(dbgMissLayer));
        LogCAD("[COLOR DEBUG] LayerMap size=" + std::to_string(m_layerMap.size()));
        // İlk 5 layer'ı listele
        int lc = 0;
        for (const auto& [name, layer] : m_layerMap) {
            if (lc++ >= 5) break;
            const auto& c = layer.GetColor();
            LogCAD("  Layer '" + name + "' color=(" + std::to_string((int)c.r) + ","
                   + std::to_string((int)c.g) + "," + std::to_string((int)c.b) + ")");
        }
    }

    // Compute bounding box BEFORE adding test pattern
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto& v : vertices) {
        if (v.pos[0] < minX) minX = v.pos[0];
        if (v.pos[1] < minY) minY = v.pos[1];
        if (v.pos[0] > maxX) maxX = v.pos[0];
        if (v.pos[1] > maxY) maxY = v.pos[1];
    }
    LogCAD("[UpdateCAD] Vertex bounds: (" + std::to_string(minX) + "," + std::to_string(minY)
           + ") -> (" + std::to_string(maxX) + "," + std::to_string(maxY) + ")");

    m_cadLineVertexCount = static_cast<uint32_t>(vertices.size());
    if (m_cadLineVertexCount == 0) return;

    // Rebuild vertex buffer
    DestroyBuffer(m_cadVertexBuffer, m_cadVertexMemory);

    VkDeviceSize bufferSize = sizeof(geom::Vertex) * m_cadLineVertexCount;
    bool bufOk = CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_cadVertexBuffer, m_cadVertexMemory);

    if (!bufOk) {
        LogCAD("[UpdateCAD] ERROR: CreateBuffer FAILED for " + std::to_string(bufferSize) + " bytes!");
        m_cadLineVertexCount = 0;
        return;
    }

    void* data;
    VkResult mapResult = vkMapMemory(m_device, m_cadVertexMemory, 0, bufferSize, 0, &data);
    if (mapResult != VK_SUCCESS) {
        LogCAD("[UpdateCAD] ERROR: vkMapMemory FAILED with code " + std::to_string(mapResult));
        m_cadLineVertexCount = 0;
        return;
    }
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_device, m_cadVertexMemory);

    // Verify: read back first 2 vertices
    void* readback;
    if (vkMapMemory(m_device, m_cadVertexMemory, 0, bufferSize, 0, &readback) == VK_SUCCESS) {
        const geom::Vertex* vb = static_cast<const geom::Vertex*>(readback);
        for (uint32_t i = 0; i < std::min(2u, m_cadLineVertexCount); ++i) {
            LogCAD("[UpdateCAD] READBACK[" + std::to_string(i) + "] pos=("
                   + std::to_string(vb[i].pos[0]) + "," + std::to_string(vb[i].pos[1])
                   + ") color=(" + std::to_string(vb[i].color[0]) + "," + std::to_string(vb[i].color[1]) + "," + std::to_string(vb[i].color[2]) + ")");
        }
        vkUnmapMemory(m_device, m_cadVertexMemory);
    }

    LogCAD("[UpdateCAD] Buffer created OK, " + std::to_string(m_cadLineVertexCount) + " vertices, "
           + std::to_string(bufferSize) + " bytes, handle=" + std::to_string(reinterpret_cast<uint64_t>(m_cadVertexBuffer)));

    // ── Fat line quad buffer ──────────────────────────────────────────────────
    DestroyBuffer(m_cadFatVertexBuffer, m_cadFatVertexMemory);
    m_cadFatVertexCount = static_cast<uint32_t>(fatVertices.size());
    if (m_cadFatVertexCount > 0) {
        VkDeviceSize fatSize = sizeof(geom::Vertex) * m_cadFatVertexCount;
        bool fatOk = CreateBuffer(fatSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_cadFatVertexBuffer, m_cadFatVertexMemory);
        if (fatOk) {
            void* fatData;
            if (vkMapMemory(m_device, m_cadFatVertexMemory, 0, fatSize, 0, &fatData) == VK_SUCCESS) {
                memcpy(fatData, fatVertices.data(), static_cast<size_t>(fatSize));
                vkUnmapMemory(m_device, m_cadFatVertexMemory);
            }
            LogCAD("[UpdateCAD] Fat buffer: " + std::to_string(m_cadFatVertexCount) + " verts (" +
                   std::to_string(m_cadFatVertexCount / 6) + " quads)");
        } else {
            m_cadFatVertexCount = 0;
        }
    }

    // ── Hatch alpha-blend buffer ──────────────────────────────────────────────
    DestroyBuffer(m_hatchVertexBuffer, m_hatchVertexMemory);
    m_hatchVertexCount = static_cast<uint32_t>(hatchVertices.size());
    if (m_hatchVertexCount > 0) {
        VkDeviceSize hatchSize = sizeof(geom::Vertex) * m_hatchVertexCount;
        bool hatchOk = CreateBuffer(hatchSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_hatchVertexBuffer, m_hatchVertexMemory);
        if (hatchOk) {
            void* hatchData;
            if (vkMapMemory(m_device, m_hatchVertexMemory, 0, hatchSize, 0, &hatchData) == VK_SUCCESS) {
                memcpy(hatchData, hatchVertices.data(), static_cast<size_t>(hatchSize));
                vkUnmapMemory(m_device, m_hatchVertexMemory);
            }
        } else {
            m_hatchVertexCount = 0;
        }
    }
}

void VulkanRenderer::DrawCAD(VkCommandBuffer cmd) {
    g_cadFrameCounter++;

    if (m_cadLineVertexCount == 0 || !m_cadVertexBuffer) {
        if (g_cadFrameCounter <= 5) {
            LogCAD("[DrawCAD] SKIP frame=" + std::to_string(g_cadFrameCounter)
                   + " vertexCount=" + std::to_string(m_cadLineVertexCount)
                   + " buffer=" + std::string(m_cadVertexBuffer ? "valid" : "NULL"));
        }
        return;
    }

    if (!m_linePipeline) {
        LogCAD("[DrawCAD] ERROR: m_linePipeline is NULL!");
        return;
    }

    // Log first 5 frames + every 60th frame
    if (g_cadFrameCounter <= 5 || g_cadFrameCounter % 60 == 0) {
        LogCAD("[DrawCAD] frame=" + std::to_string(g_cadFrameCounter)
               + " verts=" + std::to_string(m_cadLineVertexCount)
               + " MVP[0]=" + std::to_string(m_viewProjection.data[0])
               + " MVP[5]=" + std::to_string(m_viewProjection.data[5])
               + " MVP[12]=" + std::to_string(m_viewProjection.data[12])
               + " MVP[13]=" + std::to_string(m_viewProjection.data[13])
               + " swapExtent=" + std::to_string(m_swapchainExtent.width) + "x" + std::to_string(m_swapchainExtent.height)
               + " pipeline=" + std::to_string(reinterpret_cast<uint64_t>(m_linePipeline))
               + " buffer=" + std::to_string(reinterpret_cast<uint64_t>(m_cadVertexBuffer)));
    }

    PushConstants pc;
    pc.mvp = m_viewProjection;

    // ── Thin lines (LINE_LIST) ────────────────────────────────────────────────
    if (m_cadLineVertexCount > 0 && m_cadVertexBuffer) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
        // Cihaz lineWidth limiti: wideLines varsa 1px thin çizgiler için 1.0f kullan
        if (m_deviceWideLines)
            vkCmdSetLineWidth(cmd, 1.0f);
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_cadVertexBuffer, offsets);
        vkCmdDraw(cmd, m_cadLineVertexCount, 1, 0, 0);
    }

    // ── Fat quads (TRIANGLE_LIST, world-space quad expansion) ─────────────────
    if (m_cadFatVertexCount > 0 && m_cadFatVertexBuffer && m_trianglePipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_cadFatVertexBuffer, offsets);
        vkCmdDraw(cmd, m_cadFatVertexCount, 1, 0, 0);
    }

    // ── Hatch fills (TRIANGLE_LIST, alpha-blend pipeline — drawn last for correct blending) ──
    if (m_hatchVertexCount > 0 && m_hatchVertexBuffer && m_hatchPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hatchPipeline);
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_hatchVertexBuffer, offsets);
        vkCmdDraw(cmd, m_hatchVertexCount, 1, 0, 0);
    }
}

std::array<float, 3> VulkanRenderer::GetACIColor(int colorIndex) {
    switch (colorIndex) {
        case 1: return {1.0f, 0.0f, 0.0f};   // Red
        case 2: return {1.0f, 1.0f, 0.0f};   // Yellow
        case 3: return {0.0f, 1.0f, 0.0f};   // Green
        case 4: return {0.0f, 1.0f, 1.0f};   // Cyan
        case 5: return {0.0f, 0.0f, 1.0f};   // Blue
        case 6: return {1.0f, 0.0f, 1.0f};   // Magenta
        case 7: return {1.0f, 1.0f, 1.0f};   // White
        case 8: return {0.5f, 0.5f, 0.5f};   // Dark gray
        case 9: return {0.75f, 0.75f, 0.75f}; // Light gray
        default: return {1.0f, 1.0f, 1.0f};  // White fallback
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  SSAO IMPLEMENTATION
// ════════════════════════════════════════════════════════════════════════════

// Load a compiled .spv file from the shaders/ directory next to the executable
std::vector<uint32_t> VulkanRenderer::LoadShaderSPV(const char* filename) {
    std::string path = std::string("shaders/") + filename;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        LogCAD(std::string("SSAO shader not found: ") + path);
        return {};
    }
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint32_t> code(sz / sizeof(uint32_t));
    f.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(sz));
    return code;
}

// ── Image helpers ─────────────────────────────────────────────────────────────

bool VulkanRenderer::CreateSSAOImage(uint32_t w, uint32_t h, VkFormat fmt,
                                      VkImageUsageFlags usage,
                                      VkImageAspectFlags aspect, SSAOImage& out) {
    VkImageCreateInfo ii{};
    ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType     = VK_IMAGE_TYPE_2D;
    ii.extent        = {w, h, 1};
    ii.mipLevels     = 1;
    ii.arrayLayers   = 1;
    ii.format        = fmt;
    ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage         = usage;
    ii.samples       = VK_SAMPLE_COUNT_1_BIT;
    if (vkCreateImage(m_device, &ii, nullptr, &out.image) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(m_device, out.image, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = FindMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_device, &ai, nullptr, &out.memory) != VK_SUCCESS) return false;
    vkBindImageMemory(m_device, out.image, out.memory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image                           = out.image;
    vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    vi.format                          = fmt;
    vi.subresourceRange.aspectMask     = aspect;
    vi.subresourceRange.baseMipLevel   = 0;
    vi.subresourceRange.levelCount     = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount     = 1;
    return vkCreateImageView(m_device, &vi, nullptr, &out.view) == VK_SUCCESS;
}

void VulkanRenderer::DestroySSAOImage(SSAOImage& img) {
    if (img.view)   { vkDestroyImageView(m_device, img.view,   nullptr); img.view   = VK_NULL_HANDLE; }
    if (img.image)  { vkDestroyImage    (m_device, img.image,  nullptr); img.image  = VK_NULL_HANDLE; }
    if (img.memory) { vkFreeMemory      (m_device, img.memory, nullptr); img.memory = VK_NULL_HANDLE; }
}

// ── Kernel + noise generation ─────────────────────────────────────────────────

void VulkanRenderer::GenerateKernel() {
    std::mt19937 rng(42u);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < 64; ++i) {
        float x = dist(rng) * 2.0f - 1.0f;
        float y = dist(rng) * 2.0f - 1.0f;
        float z = dist(rng); // hemisphere: z >= 0
        float len = std::sqrt(x*x + y*y + z*z);
        if (len > 1e-4f) { x /= len; y /= len; z /= len; }
        float scale = dist(rng);
        scale = 0.1f + scale * scale * 0.9f; // random, biased towards origin
        m_kernelData[i*4+0] = x * scale;
        m_kernelData[i*4+1] = y * scale;
        m_kernelData[i*4+2] = z * scale;
        m_kernelData[i*4+3] = 0.0f;
    }
}

bool VulkanRenderer::UploadNoiseTexture() {
    // 4×4 random rotation vectors (XY plane)
    std::mt19937 rng(123u);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float noiseData[16 * 4];
    for (int i = 0; i < 16; ++i) {
        float x = dist(rng) * 2.0f - 1.0f;
        float y = dist(rng) * 2.0f - 1.0f;
        float len = std::sqrt(x*x + y*y);
        if (len > 1e-4f) { x /= len; y /= len; }
        noiseData[i*4+0] = x * 0.5f + 0.5f; // encode to [0,1]
        noiseData[i*4+1] = y * 0.5f + 0.5f;
        noiseData[i*4+2] = 0.5f;
        noiseData[i*4+3] = 1.0f;
    }

    // RGBA8_UNORM: 16 pixels × 4 bytes
    std::array<uint8_t, 16*4> pixels{};
    for (int i = 0; i < 16*4; ++i)
        pixels[i] = static_cast<uint8_t>(noiseData[i] * 255.0f);

    VkDeviceSize sz = pixels.size();
    if (!CreateBuffer(sz,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_ssaoNoiseStagingBuf, m_ssaoNoiseStagingMem)) return false;

    void* mapped;
    vkMapMemory(m_device, m_ssaoNoiseStagingMem, 0, sz, 0, &mapped);
    std::memcpy(mapped, pixels.data(), sz);
    vkUnmapMemory(m_device, m_ssaoNoiseStagingMem);

    // Create device-local noise image
    if (!CreateSSAOImage(4, 4, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, m_ssaoNoise)) return false;

    // Single-use command buffer for the copy
    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool        = m_commandPool;
    cbAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;
    VkCommandBuffer tmpCmd;
    vkAllocateCommandBuffers(m_device, &cbAlloc, &tmpCmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tmpCmd, &bi);

    // Barrier: UNDEFINED → TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = m_ssaoNoise.image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask       = 0;
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(tmpCmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent      = {4, 4, 1};
    vkCmdCopyBufferToImage(tmpCmd, m_ssaoNoiseStagingBuf, m_ssaoNoise.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Barrier: TRANSFER_DST → SHADER_READ_ONLY
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(tmpCmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(tmpCmd);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &tmpCmd;
    vkQueueSubmit(m_graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &tmpCmd);
    return true;
}

// ── Render pass factory ───────────────────────────────────────────────────────

VkRenderPass VulkanRenderer::CreateSingleColorRenderPass(VkFormat fmt,
                                                          VkImageLayout initialLayout,
                                                          VkImageLayout finalLayout) {
    VkAttachmentDescription att{};
    att.format         = fmt;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout  = initialLayout;
    att.finalLayout    = finalLayout;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &ref;

    // Dependency: ensure previous sampling finishes before we write
    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpi{};
    rpi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpi.attachmentCount = 1;
    rpi.pAttachments    = &att;
    rpi.subpassCount    = 1;
    rpi.pSubpasses      = &subpass;
    rpi.dependencyCount = static_cast<uint32_t>(deps.size());
    rpi.pDependencies   = deps.data();

    VkRenderPass rp = VK_NULL_HANDLE;
    vkCreateRenderPass(m_device, &rpi, nullptr, &rp);
    return rp;
}

// ── G-Buffer draw variants ────────────────────────────────────────────────────

void VulkanRenderer::DrawGridGBuffer(VkCommandBuffer cmd) {
    if (m_gridVertexCount == 0 || !m_gridVertexBuffer) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufLinePipeline);

    GBufferPushConstants pc{};
    pc.mvp       = m_viewProjection;
    pc.modelView = m_camera.GetViewMatrix();
    vkCmdPushConstants(cmd, m_gbufPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_gridVertexBuffer, &off);
    vkCmdDraw(cmd, m_gridVertexCount, 1, 0, 0);
}

void VulkanRenderer::DrawNetworkGBuffer(VkCommandBuffer cmd) {
    if (!m_vertexBuffer) return;

    GBufferPushConstants pc{};
    pc.mvp       = m_viewProjection;
    pc.modelView = m_camera.GetViewMatrix();

    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &off);

    if (m_lineVertexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufLinePipeline);
        vkCmdPushConstants(cmd, m_gbufPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cmd, m_lineVertexCount, 1, m_lineVertexOffset, 0);
    }
    if (m_triangleVertexCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufTriPipeline);
        vkCmdPushConstants(cmd, m_gbufPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cmd, m_triangleVertexCount, 1, m_triangleVertexOffset, 0);
    }
}

// ── InitializeSSAO ────────────────────────────────────────────────────────────

bool VulkanRenderer::InitializeSSAO() {
    if (m_ssaoInitialized) return true;

    const uint32_t W = m_swapchainExtent.width;
    const uint32_t H = m_swapchainExtent.height;

    // ── Load shaders ──────────────────────────────────────────────────────
    auto gbufVert  = LoadShaderSPV("gbuffer.vert.spv");
    auto gbufFrag  = LoadShaderSPV("gbuffer.frag.spv");
    auto fsVert    = LoadShaderSPV("fullscreen.vert.spv");
    auto ssaoFrag  = LoadShaderSPV("ssao.frag.spv");
    auto blurFrag  = LoadShaderSPV("ssao_blur.frag.spv");
    auto compFrag  = LoadShaderSPV("composite.frag.spv");

    if (gbufVert.empty() || gbufFrag.empty() || fsVert.empty() ||
        ssaoFrag.empty() || blurFrag.empty() || compFrag.empty()) {
        LogCAD("SSAO disabled — compile shaders with glslc first (see bin/shaders/)");
        return false;
    }

    VkShaderModule mGBufV  = CreateShaderModule(gbufVert);
    VkShaderModule mGBufF  = CreateShaderModule(gbufFrag);
    VkShaderModule mFSV    = CreateShaderModule(fsVert);
    VkShaderModule mSSAO   = CreateShaderModule(ssaoFrag);
    VkShaderModule mBlur   = CreateShaderModule(blurFrag);
    VkShaderModule mComp   = CreateShaderModule(compFrag);

    if (!mGBufV || !mGBufF || !mFSV || !mSSAO || !mBlur || !mComp) {
        LogCAD("SSAO disabled — shader module creation failed");
        for (auto m : {mGBufV, mGBufF, mFSV, mSSAO, mBlur, mComp})
            if (m) vkDestroyShaderModule(m_device, m, nullptr);
        return false;
    }

    // ── G-buffer offscreen images ─────────────────────────────────────────
    constexpr auto COLOR_USAGE =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (!CreateSSAOImage(W, H, VK_FORMAT_R8G8B8A8_UNORM,  COLOR_USAGE, VK_IMAGE_ASPECT_COLOR_BIT,   m_gColor)    ||
        !CreateSSAOImage(W, H, VK_FORMAT_R32G32B32A32_SFLOAT, COLOR_USAGE, VK_IMAGE_ASPECT_COLOR_BIT, m_gPosition) ||
        !CreateSSAOImage(W, H, VK_FORMAT_R16G16B16A16_SFLOAT, COLOR_USAGE, VK_IMAGE_ASPECT_COLOR_BIT, m_gNormal)   ||
        !CreateSSAOImage(W, H, VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, m_gbufDepth) ||
        !CreateSSAOImage(W, H, VK_FORMAT_R8G8B8A8_UNORM, COLOR_USAGE, VK_IMAGE_ASPECT_COLOR_BIT, m_ssaoRaw)  ||
        !CreateSSAOImage(W, H, VK_FORMAT_R8G8B8A8_UNORM, COLOR_USAGE, VK_IMAGE_ASPECT_COLOR_BIT, m_ssaoBlur)) {
        LogCAD("SSAO disabled — image creation failed");
        ShutdownSSAO();
        for (auto m : {mGBufV, mGBufF, mFSV, mSSAO, mBlur, mComp})
            vkDestroyShaderModule(m_device, m, nullptr);
        return false;
    }

    // ── Samplers ──────────────────────────────────────────────────────────
    VkSamplerCreateInfo si{};
    si.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter     = VK_FILTER_LINEAR;
    si.minFilter     = VK_FILTER_LINEAR;
    si.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxAnisotropy = 1.0f;
    si.borderColor   = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    vkCreateSampler(m_device, &si, nullptr, &m_linearSampler);

    si.magFilter    = VK_FILTER_NEAREST;
    si.minFilter    = VK_FILTER_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(m_device, &si, nullptr, &m_nearestSampler);

    // ── Noise texture ─────────────────────────────────────────────────────
    if (!UploadNoiseTexture()) {
        LogCAD("SSAO disabled — noise texture upload failed");
        ShutdownSSAO();
        for (auto m : {mGBufV, mGBufF, mFSV, mSSAO, mBlur, mComp})
            vkDestroyShaderModule(m_device, m, nullptr);
        return false;
    }

    // ── G-buffer render pass (3 color + depth) ────────────────────────────
    {
        std::array<VkAttachmentDescription, 4> atts{};
        // 0: gColor
        atts[0].format        = VK_FORMAT_R8G8B8A8_UNORM;
        atts[0].samples       = VK_SAMPLE_COUNT_1_BIT;
        atts[0].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        atts[0].storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
        atts[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[0].stencilStoreOp= VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[0].finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // 1: gPosition
        atts[1] = atts[0];
        atts[1].format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        // 2: gNormal
        atts[2] = atts[0];
        atts[2].format      = VK_FORMAT_R16G16B16A16_SFLOAT;
        // 3: depth
        atts[3].format        = VK_FORMAT_D32_SFLOAT;
        atts[3].samples       = VK_SAMPLE_COUNT_1_BIT;
        atts[3].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        atts[3].storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[3].stencilStoreOp= VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[3].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentReference, 3> colorRefs{};
        for (int i = 0; i < 3; ++i) { colorRefs[i].attachment = i; colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; }
        VkAttachmentReference depthRef{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 3;
        subpass.pColorAttachments       = colorRefs.data();
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkSubpassDependency, 2> deps{};
        deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL; deps[0].dstSubpass = 0;
        deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass    = 0; deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rpi{};
        rpi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpi.attachmentCount = 4;
        rpi.pAttachments    = atts.data();
        rpi.subpassCount    = 1;
        rpi.pSubpasses      = &subpass;
        rpi.dependencyCount = 2;
        rpi.pDependencies   = deps.data();
        vkCreateRenderPass(m_device, &rpi, nullptr, &m_gbufferRenderPass);
    }

    m_ssaoRenderPass      = CreateSingleColorRenderPass(VK_FORMAT_R8G8B8A8_UNORM,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_blurRenderPass      = CreateSingleColorRenderPass(VK_FORMAT_R8G8B8A8_UNORM,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_compositeRenderPass = CreateSingleColorRenderPass(m_swapchainFormat,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // ── Framebuffers ──────────────────────────────────────────────────────
    {
        std::array<VkImageView, 4> views = {
            m_gColor.view, m_gPosition.view, m_gNormal.view, m_gbufDepth.view};
        VkFramebufferCreateInfo fbi{};
        fbi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass      = m_gbufferRenderPass;
        fbi.attachmentCount = 4;
        fbi.pAttachments    = views.data();
        fbi.width = W; fbi.height = H; fbi.layers = 1;
        vkCreateFramebuffer(m_device, &fbi, nullptr, &m_gbufferFB);
    }
    auto makeSingleFB = [&](VkRenderPass rp, VkImageView view, VkFramebuffer& fb) {
        VkFramebufferCreateInfo fbi{};
        fbi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass      = rp;
        fbi.attachmentCount = 1;
        fbi.pAttachments    = &view;
        fbi.width = W; fbi.height = H; fbi.layers = 1;
        vkCreateFramebuffer(m_device, &fbi, nullptr, &fb);
    };
    makeSingleFB(m_ssaoRenderPass,  m_ssaoRaw.view,  m_ssaoFB);
    makeSingleFB(m_blurRenderPass,  m_ssaoBlur.view, m_blurFB);

    m_compositeFBs.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
        makeSingleFB(m_compositeRenderPass, m_swapchainImageViews[i], m_compositeFBs[i]);

    // ── Descriptor set layouts ────────────────────────────────────────────
    auto makeLayout = [&](const std::vector<VkDescriptorType>& types,
                          VkShaderStageFlags stages,
                          VkDescriptorSetLayout& layout) {
        std::vector<VkDescriptorSetLayoutBinding> bindings(types.size());
        for (uint32_t i = 0; i < types.size(); ++i) {
            bindings[i].binding         = i;
            bindings[i].descriptorType  = types[i];
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags      = stages;
        }
        VkDescriptorSetLayoutCreateInfo dslci{};
        dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = static_cast<uint32_t>(bindings.size());
        dslci.pBindings    = bindings.data();
        vkCreateDescriptorSetLayout(m_device, &dslci, nullptr, &layout);
    };

    makeLayout({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
               VK_SHADER_STAGE_FRAGMENT_BIT, m_ssaoDescLayout);

    makeLayout({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               VK_SHADER_STAGE_FRAGMENT_BIT, m_kernelDescLayout);

    makeLayout({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
               VK_SHADER_STAGE_FRAGMENT_BIT, m_blurDescLayout);

    makeLayout({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
               VK_SHADER_STAGE_FRAGMENT_BIT, m_compositeDescLayout);

    // ── Descriptor pool ───────────────────────────────────────────────────
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}; // +2 for gNormal, gPosition in composite
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets       = 4;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes    = poolSizes.data();
    vkCreateDescriptorPool(m_device, &dpci, nullptr, &m_ssaoDescPool);

    // ── Kernel UBO ────────────────────────────────────────────────────────
    GenerateKernel();
    VkDeviceSize kernelSz = sizeof(m_kernelData);
    CreateBuffer(kernelSz,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_kernelUboBuffer, m_kernelUboMemory);
    void* kPtr;
    vkMapMemory(m_device, m_kernelUboMemory, 0, kernelSz, 0, &kPtr);
    std::memcpy(kPtr, m_kernelData, kernelSz);
    vkUnmapMemory(m_device, m_kernelUboMemory);

    // ── Allocate descriptor sets ──────────────────────────────────────────
    VkDescriptorSetLayout layouts[] = {
        m_ssaoDescLayout, m_kernelDescLayout, m_blurDescLayout, m_compositeDescLayout};
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = m_ssaoDescPool;
    dsai.descriptorSetCount = 4;
    dsai.pSetLayouts        = layouts;
    VkDescriptorSet sets[4];
    vkAllocateDescriptorSets(m_device, &dsai, sets);
    m_ssaoDescSet0 = sets[0];
    m_ssaoDescSet1 = sets[1];
    m_blurDescSet  = sets[2];
    m_compositeDescSet = sets[3];

    // ── Write descriptor sets ─────────────────────────────────────────────
    auto makeImageInfo = [](VkSampler s, VkImageView v) {
        VkDescriptorImageInfo ii{};
        ii.sampler     = s;
        ii.imageView   = v;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return ii;
    };

    // Set 0 — SSAO: gPosition, gNormal, noise
    std::array<VkDescriptorImageInfo, 3> ssaoImgs = {
        makeImageInfo(m_linearSampler,  m_gPosition.view),
        makeImageInfo(m_linearSampler,  m_gNormal.view),
        makeImageInfo(m_nearestSampler, m_ssaoNoise.view),
    };
    std::array<VkWriteDescriptorSet, 3> ssaoWrites{};
    for (int i = 0; i < 3; ++i) {
        ssaoWrites[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssaoWrites[i].dstSet          = m_ssaoDescSet0;
        ssaoWrites[i].dstBinding      = i;
        ssaoWrites[i].descriptorCount = 1;
        ssaoWrites[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssaoWrites[i].pImageInfo      = &ssaoImgs[i];
    }
    vkUpdateDescriptorSets(m_device, 3, ssaoWrites.data(), 0, nullptr);

    // Set 1 — Kernel UBO
    VkDescriptorBufferInfo kernelBI{m_kernelUboBuffer, 0, kernelSz};
    VkWriteDescriptorSet kWrite{};
    kWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    kWrite.dstSet          = m_ssaoDescSet1;
    kWrite.dstBinding      = 0;
    kWrite.descriptorCount = 1;
    kWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    kWrite.pBufferInfo     = &kernelBI;
    vkUpdateDescriptorSets(m_device, 1, &kWrite, 0, nullptr);

    // Set 2 — Blur: ssaoRaw
    auto blurImg = makeImageInfo(m_linearSampler, m_ssaoRaw.view);
    VkWriteDescriptorSet blurW{};
    blurW.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    blurW.dstSet          = m_blurDescSet;
    blurW.dstBinding      = 0;
    blurW.descriptorCount = 1;
    blurW.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurW.pImageInfo      = &blurImg;
    vkUpdateDescriptorSets(m_device, 1, &blurW, 0, nullptr);

    // Set 3 — Composite: gColor(0), ssaoBlur(1), gNormal(2), gPosition(3)
    std::array<VkDescriptorImageInfo, 4> compImgs = {
        makeImageInfo(m_linearSampler, m_gColor.view),
        makeImageInfo(m_linearSampler, m_ssaoBlur.view),
        makeImageInfo(m_linearSampler, m_gNormal.view),
        makeImageInfo(m_linearSampler, m_gPosition.view),
    };
    std::array<VkWriteDescriptorSet, 4> compWrites{};
    for (int i = 0; i < 4; ++i) {
        compWrites[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compWrites[i].dstSet          = m_compositeDescSet;
        compWrites[i].dstBinding      = i;
        compWrites[i].descriptorCount = 1;
        compWrites[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        compWrites[i].pImageInfo      = &compImgs[i];
    }
    vkUpdateDescriptorSets(m_device, 4, compWrites.data(), 0, nullptr);

    // ── Pipeline layouts + pipelines ──────────────────────────────────────
    // G-buffer layout: push constants only (128 bytes)
    {
        VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants)};
        VkPipelineLayoutCreateInfo pli{};
        pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges    = &pcr;
        vkCreatePipelineLayout(m_device, &pli, nullptr, &m_gbufPipelineLayout);
    }

    // SSAO layout: set0 + set1 + push constants
    {
        VkDescriptorSetLayout dsl[] = {m_ssaoDescLayout, m_kernelDescLayout};
        VkPushConstantRange pcr{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SSAOPushConstants)};
        VkPipelineLayoutCreateInfo pli{};
        pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount         = 2;
        pli.pSetLayouts            = dsl;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges    = &pcr;
        vkCreatePipelineLayout(m_device, &pli, nullptr, &m_ssaoPipelineLayout);
    }

    // Blur layout: set0 only
    {
        VkPipelineLayoutCreateInfo pli{};
        pli.sType           = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount  = 1;
        pli.pSetLayouts     = &m_blurDescLayout;
        vkCreatePipelineLayout(m_device, &pli, nullptr, &m_blurPipelineLayout);
    }

    // Composite layout: set0 + CompositePushConstants
    {
        VkPushConstantRange pcr{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CompositePushConstants)};
        VkPipelineLayoutCreateInfo pli{};
        pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount         = 1;
        pli.pSetLayouts            = &m_compositeDescLayout;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges    = &pcr;
        vkCreatePipelineLayout(m_device, &pli, nullptr, &m_compositePipelineLayout);
    }

    // ── G-buffer pipelines ────────────────────────────────────────────────
    {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = mGBufV; stages[0].pName = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = mGBufF; stages[1].pName = "main";

        VkVertexInputBindingDescription vib{0, sizeof(geom::Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription, 3> via{};
        via[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(geom::Vertex, pos)};
        via[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(geom::Vertex, normal)};
        via[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(geom::Vertex, color)};

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount   = 1;
        vi.pVertexBindingDescriptions      = &vib;
        vi.vertexAttributeDescriptionCount = 3;
        vi.pVertexAttributeDescriptions    = via.data();

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1; vs.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.lineWidth   = 2.0f;
        rs.cullMode    = VK_CULL_MODE_NONE;
        rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable  = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp   = VK_COMPARE_OP_LESS;

        // 3 color blend attachments for G-buffer MRT
        std::array<VkPipelineColorBlendAttachmentState, 3> cbAtts{};
        for (auto& a : cbAtts) {
            a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 3;
        cb.pAttachments    = cbAtts.data();

        std::vector<VkDynamicState> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates.data();

        VkGraphicsPipelineCreateInfo gpci{};
        gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount          = 2;
        gpci.pStages             = stages;
        gpci.pVertexInputState   = &vi;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState      = &vs;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState   = &ms;
        gpci.pDepthStencilState  = &ds;
        gpci.pColorBlendState    = &cb;
        gpci.pDynamicState       = &dyn;
        gpci.layout              = m_gbufPipelineLayout;
        gpci.renderPass          = m_gbufferRenderPass;
        gpci.subpass             = 0;
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &m_gbufLinePipeline);

        ia.topology  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        rs.lineWidth = 1.0f;
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &m_gbufTriPipeline);
    }

    // ── Fullscreen pipelines (SSAO, Blur, Composite) ──────────────────────
    auto makeFullscreenPipeline = [&](VkShaderModule fragModule,
                                      VkPipelineLayout layout,
                                      VkRenderPass rp,
                                      VkPipeline& pipeline) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = mFSV; stages[0].pName = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule; stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO; // no vertex input

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1; vs.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.lineWidth   = 1.0f;
        rs.cullMode    = VK_CULL_MODE_NONE;
        rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments    = &cba;

        std::vector<VkDynamicState> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates.data();

        VkGraphicsPipelineCreateInfo gpci{};
        gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount          = 2;
        gpci.pStages             = stages;
        gpci.pVertexInputState   = &vi;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState      = &vs;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState   = &ms;
        gpci.pColorBlendState    = &cb;
        gpci.pDynamicState       = &dyn;
        gpci.layout              = layout;
        gpci.renderPass          = rp;
        gpci.subpass             = 0;
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline);
    };

    makeFullscreenPipeline(mSSAO, m_ssaoPipelineLayout, m_ssaoRenderPass,      m_ssaoPipeline);
    makeFullscreenPipeline(mBlur, m_blurPipelineLayout,  m_blurRenderPass,      m_blurPipeline);
    makeFullscreenPipeline(mComp, m_compositePipelineLayout, m_compositeRenderPass, m_compositePipeline);

    // Destroy shader modules — no longer needed after pipeline creation
    for (auto m : {mGBufV, mGBufF, mFSV, mSSAO, mBlur, mComp})
        vkDestroyShaderModule(m_device, m, nullptr);

    m_ssaoInitialized = true;
    LogCAD("SSAO initialized (radius=0.5m, 64 samples, 4x4 noise)");
    return true;
}

// ── ShutdownSSAO ──────────────────────────────────────────────────────────────

void VulkanRenderer::ShutdownSSAO() {
    if (!m_device) return;
    vkDeviceWaitIdle(m_device);

    // Pipelines
    for (auto p : {m_gbufLinePipeline, m_gbufTriPipeline,
                   m_ssaoPipeline, m_blurPipeline, m_compositePipeline})
        if (p) vkDestroyPipeline(m_device, p, nullptr);
    m_gbufLinePipeline = m_gbufTriPipeline = m_ssaoPipeline =
    m_blurPipeline     = m_compositePipeline = VK_NULL_HANDLE;

    // Pipeline layouts
    for (auto l : {m_gbufPipelineLayout, m_ssaoPipelineLayout,
                   m_blurPipelineLayout, m_compositePipelineLayout})
        if (l) vkDestroyPipelineLayout(m_device, l, nullptr);
    m_gbufPipelineLayout = m_ssaoPipelineLayout =
    m_blurPipelineLayout = m_compositePipelineLayout = VK_NULL_HANDLE;

    // Descriptor pool (frees all sets)
    if (m_ssaoDescPool) { vkDestroyDescriptorPool(m_device, m_ssaoDescPool, nullptr); m_ssaoDescPool = VK_NULL_HANDLE; }
    m_ssaoDescSet0 = m_ssaoDescSet1 = m_blurDescSet = m_compositeDescSet = VK_NULL_HANDLE;

    // Descriptor set layouts
    for (auto l : {m_ssaoDescLayout, m_kernelDescLayout, m_blurDescLayout, m_compositeDescLayout})
        if (l) vkDestroyDescriptorSetLayout(m_device, l, nullptr);
    m_ssaoDescLayout = m_kernelDescLayout = m_blurDescLayout = m_compositeDescLayout = VK_NULL_HANDLE;

    // Kernel UBO
    DestroyBuffer(m_kernelUboBuffer, m_kernelUboMemory);

    // Composite framebuffers
    for (auto fb : m_compositeFBs) if (fb) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_compositeFBs.clear();

    // Single framebuffers
    for (auto fb : {m_gbufferFB, m_ssaoFB, m_blurFB}) if (fb) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_gbufferFB = m_ssaoFB = m_blurFB = VK_NULL_HANDLE;

    // Render passes
    for (auto rp : {m_gbufferRenderPass, m_ssaoRenderPass, m_blurRenderPass, m_compositeRenderPass})
        if (rp) vkDestroyRenderPass(m_device, rp, nullptr);
    m_gbufferRenderPass = m_ssaoRenderPass = m_blurRenderPass = m_compositeRenderPass = VK_NULL_HANDLE;

    // Samplers
    if (m_linearSampler)  { vkDestroySampler(m_device, m_linearSampler,  nullptr); m_linearSampler  = VK_NULL_HANDLE; }
    if (m_nearestSampler) { vkDestroySampler(m_device, m_nearestSampler, nullptr); m_nearestSampler = VK_NULL_HANDLE; }

    // Images
    DestroySSAOImage(m_gColor);
    DestroySSAOImage(m_gPosition);
    DestroySSAOImage(m_gNormal);
    DestroySSAOImage(m_gbufDepth);
    DestroySSAOImage(m_ssaoRaw);
    DestroySSAOImage(m_ssaoBlur);
    DestroySSAOImage(m_ssaoNoise);

    // Noise staging buffer
    DestroyBuffer(m_ssaoNoiseStagingBuf, m_ssaoNoiseStagingMem);

    m_ssaoInitialized = false;
}

// ── Clash Compute Pipeline ─────────────────────────────────────────────────

bool VulkanRenderer::InitializeClashCompute() {
    if (m_clashComputeReady) return true;

    auto compSpv = LoadShaderSPV("clash_detection.comp.spv");
    if (compSpv.empty()) {
        LogCAD("Clash compute disabled — clash_detection.comp.spv not found");
        return false;
    }

    VkShaderModule compMod = CreateShaderModule(compSpv);
    if (!compMod) return false;

    // Descriptor set layout: 3 SSBOs (pipes, arches, results)
    VkDescriptorSetLayoutBinding bindings[3]{};
    for (int i = 0; i < 3; ++i) {
        bindings[i].binding            = i;
        bindings[i].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount    = 1;
        bindings[i].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslCI{};
    dslCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslCI.bindingCount = 3;
    dslCI.pBindings    = bindings;
    if (vkCreateDescriptorSetLayout(m_device, &dslCI, nullptr, &m_clashComputeDSLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, compMod, nullptr);
        return false;
    }

    // Push constant: 4 × uint/float = 16 bytes
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset     = 0;
    pc.size       = 16;

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &m_clashComputeDSLayout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &pc;
    if (vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_clashComputeLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, compMod, nullptr);
        return false;
    }

    VkComputePipelineCreateInfo cpCI{};
    cpCI.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpCI.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpCI.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    cpCI.stage.module = compMod;
    cpCI.stage.pName  = "main";
    cpCI.layout       = m_clashComputeLayout;
    bool ok = (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &cpCI, nullptr,
                                         &m_clashComputePipeline) == VK_SUCCESS);
    vkDestroyShaderModule(m_device, compMod, nullptr);

    if (!ok) {
        LogCAD("Clash compute pipeline creation failed");
        return false;
    }

    // Descriptor pool (one set, 3 storage buffers)
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 3;
    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets       = 1;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes    = &poolSize;
    if (vkCreateDescriptorPool(m_device, &poolCI, nullptr, &m_clashComputePool) != VK_SUCCESS)
        return false;

    m_clashComputeReady = true;
    LogCAD("Clash compute pipeline ready");
    return true;
}

void VulkanRenderer::ShutdownClashCompute() {
    if (!m_device) return;
    if (m_clashComputePipeline) { vkDestroyPipeline(m_device, m_clashComputePipeline, nullptr); m_clashComputePipeline = VK_NULL_HANDLE; }
    if (m_clashComputeLayout)   { vkDestroyPipelineLayout(m_device, m_clashComputeLayout, nullptr); m_clashComputeLayout = VK_NULL_HANDLE; }
    if (m_clashComputeDSLayout) { vkDestroyDescriptorSetLayout(m_device, m_clashComputeDSLayout, nullptr); m_clashComputeDSLayout = VK_NULL_HANDLE; }
    if (m_clashComputePool)     { vkDestroyDescriptorPool(m_device, m_clashComputePool, nullptr); m_clashComputePool = VK_NULL_HANDLE; }
    m_clashComputeReady = false;
}

std::vector<VulkanRenderer::GpuClashResult>
VulkanRenderer::RunClashDetectionGPU(const std::vector<GpuPipe>& pipes,
                                      const std::vector<GpuArchBox>& arches,
                                      float tolerance,
                                      uint32_t maxResults) {
    if (!m_clashComputeReady && !InitializeClashCompute())
        return {};                          // fall back to CPU path
    if (pipes.empty() || arches.empty())
        return {};

    uint32_t numPipes  = static_cast<uint32_t>(pipes.size());
    uint32_t numArches = static_cast<uint32_t>(arches.size());

    // ── Buffer sizes ──────────────────────────────────────────────────────────
    VkDeviceSize pipeSz   = numPipes  * sizeof(GpuPipe);
    VkDeviceSize archSz   = numArches * sizeof(GpuArchBox);
    // ResultBuffer layout: uint32_t count, then maxResults × GpuClashResult
    VkDeviceSize resultSz = sizeof(uint32_t) + maxResults * sizeof(GpuClashResult);

    // Device-local SSBOs (fast GPU access)
    VkBuffer pipeBuf{}, archBuf{}, resBuf{};
    VkDeviceMemory pipeMem{}, archMem{}, resMem{};
    auto deviceLocal  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    auto storageXfer  = static_cast<VkBufferUsageFlags>(
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto resultUsage  = static_cast<VkBufferUsageFlags>(
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    // Staging buffers (CPU-visible, write-only for input; read-only for result)
    VkBuffer stagePipeBuf{}, stageArchBuf{}, stageResBuf{};
    VkDeviceMemory stagePipeMem{}, stageArchMem{}, stageResMem{};
    auto hostCoherent = static_cast<VkMemoryPropertyFlags>(
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto stageSrcUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto stageDstUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    bool ok =
        CreateBuffer(pipeSz,   storageXfer,  deviceLocal,  pipeBuf,       pipeMem)  &&
        CreateBuffer(archSz,   storageXfer,  deviceLocal,  archBuf,       archMem)  &&
        CreateBuffer(resultSz, resultUsage,  deviceLocal,  resBuf,        resMem)   &&
        CreateBuffer(pipeSz,   stageSrcUsage, hostCoherent, stagePipeBuf, stagePipeMem) &&
        CreateBuffer(archSz,   stageSrcUsage, hostCoherent, stageArchBuf, stageArchMem) &&
        CreateBuffer(resultSz, stageDstUsage, hostCoherent, stageResBuf,  stageResMem);

    if (!ok) {
        for (auto [b, m] : std::initializer_list<std::pair<VkBuffer, VkDeviceMemory>>{
                {pipeBuf, pipeMem}, {archBuf, archMem}, {resBuf, resMem},
                {stagePipeBuf, stagePipeMem}, {stageArchBuf, stageArchMem}, {stageResBuf, stageResMem}})
            DestroyBuffer(b, m);
        return {};
    }

    // Upload inputs to staging, then copy to device-local
    void* p{};
    vkMapMemory(m_device, stagePipeMem, 0, pipeSz, 0, &p);
    std::memcpy(p, pipes.data(), pipeSz);
    vkUnmapMemory(m_device, stagePipeMem);

    vkMapMemory(m_device, stageArchMem, 0, archSz, 0, &p);
    std::memcpy(p, arches.data(), archSz);
    vkUnmapMemory(m_device, stageArchMem);

    // Zero result count in staging, copy to device
    vkMapMemory(m_device, stageResMem, 0, sizeof(uint32_t), 0, &p);
    std::memset(p, 0, sizeof(uint32_t));
    vkUnmapMemory(m_device, stageResMem);

    // ── Descriptor set ───────────────────────────────────────────────────────
    // Reset pool and allocate fresh set each call (simple; runs infrequently)
    vkResetDescriptorPool(m_device, m_clashComputePool, 0);

    VkDescriptorSetAllocateInfo dsAlloc{};
    dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAlloc.descriptorPool     = m_clashComputePool;
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts        = &m_clashComputeDSLayout;
    VkDescriptorSet ds{};
    if (vkAllocateDescriptorSets(m_device, &dsAlloc, &ds) != VK_SUCCESS) {
        DestroyBuffer(pipeBuf, pipeMem);
        DestroyBuffer(archBuf, archMem);
        DestroyBuffer(resBuf,  resMem);
        return {};
    }

    VkDescriptorBufferInfo dbi[3]{};
    dbi[0] = {pipeBuf, 0, pipeSz};
    dbi[1] = {archBuf, 0, archSz};
    dbi[2] = {resBuf,  0, resultSz};

    VkWriteDescriptorSet writes[3]{};
    for (int i = 0; i < 3; ++i) {
        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet          = ds;
        writes[i].dstBinding      = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo     = &dbi[i];
    }
    vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);

    // ── Single-use command buffer ────────────────────────────────────────────
    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool        = m_commandPool;
    cbAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;
    VkCommandBuffer cb{};
    vkAllocateCommandBuffers(m_device, &cbAlloc, &cb);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &beginInfo);

    // Transfer: staging → device-local SSBOs
    VkBufferCopy copyPipe{0, 0, pipeSz};
    VkBufferCopy copyArch{0, 0, archSz};
    VkBufferCopy copyRes{0, 0, resultSz};
    vkCmdCopyBuffer(cb, stagePipeBuf, pipeBuf, 1, &copyPipe);
    vkCmdCopyBuffer(cb, stageArchBuf, archBuf, 1, &copyArch);
    vkCmdCopyBuffer(cb, stageResBuf,  resBuf,  1, &copyRes);  // zero the count

    // Barrier: transfer writes → compute reads
    VkBufferMemoryBarrier xferBarriers[3]{};
    for (int i = 0; i < 3; ++i) {
        xferBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        xferBarriers[i].srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        xferBarriers[i].dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        xferBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        xferBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        xferBarriers[i].offset = 0;
        xferBarriers[i].size   = VK_WHOLE_SIZE;
    }
    xferBarriers[0].buffer = pipeBuf;
    xferBarriers[1].buffer = archBuf;
    xferBarriers[2].buffer = resBuf;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 3, xferBarriers, 0, nullptr);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_clashComputePipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_clashComputeLayout, 0, 1, &ds, 0, nullptr);

    struct { uint32_t pipeCount, archCount; float softTol; uint32_t maxRes; } pc;
    pc.pipeCount = numPipes;
    pc.archCount = numArches;
    pc.softTol   = tolerance;
    pc.maxRes    = maxResults;
    vkCmdPushConstants(cb, m_clashComputeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    uint32_t gx = (numPipes  + 15) / 16;
    uint32_t gy = (numArches + 15) / 16;
    vkCmdDispatch(cb, gx, gy, 1);

    // Barrier: compute writes → transfer reads (for result copy back)
    VkBufferMemoryBarrier computeBarrier{};
    computeBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    computeBarrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    computeBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    computeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    computeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    computeBarrier.buffer = resBuf;
    computeBarrier.offset = 0;
    computeBarrier.size   = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &computeBarrier, 0, nullptr);

    // Transfer: device-local result → staging readback buffer
    vkCmdCopyBuffer(cb, resBuf, stageResBuf, 1, &copyRes);

    vkEndCommandBuffer(cb);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cb;
    vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cb);

    // ── Read back results from staging buffer ────────────────────────────────
    std::vector<GpuClashResult> out;
    vkMapMemory(m_device, stageResMem, 0, resultSz, 0, &p);
    uint32_t count = *reinterpret_cast<uint32_t*>(p);
    count = std::min(count, maxResults);
    auto* resPtr = reinterpret_cast<GpuClashResult*>(
        reinterpret_cast<uint8_t*>(p) + sizeof(uint32_t));
    out.assign(resPtr, resPtr + count);
    vkUnmapMemory(m_device, stageResMem);

    DestroyBuffer(pipeBuf,      pipeMem);
    DestroyBuffer(archBuf,      archMem);
    DestroyBuffer(resBuf,       resMem);
    DestroyBuffer(stagePipeBuf, stagePipeMem);
    DestroyBuffer(stageArchBuf, stageArchMem);
    DestroyBuffer(stageResBuf,  stageResMem);
    return out;
}

} // namespace render
} // namespace vkt
