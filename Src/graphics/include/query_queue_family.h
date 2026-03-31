#ifndef ORDA_GRAPHICS_QUERY_QUEUE_FAMILY_H_
#define ORDA_GRAPHICS_QUERY_QUEUE_FAMILY_H_

#include <Volk/volk.h>

// Temporary structure to group queue family indices.
// The desiderata is to have different family for each queue type.
//
// @TODO: replace this structure in such a way the client can pass
//        the desired queue flag bit and for each of them return
//        a unique family type if available.
struct QueueFamilyIndices
{
    uint32_t graphics;      // supports graphics + present
    uint32_t compute;       // prefer dedicated
    uint32_t transfer;      // prefer dedicated
    uint32_t video_decode;  // Optional - UINT32_MAX if unavailable
    uint32_t video_encode;  // Optional - UINT32_MAX if unavailable
};

void QueryQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface, QueueFamilyIndices* out_queue_family_indices);

#endif // ORDA_GRAPHICS_QUERY_QUEUE_FAMILY_H_