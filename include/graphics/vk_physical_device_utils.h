// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.



#ifndef ORDA_GRAPHICS_VK_PHYSICAL_DEVICE_UTILS_H
#define ORDA_GRAPHICS_VK_PHYSICAL_DEVICE_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <span>

namespace vk_utils {

// Requires valid instance handle.
// Select (create) the physical device to use for gpu workload.
std::expected<VkPhysicalDevice, VkResult> CreatePhysicalDevice(
    VkInstance instance,
    std::span<const char* const> extensions,
    const VkPhysicalDeviceFeatures& features);

// Return the highest sample count flags bits supported by the physical device,
// across all types of framebuffers (color, depth, stencil).
// In case no matches occur, it will return an error.
//
// Usage:
//  std::min(FindMaxSampleCounts(phys_device).value(), VK_SAMPLE_COUNT_4_BIT);
//
//  Or you can list all sample count bit supported:
//  std::vector<VkSampleCountFlagBits> sample_count_supported = {};
//  for (VkSampleCountFlagBits i = VK_SAMPLE_COUNT_1_BIT; i <= FindMaxSampleCounts().value(phys_device); i << i)
//  {
//      sample_count_supported.push_back(i);
//  }
std::expected<VkSampleCountFlagBits, VkResult> FindMaxSampleCount(
    VkPhysicalDevice phys_device);

// Return the first supported format in the requested_formats list.
std::expected<VkFormat, VkResult> FindFirstSupportedFormat(
    VkPhysicalDevice phys_device,
    std::span<const VkFormat> requested_formats,
    VkImageTiling tiling,
    VkFormatFeatureFlags features);
}

#endif //ORDA_GRAPHICS_VK_PHYSICAL_DEVICE_UTILS_H
