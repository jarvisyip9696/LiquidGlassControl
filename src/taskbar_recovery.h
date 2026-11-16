#pragma once
#include <shellapi.h>

namespace taskbar_recovery {
template <class Read, class Write> bool restore(const std::wstring& path, Read read, Write write) {
    if (GetPrivateProfileIntW(L"TaskbarRecovery", L"Pending", 0, path.c_str())) {
        bool original = GetPrivateProfileIntW(L"TaskbarRecovery", L"Original", 0, path.c_str()) != 0;
        bool applied = GetPrivateProfileIntW(L"TaskbarRecovery", L"Applied", 0, path.c_str()) != 0;
        if (((read() & ABS_AUTOHIDE) != 0) == applied && !write(original))
            return false;
        WritePrivateProfileStringW(L"TaskbarRecovery", nullptr, nullptr, path.c_str());
    }
    WritePrivateProfileStringW(L"Taskbar", L"AutoHide", L"0", path.c_str());
    return true;
}

inline bool restore(const std::wstring& path) {
    auto read = []() {
        APPBARDATA a{};
        a.cbSize = sizeof(a);
        return (DWORD)SHAppBarMessage(ABM_GETSTATE, &a);
    };
    return restore(path, read, [&](bool hide) {
        APPBARDATA a{};
        a.cbSize = sizeof(a);
        a.hWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
        a.lParam = (read() & ~ABS_AUTOHIDE) | (hide ? ABS_AUTOHIDE : 0);
        SHAppBarMessage(ABM_SETSTATE, &a);
        return ((read() & ABS_AUTOHIDE) != 0) == hide;
    });
}
#ifdef LG_ENABLE_SELF_TEST
inline bool test(const std::wstring& path) {
    DWORD state = ABS_ALWAYSONTOP | ABS_AUTOHIDE;
    unsigned writes = 0;
    auto read = [&] {
        return state;
    };
    auto write = [&](bool hide) {
        ++writes;
        state = (state & ~ABS_AUTOHIDE) | (hide ? ABS_AUTOHIDE : 0);
        return true;
    };
    WritePrivateProfileStringW(L"TaskbarRecovery", L"Pending", L"1", path.c_str());
    WritePrivateProfileStringW(L"TaskbarRecovery", L"Original", L"0", path.c_str());
    WritePrivateProfileStringW(L"TaskbarRecovery", L"Applied", L"1", path.c_str());
    bool ok = restore(path, read, write) && state == ABS_ALWAYSONTOP && writes == 1 &&
              restore(path, read, write) && writes == 1;
    DeleteFileW(path.c_str());
    return ok;
}
#endif
}
