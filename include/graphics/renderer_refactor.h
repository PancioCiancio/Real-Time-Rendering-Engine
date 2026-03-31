#ifndef ORDA_GRAPHICS_RENDERER_REFACTOR_H_
#define ORDA_GRAPHICS_RENDERER_REFACTOR_H_

#include <Volk/volk.h>

// Allocation callbacks used to make renderer's internal cpu allocation
struct AllocationCallback
{
    void* user_data;
    void* (*alloc_func)(void* user_data, size_t size, size_t alignemnt);
    void (*free_func)(void* user_data, void* memory);
};

struct RendererDescriptor
{
    size_t instance_requested_layers_count;
    size_t instance_requested_extensions_count;
    size_t device_requested_extensions_count; 

    const char** instance_requested_layers;
    const char** instance_requested_extensions;
    const char** device_requested_extensions;

    // VkAllocationCallbacks* vk_allocator;
};

struct Renderer;    // Opaque forward declaration

using Renderer_t = Renderer*;

// Construct the renderer
// @todo pass the allocation callback to control how the renderer is allocated.
void CreateRenderer(const RendererDescriptor* renderer_desc, const AllocationCallback* allocator, Renderer_t* out_renderer);

// Destroy the renderer. Must be created with CreateRenderer.
// @todo pass the allocation callback to control how the renderer is deallocated.
void DestroyRenderer(Renderer_t renderer, const AllocationCallback* allocator);


#endif // ORDA_GRAPHICS_RENDERER_REFACTOR_H_