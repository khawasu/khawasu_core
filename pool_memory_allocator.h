#pragma once

#include "types.h"
#include <bitset>


template <int piece_size, int count>
class PoolMemoryAllocator
{
public:
    ubyte packets[count][piece_size];
    std::bitset<count> used_bits{0};

    void* alloc(uint size) {
        if (size > piece_size)
            return malloc(size);
        if (used_bits.all())
            return malloc(size);
        for (int i = 0; i < count; ++i) {
            if (!used_bits[i]) {
                used_bits[i] = 1;
                return packets[i];
            }
        }
        return malloc(size);
    }

    void free(void* ptr_) {
        auto ptr = (ubyte*) ptr_;
        if (&packets[0][0] > ptr || ptr > &packets[count - 1][piece_size - 1])
            free(ptr);
        else {
            auto index = (ptr - &packets[0][0]) / piece_size;
            used_bits[index] = 0;
        }
    }
};
