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
#include <expected>
#include <SDL3/SDL.h>

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
    } swapchain_;

    struct
    {
        VkImage sample_color_image              = {};
        VkImageView sample_color_image_view     = {};
        VkDeviceMemory sample_color_mem         = {};

        VkImage depth_stencil_image             = {};
        VkImageView depth_stencil_image_view    = {};
        VkDeviceMemory depth_stencil_mem        = {};
    } framebuffer_;

    static constexpr size_t kMaxFifCount = 2;
    struct
    {
        VkSemaphore acquired_image_semaphores[kMaxFifCount] = {};
        VkFence submit_fences[kMaxFifCount]                 = {};
        VkCommandPool cmd_pool[kMaxFifCount]                = {};
        VkCommandBuffer cmd_buffer[kMaxFifCount]            = {};   // We are creating one command buffer for each command pool.
    } frames_in_flight_;
};

#endif //ORDA_VK_RENDERER_H
