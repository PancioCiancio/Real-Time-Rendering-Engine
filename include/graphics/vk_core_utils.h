//
// Created by apant on 02/04/2026.
//


#ifndef ORDA_GRAPHICS_VK_CORE_UTILS_H
#define ORDA_GRAPHICS_VK_CORE_UTILS_H

#include <Volk/volk.h>
#include <string_view>
#include <source_location>
#include <print>
#include <stdexcept>

namespace vk_utils {

// Convert vulkan result into readable string
constexpr std::string_view VK_RESULT_TO_STRING(VkResult result) noexcept
{
    switch (result)
    {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            // ... add cases as needed
        default: return "VK_UNKNOWN_ERROR";
    }
}

    // VK_CHECK  — throws on failure. Use for unrecoverable paths.
    // Caller site location is captured via default parameter.
inline void VK_CHECK(
    VkResult result,
    std::source_location src_loc = std::source_location::current())
{
    if (result != VK_SUCCESS)
    {
        std::println(stderr, "[Vulkan] Error {} at {}:{} in `{}`",
            VK_RESULT_TO_STRING(result),
            src_loc.file_name(),
            src_loc.line(),
            src_loc.function_name()
        );

        throw std::runtime_error(std::string(VK_RESULT_TO_STRING(result)));
    }
}

// VK_TRY  — propagates via std::unexpected. Use inside functions
// that return std::expected<T, VkResult>.
#define VK_TRY(expr)                                    \
    do {                                                \
        const VkResult vk_result_ = (expr);             \
        if (vk_result_ != VK_SUCCESS)                   \
        {                                               \
            return std::unexpected(vk_result_);         \
        }                                               \
    } while (0)
}

#endif //ORDA_GRAPHICS_VK_CORE_UTILS_H
