#ifndef ORDA_GRAPHICS_QUERY_GPU_H_
#define ORDA_GRAPHICS_QUERY_GPU_H_

#include <Volk/volk.h>

void QuerySuitablePhysicalDevice(
    VkInstance instance,  
    VkPhysicalDeviceFeatures requested_features, 
    uint32_t requested_extension_count, 
    const char** requested_extensions, 
    VkPhysicalDevice* out_physical_device);

#endif // ORDA_GRAPHICS_QUERY_GPU_H_