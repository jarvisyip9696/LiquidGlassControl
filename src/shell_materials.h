#pragma once

// Append narrowly scoped brush rules. Never select a theme or write layout settings.
namespace shell_materials {
using explorer_background::Value;
using explorer_background::Key;
using explorer_background::read;
using explorer_background::write;
using explorer_background::string;
using explorer_background::io;

struct Surface {
    const wchar_t* module;
    const wchar_t* journal;
    std::vector<const wchar_t*> targets;
};

inline const Surface& surface(int kind) {
    static const Surface surfaces[] = {
        {L"windows-11-taskbar-styler",
         L"taskbar-material-originals.bin",
         {L"Taskbar.TaskbarFrame > Grid#RootGrid > Taskbar.TaskbarBackground > Grid > Rectangle#BackgroundFill"}},
        {L"windows-11-start-menu-styler",
         L"start-menu-material-originals.bin",
         {L"Border#AcrylicBorder", L"Border#AcrylicOverlay"}},
        {L"windows-11-notification-center-styler",
         L"notification-material-originals.bin",
         {L"Grid#NotificationCenterGrid", L"Grid#CalendarCenterGrid"}}};
    if (kind < 0 || kind > 2)
        throw std::runtime_error("surface");
    return surfaces[kind];
}

inline std::wstring keyPath(int kind) {
    return std::wstring(L"SOFTWARE\\Windhawk\\Engine\\Mods\\") + surface(kind).module;
}

inline bool exists(int kind, const std::wstring& dir) {
    return GetFileAttributesW((dir + surface(kind).journal).c_str()) != INVALID_FILE_ATTRIBUTES;
}

inline bool available(int kind) {
    Key root;
    DWORD disabled = 1, n = sizeof(disabled);
    return RegOpenKeyExW(
               HKEY_LOCAL_MACHINE, keyPath(kind).c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &root.h) ==
               ERROR_SUCCESS &&
           RegQueryValueExW(root.h, L"Disabled", nullptr, nullptr, reinterpret_cast<BYTE*>(&disabled), &n) ==
               ERROR_SUCCESS &&
           !disabled;
}

inline std::vector<Value> fields(int kind, DWORD first) {
    std::vector<Value> values;
    auto& s = surface(kind);
    for (size_t i = 0; i < s.targets.size(); ++i) {
        auto p = L"controlStyles[" + std::to_wstring(first + i) + L"]";
        values.push_back(string(p + L".target", s.targets[i]));
        const wchar_t* brush = kind == 0 ? L"Fill:=<WindhawkBlur BlurAmount=\"12\" TintColor=\"#50202C3C\"/>"
                               : kind == 1 && i == 1
                                   ? L"Background=Transparent"
                                   : L"Background:=<WindhawkBlur BlurAmount=\"12\" TintColor=\"#50202C3C\"/>";
        values.push_back(string(p + L".styles[0]", brush));
    }
    return values;
}

inline bool equal(const Value& a, const Value& b) {
    return a.type == b.type && a.data == b.data;
}

inline bool vacant(const Value& v) {
    return v.type == 0xffffffff || (v.type == REG_SZ && v.data.size() <= sizeof(wchar_t));
}

inline void save(const std::wstring& file, int kind, DWORD first, const std::vector<Value>& originals) {
    HANDLE h =
        CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("journal create");
    try {
        DWORD magic = 0x53474c31, k = kind, count = (DWORD)originals.size();
        io(h, &magic, 4, true);
        io(h, &k, 4, true);
        io(h, &first, 4, true);
        io(h, &count, 4, true);
        for (auto& v : originals) {
            DWORD type = v.type, n = (DWORD)v.data.size();
            io(h, &type, 4, true);
            io(h, &n, 4, true);
            if (n)
                io(h, const_cast<BYTE*>(v.data.data()), n, true);
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

inline std::vector<Value> load(const std::wstring& file, int kind, DWORD& first) {
    HANDLE h = CreateFileW(
        file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("journal open");
    try {
        DWORD magic = 0, k = 0, count = 0;
        io(h, &magic, 4, false);
        io(h, &k, 4, false);
        io(h, &first, 4, false);
        io(h, &count, 4, false);
        if (magic != 0x53474c31 || k != (DWORD)kind || first + surface(kind).targets.size() > 4096 ||
            count != surface(kind).targets.size() * 2)
            throw std::runtime_error("journal header");
        auto values = fields(kind, first);
        for (auto& v : values) {
            DWORD n = 0;
            io(h, &v.type, 4, false);
            io(h, &n, 4, false);
            if (n > 65536)
                throw std::runtime_error("journal size");
            v.data.resize(n);
            if (n)
                io(h, v.data.data(), n, false);
        }
        CloseHandle(h);
        return values;
    } catch (...) {
        CloseHandle(h);
        throw;
    }
}

inline bool configured(int kind, const std::wstring& dir) {
    try {
        if (!exists(kind, dir))
            return false;
        DWORD first = 0;
        load(dir + surface(kind).journal, kind, first);
        Key root, settings;
        if (RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, keyPath(kind).c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &root.h) !=
                ERROR_SUCCESS ||
            RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE, &settings.h) != ERROR_SUCCESS)
            return false;
        auto desired = fields(kind, first);
        return equal(read(settings.h, desired[0].name), desired[0]);
    } catch (...) {
        return false;
    }
}

inline bool slotVacant(HKEY settings, DWORD index) {
    auto prefix = L"controlStyles[" + std::to_wstring(index) + L"].";
    for (DWORD i = 0;; ++i) {
        wchar_t name[16384]{};
        DWORD n = _countof(name);
        LONG result = RegEnumValueW(settings, i, name, &n, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("enumerate");
        if (std::wstring(name).compare(0, prefix.size(), prefix) == 0 && !vacant(read(settings, name)))
            return false;
    }
    return true;
}

// Takes an already opened key so regression tests can use disposable HKCU fixtures.
inline int apply(HKEY root, HKEY settings, int kind, bool on, const std::wstring& dir) {
    try {
        auto file = dir + surface(kind).journal;
        bool have = exists(kind, dir);
        DWORD first = 0;
        std::vector<Value> originals;
        if (!on && !have)
            return 0;
        if (have)
            originals = load(file, kind, first);
        else {
            for (; first < 4096; ++first)
                if (vacant(read(settings, L"controlStyles[" + std::to_wstring(first) + L"].target")))
                    break;
            if (first + surface(kind).targets.size() > 4096)
                return 8;
            for (size_t i = 0; i < surface(kind).targets.size(); ++i)
                if (!slotVacant(settings, first + (DWORD)i))
                    return 8;
            for (auto& v : fields(kind, first))
                originals.push_back(read(settings, v.name));
            save(file, kind, first, originals);
        }
        auto desired = fields(kind, first);
        // Another tool may have edited our slots. Preserve its work and the recovery file.
        for (size_t i = 0; i < desired.size(); ++i) {
            auto actual = read(settings, desired[i].name);
            DWORD slot = 0;
            bool placeholder = style_slots::targetName(actual.name, slot) && style_slots::inactive(actual);
            if (!equal(actual, desired[i]) && !equal(actual, originals[i]) && !placeholder)
                return 8;
        }
        bool deferred = false;
        auto values = on ? desired : style_slots::restoration(settings, originals, deferred);
        for (auto& v : values)
            write(settings, v);
        for (auto& v : values)
            if (!equal(read(settings, v.name), v))
                return 5;
        explorer_background::notify(root);
        if (!on && !deferred && !DeleteFileW(file.c_str()))
            return 4;
        return 0;
    } catch (...) {
        return 6;
    }
}

inline int run(int kind, bool on, const std::wstring& dir) {
    try {
        if (!on && !exists(kind, dir))
            return 0;
        Key root, settings;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          keyPath(kind).c_str(),
                          0,
                          KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                          &root.h) != ERROR_SUCCESS ||
            RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &settings.h) !=
                ERROR_SUCCESS)
            return 3;
        if (on) {
            DWORD disabled = 1, n = 4;
            if (RegQueryValueExW(
                    root.h, L"Disabled", nullptr, nullptr, reinterpret_cast<BYTE*>(&disabled), &n) !=
                    ERROR_SUCCESS ||
                disabled)
                return 7;
        }
        return apply(root.h, settings.h, kind, on, dir);
    } catch (...) {
        return 6;
    }
}

inline int cleanupTaskbar(const std::wstring& dir) {
    try {
        if (configured(0, dir))
            return 0;
        Key root, settings;
        bool plate =
                 GetFileAttributesW((dir + L"native-osd-originals.bin").c_str()) != INVALID_FILE_ATTRIBUTES,
             children =
                 GetFileAttributesW((dir + L"native-osd-switches.bin").c_str()) != INVALID_FILE_ATTRIBUTES;
        if (plate || children) {
            if (RegOpenKeyExW(
                    HKEY_LOCAL_MACHINE, native_osd::Root, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &root.h) !=
                    ERROR_SUCCESS ||
                RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE, &settings.h) != ERROR_SUCCESS)
                return 3;
            if (plate) {
                DWORD index = 0;
                native_osd::load(dir + L"native-osd-originals.bin", index);
                auto desired = native_osd::desired(index);
                if (equal(read(settings.h, desired[0].name), desired[0]))
                    return 0;
            }
            if (children) {
                DWORD a = 0, b = 0;
                native_switches::load(dir + L"native-osd-switches.bin", a, b);
                auto desired = native_switches::fields(a, b, 0);
                for (int i : {0, 3})
                    if (equal(read(settings.h, desired[i].name), desired[i]))
                        return 0;
            }
            int code = native_switches::run(0, false, dir);
            if (code)
                return code;
        }
        return run(0, false, dir);
    } catch (...) {
        return 6;
    }
}

inline int nativeOperation(int mask, bool motion, const std::wstring& dir) {
    int code = native_switches::run(mask, motion, dir);
    return code ? code : cleanupTaskbar(dir);
}

inline int operation(int kind, bool on, const std::wstring& dir) {
    int code = run(kind, on, dir);
    return code || kind != 0 ? code : cleanupTaskbar(dir);
}
}
