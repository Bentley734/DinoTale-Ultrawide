#include "ground_runtime.hpp"
#include <cassert>
#include <limits>
#include <vector>

int main() {
    using dinotale32x9::ground_minimum_tile_x;
    assert(ground_minimum_tile_x(272,1) == -21);
    assert(ground_minimum_tile_x(272,0.5f) == -42);
    assert(ground_minimum_tile_x(272,2) == -11);
    assert(ground_minimum_tile_x(0,1) == 0);
    assert(ground_minimum_tile_x(272,0) == 0);
    assert(ground_minimum_tile_x(272,-1) == 0);
    assert(ground_minimum_tile_x(272,std::numeric_limits<float>::quiet_NaN()) == 0);
    assert(ground_minimum_tile_x(272,0.0001f) == 0);
    std::vector<uint8_t> memory(0x100000);
    assert(!dinotale32x9::read_ground_runtime(memory,272).eligible());
    assert(!dinotale32x9::read_ground_runtime(memory,0).eligible());
}
