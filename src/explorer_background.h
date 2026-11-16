#pragma once
#include <stdexcept>

// This backend can open ONLY the File Explorer Styler settings key.
// It changes background brushes, never themes, dimensions, margins or taskbar settings.
namespace explorer_background {
constexpr wchar_t KeyPath[] = L"SOFTWARE\\Windhawk\\Engine\\Mods\\windows-11-file-explorer-styler";
constexpr size_t LegacyStyleCount = 8;

struct Value {
    std::wstring name;
    DWORD type = 0xffffffff;
    std::vector<BYTE> data;
};

struct Key {
    HKEY h{};

    ~Key() {
        if (h)
            RegCloseKey(h);
    }
};

inline Value read(HKEY k, const std::wstring& name) {
    Value v;
    v.name = name;
    DWORD n = 0;
    LONG r = RegQueryValueExW(k, name.c_str(), nullptr, &v.type, nullptr, &n);
    if (r == ERROR_FILE_NOT_FOUND) {
        v.type = 0xffffffff;
        return v;
    }
    if (r != ERROR_SUCCESS || n > 65536)
        throw std::runtime_error("read");
    v.data.resize(n);
    if (RegQueryValueExW(k, name.c_str(), nullptr, &v.type, v.data.data(), &n) != ERROR_SUCCESS)
        throw std::runtime_error("read");
    return v;
}

inline Value string(const std::wstring& name, const wchar_t* text) {
    Value v;
    v.name = name;
    v.type = REG_SZ;
    auto bytes = reinterpret_cast<const BYTE*>(text);
    v.data.assign(bytes, bytes + (wcslen(text) + 1) * sizeof(wchar_t));
    return v;
}

inline std::vector<Value> schema() {
    std::vector<Value> values(2);
    values[0].name = L"backgroundTranslucentEffect";
    values[1].name = L"backgroundTranslucentEffectRegion";
    for (size_t i = 0; i < LegacyStyleCount; ++i) {
        auto prefix = L"controlStyles[" + std::to_wstring(i) + L"]";
        Value target;
        target.name = prefix + L".target";
        values.push_back(std::move(target));
        Value style;
        style.name = prefix + L".styles[0]";
        values.push_back(std::move(style));
    }
    return values;
}

inline void write(HKEY k, const Value& v) {
    LONG r = v.type == 0xffffffff
                 ? RegDeleteValueW(k, v.name.c_str())
                 : RegSetValueExW(k, v.name.c_str(), 0, v.type, v.data.data(), (DWORD)v.data.size());
    if (r != ERROR_SUCCESS && !(r == ERROR_FILE_NOT_FOUND && v.type == 0xffffffff))
        throw std::runtime_error("write");
}

inline void io(HANDLE h, void* p, DWORD n, bool out) {
    DWORD count = 0;
    BOOL ok = out ? WriteFile(h, p, n, &count, nullptr) : ReadFile(h, p, n, &count, nullptr);
    if (!ok || count != n)
        throw std::runtime_error("backup");
}

inline std::vector<Value> load(const std::wstring& file) {
    auto values = schema();
    HANDLE h = CreateFileW(
        file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("no backup");
    try {
        DWORD magic = 0;
        io(h, &magic, 4, false);
        if (magic != 0x45424731)
            throw std::runtime_error("invalid backup");
        for (auto& v : values) {
            DWORD n = 0;
            io(h, &v.type, 4, false);
            io(h, &n, 4, false);
            if (n > 65536)
                throw std::runtime_error("invalid size");
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

inline void notify(HKEY root) {
    DWORD stamp = 0, n = 4;
    RegQueryValueExW(root, L"SettingsChangeTime", nullptr, nullptr, reinterpret_cast<BYTE*>(&stamp), &n);
    ++stamp;
    if (RegSetValueExW(root, L"SettingsChangeTime", 0, REG_DWORD, reinterpret_cast<BYTE*>(&stamp), 4) !=
        ERROR_SUCCESS)
        throw std::runtime_error("reload");
}

inline int restore(const std::wstring& dir) {
    try {
        Key root, settings;
        if (RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, KeyPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &root.h) !=
            ERROR_SUCCESS)
            return 3;
        if (RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &settings.h) !=
            ERROR_SUCCESS)
            return 3;
        auto file = dir + L"explorer-background-originals.bin";
        auto values = load(file);
        for (auto& value : values)
            write(settings.h, value);
        notify(root.h);
        for (auto& value : values) {
            auto actual = read(settings.h, value.name);
            if (actual.type != value.type || actual.data != value.data)
                return 5;
        }
        return DeleteFileW(file.c_str()) ? 0 : 4;
    } catch (...) {
        return 6;
    }
}
}