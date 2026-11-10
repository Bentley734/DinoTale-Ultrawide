#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <vector>
#include "patch_plan.hpp"

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) { return 2; }
    if (std::wcscmp(argv[1], L"--reject") == 0) {
        HMODULE module = LoadLibraryExW(argv[2], nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
        if (module != nullptr) { FreeLibrary(module); return 1; }
        if (GetLastError() != ERROR_DLL_INIT_FAILED) { return 1; }
        std::puts("Unsupported host rejected before installation");
        return 0;
    }
    if (std::wcscmp(argv[1], L"--verify") != 0) { return 2; }
    const DWORD pid = DWORD(std::wcstoul(argv[2], nullptr, 10));
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) { return 1; }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    uint8_t* base = nullptr;
    uint8_t* companion = nullptr;
    size_t companion_size = 0;
    bool companion_loaded = false;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, L"DinoTale.exe") == 0) { base = entry.modBaseAddr; }
            if (_wcsicmp(entry.szModule, L"DinoTale32x9.dll") == 0) {
                companion_loaded = true;
                companion = entry.modBaseAddr;
                companion_size = entry.modBaseSize;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (base == nullptr || !companion_loaded) { return 1; }
    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) { return 1; }
    bool good = true;
    for (const auto& patch : dinotale32x9::patches) {
        std::array<uint8_t,32> bytes{};
        SIZE_T count = 0;
        if (!ReadProcessMemory(process, base + patch.rva, bytes.data(), patch.size, &count) || count != patch.size
            || std::memcmp(bytes.data(), patch.replacement.data(), patch.size) != 0) {
            good = false;
        }
    }
    const std::array hooks{
        std::pair{dinotale32x9::visibility_rvas[0], std::span<const uint8_t>{dinotale32x9::visibility_prologue}},
        std::pair{dinotale32x9::visibility_rvas[1], std::span<const uint8_t>{dinotale32x9::visibility_prologue}},
        std::pair{dinotale32x9::background_copy_rva, std::span<const uint8_t>{dinotale32x9::background_copy_prologue}},
        std::pair{dinotale32x9::background_cover_rva, std::span<const uint8_t>{dinotale32x9::background_cover_prologue}},
        std::pair{dinotale32x9::camera_wall_rva, std::span<const uint8_t>{dinotale32x9::camera_wall_prologue}}
    };
    for (const auto& [rva, prologue] : hooks) {
        std::array<uint8_t,32> bytes{};
        SIZE_T count = 0;
        if (!ReadProcessMemory(process, base+rva, bytes.data(), prologue.size(), &count) || count != prologue.size()) { good = false; continue; }
        uintptr_t destination = 0;
        std::memcpy(&destination, bytes.data()+6, sizeof(destination));
        const auto expected = dinotale32x9::make_function_jump(rva, prologue, destination);
        if (std::memcmp(bytes.data(), expected.replacement.data(), prologue.size()) != 0
                || destination < reinterpret_cast<uintptr_t>(companion)
                || destination >= reinterpret_cast<uintptr_t>(companion)+companion_size) { good = false; }
    }
    auto read = [&](size_t offset, void* data, size_t size) {
        SIZE_T count = 0;
        return offset <= companion_size && size <= companion_size-offset
            && ReadProcessMemory(process, companion+offset, data, size, &count) && count == size;
    };
    uint64_t visibility_checks = 0, wide_checks = 0, background_tiles = 0, masks_cleared = 0;
    uint64_t camera_steps = 0, eased_camera_steps = 0;
    bool checks_found = false, wide_found = false, tiles_found = false, masks_found = false;
    bool camera_found = false, eased_camera_found = false;
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    IMAGE_EXPORT_DIRECTORY exports{};
    if (read(0, &dos, sizeof(dos)) && dos.e_lfanew>0 && read(size_t(dos.e_lfanew), &nt, sizeof(nt))
            && read(nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, &exports, sizeof(exports))
            && exports.NumberOfNames<64 && exports.NumberOfFunctions<64) {
        std::vector<DWORD> names(exports.NumberOfNames), functions(exports.NumberOfFunctions);
        std::vector<WORD> ordinals(exports.NumberOfNames);
        if (read(exports.AddressOfNames, names.data(), names.size()*sizeof(DWORD))
                && read(exports.AddressOfNameOrdinals, ordinals.data(), ordinals.size()*sizeof(WORD))
                && read(exports.AddressOfFunctions, functions.data(), functions.size()*sizeof(DWORD))) {
            for (size_t i=0; i<names.size(); ++i) {
                char name[64]{};
                if (!read(names[i], name, sizeof(name)-1) || ordinals[i]>=functions.size()) { continue; }
                if (std::strcmp(name, "dinotale32x9_visibility_checks") == 0) {
                    checks_found = read(functions[ordinals[i]], &visibility_checks, sizeof(visibility_checks));
                }
                if (std::strcmp(name, "dinotale32x9_wide_visibility_checks") == 0) {
                    wide_found = read(functions[ordinals[i]], &wide_checks, sizeof(wide_checks));
                }
                if (std::strcmp(name, "dinotale32x9_background_tiles") == 0) {
                    tiles_found = read(functions[ordinals[i]], &background_tiles, sizeof(background_tiles));
                }
                if (std::strcmp(name, "dinotale32x9_background_masks_cleared") == 0) {
                    masks_found = read(functions[ordinals[i]], &masks_cleared, sizeof(masks_cleared));
                }
                if (std::strcmp(name, "dinotale32x9_camera_steps") == 0) {
                    camera_found = read(functions[ordinals[i]], &camera_steps, sizeof(camera_steps));
                }
                if (std::strcmp(name, "dinotale32x9_eased_camera_steps") == 0) {
                    eased_camera_found = read(functions[ordinals[i]], &eased_camera_steps, sizeof(eased_camera_steps));
                }
            }
        }
    }
    if (!checks_found || !wide_found || !tiles_found || !masks_found || !camera_found || !eased_camera_found) { good = false; }
    CloseHandle(process);
    if (good) { std::puts("Official host loaded companion DLL; five patches and all five placement/background/camera hooks verified"); }
    if (checks_found && wide_found) {
        std::printf("Placement checks: %llu; expanded-coordinate checks: %llu\n", static_cast<unsigned long long>(visibility_checks), static_cast<unsigned long long>(wide_checks));
    }
    if (tiles_found && masks_found) {
        std::printf("Extended background tiles: %llu; qualified masks cleared: %llu\n", static_cast<unsigned long long>(background_tiles), static_cast<unsigned long long>(masks_cleared));
    }
    if (camera_found && eased_camera_found) {
        std::printf("Normal camera steps: %llu; eased camera steps: %llu\n", static_cast<unsigned long long>(camera_steps), static_cast<unsigned long long>(eased_camera_steps));
    }
    return good ? 0 : 1;
}
