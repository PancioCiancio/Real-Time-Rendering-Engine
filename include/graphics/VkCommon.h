//
// Created by apant on 02/08/2025.
//

#ifndef COMMON_H
#define COMMON_H

#include <Volk/volk.h>
#include <cstdint>

namespace Renderer
{
/// Wrap most common vulkan calls (e.g. VkCreateInstance, VkCreateBuffer, VkCreateImage, ...).
/// To delete vulkan resources, you must do the normal vulkan calls (e.g. vkDestroyInstance, ...).

// ==========================
// Physical Device
// ==========================

#pragma region VkPhysicalDevice
void vk_query_sample_counts(
    VkPhysicalDevice gpu,
    VkSampleCountFlagBits *p_sample);

void vk_query_supported_format(
    VkPhysicalDevice gpu,
    uint32_t requested_format_count,
    const VkFormat *p_requested_formats,
    VkImageTiling tiling,
    VkFormatFeatureFlags features,
    VkFormat *p_format);


/// @todo Sketch implementation. Refactor it...
/// Go through every memory type to look what fit the request.
uint32_t vk_query_memory_type_idx(
    uint32_t type_filter,
    VkMemoryPropertyFlags memory_property_flags,
    const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties);
#pragma endregion


// ==========================
// Swapchain
// ==========================

#pragma region VkSwapchainKHR
void vk_create_swapchain(
    VkDevice device,
    VkSurfaceKHR surface,
    VkSurfaceFormatKHR *p_format,
    VkSurfaceCapabilitiesKHR *p_capabilities,
    VkSwapchainKHR old_swapchain,
    VkAllocationCallbacks *p_allocator,
    VkSwapchainKHR *p_swapchain);

void vk_query_swapchain_images(
    VkDevice device,
    VkSwapchainKHR swapchain,
    VkImage *p_swapchain_images);
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
    VkDeviceMemory *p_memory);

/// @warning	Provided buffer must be valid.
void vk_create_buffer_view(
    VkDevice device,
    VkBuffer buffer,
    VkFormat format,
    VkDeviceSize offset,
    VkDeviceSize range,
    VkAllocationCallbacks *p_allocator,
    VkBufferView *p_buffer_view);
#pragma endregion

}

#endif //COMMON_H
