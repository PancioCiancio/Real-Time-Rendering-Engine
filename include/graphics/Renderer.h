// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#ifndef RUN_H
#define RUN_H

#include <Volk/volk.h>
#include <SDL2/SDL.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace Renderer
{
/// Implement application specific render logic (e.g.
/// batch, pipeline, framebuffers, loop, ...)

/// Groups all vertex info of all geometries that fit one draw call.
struct BatchCpu
{
    std::vector<glm::vec3> position;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> color;
    std::vector<uint32_t> indices;
};

/// Groups all gpu buffers and gpu memories that fit one draw call command.
/// It reflects the BatchCpu data.
struct BatchGpu
{
    VkBuffer positionBuffer = {};
    VkDeviceMemory positionMem = {};

    VkBuffer normalBuffer = {};
    VkDeviceMemory normalMem = {};

    VkBuffer colorBuffer = {};
    VkDeviceMemory colorMem = {};

    VkBuffer indexBuffer = {};
    VkDeviceMemory indexMem = {};
};

/// Uniform buffer
struct PerFrameDataCpu
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
};

template <size_t N>
struct PerFrameDataGpu
{
    VkBuffer buffers[N] = {};
    VkDeviceMemory memory[N] = {};
    void *data_mapped[N] = {};
    VkDescriptorSet descriptorSets[N] = {};
};

/**
 * @brief Frame in flight (FIF) control CPU pacing and resource reuse.
 *
 * SoA of objects needed to handle the vulkan presentation synchronization
 * properly.
 */
struct FramesInFlightType
{
    /**
     * @brief Defines the amount of frames in flight generated.
     */
    static constexpr size_t MAX_FIF_COUNT = 2;

    /**
     * Semaphore signaled when the image is acquired by the swapchain.
     */
    VkSemaphore acquiredImageSemaphore[MAX_FIF_COUNT] = {};

    /**
     * Fence signaled when all commands submitted have completed execution.
     */
    VkFence submitFence[MAX_FIF_COUNT] = {};

    /**
     *
     */
    VkCommandPool commandPool[MAX_FIF_COUNT] = {};

    /**
     *
     */
    VkCommandBuffer commandBuffer[MAX_FIF_COUNT] = {};
};

/**
 * @brief Groups up the swapchain resources needed for presentation rendering.
 *
 * Swapchain related objects are heap allocated since swapchain is subject to
 * recreation. Different presentation mode can influence the swapchain creation
 * and related objects, therefore, they should handle dynamically
 */
struct PresentationFrameType
{
    /**
     *
     */
    std::vector<VkImage> image = {};

    /**
     *
     */
    std::vector<VkImageView> imageView = {};

    /**
     *
     */
    std::vector<VkFramebuffer> framebuffer = {};

    /**
     *
     */
    std::vector<VkSemaphore> renderFinishedSemaphore = {};
};

class Renderer
{
public:
    /// Init all renderer resources
    void Init();

    /// Issue renderer commands and draw on the screen
    void Update(double delta_time);

    /// De-init all renderer resources in order
    void Teardown() const;

private:
    void InitInstance();

    void InitSurface();

    void InitDevice();

    void InitSwapchain();

    /// Multi-sample image resolver, depth-stencil image
    void InitOtherImages();

    void InitCommand();

    void InitBatch();

    void InitFramebuffers();

    void InitRenderpass();

    void InitUniformBuffer();

    void InitPipeline();

    void PrepareDepthStencil();

private:
    /**
     *
     */
    VkInstance instance_ = {};

    /**
     *
     */
    VkDebugUtilsMessengerEXT debug_messenger_ = {};

    /**
     *
     */
    VkPhysicalDevice physical_device_ = {};

    /**
     *
     */
    VkDevice device_ = {};

    /**
     *
     */
    VkQueue present_queue_ = {};

    /**
     *
     */
    uint32_t present_queue_family_index_ = {};

    /**
     *
     */
    FramesInFlightType framesInFlight = {};

    /**
     *
     */
    VkSurfaceFormatKHR surfaceFormat = {};

    /**
     *
     */
    VkFormat depth_stencil_format_ = {};

    SDL_Window *window = {};
    VkExtent2D extent_ = {};
    uint32_t image_count_ = 0;

    /**
     *
     */
    VkSurfaceKHR surface_ = {};

    /**
     *
     */
    VkSwapchainKHR swapchain_ = {};

    /**
     *
     */
    PresentationFrameType presentationFrames = {};

    /**
     *
     */
    VkImage framebufferSampleImage = {};

    /**
     *
     */
    VkImageView framebufferSampleImageView = {};

    /**
     *
     */
    VkDeviceMemory framebufferSampleImageMemory = {};

    /**
     *
     */
    VkImage depth_stencil_image_ = {};

    /**
     *
     */
    VkImageView depth_stencil_image_view_ = {};

    /**
     *
     */
    VkDeviceMemory depth_stencil_memory_ = {};

    /**
     *
     */
    PerFrameDataGpu<FramesInFlightType::MAX_FIF_COUNT> uniformBufferFrames = {};

    /**
     *
     */
    VkRenderPass renderPass = {};

    /**
     *
     */
    VkPipelineLayout pipelineLayout = {};

    /**
     *
     */
    VkPipeline pipeline = {};

    /**
     *
     */
    VkPipeline pipelineWireframe = {};

    /**
     *
     */
    VkDescriptorSetLayout descriptorSetLayout = {};

    /**
     *
     */
    VkDescriptorPool descriptorPool = {};

    /**
     *
     */
    BatchCpu batchData = {};

    /**
     *
     */
    BatchGpu batch = {};

    /**
     *
     */
    VkDeviceSize vertexBufferSize = {};

    /**
     *
     */
    VkBuffer stageVertexBuffer = {};

    /**
     *
     */
    VkDeviceMemory stageVertexMemory = {};

    /**
     *
     */
    VkBuffer vertexBuffer = {};

    /**
     *
     */
    VkDeviceMemory vertexMemory = {};
};
}

#endif //RUN_H
