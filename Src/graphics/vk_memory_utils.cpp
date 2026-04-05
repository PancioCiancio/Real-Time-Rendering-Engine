// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#include "vk_memory_utils.h"

#include "vk_core_utils.h"

namespace vk_utils {

std::expected<uint32_t, MemoryError> FindMemoryType(
    const VkPhysicalDeviceMemoryProperties& mem_props,
    const MemoryRequirement& requirement)
{
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
    {
        // type_filter is a bitmask — bit i set means type i is compatible
        // with the resource (buffer or image).
        const bool type_compatible = (requirement.type_filter & (1u << i)) != 0;

        // The memory type must expose all property flags we asked for.
        const bool flags_match =
            (mem_props.memoryTypes[i].propertyFlags & requirement.property_flags)
            == requirement.property_flags;

        if (type_compatible && flags_match)
        {
            return i;
        }
    }

    return std::unexpected(MemoryError::kNoSuitableType);
}

std::expected<BufferResult, VkResult> CreateBuffer(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flag_bits)
{
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage_flags;

    VkBuffer buffer = {};
    VK_CHECK(vkCreateBuffer(
        device,
        &buffer_create_info,
        nullptr,                    // VkAllocator
        &buffer));

    VkMemoryRequirements mem_requirements = {};
    vkGetBufferMemoryRequirements(
        device,
        buffer,
        &mem_requirements);

    // Gpus have multiple heaps and each heap has different memory type support.
    // VkPhysicalDeviceMemoryProperties contains all memory types of every heap.
    VkPhysicalDeviceMemoryProperties memory_properties = {};
    vkGetPhysicalDeviceMemoryProperties(
        physical_device,
        &memory_properties);

    const uint32_t memory_type = FindMemoryType(
        memory_properties,
        MemoryRequirement(
            mem_requirements.memoryTypeBits,
            memory_property_flag_bits)
        ).value();

    VkMemoryAllocateInfo buffer_memory_allocate_info = {};
    buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_memory_allocate_info.pNext = nullptr;
    buffer_memory_allocate_info.allocationSize = mem_requirements.size;
    buffer_memory_allocate_info.memoryTypeIndex = memory_type;

    VkDeviceMemory memory = {};
    VK_CHECK(vkAllocateMemory(
        device,
        &buffer_memory_allocate_info,
        nullptr,                            // VkAllocationCallback
        &memory));

    VK_CHECK(vkBindBufferMemory(
        device,
        buffer,
        memory,
        0));

    return BufferResult(buffer, memory);
}

std::expected<VkBufferView, VkResult> CreateBufferView(
    VkDevice device,
    VkBuffer buffer,
    VkFormat format,
    VkDeviceSize offset,
    VkDeviceSize range)
{
    VkBufferViewCreateInfo buffer_view_create_info = {};
    buffer_view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    buffer_view_create_info.flags = 0;
    buffer_view_create_info.buffer = buffer;
    buffer_view_create_info.format = format;
    buffer_view_create_info.offset = offset;
    buffer_view_create_info.range = range;

    VkBufferView buffer_view = {};
    VK_CHECK(vkCreateBufferView(
        device,
        &buffer_view_create_info,
        nullptr,
        &buffer_view));

    return buffer_view;
}

std::expected<ImageResult, ImageError> CreateImage(
    VkDevice device,
    VkPhysicalDevice phys_device,
    VkImageType image_type,
    VkFormat format,
    VkExtent3D extent,
    VkSampleCountFlagBits samples,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlagBits memory_property_flag_bits)
{
    VkImageCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.flags = 0;
    create_info.imageType = image_type;
    create_info.format = format;
    create_info.extent = extent;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.samples = samples;
    create_info.tiling = tiling;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image = {};
    VK_CHECK(vkCreateImage(
        device,
        &create_info,
        nullptr,
        &image));

    VkMemoryRequirements mem_requirements = {};
    vkGetImageMemoryRequirements(device, image, &mem_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties = {};
    vkGetPhysicalDeviceMemoryProperties(phys_device, &memory_properties);

    const uint32_t memory_type = FindMemoryType(
        memory_properties,
        MemoryRequirement(
            mem_requirements.memoryTypeBits,
            memory_property_flag_bits)
        ).value();

    VkMemoryAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.allocationSize = mem_requirements.size;
    allocate_info.memoryTypeIndex = memory_type;

    VkDeviceMemory memory = {};
    VK_CHECK(vkAllocateMemory(
        device,
        &allocate_info,
        nullptr,    // allocator
        &memory));

    VK_CHECK(vkBindImageMemory(
        device,
        image,
        memory,
        0));

    return ImageResult(image, memory);
}

std::expected<VkImageView, ImageError> CreateImageView(
    VkDevice device,
    VkImage image,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    VkFormat format,
    VkComponentMapping components)
{
    VkImageSubresourceRange subresource_range = {};
    subresource_range.aspectMask = aspect_flags;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;

    VkImageViewCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.image = image;
    create_info.viewType = view_type;
    create_info.format = format;
    create_info.components = components;
    create_info.subresourceRange = subresource_range;

    VkImageView image_view = {};
    VK_CHECK(vkCreateImageView(
        device,
        &create_info,
        nullptr,
        &image_view));

    return image_view;
}

}
