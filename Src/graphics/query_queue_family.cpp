#include "query_queue_family.h"

#include <vector>
#include <print>

#include "vk_assert.h"

// Score how "dedicated" a family is for a given purpose.
// A lower flag count means less overlap = more specialized.
int32_t QueueCountExtraBits(VkQueueFlags flags, VkQueueFlags desired)
{
    auto extra = static_cast<int32_t>(flags & ~desired); // bits present but not needed
    int32_t count = 0;
    while (static_cast<bool>(extra)) { count += extra & 1; extra >>= 1; }
    return count;
}

void QueryQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface, QueueFamilyIndices* out_queue_family_indices)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, families.data());

    // UINT32_MAX = not yet found
    out_queue_family_indices->graphics     = UINT32_MAX;
    out_queue_family_indices->compute      = UINT32_MAX;
    out_queue_family_indices->transfer     = UINT32_MAX;
    out_queue_family_indices->video_decode = UINT32_MAX;
    out_queue_family_indices->video_encode = UINT32_MAX;

    // Track the "specialization score" for compute and transfer
    // — lower extra bits = better candidate
    int32_t best_compute_score  = INT32_MAX;
    int32_t best_transfer_score = INT32_MAX;

    for (uint32_t i = 0; i < count; i++)
    {
        const VkQueueFlags flags = families[i].queueFlags;

        // --- Graphics + Present (must be same family for simplicity) ---
        if (out_queue_family_indices->graphics == UINT32_MAX && (flags & VK_QUEUE_GRAPHICS_BIT))
        {
            bool present_ok = true;
            if (surface != VK_NULL_HANDLE)
            {
                VkBool32 supported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supported);
                present_ok = (supported == VK_TRUE);
            }
            if (present_ok)
            {
                out_queue_family_indices->graphics = i;
            }
        }

        // --- Async Compute: supports compute, prefer no graphics bit ---
        if (flags & VK_QUEUE_COMPUTE_BIT)
        {
            int32_t score = QueueCountExtraBits(flags, VK_QUEUE_COMPUTE_BIT);
            if (score < best_compute_score)
            {
                best_compute_score = score;
                out_queue_family_indices->compute = i;
            }
        }

        // --- DMA Transfer: prefer no graphics, no compute ---
        if (flags & VK_QUEUE_TRANSFER_BIT)
        {
            int32_t score = QueueCountExtraBits(flags, VK_QUEUE_TRANSFER_BIT);
            if (score < best_transfer_score)
            {
                best_transfer_score = score;
                out_queue_family_indices->transfer = i;
            }
        }

        // --- Video (optional, discrete hardware blocks) ---
        if (flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) out_queue_family_indices->video_decode = i;
        if (flags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) out_queue_family_indices->video_encode = i;
    }

    // Graphics and transfer are hard requirements
    VK_CHECK(out_queue_family_indices->graphics == UINT32_MAX ? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS);
    VK_CHECK(out_queue_family_indices->transfer == UINT32_MAX ? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS);

    // Compute falls back to graphics family — valid, just not async
    if (out_queue_family_indices->compute == UINT32_MAX)
    {
        std::println("[WARN] No dedicated compute family — falling back to graphics family");
        out_queue_family_indices->compute = out_queue_family_indices->graphics;
    }

    // Video is optional — just log
    if (out_queue_family_indices->video_decode == UINT32_MAX) std::println("[INFO] Video decode not supported");
    if (out_queue_family_indices->video_encode == UINT32_MAX) std::println("[INFO] Video encode not supported");
}
