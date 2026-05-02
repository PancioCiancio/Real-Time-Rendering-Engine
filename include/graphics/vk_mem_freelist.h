// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#ifndef ORDA_VK_MEM_FREELIST_H
#define ORDA_VK_MEM_FREELIST_H

#include <algorithm>
#include <Volk/volk.h>
#include <vector>
#include <cassert>

namespace gpu
{

// Immutable data.
//
// Represent the memory block used needed for vulkan resources {buffer, image, ... }
// that are allocated and bound inside the same larger memory.
struct MemoryBlock
{
    VkDeviceSize offset = 0;
    VkDeviceSize size   = 0;
};

// The memory block is considered valid only if has a valid size.
constexpr bool IsMemoryBlockValid(const MemoryBlock& memory_block)
{
    return memory_block.offset >= 0 && memory_block.size > 0;
}

// Manages sub-allocations within a single VkDeviceMemory.
// Does NOT own the VkDeviceMemory - the caller creates and destroys it.
// Typical usage: one GpuFreeList per memory type pool
// (device local, host visible, etc...)
class FreeList
{
public:
    //
    explicit FreeList(VkDeviceSize capacity) noexcept;

    //
    void Init();

    //
    [[nodiscard]] MemoryBlock Allocate(VkDeviceSize size, VkDeviceSize alignment);

    //
    void Free(VkDeviceSize offset);

private:
    void Coalesce();

    VkDeviceSize capacity_                  = 0;
    std::vector<MemoryBlock> free_blocks_   = {};
    std::vector<MemoryBlock> used_blocks_   = {};
};

}   // namespace gpu

#endif //ORDA_VK_MEM_FREELIST_H
