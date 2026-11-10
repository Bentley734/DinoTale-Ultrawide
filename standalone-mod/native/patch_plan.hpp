#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace dinotale32x9 {
inline constexpr uint64_t supported_file_size = 19233280;
inline constexpr uint64_t supported_fnv1a64 = 0x6F9C27B392AC3D9CULL;
inline constexpr char supported_sha256[] = "2cb1a2ee3f01ef75eaaae98485b0022e3b6991a5181aa71f824a7641f362cf8b";

struct Patch {
    uint32_t rva;
    size_t size;
    std::array<uint8_t, 8> original;
    std::array<uint8_t, 8> replacement;
};

// Exact Windows DinoTale 0.1.8 locations, guarded by whole-file identity
// and original bytes. There is no signature search or modification of the
// executable on disk. The heap instruction is verified in its entirety.
inline constexpr std::array<Patch, 4> patches{{
    {0xCF4720, 8, {0x1C,0xC7,0x71,0x1C,0xC7,0x71,0xFC,0x3F}, {0x1C,0xC7,0x71,0x1C,0xC7,0x71,0x0C,0x40}},
    {0xCF4740, 8, {0xAB,0xAA,0xAA,0xAA,0xAA,0xAA,0x3A,0x40}, {0xAB,0xAA,0xAA,0xAA,0xAA,0xAA,0x4A,0x40}},
    {0xD1D714, 4, {0x39,0x8E,0xE3,0x3F}, {0x39,0x8E,0x63,0x40}},
    {0x1A9CC9, 8, {0x48,0xC7,0x42,0x78,0x00,0x00,0x40,0x80}, {0x48,0xC7,0x42,0x78,0x00,0x00,0x80,0x80}},
}};

inline uint64_t hash_bytes(std::span<const uint8_t> bytes, uint64_t hash = 14695981039346656037ULL) {
    for (uint8_t byte : bytes) {
        hash = (hash ^ byte) * 1099511628211ULL;
    }
    return hash;
}

inline bool matches(std::span<const uint8_t> image, bool patched = false) {
    for (const auto& patch : patches) {
        if (patch.rva > image.size() || patch.size > image.size() - patch.rva) {
            return false;
        }
        const auto& expected = patched ? patch.replacement : patch.original;
        if (std::memcmp(image.data() + patch.rva, expected.data(), patch.size) != 0) {
            return false;
        }
    }
    return true;
}
}
