//
// Created by apant on 08/10/2025.
//


#ifndef ADROENGINE_MEMORT_H
#define ADROENGINE_MEMORT_H

#include <stdio.h>

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