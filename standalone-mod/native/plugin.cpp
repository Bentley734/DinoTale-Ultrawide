#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>

#include "patch_plan.hpp"
#include "visibility_math.hpp"
#include "background_repeat.hpp"
#include "background_lookup.hpp"
#include "background_cover.hpp"
#include "camera_smoothing.hpp"
#include "recomp.h"

extern "C" {
__declspec(dllexport) uint32_t recomp_api_version = 1;
__declspec(dllexport) void dinotale32x9_marker(uint8_t*, void*) {}
__declspec(dllexport) uint64_t dinotale32x9_visibility_checks = 0;
__declspec(dllexport) uint64_t dinotale32x9_wide_visibility_checks = 0;
__declspec(dllexport) uint64_t dinotale32x9_background_tiles = 0;
__declspec(dllexport) uint64_t dinotale32x9_background_masks_cleared = 0;
__declspec(dllexport) uint64_t dinotale32x9_camera_steps = 0;
__declspec(dllexport) uint64_t dinotale32x9_eased_camera_steps = 0;
}

namespace {
uint8_t* host_image = nullptr;
bool installed = false;
using Predicate = void (*)(uint8_t*, recomp_context*);
using ScreenXMapper = int32_t (*)(int32_t);
ScreenXMapper screen_x_mapper = nullptr;
struct VisibilityHook {
    dinotale32x9::Patch patch{};
    void* trampoline = nullptr;
    Predicate original = nullptr;
    bool applied = false;
};
std::array<VisibilityHook, 2> visibility_hooks{};
std::array<VisibilityHook, 3> background_hooks{};
using CoverFunction = void (*)(uint8_t*, uint32_t, uint32_t);
dinotale32x9::HorizontalCameraSmoother camera_smoother;

bool expanded_view_active() { return screen_x_mapper(-1048576) != -1048576; }

void copy_background_tiles(uint8_t* rdram, recomp_context* ctx) {
    const uint32_t stack = uint32_t(ctx->r29);
    const bool capture = stack >= 0x80000000U && stack <= 0x80800000U-0x20U;
    dinotale32x9::BackgroundTileRequest request{};
    if (capture) {
        auto argument = [&](uint32_t offset) {
            uint32_t value = 0;
            std::memcpy(&value, rdram+(stack-0x80000000U)+offset, sizeof(value));
            return int32_t(int16_t(value));
        };
        request = {uint32_t(ctx->r4), uint32_t(ctx->r5), uint32_t(ctx->r7), int32_t(int16_t(ctx->r6)),
            argument(0x10), argument(0x14), argument(0x18), argument(0x1C)};
    }
    background_hooks[0].original(rdram, ctx);
    if (capture && request.start_x<0 && expanded_view_active()) {
        const auto result = dinotale32x9::fill_negative_background_tiles({rdram,0x800000}, request);
        dinotale32x9_background_tiles += result.entries_written;
    }
}

void cover_background(uint8_t* rdram, uint32_t rt, uint32_t object) {
    const std::span<uint8_t> memory{rdram,0x800000};
    bool inhibit = false;
    if (expanded_view_active()) {
        const uint32_t background = dinotale32x9::find_background_for_render_target(memory, rt);
        if (background) {
            const uint32_t at = rt-0x80000000U;
            const uint8_t format = rdram[(at+8)^3U];
            // CI8, IA8 and I8 use the same tile-ID/ring-buffer pipeline.
            if (format >= 2 && format <= 4 && rdram[(at+9)^3U] == 1) {
                inhibit = dinotale32x9::inspect_background_repeat(memory, background).eligible
                    || dinotale32x9::background_left_strip_transparent(memory,rt,object);
            }
        }
    }
    // Suppress the native mask before it is published to the render thread.
    // Transparent foreground strips must reveal the repeated sky beneath.
    dinotale32x9::dispatch_background_cover(memory,object,inhibit,[&]() {
        reinterpret_cast<CoverFunction>(background_hooks[1].trampoline)(rdram,rt,object);
    });
    if (inhibit) { ++dinotale32x9_background_masks_cleared; }
}

__declspec(noinline) void ease_camera(uint8_t* rdram, recomp_context* ctx) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    const uint32_t camera = uint32_t(ctx->r4);
    const std::span<const uint8_t> memory{rdram,0x800000};
    uint32_t at = 0;
    bool enabled = caller == reinterpret_cast<uintptr_t>(host_image)+dinotale32x9::camera_follow_return_rva
        && expanded_view_active() && (camera&3U) == 0
        && dinotale32x9::background_cover_detail::range(camera,0xF0,memory.size(),at);
    if (enabled) {
        enabled = dinotale32x9::background_cover_detail::word(memory,at+0xEC) == 0x8002ABD0U;
    }
    if (enabled) {
        const uint32_t player = dinotale32x9::background_cover_detail::word(memory,at+0xE8);
        float position = 0, velocity = 0;
        std::memcpy(&position,rdram+at+0x84,sizeof(position));
        std::memcpy(&velocity,rdram+at+0xAC,sizeof(velocity));
        const float eased = camera_smoother.smooth({camera,player,GetTickCount64(),position,velocity,true});
        std::memcpy(rdram+at+0xAC,&eased,sizeof(eased));
        ++dinotale32x9_camera_steps;
        if (eased != velocity) { ++dinotale32x9_eased_camera_steps; }
    }
    else { camera_smoother.reset(); }
    // The game's wall probe runs after easing and remains authoritative.
    background_hooks[2].original(rdram,ctx);
    if (enabled) {
        float accepted = 0;
        std::memcpy(&accepted,rdram+at+0xAC,sizeof(accepted));
        camera_smoother.accept_clamped_velocity(accepted);
    }
}

