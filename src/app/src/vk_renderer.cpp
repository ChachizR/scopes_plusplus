#include "vk_renderer.hpp"

#include "runtime_paths.hpp"

namespace scpp {

namespace {

[[nodiscard]]
auto IsExtensionAvailable(std::span<const VkExtensionProperties> properties, std::string_view extension) noexcept -> bool {
    return std::ranges::any_of(properties, [extension](const VkExtensionProperties& property) {
        return extension == property.extensionName;
    });
}

} // namespace

VulkanRenderer::VulkanRenderer(GLFWwindow* window)
    : m_window{window} {
    if (!CreateInstance() || !SelectPhysicalDevice() || !CreateDevice() || !CreateDescriptorPool() || !CreateUploadCommandPool()) {
        Shutdown();
        return;
    }

    if (m_window != nullptr && !CreateWindowSurfaceAndSwapchain()) {
        Shutdown();
        return;
    }

    m_initialized = true;
    std::println("Initialized Vulkan renderer with device '{}'", GetDeviceName());
}

VulkanRenderer::~VulkanRenderer() {
    Shutdown();
}

auto VulkanRenderer::GetDeviceName() const noexcept -> std::string_view {
    if (m_physicalDevice == VK_NULL_HANDLE) {
        return {};
    }

    return m_physicalDeviceProperties.deviceName;
}

auto VulkanRenderer::InitImGuiBackend() noexcept -> bool {
    if (!m_initialized || m_window == nullptr || m_mainWindowData.Surface == VK_NULL_HANDLE) {
        return false;
    }

    if (!ImGui_ImplGlfw_InitForVulkan(m_window, true)) {
        m_lastError = "ImGui_ImplGlfw_InitForVulkan failed";
        std::println("{}", m_lastError);
        return false;
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion                   = VK_API_VERSION_1_2;
    initInfo.Instance                     = m_instance;
    initInfo.PhysicalDevice               = m_physicalDevice;
    initInfo.Device                       = m_device;
    initInfo.QueueFamily                  = m_queueFamily;
    initInfo.Queue                        = m_queue;
    initInfo.PipelineCache                = m_pipelineCache;
    initInfo.DescriptorPool               = m_descriptorPool;
    initInfo.MinImageCount                = m_minImageCount;
    initInfo.ImageCount                   = m_mainWindowData.ImageCount;
    initInfo.PipelineInfoMain.RenderPass  = m_mainWindowData.RenderPass;
    initInfo.PipelineInfoMain.Subpass     = 0u;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        m_lastError = "ImGui_ImplVulkan_Init failed";
        std::println("{}", m_lastError);
        ImGui_ImplGlfw_Shutdown();
        return false;
    }

    m_imguiBackendInitialized = true;
    return true;
}

void VulkanRenderer::ShutdownImGuiBackend() noexcept {
    if (!m_imguiBackendInitialized) {
        return;
    }

    WaitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    m_imguiBackendInitialized = false;
}

void VulkanRenderer::NewFrame() noexcept {
    if (!m_imguiBackendInitialized) {
        return;
    }

    RebuildSwapchainIfNeeded();
    ImGui_ImplVulkan_NewFrame();
}

void VulkanRenderer::RenderFrame(ImVec4 clearColor) noexcept {
    if (!m_imguiBackendInitialized) {
        return;
    }

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData == nullptr || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f) {
        return;
    }

    m_mainWindowData.ClearValue.color.float32[0] = clearColor.x * clearColor.w;
    m_mainWindowData.ClearValue.color.float32[1] = clearColor.y * clearColor.w;
    m_mainWindowData.ClearValue.color.float32[2] = clearColor.z * clearColor.w;
    m_mainWindowData.ClearValue.color.float32[3] = clearColor.w;

    VkSemaphore imageAcquiredSemaphore  = m_mainWindowData.FrameSemaphores[m_mainWindowData.SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore renderCompleteSemaphore = m_mainWindowData.FrameSemaphores[m_mainWindowData.SemaphoreIndex].RenderCompleteSemaphore;

    auto result = vkAcquireNextImageKHR(m_device, m_mainWindowData.Swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &m_mainWindowData.FrameIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_swapChainRebuild = true;
        return;
    }
    if (result != VK_SUCCESS) {
        std::println("vkAcquireNextImageKHR failed: {}", VulkanResultToString(result));
        return;
    }

    ImGui_ImplVulkanH_Frame* frame = &m_mainWindowData.Frames[m_mainWindowData.FrameIndex];

    result = vkWaitForFences(m_device, 1u, &frame->Fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        std::println("vkWaitForFences failed: {}", VulkanResultToString(result));
        return;
    }
    vkResetFences(m_device, 1u, &frame->Fence);
    vkResetCommandPool(m_device, frame->CommandPool, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    result = vkBeginCommandBuffer(frame->CommandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        std::println("vkBeginCommandBuffer failed: {}", VulkanResultToString(result));
        return;
    }

    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext           = nullptr,
        .renderPass      = m_mainWindowData.RenderPass,
        .framebuffer     = frame->Framebuffer,
        .renderArea      = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{static_cast<uint32_t>(m_mainWindowData.Width), static_cast<uint32_t>(m_mainWindowData.Height)}},
        .clearValueCount = 1u,
        .pClearValues    = &m_mainWindowData.ClearValue,
    };

    vkCmdBeginRenderPass(frame->CommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(drawData, frame->CommandBuffer);
    vkCmdEndRenderPass(frame->CommandBuffer);

    result = vkEndCommandBuffer(frame->CommandBuffer);
    if (result != VK_SUCCESS) {
        std::println("vkEndCommandBuffer failed: {}", VulkanResultToString(result));
        return;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 1u,
        .pWaitSemaphores      = &imageAcquiredSemaphore,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1u,
        .pCommandBuffers      = &frame->CommandBuffer,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores    = &renderCompleteSemaphore,
    };

    result = vkQueueSubmit(m_queue, 1u, &submitInfo, frame->Fence);
    if (result != VK_SUCCESS) {
        std::println("vkQueueSubmit failed: {}", VulkanResultToString(result));
        return;
    }

    VkPresentInfoKHR presentInfo{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1u,
        .pWaitSemaphores    = &renderCompleteSemaphore,
        .swapchainCount     = 1u,
        .pSwapchains        = &m_mainWindowData.Swapchain,
        .pImageIndices      = &m_mainWindowData.FrameIndex,
        .pResults           = nullptr,
    };

    result = vkQueuePresentKHR(m_queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_swapChainRebuild = true;
        return;
    }
    if (result != VK_SUCCESS) {
        std::println("vkQueuePresentKHR failed: {}", VulkanResultToString(result));
        return;
    }

    m_mainWindowData.SemaphoreIndex = (m_mainWindowData.SemaphoreIndex + 1u) % m_mainWindowData.SemaphoreCount;
}

void VulkanRenderer::WaitIdle() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

auto VulkanRenderer::UploadSourcePreview(SourceFrameView frame) noexcept -> bool {
    if (!m_imguiBackendInitialized || frame.format != SourceFormat::BGRA_8888 || frame.dims.Area() == 0u || frame.data.empty()) {
        return false;
    }
    if (frame.sequence == m_sourcePreviewSequence && m_sourcePreviewDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    const auto expectedTightSize = static_cast<size_t>(frame.dims.width) * static_cast<size_t>(frame.dims.height) * 4u;
    const uint8_t* uploadData    = frame.data.data();
    std::vector<uint8_t> packedData;

    if (frame.lineStrideBytes != frame.dims.width * 4u) {
        packedData.resize(expectedTightSize);
        for (uint32_t y = 0u; y < frame.dims.height; ++y) {
            const auto srcOffset = static_cast<size_t>(y) * frame.lineStrideBytes;
            const auto dstOffset = static_cast<size_t>(y) * frame.dims.width * 4u;
            std::memcpy(packedData.data() + dstOffset, frame.data.data() + srcOffset, static_cast<size_t>(frame.dims.width) * 4u);
        }
        uploadData = packedData.data();
    }

    if (m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims != frame.dims) {
        WaitIdle();
        DestroyFalseColorResources();
        DestroyLumaWaveformResources();
        DestroyRgbWaveformResources();
        DestroyRgbParadeResources();
        DestroyRgbBlacklevelResources();
        DestroyYuvParadeResources();
        DestroyUvScopeResources();
        DestroyXyzScopeResources();
        DestroyDiamondScopeResources();
        DestroySourcePreviewImage();
        if (!RecreateSourcePreviewImage(frame.dims)) {
            return false;
        }
    }

    if (!WaitForPreviewUpload() || !EnsureSourcePreviewStagingBuffer(expectedTightSize)) {
        return false;
    }

    std::memcpy(m_sourcePreviewStagingMapped, uploadData, expectedTightSize);

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    TransitionImageLayout(commandBuffer, m_sourcePreviewImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(commandBuffer, m_sourcePreviewStagingBuffer, m_sourcePreviewImage, frame.dims);
    TransitionImageLayout(commandBuffer, m_sourcePreviewImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_sourcePreviewSequence = frame.sequence;
    return true;
}

auto VulkanRenderer::RenderFalseColor(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool falseColorCurrent =
        m_falseColorImGuiDescriptor != VK_NULL_HANDLE &&
        m_falseColorRenderedSourceSequence == m_sourcePreviewSequence &&
        m_falseColorRenderedColorSpace == renderSettings.colorSpace &&
        m_falseColorRenderedYuvRange == renderSettings.yuvRange &&
        !m_falseColorMapDirty;

    if (falseColorCurrent) {
        return true;
    }

    if (!EnsureFalseColorResources()) {
        return false;
    }

    if (m_falseColorUploadedRange != renderSettings.yuvRange) {
        m_falseColorMapDirty = true;
    }
    if (!UpdateFalseColorMapBuffer(renderSettings.yuvRange)) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    TransitionImageLayout(commandBuffer, m_falseColorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_falseColorPipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_falseColorPipelineLayout,
        0u,
        1u,
        &m_falseColorDescriptorSet,
        0u,
        nullptr);

    struct PushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
        int32_t  yuvRange;
    } constants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace),
        static_cast<int32_t>(renderSettings.yuvRange)};

    vkCmdPushConstants(commandBuffer, m_falseColorPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(PushConstants), &constants);

    const auto groupsX = (m_sourcePreviewDims.width + 15u) / 16u;
    const auto groupsY = (m_sourcePreviewDims.height + 15u) / 16u;
    vkCmdDispatch(commandBuffer, groupsX, groupsY, 1u);

    TransitionImageLayout(commandBuffer, m_falseColorImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_falseColorRenderedSourceSequence = m_sourcePreviewSequence;
    m_falseColorRenderedColorSpace     = renderSettings.colorSpace;
    m_falseColorRenderedYuvRange       = renderSettings.yuvRange;
    return true;
}

auto VulkanRenderer::RenderLumaWaveform(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool waveformCurrent =
        m_lumaWaveformImGuiDescriptor != VK_NULL_HANDLE &&
        m_lumaWaveformRenderedSourceSequence == m_sourcePreviewSequence &&
        m_lumaWaveformRenderedColorSpace == renderSettings.colorSpace &&
        m_lumaWaveformRenderedYuvRange == renderSettings.yuvRange;

    if (waveformCurrent) {
        return true;
    }

    if (!EnsureLumaWaveformResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkDeviceSize histogramBytes = 580u * 256u * sizeof(uint32_t);
    vkCmdFillBuffer(commandBuffer, m_lumaWaveformHistogramBuffer, 0u, histogramBytes, 0u);

    VkBufferMemoryBarrier clearBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_lumaWaveformHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &clearBarrier,
        0u,
        nullptr);

    struct AccumulatePushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
        int32_t  yuvRange;
    } accumulateConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace),
        static_cast<int32_t>(renderSettings.yuvRange)};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_lumaWaveformAccumulatePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_lumaWaveformAccumulatePipelineLayout,
        0u,
        1u,
        &m_lumaWaveformAccumulateDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_lumaWaveformAccumulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(AccumulatePushConstants), &accumulateConstants);
    vkCmdDispatch(commandBuffer, (m_sourcePreviewDims.width + 15u) / 16u, (m_sourcePreviewDims.height + 15u) / 16u, 1u);

    VkBufferMemoryBarrier accumulateBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_lumaWaveformHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &accumulateBarrier,
        0u,
        nullptr);

    TransitionImageLayout(commandBuffer, m_lumaWaveformImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    const float sampleCount = std::ceil(static_cast<float>(m_sourcePreviewDims.width) / 580.0f);
    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        (1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))) / sampleCount};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_lumaWaveformImagePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_lumaWaveformImagePipelineLayout,
        0u,
        1u,
        &m_lumaWaveformImageDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_lumaWaveformImagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (580u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_lumaWaveformImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_lumaWaveformRenderedSourceSequence = m_sourcePreviewSequence;
    m_lumaWaveformRenderedColorSpace     = renderSettings.colorSpace;
    m_lumaWaveformRenderedYuvRange       = renderSettings.yuvRange;
    return true;
}

