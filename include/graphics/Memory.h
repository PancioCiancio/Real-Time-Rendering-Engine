// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#ifndef ADROENGINE_MEMORT_H
#define ADROENGINE_MEMORT_H

#include <cstddef>

namespace Memory
{
/**
 *
 * @param size The size of the memory requested.
 * @param alignment The required alignment. Must be power of two
 * @return The size + alignment.
 */
static std::size_t Align(std::size_t size, std::size_t alignment)
{
    // assert(alignment % 2 && "Alignment must be a power of two");
    return (size + alignment - 1) & ~(alignment - 1);
}
}

#endif //ADROENGINE_MEMORT_H