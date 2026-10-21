#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <commctrl.h>
#include <uxtheme.h>
#include <cmath>
#include <powrprof.h>
#include "localization.h"
#include "glass_controller.h"
#include "lgengine_preview.h"
#include "optical_labels.h"
#include "shell_diagnostics.h"
#include "taskbar_recovery.h"
#include "explorer_background.h"
#include "native_osd.h"
#include "native_switches.h"
#include "shell_materials.h"
#ifdef LG_ENABLE_SELF_TEST
#ifdef LG_ENABLE_SELF_TEST
#include "shell_materials_test.h"
#endif
#endif
#include "full_themes.h"
#ifdef LG_ENABLE_SELF_TEST
#ifdef LG_ENABLE_SELF_TEST
#include "full_themes_test.h"
#endif
#endif

namespace {
const wchar_t* ClassName=L"LiquidGlass.Control";
constexpr wchar_t ProbeClassName[]=L"LiquidGlass.Control.NativeProbe";
const wchar_t* InstanceName=L"Local\\LiquidGlass.Control.SingleInstance";
enum {Language=101,Refresh,Apply,Restore,Pause,Scope,Material,Corners,List=113,Status,Note,
    Header=130,Pages,Compatibility,SettingsNote=152,ExitApp,
    NativeInfo=162,VolumeHeading=166,BrightnessHeading,
    VolumeOn=180,VolumeOff,BrightnessOn,BrightnessOff,SettingsPages=190,LanguageLabel,StartupAnimation,MotionText,CompatText,AboutText,ColorThemeLabel,ColorTheme,
    TaskbarToggle=210,StartToggle,NotificationToggle,TaskbarHeading,StartHeading,NotificationHeading,TaskbarHint,StartHint,NotificationHint,
    ExplorerToggle=230,ExplorerHeading,OpacityLabel,OpacitySlider,RadiusLabel,RadiusSlider,TintLabel,TintCombo,BatteryFriendly,EffectNote,EffectValue,DockToggle,DockHeading,BlurLabel,BlurSlider,OpticalPreview,RestoreOriginal,KeyboardToggle,KeyboardHeading,CalendarToggle=260,CalendarHeading};
HWND mainWindow{}, language{}, scope{}, material{}, corners{}, list{}, status{}, note{};
HFONT font{}; HBRUSH background{}; NOTIFYICONDATAW tray{};
bool enabled=false,busy=false,selfTestMode=false,nativeProbeMode=false; UINT dpi=96;
std::map<HWND,RECT> controlLayout;
HWINEVENTHOOK windowEvents{},nameEvents{};unsigned attributeWrites=0;std::set<HWND> pendingWindows,notEligible;bool windowScanPending=false; // notEligible: matching windows App glass could not manage (elevated, no DWM support); cleared on DESTROY and on Apply
bool volumeOn=false,brightnessOn=false,startupAnimation=true,startupPlayed=false;ULONGLONG startupTime=0;
std::map<std::wstring,HICON> appIcons;HWINEVENTHOOK titleHook{};DWORD appBorder=0xFFFFFFFF,appText=0xFFFFFFFF;
glass::Controller engine;glass::Config glassConfig;bool explorerOn=false,explorerByApp=false,dockOn=false,keyboardOn=false,batteryFriendly=true;HANDLE migrationJob{};HPOWERNOTIFY powerNotification{},acNotification{};bool engineReady=false,engineAttempted=false;
DWORD glassExtraFeatures=0;// Calendar (own switch) and Toasts (no switch yet) bits carried from the ini; see glass_features.h
DWORD confirmedMaterials=0,failedMaterials=0,pausedFeatures=0,fallbackMaterials=0;
constexpr GUID PowerSavingStatus={0xe00958c0,0xc213,0x4ace,{0xac,0x77,0xfe,0xcc,0xed,0x2e,0xee,0xa5}};
bool glassPendingDiscover=false;
lgpreview::Controller opticalPreview;
ULONGLONG opticalStartup=0;bool opticalUiReady=false,opticalSuspended=false;
enum {OptArrival=300,OptIndicator,OptSlosh,OptKnob,OptReplay,OptSample,OptTitle,OptNote,OptLabel1,OptLabel2,OptLabel3,OptLabel4,OptShape,OptShapeLabel,OptLook1,OptLook2,OptLook3,OptLook4,OptLook5,OptLook6,OptLook7,OptLook8,OptLookLabel1,OptLookLabel2,OptLookLabel3,OptLookLabel4,OptLookLabel5,OptLookLabel6,OptLookLabel7,OptLookLabel8};
enum {AppPages=400,AppBorder,AppText,TitleOn,TitleHeading,TitleNote,TitleInsetLabel,TitleInset,TitleLook1,TitleLook2,TitleLook3,TitleLook4,TitleLook5,TitleLook6,TitleLook7,TitleLook8,TitleLabel1,TitleLabel2,TitleLabel3,TitleLabel4,TitleLabel5,TitleLabel6,TitleLabel7,TitleLabel8};
// Row expanders (System glass rows 0..7, then App glass > Title-bar glass) and the System glass panels; each slider directly follows its label.
enum {ExpandVolume=440,ExpandBrightness,ExpandTaskbar,ExpandStart,ExpandNotification,ExpandCalendar,ExpandExplorer,ExpandDock,ExpandTitle,
    LookOpacityLabel=450,LookOpacity,LookBlurLabel,LookBlur,LookRadiusLabel,LookRadius,LookTintLabel,LookTint,DockScaleLabel=460,DockScale,DockSpreadLabel,DockSpread,DockSpeedLabel,DockSpeed,DockBounceLabel,DockBounce,PageScroll=470};
void syncOpticalControls(int action=0);void syncTitleGlass();
void applyGlass(bool discover=true);
void queueGlass(bool discover=true);
void updateEffectLabels();
void observeWindows();
bool migrationNeeded();
void migrateLegacy();
std::wstring iniPath;
HWND tabs{},compatibility{},nativeInfo{};
bool shellOn[3]{};
std::vector<HWND> pageControls[3];int buildingPage=-1,currentPage=0;
// Each top tab is one page of stacked blocks (control rects in DIP, relative to the block top). An expanded row shows its panel right under it; a page taller than the card scrolls (placePage, control_ui.h).
struct Block{std::vector<std::pair<HWND,RECT>> items;int height=0;};
Block appBlock,titleRow,titlePanel,systemRows[9],indicatorPanel,lookPanel,dockPanel,systemFooter,settingsBlock;Block* buildingBlock=nullptr;const Block* revealBlock=nullptr;
int systemOpen=-1,pageScroll[3]{},pageMaxScroll[3]{};bool titleOpen=false;HWND pageScrollBar{}; // systemOpen: expanded System glass row (0 Volume .. 7 Dock; Keyboard has no panel), -1 = none
void expandSystemRow(int row);void expandTitleRow(bool open);void placePage();void relayout();void scrollPage(int offset);void loadLookPanel();void updatePanelLabels();void syncIndicatorPanel();
bool indicatorOpen(){return currentPage==1&&(systemOpen==0||systemOpen==1);} // Volume and Brightness share the LGEngine indicator panel
bool exitAfterRestore=false;
void restoreAll(bool exit=false);
void updateCompatibility();
void refresh(bool recollect);
void releaseAppList();
void updateNativeInfo();
void finishPageMotion();void beginPageMotion();
bool shellBackupExists();
struct Entry {HWND hwnd; DWORD pid; std::wstring title,exe,cls;HICON icon{};};
// border/text: the window's own colours (34/36) read before the first apply, put back by restoreWindows.
struct Snapshot {DWORD pid;std::wstring cls;int backdrop{},corner{};bool hasBackdrop=false,hasCorner=false;int appliedBackdrop=-1,appliedCorner=-1;DWORD appliedBorder=0xFFFFFFFF,appliedText=0xFFFFFFFF;DWORD border=0xFFFFFFFF,text=0xFFFFFFFF;bool hasBorder=false,hasText=false;int resets=0;ULONGLONG resetSince=0;bool managesOwn=false;}; // resets/resetSince/managesOwn: back-off for apps that undo the backdrop themselves
// Per-attribute outcome of one sync pass, for the status line ("2 refused the backdrop (0x80070057)").
struct ApplyStats {int ok=0,total=0,backdropRefused=0,cornerRefused=0,unsupported=0;HRESULT backdropError=S_OK,cornerError=S_OK;};
std::vector<Entry> entries;std::map<HWND,Snapshot> originals;
HWND lastListedWindow{};
std::wstring selectedExe;int mode=0,backdrop=3,corner=2;
const wchar_t* tr(const wchar_t* zh,const wchar_t* en){return l10n::translate(zh,en);}
const int lookDefault[8]={5,32,90,68,30,24,100,50},lookMax[8]={100,100,100,100,100,28,150,100};
const wchar_t* const lookKey[8]={L"Dark",L"Blur",L"Refraction",L"Rim",L"Shadow",L"Round",L"Intensity",L"Elevation"};
void updateLookLabels(){
    auto names=opticalui::kLook[int(l10n::current)];
    for(int i=0;i<8;++i){HWND slider=GetDlgItem(mainWindow,OptLook1+i);if(!slider)continue;std::wstring text=std::wstring(names[i])+L": "+std::to_wstring((int)SendMessageW(slider,TBM_GETPOS,0,0));SetDlgItemTextW(mainWindow,OptLookLabel1+i,text.c_str());}
    SetDlgItemTextW(mainWindow,OptShapeLabel,names[8]);
}
// App glass > Title-bar glass: its own look sliders (defaults = lgpreview::AppGlass), saved in [AppGlass]; self-test and probe never write the ini.
const int titleDefault[8]={0,0,70,60,0,8,100,0};
bool persistent(){return !selfTestMode&&!nativeProbeMode;}
void appWrite(const wchar_t* key,int v){if(persistent())WritePrivateProfileStringW(L"AppGlass",key,std::to_wstring(v).c_str(),iniPath.c_str());}
std::wstring titleKey(int i){return std::wstring(L"Title")+lookKey[i];}
void updateTitleLabels(){
    auto names=opticalui::kLook[int(l10n::current)];
    for(int i=0;i<8;++i){HWND slider=GetDlgItem(mainWindow,TitleLook1+i);if(!slider)continue;std::wstring text=std::wstring(names[i])+L": "+std::to_wstring((int)SendMessageW(slider,TBM_GETPOS,0,0));SetDlgItemTextW(mainWindow,TitleLabel1+i,text.c_str());}
    if(HWND inset=GetDlgItem(mainWindow,TitleInset)){std::wstring text=std::wstring(tr(L"边距",L"Inset"))+L": "+std::to_wstring((int)SendMessageW(inset,TBM_GETPOS,0,0));SetDlgItemTextW(mainWindow,TitleInsetLabel,text.c_str());}
}
void saveTitleSlider(int id){
    int pos=(int)SendDlgItemMessageW(mainWindow,id,TBM_GETPOS,0,0);
    if(id==TitleInset)appWrite(L"TitleInset",pos);else if(id>=TitleLook1&&id<=TitleLook8)appWrite(titleKey(id-TitleLook1).c_str(),pos);
}
void syncOpticalControls(int action){
    if(!opticalUiReady||!GetDlgItem(mainWindow,OptReplay))return;
    lgpreview::Controls c;int ids[]={OptArrival,OptIndicator,OptSlosh,OptKnob};int* values[]={&c.arrival,&c.indicator,&c.slosh,&c.knob};
    for(int i=0;i<4;++i){int n=(int)SendDlgItemMessageW(mainWindow,ids[i],CB_GETCURSEL,0,0);if(n>=0)*values[i]=n;}
    int* look[]={&c.dark,&c.blur,&c.refraction,&c.rim,&c.shadow,&c.round,&c.intensity,&c.elevation};
    for(int i=0;i<8;++i){HWND slider=GetDlgItem(mainWindow,OptLook1+i);if(slider)*look[i]=(int)SendMessageW(slider,TBM_GETPOS,0,0);}
    int shape=(int)SendDlgItemMessageW(mainWindow,OptShape,CB_GETCURSEL,0,0);if(shape>=0)c.shape=shape;
    c.enabled=indicatorOpen();c.osd=(volumeOn?1:0)|(brightnessOn?2:0);c.action=action; // System glass > Effects Volume = bit 0, Brightness = bit 1
    opticalPreview.controls(c);syncTitleGlass(); // resent after the adapter connects
}
void saveOpticalCombo(int id){
    if(selfTestMode||nativeProbeMode)return;
    const wchar_t* key=id==OptArrival?L"Arrival":id==OptIndicator?L"Indicator":id==OptSlosh?L"Slosh":id==OptKnob?L"Knob":id==OptShape?L"Shape":nullptr;int n=(int)SendDlgItemMessageW(mainWindow,id,CB_GETCURSEL,0,0);
    if(key&&n>=0)WritePrivateProfileStringW(L"LGEngine",key,std::to_wstring(n).c_str(),iniPath.c_str());
}
void saveLookSlider(int id){
    if(selfTestMode||nativeProbeMode||id<OptLook1||id>OptLook8)return;
    WritePrivateProfileStringW(L"LGEngine",lookKey[id-OptLook1],std::to_wstring((int)SendDlgItemMessageW(mainWindow,id,TBM_GETPOS,0,0)).c_str(),iniPath.c_str());
}
// A status line belongs to the page it came from (-1 = the page now shown); other pages show it blank, not stale.
// diagText: live engine diagnostics (host stages, dock), shown under the message on System glass (not while the indicator panel is open); see updateDiagnostics.
std::wstring statusText,diagText;int statusPage=0;
// The action message comes first; diagnostics ("Diagnostics: host stages · dock") follow on a second line only when the message fits on one line. The box ellipsizes the rest.
void refreshStatus(){
    if(status&&!(GetWindowLongPtrW(status,GWL_STYLE)&SS_WORDELLIPSIS))SetWindowLongPtrW(status,GWL_STYLE,GetWindowLongPtrW(status,GWL_STYLE)|SS_WORDELLIPSIS);
    std::wstring s=statusPage==currentPage?statusText:std::wstring();
    if(currentPage==1&&!indicatorOpen()&&!diagText.empty()&&s.size()<=90&&s.find(L'\n')==std::wstring::npos){
        std::wstring flat=diagText;for(size_t i;(i=flat.find(L"\r\n"))!=std::wstring::npos;)flat.replace(i,2,L" · ");
        s+=(s.empty()?L"":L"\r\n")+std::wstring(tr(L"诊断：",L"Diagnostics:"))+L" "+flat;
    }
    SetWindowTextW(status,s.c_str());
}
void message(const std::wstring& s,int page=-1){statusText=s;statusPage=page<0?currentPage:page;refreshStatus();}
void updateOpticalButtons(){
    if(!opticalUiReady||!mainWindow||!GetDlgItem(mainWindow,OptReplay)||!GetDlgItem(mainWindow,OptSample))return;
    // Status bits from the child: 32 = its host is connected, 1 = that host speaks protocol 1.1 (the sample lens needs it).
    bool live=opticalPreview.synchronized();unsigned bits=live?opticalPreview.status():0;
    EnableWindow(GetDlgItem(mainWindow,OptReplay),live&&(bits&32));EnableWindow(GetDlgItem(mainWindow,OptSample),live&&(bits&33)==33&&(volumeOn||brightnessOn));
    static bool standDownShown=false;bool standDown=(bits&64)!=0;if(standDown&&!standDownShown)message(tr(L"Liquid Glass 已在显示它自己的玻璃音量指示器，因此此指示器保持关闭。要使用此指示器，请在 Liquid Glass 中关闭其玻璃指示器。",L"Liquid Glass is already showing its own glass volume indicator, so this one stays off. Turn off the glass indicator in Liquid Glass to use this one."),1);standDownShown=standDown;
}
unsigned opticalFailures=0;ULONGLONG opticalRetryAt=0;std::wstring opticalFailureText;bool opticalHostSeen=false;DWORD opticalHostError=0;
// 4551 = Smart App Control / WDAC, 1260 = blocked by policy, 577 = invalid image hash: retrying only pops more toasts.
bool opticalPolicyBlocked(){return opticalHostError==4551||opticalHostError==1260||opticalHostError==577;}
void opticalFailed(const std::wstring& text){
    if(mainWindow)KillTimer(mainWindow,16);
    opticalStartup=0;opticalPreview.stop();opticalHostSeen=false;++opticalFailures;
    ULONGLONG wait=std::min<ULONGLONG>(30000ull<<std::min(opticalFailures-1,4u),600000ull);opticalRetryAt=GetTickCount64()+wait;
    // Timer 22 retries once the backoff has passed; the count resets only when a host really connects.
    if(mainWindow){if(opticalFailures<3)SetTimer(mainWindow,22,UINT(wait+50),nullptr);else KillTimer(mainWindow,22);}
    opticalFailureText=text;message(text,1);updateOpticalButtons();
}
// "Connected" only once the adapter's host has said Hello; before that the adapter alone is up.
// A stored "waiting" line is upgraded even off the LGEngine section, so it never comes back stale.
void opticalConnectedMessage(){
    std::wstring waiting=tr(L"LGEngine 预览已启动，正在等待引擎宿主...",L"LGEngine preview started; waiting for the engine host...");
    if(indicatorOpen()||(statusPage==1&&statusText==waiting))message(opticalHostSeen?tr(L"LGEngine 预览已连接；光学参数只影响预览。",L"LGEngine preview connected; optical controls affect the preview only."):waiting,1);
}
bool opticalMayStart(bool user){
    if(user){opticalFailures=0;return true;}
    return opticalFailures==0||(opticalFailures<3&&GetTickCount64()>=opticalRetryAt);
}
bool startOptical(){
    if(opticalPreview.start(mainWindow)){opticalHostError=0;opticalHostSeen=false;opticalStartup=GetTickCount64();SetTimer(mainWindow,16,500,nullptr);return true;}
    DWORD code=opticalPreview.lastError();
    if(code==ERROR_FILE_NOT_FOUND||code==ERROR_PATH_NOT_FOUND){opticalFailed(tr(L"未找到 LGEngine 运行时（程序旁没有 lgengine 文件夹）。LGEngine 页面和指示器需要完整的便携 ZIP。",L"LGEngine runtime not found (no lgengine folder next to the app). The LGEngine page and the indicator need the full portable ZIP."));opticalFailures=3;KillTimer(mainWindow,22);}
    else opticalFailed(std::wstring(tr(L"无法启动 LGEngine 预览，错误：",L"Cannot start LGEngine preview. Error: "))+std::to_wstring(code));
    return false;
}
void syncOpticalPreview(){
    if(opticalPreview.exited())opticalFailed(tr(L"LGEngine 预览未能连接，请检查适配器及 Windows 应用控制状态。",L"LGEngine preview did not connect. Check the adapter and Windows application control."));
    if(!selfTestMode&&!nativeProbeMode&&!opticalSuspended&&opticalUiReady&&GetDlgItem(mainWindow,OptReplay)&&!opticalPreview.running()&&(volumeOn||brightnessOn||IsDlgButtonChecked(mainWindow,TitleOn)==BST_CHECKED||indicatorOpen())){if(!opticalMayStart(false)||!startOptical()){updateOpticalButtons();return;}}
    lgpreview::Settings value;value.opacity=glassConfig.opacity;value.blur=glassConfig.blur;value.radius=glassConfig.radius;value.tint=glassConfig.tint;value.savePower=batteryFriendly;value.language=(DWORD)l10n::current;
    BOOL motion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&motion,0);HIGHCONTRASTW high{};high.cbSize=sizeof(high);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);value.reducedMotion=!motion||(high.dwFlags&HCF_HIGHCONTRASTON);
    if(opticalPreview.publish(value)){syncOpticalControls();opticalStartup=0;if(mainWindow)KillTimer(mainWindow,16);}
    else if(opticalPreview.running()&&mainWindow){if(!opticalStartup)opticalStartup=GetTickCount64();SetTimer(mainWindow,16,500,nullptr);}
    updateOpticalButtons();
}
int selection(HWND h){return (int)SendMessageW(h,CB_GETCURSEL,0,0);}
std::wstring classOf(HWND h){wchar_t c[256]{};GetClassNameW(h,c,256);return c;}
std::wstring executable(DWORD pid){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);wchar_t path[32768]{};DWORD n=32768;if(p){if(!QueryFullProcessImageNameW(p,0,path,&n))path[0]=0;CloseHandle(p);}std::wstring s=path;auto i=s.find_last_of(L"\\/");return i==std::wstring::npos?s:s.substr(i+1);}
bool same(const std::wstring&a,const std::wstring&b){return _wcsicmp(a.c_str(),b.c_str())==0;}
bool systemExecutable(const std::wstring&exe){return same(exe,L"StartMenuExperienceHost.exe")||same(exe,L"SearchHost.exe")||same(exe,L"SearchApp.exe")||same(exe,L"ShellExperienceHost.exe")||same(exe,L"ShellHost.exe");}
bool usesExplorer(int selectedMode,const std::wstring&exe){return selectedMode==1||selectedMode==2||(selectedMode==0&&same(exe,L"explorer.exe"));}
bool explorerBackupExists(){auto dir=iniPath.substr(0,iniPath.find_last_of(L"\\")+1);return GetFileAttributesW((dir+L"explorer-background-originals.bin").c_str())!=INVALID_FILE_ATTRIBUTES;}
bool nativeBackupExists(){auto dir=iniPath.substr(0,iniPath.find_last_of(L"\\")+1);return GetFileAttributesW((dir+L"native-osd-originals.bin").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((dir+L"native-osd-switches.bin").c_str())!=INVALID_FILE_ATTRIBUTES;}
std::wstring settingsDir(){return iniPath.substr(0,iniPath.find_last_of(L"\\")+1);}
// Explorer and the shell hosts keep the injected engine DLL loaded until they
// restart. Extract it per user, outside the app folder, so that folder can
// always be moved or deleted while the shell is running.
std::wstring engineDir(){wchar_t base[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);if(!n||n>=MAX_PATH)return settingsDir();std::wstring dir=std::wstring(base)+L"\\LiquidGlassControl\\";CreateDirectoryW(dir.c_str(),nullptr);dir+=L"Engine\\";if(!CreateDirectoryW(dir.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return settingsDir();return dir;}
// A switch the user turns on after a failed engine load tries the load again (once per click).
bool engineToggleOn(int id){int ids[]={VolumeOn,BrightnessOn,TaskbarToggle,StartToggle,NotificationToggle,CalendarToggle,ExplorerToggle,DockToggle,KeyboardToggle};bool on[]={volumeOn,brightnessOn,shellOn[0],shellOn[1],shellOn[2],(glassExtraFeatures&glass::Calendar)!=0,explorerOn,dockOn,keyboardOn};for(int i=0;i<9;++i)if(id==ids[i])return !on[i];return false;}
bool ensureEngine(){
    if(selfTestMode)return false;
    if(engineReady)return true;if(engineAttempted)return false;engineAttempted=true;
    auto runtimeDir=engineDir();engineReady=engine.initialize(runtimeDir,nativeProbeMode);
    // Clean up copies that 0.13.0-beta.5 and earlier extracted beside the EXE.
    if(_wcsicmp(runtimeDir.c_str(),settingsDir().c_str())!=0)glass::Controller::sweepEngines(settingsDir(),L"");
    if(!engineReady){message(engine.policyBlocked()?tr(L"Windows 应用程序控制策略阻止了引擎，请使用受信任签名或管理员批准的版本。",L"Windows application-control policy blocked the engine. A trusted signed or administrator-approved build is required."):tr(L"独立引擎无法加载，请检查 AppData\\Local\\LiquidGlassControl\\Engine 是否可写。",L"Standalone engine could not load. Check that AppData\\Local\\LiquidGlassControl\\Engine is writable."));engine.shutdown();}
    return engineReady;
}
bool shellBackupExists(){for(int i=0;i<3;++i)if(full_themes::pending(i,settingsDir())||shell_materials::exists(i,settingsDir()))return true;return false;}
void updateNativeInfo(){
    bool pending=migrationJob!=nullptr;
    EnableWindow(GetDlgItem(mainWindow,RestoreOriginal),!pending);
    if(nativeInfo)SetWindowTextW(nativeInfo,pending?tr(L"正在恢复旧版主题，请稍候……",L"Restoring the previous version's theme..."):tr(L"独立材质引擎 · 不需要 Windhawk · 保留系统布局\r\n点击各项右侧的箭头调整其参数；省电选项在“设置”中。",L"Standalone materials · No Windhawk required · System layout preserved\r\nOpen a row's arrow for its own settings; battery saving is in Settings."));
    int ids[]={VolumeOn,BrightnessOn,TaskbarToggle,StartToggle,NotificationToggle,CalendarToggle,ExplorerToggle,DockToggle,KeyboardToggle};bool states[]={volumeOn,brightnessOn,shellOn[0],shellOn[1],shellOn[2],(glassExtraFeatures&glass::Calendar)!=0,explorerOn,dockOn,keyboardOn};
    for(int i=0;i<9;++i){HWND h=GetDlgItem(mainWindow,ids[i]);if(!h)continue;SendMessageW(h,BM_SETCHECK,states[i]?BST_CHECKED:BST_UNCHECKED,0);EnableWindow(h,!pending&&!migrationNeeded());InvalidateRect(h,nullptr,FALSE);}
    int headings[]={VolumeHeading,BrightnessHeading,TaskbarHeading,StartHeading,NotificationHeading,CalendarHeading,ExplorerHeading,DockHeading,KeyboardHeading};const wchar_t*zh[]={L"音量提示",L"亮度提示",L"任务栏",L"开始菜单",L"通知中心",L"日历",L"资源管理器",L"Dock 悬停动画",L"键盘布局切换"};const wchar_t*en[]={L"Volume indicator",L"Brightness indicator",L"Taskbar",L"Start menu",L"Notification center",L"Calendar",L"File Explorer",L"Dock hover animation",L"Keyboard layout switcher"};
    DWORD bits[]={glass::Volume,glass::Brightness,glass::Taskbar|glass::Capsule,glass::Start,glass::Notifications,glass::Calendar,glass::Explorer,glass::Dock,glass::Keyboard};
    int flyOwner=lgpreview::flyoutOwner();
    // Volume and Brightness drive the LGEngine indicator (the adapter), not the XAML engine: their status comes from the adapter's host, not the shell engine's reports.
    for(int i=0;i<9;++i){DWORD bit=bits[i];std::wstring text=tr(zh[i],en[i]);if(states[i]){
        if(i<2)text+=(opticalFailures>=3||(opticalFailures&&!opticalPreview.running()))?tr(L" · 未生效",L" · Not applied"):(flyOwner==1)?tr(L" · 已被 Liquid Glass 隐藏",L" · Hidden by Liquid Glass"):opticalHostSeen?tr(L" · 已应用",L" · Applied"):tr(L" · 等待连接",L" · Connecting");
        else text+=(pausedFeatures&bit)?tr(L" · 已暂停",L" · Paused"):(failedMaterials&bit)?tr(L" · 未生效",L" · Not applied"):(fallbackMaterials&bit)?tr(L" · 亚克力（模糊固定）",L" · Acrylic (fixed blur)"):((confirmedMaterials&bit)==bit)?(i==7?tr(L" · 已应用",L" · Applied"):tr(L" · 已应用（自定义模糊）",L" · Applied (custom blur)")):tr(L" · 等待连接",L" · Connecting");}
        SetWindowTextW(GetDlgItem(mainWindow,headings[i]),text.c_str());}

    for(int id:{TaskbarHint,StartHint,NotificationHint})SetWindowTextW(GetDlgItem(mainWindow,id),L"");
}
bool shellClass(const std::wstring& c){return c==L"Shell_TrayWnd"||c==L"Shell_SecondaryTrayWnd"||c==L"Progman"||c==L"WorkerW"||c==L"XamlExplorerHostIslandWindow"||c==L"MultitaskingViewFrame"||c==L"TaskSwitcherWnd";}
// Title-bar glass target: the foreground window, when it passes the enumerate() filters and matches the App glass rule.
std::vector<Entry> collect();
HWND lastTitleTarget{}; // the last eligible foreground window: kept while this app itself is in front, so the strip stays on the app being styled
HWND eligibleTitle(HWND h){
    if(!h||!IsWindowVisible(h)||GetAncestor(h,GA_ROOT)!=h||GetWindow(h,GW_OWNER)||(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return nullptr;
    DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId())return nullptr;
    auto cls=classOf(h);if(shellClass(cls))return nullptr;
    auto exe=executable(pid);if(exe.empty()||systemExecutable(exe))return nullptr;
    bool explorer=cls==L"CabinetWClass"||cls==L"ExploreWClass";
    return ((mode==0&&same(exe,selectedExe))||(mode==1&&explorer)||mode==2)?h:nullptr;
}
HWND titleTarget(){
    if(!enabled||IsDlgButtonChecked(mainWindow,TitleOn)!=BST_CHECKED)return nullptr; // the scope counts once applied, as the note says; Restore takes the strip away
    HWND h=GetForegroundWindow();if(!h)return nullptr;
    DWORD pid=0;GetWindowThreadProcessId(h,&pid);
    if(pid==GetCurrentProcessId())return eligibleTitle(lastTitleTarget); // this window is in front: the strip stays on the last app (if it is still open and eligible)
    HWND e=eligibleTitle(h);if(e)lastTitleTarget=e;return e;
}
// Title-bar glass note: the explanation, plus one short sentence when it is on but cannot show anything.
std::wstring titleNoteShown;
void updateTitleNote(bool force=false){
    HWND noteBox=mainWindow?GetDlgItem(mainWindow,TitleNote):nullptr;if(!noteBox)return;
    std::wstring text=tr(L"使用上方选择的应用（点击「应用」）。仅显示在活动窗口上，窗口移动时隐藏。请保持较低的模糊，以便标题清晰可读。",L"Uses the apps chosen above (Apply). Shows on the active window only and hides while it moves. Keep Blur low so the title stays readable.");
    if(IsDlgButtonChecked(mainWindow,TitleOn)==BST_CHECKED){
        if(!enabled)text+=std::wstring(L"\r\n")+tr(L"尚未应用任何应用：请在上方选择应用并点击「应用」。",L"Nothing is applied yet: choose an app above and press Apply.");
        else if(!titleTarget()&&!eligibleTitle(lastTitleTarget)){bool any=false;for(auto&e:collect())if(eligibleTitle(e.hwnd)){any=true;break;}if(!any)text+=std::wstring(L"\r\n")+tr(L"没有符合条件的应用窗口。",L"No matching app window is open.");}
    }
    if(force||text!=titleNoteShown){titleNoteShown=text;SetWindowTextW(noteBox,text.c_str());}
}
void syncTitleGlass(){
    if(!mainWindow||!GetDlgItem(mainWindow,TitleOn))return;
    updateTitleNote();if(!opticalPreview.connected())return;
    lgpreview::AppGlass v;v.on=IsDlgButtonChecked(mainWindow,TitleOn)==BST_CHECKED?1:0;v.inset=(int)SendDlgItemMessageW(mainWindow,TitleInset,TBM_GETPOS,0,0);
    int* look[]={&v.dark,&v.blur,&v.refraction,&v.rim,&v.shadow,&v.round,&v.intensity,&v.elevation};
    for(int i=0;i<8;++i)*look[i]=(int)SendDlgItemMessageW(mainWindow,TitleLook1+i,TBM_GETPOS,0,0);
    v.target=(unsigned long long)(UINT_PTR)titleTarget();opticalPreview.appGlass(v);
}
void CALLBACK foregroundChanged(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD){syncTitleGlass();}
void observeForeground(){bool need=!selfTestMode&&!nativeProbeMode&&IsDlgButtonChecked(mainWindow,TitleOn)==BST_CHECKED;if(need&&!titleHook)titleHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundChanged,0,0,WINEVENT_OUTOFCONTEXT);else if(!need&&titleHook){UnhookWinEvent(titleHook);titleHook=nullptr;}}
bool valid(HWND h,const Snapshot& s){DWORD pid=0;GetWindowThreadProcessId(h,&pid);return IsWindow(h)&&pid==s.pid&&classOf(h)==s.cls&&GetPropW(h,L"LiquidGlass.Control.Owner")==mainWindow;}
void saveLanguage(){l10n::save(iniPath);}
// Timer 23 (2 s) re-asserts the attributes while App glass has rules: apps reset them when they recreate or retheme a window.
void setRulesActive(bool active){enabled=active;observeWindows();if(mainWindow){if(active)SetTimer(mainWindow,23,2000,nullptr);else KillTimer(mainWindow,23);}}
void CALLBACK windowChanged(HWINEVENTHOOK,DWORD event,HWND h,LONG object,LONG child,DWORD,DWORD){
    if(event==EVENT_OBJECT_DESTROY&&h)notEligible.erase(h);
    if((!enabled&&!glassConfig.features)||!h||object!=OBJID_WINDOW||child!=CHILDID_SELF)return;
    DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId())return;
    // Only queue relevant shell hosts; app-only updates do not rediscover or reinject every shell thread.
    // Styler initializes on creation, including hidden CoreWindow hosts.
    // Visibility/root checks still apply to ordinary application rules.
    bool glassTarget=glassConfig.features&&engine.wantsWindow(h);
    if(!glassTarget&&(notEligible.count(h)||!enabled||!IsWindowVisible(h)||GetAncestor(h,GA_ROOT)!=h))return;
    if(pendingWindows.size()<128)pendingWindows.insert(h);
    if(!windowScanPending)windowScanPending=SetTimer(mainWindow,3,180,nullptr)!=0;
}
// A window that gets its title (or a new one) after it was shown: enumerate() skips untitled windows, so the name change is the cue to look again.
bool appRuleMatches(HWND h){
    if(!h||!IsWindowVisible(h)||GetAncestor(h,GA_ROOT)!=h||GetWindow(h,GW_OWNER)||(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)||GetWindowTextLengthW(h)<=0)return false;
    DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId())return false;
    auto cls=classOf(h);if(shellClass(cls))return false;
    auto exe=executable(pid);if(exe.empty()||systemExecutable(exe))return false;
    bool explorer=cls==L"CabinetWClass"||cls==L"ExploreWClass";
    return (mode==0&&same(exe,selectedExe))||(mode==1&&explorer)||mode==2;
}
void CALLBACK nameChanged(HWINEVENTHOOK,DWORD,HWND h,LONG object,LONG child,DWORD,DWORD){
    if(!enabled||!h||object!=OBJID_WINDOW||child!=CHILDID_SELF||originals.count(h)||notEligible.count(h)||!appRuleMatches(h))return; // windows already managed need nothing; timer 23 keeps them
    if(pendingWindows.size()<128)pendingWindows.insert(h);
    if(!windowScanPending)windowScanPending=SetTimer(mainWindow,3,180,nullptr)!=0;
}
void observeWindows(){bool need=enabled||glassConfig.features;if(need&&!windowEvents)windowEvents=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_SHOW,nullptr,windowChanged,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);else if(!need&&windowEvents){UnhookWinEvent(windowEvents);windowEvents=nullptr;KillTimer(mainWindow,3);windowScanPending=false;pendingWindows.clear();}
    if(enabled&&!nameEvents)nameEvents=SetWinEventHook(EVENT_OBJECT_NAMECHANGE,EVENT_OBJECT_NAMECHANGE,nullptr,nameChanged,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);else if(!enabled&&nameEvents){UnhookWinEvent(nameEvents);nameEvents=nullptr;}}
