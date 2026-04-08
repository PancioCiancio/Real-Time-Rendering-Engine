// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#ifndef ORDA_GRAPHICS_VK_INSTANCE_UTILS_H
#define ORDA_GRAPHICS_VK_INSTANCE_UTILS_H

#include <Volk/volk.h>
#include <expected>
#include <span>

namespace vk_utils {

std::expected<VkInstance, VkResult> CreateInstance(
    std::span<const char* const> layers,
    std::span<const char* const> extensions,
    const VkDebugUtilsMessengerCreateInfoEXT* debug_create_info = nullptr);

}

#endif //ORDA_GRAPHICS_VK_INSTANCE_UTILS_H
