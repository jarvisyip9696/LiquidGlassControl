#pragma once
// SPDX-License-Identifier: GPL-3.0-only
// Adapted from m417z, Windows 11 Taskbar Styler 1.10, registry compatibility
// hooks (user-supplied source). See third_party/windhawk/README.md and COPYING.
// Windhawk hook installation is replaced with statically linked MinHook.
#include <windows.h>
#include "../third_party/minhook/include/MinHook.h"

namespace xaml_subscription {
inline thread_local bool subscribing = false;
inline decltype(&RegOpenKeyExW) originalOpen = nullptr;
inline decltype(&RegQueryValueExW) originalQuery = nullptr;

inline LSTATUS WINAPI openKey(HKEY key, LPCWSTR sub, DWORD options, REGSAM access, PHKEY result) {
    LSTATUS status = originalOpen(key, sub, options, access, result);
    if (status == ERROR_SUCCESS || !subscribing || key != HKEY_LOCAL_MACHINE || !sub ||
        _wcsicmp(sub, L"Software\\Microsoft\\XAML\\Debug") != 0)
        return status;
    // The XAML key often doesn't exist. This is a read-only substitute handle;
    // nothing is written to the real registry, as in the upstream implementation.
    return originalOpen(HKEY_LOCAL_MACHINE, L"Software\\Microsoft", options, access, result);
}

inline LSTATUS WINAPI
queryValue(HKEY key, LPCWSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD bytes) {
    if (!subscribing || !name || _wcsicmp(name, L"DisableCompositionDiag") != 0)
        return originalQuery(key, name, reserved, type, data, bytes);
    if (type)
        *type = REG_DWORD;
    if (data && (!bytes || *bytes < sizeof(DWORD))) {
        if (bytes)
            *bytes = sizeof(DWORD);
        return ERROR_MORE_DATA;
    }
    if (data)
        *reinterpret_cast<DWORD*>(data) = 1;
    if (bytes)
        *bytes = sizeof(DWORD);
    return ERROR_SUCCESS;
}

inline INIT_ONCE once = INIT_ONCE_STATIC_INIT;
inline HRESULT initialization = E_PENDING;

inline BOOL CALLBACK initialize(PINIT_ONCE, void*, void**) {
    HMODULE base = GetModuleHandleW(L"kernelbase.dll");
    auto open = base ? GetProcAddress(base, "RegOpenKeyExW") : nullptr;
    auto query = base ? GetProcAddress(base, "RegQueryValueExW") : nullptr;
    if (!open || !query) {
        initialization = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
        return TRUE;
    }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        initialization = E_FAIL;
        return TRUE;
    }
    status = MH_CreateHook((void*)open, (void*)&openKey, (void**)&originalOpen);
    if (status != MH_OK) {
        initialization = E_FAIL;
        return TRUE;
    }
    status = MH_CreateHook((void*)query, (void*)&queryValue, (void**)&originalQuery);
    if (status != MH_OK) {
        MH_RemoveHook((void*)open);
        initialization = E_FAIL;
        return TRUE;
    }
    // Enable only hooks owned by this engine, never MH_ALL_HOOKS.
    status = MH_EnableHook((void*)open);
    if (status == MH_OK) {
        status = MH_EnableHook((void*)query);
        if (status != MH_OK)
            MH_DisableHook((void*)open);
    }
    if (status != MH_OK) {
        MH_RemoveHook((void*)query);
        MH_RemoveHook((void*)open);
        initialization = E_FAIL;
        return TRUE;
    }
    HMODULE pinned{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       (LPCWSTR)&initialize,
                       &pinned);
    initialization = S_OK;
    return TRUE;
}

class Scope {
    bool previous;

  public:
    explicit Scope(IUnknown* = nullptr) : previous(subscribing) {
        InitOnceExecuteOnce(&once, initialize, nullptr, nullptr);
        if (SUCCEEDED(initialization))
            subscribing = true;
    }

    ~Scope() {
        subscribing = previous;
    }

    HRESULT status() const {
        return initialization;
    }

    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;
};

// Invoked only by the isolated self-test process, never against the live shell.
inline HRESULT test() {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return E_FAIL;
    auto module = GetModuleHandleW(L"kernelbase.dll");
    auto query = (decltype(&RegQueryValueExW))GetProcAddress(module, "RegQueryValueExW");
    if (!query) {
        RegCloseKey(key);
        return E_FAIL;
    }
    DWORD before{}, bytes = 4;
    auto baseline = query(key, L"DisableCompositionDiag", nullptr, nullptr, (BYTE*)&before, &bytes);
    bool ok = true;
    {
        Scope scope;
        ok = SUCCEEDED(scope.status());
        DWORD type{}, value{}, size = 4;
        ok = ok &&
             query(key, L"DisableCompositionDiag", nullptr, &type, (BYTE*)&value, &size) == ERROR_SUCCESS &&
             type == REG_DWORD && value == 1 && size == 4;
        BYTE shortBuffer = 0x5a;
        size = 1;
        ok =
            ok &&
            query(key, L"DisableCompositionDiag", nullptr, nullptr, &shortBuffer, &size) == ERROR_MORE_DATA &&
            shortBuffer == 0x5a && size == 4;

        struct Context {
            HKEY key;
            decltype(query) function;
            LSTATUS expected;
            DWORD value;
            bool ok = false;
        } ctx{key, query, baseline, before};

        HANDLE thread = CreateThread(
            nullptr,
            0,
            [](void* p) -> DWORD {
                auto& c = *(Context*)p;
                DWORD value{}, size = 4;
                auto result =
                    c.function(c.key, L"DisableCompositionDiag", nullptr, nullptr, (BYTE*)&value, &size);
                c.ok = result == c.expected && (result != ERROR_SUCCESS || value == c.value);
                return 0;
            },
            &ctx,
            0,
            nullptr);
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
            ok = ctx.ok && ok;
        } else
            ok = false;
    }
    DWORD after{};
    bytes = 4;
    auto result = query(key, L"DisableCompositionDiag", nullptr, nullptr, (BYTE*)&after, &bytes);
    RegCloseKey(key);
    return ok && result == baseline && (result != ERROR_SUCCESS || before == after) ? S_OK : E_FAIL;
}
}
