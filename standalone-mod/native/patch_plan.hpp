#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace dinotale32x9 {
inline constexpr uint64_t supported_file_size = 19233280;
inline constexpr uint64_t supported_fnv1a64 = 0x6F9C27B392AC3D9CULL;
inline constexpr char supported_sha256[] = "2cb1a2ee3f01ef75eaaae98485b0022e3b6991a5181aa71f824a7641f362cf8b";

struct Patch {
    uint32_t rva;
    size_t size;
    std::array<uint8_t, 32> original;
    std::array<uint8_t, 32> replacement;
};

// Exact Windows DinoTale 0.1.8 locations, guarded by whole-file identity
// and original bytes. There is no signature search or modification of the
// executable on disk. The heap instruction is verified in its entirety.
inline constexpr std::array<Patch, 5> patches{{
    {0xCF4720, 8, {0x1C,0xC7,0x71,0x1C,0xC7,0x71,0xFC,0x3F}, {0x1C,0xC7,0x71,0x1C,0xC7,0x71,0x0C,0x40}},
    {0xCF4740, 8, {0xAB,0xAA,0xAA,0xAA,0xAA,0xAA,0x3A,0x40}, {0xAB,0xAA,0xAA,0xAA,0xAA,0xAA,0x4A,0x40}},
    {0xD1D714, 4, {0x39,0x8E,0xE3,0x3F}, {0x39,0x8E,0x63,0x40}},
    {0x1A9CC9, 8, {0x48,0xC7,0x42,0x78,0x00,0x00,0x40,0x80}, {0x48,0xC7,0x42,0x78,0x00,0x00,0x80,0x80}},
    // The request pool has 104-byte slots but the game compared the number
    // of uint16_t tile entries with 105. 32:9 rows contain 54/55 entries and
    // overwrite the next slot. Use the heap path above 52 entries instead.
    {0x1613F0, 5, {0x48,0x83,0x7A,0x28,0x69}, {0x48,0x83,0x7A,0x28,0x35}},
}};

inline constexpr std::array<uint32_t, 2> visibility_rvas{0x182900, 0x182B10};
inline constexpr std::array<uint8_t, 18> visibility_prologue{
    0x56,0x57,0x48,0x83,0xEC,0x28,0x48,0x89,0xD6,0x48,0x89,0xCF,0x8B,0x82,0xE8,0,0,0
};
inline constexpr uint32_t screen_x_mapper_rva = 0x20880;
inline constexpr std::array<uint8_t, 7> screen_x_mapper_prologue{0x56,0x48,0x83,0xEC,0x20,0x89,0xCE};
inline constexpr uint32_t background_copy_rva = 0x1B4120;
inline constexpr std::array<uint8_t,16> background_copy_prologue{
    0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x55,0x53,0x48,0x83,0xEC,0x48
};
inline constexpr uint32_t background_cover_rva = 0x20900;
inline constexpr std::array<uint8_t,15> background_cover_prologue{
    0x56,0x57,0x53,0x48,0x83,0xEC,0x20,0x44,0x89,0xC6,0x89,0xD3,0x48,0x89,0xCF
};
inline constexpr uint32_t camera_wall_rva = 0xEC3C0;
inline constexpr uint32_t camera_follow_return_rva = 0xEE48B;
inline constexpr std::array<uint8_t,18> camera_wall_prologue{
    0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x83,0xEC,0x38,0x0F,0x29,0x74,0x24,0x20
};

inline bool camera_guards_match(std::span<const uint8_t> image) {
    return camera_wall_rva <= image.size() && camera_wall_prologue.size() <= image.size()-camera_wall_rva
        && std::memcmp(image.data()+camera_wall_rva,camera_wall_prologue.data(),camera_wall_prologue.size()) == 0;
}

inline bool background_guards_match(std::span<const uint8_t> image) {
    for (const auto [rva, bytes] : std::array<std::pair<uint32_t,std::span<const uint8_t>>,2>{{
            {background_copy_rva, background_copy_prologue}, {background_cover_rva, background_cover_prologue}}}) {
        if (rva > image.size() || bytes.size() > image.size()-rva
                || std::memcmp(image.data()+rva, bytes.data(), bytes.size()) != 0) { return false; }
    }
    return true;
}

inline bool visibility_guards_match(std::span<const uint8_t> image) {
    if (screen_x_mapper_rva > image.size() || screen_x_mapper_prologue.size() > image.size() - screen_x_mapper_rva
            || std::memcmp(image.data()+screen_x_mapper_rva, screen_x_mapper_prologue.data(), screen_x_mapper_prologue.size()) != 0) { return false; }
    for (uint32_t rva : visibility_rvas) {
        if (rva > image.size() || visibility_prologue.size() > image.size()-rva
                || std::memcmp(image.data()+rva, visibility_prologue.data(), visibility_prologue.size()) != 0) { return false; }
    }
    return true;
}

inline Patch make_function_jump(uint32_t rva, std::span<const uint8_t> prologue, uintptr_t destination) {
    Patch hook{};
    hook.rva = rva;
    hook.size = prologue.size();
    std::memcpy(hook.original.data(), prologue.data(), hook.size);
    hook.replacement.fill(0x90);
    const uint8_t instruction[]{0xFF,0x25,0,0,0,0};
    std::memcpy(hook.replacement.data(), instruction, sizeof(instruction));
    std::memcpy(hook.replacement.data()+6, &destination, sizeof(destination));
    return hook;
}

inline Patch make_visibility_jump(uint32_t rva, uintptr_t destination) {
    return make_function_jump(rva, visibility_prologue, destination);
}

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
