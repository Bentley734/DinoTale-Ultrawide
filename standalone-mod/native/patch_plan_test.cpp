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
    assert(!visibility_guards_match(image));
    for (uint32_t rva : visibility_rvas) {
        std::memcpy(image.data()+rva, visibility_prologue.data(), visibility_prologue.size());
    }
    std::memcpy(image.data()+screen_x_mapper_rva, screen_x_mapper_prologue.data(), screen_x_mapper_prologue.size());
    assert(visibility_guards_match(image));
    image[visibility_rvas[1]+17] ^= 1;
    assert(!visibility_guards_match(image));
    image[visibility_rvas[1]+17] ^= 1;
    const auto jump = make_visibility_jump(visibility_rvas[0], 0x123456789ABCDEF0ULL);
    assert(jump.size == 18);
    assert(std::memcmp(jump.original.data(), visibility_prologue.data(), 18) == 0);
    uintptr_t address = 0;
    std::memcpy(&address, jump.replacement.data()+6, sizeof(address));
    assert(address == 0x123456789ABCDEF0ULL);
    assert(jump.replacement[0] == 0xFF && jump.replacement[1] == 0x25);
    for (size_t i=14; i<18; ++i) { assert(jump.replacement[i] == 0x90); }
    assert(!background_guards_match(image));
    assert(!camera_guards_match(image));
    std::memcpy(image.data()+camera_wall_rva,camera_wall_prologue.data(),camera_wall_prologue.size());
    assert(camera_guards_match(image));
    image[camera_wall_rva+17] ^= 1;
    assert(!camera_guards_match(image));
    image[camera_wall_rva+17] ^= 1;
    std::memcpy(image.data()+background_copy_rva, background_copy_prologue.data(), background_copy_prologue.size());
    std::memcpy(image.data()+background_cover_rva, background_cover_prologue.data(), background_cover_prologue.size());
    assert(background_guards_match(image));
    const auto copy_jump = make_function_jump(background_copy_rva,background_copy_prologue,0x12345678);
    const auto cover_jump = make_function_jump(background_cover_rva,background_cover_prologue,0x12345678);
    assert(copy_jump.size == 16 && cover_jump.size == 15);
    image[background_copy_rva+15] ^= 1;
    assert(!background_guards_match(image));
    image[background_copy_rva+15] ^= 1;
    image[background_cover_rva+14] ^= 1;
    assert(!background_guards_match(image));
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