void run_visibility(size_t index, uint8_t* rdram, recomp_context* ctx) {
    ++dinotale32x9_visibility_checks;
    const uint32_t position = uint32_t(ctx->r4);
    dinotale32x9::dispatch_mapped_visibility({rdram, 0x800000}, position,
        [](int32_t x) {
            const int32_t mapped = screen_x_mapper(x);
            if (mapped != x) { ++dinotale32x9_wide_visibility_checks; }
            return mapped;
        },
        [&]() { visibility_hooks[index].original(rdram, ctx); });
}

void outer_visibility(uint8_t* rdram, recomp_context* ctx) { run_visibility(0, rdram, ctx); }
void inner_visibility(uint8_t* rdram, recomp_context* ctx) { run_visibility(1, rdram, ctx); }

void free_visibility_trampolines() {
    for (auto hooks : {std::span<VisibilityHook>{visibility_hooks},std::span<VisibilityHook>{background_hooks}}) {
        for (auto& hook : hooks) {
            if (hook.trampoline) { VirtualFree(hook.trampoline, 0, MEM_RELEASE); }
            hook.trampoline = nullptr;
            hook.original = nullptr;
        }
    }
}

bool prepare_visibility_hooks() {
    const std::array<Predicate, 2> replacements{outer_visibility, inner_visibility};
    for (size_t i=0; i<visibility_hooks.size(); ++i) {
        auto& hook = visibility_hooks[i];
        hook.patch = dinotale32x9::make_visibility_jump(dinotale32x9::visibility_rvas[i], reinterpret_cast<uintptr_t>(replacements[i]));
        hook.trampoline = VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!hook.trampoline) { free_visibility_trampolines(); return false; }
        auto* code = static_cast<uint8_t*>(hook.trampoline);
        std::memcpy(code, hook.patch.original.data(), hook.patch.size);
        const auto back = dinotale32x9::make_visibility_jump(0, reinterpret_cast<uintptr_t>(host_image+hook.patch.rva+hook.patch.size));
        std::memcpy(code+hook.patch.size, back.replacement.data(), 14);
        DWORD unused = 0;
        if (!VirtualProtect(code, 32, PAGE_EXECUTE_READ, &unused)) { free_visibility_trampolines(); return false; }
        FlushInstructionCache(GetCurrentProcess(), code, 32);
        hook.original = reinterpret_cast<Predicate>(hook.trampoline);
    }
    screen_x_mapper = reinterpret_cast<ScreenXMapper>(host_image+dinotale32x9::screen_x_mapper_rva);
    const std::array<dinotale32x9::Patch,3> backgrounds{
        dinotale32x9::make_function_jump(dinotale32x9::background_copy_rva, dinotale32x9::background_copy_prologue,
            reinterpret_cast<uintptr_t>(&copy_background_tiles)),
        dinotale32x9::make_function_jump(dinotale32x9::background_cover_rva, dinotale32x9::background_cover_prologue,
            reinterpret_cast<uintptr_t>(&cover_background)),
        dinotale32x9::make_function_jump(dinotale32x9::camera_wall_rva, dinotale32x9::camera_wall_prologue,
            reinterpret_cast<uintptr_t>(&ease_camera))
    };
    for (size_t i=0; i<background_hooks.size(); ++i) {
        auto& hook = background_hooks[i];
        hook.patch = backgrounds[i];
        hook.trampoline = VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!hook.trampoline) { free_visibility_trampolines(); return false; }
        auto* code = static_cast<uint8_t*>(hook.trampoline);
        std::memcpy(code, hook.patch.original.data(), hook.patch.size);
        const auto back = dinotale32x9::make_function_jump(0,
            {hook.patch.original.data(),hook.patch.size}, reinterpret_cast<uintptr_t>(host_image+hook.patch.rva+hook.patch.size));
        std::memcpy(code+hook.patch.size, back.replacement.data(), 14);
        DWORD unused = 0;
        if (!VirtualProtect(code,32,PAGE_EXECUTE_READ,&unused)) { free_visibility_trampolines(); return false; }
        FlushInstructionCache(GetCurrentProcess(),code,32);
        hook.original = reinterpret_cast<Predicate>(hook.trampoline);
    }
    return true;
}