void selectPage(int page){
    if(page<0||page>2)return;bool changed=currentPage!=page;finishPageMotion();currentPage=page;refreshStatus();
    TabCtrl_SetCurSel(tabs,page);relayout();
    if(changed){beginPageMotion();if(page==0)refresh(true);else if(list)releaseAppList();}
}
// The LGEngine preview runs only while the indicator panel (Volume or Brightness row) is open on System glass.
void syncIndicatorPanel(){if(indicatorOpen()&&!nativeProbeMode&&!selfTestMode){syncOpticalPreview();if(!opticalPreview.running()&&opticalFailures&&!opticalFailureText.empty())message(opticalFailureText);}else syncOpticalControls();refreshStatus();}
void restoreWindows(){
    int failures=0;
    for(auto i=originals.begin();i!=originals.end();){auto h=i->first;auto&s=i->second;
        if(!valid(h,s)){i=originals.erase(i);continue;}
        // Each attribute that was put back clears its applied flag, so a later apply() writes it again; only the failed ones stay marked.
        bool ok=true;if(s.hasBackdrop){if(SUCCEEDED(DwmSetWindowAttribute(h,38,&s.backdrop,sizeof(int))))s.appliedBackdrop=-1;else ok=false;}
        if(s.hasCorner){if(SUCCEEDED(DwmSetWindowAttribute(h,33,&s.corner,sizeof(int))))s.appliedCorner=-1;else ok=false;}
        // Border (34) and title text (36) go back to what the window had before the first apply (default when it could not be read).
        DWORD unset=0xFFFFFFFF;if(s.appliedBorder!=unset){DWORD was=s.hasBorder?s.border:unset;DwmSetWindowAttribute(h,34,&was,sizeof(DWORD));s.appliedBorder=unset;}if(s.appliedText!=unset){DWORD was=s.hasText?s.text:unset;DwmSetWindowAttribute(h,36,&was,sizeof(DWORD));s.appliedText=unset;}
        if(ok){RemovePropW(h,L"LiquidGlass.Control.Owner");i=originals.erase(i);}else{++failures;++i;}
    }
    message(failures?tr(L"部分窗口恢复失败，可再次尝试恢复。",L"Some windows could not be restored. Try Restore again."):tr(L"已恢复受管理窗口的原始属性。",L"Original attributes restored for managed windows."));
}
bool applyWindow(HWND h,ApplyStats* stats=nullptr){
    // Enforce the app-only boundary even when a caller bypasses enumeration.
    DWORD targetPid=0;GetWindowThreadProcessId(h,&targetPid);
    auto targetClass=classOf(h);
    if(shellClass(targetClass))return false;
    auto targetExe=executable(targetPid);
    if(systemExecutable(targetExe))return false;
    auto it=originals.find(h);
    if(it!=originals.end()&&!valid(h,it->second)){originals.erase(it);it=originals.end();}
    if(it==originals.end()){
        Snapshot s;GetWindowThreadProcessId(h,&s.pid);s.cls=classOf(h);
        s.hasBackdrop=SUCCEEDED(DwmGetWindowAttribute(h,38,&s.backdrop,sizeof(int)));
        s.hasCorner=SUCCEEDED(DwmGetWindowAttribute(h,33,&s.corner,sizeof(int)));
        s.hasBorder=SUCCEEDED(DwmGetWindowAttribute(h,34,&s.border,sizeof(DWORD)));s.hasText=SUCCEEDED(DwmGetWindowAttribute(h,36,&s.text,sizeof(DWORD)));
        if(!s.hasBackdrop&&!s.hasCorner){if(stats)++stats->unsupported;return false;}
        if(!SetPropW(h,L"LiquidGlass.Control.Owner",mainWindow))return false;
        it=originals.emplace(h,s).first;
    }
    bool requested=false,ok=true;
    if(it->second.hasBackdrop){requested=true;if(it->second.appliedBackdrop!=backdrop){++attributeWrites;HRESULT hr=DwmSetWindowAttribute(h,38,&backdrop,sizeof(int));bool changed=SUCCEEDED(hr);if(changed)it->second.appliedBackdrop=backdrop;else if(stats){++stats->backdropRefused;if(SUCCEEDED(stats->backdropError))stats->backdropError=hr;}ok=changed&&ok;}}
    else if(stats)++stats->unsupported;
    if(it->second.hasCorner){requested=true;if(it->second.appliedCorner!=corner){++attributeWrites;HRESULT hr=DwmSetWindowAttribute(h,33,&corner,sizeof(int));bool changed=SUCCEEDED(hr);if(changed)it->second.appliedCorner=corner;else if(stats){++stats->cornerRefused;if(SUCCEEDED(stats->cornerError))stats->cornerError=hr;}ok=changed&&ok;}}
    // Border (34) and title text (36) colours: their failure does not change the result.
    if(appBorder!=it->second.appliedBorder){++attributeWrites;if(SUCCEEDED(DwmSetWindowAttribute(h,34,&appBorder,sizeof(DWORD))))it->second.appliedBorder=appBorder;}
    if(appText!=it->second.appliedText){++attributeWrites;if(SUCCEEDED(DwmSetWindowAttribute(h,36,&appText,sizeof(DWORD))))it->second.appliedText=appText;}
    return requested&&ok&&it->second.hasBackdrop;
}
// "1 app manages its own backdrop": windows the 2 s re-assert gave up on; empty when none.
std::wstring ownManagedText(){int n=0;for(auto&e:originals)if(e.second.managesOwn)++n;if(!n)return std::wstring();return n==1?std::wstring(tr(L"1 个应用自行管理背景材质",L"1 app manages its own backdrop")):std::to_wstring(n)+tr(L" 个应用自行管理背景材质",L" apps manage their own backdrop");}
BOOL CALLBACK enumerate(HWND h,LPARAM data){
    auto& out=*reinterpret_cast<std::vector<Entry>*>(data);DWORD pid=0;GetWindowThreadProcessId(h,&pid);
    if(pid==GetCurrentProcessId()||!IsWindowVisible(h))return TRUE;
    auto cls=classOf(h);if(shellClass(cls))return TRUE;
    if(GetWindow(h,GW_OWNER)||GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)return TRUE;
    wchar_t title[512]{};GetWindowTextW(h,title,512);if(!title[0])return TRUE;
    std::wstring exe=executable(pid);if(exe.empty())return TRUE;
    // System surfaces are never application targets.
    if(systemExecutable(exe))return TRUE;
    out.push_back({h,pid,title,exe,cls});return TRUE;
}
std::vector<Entry> collect(){std::vector<Entry> v;EnumWindows(enumerate,reinterpret_cast<LPARAM>(&v));return v;}
HICON applicationIcon(const Entry&e){
    wchar_t file[32768]{};DWORD length=32768;HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.pid);if(process){QueryFullProcessImageNameW(process,0,file,&length);CloseHandle(process);}std::wstring key=file[0]?file:e.exe;
    auto found=appIcons.find(key);if(found!=appIcons.end())return found->second;
    DWORD_PTR supplied=0;HICON icon=nullptr;SendMessageTimeoutW(e.hwnd,WM_GETICON,ICON_SMALL2,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,20,&supplied);if(supplied)icon=CopyIcon((HICON)supplied);
    if(!icon){HICON cls=(HICON)GetClassLongPtrW(e.hwnd,GCLP_HICONSM);if(cls)icon=CopyIcon(cls);}
    if(!icon&&file[0]){HICON extractedIcon=nullptr;ExtractIconExW(file,0,nullptr,&extractedIcon,1);icon=extractedIcon;}
    if(!icon)icon=CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));appIcons[key]=icon;return icon;
}
void refresh(bool recollect=true){
    // A tray-only process needs no application list or copied icon handles.
    if(!IsWindowVisible(mainWindow)||currentPage!=0)return;
    HWND previous=lastListedWindow;int n=(int)SendMessageW(list,LB_GETCURSEL,0,0);if(n>=0&&n<(int)entries.size())previous=entries[n].hwnd;
    SendMessageW(list,WM_SETREDRAW,FALSE,0);if(recollect)entries=collect();SendMessageW(list,LB_RESETCONTENT,0,0);int sel=0;
    for(size_t i=0;i<entries.size();++i){if(!entries[i].icon)entries[i].icon=applicationIcon(entries[i]);bool explorer=entries[i].cls==L"CabinetWClass"||entries[i].cls==L"ExploreWClass";std::wstring text=entries[i].exe+(explorer?tr(L" [整窗后端]  —  ",L" [whole-window backend]  —  "):tr(L" [仅外框/待验证]  —  ",L" [frame only/unverified]  —  "))+entries[i].title;SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));if(entries[i].hwnd==previous)sel=(int)i;}
    if(!entries.empty())SendMessageW(list,LB_SETCURSEL,sel,0);
    SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,FALSE);
    for(auto i=appIcons.begin();i!=appIcons.end();){bool used=false;for(auto&e:entries)if(e.icon==i->second){used=true;break;}if(!used){DestroyIcon(i->second);i=appIcons.erase(i);}else ++i;}
    updateCompatibility();
}
void releaseAppList(){
    int selected=(int)SendMessageW(list,LB_GETCURSEL,0,0);if(selected>=0&&selected<(int)entries.size())lastListedWindow=entries[selected].hwnd;
    SendMessageW(list,LB_RESETCONTENT,0,0);std::vector<Entry>().swap(entries);
    for(auto const&icon:appIcons)if(icon.second)DestroyIcon(icon.second);appIcons.clear();
}
void updateCompatibility(){
    if(!compatibility)return;
    int n=(int)SendMessageW(list,LB_GETCURSEL,0,0);bool explorer=selection(scope)==1||(n>=0&&n<(int)entries.size()&&(entries[n].cls==L"CabinetWClass"||entries[n].cls==L"ExploreWClass"));
    SetWindowTextW(compatibility,explorer?tr(L"资源管理器：独立 XAML 材质；经典内容区可能仍不透明。",L"Explorer: standalone XAML materials; classic content may remain opaque."):tr(L"其他应用：仅尝试外框材质；Chrome / Electron 等内容区通常不受影响。",L"Other apps: frame material only; Chrome / Electron content usually stays opaque."));
}
void sync(bool report){
    if(!enabled||busy)return;busy=true;
    for(auto it=originals.begin();it!=originals.end();)if(!valid(it->first,it->second))it=originals.erase(it);else ++it;
    ApplyStats st;int&ok=st.ok;int&total=st.total;
    if(report){notEligible.clear();for(auto&o:originals){o.second.managesOwn=false;o.second.resets=0;o.second.resetSince=0;}} // an explicit Apply retries everything
    for(const auto&e:collect()){
        bool explorer=e.cls==L"CabinetWClass"||e.cls==L"ExploreWClass";
        if((mode==0&&same(e.exe,selectedExe))||(mode==1&&explorer)||(mode==2)){
            ++total;if(applyWindow(e.hwnd,&st))++ok;
        }
    }
    if(report&&!total){message(tr(L"当前没有匹配的窗口。规则已保存，匹配窗口打开后会自动应用。",L"No matching window is open. The rule is saved and applies when one opens."));busy=false;return;}
    if(report){
        // "Applied to 5 of 7 · 2 refused the backdrop (0x80070057)": the per-attribute results behind the count.
        auto hex=[](HRESULT hr){wchar_t t[16]{};swprintf(t,16,L"0x%08lX",(unsigned long)hr);return std::wstring(t);};
        std::wstring s=tr(L"已应用到 ",L"Applied to ");s+=std::to_wstring(ok)+tr(L" / ",L" of ")+std::to_wstring(total);
        if(st.backdropRefused)s+=L" · "+std::to_wstring(st.backdropRefused)+tr(L" 个拒绝了背景材质 (",L" refused the backdrop (")+hex(st.backdropError)+L")";
        if(st.cornerRefused)s+=L" · "+std::to_wstring(st.cornerRefused)+tr(L" 个拒绝了圆角设置 (",L" refused the corner style (")+hex(st.cornerError)+L")";
        if(st.unsupported)s+=std::wstring(L" · ")+tr(L"不支持背景材质：",L"no backdrop support: ")+std::to_wstring(st.unsupported);
        if(auto own=ownManagedText();!own.empty())s+=L" · "+own;
        s+=tr(L"。若无可见变化，该应用可能不支持外部背景材质。",L". If unchanged visually, the app may not support external backdrop requests.");message(s);}
    busy=false;
}
// New or renamed windows: handle just those HWNDs (no EnumWindows). A matching window that cannot be managed is remembered so later title changes cost nothing.
void syncPending(const std::set<HWND>&windows){
    if(!enabled||busy)return;busy=true;
    for(HWND h:windows){
        if(!IsWindow(h)||notEligible.count(h)||!appRuleMatches(h))continue;
        applyWindow(h);if(!originals.count(h)&&notEligible.size()<512)notEligible.insert(h);
    }
    busy=false;
}
// Timer 23: apps reset the DWM attributes (38 backdrop, 33 corner, 34 border, 36 text) when they recreate or retheme a window. Read each back and re-apply what no longer matches.
void reassertApps(){
    if(!enabled||busy||originals.empty())return;
    busy=true;
    bool flagged=false;
    for(auto&entry:originals){
        HWND h=entry.first;Snapshot&s=entry.second;if(s.managesOwn||!valid(h,s)||!appRuleMatches(h))continue;
        // Only the backdrop (38) is compared; border/text read-back quirks must never cause rewrites.
        int now=0;if(!(s.hasBackdrop&&s.appliedBackdrop>=0&&SUCCEEDED(DwmGetWindowAttribute(h,38,&now,sizeof(int)))&&now!=s.appliedBackdrop))continue;
        // An app that resets its backdrop 3 times within 30 s manages it itself: stop fighting it (no flicker) and report it.
        ULONGLONG t=GetTickCount64();if(!s.resetSince||t-s.resetSince>30000){s.resetSince=t;s.resets=0;}
        if(++s.resets>=3){s.managesOwn=true;flagged=true;continue;}
        s.appliedBackdrop=-1;s.appliedCorner=-1;s.appliedBorder=0xFFFFFFFF;s.appliedText=0xFFFFFFFF;applyWindow(h);
    }
    busy=false;
    if(flagged)message(ownManagedText());
}
// System glass > Effects Volume / Brightness drive the LGEngine indicator: [LGEngine] VolumeOn / BrightnessOn, sent to the adapter as Controls.osd bits 0 / 1.
// turnedOn: the switch just clicked went ON. Only that resets the start back-off; turning one off while the other stays on must not relaunch a failed or blocked adapter.
void osdSwitched(bool turnedOn){
    if(persistent()){WritePrivateProfileStringW(L"LGEngine",L"VolumeOn",volumeOn?L"1":L"0",iniPath.c_str());WritePrivateProfileStringW(L"LGEngine",L"BrightnessOn",brightnessOn?L"1":L"0",iniPath.c_str());}
    if(turnedOn){opticalSuspended=false;opticalMayStart(true);}
    else if(!volumeOn&&!brightnessOn&&!nativeProbeMode&&!selfTestMode&&IsDlgButtonChecked(mainWindow,TitleOn)!=BST_CHECKED){ // both off: stop the adapter as the old single switch did (title-bar glass keeps it)
        opticalSuspended=true;opticalStartup=0;KillTimer(mainWindow,16);KillTimer(mainWindow,22);opticalPreview.stop();opticalHostSeen=false;lgpreview::restoreFlyouts();
    }
    syncOpticalPreview();syncOpticalControls();updateOpticalButtons();updateNativeInfo();
    if(nativeProbeMode)queueGlass(); // the probe still drives the XAML indicator through the glass feature mask
}
// Timer 20 (2 s, while the shell engine runs): System glass shows the engine's host stages and, with the Dock on, its icon counts under the status message.
void updateDiagnostics(){
    if(!mainWindow||currentPage!=1||!IsWindowVisible(mainWindow))return;
    std::wstring text;
    if(engineReady&&glassConfig.features){
        text=engine.hostSummary();
        if(dockOn){wchar_t dock[128]{};swprintf(dock,128,tr(L"Dock：%ld 个图标，%ld 个已动画，指针 %ld",L"Dock: %ld icons, %ld animated, pointer %ld"),(long)engine.dockIcons(),(long)engine.dockBound(),(long)engine.dockPointer());if(!text.empty())text+=L"\r\n";text+=dock;}
    }
    if(text!=diagText){diagText=text;refreshStatus();}
}
// Combo indices -> DWM values. Border: default, none, accent, light, dark. Title text: default, light, dark.
DWORD borderValue(int index){
    if(index==1)return 0xFFFFFFFE;
    if(index==2){DWORD argb=0;BOOL opaque=FALSE;if(SUCCEEDED(DwmGetColorizationColor(&argb,&opaque)))return RGB((argb>>16)&255,(argb>>8)&255,argb&255);return 0xFFFFFFFF;}
    return index==3?RGB(232,232,232):index==4?RGB(40,40,40):0xFFFFFFFF;
}
DWORD titleTextValue(int index){return index==1?RGB(255,255,255):index==2?RGB(0,0,0):0xFFFFFFFF;}
void setRule(int materialIndex,int cornerIndex,int borderIndex,int textIndex){
    backdrop=materialIndex==0?3:materialIndex==1?2:materialIndex==2?4:1;corner=cornerIndex==0?2:cornerIndex==1?3:1;appBorder=borderValue(borderIndex);appText=titleTextValue(textIndex);
}
void saveRule(){
    appWrite(L"Active",1);appWrite(L"Mode",mode);if(persistent())WritePrivateProfileStringW(L"AppGlass",L"Exe",selectedExe.c_str(),iniPath.c_str());
    appWrite(L"Material",selection(material));appWrite(L"Corners",selection(corners));appWrite(L"Border",selection(GetDlgItem(mainWindow,AppBorder)));appWrite(L"Text",selection(GetDlgItem(mainWindow,AppText)));appWrite(L"ExplorerByApp",explorerByApp?1:0);
}
// Startup: reselect the saved rule and, when it was active, reapply it after the first scan delay (timer 3).
void loadAppGlass(){
    if(selfTestMode||nativeProbeMode)return;
    auto get=[](const wchar_t* key,int fallback,int high){return std::clamp((int)GetPrivateProfileIntW(L"AppGlass",key,fallback,iniPath.c_str()),0,high);};
    int m=get(L"Mode",2,2),mat=get(L"Material",0,3),cor=get(L"Corners",0,2),bor=get(L"Border",0,4),txt=get(L"Text",0,2);
    wchar_t exe[MAX_PATH]{};GetPrivateProfileStringW(L"AppGlass",L"Exe",L"",exe,MAX_PATH,iniPath.c_str());
    SendMessageW(scope,CB_SETCURSEL,m,0);SendMessageW(material,CB_SETCURSEL,mat,0);SendMessageW(corners,CB_SETCURSEL,cor,0);SendDlgItemMessageW(mainWindow,AppBorder,CB_SETCURSEL,bor,0);SendDlgItemMessageW(mainWindow,AppText,CB_SETCURSEL,txt,0);
    mode=m;selectedExe=exe;setRule(mat,cor,bor,txt);explorerByApp=get(L"ExplorerByApp",0,1)&&explorerOn;updateCompatibility();
    if(get(L"Active",0,1)==1&&!(mode==0&&selectedExe.empty())){setRulesActive(true);windowScanPending=SetTimer(mainWindow,3,1200,nullptr)!=0;}
    observeForeground();syncTitleGlass();
}
void apply(){
    // App glass only writes DWM window attributes, so a pending old-theme restore does not block it; only the System glass part (File Explorer) waits.
    int nextMode=selection(scope);std::wstring exe;
    if(nextMode==0){int n=(int)SendMessageW(list,LB_GETCURSEL,0,0);if(n<0||n>=(int)entries.size()){message(tr(L"请先选择一个应用窗口。",L"Select an application window first."));return;}exe=entries[n].exe;}
    // A restore that failed for some windows no longer stops the new rule: those windows keep their snapshot and are re-applied below, and the status line says so.
    setRulesActive(false);restoreWindows();size_t stuck=originals.size();
    mode=nextMode;selectedExe=exe;setRule(selection(material),selection(corners),selection(GetDlgItem(mainWindow,AppBorder)),selection(GetDlgItem(mainWindow,AppText)));
    // explorerByApp: App glass turned File Explorer on, so App glass may turn it off again (never a System glass choice).
    if(usesExplorer(mode,selectedExe)){if(!explorerOn)explorerByApp=true;explorerOn=true;applyGlass();}
    else if(explorerByApp){explorerByApp=false;explorerOn=false;applyGlass();}
    setRulesActive(true);sync(true);saveRule();syncTitleGlass();
    std::wstring more;if(stuck)more+=std::wstring(L" · ")+tr(L"此前未能还原的窗口：",L"windows that could not be restored earlier: ")+std::to_wstring(stuck);
    if(migrationJob||migrationNeeded())more+=std::wstring(L" · ")+tr(L"旧主题还原完成后系统玻璃才会继续。",L"System glass resumes after the old theme is restored.");
    if(!more.empty())message(statusText+more);
}
// The App glass Restore button: reverts only what App glass applied. System glass and the indicator are left alone
// (the tray menu's Restore still calls restoreAll).
void restoreApps(){
    setRulesActive(false);restoreWindows();if(!originals.empty())return;
    if(explorerByApp){explorerByApp=false;explorerOn=false;applyGlass();}
    appWrite(L"Active",0);appWrite(L"ExplorerByApp",0);syncTitleGlass();
}
void restoreAll(bool exit){
    if(migrationJob){message(tr(L"正在恢复旧主题……",L"Restoring the old theme..."));return;}
    setRulesActive(false);restoreWindows();if(!originals.empty())return;appWrite(L"Active",0); // the windows are back: no startup reapply, even if the taskbar restore below fails
    if(!nativeProbeMode&&!taskbar_recovery::restore(iniPath)){message(tr(L"任务栏恢复失败，恢复记录已保留。",L"Taskbar restore failed; recovery record retained."));return;}appWrite(L"ExplorerByApp",0);appWrite(L"TitleOn",0);CheckDlgButton(mainWindow,TitleOn,BST_UNCHECKED);observeForeground();syncTitleGlass();opticalSuspended=true;opticalPreview.stop();KillTimer(mainWindow,16);opticalStartup=0;updateOpticalButtons();if(persistent()){WritePrivateProfileStringW(L"LGEngine",L"VolumeOn",L"0",iniPath.c_str());WritePrivateProfileStringW(L"LGEngine",L"BrightnessOn",L"0",iniPath.c_str());WritePrivateProfileStringW(L"LGEngine",L"IndicatorOn",L"0",iniPath.c_str());}dockOn=false;WritePrivateProfileStringW(L"Taskbar",L"AutoHide",L"0",iniPath.c_str());volumeOn=brightnessOn=explorerOn=explorerByApp=keyboardOn=false;for(bool&on:shellOn)on=false;glassConfig.features=0;glassExtraFeatures=0;WritePrivateProfileStringW(L"Glass",L"Features",L"0",iniPath.c_str());WritePrivateProfileStringW(L"Glass",L"Calendar",L"0",iniPath.c_str());applyGlass(false);
    if(migrationNeeded()){exitAfterRestore=exit;migrateLegacy();return;}
    message(tr(L"已还原所有效果。",L"All effects restored."));if(exit)DestroyWindow(mainWindow);
}
void combo(HWND h,std::initializer_list<const wchar_t*> labels,int sel){SendMessageW(h,CB_RESETCONTENT,0,0);for(auto s:labels)SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(h,CB_SETCURSEL,sel<0?0:sel,0);}
#include "ui_style.h"
#include "control_ui.h"
#ifdef LG_ENABLE_SELF_TEST
#include "ui_snapshot_test.h"
#endif
#include "glass_features.h"
// System glass panels: one Opacity/Frost/Radius/Tint set serves whichever surface row is open (Taskbar..Explorer = families 0..4), the Dock row has its own four sliders. Changes save to the family/Dock ini sections and apply live.
glass::Family lookFamily(){return (glass::Family)(systemOpen>=2&&systemOpen<=6?systemOpen-2:0);}
void updatePanelLabels(){
    const wchar_t*names[]={tr(L"不透明度",L"Opacity"),tr(L"磨砂",L"Frost"),tr(L"圆角",L"Radius"),tr(L"放大",L"Magnification"),tr(L"范围",L"Spread"),tr(L"速度",L"Speed"),tr(L"弹性",L"Bounce")};int ids[]={LookOpacity,LookBlur,LookRadius,DockScale,DockSpread,DockSpeed,DockBounce};bool percent[]={true,false,false,true,true,false,false};
    for(int i=0;i<7;++i){std::wstring text=std::wstring(names[i])+L"  "+std::to_wstring((int)SendDlgItemMessageW(mainWindow,ids[i],TBM_GETPOS,0,0))+(percent[i]?L"%":L"");SetDlgItemTextW(mainWindow,ids[i]-1,text.c_str());}
    SetDlgItemTextW(mainWindow,LookTintLabel,tr(L"色调",L"Tint"));
}
void loadLookPanel(){
    if(!GetDlgItem(mainWindow,LookOpacity))return;auto f=lookFamily();glass::Look look=glassConfig.looks[f];if(glassLookLinked[f]){look.opacity=glassConfig.opacity;look.blur=glassConfig.blur;look.radius=glassConfig.radius;look.tint=glassConfig.tint;} // a linked family still follows the shared values
    SendDlgItemMessageW(mainWindow,LookOpacity,TBM_SETPOS,TRUE,look.opacity);SendDlgItemMessageW(mainWindow,LookBlur,TBM_SETPOS,TRUE,look.blur);SendDlgItemMessageW(mainWindow,LookRadius,TBM_SETPOS,TRUE,look.radius);SendDlgItemMessageW(mainWindow,LookTint,CB_SETCURSEL,look.tint,0);
    SendDlgItemMessageW(mainWindow,DockScale,TBM_SETPOS,TRUE,glassConfig.dockScale);SendDlgItemMessageW(mainWindow,DockSpread,TBM_SETPOS,TRUE,glassConfig.dockSpread);SendDlgItemMessageW(mainWindow,DockSpeed,TBM_SETPOS,TRUE,glassConfig.dockSpeed);SendDlgItemMessageW(mainWindow,DockBounce,TBM_SETPOS,TRUE,glassConfig.dockBounce);
    updatePanelLabels();
}
ULONGLONG panelApplyTime=0;
void savePanel(bool dock){
    auto pos=[](int id){return (DWORD)SendDlgItemMessageW(mainWindow,id,TBM_GETPOS,0,0);};
    if(dock)saveDock(pos(DockScale),pos(DockSpread),pos(DockSpeed),pos(DockBounce));
    else{glass::Look look;look.opacity=pos(LookOpacity);look.blur=pos(LookBlur);look.radius=pos(LookRadius);int tint=selection(GetDlgItem(mainWindow,LookTint));look.tint=tint<0?0:(DWORD)tint;saveGlassLook(lookFamily(),look);}
    updatePanelLabels();KillTimer(mainWindow,31);panelApplyTime=GetTickCount64();applyGlass(false);
}
// While a look or dock slider is dragged: memory only (the ini is written on release by savePanel), applied at most every 33 ms; timer 31 flushes the latest value.
void previewPanel(bool dock){
    auto pos=[](int id){return (DWORD)SendDlgItemMessageW(mainWindow,id,TBM_GETPOS,0,0);};
    if(dock){glass::Config c=glassConfig;c.dockScale=pos(DockScale);c.dockSpread=pos(DockSpread);c.dockSpeed=pos(DockSpeed);c.dockBounce=pos(DockBounce);c=glass::normalize(c);glassConfig.dockScale=c.dockScale;glassConfig.dockSpread=c.dockSpread;glassConfig.dockSpeed=c.dockSpeed;glassConfig.dockBounce=c.dockBounce;}
    else{auto f=lookFamily();glass::Look look;look.opacity=pos(LookOpacity);look.blur=pos(LookBlur);look.radius=pos(LookRadius);int tint=selection(GetDlgItem(mainWindow,LookTint));look.tint=tint<0?0:(DWORD)tint;glassConfig.looks[f]=glass::normalize(look);glassLookLinked[f]=false;} // unlinked in memory so applyGlass does not copy the shared values over it
    ULONGLONG now=GetTickCount64();if(now-panelApplyTime>=33){KillTimer(mainWindow,31);panelApplyTime=now;glassPreviewOnly=true;applyGlass(false);glassPreviewOnly=false;}else SetTimer(mainWindow,31,UINT(33-(now-panelApplyTime)),nullptr);
}
#ifdef LG_ENABLE_SELF_TEST
int selfTest(){
    bool allPassed=true;std::string checks;
    bool passed=FindResourceW(nullptr,MAKEINTRESOURCEW(202),RT_RCDATA)!=nullptr;passed=passed&&shellClass(L"MultitaskingViewFrame")&&shellClass(L"TaskSwitcherWnd")&&!shellClass(L"CabinetWClass");HRESULT graphicsResult=E_PENDING;
    auto record=[&](const char*name){checks+=(passed?"PASS: ":"FAIL: ");checks+=name;checks+="\r\n";allPassed=passed&&allPassed;passed=true;};
    record("embedded notices and app/shell boundary");
    passed=uiMotion::test();record("spring motion is frame-rate independent, continuous on reversal and settles to idle");
    passed=!engineReady&&!engineAttempted&&!engine.available();record("idle startup defers engine loading");
    {auto previous=lastListedWindow;auto icon=CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));appIcons[L"test-only"]=icon;entries.push_back({mainWindow,GetCurrentProcessId(),L"test",L"test",L"test",icon});SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"test"));SendMessageW(list,LB_SETCURSEL,0,0);releaseAppList();passed=entries.empty()&&entries.capacity()==0&&appIcons.empty()&&SendMessageW(list,LB_GETCOUNT,0,0)==0&&lastListedWindow==mainWindow;lastListedWindow=previous;record("tray cleanup releases application entries and icon handles");}
    {glass::StatusCursor cursor;auto a=cursor.take(glass::Taskbar,0,0,0);auto b=cursor.take(glass::Taskbar,0,0,0);auto c=cursor.take(glass::Taskbar,glass::Taskbar,0,0);auto d=cursor.take(0,0,glass::Taskbar,E_FAIL);auto e=cursor.take(0,0,glass::Taskbar,E_FAIL);auto f=cursor.take(0,0,glass::Taskbar,E_ACCESSDENIED);auto g=cursor.take(glass::Taskbar,0,0,0);auto h=cursor.take(glass::Taskbar,0,0,0,true);passed=a.success==glass::Taskbar&&!b.success&&!b.failed&&c.success==glass::Taskbar&&d.failed==glass::Taskbar&&!e.failed&&f.failed==glass::Taskbar&&g.success==glass::Taskbar&&h.success==glass::Taskbar;record("material reporting suppresses duplicates and retains state/error transitions");}
    // Exercise the embedded engine and its COM factory without attaching to
    // Explorer or changing any live shell material. CI needs no Windhawk.
    auto testDir=settingsDir()+L"engine-test-"+std::to_wstring(GetCurrentProcessId())+L"\\";
    bool made=CreateDirectoryW(testDir.c_str(),nullptr)!=FALSE;
    if(made){glass::Controller probe;bool loaded=probe.initialize(testDir)&&probe.available();passed=loaded;record("embedded engine load");
        if(loaded){passed=probe.factoryTest();record("engine COM factory");graphicsResult=probe.graphicsTest();passed=SUCCEEDED(graphicsResult);record("composition graph");passed=SUCCEEDED(probe.connectTest());record("engine configuration connection");}
        else{char reason[128]{};snprintf(reason,sizeof(reason),"Engine load Win32 error: %lu\r\nSKIP: COM factory, composition and connection (engine not loaded)\r\n",(unsigned long)probe.initializationError());checks+=reason;}
        auto runtime=probe.runtimePath();probe.shutdown();
        bool inspection=probe.initialize(testDir,true);auto mappingName=glass::mappingName(GetCurrentProcessId());HANDLE regular=OpenFileMappingW(FILE_MAP_READ,FALSE,mappingName.c_str()),isolated=OpenFileMappingW(FILE_MAP_READ,FALSE,(mappingName+L".Probe").c_str());
        passed=inspection&&!regular&&isolated&&SUCCEEDED(probe.connectTest());if(regular)CloseHandle(regular);if(isolated)CloseHandle(isolated);probe.shutdown();record("native inspection uses isolated config and engine connection");
        DeleteFileW(runtime.c_str());RemoveDirectoryW(testDir.c_str());
    }else{passed=false;record("create isolated engine test directory");}
    glass::Config invalid;invalid.features=0xffffffff;invalid.opacity=500;invalid.radius=999;invalid.tint=999;auto safe=glass::normalize(invalid);passed=passed&&safe.features==2047&&glass::active(glass::TaskbarFill,glass::Capsule)&&glass::active(glass::TaskbarRoot,glass::Capsule)&&!glass::active(glass::TaskbarFill,glass::Dock)&&safe.opacity==90&&safe.radius==28&&safe.tint==3;
    passed=glass::selfTestSnapshot()&&passed;
    passed=passed&&glass::notificationHostWindow(L"ShellHost.exe",L"ControlCenterWindow")&&glass::notificationHostWindow(L"ShellExperienceHost.exe",L"Windows.UI.Core.CoreWindow")&&!glass::notificationHostWindow(L"ShellHost.exe",L"Windows.UI.Core.CoreWindow")&&!glass::notificationHostWindow(L"ShellExperienceHost.exe",L"ControlCenterWindow")&&!glass::notificationHostWindow(L"other.exe",L"ControlCenterWindow");
    {auto quick=glass::identify(L"ControlCenterRegion",L"Windows.UI.Xaml.Controls.Grid",2);auto calendar=glass::identify(L"CalendarCenterGrid",L"Windows.UI.Xaml.Controls.Grid",2);auto notification=glass::identify(L"NotificationCenterGrid",L"Windows.UI.Xaml.Controls.Grid",2);
    passed=passed&&quick==glass::NotificationAuxiliary&&glass::active(quick,glass::Notifications)&&glass::feature(quick)==0&&calendar==glass::CalendarPlate&&notification==glass::NotificationPlate&&glass::feature(calendar)==glass::Calendar&&glass::active(calendar,glass::Calendar)&&!glass::active(calendar,glass::Notifications)&&!glass::active(quick,glass::Start);}
    passed=passed&&glass::startHost(L"SearchHost.exe")&&glass::startHost(L"SearchApp.exe")&&glass::startHost(L"StartMenuExperienceHost.exe")&&!glass::startHost(L"explorer.exe");
    record("feature bounds and consistent configuration snapshots");
    // Owner-drawn switches must keep checked state through real button commands. The command is sent as the switch's own BN_CLICKED: BM_CLICK needs an active, shown window, and the test window stays hidden.
    {bool before=dockOn;HWND toggle=GetDlgItem(mainWindow,DockToggle);auto click=[&]{SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(DockToggle,BN_CLICKED),reinterpret_cast<LPARAM>(toggle));};click();passed=passed&&toggle&&dockOn!=before&&SendMessageW(toggle,BM_GETCHECK,0,0)==(dockOn?BST_CHECKED:BST_UNCHECKED);click();passed=passed&&dockOn==before&&SendMessageW(toggle,BM_GETCHECK,0,0)==(dockOn?BST_CHECKED:BST_UNCHECKED);KillTimer(mainWindow,14);glassPendingDiscover=false;}
    record("owner-drawn switches keep checked state through button commands");
    // Reset tuning preserves selected effects and writes factory material values.
    {auto savedPath=iniPath;auto savedConfig=glassConfig;bool savedBattery=batteryFriendly;iniPath=settingsDir()+L"reset-tuning-self-test.ini";
    bool savedLinked[glass::FamilyCount];std::copy(std::begin(glassLookLinked),std::end(glassLookLinked),savedLinked);
    glassConfig.opacity=80;glassConfig.blur=38;glassConfig.radius=25;glassConfig.tint=2;glassConfig.features=glass::Keyboard|glass::Taskbar;batteryFriendly=false;
    // The Taskbar row's own sliders save an unlinked look; the Dock sliders save the dock; Reset all looks relinks and restores both. A failing part lists the values it saw.
    selectPage(1);expandSystemRow(2);SendDlgItemMessageW(mainWindow,LookOpacity,TBM_SETPOS,TRUE,70);SendDlgItemMessageW(mainWindow,LookBlur,TBM_SETPOS,TRUE,30);savePanel(false);
    {int row=systemOpen,slider=(int)SendDlgItemMessageW(mainWindow,LookOpacity,TBM_GETPOS,0,0),blur=(int)GetPrivateProfileIntW(L"Glass.Taskbar",L"Blur",-1,iniPath.c_str());std::string name="Taskbar row sliders save its own look";
    passed=!glassLookLinked[glass::TaskbarLook]&&glassConfig.looks[glass::TaskbarLook].opacity==70&&blur==30;
    if(!passed){char d[128];snprintf(d,sizeof(d)," (row %d, slider %d, opacity %lu, linked %d, ini blur %d)",row,slider,(unsigned long)glassConfig.looks[glass::TaskbarLook].opacity,glassLookLinked[glass::TaskbarLook]?1:0,blur);name+=d;}record(name.c_str());}
    {std::string name="other surfaces stay on the shared look";passed=glassLookLinked[glass::StartLook]&&glassLookLinked[glass::NotificationLook]&&glassLookLinked[glass::CalendarLook]&&glassLookLinked[glass::ExplorerLook];
    if(!passed){char d[64];snprintf(d,sizeof(d)," (linked %d%d%d%d%d)",glassLookLinked[0]?1:0,glassLookLinked[1]?1:0,glassLookLinked[2]?1:0,glassLookLinked[3]?1:0,glassLookLinked[4]?1:0);name+=d;}record(name.c_str());}
    expandSystemRow(7);SendDlgItemMessageW(mainWindow,DockScale,TBM_SETPOS,TRUE,180);savePanel(true);
    {int row=systemOpen,slider=(int)SendDlgItemMessageW(mainWindow,DockScale,TBM_GETPOS,0,0),scale=(int)GetPrivateProfileIntW(L"Glass.Dock",L"Scale",-1,iniPath.c_str());std::string name="Dock sliders save the dock";
    passed=glassConfig.dockScale==180&&scale==180;
    if(!passed){char d[96];snprintf(d,sizeof(d)," (row %d, slider %d, scale %lu, ini scale %d)",row,slider,(unsigned long)glassConfig.dockScale,scale);name+=d;}record(name.c_str());}
    expandSystemRow(2);SendMessageW(mainWindow,WM_COMMAND,RestoreOriginal,0);
    passed=passed&&glassConfig.opacity==35&&glassConfig.blur==12&&glassConfig.radius==12&&glassConfig.tint==2&&!batteryFriendly&&glassConfig.features==(glass::Keyboard|glass::Taskbar)&&GetPrivateProfileIntW(L"Glass",L"Opacity",999,iniPath.c_str())==35&&glassLookLinked[glass::TaskbarLook]&&glassConfig.dockScale==126&&SendDlgItemMessageW(mainWindow,LookOpacity,TBM_GETPOS,0,0)==35&&SendDlgItemMessageW(mainWindow,LookBlur,TBM_GETPOS,0,0)==12&&SendDlgItemMessageW(mainWindow,DockScale,TBM_GETPOS,0,0)==126;
    DeleteFileW(iniPath.c_str());iniPath=savedPath;glassConfig=savedConfig;batteryFriendly=savedBattery;std::copy(std::begin(savedLinked),std::end(savedLinked),glassLookLinked);expandSystemRow(-1);loadLookPanel();selectPage(0);}
    record("Reset all looks relinks and restores defaults");
    // Rapid switch changes publish only the final requested mask after batching.
    {auto savedConfig=glassConfig;bool oldVolume=volumeOn,oldBrightness=brightnessOn,oldExplorer=explorerOn,oldDock=dockOn,oldKeyboard=keyboardOn;bool oldShell[3]={shellOn[0],shellOn[1],shellOn[2]};DWORD oldExtra=glassExtraFeatures;
    // Volume/Brightness no longer reach the XAML engine; Calendar is its own bit, independent of Notifications.
    volumeOn=brightnessOn=true;glassExtraFeatures=0;explorerOn=dockOn=keyboardOn=false;for(bool&on:shellOn)on=false;shellOn[0]=true;queueGlass();shellOn[0]=false;shellOn[2]=true;glassExtraFeatures=glass::Calendar;queueGlass();shellOn[2]=false;queueGlass();
    passed=passed&&glassPendingDiscover&&glassConfig.features==glass::Calendar&&(desiredGlassFeatures()&(glass::Volume|glass::Brightness))==0&&SendMessageW(GetDlgItem(mainWindow,TaskbarToggle),BM_GETCHECK,0,0)==BST_UNCHECKED&&SendMessageW(GetDlgItem(mainWindow,CalendarToggle),BM_GETCHECK,0,0)==BST_CHECKED&&SendMessageW(GetDlgItem(mainWindow,VolumeOn),BM_GETCHECK,0,0)==BST_CHECKED;
    KillTimer(mainWindow,14);glassPendingDiscover=false;glassExtraFeatures=oldExtra;volumeOn=oldVolume;brightnessOn=oldBrightness;explorerOn=oldExplorer;dockOn=oldDock;keyboardOn=oldKeyboard;for(int i=0;i<3;++i)shellOn[i]=oldShell[i];glassConfig=savedConfig;updateNativeInfo();}
    { // OSD migration: marker set uses the stored switches; no marker follows IndicatorOn (0 = both off even with old XAML bits, absent/1 = both on)
        auto a=osdMigration(false,1,0,false,false),b=osdMigration(false,0,glass::Volume|glass::Brightness,false,false),c=osdMigration(true,0,0,true,false),d=osdMigration(true,1,glass::Volume,false,true);
        passed=passed&&a.volume&&a.brightness&&!b.volume&&!b.brightness&&c.volume&&!c.brightness&&!d.volume&&d.brightness;
    }
    passed=passed&&glass::unifiedTaskbar(0)==0&&glass::unifiedTaskbar(glass::Taskbar)==(glass::Taskbar|glass::Capsule)&&glass::unifiedTaskbar(glass::Capsule|glass::Dock)==(glass::Taskbar|glass::Capsule|glass::Dock)&&glass::unifiedTaskbar(glass::Start)==glass::Start&&pageControls[1].size()==26+indicatorPanel.items.size()+lookPanel.items.size()+dockPanel.items.size()+systemFooter.items.size()&&indicatorPanel.items.size()==30&&lookPanel.items.size()==8&&dockPanel.items.size()==8&&systemFooter.items.size()==2;
    passed=passed&&!glass::active(glass::TaskbarAuxiliary,glass::Capsule)&&glass::active(glass::TaskbarAuxiliary,glass::Taskbar)&&glass::feature(glass::TaskbarAuxiliary)==glass::Taskbar;
    passed=passed&&glass::identify(L"BackgroundFill",L"Windows.UI.Xaml.Shapes.Rectangle",0)==glass::TaskbarFill&&glass::identify(L"BackgroundFill",L"Windows.UI.Xaml.Shapes.Rectangle",3)==glass::NoSurface&&glass::identify(L"MyDocument",L"Grid",3)==glass::NoSurface;
    passed=passed&&!glass::active(glass::VolumePlate,glass::Brightness)&&glass::active(glass::BrightnessPlate,glass::Brightness)&&!glass::active(glass::TaskbarFill,glass::Start);
    record("switch batching and independent surface selection");
    // Exercise first launch and saved preferences without touching the user's INI.
    auto userIni=iniPath;auto userLanguage=l10n::current;
    iniPath=userIni+L".language-test-"+std::to_wstring(GetCurrentProcessId());
    passed=passed&&l10n::read(iniPath)==l10n::English;
    for(int i=0;i<l10n::Count;++i){l10n::current=(l10n::Language)i;saveLanguage();passed=passed&&l10n::read(iniPath)==l10n::current;}
    passed=DeleteFileW(iniPath.c_str())&&passed;iniPath=userIni;l10n::current=userLanguage;
    record("language preference round trips");
    // Removed commands must not retain hidden controls or message handlers.
    for(int removedId:{109,110,111,112,116,117,120,121})passed=passed&&!GetDlgItem(mainWindow,removedId);
    passed=passed&&usesExplorer(0,L"explorer.exe")&&usesExplorer(1,L"")&&usesExplorer(2,L"")&&!usesExplorer(0,L"notepad.exe");
    passed=passed&&TabCtrl_GetItemCount(tabs)==3;
    selectPage(1);passed=passed&&((GetWindowLongPtrW(GetDlgItem(mainWindow,VolumeOn),GWL_STYLE)&WS_VISIBLE)!=0)&&((GetWindowLongPtrW(list,GWL_STYLE)&WS_VISIBLE)==0);
    passed=passed&&!GetDlgItem(mainWindow,VolumeOff)&&!GetDlgItem(mainWindow,BrightnessOff);
    for(int id:{VolumeOn,BrightnessOn,TaskbarToggle,StartToggle,NotificationToggle,CalendarToggle,ExplorerToggle,DockToggle}){HWND h=GetDlgItem(mainWindow,id);RECT r{};GetWindowRect(h,&r);passed=passed&&h&&((GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW)&&r.right-r.left<=MulDiv(110,dpi,96)&&r.bottom-r.top<=MulDiv(40,dpi,96);}
    passed=shell_materials::regressionTest(settingsDir())&&passed;
    passed=full_themes::regressionTest(settingsDir())&&passed;
    passed=taskbar_recovery::test(iniPath+L".taskbar-test-"+std::to_wstring(GetCurrentProcessId()))&&passed;
    record("system controls and isolated theme recovery");
    // One page per top tab, no sub-tabs: rows expand in place (one System glass row at a time) and every shown control stays between the tabs and the status line.
    auto shown=[](int id){HWND c=GetDlgItem(mainWindow,id);return c&&(GetWindowLongPtrW(c,GWL_STYLE)&WS_VISIBLE)!=0;};
    auto clearOfChrome=[](){RECT top{},bottom{};GetWindowRect(tabs,&top);GetWindowRect(status,&bottom);bool ok=true;for(auto&e:controlLayout){HWND c=e.first;if(c==tabs||c==status||c==GetDlgItem(mainWindow,Header)||c==GetDlgItem(mainWindow,Pause)||!(GetWindowLongPtrW(c,GWL_STYLE)&WS_VISIBLE))continue;RECT r{};GetWindowRect(c,&r);ok=ok&&r.top>=top.bottom&&r.bottom<=bottom.top&&r.bottom>r.top;}return ok;};
    selectPage(2);passed=passed&&shown(SettingsNote)&&shown(Language)&&shown(BatteryFriendly)&&shown(AboutText)&&shown(ExitApp)&&GetDlgItem(mainWindow,OpticalPreview)&&!GetDlgItem(mainWindow,AppPages)&&!GetDlgItem(mainWindow,SettingsPages)&&!GetDlgItem(mainWindow,SettingsPages+1000)&&clearOfChrome();
    selectPage(1);passed=passed&&shown(KeyboardToggle)&&shown(RestoreOriginal)&&!shown(LookOpacity)&&!shown(OptArrival)&&!shown(PageScroll)&&clearOfChrome();
    expandSystemRow(2);passed=passed&&systemOpen==2&&shown(LookOpacity)&&shown(LookTint)&&!shown(Language)&&SendMessageW(GetDlgItem(mainWindow,LookTint),CB_GETCOUNT,0,0)==4&&!shown(DockScale)&&!shown(OptArrival)&&GetPropW(GetDlgItem(mainWindow,ExpandTaskbar),L"LG.Open")&&clearOfChrome();
    expandSystemRow(7);passed=passed&&shown(DockScale)&&shown(DockBounce)&&!shown(LookOpacity)&&!GetPropW(GetDlgItem(mainWindow,ExpandTaskbar),L"LG.Open")&&clearOfChrome();
    SendMessageW(mainWindow,WM_COMMAND,ExpandDock,0);passed=passed&&systemOpen==-1&&!shown(DockScale);
    expandSystemRow(0);passed=passed&&indicatorOpen()&&shown(OptArrival)&&shown(OptNote)&&shown(OpticalPreview)&&pageMaxScroll[1]>0&&shown(PageScroll)&&clearOfChrome();
    scrollPage(1<<20);passed=passed&&pageScroll[1]==pageMaxScroll[1]&&shown(RestoreOriginal)&&shown(KeyboardToggle)&&!shown(OptArrival)&&clearOfChrome();
    expandSystemRow(1);passed=passed&&shown(OptArrival)&&shown(BrightnessOn)&&clearOfChrome();expandSystemRow(-1);passed=passed&&pageScroll[1]==0&&!shown(PageScroll);
    selectPage(0);passed=passed&&shown(TitleOn)&&!shown(TitleInset)&&!shown(TitleNote);expandTitleRow(true);passed=passed&&titleOpen&&shown(TitleNote)&&shown(TitleLook8)&&clearOfChrome();expandTitleRow(false);passed=passed&&!shown(TitleInset)&&shown(List);
    selectPage(2);passed=passed&&!shown(LookOpacity)&&!shown(DockScale)&&!shown(TitleOn);
    {RECT client{},last{};GetClientRect(tabs,&client);TabCtrl_GetItemRect(tabs,TabCtrl_GetItemCount(tabs)-1,&last);passed=passed&&last.right<=client.right;}
    selectPage(0);
    WNDCLASSW shellFixtureClass{};shellFixtureClass.lpfnWndProc=DefWindowProcW;shellFixtureClass.hInstance=GetModuleHandleW(nullptr);shellFixtureClass.lpszClassName=L"Shell_SecondaryTrayWnd";
    bool registered=RegisterClassW(&shellFixtureClass)!=0;
    HWND shellFixture=registered?CreateWindowW(shellFixtureClass.lpszClassName,L"Boundary fixture",WS_POPUP,0,0,20,20,nullptr,nullptr,shellFixtureClass.hInstance,nullptr):nullptr;
    passed=passed&&shellFixture&&!applyWindow(shellFixture)&&originals.empty();
    if(shellFixture)DestroyWindow(shellFixture);
    if(registered)UnregisterClassW(shellFixtureClass.lpszClassName,shellFixtureClass.hInstance);
    record("tab navigation and shell exclusion");
    // Exercise actual controls without persisting language or changing system preferences.
    l10n::current=l10n::English;localize();wchar_t text[256]{};
    GetWindowTextW(GetDlgItem(mainWindow,Apply),text,256);passed=passed&&wcscmp(text,L"Apply")==0;
    GetWindowTextW(GetDlgItem(mainWindow,Restore),text,256);passed=passed&&wcscmp(text,L"Restore")==0;
    GetWindowTextW(GetDlgItem(mainWindow,VolumeOn),text,256);passed=passed&&wcscmp(text,L"Volume glass")==0;
    GetWindowTextW(GetDlgItem(mainWindow,NotificationHeading),text,256);passed=passed&&wcsncmp(text,L"Notification center",wcslen(L"Notification center"))==0;GetWindowTextW(GetDlgItem(mainWindow,CalendarHeading),text,256);passed=passed&&wcsncmp(text,L"Calendar",wcslen(L"Calendar"))==0;
    GetWindowTextW(GetDlgItem(mainWindow,ExpandDock),text,256);passed=passed&&wcscmp(text,L"Show Dock hover animation settings")==0;GetWindowTextW(GetDlgItem(mainWindow,RestoreOriginal),text,256);passed=passed&&wcscmp(text,L"Reset all looks")==0;
    passed=passed&&SendMessageW(scope,CB_GETCOUNT,0,0)==3&&SendMessageW(material,CB_GETCOUNT,0,0)==4;
    l10n::current=l10n::Simplified;localize();GetWindowTextW(GetDlgItem(mainWindow,Apply),text,256);passed=passed&&wcscmp(text,L"应用")==0;
    GetWindowTextW(GetDlgItem(mainWindow,Restore),text,256);passed=passed&&wcscmp(text,L"还原")==0;
    GetWindowTextW(GetDlgItem(mainWindow,VolumeOn),text,256);passed=passed&&wcscmp(text,L"音量玻璃")==0;
    GetWindowTextW(GetDlgItem(mainWindow,StartHeading),text,256);passed=passed&&wcsncmp(text,L"开始菜单",wcslen(L"开始菜单"))==0;
    const wchar_t* expected[]={L"Apply",L"应用",L"應用",L"Zastosuj",L"Áp dụng"}; for(int i=0;i<l10n::Count;++i){l10n::current=(l10n::Language)i;localize();GetWindowTextW(GetDlgItem(mainWindow,Apply),text,256);passed=passed&&wcscmp(text,expected[i])==0&&selection(language)==i&&SendMessageW(language,CB_GETCOUNT,0,0)==l10n::Count;} l10n::current=userLanguage;localize();
    for(auto&e:entries)passed=passed&&e.icon!=nullptr;
    auto switches=native_switches::fields(1,2,1);passed=passed&&switches.size()==6&&switches[1].data!=switches[4].data;
    for(int removed:{140,141,142,143,144,150,151,154,155})passed=passed&&!GetDlgItem(mainWindow,removed);
    auto nativeValues=native_osd::desired(0);passed=passed&&nativeValues.size()==4&&wcscmp(native_osd::Target,L"Grid#ConfirmatorMainGrid")==0;
    passed=passed&&font&&headingFont&&actionFont&&!GetDlgItem(mainWindow,MotionText)&&!GetDlgItem(mainWindow,CompatText);
    record("five-language controls, icons and legacy compatibility");
    // Exercise control geometry and font recreation at common monitor DPIs,
    // including a round trip. This does not change the user's display settings.
    {UINT savedDpi=dpi;selectPage(1);expandSystemRow(0);for(UINT testDpi:{96u,144u,192u,96u}){dpi=testDpi;resizeControls();for(int languageIndex=0;languageIndex<l10n::Count;++languageIndex){l10n::current=(l10n::Language)languageIndex;localize();{RECT client{},last{};GetClientRect(tabs,&client);TabCtrl_GetItemRect(tabs,TabCtrl_GetItemCount(tabs)-1,&last);passed=passed&&last.right<=client.right;}for(int id:{(int)LookOpacity,(int)DockScale,(int)TaskbarToggle,(int)RestoreOriginal,(int)KeyboardToggle,(int)ExpandDock}){RECT r{};GetWindowRect(GetDlgItem(mainWindow,id),&r);passed=passed&&r.right>r.left&&r.bottom>r.top;}RECT footer{};GetWindowRect(status,&footer);passed=passed&&clearOfChrome()&&shown(OptNote);{RECT optNote{},replay{},rowLeft{},rowAbove{};GetWindowRect(GetDlgItem(mainWindow,OptNote),&optNote);GetWindowRect(GetDlgItem(mainWindow,OptReplay),&replay);passed=passed&&optNote.bottom<=footer.top;for(int i=0;i<8;++i){RECT slider{},caption{};GetWindowRect(GetDlgItem(mainWindow,OptLook1+i),&slider);GetWindowRect(GetDlgItem(mainWindow,OptLookLabel1+i),&caption);passed=passed&&slider.right>slider.left&&caption.right<=slider.left&&slider.top>=replay.bottom&&slider.bottom<=optNote.top&&(i<2||slider.top>=rowAbove.bottom)&&(i%2==0||rowLeft.right<=caption.left);if(i%2==0)rowLeft=slider;else rowAbove=rowLeft.bottom>slider.bottom?rowLeft:slider;}}}LOGFONTW lf{};GetObjectW(font,sizeof(lf),&lf);passed=passed&&lf.lfHeight==-MulDiv(15,dpi,96);}dpi=savedDpi;l10n::current=userLanguage;resizeControls();localize();expandSystemRow(-1);selectPage(0);}
    record("96/144/192 DPI geometry, indicator panel layout and fonts");
    // Check both persisted backup formats without touching registry settings.
    std::wstring testBackup=iniPath.substr(0,iniPath.find_last_of(L"\\")+1)+L"native-backup-test-"+std::to_wstring(GetCurrentProcessId())+L".bin";
    try{
        auto saved=nativeValues;for(auto&v:saved){v.type=0xffffffff;v.data.clear();}
        native_osd::save(testBackup,12,saved);DWORD index=0;auto loaded=native_osd::load(testBackup,index);
        passed=passed&&index==12&&loaded.size()==4;for(auto&v:loaded)passed=passed&&v.type==0xffffffff&&v.data.empty();DeleteFileW(testBackup.c_str());
        HANDLE legacy=CreateFileW(testBackup.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(legacy==INVALID_HANDLE_VALUE)throw std::runtime_error("legacy fixture");DWORD magic=0x4e4f5331,slot=9,missing=0xffffffff,size=0;native_osd::io(legacy,&magic,4,true);native_osd::io(legacy,&slot,4,true);for(int i=0;i<3;++i){native_osd::io(legacy,&missing,4,true);native_osd::io(legacy,&size,4,true);}CloseHandle(legacy);
        loaded=native_osd::load(testBackup,index);passed=passed&&index==9&&loaded.size()==3;DeleteFileW(testBackup.c_str());
    }catch(...){passed=false;DeleteFileW(testBackup.c_str());}
    record("v1/v2 recovery journal compatibility");
    HWND fixture=CreateWindowExW(0,ClassName,L"Restoration fixture",WS_OVERLAPPEDWINDOW,0,0,300,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    int originalBackdrop=0,originalCorner=0,afterBackdrop=0,afterCorner=0;
    bool supported=fixture&&SUCCEEDED(DwmGetWindowAttribute(fixture,38,&originalBackdrop,sizeof(int)))&&SUCCEEDED(DwmGetWindowAttribute(fixture,33,&originalCorner,sizeof(int)));
    if(supported){
        bool accepted=applyWindow(fixture);unsigned writes=attributeWrites;applyWindow(fixture);passed=passed&&attributeWrites==writes;restoreWindows();
        bool restored=SUCCEEDED(DwmGetWindowAttribute(fixture,38,&afterBackdrop,sizeof(int)))&&SUCCEEDED(DwmGetWindowAttribute(fixture,33,&afterCorner,sizeof(int)));
        passed=passed&&accepted&&restored&&originalBackdrop==afterBackdrop&&originalCorner==afterCorner&&originals.empty()&&!GetPropW(fixture,L"LiquidGlass.Control.Owner");
    }
    if(fixture)DestroyWindow(fixture);
    record("DWM restoration where supported");
    {auto savedLanguage=l10n::current;l10n::current=l10n::English;localize();selectPage(1);expandSystemRow(-1);RemovePropW(tabs,L"LG.Sel");passed=saveUiProof(mainWindow,settingsDir()+L"ui-system-effects.bmp");
        for(int id:{BrightnessOn,KeyboardToggle}){auto h=GetDlgItem(mainWindow,id);auto before=SendMessageW(h,BM_GETCHECK,0,0);SendMessageW(h,BM_SETCHECK,BST_CHECKED,0);motionFinish(h);passed=passed&&SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;auto mo=motionOf(h);mo->v[kHover]=mo->t[kHover]=1;auto previousFocus=SetFocus(h);SendMessageW(h,WM_UPDATEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);passed=saveUiProof(mainWindow,settingsDir()+(id==BrightnessOn?L"ui-brightness-hover.bmp":L"ui-keyboard-hover.bmp"))&&passed;SetFocus(previousFocus);SendMessageW(h,BM_SETCHECK,BST_UNCHECKED,0);motionFinish(h);passed=passed&&SendMessageW(h,BM_GETCHECK,0,0)==BST_UNCHECKED;SendMessageW(h,BM_SETCHECK,before,0);motionFinish(h);mo->v[kHover]=mo->t[kHover]=0;}

        expandSystemRow(2);RemovePropW(tabs,L"LG.Sel");passed=saveUiProof(mainWindow,settingsDir()+L"ui-system-material.bmp")&&passed;
        l10n::current=l10n::Simplified;localize();RemovePropW(tabs,L"LG.Sel");passed=saveUiProof(mainWindow,settingsDir()+L"ui-system-material-zh.bmp")&&passed;
        expandSystemRow(0);passed=shown(OptArrival)&&!shown(LookOpacity)&&saveUiProof(mainWindow,settingsDir()+L"ui-lgengine-zh.bmp")&&passed;selectPage(2);passed=!shown(OptArrival)&&passed; // by style: the test window is never shown, so IsWindowVisible is always false
        l10n::current=savedLanguage;localize();expandSystemRow(-1);selectPage(0);record("system glass pages render using native controls without a host overlay");}
    passed=allPassed;
    std::wstring report=iniPath.substr(0,iniPath.find_last_of(L"\\")+1)+L"self-test.txt";
    const char* result=passed?(supported?"PASS: embedded standalone engine, COM factory and GPU blur graph; legacy taskbar recovery; isolated feature masks; bounded material controls; five-language compact switches; legacy-theme restore/conflicts; isolated shell apply/restore/conflicts; native motion rule; v1/v2 backup compatibility; zero duplicate DWM writes; restore.\r\n":"PASS: embedded standalone engine, COM factory and GPU blur graph; legacy taskbar recovery; isolated feature masks; bounded material controls; five-language compact switches; legacy-theme restore/conflicts; isolated shell apply/restore/conflicts; native motion rule; v1/v2 backup compatibility. SKIP: DWM backdrop unavailable.\r\n"):"FAIL: UI, caching, backup compatibility, or restoration check.\r\n";
    HANDLE file=CreateFileW(report.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;
    if(file!=INVALID_HANDLE_VALUE){WriteFile(file,result,(DWORD)lstrlenA(result),&written,nullptr);WriteFile(file,checks.data(),(DWORD)checks.size(),&written,nullptr);char details[100]{};int n=snprintf(details,sizeof(details),"Composition result (E_PENDING means not run): 0x%08lx\r\n",(unsigned long)graphicsResult);WriteFile(file,details,n,&written,nullptr);CloseHandle(file);}else passed=false;
    return passed?0:2;
}
#endif
void show(){
    BOOL systemMotion=!reducedMotion();
    if(!startupPlayed&&startupAnimation&&systemMotion){startupPlayed=true;startupTime=GetTickCount64();SetWindowLongPtrW(mainWindow,GWL_EXSTYLE,GetWindowLongPtrW(mainWindow,GWL_EXSTYLE)|WS_EX_LAYERED);SetLayeredWindowAttributes(mainWindow,0,0,LWA_ALPHA);ShowWindow(mainWindow,IsIconic(mainWindow)?SW_RESTORE:SW_SHOW);if(!SetTimer(mainWindow,4,uiFrameInterval(),nullptr)){SetLayeredWindowAttributes(mainWindow,0,255,LWA_ALPHA);SetWindowLongPtrW(mainWindow,GWL_EXSTYLE,GetWindowLongPtrW(mainWindow,GWL_EXSTYLE)&~WS_EX_LAYERED);}}else{startupPlayed=true;ShowWindow(mainWindow,IsIconic(mainWindow)?SW_RESTORE:SW_SHOW);}refresh();SetForegroundWindow(mainWindow);
}
void menu(){HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,1,tr(L"打开设置",L"Open settings"));AppendMenuW(m,MF_STRING,2,tr(L"还原",L"Restore"));AppendMenuW(m,MF_STRING,3,tr(L"还原并退出",L"Restore and exit"));POINT p;GetCursorPos(&p);SetForegroundWindow(mainWindow);int n=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,mainWindow,nullptr);DestroyMenu(m);if(n==1)show();if(n==2)restoreAll();if(n==3)restoreAll(true);PostMessageW(mainWindow,WM_NULL,0,0);}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    static UINT attachAck=RegisterWindowMessageW(glass::AttachAckMessage);if(msg==attachAck){engine.acknowledge((DWORD)w);return 0;}
    static UINT engineStatus=RegisterWindowMessageW(glass::StatusMessage);if(msg==engineStatus){static HRESULT shownEngineError=S_OK;if(!w){if(FAILED((HRESULT)l)&&glassConfig.features&&(HRESULT)l!=shownEngineError){shownEngineError=(HRESULT)l;wchar_t text[256]{};swprintf(text,256,tr(L"系统外壳引擎报告错误 0x%08lX，详情见 glass-engine 日志。",L"The shell engine reported error 0x%08lX. Details are in the glass-engine log."),(unsigned long)l);message(text,1);}return 0;}if(SUCCEEDED((HRESULT)l))shownEngineError=S_OK; // glass works again: the same error is news next time
    DWORD flags=(DWORD)w&glassConfig.features&~pausedFeatures;if(!flags)return 0;bool unchanged=FAILED((HRESULT)l)?(failedMaterials&flags)==flags:((confirmedMaterials&flags)==flags&&!(failedMaterials&flags)&&((fallbackMaterials&flags)==((HRESULT)l==S_FALSE?flags:0)));if(unchanged)return 0;if(FAILED((HRESULT)l)){failedMaterials|=flags;confirmedMaterials&=~flags;message(tr(L"部分系统材质未生效，请查看各项连接状态。",L"Some shell materials were not applied. Check each connection status."),1);}else{confirmedMaterials|=flags;failedMaterials&=~flags;if((HRESULT)l==S_FALSE)fallbackMaterials|=flags;else fallbackMaterials&=~flags;message((HRESULT)l==S_FALSE?tr(L"系统材质已改用亚克力后备（模糊固定）。",L"Shell materials applied with the acrylic fallback (fixed blur)."):tr(L"系统材质属性已写入，请查看对应区域的外观。",L"Shell material properties written. Check the selected surface appearance."),1);}updateNativeInfo();return 0;}
    static UINT restart=RegisterWindowMessageW(L"TaskbarCreated");if(msg==restart){Shell_NotifyIconW(NIM_ADD,&tray);Shell_NotifyIconW(NIM_SETVERSION,&tray);if(glassConfig.features)engine.discover();return 0;}
    switch(msg){
    case WM_PRINTCLIENT:printControlClient(h,reinterpret_cast<HDC>(w));return 0;
    case WM_SHOWWINDOW:if(!w&&list)releaseAppList();break;
    case WM_SIZE:if(w==SIZE_MINIMIZED){SetPropW(h,L"LG.Minimized",(HANDLE)1);if(list)releaseAppList();}else if(RemovePropW(h,L"LG.Minimized")&&list&&currentPage==0)refresh(true);break;
    case WM_DPICHANGED:{finishPageMotion();dpi=HIWORD(w);auto suggested=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);resizeControls();return 0;}
    case WM_PAINT:paintChrome(h);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_DRAWITEM:if(drawModernControl(reinterpret_cast<DRAWITEMSTRUCT*>(l)))return TRUE;break;
    case WM_MEASUREITEM:{auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);if(item->CtlType==ODT_COMBOBOX){item->itemHeight=MulDiv(26,dpi,96);return TRUE;}if(item->CtlID==List){item->itemHeight=MulDiv(48,dpi,96);return TRUE;}}break;
    case WM_NOTIFY:if(reinterpret_cast<NMHDR*>(l)->code==TCN_SELCHANGE){if(reinterpret_cast<NMHDR*>(l)->idFrom==Pages)selectPage(TabCtrl_GetCurSel(tabs));}return 0;
    case WM_MOUSEWHEEL:{int delta=GET_WHEEL_DELTA_WPARAM(w);if(pageMaxScroll[currentPage]>0&&delta){scrollPage(pageScroll[currentPage]-delta*48/WHEEL_DELTA);return 0;}}break;
    case WM_VSCROLL:if((HWND)l==pageScrollBar&&pageScrollBar){int v=pageScroll[currentPage];switch(LOWORD(w)){case SB_LINEUP:v-=36;break;case SB_LINEDOWN:v+=36;break;case SB_PAGEUP:v-=404;break;case SB_PAGEDOWN:v+=404;break;case SB_TOP:v=0;break;case SB_BOTTOM:v=pageMaxScroll[currentPage];break;case SB_THUMBTRACK:case SB_THUMBPOSITION:{SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_TRACKPOS;GetScrollInfo(pageScrollBar,SB_CTL,&si);v=si.nTrackPos;break;}default:return 0;}scrollPage(v);return 0;}break;
    case WM_COMMAND:if(engineAttempted&&!engineReady&&HIWORD(w)==BN_CLICKED&&engineToggleOn(LOWORD(w)))engineAttempted=false;switch(LOWORD(w)){
        case Language:if(HIWORD(w)==CBN_SELCHANGE){int selected=selection(language);if(selected>=0&&selected<l10n::Count){l10n::current=(l10n::Language)selected;saveLanguage();localize();}}break;
        case ColorTheme:if(HIWORD(w)==CBN_SELCHANGE)applyColorTheme(selection(GetDlgItem(h,ColorTheme)),true);break;
        case List:if(HIWORD(w)==LBN_SELCHANGE)updateCompatibility();break;
        case Scope:if(HIWORD(w)==CBN_SELCHANGE)updateCompatibility();break;
        case LookTint:if(HIWORD(w)==CBN_SELCHANGE)savePanel(false);break;
        case ExpandVolume:case ExpandBrightness:case ExpandTaskbar:case ExpandStart:case ExpandNotification:case ExpandCalendar:case ExpandExplorer:case ExpandDock:{int row=LOWORD(w)-ExpandVolume;expandSystemRow(systemOpen==row?-1:row);break;}
        case ExpandTitle:expandTitleRow(!titleOpen);break;
        case BatteryFriendly:batteryFriendly=!batteryFriendly;SendMessageW(GetDlgItem(h,BatteryFriendly),BM_SETCHECK,batteryFriendly?BST_CHECKED:BST_UNCHECKED,0);applyGlass(false);break;
        case StartupAnimation:startupAnimation=!startupAnimation;SendMessageW(GetDlgItem(h,StartupAnimation),BM_SETCHECK,startupAnimation?BST_CHECKED:BST_UNCHECKED,0);WritePrivateProfileStringW(L"UI",L"StartupAnimation",startupAnimation?L"1":L"0",iniPath.c_str());break;
        case VolumeOn:volumeOn=!volumeOn;osdSwitched(volumeOn);break;
        case BrightnessOn:brightnessOn=!brightnessOn;osdSwitched(brightnessOn);break;
        case CalendarToggle:glassExtraFeatures^=glass::Calendar;queueGlass();break; // independent of Notification center (own bit, saved as [Glass] Calendar)
        case OptArrival:case OptIndicator:case OptSlosh:case OptKnob:case OptShape:if(HIWORD(w)==CBN_SELCHANGE){saveOpticalCombo(LOWORD(w));syncOpticalControls();}break;
        case OptReplay:syncOpticalControls(1);break;case OptSample:syncOpticalControls(2);break;        case TitleOn:{bool boxOn=IsDlgButtonChecked(mainWindow,TitleOn)!=BST_CHECKED;CheckDlgButton(mainWindow,TitleOn,boxOn?BST_CHECKED:BST_UNCHECKED);/* owner-drawn switch, as OptOn */appWrite(L"TitleOn",boxOn?1:0);if(boxOn){opticalSuspended=false;if(!opticalPolicyBlocked())opticalMayStart(true);}observeForeground();syncOpticalPreview();syncTitleGlass();break;}
        case OpticalPreview:opticalSuspended=false;if(!opticalPolicyBlocked())opticalMayStart(true);if(!indicatorOpen())expandSystemRow(0);else syncOpticalPreview();break; // a policy block retries only from the OptOn switch
        case RestoreOriginal:resetGlassDefaults();loadLookPanel();break;
        case KeyboardToggle:keyboardOn=!keyboardOn;queueGlass();break;
        case DockToggle:dockOn=!dockOn;queueGlass();break;

        case ExplorerToggle:explorerOn=!explorerOn;explorerByApp=false;queueGlass();break;
        case TaskbarToggle:case StartToggle:case NotificationToggle:{int kind=LOWORD(w)-TaskbarToggle;shellOn[kind]=!shellOn[kind];queueGlass();break;}
        case ExitApp:restoreAll(true);break;
        case Refresh:refresh();break;case Apply:if(!nativeProbeMode)apply();break;
        case Restore:restoreApps();break;
        case Pause:ShowWindow(h,SW_HIDE);break;
        }return 0;
    case WM_HSCROLL:{int titleId=l?GetDlgCtrlID((HWND)l):0;if(titleId==TitleInset||(titleId>=TitleLook1&&titleId<=TitleLook8)){updateTitleLabels();syncTitleGlass();if(LOWORD(w)!=TB_THUMBTRACK)saveTitleSlider(titleId);return 0;}}
    {int look=l?GetDlgCtrlID((HWND)l)-OptLook1:-1;if(look>=0&&look<8){updateLookLabels();syncOpticalControls();if(LOWORD(w)!=TB_THUMBTRACK)saveLookSlider(OptLook1+look);return 0;}}
    {int id=l?GetDlgCtrlID((HWND)l):0;bool dock=id==DockScale||id==DockSpread||id==DockSpeed||id==DockBounce;if(dock||id==LookOpacity||id==LookBlur||id==LookRadius){updatePanelLabels();if(LOWORD(w)!=TB_THUMBTRACK)savePanel(dock);else previewPanel(dock);}}return 0;
    case WM_POWERBROADCAST:if(w==PBT_POWERSETTINGCHANGE||w==PBT_APMPOWERSTATUSCHANGE){refreshUiPreferences();applyGlass(false);}return TRUE;
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:case WM_SYSCOLORCHANGE:syncOpticalPreview();refreshUiPreferences();applyColorTheme(colorTheme,false);if(engineReady)applyGlass(false);break;
    case glass::HostExitedMessage:if(engine.hostExited((DWORD)w)){confirmedMaterials&=~(glass::Start|glass::Notifications);fallbackMaterials&=~(glass::Start|glass::Notifications);updateNativeInfo();SetTimer(h,18,250,nullptr);SetTimer(h,6,3500,nullptr);}return 0;
    case WM_TIMER:if(w==kMotionTimer){tickControlMotion();return 0;}if(w==31){KillTimer(h,31);panelApplyTime=GetTickCount64();glassPreviewOnly=true;applyGlass(false);glassPreviewOnly=false;return 0;}if(w==21){KillTimer(h,21);engine.retryPackages();return 0;}if(w==20){engine.tick();updateDiagnostics();return 0;}if(w==23){reassertApps();return 0;}if(w==18){KillTimer(h,18);if(UINT next=engine.recoverHosts())SetTimer(h,18,next,nullptr);return 0;}if(w==22){KillTimer(h,22);syncOpticalPreview();return 0;}if(w==16){if(opticalPreview.connected()&&!opticalPreview.synchronized())syncOpticalPreview();if(opticalPreview.synchronized()){KillTimer(h,16);opticalConnectedMessage();updateOpticalButtons();}else if(!opticalStartup)KillTimer(h,16);else if(!opticalPreview.running()||GetTickCount64()-opticalStartup>15000)opticalFailed(tr(L"LGEngine 预览未能连接，请检查适配器及 Windows 应用控制状态。",L"LGEngine preview did not connect. Check the adapter and Windows application control."));return 0;}
        if(w==14){bool discover=glassPendingDiscover;applyGlass(discover);}else if(w==12){tickColorTheme();}else if(w==13){tickPageMotion();}else if(w==4){float t=std::min(1.f,(GetTickCount64()-startupTime)/220.f),ease=1-powf(1-t,4);SetLayeredWindowAttributes(h,0,(BYTE)(255*ease),LWA_ALPHA);if(t>=1){KillTimer(h,4);SetWindowLongPtrW(h,GWL_EXSTYLE,GetWindowLongPtrW(h,GWL_EXSTYLE)&~WS_EX_LAYERED);RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);}}else if(w==3){KillTimer(h,3);windowScanPending=false;auto windows=std::move(pendingWindows);pendingWindows.clear();syncPending(windows);if(glassConfig.features)for(auto window:windows)if(IsWindow(window))engine.discoverWindow(window);}else if(w==2)pollMigration();else if(w==5){KillTimer(h,5);engine.expireHooks(true);}else if(w==6){KillTimer(h,6);failedMaterials|=glassConfig.features&~pausedFeatures&~confirmedMaterials&~(glass::Volume|glass::Brightness);updateNativeInfo();if(failedMaterials)message(tr(L"部分区域尚未连接，已标记为未生效；打开对应区域后可重试。",L"Some surfaces are not connected and are marked Not applied; open them and retry."),1);}return 0;
    case lgpreview::HostState:
        if(w==1&&opticalPreview.owns(reinterpret_cast<HWND>(l))){opticalHostSeen=true;opticalHostError=0;opticalFailures=0;opticalFailureText.clear();KillTimer(h,22);opticalConnectedMessage();updateOpticalButtons();updateNativeInfo();}
        // 3 = the adapter's host launch failed with this Win32 code; it arrives just before Event 2.
        else if(w==3&&opticalPreview.running())opticalHostError=(DWORD)l;
        else if(w==2&&opticalPreview.exited()){lgpreview::restoreFlyouts();opticalFailed(opticalHostSeen?tr(L"LGEngine 预览意外停止，稍后将自动重启。",L"LGEngine preview stopped unexpectedly; it restarts shortly."):tr(L"LGEngine 预览未能连接，请检查适配器及 Windows 应用控制状态。",L"LGEngine preview did not connect. Check the adapter and Windows application control."));}
