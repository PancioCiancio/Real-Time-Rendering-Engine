//
// Created by apant on 02/08/2025.
//

#include "VkCommon.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "vk_core_utils.h"

namespace Renderer
{
// ==========================
// Physical Device
// ==========================

#pragma region VkPhysicalDevice

void vk_query_sample_counts(
    VkPhysicalDevice gpu,
    VkSampleCountFlagBits *p_sample)
{
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(gpu, &properties);

    const uint32_t sample_counts =
        properties.limits.framebufferColorSampleCounts &
        properties.limits.framebufferDepthSampleCounts &
        properties.limits.framebufferStencilSampleCounts;

    constexpr uint32_t sample_bits_list_count = 4;
    constexpr VkSampleCountFlagBits priority_sample_bits[sample_bits_list_count] = {
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_2_BIT,
        VK_SAMPLE_COUNT_1_BIT
    };

    bool sample_count_found = false;
    for (uint32_t i = 0; i < sample_bits_list_count && !sample_count_found; i++)
    {
        if (priority_sample_bits[i] & sample_counts)
        {
            *p_sample = priority_sample_bits[i];
            sample_count_found = true;
        }
    }
}

void vk_query_supported_format(
    VkPhysicalDevice gpu,
    uint32_t requested_format_count,
    const VkFormat *p_requested_formats,
    VkImageTiling tiling,
    VkFormatFeatureFlags features,
    VkFormat *p_format)
{
    for (uint32_t i = 0; i < requested_format_count && *p_format == VK_FORMAT_UNDEFINED; i++)
    {
        VkFormatProperties properties = {};
        vkGetPhysicalDeviceFormatProperties(gpu, p_requested_formats[i], &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (properties.linearTilingFeatures & features) == features)
        {
            *p_format = p_requested_formats[i];
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                 (properties.optimalTilingFeatures & features) == features)
        {
            *p_format = p_requested_formats[i];
        }
    }
}

uint32_t vk_query_memory_type_idx(
    uint32_t type_filter,
    VkMemoryPropertyFlags memory_property_flags,
    const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties)
{
    for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++)
    {
        const bool is_type_supported = (type_filter & (1 << i)) != 0;
        const bool has_required_properties =
            (physical_device_memory_properties.memoryTypes[i].propertyFlags &
             memory_property_flags)
            == memory_property_flags;

        if (is_type_supported && has_required_properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}
#pragma endregion


// ==========================
// Buffer
// ==========================

#pragma region VkBuffer / VkBufferView
void vk_create_buffer(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flag_bits,
    VkAllocationCallbacks *p_allocator,
    VkBuffer *p_buffer,
    VkDeviceMemory *p_memory)
{
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage_flags;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = nullptr;

    vk_utils::VK_CHECK(vkCreateBuffer(
        device,
        &buffer_create_info,
        p_allocator,
        p_buffer));

    VkMemoryRequirements mem_requirements = {};
    vkGetBufferMemoryRequirements(
        device,
        *p_buffer,
        &mem_requirements);

    // Gpus have multiple heaps and each heap has different memory type support.
    // VkPhysicalDeviceMemoryProperties contains all memory types of every heap.
    VkPhysicalDeviceMemoryProperties memory_properties = {};
    vkGetPhysicalDeviceMemoryProperties(
        physical_device,
        &memory_properties);

    const uint32_t memory_type_index = vk_query_memory_type_idx(
        mem_requirements.memoryTypeBits,
        memory_property_flag_bits,
        memory_properties);

    VkMemoryAllocateInfo buffer_memory_allocate_info = {};
    buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_memory_allocate_info.pNext = nullptr;
    buffer_memory_allocate_info.allocationSize = mem_requirements.size;
    buffer_memory_allocate_info.memoryTypeIndex = memory_type_index;

    vk_utils::VK_CHECK(vkAllocateMemory(device, &buffer_memory_allocate_info, p_allocator, p_memory));
    vk_utils::VK_CHECK(vkBindBufferMemory(device, *p_buffer, *p_memory, 0));
}

void vk_create_buffer_view(
    VkDevice device,
    VkBuffer buffer,
    VkFormat format,
    VkDeviceSize offset,
    VkDeviceSize range,
    VkAllocationCallbacks *p_allocator,
    VkBufferView *p_buffer_view)
{
    VkBufferViewCreateInfo buffer_view_create_info = {};
    buffer_view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    buffer_view_create_info.flags = 0;
    buffer_view_create_info.buffer = buffer;
    buffer_view_create_info.format = format;
    buffer_view_create_info.offset = offset;
    buffer_view_create_info.range = range;

    vk_utils::VK_CHECK(vkCreateBufferView(
        device,
        &buffer_view_create_info,
        p_allocator,
        p_buffer_view));
}

#pragma endregion

#pragma endregion
}