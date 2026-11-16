#pragma once

// Select the installed Windhawk theme, exactly as its theme picker does.
// Existing user styles are retained. Module activation is shared with native indicator overrides.
namespace full_themes {
using shell_materials::Value;
using shell_materials::Key;
using shell_materials::read;
using shell_materials::write;
using shell_materials::string;
using shell_materials::io;
using shell_materials::equal;

inline const wchar_t* theme(int kind) {
    return kind == 2 ? L"LiquidGlass" : L"LiquidGlass2";
}

inline std::wstring file(int kind, const std::wstring& dir, bool module = false) {
    const wchar_t* names[] = {L"taskbar", L"start-menu", L"notification"};
    if (kind < 0 || kind > 2)
        throw std::runtime_error("kind");
    return dir + names[kind] + (module ? L"-module-originals.bin" : L"-theme-originals.bin");
}

inline bool exists(int kind, const std::wstring& dir) {
    return GetFileAttributesW(file(kind, dir).c_str()) != INVALID_FILE_ATTRIBUTES;
}

inline bool pending(int kind, const std::wstring& dir) {
    return exists(kind, dir) || GetFileAttributesW(file(kind, dir, true).c_str()) != INVALID_FILE_ATTRIBUTES;
}

inline Value disabled(DWORD state) {
    Value v;
    v.name = L"Disabled";
    v.type = REG_DWORD;
    auto p = reinterpret_cast<BYTE*>(&state);
    v.data.assign(p, p + 4);
    return v;
}

inline void save(const std::wstring& path, int kind, const Value& v) {
    HANDLE h =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("theme backup");
    try {
        DWORD magic = 0x46544831, k = kind, type = v.type, n = (DWORD)v.data.size();
        io(h, &magic, 4, true);
        io(h, &k, 4, true);
        io(h, &type, 4, true);
        io(h, &n, 4, true);
        if (n)
            io(h, const_cast<BYTE*>(v.data.data()), n, true);
        if (!FlushFileBuffers(h))
            throw std::runtime_error("theme flush");
        CloseHandle(h);
    } catch (...) {
        CloseHandle(h);
        DeleteFileW(path.c_str());
        throw;
    }
}

inline Value load(const std::wstring& path, int kind, const wchar_t* name) {
    HANDLE h = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("theme backup missing");
    try {
        Value v;
        v.name = name;
        DWORD magic = 0, k = 0, n = 0;
        io(h, &magic, 4, false);
        io(h, &k, 4, false);
        io(h, &v.type, 4, false);
        io(h, &n, 4, false);
        if (magic != 0x46544831 || k != (DWORD)kind || n > 65536)
            throw std::runtime_error("theme backup invalid");
        v.data.resize(n);
        if (n)
            io(h, v.data.data(), n, false);
        CloseHandle(h);
        return v;
    } catch (...) {
        CloseHandle(h);
        throw;
    }
}

inline void enable(HKEY root, int kind, const std::wstring& dir) {
    auto path = file(kind, dir, true);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        save(path, kind, read(root, L"Disabled"));
    else
        load(path, kind, L"Disabled");
    write(root, disabled(0));
    if (!equal(read(root, L"Disabled"), disabled(0)))
        throw std::runtime_error("enable");
}

inline int release(HKEY root, int kind, const std::wstring& dir, bool keep) {
    auto path = file(kind, dir, true);
    if (keep || exists(kind, dir) || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return 0;
    auto original = load(path, kind, L"Disabled");
    write(root, original);
    explorer_background::notify(root);
    if (!equal(read(root, L"Disabled"), original))
        return 5;
    return DeleteFileW(path.c_str()) ? 0 : 4;
}

inline int
apply(HKEY root, HKEY settings, int kind, bool on, const std::wstring& dir, bool keepModule = false) {
    try {
        auto path = file(kind, dir);
        bool have = exists(kind, dir);
        auto chosen = string(L"theme", theme(kind));
        if (on) {
            if (!have)
                save(path, kind, read(settings, L"theme"));
            auto original = load(path, kind, L"theme");
            auto current = read(settings, L"theme");
            if (!equal(current, chosen) && !equal(current, original))
                return 8;
            write(settings, chosen);
            enable(root, kind, dir);
        } else if (have) {
            auto original = load(path, kind, L"theme");
            auto current = read(settings, L"theme");
            if (!equal(current, chosen) && !equal(current, original))
                return 8;
            write(settings, original);
            if (!equal(read(settings, L"theme"), original))
                return 5;
            if (!DeleteFileW(path.c_str()))
                return 4;
        }
        explorer_background::notify(root);
        if (on && !equal(read(settings, L"theme"), chosen))
            return 5;
        return on ? 0 : release(root, kind, dir, keepModule);
    } catch (...) {
        return 6;
    }
}

inline bool configured(int kind, const std::wstring& dir) {
    try {
        if (!exists(kind, dir))
            return false;
        Key root, settings;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          shell_materials::keyPath(kind).c_str(),
                          0,
                          KEY_QUERY_VALUE | KEY_WOW64_64KEY,
                          &root.h) != ERROR_SUCCESS ||
            RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE, &settings.h) != ERROR_SUCCESS)
            return false;
        return equal(read(settings.h, L"theme"), string(L"theme", theme(kind)));
    } catch (...) {
        return false;
    }
}

