// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#include "vk_renderer.h"

#include <SDL2/SDL_vulkan.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <array>
#include <cstring>
#include <print>
#include <unordered_set>

#define STB_IMAGE_IMPLEMENTATION
#include <type_traits>
#include <stb/stb_image.h>

#include "MeshLoader.h"
#include "../FileSystem.h"

void Renderer::Load(SDL_Window *window)
{
    volkInitialize();

    CreateInstance();
    volkLoadInstance(context_.instance);

    CreateSurface(window);
    CreateDevice();
    volkLoadDevice(context_.device);

    vkGetDeviceQueue(context_.device, context_.graphics_queue_family_index, 0, &context_.graphics_queue);
    // vkGetDeviceQueue(context_.device, 1, 0, &context_.transfer_queue);

    CreateSwapchain(VK_NULL_HANDLE);
    CreateFif();
    CreateImages();
    PrepareDepthStencil();

    CreateStageBuffer();
    CreateBatch();
    CreateUbo();
    CreateRenderPass();
    CreateFramebuffers();
    CreatePipeline();

    LoadTextures();
}

void Renderer::Update(double delta_time)
{
    if (loop_.resize_requested)
    {
        vkDeviceWaitIdle(context_.device);

        // Destroy: framebuffers
        for (auto& framebuffer : swapchain_.framebuffers)
        {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }

        // Destroy: depth+stencil, color sampler, swapchain image vies
        vkDestroyImageView(context_.device, swapchain_.sample_color_image_view, nullptr);
        vkDestroyImageView(context_.device, swapchain_.depth_stencil_image_view, nullptr);

        vkDestroyImage(context_.device, swapchain_.sample_color_image, nullptr);
        vkDestroyImage(context_.device, swapchain_.depth_stencil_image, nullptr);

        vkFreeMemory(context_.device, swapchain_.sample_color_mem, nullptr);
        vkFreeMemory(context_.device, swapchain_.depth_stencil_mem, nullptr);

        for (auto& image_view : swapchain_.image_views)
        {
            vkDestroyImageView(context_.device, image_view, nullptr);
        }

        for (auto& semaphore : swapchain_.renderer_finished_semaphores)
        {
            vkDestroySemaphore(context_.device, semaphore, nullptr);
        }

        VkSwapchainKHR old_swapchain = swapchain_.swapchain;
        CreateSwapchain(old_swapchain);                                  // Create the new swapchain by reusing the old one
        vkDestroySwapchainKHR(context_.device, old_swapchain, nullptr);  // Destroy the old swapchain

        CreateImages();
        CreateFramebuffers();
        PrepareDepthStencil();

        loop_.resize_requested = false;
    }

    // Camera calculation
    glm::vec3 camera_pos        = {0.0f, 0.0f, -200.0f};
    glm::vec3 camera_pos_new    = {0.0f, 0.0f, -200.0f};
    glm::vec3 camera_front      = {0.0f, 0.0f, 1.0f};
    glm::vec3 camera_up         = {0.0f, 1.0f, 0.0f};

    constexpr float p = 1.0f / 100.0f;
    constexpr float t = 0.396f;
    const float half_time = -t / glm::log2(p);

    const float camera_lerp_alpha = 1.0f - glm::pow(2.0f, -static_cast<float>(delta_time) / half_time);
    camera_pos = glm::mix(camera_pos, camera_pos_new, camera_lerp_alpha);

    // Vulkan wait
    vkWaitForFences(context_.device, 1, &frame_data_.submit_fences[loop_.frame_index], VK_TRUE, UINT64_MAX);
    uint32_t next_image = 0u;
    const VkResult acquire_image_result = vkAcquireNextImageKHR(context_.device, swapchain_.swapchain, UINT64_MAX, frame_data_.acquired_image_semaphores[loop_.frame_index], VK_NULL_HANDLE, &next_image);

    if (acquire_image_result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        loop_.resize_requested = true;
        return;
    }

    vkResetFences(context_.device, 1, &frame_data_.submit_fences[loop_.frame_index]);
    vkResetCommandPool(context_.device, frame_data_.cmd_pool[loop_.frame_index], 0);

    glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchain_.extent.width) / static_cast<float>(swapchain_.extent.height), 0.1f, 1000.0f);

    std::memcpy(frame_data_.camera_data[loop_.frame_index].view, glm::value_ptr(view), sizeof(float) * 16);
    std::memcpy(frame_data_.camera_data[loop_.frame_index].projection, glm::value_ptr(projection), sizeof(float) * 16);

    frame_data_.camera_data[loop_.frame_index].projection[5] *= -1;

    void* ubo_mapped_data = {};
    vkMapMemory(context_.device, frame_data_.ubo_mem[loop_.frame_index], 0, sizeof(float) * 16 * 2, 0, &ubo_mapped_data);
    std::memcpy(ubo_mapped_data, &frame_data_.camera_data[loop_.frame_index], sizeof(float) * 16 * 2);
    vkUnmapMemory(context_.device, frame_data_.ubo_mem[loop_.frame_index]);

    VkCommandBufferBeginInfo cmd_begin_info = {};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = 0;
    cmd_begin_info.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(frame_data_.cmd_buffer[loop_.frame_index], &cmd_begin_info);

    vkCmdBindDescriptorSets(frame_data_.cmd_buffer[loop_.frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline_layout, 0, 1, &frame_data_.descriptor_sets[loop_.frame_index], 0, nullptr);
    std::array<VkClearValue, 2> clear_values = {};
    clear_values[0].color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f }};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = pipeline_.render_pass;
    render_pass_begin_info.framebuffer = swapchain_.framebuffers[next_image];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = swapchain_.extent;
    render_pass_begin_info.clearValueCount = clear_values.size();
    render_pass_begin_info.pClearValues = clear_values.data();
    vkCmdBeginRenderPass(frame_data_.cmd_buffer[loop_.frame_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(frame_data_.cmd_buffer[loop_.frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.physical_base_rendering_pipeline);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_.extent.width);
    viewport.height = static_cast<float>(swapchain_.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frame_data_.cmd_buffer[loop_.frame_index], 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_.extent;
    vkCmdSetScissor(frame_data_.cmd_buffer[loop_.frame_index], 0, 1, &scissor);

    std::array<VkBuffer, 3> buffer_binds = {};
    buffer_binds[0] = scene_.vertex_buffer;
    buffer_binds[1] = scene_.vertex_buffer;
    buffer_binds[2] = scene_.vertex_buffer;

    std::array<VkDeviceSize, 3> buffer_bind_offsets = {};
    buffer_bind_offsets[0] = scene_.vertex_position_offset;
    buffer_bind_offsets[1] = scene_.vertex_normal_offset;
    buffer_bind_offsets[2] = scene_.vertex_uv_offset;
    vkCmdBindVertexBuffers(frame_data_.cmd_buffer[loop_.frame_index], 0, buffer_binds.size(), buffer_binds.data(), buffer_bind_offsets.data());
    vkCmdBindIndexBuffer(frame_data_.cmd_buffer[loop_.frame_index], scene_.index_buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirect(frame_data_.cmd_buffer[loop_.frame_index], scene_.indirect_draw_buffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
    vkCmdEndRenderPass(frame_data_.cmd_buffer[loop_.frame_index]);
    vkEndCommandBuffer(frame_data_.cmd_buffer[loop_.frame_index]);

    std::array<VkSemaphore, 1> wait_semaphores = {};
    wait_semaphores[0] = frame_data_.acquired_image_semaphores[loop_.frame_index];

    std::array<VkPipelineStageFlags, 1> wait_stages = {};
    wait_stages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    std::array<VkSemaphore, 1> signal_semaphores = {};
    signal_semaphores[0] = swapchain_.renderer_finished_semaphores[next_image];

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = wait_semaphores.size();
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame_data_.cmd_buffer[loop_.frame_index];
    submit_info.signalSemaphoreCount = signal_semaphores.size();
    submit_info.pSignalSemaphores = signal_semaphores.data();
    vkQueueSubmit(context_.graphics_queue, 1, &submit_info, frame_data_.submit_fences[loop_.frame_index]);

    VkResult result = {};
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &swapchain_.renderer_finished_semaphores[next_image];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_.swapchain;
    present_info.pImageIndices = &next_image;
    present_info.pResults = &result;
    const VkResult present_result = vkQueuePresentKHR(context_.graphics_queue, &present_info);

    if (present_result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        loop_.resize_requested = true;
        return;
    }

    loop_.frame_index = (loop_.frame_index + 1) % kMaxFifCount;
}

void Renderer::Unload()
{
    vkDeviceWaitIdle(context_.device);

    vkDestroySampler(context_.device, scene_.texture_sampler, nullptr);

    for (auto& image_view : scene_.texture_image_views)
    {
        vkDestroyImageView(context_.device, image_view, nullptr);
    }

    for (auto& image : scene_.texture_images)
    {
        vkDestroyImage(context_.device, image, nullptr);
    }

    for (auto& mem : scene_.texture_mems)
    {
        vkFreeMemory(context_.device, mem, nullptr);
    }

    vkFreeMemory(context_.device, scene_.vertex_mem, nullptr);
    vkFreeMemory(context_.device, scene_.index_mem, nullptr);
    vkFreeMemory(context_.device, scene_.indirect_draw_mem, nullptr);
    vkFreeMemory(context_.device, scene_.stage_mem, nullptr);

    vkDestroyBuffer(context_.device, scene_.vertex_buffer, nullptr);
    vkDestroyBuffer(context_.device, scene_.index_buffer, nullptr);
    vkDestroyBuffer(context_.device, scene_.indirect_draw_buffer, nullptr);
    vkDestroyBuffer(context_.device, scene_.stage_buffer, nullptr);

    vkDestroyPipeline(context_.device, pipeline_.physical_base_rendering_pipeline, nullptr);
    vkDestroyPipelineLayout(context_.device, pipeline_.pipeline_layout, nullptr);
    vkDestroyDescriptorPool(context_.device, pipeline_.descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(context_.device, pipeline_.descriptor_set_layout, nullptr);
    vkDestroyRenderPass(context_.device, pipeline_.render_pass, nullptr);

    for (size_t i = 0; i < kMaxFifCount; i++)
    {
        vkFreeMemory(context_.device, frame_data_.ubo_mem[i], nullptr);
        vkFreeMemory(context_.device, frame_data_.ssbo_mem[i], nullptr);

        vkDestroyBuffer(context_.device, frame_data_.ubo_buffer[i], nullptr);
        vkDestroyBuffer(context_.device, frame_data_.ssbo_buffer[i], nullptr);

        vkDestroySemaphore(context_.device, frame_data_.acquired_image_semaphores[i], nullptr);
        vkDestroyFence(context_.device, frame_data_.submit_fences[i], nullptr);
        vkDestroyCommandPool(context_.device, frame_data_.cmd_pool[i], nullptr);
    }

    vkFreeMemory(context_.device, swapchain_.depth_stencil_mem, nullptr);
    vkFreeMemory(context_.device, swapchain_.sample_color_mem, nullptr);

    vkDestroyImageView(context_.device, swapchain_.depth_stencil_image_view, nullptr);
    vkDestroyImageView(context_.device, swapchain_.sample_color_image_view, nullptr);

    vkDestroyImage(context_.device, swapchain_.depth_stencil_image, nullptr);
    vkDestroyImage(context_.device, swapchain_.sample_color_image, nullptr);

    for (size_t i = 0; i < swapchain_.images.size(); i++)
    {
        vkDestroyFramebuffer(context_.device, swapchain_.framebuffers[i], nullptr);
        vkDestroyImageView(context_.device, swapchain_.image_views[i], nullptr);

        vkDestroySemaphore(context_.device, swapchain_.renderer_finished_semaphores[i], nullptr);
    }

    vkDestroySwapchainKHR(context_.device, swapchain_.swapchain, nullptr);

    vkDestroyDevice(context_.device, nullptr);
    vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
    // vkDestroyDebugUtilsMessengerEXT(context_.instance, context_.debug_messenger, nullptr);
    vkDestroyInstance(context_.instance, nullptr);
}

void Renderer::LoadTextures()
{
    VkPhysicalDeviceProperties phys_device_properties = {};
    vkGetPhysicalDeviceProperties(context_.phys_device, &phys_device_properties);

    // Get array of paths.
    // load the raw textures.
    constexpr size_t texture_count = 2;
    const std::array<const char*, texture_count> textures_path = {
        "../resources/textures/mega_mike_z/T_ZMike_Green_Base_color.png",
        "../resources/textures/mega_mike_z/T_ZMike_Normal_DirectX.png"
    };

    scene_.texture_images.resize(texture_count);
    scene_.texture_image_views.resize(texture_count);
    scene_.texture_mems.resize(texture_count);

    std::array<stbi_uc*, texture_count> textures_pixels = {};
    std::array<int, texture_count> textures_width       = {};
    std::array<int, texture_count> textures_height      = {};
    std::array<int, texture_count> textures_channels    = {};
    std::array<VkDeviceSize, texture_count> textures_mem_size       = {};
    std::array<VkDeviceSize, texture_count> textures_mem_offsets    = {};

    for (size_t i = 0; i < texture_count; i++)
    {
        textures_pixels[i] = stbi_load(textures_path[i], &textures_width[i], &textures_height[i], &textures_channels[i], STBI_rgb_alpha);
        textures_mem_size[i] = textures_width[i] * textures_height[i] * 4;
    }

    for (size_t i = 1; i < texture_count; i++)
    {
        textures_mem_offsets[i] = Math::Align(textures_mem_offsets[i-1] + textures_mem_size[i - 1], phys_device_properties.limits.minMemoryMapAlignment);
    }

    // Write the pixels into the staging buffer.
    const VkDeviceSize total_image_size = textures_mem_offsets[texture_count - 1] + textures_mem_size[texture_count - 1];
    void* data = {};
    vkMapMemory(context_.device, scene_.stage_mem, 0, total_image_size, 0, &data);
    char* base_ptr = static_cast<char*>(data);

    for (size_t i = 0; i < texture_count; i++)
    {
        std::memcpy(base_ptr + textures_mem_offsets[i], textures_pixels[i], textures_mem_size[i]);
    }

    vkUnmapMemory(context_.device, scene_.stage_mem);

    // Free the raw image pixel data
    for (auto& pixels : textures_pixels)
    {
        stbi_image_free(pixels);
    }

    // @todo: we are creating albedo and normal with the same format. It's wrong!!!
    for (size_t i = 0; i < texture_count; i++)
    {
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = static_cast<uint32_t>(textures_width[i]);
        image_info.extent.height = static_cast<uint32_t>(textures_height[i]);
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateImage(context_.device, &image_info, nullptr, &scene_.texture_images[i]);

        VkMemoryRequirements mem_requirements = {};
        vkGetImageMemoryRequirements(context_.device, scene_.texture_images[i], &mem_requirements);
        const uint32_t image_mem_type = ChooseHeapFromFlags(mem_requirements, mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo image_alloc_info = {};
        image_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        image_alloc_info.allocationSize = mem_requirements.size;
        image_alloc_info.memoryTypeIndex = image_mem_type;

        vkAllocateMemory(context_.device, &image_alloc_info, nullptr, &scene_.texture_mems[i]);
        vkBindImageMemory(context_.device, scene_.texture_images[i], scene_.texture_mems[i], 0);
    }

    VkCommandBufferBeginInfo cmd_begin_info = {};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(frame_data_.cmd_buffer[0], &cmd_begin_info);

    // Transition Undefined -> Transfer Dst
    std::array<VkImageMemoryBarrier, texture_count> barriers = {};
    for (size_t i = 0; i < texture_count; i++)
    {
        VkImageMemoryBarrier& barrier = barriers[i];
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = scene_.texture_images[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    vkCmdPipelineBarrier(
        frame_data_.cmd_buffer[0],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data()
    );

    // Copy Buffer to Image
    std::array<VkBufferImageCopy, texture_count> regions = {};
    for (size_t i = 0; i < texture_count; i++)
    {
        VkBufferImageCopy& region = regions[i];
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = { static_cast<uint32_t>(textures_width[i]), static_cast<uint32_t>(textures_height[i]), 1 };

        vkCmdCopyBufferToImage(
            frame_data_.cmd_buffer[0],
            scene_.stage_buffer,
            scene_.texture_images[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &regions[i]
        );
    }


    // Transition Transfer Dst -> Shader Read Only
    for (auto& barrier : barriers)
    {
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    vkCmdPipelineBarrier(
        frame_data_.cmd_buffer[0],
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data()
    );

    vkEndCommandBuffer(frame_data_.cmd_buffer[0]);

    // Submit and wait (Synchronous upload for boilerplate simplicity)
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame_data_.cmd_buffer[0];

    vkQueueSubmit(context_.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphics_queue);

    for (size_t i = 0; i < texture_count; i++)
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = scene_.texture_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        vkCreateImageView(context_.device, &view_info, nullptr, &scene_.texture_image_views[i]);
    }

    // Quick query for max anisotropy based on your device features
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(context_.phys_device, &properties);

    // Create sampler
    // @todo: should normal, ambient occlusion, and other use the same sampler?
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(context_.device, &sampler_info, nullptr, &scene_.texture_sampler);

    // @todo: textures are not modified by the CPU across frames. Therefore, frame in flight is not necessary here.
    std::array<VkWriteDescriptorSet, texture_count> write_desc_sets = {};
    for (auto& descriptor_set : frame_data_.descriptor_sets)
    {
        for (size_t i = 0; i < texture_count; i++)
        {
            uint32_t texture_index = i;

            VkDescriptorImageInfo desc_image_info = {};
            desc_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            desc_image_info.imageView = scene_.texture_image_views[i];
            desc_image_info.sampler = scene_.texture_sampler;

            VkWriteDescriptorSet& write_desc = write_desc_sets[i];
            write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc.dstSet = descriptor_set;
            write_desc.dstBinding = 2; // Assuming bindless array is at binding = 2
            write_desc.dstArrayElement = texture_index;
            write_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc.descriptorCount = 1;
            write_desc.pImageInfo = &desc_image_info;
        }

        vkUpdateDescriptorSets(context_.device, write_desc_sets.size(), write_desc_sets.data(), 0, nullptr);
    }
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
    app_info.apiVersion = VK_MAKE_VERSION(1, 3, 0);         // Vulkan version must be > 1.2 to enable VK_EXT_descriptor_indexing, VK_KHR_shader_draw_parameters, VK_KHR_dynamic_rendering.

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    // #ifdef _DEBUG
    // create_info.pNext = &DEBUG_UTILS_MESSENGER_CREATE_INFO;
    // #endif
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(std::size(layers));
    create_info.ppEnabledLayerNames = layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
    create_info.ppEnabledExtensionNames = extensions.data();

    vkCreateInstance(&create_info, nullptr, &context_.instance);
}

void Renderer::CreateSurface(SDL_Window *window)
{
    SDL_Vulkan_CreateSurface(window, context_.instance, &context_.surface);
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

    std::array<const char*, 1> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};   // Provides access to three additional built-in shader variables in Vulkan (Indirect drawing command)

    // Selecting physical device.
    uint32_t phys_device_count = 0;
    vkEnumeratePhysicalDevices(context_.instance, &phys_device_count, nullptr);

    std::vector<VkPhysicalDevice> phys_devices(phys_device_count);  // We can list up to four physical devices.
    vkEnumeratePhysicalDevices(context_.instance, &phys_device_count, phys_devices.data());

    for (auto phys_device : phys_devices)
    {
        VkPhysicalDeviceFeatures phys_device_features = {};
        vkGetPhysicalDeviceFeatures(phys_device, &phys_device_features);

        uint32_t phys_device_ext_count = {};
        vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &phys_device_ext_count, nullptr);

        std::vector<VkExtensionProperties> phys_device_exts(phys_device_ext_count);
        vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &phys_device_ext_count, phys_device_exts.data());

        std::unordered_set<std::string> exts_supported = {};
        for (const auto& ext : phys_device_exts)
        {
            exts_supported.insert(ext.extensionName);
        }

        bool are_exts_supported = true;
        for (const char* ext : device_extensions)
        {
            if (!exts_supported.contains(ext))
            {
                are_exts_supported = false;
            }
        }

        VkPhysicalDeviceProperties phys_device_properties = {};
        vkGetPhysicalDeviceProperties(phys_device, &phys_device_properties);

        const bool is_suitable =
            required_features.geometryShader == phys_device_features.geometryShader &&
            required_features.tessellationShader == phys_device_features.tessellationShader &&
            required_features.multiDrawIndirect == phys_device_features.multiDrawIndirect &&
            required_features.fillModeNonSolid == phys_device_features.fillModeNonSolid &&
            required_features.sampleRateShading == phys_device_features.sampleRateShading &&
            required_features.samplerAnisotropy == phys_device_features.samplerAnisotropy;

        if (are_exts_supported && is_suitable && phys_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            context_.phys_device = phys_device;
            std::println("VK: {}", phys_device_properties.deviceName);
        }

        // Elsewhere the context_.phys_device will be VK_NULL_HANDLE and the application will crash.
    }

    // Find queues
    uint32_t queue_family_count = {};
    vkGetPhysicalDeviceQueueFamilyProperties(context_.phys_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.phys_device, &queue_family_count, queue_family_properties.data());

    for (uint32_t i = 0; i < queue_family_count; i++)
    {
        VkBool32 support_presentation = {};
        vkGetPhysicalDeviceSurfaceSupportKHR(context_.phys_device, i, context_.surface, &support_presentation);

        if (context_.graphics_queue_family_index == 0 && queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && support_presentation)
        {
            context_.graphics_queue_family_index = i;
            continue;   // We want dedicated family for each type of queue. Therefor, skip as soon we find the queue.
        }

        // if (context_.transfer_queue_family_index == 0 && queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        // {
        //     context_.transfer_queue_family_index = i;
        // }
    }

    constexpr std::array<float, 1> priorities = { 1.0f };
    std::array<VkDeviceQueueCreateInfo, 1> queue_create_info = {};
    // Graphics + Presentation queue
    queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info[0].queueFamilyIndex = context_.graphics_queue_family_index;
    queue_create_info[0].queueCount = 1;
    queue_create_info[0].pQueuePriorities = priorities.data();
    // // Transfer queue
    // queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    // queue_create_info[1].queueFamilyIndex = context_.transfer_queue_family_index;
    // queue_create_info[1].queueCount = 1;
    // queue_create_info[1].pQueuePriorities = priorities.data();

    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12_features.descriptorBindingPartiallyBound = VK_TRUE;
    vk12_features.runtimeDescriptorArray = VK_TRUE;
    vk12_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vk12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vk12_features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.flags = 0;
    device_create_info.pNext = &vk12_features;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_info.size());
    device_create_info.pQueueCreateInfos = queue_create_info.data();
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.pEnabledFeatures = &required_features;

    vkCreateDevice(context_.phys_device, &device_create_info, nullptr, &context_.device);
}

void Renderer::CreateSwapchain(VkSwapchainKHR old_swapchain)
{
    // Getting the extents directly from the surface capabilities, return
    // correctly on windows platform. Not sure about other platforms

    VkSurfaceCapabilitiesKHR caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.phys_device, context_.surface, &caps);

    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.phys_device, context_.surface, &surface_format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.phys_device, context_.surface, &surface_format_count, surface_formats.data());

    // Find the suitable surface format
    VkSurfaceFormatKHR select_surface_format = {};
    for (VkSurfaceFormatKHR surface_format : surface_formats)
    {
        // Preferred surface format
        if (surface_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            select_surface_format = surface_format;
            break;
        }
    }

    if (select_surface_format.format == VK_FORMAT_UNDEFINED)
    {
        select_surface_format = surface_formats[0];
    }

    constexpr VkPresentModeKHR present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    swapchain_.surface_format = select_surface_format;
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
    create_info.oldSwapchain = old_swapchain;

    vkCreateSwapchainKHR(context_.device, &create_info, nullptr, &swapchain_.swapchain);

    // Init images
    uint32_t swapchain_image_count = {};
    vkGetSwapchainImagesKHR(context_.device, swapchain_.swapchain, &swapchain_image_count, nullptr);
    swapchain_.images.resize(swapchain_image_count);
    swapchain_.image_views.resize(swapchain_image_count);
    swapchain_.renderer_finished_semaphores.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(context_.device, swapchain_.swapchain, &swapchain_image_count, swapchain_.images.data());

    // Init image views
    for (uint32_t i = 0; i < swapchain_image_count; i++)
    {
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
        vkCreateImageView(context_.device, &image_view_create_info, nullptr, &swapchain_.image_views[i]);
    }

    // Create synchronization objects
    for (uint32_t i = 0; i < create_info.minImageCount; i++)
    {
        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_create_info.flags = 0;
        vkCreateSemaphore(context_.device, &semaphore_create_info, nullptr, &swapchain_.renderer_finished_semaphores[i]);
    }
}

void Renderer::CreateFif()
{
    VkCommandPoolCreateInfo cmd_pool_create_info = {};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = context_.graphics_queue_family_index;

    for (size_t i = 0; i < kMaxFifCount; i++)
    {
        VkCommandPool &cmd_pool = frame_data_.cmd_pool[i];
        vkCreateCommandPool(context_.device, &cmd_pool_create_info, nullptr, &cmd_pool);

        VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {};
        cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buffer_alloc_info.commandPool = cmd_pool;
        cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buffer_alloc_info.commandBufferCount = 1;
        vkAllocateCommandBuffers(context_.device, &cmd_buffer_alloc_info, &frame_data_.cmd_buffer[i]);

        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_create_info.flags = 0;
        vkCreateSemaphore(context_.device, &semaphore_create_info, nullptr, &frame_data_.acquired_image_semaphores[i]);

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(context_.device, &fence_create_info, nullptr, &frame_data_.submit_fences[i]);
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

    if (selected_type == ~0u)
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
    image_view_create_infos[0].image = swapchain_.sample_color_image;
    image_view_create_infos[0].viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_infos[0].format = swapchain_.surface_format.format;
    image_view_create_infos[0].components = component_swizzle;
    image_view_create_infos[0].subresourceRange = subresource_ranges[0];
    // Depth + Stencil
    image_view_create_infos[1].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_infos[1].pNext = nullptr;
    image_view_create_infos[1].flags = 0;
    image_view_create_infos[1].image = swapchain_.depth_stencil_image;
    image_view_create_infos[1].viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_infos[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    image_view_create_infos[1].components = component_swizzle;
    image_view_create_infos[1].subresourceRange = subresource_ranges[1];

    // 1: Sample color attachment.
    // 2: Depth stencil.
    // We need only one for each type, because they will be discarded on re-use.
    std::array<VkImage*, 2> images = { &swapchain_.sample_color_image, &swapchain_.depth_stencil_image };
    std::array<VkImageView*, 2> image_views = {&swapchain_.sample_color_image_view, &swapchain_.depth_stencil_image_view};
    std::array<VkDeviceMemory*, 2> mems = { &swapchain_.sample_color_mem, &swapchain_.depth_stencil_mem };

    for (size_t i = 0; i < images.size(); i++)
    {
        vkCreateImage(context_.device, &image_create_infos[i], nullptr, images[i]);
        image_view_create_infos[i].image = *images[i];

        VkMemoryRequirements mem_requirements = {};
        vkGetImageMemoryRequirements(context_.device, *images[i], &mem_requirements);

        const uint32_t memory_type = ChooseHeapFromFlags(
            mem_requirements,
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.allocationSize = mem_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        vkAllocateMemory(context_.device, &allocate_info, nullptr, mems[i]);
        vkBindImageMemory(context_.device, *images[i], *mems[i], 0);
        vkCreateImageView(context_.device, &image_view_create_infos[i], nullptr, image_views[i]);
    }
}

void Renderer::PrepareDepthStencil()
{
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame_data_.cmd_buffer[0], &cmd_buffer_begin_info);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.image = swapchain_.depth_stencil_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(frame_data_.cmd_buffer[0],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);

    vkEndCommandBuffer(frame_data_.cmd_buffer[0]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame_data_.cmd_buffer[0];

    vkQueueSubmit(context_.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphics_queue);
}

void Renderer::CreateUbo()
{
    constexpr VkDeviceSize buffer_size = sizeof(frame_data_);

    for (uint32_t i = 0; i < kMaxFifCount; i++)
    {
        VkBufferCreateInfo buffer_create_info = {};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.flags = 0;
        buffer_create_info.size = buffer_size;
        buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        vkCreateBuffer(context_.device, &buffer_create_info, nullptr, &frame_data_.ubo_buffer[i]);

        VkMemoryRequirements mem_requirements = {};
        vkGetBufferMemoryRequirements(context_.device, frame_data_.ubo_buffer[i], &mem_requirements);

        const uint32_t mem_type = ChooseHeapFromFlags(mem_requirements, mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo buffer_memory_allocate_info = {};
        buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        buffer_memory_allocate_info.pNext = nullptr;
        buffer_memory_allocate_info.allocationSize = mem_requirements.size;
        buffer_memory_allocate_info.memoryTypeIndex = mem_type;

        vkAllocateMemory(context_.device, &buffer_memory_allocate_info, nullptr, &frame_data_.ubo_mem[i]);
        vkBindBufferMemory(context_.device, frame_data_.ubo_buffer[i], frame_data_.ubo_mem[i], 0);
    }
}

void Renderer::CreateRenderPass()
{
    constexpr VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_4_BIT;

    VkAttachmentDescription color_attachment = {};
    color_attachment.flags = 0;
    color_attachment.format = swapchain_.surface_format.format;
    color_attachment.samples = sample_count;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription color_resolver_attachment = {};
    color_resolver_attachment.flags = 0;
    color_resolver_attachment.format = swapchain_.surface_format.format;
    color_resolver_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_resolver_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_resolver_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_resolver_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_resolver_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_resolver_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_resolver_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_resolver_attachment_ref = {};
    color_resolver_attachment_ref.attachment = 2;
    color_resolver_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth + stencil
    VkAttachmentDescription depth_attachment = {};
    depth_attachment.flags = 0;
    depth_attachment.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    depth_attachment.samples = sample_count;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pResolveAttachments = &color_resolver_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    const std::array<VkAttachmentDescription, 3> attachment_descs = {
        color_attachment,
        depth_attachment,
        color_resolver_attachment,
    };

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.pNext = nullptr;
    render_pass_create_info.flags = 0;
    render_pass_create_info.attachmentCount = attachment_descs.size();
    render_pass_create_info.pAttachments = attachment_descs.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    vkCreateRenderPass(context_.device, &render_pass_create_info, nullptr, &pipeline_.render_pass);
}

void Renderer::CreateFramebuffers()
{
    swapchain_.framebuffers.resize(swapchain_.images.size());

    for (size_t i = 0; i < swapchain_.images.size(); i++)
    {
        const VkImageView attachments[3] = {
            swapchain_.sample_color_image_view,   // Multisample
            swapchain_.depth_stencil_image_view,
            swapchain_.image_views[i],              // Multisample resolver to 1 sample.
        };

        VkFramebufferCreateInfo framebuffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipeline_.render_pass,
            .attachmentCount = 3,
            .pAttachments = &attachments[0],
            .width = swapchain_.extent.width,
            .height = swapchain_.extent.height,
            .layers = 1,
        };

        vkCreateFramebuffer(context_.device, &framebuffer_create_info, nullptr, &swapchain_.framebuffers[i]);
    }
}

void Renderer::CreatePipeline()
{
    std::vector<char> vert_shader_code = FileSystem::ReadFile("../resources/shaders/indirect_vert.spv");
    std::vector<char> frag_shader_code = FileSystem::ReadFile("../resources/shaders/frag.spv");

    std::array<VkShaderModule, 2> shader_modules = {};

    VkShaderModuleCreateInfo vertex_module_create_info = {};
    vertex_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_module_create_info.flags = 0;
    vertex_module_create_info.codeSize = static_cast<uint32_t>(vert_shader_code.size());
    vertex_module_create_info.pCode = reinterpret_cast<const uint32_t*>(vert_shader_code.data());

    VkShaderModuleCreateInfo frag_module_create_info = {};
    frag_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_module_create_info.flags = 0;
    frag_module_create_info.codeSize = static_cast<uint32_t>(frag_shader_code.size());
    frag_module_create_info.pCode = reinterpret_cast<const uint32_t*>(frag_shader_code.data());

    vkCreateShaderModule(context_.device, &vertex_module_create_info, nullptr, &shader_modules[0]);
    vkCreateShaderModule(context_.device, &frag_module_create_info, nullptr, &shader_modules[1]);

    VkPipelineShaderStageCreateInfo vert_stage_create_info = {};
    vert_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_create_info.flags = 0;
    vert_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_create_info.module = shader_modules[0];
    vert_stage_create_info.pName = "main";
    vert_stage_create_info.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo frag_stage_create_info = {};
    frag_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_create_info.flags = 0;
    frag_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_create_info.module = shader_modules[1];
    frag_stage_create_info.pName = "main";
    frag_stage_create_info.pSpecializationInfo = nullptr;

    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
        vert_stage_create_info,
        frag_stage_create_info
    };

    const std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dyn_state_create_info = {};
    dyn_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state_create_info.flags = 0;
    dyn_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dyn_state_create_info.pDynamicStates = dynamic_states.data();

    std::array<VkVertexInputBindingDescription, 3> bind_descs = {};
    bind_descs[0].binding   = 0;
    bind_descs[0].stride    = sizeof(float) * 3;
    bind_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    bind_descs[1].binding   = 1;
    bind_descs[1].stride    = sizeof(float) * 3;
    bind_descs[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    bind_descs[2].binding   = 2;
    bind_descs[2].stride    = sizeof(float) * 2;
    bind_descs[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attribute_descs = {};
    // Position
    attribute_descs[0].location     = 0;
    attribute_descs[0].binding      = 0;
    attribute_descs[0].format       = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descs[0].offset       = 0;
    // Normal
    attribute_descs[1].location     = 1;
    attribute_descs[1].binding      = 1;
    attribute_descs[1].format       = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descs[1].offset       = 0;
    // UV
    attribute_descs[2].location     = 2;
    attribute_descs[2].binding      = 2;
    attribute_descs[2].format       = VK_FORMAT_R32G32_SFLOAT;
    attribute_descs[2].offset       = 0;


    VkPipelineVertexInputStateCreateInfo vertext_input_info = {};
    vertext_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertext_input_info.flags = 0;
    vertext_input_info.vertexBindingDescriptionCount = bind_descs.size();
    vertext_input_info.pVertexBindingDescriptions = bind_descs.data();
    vertext_input_info.vertexAttributeDescriptionCount = attribute_descs.size();
    vertext_input_info.pVertexAttributeDescriptions = attribute_descs.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.pNext = nullptr;
    input_assembly_info.flags = 0;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    const VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapchain_.extent.width),
        .height = static_cast<float>(swapchain_.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D scissor = {
        .offset = {0, 0},
        .extent = swapchain_.extent,
    };

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.flags = 0;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.flags = 0;
    rasterization_info.depthClampEnable = VK_FALSE;
    rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_info.depthBiasEnable = VK_FALSE;
    rasterization_info.depthBiasConstantFactor = 0.0f;
    rasterization_info.depthBiasClamp = 0.0f;
    rasterization_info.depthBiasSlopeFactor = 0.0f;
    rasterization_info.lineWidth = 1.0f;

    constexpr VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_4_BIT;

    VkPipelineMultisampleStateCreateInfo multiple_sample_info = {};
    multiple_sample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multiple_sample_info.flags = 0;
    multiple_sample_info.rasterizationSamples = sample_count;
    multiple_sample_info.sampleShadingEnable = VK_TRUE;
    multiple_sample_info.minSampleShading = 1.0f;
    multiple_sample_info.pSampleMask = nullptr;
    multiple_sample_info.alphaToCoverageEnable = VK_FALSE;
    multiple_sample_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_info = {};
    color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.flags = 0;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;
    color_blend_info.blendConstants[0] = 0.0f;
    color_blend_info.blendConstants[1] = 0.0f;
    color_blend_info.blendConstants[2] = 0.0f;
    color_blend_info.blendConstants[3] = 0.0f;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
    // Ubo
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // Ssbo
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // Bindless texture array
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 100;  // @todo: temporary constant max texture array.
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorBindingFlags, 3> binding_flags = {};
    binding_flags[0] = 0;   // ubo
    binding_flags[1] = 0;   // ssbo
    binding_flags[2] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo layout_flags = {};
    layout_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    layout_flags.bindingCount = binding_flags.size();
    layout_flags.pBindingFlags = binding_flags.data();

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_info.pNext = &layout_flags;
    layout_info.bindingCount = bindings.size();
    layout_info.pBindings = bindings.data();

    vkCreateDescriptorSetLayout(context_.device, &layout_info, nullptr, &pipeline_.descriptor_set_layout);

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.flags = 0;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &pipeline_.descriptor_set_layout;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = nullptr;

    vkCreatePipelineLayout(context_.device, &pipeline_layout_create_info, nullptr, &pipeline_.pipeline_layout);

    VkStencilOpState stencil_op = {};
    stencil_op.failOp = VK_STENCIL_OP_KEEP;
    stencil_op.passOp = VK_STENCIL_OP_REPLACE;
    stencil_op.depthFailOp = VK_STENCIL_OP_KEEP;
    stencil_op.compareOp = VK_COMPARE_OP_ALWAYS;
    stencil_op.compareMask = 0xFF;
    stencil_op.writeMask = 0xFF;
    stencil_op.reference = 1;

    // Depth + Stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
    depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state_create_info.stencilTestEnable = VK_TRUE;
    depth_stencil_state_create_info.front = stencil_op;
    depth_stencil_state_create_info.back = stencil_op;
    depth_stencil_state_create_info.minDepthBounds = 0.0f;
    depth_stencil_state_create_info.maxDepthBounds = 1.0f;

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
    graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create_info.flags = 0;
    graphics_pipeline_create_info.stageCount = 2;
    graphics_pipeline_create_info.pStages = shader_stages.data();
    graphics_pipeline_create_info.pVertexInputState = &vertext_input_info;
    graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_info;
    graphics_pipeline_create_info.pTessellationState = nullptr;
    graphics_pipeline_create_info.pViewportState = &viewport_info;
    graphics_pipeline_create_info.pRasterizationState = &rasterization_info;
    graphics_pipeline_create_info.pMultisampleState = &multiple_sample_info;
    graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
    graphics_pipeline_create_info.pColorBlendState = &color_blend_info;
    graphics_pipeline_create_info.pDynamicState = &dyn_state_create_info;
    graphics_pipeline_create_info.layout = pipeline_.pipeline_layout;
    graphics_pipeline_create_info.renderPass = pipeline_.render_pass;
    graphics_pipeline_create_info.subpass = 0;
    graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    graphics_pipeline_create_info.basePipelineIndex = -1;

    vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline_.physical_base_rendering_pipeline);

    std::array<VkDescriptorPoolSize, 3> pool_sizes = {};
    // Ubo
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = kMaxFifCount;
    // Ssbo
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[1].descriptorCount = kMaxFifCount;
    // Bindless texturing
    pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[2].descriptorCount = 100 * kMaxFifCount;    // @todo: define max count of texture array.

    VkDescriptorPoolCreateInfo pool_create_info = {};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_create_info.maxSets = kMaxFifCount;
    pool_create_info.poolSizeCount = pool_sizes.size();
    pool_create_info.pPoolSizes = pool_sizes.data();

    vkCreateDescriptorPool(context_.device, &pool_create_info, nullptr, &pipeline_.descriptor_pool);

    std::array<VkDescriptorSetLayout, 2> desc_set_layouts = {};
    desc_set_layouts[0] = pipeline_.descriptor_set_layout;
    desc_set_layouts[1] = pipeline_.descriptor_set_layout;

    VkDescriptorSetAllocateInfo desc_set_allocate_info = {};
    desc_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    desc_set_allocate_info.descriptorPool = pipeline_.descriptor_pool;
    desc_set_allocate_info.descriptorSetCount = kMaxFifCount;
    desc_set_allocate_info.pSetLayouts = desc_set_layouts.data();

    vkAllocateDescriptorSets(context_.device, &desc_set_allocate_info, &frame_data_.descriptor_sets[0]);

    for (size_t i = 0; i < kMaxFifCount; i++)
    {
        VkDescriptorBufferInfo ubo_info = {};
        ubo_info.buffer = frame_data_.ubo_buffer[i];
        ubo_info.offset = 0;
        ubo_info.range = sizeof(float) * 16 * 2;

        VkDescriptorBufferInfo ssbo_info = {};
        ssbo_info.buffer = frame_data_.ssbo_buffer[0];      // Use the same buffer since the geometry doesn't change (yet).
        ssbo_info.offset = 0;
        ssbo_info.range = sizeof(SsboObjectData);

        std::array<VkWriteDescriptorSet, 2> descriptor_sets = {};
        // Ubo
        descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_sets[0].dstSet = frame_data_.descriptor_sets[i];
        descriptor_sets[0].dstBinding = 0;
        descriptor_sets[0].dstArrayElement = 0;
        descriptor_sets[0].descriptorCount = 1;
        descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_sets[0].pBufferInfo = &ubo_info;
        // Ssbo
        descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_sets[1].dstSet = frame_data_.descriptor_sets[i];
        descriptor_sets[1].dstBinding = 1;
        descriptor_sets[1].dstArrayElement = 0;
        descriptor_sets[1].descriptorCount = 1;
        descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_sets[1].pBufferInfo = &ssbo_info;

        vkUpdateDescriptorSets(context_.device, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
    }

    vkDestroyShaderModule(context_.device, shader_modules[0], nullptr);
    vkDestroyShaderModule(context_.device, shader_modules[1], nullptr);
}

void Renderer::CreateStageBuffer()
{
    VkPhysicalDeviceProperties phys_device_properties = {};
    vkGetPhysicalDeviceProperties(context_.phys_device, &phys_device_properties);

    // Allocate 500mb (vulkan best practice)
    const size_t buffer_size = Math::Align(1024 * 1024 * 512, phys_device_properties.limits.minMemoryMapAlignment);

    VkBufferCreateInfo vert_stage_buff_create_info = {};
    vert_stage_buff_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vert_stage_buff_create_info.flags = 0;
    vert_stage_buff_create_info.size = buffer_size;
    vert_stage_buff_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    vkCreateBuffer(context_.device, &vert_stage_buff_create_info, nullptr, &scene_.stage_buffer);

    VkMemoryRequirements vertex_mem_requirements = {};
    vkGetBufferMemoryRequirements(context_.device, scene_.stage_buffer, &vertex_mem_requirements);

    const uint32_t vertex_mem_type = ChooseHeapFromFlags(vertex_mem_requirements, vertex_mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    VkMemoryAllocateInfo vertex_mem_alloc_info = {};
    vertex_mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertex_mem_alloc_info.pNext = nullptr;
    vertex_mem_alloc_info.allocationSize = vertex_mem_requirements.size;
    vertex_mem_alloc_info.memoryTypeIndex = vertex_mem_type;

    vkAllocateMemory(context_.device, &vertex_mem_alloc_info, nullptr, &scene_.stage_mem);
    vkBindBufferMemory(context_.device, scene_.stage_buffer, scene_.stage_mem, 0);
}

void Renderer::CreateBatch()
{
    // In this demo we don't update the model matrix or any vertex input
    // data. Which means, we can copy and unmap all memory.
    // Only the uniform buffer with view and projection matrices are gonna
    // be updated.

    BatchCpu batch_data = {};
    MeshLoader::Load("../resources/meshes/mega_mike_z.fbx", &batch_data);

    // Cpu alignment
    VkPhysicalDeviceProperties phys_device_properties = {};
    vkGetPhysicalDeviceProperties(context_.phys_device, &phys_device_properties);
    const VkDeviceSize min_alignment = phys_device_properties.limits.minMemoryMapAlignment;

    const VkDeviceSize position_size    = sizeof(glm::vec3) * batch_data.position.size();
    const VkDeviceSize normal_size      = sizeof(glm::vec3) * batch_data.normals.size();
    const VkDeviceSize uv_size          = sizeof(glm::vec2) * batch_data.uvs.size();

    scene_.vertex_position_offset = 0;
    scene_.vertex_normal_offset = Math::Align(scene_.vertex_position_offset + position_size, min_alignment);
    scene_.vertex_uv_offset = Math::Align(scene_.vertex_normal_offset + normal_size, min_alignment);

    const VkDeviceSize vertex_buffer_size = scene_.vertex_uv_offset + uv_size;


    // Ssbo
    VkBufferCreateInfo ssbo_buffer_create_info = {};
    ssbo_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ssbo_buffer_create_info.flags = 0;
    ssbo_buffer_create_info.size = sizeof(SsboObjectData);
    ssbo_buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    vkCreateBuffer(context_.device, &ssbo_buffer_create_info, nullptr, &frame_data_.ssbo_buffer[0]);

    VkMemoryRequirements ssbo_mem_requirements = {};
    vkGetBufferMemoryRequirements(context_.device, frame_data_.ssbo_buffer[0], &ssbo_mem_requirements);

    const uint32_t mem_type = ChooseHeapFromFlags(ssbo_mem_requirements, ssbo_mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo ssbo_mem_alloc_info = {};
    ssbo_mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ssbo_mem_alloc_info.pNext = nullptr;
    ssbo_mem_alloc_info.allocationSize = ssbo_mem_requirements.size;
    ssbo_mem_alloc_info.memoryTypeIndex = mem_type;

    vkAllocateMemory(context_.device, &ssbo_mem_alloc_info, nullptr, &frame_data_.ssbo_mem[0]);
    vkBindBufferMemory(context_.device, frame_data_.ssbo_buffer[0], frame_data_.ssbo_mem[0], 0);

    void* ssbo_mapped_data = {};
    SsboObjectData ssbo_object_data = {};
    ssbo_object_data.texture_id = 0;
    ssbo_object_data.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -80.0f, 100.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    vkMapMemory(context_.device, frame_data_.ssbo_mem[0], 0, sizeof(SsboObjectData), 0, &ssbo_mapped_data);
    std::memcpy(ssbo_mapped_data, &ssbo_object_data, sizeof(SsboObjectData));
    vkUnmapMemory(context_.device, frame_data_.ssbo_mem[0]);

    // Load the mesh

    // Indirect Drawing
    VkBufferCreateInfo indirect_draw_buff_create_info = {};
    indirect_draw_buff_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indirect_draw_buff_create_info.flags = 0;
    indirect_draw_buff_create_info.size = Math::ToClosestPowerOfTwo<size_t>(sizeof(VkDrawIndexedIndirectCommand));                      // 1 model matrix
    indirect_draw_buff_create_info.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    vkCreateBuffer(context_.device, &indirect_draw_buff_create_info, nullptr, &scene_.indirect_draw_buffer);

    VkMemoryRequirements indirect_draw_mem_requirements = {};
    vkGetBufferMemoryRequirements(context_.device, scene_.indirect_draw_buffer, &indirect_draw_mem_requirements);
    const uint32_t indirect_draw_mem_type = ChooseHeapFromFlags(indirect_draw_mem_requirements, indirect_draw_mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo indirect_draw_mem_alloc_info = {};
    indirect_draw_mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    indirect_draw_mem_alloc_info.pNext = nullptr;
    indirect_draw_mem_alloc_info.allocationSize = indirect_draw_mem_requirements.size;
    indirect_draw_mem_alloc_info.memoryTypeIndex = indirect_draw_mem_type;

    vkAllocateMemory(context_.device, &indirect_draw_mem_alloc_info, nullptr, &scene_.indirect_draw_mem);
    vkBindBufferMemory(context_.device, scene_.indirect_draw_buffer, scene_.indirect_draw_mem, 0);

    VkDrawIndexedIndirectCommand indirect_draw_cmd = {};
    indirect_draw_cmd.indexCount = batch_data.indices.size();
    indirect_draw_cmd.instanceCount = 1;
    indirect_draw_cmd.firstIndex = 0;
    indirect_draw_cmd.vertexOffset = 0;
    indirect_draw_cmd.firstInstance = 0;

    void* indirect_draw_mapped_data = {};
    vkMapMemory(context_.device, scene_.indirect_draw_mem, 0, sizeof(VkDrawIndexedIndirectCommand), 0, &indirect_draw_mapped_data);
    std::memcpy(indirect_draw_mapped_data, &indirect_draw_cmd, sizeof(VkDrawIndexedIndirectCommand));
    vkUnmapMemory(context_.device, scene_.indirect_draw_mem);

    // Copy into the stage buffer previously created.
    void* vertex_stage_mapped_data = {};
    vkMapMemory(context_.device, scene_.stage_mem, 0, vertex_buffer_size, 0, &vertex_stage_mapped_data);
    char* base_ptr = static_cast<char*>(vertex_stage_mapped_data);
    std::memcpy(base_ptr + scene_.vertex_position_offset, batch_data.position.data(), position_size);
    std::memcpy(base_ptr + scene_.vertex_normal_offset, batch_data.normals.data(), normal_size);
    std::memcpy(base_ptr + scene_.vertex_uv_offset, batch_data.uvs.data(), uv_size);
    vkUnmapMemory(context_.device, scene_.stage_mem);


    // Vertex Local buffer
    VkBufferCreateInfo vert_local_buff_create_info = {};
    vert_local_buff_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vert_local_buff_create_info.flags = 0;
    vert_local_buff_create_info.size = Math::Align(vertex_buffer_size, min_alignment);
    vert_local_buff_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vkCreateBuffer(context_.device, &vert_local_buff_create_info, nullptr, &scene_.vertex_buffer);

    VkMemoryRequirements vertex_local_mem_requirements = {};
    vkGetBufferMemoryRequirements(context_.device, scene_.vertex_buffer, &vertex_local_mem_requirements);

    const uint32_t vertex_local_mem_type = ChooseHeapFromFlags(vertex_local_mem_requirements, vertex_local_mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo vertex_local_mem_alloc_info = {};
    vertex_local_mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertex_local_mem_alloc_info.pNext = nullptr;
    vertex_local_mem_alloc_info.allocationSize = vertex_local_mem_requirements.size;
    vertex_local_mem_alloc_info.memoryTypeIndex = vertex_local_mem_type;

    vkAllocateMemory(context_.device, &vertex_local_mem_alloc_info, nullptr, &scene_.vertex_mem);
    vkBindBufferMemory(context_.device, scene_.vertex_buffer, scene_.vertex_mem, 0);


    // Index buffer
    VkBufferCreateInfo index_buff_create_info = {};
    index_buff_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buff_create_info.flags = 0;
    index_buff_create_info.size = batch_data.indices.size() * sizeof(uint32_t);
    index_buff_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vkCreateBuffer(context_.device, &index_buff_create_info, nullptr, &scene_.index_buffer);

    VkMemoryRequirements index_mem_requirements = {};
    vkGetBufferMemoryRequirements(context_.device, scene_.index_buffer, &index_mem_requirements);
    const uint32_t index_mem_type = ChooseHeapFromFlags(index_mem_requirements, index_mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    VkMemoryAllocateInfo index_mem_alloc_info = {};
    index_mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    index_mem_alloc_info.pNext = nullptr;
    index_mem_alloc_info.allocationSize = index_mem_requirements.size;
    index_mem_alloc_info.memoryTypeIndex = index_mem_type;
    vkAllocateMemory(context_.device, &index_mem_alloc_info, nullptr, &scene_.index_mem);
    vkBindBufferMemory(context_.device, scene_.index_buffer, scene_.index_mem, 0);

    void* index_mapped_data = {};
    vkMapMemory(context_.device, scene_.index_mem, 0, batch_data.indices.size() * sizeof(uint32_t), 0, &index_mapped_data);
    std::memcpy(index_mapped_data, batch_data.indices.data(), sizeof(uint32_t) * batch_data.indices.size());
    vkUnmapMemory(context_.device, scene_.index_mem);


    // Copy stage buffer into local buffer
    VkCommandBufferBeginInfo copy_buffer_cmd_begin_info = {};
    copy_buffer_cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    copy_buffer_cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame_data_.cmd_buffer[0], &copy_buffer_cmd_begin_info);

    std::array<VkBufferMemoryBarrier, 2> copy_buffer_mem_barrier = {};
    copy_buffer_mem_barrier[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    copy_buffer_mem_barrier[0].size = Math::Align(vertex_buffer_size, min_alignment);;
    copy_buffer_mem_barrier[0].buffer = scene_.vertex_buffer;
    copy_buffer_mem_barrier[0].srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    copy_buffer_mem_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_buffer_mem_barrier[0].offset = 0;
    vkCmdPipelineBarrier(frame_data_.cmd_buffer[0], VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &copy_buffer_mem_barrier[0], 0, nullptr);

    VkBufferCopy copy_buffer = {};
    copy_buffer.srcOffset = 0;
    copy_buffer.dstOffset = 0;
    copy_buffer.size = Math::Align(vertex_buffer_size, min_alignment);;
    vkCmdCopyBuffer(frame_data_.cmd_buffer[0], scene_.stage_buffer, scene_.vertex_buffer, 1, &copy_buffer);

    copy_buffer_mem_barrier[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    copy_buffer_mem_barrier[1].size = Math::Align(vertex_buffer_size, min_alignment);;
    copy_buffer_mem_barrier[1].buffer = scene_.vertex_buffer;
    copy_buffer_mem_barrier[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_buffer_mem_barrier[1].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    copy_buffer_mem_barrier[1].offset = 0;
    vkCmdPipelineBarrier(frame_data_.cmd_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &copy_buffer_mem_barrier[1], 0, nullptr);
    vkEndCommandBuffer(frame_data_.cmd_buffer[0]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame_data_.cmd_buffer[0];

    vkQueueSubmit(context_.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphics_queue);
}