auto VulkanRenderer::RenderRgbWaveform(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool waveformCurrent =
        m_rgbWaveformImGuiDescriptor != VK_NULL_HANDLE &&
        m_rgbWaveformRenderedSourceSequence == m_sourcePreviewSequence &&
        m_rgbWaveformRenderedColorSpace == renderSettings.colorSpace;

    if (waveformCurrent) {
        return true;
    }

    if (!EnsureRgbWaveformResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkDeviceSize histogramBytes = 580u * 256u * 3u * sizeof(uint32_t);
    vkCmdFillBuffer(commandBuffer, m_rgbWaveformHistogramBuffer, 0u, histogramBytes, 0u);

    VkBufferMemoryBarrier clearBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_rgbWaveformHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &clearBarrier,
        0u,
        nullptr);

    struct AccumulatePushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
    } accumulateConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace)};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_rgbWaveformAccumulatePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_rgbWaveformAccumulatePipelineLayout,
        0u,
        1u,
        &m_rgbWaveformAccumulateDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_rgbWaveformAccumulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(AccumulatePushConstants), &accumulateConstants);
    vkCmdDispatch(commandBuffer, (m_sourcePreviewDims.width + 15u) / 16u, (m_sourcePreviewDims.height + 15u) / 16u, 1u);

    VkBufferMemoryBarrier accumulateBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_rgbWaveformHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &accumulateBarrier,
        0u,
        nullptr);

    TransitionImageLayout(commandBuffer, m_rgbWaveformImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    const float sampleCount = std::ceil(static_cast<float>(m_sourcePreviewDims.width) / 580.0f);
    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        (1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))) / sampleCount};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_rgbWaveformImagePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_rgbWaveformImagePipelineLayout,
        0u,
        1u,
        &m_rgbWaveformImageDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_rgbWaveformImagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (580u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_rgbWaveformImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_rgbWaveformRenderedSourceSequence = m_sourcePreviewSequence;
    m_rgbWaveformRenderedColorSpace     = renderSettings.colorSpace;
    return true;
}

auto VulkanRenderer::RenderRgbParade(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool paradeCurrent =
        m_rgbParadeImGuiDescriptor != VK_NULL_HANDLE &&
        m_rgbParadeRenderedSourceSequence == m_sourcePreviewSequence &&
        m_rgbParadeRenderedColorSpace == renderSettings.colorSpace;

    if (paradeCurrent) {
        return true;
    }

    if (m_rgbWaveformRenderedSourceSequence != m_sourcePreviewSequence || m_rgbWaveformRenderedColorSpace != renderSettings.colorSpace) {
        if (!RenderRgbWaveform(renderSettings)) {
            return false;
        }
    }

    if (!EnsureRgbParadeResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    TransitionImageLayout(commandBuffer, m_rgbParadeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    const float sampleCount = std::ceil(static_cast<float>(m_sourcePreviewDims.width) / 580.0f);
    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        (1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))) / sampleCount};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_rgbParadePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_rgbParadePipelineLayout,
        0u,
        1u,
        &m_rgbParadeDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_rgbParadePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (580u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_rgbParadeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_rgbParadeRenderedSourceSequence = m_sourcePreviewSequence;
    m_rgbParadeRenderedColorSpace     = renderSettings.colorSpace;
    return true;
}

auto VulkanRenderer::RenderRgbBlacklevel(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool blacklevelCurrent =
        m_rgbBlacklevelImGuiDescriptor != VK_NULL_HANDLE &&
        m_rgbBlacklevelRenderedSourceSequence == m_sourcePreviewSequence &&
        m_rgbBlacklevelRenderedColorSpace == renderSettings.colorSpace;

    if (blacklevelCurrent) {
        return true;
    }

    if (m_rgbWaveformRenderedSourceSequence != m_sourcePreviewSequence || m_rgbWaveformRenderedColorSpace != renderSettings.colorSpace) {
        if (!RenderRgbWaveform(renderSettings)) {
            return false;
        }
    }

    if (!EnsureRgbBlacklevelResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    TransitionImageLayout(commandBuffer, m_rgbBlacklevelImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    const float sampleCount = std::ceil(static_cast<float>(m_sourcePreviewDims.width) / 580.0f);
    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        (1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))) / sampleCount};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_rgbBlacklevelPipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_rgbBlacklevelPipelineLayout,
        0u,
        1u,
        &m_rgbBlacklevelDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_rgbBlacklevelPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (580u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_rgbBlacklevelImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_rgbBlacklevelRenderedSourceSequence = m_sourcePreviewSequence;
    m_rgbBlacklevelRenderedColorSpace     = renderSettings.colorSpace;
    return true;
}

auto VulkanRenderer::RenderYuvParade(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool paradeCurrent =
        m_yuvParadeImGuiDescriptor != VK_NULL_HANDLE &&
        m_yuvParadeRenderedSourceSequence == m_sourcePreviewSequence &&
        m_yuvParadeRenderedColorSpace == renderSettings.colorSpace &&
        m_yuvParadeRenderedYuvRange == renderSettings.yuvRange;

    if (paradeCurrent) {
        return true;
    }

    if (!EnsureYuvParadeResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkDeviceSize histogramBytes = 580u * 256u * 3u * sizeof(uint32_t);
    vkCmdFillBuffer(commandBuffer, m_yuvWaveformHistogramBuffer, 0u, histogramBytes, 0u);

    VkBufferMemoryBarrier clearBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_yuvWaveformHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &clearBarrier,
        0u,
        nullptr);

    struct AccumulatePushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
        int32_t  yuvRange;
    } accumulateConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace),
        static_cast<int32_t>(renderSettings.yuvRange)};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_yuvWaveformAccumulatePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_yuvWaveformAccumulatePipelineLayout,
        0u,
        1u,
        &m_yuvWaveformAccumulateDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_yuvWaveformAccumulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(AccumulatePushConstants), &accumulateConstants);
    vkCmdDispatch(commandBuffer, (m_sourcePreviewDims.width + 15u) / 16u, (m_sourcePreviewDims.height + 15u) / 16u, 1u);

    VkBufferMemoryBarrier accumulateBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_yuvWaveformHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &accumulateBarrier,
        0u,
        nullptr);

    TransitionImageLayout(commandBuffer, m_yuvParadeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    const float sampleCount = std::ceil(static_cast<float>(m_sourcePreviewDims.width) / 580.0f);
    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        (1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))) / sampleCount};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_yuvParadePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_yuvParadePipelineLayout,
        0u,
        1u,
        &m_yuvParadeDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_yuvParadePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (580u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_yuvParadeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_yuvParadeRenderedSourceSequence = m_sourcePreviewSequence;
    m_yuvParadeRenderedColorSpace     = renderSettings.colorSpace;
    m_yuvParadeRenderedYuvRange       = renderSettings.yuvRange;
    return true;
}

auto VulkanRenderer::RenderUvScope(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool scopeCurrent =
        m_uvScopeImGuiDescriptor != VK_NULL_HANDLE &&
        m_uvScopeRenderedSourceSequence == m_sourcePreviewSequence &&
        m_uvScopeRenderedColorSpace == renderSettings.colorSpace &&
        m_uvScopeRenderedYuvRange == renderSettings.yuvRange;

    if (scopeCurrent) {
        return true;
    }

    if (!EnsureUvScopeResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkDeviceSize histogramBytes = 256u * 256u * sizeof(uint32_t);
    vkCmdFillBuffer(commandBuffer, m_uvScopeHistogramBuffer, 0u, histogramBytes, 0u);

    VkBufferMemoryBarrier clearBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_uvScopeHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &clearBarrier,
        0u,
        nullptr);

    struct AccumulatePushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
        int32_t  yuvRange;
    } accumulateConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace),
        static_cast<int32_t>(renderSettings.yuvRange)};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_uvScopeAccumulatePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_uvScopeAccumulatePipelineLayout,
        0u,
        1u,
        &m_uvScopeAccumulateDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_uvScopeAccumulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(AccumulatePushConstants), &accumulateConstants);
    vkCmdDispatch(commandBuffer, (m_sourcePreviewDims.width + 15u) / 16u, (m_sourcePreviewDims.height + 15u) / 16u, 1u);

    VkBufferMemoryBarrier accumulateBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_uvScopeHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &accumulateBarrier,
        0u,
        nullptr);

    TransitionImageLayout(commandBuffer, m_uvScopeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_uvScopeImagePipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_uvScopeImagePipelineLayout,
        0u,
        1u,
        &m_uvScopeImageDescriptorSet,
        0u,
        nullptr);
    vkCmdPushConstants(commandBuffer, m_uvScopeImagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (256u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_uvScopeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_uvScopeRenderedSourceSequence = m_sourcePreviewSequence;
    m_uvScopeRenderedColorSpace     = renderSettings.colorSpace;
    m_uvScopeRenderedYuvRange       = renderSettings.yuvRange;
    return true;
}

auto VulkanRenderer::RenderXyzScope(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool scopeCurrent =
        m_xyzScopeImGuiDescriptor != VK_NULL_HANDLE &&
        m_xyzScopeRenderedSourceSequence == m_sourcePreviewSequence &&
        m_xyzScopeRenderedColorSpace == renderSettings.colorSpace;

    if (scopeCurrent) {
        return true;
    }

    if (!EnsureXyzScopeResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkDeviceSize histogramBytes = 256u * 256u * sizeof(uint32_t);
    vkCmdFillBuffer(commandBuffer, m_xyzScopeHistogramBuffer, 0u, histogramBytes, 0u);

    VkBufferMemoryBarrier clearBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_xyzScopeHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &clearBarrier, 0u, nullptr);

    struct AccumulatePushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
    } accumulateConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace)};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_xyzScopeAccumulatePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_xyzScopeAccumulatePipelineLayout, 0u, 1u, &m_xyzScopeAccumulateDescriptorSet, 0u, nullptr);
    vkCmdPushConstants(commandBuffer, m_xyzScopeAccumulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(AccumulatePushConstants), &accumulateConstants);
    vkCmdDispatch(commandBuffer, (m_sourcePreviewDims.width + 15u) / 16u, (m_sourcePreviewDims.height + 15u) / 16u, 1u);

    VkBufferMemoryBarrier accumulateBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_xyzScopeHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &accumulateBarrier, 0u, nullptr);

    TransitionImageLayout(commandBuffer, m_xyzScopeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_xyzScopeImagePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_xyzScopeImagePipelineLayout, 0u, 1u, &m_xyzScopeImageDescriptorSet, 0u, nullptr);
    vkCmdPushConstants(commandBuffer, m_xyzScopeImagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (256u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_xyzScopeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_xyzScopeRenderedSourceSequence = m_sourcePreviewSequence;
    m_xyzScopeRenderedColorSpace     = renderSettings.colorSpace;
    return true;
}

auto VulkanRenderer::RenderDiamondScope(RenderSettings renderSettings) noexcept -> bool {
    if (!m_imguiBackendInitialized || m_sourcePreviewImage == VK_NULL_HANDLE || m_sourcePreviewDims.Area() == 0u) {
        return false;
    }

    const bool scopeCurrent =
        m_diamondScopeImGuiDescriptor != VK_NULL_HANDLE &&
        m_diamondScopeRenderedSourceSequence == m_sourcePreviewSequence &&
        m_diamondScopeRenderedColorSpace == renderSettings.colorSpace;

    if (scopeCurrent) {
        return true;
    }

    if (!EnsureDiamondScopeResources()) {
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    constexpr VkDeviceSize histogramBytes = 256u * 256u * sizeof(uint32_t);
    vkCmdFillBuffer(commandBuffer, m_diamondScopeHistogramBuffer, 0u, histogramBytes, 0u);

    VkBufferMemoryBarrier clearBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_diamondScopeHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &clearBarrier, 0u, nullptr);

    struct AccumulatePushConstants {
        uint32_t width;
        uint32_t height;
        int32_t  colorSpace;
    } accumulateConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        static_cast<int32_t>(renderSettings.colorSpace)};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_diamondScopeAccumulatePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_diamondScopeAccumulatePipelineLayout, 0u, 1u, &m_diamondScopeAccumulateDescriptorSet, 0u, nullptr);
    vkCmdPushConstants(commandBuffer, m_diamondScopeAccumulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(AccumulatePushConstants), &accumulateConstants);
    vkCmdDispatch(commandBuffer, (m_sourcePreviewDims.width + 15u) / 16u, (m_sourcePreviewDims.height + 15u) / 16u, 1u);

    VkBufferMemoryBarrier accumulateBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = m_diamondScopeHistogramBuffer,
        .offset              = 0u,
        .size                = histogramBytes,
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &accumulateBarrier, 0u, nullptr);

    TransitionImageLayout(commandBuffer, m_diamondScopeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    struct ImagePushConstants {
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        float    brightness;
    } imageConstants{
        m_sourcePreviewDims.width,
        m_sourcePreviewDims.height,
        1.0f + 8.0f * (1080.0f / static_cast<float>(m_sourcePreviewDims.height))};

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_diamondScopeImagePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_diamondScopeImagePipelineLayout, 0u, 1u, &m_diamondScopeImageDescriptorSet, 0u, nullptr);
    vkCmdPushConstants(commandBuffer, m_diamondScopeImagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(ImagePushConstants), &imageConstants);
    vkCmdDispatch(commandBuffer, (256u + 15u) / 16u, (256u + 15u) / 16u, 1u);

    TransitionImageLayout(commandBuffer, m_diamondScopeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer)) {
        return false;
    }

    m_diamondScopeRenderedSourceSequence = m_sourcePreviewSequence;
    m_diamondScopeRenderedColorSpace     = renderSettings.colorSpace;
    return true;
}

void VulkanRenderer::SetFalseColorMap(FalseColorMapDataRef map, std::string_view name) {
    m_falseColorMap                 = FalseColorMap(map);
    m_selectedFalseColorMapName     = name;
    m_falseColorMapDirty            = true;
}

