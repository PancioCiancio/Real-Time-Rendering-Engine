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

}