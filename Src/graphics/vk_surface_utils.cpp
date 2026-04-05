// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#include "vk_surface_utils.h"

#include <vector>

#include "vk_core_utils.h"

namespace vk_utils {

std::expected<VkSurfaceFormatKHR, SurfaceError> SelectSurfaceFormat(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    const SurfaceFormatPreference& preference)
{
    uint32_t count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &count, nullptr) != VK_SUCCESS)
    {
        return std::unexpected(SurfaceError::kQueryFailed);
    }

    std::vector<VkSurfaceFormatKHR> formats(count);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &count, formats.data()) != VK_SUCCESS)
    {
        return std::unexpected(SurfaceError::kQueryFailed);
    }

    if (formats.empty())
    {
        return std::unexpected(SurfaceError::kNoSuitableFormat);
    }

    const VkFormat pref_fmt = preference.format.value_or(VK_FORMAT_B8G8R8A8_SRGB);
    const VkColorSpaceKHR pref_space = preference.color_space.value_or(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

    // Pass 1 — exact match on both format and color space
    for (const auto& f : formats)
    {
        if (f.format == pref_fmt && f.colorSpace == pref_space)
        {
            return f;
        }
    }

    // Pass 2 — format-only match
    // Color space is less critical in SDR pipelines. A wrong color space
    // causes a subtle perceptual shift, not a hard failure.
    for (const auto& f : formats)
    {
        if (f.format == pref_fmt)
        {
            return f;
        }
    }

    // Pass 3 — driver's first available format
    // Better than failing outright — let the client decide if it cares.
    return formats.front();
}

}