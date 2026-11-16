#pragma once

// Independent material switches for the two children of the native OSD plate.
namespace native_switches {
using native_osd::Value;
using native_osd::Key;
using native_osd::read;
using native_osd::write;
using native_osd::string;
using native_osd::io;

inline std::vector<Value> fields(DWORD first, DWORD second, int mask) {
    std::vector<Value> result;
    for (int kind = 0; kind < 2; ++kind) {
        auto prefix = L"controlStyles[" + std::to_wstring(kind ? second : first) + L"]";
        result.push_back(string(prefix + L".target",
                                kind ? L"Grid#ConfirmatorMainGrid > Grid#BrightnessConfirmator"
                                     : L"Grid#ConfirmatorMainGrid > Grid#VolumeConfirmator"));
        result.push_back(string(
            prefix + L".styles[0]",
            mask & (1 << kind)
                ? L"Background:=<WindhawkBlur BlurAmount=\"10\" TintColor=\"#50202C3C\"/>"
                : L"Background:=<SolidColorBrush Color=\"{ThemeResource SystemChromeMediumColor}\"/>"));
        result.push_back(string(prefix + L".styles[1]", L"CornerRadius=28"));
    }
    return result;
}

inline void save(const std::wstring& file, DWORD first, DWORD second, std::vector<Value>& values) {
    HANDLE h =
        CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("backup");
    try {
        DWORD magic = 0x4e535731;
        io(h, &magic, 4, true);
        io(h, &first, 4, true);
        io(h, &second, 4, true);
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

inline std::vector<Value> load(const std::wstring& file, DWORD& first, DWORD& second) {
    HANDLE h = CreateFileW(
        file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("backup");
    std::vector<Value> values;
    try {
        DWORD magic = 0;
        io(h, &magic, 4, false);
        io(h, &first, 4, false);
        io(h, &second, 4, false);
        if (magic != 0x4e535731 || first > 4095 || second > 4095 || first == second)
            throw std::runtime_error("index");
        values = fields(first, second, 0);
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

inline int run(int mask, bool motion, const std::wstring& dir, bool keepOff = false) {
    if (mask < 0 || mask > 3)
        return 10;
    try {
        Key root, settings;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          native_osd::Root,
                          0,
                          KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                          &root.h) != ERROR_SUCCESS)
            return 3;
        if (RegOpenKeyExW(root.h, L"Settings", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &settings.h) !=
            ERROR_SUCCESS)
            return 3;
        std::wstring backup = dir + L"native-osd-switches.bin",
                     plateBackup = dir + L"native-osd-originals.bin";
        DWORD first = 0, second = 0;
        std::vector<Value> originals;
        bool haveChildren = GetFileAttributesW(backup.c_str()) != INVALID_FILE_ATTRIBUTES;
        if (haveChildren)
            originals = load(backup, first, second);
        if (mask == 0 && !keepOff) {
            if (haveChildren) {
                bool deferred = false;
                auto values = style_slots::restoration(settings.h, originals, deferred);
                for (auto& v : values)
                    write(settings.h, v);
                for (auto& v : values) {
                    auto actual = read(settings.h, v.name);
                    if (actual.type != v.type || actual.data != v.data)
                        return 5;
                }
                explorer_background::notify(root.h);
                if (!deferred && !DeleteFileW(backup.c_str()))
                    return 4;
            }
            return GetFileAttributesW(plateBackup.c_str()) != INVALID_FILE_ATTRIBUTES
                       ? native_osd::run(true, dir)
                       : 0;
        }
        int result = native_osd::run(false, dir, motion);
        if (result)
            return result;
        if (!haveChildren) {
            DWORD slots[2]{};
            int found = 0;
            for (DWORD i = 0; i < 4096 && found < 2; ++i) {
                auto prefix = L"controlStyles[" + std::to_wstring(i) + L"]";
                auto target = read(settings.h, prefix + L".target");
                if (target.type == 0xffffffff || target.data.size() <= 2) {
                    auto extra = read(settings.h, prefix + L".styles[2]");
                    if (extra.data.size() > 2)
                        return 8;
                    slots[found++] = i;
                }
            }
            if (found < 2)
                return 8;
            first = slots[0];
            second = slots[1];
            for (auto& v : fields(first, second, mask))
                originals.push_back(read(settings.h, v.name));
            save(backup, first, second, originals);
        }
        DWORD plate = 0;
        native_osd::load(plateBackup, plate);
        auto clear =
            string(L"controlStyles[" + std::to_wstring(plate) + L"].styles[0]", L"Background=Transparent");
        write(settings.h, clear);
        auto desired = fields(first, second, mask);
        for (auto& v : desired)
            write(settings.h, v);
        explorer_background::notify(root.h);
        for (auto& v : desired) {
            auto actual = read(settings.h, v.name);
            if (actual.type != v.type || actual.data != v.data)
                return 5;
        }
        return 0;
    } catch (...) {
        return 6;
    }
}
}
