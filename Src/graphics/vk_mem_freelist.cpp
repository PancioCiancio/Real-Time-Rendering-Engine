// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#include "vk_mem_freelist.h"

#include "Memory.h"

namespace gpu
{

FreeList::FreeList(VkDeviceSize capacity) noexcept
    : capacity_(capacity)
{

}

void FreeList::Init()
{
    free_blocks_ = { MemoryBlock(0, capacity_) };
    used_blocks_.clear();
}

MemoryBlock FreeList::Allocate(VkDeviceSize size, VkDeviceSize alignment)
{
    // Ensure size and alignment are valid.
    assert(size > 0);
    assert(alignment > 0);

    for (size_t i = 0; i < free_blocks_.size(); i++)
    {
        const MemoryBlock& mem_block = free_blocks_[i];

        const VkDeviceSize aligned_offset   = Memory::Align(mem_block.offset, alignment);
        const VkDeviceSize padding          = aligned_offset - mem_block.offset;
        const VkDeviceSize total_needed     = size + padding;

        if (mem_block.size < total_needed)
        {
            // There is not enough room to host the requested memory size.
            continue;
        }

        // This block has enough room to host the requested memory size.
        MemoryBlock chosen = mem_block;
        free_blocks_.erase(free_blocks_.begin() + static_cast<ptrdiff_t>(i));   // Remove the free block we are carving from.

        if (padding > 0)
        {
            // Return the padding prefix to the free list.
            free_blocks_.push_back(MemoryBlock(chosen.offset, padding));
        }

        // Return the siffix remainder to the free list
        // This might happen when the chosen block has greater memory than we need.
        const VkDeviceSize remainder = chosen.size - total_needed;
        if (remainder > 0)
        {
            free_blocks_.push_back(MemoryBlock(aligned_offset + size, remainder));
        }

        const MemoryBlock alloc_result = MemoryBlock(aligned_offset, size);
        used_blocks_.push_back(alloc_result);

        return alloc_result;
    }

    return MemoryBlock();   // zero alloc (invalid)
}

void FreeList::Free(VkDeviceSize offset)
{
    for (size_t i = 0; i < used_blocks_.size(); i++)
    {
        if (used_blocks_[i].offset != offset)
        {
            continue;
        }

        free_blocks_.push_back(used_blocks_[i]);
        used_blocks_.erase(used_blocks_.begin() + static_cast<ptrdiff_t>(i));

        Coalesce();

        return;
    }

    assert(false && "GpuFreeList::Free offset not found in used blocks");
}

void FreeList::Coalesce()
{
    // Sort by offset so adjacent blocks are neighbours in the vector.
    std::ranges::sort(
        free_blocks_,
        [](const MemoryBlock& a, const MemoryBlock& b) { return a.offset < b.offset; });

    size_t i = 0;
    while (i < free_blocks_.size() - 1)
    {
        MemoryBlock& current  = free_blocks_[i];
        MemoryBlock& next     = free_blocks_[i + 1];

        if (current.offset + current.size == next.offset)
        {
            current.size += next.size;
            free_blocks_.erase(free_blocks_.begin() + static_cast<ptrdiff_t>(i) + 1);
            // Do not advance: current might now be contiguos with the new next.
        }
        else
        {
            ++i;
        }
    }
}

}