auto VulkanRenderer::CreateInstance() noexcept -> bool {
    std::vector<const char*> instanceExtensions;

    if (m_window != nullptr) {
        if (!glfwVulkanSupported()) {
            m_lastError = "GLFW reports Vulkan is not supported";
            std::println("{}", m_lastError);
            return false;
        }

        uint32_t glfwExtensionCount = 0u;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (glfwExtensions == nullptr || glfwExtensionCount == 0u) {
            m_lastError = "glfwGetRequiredInstanceExtensions returned no extensions";
            std::println("{}", m_lastError);
            return false;
        }

        instanceExtensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

    uint32_t extensionPropertyCount = 0u;
    auto result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionPropertyCount, nullptr);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkEnumerateInstanceExtensionProperties failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionPropertyCount, extensionProperties.data());
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkEnumerateInstanceExtensionProperties list failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    VkInstanceCreateFlags instanceFlags = 0;
    if (IsExtensionAvailable(extensionProperties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (IsExtensionAvailable(extensionProperties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        instanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = nullptr,
        .pApplicationName   = "Scopes++",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName        = "Scopes++ Vulkan",
        .engineVersion      = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion         = VK_API_VERSION_1_2,
    };

    VkInstanceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = instanceFlags,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = 0u,
        .ppEnabledLayerNames     = nullptr,
        .enabledExtensionCount   = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };

    result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkCreateInstance failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    return true;
}

auto VulkanRenderer::SelectPhysicalDevice() noexcept -> bool {
    uint32_t deviceCount = 0u;
    auto result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0u) {
        m_lastError = std::format("vkEnumeratePhysicalDevices failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkEnumeratePhysicalDevices list failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    for (const auto device : devices) {
        uint32_t queueFamilyCount = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t family = 0u; family < queueFamilyCount; ++family) {
            if ((queueFamilies[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) {
                continue;
            }

            m_physicalDevice = device;
            m_queueFamily    = family;
            vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);
            return true;
        }
    }

    m_lastError = "No Vulkan graphics queue family found";
    std::println("{}", m_lastError);
    return false;
}

auto VulkanRenderer::CreateDevice() noexcept -> bool {
    std::vector<const char*> deviceExtensions;
    if (m_window != nullptr) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    uint32_t extensionPropertyCount = 0u;
    auto result = vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionPropertyCount, nullptr);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkEnumerateDeviceExtensionProperties failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
    result = vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionPropertyCount, extensionProperties.data());
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkEnumerateDeviceExtensionProperties list failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (IsExtensionAvailable(extensionProperties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
        deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }
#endif

    constexpr float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo{
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .queueFamilyIndex = m_queueFamily,
        .queueCount       = 1u,
        .pQueuePriorities = &queuePriority,
    };

    VkDeviceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .queueCreateInfoCount    = 1u,
        .pQueueCreateInfos       = &queueInfo,
        .enabledLayerCount       = 0u,
        .ppEnabledLayerNames     = nullptr,
        .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures        = nullptr,
    };

    result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkCreateDevice failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    vkGetDeviceQueue(m_device, m_queueFamily, 0u, &m_queue);
    return true;
}

auto VulkanRenderer::CreateDescriptorPool() noexcept -> bool {
    constexpr std::array poolSizes{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 32u},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32u},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32u},
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 64u,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data(),
    };

    const auto result = vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkCreateDescriptorPool failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    return true;
}

auto VulkanRenderer::CreateUploadCommandPool() noexcept -> bool {
    VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_queueFamily,
    };

    auto result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_uploadCommandPool);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkCreateCommandPool failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_uploadCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u,
    };

    result = vkAllocateCommandBuffers(m_device, &allocInfo, &m_uploadCommandBuffer);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkAllocateCommandBuffers failed for preview uploads: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    result = vkCreateFence(m_device, &fenceInfo, nullptr, &m_uploadFence);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("vkCreateFence failed for preview uploads: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    return true;
}

auto VulkanRenderer::CreateWindowSurfaceAndSwapchain() noexcept -> bool {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    auto result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        m_lastError = std::format("glfwCreateWindowSurface failed: {}", VulkanResultToString(result));
        std::println("{}", m_lastError);
        return false;
    }

    VkBool32 hasSurfaceSupport = VK_FALSE;
    result = vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_queueFamily, surface, &hasSurfaceSupport);
    if (result != VK_SUCCESS || hasSurfaceSupport != VK_TRUE) {
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
        m_lastError = "Selected Vulkan queue family does not support the GLFW window surface";
        std::println("{}", m_lastError);
        return false;
    }

    constexpr std::array requestedFormats{
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };

    m_mainWindowData.Surface       = surface;
    m_mainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        m_physicalDevice,
        m_mainWindowData.Surface,
        requestedFormats.data(),
        static_cast<int>(requestedFormats.size()),
        VK_COLORSPACE_SRGB_NONLINEAR_KHR);

    constexpr std::array presentModes{VK_PRESENT_MODE_FIFO_KHR};
    m_mainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        m_physicalDevice,
        m_mainWindowData.Surface,
        presentModes.data(),
        static_cast<int>(presentModes.size()));

    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width <= 0 || height <= 0) {
        width  = 1;
        height = 1;
    }

    ImGui_ImplVulkanH_CreateOrResizeWindow(
        m_instance,
        m_physicalDevice,
        m_device,
        &m_mainWindowData,
        m_queueFamily,
        nullptr,
        width,
        height,
        m_minImageCount,
        0);

    return true;
}

