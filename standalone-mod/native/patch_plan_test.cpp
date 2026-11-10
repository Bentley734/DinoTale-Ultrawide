#include "patch_plan.hpp"
#include <cassert>
#include <vector>

int main() {
    using namespace dinotale32x9;
    const uint8_t known[]{'h','e','l','l','o'};
    assert(hash_bytes(known) == 0xA430D84680AABD0BULL);
    assert(hash_bytes({known + 2, 3}, hash_bytes({known, 2})) == hash_bytes(known));
    std::vector<uint8_t> image(0xD1D718, 0xA5);
    assert(!matches(image));
    for (const auto& p : patches) { std::memcpy(image.data() + p.rva, p.original.data(), p.size); }
    assert(matches(image));
    assert(!matches({image.data(), image.size()-1}));
    // Check every site before writing anything, so a changed host cannot be
    // left with only some of the limits lifted.
    image[patches.back().rva] ^= 1;
    assert(!matches(image));
    image[patches.back().rva] ^= 1;
    for (const auto& p : patches) { std::memcpy(image.data() + p.rva, p.replacement.data(), p.size); }
    assert(matches(image, true));
    assert(!matches(image));
    for (const auto& p : patches) { std::memcpy(image.data() + p.rva, p.original.data(), p.size); }
    assert(matches(image));
    return 0;
}
