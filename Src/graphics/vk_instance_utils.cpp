// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#include "vk_instance_utils.h"

#include "vk_core_utils.h"

namespace vk_utils {

std::expected<VkInstance, VkResult> CreateInstance(
    std::span<const char* const> layers,
    std::span<const char* const> extensions,
    const VkDebugUtilsMessengerCreateInfoEXT* debug_create_info)
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Orda";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = debug_create_info;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(std::size(layers));
    create_info.ppEnabledLayerNames = layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
    create_info.ppEnabledExtensionNames = extensions.data();

    VkInstance instance = {};
    VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance));

    return instance;
}

}