#include "renderer_refactor.h"

#include <Volk/volk.h>
#include <array>
#include <vector>

#include "vk_assert.h"
#include "query_gpu.h"
#include "query_queue_family.h"

struct Renderer
{
    VkAllocationCallbacks vk_allocator;
    VkInstance instance;
    VkPhysicalDevice physical_device;   // Support only one physical device.
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    uint32_t graphics_queue_family_index;
    uint32_t compute_queue_family_index;
    uint32_t transfer_queue_family_index;

    // Controls CPU pacing and resource reuse.
    struct
    {
        VkSemaphore acquired_image_semaphores[2];
        VkFence submit_fences[2];
        VkCommandPool command_pool[2];
        VkCommandBuffer command_buffer[2];
    } frame_in_flight;

    // Swapchain resources needed for presentation rendering.
    struct
    {
        VkImage* images;
        VkImageView* image_views;
        VkFramebuffer* framebuffers;
        VkSemaphore* render_finished_semaphores;
    } frame_presentation;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VkSurfaceFormatKHR surface_format;
    VkFormat depth_stencil_format;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkImage framebuffer_sample_image;
    VkImageView framebuffer_sample_image_view;
    VkDeviceMemory framebuffer_sample_image_memory;
    VkImage depth_stencil_image;
    VkImageView depth_stencil_image_view;
    VkDeviceMemory depth_stencil_memory;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkPipeline wireframe_pipeline;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
};

// Create vulkan instance
void CreateInstance(const RendererDescriptor* renderer_desc, const AllocationCallback* allocator, Renderer_t out_renderer)
{
    VkApplicationInfo app_info = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "Adro";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    // instance_create_info.pNext                   = &DEBUG_UTILS_MESSENGER_CREATE_INFO;
    instance_create_info.pApplicationInfo           = &app_info;
    instance_create_info.enabledLayerCount          = renderer_desc->instance_requested_layers_count;
    instance_create_info.ppEnabledLayerNames        = &renderer_desc->instance_requested_layers[0];
    instance_create_info.enabledExtensionCount      = renderer_desc->instance_requested_extensions_count;
    instance_create_info.ppEnabledExtensionNames    = &renderer_desc->instance_requested_extensions[0];

    VkAllocationCallbacks* vk_allocator = nullptr;

    VK_CHECK(vkCreateInstance(&instance_create_info, vk_allocator, &out_renderer->instance));
    volkLoadInstance(out_renderer->instance);
    // vkCreateDebugUtilsMessengerEXT(out_renderer->instance, &DEBUG_UTILS_MESSENGER_CREATE_INFO, renderer_desc->vk_allocator, &debugMessenger);
}

void CreateDevice(const RendererDescriptor* renderer_desc, const AllocationCallback* allocator, Renderer_t out_renderer)
{
    VkPhysicalDeviceFeatures required_features = {};
    required_features.geometryShader        = VK_TRUE;
    required_features.tessellationShader    = VK_TRUE;
    required_features.multiDrawIndirect     = VK_TRUE;
    required_features.fillModeNonSolid      = VK_TRUE;
    required_features.sampleRateShading     = VK_TRUE;
    required_features.samplerAnisotropy     = VK_TRUE;

    QuerySuitablePhysicalDevice(
        out_renderer->instance, 
        required_features, 
        renderer_desc->device_requested_extensions_count, 
        renderer_desc->device_requested_extensions,
        &out_renderer->physical_device);

    QueueFamilyIndices queue_family_indices = {};

    QueryQueueFamilies(
        out_renderer->physical_device,
        out_renderer->surface,
        &queue_family_indices);

    constexpr float kQueuePriorities = 1.0f;

    uint32_t requested_families[] = {
        out_renderer->graphics_queue_family_index,
        out_renderer->compute_queue_family_index,
        out_renderer->transfer_queue_family_index,
        // omit video ...
    };

    // @TODO: find a way to use the allocator passed in instead.
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    for (uint32_t family : requested_families)
    {
        if (family == UINT32_MAX)
        {
            continue; // optional family not available
        }

        // Check if already added
        bool already_added = false;
        for (auto& info : queue_create_infos)
        {
            if (info.queueFamilyIndex == family) 
            { 
                already_added = true; 
                break; 
            }
        }

        if (already_added)
        {
            continue;
        }

        VkDeviceQueueCreateInfo info = {};
        info.sType              = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex   = family;
        info.queueCount         = 1;
        info.pQueuePriorities   = &kQueuePriorities;
        queue_create_infos.push_back(info);
    }

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.flags                    = 0;
    device_create_info.queueCreateInfoCount     = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos        = queue_create_infos.data();
    device_create_info.enabledExtensionCount    = renderer_desc->device_requested_extensions_count;
    device_create_info.ppEnabledExtensionNames  = &renderer_desc->device_requested_extensions[0];
    device_create_info.pEnabledFeatures         = &required_features;

    constexpr uint32_t queue_family_index = 0;
    VkAllocationCallbacks* vk_allocator = nullptr;
    VK_CHECK(vkCreateDevice(out_renderer->physical_device, &device_create_info, vk_allocator, &out_renderer->device));
    volkLoadDevice(out_renderer->device);
    vkGetDeviceQueue(out_renderer->device, out_renderer->graphics_queue_family_index, 0, &out_renderer->graphics_queue);
    vkGetDeviceQueue(out_renderer->device, out_renderer->compute_queue_family_index, 0, &out_renderer->compute_queue);
    vkGetDeviceQueue(out_renderer->device, out_renderer->transfer_queue_family_index, 0, &out_renderer->transfer_queue);
}

void CreateRenderer(const RendererDescriptor* renderer_desc, const AllocationCallback* allocator, Renderer_t* out_renderer)
{
    // Use the allocator to allocate the Renderer itself
    void* mem = allocator->alloc_func(
        allocator->user_data,   // defined on client-side
        sizeof(Renderer), 
        alignof(Renderer));

    *out_renderer = new(mem) Renderer();    // Placement new

    VK_CHECK(volkInitialize());

    CreateInstance(renderer_desc, allocator, *out_renderer);
    CreateDevice(renderer_desc, allocator, *out_renderer);
}

void DestroyRenderer(Renderer_t renderer, const AllocationCallback* allocator)
{
    renderer->~Renderer();  // Explicit destructor (placement new)
    allocator->free_func(
        allocator->user_data,
        renderer);
}