auto VulkanRenderer::RecreateSourcePreviewImage(Dims2D dims) noexcept -> bool {
    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_B8G8R8A8_UNORM,
        .extent                = VkExtent3D{dims.width, dims.height, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_sourcePreviewImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for source preview: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_sourcePreviewImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for source preview image");
        DestroySourcePreviewImage();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_sourcePreviewMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for source preview image: {}", VulkanResultToString(result));
        DestroySourcePreviewImage();
        return false;
    }

    vkBindImageMemory(m_device, m_sourcePreviewImage, m_sourcePreviewMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_sourcePreviewImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_B8G8R8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_sourcePreviewImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for source preview: {}", VulkanResultToString(result));
        DestroySourcePreviewImage();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sourcePreviewSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for source preview: {}", VulkanResultToString(result));
        DestroySourcePreviewImage();
        return false;
    }

    if (!WaitForPreviewUpload()) {
        DestroySourcePreviewImage();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroySourcePreviewImage();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_sourcePreviewImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroySourcePreviewImage();
        return false;
    }

    m_sourcePreviewDescriptor = ImGui_ImplVulkan_AddTexture(m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_sourcePreviewDims       = dims;
    m_sourcePreviewSequence   = 0u;
    return true;
}

auto VulkanRenderer::EnsureFalseColorResources() noexcept -> bool {
    if (!m_falseColorPipelineInitialized && !CreateFalseColorPipeline()) {
        return false;
    }

    if (m_falseColorImage != VK_NULL_HANDLE && m_falseColorDims == m_sourcePreviewDims && m_falseColorImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyFalseColorResources();

    if (!CreateBuffer(
            256u * sizeof(glm::u8vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_falseColorMapBuffer,
            m_falseColorMapMemory)) {
        return false;
    }

    auto result = vkMapMemory(m_device, m_falseColorMapMemory, 0, 256u * sizeof(glm::u8vec4), 0, &m_falseColorMapMapped);
    if (result != VK_SUCCESS) {
        std::println("vkMapMemory failed for false color map: {}", VulkanResultToString(result));
        DestroyFalseColorResources();
        return false;
    }
    m_falseColorMapDirty = true;

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{m_sourcePreviewDims.width, m_sourcePreviewDims.height, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    result = vkCreateImage(m_device, &imageInfo, nullptr, &m_falseColorImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for false color: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_falseColorImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for false color image");
        DestroyFalseColorResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_falseColorMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for false color image: {}", VulkanResultToString(result));
        DestroyFalseColorResources();
        return false;
    }

    vkBindImageMemory(m_device, m_falseColorImage, m_falseColorMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_falseColorImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_falseColorImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for false color: {}", VulkanResultToString(result));
        DestroyFalseColorResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_falseColorSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for false color: {}", VulkanResultToString(result));
        DestroyFalseColorResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyFalseColorResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_falseColorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyFalseColorResources();
        return false;
    }

    m_falseColorImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_falseColorSampler, m_falseColorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_falseColorDims            = m_sourcePreviewDims;
    return UpdateFalseColorDescriptors();
}

auto VulkanRenderer::CreateFalseColorPipeline() noexcept -> bool {
    if (m_falseColorPipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{
        VkDescriptorSetLayoutBinding{
            .binding            = 0u,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1u,
            .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        VkDescriptorSetLayoutBinding{
            .binding            = 1u,
            .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount    = 1u,
            .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        VkDescriptorSetLayoutBinding{
            .binding            = 2u,
            .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount    = 1u,
            .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data(),
    };

    auto result = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_falseColorDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for false color: {}", VulkanResultToString(result));
        return false;
    }

    VkPushConstantRange pushConstants{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0u,
        .size       = sizeof(uint32_t) * 4u,
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_falseColorDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &pushConstants,
    };

    result = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_falseColorPipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for false color: {}", VulkanResultToString(result));
        DestroyFalseColorPipeline();
        return false;
    }

    const auto shaderModule = LoadShaderModule("shaders/false_color.comp.spv");
    if (shaderModule == VK_NULL_HANDLE) {
        DestroyFalseColorPipeline();
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
        .module              = shaderModule,
        .pName               = "main",
        .pSpecializationInfo = nullptr,
    };

    VkComputePipelineCreateInfo pipelineInfo{
        .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .stage              = stageInfo,
        .layout             = m_falseColorPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };

    result = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &m_falseColorPipeline);
    vkDestroyShaderModule(m_device, shaderModule, nullptr);
    if (result != VK_SUCCESS) {
        std::println("vkCreateComputePipelines failed for false color: {}", VulkanResultToString(result));
        DestroyFalseColorPipeline();
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = m_descriptorPool,
        .descriptorSetCount = 1u,
        .pSetLayouts        = &m_falseColorDescriptorSetLayout,
    };

    result = vkAllocateDescriptorSets(m_device, &allocInfo, &m_falseColorDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for false color: {}", VulkanResultToString(result));
        DestroyFalseColorPipeline();
        return false;
    }

    m_falseColorPipelineInitialized = true;
    return true;
}

auto VulkanRenderer::LoadShaderModule(const std::filesystem::path& path) noexcept -> VkShaderModule {
    auto shaderData = LoadFile(ResolveRuntimePath(path));
    if (!shaderData) {
        std::println("Failed to load Vulkan shader '{}': {}", path.string(), shaderData.error());
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = shaderData->size(),
        .pCode    = reinterpret_cast<const uint32_t*>(shaderData->data()),
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    const auto result = vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        std::println("vkCreateShaderModule failed for '{}': {}", path.string(), VulkanResultToString(result));
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

auto VulkanRenderer::UpdateFalseColorDescriptors() noexcept -> bool {
    if (m_falseColorDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_falseColorImageView == VK_NULL_HANDLE || m_falseColorMapBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{
        .sampler     = m_sourcePreviewSampler,
        .imageView   = m_sourcePreviewImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkDescriptorImageInfo outputInfo{
        .sampler     = VK_NULL_HANDLE,
        .imageView   = m_falseColorImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkDescriptorBufferInfo mapInfo{
        .buffer = m_falseColorMapBuffer,
        .offset = 0u,
        .range  = 256u * sizeof(glm::u8vec4),
    };

    std::array<VkWriteDescriptorSet, 3> writes{
        VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = m_falseColorDescriptorSet,
            .dstBinding       = 0u,
            .dstArrayElement  = 0u,
            .descriptorCount  = 1u,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &sourceInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = m_falseColorDescriptorSet,
            .dstBinding       = 1u,
            .dstArrayElement  = 0u,
            .descriptorCount  = 1u,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo       = &outputInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        },
        VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = m_falseColorDescriptorSet,
            .dstBinding       = 2u,
            .dstArrayElement  = 0u,
            .descriptorCount  = 1u,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &mapInfo,
            .pTexelBufferView = nullptr,
        },
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::UpdateFalseColorMapBuffer(SourceYUVRange yuvRange) noexcept -> bool {
    if (!m_falseColorMapDirty) {
        return true;
    }
    if (m_falseColorMapMapped == nullptr) {
        return false;
    }

    const auto& mapData = (yuvRange == SourceYUVRange::Full)
        ? m_falseColorMap.GetDataFullRange()
        : m_falseColorMap.GetDataLimitedRange();
    std::memcpy(m_falseColorMapMapped, mapData.data(), 256u * sizeof(glm::u8vec4));
    m_falseColorUploadedRange = yuvRange;
    m_falseColorMapDirty      = false;
    return true;
}

auto VulkanRenderer::EnsureLumaWaveformResources() noexcept -> bool {
    if (!m_lumaWaveformPipelineInitialized && !CreateLumaWaveformPipelines()) {
        return false;
    }
    if (m_lumaWaveformImage != VK_NULL_HANDLE && m_lumaWaveformImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyLumaWaveformResources();

    constexpr VkDeviceSize histogramBytes = 580u * 256u * sizeof(uint32_t);
    if (!CreateBuffer(
            histogramBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_lumaWaveformHistogramBuffer,
            m_lumaWaveformHistogramMemory)) {
        return false;
    }

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{580u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_lumaWaveformImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for luma waveform: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_lumaWaveformImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for luma waveform image");
        DestroyLumaWaveformResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_lumaWaveformMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for luma waveform image: {}", VulkanResultToString(result));
        DestroyLumaWaveformResources();
        return false;
    }

    vkBindImageMemory(m_device, m_lumaWaveformImage, m_lumaWaveformMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_lumaWaveformImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_lumaWaveformImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for luma waveform: {}", VulkanResultToString(result));
        DestroyLumaWaveformResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_lumaWaveformSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for luma waveform: {}", VulkanResultToString(result));
        DestroyLumaWaveformResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyLumaWaveformResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_lumaWaveformImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyLumaWaveformResources();
        return false;
    }

    m_lumaWaveformImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_lumaWaveformSampler, m_lumaWaveformImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateLumaWaveformDescriptors();
}

auto VulkanRenderer::CreateLumaWaveformPipelines() noexcept -> bool {
    if (m_lumaWaveformPipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> accumulateBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo accumulateLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(accumulateBindings.size()),
        .pBindings    = accumulateBindings.data(),
    };
    auto result = vkCreateDescriptorSetLayout(m_device, &accumulateLayoutInfo, nullptr, &m_lumaWaveformAccumulateDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for luma waveform accumulate: {}", VulkanResultToString(result));
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> imageBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo imageLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(imageBindings.size()),
        .pBindings    = imageBindings.data(),
    };
    result = vkCreateDescriptorSetLayout(m_device, &imageLayoutInfo, nullptr, &m_lumaWaveformImageDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for luma waveform image: {}", VulkanResultToString(result));
        DestroyLumaWaveformPipelines();
        return false;
    }

    VkPushConstantRange accumulatePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(int32_t) * 2u};
    VkPipelineLayoutCreateInfo accumulatePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_lumaWaveformAccumulateDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &accumulatePush,
    };
    result = vkCreatePipelineLayout(m_device, &accumulatePipelineLayoutInfo, nullptr, &m_lumaWaveformAccumulatePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for luma waveform accumulate: {}", VulkanResultToString(result));
        DestroyLumaWaveformPipelines();
        return false;
    }

    VkPushConstantRange imagePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo imagePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_lumaWaveformImageDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &imagePush,
    };
    result = vkCreatePipelineLayout(m_device, &imagePipelineLayoutInfo, nullptr, &m_lumaWaveformImagePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for luma waveform image: {}", VulkanResultToString(result));
        DestroyLumaWaveformPipelines();
        return false;
    }

    auto createComputePipeline = [&](const std::filesystem::path& shaderPath, VkPipelineLayout layout, VkPipeline& pipeline) -> bool {
        const auto shaderModule = LoadShaderModule(shaderPath);
        if (shaderModule == VK_NULL_HANDLE) {
            return false;
        }
        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, layout, VK_NULL_HANDLE, -1};
        const auto pipelineResult = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            std::println("vkCreateComputePipelines failed for '{}': {}", shaderPath.string(), VulkanResultToString(pipelineResult));
            return false;
        }
        return true;
    };

    if (!createComputePipeline("shaders/waveform_luma_accumulate.comp.spv", m_lumaWaveformAccumulatePipelineLayout, m_lumaWaveformAccumulatePipeline) ||
        !createComputePipeline("shaders/waveform_luma_image.comp.spv", m_lumaWaveformImagePipelineLayout, m_lumaWaveformImagePipeline)) {
        DestroyLumaWaveformPipelines();
        return false;
    }

    VkDescriptorSetAllocateInfo accumulateAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_lumaWaveformAccumulateDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &accumulateAllocInfo, &m_lumaWaveformAccumulateDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for luma waveform accumulate: {}", VulkanResultToString(result));
        DestroyLumaWaveformPipelines();
        return false;
    }
    VkDescriptorSetAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_lumaWaveformImageDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &imageAllocInfo, &m_lumaWaveformImageDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for luma waveform image: {}", VulkanResultToString(result));
        DestroyLumaWaveformPipelines();
        return false;
    }

    m_lumaWaveformPipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateLumaWaveformDescriptors() noexcept -> bool {
    if (m_lumaWaveformAccumulateDescriptorSet == VK_NULL_HANDLE || m_lumaWaveformImageDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_lumaWaveformImageView == VK_NULL_HANDLE || m_lumaWaveformHistogramBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo histogramInfo{m_lumaWaveformHistogramBuffer, 0u, 580u * 256u * sizeof(uint32_t)};
    VkDescriptorImageInfo waveformInfo{VK_NULL_HANDLE, m_lumaWaveformImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lumaWaveformAccumulateDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sourceInfo, nullptr, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lumaWaveformAccumulateDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lumaWaveformImageDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lumaWaveformImageDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &waveformInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureRgbWaveformResources() noexcept -> bool {
    if (!m_rgbWaveformPipelineInitialized && !CreateRgbWaveformPipelines()) {
        return false;
    }
    if (m_rgbWaveformImage != VK_NULL_HANDLE && m_rgbWaveformImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyRgbWaveformResources();

    constexpr VkDeviceSize histogramBytes = 580u * 256u * 3u * sizeof(uint32_t);
    if (!CreateBuffer(
            histogramBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_rgbWaveformHistogramBuffer,
            m_rgbWaveformHistogramMemory)) {
        return false;
    }

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{580u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_rgbWaveformImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for RGB waveform: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_rgbWaveformImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for RGB waveform image");
        DestroyRgbWaveformResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_rgbWaveformMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for RGB waveform image: {}", VulkanResultToString(result));
        DestroyRgbWaveformResources();
        return false;
    }

    vkBindImageMemory(m_device, m_rgbWaveformImage, m_rgbWaveformMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_rgbWaveformImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_rgbWaveformImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for RGB waveform: {}", VulkanResultToString(result));
        DestroyRgbWaveformResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_rgbWaveformSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for RGB waveform: {}", VulkanResultToString(result));
        DestroyRgbWaveformResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyRgbWaveformResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_rgbWaveformImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyRgbWaveformResources();
        return false;
    }

    m_rgbWaveformImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_rgbWaveformSampler, m_rgbWaveformImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateRgbWaveformDescriptors();
}

auto VulkanRenderer::CreateRgbWaveformPipelines() noexcept -> bool {
    if (m_rgbWaveformPipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> accumulateBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo accumulateLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(accumulateBindings.size()),
        .pBindings    = accumulateBindings.data(),
    };
    auto result = vkCreateDescriptorSetLayout(m_device, &accumulateLayoutInfo, nullptr, &m_rgbWaveformAccumulateDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for RGB waveform accumulate: {}", VulkanResultToString(result));
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> imageBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo imageLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(imageBindings.size()),
        .pBindings    = imageBindings.data(),
    };
    result = vkCreateDescriptorSetLayout(m_device, &imageLayoutInfo, nullptr, &m_rgbWaveformImageDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for RGB waveform image: {}", VulkanResultToString(result));
        DestroyRgbWaveformPipelines();
        return false;
    }

    VkPushConstantRange accumulatePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(int32_t)};
    VkPipelineLayoutCreateInfo accumulatePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_rgbWaveformAccumulateDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &accumulatePush,
    };
    result = vkCreatePipelineLayout(m_device, &accumulatePipelineLayoutInfo, nullptr, &m_rgbWaveformAccumulatePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for RGB waveform accumulate: {}", VulkanResultToString(result));
        DestroyRgbWaveformPipelines();
        return false;
    }

    VkPushConstantRange imagePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo imagePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_rgbWaveformImageDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &imagePush,
    };
    result = vkCreatePipelineLayout(m_device, &imagePipelineLayoutInfo, nullptr, &m_rgbWaveformImagePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for RGB waveform image: {}", VulkanResultToString(result));
        DestroyRgbWaveformPipelines();
        return false;
    }

    auto createComputePipeline = [&](const std::filesystem::path& shaderPath, VkPipelineLayout layout, VkPipeline& pipeline) -> bool {
        const auto shaderModule = LoadShaderModule(shaderPath);
        if (shaderModule == VK_NULL_HANDLE) {
            return false;
        }
        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, layout, VK_NULL_HANDLE, -1};
        const auto pipelineResult = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            std::println("vkCreateComputePipelines failed for '{}': {}", shaderPath.string(), VulkanResultToString(pipelineResult));
            return false;
        }
        return true;
    };

    if (!createComputePipeline("shaders/waveform_rgb_accumulate.comp.spv", m_rgbWaveformAccumulatePipelineLayout, m_rgbWaveformAccumulatePipeline) ||
        !createComputePipeline("shaders/waveform_rgb_image.comp.spv", m_rgbWaveformImagePipelineLayout, m_rgbWaveformImagePipeline)) {
        DestroyRgbWaveformPipelines();
        return false;
    }

    VkDescriptorSetAllocateInfo accumulateAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_rgbWaveformAccumulateDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &accumulateAllocInfo, &m_rgbWaveformAccumulateDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for RGB waveform accumulate: {}", VulkanResultToString(result));
        DestroyRgbWaveformPipelines();
        return false;
    }
    VkDescriptorSetAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_rgbWaveformImageDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &imageAllocInfo, &m_rgbWaveformImageDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for RGB waveform image: {}", VulkanResultToString(result));
        DestroyRgbWaveformPipelines();
        return false;
    }

    m_rgbWaveformPipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateRgbWaveformDescriptors() noexcept -> bool {
    if (m_rgbWaveformAccumulateDescriptorSet == VK_NULL_HANDLE || m_rgbWaveformImageDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_rgbWaveformImageView == VK_NULL_HANDLE || m_rgbWaveformHistogramBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo histogramInfo{m_rgbWaveformHistogramBuffer, 0u, 580u * 256u * 3u * sizeof(uint32_t)};
    VkDescriptorImageInfo waveformInfo{VK_NULL_HANDLE, m_rgbWaveformImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbWaveformAccumulateDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sourceInfo, nullptr, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbWaveformAccumulateDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbWaveformImageDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbWaveformImageDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &waveformInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureRgbParadeResources() noexcept -> bool {
    if (!m_rgbParadePipelineInitialized && !CreateRgbParadePipeline()) {
        return false;
    }
    if (m_rgbParadeImage != VK_NULL_HANDLE && m_rgbParadeImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyRgbParadeResources();

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{580u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_rgbParadeImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for RGB parade: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_rgbParadeImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for RGB parade image");
        DestroyRgbParadeResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_rgbParadeMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for RGB parade image: {}", VulkanResultToString(result));
        DestroyRgbParadeResources();
        return false;
    }

    vkBindImageMemory(m_device, m_rgbParadeImage, m_rgbParadeMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_rgbParadeImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_rgbParadeImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for RGB parade: {}", VulkanResultToString(result));
        DestroyRgbParadeResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_rgbParadeSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for RGB parade: {}", VulkanResultToString(result));
        DestroyRgbParadeResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyRgbParadeResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_rgbParadeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyRgbParadeResources();
        return false;
    }

    m_rgbParadeImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_rgbParadeSampler, m_rgbParadeImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateRgbParadeDescriptors();
}

auto VulkanRenderer::CreateRgbParadePipeline() noexcept -> bool {
    if (m_rgbParadePipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data(),
    };
    auto result = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_rgbParadeDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for RGB parade: {}", VulkanResultToString(result));
        return false;
    }

    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_rgbParadeDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &push,
    };
    result = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_rgbParadePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for RGB parade: {}", VulkanResultToString(result));
        DestroyRgbParadePipeline();
        return false;
    }

    const auto shaderModule = LoadShaderModule("shaders/waveform_rgb_parade_image.comp.spv");
    if (shaderModule == VK_NULL_HANDLE) {
        DestroyRgbParadePipeline();
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, m_rgbParadePipelineLayout, VK_NULL_HANDLE, -1};
    result = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &m_rgbParadePipeline);
    vkDestroyShaderModule(m_device, shaderModule, nullptr);
    if (result != VK_SUCCESS) {
        std::println("vkCreateComputePipelines failed for RGB parade: {}", VulkanResultToString(result));
        DestroyRgbParadePipeline();
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_rgbParadeDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &allocInfo, &m_rgbParadeDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for RGB parade: {}", VulkanResultToString(result));
        DestroyRgbParadePipeline();
        return false;
    }

    m_rgbParadePipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateRgbParadeDescriptors() noexcept -> bool {
    if (m_rgbParadeDescriptorSet == VK_NULL_HANDLE || m_rgbWaveformHistogramBuffer == VK_NULL_HANDLE || m_rgbParadeImageView == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorBufferInfo histogramInfo{m_rgbWaveformHistogramBuffer, 0u, 580u * 256u * 3u * sizeof(uint32_t)};
    VkDescriptorImageInfo paradeInfo{VK_NULL_HANDLE, m_rgbParadeImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 2> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbParadeDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbParadeDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &paradeInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureRgbBlacklevelResources() noexcept -> bool {
    if (!m_rgbBlacklevelPipelineInitialized && !CreateRgbBlacklevelPipeline()) {
        return false;
    }
    if (m_rgbBlacklevelImage != VK_NULL_HANDLE && m_rgbBlacklevelImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyRgbBlacklevelResources();

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{580u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_rgbBlacklevelImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for RGB blacklevel: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_rgbBlacklevelImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for RGB blacklevel image");
        DestroyRgbBlacklevelResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_rgbBlacklevelMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for RGB blacklevel image: {}", VulkanResultToString(result));
        DestroyRgbBlacklevelResources();
        return false;
    }

    vkBindImageMemory(m_device, m_rgbBlacklevelImage, m_rgbBlacklevelMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_rgbBlacklevelImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_rgbBlacklevelImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for RGB blacklevel: {}", VulkanResultToString(result));
        DestroyRgbBlacklevelResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_rgbBlacklevelSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for RGB blacklevel: {}", VulkanResultToString(result));
        DestroyRgbBlacklevelResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyRgbBlacklevelResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_rgbBlacklevelImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyRgbBlacklevelResources();
        return false;
    }

    m_rgbBlacklevelImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_rgbBlacklevelSampler, m_rgbBlacklevelImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateRgbBlacklevelDescriptors();
}

auto VulkanRenderer::CreateRgbBlacklevelPipeline() noexcept -> bool {
    if (m_rgbBlacklevelPipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data(),
    };
    auto result = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_rgbBlacklevelDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for RGB blacklevel: {}", VulkanResultToString(result));
        return false;
    }

    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_rgbBlacklevelDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &push,
    };
    result = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_rgbBlacklevelPipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for RGB blacklevel: {}", VulkanResultToString(result));
        DestroyRgbBlacklevelPipeline();
        return false;
    }

    const auto shaderModule = LoadShaderModule("shaders/waveform_rgb_blacklevel_image.comp.spv");
    if (shaderModule == VK_NULL_HANDLE) {
        DestroyRgbBlacklevelPipeline();
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, m_rgbBlacklevelPipelineLayout, VK_NULL_HANDLE, -1};
    result = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &m_rgbBlacklevelPipeline);
    vkDestroyShaderModule(m_device, shaderModule, nullptr);
    if (result != VK_SUCCESS) {
        std::println("vkCreateComputePipelines failed for RGB blacklevel: {}", VulkanResultToString(result));
        DestroyRgbBlacklevelPipeline();
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_rgbBlacklevelDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &allocInfo, &m_rgbBlacklevelDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for RGB blacklevel: {}", VulkanResultToString(result));
        DestroyRgbBlacklevelPipeline();
        return false;
    }

    m_rgbBlacklevelPipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateRgbBlacklevelDescriptors() noexcept -> bool {
    if (m_rgbBlacklevelDescriptorSet == VK_NULL_HANDLE || m_rgbWaveformHistogramBuffer == VK_NULL_HANDLE || m_rgbBlacklevelImageView == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorBufferInfo histogramInfo{m_rgbWaveformHistogramBuffer, 0u, 580u * 256u * 3u * sizeof(uint32_t)};
    VkDescriptorImageInfo blacklevelInfo{VK_NULL_HANDLE, m_rgbBlacklevelImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 2> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbBlacklevelDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_rgbBlacklevelDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &blacklevelInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureYuvParadeResources() noexcept -> bool {
    if (!m_yuvParadePipelineInitialized && !CreateYuvParadePipelines()) {
        return false;
    }
    if (m_yuvParadeImage != VK_NULL_HANDLE && m_yuvParadeImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyYuvParadeResources();

    constexpr VkDeviceSize histogramBytes = 580u * 256u * 3u * sizeof(uint32_t);
    if (!CreateBuffer(
            histogramBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_yuvWaveformHistogramBuffer,
            m_yuvWaveformHistogramMemory)) {
        return false;
    }

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{580u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_yuvParadeImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for YUV parade: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_yuvParadeImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for YUV parade image");
        DestroyYuvParadeResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_yuvParadeMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for YUV parade image: {}", VulkanResultToString(result));
        DestroyYuvParadeResources();
        return false;
    }

    vkBindImageMemory(m_device, m_yuvParadeImage, m_yuvParadeMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_yuvParadeImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_yuvParadeImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for YUV parade: {}", VulkanResultToString(result));
        DestroyYuvParadeResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_yuvParadeSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for YUV parade: {}", VulkanResultToString(result));
        DestroyYuvParadeResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyYuvParadeResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_yuvParadeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyYuvParadeResources();
        return false;
    }

    m_yuvParadeImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_yuvParadeSampler, m_yuvParadeImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateYuvParadeDescriptors();
}

auto VulkanRenderer::CreateYuvParadePipelines() noexcept -> bool {
    if (m_yuvParadePipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> accumulateBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo accumulateLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(accumulateBindings.size()),
        .pBindings    = accumulateBindings.data(),
    };
    auto result = vkCreateDescriptorSetLayout(m_device, &accumulateLayoutInfo, nullptr, &m_yuvWaveformAccumulateDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for YUV waveform accumulate: {}", VulkanResultToString(result));
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> paradeBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo paradeLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(paradeBindings.size()),
        .pBindings    = paradeBindings.data(),
    };
    result = vkCreateDescriptorSetLayout(m_device, &paradeLayoutInfo, nullptr, &m_yuvParadeDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for YUV parade: {}", VulkanResultToString(result));
        DestroyYuvParadePipelines();
        return false;
    }

    VkPushConstantRange accumulatePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(int32_t) * 2u};
    VkPipelineLayoutCreateInfo accumulatePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_yuvWaveformAccumulateDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &accumulatePush,
    };
    result = vkCreatePipelineLayout(m_device, &accumulatePipelineLayoutInfo, nullptr, &m_yuvWaveformAccumulatePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for YUV waveform accumulate: {}", VulkanResultToString(result));
        DestroyYuvParadePipelines();
        return false;
    }

    VkPushConstantRange paradePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo paradePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_yuvParadeDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &paradePush,
    };
    result = vkCreatePipelineLayout(m_device, &paradePipelineLayoutInfo, nullptr, &m_yuvParadePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for YUV parade: {}", VulkanResultToString(result));
        DestroyYuvParadePipelines();
        return false;
    }

    auto createComputePipeline = [&](const std::filesystem::path& shaderPath, VkPipelineLayout layout, VkPipeline& pipeline) -> bool {
        const auto shaderModule = LoadShaderModule(shaderPath);
        if (shaderModule == VK_NULL_HANDLE) {
            return false;
        }
        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, layout, VK_NULL_HANDLE, -1};
        const auto pipelineResult = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            std::println("vkCreateComputePipelines failed for '{}': {}", shaderPath.string(), VulkanResultToString(pipelineResult));
            return false;
        }
        return true;
    };

    if (!createComputePipeline("shaders/waveform_yuv_accumulate.comp.spv", m_yuvWaveformAccumulatePipelineLayout, m_yuvWaveformAccumulatePipeline) ||
        !createComputePipeline("shaders/waveform_yuv_parade_image.comp.spv", m_yuvParadePipelineLayout, m_yuvParadePipeline)) {
        DestroyYuvParadePipelines();
        return false;
    }

    VkDescriptorSetAllocateInfo accumulateAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_yuvWaveformAccumulateDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &accumulateAllocInfo, &m_yuvWaveformAccumulateDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for YUV waveform accumulate: {}", VulkanResultToString(result));
        DestroyYuvParadePipelines();
        return false;
    }
    VkDescriptorSetAllocateInfo paradeAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_yuvParadeDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &paradeAllocInfo, &m_yuvParadeDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for YUV parade: {}", VulkanResultToString(result));
        DestroyYuvParadePipelines();
        return false;
    }

    m_yuvParadePipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateYuvParadeDescriptors() noexcept -> bool {
    if (m_yuvWaveformAccumulateDescriptorSet == VK_NULL_HANDLE || m_yuvParadeDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_yuvParadeImageView == VK_NULL_HANDLE || m_yuvWaveformHistogramBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo histogramInfo{m_yuvWaveformHistogramBuffer, 0u, 580u * 256u * 3u * sizeof(uint32_t)};
    VkDescriptorImageInfo paradeInfo{VK_NULL_HANDLE, m_yuvParadeImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_yuvWaveformAccumulateDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sourceInfo, nullptr, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_yuvWaveformAccumulateDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_yuvParadeDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_yuvParadeDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &paradeInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureUvScopeResources() noexcept -> bool {
    if (!m_uvScopePipelineInitialized && !CreateUvScopePipelines()) {
        return false;
    }
    if (m_uvScopeImage != VK_NULL_HANDLE && m_uvScopeImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyUvScopeResources();

    constexpr VkDeviceSize histogramBytes = 256u * 256u * sizeof(uint32_t);
    if (!CreateBuffer(
            histogramBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_uvScopeHistogramBuffer,
            m_uvScopeHistogramMemory)) {
        return false;
    }

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{256u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_uvScopeImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for UV vectorscope: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_uvScopeImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for UV vectorscope image");
        DestroyUvScopeResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_uvScopeMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for UV vectorscope image: {}", VulkanResultToString(result));
        DestroyUvScopeResources();
        return false;
    }

    vkBindImageMemory(m_device, m_uvScopeImage, m_uvScopeMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_uvScopeImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0u,
            .levelCount     = 1u,
            .baseArrayLayer = 0u,
            .layerCount     = 1u,
        },
    };

    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_uvScopeImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for UV vectorscope: {}", VulkanResultToString(result));
        DestroyUvScopeResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_uvScopeSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for UV vectorscope: {}", VulkanResultToString(result));
        DestroyUvScopeResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyUvScopeResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_uvScopeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyUvScopeResources();
        return false;
    }

    m_uvScopeImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_uvScopeSampler, m_uvScopeImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateUvScopeDescriptors();
}

auto VulkanRenderer::CreateUvScopePipelines() noexcept -> bool {
    if (m_uvScopePipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> accumulateBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo accumulateLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(accumulateBindings.size()),
        .pBindings    = accumulateBindings.data(),
    };
    auto result = vkCreateDescriptorSetLayout(m_device, &accumulateLayoutInfo, nullptr, &m_uvScopeAccumulateDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for UV vectorscope accumulate: {}", VulkanResultToString(result));
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> imageBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo imageLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(imageBindings.size()),
        .pBindings    = imageBindings.data(),
    };
    result = vkCreateDescriptorSetLayout(m_device, &imageLayoutInfo, nullptr, &m_uvScopeImageDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for UV vectorscope image: {}", VulkanResultToString(result));
        DestroyUvScopePipelines();
        return false;
    }

    VkPushConstantRange accumulatePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(int32_t) * 2u};
    VkPipelineLayoutCreateInfo accumulatePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_uvScopeAccumulateDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &accumulatePush,
    };
    result = vkCreatePipelineLayout(m_device, &accumulatePipelineLayoutInfo, nullptr, &m_uvScopeAccumulatePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for UV vectorscope accumulate: {}", VulkanResultToString(result));
        DestroyUvScopePipelines();
        return false;
    }

    VkPushConstantRange imagePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo imagePipelineLayoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1u,
        .pSetLayouts            = &m_uvScopeImageDescriptorSetLayout,
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges    = &imagePush,
    };
    result = vkCreatePipelineLayout(m_device, &imagePipelineLayoutInfo, nullptr, &m_uvScopeImagePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for UV vectorscope image: {}", VulkanResultToString(result));
        DestroyUvScopePipelines();
        return false;
    }

    auto createComputePipeline = [&](const std::filesystem::path& shaderPath, VkPipelineLayout layout, VkPipeline& pipeline) -> bool {
        const auto shaderModule = LoadShaderModule(shaderPath);
        if (shaderModule == VK_NULL_HANDLE) {
            return false;
        }
        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, layout, VK_NULL_HANDLE, -1};
        const auto pipelineResult = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            std::println("vkCreateComputePipelines failed for '{}': {}", shaderPath.string(), VulkanResultToString(pipelineResult));
            return false;
        }
        return true;
    };

    if (!createComputePipeline("shaders/scope_uv_accumulate.comp.spv", m_uvScopeAccumulatePipelineLayout, m_uvScopeAccumulatePipeline) ||
        !createComputePipeline("shaders/scope_uv_image.comp.spv", m_uvScopeImagePipelineLayout, m_uvScopeImagePipeline)) {
        DestroyUvScopePipelines();
        return false;
    }

    VkDescriptorSetAllocateInfo accumulateAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_uvScopeAccumulateDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &accumulateAllocInfo, &m_uvScopeAccumulateDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for UV vectorscope accumulate: {}", VulkanResultToString(result));
        DestroyUvScopePipelines();
        return false;
    }
    VkDescriptorSetAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_uvScopeImageDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &imageAllocInfo, &m_uvScopeImageDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for UV vectorscope image: {}", VulkanResultToString(result));
        DestroyUvScopePipelines();
        return false;
    }

    m_uvScopePipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateUvScopeDescriptors() noexcept -> bool {
    if (m_uvScopeAccumulateDescriptorSet == VK_NULL_HANDLE || m_uvScopeImageDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_uvScopeImageView == VK_NULL_HANDLE || m_uvScopeHistogramBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo histogramInfo{m_uvScopeHistogramBuffer, 0u, 256u * 256u * sizeof(uint32_t)};
    VkDescriptorImageInfo scopeInfo{VK_NULL_HANDLE, m_uvScopeImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_uvScopeAccumulateDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sourceInfo, nullptr, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_uvScopeAccumulateDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_uvScopeImageDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_uvScopeImageDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &scopeInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureXyzScopeResources() noexcept -> bool {
    if (!m_xyzScopePipelineInitialized && !CreateXyzScopePipelines()) {
        return false;
    }
    if (m_xyzScopeImage != VK_NULL_HANDLE && m_xyzScopeImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyXyzScopeResources();

    constexpr VkDeviceSize histogramBytes = 256u * 256u * sizeof(uint32_t);
    if (!CreateBuffer(histogramBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_xyzScopeHistogramBuffer, m_xyzScopeHistogramMemory)) {
        return false;
    }

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{256u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_xyzScopeImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for CIE scope: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_xyzScopeImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for CIE scope image");
        DestroyXyzScopeResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memoryRequirements.size, *memoryType};
    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_xyzScopeMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for CIE scope image: {}", VulkanResultToString(result));
        DestroyXyzScopeResources();
        return false;
    }
    vkBindImageMemory(m_device, m_xyzScopeImage, m_xyzScopeMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_xyzScopeImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u},
    };
    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_xyzScopeImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for CIE scope: {}", VulkanResultToString(result));
        DestroyXyzScopeResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_xyzScopeSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for CIE scope: {}", VulkanResultToString(result));
        DestroyXyzScopeResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyXyzScopeResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_xyzScopeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyXyzScopeResources();
        return false;
    }

    m_xyzScopeImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_xyzScopeSampler, m_xyzScopeImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateXyzScopeDescriptors();
}

auto VulkanRenderer::CreateXyzScopePipelines() noexcept -> bool {
    if (m_xyzScopePipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> accumulateBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo accumulateLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(accumulateBindings.size()), accumulateBindings.data()};
    auto result = vkCreateDescriptorSetLayout(m_device, &accumulateLayoutInfo, nullptr, &m_xyzScopeAccumulateDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for CIE scope accumulate: {}", VulkanResultToString(result));
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> imageBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo imageLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(imageBindings.size()), imageBindings.data()};
    result = vkCreateDescriptorSetLayout(m_device, &imageLayoutInfo, nullptr, &m_xyzScopeImageDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for CIE scope image: {}", VulkanResultToString(result));
        DestroyXyzScopePipelines();
        return false;
    }

    VkPushConstantRange accumulatePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(int32_t)};
    VkPipelineLayoutCreateInfo accumulatePipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1u, &m_xyzScopeAccumulateDescriptorSetLayout, 1u, &accumulatePush};
    result = vkCreatePipelineLayout(m_device, &accumulatePipelineLayoutInfo, nullptr, &m_xyzScopeAccumulatePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for CIE scope accumulate: {}", VulkanResultToString(result));
        DestroyXyzScopePipelines();
        return false;
    }

    VkPushConstantRange imagePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo imagePipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1u, &m_xyzScopeImageDescriptorSetLayout, 1u, &imagePush};
    result = vkCreatePipelineLayout(m_device, &imagePipelineLayoutInfo, nullptr, &m_xyzScopeImagePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for CIE scope image: {}", VulkanResultToString(result));
        DestroyXyzScopePipelines();
        return false;
    }

    auto createComputePipeline = [&](const std::filesystem::path& shaderPath, VkPipelineLayout layout, VkPipeline& pipeline) -> bool {
        const auto shaderModule = LoadShaderModule(shaderPath);
        if (shaderModule == VK_NULL_HANDLE) {
            return false;
        }
        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, layout, VK_NULL_HANDLE, -1};
        const auto pipelineResult = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            std::println("vkCreateComputePipelines failed for '{}': {}", shaderPath.string(), VulkanResultToString(pipelineResult));
            return false;
        }
        return true;
    };

    if (!createComputePipeline("shaders/scope_xyz_accumulate.comp.spv", m_xyzScopeAccumulatePipelineLayout, m_xyzScopeAccumulatePipeline) ||
        !createComputePipeline("shaders/scope_xyz_image.comp.spv", m_xyzScopeImagePipelineLayout, m_xyzScopeImagePipeline)) {
        DestroyXyzScopePipelines();
        return false;
    }

    VkDescriptorSetAllocateInfo accumulateAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_xyzScopeAccumulateDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &accumulateAllocInfo, &m_xyzScopeAccumulateDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for CIE scope accumulate: {}", VulkanResultToString(result));
        DestroyXyzScopePipelines();
        return false;
    }
    VkDescriptorSetAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_xyzScopeImageDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &imageAllocInfo, &m_xyzScopeImageDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for CIE scope image: {}", VulkanResultToString(result));
        DestroyXyzScopePipelines();
        return false;
    }

    m_xyzScopePipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateXyzScopeDescriptors() noexcept -> bool {
    if (m_xyzScopeAccumulateDescriptorSet == VK_NULL_HANDLE || m_xyzScopeImageDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_xyzScopeImageView == VK_NULL_HANDLE || m_xyzScopeHistogramBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo histogramInfo{m_xyzScopeHistogramBuffer, 0u, 256u * 256u * sizeof(uint32_t)};
    VkDescriptorImageInfo scopeInfo{VK_NULL_HANDLE, m_xyzScopeImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_xyzScopeAccumulateDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sourceInfo, nullptr, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_xyzScopeAccumulateDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_xyzScopeImageDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_xyzScopeImageDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &scopeInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::EnsureDiamondScopeResources() noexcept -> bool {
    if (!m_diamondScopePipelineInitialized && !CreateDiamondScopePipelines()) {
        return false;
    }
    if (m_diamondScopeImage != VK_NULL_HANDLE && m_diamondScopeImGuiDescriptor != VK_NULL_HANDLE) {
        return true;
    }

    WaitIdle();
    DestroyDiamondScopeResources();

    constexpr VkDeviceSize histogramBytes = 256u * 256u * sizeof(uint32_t);
    if (!CreateBuffer(histogramBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_diamondScopeHistogramBuffer, m_diamondScopeHistogramMemory)) {
        return false;
    }

    VkImageCreateInfo imageInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{256u, 256u, 1u},
        .mipLevels             = 1u,
        .arrayLayers           = 1u,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    auto result = vkCreateImage(m_device, &imageInfo, nullptr, &m_diamondScopeImage);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImage failed for double diamond scope: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_diamondScopeImage, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memoryType) {
        std::println("No suitable device-local memory type for double diamond scope image");
        DestroyDiamondScopeResources();
        return false;
    }

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memoryRequirements.size, *memoryType};
    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_diamondScopeMemory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for double diamond scope image: {}", VulkanResultToString(result));
        DestroyDiamondScopeResources();
        return false;
    }
    vkBindImageMemory(m_device, m_diamondScopeImage, m_diamondScopeMemory, 0);

    VkImageViewCreateInfo imageViewInfo{
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .image    = m_diamondScopeImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u},
    };
    result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_diamondScopeImageView);
    if (result != VK_SUCCESS) {
        std::println("vkCreateImageView failed for double diamond scope: {}", VulkanResultToString(result));
        DestroyDiamondScopeResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_diamondScopeSampler);
    if (result != VK_SUCCESS) {
        std::println("vkCreateSampler failed for double diamond scope: {}", VulkanResultToString(result));
        DestroyDiamondScopeResources();
        return false;
    }

    auto commandBuffer = BeginUploadCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        DestroyDiamondScopeResources();
        return false;
    }
    TransitionImageLayout(commandBuffer, m_diamondScopeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndUploadCommands(commandBuffer, true)) {
        DestroyDiamondScopeResources();
        return false;
    }

    m_diamondScopeImGuiDescriptor = ImGui_ImplVulkan_AddTexture(m_diamondScopeSampler, m_diamondScopeImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return UpdateDiamondScopeDescriptors();
}

