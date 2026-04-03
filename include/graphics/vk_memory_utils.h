//
// Created by apant on 02/04/2026.
//


#ifndef ORDA_VK_MEMORY_UTILS_H
#define ORDA_VK_MEMORY_UTILS_H

#include <Volk/volk.h>
#include <expected>

namespace vk_utils {

// Memory Error types
enum class MemoryError
{
    kNoSuitableType
};

enum class ImageError
{

};

struct MemoryRequirement
{
    // type_filter comes from VkMemoryRequirements::memoryTypeBits.
    // It is a bitmask — each set bit means that memory type is compatible.
    uint32_t type_filter;
    VkMemoryPropertyFlags property_flags;
};

// Usage:
//   VkMemoryRequirements req = {};
//   vkGetBufferMemoryRequirements(device, buffer, &req);
//
//   VkPhysicalDeviceMemoryProperties mem_props = {};
//   vkGetPhysicalDeviceMemoryProperties(gpu, &mem_props);
//
//   auto idx = vkutil::FindMemoryType(
//       mem_props,
//       { req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT });
[[nodiscard]] std::expected<uint32_t, MemoryError> FindMemoryType(
    const VkPhysicalDeviceMemoryProperties& mem_props,
    const MemoryRequirement& requirement);

struct BufferResult
{
    VkBuffer buffer;
    VkDeviceMemory memory;
};


std::expected<BufferResult, VkResult> CreateBuffer(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flag_bits);

std::expected<VkBufferView, VkResult> CreateBufferView(
    VkDevice device,
    VkBuffer buffer,
    VkFormat format,
    VkDeviceSize offset,
    VkDeviceSize range);

struct ImageResult
{
    VkImage image;
    VkDeviceMemory memory;
};

[[nodiscard]] std::expected<ImageResult, ImageError> CreateImage(
    VkDevice device,
    VkPhysicalDevice phys_device,
    VkImageType image_type,
    VkFormat format,
    VkExtent3D extent,
    VkSampleCountFlagBits samples,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlagBits memory_property_flag_bits);

[[nodiscard]] std::expected<VkImageView, ImageError> CreateImageView(
    VkDevice device,
    VkImage image,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    VkFormat format,
    VkComponentMapping components);

}

#endif //ORDA_VK_MEMORY_UTILS_H
