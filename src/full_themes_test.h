#pragma once

namespace full_themes {
inline bool regressionTest(const std::wstring& base) {
    auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    auto key = L"Software\\LiquidGlassWindow\\ThemeTests\\" + suffix;
    auto dir = base + L"theme-test-" + suffix + L"\\";
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
        auto custom = string(L"controlStyles[0].target", L"My existing custom rule");
        write(settings.h, custom);
        for (int kind = 0; kind < 3; ++kind) {
            auto original = string(L"theme", kind == 0 ? L"My original theme" : L"");
            auto state = disabled(kind == 1 ? 0 : 1);
            write(settings.h, original);
            write(root.h, state);
            passed = (apply(root.h, settings.h, kind, true, dir) == 0) &&
                     equal(read(settings.h, L"theme"), string(L"theme", theme(kind))) &&
                     equal(read(root.h, L"Disabled"), disabled(0)) &&
                     equal(read(settings.h, custom.name), custom) && passed;
            passed = (apply(root.h, settings.h, kind, true, dir) == 0) &&
                     equal(load(file(kind, dir), kind, L"theme"), original) &&
                     equal(load(file(kind, dir, true), kind, L"Disabled"), state) && passed;
            write(settings.h, string(L"theme", L"Changed by user"));
            passed = (apply(root.h, settings.h, kind, false, dir) == 8) && exists(kind, dir) && passed;
            write(settings.h, string(L"theme", theme(kind)));
            passed = (apply(root.h, settings.h, kind, false, dir, kind == 0) == 0) &&
                     equal(read(settings.h, L"theme"), original) &&
                     equal(read(settings.h, custom.name), custom) && passed;
            if (kind == 0)
                passed = pending(kind, dir) && equal(read(root.h, L"Disabled"), disabled(0)) &&
                         (release(root.h, kind, dir, false) == 0) && passed;
            passed = equal(read(root.h, L"Disabled"), state) && !pending(kind, dir) && passed;
        }
        RegDeleteValueW(settings.h, L"theme");
        RegDeleteValueW(root.h, L"Disabled");
        auto missingTheme = read(settings.h, L"theme"), missingState = read(root.h, L"Disabled");
        passed = (apply(root.h, settings.h, 1, true, dir) == 0) &&
                 (apply(root.h, settings.h, 1, false, dir) == 0) &&
                 equal(read(settings.h, L"theme"), missingTheme) &&
                 equal(read(root.h, L"Disabled"), missingState) && passed;
    } catch (...) {
        passed = false;
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
    for (int kind = 0; kind < 3; ++kind) {
        DeleteFileW(file(kind, dir).c_str());
        DeleteFileW(file(kind, dir, true).c_str());
    }
    RemoveDirectoryW(dir.c_str());
    return passed;
}
}
