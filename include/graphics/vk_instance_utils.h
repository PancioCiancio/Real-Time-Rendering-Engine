#ifndef ORDA_GRAPHICS_VK_INSTANCE_UTILS_H
#define ORDA_GRAPHICS_VK_INSTANCE_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <span>

namespace vk_utils {

std::expected<VkInstance, VkResult> CreateInstance(
    std::span<const char* const> layers,
    std::span<const char* const> extensions);

}

#endif //ORDA_GRAPHICS_VK_INSTANCE_UTILS_H
