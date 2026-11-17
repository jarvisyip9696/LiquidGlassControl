#include "../src/localization.h"
#include <cstdio>

int main() {
    static_assert(l10n::Count == 3);
    static_assert(sizeof(l10n::codes) / sizeof(*l10n::codes) == l10n::Count);
    static_assert(sizeof(l10n::names) / sizeof(*l10n::names) == l10n::Count);
    const wchar_t* labels[] = {L"Interface language", L"界面语言", L"界面語言"};
    for (int i = 0; i < l10n::Count; ++i) {
        l10n::current = (l10n::Language)i;
        if (wcscmp(l10n::translate(L"界面语言", L"Interface language"), labels[i]))
            return 1;
    }
    wchar_t directory[MAX_PATH]{}, file[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, directory) || !GetTempFileNameW(directory, L"l10", 0, file))
        return 2;
    bool passed = l10n::read(file) == l10n::Simplified;
    for (int i = 0; i < l10n::Count; ++i) {
        l10n::current = (l10n::Language)i;
        l10n::save(file);
        passed = passed && l10n::read(file) == l10n::current;
    }
    const wchar_t* legacy[] = {L"zh-TW", L"zh-HK", L"zh-Hant", L"zh-CN", L"pl", L"vi", L"unknown"};
    for (int i = 0; i < 7; ++i) {
        passed = WritePrivateProfileStringW(L"UI", L"Language", legacy[i], file) && passed;
        passed = passed && l10n::read(file) == (i < 3    ? l10n::Traditional
                                                : i == 3 ? l10n::Simplified
                                                         : l10n::English);
    }
    if (!DeleteFileW(file))
        return 3;
    passed = passed && l10n::read(file) == l10n::systemLanguage();
    if (!passed)
        return 4;
    std::puts("PASS: Chinese/English labels, preference round trips, legacy fallback and system default.");
}
