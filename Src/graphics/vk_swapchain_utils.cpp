//
// Created by apant on 02/04/2026.
//

#include "vk_swapchain_utils.h"

#include <algorithm>
#include <vector>

#include "vk_core_utils.h"

namespace vk_utils {

[[nodiscard]] std::vector<VkPresentModeKHR> QueryPresentMode(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &count, nullptr);

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &count, modes.data());
    return modes;
}

// FIFO is guaranteed by the spec — safe unconditional fallback.
[[nodiscard]] VkPresentModeKHR SelectPresentMode(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    std::optional<VkPresentModeKHR> preferred)
{
    if (!preferred)
    {
        // Default preference: MAILBOX (low-latency, no tearing).
        // Falls back to FIFO if unavailable.
        preferred = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    const auto modes = QueryPresentMode(phys_device, surface);
    const auto it = std::ranges::find(modes, *preferred);

    return (it != modes.end()) ? *it : VK_PRESENT_MODE_FIFO_KHR;
}

// Some window systems report currentExtent as UINT32_MAX to signal
// "the extent is determined by the swapchain" — in that case we use
// the requested extent, clamped to the supported min/max.
[[nodiscard]] VkExtent2D ResolveExtent(
    const VkSurfaceCapabilitiesKHR& caps,
    VkExtent2D requested)
{
    if (caps.currentExtent.width != UINT32_MAX)
    {
        return caps.currentExtent;
    }

    return VkExtent2D {
        .width  = std::clamp(requested.width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
        .height = std::clamp(requested.height, caps.minImageExtent.height, caps.maxImageExtent.height),
    };
}

// -----------------------------------------------------------------------
// Image count
// -----------------------------------------------------------------------

// Request one more than the minimum so the CPU is less likely to stall
// waiting for the driver to release an image.
[[nodiscard]] uint32_t ResolveImageCount(const VkSurfaceCapabilitiesKHR& caps)
{
    const uint32_t desired = caps.minImageCount + 1;

    // maxImageCount == 0 means "no upper limit"
    if (caps.maxImageCount > 0)
    {
        return std::min(desired, caps.maxImageCount);
    }

    return desired;
}


// -----------------------------------------------------------------------
// create_swapchain
// -----------------------------------------------------------------------

std::expected<SwapchainResult, SwapchainError> CreateSwapchain(
    VkDevice device,
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    const SwapchainDesc& desc)
{
    VkSurfaceCapabilitiesKHR caps = {};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &caps) != VK_SUCCESS)
    {
        return std::unexpected(SwapchainError::kSurfaceCapabilitiesFailed);
    }

    const VkPresentModeKHR present_mode = SelectPresentMode(
        phys_device, surface, desc.preferred_present_mode);

    const VkExtent2D extent = ResolveExtent(caps, desc.extent);
    const uint32_t image_count = ResolveImageCount(caps);

    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.minImageCount = image_count;
    info.imageFormat = desc.surface_format.format;
    info.imageColorSpace = desc.surface_format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;                                  // >1 for stereoscopic
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;          // revisit if present != graphics
    info.preTransform = caps.currentTransform;                  // respect device orientation (mobile)
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = present_mode;
    info.clipped = VK_TRUE;                                     // discard pixels obscured by other windows
    info.oldSwapchain = desc.old_swapchain;                     // VK_NULL_HANDLE on first creation

    VkSwapchainKHR swapchain = {};
    if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS)
    {
        return std::unexpected(SwapchainError::kCreationFailed);
    }

    return SwapchainResult(swapchain, extent, image_count);
}

std::expected<std::vector<VkImage>, SwapchainError> GetSwapchainImages(
    VkDevice device,
    VkSwapchainKHR swapchain)
{
    uint32_t count = 0;
    if (vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr) != VK_SUCCESS)
    {
        return std::unexpected(SwapchainError::kImageQueryFailed);
    }

    std::vector<VkImage> images(count);
    if (vkGetSwapchainImagesKHR(device, swapchain, &count, images.data()) != VK_SUCCESS)
    {
        return std::unexpected(SwapchainError::kImageQueryFailed);
    }

    return images;
}

}