// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#include "vk_renderer.h"

#include <SDL3/SDL_vulkan.h>
#include <array>
#include <bits/stl_queue.h>

void Renderer::Load(SDL_Window *window)
{
    volkInitialize();

    CreateInstance();
    CreateSurface(window);
    CreateDevice();

    volkLoadDevice(context_.device);

    vkGetDeviceQueue(context_.device, 0, 0, &context_.graphics_queue);
    vkGetDeviceQueue(context_.device, 1, 0, &context_.transfer_queue);

    CreateSwapchain();
    CreateFif();
    CreateImages();
    PrepareDepthStencil();
}

void Renderer::CreateInstance()
{
    // Layers requested by the application.
    std::array<const char*, 1> layers = {
        "VK_LAYER_KHRONOS_validation"};

    // extensions requested by the application.
    std::array<const char*, 3> extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,      // We want to enable debug
        VK_KHR_SURFACE_EXTENSION_NAME,          // Need for presentation
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME};   // Platform specification

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Orda";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 3, 0);         // Use 1.3 for dynamic rendering

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    #ifdef _DEBUG
    create_info.pNext = &DEBUG_UTILS_MESSENGER_CREATE_INFO;
    #endif
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(std::size(layers));
    create_info.ppEnabledLayerNames = layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
    create_info.ppEnabledExtensionNames = extensions.data();
}

void Renderer::CreateSurface(SDL_Window *window)
{
    SDL_Vulkan_CreateSurface(window, context_.instance, nullptr, &context_.surface);
}

void Renderer::CreateDevice()
{
    // Define here the features requested by the application.
    VkPhysicalDeviceFeatures required_features = {};
    required_features.geometryShader        = VK_TRUE;
    required_features.tessellationShader    = VK_TRUE;
    required_features.multiDrawIndirect     = VK_TRUE;
    required_features.fillModeNonSolid      = VK_TRUE;
    required_features.sampleRateShading     = VK_TRUE;
    required_features.samplerAnisotropy     = VK_TRUE;

    std::array<const char*, 3> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_dynamic_rendering",         // Allow to bind render pass and framebuffer dynamically.
        "VK_KHR_shader_draw_parameters"};   // Provides access to three additional built-in shader variables in Vulkan (Indirect drawing command)

    uint32_t phys_device_count = 0;
    vkEnumeratePhysicalDevices(context_.instance, &phys_device_count, nullptr);

    std::array<VkPhysicalDevice, 4> phys_devices = {};  // We can list up to four physical devices.
    vkEnumeratePhysicalDevices(context_.instance, &phys_device_count, phys_devices.data());

    context_.phys_device = phys_devices[0]; // Using vulkan caps viewer, we know we are looking to the first gpu.

    constexpr std::array<float, 1> priorities = { 1.0f };
    std::array<VkDeviceQueueCreateInfo, 2> queue_create_info = {};
    // Graphics + Presentation queue (using vulkan caps viewer).
    queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info[0].queueFamilyIndex = 0;
    queue_create_info[0].queueCount = 1;
    queue_create_info[0].pQueuePriorities = priorities.data();
    // Transfer queue (using vulkan caps viewer)
    queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info[1].queueFamilyIndex = 1;
    queue_create_info[1].queueCount = 1;
    queue_create_info[1].pQueuePriorities = priorities.data();

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_info.size());
    device_create_info.pQueueCreateInfos = queue_create_info.data();
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.pEnabledFeatures = &required_features;

    vkCreateDevice(context_.phys_device, &device_create_info, nullptr, &context_.device);
}

