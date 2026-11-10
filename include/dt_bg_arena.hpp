#ifndef DT_BG_ARENA_HPP
#define DT_BG_ARENA_HPP

#include <algorithm>
#include <cstdint>
#include <vector>

namespace dinotale {

// The US game's heap ends at 0x80400000 (func_80064BAC). The runtime's
// reserved addresses begin at 0x80800000 (librecomp/addresses.hpp). Keep the
// enlarged CI8 background rings in the otherwise unused expansion memory.
// Addresses and bookkeeping are used only on the game's thread.
class BackgroundArena {
public:
    static constexpr uint32_t begin = 0x80400000U;
    static constexpr uint32_t end = 0x80800000U;
    static constexpr uint32_t capacity = end - begin;
    static constexpr uint32_t alignment = 64U;

    uint32_t allocate(uint32_t size) {
        if ((size == 0) || (size > capacity)) {
            return 0;
        }
        const uint32_t aligned_size = (size + alignment - 1U) & ~(alignment - 1U);
        uint32_t cursor = begin;
        auto position = allocations.begin();
        for (; position != allocations.end(); ++position) {
            if (position->address - cursor >= aligned_size) {
                break;
            }
            cursor = position->address + position->size;
        }
        if (end - cursor < aligned_size) {
            return 0;
        }
        allocations.insert(position, {cursor, aligned_size});
        return cursor;
    }

    bool release(uint32_t address) {
        const auto position = std::lower_bound(allocations.begin(), allocations.end(), address,
            [](const Block& block, uint32_t value) { return block.address < value; });
        if ((position == allocations.end()) || (position->address != address)) {
            return false;
        }
        allocations.erase(position);
        return true;
    }

    static bool contains(uint32_t address) {
        return (address >= begin) && (address < end);
    }

    void reset() {
        allocations.clear();
    }

private:
    struct Block {
        uint32_t address;
        uint32_t size;
    };
    std::vector<Block> allocations;
};

} // namespace dinotale

#endif // DT_BG_ARENA_HPP
