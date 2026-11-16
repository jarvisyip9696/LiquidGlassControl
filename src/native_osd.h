#pragma once
#include "style_slots.h"

// Taskbar Styler also hosts HWConfirmatorUI. Only its named native OSD plate
// is targeted. Theme, taskbar selectors and every layout property are untouched.
namespace native_osd {
constexpr wchar_t Root[] = L"SOFTWARE\\Windhawk\\Engine\\Mods\\windows-11-taskbar-styler";
constexpr wchar_t Target[] = L"Grid#ConfirmatorMainGrid";
using explorer_background::Value;
using explorer_background::Key;
using explorer_background::read;
using explorer_background::write;
using explorer_background::string;
using explorer_background::io;

inline std::vector<Value> desired(DWORD index) {
    auto prefix = L"controlStyles[" + std::to_wstring(index) + L"]";
    return {
        string(prefix + L".target", Target),
        string(prefix + L".styles[0]",
               L"Background:=<WindhawkBlur BlurAmount=\"10\" TintColor=\"#50202C3C\"/>"),
        string(
            prefix + L".styles[1]",
            L"BorderBrush:=<LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"1,1\"><GradientStop Color=\"#A0FFFFFF\" Offset=\"0\"/><GradientStop Color=\"#207CACD8\" Offset=\"0.5\"/><GradientStop Color=\"#60FFFFFF\" Offset=\"1\"/></LinearGradientBrush>"),
        string(
            prefix + L".styles[2]",
            L"Transitions:=<TransitionCollection><EntranceThemeTransition FromHorizontalOffset=\"0\" FromVerticalOffset=\"10\" IsStaggeringEnabled=\"False\"/><RepositionThemeTransition IsStaggeringEnabled=\"False\"/></TransitionCollection>")};
}

inline void save(const std::wstring& file, DWORD index, std::vector<Value>& values) {
    HANDLE h =
        CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("backup create");
    try {
        DWORD magic = 0x4e4f5332;
        io(h, &magic, 4, true);
        io(h, &index, 4, true);
        for (auto& v : values) {
            DWORD n = (DWORD)v.data.size();
            io(h, &v.type, 4, true);
            io(h, &n, 4, true);
            if (n)
                io(h, v.data.data(), n, true);
        }
        if (!FlushFileBuffers(h))
            throw std::runtime_error("flush");
        CloseHandle(h);
    } catch (...) {
        CloseHandle(h);
        DeleteFileW(file.c_str());
        throw;
    }
}

inline std::vector<Value> load(const std::wstring& file, DWORD& index) {
    HANDLE h = CreateFileW(
        file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("no backup");
    std::vector<Value> values;
    try {
        DWORD magic = 0;
        io(h, &magic, 4, false);
        io(h, &index, 4, false);
        if ((magic != 0x4e4f5331 && magic != 0x4e4f5332) || index > 4095)
            throw std::runtime_error("bad backup");
        values = desired(index);
        if (magic == 0x4e4f5331)
            values.resize(3);
        for (auto& v : values) {
            DWORD n = 0;
            io(h, &v.type, 4, false);
            io(h, &n, 4, false);
            if (n > 65536)
                throw std::runtime_error("size");
            v.data.resize(n);
            if (n)
                io(h, v.data.data(), n, false);
        }
        CloseHandle(h);
    } catch (...) {
        CloseHandle(h);
        throw;
    }
    return values;
}

inline int run(bool restore, const std::wstring& dir, bool animate = true) {
    try {
        Key root, settings;
        if (RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, Root, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &root.h) !=
            ERROR_SUCCESS)
            return 3;
        if (RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &settings.h) !=
            ERROR_SUCCESS)
            return 3;
        auto file = dir + L"native-osd-originals.bin";
        DWORD index = 0;
        std::vector<Value> originals;
        if (restore || GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            originals = load(file, index);
        else {
            DWORD disabled = 1, n = 4;
            RegQueryValueExW(root.h, L"Disabled", nullptr, nullptr, reinterpret_cast<BYTE*>(&disabled), &n);
            if (disabled)
                return 7;
            for (; index < 4096; ++index) {
                auto v = read(settings.h, L"controlStyles[" + std::to_wstring(index) + L"].target");
                if (v.type == 0xffffffff || v.data.size() <= sizeof(wchar_t))
                    break;
            }
            if (index == 4096)
                return 8;
            auto values = desired(index);
            for (auto& v : values)
                originals.push_back(read(settings.h, v.name));
            // A blank selector should not have hidden nonempty styles following it.
            auto extra = read(settings.h, L"controlStyles[" + std::to_wstring(index) + L"].styles[3]");
            if (extra.data.size() > sizeof(wchar_t))
                return 8;
            save(file, index, originals);
        }
        if (!restore && originals.size() == 3) {
            originals.push_back(read(settings.h, desired(index)[3].name));
            auto temporary = file + L".upgrade." + std::to_wstring(GetCurrentProcessId()) + L"." +
                             std::to_wstring(GetTickCount64());
            save(temporary, index, originals);
            if (!MoveFileExW(
                    temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                DeleteFileW(temporary.c_str());
                throw std::runtime_error("backup upgrade");
            }
        }
        BOOL systemAnimations = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &systemAnimations, 0);
        bool deferred = false;
        auto values = restore ? style_slots::restoration(settings.h, originals, deferred) : desired(index);
        if (!restore && (!animate || !systemAnimations))
            values[3] = originals[3];
        for (auto& v : values)
            write(settings.h, v);
        explorer_background::notify(root.h);
        for (auto& v : values) {
            auto current = read(settings.h, v.name);
            if (current.type != v.type || current.data != v.data)
                return 5;
        }
        if (restore && !deferred && !DeleteFileW(file.c_str()))
            return 4;
        return 0;
    } catch (...) {
        return 6;
    }
}
}