return 0;
    case lgpreview::Event:if((w==1||w==2)&&opticalPreview.accept(reinterpret_cast<HWND>(l))){if(w==1){syncOpticalPreview();if(opticalPreview.synchronized()){KillTimer(h,16);opticalConnectedMessage();}updateOpticalButtons();}else if(opticalPolicyBlocked()){opticalFailed(tr(L"LGEngine 宿主被智能应用控制（Windows 应用程序控制）阻止。玻璃指示器保持关闭，继续使用 Windows 自带的弹窗。",L"LGEngine host blocked by Smart App Control (Windows application control). The glass indicator stays off; Windows' own pop-up is used."));opticalFailures=3;KillTimer(h,22);updateNativeInfo();}else opticalFailed(tr(L"LGEngine 宿主不可用；详情见 lgengine\\host-client.log。关闭再打开“音量”或“亮度”开关即可重试。",L"LGEngine host unavailable; details in lgengine\\host-client.log. Turn Volume or Brightness off and on to retry."));}return 0;
    case WM_APP+1:if(LOWORD(l)==WM_CONTEXTMENU)menu();else if(LOWORD(l)==NIN_SELECT||LOWORD(l)==NIN_KEYSELECT)show();return 0;
    case WM_APP+2:show();return 0;
    case WM_APP+3:if(migrationJob)return 0;setRulesActive(false);restoreWindows();if(!originals.empty()){show();return 0;}if(!taskbar_recovery::restore(iniPath)){show();return 0;}WritePrivateProfileStringW(L"Taskbar",L"AutoHide",L"0",iniPath.c_str());engine.shutdown();DestroyWindow(h);return 0;
    case WM_APP+5:migrateLegacy();return 0;
    case WM_CLOSE:ShowWindow(h,SW_HIDE);return 0;
    case WM_QUERYENDSESSION:return TRUE; // another app may still cancel the shutdown: tear down only once it is certain
    case WM_ENDSESSION:if(w){taskbar_recovery::restore(iniPath);setRulesActive(false);restoreWindows();engine.shutdown();}return 0;
    case WM_CTLCOLORLISTBOX:{SetBkColor(reinterpret_cast<HDC>(w),cardColor());SetTextColor(reinterpret_cast<HDC>(w),textColor());return reinterpret_cast<LRESULT>(cardBrush);}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{int id=GetDlgCtrlID(reinterpret_cast<HWND>(l));bool common=id==Header||id==Status;SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),highContrast()?textColor():id==Header?textColor():palette().pressed);return reinterpret_cast<LRESULT>(common?background:cardBrush);}
    case WM_DESTROY:
        opticalPreview.stop();lgpreview::restoreFlyouts();finishPageMotion();KillTimer(h,12);KillTimer(h,23);KillTimer(h,5);KillTimer(h,6);KillTimer(h,1);KillTimer(h,2);KillTimer(h,3);KillTimer(h,4);Shell_NotifyIconW(NIM_DELETE,&tray);PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int){
    if(wcscmp(commandLine,L"--diagnose-shell")==0){wchar_t file[32768]{};GetModuleFileNameW(nullptr,file,32768);std::wstring path=file;path.resize(path.find_last_of(L"\\")+1);return glass::diagnoseShell(path+L"shell-diagnostics.txt");}
    if(wcscmp(commandLine,L"--legacy-restore-all")==0){wchar_t file[32768]{};GetModuleFileNameW(nullptr,file,32768);std::wstring dir=file;dir.resize(dir.find_last_of(L"\\")+1);return restoreLegacy(dir);}
    if(wcscmp(commandLine,L"--close-running")==0||wcscmp(commandLine,L"--close-native-probe")==0){auto old=FindWindowW(wcscmp(commandLine,L"--close-native-probe")==0?ProbeClassName:ClassName,nullptr);if(!old)return 0;DWORD_PTR result=0;return SendMessageTimeoutW(old,WM_APP+3,0,0,SMTO_ABORTIFHUNG,5000,&result)?0:1;}
#ifdef LG_ENABLE_SELF_TEST
    bool testing=wcscmp(commandLine,L"--self-test")==0;
#else
    bool testing=false;
#endif
    selfTestMode=testing;nativeProbeMode=wcscmp(commandLine,L"--probe-native-content")==0;if(nativeProbeMode){ClassName=ProbeClassName;InstanceName=L"Local\\LiquidGlass.Control.NativeProbe.SingleInstance";}
    bool backgroundStart=wcscmp(commandLine,L"--background")==0;
    if(*commandLine&&!testing&&!backgroundStart&&!nativeProbeMode)return 10;
    HANDLE mutex=CreateMutexW(nullptr,FALSE,testing?L"Local\\LiquidGlass.Control.SelfTest":InstanceName);if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){auto old=FindWindowW(ClassName,nullptr);if(old)PostMessageW(old,WM_APP+2,0,0);CloseHandle(mutex);return 0;}
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);dpi=GetDpiForSystem();
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX common{};common.dwSize=sizeof(common);common.dwICC=ICC_TAB_CLASSES|ICC_BAR_CLASSES;InitCommonControlsEx(&common);
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);iniPath=path;iniPath=iniPath.substr(0,iniPath.find_last_of(L"\\")+1)+L"settings.ini";
    if(nativeProbeMode)iniPath+=L".native-probe-"+std::to_wstring(GetCurrentProcessId());
    l10n::current=l10n::read(iniPath);
    glassConfig.features=GetPrivateProfileIntW(L"Glass",L"Features",0,iniPath.c_str());glassConfig.opacity=GetPrivateProfileIntW(L"Glass",L"Opacity",35,iniPath.c_str());glassConfig.radius=GetPrivateProfileIntW(L"Glass",L"Radius",12,iniPath.c_str());glassConfig.tint=GetPrivateProfileIntW(L"Glass",L"Tint",0,iniPath.c_str());glassConfig.blur=GetPrivateProfileIntW(L"Glass",L"Blur",12,iniPath.c_str());glassConfig=glass::normalize(glassConfig);glassConfig.features=glass::unifiedTaskbar(glassConfig.features);loadGlassSettings();
    if(nativeProbeMode)glassConfig.features=glass::Volume|glass::Brightness|glass::Taskbar|glass::Start|glass::Notifications|glass::Keyboard;
    if(!nativeProbeMode&&migrationNeeded())glassConfig.features=0;
    keyboardOn=(glassConfig.features&glass::Keyboard)!=0;dockOn=(glassConfig.features&glass::Dock)!=0;
    if(nativeProbeMode){volumeOn=(glassConfig.features&glass::Volume)!=0;brightnessOn=(glassConfig.features&glass::Brightness)!=0;}else loadOsdSettings(); // the probe still inspects the XAML indicator; otherwise the switches belong to the LGEngine indicator
    explorerOn=(glassConfig.features&glass::Explorer)!=0;for(int i=0;i<3;++i)shellOn[i]=(glassConfig.features&(glass::Taskbar<<i))!=0;
    batteryFriendly=GetPrivateProfileIntW(L"Glass",L"BatteryFriendly",1,iniPath.c_str())!=0;startupAnimation=GetPrivateProfileIntW(L"UI",L"StartupAnimation",1,iniPath.c_str())!=0;
    applyColorTheme(GetPrivateProfileIntW(L"UI",L"ColorTheme",0,iniPath.c_str()),false);WNDCLASSEXW c{};c.cbSize=sizeof(c);c.lpfnWndProc=proc;c.hInstance=instance;c.lpszClassName=ClassName;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);c.hIconSm=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);c.hbrBackground=background;
    if(!RegisterClassExW(&c)){DeleteObject(background);CloseHandle(mutex);return 1;}
    RECT size={0,0,MulDiv(790,dpi,96),MulDiv(630,dpi,96)};AdjustWindowRectEx(&size,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
    lgpreview::restoreFlyouts();
    mainWindow=CreateWindowExW(0,ClassName,L"Liquid Glass",WS_CLIPCHILDREN|WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,size.right-size.left,size.bottom-size.top,nullptr,nullptr,instance,nullptr);
    if(!mainWindow){DeleteObject(background);CloseHandle(mutex);return 1;}
    // The system DPI is the sign-in one: on a monitor rescaled since then, or a different one, size for the window's own DPI as WM_DPICHANGED does.
    if(UINT actual=GetDpiForWindow(mainWindow);actual&&actual!=dpi){dpi=actual;RECT fit={0,0,MulDiv(790,dpi,96),MulDiv(630,dpi,96)};AdjustWindowRectExForDpi(&fit,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,dpi);SetWindowPos(mainWindow,nullptr,0,0,fit.right-fit.left,fit.bottom-fit.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);resizeControls();}
    // Keep the window inside the monitor work area. Default placement could put its bottom under the taskbar, hiding the lower controls; only the position changes, never the size or the taskbar layout.
    {MONITORINFO mi{};mi.cbSize=sizeof(mi);RECT w{};GetWindowRect(mainWindow,&w);if(GetMonitorInfoW(MonitorFromWindow(mainWindow,MONITOR_DEFAULTTONEAREST),&mi)){int width=w.right-w.left,height=w.bottom-w.top,x=w.left,y=w.top;if(y+height>mi.rcWork.bottom)y=mi.rcWork.bottom-height;if(y<mi.rcWork.top)y=mi.rcWork.top;if(x+width>mi.rcWork.right)x=mi.rcWork.right-width;if(x<mi.rcWork.left)x=mi.rcWork.left;SetWindowPos(mainWindow,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}}
    buildUI();themeWindow(mainWindow);if(nativeProbeMode){SetWindowTextW(mainWindow,L"Liquid Glass — native content probe (no material overrides)");selectPage(1);}
#ifdef LG_ENABLE_SELF_TEST
    if(testing){int result=selfTest();DestroyWindow(mainWindow);for(auto&icon:appIcons)if(icon.second)DestroyIcon(icon.second);destroyFonts();DeleteObject(background);CloseHandle(mutex);CoUninitialize();return result;}
#endif
    if(!nativeProbeMode&&!taskbar_recovery::restore(iniPath)){message(tr(L"任务栏恢复失败，恢复记录已保留。",L"Taskbar restore failed; recovery record retained."));}
    updateNativeInfo();loadAppGlass();
    powerNotification=RegisterPowerSettingNotification(mainWindow,&PowerSavingStatus,DEVICE_NOTIFY_WINDOW_HANDLE);acNotification=RegisterPowerSettingNotification(mainWindow,&GUID_ACDC_POWER_SOURCE,DEVICE_NOTIFY_WINDOW_HANDLE);
    tray.cbSize=sizeof(tray);tray.hWnd=mainWindow;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=WM_APP+1;tray.hIcon=c.hIconSm?c.hIconSm:c.hIcon;tray.uVersion=NOTIFYICON_VERSION_4;
    if(!Shell_NotifyIconW(NIM_ADD,&tray)){MessageBoxW(mainWindow,tr(L"无法创建托盘图标，程序将退出。",L"Cannot create tray icon; exiting."),L"Liquid Glass",MB_ICONERROR);DestroyWindow(mainWindow);}else{Shell_NotifyIconW(NIM_SETVERSION,&tray);if(!backgroundStart)show();if(migrationNeeded())PostMessageW(mainWindow,WM_APP+5,0,0);else if(glassConfig.features)applyGlass();}
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN&&msg.wParam==VK_TAB&&(msg.hwnd==mainWindow||IsChild(mainWindow,msg.hwnd)))revealTabTarget(GetKeyState(VK_SHIFT)<0); // scroll the next tab stop into view before the dialog manager moves focus to it
        if(msg.message==WM_MOUSEWHEEL&&msg.hwnd!=mainWindow&&IsChild(mainWindow,msg.hwnd)){ // with "Scroll inactive windows" the wheel goes to the control under the cursor: unfocused sliders and combos must not change, and the app list hands the wheel back at its ends
            wchar_t cls[32]{};GetClassNameW(msg.hwnd,cls,32);bool combo=!_wcsicmp(cls,WC_COMBOBOXW);int delta=GET_WHEEL_DELTA_WPARAM(msg.wParam);
            if((combo||!_wcsicmp(cls,TRACKBAR_CLASSW))&&GetFocus()!=msg.hwnd&&!(combo&&SendMessageW(msg.hwnd,CB_GETDROPPEDSTATE,0,0)))msg.hwnd=mainWindow;
            else if(msg.hwnd==list&&delta){SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_ALL;bool bar=(GetWindowLongPtrW(list,GWL_STYLE)&WS_VSCROLL)&&GetScrollInfo(list,SB_VERT,&si)&&si.nMax>si.nMin;if(!bar||(delta>0&&si.nPos<=si.nMin)||(delta<0&&si.nPos+(int)si.nPage>si.nMax))msg.hwnd=mainWindow;}}
        if(!IsDialogMessageW(mainWindow,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    if(windowEvents)UnhookWinEvent(windowEvents);if(titleHook)UnhookWinEvent(titleHook);if(powerNotification)UnregisterPowerSettingNotification(powerNotification);if(acNotification)UnregisterPowerSettingNotification(acNotification);engine.shutdown();if(nativeProbeMode)DeleteFileW(iniPath.c_str());if(migrationJob)CloseHandle(migrationJob);for(auto&icon:appIcons)if(icon.second)DestroyIcon(icon.second);destroyFonts();DeleteObject(background);CloseHandle(mutex);CoUninitialize();return 0;
}