auto VulkanRenderer::CreateDiamondScopePipelines() noexcept -> bool {
    if (m_diamondScopePipelineInitialized) {
        return true;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> accumulateBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo accumulateLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(accumulateBindings.size()), accumulateBindings.data()};
    auto result = vkCreateDescriptorSetLayout(m_device, &accumulateLayoutInfo, nullptr, &m_diamondScopeAccumulateDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for double diamond scope accumulate: {}", VulkanResultToString(result));
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> imageBindings{
        VkDescriptorSetLayoutBinding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo imageLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(imageBindings.size()), imageBindings.data()};
    result = vkCreateDescriptorSetLayout(m_device, &imageLayoutInfo, nullptr, &m_diamondScopeImageDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreateDescriptorSetLayout failed for double diamond scope image: {}", VulkanResultToString(result));
        DestroyDiamondScopePipelines();
        return false;
    }

    VkPushConstantRange accumulatePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(int32_t)};
    VkPipelineLayoutCreateInfo accumulatePipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1u, &m_diamondScopeAccumulateDescriptorSetLayout, 1u, &accumulatePush};
    result = vkCreatePipelineLayout(m_device, &accumulatePipelineLayoutInfo, nullptr, &m_diamondScopeAccumulatePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for double diamond scope accumulate: {}", VulkanResultToString(result));
        DestroyDiamondScopePipelines();
        return false;
    }

    VkPushConstantRange imagePush{VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(uint32_t) * 2u + sizeof(float)};
    VkPipelineLayoutCreateInfo imagePipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1u, &m_diamondScopeImageDescriptorSetLayout, 1u, &imagePush};
    result = vkCreatePipelineLayout(m_device, &imagePipelineLayoutInfo, nullptr, &m_diamondScopeImagePipelineLayout);
    if (result != VK_SUCCESS) {
        std::println("vkCreatePipelineLayout failed for double diamond scope image: {}", VulkanResultToString(result));
        DestroyDiamondScopePipelines();
        return false;
    }

    auto createComputePipeline = [&](const std::filesystem::path& shaderPath, VkPipelineLayout layout, VkPipeline& pipeline) -> bool {
        const auto shaderModule = LoadShaderModule(shaderPath);
        if (shaderModule == VK_NULL_HANDLE) {
            return false;
        }
        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr};
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stageInfo, layout, VK_NULL_HANDLE, -1};
        const auto pipelineResult = vkCreateComputePipelines(m_device, m_pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            std::println("vkCreateComputePipelines failed for '{}': {}", shaderPath.string(), VulkanResultToString(pipelineResult));
            return false;
        }
        return true;
    };

    if (!createComputePipeline("shaders/scope_diamond_accumulate.comp.spv", m_diamondScopeAccumulatePipelineLayout, m_diamondScopeAccumulatePipeline) ||
        !createComputePipeline("shaders/scope_diamond_image.comp.spv", m_diamondScopeImagePipelineLayout, m_diamondScopeImagePipeline)) {
        DestroyDiamondScopePipelines();
        return false;
    }

    VkDescriptorSetAllocateInfo accumulateAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_diamondScopeAccumulateDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &accumulateAllocInfo, &m_diamondScopeAccumulateDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for double diamond scope accumulate: {}", VulkanResultToString(result));
        DestroyDiamondScopePipelines();
        return false;
    }
    VkDescriptorSetAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_descriptorPool, 1u, &m_diamondScopeImageDescriptorSetLayout};
    result = vkAllocateDescriptorSets(m_device, &imageAllocInfo, &m_diamondScopeImageDescriptorSet);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateDescriptorSets failed for double diamond scope image: {}", VulkanResultToString(result));
        DestroyDiamondScopePipelines();
        return false;
    }

    m_diamondScopePipelineInitialized = true;
    return true;
}