void Renderer::CreateSwapchain()
{
    VkSurfaceCapabilitiesKHR caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.phys_device, context_.surface, &caps);

    constexpr VkPresentModeKHR present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    swapchain_.surface_format = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};   // Known from vulkan caps viewer
    swapchain_.extent = caps.currentExtent;

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = context_.surface;
    create_info.minImageCount = caps.minImageCount + 1;
    create_info.imageFormat = swapchain_.surface_format.format;
    create_info.imageColorSpace = swapchain_.surface_format.colorSpace;
    create_info.imageExtent = swapchain_.extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = nullptr;

    vkCreateSwapchainKHR(context_.device, &create_info, nullptr, &swapchain_.swapchain);

    // Init images
    uint32_t swapchain_image_count = {};
    vkGetSwapchainImagesKHR(context_.device, swapchain_.swapchain, &swapchain_image_count, nullptr);
    swapchain_.images.reserve(swapchain_image_count);
    swapchain_.image_views.reserve(swapchain_image_count);
    swapchain_.renderer_finished_semaphores.reserve(swapchain_image_count);
    vkGetSwapchainImagesKHR(context_.device, swapchain_.swapchain, &swapchain_image_count, swapchain_.images.data());

    // Init image views
    for (uint32_t i = 0; i < swapchain_image_count; i++)
    {
        VkImageView image_view = {};

        VkImageSubresourceRange subresource_range = {};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.pNext = nullptr;
        image_view_create_info.flags = 0;
        image_view_create_info.image = swapchain_.images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = swapchain_.surface_format.format;
        image_view_create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        image_view_create_info.subresourceRange = subresource_range;
        vkCreateImageView(context_.device, &image_view_create_info, nullptr, &image_view);

        swapchain_.image_views.push_back(image_view);
    }

    // Create synchronization objects
    for (uint32_t i = 0; i < create_info.minImageCount; i++)
    {
        VkSemaphore semaphore = {};

        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_create_info.flags = 0;
        vkCreateSemaphore(context_.device, &semaphore_create_info, nullptr, &semaphore);

        swapchain_.renderer_finished_semaphores.push_back(semaphore);
    }
}

void Renderer::CreateFif()
{
    VkCommandPoolCreateInfo cmd_pool_create_info = {};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (size_t i = 0; i < kMaxFifCount; i++)
    {
        VkCommandPool &cmd_pool = frames_in_flight_.cmd_pool[i];
        vkCreateCommandPool(context_.device, &cmd_pool_create_info, nullptr, &cmd_pool);

        VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {};
        cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buffer_alloc_info.commandPool = cmd_pool;
        cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buffer_alloc_info.commandBufferCount = 1;
        vkAllocateCommandBuffers(context_.device, &cmd_buffer_alloc_info, &frames_in_flight_.cmd_buffer[i]);

        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_create_info.flags = 0;
        vkCreateSemaphore(context_.device, &semaphore_create_info, nullptr, &frames_in_flight_.acquired_image_semaphores[i]);

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(context_.device, &fence_create_info, nullptr, &frames_in_flight_.submit_fences[i]);
    }
}

uint32_t Renderer::ChooseHeapFromFlags(
    const VkMemoryRequirements &mem_requirements,
    VkMemoryPropertyFlags required_flags,
    VkMemoryPropertyFlags preferred_flags) const
{
    VkPhysicalDeviceMemoryProperties mem_properties = {};
    vkGetPhysicalDeviceMemoryProperties(context_.phys_device, &mem_properties);

    uint32_t selected_type = ~0u;
    uint32_t memory_type = 0;

    for (memory_type = 0; memory_type < 32; ++memory_type)
    {
        if (mem_requirements.memoryTypeBits & (1 << memory_type))
        {
            const VkMemoryType& type = mem_properties.memoryTypes[memory_type];

            if ((type.propertyFlags & preferred_flags) == preferred_flags)
            {
                selected_type = memory_type;
                break;
            }
        }
    }

    if (selected_type != ~0u)
    {
        for (memory_type = 0; memory_type < 32; ++memory_type)
        {
            if (mem_requirements.memoryTypeBits & (1 << memory_type))
            {
                const VkMemoryType& type = mem_properties.memoryTypes[memory_type];

                if ((type.propertyFlags & required_flags) == required_flags)
                {
                    selected_type = memory_type;
                    break;
                }
            }
        }
    }

    return selected_type;
}

