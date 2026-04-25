// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#ifndef ORDA_VK_RENDERER_H
#define ORDA_VK_RENDERER_H

#include <Volk/volk.h>
#include <vector>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

class Math
{
public:
    template<typename R>
    static R ToClosestPowerOfTwo(const double x)
    {
        return static_cast<R>(pow(2.0f, ceil(log2(x))));
    }

    static size_t Align(size_t size, size_t alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

struct BatchCpu
{
    std::vector<glm::vec3> position;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
};

class Renderer
{
public:
    void Load(SDL_Window* window);
    void Update(double delta_time);
    void Unload();

private:
    void LoadTextures();
    void CreateInstance();
    void CreateSurface(SDL_Window* window);
    void CreateDevice();
    void CreateSwapchain(VkSwapchainKHR old_swapchain);
    void CreateFif();
    uint32_t ChooseHeapFromFlags(const VkMemoryRequirements& mem_requirements, VkMemoryPropertyFlags required_flags, VkMemoryPropertyFlags preferred_flags) const;
    void CreateImages();
    void PrepareDepthStencil();
    void CreateUbo();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreatePipeline();
    void CreateStageBuffer();
    void CreateBatch();

private:

    // Core objects needed in every vulkan application.
    // Their lifetime usually are equal to the entire application.
    struct
    {
        VkInstance instance                         = {};
        // #ifdef _DEBUG
        VkDebugUtilsMessengerEXT debug_messenger    = {};
        // #endif
        VkSurfaceKHR surface                        = {};
        VkPhysicalDevice phys_device                = {};
        VkDevice device                             = {};
        VkQueue graphics_queue                      = {};
        VkQueue transfer_queue                      = {};   // Currently unused. Need to setup a command pool and buffer
        uint32_t graphics_queue_family_index        = {};
        uint32_t transfer_queue_family_index        = {};   // Currently unused. Need to setup a command pool and buffer
    } context_;

    // Swapchain related objectes. Used for presentation.
    // Their lifetime is strictly related to window and they must be
    // reconstructed every time the window size changes.
    // Swapchain images can vary based on the presentation mode.
    struct
    {
        VkSwapchainKHR swapchain                                = {};
        VkExtent2D extent                                       = {};
        VkSurfaceFormatKHR surface_format                       = {};
        std::vector<VkImage> images                             = {};
        std::vector<VkImageView> image_views                    = {};
        std::vector<VkSemaphore> renderer_finished_semaphores   = {};

        // MSAA
        VkImage sample_color_image              = {};
        VkImageView sample_color_image_view     = {};
        VkDeviceMemory sample_color_mem         = {};

        // Depth Stencil
        VkImage depth_stencil_image             = {};
        VkImageView depth_stencil_image_view    = {};
        VkDeviceMemory depth_stencil_mem        = {};

        std::vector<VkFramebuffer> framebuffers = {};
    } swapchain_;

    // Per-frame objects. Try to avoid cpu stalls.
    //
    // @note:
    // For each frame-in-flight (FIF)
    // - semaphore (acquire)
    // - fence (for submit, wait and reset)
    // - command pool + command buffer (for best practice)
    // For each swapchain image
    // - semaphore (renderer end)
    // - framebuffer (connected to swapchain image)
    static constexpr size_t kMaxFifCount = 2;
    struct
    {
        struct alignas(16)
        {
            float view[16]          = {};   // 4x4 Matrix
            float projection[16]    = {};   // 4x4 Matrix
        } camera_data[kMaxFifCount];

        // Uniform buffer
        VkBuffer ubo_buffer[kMaxFifCount]                   = {};
        VkDeviceMemory ubo_mem[kMaxFifCount]                = {};

        // Ssbo
        // @todo: create different ssbo based on rate of updates. They might be not tight together per-frame
        VkBuffer ssbo_buffer[kMaxFifCount]                  = {};
        VkDeviceMemory ssbo_mem[kMaxFifCount]               = {};

        VkDescriptorSet descriptor_sets[kMaxFifCount]       = {};

        // Sync Objects - Frame in flight
        VkSemaphore acquired_image_semaphores[kMaxFifCount] = {};
        VkFence submit_fences[kMaxFifCount]                 = {};
        VkCommandPool cmd_pool[kMaxFifCount]                = {};
        VkCommandBuffer cmd_buffer[kMaxFifCount]            = {};   // We are creating one command buffer for each command pool.
    } frame_data_;

    struct
    {
        VkRenderPass render_pass                            = {};
        VkDescriptorSetLayout descriptor_set_layout         = {};
        VkDescriptorPool descriptor_pool                    = {};
        VkPipelineLayout pipeline_layout                    = {};
        VkPipeline physical_base_rendering_pipeline         = {};
    } pipeline_;

    struct SsboObjectData
    {
        alignas(16) uint32_t texture_id     = {};
        alignas(16) glm::mat4 model_matrix  = {};
    };

    // Scene has:
    // - vertex pack together different geometries { position, normal, color, ... }
    // - store the offset for each vertex property. They will be used for bind commands.
    // - index pack together different geometries' indices.
    // - indirect draw pack together different indirect draw commands.
    struct
    {
        VkBuffer vertex_buffer              = {};
        VkDeviceMemory vertex_mem           = {};

        VkDeviceSize vertex_position_offset = {};
        VkDeviceSize vertex_normal_offset   = {};
        VkDeviceSize vertex_uv_offset    = {};

        VkBuffer index_buffer               = {};
        VkDeviceMemory index_mem            = {};

        VkBuffer indirect_draw_buffer       = {};
        VkDeviceMemory indirect_draw_mem    = {};

        VkImage texture_image               = {};
        VkImageView texture_image_view      = {};
        VkDeviceMemory texture_mem          = {};
        VkSampler texture_sampler           = {};

        VkBuffer stage_buffer               = {};
        VkDeviceMemory stage_mem            = {};
    } scene_;

    struct
    {
        bool resize_requested   = {};
        uint32_t frame_index    = {};
    } loop_;
};

#endif //ORDA_VK_RENDERER_H
