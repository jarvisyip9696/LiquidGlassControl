#pragma once

namespace shell_materials {
inline bool regressionTest(const std::wstring& base) {
    auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    auto key = L"Software\\LiquidGlassWindow\\Tests\\" + suffix;
    auto dir = base + L"shell-material-test-" + suffix + L"\\";
    bool passed = true;
    if (!CreateDirectoryW(dir.c_str(), nullptr))
        return false;
    try {
        Key root, settings;
        if (RegCreateKeyExW(
                HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &root.h, nullptr) !=
                ERROR_SUCCESS ||
            RegCreateKeyExW(
                root.h, L"Settings", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &settings.h, nullptr) !=
                ERROR_SUCCESS)
            throw std::runtime_error("fixture");
        write(settings.h, string(L"theme", L"Keep my custom theme"));
        write(settings.h, string(L"controlStyles[0].target", L"Existing.Control"));
        write(settings.h, string(L"controlStyles[0].styles[0]", L"Margin=77,0,0,0"));
        write(settings.h, string(L"controlStyles[1].target", L""));
        write(settings.h, string(L"controlStyles[1].styles[0]", L""));
        Value layout;
        layout.name = L"taskbarHeight";
        layout.type = REG_DWORD;
        DWORD height = 48;
        layout.data.assign(reinterpret_cast<BYTE*>(&height), reinterpret_cast<BYTE*>(&height) + 4);
        write(settings.h, layout);
        auto snapshot = [&]() {
            std::map<std::wstring, Value> values;
            for (DWORD i = 0;; ++i) {
                wchar_t name[16384]{};
                DWORD n = _countof(name);
                LONG result = RegEnumValueW(settings.h, i, name, &n, nullptr, nullptr, nullptr, nullptr);
                if (result == ERROR_NO_MORE_ITEMS)
                    break;
                if (result != ERROR_SUCCESS)
                    throw std::runtime_error("fixture enumerate");
                values.emplace(name, read(settings.h, name));
            }
            return values;
        };
        auto sameSnapshot = [&](const std::map<std::wstring, Value>& before) {
            auto after = snapshot();
            if (after.size() != before.size())
                return false;
            for (auto& entry : before) {
                auto found = after.find(entry.first);
                if (found == after.end() || !equal(entry.second, found->second))
                    return false;
            }
            return true;
        };
        auto before = snapshot();
        for (int kind = 0; kind < 3; ++kind) {
            passed = (apply(root.h, settings.h, kind, true, dir) == 0) && passed;
            DWORD first = 0;
            auto original = load(dir + surface(kind).journal, kind, first);
            passed = (first == 1 && original.size() == fields(kind, 1).size()) && passed;
            for (auto& v : fields(kind, 1))
                passed = equal(read(settings.h, v.name), v) && passed;
            passed = equal(read(settings.h, L"theme"), before.at(L"theme")) &&
                     equal(read(settings.h, L"taskbarHeight"), layout) && passed;
            passed = (apply(root.h, settings.h, kind, true, dir) == 0) && passed;
            DWORD again = 0;
            auto reloaded = load(dir + surface(kind).journal, kind, again);
            for (size_t i = 0; i < original.size(); ++i)
                passed = equal(original[i], reloaded[i]) && passed;
            auto styles = fields(kind, 1);
            write(settings.h, string(styles[1].name, L"User edited this slot"));
            passed = (apply(root.h, settings.h, kind, false, dir) == 8) && exists(kind, dir) && passed;
            write(settings.h, styles[1]);
            passed = (apply(root.h, settings.h, kind, false, dir) == 0) && sameSnapshot(before) &&
                     !exists(kind, dir) && passed;
            passed = (apply(root.h, settings.h, kind, false, dir) == 0) && passed;
        }
        // Taskbar first, native indicators later: switching taskbar Off must not truncate them.
        passed = (apply(root.h, settings.h, 0, true, dir) == 0) && passed;
        auto laterTarget = string(L"controlStyles[2].target", L"Grid#VolumeConfirmator");
        auto laterStyle = string(L"controlStyles[2].styles[0]", L"Background=Transparent");
        write(settings.h, laterTarget);
        write(settings.h, laterStyle);
        passed = (apply(root.h, settings.h, 0, false, dir) == 0) && exists(0, dir) &&
                 style_slots::inactive(read(settings.h, L"controlStyles[1].target")) &&
                 equal(read(settings.h, laterTarget.name), laterTarget) && passed;
        passed = (apply(root.h, settings.h, 0, true, dir) == 0) && passed;
        passed = (apply(root.h, settings.h, 0, false, dir) == 0) && passed;
        RegDeleteValueW(settings.h, laterTarget.name.c_str());
        RegDeleteValueW(settings.h, laterStyle.name.c_str());
        passed = (apply(root.h, settings.h, 0, false, dir) == 0) && sameSnapshot(before) && !exists(0, dir) &&
                 passed;
        // Native indicators first, taskbar later: restoring the native slots preserves taskbar.
        std::vector<Value> nativeOriginals;
        for (int slot : {1, 2}) {
            auto prefix = L"controlStyles[" + std::to_wstring(slot) + L"]";
            nativeOriginals.push_back(read(settings.h, prefix + L".target"));
            nativeOriginals.push_back(read(settings.h, prefix + L".styles[0]"));
            write(settings.h, string(prefix + L".target", L"Grid#VolumeConfirmator"));
            write(settings.h, string(prefix + L".styles[0]", L"Background=Transparent"));
        }
        passed = (apply(root.h, settings.h, 0, true, dir) == 0) && passed;
        bool deferred = false;
        auto restored = style_slots::restoration(settings.h, nativeOriginals, deferred);
        for (auto& v : restored)
            write(settings.h, v);
        passed = deferred && style_slots::inactive(read(settings.h, L"controlStyles[1].target")) &&
                 equal(read(settings.h, L"controlStyles[3].target"), fields(0, 3)[0]) && passed;
        passed = (apply(root.h, settings.h, 0, false, dir) == 0) && passed;
        restored = style_slots::restoration(settings.h, nativeOriginals, deferred);
        for (auto& v : restored)
            write(settings.h, v);
        passed = !deferred && sameSnapshot(before) && passed;
        write(settings.h, string(L"controlStyles[1].styles[3]", L"Height=999"));
        passed = (apply(root.h, settings.h, 0, true, dir) == 8) && !exists(0, dir) && passed;
        RegDeleteValueW(settings.h, L"controlStyles[1].styles[3]");
        passed = sameSnapshot(before) && passed;
        auto corrupt = dir + surface(0).journal;
        HANDLE file = CreateFileW(
            corrupt.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("corrupt fixture");
        DWORD invalid = 0, n = 0;
        WriteFile(file, &invalid, 4, &n, nullptr);
        CloseHandle(file);
        passed = (apply(root.h, settings.h, 0, false, dir) == 6) && sameSnapshot(before) && exists(0, dir) &&
                 passed;
    } catch (...) {
        passed = false;
    }
    // Delete only this uniquely named test fixture; never touch the live Windhawk keys.
    if (RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str()) != ERROR_SUCCESS)
        passed = false;
    for (int kind = 0; kind < 3; ++kind) {
        auto file = dir + surface(kind).journal;
        if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(file.c_str()))
            passed = false;
    }
    if (!RemoveDirectoryW(dir.c_str()))
        passed = false;
    return passed;
}
}