// Loader callbacks avoid engine calls, worker threads, other DLL loads,
// and CNG provider initialization. Identity uses only direct file reads and
// a pure hash, followed by exact bytes at each patch site.
bool supported_host() {
    wchar_t path[32768];
    const DWORD length = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
    if (length == 0 || length >= std::size(path)) { return false; }
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { return false; }
    LARGE_INTEGER size{};
    bool good = GetFileSizeEx(file, &size) && uint64_t(size.QuadPart) == dinotale32x9::supported_file_size;
    uint64_t hash = 14695981039346656037ULL;
    uint8_t buffer[65536];
    uint64_t total = 0;
    while (good) {
        DWORD count = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { good = false; break; }
        if (count == 0) { break; }
        total += count;
        hash = dinotale32x9::hash_bytes({buffer, count}, hash);
    }
    CloseHandle(file);
    return good && total == dinotale32x9::supported_file_size && hash == dinotale32x9::supported_fnv1a64;
}

bool write_patch(const dinotale32x9::Patch& patch, bool restore) {
    uint8_t* target = host_image + patch.rva;
    const auto& expected = restore ? patch.replacement : patch.original;
    const auto& value = restore ? patch.original : patch.replacement;
    if (std::memcmp(target, expected.data(), patch.size) != 0) { return false; }
    DWORD protection = 0;
    if (!VirtualProtect(target, patch.size, PAGE_EXECUTE_READWRITE, &protection)) { return false; }
    std::memcpy(target, value.data(), patch.size);
    FlushInstructionCache(GetCurrentProcess(), target, patch.size);
    DWORD unused = 0;
    if (!VirtualProtect(target, patch.size, protection, &unused)) {
        // Restore the original bytes before refusing a partially applied mod.
        std::memcpy(target, expected.data(), patch.size);
        FlushInstructionCache(GetCurrentProcess(), target, patch.size);
        VirtualProtect(target, patch.size, protection, &unused);
        return false;
    }
    return true;
}

bool install() {
    if (!supported_host()) {
        OutputDebugStringW(L"DinoTale 32:9: unsupported executable. Requires the stock Windows 0.1.8 build; no changes made.\n");
        return false;
    }
    host_image = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(host_image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) { return false; }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(host_image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { return false; }
    const std::span<const uint8_t> image{host_image, nt->OptionalHeader.SizeOfImage};
    if (!dinotale32x9::matches(image) || !dinotale32x9::visibility_guards_match(image)
            || !dinotale32x9::background_guards_match(image) || !dinotale32x9::camera_guards_match(image)) { return false; }
    if (!prepare_visibility_hooks()) { return false; }
    size_t applied = 0;
    for (; applied < dinotale32x9::patches.size(); ++applied) {
        if (!write_patch(dinotale32x9::patches[applied], false)) {
            while (applied > 0) { write_patch(dinotale32x9::patches[--applied], true); }
            free_visibility_trampolines();
            return false;
        }
    }
    for (size_t i=0; i<visibility_hooks.size(); ++i) {
        if (!write_patch(visibility_hooks[i].patch, false)) {
            while (i>0) { write_patch(visibility_hooks[--i].patch, true); visibility_hooks[i].applied = false; }
            while (applied>0) { write_patch(dinotale32x9::patches[--applied], true); }
            free_visibility_trampolines();
            return false;
        }
        visibility_hooks[i].applied = true;
    }
    for (size_t i=0; i<background_hooks.size(); ++i) {
        if (!write_patch(background_hooks[i].patch,false)) {
            while (i>0) { write_patch(background_hooks[--i].patch,true); background_hooks[i].applied=false; }
            for (auto& hook : visibility_hooks) { if (hook.applied) { write_patch(hook.patch,true); hook.applied=false; } }
            while (applied>0) { write_patch(dinotale32x9::patches[--applied],true); }
            free_visibility_trampolines();
            return false;
        }
        background_hooks[i].applied = true;
    }
    OutputDebugStringW(L"DinoTale 32:9: enabled on stock Windows 0.1.8; executable on disk unchanged.\n");
    installed = true;
    return true;
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        return install() ? TRUE : FALSE;
    }
    if (reason == DLL_PROCESS_DETACH && installed && reserved == nullptr) {
        for (auto it=background_hooks.rbegin(); it!=background_hooks.rend(); ++it) {
            if (it->applied) { write_patch(it->patch,true); it->applied=false; }
        }
        for (auto it=visibility_hooks.rbegin(); it!=visibility_hooks.rend(); ++it) {
            if (it->applied) { write_patch(it->patch, true); it->applied = false; }
        }
        for (auto it = dinotale32x9::patches.rbegin(); it != dinotale32x9::patches.rend(); ++it) {
            write_patch(*it, true);
        }
        installed = false;
        free_visibility_trampolines();
    }
    return TRUE;
}