void Renderer::CreateImages()
{
    // Define how much sample to apply and color attachment resolver.
    constexpr VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_4_BIT;
    constexpr VkComponentMapping component_swizzle = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

    // Gather memory properties to allocate images.
    VkPhysicalDeviceMemoryProperties memory_properties = {};
    vkGetPhysicalDeviceMemoryProperties(context_.phys_device, &memory_properties);

    std::array<VkImageCreateInfo, 2> image_create_infos = {};
    // Sample color
    image_create_infos[0].sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_infos[0].flags = 0;
    image_create_infos[0].imageType = VK_IMAGE_TYPE_2D;
    image_create_infos[0].format = swapchain_.surface_format.format;
    image_create_infos[0].extent = { swapchain_.extent.width, swapchain_.extent.height, 1 };
    image_create_infos[0].mipLevels = 1;
    image_create_infos[0].arrayLayers = 1;
    image_create_infos[0].samples = sample_count;
    image_create_infos[0].tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_infos[0].usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_create_infos[0].sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_infos[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Depth + Stencil
    image_create_infos[1].sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_infos[1].flags = 0;
    image_create_infos[1].imageType = VK_IMAGE_TYPE_2D;
    image_create_infos[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    image_create_infos[1].extent = { swapchain_.extent.width, swapchain_.extent.height, 1 };
    image_create_infos[1].mipLevels = 1;
    image_create_infos[1].arrayLayers = 1;
    image_create_infos[1].samples = sample_count;
    image_create_infos[1].tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_infos[1].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_create_infos[1].sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_infos[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    std::array<VkImageSubresourceRange, 2> subresource_ranges = {};
    // Sample Color
    subresource_ranges[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_ranges[0].baseMipLevel = 0;
    subresource_ranges[0].levelCount = 1;
    subresource_ranges[0].baseArrayLayer = 0;
    subresource_ranges[0].layerCount = 1;
    // Depth + Stencil
    subresource_ranges[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    subresource_ranges[1].baseMipLevel = 0;
    subresource_ranges[1].levelCount = 1;
    subresource_ranges[1].baseArrayLayer = 0;
    subresource_ranges[1].layerCount = 1;

    std::array<VkImageViewCreateInfo, 2> image_view_create_infos = {};
    // Sample Color
    image_view_create_infos[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_infos[0].pNext = nullptr;
    image_view_create_infos[0].flags = 0;
    image_view_create_infos[0].image = framebuffer_.sample_color_image;
    image_view_create_infos[0].viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_infos[0].format = swapchain_.surface_format.format;
    image_view_create_infos[0].components = component_swizzle;
    image_view_create_infos[0].subresourceRange = subresource_ranges[0];
    // Depth + Stencil
    image_view_create_infos[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_infos[1].pNext = nullptr;
    image_view_create_infos[1].flags = 0;
    image_view_create_infos[1].image = framebuffer_.depth_stencil_image;
    image_view_create_infos[1].viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_infos[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    image_view_create_infos[1].components = component_swizzle;
    image_view_create_infos[1].subresourceRange = subresource_ranges[1];

    // 1: Sample color attachment.
    // 2: Depth stencil.
    // We need only one for each type, because they will be discarded on re-use.
    std::array<VkImage*, 2> images = { &framebuffer_.sample_color_image, &framebuffer_.depth_stencil_image };
    std::array<VkImageView*, 2> image_views = {&framebuffer_.sample_color_image_view, &framebuffer_.depth_stencil_image_view};
    std::array<VkDeviceMemory*, 2> mems = { &framebuffer_.sample_color_mem, &framebuffer_.depth_stencil_mem };

    for (size_t i = 0; i < images.size(); i++)
    {
        vkCreateImage(context_.device, &image_create_infos[0], nullptr, images[0]);

        VkMemoryRequirements mem_requirements = {};
        vkGetImageMemoryRequirements(context_.device, *images[0], &mem_requirements);

        const uint32_t memory_type = ChooseHeapFromFlags(
            mem_requirements,
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.allocationSize = mem_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        vkAllocateMemory(context_.device, &allocate_info, nullptr, mems[0]);
        vkBindImageMemory(context_.device, *images[0], *mems[0], 0);
        vkCreateImageView(context_.device, &image_view_create_infos[0], nullptr, image_views[0]);
    }
}

void Renderer::PrepareDepthStencil()
{
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frames_in_flight_.cmd_buffer[0], &cmd_buffer_begin_info);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.image = framebuffer_.depth_stencil_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(frames_in_flight_.cmd_buffer[0],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);

    vkEndCommandBuffer(frames_in_flight_.cmd_buffer[0]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frames_in_flight_.cmd_buffer[0];

    vkQueueSubmit(context_.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphics_queue);
}
