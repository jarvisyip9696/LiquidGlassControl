#pragma once
#include "app_version.h"
#include <windows.h>
#include <string>
#include <map>
#include <array>
#include <algorithm>
namespace l10n {
enum Language {English,Simplified,Traditional,Polish,Vietnamese,Count};
inline Language current=English;
inline const wchar_t* codes[]={L"en",L"zh",L"zh-Hant",L"pl",L"vi"};
inline const wchar_t* names[]={L"English",L"简体中文",L"繁體中文",L"Polski",L"Tiếng Việt"};
inline Language read(const std::wstring&path){wchar_t code[32]{};GetPrivateProfileStringW(L"UI",L"Language",L"en",code,32,path.c_str());for(int i=0;i<Count;++i)if(_wcsicmp(code,codes[i])==0)return (Language)i;if(_wcsicmp(code,L"zh-TW")==0||_wcsicmp(code,L"zh-HK")==0)return Traditional;if(_wcsicmp(code,L"zh-CN")==0)return Simplified;return English;}
inline void save(const std::wstring&path){WritePrivateProfileStringW(L"UI",L"Language",codes[current],path.c_str());}
struct Text{const wchar_t*en;const wchar_t*pl;const wchar_t*vi;};
inline const Text catalog[]={
{L"Cannot start LGEngine preview. Error: ",L"Nie można uruchomić podglądu LGEngine. Błąd: ",L"Không thể mở bản xem trước LGEngine. Lỗi: "},
{L"LGEngine motion and indicators",L"Animacje i wskaźniki LGEngine",L"Chuyển động và chỉ báo LGEngine"},
{L"Volume / brightness OSD in background",L"Wskaźnik głośności / jasności w tle",L"Chỉ báo âm lượng / độ sáng trong nền"},
{L"Settings save automatically. When enabled, the OSD also works on other pages and in the tray.",L"Ustawienia zapisują się automatycznie. Włączony wskaźnik działa też na innych stronach i w zasobniku.",L"Cài đặt tự động lưu. Khi bật, chỉ báo vẫn hoạt động ở trang khác và trong khay hệ thống."},
{L"Reset material sliders",L"Resetuj suwaki materiału",L"Đặt lại thanh trượt vật liệu"},
{L"Keyboard layout switcher",L"Przełącznik układu klawiatury",L"Chuyển bố cục bàn phím"},
{L"LGEngine preview did not connect. Check the adapter and Windows application control.",L"Podgląd LGEngine nie połączył się. Sprawdź adapter i kontrolę aplikacji Windows.",L"LGEngine chưa kết nối. Kiểm tra bộ điều hợp và kiểm soát ứng dụng Windows."},
{L"LGEngine optical preview",L"Podgląd szkła LGEngine",L"Xem trước kính LGEngine"},
{L"Cannot start LGEngine preview. Build the optional adapter first. Error: ",L"Nie można uruchomić podglądu LGEngine. Najpierw zbuduj opcjonalny adapter. Błąd: ",L"Không thể mở bản xem trước LGEngine. Hãy dựng bộ điều hợp tùy chọn trước. Lỗi: "},
{L"LGEngine preview connected; optical controls affect the preview only.",L"Podgląd LGEngine połączony; parametry optyczne dotyczą tylko podglądu.",L"Đã kết nối LGEngine; thông số quang học chỉ áp dụng cho bản xem trước."},
{L"LGEngine preview started; waiting for the engine host...",L"Podgląd LGEngine uruchomiony; czekam na hosta silnika...",L"Đã khởi động LGEngine; đang chờ máy chủ của engine..."},
{L"LGEngine host blocked by Smart App Control (Windows application control). The glass indicator stays off; Windows' own pop-up is used.",L"Host LGEngine zablokowany przez Inteligentną kontrolę aplikacji (kontrola aplikacji Windows). Szklany wskaźnik pozostaje wyłączony; używane jest własne okienko Windows.",L"Máy chủ LGEngine bị Kiểm soát ứng dụng thông minh (kiểm soát ứng dụng Windows) chặn. Chỉ báo kính vẫn tắt; Windows dùng cửa sổ bật lên của riêng nó."},
{L"LGEngine host unavailable; details in lgengine\\host-client.log. Turn Indicator off and on to retry.",L"Host LGEngine niedostępny; szczegóły w lgengine\\host-client.log. Wyłącz i włącz Wskaźnik, aby ponowić.",L"LGEngine host không khả dụng; chi tiết trong lgengine\\host-client.log. Tắt rồi bật Chỉ báo để thử lại."},{L"LGEngine preview stopped unexpectedly; it restarts shortly.",L"Podgląd LGEngine nieoczekiwanie się zatrzymał; wkrótce uruchomi się ponownie.",L"Bản xem trước LGEngine dừng bất ngờ; sẽ sớm khởi động lại."},{L"LGEngine runtime not found (no lgengine folder next to the app). The LGEngine page and the indicator need the full portable ZIP.",L"Nie znaleziono środowiska LGEngine (brak folderu lgengine obok aplikacji). Strona LGEngine i wskaźnik wymagają pełnego przenośnego pliku ZIP.",L"Không tìm thấy LGEngine (không có thư mục lgengine cạnh ứng dụng). Trang LGEngine và chỉ báo cần bản ZIP di động đầy đủ."},
{L"About",L"Informacje",L"Giới thiệu"},
{L" · Applied",L" · Zastosowano",L" · Đã áp dụng"},
{L"Windows application-control policy blocked the engine. A trusted signed or administrator-approved build is required.",L"Zasady kontroli aplikacji Windows zablokowały silnik. Wymagana jest zaufana podpisana lub zatwierdzona przez administratora wersja.",L"Chính sách kiểm soát ứng dụng Windows đã chặn công cụ. Cần bản dựng có chữ ký đáng tin cậy hoặc được quản trị viên phê duyệt."},
{L" · Fallback",L" · Materiał zastępczy",L" · Chất liệu dự phòng"},
{L" · Acrylic (fixed blur)",L" · Akryl (stałe rozmycie)",L" · Acrylic (độ mờ cố định)"},
{L" · Applied (custom blur)",L" · Zastosowano (własne rozmycie)",L" · Đã áp dụng (độ mờ tùy chỉnh)"},


{L" · Paused",L" · Wstrzymano",L" · Tạm dừng"},{L"Effects paused by battery saving or system accessibility settings.",L"Efekty wstrzymane przez oszczędzanie energii lub ustawienia ułatwień dostępu.",L"Hiệu ứng tạm dừng do tiết kiệm pin hoặc cài đặt trợ năng hệ thống."},
{L"Dock hover animation",L"Animacja Dock",L"Hoạt ảnh Dock"},
{L"Blur strength: ",L"Siła rozmycia: ",L"Độ mờ: "},
{L"Taskbar restore failed; recovery record retained.",L"Nie przywrócono paska; zachowano dane odzyskiwania.",L"Không khôi phục được thanh tác vụ; đã giữ bản ghi khôi phục."},
{L"Liquid Glass",L"Liquid Glass",L"Liquid Glass"},
{L"Apply",L"Zastosuj",L"Áp dụng"},{L"Restore",L"Przywróć",L"Khôi phục"},
{L"Refresh windows",L"Odśwież okna",L"Làm mới cửa sổ"},{L"Hide to tray",L"Ukryj w zasobniku",L"Ẩn xuống khay"},
{L"App glass",L"Szkło aplikacji",L"Kính ứng dụng"},{L"System glass",L"Szkło systemowe",L"Kính hệ thống"},
{L"Settings",L"Ustawienia",L"Cài đặt"},{L"General",L"Ogólne",L"Chung"},{L"Glass",L"Szkło",L"Hiệu ứng kính"},
{L"Interface language",L"Język interfejsu",L"Ngôn ngữ giao diện"},{L"Interface color",L"Kolor interfejsu",L"Màu giao diện"},
{L"Volume indicator",L"Wskaźnik głośności",L"Chỉ báo âm lượng"},{L"Brightness indicator",L"Wskaźnik jasności",L"Chỉ báo độ sáng"},
{L"Volume glass",L"Szkło głośności",L"Kính âm lượng"},{L"Brightness glass",L"Szkło jasności",L"Kính độ sáng"},
{L"Taskbar",L"Pasek zadań",L"Thanh tác vụ"},{L"Start menu",L"Menu Start",L"Menu Bắt đầu"},
{L"Notification center and calendar",L"Powiadomienia i kalendarz",L"Thông báo và lịch"},{L"File Explorer",L"Eksplorator plików",L"Trình khám phá tệp"},
{L"Taskbar glass",L"Szkło paska zadań",L"Kính thanh tác vụ"},{L"Start menu glass",L"Szkło menu Start",L"Kính menu Bắt đầu"},
{L"Notification center glass",L"Szkło powiadomień",L"Kính thông báo"},{L"Explorer glass",L"Szkło Eksploratora",L"Kính trình khám phá"},
{L"On",L"Wł.",L"Bật"},{L"Off",L"Wył.",L"Tắt"},
{L" · Connected",L" · Połączono",L" · Đã kết nối"},{L" · Connecting",L" · Łączenie",L" · Đang kết nối"},{L" · Not applied",L" · Nie zastosowano",L" · Chưa áp dụng"},
{L" · Waiting for the indicator",L" · Czeka na wskaźnik",L" · Đang chờ chỉ báo"},{L" · Hidden by Liquid Glass",L" · Ukryte przez Liquid Glass",L" · Bị Liquid Glass ẩn"},{L" · Replaced by the LGEngine indicator",L" · Zastąpione wskaźnikiem LGEngine",L" · Được thay bằng chỉ báo LGEngine"},
{L"Liquid Glass is already showing its own glass volume indicator, so this one stays off. Turn off the glass indicator in Liquid Glass to use this one.",L"Liquid Glass już pokazuje własny szklany wskaźnik głośności, więc ten pozostaje wyłączony. Aby używać tego, wyłącz szklany wskaźnik w Liquid Glass.",L"Liquid Glass đang hiển thị chỉ báo âm lượng kính của riêng nó, nên chỉ báo này vẫn tắt. Để dùng chỉ báo này, hãy tắt chỉ báo kính trong Liquid Glass."},
{L"Tint strength (lower is clearer)",L"Siła zabarwienia (mniej = większa przejrzystość)",L"Độ đậm màu (thấp hơn sẽ trong hơn)"},
{L"Panel corner radius",L"Zaokrąglenie panelu",L"Bán kính bo góc"},{L"Glass tint",L"Odcień szkła",L"Tông màu kính"},
{L" corner radius",L" promień narożników",L" bán kính góc"},
{L"Follow system",L"Zgodnie z systemem",L"Theo hệ thống"},{L"Light",L"Jasny",L"Sáng"},{L"Dark",L"Ciemny",L"Tối"},
{L"Classic blue",L"Klasyczny niebieski",L"Xanh cổ điển"},{L"Violet",L"Fioletowy",L"Tím"},{L"Jade",L"Jadeitowy",L"Xanh ngọc"},
{L"Warm orange",L"Ciepły pomarańczowy",L"Cam ấm"},{L"Rose",L"Różany",L"Hồng"},{L"Lake blue",L"Błękit jeziora",L"Xanh hồ"},{L"Graphite",L"Grafitowy",L"Than chì"},
{L"Rounded",L"Zaokrąglone",L"Bo tròn"},{L"Small rounded",L"Lekko zaokrąglone",L"Bo nhẹ"},{L"Square",L"Proste",L"Góc vuông"},{L"No backdrop",L"Bez materiału tła",L"Không có nền"},
{L"Selected app (by executable name)",L"Wybrana aplikacja (nazwa procesu)",L"Ứng dụng đã chọn (theo tên tiến trình)"},
{L"File Explorer windows",L"Okna Eksploratora plików",L"Cửa sổ Trình khám phá tệp"},{L"All apps (content not guaranteed)",L"Wszystkie aplikacje (wg zgodności)",L"Tất cả ứng dụng (tùy mức tương thích)"},
{L" [whole-window backend]  —  ",L" [materiał okna]  —  ",L" [chất liệu cửa sổ]  —  "},{L" [frame only/unverified]  —  ",L" [tylko ramka/niezweryfikowane]  —  ",L" [chỉ khung/chưa xác minh]  —  "},
{L"Reduce material cost on battery or in battery saver",L"Ogranicz efekty na baterii lub w trybie oszczędzania",L"Giảm hiệu ứng khi dùng pin hoặc tiết kiệm pin"},
{L"Fade and gently slide the app in at startup",L"Łagodne pojawianie się aplikacji przy uruchamianiu",L"Hiện dần và trượt nhẹ khi khởi động"},
{L"Restore and exit",L"Przywróć i zamknij",L"Khôi phục và thoát"},{L"Open settings",L"Otwórz ustawienia",L"Mở cài đặt"},
{L"Liquid Glass · Native indicators",L"Liquid Glass · Wskaźniki systemowe",L"Liquid Glass · Chỉ báo hệ thống"},
{L"Choose an app and Apply; Restore reverts the effect.\r\nSystem glass runs independently; custom-drawn content can remain opaque.",L"Wybierz aplikację i kliknij Zastosuj. Przywróć cofa efekt.\r\nSzkło systemowe działa niezależnie. Własna zawartość aplikacji może pozostać nieprzezroczysta.",L"Chọn ứng dụng rồi nhấn Áp dụng; Khôi phục sẽ hoàn tác hiệu ứng.\r\nKính hệ thống hoạt động độc lập; nội dung tự vẽ có thể vẫn đục."},
{L"Changes are saved and applied; taskbar geometry is preserved.\r\nIndependent blur and tint controls; Dock hover has a separate switch.",L"Zmiany są zapisywane i stosowane bez zmiany rozmiaru paska zadań.\r\nOddzielne suwaki rozmycia i barwy; osobny przełącznik animacji Docka.",L"Thay đổi được lưu và áp dụng; giữ nguyên kích thước thanh tác vụ.\r\nĐiều chỉnh độ mờ và màu riêng; Dock có công tắc riêng."},
{L"Language, color and startup animation preferences are saved automatically.\r\nClosing settings keeps the app running in the tray.",L"Język, kolor i animacja uruchamiania są zapisywane automatycznie.\r\nPo zamknięciu okna aplikacja działa w zasobniku.",L"Ngôn ngữ, màu sắc và tùy chọn chuyển động được lưu tự động.\r\nĐóng cửa sổ vẫn giữ ứng dụng chạy trong khay hệ thống."},
{L"Liquid Glass " LG_VERSION_W L"\r\nStandalone Windows materials, distributed as one EXE.\r\n\r\nEvent driven, with no periodic background scans or idle repaints.\r\nIcon cache is pruned; XAML nodes use weak references.\r\nOptional low-cost material fallback on battery.\r\nFull optical refraction is not implemented.",L"Liquid Glass " LG_VERSION_W L"\r\nNiezależne materiały Windows w jednym pliku EXE.\r\n\r\nDziałanie oparte na zdarzeniach, bez okresowego skanowania i odświeżania w bezczynności.\r\nPamięć ikon jest porządkowana; węzły XAML używają słabych odwołań.\r\nOpcjonalne uproszczone materiały na baterii.\r\nPełne załamanie światła nie jest zaimplementowane.",L"Liquid Glass " LG_VERSION_W L"\r\nChất liệu Windows độc lập, phân phối bằng một tệp EXE.\r\n\r\nHoạt động theo sự kiện, không quét định kỳ hay vẽ lại khi nhàn rỗi.\r\nDọn bộ nhớ đệm biểu tượng; nút XAML dùng tham chiếu yếu.\r\nCó chất liệu dự phòng nhẹ hơn khi dùng pin.\r\nChưa hỗ trợ khúc xạ quang học đầy đủ."},
{L"Standalone materials · No Windhawk required · System layout preserved\r\nSettings > Glass: tint, opacity, corners and power saving.",L"Niezależne materiały · Bez Windhawk · Oryginalny układ systemu\r\nUstawienia > Szkło: odcień, krycie, narożniki i oszczędzanie energii.",L"Chất liệu độc lập · Không cần Windhawk · Giữ bố cục hệ thống\r\nCài đặt > Hiệu ứng kính: màu, độ đậm, góc và tiết kiệm pin."},
{L"Choose an app or a page. Language: Settings > General. Materials: Glass.",L"Wybierz aplikację lub kartę. Język: Ustawienia > Ogólne. Materiały: Szkło.",L"Chọn ứng dụng hoặc thẻ. Ngôn ngữ: Cài đặt > Chung. Chất liệu: Hiệu ứng kính."},
{L"Choose an app or a page. Language: Settings > General. Materials: System glass > Material.",L"Wybierz aplikację lub kartę. Język: Ustawienia > Ogólne. Materiały: Szkło systemowe > Materiał.",L"Chọn ứng dụng hoặc thẻ. Ngôn ngữ: Cài đặt > Chung. Chất liệu: Kính hệ thống > Chất liệu."},
{L"Standalone materials · No Windhawk required · System layout preserved\r\nSystem glass > Material: background adaptation, opacity, corners and power saving.",L"Niezależne materiały · Bez Windhawk · Oryginalny układ systemu\r\nSzkło systemowe > Materiał: dopasowanie do tła, krycie, narożniki i oszczędzanie energii.",L"Chất liệu độc lập · Không cần Windhawk · Giữ bố cục hệ thống\r\nKính hệ thống > Chất liệu: thích ứng nền, độ đậm, góc và tiết kiệm pin."},
{L"Explorer: standalone XAML materials; classic content may remain opaque.",L"Eksplorator: niezależne materiały XAML; klasyczna zawartość może pozostać nieprzezroczysta.",L"Trình khám phá: chất liệu XAML độc lập; nội dung cổ điển có thể vẫn đục."},
{L"Other apps: frame material only; Chrome / Electron content usually stays opaque.",L"Inne aplikacje: tylko materiał ramki; zawartość Chrome / Electron zwykle pozostaje nieprzezroczysta.",L"Ứng dụng khác: chỉ chất liệu khung; nội dung Chrome / Electron thường vẫn đục."},
{L"App window attribute requests accepted: ",L"Zaakceptowane żądania atrybutów okien: ",L"Yêu cầu thuộc tính cửa sổ được chấp nhận: "},
{L". If unchanged visually, the app may not support external backdrop requests.",L". Brak zmiany wyglądu może oznaczać brak obsługi zewnętrznego materiału tła.",L". Nếu giao diện không đổi, ứng dụng có thể không hỗ trợ yêu cầu chất liệu nền bên ngoài."},
{L"Select an application window first.",L"Najpierw wybierz okno aplikacji.",L"Hãy chọn một cửa sổ ứng dụng trước."},
{L"Original attributes restored for managed windows.",L"Przywrócono oryginalne atrybuty zarządzanych okien.",L"Đã khôi phục thuộc tính gốc của các cửa sổ được quản lý."},
{L"Some windows could not be restored. Try Restore again.",L"Nie udało się przywrócić części okien. Spróbuj ponownie.",L"Không thể khôi phục một số cửa sổ. Hãy thử Khôi phục lần nữa."},
{L"All effects restored.",L"Przywrócono wszystkie efekty.",L"Đã khôi phục tất cả hiệu ứng."},
{L"Standalone materials turned off.",L"Wyłączono niezależne materiały.",L"Đã tắt chất liệu độc lập."},
{L"Connecting materials; each item shows its result. Open the selected surface.",L"Łączenie materiałów. Każdy element pokazuje wynik. Otwórz wybrany panel.",L"Đang kết nối chất liệu; mỗi mục sẽ hiện kết quả. Hãy mở khu vực đã chọn."},
{L"Shell material properties written. Check the selected surface appearance.",L"Zapisano właściwości materiału systemowego. Sprawdź wygląd wybranego panelu.",L"Đã ghi thuộc tính chất liệu hệ thống. Hãy kiểm tra khu vực đã chọn."},
{L"Shell materials applied with the acrylic fallback (fixed blur).",L"Zastosowano materiały systemowe w trybie zastępczym akrylu (stałe rozmycie).",L"Đã áp dụng chất liệu hệ thống bằng acrylic dự phòng (độ mờ cố định)."},
{L"The shell engine reported error 0x%08lX. Details are in the glass-engine log.",L"Silnik powłoki zgłosił błąd 0x%08lX. Szczegóły są w dzienniku glass-engine.",L"Công cụ giao diện hệ thống báo lỗi 0x%08lX. Chi tiết có trong nhật ký glass-engine."},
{L"Some shell materials were not applied. Check each connection status.",L"Nie zastosowano części materiałów. Sprawdź stan połączenia każdego elementu.",L"Một số chất liệu chưa được áp dụng. Hãy kiểm tra trạng thái từng mục."},
{L"Some surfaces are not connected and are marked Not applied; open them and retry.",L"Niektóre panele nie są połączone i mają stan Nie zastosowano. Otwórz je i spróbuj ponownie.",L"Một số khu vực chưa kết nối và được đánh dấu Chưa áp dụng; hãy mở rồi thử lại."},
{L"Restoring the previous version's theme...",L"Przywracanie poprzedniego motywu...",L"Đang khôi phục chủ đề phiên bản trước..."},
{L"Restoring the old theme...",L"Przywracanie starego motywu...",L"Đang khôi phục chủ đề cũ..."},
{L"Old theme not yet restored. Click Restore to retry; recovery files retained.",L"Stary motyw nie został przywrócony. Kliknij Przywróć. Kopie odzyskiwania zachowano.",L"Chưa khôi phục chủ đề cũ. Nhấn Khôi phục để thử lại; tệp khôi phục vẫn được giữ."},
{L"Previous theme restore incomplete, code: ",L"Przywracanie poprzedniego motywu niepełne, kod: ",L"Chưa khôi phục xong chủ đề trước, mã lỗi: "},
{L"Previous theme restored. Taskbar uses the original system layout.",L"Przywrócono poprzedni motyw. Pasek zadań używa oryginalnego układu systemu.",L"Đã khôi phục chủ đề trước. Thanh tác vụ dùng bố cục hệ thống gốc."},
{L"Standalone engine could not load. Check that the app folder is writable.",L"Nie można załadować silnika. Sprawdź możliwość zapisu w folderze aplikacji.",L"Không tải được công cụ độc lập. Hãy kiểm tra quyền ghi vào thư mục ứng dụng."},
{L"Cannot create tray icon; exiting.",L"Nie można utworzyć ikony w zasobniku. Zamykanie.",L"Không tạo được biểu tượng khay hệ thống; đang thoát."}
};
inline void replace(std::wstring&s,const wchar_t*from,const wchar_t*to){size_t at=0;while((at=s.find(from,at))!=std::wstring::npos){s.replace(at,wcslen(from),to);at+=wcslen(to);}}
inline const Text* findTranslation(const wchar_t*english){
    // Index existing literals, without duplicating strings or allocating map nodes.
    static const auto index=[] {std::array<const Text*,sizeof(catalog)/sizeof(catalog[0])> sorted{};for(size_t i=0;i<sorted.size();++i)sorted[i]=&catalog[i];std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return wcscmp(a->en,b->en)<0;});return sorted;}();
    auto found=std::lower_bound(index.begin(),index.end(),english,[](auto entry,const wchar_t*key){return wcscmp(entry->en,key)<0;});return found!=index.end()&&wcscmp((*found)->en,english)==0?*found:nullptr;
}
inline const wchar_t* translate(const wchar_t*zh,const wchar_t*en){
    if(current==English)return en;if(current==Simplified)return zh;
    if(current==Traditional){static std::map<std::wstring,std::wstring> cache;auto i=cache.find(zh);if(i!=cache.end())return i->second.c_str();int n=LCMapStringEx(L"zh-TW",LCMAP_TRADITIONAL_CHINESE,zh,-1,nullptr,0,nullptr,nullptr,0);std::wstring text;if(n>0){text.resize(n);LCMapStringEx(L"zh-TW",LCMAP_TRADITIONAL_CHINESE,zh,-1,&text[0],n,nullptr,nullptr,0);text.pop_back();}else text=zh;
        replace(text,L"任務欄",L"工作列");replace(text,L"資源管理器",L"檔案總管");replace(text,L"窗口",L"視窗");replace(text,L"設置",L"設定");replace(text,L"托盤",L"系統匣");replace(text,L"默認",L"預設");replace(text,L"接口",L"介面");replace(text,L"禁用",L"停用");replace(text,L"程序",L"程式");replace(text,L"軟件",L"軟體");replace(text,L"鼠標",L"滑鼠");replace(text,L"內存",L"記憶體");return cache.emplace(zh,std::move(text)).first->second.c_str();}
    if(auto entry=findTranslation(en))return current==Polish?entry->pl:entry->vi;
    return en;
}
}
