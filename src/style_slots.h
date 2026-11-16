#pragma once

// A blank target terminates Styler's list. Keep an inert comment while later rules are active.
namespace style_slots {
using explorer_background::Value;
using explorer_background::read;
inline constexpr wchar_t Inactive[] = L"// LiquidGlass inactive background slot";

inline bool targetName(const std::wstring& name, DWORD& index) {
    const std::wstring prefix = L"controlStyles[";
    if (name.compare(0, prefix.size(), prefix) != 0)
        return false;
    wchar_t* end = nullptr;
    unsigned long n = wcstoul(name.c_str() + prefix.size(), &end, 10);
    if (n > 4095 || !end || wcscmp(end, L"].target") != 0)
        return false;
    index = (DWORD)n;
    return true;
}

inline std::wstring text(const Value& v) {
    if (v.type != REG_SZ || v.data.empty())
        return {};
    std::wstring s(reinterpret_cast<const wchar_t*>(v.data.data()), v.data.size() / sizeof(wchar_t));
    auto zero = s.find(L'\0');
    if (zero != std::wstring::npos)
        s.resize(zero);
    return s;
}

inline bool inactive(const Value& v) {
    return text(v) == Inactive;
}

inline bool laterActive(HKEY settings, DWORD index, const std::vector<DWORD>& ignored) {
    for (DWORD i = 0;; ++i) {
        wchar_t name[16384]{};
        DWORD n = _countof(name);
        LONG result = RegEnumValueW(settings, i, name, &n, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("enumerate slots");
        DWORD candidate = 0;
        if (targetName(name, candidate) && candidate > index &&
            std::find(ignored.begin(), ignored.end(), candidate) == ignored.end()) {
            auto value = text(read(settings, name));
            if (!value.empty() && value.compare(0, 2, L"//") != 0)
                return true;
        }
    }
    return false;
}

inline std::vector<Value> restoration(HKEY settings, const std::vector<Value>& originals, bool& deferred) {
    std::vector<DWORD> owned;
    for (auto& v : originals) {
        DWORD index = 0;
        if (targetName(v.name, index))
            owned.push_back(index);
    }
    auto result = originals;
    deferred = false;
    for (auto& v : result) {
        DWORD index = 0;
        if (targetName(v.name, index) && text(v).empty() && laterActive(settings, index, owned)) {
            v = explorer_background::string(v.name, Inactive);
            deferred = true;
        }
    }
    return result;
}
}
