#pragma once

#include "pch.hpp"
#include "video_source.hpp"
#include "false_color.hpp"

#include <vulkan/vulkan.h>

namespace scpp {

class VulkanRenderer {
private:
    GLFWwindow* m_window{nullptr};

    VkInstance       m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice         m_device{VK_NULL_HANDLE};
    VkQueue          m_queue{VK_NULL_HANDLE};
    uint32_t         m_queueFamily{std::numeric_limits<uint32_t>::max()};

    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkPipelineCache  m_pipelineCache{VK_NULL_HANDLE};

    VkCommandPool m_uploadCommandPool{VK_NULL_HANDLE};
    VkCommandBuffer m_uploadCommandBuffer{VK_NULL_HANDLE};
    VkFence         m_uploadFence{VK_NULL_HANDLE};
    bool            m_uploadSubmitted{false};

    VkPhysicalDeviceProperties m_physicalDeviceProperties{};
    ImGui_ImplVulkanH_Window   m_mainWindowData{};

    VkImage        m_sourcePreviewImage{VK_NULL_HANDLE};
    VkDeviceMemory m_sourcePreviewMemory{VK_NULL_HANDLE};
    VkImageView    m_sourcePreviewImageView{VK_NULL_HANDLE};
    VkSampler      m_sourcePreviewSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_sourcePreviewDescriptor{VK_NULL_HANDLE};
    Dims2D          m_sourcePreviewDims{0u, 0u};
    uint64_t        m_sourcePreviewSequence{0u};

    VkBuffer       m_sourcePreviewStagingBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_sourcePreviewStagingMemory{VK_NULL_HANDLE};
    void*          m_sourcePreviewStagingMapped{nullptr};
    VkDeviceSize   m_sourcePreviewStagingCapacity{0u};

    VkImage        m_falseColorImage{VK_NULL_HANDLE};
    VkDeviceMemory m_falseColorMemory{VK_NULL_HANDLE};
    VkImageView    m_falseColorImageView{VK_NULL_HANDLE};
    VkSampler      m_falseColorSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_falseColorImGuiDescriptor{VK_NULL_HANDLE};
    Dims2D          m_falseColorDims{0u, 0u};
    VkBuffer       m_falseColorMapBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_falseColorMapMemory{VK_NULL_HANDLE};
    void*          m_falseColorMapMapped{nullptr};
    VkDescriptorSetLayout m_falseColorDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_falseColorDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_falseColorPipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_falseColorPipeline{VK_NULL_HANDLE};
    bool                  m_falseColorPipelineInitialized{false};
    bool                  m_falseColorMapDirty{true};
    SourceYUVRange        m_falseColorUploadedRange{SourceYUVRange::max};
    uint64_t              m_falseColorRenderedSourceSequence{0u};
    SourceColorSpace      m_falseColorRenderedColorSpace{SourceColorSpace::max};
    SourceYUVRange        m_falseColorRenderedYuvRange{SourceYUVRange::max};
    FalseColorMap         m_falseColorMap{c_falseColorMaps[0].second};
    std::string_view      m_selectedFalseColorMapName{c_falseColorMaps[0].first};

