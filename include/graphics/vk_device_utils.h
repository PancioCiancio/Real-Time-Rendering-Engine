//
// Created by apant on 02/04/2026.
//


#ifndef ORDA_VK_DEVICE_UTILS_H
#define ORDA_VK_DEVICE_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <span>

namespace vk_utils {

// Requires valid physical device handle.
// Create the device. Handle to all gpu operation (commands, queues, ...).
std::expected<VkDevice, VkResult> CreateDevice(
    VkPhysicalDevice physical_device,
    std::span<const char* const> extensions,
    const VkPhysicalDeviceFeatures& features);
}

#endif //ORDA_VK_DEVICE_UTILS_H
