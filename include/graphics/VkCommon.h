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
void vk_query_supported_format(
    VkPhysicalDevice gpu,
    uint32_t requested_format_count,
    const VkFormat *p_requested_formats,
    VkImageTiling tiling,
    VkFormatFeatureFlags features,
    VkFormat *p_format);

}

#endif //COMMON_H
