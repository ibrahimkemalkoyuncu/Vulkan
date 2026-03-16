/**
 * @file VulkanRenderer.cpp
 * @brief Vulkan Renderer implementasyonu — gerçek GPU rendering
 */

#include "render/VulkanRenderer.hpp"
#include "cad/Line.hpp"
#include "cad/Arc.hpp"
#include "cad/Circle.hpp"
#include "cad/Polyline.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// File-based diagnostic logging for CAD rendering pipeline
static void LogCAD(const std::string& msg) {
    static std::ofstream logFile("C:/Users/afney/Desktop/vkt_cad_debug.log", std::ios::trunc);
    if (logFile.is_open()) {
        logFile << msg << std::endl;
        logFile.flush();
    }
    std::cout << msg << std::endl;
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
        if (!CreateDepthResources()) throw std::runtime_error("Depth buffer olusturulamadi");
        if (!CreateRenderPass()) throw std::runtime_error("RenderPass olusturulamadi");
        if (!CreateFramebuffers()) throw std::runtime_error("Framebuffer olusturulamadi");
        if (!CreatePipelineLayout()) throw std::runtime_error("PipelineLayout olusturulamadi");
        if (!CreateGraphicsPipelines()) throw std::runtime_error("Pipeline olusturulamadi");
        if (!CreateCommandPool()) throw std::runtime_error("CommandPool olusturulamadi");
        if (!CreateCommandBuffers()) throw std::runtime_error("CommandBuffer olusturulamadi");
        if (!CreateSyncObjects()) throw std::runtime_error("Sync nesneleri olusturulamadi");

        CreateGridVertexData();

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

    // Vertex buffers
    DestroyBuffer(m_vertexBuffer, m_vertexMemory);
    DestroyBuffer(m_gridVertexBuffer, m_gridVertexMemory);
    DestroyBuffer(m_cadVertexBuffer, m_cadVertexMemory);

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
    clearValues[0].color = {{0.12f, 0.12f, 0.15f, 1.0f}}; // Koyu gri arka plan
    clearValues[1].depthStencil = {1.0f, 0};

    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    // Grid çiz
    DrawGrid(cmd);

    // Network çiz
    UpdateNetworkVertexData(network);
    DrawNetwork(cmd, network);
}

void VulkanRenderer::EndFrame() {
    if (!m_initialized || !m_frameActive) return;
    m_frameActive = false;

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    vkCmdEndRenderPass(cmd);
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

uint32_t VulkanRenderer::Pick(int /*screenX*/, int /*screenY*/) {
    // TODO: GPU-based color picking veya ray-cast
    return 0;
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
    deviceFeatures.wideLines = VK_TRUE; // Boru çizimi için kalın çizgi
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
    CleanupSwapchain();
    CreateSwapchain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
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
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

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
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    return vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass) == VK_SUCCESS;
}

bool VulkanRenderer::CreateFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            m_swapchainImageViews[i],
            m_depthImageView
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = m_swapchainExtent.width;
        fbInfo.height = m_swapchainExtent.height;
        fbInfo.layers = 1;

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
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
    // 2D grid: -50m to +50m, her 1m'de bir çizgi
    std::vector<geom::Vertex> vertices;
    const float extent = 50.0f;
    const float step = 1.0f;
    const float gridColor[3] = {0.25f, 0.25f, 0.30f};
    const float axisColorX[3] = {0.6f, 0.2f, 0.2f};
    const float axisColorY[3] = {0.2f, 0.6f, 0.2f};

    for (float i = -extent; i <= extent; i += step) {
        const float* color = gridColor;
        if (std::abs(i) < 0.01f) continue; // Eksenleri ayrı çiz

        // X yönü çizgileri
        geom::Vertex v1{}, v2{};
        v1.pos[0] = -extent; v1.pos[1] = i; v1.pos[2] = 0.0f;
        v2.pos[0] = extent;  v2.pos[1] = i; v2.pos[2] = 0.0f;
        std::copy(color, color + 3, v1.color);
        std::copy(color, color + 3, v2.color);
        vertices.push_back(v1);
        vertices.push_back(v2);

        // Y yönü çizgileri
        v1.pos[0] = i; v1.pos[1] = -extent; v1.pos[2] = 0.0f;
        v2.pos[0] = i; v2.pos[1] = extent;  v2.pos[2] = 0.0f;
        vertices.push_back(v1);
        vertices.push_back(v2);
    }

    // X ekseni (kırmızı)
    {
        geom::Vertex v1{}, v2{};
        v1.pos[0] = -extent; v1.pos[1] = 0; v1.pos[2] = 0;
        v2.pos[0] = extent;  v2.pos[1] = 0; v2.pos[2] = 0;
        std::copy(axisColorX, axisColorX + 3, v1.color);
        std::copy(axisColorX, axisColorX + 3, v2.color);
        vertices.push_back(v1);
        vertices.push_back(v2);
    }

    // Y ekseni (yeşil)
    {
        geom::Vertex v1{}, v2{};
        v1.pos[0] = 0; v1.pos[1] = -extent; v1.pos[2] = 0;
        v2.pos[0] = 0; v2.pos[1] = extent;  v2.pos[2] = 0;
        std::copy(axisColorY, axisColorY + 3, v1.color);
        std::copy(axisColorY, axisColorY + 3, v2.color);
        vertices.push_back(v1);
        vertices.push_back(v2);
    }

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
    vertices.reserve(entities.size() * 8); // rough estimate

    int lineCount = 0, arcCount = 0, circleCount = 0, polyCount = 0, skipCount = 0;

    for (const auto& entity : entities) {
        if (!entity) { skipCount++; continue; }

        size_t prevSize = vertices.size();

        // Determine color from entity
        cad::Color col = entity->GetColor();
        float r, g, b;
        if (col.a == 0) {
            // ByLayer — default to white
            r = 1.0f; g = 1.0f; b = 1.0f;
        } else {
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

        switch (entity->GetType()) {
        case cad::EntityType::Line: {
            const auto* line = static_cast<const cad::Line*>(entity.get());
            auto s = line->GetStart();
            auto e = line->GetEnd();
            addVertex(s.x, s.y, s.z);
            addVertex(e.x, e.y, e.z);
            lineCount++;
            break;
        }
        case cad::EntityType::Arc: {
            const auto* arc = static_cast<const cad::Arc*>(entity.get());
            double sweep = arc->GetSweepAngleRadians();
            int segments = std::max(4, static_cast<int>(std::abs(sweep) / (M_PI / 16.0)));
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
            constexpr int segments = 32;
            auto center = circle->GetCenter();
            double radius = circle->GetRadius();
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
                    addVertex(v0.pos.x, v0.pos.y, v0.pos.z);
                    addVertex(v1.pos.x, v1.pos.y, v1.pos.z);
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);

    PushConstants pc;
    pc.mvp = m_viewProjection;
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_cadVertexBuffer, offsets);
    vkCmdDraw(cmd, m_cadLineVertexCount, 1, 0, 0);
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

} // namespace render
} // namespace vkt
