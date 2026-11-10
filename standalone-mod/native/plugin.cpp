#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "patch_plan.hpp"

extern "C" {
__declspec(dllexport) uint32_t recomp_api_version = 1;
__declspec(dllexport) void dinotale32x9_marker(uint8_t*, void*) {}
}

namespace {
uint8_t* host_image = nullptr;
bool installed = false;

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
    if (!dinotale32x9::matches({host_image, nt->OptionalHeader.SizeOfImage})) { return false; }
    size_t applied = 0;
    for (; applied < dinotale32x9::patches.size(); ++applied) {
        if (!write_patch(dinotale32x9::patches[applied], false)) {
            while (applied > 0) { write_patch(dinotale32x9::patches[--applied], true); }
            return false;
        }
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
        for (auto it = dinotale32x9::patches.rbegin(); it != dinotale32x9::patches.rend(); ++it) {
            write_patch(*it, true);
        }
        installed = false;
    }
    return TRUE;
}
