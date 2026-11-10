#include "background_cover.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace detail = dinotale32x9::background_cover_detail;
using dinotale32x9::background_tile_zero_transparent;
using dinotale32x9::dispatch_background_cover;
using dinotale32x9::background_left_strip_transparent;

namespace {
constexpr uint32_t bg = 0x80000100u, rt = 0x80000200u;
constexpr uint32_t graphics = 0x80000400u, palette = 0x80000800u, object = 0x80000c00u;
constexpr uint32_t physical(uint32_t address) { return address & 0x1fffffffu; }
void word(std::vector<uint8_t>& memory, uint32_t address, uint32_t value) {
    const uint32_t at = physical(address);
    for (uint32_t i = 0; i < 4; ++i) { memory[at + i] = uint8_t(value >> (8 * i)); }
}
void half(std::vector<uint8_t>& memory, uint32_t address, uint16_t value) {
    const uint32_t at = physical(address) ^ 2u;
    memory[at] = uint8_t(value); memory[at + 1] = uint8_t(value >> 8);
}
void byte(std::vector<uint8_t>& memory, uint32_t address, uint8_t value) {
    memory[physical(address) ^ 3u] = value;
}
std::vector<uint8_t> fixture(uint8_t format = 2) {
    std::vector<uint8_t> memory(0x2000, 0xa7);
    word(memory, bg + 0x0c, graphics);
    word(memory, bg + 0x14, 0x80001000); // Deliberately different from active RT palette.
    word(memory, rt + 4, palette);
    byte(memory, rt + 8, format);
    byte(memory, rt + 9, 1);
    for (uint32_t i = 0; i < 256; ++i) {
        byte(memory, graphics + i, 0);
        half(memory, palette + i * 2, 0xabce); // Colorful RGB, fully transparent alpha.
    }
    return memory;
}

void test_formats() {
    auto memory = fixture();
    for (uint32_t i = 0; i < 256; ++i) { byte(memory, graphics + i, uint8_t(i % 18)); }
    byte(memory, graphics + 255, 255);
    const auto before = memory;
    assert(background_tile_zero_transparent(memory, bg, rt));
    assert(memory == before);
    half(memory, palette + 100 * 2, 1); // Unused palette entry is irrelevant.
    assert(background_tile_zero_transparent(memory, bg, rt));
    byte(memory, graphics + 63, 100);
    assert(!background_tile_zero_transparent(memory, bg, rt));

    memory = fixture(3);
    for (uint32_t i = 0; i < 256; ++i) { byte(memory, graphics + i, uint8_t((i % 16) << 4)); }
    word(memory, rt + 4, 0); // IA8 needs no palette.
    assert(background_tile_zero_transparent(memory, bg, rt));
    byte(memory, graphics + 255, 0xf1);
    assert(!background_tile_zero_transparent(memory, bg, rt));

    memory = fixture(4);
    word(memory, rt + 4, 0); // I8 alpha equals intensity, not a low nibble.
    assert(background_tile_zero_transparent(memory, bg, rt));
    byte(memory, graphics + 255, 0x10);
    assert(!background_tile_zero_transparent(memory, bg, rt));
}

void test_invalid_inputs() {
    for (int scenario = 0; scenario < 10; ++scenario) {
        auto memory = fixture();
        uint32_t background = bg, target = rt;
        switch (scenario) {
        case 0: background = 0; break;
        case 1: target = 0x80001ff8; break;
        case 2: word(memory, bg + 0x0c, 0x80001f04); break;
        case 3: word(memory, rt + 4, 0x80001e04); break;
        case 4: word(memory, rt + 4, palette + 1); break;
        case 5: byte(memory, rt + 8, 0); break;
        case 6: byte(memory, rt + 8, 5); break;
        case 7: byte(memory, rt + 9, 0); break;
        case 8: background += 1; break;
        case 9: memory.pop_back(); break;
        }
        const auto before = memory;
        assert(!background_tile_zero_transparent(memory, background, target));
        assert(memory == before);
    }
}

void test_dispatch() {
    for (const uint32_t bits : {0xc3310000u, 0x80000000u, 0x7fc12345u}) {
        auto memory = fixture();
        word(memory, object + 0x18, bits);
        const auto before = memory;
        int called = 0;
        const int result = dispatch_background_cover(memory, object, true, [&]() {
            ++called;
            assert(detail::word(memory, physical(object) + 0x18) == 0);
            return 41;
        });
        assert(result == 41 && called == 1 && memory == before);
        called = 0;
        dispatch_background_cover(memory, object, false, [&]() {
            ++called;
            assert(detail::word(memory, physical(object) + 0x18) == bits);
        });
        assert(called == 1 && memory == before);
    }
    for (const uint32_t pointer : {0u, 0x80000001u, 0x80001fe8u}) {
        auto memory = fixture();
        const auto before = memory;
        int called = 0;
        dispatch_background_cover(memory, pointer, true, [&]() { ++called; });
        assert(called == 1 && memory == before);
    }
}

void test_current_strip() {
    auto memory = fixture();
    constexpr uint32_t buffer = 0x80001200u;
    word(memory, rt, buffer);
    half(memory, rt + 0x0a, 32);
    half(memory, rt + 0x0c, 16);
    half(memory, rt + 0x18, 24);
    half(memory, rt + 0x1a, 12);
    word(memory, rt + 0x10, 29);
    word(memory, rt + 0x14, 14);
    word(memory, rt + 0x34, 0);
    word(memory, object + 0x18, std::bit_cast<uint32_t>(-4.25f)); // Native mask rounds up to5.
    for (uint32_t i = 0; i < 512; ++i) { byte(memory, buffer + i, 1); }
    half(memory, palette + 2, 0xffff); // Opaque outside the checked strip.
    for (uint32_t y = 0; y < 12; ++y) {
        for (uint32_t x = 0; x < 5; ++x) {
            byte(memory, buffer + ((14 + y) * 32 + 29 + x) % 512, 0);
        }
    }
    assert(background_tile_zero_transparent(memory, bg, rt));
    const auto before = memory;
    assert(background_left_strip_transparent(memory, rt, object));
    assert(memory == before);
    // A single stale opaque cached texel must retain the existing mask.
    byte(memory, buffer + ((14 + 7) * 32 + 29 + 4) % 512, 1);
    assert(!background_left_strip_transparent(memory, rt, object));
    memory = before;
    // Source column5 is the legitimate boundary art and must stay excluded.
    byte(memory, buffer + ((14 + 7) * 32 + 29 + 5) % 512, 1);
    assert(background_left_strip_transparent(memory, rt, object));
    word(memory, rt + 0x34, 2);
    word(memory, rt + 0x28, std::bit_cast<uint32_t>(0.9999999403953552f));
    assert(background_left_strip_transparent(memory, rt, object)); // Native10.10 step stays1024.
    // A half-scale frame masks twice as many source columns.
    word(memory, rt + 0x34, 2);
    word(memory, rt + 0x28, std::bit_cast<uint32_t>(0.5f));
    assert(!background_left_strip_transparent(memory, rt, object));
    for (uint32_t y = 0; y < 12; ++y) {
        for (uint32_t x = 0; x < 10; ++x) {
            byte(memory, buffer + ((14 + y) * 32 + 29 + x) % 512, 0);
        }
    }
    assert(background_left_strip_transparent(memory, rt, object));
    word(memory, rt + 0x28, std::bit_cast<uint32_t>(0.0f));
    assert(!background_left_strip_transparent(memory, rt, object));
    word(memory, rt + 0x34, 0);
    word(memory, object + 0x18, 0x7fc12345);
    assert(!background_left_strip_transparent(memory, rt, object));
    word(memory, object + 0x18, std::bit_cast<uint32_t>(-5.0f));
    word(memory, rt, 0x80001fff);
    assert(!background_left_strip_transparent(memory, rt, object));
}
}

int main() {
    test_formats();
    test_invalid_inputs();
    test_dispatch();
    test_current_strip();
    std::cout << "Background transparency and mask dispatch checks passed.\n";
}
