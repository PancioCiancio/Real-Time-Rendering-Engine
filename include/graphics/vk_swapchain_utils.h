//
// Created by apant on 02/04/2026.
//


#ifndef ORDA_VK_SWAPCHAIN_UTILS_H
#define ORDA_VK_SWAPCHAIN_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <optional>
#include <vector>

namespace vk_utils {

// Swapchain Error types
enum class SwapchainError
{
    kSurfaceCapabilitiesFailed,
    kCreationFailed,
    kImageQueryFailed
};

struct SwapchainDesc
{
    VkSurfaceFormatKHR surface_format = {};
    VkExtent2D extent = {};

    // If nullopt, MAILBOX is attempted, FIFO used as fallback.
    // FIFO is the only mode guaranteed to be available.
    std::optional<VkPresentModeKHR> preferred_present_mode;

    // Pass the previous swapchain handle during resize/recreate.
    // Vulkan may reuse resources from it. Pass VK_NULL_HANDLE (default) on
    // first creation.
    VkSwapchainKHR old_swapchain;
};

struct SwapchainResult
{
    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    uint32_t image_count;
};

[[nodiscard]] std::expected<SwapchainResult, SwapchainError> CreateSwapchain(
    VkDevice device,
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    const SwapchainDesc& desc);

[[nodiscard]] std::expected<std::vector<VkImage>, SwapchainError> GetSwapchainImages(
    VkDevice device,
    VkSwapchainKHR swapchain);

}

#endif //ORDA_VK_SWAPCHAIN_UTILS_H
