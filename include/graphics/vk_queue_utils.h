//
// Created by apant on 02/04/2026.
//


#ifndef ORDA_VK_QUEUE_UTILS_H
#define ORDA_VK_QUEUE_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <span>

namespace vk_utils {

enum class QueueError
{
    kPresentUnavailable,
    kGraphicsUnavailable,
    kComputeUnavailable,
    kTransferUnavailable,
};

[[nodiscard]] std::expected<uint32_t, QueueError> GetPresentQueueIndex(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    std::span<const VkQueueFamilyProperties> families);

[[nodiscard]] std::expected<uint32_t, QueueError> GetGraphicsQueueIndex(
    std::span<const VkQueueFamilyProperties> families);

[[nodiscard]] std::expected<uint32_t, QueueError> GetComputeQueueIndex(
    std::span<const VkQueueFamilyProperties> families);

[[nodiscard]] std::expected<uint32_t, QueueError> GetTransferQueueIndex(
    std::span<const VkQueueFamilyProperties> families);


// Dedicated queue index — no fallback, fails if no exclusive family.
[[nodiscard]] std::expected<uint32_t, QueueError> GetDedicatedComputeQueueIndex(
    std::span<const VkQueueFamilyProperties> families);

// Dedicated queue index — no fallback, fails if no exclusive family.
[[nodiscard]] std::expected<uint32_t, QueueError> GetDedicatedTransferQueueIndex(
    std::span<const VkQueueFamilyProperties> families);
}

#endif //ORDA_VK_QUEUE_UTILS_H