auto VulkanRenderer::UpdateDiamondScopeDescriptors() noexcept -> bool {
    if (m_diamondScopeAccumulateDescriptorSet == VK_NULL_HANDLE || m_diamondScopeImageDescriptorSet == VK_NULL_HANDLE || m_sourcePreviewImageView == VK_NULL_HANDLE || m_diamondScopeImageView == VK_NULL_HANDLE || m_diamondScopeHistogramBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo sourceInfo{m_sourcePreviewSampler, m_sourcePreviewImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo histogramInfo{m_diamondScopeHistogramBuffer, 0u, 256u * 256u * sizeof(uint32_t)};
    VkDescriptorImageInfo scopeInfo{VK_NULL_HANDLE, m_diamondScopeImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_diamondScopeAccumulateDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sourceInfo, nullptr, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_diamondScopeAccumulateDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_diamondScopeImageDescriptorSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &histogramInfo, nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_diamondScopeImageDescriptorSet, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &scopeInfo, nullptr, nullptr},
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    return true;
}

auto VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) noexcept -> bool {
    VkBufferCreateInfo bufferInfo{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = size,
        .usage                 = usage,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices   = nullptr,
    };

    auto result = vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        std::println("vkCreateBuffer failed: {}", VulkanResultToString(result));
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);
    const auto memoryType = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
    if (!memoryType) {
        std::println("No suitable memory type for Vulkan buffer");
        vkDestroyBuffer(m_device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memoryRequirements.size,
        .memoryTypeIndex = *memoryType,
    };

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        std::println("vkAllocateMemory failed for buffer: {}", VulkanResultToString(result));
        vkDestroyBuffer(m_device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

auto VulkanRenderer::EnsureSourcePreviewStagingBuffer(VkDeviceSize size) noexcept -> bool {
    if (m_sourcePreviewStagingBuffer != VK_NULL_HANDLE && m_sourcePreviewStagingCapacity >= size && m_sourcePreviewStagingMapped != nullptr) {
        return true;
    }

    DestroySourcePreviewStagingBuffer();

    if (!CreateBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_sourcePreviewStagingBuffer,
            m_sourcePreviewStagingMemory)) {
        return false;
    }

    auto result = vkMapMemory(m_device, m_sourcePreviewStagingMemory, 0, size, 0, &m_sourcePreviewStagingMapped);
    if (result != VK_SUCCESS) {
        std::println("vkMapMemory failed for source preview staging buffer: {}", VulkanResultToString(result));
        DestroySourcePreviewStagingBuffer();
        return false;
    }

    m_sourcePreviewStagingCapacity = size;
    std::println("Allocated Vulkan source preview staging buffer: {} bytes", size);
    return true;
}

auto VulkanRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept -> std::optional<uint32_t> {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

    for (uint32_t i = 0u; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0u && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return std::nullopt;
}

auto VulkanRenderer::BeginUploadCommands() noexcept -> VkCommandBuffer {
    if (m_uploadCommandBuffer == VK_NULL_HANDLE || !WaitForPreviewUpload()) {
        return VK_NULL_HANDLE;
    }

    auto result = vkResetCommandBuffer(m_uploadCommandBuffer, 0);
    if (result != VK_SUCCESS) {
        std::println("vkResetCommandBuffer failed for preview upload: {}", VulkanResultToString(result));
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    result = vkBeginCommandBuffer(m_uploadCommandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        std::println("vkBeginCommandBuffer failed for preview upload: {}", VulkanResultToString(result));
        return VK_NULL_HANDLE;
    }

    return m_uploadCommandBuffer;
}

auto VulkanRenderer::EndUploadCommands(VkCommandBuffer commandBuffer, bool waitForCompletion) noexcept -> bool {
    auto result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        std::println("vkEndCommandBuffer failed for preview upload: {}", VulkanResultToString(result));
        return false;
    }

    VkSubmitInfo submitInfo{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 0u,
        .pWaitSemaphores      = nullptr,
        .pWaitDstStageMask    = nullptr,
        .commandBufferCount   = 1u,
        .pCommandBuffers      = &commandBuffer,
        .signalSemaphoreCount = 0u,
        .pSignalSemaphores    = nullptr,
    };

    result = vkResetFences(m_device, 1u, &m_uploadFence);
    if (result != VK_SUCCESS) {
        std::println("vkResetFences failed for preview upload: {}", VulkanResultToString(result));
        return false;
    }

    result = vkQueueSubmit(m_queue, 1u, &submitInfo, m_uploadFence);
    if (result != VK_SUCCESS) {
        std::println("vkQueueSubmit failed for preview upload: {}", VulkanResultToString(result));
        VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        vkDestroyFence(m_device, m_uploadFence, nullptr);
        m_uploadFence = VK_NULL_HANDLE;
        vkCreateFence(m_device, &fenceInfo, nullptr, &m_uploadFence);
        m_uploadSubmitted = false;
        return false;
    }

    m_uploadSubmitted = true;

    if (waitForCompletion) {
        return WaitForPreviewUpload();
    }

    return true;
}

auto VulkanRenderer::WaitForPreviewUpload() noexcept -> bool {
    if (m_uploadFence == VK_NULL_HANDLE) {
        return false;
    }

    auto result = vkWaitForFences(m_device, 1u, &m_uploadFence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        std::println("vkWaitForFences failed for preview upload: {}", VulkanResultToString(result));
        return false;
    }

    m_uploadSubmitted = false;
    return true;
}

void VulkanRenderer::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = 0,
        .dstAccessMask       = 0,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0u,
               .levelCount     = 1u,
               .baseArrayLayer = 0u,
               .layerCount     = 1u,
        },
    };

    VkPipelineStageFlags sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        destinationStage      = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        &barrier);
}