    VkImage        m_lumaWaveformImage{VK_NULL_HANDLE};
    VkDeviceMemory m_lumaWaveformMemory{VK_NULL_HANDLE};
    VkImageView    m_lumaWaveformImageView{VK_NULL_HANDLE};
    VkSampler      m_lumaWaveformSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_lumaWaveformImGuiDescriptor{VK_NULL_HANDLE};
    VkBuffer       m_lumaWaveformHistogramBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_lumaWaveformHistogramMemory{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_lumaWaveformAccumulateDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_lumaWaveformImageDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_lumaWaveformAccumulateDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet       m_lumaWaveformImageDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_lumaWaveformAccumulatePipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout      m_lumaWaveformImagePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_lumaWaveformAccumulatePipeline{VK_NULL_HANDLE};
    VkPipeline            m_lumaWaveformImagePipeline{VK_NULL_HANDLE};
    bool                  m_lumaWaveformPipelineInitialized{false};
    uint64_t              m_lumaWaveformRenderedSourceSequence{0u};
    SourceColorSpace      m_lumaWaveformRenderedColorSpace{SourceColorSpace::max};
    SourceYUVRange        m_lumaWaveformRenderedYuvRange{SourceYUVRange::max};

    VkImage        m_rgbWaveformImage{VK_NULL_HANDLE};
    VkDeviceMemory m_rgbWaveformMemory{VK_NULL_HANDLE};
    VkImageView    m_rgbWaveformImageView{VK_NULL_HANDLE};
    VkSampler      m_rgbWaveformSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_rgbWaveformImGuiDescriptor{VK_NULL_HANDLE};
    VkBuffer       m_rgbWaveformHistogramBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_rgbWaveformHistogramMemory{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_rgbWaveformAccumulateDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_rgbWaveformImageDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_rgbWaveformAccumulateDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet       m_rgbWaveformImageDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_rgbWaveformAccumulatePipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout      m_rgbWaveformImagePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_rgbWaveformAccumulatePipeline{VK_NULL_HANDLE};
    VkPipeline            m_rgbWaveformImagePipeline{VK_NULL_HANDLE};
    bool                  m_rgbWaveformPipelineInitialized{false};
    uint64_t              m_rgbWaveformRenderedSourceSequence{0u};
    SourceColorSpace      m_rgbWaveformRenderedColorSpace{SourceColorSpace::max};

    VkImage        m_rgbParadeImage{VK_NULL_HANDLE};
    VkDeviceMemory m_rgbParadeMemory{VK_NULL_HANDLE};
    VkImageView    m_rgbParadeImageView{VK_NULL_HANDLE};
    VkSampler      m_rgbParadeSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_rgbParadeImGuiDescriptor{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_rgbParadeDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_rgbParadeDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_rgbParadePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_rgbParadePipeline{VK_NULL_HANDLE};
    bool                  m_rgbParadePipelineInitialized{false};
    uint64_t              m_rgbParadeRenderedSourceSequence{0u};
    SourceColorSpace      m_rgbParadeRenderedColorSpace{SourceColorSpace::max};

    VkImage        m_rgbBlacklevelImage{VK_NULL_HANDLE};
    VkDeviceMemory m_rgbBlacklevelMemory{VK_NULL_HANDLE};
    VkImageView    m_rgbBlacklevelImageView{VK_NULL_HANDLE};
    VkSampler      m_rgbBlacklevelSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_rgbBlacklevelImGuiDescriptor{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_rgbBlacklevelDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_rgbBlacklevelDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_rgbBlacklevelPipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_rgbBlacklevelPipeline{VK_NULL_HANDLE};
    bool                  m_rgbBlacklevelPipelineInitialized{false};
    uint64_t              m_rgbBlacklevelRenderedSourceSequence{0u};
    SourceColorSpace      m_rgbBlacklevelRenderedColorSpace{SourceColorSpace::max};

    VkImage        m_yuvParadeImage{VK_NULL_HANDLE};
    VkDeviceMemory m_yuvParadeMemory{VK_NULL_HANDLE};
    VkImageView    m_yuvParadeImageView{VK_NULL_HANDLE};
    VkSampler      m_yuvParadeSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_yuvParadeImGuiDescriptor{VK_NULL_HANDLE};
    VkBuffer       m_yuvWaveformHistogramBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_yuvWaveformHistogramMemory{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_yuvWaveformAccumulateDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_yuvParadeDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_yuvWaveformAccumulateDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet       m_yuvParadeDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_yuvWaveformAccumulatePipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout      m_yuvParadePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_yuvWaveformAccumulatePipeline{VK_NULL_HANDLE};
    VkPipeline            m_yuvParadePipeline{VK_NULL_HANDLE};
    bool                  m_yuvParadePipelineInitialized{false};
    uint64_t              m_yuvParadeRenderedSourceSequence{0u};
    SourceColorSpace      m_yuvParadeRenderedColorSpace{SourceColorSpace::max};
    SourceYUVRange        m_yuvParadeRenderedYuvRange{SourceYUVRange::max};

    VkImage        m_uvScopeImage{VK_NULL_HANDLE};
    VkDeviceMemory m_uvScopeMemory{VK_NULL_HANDLE};
    VkImageView    m_uvScopeImageView{VK_NULL_HANDLE};
    VkSampler      m_uvScopeSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_uvScopeImGuiDescriptor{VK_NULL_HANDLE};
    VkBuffer       m_uvScopeHistogramBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_uvScopeHistogramMemory{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_uvScopeAccumulateDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_uvScopeImageDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_uvScopeAccumulateDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet       m_uvScopeImageDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_uvScopeAccumulatePipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout      m_uvScopeImagePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_uvScopeAccumulatePipeline{VK_NULL_HANDLE};
    VkPipeline            m_uvScopeImagePipeline{VK_NULL_HANDLE};
    bool                  m_uvScopePipelineInitialized{false};
    uint64_t              m_uvScopeRenderedSourceSequence{0u};
    SourceColorSpace      m_uvScopeRenderedColorSpace{SourceColorSpace::max};
    SourceYUVRange        m_uvScopeRenderedYuvRange{SourceYUVRange::max};

    VkImage        m_xyzScopeImage{VK_NULL_HANDLE};
    VkDeviceMemory m_xyzScopeMemory{VK_NULL_HANDLE};
    VkImageView    m_xyzScopeImageView{VK_NULL_HANDLE};
    VkSampler      m_xyzScopeSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_xyzScopeImGuiDescriptor{VK_NULL_HANDLE};
    VkBuffer       m_xyzScopeHistogramBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_xyzScopeHistogramMemory{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_xyzScopeAccumulateDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_xyzScopeImageDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_xyzScopeAccumulateDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet       m_xyzScopeImageDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_xyzScopeAccumulatePipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout      m_xyzScopeImagePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_xyzScopeAccumulatePipeline{VK_NULL_HANDLE};
    VkPipeline            m_xyzScopeImagePipeline{VK_NULL_HANDLE};
    bool                  m_xyzScopePipelineInitialized{false};
    uint64_t              m_xyzScopeRenderedSourceSequence{0u};
    SourceColorSpace      m_xyzScopeRenderedColorSpace{SourceColorSpace::max};

    VkImage        m_diamondScopeImage{VK_NULL_HANDLE};
    VkDeviceMemory m_diamondScopeMemory{VK_NULL_HANDLE};
    VkImageView    m_diamondScopeImageView{VK_NULL_HANDLE};
    VkSampler      m_diamondScopeSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_diamondScopeImGuiDescriptor{VK_NULL_HANDLE};
    VkBuffer       m_diamondScopeHistogramBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_diamondScopeHistogramMemory{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_diamondScopeAccumulateDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_diamondScopeImageDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet       m_diamondScopeAccumulateDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet       m_diamondScopeImageDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout      m_diamondScopeAccumulatePipelineLayout{VK_NULL_HANDLE};
    VkPipelineLayout      m_diamondScopeImagePipelineLayout{VK_NULL_HANDLE};
    VkPipeline            m_diamondScopeAccumulatePipeline{VK_NULL_HANDLE};
    VkPipeline            m_diamondScopeImagePipeline{VK_NULL_HANDLE};
    bool                  m_diamondScopePipelineInitialized{false};
    uint64_t              m_diamondScopeRenderedSourceSequence{0u};
    SourceColorSpace      m_diamondScopeRenderedColorSpace{SourceColorSpace::max};

    bool m_initialized{false};
    bool m_imguiBackendInitialized{false};
    bool m_swapChainRebuild{false};

    uint32_t    m_minImageCount{2u};
    std::string m_lastError;

public:
    explicit VulkanRenderer(GLFWwindow* window = nullptr);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&)            = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&)                 = delete;
    VulkanRenderer& operator=(VulkanRenderer&&)      = delete;

    [[nodiscard]]
    auto IsInitialized() const noexcept -> bool { return m_initialized; }

    [[nodiscard]]
    auto GetLastError() const noexcept -> std::string_view { return m_lastError; }

    [[nodiscard]]
    auto GetDeviceName() const noexcept -> std::string_view;

    [[nodiscard]]
    auto InitImGuiBackend() noexcept -> bool;

    void ShutdownImGuiBackend() noexcept;

    void NewFrame() noexcept;

    void RenderFrame(ImVec4 clearColor) noexcept;

    void WaitIdle() noexcept;

    [[nodiscard]]
    auto UploadSourcePreview(SourceFrameView frame) noexcept -> bool;

    [[nodiscard]]
    auto HasSourcePreview() const noexcept -> bool { return m_sourcePreviewDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetSourcePreviewTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_sourcePreviewDescriptor);
    }

    [[nodiscard]]
    auto GetSourcePreviewDims() const noexcept -> Dims2D { return m_sourcePreviewDims; }

    [[nodiscard]]
    auto RenderFalseColor(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasFalseColor() const noexcept -> bool { return m_falseColorImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetFalseColorTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_falseColorImGuiDescriptor);
    }

    [[nodiscard]]
    auto GetSelectedFalseColorMapName() const noexcept -> std::string_view { return m_selectedFalseColorMapName; }

    void SetFalseColorMap(FalseColorMapDataRef map, std::string_view name = ""sv);

    [[nodiscard]]
    auto RenderLumaWaveform(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasLumaWaveform() const noexcept -> bool { return m_lumaWaveformImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetLumaWaveformTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_lumaWaveformImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderRgbWaveform(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasRgbWaveform() const noexcept -> bool { return m_rgbWaveformImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetRgbWaveformTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_rgbWaveformImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderRgbParade(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasRgbParade() const noexcept -> bool { return m_rgbParadeImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetRgbParadeTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_rgbParadeImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderRgbBlacklevel(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasRgbBlacklevel() const noexcept -> bool { return m_rgbBlacklevelImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetRgbBlacklevelTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_rgbBlacklevelImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderYuvParade(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasYuvParade() const noexcept -> bool { return m_yuvParadeImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetYuvParadeTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_yuvParadeImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderUvScope(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasUvScope() const noexcept -> bool { return m_uvScopeImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetUvScopeTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_uvScopeImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderXyzScope(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasXyzScope() const noexcept -> bool { return m_xyzScopeImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetXyzScopeTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_xyzScopeImGuiDescriptor);
    }

    [[nodiscard]]
    auto RenderDiamondScope(RenderSettings renderSettings) noexcept -> bool;

    [[nodiscard]]
    auto HasDiamondScope() const noexcept -> bool { return m_diamondScopeImGuiDescriptor != VK_NULL_HANDLE; }

    [[nodiscard]]
    auto GetDiamondScopeTextureID() const noexcept -> ImTextureID {
        return reinterpret_cast<ImTextureID>(m_diamondScopeImGuiDescriptor);
    }

private:
    [[nodiscard]]
    auto CreateInstance() noexcept -> bool;

    [[nodiscard]]
    auto SelectPhysicalDevice() noexcept -> bool;

    [[nodiscard]]
    auto CreateDevice() noexcept -> bool;

    [[nodiscard]]
    auto CreateDescriptorPool() noexcept -> bool;

    [[nodiscard]]
    auto CreateUploadCommandPool() noexcept -> bool;

    [[nodiscard]]
    auto CreateWindowSurfaceAndSwapchain() noexcept -> bool;

    [[nodiscard]]
    auto RecreateSourcePreviewImage(Dims2D dims) noexcept -> bool;

    [[nodiscard]]
    auto EnsureFalseColorResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateFalseColorPipeline() noexcept -> bool;

    [[nodiscard]]
    auto LoadShaderModule(const std::filesystem::path& path) noexcept -> VkShaderModule;

    [[nodiscard]]
    auto UpdateFalseColorDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto UpdateFalseColorMapBuffer(SourceYUVRange yuvRange) noexcept -> bool;

    [[nodiscard]]
    auto EnsureLumaWaveformResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateLumaWaveformPipelines() noexcept -> bool;

    [[nodiscard]]
    auto UpdateLumaWaveformDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureRgbWaveformResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateRgbWaveformPipelines() noexcept -> bool;

    [[nodiscard]]
    auto UpdateRgbWaveformDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureRgbParadeResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateRgbParadePipeline() noexcept -> bool;

    [[nodiscard]]
    auto UpdateRgbParadeDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureRgbBlacklevelResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateRgbBlacklevelPipeline() noexcept -> bool;

    [[nodiscard]]
    auto UpdateRgbBlacklevelDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureYuvParadeResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateYuvParadePipelines() noexcept -> bool;

    [[nodiscard]]
    auto UpdateYuvParadeDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureUvScopeResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateUvScopePipelines() noexcept -> bool;

    [[nodiscard]]
    auto UpdateUvScopeDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureXyzScopeResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateXyzScopePipelines() noexcept -> bool;

    [[nodiscard]]
    auto UpdateXyzScopeDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto EnsureDiamondScopeResources() noexcept -> bool;

    [[nodiscard]]
    auto CreateDiamondScopePipelines() noexcept -> bool;

    [[nodiscard]]
    auto UpdateDiamondScopeDescriptors() noexcept -> bool;

    [[nodiscard]]
    auto CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) noexcept -> bool;

    [[nodiscard]]
    auto EnsureSourcePreviewStagingBuffer(VkDeviceSize size) noexcept -> bool;

    [[nodiscard]]
    auto FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept -> std::optional<uint32_t>;

    [[nodiscard]]
    auto BeginUploadCommands() noexcept -> VkCommandBuffer;

    [[nodiscard]]
    auto EndUploadCommands(VkCommandBuffer commandBuffer, bool waitForCompletion = false) noexcept -> bool;

    [[nodiscard]]
    auto WaitForPreviewUpload() noexcept -> bool;

    void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept;

    void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, Dims2D dims) noexcept;

    void DestroySourcePreviewImage() noexcept;

    void DestroySourcePreviewStagingBuffer() noexcept;

    void DestroyFalseColorResources() noexcept;

    void DestroyFalseColorPipeline() noexcept;

    void DestroyLumaWaveformResources() noexcept;

    void DestroyLumaWaveformPipelines() noexcept;

    void DestroyRgbWaveformResources() noexcept;

    void DestroyRgbWaveformPipelines() noexcept;

    void DestroyRgbParadeResources() noexcept;

    void DestroyRgbParadePipeline() noexcept;

    void DestroyRgbBlacklevelResources() noexcept;

    void DestroyRgbBlacklevelPipeline() noexcept;

    void DestroyYuvParadeResources() noexcept;

    void DestroyYuvParadePipelines() noexcept;

    void DestroyUvScopeResources() noexcept;

    void DestroyUvScopePipelines() noexcept;

    void DestroyXyzScopeResources() noexcept;

    void DestroyXyzScopePipelines() noexcept;

    void DestroyDiamondScopeResources() noexcept;

    void DestroyDiamondScopePipelines() noexcept;

    void RebuildSwapchainIfNeeded() noexcept;

    void Shutdown() noexcept;

    [[nodiscard]]
    static auto VulkanResultToString(VkResult result) noexcept -> std::string_view;
};

} // namespace scpp
