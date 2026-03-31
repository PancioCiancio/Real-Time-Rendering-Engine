#include "graphics/renderer_refactor.h"

#include <stdlib.h>

void* RendererAlloc(void* user_data, size_t size, size_t alignemnt)
{
	return _aligned_malloc(size, alignemnt);
}

void RendererFree(void* user_data, void* memory)
{
	_aligned_free(memory);
}

int main()
{
	RendererDescriptor renderer_desc = {};

	AllocationCallback renderer_allocator = {};
	renderer_allocator.alloc_func 	= RendererAlloc;
	renderer_allocator.free_func 	= RendererFree;

	Renderer_t renderer = nullptr;

	CreateRenderer(
		&renderer_desc, 	// Renderer Descriptor
		&renderer_allocator, 	
		&renderer);
	
	DestroyRenderer(renderer, &renderer_allocator);

	return 0;
}