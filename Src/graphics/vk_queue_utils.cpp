//
// Created by apant on 02/04/2026.
//

#include "vk_queue_utils.h"

#include "vk_core_utils.h"

namespace vk_utils {

// Converts a std::optional to std::expected, supplying the error
// value only at the final boundary. This keeps the internal chain
// in optional-land and avoids repeating the error type everywhere.
template<typename E>
[[nodiscard]] std::expected<uint32_t, E> ToExpected(
    std::optional<uint32_t> opt, E err)
{
    if (opt) return *opt;
    return std::unexpected(err);
}

// First family whose flags contain all bits of `required`.
[[nodiscard]] std::optional<uint32_t> FindFirst(
    std::span<const VkQueueFamilyProperties> families,
    VkQueueFlags required)
{
    for (uint32_t i = 0; i < families.size(); ++i)
        if ((families[i].queueFlags & required) == required)
            return i;

    return std::nullopt;
}

// First family that has `required` but NOT `excluded` — i.e. a
// separate/async family. Falls through to nullopt if none found.
[[nodiscard]] std::optional<uint32_t> FindSeparate(
    std::span<const VkQueueFamilyProperties> families,
    VkQueueFlags required,
    VkQueueFlags excluded)
{
    for (uint32_t i = 0; i < families.size(); ++i)
    {
        const VkQueueFlags flags = families[i].queueFlags;
        if ((flags & required) == required && (flags & excluded) == 0)
            return i;
    }

    return std::nullopt;
}

// Family that has `required` and only `required` — truly dedicated.
[[nodiscard]] std::optional<uint32_t> FindDedicated(
    std::span<const VkQueueFamilyProperties> families,
    VkQueueFlags required,
    VkQueueFlags excluded)
{
    for (uint32_t i = 0; i < families.size(); ++i)
    {
        const VkQueueFlags flags = families[i].queueFlags;
        if (flags == required && (flags & excluded) == 0)
            return i;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<uint32_t> FindPresent(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    std::span<const VkQueueFamilyProperties> families)
{
    for (uint32_t i = 0; i < families.size(); ++i)
    {
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, surface, &supported);
        if (supported == VK_TRUE) return i;
    }

    return std::nullopt;
}

std::expected<uint32_t, QueueError> GetPresentQueueIndex(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    std::span<const VkQueueFamilyProperties> families)
{
    return ToExpected(
        FindPresent(phys_device, surface, families),
        QueueError::kPresentUnavailable);
}

std::expected<uint32_t, QueueError> GetGraphicsQueueIndex(
    std::span<const VkQueueFamilyProperties> families)
{
    return ToExpected(
        FindFirst(families, VK_QUEUE_GRAPHICS_BIT),
        QueueError::kGraphicsUnavailable);
}

// Compute: prefer a separate family (no transfer), fall back to any
// family with compute support.
std::expected<uint32_t, QueueError> GetComputeQueueIndex(
    std::span<const VkQueueFamilyProperties> families)
{
    return ToExpected(
        FindSeparate(families, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT)
            .or_else([&]() {
                return FindFirst(families, VK_QUEUE_COMPUTE_BIT);
            }),
        QueueError::kComputeUnavailable);
}

// Transfer: prefer a separate family (no compute), fall back to any
// family with transfer support.
std::expected<uint32_t, QueueError> GetTransferQueueIndex(
    std::span<const VkQueueFamilyProperties> families)
{
    return ToExpected(
        FindSeparate(families, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT)
            .or_else([&]() {
                return FindFirst(families, VK_QUEUE_TRANSFER_BIT);
            }),
        QueueError::kTransferUnavailable);
}

std::expected<uint32_t, QueueError> GetDedicatedComputeQueueIndex(
    std::span<const VkQueueFamilyProperties> families)
{
    return ToExpected(
        FindDedicated(families, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT),
        QueueError::kComputeUnavailable);
}

std::expected<uint32_t, QueueError> GetDedicatedTransferQueueIndex(
    std::span<const VkQueueFamilyProperties> families)
{
    return ToExpected(
        FindDedicated(families, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT),
        QueueError::kTransferUnavailable);
}

}