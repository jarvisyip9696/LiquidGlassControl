#pragma once
#include "app_version.h"
#include <windows.h>
#include <string>
#include <map>

namespace l10n {
enum Language { English, Simplified, Traditional, Count };

inline Language current = English;
inline const wchar_t* codes[] = {L"en", L"zh", L"zh-Hant"};
inline const wchar_t* names[] = {L"English", L"简体中文", L"繁體中文"};

// First launch follows the Windows display language; anything not translated falls back to English.
inline Language systemLanguage() {
    LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)) {
    case LANG_CHINESE: {
        WORD sub = SUBLANGID(id);
        return sub == SUBLANG_CHINESE_TRADITIONAL || sub == SUBLANG_CHINESE_HONGKONG ||
                       sub == SUBLANG_CHINESE_MACAU
                   ? Traditional
                   : Simplified;
    }
    default:
        return English;
    }
}

inline Language read(const std::wstring& path) {
    wchar_t code[32]{};
    GetPrivateProfileStringW(L"UI", L"Language", L"", code, 32, path.c_str());
    if (!code[0])
        return GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES
                   ? systemLanguage()
                   : Simplified; /* settings from an older version without a choice kept the old default */
    for (int i = 0; i < Count; ++i)
        if (_wcsicmp(code, codes[i]) == 0)
            return (Language)i;
    if (_wcsicmp(code, L"zh-TW") == 0 || _wcsicmp(code, L"zh-HK") == 0 || _wcsicmp(code, L"zh-Hant") == 0)
        return Traditional;
    if (_wcsicmp(code, L"zh-CN") == 0)
        return Simplified;
    return English;
}

inline void save(const std::wstring& path) {
    WritePrivateProfileStringW(L"UI", L"Language", codes[current], path.c_str());
}

inline void replace(std::wstring& s, const wchar_t* from, const wchar_t* to) {
    size_t at = 0;
    while ((at = s.find(from, at)) != std::wstring::npos) {
        s.replace(at, wcslen(from), to);
        at += wcslen(to);
    }
}

inline const wchar_t* translate(const wchar_t* zh, const wchar_t* en) {
    if (current == English)
        return en;
    if (current == Simplified)
        return zh;
    if (current == Traditional) {
        static std::map<std::wstring, std::wstring> cache;
        auto cached = cache.find(zh);
        if (cached != cache.end())
            return cached->second.c_str();
        int characterCount =
            LCMapStringEx(L"zh-TW", LCMAP_TRADITIONAL_CHINESE, zh, -1, nullptr, 0, nullptr, nullptr, 0);
        std::wstring text;
        if (characterCount > 0) {
            text.resize(characterCount);
            LCMapStringEx(
                L"zh-TW", LCMAP_TRADITIONAL_CHINESE, zh, -1, &text[0], characterCount, nullptr, nullptr, 0);
            text.pop_back();
        } else
            text = zh;
        replace(text, L"任務欄", L"工作列");
        replace(text, L"資源管理器", L"檔案總管");
        replace(text, L"窗口", L"視窗");
        replace(text, L"設置", L"設定");
        replace(text, L"托盤", L"系統匣");
        replace(text, L"默認", L"預設");
        replace(text, L"接口", L"介面");
        replace(text, L"禁用", L"停用");
        replace(text, L"程序", L"程式");
        replace(text, L"軟件", L"軟體");
        replace(text, L"鼠標", L"滑鼠");
        replace(text, L"內存", L"記憶體");
        return cache.emplace(zh, std::move(text)).first->second.c_str();
    }
    return en;
}
}