inline bool nativeActive(const std::wstring& dir) {
    bool plate = GetFileAttributesW((dir + L"native-osd-originals.bin").c_str()) != INVALID_FILE_ATTRIBUTES,
         children = GetFileAttributesW((dir + L"native-osd-switches.bin").c_str()) != INVALID_FILE_ATTRIBUTES;
    if (!plate && !children)
        return false;
    Key root, settings;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, native_osd::Root, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &root.h) !=
            ERROR_SUCCESS ||
        RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE, &settings.h) != ERROR_SUCCESS)
        throw std::runtime_error("native state");
    if (children) {
        DWORD a = 0, b = 0;
        native_switches::load(dir + L"native-osd-switches.bin", a, b);
        auto values = native_switches::fields(a, b, 0);
        for (int i : {0, 3})
            if (equal(read(settings.h, values[i].name), values[i]))
                return true;
    }
    if (plate) {
        DWORD slot = 0;
        native_osd::load(dir + L"native-osd-originals.bin", slot);
        auto values = native_osd::desired(slot);
        if (equal(read(settings.h, values[0].name), values[0]))
            return true;
    }
    return false;
}

inline int releaseModule(int kind, const std::wstring& dir) {
    try {
        if (!pending(kind, dir))
            return 0;
        Key root;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          shell_materials::keyPath(kind).c_str(),
                          0,
                          KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                          &root.h) != ERROR_SUCCESS)
            return 3;
        return release(
            root.h, kind, dir, kind == 0 && (nativeActive(dir) || shell_materials::configured(0, dir)));
    } catch (...) {
        return 6;
    }
}

inline int nativeMask(const std::wstring& dir) {
    try {
        if (GetFileAttributesW((dir + L"native-osd-switches.bin").c_str()) == INVALID_FILE_ATTRIBUTES)
            return nativeActive(dir) ? 3 : 0;
        Key root, settings;
        if (RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, native_osd::Root, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &root.h) !=
                ERROR_SUCCESS ||
            RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE, &settings.h) != ERROR_SUCCESS)
            return 0;
        DWORD a = 0, b = 0;
        native_switches::load(dir + L"native-osd-switches.bin", a, b);
        auto fields = native_switches::fields(a, b, 3);
        return (equal(read(settings.h, fields[1].name), fields[1]) ? 1 : 0) |
               (equal(read(settings.h, fields[4].name), fields[4]) ? 2 : 0);
    } catch (...) {
        return 0;
    }
}

inline int nativeOperation(int mask, bool motion, const std::wstring& dir, bool forceRestore = false) {
    try {
        bool keepOff = !forceRestore && configured(0, dir);
        if (mask || keepOff) {
            Key root;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                              native_osd::Root,
                              0,
                              KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                              &root.h) != ERROR_SUCCESS)
                return 3;
            enable(root.h, 0, dir);
            explorer_background::notify(root.h);
        }
        int code = native_switches::run(mask, motion, dir, keepOff);
        if (code)
            return code;
        code = shell_materials::cleanupTaskbar(dir);
        return code ? code : releaseModule(0, dir);
    } catch (...) {
        return 6;
    }
}

inline int operation(int kind, bool on, const std::wstring& dir) {
    try {
        if (shell_materials::exists(kind, dir)) {
            if (on)
                return 8;
            int code = shell_materials::operation(kind, false, dir);
            if (code)
                return code;
        }
        if (!on && !pending(kind, dir))
            return 0;
        Key root, settings;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          shell_materials::keyPath(kind).c_str(),
                          0,
                          KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                          &root.h) != ERROR_SUCCESS ||
            RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &settings.h) !=
                ERROR_SUCCESS)
            return 3;
        int code = apply(root.h, settings.h, kind, on, dir, kind == 0 && nativeActive(dir));
        if (code)
            return code;
        if (kind == 0) {
            auto ini = dir + L"settings.ini";
            int prior = nativeMask(dir);
            int mask = (GetPrivateProfileIntW(L"Native", L"VolumeOn", prior & 1, ini.c_str()) ? 1 : 0) |
                       (GetPrivateProfileIntW(L"Native", L"BrightnessOn", prior & 2, ini.c_str()) ? 2 : 0);
            code =
                nativeOperation(mask, GetPrivateProfileIntW(L"Native", L"Motion", 1, ini.c_str()) != 0, dir);
            if (code)
                return code;
        }
        return releaseModule(kind, dir);
    } catch (...) {
        return 6;
    }
}
}
