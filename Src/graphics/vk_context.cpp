// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#include "vk_context.h"

#include <SDL2/SDL_vulkan.h>
#include <vector>

#include "vk_core_utils.h"
#include "vk_instance_utils.h"
#include "vk_physical_device_utils.h"
#include "vk_device_utils.h"
#include "vk_queue_utils.h"

namespace vk_utils
{
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
    void *pUserData)
{
    // ReSharper disable once CppDFAUnusedValue
    const char *severity;
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "[VERBOSE]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severity = "[INFO]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "[WARNING]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "[ERROR]";
            break;
        default: severity = "";
            break;
    }

    // @TODO:	cover the message type as well....

    std::printf("[VK] %s %s\n", severity, callbackData->pMessage);

    return VK_FALSE;
}

constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_UTILS_MESSAGE_SEVERITY_FLAGS =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_UTILS_MESSAGE_TYPE_FLAGS =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

constexpr VkDebugUtilsMessengerCreateInfoEXT DEBUG_UTILS_MESSENGER_CREATE_INFO = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .pNext = nullptr,
    .flags = 0,
    .messageSeverity = DEBUG_UTILS_MESSAGE_SEVERITY_FLAGS,
    .messageType = DEBUG_UTILS_MESSAGE_TYPE_FLAGS,
    .pfnUserCallback = DebugCallback,
    .pUserData = nullptr,
};

std::expected<VulkanContext, VulkanError>CreateContext(SDL_Window *window)
{
    // Default init context.
    VulkanContext context = {};

    // Load global vk dispatch table using volk library.
    // Must be done before any vk call.
    VK_CHECK(volkInitialize());

    // 1. Instance
    // Define here other instance layers
    std::array<const char*, 1> layers = {
        "VK_LAYER_KHRONOS_validation"};

    // Define here other instance extensions
    std::array<const char*, 3> extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

    // @todo is the right functional approach?
    context.instance = CreateInstance(layers, extensions).value();

    // Load instance dispatch table. The VkInstance must be valid.
    volkLoadInstance(context.instance);

    // 2. Debugger
    vkCreateDebugUtilsMessengerEXT(
        context.instance,
        &DEBUG_UTILS_MESSENGER_CREATE_INFO,
        nullptr,
        &context.debug);

    // 3. Surface
    SDL_Vulkan_CreateSurface(
            window,
            context.instance,
            &context.surface);

    // 4. Physical Device
    VkPhysicalDeviceFeatures required_features = {};
    required_features.geometryShader = VK_TRUE;
    required_features.tessellationShader = VK_TRUE;
    required_features.multiDrawIndirect = VK_TRUE;
    required_features.fillModeNonSolid = VK_TRUE;
    required_features.sampleRateShading = VK_TRUE;
    required_features.samplerAnisotropy = VK_TRUE;

    std::array<const char*, 2> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_shader_draw_parameters"};

    context.phys_device = CreatePhysicalDevice(
        context.instance,
        device_extensions,
        required_features).value();

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        context.phys_device,
        &queue_family_count,
        nullptr);    // pQueueFamilyProperties;

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        context.phys_device,
        &queue_family_count,
        queue_family_properties.data());

    // 5. Device
    context.device = CreateDevice(
        context.phys_device,
        device_extensions,
        required_features).value();

    // Load device dispatch table. Device must be valid.
    volkLoadDevice(context.device);

    context.present_family_idx = GetPresentQueueIndex(
        context.phys_device,
        context.surface,
        queue_family_properties).value();

    vkGetDeviceQueue(
        context.device,
        context.present_family_idx,
        0,      // queueIndex
        &context.present_queue);

    return context;
}

void DestroyContext(const VulkanContext &context)
{
    // All handles must be valid. Intentionally not checked because
    // they must be valid at this point.
    // Maybe they aren't if the initialization failed.

    vkDeviceWaitIdle(context.device);

    vkDestroyDevice(context.device, nullptr);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vkDestroyDebugUtilsMessengerEXT(context.instance, context.debug, nullptr);
    vkDestroyInstance(context.instance, nullptr);
}

}
