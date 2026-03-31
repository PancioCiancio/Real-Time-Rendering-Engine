#include "query_gpu.h"

#include <vector>
#include <string.h>
#include <print>

#include "vk_assert.h"

bool PhysicalDeviceSupportsExtensions(VkPhysicalDevice physical_device, uint32_t count, const char** requested)
{
    uint32_t supported_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &supported_count, nullptr);

    std::vector<VkExtensionProperties> supported(supported_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &supported_count, supported.data());

    for (uint32_t i = 0; i < count; i++)
    {
        bool found = false;
        for (auto& ext : supported)
        {
            if (strcmp(requested[i], ext.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false; // stop at first missing extension
    }
    return true;
}

bool PhysicalDeviceSupportsFeatures(VkPhysicalDevice gpu, VkPhysicalDeviceFeatures requested)
{
    VkPhysicalDeviceFeatures supported = {};
    vkGetPhysicalDeviceFeatures(gpu, &supported);

    const VkBool32* req = reinterpret_cast<const VkBool32*>(&requested);
    const VkBool32* sup = reinterpret_cast<const VkBool32*>(&supported);
    constexpr size_t count = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

    for (size_t i = 0; i < count; i++)
    {
        if (req[i] == VK_TRUE && sup[i] == VK_FALSE)
        {
            return false;
        }
    }

    return true;
}

void QuerySuitablePhysicalDevice(VkInstance instance, VkPhysicalDeviceFeatures requested_features, uint32_t requested_extension_count, const char** requested_extensions, VkPhysicalDevice* out_physical_devices)
{
    uint32_t physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

    VkPhysicalDevice selected   = VK_NULL_HANDLE;
    int32_t best_score          = -1;

    for (VkPhysicalDevice physical_device : physical_devices)
    {
        // Hard requirements — skip GPU entirely if unmet
        if (!PhysicalDeviceSupportsExtensions(physical_device, requested_extension_count, requested_extensions)) continue;
        if (!PhysicalDeviceSupportsFeatures(physical_device, requested_features)) continue;

        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(physical_device, &props);

        // Scoring — extend this as you need (VRAM size, Vulkan version, etc.)
        int32_t score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 10;
        }

        if (score > best_score) 
        { 
            best_score = score; selected = physical_device; 
        }
    }

    VK_CHECK(selected == VK_NULL_HANDLE ? VK_ERROR_FEATURE_NOT_PRESENT : VK_SUCCESS);

    VkPhysicalDeviceProperties selected_props = {};
    vkGetPhysicalDeviceProperties(selected, &selected_props);
    std::println("[GPU] {} (device: {}, vendor: {})",
        selected_props.deviceName,
        selected_props.deviceID,
        selected_props.vendorID);

    *out_physical_devices = selected;
}