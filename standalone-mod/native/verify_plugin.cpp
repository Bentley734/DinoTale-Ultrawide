#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
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
    bool companion_loaded = false;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, L"DinoTale.exe") == 0) { base = entry.modBaseAddr; }
            if (_wcsicmp(entry.szModule, L"DinoTale32x9.dll") == 0) { companion_loaded = true; }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (base == nullptr || !companion_loaded) { return 1; }
    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) { return 1; }
    bool good = true;
    for (const auto& patch : dinotale32x9::patches) {
        std::array<uint8_t,8> bytes{};
        SIZE_T count = 0;
        if (!ReadProcessMemory(process, base + patch.rva, bytes.data(), patch.size, &count) || count != patch.size
            || std::memcmp(bytes.data(), patch.replacement.data(), patch.size) != 0) {
            good = false;
        }
    }
    CloseHandle(process);
    if (good) { std::puts("Official host loaded companion DLL; all four in-memory patches verified"); }
    return good ? 0 : 1;
}
