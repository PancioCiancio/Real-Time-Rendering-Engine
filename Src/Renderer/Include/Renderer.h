//
// Created by apant on 02/08/2025.
//

#ifndef RUN_H
#define RUN_H

#include <volk/volk.h>
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
    VkImage *image = {};

    /**
     *
     */
    VkImageView *imageView = {};

    /**
     *
     */
    VkFramebuffer *framebuffer = {};

    /**
     *
     */
    VkSemaphore *renderFinishedSemaphore = {};
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

    void InitFramebuffers() const;

    void InitRenderpass();

    void InitUniformBuffer();

    void InitPipeline();

    void PrepareDepthStencil();

private:
    /**
     *
     */
    VkInstance instance = {};

    /**
     *
     */
    VkDebugUtilsMessengerEXT debugMessenger = {};

    /**
     *
     */
    VkPhysicalDevice gpu = {};

    /**
     *
     */
    VkDevice device = {};

    /**
     *
     */
    VkQueue queue = {};

    /**
     *
     */
    uint32_t queueFamilyIndex = {};

    /**
     *
     */
    FramesInFlightType framesInFlight = {};

    /**
     *
     */
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};

    /**
     *
     */
    VkSurfaceFormatKHR surfaceFormat = {};

    /**
     *
     */
    VkFormat depthStencilFormat = {};

    /**
     *
     */
    SDL_Window *window = {};

    /**
     *
     */
    VkSurfaceKHR surface = {};

    /**
     *
     */
    VkSwapchainKHR swapchain = {};

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
    VkImage depthStencilImage = {};

    /**
     *
     */
    VkImageView depthStencilImageView = {};

    /**
     *
     */
    VkDeviceMemory depthStencilMemory = {};

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
};
}

#endif //RUN_H
