//
// Created by apant on 02/04/2026.
//


#ifndef ORDA_VK_SURFACE_UTILS_H
#define ORDA_VK_SURFACE_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <optional>

namespace vk_utils {

// Surface Error types
enum class SurfaceError
{
    kQueryFailed,
    kNoSuitableFormat
};

// Describes what the client prefers.
// Both fields are optional — if absent the function applies its own
// sensible defaults (BGRA8_SRGB + SRGB_NONLINEAR).
//
// Why let the client decide?
// HDR pipelines need RGBA16_SFLOAT + HDR10_ST2084.
// Offline/capture tools may want LINEAR color space to avoid double-gamma.
// Letting the client express a preference keeps the layer policy-free.
struct SurfaceFormatPreference
{
    std::optional<VkFormat> format;
    std::optional<VkColorSpaceKHR> color_space;
};

// Selects the best available surface format.
//
// Priority:
//   1. An exact match for both format and color_space (if preferences given).
//   2. A format-only match (color_space is less critical in SDR pipelines).
//   3. The implementation's first available format as a safe fallback.
[[nodiscard]] std::expected<VkSurfaceFormatKHR, SurfaceError> SelectSurfaceFormat(
    VkPhysicalDevice phys_device,
    VkSurfaceKHR surface,
    const SurfaceFormatPreference& preference = {});

}

#endif //ORDA_VK_SURFACE_UTILS_H
