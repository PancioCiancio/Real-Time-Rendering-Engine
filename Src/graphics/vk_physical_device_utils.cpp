// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#include "vk_physical_device_utils.h"

#include <ranges>
#include <algorithm>
#include <cstring>
#include <vector>

#include "vk_core_utils.h"

namespace vk_utils {

[[nodiscard]] bool EvaluateExtensions(
    std::span<const char* const> required,
    std::span<const VkExtensionProperties> available)
{
    return std::ranges::all_of(required, [&](const char* req)
    {
        return std::ranges::any_of(available, [req](const VkExtensionProperties& prop)
        {
            return std::strcmp(prop.extensionName, req) == 0;
        });
    });
}

[[nodiscard]] bool EvaluateFeatures(
    const VkPhysicalDeviceFeatures& required,
    const VkPhysicalDeviceFeatures& available)
{
    constexpr size_t field_count = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

    const auto* req = reinterpret_cast<const VkBool32*>(&required);
    const auto* sup = reinterpret_cast<const VkBool32*>(&available);

    // Fall if any feature requested is not supported.
    for (size_t i = 0; i < field_count; i++)
    {
        if (req[i] == VK_TRUE && sup[i] == VK_FALSE)
        {
            return false;
        }
    }

    return true;
}

std::expected<VkPhysicalDevice, VkResult> CreatePhysicalDevice(
    VkInstance instance,
    std::span<const char* const> extensions,
    const VkPhysicalDeviceFeatures& features)
{
    // Retrieve the number of physical devices
    uint32_t phys_device_count = 0;
    VK_TRY(vkEnumeratePhysicalDevices(
        instance,
        &phys_device_count,
        nullptr));  // Array of VkPhysicalDevice

    // Fill up the physical devices
    std::vector<VkPhysicalDevice> phys_devices(phys_device_count);
    VK_TRY(vkEnumeratePhysicalDevices(
        instance,
        &phys_device_count,
        phys_devices.data()));

    // Fallback device in case a discrete gpu is not present
    VkPhysicalDevice fallback_phys_device = {};

    for (VkPhysicalDevice phys_device : phys_devices)
    {
        uint32_t phys_device_extension_count = 0;
        VK_TRY(vkEnumerateDeviceExtensionProperties(
            phys_device,
            nullptr, // const char* pLayerName
            &phys_device_extension_count,
            nullptr)); // VkExtensionProperties

        std::vector<VkExtensionProperties> phys_device_ext_properties(phys_device_extension_count);
        VK_TRY(vkEnumerateDeviceExtensionProperties(
            phys_device,
            nullptr, // const char* pLayerName
            &phys_device_extension_count,
            phys_device_ext_properties.data()));

        if (!EvaluateExtensions(extensions, phys_device_ext_properties))
        {
            return std::unexpected(VK_ERROR_EXTENSION_NOT_PRESENT);
        }

        VkPhysicalDeviceFeatures supported_features = {};
        vkGetPhysicalDeviceFeatures(phys_device, &supported_features);

        if (!EvaluateFeatures(features, supported_features))
        {
            return std::unexpected(VK_ERROR_FEATURE_NOT_PRESENT);
        }

        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(phys_device, &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            return phys_device;
        }
        else
        {
            fallback_phys_device = phys_device;
        }
    }

    if (fallback_phys_device != VK_NULL_HANDLE)
    {
        return fallback_phys_device;
    }
    else
    {
        return std::unexpected(VK_ERROR_INITIALIZATION_FAILED);
    }
}

std::expected<VkSampleCountFlagBits, VkResult> FindMaxSampleCount(
    VkPhysicalDevice phys_device)
{
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(phys_device, &properties);

    // Get the maximum sample counts supported across all type
    // of framebuffers.
    const uint32_t sample_counts =
        properties.limits.framebufferColorSampleCounts &
        properties.limits.framebufferDepthSampleCounts &
        properties.limits.framebufferStencilSampleCounts;

    // List all count bit. You can see it in the original vulkan code.
    constexpr std::array<VkSampleCountFlagBits, 7> sample_list = {
        VK_SAMPLE_COUNT_64_BIT,
        VK_SAMPLE_COUNT_32_BIT,
        VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_2_BIT,
        VK_SAMPLE_COUNT_1_BIT,
    };

    // Return the highest sample count bit that matches the limits.
    for (const VkSampleCountFlagBits count_bit : sample_list)
    {
        if (sample_counts & count_bit)
        {
            return count_bit;
        }
    }

    // @todo return a more appropriate error code.
    return std::unexpected(VK_ERROR_INITIALIZATION_FAILED);;
}

std::expected<VkFormat, VkResult> FindFirstSupportedFormat(
    VkPhysicalDevice phys_device,
    std::span<const VkFormat> requested_formats,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    for (uint32_t i = 0; i < requested_formats.size(); i++)
    {
        VkFormatProperties properties = {};
        vkGetPhysicalDeviceFormatProperties(phys_device, requested_formats[i], &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (properties.linearTilingFeatures & features) == features)
        {
            return requested_formats[i];
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                 (properties.optimalTilingFeatures & features) == features)
        {
            return requested_formats[i];
        }
    }

    return std::unexpected(VK_ERROR_FORMAT_NOT_SUPPORTED);
}

}
