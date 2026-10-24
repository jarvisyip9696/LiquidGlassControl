#include "../src/localization.h"
#include <cstdio>
int main(){
    const wchar_t* languageLabels[]={L"Interface language",L"界面语言",L"界面語言",L"Język interfejsu",L"Ngôn ngữ giao diện"};
    const wchar_t* entranceLabels[]={L"Entrance",L"入场",L"入場",L"Wejście",L"Xuất hiện"};
    for(int i=0;i<l10n::Count;++i){l10n::current=(l10n::Language)i;
        if(wcscmp(l10n::translate(L"界面语言",L"Interface language"),languageLabels[i])||wcscmp(l10n::translate(L"入场",L"Entrance"),entranceLabels[i]))return 8;
    }
    for(const auto&entry:l10n::catalog){
        if(l10n::findTranslation(entry.en)!=&entry)return 1;
        l10n::current=l10n::English;if(wcscmp(l10n::translate(L"test",entry.en),entry.en))return 2;
        l10n::current=l10n::Polish;if(wcscmp(l10n::translate(L"test",entry.en),entry.pl))return 3;
        l10n::current=l10n::Vietnamese;if(wcscmp(l10n::translate(L"test",entry.en),entry.vi))return 4;
    }
    l10n::current=l10n::Traditional;if(wcscmp(l10n::translate(L"程序设置",L"Program settings"),L"程式設定"))return 5;
    l10n::current=l10n::Simplified;if(wcscmp(l10n::translate(L"设置",L"Settings"),L"设置"))return 6;
    if(l10n::findTranslation(L"missing-key"))return 7;
    std::puts("PASS: all catalog lookups and English/Polish/Vietnamese results; simplified/traditional UI terms; missing-key lookup.");
}
