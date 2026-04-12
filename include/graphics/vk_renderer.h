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
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

class Math
{
public:
    template<typename R>
    static R ToClosestPowerOfTwo(const double x)
    {
        static_cast<R>(pow(2.0f, ceil(log2(x))));
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
    std::vector<glm::vec4> color;
    std::vector<uint32_t> indices;
};

class Renderer
{
public:
    void Load(SDL_Window* window);

private:
    void CreateInstance();
    void CreateSurface(SDL_Window* window);
    void CreateDevice();
    void CreateSwapchain();
    void CreateFif();
    uint32_t ChooseHeapFromFlags(const VkMemoryRequirements& mem_requirements, VkMemoryPropertyFlags required_flags, VkMemoryPropertyFlags preferred_flags) const;
    void CreateImages();
    void PrepareDepthStencil();
    void CreateUbo();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreatePipeline();
    void CreateBatch();

private:
    struct
    {
        VkInstance instance                         = {};
        #ifdef _DEBUG
        VkDebugUtilsMessengerEXT debug_messenger    = {};
        #endif
        VkSurfaceKHR surface                        = {};
        VkPhysicalDevice phys_device                = {};
        VkDevice device                             = {};
        VkQueue graphics_queue                      = {};
        VkQueue transfer_queue                      = {};   // Currently unused. Need to setup a command pool and buffer
        uint32_t graphics_queue_family_index        = {};
        uint32_t transfer_queue_family_index        = {};   // Currently unused. Need to setup a command pool and buffer
    } context_;

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

    static constexpr size_t kMaxFifCount = 2;
    struct
    {
        struct alignas(16)
        {
            float view[16]          = {};   // 4x4 Matrix
            float projection[16]    = {};   // 4x4 Matrix
        } camera_data;

        // Uniform buffer
        VkBuffer ubo_buffer[kMaxFifCount]                   = {};
        VkDeviceMemory ubo_mem[kMaxFifCount]                = {};

        // Ssbo
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

    struct
    {
        VkBuffer vertex_buffer = {};
        VkDeviceMemory vertex_mem = {};

        VkBuffer index_buffer = {};
        VkDeviceMemory index_mem = {};

        VkBuffer indirect_draw_buffer = {};
        VkDeviceMemory indirect_draw_mem = {};
    } scene_;
};

#endif //ORDA_VK_RENDERER_H