void VulkanRenderer::CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, Dims2D dims) noexcept {
    VkBufferImageCopy region{
        .bufferOffset      = 0u,
        .bufferRowLength   = 0u,
        .bufferImageHeight = 0u,
        .imageSubresource  = VkImageSubresourceLayers{
             .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
             .mipLevel       = 0u,
             .baseArrayLayer = 0u,
             .layerCount     = 1u,
        },
        .imageOffset = VkOffset3D{0, 0, 0},
        .imageExtent = VkExtent3D{dims.width, dims.height, 1u},
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
}

void VulkanRenderer::DestroySourcePreviewImage() noexcept {
    if (m_sourcePreviewDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_sourcePreviewDescriptor);
        }
        m_sourcePreviewDescriptor = VK_NULL_HANDLE;
    }
    if (m_sourcePreviewSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sourcePreviewSampler, nullptr);
        m_sourcePreviewSampler = VK_NULL_HANDLE;
    }
    if (m_sourcePreviewImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_sourcePreviewImageView, nullptr);
        m_sourcePreviewImageView = VK_NULL_HANDLE;
    }
    if (m_sourcePreviewImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_sourcePreviewImage, nullptr);
        m_sourcePreviewImage = VK_NULL_HANDLE;
    }
    if (m_sourcePreviewMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_sourcePreviewMemory, nullptr);
        m_sourcePreviewMemory = VK_NULL_HANDLE;
    }

    m_sourcePreviewDims     = Dims2D{0u, 0u};
    m_sourcePreviewSequence = 0u;
}

void VulkanRenderer::DestroySourcePreviewStagingBuffer() noexcept {
    if (m_sourcePreviewStagingBuffer != VK_NULL_HANDLE || m_sourcePreviewStagingMemory != VK_NULL_HANDLE) {
        [[maybe_unused]] const auto uploadComplete = WaitForPreviewUpload();
    }

    if (m_sourcePreviewStagingMapped != nullptr) {
        vkUnmapMemory(m_device, m_sourcePreviewStagingMemory);
        m_sourcePreviewStagingMapped = nullptr;
    }
    if (m_sourcePreviewStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_sourcePreviewStagingBuffer, nullptr);
        m_sourcePreviewStagingBuffer = VK_NULL_HANDLE;
    }
    if (m_sourcePreviewStagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_sourcePreviewStagingMemory, nullptr);
        m_sourcePreviewStagingMemory = VK_NULL_HANDLE;
    }

    m_sourcePreviewStagingCapacity = 0u;
}

void VulkanRenderer::DestroyFalseColorResources() noexcept {
    if (m_falseColorImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_falseColorImGuiDescriptor);
        }
        m_falseColorImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_falseColorMapMapped != nullptr) {
        vkUnmapMemory(m_device, m_falseColorMapMemory);
        m_falseColorMapMapped = nullptr;
    }
    if (m_falseColorMapBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_falseColorMapBuffer, nullptr);
        m_falseColorMapBuffer = VK_NULL_HANDLE;
    }
    if (m_falseColorMapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_falseColorMapMemory, nullptr);
        m_falseColorMapMemory = VK_NULL_HANDLE;
    }
    if (m_falseColorSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_falseColorSampler, nullptr);
        m_falseColorSampler = VK_NULL_HANDLE;
    }
    if (m_falseColorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_falseColorImageView, nullptr);
        m_falseColorImageView = VK_NULL_HANDLE;
    }
    if (m_falseColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_falseColorImage, nullptr);
        m_falseColorImage = VK_NULL_HANDLE;
    }
    if (m_falseColorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_falseColorMemory, nullptr);
        m_falseColorMemory = VK_NULL_HANDLE;
    }

    m_falseColorDims     = Dims2D{0u, 0u};
    m_falseColorMapDirty = true;
    m_falseColorUploadedRange = SourceYUVRange::max;
    m_falseColorRenderedSourceSequence = 0u;
    m_falseColorRenderedColorSpace     = SourceColorSpace::max;
    m_falseColorRenderedYuvRange       = SourceYUVRange::max;
}

void VulkanRenderer::DestroyFalseColorPipeline() noexcept {
    if (m_falseColorDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_falseColorDescriptorSet);
        m_falseColorDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_falseColorPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_falseColorPipeline, nullptr);
        m_falseColorPipeline = VK_NULL_HANDLE;
    }
    if (m_falseColorPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_falseColorPipelineLayout, nullptr);
        m_falseColorPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_falseColorDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_falseColorDescriptorSetLayout, nullptr);
        m_falseColorDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_falseColorPipelineInitialized = false;
}

void VulkanRenderer::DestroyLumaWaveformResources() noexcept {
    if (m_lumaWaveformImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_lumaWaveformImGuiDescriptor);
        }
        m_lumaWaveformImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_lumaWaveformSampler, nullptr);
        m_lumaWaveformSampler = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_lumaWaveformImageView, nullptr);
        m_lumaWaveformImageView = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_lumaWaveformImage, nullptr);
        m_lumaWaveformImage = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_lumaWaveformMemory, nullptr);
        m_lumaWaveformMemory = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformHistogramBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_lumaWaveformHistogramBuffer, nullptr);
        m_lumaWaveformHistogramBuffer = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformHistogramMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_lumaWaveformHistogramMemory, nullptr);
        m_lumaWaveformHistogramMemory = VK_NULL_HANDLE;
    }

    m_lumaWaveformRenderedSourceSequence = 0u;
    m_lumaWaveformRenderedColorSpace     = SourceColorSpace::max;
    m_lumaWaveformRenderedYuvRange       = SourceYUVRange::max;
}

void VulkanRenderer::DestroyLumaWaveformPipelines() noexcept {
    if (m_lumaWaveformAccumulateDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_lumaWaveformAccumulateDescriptorSet);
        m_lumaWaveformAccumulateDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformImageDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_lumaWaveformImageDescriptorSet);
        m_lumaWaveformImageDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformAccumulatePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_lumaWaveformAccumulatePipeline, nullptr);
        m_lumaWaveformAccumulatePipeline = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformImagePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_lumaWaveformImagePipeline, nullptr);
        m_lumaWaveformImagePipeline = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformAccumulatePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_lumaWaveformAccumulatePipelineLayout, nullptr);
        m_lumaWaveformAccumulatePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformImagePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_lumaWaveformImagePipelineLayout, nullptr);
        m_lumaWaveformImagePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformAccumulateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_lumaWaveformAccumulateDescriptorSetLayout, nullptr);
        m_lumaWaveformAccumulateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_lumaWaveformImageDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_lumaWaveformImageDescriptorSetLayout, nullptr);
        m_lumaWaveformImageDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_lumaWaveformPipelineInitialized = false;
}

void VulkanRenderer::DestroyRgbWaveformResources() noexcept {
    if (m_rgbWaveformImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_rgbWaveformImGuiDescriptor);
        }
        m_rgbWaveformImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_rgbWaveformSampler, nullptr);
        m_rgbWaveformSampler = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_rgbWaveformImageView, nullptr);
        m_rgbWaveformImageView = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_rgbWaveformImage, nullptr);
        m_rgbWaveformImage = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_rgbWaveformMemory, nullptr);
        m_rgbWaveformMemory = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformHistogramBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_rgbWaveformHistogramBuffer, nullptr);
        m_rgbWaveformHistogramBuffer = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformHistogramMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_rgbWaveformHistogramMemory, nullptr);
        m_rgbWaveformHistogramMemory = VK_NULL_HANDLE;
    }

    m_rgbWaveformRenderedSourceSequence = 0u;
    m_rgbWaveformRenderedColorSpace     = SourceColorSpace::max;
}

void VulkanRenderer::DestroyRgbWaveformPipelines() noexcept {
    if (m_rgbWaveformAccumulateDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_rgbWaveformAccumulateDescriptorSet);
        m_rgbWaveformAccumulateDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformImageDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_rgbWaveformImageDescriptorSet);
        m_rgbWaveformImageDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformAccumulatePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_rgbWaveformAccumulatePipeline, nullptr);
        m_rgbWaveformAccumulatePipeline = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformImagePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_rgbWaveformImagePipeline, nullptr);
        m_rgbWaveformImagePipeline = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformAccumulatePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_rgbWaveformAccumulatePipelineLayout, nullptr);
        m_rgbWaveformAccumulatePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformImagePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_rgbWaveformImagePipelineLayout, nullptr);
        m_rgbWaveformImagePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformAccumulateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_rgbWaveformAccumulateDescriptorSetLayout, nullptr);
        m_rgbWaveformAccumulateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_rgbWaveformImageDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_rgbWaveformImageDescriptorSetLayout, nullptr);
        m_rgbWaveformImageDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_rgbWaveformPipelineInitialized = false;
}

void VulkanRenderer::DestroyRgbParadeResources() noexcept {
    if (m_rgbParadeImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_rgbParadeImGuiDescriptor);
        }
        m_rgbParadeImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_rgbParadeSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_rgbParadeSampler, nullptr);
        m_rgbParadeSampler = VK_NULL_HANDLE;
    }
    if (m_rgbParadeImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_rgbParadeImageView, nullptr);
        m_rgbParadeImageView = VK_NULL_HANDLE;
    }
    if (m_rgbParadeImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_rgbParadeImage, nullptr);
        m_rgbParadeImage = VK_NULL_HANDLE;
    }
    if (m_rgbParadeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_rgbParadeMemory, nullptr);
        m_rgbParadeMemory = VK_NULL_HANDLE;
    }

    m_rgbParadeRenderedSourceSequence = 0u;
    m_rgbParadeRenderedColorSpace     = SourceColorSpace::max;
}

void VulkanRenderer::DestroyRgbParadePipeline() noexcept {
    if (m_rgbParadeDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_rgbParadeDescriptorSet);
        m_rgbParadeDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_rgbParadePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_rgbParadePipeline, nullptr);
        m_rgbParadePipeline = VK_NULL_HANDLE;
    }
    if (m_rgbParadePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_rgbParadePipelineLayout, nullptr);
        m_rgbParadePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_rgbParadeDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_rgbParadeDescriptorSetLayout, nullptr);
        m_rgbParadeDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_rgbParadePipelineInitialized = false;
}

void VulkanRenderer::DestroyRgbBlacklevelResources() noexcept {
    if (m_rgbBlacklevelImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_rgbBlacklevelImGuiDescriptor);
        }
        m_rgbBlacklevelImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_rgbBlacklevelSampler, nullptr);
        m_rgbBlacklevelSampler = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_rgbBlacklevelImageView, nullptr);
        m_rgbBlacklevelImageView = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_rgbBlacklevelImage, nullptr);
        m_rgbBlacklevelImage = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_rgbBlacklevelMemory, nullptr);
        m_rgbBlacklevelMemory = VK_NULL_HANDLE;
    }

    m_rgbBlacklevelRenderedSourceSequence = 0u;
    m_rgbBlacklevelRenderedColorSpace     = SourceColorSpace::max;
}

