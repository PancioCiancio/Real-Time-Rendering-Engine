// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#include "vk_swapchain.h"

#include <optional>

#include "vk_core_utils.h"
#include "vk_physical_device_utils.h"
#include "vk_surface_utils.h"
#include "vk_swapchain_utils.h"
#include "vk_memory_utils.h"

namespace vk_utils
{
std::expected<VulkanSwapchain, VkResult> CreateSwapchain(
    const VulkanContext &ctx,
    VkExtent2D extent)
{
    VulkanSwapchain swapchain = {};

    swapchain.surface_format = SelectSurfaceFormat(
        ctx.phys_device,
        ctx.surface,
        {VK_FORMAT_R8G8B8A8_SRGB, std::nullopt}).value();

    SwapchainResult swapchain_result = BuildSwapchain(
        ctx.device,
        ctx.phys_device,
        ctx.surface,
        {
            swapchain.surface_format,
            extent,
            std::nullopt,
            VK_NULL_HANDLE
        }).value();

    swapchain.swapchain = swapchain_result.swapchain;
    swapchain.extent = swapchain_result.extent;

    swapchain.images = GetSwapchainImages(
        ctx.device,
        swapchain.swapchain).value();

    swapchain.image_views.reserve(swapchain_result.image_count);
    swapchain.framebuffers.reserve(swapchain_result.image_count);
    swapchain.render_finished_semaphores.reserve(swapchain_result.image_count);

    // Create the view of the swapchain images
    for (uint32_t i = 0; i < swapchain_result.image_count; i++)
    {
        VkImageView image_view = {};
        image_view = CreateImageView(
            ctx.device,
             swapchain.images[i],
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            swapchain.surface_format.format,
            {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            }).value();

        swapchain.image_views.push_back(image_view);
    }

    for (uint32_t i = 0; i < swapchain_result.image_count; i++)
    {
        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_create_info.flags = 0;

        VkSemaphore semaphore = {};
        VK_CHECK(
            vkCreateSemaphore(
                ctx.device,
                &semaphore_create_info,
                nullptr,
                &semaphore));

        swapchain.render_finished_semaphores.push_back(semaphore);
    }

    return swapchain;
}

void DestroySwapchain(
    const VulkanContext &ctx,
    VulkanSwapchain& swapchain)
{
    vkDeviceWaitIdle(ctx.device);

    const uint32_t framebuffer_count = swapchain.framebuffers.size();
    for (uint32_t i = 0; i < framebuffer_count; i++)
    {
        vkDestroyFramebuffer(ctx.device, swapchain.framebuffers[i], nullptr);
    }

    const uint32_t image_count = swapchain.image_views.size();
    for (uint32_t i = 0; i < image_count; i++)
    {
        vkDestroyImageView(ctx.device, swapchain.image_views[i], nullptr);
    }

    const uint32_t semaphore_count = swapchain.render_finished_semaphores.size();
    for (uint32_t i = 0; i < semaphore_count; i++)
    {
        vkDestroySemaphore(ctx.device, swapchain.render_finished_semaphores[i], nullptr);
    }

    vkDestroySwapchainKHR(ctx.device, swapchain.swapchain, nullptr);

    swapchain.framebuffers.clear();
    swapchain.image_views.clear();
    swapchain.images.clear();
    swapchain.render_finished_semaphores.clear();
}
}
