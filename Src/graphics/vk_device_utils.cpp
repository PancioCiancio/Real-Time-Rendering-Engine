// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#include "vk_device_utils.h"

#include <vector>

#include "vk_core_utils.h"

namespace vk_utils {

std::expected<VkDevice, VkResult> CreateDevice(
    VkPhysicalDevice physical_device,
    std::span<const char* const> extensions,
    const VkPhysicalDeviceFeatures& features)
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        nullptr);    // pQueueFamilyProperties;

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_family_properties.data());

    // Initialize the device with at least one queue of each family
    // listed in the physical device.
    constexpr std::array<float, 1> priorities = { 1.0f };
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos(queue_family_count);
    for (uint32_t i = 0; i < queue_family_count; i++)
    {

        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = i;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = priorities.data();

        queue_create_infos[i] = queue_create_info;
    }

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = queue_family_count;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.enabledExtensionCount = extensions.size();
    device_create_info.ppEnabledExtensionNames = extensions.data();
    device_create_info.pEnabledFeatures = &features;

    VkDevice device = {};
    VK_CHECK(vkCreateDevice(
        physical_device,
        &device_create_info,
        nullptr,
        &device));

    return device;
}


}