void VulkanRenderer::DestroyRgbBlacklevelPipeline() noexcept {
    if (m_rgbBlacklevelDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_rgbBlacklevelDescriptorSet);
        m_rgbBlacklevelDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_rgbBlacklevelPipeline, nullptr);
        m_rgbBlacklevelPipeline = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_rgbBlacklevelPipelineLayout, nullptr);
        m_rgbBlacklevelPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_rgbBlacklevelDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_rgbBlacklevelDescriptorSetLayout, nullptr);
        m_rgbBlacklevelDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_rgbBlacklevelPipelineInitialized = false;
}

void VulkanRenderer::DestroyYuvParadeResources() noexcept {
    if (m_yuvParadeImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_yuvParadeImGuiDescriptor);
        }
        m_yuvParadeImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_yuvParadeSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_yuvParadeSampler, nullptr);
        m_yuvParadeSampler = VK_NULL_HANDLE;
    }
    if (m_yuvParadeImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_yuvParadeImageView, nullptr);
        m_yuvParadeImageView = VK_NULL_HANDLE;
    }
    if (m_yuvParadeImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_yuvParadeImage, nullptr);
        m_yuvParadeImage = VK_NULL_HANDLE;
    }
    if (m_yuvParadeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_yuvParadeMemory, nullptr);
        m_yuvParadeMemory = VK_NULL_HANDLE;
    }
    if (m_yuvWaveformHistogramBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_yuvWaveformHistogramBuffer, nullptr);
        m_yuvWaveformHistogramBuffer = VK_NULL_HANDLE;
    }
    if (m_yuvWaveformHistogramMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_yuvWaveformHistogramMemory, nullptr);
        m_yuvWaveformHistogramMemory = VK_NULL_HANDLE;
    }

    m_yuvParadeRenderedSourceSequence = 0u;
    m_yuvParadeRenderedColorSpace     = SourceColorSpace::max;
    m_yuvParadeRenderedYuvRange       = SourceYUVRange::max;
}

void VulkanRenderer::DestroyYuvParadePipelines() noexcept {
    if (m_yuvWaveformAccumulateDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_yuvWaveformAccumulateDescriptorSet);
        m_yuvWaveformAccumulateDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_yuvParadeDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_yuvParadeDescriptorSet);
        m_yuvParadeDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_yuvWaveformAccumulatePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_yuvWaveformAccumulatePipeline, nullptr);
        m_yuvWaveformAccumulatePipeline = VK_NULL_HANDLE;
    }
    if (m_yuvParadePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_yuvParadePipeline, nullptr);
        m_yuvParadePipeline = VK_NULL_HANDLE;
    }
    if (m_yuvWaveformAccumulatePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_yuvWaveformAccumulatePipelineLayout, nullptr);
        m_yuvWaveformAccumulatePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_yuvParadePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_yuvParadePipelineLayout, nullptr);
        m_yuvParadePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_yuvWaveformAccumulateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_yuvWaveformAccumulateDescriptorSetLayout, nullptr);
        m_yuvWaveformAccumulateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_yuvParadeDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_yuvParadeDescriptorSetLayout, nullptr);
        m_yuvParadeDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_yuvParadePipelineInitialized = false;
}

void VulkanRenderer::DestroyUvScopeResources() noexcept {
    if (m_uvScopeImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_uvScopeImGuiDescriptor);
        }
        m_uvScopeImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_uvScopeSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_uvScopeSampler, nullptr);
        m_uvScopeSampler = VK_NULL_HANDLE;
    }
    if (m_uvScopeImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_uvScopeImageView, nullptr);
        m_uvScopeImageView = VK_NULL_HANDLE;
    }
    if (m_uvScopeImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_uvScopeImage, nullptr);
        m_uvScopeImage = VK_NULL_HANDLE;
    }
    if (m_uvScopeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_uvScopeMemory, nullptr);
        m_uvScopeMemory = VK_NULL_HANDLE;
    }
    if (m_uvScopeHistogramBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_uvScopeHistogramBuffer, nullptr);
        m_uvScopeHistogramBuffer = VK_NULL_HANDLE;
    }
    if (m_uvScopeHistogramMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_uvScopeHistogramMemory, nullptr);
        m_uvScopeHistogramMemory = VK_NULL_HANDLE;
    }

    m_uvScopeRenderedSourceSequence = 0u;
    m_uvScopeRenderedColorSpace     = SourceColorSpace::max;
    m_uvScopeRenderedYuvRange       = SourceYUVRange::max;
}

void VulkanRenderer::DestroyUvScopePipelines() noexcept {
    if (m_uvScopeAccumulateDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_uvScopeAccumulateDescriptorSet);
        m_uvScopeAccumulateDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_uvScopeImageDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_uvScopeImageDescriptorSet);
        m_uvScopeImageDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_uvScopeAccumulatePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_uvScopeAccumulatePipeline, nullptr);
        m_uvScopeAccumulatePipeline = VK_NULL_HANDLE;
    }
    if (m_uvScopeImagePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_uvScopeImagePipeline, nullptr);
        m_uvScopeImagePipeline = VK_NULL_HANDLE;
    }
    if (m_uvScopeAccumulatePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_uvScopeAccumulatePipelineLayout, nullptr);
        m_uvScopeAccumulatePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_uvScopeImagePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_uvScopeImagePipelineLayout, nullptr);
        m_uvScopeImagePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_uvScopeAccumulateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_uvScopeAccumulateDescriptorSetLayout, nullptr);
        m_uvScopeAccumulateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_uvScopeImageDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_uvScopeImageDescriptorSetLayout, nullptr);
        m_uvScopeImageDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_uvScopePipelineInitialized = false;
}

void VulkanRenderer::DestroyXyzScopeResources() noexcept {
    if (m_xyzScopeImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_xyzScopeImGuiDescriptor);
        }
        m_xyzScopeImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_xyzScopeSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_xyzScopeSampler, nullptr);
        m_xyzScopeSampler = VK_NULL_HANDLE;
    }
    if (m_xyzScopeImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_xyzScopeImageView, nullptr);
        m_xyzScopeImageView = VK_NULL_HANDLE;
    }
    if (m_xyzScopeImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_xyzScopeImage, nullptr);
        m_xyzScopeImage = VK_NULL_HANDLE;
    }
    if (m_xyzScopeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_xyzScopeMemory, nullptr);
        m_xyzScopeMemory = VK_NULL_HANDLE;
    }
    if (m_xyzScopeHistogramBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_xyzScopeHistogramBuffer, nullptr);
        m_xyzScopeHistogramBuffer = VK_NULL_HANDLE;
    }
    if (m_xyzScopeHistogramMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_xyzScopeHistogramMemory, nullptr);
        m_xyzScopeHistogramMemory = VK_NULL_HANDLE;
    }

    m_xyzScopeRenderedSourceSequence = 0u;
    m_xyzScopeRenderedColorSpace     = SourceColorSpace::max;
}

void VulkanRenderer::DestroyXyzScopePipelines() noexcept {
    if (m_xyzScopeAccumulateDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_xyzScopeAccumulateDescriptorSet);
        m_xyzScopeAccumulateDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_xyzScopeImageDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_xyzScopeImageDescriptorSet);
        m_xyzScopeImageDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_xyzScopeAccumulatePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_xyzScopeAccumulatePipeline, nullptr);
        m_xyzScopeAccumulatePipeline = VK_NULL_HANDLE;
    }
    if (m_xyzScopeImagePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_xyzScopeImagePipeline, nullptr);
        m_xyzScopeImagePipeline = VK_NULL_HANDLE;
    }
    if (m_xyzScopeAccumulatePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_xyzScopeAccumulatePipelineLayout, nullptr);
        m_xyzScopeAccumulatePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_xyzScopeImagePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_xyzScopeImagePipelineLayout, nullptr);
        m_xyzScopeImagePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_xyzScopeAccumulateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_xyzScopeAccumulateDescriptorSetLayout, nullptr);
        m_xyzScopeAccumulateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_xyzScopeImageDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_xyzScopeImageDescriptorSetLayout, nullptr);
        m_xyzScopeImageDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_xyzScopePipelineInitialized = false;
}

void VulkanRenderer::DestroyDiamondScopeResources() noexcept {
    if (m_diamondScopeImGuiDescriptor != VK_NULL_HANDLE) {
        if (m_imguiBackendInitialized) {
            ImGui_ImplVulkan_RemoveTexture(m_diamondScopeImGuiDescriptor);
        }
        m_diamondScopeImGuiDescriptor = VK_NULL_HANDLE;
    }
    if (m_diamondScopeSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_diamondScopeSampler, nullptr);
        m_diamondScopeSampler = VK_NULL_HANDLE;
    }
    if (m_diamondScopeImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_diamondScopeImageView, nullptr);
        m_diamondScopeImageView = VK_NULL_HANDLE;
    }
    if (m_diamondScopeImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_diamondScopeImage, nullptr);
        m_diamondScopeImage = VK_NULL_HANDLE;
    }
    if (m_diamondScopeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_diamondScopeMemory, nullptr);
        m_diamondScopeMemory = VK_NULL_HANDLE;
    }
    if (m_diamondScopeHistogramBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_diamondScopeHistogramBuffer, nullptr);
        m_diamondScopeHistogramBuffer = VK_NULL_HANDLE;
    }
    if (m_diamondScopeHistogramMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_diamondScopeHistogramMemory, nullptr);
        m_diamondScopeHistogramMemory = VK_NULL_HANDLE;
    }

    m_diamondScopeRenderedSourceSequence = 0u;
    m_diamondScopeRenderedColorSpace     = SourceColorSpace::max;
}

void VulkanRenderer::DestroyDiamondScopePipelines() noexcept {
    if (m_diamondScopeAccumulateDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_diamondScopeAccumulateDescriptorSet);
        m_diamondScopeAccumulateDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_diamondScopeImageDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1u, &m_diamondScopeImageDescriptorSet);
        m_diamondScopeImageDescriptorSet = VK_NULL_HANDLE;
    }
    if (m_diamondScopeAccumulatePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_diamondScopeAccumulatePipeline, nullptr);
        m_diamondScopeAccumulatePipeline = VK_NULL_HANDLE;
    }
    if (m_diamondScopeImagePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_diamondScopeImagePipeline, nullptr);
        m_diamondScopeImagePipeline = VK_NULL_HANDLE;
    }
    if (m_diamondScopeAccumulatePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_diamondScopeAccumulatePipelineLayout, nullptr);
        m_diamondScopeAccumulatePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_diamondScopeImagePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_diamondScopeImagePipelineLayout, nullptr);
        m_diamondScopeImagePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_diamondScopeAccumulateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_diamondScopeAccumulateDescriptorSetLayout, nullptr);
        m_diamondScopeAccumulateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_diamondScopeImageDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_diamondScopeImageDescriptorSetLayout, nullptr);
        m_diamondScopeImageDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_diamondScopePipelineInitialized = false;
}

void VulkanRenderer::RebuildSwapchainIfNeeded() noexcept {
    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);

    if (width <= 0 || height <= 0) {
        return;
    }

    if (!m_swapChainRebuild && m_mainWindowData.Width == width && m_mainWindowData.Height == height) {
        return;
    }

    WaitIdle();
    ImGui_ImplVulkan_SetMinImageCount(m_minImageCount);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        m_instance,
        m_physicalDevice,
        m_device,
        &m_mainWindowData,
        m_queueFamily,
        nullptr,
        width,
        height,
        m_minImageCount,
        0);
    m_mainWindowData.FrameIndex = 0u;
    m_swapChainRebuild          = false;
}

void VulkanRenderer::Shutdown() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        WaitIdle();
    }

    DestroyFalseColorResources();
    DestroyLumaWaveformResources();
    DestroyRgbWaveformResources();
    DestroyRgbParadeResources();
    DestroyRgbBlacklevelResources();
    DestroyYuvParadeResources();
    DestroyUvScopeResources();
    DestroyXyzScopeResources();
    DestroyDiamondScopeResources();
    DestroySourcePreviewImage();
    DestroySourcePreviewStagingBuffer();

    ShutdownImGuiBackend();

    if (m_mainWindowData.Surface != VK_NULL_HANDLE) {
        ImGui_ImplVulkanH_DestroyWindow(m_instance, m_device, &m_mainWindowData, nullptr);
        vkDestroySurfaceKHR(m_instance, m_mainWindowData.Surface, nullptr);
        m_mainWindowData.Surface = VK_NULL_HANDLE;
    }

    DestroyFalseColorPipeline();
    DestroyLumaWaveformPipelines();
    DestroyRgbWaveformPipelines();
    DestroyRgbParadePipeline();
    DestroyRgbBlacklevelPipeline();
    DestroyYuvParadePipelines();
    DestroyUvScopePipelines();
    DestroyXyzScopePipelines();
    DestroyDiamondScopePipelines();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_uploadFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_uploadFence, nullptr);
        m_uploadFence = VK_NULL_HANDLE;
    }

    if (m_uploadCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_uploadCommandPool, nullptr);
        m_uploadCommandPool = VK_NULL_HANDLE;
        m_uploadCommandBuffer = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    m_initialized              = false;
    m_physicalDevice           = VK_NULL_HANDLE;
    m_queue                    = VK_NULL_HANDLE;
    m_queueFamily              = std::numeric_limits<uint32_t>::max();
    m_physicalDeviceProperties = {};
}

auto VulkanRenderer::VulkanResultToString(VkResult result) noexcept -> std::string_view {
    switch (result) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    default:
        return "Unknown VkResult";
    }
}

} // namespace scpp
